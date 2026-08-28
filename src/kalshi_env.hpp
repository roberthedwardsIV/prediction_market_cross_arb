#pragma once

#include <cstdlib>
#include <fstream>
#include <string>
#include <atomic>
#include <chrono>
#include <ctime>

inline std::string envValue(const std::string& key) {
    auto trim = [](std::string v) {
        while (!v.empty() && (v.back() == '\r' || v.back() == '\n' || v.back() == ' ' || v.back() == '\t')) {
            v.pop_back();
        }
        if (v.size() >= 2 && ((v.front() == '"' && v.back() == '"') || (v.front() == '\'' && v.back() == '\''))) {
            v = v.substr(1, v.size() - 2);
        }
        return v;
    };
    if (const char* v = std::getenv(key.c_str())) {
        return trim(v);
    }
    std::ifstream f(".env");
    std::string line;
    while (std::getline(f, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        if (line.substr(0, eq) == key) {
            return trim(line.substr(eq + 1));
        }
    }
    return "";
}

inline long long nextGeminiNonce() {
    static std::atomic<long long> last{0};
    long long now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    long long prev = last.load();
    while (true) {
        long long cand = now > prev ? now : prev + 1;
        if (last.compare_exchange_weak(prev, cand)) return cand;
    }
}

inline bool kalshiIsProd() {
    return envValue("KALSHI_ENV") == "prod";
}

inline bool assisiLiveOrders() {
    return envValue("ASSISI_LIVE") == "1";
}

inline bool& assisiClockOnlyFlag() {
    static bool v = false;
    return v;
}

inline bool assisiClockOnly() {
    return assisiClockOnlyFlag();
}

inline bool& assisiReplayFlag() {
    static bool v = false;
    return v;
}

inline bool assisiReplay() {
    return assisiReplayFlag();
}

inline long int& assisiNowOverride() {
    static long int v = 0;
    return v;
}

inline long int assisiNow() {
    if (assisiNowOverride() != 0) return assisiNowOverride();
    return static_cast<long int>(std::time(nullptr));
}

inline std::string unixMsUtc(long long ms) {
    std::time_t s = static_cast<std::time_t>(ms / 1000);
    std::tm* utc = std::gmtime(&s);
    if (!utc) return "?";
    char buf[40];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S UTC", utc);
    return buf;
}

inline bool geminiIsSandbox() {
    return envValue("GEMINI_ENV") == "sandbox";
}

inline std::string polymarketRestBase() {
    return "https://api.polymarket.us";
}

inline std::string polymarketWsMarketsUrl() {
    return "wss://api.polymarket.us/v1/ws/markets";
}

inline std::string geminiRestBase() {
    if (geminiIsSandbox()) {
        return "https://api.sandbox.gemini.com";
    }
    return "https://api.gemini.com";
}

inline std::string geminiWsHost() {
    if (geminiIsSandbox()) {
        return "api.sandbox.gemini.com";
    }
    return "ws.gemini.com";
}

inline std::string geminiWsUrl() {
    if (geminiIsSandbox()) {
        return "wss://api.sandbox.gemini.com";
    }
    return "wss://ws.gemini.com";
}

inline std::string kalshiRestBase() {
    if (kalshiIsProd()) {
        return "https://external-api.kalshi.com";
    }
    return "https://external-api.demo.kalshi.co";
}

inline std::string kalshiRestOrderUrl() {
    return kalshiRestBase() + "/trade-api/v2/portfolio/events/orders";
}

inline std::string kalshiWsUrl() {
    if (kalshiIsProd()) {
        return "wss://external-api-ws.kalshi.com/trade-api/ws/v2";
    }
    return "wss://external-api-ws.demo.kalshi.co/trade-api/ws/v2";
}

inline std::string kalshiWsHost() {
    if (kalshiIsProd()) {
        return "external-api-ws.kalshi.com";
    }
    return "external-api-ws.demo.kalshi.co";
}

inline std::string kalshiWsOrigin() {
    if (kalshiIsProd()) {
        return "https://kalshi.com";
    }
    return "https://demo.kalshi.co";
}
