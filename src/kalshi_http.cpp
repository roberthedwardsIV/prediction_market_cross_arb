#include "kalshi_http.hpp"
#include "kalshi_env.hpp"

#include <fstream>
#include <iostream>
#include <chrono>
#include <cmath>
#include <vector>

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/rsa.h>
#include <nlohmann/json.hpp>
#include <ixwebsocket/IXHttpClient.h>
#include <ixwebsocket/IXNetSystem.h>

static bool loadKalshiKey(std::string& key_id, std::string& pem_path) {
    std::ifstream env_file(".env");
    if (!env_file) {
        std::cerr << "could not open .env\n";
        return false;
    }
    std::string line;
    while (std::getline(env_file, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string name = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        if (!val.empty() && val.back() == '\r') val.pop_back();
        if (name == "KALSHI_API_KEY") key_id = val;
        if (name == "KALSHI_PRIVATE_KEY_PATH") pem_path = val;
    }
    return !key_id.empty() && !pem_path.empty();
}

KalshiHttpResult kalshiHttp(const std::string& method, const std::string& sign_path, const std::string& body) {
    KalshiHttpResult out;
    std::string key_id, pem_path;
    if (!loadKalshiKey(key_id, pem_path)) {
        out.error = "missing kalshi key";
        return out;
    }
    BIO* bio = BIO_new_file(pem_path.c_str(), "r");
    if (!bio) {
        out.error = "pem open failed";
        return out;
    }
    EVP_PKEY* pkey = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    if (!pkey) {
        out.error = "pem parse failed";
        return out;
    }

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    std::string ts = std::to_string(ms);
    std::string to_sign = ts + method + sign_path;

    EVP_MD_CTX* md_ctx = EVP_MD_CTX_new();
    EVP_PKEY_CTX* pkey_ctx = nullptr;
    bool sign_ok = md_ctx &&
        EVP_DigestSignInit(md_ctx, &pkey_ctx, EVP_sha256(), nullptr, pkey) == 1 &&
        EVP_PKEY_CTX_set_rsa_padding(pkey_ctx, RSA_PKCS1_PSS_PADDING) == 1 &&
        EVP_PKEY_CTX_set_rsa_pss_saltlen(pkey_ctx, RSA_PSS_SALTLEN_DIGEST) == 1 &&
        EVP_PKEY_CTX_set_rsa_mgf1_md(pkey_ctx, EVP_sha256()) == 1 &&
        EVP_DigestSignUpdate(md_ctx, to_sign.data(), to_sign.size()) == 1;
    if (!sign_ok) {
        EVP_MD_CTX_free(md_ctx);
        EVP_PKEY_free(pkey);
        out.error = "sign failed";
        return out;
    }
    size_t sig_len = 0;
    if (EVP_DigestSignFinal(md_ctx, nullptr, &sig_len) != 1) {
        EVP_MD_CTX_free(md_ctx);
        EVP_PKEY_free(pkey);
        out.error = "sign size failed";
        return out;
    }
    std::vector<unsigned char> sig(sig_len);
    if (EVP_DigestSignFinal(md_ctx, sig.data(), &sig_len) != 1) {
        EVP_MD_CTX_free(md_ctx);
        EVP_PKEY_free(pkey);
        out.error = "sign final failed";
        return out;
    }
    EVP_MD_CTX_free(md_ctx);

    std::string signature(((sig.size() + 2) / 3) * 4, '\0');
    int n = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(&signature[0]), sig.data(), static_cast<int>(sig.size()));
    signature.resize(static_cast<size_t>(n));
    EVP_PKEY_free(pkey);

    std::string url = kalshiRestBase() + sign_path;
    ix::initNetSystem();
    ix::HttpClient http;
    auto args = http.createRequest(url, method == "POST" ? ix::HttpClient::kPost : ix::HttpClient::kGet);
    args->extraHeaders["KALSHI-ACCESS-KEY"] = key_id;
    args->extraHeaders["KALSHI-ACCESS-SIGNATURE"] = signature;
    args->extraHeaders["KALSHI-ACCESS-TIMESTAMP"] = ts;
    args->extraHeaders["Origin"] = kalshiWsOrigin();
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

bool refreshKalshiBalance(float& cash_out) {
    auto r = kalshiHttp("GET", "/trade-api/v2/portfolio/balance", "");
    if (r.status != 200) {
        std::cout << "kalshi balance " << r.status << " " << r.error << "\n";
        return false;
    }
    try {
        nlohmann::json j = nlohmann::json::parse(r.body);
        if (j.contains("balance_dollars")) {
            if (j["balance_dollars"].is_string()) {
                cash_out = std::stof(j["balance_dollars"].get<std::string>());
            } else {
                cash_out = j["balance_dollars"].get<float>();
            }
            return true;
        }
        if (j.contains("balance")) {
            cash_out = j["balance"].get<float>() / 100.0f;
            return true;
        }
    } catch (...) {
    }
    return false;
}

static float jsonMoney(const nlohmann::json& j, const char* key) {
    if (!j.contains(key)) return 0;
    if (j[key].is_string()) {
        try { return std::stof(j[key].get<std::string>()); } catch (...) { return 0; }
    }
    if (j[key].is_number()) return j[key].get<float>();
    return 0;
}

static int jsonCount(const nlohmann::json& j) {
    if (j.contains("position_fp")) {
        try {
            if (j["position_fp"].is_string()) return static_cast<int>(std::round(std::stof(j["position_fp"].get<std::string>())));
            return static_cast<int>(std::round(j["position_fp"].get<float>()));
        } catch (...) {}
    }
    if (j.contains("position")) {
        try {
            if (j["position"].is_number()) return j["position"].get<int>();
            if (j["position"].is_string()) return std::stoi(j["position"].get<std::string>());
        } catch (...) {}
    }
    return 0;
}

bool refreshKalshiPositions(std::vector<KalshiLot>& lots) {
    lots.clear();
    auto r = kalshiHttp("GET", "/trade-api/v2/portfolio/positions", "");
    if (r.status != 200) {
        std::cout << "kalshi positions " << r.status << " " << r.error << "\n";
        return false;
    }
    try {
        nlohmann::json j = nlohmann::json::parse(r.body);
        if (!j.contains("market_positions") || !j["market_positions"].is_array()) return true;
        for (auto& p : j["market_positions"]) {
            KalshiLot lot;
            lot.ticker = p.value("ticker", "");
            int n = jsonCount(p);
            if (n == 0 || lot.ticker.empty()) continue;
            float exposure = jsonMoney(p, "market_exposure_dollars");
            float avg = std::abs(n) > 0 ? std::abs(exposure) / static_cast<float>(std::abs(n)) : 0;
            if (n > 0) {
                lot.yes_count = n;
                lot.avg_yes = avg;
            } else {
                lot.no_count = -n;
                lot.avg_no = avg;
            }
            lots.push_back(lot);
        }
        return true;
    } catch (...) {
    }
    return false;
}
