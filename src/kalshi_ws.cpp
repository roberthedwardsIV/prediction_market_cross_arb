#include "kalshi_ws.hpp"
#include "kalshi_env.hpp"
#include "latency.hpp"
#include "json_find.hpp"

#include <iostream>
#include <chrono>
#include <thread>
#include <string>
#include <fstream>
#include <vector>

#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/rsa.h>
#include <nlohmann/json.hpp>

bool startKalshiWebsocket(MarketData& md, std::unordered_map<std::string, long int>& kalshi_ids, void (*on_tick)(MarketData&, long int)) {
    ix::initNetSystem();

    ix::WebSocket webSocket;
    ix::WebSocketHttpHeaders headers;
    
    std::ifstream env_file(".env");
    if (!env_file) {
        std::cerr << "could not open .env\n";
        return 1;
    }

    std::string key_id, pem_path;
    std::string line;
    while (std::getline(env_file, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string name = line.substr(0, eq);
        std::string val  = line.substr(eq + 1);
        if (name == "KALSHI_API_KEY") {
            key_id = val;
        }
        if (name == "KALSHI_PRIVATE_KEY_PATH") {
            pem_path = val;
        }
    }
    if (key_id.empty()) {
        std::cerr << "missing KALSHI_API_KEY in .env\n";
        return 1;
    }
    if (pem_path.empty()) {
        std::cerr << "missing KALSHI_PRIVATE_KEY_PATH in .env\n";
        return 1;
    }   
    std::ifstream pem_file(pem_path);
    if (!pem_file) {
        std::cerr << "could not open pem file\n";
        return 1;
    }
    BIO* bio = BIO_new_file(pem_path.c_str(), "r");
    if (!bio) {
        std::cerr << "BIO_new_file failed\n";
        return 1;
    }
    EVP_PKEY* pkey = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    if (!pkey) {
        std::cerr << "could not parse pem\n";
        ERR_print_errors_fp(stderr);
        return 1;
}
    headers["KALSHI-ACCESS-KEY"] = key_id;
    std::cout << "key_id length: " << key_id.size() << "\n";

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    std::string ts = std::to_string(ms);
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
        return 1;
    }

    size_t sig_len = 0;
    if (EVP_DigestSignFinal(md_ctx, nullptr, &sig_len) != 1) {
        std::cerr << "sign size failed\n";
        ERR_print_errors_fp(stderr);
        EVP_MD_CTX_free(md_ctx);
        EVP_PKEY_free(pkey);
        return 1;
    }
    std::vector<unsigned char> sig(sig_len);
    if (EVP_DigestSignFinal(md_ctx, sig.data(), &sig_len) != 1) {
        std::cerr << "sign failed\n";
        ERR_print_errors_fp(stderr);
        EVP_MD_CTX_free(md_ctx);
        EVP_PKEY_free(pkey);
        return 1;
    }
    EVP_MD_CTX_free(md_ctx);

    std::cout << "sig bytes: " << sig.size() << "\n";

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
    std::cout << "to_sign: [" << to_sign << "] " << unixMsUtc(ms) << "\n";

    webSocket.setUrl(kalshiWsUrl());
    webSocket.disablePerMessageDeflate();
    webSocket.disableAutomaticReconnection();
    webSocket.setHandshakeTimeout(15);
    webSocket.setExtraHeaders(headers);
    webSocket.setOnMessageCallback([&webSocket, &md, &kalshi_ids, on_tick](const ix::WebSocketMessagePtr& msg) {
    if (msg->type == ix::WebSocketMessageType::Open) {
        std::cout << "open\n";
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
    } else if (msg->type == ix::WebSocketMessageType::Error) {
        std::cout << "error: " << msg->errorInfo.reason << "\n";
    } else if (msg->type == ix::WebSocketMessageType::Message) {
        latencyArrive();
        std::string kind;
        if (!jsonField(msg->str, "type", kind)) {
            return;
        }
        if (kind == "ticker") {
            std::string bid_s, ask_s, ticker;
            if (!jsonField(msg->str, "yes_bid_dollars", bid_s) ||
                !jsonField(msg->str, "yes_ask_dollars", ask_s) ||
                !jsonField(msg->str, "market_ticker", ticker)) {
                return;
            }
            float yes_bid = std::stof(bid_s);
            float yes_ask = std::stof(ask_s);
            float no_bid = 1.0f - yes_ask;
            float no_ask = 1.0f - yes_bid;
            std::string bid_n_s, ask_n_s;
            int yes_bid_n = jsonSizeOrUnknown(jsonField(msg->str, "yes_bid_size_fp", bid_n_s), bid_n_s);
            int yes_ask_n = jsonSizeOrUnknown(jsonField(msg->str, "yes_ask_size_fp", ask_n_s), ask_n_s);
            auto it = kalshi_ids.find(ticker);
            if (it == kalshi_ids.end()) {
                return;
            }
            long int venue_id = it->second;
            md.price_update(venue_id, Kalshi, yes_bid, yes_ask, no_bid, no_ask,
                yes_bid_n, yes_ask_n, yes_ask_n, yes_bid_n);
            latencyParsed();
            if(on_tick) { on_tick(md, venue_id); }
        } else if (kind == "error") {
            std::cout << "kalshi: " << msg->str << "\n";
        }
    }
    });

    

    webSocket.start();
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
