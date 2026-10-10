#include <iostream>
#include <cstdio>
#include <nlohmann/json.hpp>

#include "polymarket_order.hpp"
#include "kalshi_env.hpp"
#include "polymarket_http.hpp"
#include "money.hpp"
#include "json_find.hpp"

PolymarketOrderResult sendPolymarketOrder(std::string slug, OrderIntent idea) {
    PolymarketOrderResult result;
    std::cout << slug << "\n";
    nlohmann::json j;
    j["marketSlug"] = slug;
    j["type"] = "ORDER_TYPE_LIMIT";
    j["quantity"] = idea.size;
    j["price"]["value"] = formatCents4(idea.limit_price);
    j["price"]["currency"] = "USD";
    if (idea.buy) {
        j["intent"] = (idea.side == YesSided) ? "ORDER_INTENT_BUY_LONG" : "ORDER_INTENT_BUY_SHORT";
    } else {
        j["intent"] = (idea.side == YesSided) ? "ORDER_INTENT_SELL_LONG" : "ORDER_INTENT_SELL_SHORT";
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
        int filled = 0;
        int px = 0;
        if (r.contains("executions") && r["executions"].is_array()) {
            for (auto& e : r["executions"]) {
                std::string t = e.value("type", "");
                if (t.find("FILL") == std::string::npos) continue;
                int sh = 0;
                if (e.contains("lastShares")) {
                    sh = jsonContracts(e["lastShares"].is_string()
                        ? e["lastShares"].get<std::string>() : e["lastShares"].dump());
                }
                filled += sh;
                if (e.contains("lastPx") && e["lastPx"].is_object() && e["lastPx"].contains("value")) {
                    auto v = e["lastPx"]["value"];
                    px = parseCents(v.is_string() ? v.get<std::string>() : v.dump());
                }
            }
        }
        result.fill_count = filled;
        result.fill_price = px;
    } catch (...) {
        return result;
    }
    if (assisiLiveOrders() && result.fill_count <= 0) {
        std::cout << "polymarket FOK no fill\n";
        return result;
    }
    result.ok = result.fill_count > 0;
    return result;
}
