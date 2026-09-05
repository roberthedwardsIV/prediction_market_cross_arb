#include "monitor.hpp"
#include "strategy.hpp"
#include "kalshi_env.hpp"

#include <fstream>
#include <sstream>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <deque>
#include <thread>
#include <chrono>
#include <ctime>
#include <iostream>
#include <atomic>
#include <unordered_map>
#include <filesystem>
#include <vector>
#include <algorithm>
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
static std::atomic<bool> mon_replay_req{false};
static std::string mon_replay_tape;
static bool mon_replay_running = false;
static bool mon_replay_done = false;
static int mon_replay_ticks = 0;
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

namespace {

struct PendingLine {
    std::time_t t;
    std::string text;
};

struct LogWorker {
    std::mutex mtx;
    std::condition_variable cv;
    std::deque<PendingLine> pending;
    bool stop = false;
    std::thread th;

    LogWorker() : th([this]() { run(); }) {}

    ~LogWorker() {
        {
            std::lock_guard<std::mutex> lock(mtx);
            stop = true;
        }
        cv.notify_one();
        if (th.joinable()) th.join();
    }

    void push(std::time_t t, const std::string& text) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            pending.push_back(PendingLine{t, text});
        }
        cv.notify_one();
    }

    void emit(const PendingLine& p) {
        char buf[16];
        std::tm tm_buf{};
        localtime_r(&p.t, &tm_buf);
        std::strftime(buf, sizeof(buf), "%H:%M:%S", &tm_buf);
        std::string row = std::string(buf) + "  " + p.text;
        {
            std::lock_guard<std::mutex> lock(mon_mtx);
            mon_log.push_front(row);
            while (mon_log.size() > 80) mon_log.pop_back();
        }
        std::cout << row << "\n";
    }

    void run() {
        std::deque<PendingLine> batch;
        while (true) {
            {
                std::unique_lock<std::mutex> lock(mtx);
                cv.wait(lock, [this]() { return stop || !pending.empty(); });
                batch.swap(pending);
                if (batch.empty() && stop) break;
            }
            for (const auto& p : batch) emit(p);
            batch.clear();
            std::cout.flush();
        }
    }
};

LogWorker& logWorker() {
    static LogWorker w;
    return w;
}

}

void monitorLog(const std::string& line) {
    logWorker().push(std::time(nullptr), line);
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

void monitorRequestReplay(const std::string& tape) {
    {
        std::lock_guard<std::mutex> lock(mon_mtx);
        mon_replay_tape = tape;
    }
    mon_replay_req.store(true);
}

bool monitorReplayRequested(std::string& tape_out) {
    if (!mon_replay_req.exchange(false)) return false;
    std::lock_guard<std::mutex> lock(mon_mtx);
    tape_out = mon_replay_tape;
    return true;
}

void monitorReplayState(bool running, bool done, int ticks, const std::string& tape) {
    std::lock_guard<std::mutex> lock(mon_mtx);
    mon_replay_running = running;
    mon_replay_done = done;
    mon_replay_ticks = ticks;
    mon_replay_tape = tape;
}

static bool safeTapeName(const std::string& name) {
    if (name.empty() || name.size() > 200) return false;
    if (name.find('/') != std::string::npos || name.find('\\') != std::string::npos) return false;
    if (name.find("..") != std::string::npos) return false;
    return name.size() > 4 && name.compare(name.size() - 4, 4, ".csv") == 0;
}

static std::vector<std::string> listTapes() {
    std::vector<std::string> out;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(".", ec)) {
        if (!entry.is_regular_file()) continue;
        std::string name = entry.path().filename().string();
        if (!safeTapeName(name)) continue;
        if (name.rfind("markets", 0) == 0) continue;
        if (name.rfind("latency", 0) == 0) continue;
        if (name.rfind("bench_latency", 0) == 0) continue;
        out.push_back(name);
    }
    std::sort(out.begin(), out.end());
    return out;
}

static std::string percentDecode(const std::string& in) {
    std::string out;
    for (size_t i = 0; i < in.size(); i++) {
        if (in[i] == '%' && i + 2 < in.size()) {
            out.push_back(static_cast<char>(std::stoi(in.substr(i + 1, 2), nullptr, 16)));
            i += 2;
        } else if (in[i] == '+') {
            out.push_back(' ');
        } else {
            out.push_back(in[i]);
        }
    }
    return out;
}

void monitorRefresh(MarketData& md, PortfolioManager& pm,
    const std::unordered_map<long int, std::string>& kalshi,
    const std::unordered_map<long int, std::string>& polymarket,
    const std::unordered_map<long int, std::string>& gemini) {
    nlohmann::json j;
    j["live"] = assisiLiveOrders();
    j["live_allowed"] = assisiLiveAllowed();
    j["clock"] = assisiClockOnly();
    j["halted"] = monitorHalted();
    j["prod"] = kalshiIsProd();
    j["working"] = mon_working;
    Portfolio p = pm.getPortfolio();
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
        row["k_yes_n"] = k.yes_ask_n;
        row["k_no_n"] = k.no_ask_n;
        row["p_yes"] = {pmkt.yes_bid, pmkt.yes_ask};
        row["p_no"] = {pmkt.no_bid, pmkt.no_ask};
        row["p_yes_n"] = pmkt.yes_ask_n;
        row["p_no_n"] = pmkt.no_ask_n;
        row["g_yes"] = {g.yes_bid, g.yes_ask};
        row["g_no"] = {g.no_bid, g.no_ask};
        row["g_yes_n"] = g.yes_ask_n;
        row["g_no_n"] = g.no_ask_n;
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
        j["replay"] = mon_replay_running;
        j["replay_done"] = mon_replay_done;
        j["replay_ticks"] = mon_replay_ticks;
        j["replay_tape"] = mon_replay_tape;
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
            std::string query;
            auto q = uri.find('?');
            if (q != std::string::npos) {
                query = uri.substr(q + 1);
                uri = uri.substr(0, q);
            }
            auto queryParam = [&](const std::string& key) {
                std::string needle = key + "=";
                size_t p = 0;
                while (p < query.size()) {
                    size_t e = query.find('&', p);
                    if (e == std::string::npos) e = query.size();
                    std::string kv = query.substr(p, e - p);
                    if (kv.compare(0, needle.size(), needle) == 0) return kv.substr(needle.size());
                    p = e + 1;
                }
                return std::string();
            };
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
            if (uri == "/api/tapes" && request->method == "GET") {
                headers["Content-Type"] = "application/json";
                nlohmann::json t;
                t["tapes"] = listTapes();
                return std::make_shared<ix::HttpResponse>(200, "OK", ix::HttpErrorCode::Ok, headers, t.dump());
            }
            if (uri == "/api/start" && (request->method == "POST" || request->method == "GET")) {
                headers["Content-Type"] = "application/json";
                if (mon_trading) {
                    return std::make_shared<ix::HttpResponse>(200, "OK", ix::HttpErrorCode::Ok, headers, "{\"ok\":true,\"already\":true}");
                }
                std::string mode = queryParam("mode");
                if (mode.empty()) mode = "paper";
                bool replay_busy, replay_done;
                {
                    std::lock_guard<std::mutex> lock(mon_mtx);
                    replay_busy = mon_replay_running;
                    replay_done = mon_replay_done;
                }
                if (replay_busy) {
                    return std::make_shared<ix::HttpResponse>(409, "Conflict", ix::HttpErrorCode::Ok, headers, "{\"ok\":false,\"error\":\"replay in progress\"}");
                }
                if (mode == "replay") {
                    std::string tape = percentDecode(queryParam("tape"));
                    if (!safeTapeName(tape)) {
                        return std::make_shared<ix::HttpResponse>(400, "Bad Request", ix::HttpErrorCode::Ok, headers, "{\"ok\":false,\"error\":\"tape must be a .csv in the working directory\"}");
                    }
                    if (!std::filesystem::is_regular_file(tape)) {
                        return std::make_shared<ix::HttpResponse>(404, "Not Found", ix::HttpErrorCode::Ok, headers, "{\"ok\":false,\"error\":\"tape not found\"}");
                    }
                    monitorRequestReplay(tape);
                    monitorLog("replay requested from UI, tape=" + tape);
                    return std::make_shared<ix::HttpResponse>(200, "OK", ix::HttpErrorCode::Ok, headers, "{\"ok\":true}");
                }
                if (replay_done) {
                    monitorLog("start refused: portfolio holds replay fills, restart the process to trade");
                    return std::make_shared<ix::HttpResponse>(409, "Conflict", ix::HttpErrorCode::Ok, headers, "{\"ok\":false,\"error\":\"restart the process to trade after a replay\"}");
                }
                if (mode == "live") {
                    if (!assisiLiveAllowed()) {
                        monitorLog("live start refused: ASSISI_LIVE is not 1");
                        return std::make_shared<ix::HttpResponse>(403, "Forbidden", ix::HttpErrorCode::Ok, headers, "{\"ok\":false,\"error\":\"ASSISI_LIVE must be 1\"}");
                    }
                    assisiClockOnlyFlag() = false;
                    assisiLiveOverride() = 1;
                } else if (mode == "clock") {
                    assisiClockOnlyFlag() = true;
                    assisiLiveOverride() = 0;
                } else if (mode == "paper") {
                    assisiClockOnlyFlag() = false;
                    assisiLiveOverride() = 0;
                } else {
                    return std::make_shared<ix::HttpResponse>(400, "Bad Request", ix::HttpErrorCode::Ok, headers, "{\"ok\":false,\"error\":\"mode must be paper, clock, live, or replay\"}");
                }
                monitorRequestTrade();
                monitorLog("start requested from UI, mode=" + mode);
                return std::make_shared<ix::HttpResponse>(200, "OK", ix::HttpErrorCode::Ok, headers, "{\"ok\":true}");
            }
            headers["Content-Type"] = "text/html; charset=utf-8";
            std::string html = loadUi();
            if (html.empty()) {
                html = "<html><body style='background:#eef0f2;color:#1e2328;font-family:Segoe UI,Tahoma,sans-serif;padding:16px'>Assisi UI missing ui/index.html (run from repo root).</body></html>";
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
