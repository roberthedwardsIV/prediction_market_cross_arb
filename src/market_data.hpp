#pragma once
#include <cstring>
#include <mutex>

const int Kalshi = 1;
const int Polymarket = 2;
const int Gemini = 3;
const int NoSided = 1;
const int YesSided = 2;

struct Snapshot {
    long int market_id;
    long int venue_id;
    int venue;
    float yes_bid;
    float yes_ask;
    float no_bid;
    float no_ask;
    int yes_bid_n;
    int yes_ask_n;
    int no_bid_n;
    int no_ask_n;

    Snapshot()
        : market_id(0), venue_id(0), venue(0),
          yes_bid(0), yes_ask(0), no_bid(0), no_ask(0),
          yes_bid_n(0), yes_ask_n(0), no_bid_n(0), no_ask_n(0) {}
};

class Market {
    Snapshot kalshi_market_view;
    Snapshot polymarket_market_view;
    Snapshot gemini_market_view;
    long int market_id;
    long int expiration_date;

    static Snapshot blank(int venue) {
        Snapshot s;
        s.market_id = 0;
        s.venue_id = 0;
        s.venue = venue;
        s.yes_bid = s.yes_ask = s.no_bid = s.no_ask = 0.00;
        s.yes_bid_n = s.yes_ask_n = s.no_bid_n = s.no_ask_n = 0;
        return s;
    }

    public:
        Market() {
            market_id = 0;
            expiration_date = 0;
            kalshi_market_view = blank(Kalshi);
            polymarket_market_view = blank(Polymarket);
            gemini_market_view = blank(Gemini);
        }
        Market(long int id, long int expiration, long int kalshi_id = 0, long int polymarket_id = 0, long int gemini_id = 0) {
            market_id = id;
            expiration_date = expiration;
            kalshi_market_view = blank(Kalshi);
            polymarket_market_view = blank(Polymarket);
            gemini_market_view = blank(Gemini);
            kalshi_market_view.market_id = polymarket_market_view.market_id = gemini_market_view.market_id = market_id;
            if (kalshi_id) kalshi_market_view.venue_id = kalshi_id;
            if (polymarket_id) polymarket_market_view.venue_id = polymarket_id;
            if (gemini_id) gemini_market_view.venue_id = gemini_id;
        }

        void snapshot_update(long int venue_id, int venue, float yes_bid, float yes_ask, float no_bid, float no_ask,
            int yes_bid_n, int yes_ask_n, int no_bid_n, int no_ask_n) {
            if (venue == Kalshi) {
                kalshi_market_view.venue_id = venue_id;
                kalshi_market_view.yes_bid = yes_bid;
                kalshi_market_view.yes_ask = yes_ask;
                kalshi_market_view.no_bid = no_bid;
                kalshi_market_view.no_ask = no_ask;
                kalshi_market_view.yes_bid_n = yes_bid_n;
                kalshi_market_view.yes_ask_n = yes_ask_n;
                kalshi_market_view.no_bid_n = no_bid_n;
                kalshi_market_view.no_ask_n = no_ask_n;
            } else if (venue == Polymarket) {
                polymarket_market_view.venue_id = venue_id;
                polymarket_market_view.yes_bid = yes_bid;
                polymarket_market_view.yes_ask = yes_ask;
                polymarket_market_view.no_bid = no_bid;
                polymarket_market_view.no_ask = no_ask;
                polymarket_market_view.yes_bid_n = yes_bid_n;
                polymarket_market_view.yes_ask_n = yes_ask_n;
                polymarket_market_view.no_bid_n = no_bid_n;
                polymarket_market_view.no_ask_n = no_ask_n;
            } else if (venue == Gemini) {
                gemini_market_view.venue_id = venue_id;
                gemini_market_view.yes_bid = yes_bid;
                gemini_market_view.yes_ask = yes_ask;
                gemini_market_view.no_bid = no_bid;
                gemini_market_view.no_ask = no_ask;
                gemini_market_view.yes_bid_n = yes_bid_n;
                gemini_market_view.yes_ask_n = yes_ask_n;
                gemini_market_view.no_bid_n = no_bid_n;
                gemini_market_view.no_ask_n = no_ask_n;
            }
        }

        long int getMarketId() { return market_id; }
        long int getExpirationDate() { return expiration_date; }
        Snapshot getKalshiSnapshot() { return kalshi_market_view; }
        Snapshot getPolymarketSnapshot() { return polymarket_market_view; }
        Snapshot getGeminiSnapshot() { return gemini_market_view; }
};

class MarketData {
    Market markets_[16];
    int market_count_;
    std::mutex mtx_;

public:
    MarketData() { market_count_ = 0; }

    void price_update(long int venue_id, int venue, float yes_bid, float yes_ask, float no_bid, float no_ask,
        int yes_bid_n, int yes_ask_n, int no_bid_n, int no_ask_n) {
        std::lock_guard<std::mutex> lock(mtx_);
        for (int i = 0; i < market_count_; i++) {
            if (venue == Kalshi && markets_[i].getKalshiSnapshot().venue_id == venue_id) {
                markets_[i].snapshot_update(venue_id, venue, yes_bid, yes_ask, no_bid, no_ask,
                    yes_bid_n, yes_ask_n, no_bid_n, no_ask_n);
                return;
            }
            if (venue == Polymarket && markets_[i].getPolymarketSnapshot().venue_id == venue_id) {
                markets_[i].snapshot_update(venue_id, venue, yes_bid, yes_ask, no_bid, no_ask,
                    yes_bid_n, yes_ask_n, no_bid_n, no_ask_n);
                return;
            }
            if (venue == Gemini && markets_[i].getGeminiSnapshot().venue_id == venue_id) {
                markets_[i].snapshot_update(venue_id, venue, yes_bid, yes_ask, no_bid, no_ask,
                    yes_bid_n, yes_ask_n, no_bid_n, no_ask_n);
                return;
            }
        }
    }

    void Register(long int market_id, long int expiration_date, long int kal_id = 0, long int pm_id = 0, long int gem_id = 0) {
        if (market_count_ < 16 && getMarketById(market_id).getMarketId() == 0) {
            markets_[market_count_] = Market(market_id, expiration_date, kal_id, pm_id, gem_id);
            market_count_++;
        }
    }

    int marketCount() {
        std::lock_guard<std::mutex> lock(mtx_);
        return market_count_;
    }

    Market getMarketAt(int i) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (i < 0 || i >= market_count_) return Market();
        return markets_[i];
    }

    Market getMarketById(long int market_id) {
        std::lock_guard<std::mutex> lock(mtx_);
        for (int i = 0; i < market_count_; i++) {
            if (markets_[i].getMarketId() == market_id) {
                return markets_[i];
            }
        }
        return Market();
    }

    Snapshot getSnapshotByVenueId(long int venue_id, int venue) {
        std::lock_guard<std::mutex> lock(mtx_);
        for (int i = 0; i < market_count_; i++) {
            if (venue == Kalshi && markets_[i].getKalshiSnapshot().venue_id == venue_id) {
                return markets_[i].getKalshiSnapshot();
            }
            if (venue == Polymarket && markets_[i].getPolymarketSnapshot().venue_id == venue_id) {
                return markets_[i].getPolymarketSnapshot();
            }
            if (venue == Gemini && markets_[i].getGeminiSnapshot().venue_id == venue_id) {
                return markets_[i].getGeminiSnapshot();
            }
        }
        return Snapshot();
    }
};

    };
    };
    };
    };
    };
    };
    };
    };
    };
    };
    };
    };
    };
    };
    };
    };
    };
    };
    };
    };
    };
    };
    };
    };
    };
    };
    };
    };
    };
    };
    };
    };
    };