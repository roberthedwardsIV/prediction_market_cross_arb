#include <gtest/gtest.h>
#include "tape.hpp"

TEST(Tape, ParsesValidLine) {
    TapeTick t{};
    std::string line =
        "1000,1,1,0.40,0.42,0.58,0.60,10,10,10,10";
    ASSERT_TRUE(tapeParseLine(line, t));
    EXPECT_EQ(t.ts_ms, 1000);
    EXPECT_EQ(t.market_id, 1);
    EXPECT_EQ(t.venue, Kalshi);
    EXPECT_FLOAT_EQ(t.yes_ask, 0.42f);
}

TEST(Tape, RejectsHeader) {
    TapeTick t{};
    EXPECT_FALSE(tapeParseLine("ts_ms,market_id,venue,...", t));
}

TEST(Tape, RejectsEmpty) {
    TapeTick t{};
    EXPECT_FALSE(tapeParseLine("", t));
}