#pragma once
#include <algorithm>
#include <atomic>
#include <mutex>
#include <string>
#include "portfolio.hpp"
#include "strategy.hpp"

enum class RiskReason {
    Ok,
    ContractCap,
    VenueCash,
    VenueReserve,
    NoCash,
    Allocation,
    NoEquity,
    MarketPct,
    InFlight,
    BelowMinEdge,
    Halted
};

inline std::string ReasonLogger(RiskReason reason) {
    switch (reason) {
        case RiskReason::Ok:
            return "Ok";
        case RiskReason::ContractCap:
            return "Contract cap reached";
        case RiskReason::VenueCash:
            return "Venue cash insufficient";
        case RiskReason::VenueReserve:
            return "Venue reserves too low";
        case RiskReason::NoCash:
            return "No cash";
        case RiskReason::Allocation:
            return "Maximum cash allocation reached";
        case RiskReason::NoEquity:
            return "No equity";
        case RiskReason::MarketPct:
            return "Market percentage limit reached";
        case RiskReason::InFlight:
            return "In-flight order";
        case RiskReason::BelowMinEdge:
            return "Below min edge at size";
        case RiskReason::Halted:
            return "Kill switch halted";
    }
    return "Unknown";
}

struct RiskLimits {
    float max_market_pct;
    float max_allocation_pct;
    float venue_reserve_pct;
    int max_contracts_per_market;
    float chase_reserve;
    float max_drawdown;
};

struct RiskState {
    std::atomic<bool> halted{false};
    std::mutex mtx;
    float realized_loss = 0.0f;

    bool isHalted() const { return halted.load(); }

    void clearForTests() {
        halted.store(false);
        std::lock_guard<std::mutex> lock(mtx);
        realized_loss = 0.0f;
    }
};

inline bool riskNoteFill(RiskState& state, const RiskLimits& limits, const Fill& fill) {
    if (state.isHalted()) return false;
    float fee = takerFee(fill.venue, fill.price, fill.size);
    bool newly_tripped = false;
    {
        std::lock_guard<std::mutex> lock(state.mtx);
        state.realized_loss += fee;
        if (state.realized_loss >= limits.max_drawdown) {
            bool was = state.halted.exchange(true);
            newly_tripped = !was;
        }
    }
    return newly_tripped;
}

inline float nextChasePrice(float current) {
    return std::round((current + 0.01f) * 100.0f) / 100.0f;
}

inline float chaseCeiling(float other_price, int other_venue, int chase_venue, float chase_reserve) {
    float room = 1.0f - chase_reserve - other_price - takerFee(other_venue, other_price, 1);
    float best = 0.0f;
    for (int c = 1; c <= 99; c++) {
        float p = static_cast<float>(c) / 100.0f;
        if (p + takerFee(chase_venue, p, 1) <= room + 1e-6f) best = p;
    }
    return best;
}

inline bool canBumpChase(float current, float ceiling) {
    return nextChasePrice(current) <= ceiling + 1e-6f;
}

inline float legCost(const OrderIntent& leg, int size) {
    return (size * leg.limit_price) + takerFee(leg.venue, leg.limit_price, size);
}

inline bool pairProfitableAtSize(const OrderIntent& yes, const OrderIntent& no, int size) {
    if (size < 1) return false;
    return legCost(yes, size) + legCost(no, size) < static_cast<float>(size);
}

template <class CostFn>
inline int fitSize(int size, float cap, CostFn cost) {
    if (size < 1 || cap <= 0.0f) return 0;
    int n = size;
    float unit = cost(1);
    if (unit > 0.0f) {
        int est = static_cast<int>(cap / unit);
        if (est < n) n = est;
    }
    if (n < 0) n = 0;
    while (n >= 1 && cost(n) > cap) n--;
    while (n < size && cost(n + 1) <= cap) n++;
    return n;
}

inline RiskReason approve_pair(OrderIntent& yes, OrderIntent& no, const Portfolio& p, const RiskLimits& limits,
                               RiskState* gate = nullptr) {
    auto reject = [&](RiskReason r) {
        yes.size = 0;
        no.size = 0;
        return r;
    };
    if (gate && gate->isHalted()) return reject(RiskReason::Halted);
    auto yesCost = [&](int n) { return legCost(yes, n); };
    auto noCost = [&](int n) { return legCost(no, n); };
    auto pairCost = [&](int n) { return legCost(yes, n) + legCost(no, n); };
    bool same_venue = yes.venue == no.venue;

    Position ypos = p.positionFor(yes.market_id);
    Position npos = (no.market_id == yes.market_id) ? ypos : p.positionFor(no.market_id);

    int size = std::min(yes.size, no.size);
    int room = limits.max_contracts_per_market - (ypos.yes_count + npos.no_count);
    if (2 * size > room) size = room / 2;
    if (size < 1) return reject(RiskReason::ContractCap);

    if (same_venue) {
        size = fitSize(size, p.venueCash(yes.venue), pairCost);
    } else {
        size = fitSize(size, p.venueCash(yes.venue), yesCost);
        size = fitSize(size, p.venueCash(no.venue), noCost);
    }
    if (size < 1) return reject(RiskReason::VenueCash);

    float keep = 1.0f - limits.venue_reserve_pct;
    auto reserveRoom = [&](int venue) {
        float cash = p.venueCashGross(venue);
        float exp = p.venueExposure(venue);
        float reserved = p.venueReserved(venue);
        return (cash + exp) * keep - exp - reserved;
    };
    if (same_venue) {
        size = fitSize(size, reserveRoom(yes.venue), pairCost);
    } else {
        size = fitSize(size, reserveRoom(yes.venue), yesCost);
        size = fitSize(size, reserveRoom(no.venue), noCost);
    }
    if (size < 1) return reject(RiskReason::VenueReserve);

    float total_cash = p.totalCash();
    float total_exposure = p.totalExposure();
    if (total_cash == 0.0f) return reject(RiskReason::NoCash);

    size = fitSize(size, (total_cash - p.totalReserved()) * limits.max_allocation_pct - total_exposure, pairCost);
    if (size < 1) return reject(RiskReason::Allocation);

    float total_equity = total_cash + total_exposure;
    if (total_equity == 0.0f) return reject(RiskReason::NoEquity);

    float market_exposure =
        (ypos.average_yes_price * ypos.yes_count) +
        (npos.average_no_price * npos.no_count);
    size = fitSize(size, total_equity * limits.max_market_pct - market_exposure, pairCost);
    if (size < 1) return reject(RiskReason::MarketPct);

    while (size >= 1 && !pairProfitableAtSize(yes, no, size)) {
        size -= 1;
    }
    if (size < 1) return reject(RiskReason::BelowMinEdge);

    yes.size = size;
    no.size = size;
    return RiskReason::Ok;
}

inline RiskReason approve_pair(OrderIntent& yes, OrderIntent& no, PortfolioManager& book, const RiskLimits& limits,
                               RiskState* gate = nullptr) {
    return approve_pair(yes, no, book.getPortfolio(), limits, gate);
}

inline bool reserve_pair(PortfolioManager& book, const OrderIntent& yes, const OrderIntent& no) {
    float yes_cost = legCost(yes, yes.size);
    float no_cost = legCost(no, no.size);
    return book.tryReserveTwo(yes.venue, yes_cost, no.venue, no_cost);
}

inline void release_pair(PortfolioManager& book, const OrderIntent& yes, const OrderIntent& no) {
    float yes_cost = legCost(yes, yes.size);
    float no_cost = legCost(no, no.size);
    book.releaseReserveTwo(yes.venue, yes_cost, no.venue, no_cost);
}

inline bool reserve_leg(PortfolioManager& book, const OrderIntent& leg) {
    return book.tryReserve(leg.venue, legCost(leg, leg.size));
}

inline void release_leg(PortfolioManager& book, const OrderIntent& leg) {
    book.releaseReserve(leg.venue, legCost(leg, leg.size));
}

#include <cstdio>
#include <unordered_map>
#include "kalshi_env.hpp"

inline RiskLimits defaultRiskLimits() {
    return RiskLimits{0.70f, 0.80f, 0.10f, 100, 0.01f, 10.0f};
}

inline float riskEnvFloat(const std::string& raw, float fallback) {
    if (raw.empty()) return fallback;
    try {
        return std::stof(raw);
    } catch (...) {
        return fallback;
    }
}

inline int riskEnvInt(const std::string& raw, int fallback) {
    if (raw.empty()) return fallback;
    try {
        return std::stoi(raw);
    } catch (...) {
        return fallback;
    }
}

inline std::string riskEnvLookup(const std::unordered_map<std::string, std::string>* overrides,
                                 const char* key) {
    if (overrides) {
        auto it = overrides->find(key);
        if (it != overrides->end()) return it->second;
    }
    return envValue(key);
}

inline RiskLimits loadRiskLimits(const std::unordered_map<std::string, std::string>* overrides = nullptr) {
    RiskLimits d = defaultRiskLimits();
    RiskLimits lim;
    lim.max_market_pct = riskEnvFloat(riskEnvLookup(overrides, "ASSISI_RISK_MAX_MARKET_PCT"), d.max_market_pct);
    lim.max_allocation_pct = riskEnvFloat(riskEnvLookup(overrides, "ASSISI_RISK_MAX_ALLOCATION_PCT"), d.max_allocation_pct);
    lim.venue_reserve_pct = riskEnvFloat(riskEnvLookup(overrides, "ASSISI_RISK_VENUE_RESERVE_PCT"), d.venue_reserve_pct);
    lim.max_contracts_per_market = riskEnvInt(riskEnvLookup(overrides, "ASSISI_RISK_MAX_CONTRACTS"), d.max_contracts_per_market);
    lim.chase_reserve = riskEnvFloat(riskEnvLookup(overrides, "ASSISI_RISK_CHASE_RESERVE"), d.chase_reserve);
    lim.max_drawdown = riskEnvFloat(riskEnvLookup(overrides, "ASSISI_RISK_MAX_DRAWDOWN"), d.max_drawdown);
    return lim;
}

inline std::string riskLimitsLogLine(const RiskLimits& lim) {
    char buf[256];
    std::snprintf(buf, sizeof(buf),
        "risk limits max_market_pct=%.2f max_allocation_pct=%.2f venue_reserve_pct=%.2f "
        "max_contracts=%d chase_reserve=%.2f max_drawdown=%.2f",
        lim.max_market_pct, lim.max_allocation_pct, lim.venue_reserve_pct,
        lim.max_contracts_per_market, lim.chase_reserve, lim.max_drawdown);
    return std::string(buf);
}
