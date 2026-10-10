#pragma once
#include "strategy.hpp"

struct Fill {
    long int market_id;
    long int venue_id;
    int venue;
    int side;
    int size;
    int price;
    bool buy;
};

enum class FillApply {
    Applied,
    Ignored,
    Unregistered
};

inline bool takeReportedFill(int reported_count, int reported_price, int limit_price,
                             int& size_out, int& price_out) {
    if (reported_count <= 0) {
        size_out = 0;
        price_out = 0;
        return false;
    }
    size_out = reported_count;
    price_out = reported_price > 0 ? reported_price : limit_price;
    return true;
}

class Execute {
    Fill order_fill;

    public:
        Execute() {
            order_fill.side = 0;
            order_fill.venue = 0;
            order_fill.market_id = 0;
            order_fill.venue_id = 0;
            order_fill.size = 0;
            order_fill.price = 0;
            order_fill.buy = false;
        }

        void Executioner(OrderIntent local_intent) {
            if ((local_intent.market_id != 0) && (local_intent.venue_id != 0) && (local_intent.size > 0)) {
                order_fill.side = local_intent.side;
                order_fill.market_id = local_intent.market_id;
                order_fill.venue_id = local_intent.venue_id;
                order_fill.venue = local_intent.venue;
                order_fill.size = local_intent.size;
                order_fill.price = local_intent.limit_price;
                order_fill.buy = local_intent.buy;
            }
        }

        Fill getFill() { return order_fill; }
};
