#include "gemini_ws.hpp"
#include "kalshi_env.hpp"
#include "latency.hpp"
#include "json_find.hpp"

#include <iostream>
#include <chrono>
#include <thread>
#include <string>
#include <cctype>
#include <unordered_map>

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
    ix::WebSocket webSocket;
    webSocket.setUrl(geminiWsUrl() + "/");
    webSocket.disablePerMessageDeflate();
    webSocket.setHandshakeTimeout(15);
    ix::WebSocketHttpHeaders headers;
    headers["Host"] = geminiWsHost();
    headers["Origin"] = "https://gemini.com";
    webSocket.setExtraHeaders(headers);
    webSocket.setOnMessageCallback([&webSocket, &md, &folded, on_tick](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Open) {
            std::cout << "gemini open\n";
            nlohmann::json names = nlohmann::json::array();
            for (const auto& kv : folded) {
                names.push_back(kv.first + "@bookTicker");
            }
            nlohmann::json sub;
            sub["id"] = "1";
            sub["method"] = "SUBSCRIBE";
            sub["params"] = names;
            webSocket.send(sub.dump());
        } else if (msg->type == ix::WebSocketMessageType::Error) {
            std::cout << "gemini error: " << msg->errorInfo.reason << "\n";
        } else if (msg->type == ix::WebSocketMessageType::Message) {
            latencyArrive();
            std::string symbol, bid_s, ask_s, bid_n_s, ask_n_s;
            if (!jsonField(msg->str, "s", symbol)) return;
            if (!jsonField(msg->str, "b", bid_s) || !jsonField(msg->str, "a", ask_s)) return;
            bool have_bid_n = jsonField(msg->str, "B", bid_n_s);
            bool have_ask_n = jsonField(msg->str, "A", ask_n_s);
            auto it = folded.find(geminiUpper(symbol));
            if (it == folded.end()) return;
            float yes_bid = std::stof(bid_s);
            float yes_ask = std::stof(ask_s);
            float no_bid = 1.0f - yes_ask;
            float no_ask = 1.0f - yes_bid;
            int yes_bid_n = jsonSizeOrUnknown(have_bid_n, bid_n_s);
            int yes_ask_n = jsonSizeOrUnknown(have_ask_n, ask_n_s);
            md.price_update(it->second, Gemini, yes_bid, yes_ask, no_bid, no_ask,
                yes_bid_n, yes_ask_n, yes_ask_n, yes_bid_n);
            latencyParsed();
            if (on_tick) on_tick(md, it->second);
        }
    });

    webSocket.start();
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
