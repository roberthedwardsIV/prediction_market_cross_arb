#include <string>
#include <fstream>
#include <unordered_map>
#include <sstream>

#include "market_data.hpp"
#include "kalshi_ws.hpp"

int main() {
    MarketData md;

    std::unordered_map<std::string, long int> kalshi_ids;
    std::ifstream kalshi_seed;
    kalshi_seed.open("markets.csv");
    std::string csv_line, tmp;
    getline(kalshi_seed, csv_line);
    while(getline(kalshi_seed, csv_line)) {
        tmp = csv_line;
        for(int i = 0; i<tmp.size(); i++) {
            if(tmp[i] == ',') { tmp[i] = ' '; }
        }

        std::string kal_id, pm_id, gem_id;
        long int market_id, expiration;

        std::stringstream ss(tmp);
        ss >> market_id >> expiration >> kal_id >> pm_id >> gem_id;
        if (kal_id.empty() || kal_id == "0") continue;

        long int kal_venue_id = static_cast<long int>(kalshi_ids.size() + 1);
        kalshi_ids[kal_id] = kal_venue_id;

        md.Register(market_id, expiration, kal_venue_id, 0, 0);
    }


    startKalshiWebsocket(md, kalshi_ids, nullptr);

    return 0;
}


