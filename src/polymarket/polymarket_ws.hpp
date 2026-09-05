#pragma once

#include <unordered_map>
#include "market_data.hpp"

bool startPolymarketWebsocket(MarketData& md, std::unordered_map<std::string, long int>& ids, void (*on_tick)(MarketData&, long int));
