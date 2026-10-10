#pragma once

#include <algorithm>
#include "market_data.hpp"
#include "money.hpp"

// Venue taker fees in cents. Same published formulas, integer ceil/round.
inline int takerFee(int venue, int price_cents, int contracts) {
    int p = clampPriceCents(price_cents);
    int n = contracts;
    if (n < 1) return 0;
    if (venue == Kalshi) {
        // ceil(0.07 * n * p * (1-p) * 100) / 100 dollars -> ceil(7*n*p*(100-p)/10000) cents
        return static_cast<int>((7LL * n * p * (100 - p) + 9999) / 10000);
    }
    if (venue == Polymarket) {
        // round(0.06 * n * p * (1-p) * 100) / 100 dollars
        return static_cast<int>((6LL * n * p * (100 - p) + 5000) / 10000);
    }
    if (venue == Gemini) {
        // round(0.05 * n * min(p,1-p) * 100) / 100 dollars
        int m = std::min(p, 100 - p);
        return static_cast<int>((5LL * n * m + 50) / 100);
    }
    return 0;
}

struct OrderIntent {
    long int market_id;
    long int venue_id;
    int venue;
    bool buy;
    int side;
    int size;
    int limit_price;
};

class Strategy {
    static constexpr int kBookTakePct = 70;
    OrderIntent next_yes_move, next_no_move;

    public:
        Strategy() {
            next_yes_move.market_id = 0;
            next_yes_move.venue = 0;
            next_yes_move.venue_id = 0;
            next_yes_move.buy = false;
            next_yes_move.side = YesSided;
            next_yes_move.size = 0;
            next_yes_move.limit_price = 0;

            next_no_move.market_id = 0;
            next_no_move.venue = 0;
            next_no_move.venue_id = 0;
            next_no_move.buy = false;
            next_no_move.side = NoSided;
            next_no_move.size = 0;
            next_no_move.limit_price = 0;
        }

        void strategize(Market market, long int now,
                        int /*kalshi_cash*/, int /*polymarket_cash*/, int /*gemini_cash*/) {
            next_yes_move.size = 0;
            next_no_move.size = 0;
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

            int fees = takerFee(lowest_no_ask.venue, lowest_no_ask.no_ask, 1)
                + takerFee(lowest_yes_ask.venue, lowest_yes_ask.yes_ask, 1);
            int pair = fees + lowest_yes_ask.yes_ask + lowest_no_ask.no_ask;

            if (lowest_no_ask.venue_id != 0 && lowest_yes_ask.venue_id != 0 && pair < kDollarCents) {
                int book = std::min(lowest_yes_ask.yes_ask_n, lowest_no_ask.no_ask_n);
                int size = (book * kBookTakePct) / 100;
                if (size < 1) {
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
