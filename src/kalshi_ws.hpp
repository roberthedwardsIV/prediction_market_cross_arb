#include <unordered_map>
#include "market_data.hpp"

bool startKalshiWebsocket(MarketData&, std::unordered_map<std::string, long int>& kalshi_ids, void (*on_tick)(MarketData&, long int));
