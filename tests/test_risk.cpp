#include <gtest/gtest.h>
#include "portfolio.hpp"
#include "risk.hpp"

static OrderIntent makeLeg(long market_id, int venue, int size, float price) {
    OrderIntent o{};
    o.market_id = market_id;
    o.venue_id = 1;
    o.venue = venue;
    o.buy = true;
    o.side = (venue == Kalshi ? YesSided : NoSided); // side unused by approve_pair
    o.size = size;
    o.limit_price = price;
    return o;
}

static RiskLimits looseLimits() {
    return RiskLimits{0.90f, 0.90f, 0.10f, 100, 0.01f, 10.0f};
}

TEST(Risk, ApprovesCheapPair) {
    PortfolioManager book;
    book.Register(1);
    book.setKalshiCash(100.f);
    book.setPolymarketCash(100.f);
    book.setGeminiCash(100.f);

    auto yes = makeLeg(1, Kalshi, 1, 0.40f);
    auto no  = makeLeg(1, Polymarket, 1, 0.40f);
    EXPECT_EQ(approve_pair(yes, no, book, looseLimits()), RiskReason::Ok);
}

TEST(Risk, RejectsInsufficientVenueCash) {
    PortfolioManager book;
    book.Register(1);
    book.setKalshiCash(0.f);
    book.setPolymarketCash(100.f);
    book.setGeminiCash(100.f);

    auto yes = makeLeg(1, Kalshi, 1, 0.40f);
    auto no  = makeLeg(1, Polymarket, 1, 0.40f);
    EXPECT_EQ(approve_pair(yes, no, book, looseLimits()), RiskReason::VenueCash);
}

TEST(Risk, RejectsOverContractCap) {
    PortfolioManager book;
    book.Register(1);
    book.setKalshiCash(100.f);
    book.setPolymarketCash(100.f);
    book.setGeminiCash(100.f);

    RiskLimits lim = looseLimits();
    lim.max_contracts_per_market = 1; // yes.size+no.size = 2 > 1

    auto yes = makeLeg(1, Kalshi, 1, 0.40f);
    auto no  = makeLeg(1, Polymarket, 1, 0.40f);
    EXPECT_EQ(approve_pair(yes, no, book, lim), RiskReason::ContractCap);
}

TEST(Risk, ShrinksToFitKalshiCash) {
    PortfolioManager book;
    book.Register(1);
    book.setKalshiCash(1.0f);
    book.setPolymarketCash(100.f);
    book.setGeminiCash(100.f);

    RiskLimits lim = looseLimits();
    lim.venue_reserve_pct = 0.0f;

    const int start = 10;
    auto yes = makeLeg(1, Kalshi, start, 0.40f);
    auto no = makeLeg(1, Polymarket, start, 0.40f);
    EXPECT_EQ(approve_pair(yes, no, book, lim), RiskReason::Ok);
    EXPECT_LT(yes.size, start);
    EXPECT_GE(yes.size, 1);
    EXPECT_EQ(yes.size, no.size);
}

TEST(Risk, ShrinksToFitPolymarketCash) {
    PortfolioManager book;
    book.Register(1);
    book.setKalshiCash(100.f);
    book.setPolymarketCash(1.0f);
    book.setGeminiCash(100.f);

    RiskLimits lim = looseLimits();
    lim.venue_reserve_pct = 0.0f;

    const int start = 10;
    auto yes = makeLeg(1, Polymarket, start, 0.40f);
    auto no = makeLeg(1, Kalshi, start, 0.40f);
    EXPECT_EQ(approve_pair(yes, no, book, lim), RiskReason::Ok);
    EXPECT_LT(yes.size, start);
    EXPECT_GE(yes.size, 1);
    EXPECT_EQ(yes.size, no.size);
}

TEST(Risk, ShrinksToFitGeminiCash) {
    PortfolioManager book;
    book.Register(1);
    book.setKalshiCash(100.f);
    book.setPolymarketCash(100.f);
    book.setGeminiCash(1.0f);

    RiskLimits lim = looseLimits();
    lim.venue_reserve_pct = 0.0f;

    const int start = 10;
    auto yes = makeLeg(1, Gemini, start, 0.40f);
    auto no = makeLeg(1, Polymarket, start, 0.40f);
    EXPECT_EQ(approve_pair(yes, no, book, lim), RiskReason::Ok);
    EXPECT_LT(yes.size, start);
    EXPECT_GE(yes.size, 1);
    EXPECT_EQ(yes.size, no.size);
}

TEST(Risk, FitSizeMatchesBruteForce) {
    const int venues[] = {Kalshi, Polymarket, Gemini};
    const float prices[] = {0.01f, 0.07f, 0.33f, 0.50f, 0.66f, 0.93f, 0.99f};
    const float caps[] = {0.0f, 0.005f, 0.01f, 0.37f, 1.0f, 3.21f, 9.99f, 50.0f, 100.0f, 1000.0f};
    const int size = 120;
    for (int venue : venues) {
        for (float price : prices) {
            auto leg = makeLeg(1, venue, size, price);
            auto cost = [&](int n) { return legCost(leg, n); };
            for (float cap : caps) {
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
    book.setKalshiCash(0.50f);
    book.setPolymarketCash(100.f);
    book.setGeminiCash(100.f);

    RiskLimits lim = looseLimits();
    lim.venue_reserve_pct = 0.0f;

    auto yes1 = makeLeg(1, Kalshi, 1, 0.40f);
    auto no1 = makeLeg(1, Polymarket, 1, 0.40f);
    EXPECT_EQ(approve_pair(yes1, no1, book, lim), RiskReason::Ok);
    EXPECT_TRUE(reserve_pair(book, yes1, no1));

    auto yes2 = makeLeg(2, Kalshi, 1, 0.40f);
    auto no2 = makeLeg(2, Polymarket, 1, 0.40f);
    EXPECT_EQ(approve_pair(yes2, no2, book, lim), RiskReason::VenueCash);

    release_pair(book, yes1, no1);
    yes2 = makeLeg(2, Kalshi, 1, 0.40f);
    no2 = makeLeg(2, Polymarket, 1, 0.40f);
    EXPECT_EQ(approve_pair(yes2, no2, book, lim), RiskReason::Ok);
    EXPECT_TRUE(reserve_pair(book, yes2, no2));
    release_pair(book, yes2, no2);
}

TEST(Risk, ChaseCeiling) {
    const float reserve = 0.01f;
    float ceiling = chaseCeiling(0.40f, Kalshi, Polymarket, reserve);
    EXPECT_GT(ceiling, 0.0f);
    EXPECT_LT(ceiling, 0.99f);

    float fees = takerFee(Kalshi, 0.40f, 1) + takerFee(Polymarket, ceiling, 1);
    EXPECT_LE(0.40f + ceiling + fees + reserve, 1.0f + 1e-5f);

    float over = nextChasePrice(ceiling);
    fees = takerFee(Kalshi, 0.40f, 1) + takerFee(Polymarket, over, 1);
    EXPECT_GT(0.40f + over + fees + reserve, 1.0f - 1e-5f);

    EXPECT_FALSE(canBumpChase(ceiling, ceiling));
    if (ceiling >= 0.02f) {
        EXPECT_TRUE(canBumpChase(ceiling - 0.01f, ceiling));
    }

    float tight = chaseCeiling(0.90f, Kalshi, Polymarket, reserve);
    EXPECT_LT(tight, 0.99f);
    EXPECT_FALSE(canBumpChase(tight, tight));
}

TEST(Risk, RejectsBelowMinEdge) {
    PortfolioManager book;
    book.Register(1);
    book.setKalshiCash(100.f);
    book.setPolymarketCash(100.f);
    book.setGeminiCash(100.f);

    auto yes = makeLeg(1, Kalshi, 5, 0.50f);
    auto no = makeLeg(1, Polymarket, 5, 0.50f);
    EXPECT_FALSE(pairProfitableAtSize(yes, no, 1));
    EXPECT_EQ(approve_pair(yes, no, book, looseLimits()), RiskReason::BelowMinEdge);
    EXPECT_EQ(yes.size, 0);
    EXPECT_EQ(no.size, 0);
}

TEST(Risk, FeeNonlinearity) {
    PortfolioManager book;
    book.Register(1);
    book.setKalshiCash(1000.f);
    book.setPolymarketCash(1000.f);
    book.setGeminiCash(1000.f);

    RiskLimits lim = looseLimits();
    lim.venue_reserve_pct = 0.0f;

    const float prices[] = {0.20f, 0.35f, 0.48f, 0.49f};
    for (float py : prices) {
        for (float pn : prices) {
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
        float unit = takerFee(Kalshi, 0.50f, 1);
        float at_n = takerFee(Kalshi, 0.50f, n);
        EXPECT_LE(at_n, unit * static_cast<float>(n) + 1e-5f)
            << "Kalshi ceil overestimates per-contract at n=1; n=" << n;
    }
}

TEST(Risk, KillSwitchTrips) {
    PortfolioManager book;
    book.Register(1);
    book.setKalshiCash(100.f);
    book.setPolymarketCash(100.f);
    book.setGeminiCash(100.f);

    RiskLimits lim = looseLimits();
    lim.max_drawdown = 0.05f;

    RiskState gate;
    Fill f{};
    f.market_id = 1;
    f.venue_id = 1;
    f.venue = Kalshi;
    f.side = YesSided;
    f.size = 1;
    f.price = 0.50f;
    f.buy = true;

    bool tripped = false;
    for (int i = 0; i < 20 && !tripped; i++) {
        tripped = riskNoteFill(gate, lim, f);
    }
    EXPECT_TRUE(tripped);
    EXPECT_TRUE(gate.isHalted());
    EXPECT_FALSE(riskNoteFill(gate, lim, f));

    auto yes = makeLeg(1, Kalshi, 1, 0.40f);
    auto no = makeLeg(1, Polymarket, 1, 0.40f);
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
    EXPECT_FLOAT_EQ(unset.max_market_pct, def.max_market_pct);
    EXPECT_FLOAT_EQ(unset.max_allocation_pct, def.max_allocation_pct);
    EXPECT_FLOAT_EQ(unset.venue_reserve_pct, def.venue_reserve_pct);
    EXPECT_EQ(unset.max_contracts_per_market, def.max_contracts_per_market);
    EXPECT_FLOAT_EQ(unset.chase_reserve, def.chase_reserve);
    EXPECT_FLOAT_EQ(unset.max_drawdown, def.max_drawdown);

    std::unordered_map<std::string, std::string> ov;
    ov["ASSISI_RISK_MAX_DRAWDOWN"] = "3.25";
    ov["ASSISI_RISK_MAX_CONTRACTS"] = "42";
    ov["ASSISI_RISK_VENUE_RESERVE_PCT"] = "0.25";
    RiskLimits lim = loadRiskLimits(&ov);
    EXPECT_FLOAT_EQ(lim.max_drawdown, 3.25f);
    EXPECT_EQ(lim.max_contracts_per_market, 42);
    EXPECT_FLOAT_EQ(lim.venue_reserve_pct, 0.25f);
    EXPECT_FLOAT_EQ(lim.max_market_pct, def.max_market_pct);

    std::string line = riskLimitsLogLine(lim);
    EXPECT_NE(line.find("max_drawdown=3.25"), std::string::npos);
    EXPECT_NE(line.find("max_contracts=42"), std::string::npos);
}
