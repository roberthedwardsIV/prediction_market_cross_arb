#include "polymarket_ws.hpp"
#include "kalshi_env.hpp"
#include "latency.hpp"
#include "json_find.hpp"

#include <iostream>
#include <chrono>
#include <thread>
#include <string>
#include <vector>

#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>
#include <openssl/evp.h>
#include <nlohmann/json.hpp>

static bool decodeB64(const std::string& in, std::vector<unsigned char>& out) {
    if (in.empty()) return false;
    out.assign(((in.size() + 3) / 4) * 3, 0);
    int n = EVP_DecodeBlock(out.data(), reinterpret_cast<const unsigned char*>(in.data()), static_cast<int>(in.size()));
    if (n < 0) return false;
    size_t pad = 0;
    if (!in.empty() && in.back() == '=') pad++;
    if (in.size() > 1 && in[in.size() - 2] == '=') pad++;
    out.resize(static_cast<size_t>(n) - pad);
    return !out.empty();
}

bool startPolymarketWebsocket(MarketData& md, std::unordered_map<std::string, long int>& ids, void (*on_tick)(MarketData&, long int)) {
    if (ids.empty()) return true;
    std::string key_id = envValue("POLYMARKET_KEY_ID");
    std::string secret_b64 = envValue("POLYMARKET_SECRET_KEY");
    if (key_id.empty() || secret_b64.empty()) {
        std::cout << "polymarket keys missing, skip ws\n";
        return false;
    }
    std::vector<unsigned char> raw;
    if (!decodeB64(secret_b64, raw) || raw.size() < 32) {
        std::cout << "polymarket secret decode failed\n";
        return false;
    }
    EVP_PKEY* pkey = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, nullptr, raw.data(), 32);
    if (!pkey) {
        std::cout << "polymarket ed25519 key failed\n";
        return false;
    }

    ix::initNetSystem();
    ix::WebSocket webSocket;
    ix::WebSocketHttpHeaders headers;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    std::string ts = std::to_string(ms);
    std::string path = "/v1/ws/markets";
    std::string to_sign = ts + "GET" + path;
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    size_t sig_len = 64;
    std::vector<unsigned char> sig(64);
    bool sign_ok = ctx &&
        EVP_DigestSignInit(ctx, nullptr, nullptr, nullptr, pkey) == 1 &&
        EVP_DigestSign(ctx, sig.data(), &sig_len, reinterpret_cast<const unsigned char*>(to_sign.data()), to_sign.size()) == 1;
    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pkey);
    if (!sign_ok) {
        std::cout << "polymarket ws sign failed\n";
        return false;
    }
    sig.resize(sig_len);
    std::string signature(((sig.size() + 2) / 3) * 4, '\0');
    int n = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(&signature[0]), sig.data(), static_cast<int>(sig.size()));
    signature.resize(static_cast<size_t>(n));
    headers["X-PM-Access-Key"] = key_id;
    headers["X-PM-Timestamp"] = ts;
    headers["X-PM-Signature"] = signature;

    webSocket.setUrl(polymarketWsMarketsUrl());
    webSocket.disablePerMessageDeflate();
    webSocket.disableAutomaticReconnection();
    webSocket.setHandshakeTimeout(15);
    webSocket.setExtraHeaders(headers);
    webSocket.setOnMessageCallback([&webSocket, &md, &ids, on_tick](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Open) {
            std::cout << "polymarket open\n";
            nlohmann::json names = nlohmann::json::array();
            for (const auto& kv : ids) names.push_back(kv.first);
            nlohmann::json sub;
            sub["subscribe"]["requestId"] = "mdl-sub-1";
            sub["subscribe"]["subscriptionType"] = "SUBSCRIPTION_TYPE_MARKET_DATA";
            sub["subscribe"]["marketSlugs"] = names;
            webSocket.send(sub.dump());
        } else if (msg->type == ix::WebSocketMessageType::Error) {
            std::cout << "polymarket error: " << msg->errorInfo.reason << "\n";
        } else if (msg->type == ix::WebSocketMessageType::Message) {
            latencyArrive();
            std::string slug, bid_s, ask_s, bid_n_s, ask_n_s;
            if (!jsonField(msg->str, "marketSlug", slug)) return;
            auto it = ids.find(slug);
            if (it == ids.end()) return;
            std::string bid_obj, ask_obj;
            bool have_bid_n = false, have_ask_n = false;
            if (jsonFirstObjectSlice(msg->str, "bids", bid_obj) && jsonFirstObjectSlice(msg->str, "offers", ask_obj)) {
                if (!jsonNestedValue(bid_obj, "px", bid_s) || !jsonNestedValue(ask_obj, "px", ask_s)) return;
                have_bid_n = jsonField(bid_obj, "qty", bid_n_s) || jsonNestedValue(bid_obj, "qty", bid_n_s);
                have_ask_n = jsonField(ask_obj, "qty", ask_n_s) || jsonNestedValue(ask_obj, "qty", ask_n_s);
            } else {
                if (!jsonNestedValue(msg->str, "bestBid", bid_s) || !jsonNestedValue(msg->str, "bestAsk", ask_s)) {
                    return;
                }
            }
            float yes_bid = std::stof(bid_s);
            float yes_ask = std::stof(ask_s);
            float no_bid = 1.0f - yes_ask;
            float no_ask = 1.0f - yes_bid;
            int yes_bid_n = jsonSizeOrUnknown(have_bid_n, bid_n_s);
            int yes_ask_n = jsonSizeOrUnknown(have_ask_n, ask_n_s);
            md.price_update(it->second, Polymarket, yes_bid, yes_ask, no_bid, no_ask,
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
