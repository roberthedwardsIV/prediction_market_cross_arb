#pragma once

#include <string>
#include "strategy.hpp"

struct GeminiOrderResult {
    bool ok = false;
    float fill_count = 0;
    float fill_price = 0;
};

GeminiOrderResult sendGeminiOrder(std::string symbol, OrderIntent idea);
