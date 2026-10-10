#include <iostream>
#include <cstdio>
#include <nlohmann/json.hpp>

#include "kalshi_order.hpp"
#include "kalshi_env.hpp"
#include "kalshi_http.hpp"
#include "money.hpp"
#include "json_find.hpp"

KalshiOrderResult sendKalshiOrder(std::string ticker, OrderIntent idea) {
    KalshiOrderResult result;
    std::cout << ticker << "\n";
    nlohmann::json j;
    j["ticker"] = ticker;
    j["count"] = std::to_string(idea.size);

    if (idea.buy) {
        if (idea.side == YesSided) {
            j["side"] = "bid";
            j["price"] = formatCents4(idea.limit_price);
        } else {
            j["side"] = "ask";
            j["price"] = formatCents4(complementCents(idea.limit_price));
        }
    } else if (idea.side == YesSided) {
        j["side"] = "ask";
        j["price"] = formatCents4(idea.limit_price);
    } else {
        // Flatten NO by buying YES (Kalshi is a yes book).
        j["side"] = "bid";
        j["price"] = formatCents4(complementCents(idea.limit_price));
    }

    if (assisiLiveOrders()) {
        j["time_in_force"] = "fill_or_kill";
    } else {
        j["time_in_force"] = "good_till_canceled";
    }
    j["self_trade_prevention_type"] = "taker_at_cross";
    std::cout << j.dump() << "\n";

    auto resp = kalshiHttp("POST", "/trade-api/v2/portfolio/events/orders", j.dump());
    std::cout << "kalshi url " << kalshiRestOrderUrl() << "\n";
    std::cout << "kalshi POST " << resp.status << " " << resp.error << "\n";
    std::cout << resp.body << "\n";

    if (resp.status != 201) {
        return result;
    }
    try {
        nlohmann::json r = nlohmann::json::parse(resp.body);
        if (r.contains("fill_count")) {
            std::string fcs = r["fill_count"].is_string() ? r["fill_count"].get<std::string>() : r["fill_count"].dump();
            result.fill_count = jsonContracts(fcs);
        }
        if (r.contains("average_fill_price") && !r["average_fill_price"].is_null()) {
            std::string fps = r["average_fill_price"].is_string() ? r["average_fill_price"].get<std::string>() : r["average_fill_price"].dump();
            result.fill_price = parseCents(fps);
        }
    } catch (...) {
        return result;
    }
    if (assisiLiveOrders() && result.fill_count <= 0) {
        std::cout << "kalshi FOK no fill\n";
        return result;
    }
    result.ok = result.fill_count > 0;
    return result;
}
