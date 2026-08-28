#include "monitor.hpp"
#include "strategy.hpp"
#include "kalshi_env.hpp"

#include <fstream>
#include <sstream>
#include <mutex>
#include <memory>
#include <deque>
#include <thread>
#include <chrono>
#include <ctime>
#include <iostream>
#include <atomic>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include <ixwebsocket/IXHttpServer.h>
#include <ixwebsocket/IXNetSystem.h>

static std::mutex mon_mtx;
static std::deque<std::string> mon_log;
static nlohmann::json mon_status = nlohmann::json::object();
static bool mon_working = false;
static bool mon_trading = false;
static std::atomic<bool> mon_trade_req{false};
static std::atomic<bool> mon_halted{false};
static std::unique_ptr<ix::HttpServer> mon_server;
static MarketData* mon_md = nullptr;
static PortfolioManager* mon_pm = nullptr;
static const std::unordered_map<long int, std::string>* mon_kalshi = nullptr;
static const std::unordered_map<long int, std::string>* mon_pmkt = nullptr;
static const std::unordered_map<long int, std::string>* mon_gem = nullptr;

void monitorBind(MarketData& md, PortfolioManager& pm,
    const std::unordered_map<long int, std::string>& kalshi,
    const std::unordered_map<long int, std::string>& polymarket,
    const std::unordered_map<long int, std::string>& gemini) {
    mon_md = &md;
    mon_pm = &pm;
    mon_kalshi = &kalshi;
    mon_pmkt = &polymarket;
    mon_gem = &gemini;
}

void monitorLog(const std::string& line) {
    std::lock_guard<std::mutex> lock(mon_mtx);
    auto t = std::time(nullptr);
    char buf[16];
    std::strftime(buf, sizeof(buf), "%H:%M:%S", std::localtime(&t));
    std::string row = std::string(buf) + "  " + line;
    mon_log.push_front(row);
    while (mon_log.size() > 80) mon_log.pop_back();
    std::cout << row << "\n";
}

void monitorSetWorking(bool v) {
    std::lock_guard<std::mutex> lock(mon_mtx);
    mon_working = v;
}

void monitorSetTrading(bool v) {
    std::lock_guard<std::mutex> lock(mon_mtx);
    mon_trading = v;
}

void monitorRequestTrade() {
    mon_trade_req.store(true);
}

bool monitorTradeRequested() {
    return mon_trade_req.load();
}

void monitorHalt() {
    mon_halted.store(true);
}

bool monitorHalted() {
    return mon_halted.load();
}

void monitorRefresh(MarketData& md, PortfolioManager& pm,
    const std::unordered_map<long int, std::string>& kalshi,
    const std::unordered_map<long int, std::string>& polymarket,
    const std::unordered_map<long int, std::string>& gemini) {
    nlohmann::json j;
    j["live"] = assisiLiveOrders();
    j["clock"] = assisiClockOnly();
    j["halted"] = monitorHalted();
    j["prod"] = kalshiIsProd();
    j["working"] = mon_working;
    Portfolio p = pm.getPortfolio();
    j["cash"]["internal"] = p.cash;
    j["cash"]["kalshi"] = p.kalshi_cash;
    j["cash"]["polymarket"] = p.polymarket_cash;
    j["cash"]["gemini"] = p.gemini_cash;
    j["cash"]["venue_sum"] = p.kalshi_cash + p.polymarket_cash + p.gemini_cash;
    auto name = [](const std::unordered_map<long int, std::string>& m, long int id) {
        auto it = m.find(id);
        return it == m.end() ? std::string() : it->second;
    };
    nlohmann::json books = nlohmann::json::array();
    for (int i = 0; i < md.marketCount(); i++) {
        Market mkt = md.getMarketAt(i);
        Snapshot k = mkt.getKalshiSnapshot();
        Snapshot pmkt = mkt.getPolymarketSnapshot();
        Snapshot g = mkt.getGeminiSnapshot();
        nlohmann::json row;
        row["market_id"] = mkt.getMarketId();
        std::string ticker = name(kalshi, k.venue_id);
        if (ticker.empty()) ticker = name(polymarket, pmkt.venue_id);
        if (ticker.empty()) ticker = name(gemini, g.venue_id);
        row["ticker"] = ticker;
        row["k_yes"] = {k.yes_bid, k.yes_ask};
        row["k_no"] = {k.no_bid, k.no_ask};
        row["k_n"] = {k.yes_bid_n, k.yes_ask_n};
        row["p_yes"] = {pmkt.yes_bid, pmkt.yes_ask};
        row["p_no"] = {pmkt.no_bid, pmkt.no_ask};
        row["p_n"] = {pmkt.yes_bid_n, pmkt.yes_ask_n};
        row["g_yes"] = {g.yes_bid, g.yes_ask};
        row["g_no"] = {g.no_bid, g.no_ask};
        row["g_n"] = {g.yes_bid_n, g.yes_ask_n};
        float yes = 0, no = 0;
        int yv = 0, nv = 0;
        Snapshot snaps[3] = {k, pmkt, g};
        for (int v = 0; v < 3; v++) {
            if (snaps[v].yes_ask != 0 && snaps[v].yes_ask_n >= 1 && (yes == 0 || snaps[v].yes_ask < yes)) { yes = snaps[v].yes_ask; yv = snaps[v].venue; }
            if (snaps[v].no_ask != 0 && snaps[v].no_ask_n >= 1 && (no == 0 || snaps[v].no_ask < no)) { no = snaps[v].no_ask; nv = snaps[v].venue; }
        }
        float fees = (yes != 0 && no != 0) ? takerFee(yv, yes, 1) + takerFee(nv, no, 1) : 0;
        float pair = yes + no + fees;
        row["best_yes"] = yes;
        row["best_no"] = no;
        row["pair"] = pair;
        row["edge"] = (yes != 0 && no != 0) ? (1.0f - pair) : 0;
        row["expired"] = (mkt.getExpirationDate() <= static_cast<long int>(std::time(nullptr)));
        row["yv"] = yv;
        row["nv"] = nv;
        books.push_back(row);
    }
    j["books"] = books;
    nlohmann::json pos = nlohmann::json::array();
    for (int i = 0; i < p.position_count_; i++) {
        nlohmann::json pr;
        pr["market_id"] = p.portfolio_[i].market_id;
        Market mk = md.getMarketById(p.portfolio_[i].market_id);
        std::string ticker = name(kalshi, mk.getKalshiSnapshot().venue_id);
        if (ticker.empty()) ticker = name(polymarket, mk.getPolymarketSnapshot().venue_id);
        if (ticker.empty()) ticker = name(gemini, mk.getGeminiSnapshot().venue_id);
        pr["ticker"] = ticker;
        pr["yes"] = p.portfolio_[i].yes_count;
        pr["no"] = p.portfolio_[i].no_count;
        pr["avg_yes"] = p.portfolio_[i].average_yes_price;
        pr["avg_no"] = p.portfolio_[i].average_no_price;
        pos.push_back(pr);
    }
    j["positions"] = pos;
    nlohmann::json logs = nlohmann::json::array();
    {
        std::lock_guard<std::mutex> lock(mon_mtx);
        j["working"] = mon_working;
        j["trading"] = mon_trading;
        for (auto& line : mon_log) logs.push_back(line);
    }
    j["log"] = logs;
    std::lock_guard<std::mutex> lock(mon_mtx);
    mon_status = std::move(j);
}

static std::string loadUi() {
    std::ifstream f("ui/index.html");
    if (!f) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

void monitorStart(int port) {
    ix::initNetSystem();
    mon_server.reset(new ix::HttpServer(port, "127.0.0.1"));
    mon_server->setOnConnectionCallback(
        [](ix::HttpRequestPtr request, std::shared_ptr<ix::ConnectionState>) -> ix::HttpResponsePtr {
            ix::WebSocketHttpHeaders headers;
            headers["Access-Control-Allow-Origin"] = "*";
            if (request->method == "OPTIONS") {
                headers["Access-Control-Allow-Methods"] = "GET, POST, OPTIONS";
                headers["Access-Control-Allow-Headers"] = "Content-Type";
                return std::make_shared<ix::HttpResponse>(204, "No Content", ix::HttpErrorCode::Ok, headers, "");
            }
            std::string uri = request->uri;
            auto q = uri.find('?');
            if (q != std::string::npos) uri = uri.substr(0, q);
            if (uri == "/api/status") {
                headers["Content-Type"] = "application/json";
                if (mon_md && mon_pm && mon_kalshi && mon_pmkt && mon_gem) {
                    monitorRefresh(*mon_md, *mon_pm, *mon_kalshi, *mon_pmkt, *mon_gem);
                }
                std::string body;
                {
                    std::lock_guard<std::mutex> lock(mon_mtx);
                    body = mon_status.dump();
                }
                return std::make_shared<ix::HttpResponse>(200, "OK", ix::HttpErrorCode::Ok, headers, body);
            }
            if (uri == "/api/stop" && (request->method == "POST" || request->method == "GET")) {
                headers["Content-Type"] = "application/json";
                monitorHalt();
                monitorLog("STOP — no new pairs");
                return std::make_shared<ix::HttpResponse>(200, "OK", ix::HttpErrorCode::Ok, headers, "{\"ok\":true}");
            }
            if (uri == "/api/start" && (request->method == "POST" || request->method == "GET")) {
                headers["Content-Type"] = "application/json";
                if (mon_trading) {
                    return std::make_shared<ix::HttpResponse>(200, "OK", ix::HttpErrorCode::Ok, headers, "{\"ok\":true,\"already\":true}");
                }
                monitorRequestTrade();
                monitorLog("start requested from UI");
                return std::make_shared<ix::HttpResponse>(200, "OK", ix::HttpErrorCode::Ok, headers, "{\"ok\":true}");
            }
            headers["Content-Type"] = "text/html; charset=utf-8";
            std::string html = loadUi();
            if (html.empty()) {
                html = "<html><body style='background:#0b1610;color:#c9a227;font-family:serif'>ASSISI UI missing ui/index.html</body></html>";
            }
            return std::make_shared<ix::HttpResponse>(200, "OK", ix::HttpErrorCode::Ok, headers, html);
        });
    auto res = mon_server->listen();
    if (!res.first) {
        std::cout << "monitor listen failed\n";
        return;
    }
    mon_server->start();
    monitorLog(std::string("monitor http://127.0.0.1:") + std::to_string(port));
}
