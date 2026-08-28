#pragma once

#include <string>
#include <vector>

struct GeminiHttpResult {
    int status = 0;
    std::string body;
    std::string error;
};

struct GeminiLot {
    std::string symbol;
    int yes_count = 0;
    int no_count = 0;
    float avg_yes = 0;
    float avg_no = 0;
};

bool geminiKeysPresent();
GeminiHttpResult geminiHttp(const std::string& method, const std::string& request_path, const std::string& extra_json);
bool refreshGeminiBalance(float& cash_out);
bool refreshGeminiPositions(std::vector<GeminiLot>& lots);
