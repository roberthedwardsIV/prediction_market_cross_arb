#pragma once

#include <string>
#include "strategy.hpp"

struct KalshiOrderResult {
    bool ok = false;
    int fill_count = 0;
    int fill_price = 0;
};

KalshiOrderResult sendKalshiOrder(std::string ticker, OrderIntent idea);
