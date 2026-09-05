#include <gtest/gtest.h>
#include "portfolio.hpp"
#include "risk.hpp"

static OrderIntent makeLeg(long market_id, int venue, int size, float price) {
    OrderIntent o{};
    o.market_id = market_id;
    o.venue_id = 1;
    o.venue = venue;
    o.buy = true;
    o.side = (venue == Kalshi ? YesSided : NoSided); // side unused by approve_pair
    o.size = size;
    o.limit_price = price;
    return o;
}

static RiskLimits looseLimits() {
    // max_market_pct, max_allocation_pct, venue_reserve_pct, max_contracts
    return RiskLimits{0.90f, 0.90f, 0.10f, 100};
}

TEST(Risk, ApprovesCheapPair) {
    PortfolioManager book;
    book.Register(1);
    book.setKalshiCash(100.f);
    book.setPolymarketCash(100.f);
    book.setGeminiCash(100.f);

    auto yes = makeLeg(1, Kalshi, 1, 0.40f);
    auto no  = makeLeg(1, Polymarket, 1, 0.40f);
    EXPECT_TRUE(approve_pair(yes, no, book, looseLimits()));
}

TEST(Risk, RejectsInsufficientVenueCash) {
    PortfolioManager book;
    book.Register(1);
    book.setKalshiCash(0.f);
    book.setPolymarketCash(100.f);
    book.setGeminiCash(100.f);

    auto yes = makeLeg(1, Kalshi, 1, 0.40f);
    auto no  = makeLeg(1, Polymarket, 1, 0.40f);
    EXPECT_FALSE(approve_pair(yes, no, book, looseLimits()));
}

TEST(Risk, RejectsOverContractCap) {
    PortfolioManager book;
    book.Register(1);
    book.setKalshiCash(100.f);
    book.setPolymarketCash(100.f);
    book.setGeminiCash(100.f);

    RiskLimits lim = looseLimits();
    lim.max_contracts_per_market = 1; // yes.size+no.size = 2 > 1

    auto yes = makeLeg(1, Kalshi, 1, 0.40f);
    auto no  = makeLeg(1, Polymarket, 1, 0.40f);
    EXPECT_FALSE(approve_pair(yes, no, book, lim));
}