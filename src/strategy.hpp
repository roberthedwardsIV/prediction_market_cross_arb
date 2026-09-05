#pragma once

#include <cmath>
#include <algorithm>

#include "market_data.hpp"

inline float takerFee(int venue, float price, int contracts) {
    float p = price;
    if (p < 0.01f) p = 0.01f;
    if (p > 0.99f) p = 0.99f;
    float n = static_cast<float>(contracts);
    if (venue == Kalshi) {
        return std::ceil(0.07f * n * p * (1.0f - p) * 100.0f) / 100.0f;
    }
    if (venue == Polymarket) {
        return std::round(0.06f * n * p * (1.0f - p) * 100.0f) / 100.0f;
    }
    if (venue == Gemini) {
        return std::round(0.05f * n * std::min(p, 1.0f - p) * 100.0f) / 100.0f;
    }
    return 0.00;
}

struct OrderIntent {
    long int market_id;
    long int venue_id;
    int venue;
    bool buy;
    int side;
    int size;
    float limit_price;
};

class Strategy {
    OrderIntent next_yes_move, next_no_move;

    public:
        Strategy() {
            next_yes_move.market_id = 0;
            next_yes_move.venue = 0;
            next_yes_move.venue_id = 0;
            next_yes_move.buy = 0;
            next_yes_move.side = YesSided;
            next_yes_move.size = 0;
            next_yes_move.limit_price = 0.00;

            next_no_move.market_id = 0;
            next_no_move.venue = 0;
            next_no_move.venue_id = 0;
            next_no_move.buy = 0;
            next_no_move.side = NoSided;
            next_no_move.size = 0;
            next_no_move.limit_price = 0.00;
        }

        void strategize(Market market, long int now, float kalshi_cash, float polymarket_cash, float gemini_cash) {
            if (market.getExpirationDate() <= now) {
                return;
            }
            Snapshot views[3] = {
                market.getKalshiSnapshot(),
                market.getPolymarketSnapshot(),
                market.getGeminiSnapshot()
            };

            Snapshot lowest_yes_ask;
            lowest_yes_ask.venue_id = 0;
            Snapshot lowest_no_ask;
            lowest_no_ask.venue_id = 0;

            for (int i = 0; i < 3; i++) {
                if (views[i].venue_id == 0) continue;
                if (views[i].yes_ask != 0 && views[i].yes_ask_n >= 1 &&
                    (lowest_yes_ask.venue_id == 0 || views[i].yes_ask < lowest_yes_ask.yes_ask)) {
                    lowest_yes_ask = views[i];
                }
                if (views[i].no_ask != 0 && views[i].no_ask_n >= 1 &&
                    (lowest_no_ask.venue_id == 0 || views[i].no_ask < lowest_no_ask.no_ask)) {
                    lowest_no_ask = views[i];
                }
            }

            float fees = takerFee(lowest_no_ask.venue, lowest_no_ask.no_ask, 1) + takerFee(lowest_yes_ask.venue, lowest_yes_ask.yes_ask, 1);
            float pair = fees + lowest_yes_ask.yes_ask + lowest_no_ask.no_ask;

            if (lowest_no_ask.venue_id != 0 && lowest_yes_ask.venue_id != 0 && pair < 1.0f) {

                int book = std::min(lowest_yes_ask.yes_ask_n, lowest_no_ask.no_ask_n);
                int size = (book * 70) / 100;
                if (size < 1) {
                    next_yes_move.size = 0;
                    next_no_move.size = 0;
                    return; 
                }
                next_no_move.market_id = lowest_no_ask.market_id;
                next_no_move.venue_id = lowest_no_ask.venue_id;
                next_no_move.venue = lowest_no_ask.venue;
                next_no_move.buy = true;
                next_no_move.size = size;
                next_no_move.limit_price = lowest_no_ask.no_ask;

                next_yes_move.market_id = lowest_yes_ask.market_id;
                next_yes_move.venue_id = lowest_yes_ask.venue_id;
                next_yes_move.venue = lowest_yes_ask.venue;
                next_yes_move.buy = true;
                next_yes_move.size = size;
                next_yes_move.limit_price = lowest_yes_ask.yes_ask;
            }
        }

        OrderIntent getYesOrderIntent() { return next_yes_move; }
        OrderIntent getNoOrderIntent() { return next_no_move; }
};
