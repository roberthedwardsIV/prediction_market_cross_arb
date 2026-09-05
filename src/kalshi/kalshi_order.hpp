#pragma once

#include <string>
#include "strategy.hpp"

struct KalshiOrderResult {
    bool ok = false;
    float fill_count = 0;
    float fill_price = 0;
};

KalshiOrderResult sendKalshiOrder(std::string ticker, OrderIntent idea);
