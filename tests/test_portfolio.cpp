#include <gtest/gtest.h>
#include "portfolio.hpp"

TEST(Portfolio, FillDropsCashAndRaisesExposure) {
    PortfolioManager book;
    book.Register(1);
    book.setKalshiCash(100.f);

    Fill f{};
    f.market_id = 1;
    f.venue_id = 1;
    f.venue = Kalshi;
    f.side = YesSided;
    f.size = 1;
    f.price = 0.40f;
    f.buy = true;

    float fee = takerFee(Kalshi, 0.40f, 1);
    float spent = 0.40f + fee;

    book.updatePortfolio(f);

    EXPECT_FLOAT_EQ(book.kalshiCash(), 100.f - spent);
    EXPECT_FLOAT_EQ(book.getPortfolio().kalshi_exposure, spent);
    EXPECT_EQ(book.getPositionByMarketId(1).yes_count, 1);
}