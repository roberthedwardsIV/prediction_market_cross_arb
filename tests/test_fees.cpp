#include <gtest/gtest.h>
#include "strategy.hpp"

TEST(Fees, KalshiKnown) {
    // fee = ceil(0.07 * n * p * (1-p) * 100) / 100
    // n=1, p=0.50 → ceil(1.75) / 100 = 0.02
    EXPECT_FLOAT_EQ(takerFee(Kalshi, 0.50f, 1), 0.02f);
}

TEST(Fees, PolymarketKnown) {
    // round(0.06 * 1 * 0.5 * 0.5 * 100) / 100 = round(1.5)/100 = 0.02
    EXPECT_FLOAT_EQ(takerFee(Polymarket, 0.50f, 1), 0.02f);
}

TEST(Fees, GeminiKnown) {
    // round(0.05 * 1 * min(0.5,0.5) * 100) / 100 = 0.03
    EXPECT_FLOAT_EQ(takerFee(Gemini, 0.50f, 1), 0.03f);
}

TEST(Fees, ClampsLowPrice) {
    // p clamped to 0.01 before fee math
    float a = takerFee(Kalshi, 0.00f, 1);
    float b = takerFee(Kalshi, 0.01f, 1);
    EXPECT_FLOAT_EQ(a, b);
}