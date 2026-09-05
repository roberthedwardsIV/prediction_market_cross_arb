#include "polymarket_http.hpp"
#include "kalshi_env.hpp"

#include <fstream>
#include <iostream>
#include <chrono>
#include <vector>
#include <cmath>

#include <openssl/evp.h>
#include <openssl/err.h>
#include <nlohmann/json.hpp>
#include <ixwebsocket/IXHttpClient.h>
#include <ixwebsocket/IXNetSystem.h>

static bool loadPolymarketKeys(std::string& key_id, std::string& secret_b64) {
    key_id = envValue("POLYMARKET_KEY_ID");
    secret_b64 = envValue("POLYMARKET_SECRET_KEY");
    return !key_id.empty() && !secret_b64.empty();
}

bool polymarketKeysPresent() {
    std::string a, b;
    return loadPolymarketKeys(a, b);
}

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

static bool signPolymarket(const std::string& secret_b64, const std::string& message, std::string& signature) {
    std::vector<unsigned char> raw;
    if (!decodeB64(secret_b64, raw) || raw.size() < 32) return false;
    EVP_PKEY* pkey = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, nullptr, raw.data(), 32);
    if (!pkey) return false;
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        EVP_PKEY_free(pkey);
        return false;
    }
    size_t sig_len = 64;
    std::vector<unsigned char> sig(64);
    bool ok = EVP_DigestSignInit(ctx, nullptr, nullptr, nullptr, pkey) == 1 &&
        EVP_DigestSign(ctx, sig.data(), &sig_len, reinterpret_cast<const unsigned char*>(message.data()), message.size()) == 1;
    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pkey);
    if (!ok) return false;
    sig.resize(sig_len);
    signature.assign(((sig.size() + 2) / 3) * 4, '\0');
    int n = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(&signature[0]), sig.data(), static_cast<int>(sig.size()));
    signature.resize(static_cast<size_t>(n));
    return true;
}

PolymarketHttpResult polymarketHttp(const std::string& method, const std::string& sign_path, const std::string& body) {
    PolymarketHttpResult out;
    std::string key_id, secret_b64;
    if (!loadPolymarketKeys(key_id, secret_b64)) {
        out.error = "missing polymarket key";
        return out;
    }
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    std::string ts = std::to_string(ms);
    std::string to_sign = ts + method + sign_path;
    std::string signature;
    if (!signPolymarket(secret_b64, to_sign, signature)) {
        out.error = "ed25519 sign failed";
        return out;
    }

    std::string url = polymarketRestBase() + sign_path;
    ix::initNetSystem();
    ix::HttpClient http;
    auto args = http.createRequest(url, method == "POST" ? ix::HttpClient::kPost : ix::HttpClient::kGet);
    args->extraHeaders["X-PM-Access-Key"] = key_id;
    args->extraHeaders["X-PM-Timestamp"] = ts;
    args->extraHeaders["X-PM-Signature"] = signature;
    args->extraHeaders["Content-Type"] = "application/json";
    args->connectTimeout = 15;
    args->transferTimeout = 15;
    args->compress = false;

    ix::HttpResponsePtr resp;
    if (method == "POST") {
        resp = http.post(url, body, args);
    } else {
        resp = http.get(url, args);
    }
    out.status = resp->statusCode;
    out.body = resp->body;
    out.error = resp->errorMsg;
    return out;
}

static float jsonMoney(const nlohmann::json& j, const char* key) {
    if (!j.contains(key)) return 0;
    if (j[key].is_string()) {
        try { return std::stof(j[key].get<std::string>()); } catch (...) { return 0; }
    }
    if (j[key].is_number()) return j[key].get<float>();
    if (j[key].is_object() && j[key].contains("value")) {
        return jsonMoney(j[key], "value");
    }
    return 0;
}

bool refreshPolymarketBalance(float& cash_out) {
    auto r = polymarketHttp("GET", "/v1/account/balances", "");
    if (r.status != 200) {
        std::cout << "polymarket balance " << r.status << " " << r.error << "\n";
        return false;
    }
    try {
        nlohmann::json j = nlohmann::json::parse(r.body);
        nlohmann::json bals = j.contains("balances") ? j["balances"] : j;
        if (bals.is_array()) {
            for (auto& b : bals) {
                std::string ccy = b.value("currency", "USD");
                if (ccy != "USD" && ccy != "usd") continue;
                if (b.contains("buyingPower")) {
                    cash_out = jsonMoney(b, "buyingPower");
                    return true;
                }
                cash_out = jsonMoney(b, "currentBalance");
                return true;
            }
        }
        if (j.contains("buyingPower")) {
            cash_out = jsonMoney(j, "buyingPower");
            return true;
        }
    } catch (...) {
    }
    return false;
}

static int jsonCount(const nlohmann::json& j, const char* key) {
    if (!j.contains(key)) return 0;
    try {
        if (j[key].is_number()) return static_cast<int>(std::round(j[key].get<float>()));
        if (j[key].is_string()) return static_cast<int>(std::round(std::stof(j[key].get<std::string>())));
    } catch (...) {}
    return 0;
}

bool refreshPolymarketPositions(std::vector<PolymarketLot>& lots) {
    lots.clear();
    auto r = polymarketHttp("GET", "/v1/portfolio/positions", "");
    if (r.status != 200) {
        std::cout << "polymarket positions " << r.status << " " << r.error << "\n";
        return false;
    }
    try {
        nlohmann::json j = nlohmann::json::parse(r.body);
        nlohmann::json arr = nlohmann::json::array();
        if (j.is_array()) arr = j;
        else if (j.contains("positions") && j["positions"].is_array()) arr = j["positions"];
        else if (j.contains("data") && j["data"].is_array()) arr = j["data"];
        for (auto& p : arr) {
            PolymarketLot lot;
            lot.slug = p.value("marketSlug", p.value("slug", ""));
            if (lot.slug.empty()) continue;
            std::string side = p.value("outcomeSide", p.value("intent", ""));
            int n = jsonCount(p, "quantity");
            if (n == 0) n = jsonCount(p, "qty");
            if (n == 0) continue;
            float avg = jsonMoney(p, "avgPx");
            if (avg == 0) avg = jsonMoney(p, "averagePrice");
            if (side.find("NO") != std::string::npos || side.find("SHORT") != std::string::npos) {
                lot.no_count = std::abs(n);
                lot.avg_no = avg;
            } else {
                lot.yes_count = std::abs(n);
                lot.avg_yes = avg;
            }
            lots.push_back(lot);
        }
        return true;
    } catch (...) {
    }
    return false;
}
