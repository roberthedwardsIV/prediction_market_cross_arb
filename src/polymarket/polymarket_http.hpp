#pragma once

#include <string>
#include <vector>

struct PolymarketHttpResult {
    int status = 0;
    std::string body;
    std::string error;
};

struct PolymarketLot {
    std::string slug;
    int yes_count = 0;
    int no_count = 0;
    int avg_yes = 0;
    int avg_no = 0;
};

bool polymarketKeysPresent();
PolymarketHttpResult polymarketHttp(const std::string& method, const std::string& sign_path, const std::string& body);
bool refreshPolymarketBalance(int& cash_out);
bool refreshPolymarketPositions(std::vector<PolymarketLot>& lots);
