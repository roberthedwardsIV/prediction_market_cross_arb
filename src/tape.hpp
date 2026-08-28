#pragma once

#include "market_data.hpp"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>

inline std::ofstream& tapeOut() {
    static std::ofstream f;
    return f;
}

inline std::mutex& tapeMtx() {
    static std::mutex m;
    return m;
}

inline bool tapeOpen(const std::string& path) {
    tapeOut().open(path, std::ios::out | std::ios::app);
    if (!tapeOut()) return false;
    if (tapeOut().tellp() == 0) {
        tapeOut() << "ts_ms,market_id,venue,yes_bid,yes_ask,no_bid,no_ask,"
                  << "yes_bid_n,yes_ask_n,no_bid_n,no_ask_n\n";
    }
    return true;
}

inline void tapeWrite(const Snapshot& s) {
    if (!tapeOut().is_open() || s.market_id == 0 || s.venue == 0) return;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    std::lock_guard<std::mutex> lock(tapeMtx());
    tapeOut() << ms << ',' << s.market_id << ',' << s.venue << ','
              << std::fixed << std::setprecision(4)
              << s.yes_bid << ',' << s.yes_ask << ',' << s.no_bid << ',' << s.no_ask << ','
              << s.yes_bid_n << ',' << s.yes_ask_n << ',' << s.no_bid_n << ',' << s.no_ask_n << '\n'
              << std::flush;
}

struct TapeTick {
    long int ts_ms;
    long int market_id;
    int venue;
    float yes_bid;
    float yes_ask;
    float no_bid;
    float no_ask;
    int yes_bid_n;
    int yes_ask_n;
    int no_bid_n;
    int no_ask_n;
};

inline bool tapeParseLine(const std::string& line, TapeTick& t) {
    if (line.empty() || line[0] == 't') return false;
    std::string tmp = line;
    for (char& c : tmp) if (c == ',') c = ' ';
    std::stringstream ss(tmp);
    if (!(ss >> t.ts_ms >> t.market_id >> t.venue
            >> t.yes_bid >> t.yes_ask >> t.no_bid >> t.no_ask
            >> t.yes_bid_n >> t.yes_ask_n >> t.no_bid_n >> t.no_ask_n)) {
        return false;
    }
    return t.market_id != 0 && t.venue != 0;
}
