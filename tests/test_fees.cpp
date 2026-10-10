#include <gtest/gtest.h>
#include "strategy.hpp"
#include "money.hpp"

TEST(Fees, KalshiKnown) {
    // n=1, p=50 -> ceil(1.75) = 2 cents
    EXPECT_EQ(takerFee(Kalshi, 50, 1), 2);
}

TEST(Fees, PolymarketKnown) {
    // round(1.5) = 2 cents
    EXPECT_EQ(takerFee(Polymarket, 50, 1), 2);
}

TEST(Fees, GeminiKnown) {
    // round(2.5) = 3 cents
    EXPECT_EQ(takerFee(Gemini, 50, 1), 3);
}

TEST(Fees, ClampsLowPrice) {
    EXPECT_EQ(takerFee(Kalshi, 0, 1), takerFee(Kalshi, 1, 1));
}

TEST(Money, ParseCents) {
    EXPECT_EQ(parseCents("0.40"), 40);
    EXPECT_EQ(parseCents("100"), 10000);
    EXPECT_EQ(parseCents("3.25"), 325);
    EXPECT_EQ(parseCents("0.507"), 51);
    EXPECT_EQ(parseCents("0.4"), 40);
    EXPECT_EQ(parsePct("0.70", 0), 70);
    EXPECT_EQ(parsePct("70", 0), 70);
    EXPECT_EQ(formatCents(40), "0.40");
    EXPECT_EQ(formatCents(10000), "100.00");
}
