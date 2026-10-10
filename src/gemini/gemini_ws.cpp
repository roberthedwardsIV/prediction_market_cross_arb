#include "gemini_ws.hpp"
#include "kalshi_env.hpp"
#include "latency.hpp"
#include "json_find.hpp"
#include "money.hpp"
#include "ws_reconnect.hpp"
#include "monitor.hpp"

#include <iostream>
#include <chrono>
#include <thread>
#include <string>
#include <cctype>
#include <unordered_map>
#include <atomic>

#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>
#include <nlohmann/json.hpp>

static std::string geminiUpper(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

bool startGeminiWebsocket(MarketData& md, std::unordered_map<std::string, long int>& ids, void (*on_tick)(MarketData&, long int)) {
    if (ids.empty()) return true;

    std::unordered_map<std::string, long int> folded;
    for (const auto& kv : ids) folded[geminiUpper(kv.first)] = kv.second;

    ix::initNetSystem();
    std::atomic<int> backoff_ms{1000};
    while (true) {
        ix::WebSocket webSocket;
        webSocket.setUrl(geminiWsUrl() + "/");
        webSocket.disablePerMessageDeflate();
        webSocket.disableAutomaticReconnection();
        webSocket.setHandshakeTimeout(15);
        ix::WebSocketHttpHeaders headers;
        headers["Host"] = geminiWsHost();
        headers["Origin"] = "https://gemini.com";
        webSocket.setExtraHeaders(headers);
        webSocket.setOnMessageCallback([&webSocket, &md, &folded, on_tick, &backoff_ms](const ix::WebSocketMessagePtr& msg) {
            if (msg->type == ix::WebSocketMessageType::Open) {
                backoff_ms.store(1000);
                monitorLog("gemini open");
                nlohmann::json names = nlohmann::json::array();
                for (const auto& kv : folded) {
                    names.push_back(kv.first + "@bookTicker");
                }
                nlohmann::json sub;
                sub["id"] = "1";
                sub["method"] = "SUBSCRIBE";
                sub["params"] = names;
                webSocket.send(sub.dump());
            } else if (msg->type == ix::WebSocketMessageType::Close) {
                monitorLog("gemini closed");
            } else if (msg->type == ix::WebSocketMessageType::Error) {
                monitorLog(std::string("gemini error: ") + msg->errorInfo.reason);
            } else if (msg->type == ix::WebSocketMessageType::Message) {
                latencyArrive();
                std::string symbol, bid_s, ask_s, bid_n_s, ask_n_s;
                if (!jsonField(msg->str, "s", symbol)) return;
                if (!jsonField(msg->str, "b", bid_s) || !jsonField(msg->str, "a", ask_s)) return;
                bool have_bid_n = jsonField(msg->str, "B", bid_n_s);
                bool have_ask_n = jsonField(msg->str, "A", ask_n_s);
                auto it = folded.find(geminiUpper(symbol));
                if (it == folded.end()) return;
                int yes_bid = parseCents(bid_s);
                int yes_ask = parseCents(ask_s);
                if (yes_bid <= 0 && yes_ask <= 0) return;
                int no_bid = complementCents(yes_ask);
                int no_ask = complementCents(yes_bid);
                int yes_bid_n = jsonSizeOrUnknown(have_bid_n, bid_n_s);
                int yes_ask_n = jsonSizeOrUnknown(have_ask_n, ask_n_s);
                md.price_update(it->second, Gemini, yes_bid, yes_ask, no_bid, no_ask,
                    yes_bid_n, yes_ask_n, yes_ask_n, yes_bid_n);
                latencyParsed();
                if (on_tick) on_tick(md, it->second);
            }
        });

        webSocket.start();
        waitUntilWsClosed(webSocket);
        webSocket.stop();
        int wait = backoff_ms.load();
        monitorLog("gemini reconnect in " + std::to_string(wait) + "ms");
        std::this_thread::sleep_for(std::chrono::milliseconds(wait));
        backoff_ms.store(nextWsBackoffMs(wait));
    }
}
