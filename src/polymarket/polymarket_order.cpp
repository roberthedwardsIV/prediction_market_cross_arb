#include <iostream>
#include <cstdio>
#include <nlohmann/json.hpp>

#include "polymarket_order.hpp"
#include "kalshi_env.hpp"
#include "polymarket_http.hpp"

PolymarketOrderResult sendPolymarketOrder(std::string slug, OrderIntent idea) {
    PolymarketOrderResult result;
    std::cout << slug << "\n";
    nlohmann::json j;
    j["marketSlug"] = slug;
    j["type"] = "ORDER_TYPE_LIMIT";
    j["quantity"] = idea.size;
    char pbuf[16];
    std::snprintf(pbuf, sizeof(pbuf), "%.4f", idea.limit_price);
    j["price"]["value"] = pbuf;
    j["price"]["currency"] = "USD";
    if (idea.side == YesSided) {
        j["intent"] = "ORDER_INTENT_BUY_LONG";
    } else {
        j["intent"] = "ORDER_INTENT_BUY_SHORT";
    }
    if (assisiLiveOrders()) {
        j["tif"] = "TIME_IN_FORCE_FILL_OR_KILL";
        j["synchronousExecution"] = true;
    } else {
        j["tif"] = "TIME_IN_FORCE_GOOD_TILL_CANCEL";
    }
    std::cout << j.dump() << "\n";

    auto resp = polymarketHttp("POST", "/v1/orders", j.dump());
    std::cout << "polymarket POST " << resp.status << " " << resp.error << "\n";
    std::cout << resp.body << "\n";
    if (resp.status != 200 && resp.status != 201) {
        return result;
    }
    try {
        nlohmann::json r = nlohmann::json::parse(resp.body);
        float filled = 0;
        float px = 0;
        if (r.contains("executions") && r["executions"].is_array()) {
            for (auto& e : r["executions"]) {
                std::string t = e.value("type", "");
                if (t.find("FILL") == std::string::npos) continue;
                float sh = 0;
                if (e.contains("lastShares")) {
                    if (e["lastShares"].is_string()) sh = std::stof(e["lastShares"].get<std::string>());
                    else if (e["lastShares"].is_number()) sh = e["lastShares"].get<float>();
                }
                filled += sh;
                if (e.contains("lastPx") && e["lastPx"].is_object() && e["lastPx"].contains("value")) {
                    auto v = e["lastPx"]["value"];
                    px = v.is_string() ? std::stof(v.get<std::string>()) : v.get<float>();
                }
            }
        }
        result.fill_count = filled;
        result.fill_price = px;
    } catch (...) {
        return result;
    }
    if (assisiLiveOrders() && result.fill_count <= 0.0f) {
        std::cout << "polymarket FOK no fill\n";
        return result;
    }
    result.ok = result.fill_count > 0.0f;
    return result;
}
