#pragma once

#include <string>
#include "strategy.hpp"

struct PolymarketOrderResult {
    bool ok = false;
    float fill_count = 0;
    float fill_price = 0;
};

PolymarketOrderResult sendPolymarketOrder(std::string slug, OrderIntent idea);
