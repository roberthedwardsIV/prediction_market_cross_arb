#include <iostream>
#include <cstdio>
#include <nlohmann/json.hpp>

#include "gemini_order.hpp"
#include "kalshi_env.hpp"
#include "gemini_http.hpp"

GeminiOrderResult sendGeminiOrder(std::string symbol, OrderIntent idea) {
    GeminiOrderResult result;
    std::cout << symbol << "\n";
    nlohmann::json extra;
    extra["symbol"] = symbol;
    extra["orderType"] = "limit";
    extra["side"] = "buy";
    extra["quantity"] = std::to_string(idea.size);
    char pbuf[16];
    std::snprintf(pbuf, sizeof(pbuf), "%.4f", idea.limit_price);
    extra["price"] = pbuf;
    extra["outcome"] = (idea.side == YesSided) ? "yes" : "no";
    if (assisiLiveOrders()) {
        extra["timeInForce"] = "fill-or-kill";
    } else {
        extra["timeInForce"] = "good-til-cancel";
    }
    std::cout << extra.dump() << "\n";

    auto resp = geminiHttp("POST", "/v1/prediction-markets/order", extra.dump());
    std::cout << "gemini POST " << resp.status << " " << resp.error << "\n";
    std::cout << resp.body << "\n";
    if (resp.status != 200 && resp.status != 201) {
        return result;
    }
    try {
        nlohmann::json r = nlohmann::json::parse(resp.body);
        if (r.contains("executedQuantity") || r.contains("filledQuantity")) {
            result.fill_count = r.contains("executedQuantity")
                ? (r["executedQuantity"].is_string() ? std::stof(r["executedQuantity"].get<std::string>()) : r["executedQuantity"].get<float>())
                : (r["filledQuantity"].is_string() ? std::stof(r["filledQuantity"].get<std::string>()) : r["filledQuantity"].get<float>());
        }
        if (r.contains("avgPrice") || r.contains("averagePrice")) {
            auto v = r.contains("avgPrice") ? r["avgPrice"] : r["averagePrice"];
            result.fill_price = v.is_string() ? std::stof(v.get<std::string>()) : v.get<float>();
        }
    } catch (...) {
        return result;
    }
    if (assisiLiveOrders() && result.fill_count <= 0.0f) {
        std::cout << "gemini FOK no fill\n";
        return result;
    }
    result.ok = result.fill_count > 0.0f;
    return result;
}
