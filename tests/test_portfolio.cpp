#include <gtest/gtest.h>
#include "portfolio.hpp"
#include "execution.hpp"

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

    EXPECT_EQ(book.updatePortfolio(f), FillApply::Applied);

    EXPECT_FLOAT_EQ(book.kalshiCash(), 100.f - spent);
    EXPECT_FLOAT_EQ(book.getPortfolio().kalshi_exposure, spent);
    EXPECT_EQ(book.getPositionByMarketId(1).yes_count, 1);
}

TEST(Portfolio, UnregisteredFillIsLoud) {
    PortfolioManager book;
    book.setKalshiCash(100.f);

    Fill f{};
    f.market_id = 99;
    f.venue_id = 1;
    f.venue = Kalshi;
    f.side = YesSided;
    f.size = 5;
    f.price = 0.40f;
    f.buy = true;

    EXPECT_EQ(book.updatePortfolio(f), FillApply::Unregistered);
    EXPECT_FLOAT_EQ(book.kalshiCash(), 100.f);
    EXPECT_EQ(book.getPositionByMarketId(99).yes_count, 0);
    EXPECT_FLOAT_EQ(book.getPortfolio().kalshi_exposure, 0.f);
}

TEST(Fill, MissingCountDoesNotInventSize) {
    int size = 99;
    float price = 1.0f;
    EXPECT_FALSE(takeReportedFill(0.0f, 0.50f, 0.40f, size, price));
    EXPECT_EQ(size, 0);
    EXPECT_FLOAT_EQ(price, 0.0f);

    EXPECT_FALSE(takeReportedFill(-1.0f, 0.50f, 0.40f, size, price));
    EXPECT_EQ(size, 0);

    EXPECT_TRUE(takeReportedFill(3.0f, 0.0f, 0.41f, size, price));
    EXPECT_EQ(size, 3);
    EXPECT_FLOAT_EQ(price, 0.41f);

    EXPECT_TRUE(takeReportedFill(2.0f, 0.55f, 0.41f, size, price));
    EXPECT_EQ(size, 2);
    EXPECT_FLOAT_EQ(price, 0.55f);
}
