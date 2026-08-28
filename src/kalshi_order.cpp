#include <iostream>
#include <cstdio>
#include <nlohmann/json.hpp>

#include "kalshi_order.hpp"
#include "kalshi_env.hpp"
#include "kalshi_http.hpp"

KalshiOrderResult sendKalshiOrder(std::string ticker, OrderIntent idea) {
    KalshiOrderResult result;
    std::cout << ticker << "\n";
    nlohmann::json j;
    j["ticker"] = ticker;
    j["count"] = std::to_string(idea.size);

    if(idea.side == YesSided) {
        std::cout << "bid " << idea.limit_price << "\n";
        j["side"] = "bid";
        char pbuf[16];
        std::snprintf(pbuf, sizeof(pbuf), "%.4f", idea.limit_price);
        j["price"] = pbuf;
    }
    if(idea.side == NoSided) {
        std::cout << "ask " << (1.0f - idea.limit_price) << "\n";
        j["side"] = "ask";
        char pbuf[16];
        std::snprintf(pbuf, sizeof(pbuf), "%.4f", 1.0f - idea.limit_price);
        j["price"] = pbuf;
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
            result.fill_count = std::stof(fcs);
        }
        if (r.contains("average_fill_price") && !r["average_fill_price"].is_null()) {
            std::string fps = r["average_fill_price"].is_string() ? r["average_fill_price"].get<std::string>() : r["average_fill_price"].dump();
            result.fill_price = std::stof(fps);
        }
    } catch (...) {
        return result;
    }
    if (assisiLiveOrders() && result.fill_count <= 0.0f) {
        std::cout << "kalshi FOK no fill\n";
        return result;
    }
    result.ok = !assisiLiveOrders() || result.fill_count > 0.0f;
    if (!assisiLiveOrders()) {
        result.ok = true;
        if (result.fill_count <= 0.0f) result.fill_count = static_cast<float>(idea.size);
        if (result.fill_price <= 0.0f) result.fill_price = idea.limit_price;
    }
    return result;
}
