#pragma once

#include <string>
#include <unordered_map>
#include "market_data.hpp"
#include "portfolio.hpp"

void monitorStart(int port);
void monitorBind(MarketData& md, PortfolioManager& pm,
    const std::unordered_map<long int, std::string>& kalshi,
    const std::unordered_map<long int, std::string>& polymarket,
    const std::unordered_map<long int, std::string>& gemini);
void monitorLog(const std::string& line);
void monitorSetWorking(bool v);
void monitorSetTrading(bool v);
void monitorRequestTrade();
bool monitorTradeRequested();
void monitorHalt();
bool monitorHalted();
void monitorRefresh(MarketData& md, PortfolioManager& pm,
    const std::unordered_map<long int, std::string>& kalshi,
    const std::unordered_map<long int, std::string>& polymarket,
    const std::unordered_map<long int, std::string>& gemini);
