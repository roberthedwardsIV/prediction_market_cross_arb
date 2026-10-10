#pragma once

#include <string>
#include "strategy.hpp"

struct GeminiOrderResult {
    bool ok = false;
    int fill_count = 0;
    int fill_price = 0;
};

GeminiOrderResult sendGeminiOrder(std::string symbol, OrderIntent idea);
