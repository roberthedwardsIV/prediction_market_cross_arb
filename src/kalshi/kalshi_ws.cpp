#include "kalshi_ws.hpp"
#include "kalshi_env.hpp"
#include "latency.hpp"
#include "parse_book.hpp"
#include "ws_reconnect.hpp"
#include "monitor.hpp"

#include <iostream>
#include <chrono>
#include <thread>
#include <string>
#include <fstream>
#include <vector>
#include <atomic>

#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/rsa.h>
#include <nlohmann/json.hpp>

static bool kalshiSignHeaders(ix::WebSocketHttpHeaders& headers,
                              const std::string& key_id,
                              const std::string& pem_path) {
    std::ifstream pem_file(pem_path);
    if (!pem_file) {
        std::cerr << "could not open pem file\n";
        return false;
    }
    BIO* bio = BIO_new_file(pem_path.c_str(), "r");
    if (!bio) {
        std::cerr << "BIO_new_file failed\n";
        return false;
    }
    EVP_PKEY* pkey = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    if (!pkey) {
        std::cerr << "could not parse pem\n";
        ERR_print_errors_fp(stderr);
        return false;
    }

    std::string ts = std::to_string(nextKalshiTimestamp());
    headers["KALSHI-ACCESS-KEY"] = key_id;
    headers["KALSHI-ACCESS-TIMESTAMP"] = ts;
    std::string to_sign = ts + "GET" + "/trade-api/ws/v2";

    EVP_MD_CTX* md_ctx = EVP_MD_CTX_new();
    EVP_PKEY_CTX* pkey_ctx = nullptr;
    if (!md_ctx ||
        EVP_DigestSignInit(md_ctx, &pkey_ctx, EVP_sha256(), nullptr, pkey) != 1 ||
        EVP_PKEY_CTX_set_rsa_padding(pkey_ctx, RSA_PKCS1_PSS_PADDING) != 1 ||
        EVP_PKEY_CTX_set_rsa_pss_saltlen(pkey_ctx, RSA_PSS_SALTLEN_DIGEST) != 1 ||
        EVP_PKEY_CTX_set_rsa_mgf1_md(pkey_ctx, EVP_sha256()) != 1 ||
        EVP_DigestSignUpdate(md_ctx, to_sign.data(), to_sign.size()) != 1) {
        std::cerr << "sign init/update failed\n";
        ERR_print_errors_fp(stderr);
        EVP_MD_CTX_free(md_ctx);
        EVP_PKEY_free(pkey);
        return false;
    }

    size_t sig_len = 0;
    if (EVP_DigestSignFinal(md_ctx, nullptr, &sig_len) != 1) {
        std::cerr << "sign size failed\n";
        ERR_print_errors_fp(stderr);
        EVP_MD_CTX_free(md_ctx);
        EVP_PKEY_free(pkey);
        return false;
    }
    std::vector<unsigned char> sig(sig_len);
    if (EVP_DigestSignFinal(md_ctx, sig.data(), &sig_len) != 1) {
        std::cerr << "sign failed\n";
        ERR_print_errors_fp(stderr);
        EVP_MD_CTX_free(md_ctx);
        EVP_PKEY_free(pkey);
        return false;
    }
    EVP_MD_CTX_free(md_ctx);
    EVP_PKEY_free(pkey);

    std::string signature(((sig.size() + 2) / 3) * 4, '\0');
    int n = EVP_EncodeBlock(
        reinterpret_cast<unsigned char*>(&signature[0]),
        sig.data(),
        static_cast<int>(sig.size())
    );
    signature.resize(static_cast<size_t>(n));
    headers["KALSHI-ACCESS-SIGNATURE"] = signature;
    headers["Host"] = kalshiWsHost();
    headers["Origin"] = kalshiWsOrigin();
    return true;
}

bool startKalshiWebsocket(MarketData& md, std::unordered_map<std::string, long int>& kalshi_ids, void (*on_tick)(MarketData&, long int)) {
    ix::initNetSystem();

    std::string key_id = envValue("KALSHI_API_KEY");
    std::string pem_path = envValue("KALSHI_PRIVATE_KEY_PATH");
    if (key_id.empty()) {
        std::cerr << "missing KALSHI_API_KEY\n";
        return false;
    }
    if (pem_path.empty()) {
        std::cerr << "missing KALSHI_PRIVATE_KEY_PATH\n";
        return false;
    }

    std::atomic<int> backoff_ms{1000};
    while (true) {
        ix::WebSocket webSocket;
        ix::WebSocketHttpHeaders headers;
        if (!kalshiSignHeaders(headers, key_id, pem_path)) {
            int wait = backoff_ms.load();
            monitorLog("kalshi sign failed, retry in " + std::to_string(wait) + "ms");
            std::this_thread::sleep_for(std::chrono::milliseconds(wait));
            backoff_ms.store(nextWsBackoffMs(wait));
            continue;
        }

        webSocket.setUrl(kalshiWsUrl());
        webSocket.disablePerMessageDeflate();
        webSocket.disableAutomaticReconnection();
        webSocket.setHandshakeTimeout(15);
        webSocket.setExtraHeaders(headers);
        webSocket.setOnMessageCallback([&webSocket, &md, &kalshi_ids, on_tick, &backoff_ms](const ix::WebSocketMessagePtr& msg) {
            if (msg->type == ix::WebSocketMessageType::Open) {
                backoff_ms.store(1000);
                monitorLog("kalshi open");
                nlohmann::json names = nlohmann::json::array();
                for (const auto& kv : kalshi_ids) {
                    names.push_back(kv.first);
                }
                nlohmann::json sub;
                sub["id"] = 1;
                sub["cmd"] = "subscribe";
                sub["params"]["send_initial_snapshot"] = true;
                sub["params"]["channels"] = nlohmann::json::array({"ticker"});
                sub["params"]["market_tickers"] = names;
                webSocket.send(sub.dump());
            } else if (msg->type == ix::WebSocketMessageType::Close) {
                monitorLog("kalshi closed");
            } else if (msg->type == ix::WebSocketMessageType::Error) {
                monitorLog(std::string("kalshi error: ") + msg->errorInfo.reason);
            } else if (msg->type == ix::WebSocketMessageType::Message) {
                latencyArrive();
                ParsedBook book = parseKalshiTickerJson(msg->str);
                if (!book.ok) return;
                auto it = kalshi_ids.find(book.id);
                if (it == kalshi_ids.end()) return;
                long int venue_id = it->second;
                int no_bid = complementCents(book.yes_ask);
                int no_ask = complementCents(book.yes_bid);
                md.price_update(venue_id, Kalshi, book.yes_bid, book.yes_ask, no_bid, no_ask,
                    book.yes_bid_n, book.yes_ask_n, book.yes_ask_n, book.yes_bid_n);
                latencyParsed();
                if (on_tick) on_tick(md, venue_id);
            }
        });

        webSocket.start();
        waitUntilWsClosed(webSocket);
        webSocket.stop();
        int wait = backoff_ms.load();
        monitorLog("kalshi reconnect in " + std::to_string(wait) + "ms");
        std::this_thread::sleep_for(std::chrono::milliseconds(wait));
        backoff_ms.store(nextWsBackoffMs(wait));
    }
}
