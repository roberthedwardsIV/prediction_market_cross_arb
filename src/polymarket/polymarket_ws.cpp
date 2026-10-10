#include "polymarket_ws.hpp"
#include "kalshi_env.hpp"
#include "latency.hpp"
#include "parse_book.hpp"
#include "ws_reconnect.hpp"
#include "monitor.hpp"

#include <iostream>
#include <chrono>
#include <thread>
#include <string>
#include <vector>
#include <atomic>

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

static bool polymarketSignHeaders(ix::WebSocketHttpHeaders& headers,
                                  const std::string& key_id,
                                  const std::vector<unsigned char>& raw) {
    EVP_PKEY* pkey = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, nullptr, raw.data(), 32);
    if (!pkey) {
        monitorLog("polymarket ed25519 key failed");
        return false;
    }
    std::string ts = std::to_string(nextPolymarketTimestamp());
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
        monitorLog("polymarket ws sign failed");
        return false;
    }
    sig.resize(sig_len);
    std::string signature(((sig.size() + 2) / 3) * 4, '\0');
    int n = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(&signature[0]), sig.data(), static_cast<int>(sig.size()));
    signature.resize(static_cast<size_t>(n));
    headers["X-PM-Access-Key"] = key_id;
    headers["X-PM-Timestamp"] = ts;
    headers["X-PM-Signature"] = signature;
    return true;
}

bool startPolymarketWebsocket(MarketData& md, std::unordered_map<std::string, long int>& ids, void (*on_tick)(MarketData&, long int)) {
    if (ids.empty()) return true;
    std::string key_id = envValue("POLYMARKET_KEY_ID");
    std::string secret_b64 = envValue("POLYMARKET_SECRET_KEY");
    if (key_id.empty() || secret_b64.empty()) {
        monitorLog("polymarket keys missing, skip ws");
        return false;
    }
    std::vector<unsigned char> raw;
    if (!decodeB64(secret_b64, raw) || raw.size() < 32) {
        monitorLog("polymarket secret decode failed");
        return false;
    }

    ix::initNetSystem();
    std::atomic<int> backoff_ms{1000};
    while (true) {
        ix::WebSocket webSocket;
        ix::WebSocketHttpHeaders headers;
        if (!polymarketSignHeaders(headers, key_id, raw)) {
            int wait = backoff_ms.load();
            monitorLog("polymarket sign failed, retry in " + std::to_string(wait) + "ms");
            std::this_thread::sleep_for(std::chrono::milliseconds(wait));
            backoff_ms.store(nextWsBackoffMs(wait));
            continue;
        }

        webSocket.setUrl(polymarketWsMarketsUrl());
        webSocket.disablePerMessageDeflate();
        webSocket.disableAutomaticReconnection();
        webSocket.setHandshakeTimeout(15);
        webSocket.setExtraHeaders(headers);
        webSocket.setOnMessageCallback([&webSocket, &md, &ids, on_tick, &backoff_ms](const ix::WebSocketMessagePtr& msg) {
            if (msg->type == ix::WebSocketMessageType::Open) {
                backoff_ms.store(1000);
                monitorLog("polymarket open");
                nlohmann::json names = nlohmann::json::array();
                for (const auto& kv : ids) names.push_back(kv.first);
                nlohmann::json sub;
                sub["subscribe"]["requestId"] = "mdl-sub-1";
                sub["subscribe"]["subscriptionType"] = "SUBSCRIPTION_TYPE_MARKET_DATA";
                sub["subscribe"]["marketSlugs"] = names;
                webSocket.send(sub.dump());
            } else if (msg->type == ix::WebSocketMessageType::Close) {
                monitorLog("polymarket closed");
            } else if (msg->type == ix::WebSocketMessageType::Error) {
                monitorLog(std::string("polymarket error: ") + msg->errorInfo.reason);
            } else if (msg->type == ix::WebSocketMessageType::Message) {
                latencyArrive();
                ParsedBook book = parsePolymarketBookJson(msg->str);
                if (!book.ok) return;
                auto it = ids.find(book.id);
                if (it == ids.end()) return;
                int no_bid = complementCents(book.yes_ask);
                int no_ask = complementCents(book.yes_bid);
                md.price_update(it->second, Polymarket, book.yes_bid, book.yes_ask, no_bid, no_ask,
                    book.yes_bid_n, book.yes_ask_n, book.yes_ask_n, book.yes_bid_n);
                latencyParsed();
                if (on_tick) on_tick(md, it->second);
            }
        });

        webSocket.start();
        waitUntilWsClosed(webSocket);
        webSocket.stop();
        int wait = backoff_ms.load();
        monitorLog("polymarket reconnect in " + std::to_string(wait) + "ms");
        std::this_thread::sleep_for(std::chrono::milliseconds(wait));
        backoff_ms.store(nextWsBackoffMs(wait));
    }
}
