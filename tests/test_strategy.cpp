#include <gtest/gtest.h>
#include "strategy.hpp"

TEST(Strategy, EmitsSizeOnEdge) {
    Market m(1, 9999999999L, /*kal*/10, /*pm*/20, /*gem*/0);
    m.snapshot_update(10, Kalshi, 39, 40, 59, 60, 5, 5, 5, 5);
    m.snapshot_update(20, Polymarket, 39, 41, 39, 40, 5, 5, 5, 5);

    Strategy s;
    s.strategize(m, 0, 10000, 10000, 10000);
    EXPECT_EQ(s.getYesOrderIntent().size, 3);
    EXPECT_EQ(s.getNoOrderIntent().size, 3);
    EXPECT_EQ(s.getYesOrderIntent().limit_price, 40);
    EXPECT_EQ(s.getNoOrderIntent().limit_price, 40);
}

TEST(Strategy, NoTradeWhenNoEdge) {
    Market m(1, 9999999999L, 10, 20, 0);
    m.snapshot_update(10, Kalshi, 59, 60, 59, 60, 5, 5, 5, 5);
    m.snapshot_update(20, Polymarket, 59, 60, 59, 60, 5, 5, 5, 5);

    Strategy s;
    s.strategize(m, 0, 10000, 10000, 10000);
    EXPECT_EQ(s.getYesOrderIntent().size, 0);
    EXPECT_EQ(s.getNoOrderIntent().size, 0);
}

TEST(Strategy, SkipsExpired) {
    Market m(1, 100L, 10, 20, 0);
    m.snapshot_update(10, Kalshi, 39, 40, 59, 60, 5, 5, 5, 5);
    m.snapshot_update(20, Polymarket, 39, 41, 39, 40, 5, 5, 5, 5);

    Strategy s;
    s.strategize(m, /*now*/ 200, 10000, 10000, 10000);
    EXPECT_EQ(s.getYesOrderIntent().size, 0);
}

TEST(Strategy, ClearsStaleIntentWhenEdgeDisappears) {
    Market m(1, 9999999999L, 10, 20, 0);
    m.snapshot_update(10, Kalshi, 39, 40, 59, 60, 5, 5, 5, 5);
    m.snapshot_update(20, Polymarket, 39, 41, 39, 40, 5, 5, 5, 5);

    Strategy s;
    s.strategize(m, 0, 10000, 10000, 10000);
    EXPECT_EQ(s.getYesOrderIntent().size, 3);
    EXPECT_EQ(s.getNoOrderIntent().size, 3);

    m.snapshot_update(20, Polymarket, 59, 60, 59, 60, 5, 5, 5, 5);
    s.strategize(m, 0, 10000, 10000, 10000);
    EXPECT_EQ(s.getYesOrderIntent().size, 0);
    EXPECT_EQ(s.getNoOrderIntent().size, 0);
}
