#include <gtest/gtest.h>
#include <atomic>
#include "live_busy.hpp"
#include "kalshi_env.hpp"
#include "ws_backoff.hpp"
#include "portfolio.hpp"
#include "risk.hpp"

static OrderIntent makeLeg(long market_id, int venue, int size, int price) {
    OrderIntent o{};
    o.market_id = market_id;
    o.venue_id = 1;
    o.venue = venue;
    o.buy = true;
    o.side = (venue == Kalshi ? YesSided : NoSided);
    o.size = size;
    o.limit_price = price;
    return o;
}

TEST(LiveBusy, TwoMarketsCanBothBeBusy) {
    LiveBusy busy;
    EXPECT_TRUE(busy.tryAcquire(122));
    EXPECT_TRUE(busy.tryAcquire(125));
}

TEST(LiveBusy, SameMarketRefusedWhileBusy) {
    LiveBusy busy;
    EXPECT_TRUE(busy.tryAcquire(122));
    EXPECT_FALSE(busy.tryAcquire(122));
    busy.release(122);
    EXPECT_TRUE(busy.tryAcquire(122));
}

TEST(LiveBusy, ZeroIdRefused) {
    LiveBusy busy;
    EXPECT_FALSE(busy.tryAcquire(0));
}

TEST(WsReconnect, BackoffDoublesThenCaps) {
    EXPECT_EQ(nextWsBackoffMs(0), 1000);
    EXPECT_EQ(nextWsBackoffMs(1000), 2000);
    EXPECT_EQ(nextWsBackoffMs(2000), 4000);
    EXPECT_EQ(nextWsBackoffMs(16000), 30000);
    EXPECT_EQ(nextWsBackoffMs(30000), 30000);
}

TEST(Clock, TightLoopNeverRepeats) {
    std::atomic<long long> last{0};
    long long prev = nextMonotonicMs(last);
    for (int i = 0; i < 1000; i++) {
        long long now = nextMonotonicMs(last);
        EXPECT_LT(prev, now);
        prev = now;
    }
}

TEST(Risk, TwoMarketsCanReserveWhenCashEnough) {
    PortfolioManager book;
    book.Register(122);
    book.Register(125);
    book.setKalshiCash(10000);
    book.setPolymarketCash(10000);
    book.setGeminiCash(10000);

    RiskLimits lim{90, 90, 0, 100, 1, 1000};
    auto yes1 = makeLeg(122, Kalshi, 1, 40);
    auto no1 = makeLeg(122, Polymarket, 1, 40);
    auto yes2 = makeLeg(125, Kalshi, 1, 40);
    auto no2 = makeLeg(125, Polymarket, 1, 40);

    EXPECT_EQ(approve_pair(yes1, no1, book, lim), RiskReason::Ok);
    EXPECT_TRUE(reserve_pair(book, yes1, no1));
    EXPECT_EQ(approve_pair(yes2, no2, book, lim), RiskReason::Ok);
    EXPECT_TRUE(reserve_pair(book, yes2, no2));
    release_pair(book, yes1, no1);
    release_pair(book, yes2, no2);
}
