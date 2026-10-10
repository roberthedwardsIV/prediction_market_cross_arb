#pragma once
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <mutex>
#include <string>
#include <unordered_map>
#include "portfolio.hpp"
#include "strategy.hpp"
#include "kalshi_env.hpp"

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
    int max_market_pct;
    int max_allocation_pct;
    int venue_reserve_pct;
    int max_contracts_per_market;
    int chase_reserve;
    int max_drawdown;
};

struct RiskState {
    std::atomic<bool> halted{false};
    std::mutex mtx;
    int realized_loss = 0;

    bool isHalted() const { return halted.load(); }

    void clearForTests() {
        halted.store(false);
        std::lock_guard<std::mutex> lock(mtx);
        realized_loss = 0;
    }
};

inline bool riskNoteFill(RiskState& state, const RiskLimits& limits, const Fill& fill) {
    if (state.isHalted()) return false;
    int fee = takerFee(fill.venue, fill.price, fill.size);
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

inline int nextChasePrice(int current) {
    return current + 1;
}

inline int chaseCeiling(int other_price, int other_venue, int chase_venue, int chase_reserve) {
    int room = kDollarCents - chase_reserve - other_price - takerFee(other_venue, other_price, 1);
    int best = 0;
    for (int c = 1; c <= 99; c++) {
        if (c + takerFee(chase_venue, c, 1) <= room) best = c;
    }
    return best;
}

inline bool canBumpChase(int current, int ceiling) {
    return nextChasePrice(current) <= ceiling;
}

inline int legCost(const OrderIntent& leg, int size) {
    return (size * leg.limit_price) + takerFee(leg.venue, leg.limit_price, size);
}

inline bool pairProfitableAtSize(const OrderIntent& yes, const OrderIntent& no, int size) {
    if (size < 1) return false;
    return legCost(yes, size) + legCost(no, size) < size * kDollarCents;
}

template <class CostFn>
inline int fitSize(int size, int cap, CostFn cost) {
    if (size < 1 || cap <= 0) return 0;
    int n = size;
    int unit = cost(1);
    if (unit > 0) {
        int est = cap / unit;
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

    int keep = 100 - limits.venue_reserve_pct;
    auto reserveRoom = [&](int venue) {
        int cash = p.venueCashGross(venue);
        int exp = p.venueExposure(venue);
        int reserved = p.venueReserved(venue);
        return (cash + exp) * keep / 100 - exp - reserved;
    };
    if (same_venue) {
        size = fitSize(size, reserveRoom(yes.venue), pairCost);
    } else {
        size = fitSize(size, reserveRoom(yes.venue), yesCost);
        size = fitSize(size, reserveRoom(no.venue), noCost);
    }
    if (size < 1) return reject(RiskReason::VenueReserve);

    int total_cash = p.totalCash();
    int total_exposure = p.totalExposure();
    if (total_cash == 0) return reject(RiskReason::NoCash);

    size = fitSize(size, (total_cash - p.totalReserved()) * limits.max_allocation_pct / 100 - total_exposure, pairCost);
    if (size < 1) return reject(RiskReason::Allocation);

    int total_equity = total_cash + total_exposure;
    if (total_equity == 0) return reject(RiskReason::NoEquity);

    int market_exposure =
        (ypos.average_yes_price * ypos.yes_count) +
        (npos.average_no_price * npos.no_count);
    size = fitSize(size, total_equity * limits.max_market_pct / 100 - market_exposure, pairCost);
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
    int yes_cost = legCost(yes, yes.size);
    int no_cost = legCost(no, no.size);
    return book.tryReserveTwo(yes.venue, yes_cost, no.venue, no_cost);
}

inline void release_pair(PortfolioManager& book, const OrderIntent& yes, const OrderIntent& no) {
    int yes_cost = legCost(yes, yes.size);
    int no_cost = legCost(no, no.size);
    book.releaseReserveTwo(yes.venue, yes_cost, no.venue, no_cost);
}

inline bool reserve_leg(PortfolioManager& book, const OrderIntent& leg) {
    return book.tryReserve(leg.venue, legCost(leg, leg.size));
}

inline void release_leg(PortfolioManager& book, const OrderIntent& leg) {
    book.releaseReserve(leg.venue, legCost(leg, leg.size));
}

inline RiskLimits defaultRiskLimits() {
    return RiskLimits{70, 80, 10, 100, 1, 1000};
}

inline int riskEnvInt(const std::string& raw, int fallback) {
    if (raw.empty()) return fallback;
    int v = 0;
    size_t i = 0;
    bool any = false;
    if (i < raw.size() && raw[i] == '-') return fallback;
    while (i < raw.size() && raw[i] >= '0' && raw[i] <= '9') {
        any = true;
        v = v * 10 + (raw[i] - '0');
        i++;
    }
    return any ? v : fallback;
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
    lim.max_market_pct = parsePct(riskEnvLookup(overrides, "ASSISI_RISK_MAX_MARKET_PCT"), d.max_market_pct);
    lim.max_allocation_pct = parsePct(riskEnvLookup(overrides, "ASSISI_RISK_MAX_ALLOCATION_PCT"), d.max_allocation_pct);
    lim.venue_reserve_pct = parsePct(riskEnvLookup(overrides, "ASSISI_RISK_VENUE_RESERVE_PCT"), d.venue_reserve_pct);
    lim.max_contracts_per_market = riskEnvInt(riskEnvLookup(overrides, "ASSISI_RISK_MAX_CONTRACTS"), d.max_contracts_per_market);
    std::string chase = riskEnvLookup(overrides, "ASSISI_RISK_CHASE_RESERVE");
    lim.chase_reserve = chase.empty() ? d.chase_reserve : parseCents(chase);
    std::string dd = riskEnvLookup(overrides, "ASSISI_RISK_MAX_DRAWDOWN");
    lim.max_drawdown = dd.empty() ? d.max_drawdown : parseCents(dd);
    return lim;
}

inline std::string riskLimitsLogLine(const RiskLimits& lim) {
    char buf[256];
    std::snprintf(buf, sizeof(buf),
        "risk limits max_market_pct=%d max_allocation_pct=%d venue_reserve_pct=%d "
        "max_contracts=%d chase_reserve=%s max_drawdown=%s",
        lim.max_market_pct, lim.max_allocation_pct, lim.venue_reserve_pct,
        lim.max_contracts_per_market, formatCents(lim.chase_reserve).c_str(),
        formatCents(lim.max_drawdown).c_str());
    return std::string(buf);
}
