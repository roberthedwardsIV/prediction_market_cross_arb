#include <gtest/gtest.h>
#include "portfolio.hpp"
#include "execution.hpp"

TEST(Portfolio, FillDropsCashAndRaisesExposure) {
    PortfolioManager book;
    book.Register(1);
    book.setKalshiCash(10000);

    Fill f{};
    f.market_id = 1;
    f.venue_id = 1;
    f.venue = Kalshi;
    f.side = YesSided;
    f.size = 1;
    f.price = 40;
    f.buy = true;

    int fee = takerFee(Kalshi, 40, 1);
    int spent = 40 + fee;

    EXPECT_EQ(book.updatePortfolio(f), FillApply::Applied);

    EXPECT_EQ(book.kalshiCash(), 10000 - spent);
    EXPECT_EQ(book.getPortfolio().kalshi_exposure, spent);
    EXPECT_EQ(book.getPositionByMarketId(1).yes_count, 1);
}

TEST(Portfolio, UnregisteredFillIsLoud) {
    PortfolioManager book;
    book.setKalshiCash(10000);

    Fill f{};
    f.market_id = 99;
    f.venue_id = 1;
    f.venue = Kalshi;
    f.side = YesSided;
    f.size = 5;
    f.price = 40;
    f.buy = true;

    EXPECT_EQ(book.updatePortfolio(f), FillApply::Unregistered);
    EXPECT_EQ(book.kalshiCash(), 10000);
    EXPECT_EQ(book.getPositionByMarketId(99).yes_count, 0);
    EXPECT_EQ(book.getPortfolio().kalshi_exposure, 0);
}

TEST(Portfolio, SellYesReturnsCashAndDropsCount) {
    PortfolioManager book;
    book.Register(1);
    book.setKalshiCash(10000);

    Fill buy{};
    buy.market_id = 1;
    buy.venue_id = 1;
    buy.venue = Kalshi;
    buy.side = YesSided;
    buy.size = 2;
    buy.price = 40;
    buy.buy = true;
    ASSERT_EQ(book.updatePortfolio(buy), FillApply::Applied);

    Fill sell = buy;
    sell.buy = false;
    sell.size = 2;
    sell.price = 30;
    int fee = takerFee(Kalshi, 30, 2);
    int cash_before = book.kalshiCash();
    ASSERT_EQ(book.updatePortfolio(sell), FillApply::Applied);
    EXPECT_EQ(book.getPositionByMarketId(1).yes_count, 0);
    EXPECT_EQ(book.kalshiCash(), cash_before + 2 * 30 - fee);
}

TEST(Portfolio, KalshiCoverNoSpendsYesPrice) {
    PortfolioManager book;
    book.Register(1);
    book.setKalshiCash(10000);

    Fill buy{};
    buy.market_id = 1;
    buy.venue_id = 1;
    buy.venue = Kalshi;
    buy.side = NoSided;
    buy.size = 1;
    buy.price = 40;
    buy.buy = true;
    ASSERT_EQ(book.updatePortfolio(buy), FillApply::Applied);

    Fill cover = buy;
    cover.buy = false;
    cover.price = 55;
    int fee = takerFee(Kalshi, 55, 1);
    int cash_before = book.kalshiCash();
    ASSERT_EQ(book.updatePortfolio(cover), FillApply::Applied);
    EXPECT_EQ(book.getPositionByMarketId(1).no_count, 0);
    EXPECT_EQ(book.kalshiCash(), cash_before - 55 - fee);
}

TEST(Fill, MissingCountDoesNotInventSize) {
    int size = 99;
    int price = 100;
    EXPECT_FALSE(takeReportedFill(0, 50, 40, size, price));
    EXPECT_EQ(size, 0);
    EXPECT_EQ(price, 0);

    EXPECT_FALSE(takeReportedFill(-1, 50, 40, size, price));
    EXPECT_EQ(size, 0);

    EXPECT_TRUE(takeReportedFill(3, 0, 41, size, price));
    EXPECT_EQ(size, 3);
    EXPECT_EQ(price, 41);

    EXPECT_TRUE(takeReportedFill(2, 55, 41, size, price));
    EXPECT_EQ(size, 2);
    EXPECT_EQ(price, 55);
}
