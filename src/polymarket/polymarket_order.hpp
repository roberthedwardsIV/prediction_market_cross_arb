#pragma once

#include <string>
#include "strategy.hpp"

struct PolymarketOrderResult {
    bool ok = false;
    int fill_count = 0;
    int fill_price = 0;
};

PolymarketOrderResult sendPolymarketOrder(std::string slug, OrderIntent idea);
