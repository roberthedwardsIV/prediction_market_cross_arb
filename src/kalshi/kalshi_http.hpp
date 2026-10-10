#pragma once

#include <string>
#include <vector>

struct KalshiHttpResult {
    int status = 0;
    std::string body;
    std::string error;
};

struct KalshiLot {
    std::string ticker;
    int yes_count = 0;
    int no_count = 0;
    int avg_yes = 0;
    int avg_no = 0;
};

KalshiHttpResult kalshiHttp(const std::string& method, const std::string& sign_path, const std::string& body);
bool refreshKalshiBalance(int& cash_out);
bool refreshKalshiPositions(std::vector<KalshiLot>& lots);
