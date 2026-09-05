#include <gtest/gtest.h>
#include "strategy.hpp"

TEST(Strategy, EmitsSizeOneOnEdge) {
    // yes 0.40 + no 0.40 + fees < 1.0
    Market m(1, 9999999999L, /*kal*/10, /*pm*/20, /*gem*/0);
    m.snapshot_update(10, Kalshi, 0.39f, 0.40f, 0.59f, 0.60f, 5, 5, 5, 5);
    m.snapshot_update(20, Polymarket, 0.39f, 0.41f, 0.39f, 0.40f, 5, 5, 5, 5);

    Strategy s;
    s.strategize(m, 0, 100.f, 100.f, 100.f);
    EXPECT_EQ(s.getYesOrderIntent().size, 1);
    EXPECT_EQ(s.getNoOrderIntent().size, 1);
    EXPECT_FLOAT_EQ(s.getYesOrderIntent().limit_price, 0.40f);
    EXPECT_FLOAT_EQ(s.getNoOrderIntent().limit_price, 0.40f);
}

TEST(Strategy, NoTradeWhenNoEdge) {
    Market m(1, 9999999999L, 10, 20, 0);
    // any yes + any no is at least 0.60+0.60 = 1.20 before fees
    m.snapshot_update(10, Kalshi, 0.59f, 0.60f, 0.59f, 0.60f, 5, 5, 5, 5);
    m.snapshot_update(20, Polymarket, 0.59f, 0.60f, 0.59f, 0.60f, 5, 5, 5, 5);

    Strategy s;
    s.strategize(m, 0, 100.f, 100.f, 100.f);
    EXPECT_EQ(s.getYesOrderIntent().size, 0);
    EXPECT_EQ(s.getNoOrderIntent().size, 0);
}

TEST(Strategy, SkipsExpired) {
    Market m(1, 100L, 10, 20, 0); // expiration 100
    m.snapshot_update(10, Kalshi, 0.39f, 0.40f, 0.59f, 0.60f, 5, 5, 5, 5);
    m.snapshot_update(20, Polymarket, 0.39f, 0.41f, 0.39f, 0.40f, 5, 5, 5, 5);

    Strategy s;
    s.strategize(m, /*now*/ 200, 100.f, 100.f, 100.f);
    EXPECT_EQ(s.getYesOrderIntent().size, 0);
}