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

using namespace std;

PortfolioManager test_manager;
RiskLimits limits{0.70, 0.80, 0.10, 100};
unordered_map<long int, std::string> kalshi_ids_flipped;
unordered_map<long int, std::string> pm_ids_flipped;
unordered_map<long int, std::string> gem_ids_flipped;
unordered_set<long int> expired_logged;
bool live_working = false;
MarketData* books = nullptr;

static const char* srcName(int venue) {
    if (venue == Kalshi) return "kalshi";
    if (venue == Polymarket) return "polymarket";
    if (venue == Gemini) return "gemini";
    return "?";
}

bool pairLocked(const OrderIntent& yes, const OrderIntent& no) {
    float fees = takerFee(yes.venue, yes.limit_price, yes.size) + takerFee(no.venue, no.limit_price, no.size);
    return (yes.size * (yes.limit_price + no.limit_price) + fees) < static_cast<float>(yes.size);
}

void bumpLimit(OrderIntent& idea) {
    idea.limit_price = std::round((idea.limit_price + 0.01f) * 100.0f) / 100.0f;
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
    float fill_count = 0;
    float fill_price = 0;
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
    monitorLog(latField(http_clock, latUs(k0, std::chrono::steady_clock::now())));
    if (!ok) return false;
    if (fill_price > 0) leg.limit_price = fill_price;
    Fill f;
    f.market_id = leg.market_id;
    f.venue_id = leg.venue_id;
    f.venue = leg.venue;
    f.side = leg.side;
    f.size = static_cast<int>(fill_count > 0 ? fill_count : leg.size);
    f.price = fill_price > 0 ? fill_price : leg.limit_price;
    f.buy = true;
    test_manager.updatePortfolio(f);
    return true;
}

bool missingLegIntent(Market m, Position pos, OrderIntent& out) {
    out.market_id = m.getMarketId();
    out.buy = true;
    out.size = std::abs(pos.yes_count - pos.no_count);
    out.venue = 0;
    out.venue_id = 0;
    out.limit_price = 0;
    int book_n = 0;

    Snapshot snaps[3] = { m.getKalshiSnapshot(), m.getPolymarketSnapshot(), m.getGeminiSnapshot() };
    if (pos.yes_count > pos.no_count) {
        out.side = NoSided;
        for (int i = 0; i < 3; i++) {
            if (snaps[i].venue_id == 0 || snaps[i].no_ask == 0 || snaps[i].no_ask_n < 1) continue;
            if (out.venue == 0 || snaps[i].no_ask < out.limit_price) {
                out.venue = snaps[i].venue;
                out.venue_id = snaps[i].venue_id;
                out.limit_price = snaps[i].no_ask;
                book_n = snaps[i].no_ask_n;
            }
        }
    } else {
        out.side = YesSided;
        for (int i = 0; i < 3; i++) {
            if (snaps[i].venue_id == 0 || snaps[i].yes_ask == 0 || snaps[i].yes_ask_n < 1) continue;
            if (out.venue == 0 || snaps[i].yes_ask < out.limit_price) {
                out.venue = snaps[i].venue;
                out.venue_id = snaps[i].venue_id;
                out.limit_price = snaps[i].yes_ask;
                book_n = snaps[i].yes_ask_n;
            }
        }
    }

    if(out.size > book_n) { out.size = book_n; }
    
    return out.venue != 0 && out.limit_price > 0;
}

void liveSendLeg(OrderIntent leg) {
    auto t0 = std::chrono::steady_clock::now();
    live_working = true;
    monitorSetWorking(true);
    monitorLog("missing leg start");
    bool filled = false;
    const char* clock = leg.venue == Kalshi ? "kalshi_http_us" : (leg.venue == Polymarket ? "pm_http_us" : "gem_http_us");
    for (int i = 0; i < 15; i++) {
        if (leg.limit_price >= 0.99f) break;
        if (sendVenueOrder(leg, clock)) {
            monitorLog(std::string("missing ") + srcName(leg.venue) + " filled");
            filled = true;
            break;
        }
        monitorLog(std::string("missing ") + srcName(leg.venue) + " miss, chase +0.01");
        bumpLimit(leg);
    }
    if (!filled) monitorLog("missing leg unfilled");
    monitorLog(latField("missing_total_us", latUs(t0, std::chrono::steady_clock::now())));
    refreshVenueCash();
    live_working = false;
    monitorSetWorking(false);
}

void refreshVenueCash() {
    float k = 0, p = 0, g = 0;
    if (refreshKalshiBalance(k)) test_manager.setKalshiCash(k);
    if (refreshPolymarketBalance(p)) test_manager.setPolymarketCash(p);
    if (refreshGeminiBalance(g)) test_manager.setGeminiCash(g);
}

void applyKalshiLots(const unordered_map<std::string, long int>& kalshi_ids, MarketData& md) {
    vector<KalshiLot> lots;
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
        test_manager.addVenueLot(s.market_id, s.venue, lot.yes_count, lot.no_count, lot.avg_yes, lot.avg_no);
        n++;
        if (lot.yes_count > 0) monitorLog(std::string("reconcile kalshi YES ") + lot.ticker);
        if (lot.no_count > 0) monitorLog(std::string("reconcile kalshi NO ") + lot.ticker);
    }
    if (n == 0) monitorLog("kalshi positions empty in universe");
}

void applyPolymarketLots(const unordered_map<std::string, long int>& ids, MarketData& md) {
    vector<PolymarketLot> lots;
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
        test_manager.addVenueLot(s.market_id, s.venue, lot.yes_count, lot.no_count, lot.avg_yes, lot.avg_no);
        n++;
        if (lot.yes_count > 0) monitorLog(std::string("reconcile polymarket YES ") + lot.slug);
        if (lot.no_count > 0) monitorLog(std::string("reconcile polymarket NO ") + lot.slug);
    }
    if (n == 0) monitorLog("polymarket positions empty in universe");
}

void applyGeminiLots(const unordered_map<std::string, long int>& ids, MarketData& md) {
    vector<GeminiLot> lots;
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
        test_manager.addVenueLot(s.market_id, s.venue, lot.yes_count, lot.no_count, lot.avg_yes, lot.avg_no);
        n++;
        if (lot.yes_count > 0) monitorLog(std::string("reconcile gemini YES ") + lot.symbol);
        if (lot.no_count > 0) monitorLog(std::string("reconcile gemini NO ") + lot.symbol);
    }
    if (n == 0) monitorLog("gemini positions empty in universe");
}

void liveSendPair(OrderIntent yes, OrderIntent no) {
    auto pair0 = std::chrono::steady_clock::now();
    live_working = true;
    monitorSetWorking(true);
    monitorLog("live pair start");

    auto sendSide = [&](OrderIntent& leg, OrderIntent& other, bool first) -> bool {
        const char* clock = leg.venue == Kalshi ? "kalshi_http_us" : (leg.venue == Polymarket ? "pm_http_us" : "gem_http_us");
        for (int i = 0; i < 15; i++) {
            if (!pairLocked(yes, no)) {
                monitorLog("chase stopped: pair no longer locked");
                return false;
            }
            if (sendVenueOrder(leg, clock)) {
                monitorLog(std::string(srcName(leg.venue)) + " filled");
                if (leg.side == YesSided) yes = leg; else no = leg;
                return true;
            }
            monitorLog(std::string(srcName(leg.venue)) + " miss, chase +0.01");
            bumpLimit(leg);
            if (leg.side == YesSided) yes = leg; else no = leg;
            (void)other;
            (void)first;
        }
        return false;
    };

    OrderIntent* first = &yes;
    OrderIntent* second = &no;
    if (no.venue == Kalshi && yes.venue != Kalshi) {
        first = &no;
        second = &yes;
    }
    if (!sendSide(*first, *second, true)) {
        monitorLog("skip second: first leg not filled");
        monitorLog(latField("pair_total_us", latUs(pair0, std::chrono::steady_clock::now())));
        live_working = false;
        monitorSetWorking(false);
        refreshVenueCash();
        return;
    }
    sendSide(*second, *first, false);
    refreshVenueCash();
    live_working = false;
    monitorSetWorking(false);
    monitorLog(latField("pair_total_us", latUs(pair0, std::chrono::steady_clock::now())));
    monitorLog("live pair done");
}

static Snapshot snapshotForTick(MarketData& md, long int venue_id) {
    Snapshot s = md.getSnapshotByVenueId(venue_id, Kalshi);
    if (s.market_id == 0) s = md.getSnapshotByVenueId(venue_id, Polymarket);
    if (s.market_id == 0) s = md.getSnapshotByVenueId(venue_id, Gemini);
    return s;
}

void on_tick(MarketData& md, long int venue_id) {
    Snapshot s = snapshotForTick(md, venue_id);
    Market m = md.getMarketById(s.market_id);

    if (tapeOut().is_open()) tapeWrite(s);

    long int now = assisiNow();
    if (m.getMarketId() != 0 && m.getExpirationDate() <= now) {
        if (expired_logged.insert(m.getMarketId()).second) {
            monitorLog("expired, dropped from trading");
        }
        return;
    }

    Position pos = test_manager.getPositionByMarketId(m.getMarketId());
    if (pos.yes_count != pos.no_count) {
        OrderIntent miss;
        if (missingLegIntent(m, pos, miss)) {
            latencyIntent();
            latencyRecordTick();
            if (assisiClockOnly()) {
                monitorLog(latencySendLine() + " missing-leg");
                monitorLog("clock only, no send");
            } else if (!monitorHalted() && !live_working) {
                if (assisiLiveOrders() && !assisiReplay()) {
                    monitorLog(latencySendLine() + " missing-leg");
                    std::thread([miss]() { liveSendLeg(miss); }).detach();
                } else {
                    Execute ex;
                    ex.Executioner(miss);
                    Fill f = ex.getFill();
                    if (f.size > 0) test_manager.updatePortfolio(f);
                    monitorLog("missing leg paper fill");
                }
            }
        } else {
            latencyIntent();
            latencyRecordTick();
        }
        if (latencyShouldLogSummary()) {
            monitorLog(latencySummaryLine());
        }
        return;
    }

    Strategy strat;
    strat.strategize(m, now, test_manager.kalshiCash(), test_manager.polymarketCash(), test_manager.geminiCash());

    latencyIntent();
    latencyRecordTick();
    if (latencyShouldLogSummary()) {
        monitorLog(latencySummaryLine());
    }

    OrderIntent strat_yes_idea = strat.getYesOrderIntent();
    OrderIntent strat_no_idea = strat.getNoOrderIntent();

    if (strat_yes_idea.size > 0 && strat_no_idea.size > 0) {
        RiskReason reason = approve_pair(strat_yes_idea, strat_no_idea, test_manager, limits);
        if(!(reason == RiskReason::Ok)) {
            cout << "Risk rejected: " << ReasonLogger(reason) << "\n";
            return;
        }
        if (assisiClockOnly()) {
            monitorLog(latencySendLine() + std::string(" src=") + srcName(s.venue));
            monitorLog("clock only, no send");

        } else if (!monitorHalted()) {
            if (assisiLiveOrders() && !assisiReplay()) {
                if (!live_working) {
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
                cout << "YES FILLED: " << yes_fill.size << " x $" << yes_fill.price << " VENUE: " << yes_fill.venue << "\n";
                cout << "NO FILLED: " << no_fill.size << " x $" << no_fill.price << " VENUE: " << no_fill.venue << "\n";
                if (yes_fill.size > 0) test_manager.updatePortfolio(yes_fill);
                if (no_fill.size > 0) test_manager.updatePortfolio(no_fill);
                Portfolio test_book = test_manager.getPortfolio();
                cout << "CASH BALANCE: $" << test_book.cash << " YESs: " << test_manager.getPositionByMarketId(yes_fill.market_id).yes_count << " NOs: " << test_manager.getPositionByMarketId(no_fill.market_id).no_count << " MARKET: " << no_fill.market_id << "\n";
            }
            
        }
    }
}

int main(int argc, char** argv) {
    cout << std::unitbuf;
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
    cout << "ASSISI_LIVE=" << assisiLiveOrders() << " clock_only=" << assisiClockOnly()
         << " kalshi_prod=" << kalshiIsProd() << " gemini_sandbox=" << geminiIsSandbox() << "\n";

    MarketData test;
    books = &test;

    ifstream data;
    string line, tmp, kal_id, pm_id, gem_id;
    long int market_id, expiration;
    unordered_map<std::string, long int> kalshi_ids;
    unordered_map<std::string, long int> pm_ids;
    unordered_map<std::string, long int> gem_ids;
    long int next_vid = 1;

    data.open("markets.csv");
    getline(data, line);

    while (getline(data, line)) {
        tmp = line;
        for (size_t i = 0; i < tmp.size(); i++) {
            if (tmp[i] == ',') tmp[i] = ' ';
        }
        stringstream ss(tmp);
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
        cout << "Market_id: " << market_id << " Expiration: " << expiration
             << " Kalshi: " << kal_id << " Polymarket: " << pm_id << " Gemini: " << gem_id << "\n";
        test.Register(market_id, expiration, kal_vid, pm_vid, gem_vid);
        test_manager.Register(market_id);
    }

    if (!replay_path.empty()) {
        test_manager.setKalshiCash(100);
        test_manager.setPolymarketCash(100);
        test_manager.setGeminiCash(100);
        ifstream tape(replay_path);
        if (!tape) {
            cerr << "could not open tape " << replay_path << "\n";
            return 1;
        }
        string tline;
        getline(tape, tline);
        int ticks = 0;
        while (getline(tape, tline)) {
            TapeTick tk;
            if (!tapeParseLine(tline, tk)) continue;
            Market m = test.getMarketById(tk.market_id);
            if (m.getMarketId() == 0) continue;
            Snapshot snap;
            if (tk.venue == Kalshi) snap = m.getKalshiSnapshot();
            else if (tk.venue == Polymarket) snap = m.getPolymarketSnapshot();
            else if (tk.venue == Gemini) snap = m.getGeminiSnapshot();
            if (snap.venue_id == 0) continue;
            assisiNowOverride() = tk.ts_ms / 1000;
            test.price_update(snap.venue_id, tk.venue, tk.yes_bid, tk.yes_ask, tk.no_bid, tk.no_ask,
                tk.yes_bid_n, tk.yes_ask_n, tk.no_bid_n, tk.no_ask_n);
            on_tick(test, snap.venue_id);
            ticks++;
        }
        Portfolio book = test_manager.getPortfolio();
        cout << "replay ticks=" << ticks
             << " cash=" << book.cash
             << " kalshi=" << book.kalshi_cash
             << " polymarket=" << book.polymarket_cash
             << " gemini=" << book.gemini_cash << "\n";
        for (int i = 0; i < book.position_count_; i++) {
            Position p = book.portfolio_[i];
            cout << "replay market=" << p.market_id
                 << " yes=" << p.yes_count << " @" << p.average_yes_price
                 << " no=" << p.no_count << " @" << p.average_no_price << "\n";
        }
        return 0;
    }

    monitorStart(8787);
    monitorBind(test, test_manager, kalshi_ids_flipped, pm_ids_flipped, gem_ids_flipped);
    if (!assisiClockOnly()) {
        applyKalshiLots(kalshi_ids, test);
        applyPolymarketLots(pm_ids, test);
        applyGeminiLots(gem_ids, test);
        refreshVenueCash();
    }
    monitorRefresh(test, test_manager, kalshi_ids_flipped, pm_ids_flipped, gem_ids_flipped);
    monitorLog("monitor http://127.0.0.1:8787");

    if (start_now) {
        monitorRequestTrade();
        if (assisiClockOnly()) monitorLog("CLOCK ONLY — feeds on, no orders");
        else monitorLog("trading start from --trade");
    } else {
        monitorLog("ui only — pass --trade, --clock, or click START");
    }

    while (!monitorTradeRequested()) {
        if (!assisiClockOnly()) refreshVenueCash();
        monitorRefresh(test, test_manager, kalshi_ids_flipped, pm_ids_flipped, gem_ids_flipped);
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
    }

    monitorSetTrading(true);
    monitorLog("feeds starting");
    if (!record_path.empty()) {
        if (tapeOpen(record_path)) monitorLog(std::string("recording ticks to ") + record_path);
        else monitorLog(std::string("could not open tape ") + record_path);
    }

    std::thread kalshi_th([&test, &kalshi_ids]() { startKalshiWebsocket(test, kalshi_ids, on_tick); });
    std::thread pm_th([&test, &pm_ids]() { startPolymarketWebsocket(test, pm_ids, on_tick); });
    std::thread gem_th([&test, &gem_ids]() { startGeminiWebsocket(test, gem_ids, on_tick); });
    kalshi_th.join();
    pm_th.join();
    gem_th.join();
    return 0;
}
