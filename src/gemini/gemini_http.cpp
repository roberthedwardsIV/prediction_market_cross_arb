#include "gemini_http.hpp"
#include "kalshi_env.hpp"

#include <iostream>
#include <cmath>
#include <vector>
#include <sstream>
#include <iomanip>

#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <nlohmann/json.hpp>
#include <ixwebsocket/IXHttpClient.h>
#include <ixwebsocket/IXNetSystem.h>

bool geminiKeysPresent() {
    return !envValue("GEMINI_API_KEY").empty() && !envValue("GEMINI_API_SECRET").empty();
}

static std::string b64(const std::string& in) {
    std::string out(((in.size() + 2) / 3) * 4, '\0');
    int n = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(&out[0]),
        reinterpret_cast<const unsigned char*>(in.data()), static_cast<int>(in.size()));
    out.resize(static_cast<size_t>(n));
    return out;
}

static std::string hmacSha384Hex(const std::string& secret, const std::string& payload) {
    unsigned char md[EVP_MAX_MD_SIZE];
    unsigned int md_len = 0;
    HMAC(EVP_sha384(), secret.data(), static_cast<int>(secret.size()),
        reinterpret_cast<const unsigned char*>(payload.data()), payload.size(), md, &md_len);
    std::ostringstream o;
    o << std::hex << std::setfill('0');
    for (unsigned int i = 0; i < md_len; i++) {
        o << std::setw(2) << static_cast<int>(md[i]);
    }
    return o.str();
}

GeminiHttpResult geminiHttp(const std::string& /*method*/, const std::string& request_path, const std::string& extra_json) {
    GeminiHttpResult out;
    std::string key = envValue("GEMINI_API_KEY");
    std::string secret = envValue("GEMINI_API_SECRET");
    if (key.empty() || secret.empty()) {
        out.error = "missing gemini key";
        return out;
    }
    nlohmann::json payload;
    if (!extra_json.empty()) {
        try { payload = nlohmann::json::parse(extra_json); } catch (...) { payload = nlohmann::json::object(); }
    }
    if (!payload.is_object()) payload = nlohmann::json::object();
    payload["request"] = request_path;
    payload["nonce"] = nextGeminiNonce();
    std::string acct = envValue("GEMINI_ACCOUNT");
    if (acct.empty() && key.compare(0, 7, "master-") == 0) acct = "primary";
    if (!acct.empty()) payload["account"] = acct;
    std::string raw = payload.dump();
    std::string payload_b64 = b64(raw);
    std::string sig = hmacSha384Hex(secret, payload_b64);

    std::string url = geminiRestBase() + request_path;
    ix::initNetSystem();
    ix::HttpClient http;
    auto args = http.createRequest(url, ix::HttpClient::kPost);
    args->extraHeaders["X-GEMINI-APIKEY"] = key;
    args->extraHeaders["X-GEMINI-PAYLOAD"] = payload_b64;
    args->extraHeaders["X-GEMINI-SIGNATURE"] = sig;
    args->extraHeaders["Content-Type"] = "text/plain";
    args->extraHeaders["Cache-Control"] = "no-cache";
    args->extraHeaders["Origin"] = "https://gemini.com";
    args->connectTimeout = 15;
    args->transferTimeout = 15;
    args->compress = false;

    ix::HttpResponsePtr resp = http.post(url, "", args);
    out.status = resp->statusCode;
    out.body = resp->body;
    out.error = resp->errorMsg;
    return out;
}

static void logGeminiFail(const char* what, const GeminiHttpResult& r) {
    std::string body = r.body;
    if (body.size() > 300) body.resize(300);
    std::cout << "gemini " << what << " " << r.status << " " << r.error << " " << body << "\n";
}

static float jsonMoney(const nlohmann::json& j, const char* key) {
    if (!j.contains(key)) return 0;
    if (j[key].is_string()) {
        try { return std::stof(j[key].get<std::string>()); } catch (...) { return 0; }
    }
    if (j[key].is_number()) return j[key].get<float>();
    return 0;
}

bool refreshGeminiBalance(float& cash_out) {
    auto r = geminiHttp("POST", "/v1/balances", "");
    if (r.status != 200) {
        logGeminiFail("balance", r);
        return false;
    }
    try {
        nlohmann::json j = nlohmann::json::parse(r.body);
        nlohmann::json arr = j.is_array() ? j : (j.contains("balances") ? j["balances"] : nlohmann::json::array());
        for (auto& b : arr) {
            std::string ccy = b.value("currency", "");
            if (ccy != "USD" && ccy != "usd") continue;
            cash_out = jsonMoney(b, "available");
            if (cash_out == 0) cash_out = jsonMoney(b, "amount");
            return true;
        }
    } catch (...) {
    }
    return false;
}

bool refreshGeminiPositions(std::vector<GeminiLot>& lots) {
    lots.clear();
    auto r = geminiHttp("POST", "/v1/prediction-markets/positions", "");
    if (r.status != 200) {
        logGeminiFail("positions", r);
        return false;
    }
    try {
        nlohmann::json j = nlohmann::json::parse(r.body);
        nlohmann::json arr = nlohmann::json::array();
        if (j.is_array()) arr = j;
        else if (j.contains("positions") && j["positions"].is_array()) arr = j["positions"];
        for (auto& p : arr) {
            GeminiLot lot;
            lot.symbol = p.value("symbol", p.value("instrumentSymbol", ""));
            if (lot.symbol.empty()) continue;
            std::string outcome = p.value("outcome", "yes");
            float qty = jsonMoney(p, "totalQuantity");
            if (qty == 0) qty = jsonMoney(p, "positionQuantity");
            if (qty == 0) qty = std::abs(jsonMoney(p, "position"));
            int n = static_cast<int>(std::round(qty));
            if (n == 0) continue;
            float avg = jsonMoney(p, "avgPrice");
            if (outcome == "no" || outcome == "NO") {
                lot.no_count = n;
                lot.avg_no = avg;
            } else {
                lot.yes_count = n;
                lot.avg_yes = avg;
            }
            lots.push_back(lot);
        }
        return true;
    } catch (...) {
    }
    return false;
}
