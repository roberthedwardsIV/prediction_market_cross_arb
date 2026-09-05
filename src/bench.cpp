#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>

#include "tape.hpp"
#include "market_data.hpp"
#include "latency.hpp"
#include "strategy.hpp"

int main() {
    std::ifstream data;
    std::string line;
    std::vector<TapeTick> ticks;
    data.open("ticks_replay_sample.csv");

    if(!data) { 
        std::cout << "Error\n";
        return 1; 
    }
    std::getline(data, line);
    while (std::getline(data, line)) {
        TapeTick t;
        if(tapeParseLine(line, t)) { 
            ticks.push_back(t);
        };
    }

    std::cout << ticks.size() << "\n";

    std::ifstream markets;
    MarketData md;
    std::string row, tmp, kal_id, pm_id, gem_id;
    long int market_id, expiration;
    int market_count = 0;
    long int next_vid = 1; 
    
    markets.open("markets.csv");
    std::getline(markets, row);

    while (std::getline(markets, row)) {
        tmp = row;
        for (size_t i = 0; i < tmp.size(); i++) {
            if (tmp[i] == ',') tmp[i] = ' ';
        }
        std::stringstream ss(tmp);
        ss >> market_id >> expiration >> kal_id >> pm_id >> gem_id;
        
        long int kal_vid = 0, pm_vid = 0, gem_vid = 0;
        if (!kal_id.empty() && kal_id != "0") {
            kal_vid = next_vid++;
        }
        if (!pm_id.empty() && pm_id != "0") {
            pm_vid = next_vid++;
        }
        if (!gem_id.empty() && gem_id != "0") {
            gem_vid = next_vid++;
        }
        md.Register(market_id, expiration, kal_vid, pm_vid, gem_vid);
        market_count++;
    }

    std::cout << market_count << "\n";

    for (const TapeTick& tk : ticks) {
        Market m = md.getMarketById(tk.market_id);
        if (m.getMarketId() == 0) continue;
        Snapshot snap;
        if (tk.venue == Kalshi) snap = m.getKalshiSnapshot();
        else if (tk.venue == Polymarket) snap = m.getPolymarketSnapshot();
        else if (tk.venue == Gemini) snap = m.getGeminiSnapshot();
        else continue;
        if (snap.venue_id == 0) continue;
        latencyArrive();
        md.price_update(snap.venue_id, tk.venue, tk.yes_bid, tk.yes_ask, tk.no_bid, tk.no_ask, tk.yes_bid_n, tk.yes_ask_n, tk.no_bid_n, tk.no_ask_n);
        latencyParsed();

        m = md.getMarketById(tk.market_id);
        Strategy strat; 
        strat.strategize(m, tk.ts_ms/1000, 100, 100, 100);

        latencyIntent(); 
        latencyRecordTick();

        
    }

    writeLatencyToCsv("bench_latency.csv");
    std::cout << latencySummaryLine() << "\n";
    
    return 0;

}