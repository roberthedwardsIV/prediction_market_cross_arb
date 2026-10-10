#include <gtest/gtest.h>
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

static RiskLimits looseLimits() {
    return RiskLimits{90, 90, 10, 100, 1, 1000};
}

TEST(Risk, ApprovesCheapPair) {
    PortfolioManager book;
    book.Register(1);
    book.setKalshiCash(10000);
    book.setPolymarketCash(10000);
    book.setGeminiCash(10000);

    auto yes = makeLeg(1, Kalshi, 1, 40);
    auto no  = makeLeg(1, Polymarket, 1, 40);
    EXPECT_EQ(approve_pair(yes, no, book, looseLimits()), RiskReason::Ok);
}

TEST(Risk, RejectsInsufficientVenueCash) {
    PortfolioManager book;
    book.Register(1);
    book.setKalshiCash(0);
    book.setPolymarketCash(10000);
    book.setGeminiCash(10000);

    auto yes = makeLeg(1, Kalshi, 1, 40);
    auto no  = makeLeg(1, Polymarket, 1, 40);
    EXPECT_EQ(approve_pair(yes, no, book, looseLimits()), RiskReason::VenueCash);
}

TEST(Risk, RejectsOverContractCap) {
    PortfolioManager book;
    book.Register(1);
    book.setKalshiCash(10000);
    book.setPolymarketCash(10000);
    book.setGeminiCash(10000);

    RiskLimits lim = looseLimits();
    lim.max_contracts_per_market = 1;

    auto yes = makeLeg(1, Kalshi, 1, 40);
    auto no  = makeLeg(1, Polymarket, 1, 40);
    EXPECT_EQ(approve_pair(yes, no, book, lim), RiskReason::ContractCap);
}

TEST(Risk, ShrinksToFitKalshiCash) {
    PortfolioManager book;
    book.Register(1);
    book.setKalshiCash(100);
    book.setPolymarketCash(10000);
    book.setGeminiCash(10000);

    RiskLimits lim = looseLimits();
    lim.venue_reserve_pct = 0;

    const int start = 10;
    auto yes = makeLeg(1, Kalshi, start, 40);
    auto no = makeLeg(1, Polymarket, start, 40);
    EXPECT_EQ(approve_pair(yes, no, book, lim), RiskReason::Ok);
    EXPECT_LT(yes.size, start);
    EXPECT_GE(yes.size, 1);
    EXPECT_EQ(yes.size, no.size);
}

TEST(Risk, ShrinksToFitPolymarketCash) {
    PortfolioManager book;
    book.Register(1);
    book.setKalshiCash(10000);
    book.setPolymarketCash(100);
    book.setGeminiCash(10000);

    RiskLimits lim = looseLimits();
    lim.venue_reserve_pct = 0;

    const int start = 10;
    auto yes = makeLeg(1, Polymarket, start, 40);
    auto no = makeLeg(1, Kalshi, start, 40);
    EXPECT_EQ(approve_pair(yes, no, book, lim), RiskReason::Ok);
    EXPECT_LT(yes.size, start);
    EXPECT_GE(yes.size, 1);
    EXPECT_EQ(yes.size, no.size);
}

TEST(Risk, ShrinksToFitGeminiCash) {
    PortfolioManager book;
    book.Register(1);
    book.setKalshiCash(10000);
    book.setPolymarketCash(10000);
    book.setGeminiCash(100);

    RiskLimits lim = looseLimits();
    lim.venue_reserve_pct = 0;

    const int start = 10;
    auto yes = makeLeg(1, Gemini, start, 40);
    auto no = makeLeg(1, Polymarket, start, 40);
    EXPECT_EQ(approve_pair(yes, no, book, lim), RiskReason::Ok);
    EXPECT_LT(yes.size, start);
    EXPECT_GE(yes.size, 1);
    EXPECT_EQ(yes.size, no.size);
}

TEST(Risk, FitSizeMatchesBruteForce) {
    const int venues[] = {Kalshi, Polymarket, Gemini};
    const int prices[] = {1, 7, 33, 50, 66, 93, 99};
    const int caps[] = {0, 1, 37, 100, 321, 999, 5000, 10000, 100000};
    const int size = 120;
    for (int venue : venues) {
        for (int price : prices) {
            auto leg = makeLeg(1, venue, size, price);
            auto cost = [&](int n) { return legCost(leg, n); };
            for (int cap : caps) {
                int brute = 0;
                for (int n = size; n >= 1; n--) {
                    if (cost(n) <= cap) { brute = n; break; }
                }
                EXPECT_EQ(fitSize(size, cap, cost), brute)
                    << "venue=" << venue << " price=" << price << " cap=" << cap;
            }
        }
    }
}

TEST(Risk, ReservationBlocksSecondPair) {
    PortfolioManager book;
    book.Register(1);
    book.Register(2);
    book.setKalshiCash(50);
    book.setPolymarketCash(10000);
    book.setGeminiCash(10000);

    RiskLimits lim = looseLimits();
    lim.venue_reserve_pct = 0;

    auto yes1 = makeLeg(1, Kalshi, 1, 40);
    auto no1 = makeLeg(1, Polymarket, 1, 40);
    EXPECT_EQ(approve_pair(yes1, no1, book, lim), RiskReason::Ok);
    EXPECT_TRUE(reserve_pair(book, yes1, no1));

    auto yes2 = makeLeg(2, Kalshi, 1, 40);
    auto no2 = makeLeg(2, Polymarket, 1, 40);
    EXPECT_EQ(approve_pair(yes2, no2, book, lim), RiskReason::VenueCash);

    release_pair(book, yes1, no1);
    yes2 = makeLeg(2, Kalshi, 1, 40);
    no2 = makeLeg(2, Polymarket, 1, 40);
    EXPECT_EQ(approve_pair(yes2, no2, book, lim), RiskReason::Ok);
    EXPECT_TRUE(reserve_pair(book, yes2, no2));
    release_pair(book, yes2, no2);
}

TEST(Risk, ChaseCeiling) {
    const int reserve = 1;
    int ceiling = chaseCeiling(40, Kalshi, Polymarket, reserve);
    EXPECT_GT(ceiling, 0);
    EXPECT_LT(ceiling, 99);

    int fees = takerFee(Kalshi, 40, 1) + takerFee(Polymarket, ceiling, 1);
    EXPECT_LE(40 + ceiling + fees + reserve, kDollarCents);

    int over = nextChasePrice(ceiling);
    fees = takerFee(Kalshi, 40, 1) + takerFee(Polymarket, over, 1);
    EXPECT_GT(40 + over + fees + reserve, kDollarCents);

    EXPECT_FALSE(canBumpChase(ceiling, ceiling));
    if (ceiling >= 2) {
        EXPECT_TRUE(canBumpChase(ceiling - 1, ceiling));
    }

    int tight = chaseCeiling(90, Kalshi, Polymarket, reserve);
    EXPECT_LT(tight, 99);
    EXPECT_FALSE(canBumpChase(tight, tight));
}

TEST(Risk, RejectsBelowMinEdge) {
    PortfolioManager book;
    book.Register(1);
    book.setKalshiCash(10000);
    book.setPolymarketCash(10000);
    book.setGeminiCash(10000);

    auto yes = makeLeg(1, Kalshi, 5, 50);
    auto no = makeLeg(1, Polymarket, 5, 50);
    EXPECT_FALSE(pairProfitableAtSize(yes, no, 1));
    EXPECT_EQ(approve_pair(yes, no, book, looseLimits()), RiskReason::BelowMinEdge);
    EXPECT_EQ(yes.size, 0);
    EXPECT_EQ(no.size, 0);
}

TEST(Risk, FeeNonlinearity) {
    PortfolioManager book;
    book.Register(1);
    book.setKalshiCash(100000);
    book.setPolymarketCash(100000);
    book.setGeminiCash(100000);

    RiskLimits lim = looseLimits();
    lim.venue_reserve_pct = 0;

    const int prices[] = {20, 35, 48, 49};
    for (int py : prices) {
        for (int pn : prices) {
            if (!pairProfitableAtSize(makeLeg(1, Kalshi, 1, py), makeLeg(1, Polymarket, 1, pn), 1)) {
                continue;
            }
            auto yes = makeLeg(1, Kalshi, 40, py);
            auto no = makeLeg(1, Polymarket, 40, pn);
            ASSERT_EQ(approve_pair(yes, no, book, lim), RiskReason::Ok)
                << "py=" << py << " pn=" << pn;
            EXPECT_GE(yes.size, 1);
            EXPECT_EQ(yes.size, no.size);
            EXPECT_TRUE(pairProfitableAtSize(yes, no, yes.size))
                << "size=" << yes.size << " py=" << py << " pn=" << pn
                << " cost=" << (legCost(yes, yes.size) + legCost(no, no.size));
        }
    }

    for (int n = 1; n <= 25; n++) {
        int unit = takerFee(Kalshi, 50, 1);
        int at_n = takerFee(Kalshi, 50, n);
        EXPECT_LE(at_n, unit * n)
            << "Kalshi ceil overestimates per-contract at n=1; n=" << n;
    }
}

TEST(Risk, KillSwitchTrips) {
    PortfolioManager book;
    book.Register(1);
    book.setKalshiCash(10000);
    book.setPolymarketCash(10000);
    book.setGeminiCash(10000);

    RiskLimits lim = looseLimits();
    lim.max_drawdown = 5;

    RiskState gate;
    Fill f{};
    f.market_id = 1;
    f.venue_id = 1;
    f.venue = Kalshi;
    f.side = YesSided;
    f.size = 1;
    f.price = 50;
    f.buy = true;

    bool tripped = false;
    for (int i = 0; i < 20 && !tripped; i++) {
        tripped = riskNoteFill(gate, lim, f);
    }
    EXPECT_TRUE(tripped);
    EXPECT_TRUE(gate.isHalted());
    EXPECT_FALSE(riskNoteFill(gate, lim, f));

    auto yes = makeLeg(1, Kalshi, 1, 40);
    auto no = makeLeg(1, Polymarket, 1, 40);
    EXPECT_EQ(approve_pair(yes, no, book, lim, &gate), RiskReason::Halted);
    EXPECT_EQ(yes.size, 0);
    EXPECT_EQ(no.size, 0);
}

TEST(Risk, LoadLimitsFromEnvMap) {
    RiskLimits def = defaultRiskLimits();
    std::unordered_map<std::string, std::string> blank;
    blank["ASSISI_RISK_MAX_MARKET_PCT"] = "";
    blank["ASSISI_RISK_MAX_ALLOCATION_PCT"] = "";
    blank["ASSISI_RISK_VENUE_RESERVE_PCT"] = "";
    blank["ASSISI_RISK_MAX_CONTRACTS"] = "";
    blank["ASSISI_RISK_CHASE_RESERVE"] = "";
    blank["ASSISI_RISK_MAX_DRAWDOWN"] = "";
    RiskLimits unset = loadRiskLimits(&blank);
    EXPECT_EQ(unset.max_market_pct, def.max_market_pct);
    EXPECT_EQ(unset.max_allocation_pct, def.max_allocation_pct);
    EXPECT_EQ(unset.venue_reserve_pct, def.venue_reserve_pct);
    EXPECT_EQ(unset.max_contracts_per_market, def.max_contracts_per_market);
    EXPECT_EQ(unset.chase_reserve, def.chase_reserve);
    EXPECT_EQ(unset.max_drawdown, def.max_drawdown);

    std::unordered_map<std::string, std::string> ov;
    ov["ASSISI_RISK_MAX_DRAWDOWN"] = "3.25";
    ov["ASSISI_RISK_MAX_CONTRACTS"] = "42";
    ov["ASSISI_RISK_VENUE_RESERVE_PCT"] = "0.25";
    RiskLimits lim = loadRiskLimits(&ov);
    EXPECT_EQ(lim.max_drawdown, 325);
    EXPECT_EQ(lim.max_contracts_per_market, 42);
    EXPECT_EQ(lim.venue_reserve_pct, 25);
    EXPECT_EQ(lim.max_market_pct, def.max_market_pct);

    std::string line = riskLimitsLogLine(lim);
    EXPECT_NE(line.find("max_drawdown=3.25"), std::string::npos);
    EXPECT_NE(line.find("max_contracts=42"), std::string::npos);
}
