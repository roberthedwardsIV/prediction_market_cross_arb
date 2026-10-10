#include <iostream>
#include <cstdio>
#include <nlohmann/json.hpp>

#include "gemini_order.hpp"
#include "kalshi_env.hpp"
#include "gemini_http.hpp"
#include "money.hpp"
#include "json_find.hpp"

GeminiOrderResult sendGeminiOrder(std::string symbol, OrderIntent idea) {
    GeminiOrderResult result;
    std::cout << symbol << "\n";
    nlohmann::json extra;
    extra["symbol"] = symbol;
    extra["orderType"] = "limit";
    extra["side"] = idea.buy ? "buy" : "sell";
    extra["quantity"] = std::to_string(idea.size);
    extra["price"] = formatCents4(idea.limit_price);
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
            auto q = r.contains("executedQuantity") ? r["executedQuantity"] : r["filledQuantity"];
            result.fill_count = jsonContracts(q.is_string() ? q.get<std::string>() : q.dump());
        }
        if (r.contains("avgPrice") || r.contains("averagePrice")) {
            auto v = r.contains("avgPrice") ? r["avgPrice"] : r["averagePrice"];
            result.fill_price = parseCents(v.is_string() ? v.get<std::string>() : v.dump());
        }
    } catch (...) {
        return result;
    }
    if (assisiLiveOrders() && result.fill_count <= 0) {
        std::cout << "gemini FOK no fill\n";
        return result;
    }
    result.ok = result.fill_count > 0;
    return result;
}
