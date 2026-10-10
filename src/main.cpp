#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <ctime>
#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <thread>
#include <vector>
#include <chrono>
#include <cstdlib>
#include <cstdio>

#include "kalshi_ws.hpp"
#include "kalshi_order.hpp"
#include "kalshi_http.hpp"
#include "kalshi_env.hpp"
#include "polymarket_ws.hpp"
#include "polymarket_order.hpp"
#include "polymarket_http.hpp"
#include "gemini_ws.hpp"
#include "gemini_order.hpp"
#include "gemini_http.hpp"
#include "monitor.hpp"
#include "market_data.hpp"
#include "strategy.hpp"
#include "execution.hpp"
#include "portfolio.hpp"
#include "risk.hpp"
#include "latency.hpp"
#include "tape.hpp"
#include "live_busy.hpp"

PortfolioManager portfolio;
RiskLimits limits = loadRiskLimits();
RiskState risk_gate;
std::unordered_map<long int, std::string> kalshi_ids_flipped;
std::unordered_map<long int, std::string> pm_ids_flipped;
std::unordered_map<long int, std::string> gem_ids_flipped;
std::unordered_set<long int> expired_logged;
LiveBusy live_busy;
MarketData* books = nullptr;

static bool tryBusy(long int market_id) {
    return live_busy.tryAcquire(market_id);
}

static void clearBusy(long int market_id) {
    live_busy.release(market_id);
}

static const char* srcName(int venue) {
    if (venue == Kalshi) return "kalshi";
    if (venue == Polymarket) return "polymarket";
    if (venue == Gemini) return "gemini";
    return "?";
}

static std::string moneyStr(int v) {
    return formatCents(v);
}

static bool applyFill(const Fill& f) {
    FillApply applied = portfolio.updatePortfolio(f);
    if (applied == FillApply::Unregistered) {
        monitorLog("unregistered fill market=" + std::to_string(f.market_id)
            + " venue=" + srcName(f.venue) + " size=" + std::to_string(f.size));
        return false;
    }
    if (applied == FillApply::Applied && riskNoteFill(risk_gate, limits, f)) {
        monitorHalt();
        monitorLog("kill switch: max drawdown reached");
    }
    return applied == FillApply::Applied;
}

void bumpLimit(OrderIntent& idea) {
    idea.limit_price = nextChasePrice(idea.limit_price);
}

void refreshVenueCash();

static std::string tickerFor(const OrderIntent& leg) {
    if (leg.venue == Kalshi) return kalshi_ids_flipped[leg.venue_id];
    if (leg.venue == Polymarket) return pm_ids_flipped[leg.venue_id];
    if (leg.venue == Gemini) return gem_ids_flipped[leg.venue_id];
    return "";
}

static bool sendVenueOrder(OrderIntent& leg, const char* http_clock) {
    auto k0 = std::chrono::steady_clock::now();
    bool ok = false;
    int fill_count = 0;
    int fill_price = 0;
    std::string ticker = tickerFor(leg);
    if (leg.venue == Kalshi) {
        KalshiOrderResult r = sendKalshiOrder(ticker, leg);
        ok = r.ok;
        fill_count = r.fill_count;
        fill_price = r.fill_price;
    } else if (leg.venue == Polymarket) {
        PolymarketOrderResult r = sendPolymarketOrder(ticker, leg);
        ok = r.ok;
        fill_count = r.fill_count;
        fill_price = r.fill_price;
    } else if (leg.venue == Gemini) {
        GeminiOrderResult r = sendGeminiOrder(ticker, leg);
        ok = r.ok;
        fill_count = r.fill_count;
        fill_price = r.fill_price;
    }
    monitorLog(latField(http_clock, latNs(k0, std::chrono::steady_clock::now())));
    if (!ok) return false;
    int size = 0;
    int price = 0;
    int fallback = leg.limit_price;
    if (!leg.buy && leg.venue == Kalshi && leg.side == NoSided) {
        fallback = complementCents(leg.limit_price);
    }
    if (!takeReportedFill(fill_count, fill_price, fallback, size, price)) {
        monitorLog(std::string(srcName(leg.venue)) + " fill count missing, treating as no fill");
        return false;
    }
    leg.limit_price = price;
    Fill f;
    f.market_id = leg.market_id;
    f.venue_id = leg.venue_id;
    f.venue = leg.venue;
    f.side = leg.side;
    f.size = size;
    f.price = price;
    f.buy = leg.buy;
    return applyFill(f);
}

static long int venueIdOn(const Market& m, int venue) {
    if (venue == Kalshi) return m.getKalshiSnapshot().venue_id;
    if (venue == Polymarket) return m.getPolymarketSnapshot().venue_id;
    if (venue == Gemini) return m.getGeminiSnapshot().venue_id;
    return 0;
}

static bool surplusSellIntent(long int market_id, OrderIntent& sell) {
    sell = OrderIntent{};
    sell.buy = false;
    sell.limit_price = 1;
    sell.market_id = market_id;
    Position pos = portfolio.getPositionByMarketId(market_id);
    int extra = pos.yes_count - pos.no_count;
    if (extra == 0) return false;
    if (extra > 0) {
        sell.side = YesSided;
        sell.size = extra;
        sell.venue = pos.yes_venue;
    } else {
        sell.side = NoSided;
        sell.size = -extra;
        sell.venue = pos.no_venue;
    }
    if (!books || sell.venue == 0) return false;
    sell.venue_id = venueIdOn(books->getMarketById(market_id), sell.venue);
    return sell.venue_id != 0 && sell.size > 0;
}

static bool flattenSurplus(long int market_id) {
    OrderIntent sell;
    if (!surplusSellIntent(market_id, sell)) return true;
    const char* clock = sell.venue == Kalshi ? "kalshi_http_ns"
        : (sell.venue == Polymarket ? "pm_http_ns" : "gem_http_ns");
    monitorLog(std::string("flatten leftover ") + (sell.side == YesSided ? "yes" : "no")
        + " " + std::to_string(sell.size) + " on " + srcName(sell.venue));
    if (sendVenueOrder(sell, clock)) {
        monitorLog("flatten filled");
        return true;
    }
    monitorLog("flatten missed; still directional");
    return false;
}

void liveFlatten(long int market_id) {
    auto t0 = std::chrono::steady_clock::now();
    monitorLog("flatten start");
    flattenSurplus(market_id);
    monitorLog(latField("flatten_total_ns", latNs(t0, std::chrono::steady_clock::now())));
    refreshVenueCash();
    clearBusy(market_id);
    monitorWorkingEnd();
}

void refreshVenueCash() {
    int k = 0, p = 0, g = 0;
    if (refreshKalshiBalance(k)) portfolio.setKalshiCash(k);
    if (refreshPolymarketBalance(p)) portfolio.setPolymarketCash(p);
    if (refreshGeminiBalance(g)) portfolio.setGeminiCash(g);
}

void applyKalshiLots(const std::unordered_map<std::string, long int>& kalshi_ids, MarketData& md) {
    std::vector<KalshiLot> lots;
    if (!refreshKalshiPositions(lots)) {
        monitorLog("kalshi positions unavailable");
        return;
    }
    int n = 0;
    for (auto& lot : lots) {
        auto it = kalshi_ids.find(lot.ticker);
        if (it == kalshi_ids.end()) continue;
        Snapshot s = md.getSnapshotByVenueId(it->second, Kalshi);
        if (s.market_id == 0) continue;
        portfolio.addVenueLot(s.market_id, s.venue, lot.yes_count, lot.no_count, lot.avg_yes, lot.avg_no);
        n++;
        if (lot.yes_count > 0) monitorLog(std::string("reconcile kalshi YES ") + lot.ticker);
        if (lot.no_count > 0) monitorLog(std::string("reconcile kalshi NO ") + lot.ticker);
    }
    if (n == 0) monitorLog("kalshi positions empty in universe");
}

void applyPolymarketLots(const std::unordered_map<std::string, long int>& ids, MarketData& md) {
    std::vector<PolymarketLot> lots;
    if (!refreshPolymarketPositions(lots)) {
        monitorLog("polymarket positions unavailable");
        return;
    }
    int n = 0;
    for (auto& lot : lots) {
        auto it = ids.find(lot.slug);
        if (it == ids.end()) continue;
        Snapshot s = md.getSnapshotByVenueId(it->second, Polymarket);
        if (s.market_id == 0) continue;
        portfolio.addVenueLot(s.market_id, s.venue, lot.yes_count, lot.no_count, lot.avg_yes, lot.avg_no);
        n++;
        if (lot.yes_count > 0) monitorLog(std::string("reconcile polymarket YES ") + lot.slug);
        if (lot.no_count > 0) monitorLog(std::string("reconcile polymarket NO ") + lot.slug);
    }
    if (n == 0) monitorLog("polymarket positions empty in universe");
}

void applyGeminiLots(const std::unordered_map<std::string, long int>& ids, MarketData& md) {
    std::vector<GeminiLot> lots;
    if (!refreshGeminiPositions(lots)) {
        monitorLog("gemini positions unavailable");
        return;
    }
    int n = 0;
    for (auto& lot : lots) {
        auto it = ids.find(lot.symbol);
        if (it == ids.end()) continue;
        Snapshot s = md.getSnapshotByVenueId(it->second, Gemini);
        if (s.market_id == 0) continue;
        portfolio.addVenueLot(s.market_id, s.venue, lot.yes_count, lot.no_count, lot.avg_yes, lot.avg_no);
        n++;
        if (lot.yes_count > 0) monitorLog(std::string("reconcile gemini YES ") + lot.symbol);
        if (lot.no_count > 0) monitorLog(std::string("reconcile gemini NO ") + lot.symbol);
    }
    if (n == 0) monitorLog("gemini positions empty in universe");
}

void liveSendPair(OrderIntent yes, OrderIntent no) {
    OrderIntent reserved_yes = yes;
    OrderIntent reserved_no = no;
    auto pair0 = std::chrono::steady_clock::now();
    monitorLog("live pair start");

    auto sendSide = [&](OrderIntent& leg, OrderIntent& other) -> bool {
        const char* clock = leg.venue == Kalshi ? "kalshi_http_ns" : (leg.venue == Polymarket ? "pm_http_ns" : "gem_http_ns");
        int ceiling = chaseCeiling(other.limit_price, other.venue, leg.venue, limits.chase_reserve);
        for (int i = 0; i < 15; i++) {
            if (leg.limit_price > ceiling) {
                monitorLog("chase budget exhausted");
                return false;
            }
            if (sendVenueOrder(leg, clock)) {
                monitorLog(std::string(srcName(leg.venue)) + " filled");
                if (leg.side == YesSided) yes = leg; else no = leg;
                return true;
            }
            if (!canBumpChase(leg.limit_price, ceiling)) {
                monitorLog("chase budget exhausted");
                return false;
            }
            monitorLog(std::string(srcName(leg.venue)) + " miss, chase +0.01");
            bumpLimit(leg);
            if (leg.side == YesSided) yes = leg; else no = leg;
            ceiling = chaseCeiling(other.limit_price, other.venue, leg.venue, limits.chase_reserve);
        }
        return false;
    };

    OrderIntent* first = &yes;
    OrderIntent* second = &no;
    if (no.venue == Kalshi && yes.venue != Kalshi) {
        first = &no;
        second = &yes;
    }
    if (!sendSide(*first, *second)) {
        monitorLog("skip second: first leg not filled");
        monitorLog(latField("pair_total_ns", latNs(pair0, std::chrono::steady_clock::now())));
        release_pair(portfolio, reserved_yes, reserved_no);
        refreshVenueCash();
        clearBusy(reserved_yes.market_id);
        monitorWorkingEnd();
        return;
    }
    sendSide(*second, *first);
    release_pair(portfolio, reserved_yes, reserved_no);
    flattenSurplus(reserved_yes.market_id);
    refreshVenueCash();
    clearBusy(reserved_yes.market_id);
    monitorWorkingEnd();
    monitorLog(latField("pair_total_ns", latNs(pair0, std::chrono::steady_clock::now())));
    monitorLog("live pair done");
}

void on_tick(MarketData& md, long int venue_id) {
    SlotRef ref = md.slotForVenue(venue_id);
    if (ref.slot < 0) return;
    Market m = md.getMarketAt(ref.slot);
    Snapshot s = m.getSnapshot(ref.venue);

    if (tapeOut().is_open()) tapeWrite(s);

    long int now = assisiNow();
    if (m.getMarketId() != 0 && m.getExpirationDate() <= now) {
        if (expired_logged.insert(m.getMarketId()).second) {
            monitorLog("expired, dropped from trading");
        }
        return;
    }

    Portfolio book = portfolio.getPortfolio();
    Position pos = book.positionFor(m.getMarketId());
    if (pos.yes_count != pos.no_count) {
        latencyIntent();
        latencyRecordTick();
        if (assisiClockOnly()) {
            monitorLog(latencySendLine() + " flatten");
            monitorLog("clock only, no send");
        } else if (!monitorHalted()) {
            if (assisiLiveOrders() && !assisiReplay()) {
                if (!tryBusy(m.getMarketId())) {
                    monitorLog("risk rejected: " + ReasonLogger(RiskReason::InFlight));
                } else {
                    monitorWorkingBegin();
                    long int mid = m.getMarketId();
                    monitorLog(latencySendLine() + " flatten");
                    std::thread([mid]() { liveFlatten(mid); }).detach();
                }
            } else {
                OrderIntent sell;
                if (surplusSellIntent(m.getMarketId(), sell)) {
                    Execute ex;
                    ex.Executioner(sell);
                    Fill f = ex.getFill();
                    if (f.size > 0) applyFill(f);
                    monitorLog("paper flatten");
                }
            }
        }
        if (latencyShouldLogSummary()) {
            monitorLog(latencySummaryLine());
        }
        return;
    }

    Strategy strat;
    strat.strategize(m, now, book.kalshi_cash, book.polymarket_cash, book.gemini_cash);

    latencyIntent();
    latencyRecordTick();
    if (latencyShouldLogSummary()) {
        monitorLog(latencySummaryLine());
    }

    OrderIntent strat_yes_idea = strat.getYesOrderIntent();
    OrderIntent strat_no_idea = strat.getNoOrderIntent();

    if (strat_yes_idea.size > 0 && strat_no_idea.size > 0) {
        RiskReason reason = approve_pair(strat_yes_idea, strat_no_idea, book, limits, &risk_gate);
        if (reason != RiskReason::Ok) {
            monitorLog("risk rejected: " + ReasonLogger(reason));
            return;
        }
        if (assisiClockOnly()) {
            monitorLog(latencySendLine() + std::string(" src=") + srcName(s.venue));
            monitorLog("clock only, no send");

        } else if (!monitorHalted()) {
            if (assisiLiveOrders() && !assisiReplay()) {
                if (!tryBusy(m.getMarketId())) {
                    monitorLog("risk rejected: " + ReasonLogger(RiskReason::InFlight));
                } else if (!reserve_pair(portfolio, strat_yes_idea, strat_no_idea)) {
                    clearBusy(m.getMarketId());
                    monitorLog("risk rejected: " + ReasonLogger(RiskReason::VenueCash));
                } else {
                    monitorWorkingBegin();
                    monitorLog(latencySendLine() + std::string(" src=") + srcName(s.venue));
                    std::thread([strat_yes_idea, strat_no_idea]() {
                        liveSendPair(strat_yes_idea, strat_no_idea);
                    }).detach();
                }
            } else {
                monitorLog(latencySendLine());
                Execute yes_execute, no_execute;
                yes_execute.Executioner(strat_yes_idea);
                no_execute.Executioner(strat_no_idea);
                Fill yes_fill = yes_execute.getFill();
                Fill no_fill = no_execute.getFill();
                if (yes_fill.size > 0) applyFill(yes_fill);
                if (no_fill.size > 0) applyFill(no_fill);
                Position after = portfolio.getPositionByMarketId(no_fill.market_id);
                monitorLog("paper fill market=" + std::to_string(no_fill.market_id)
                    + " yes " + std::to_string(yes_fill.size) + "@" + moneyStr(yes_fill.price) + " " + srcName(yes_fill.venue)
                    + " no " + std::to_string(no_fill.size) + "@" + moneyStr(no_fill.price) + " " + srcName(no_fill.venue)
                    + " held yes=" + std::to_string(after.yes_count) + " no=" + std::to_string(after.no_count)
                    + " cash=" + moneyStr(portfolio.totalCash()));
            }
        }
    }
}

static int runReplay(const std::string& path, MarketData& markets, bool refresh_ui) {
    std::ifstream tape(path);
    if (!tape) return -1;
    assisiReplayFlag() = true;
    portfolio.setKalshiCash(10000);
    portfolio.setPolymarketCash(10000);
    portfolio.setGeminiCash(10000);
    std::string tline;
    std::getline(tape, tline);
    int ticks = 0;
    while (std::getline(tape, tline)) {
        TapeTick tk;
        if (!tapeParseLine(tline, tk)) continue;
        Market m = markets.getMarketById(tk.market_id);
        if (m.getMarketId() == 0) continue;
        Snapshot snap;
        if (tk.venue == Kalshi) snap = m.getKalshiSnapshot();
        else if (tk.venue == Polymarket) snap = m.getPolymarketSnapshot();
        else if (tk.venue == Gemini) snap = m.getGeminiSnapshot();
        if (snap.venue_id == 0) continue;
        assisiNowOverride() = tk.ts_ms / 1000;
        markets.price_update(snap.venue_id, tk.venue, tk.yes_bid, tk.yes_ask, tk.no_bid, tk.no_ask,
            tk.yes_bid_n, tk.yes_ask_n, tk.no_bid_n, tk.no_ask_n);
        latencyArrive();
        latencyParsed();
        on_tick(markets, snap.venue_id);
        ticks++;
        if (refresh_ui && ticks % 250 == 0) {
            monitorReplayState(true, false, ticks, path);
            monitorRefresh(markets, portfolio, kalshi_ids_flipped, pm_ids_flipped, gem_ids_flipped);
        }
    }
    assisiNowOverride() = 0;
    writeLatencyToCsv("latency.csv");
    return ticks;
}

int main(int argc, char** argv) {
    bool start_now = false;
    std::string record_path;
    std::string replay_path;

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--trade") start_now = true;
        if (a == "--clock") {
            assisiClockOnlyFlag() = true;
            start_now = true;
        }
        if (a == "--record") {
            if (i + 1 < argc && argv[i + 1][0] != '-') record_path = argv[++i];
            else record_path = "ticks.csv";
        }
        if (a == "--replay" && i + 1 < argc) {
            replay_path = argv[++i];
            assisiReplayFlag() = true;
        }
    }
    std::cout << "ASSISI_LIVE=" << assisiLiveOrders() << " clock_only=" << assisiClockOnly()
         << " kalshi_prod=" << kalshiIsProd() << " gemini_sandbox=" << geminiIsSandbox() << "\n";
    limits = loadRiskLimits();
    std::cout << riskLimitsLogLine(limits) << "\n";

    MarketData markets;
    books = &markets;

    std::ifstream data;
    std::string line, tmp, kal_id, pm_id, gem_id;
    long int market_id, expiration;
    std::unordered_map<std::string, long int> kalshi_ids;
    std::unordered_map<std::string, long int> pm_ids;
    std::unordered_map<std::string, long int> gem_ids;
    long int next_vid = 1;

    data.open("markets.csv");
    std::getline(data, line);

    while (std::getline(data, line)) {
        tmp = line;
        for (size_t i = 0; i < tmp.size(); i++) {
            if (tmp[i] == ',') tmp[i] = ' ';
        }
        std::stringstream ss(tmp);
        ss >> market_id >> expiration >> kal_id >> pm_id >> gem_id;
        long int kal_vid = 0, pm_vid = 0, gem_vid = 0;
        if (!kal_id.empty() && kal_id != "0") {
            kal_vid = next_vid++;
            kalshi_ids[kal_id] = kal_vid;
            kalshi_ids_flipped[kal_vid] = kal_id;
        }
        if (!pm_id.empty() && pm_id != "0") {
            pm_vid = next_vid++;
            pm_ids[pm_id] = pm_vid;
            pm_ids_flipped[pm_vid] = pm_id;
        }
        if (!gem_id.empty() && gem_id != "0") {
            gem_vid = next_vid++;
            gem_ids[gem_id] = gem_vid;
            gem_ids_flipped[gem_vid] = gem_id;
        }
        std::cout << "Market_id: " << market_id << " Expiration: " << expiration
             << " Kalshi: " << kal_id << " Polymarket: " << pm_id << " Gemini: " << gem_id << "\n";
        markets.Register(market_id, expiration, kal_vid, pm_vid, gem_vid);
        portfolio.Register(market_id);
    }

    if (!replay_path.empty()) {
        int ticks = runReplay(replay_path, markets, false);
        if (ticks < 0) {
            std::cerr << "could not open tape " << replay_path << "\n";
            return 1;
        }
        Portfolio book = portfolio.getPortfolio();
        std::cout << "replay ticks=" << ticks
             << " cash=" << formatCents(book.kalshi_cash + book.polymarket_cash + book.gemini_cash)
             << " kalshi=" << formatCents(book.kalshi_cash)
             << " polymarket=" << formatCents(book.polymarket_cash)
             << " gemini=" << formatCents(book.gemini_cash) << "\n";
        for (int i = 0; i < book.position_count_; i++) {
            Position p = book.portfolio_[i];
            std::cout << "replay market=" << p.market_id
                 << " yes=" << p.yes_count << " @" << formatCents(p.average_yes_price)
                 << " no=" << p.no_count << " @" << formatCents(p.average_no_price) << "\n";
        }
        return 0;
    }

    monitorStart(8787);
    monitorBind(markets, portfolio, kalshi_ids_flipped, pm_ids_flipped, gem_ids_flipped);
    if (!assisiClockOnly()) {
        applyKalshiLots(kalshi_ids, markets);
        applyPolymarketLots(pm_ids, markets);
        applyGeminiLots(gem_ids, markets);
        refreshVenueCash();
    }
    monitorRefresh(markets, portfolio, kalshi_ids_flipped, pm_ids_flipped, gem_ids_flipped);
    monitorLog("monitor http://127.0.0.1:8787");
    monitorLog(riskLimitsLogLine(limits));

    if (start_now) {
        monitorRequestTrade();
        if (assisiClockOnly()) monitorLog("CLOCK ONLY: feeds on, no orders");
        else monitorLog("trading start from --trade");
    } else {
        monitorLog("ui only: pass --trade, --clock, or choose a mode and click Start");
    }

    while (!monitorTradeRequested()) {
        std::string tape;
        if (monitorReplayRequested(tape)) {
            monitorReplayState(true, false, 0, tape);
            monitorLog("replay start " + tape);
            int ticks = runReplay(tape, markets, true);
            if (ticks < 0) {
                monitorLog("could not open tape " + tape);
                monitorReplayState(false, false, 0, "");
            } else {
                Portfolio book = portfolio.getPortfolio();
                monitorLog("replay done ticks=" + std::to_string(ticks)
                    + " cash=" + moneyStr(book.kalshi_cash + book.polymarket_cash + book.gemini_cash)
                    + " kalshi=" + moneyStr(book.kalshi_cash)
                    + " polymarket=" + moneyStr(book.polymarket_cash)
                    + " gemini=" + moneyStr(book.gemini_cash));
                monitorLog("portfolio holds replay fills; restart the process to trade");
                monitorReplayState(false, true, ticks, tape);
            }
        }
        if (!assisiClockOnly() && !assisiReplay()) refreshVenueCash();
        monitorRefresh(markets, portfolio, kalshi_ids_flipped, pm_ids_flipped, gem_ids_flipped);
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
    }

    monitorSetTrading(true);
    monitorLog("feeds starting");
    if (!record_path.empty()) {
        if (tapeOpen(record_path)) monitorLog(std::string("recording ticks to ") + record_path);
        else monitorLog(std::string("could not open tape ") + record_path);
    }

    std::thread kalshi_th([&markets, &kalshi_ids]() { startKalshiWebsocket(markets, kalshi_ids, on_tick); });
    std::thread pm_th([&markets, &pm_ids]() { startPolymarketWebsocket(markets, pm_ids, on_tick); });
    std::thread gem_th([&markets, &gem_ids]() { startGeminiWebsocket(markets, gem_ids, on_tick); });
    kalshi_th.join();
    pm_th.join();
    gem_th.join();
    return 0;
}
