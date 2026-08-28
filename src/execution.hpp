#pragma once
#include "strategy.hpp"

struct Fill {
    long int market_id;
    long int venue_id;
    int venue;
    int side;
    int size;
    float price;
    bool buy;
};

class Execute {
    Fill order_fill;

    public:
        Execute() {
            order_fill.side = 0;
            order_fill.venue = 0;
            order_fill.market_id = 0;
            order_fill.venue_id = 0;
            order_fill.size = 0;
            order_fill.price = 0.00;
            order_fill.buy = 0;
        }

        void Executioner(OrderIntent local_intent) {
            if((local_intent.market_id != 0) && (local_intent.venue_id !=0) && (local_intent.size > 0)) {
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