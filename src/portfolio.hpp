#pragma once
#include <mutex>
#include <unordered_map>
#include "execution.hpp"

struct Position {
    long int market_id;
    int yes_count;
    int no_count;
    float average_yes_price;
    float average_no_price;

    Position() { market_id = yes_count = no_count = 0; average_yes_price = average_no_price = 0.00; }

    Position(long int id, int yes, int no) {
        market_id = id;
        yes_count = yes;
        no_count = no;
        average_yes_price = average_no_price = 0.00;
    }
};

struct Portfolio {
    Position portfolio_[16];
    int position_count_;
    float kalshi_cash, kalshi_exposure;
    float polymarket_cash, polymarket_exposure;
    float gemini_cash, gemini_exposure;
    float kalshi_reserved, polymarket_reserved, gemini_reserved;
    long int getPositionMarketId(long int marketId) { return marketId; }

    Position positionFor(long int market_id) const {
        for (int i = 0; i < position_count_; i++) {
            if (portfolio_[i].market_id == market_id) return portfolio_[i];
        }
        return Position();
    }

    float venueCashGross(int venue) const {
        if (venue == Kalshi) return kalshi_cash;
        if (venue == Polymarket) return polymarket_cash;
        if (venue == Gemini) return gemini_cash;
        return 0.0f;
    }

    float venueReserved(int venue) const {
        if (venue == Kalshi) return kalshi_reserved;
        if (venue == Polymarket) return polymarket_reserved;
        if (venue == Gemini) return gemini_reserved;
        return 0.0f;
    }

    float venueCash(int venue) const {
        float spendable = venueCashGross(venue) - venueReserved(venue);
        return spendable > 0.0f ? spendable : 0.0f;
    }

    float venueExposure(int venue) const {
        if (venue == Kalshi) return kalshi_exposure;
        if (venue == Polymarket) return polymarket_exposure;
        if (venue == Gemini) return gemini_exposure;
        return 0.0f;
    }

    float totalCash() const { return kalshi_cash + polymarket_cash + gemini_cash; }
    float totalReserved() const { return kalshi_reserved + polymarket_reserved + gemini_reserved; }
    float totalExposure() const { return kalshi_exposure + polymarket_exposure + gemini_exposure; }
};

class PortfolioManager {
    Portfolio local_book;
    std::mutex mtx_;
    std::unordered_map<long int, int> slot_by_market_id_;

    int slotFor(long int market_id) const {
        auto it = slot_by_market_id_.find(market_id);
        if (it == slot_by_market_id_.end()) return -1;
        return it->second;
    }

    void addExposure(int venue, float amount) {
        if (venue == Kalshi) local_book.kalshi_exposure += amount;
        else if (venue == Polymarket) local_book.polymarket_exposure += amount;
        else if (venue == Gemini) local_book.gemini_exposure += amount;
    }

    void addCash(int venue, float amount) {
        if (venue == Kalshi) local_book.kalshi_cash += amount;
        else if (venue == Polymarket) local_book.polymarket_cash += amount;
        else if (venue == Gemini) local_book.gemini_cash += amount;
    }

    float* reservedPtr(int venue) {
        if (venue == Kalshi) return &local_book.kalshi_reserved;
        if (venue == Polymarket) return &local_book.polymarket_reserved;
        if (venue == Gemini) return &local_book.gemini_reserved;
        return nullptr;
    }

    float cashOf(int venue) const {
        if (venue == Kalshi) return local_book.kalshi_cash;
        if (venue == Polymarket) return local_book.polymarket_cash;
        if (venue == Gemini) return local_book.gemini_cash;
        return 0.0f;
    }

    bool tryReserveUnlocked(int venue, float amount) {
        if (amount <= 0.0f) return true;
        float* reserved = reservedPtr(venue);
        if (!reserved) return false;
        if (cashOf(venue) - *reserved < amount) return false;
        *reserved += amount;
        return true;
    }

    void releaseReserveUnlocked(int venue, float amount) {
        if (amount <= 0.0f) return;
        float* reserved = reservedPtr(venue);
        if (!reserved) return;
        *reserved -= amount;
        if (*reserved < 0.0f) *reserved = 0.0f;
    }

    static void blendLot(int& count, float& avg_price, int add, float add_price) {
        if (add <= 0) return;
        float old_count = static_cast<float>(count);
        float total = old_count + static_cast<float>(add);
        if (total > 0) {
            avg_price = ((old_count / total) * avg_price) + ((static_cast<float>(add) / total) * add_price);
        }
        count += add;
    }

    public:
        PortfolioManager() {
            local_book.kalshi_cash = local_book.kalshi_exposure = 0.00;
            local_book.polymarket_cash = local_book.polymarket_exposure = 0.00;
            local_book.gemini_cash = local_book.gemini_exposure = 0.00;
            local_book.kalshi_reserved = local_book.polymarket_reserved = local_book.gemini_reserved = 0.00;
            local_book.position_count_ = 0;
        }

        bool tryReserve(int venue, float amount) {
            if (amount <= 0.0f) return true;
            std::lock_guard<std::mutex> lock(mtx_);
            return tryReserveUnlocked(venue, amount);
        }

        bool tryReserveTwo(int venue_a, float amount_a, int venue_b, float amount_b) {
            std::lock_guard<std::mutex> lock(mtx_);
            if (venue_a == venue_b) {
                return tryReserveUnlocked(venue_a, amount_a + amount_b);
            }
            if (!tryReserveUnlocked(venue_a, amount_a)) return false;
            if (!tryReserveUnlocked(venue_b, amount_b)) {
                releaseReserveUnlocked(venue_a, amount_a);
                return false;
            }
            return true;
        }

        void releaseReserve(int venue, float amount) {
            if (amount <= 0.0f) return;
            std::lock_guard<std::mutex> lock(mtx_);
            releaseReserveUnlocked(venue, amount);
        }

        void releaseReserveTwo(int venue_a, float amount_a, int venue_b, float amount_b) {
            std::lock_guard<std::mutex> lock(mtx_);
            if (venue_a == venue_b) {
                releaseReserveUnlocked(venue_a, amount_a + amount_b);
                return;
            }
            releaseReserveUnlocked(venue_a, amount_a);
            releaseReserveUnlocked(venue_b, amount_b);
        }

        void setKalshiCash(float v) {
            std::lock_guard<std::mutex> lock(mtx_);
            local_book.kalshi_cash = v;
        }

        void setPolymarketCash(float v) {
            std::lock_guard<std::mutex> lock(mtx_);
            local_book.polymarket_cash = v;
        }

        void setGeminiCash(float v) {
            std::lock_guard<std::mutex> lock(mtx_);
            local_book.gemini_cash = v;
        }

        float kalshiCash() {
            std::lock_guard<std::mutex> lock(mtx_);
            return local_book.kalshi_cash;
        }

        float polymarketCash() {
            std::lock_guard<std::mutex> lock(mtx_);
            return local_book.polymarket_cash;
        }

        float geminiCash() {
            std::lock_guard<std::mutex> lock(mtx_);
            return local_book.gemini_cash;
        }

        float totalCash() {
            std::lock_guard<std::mutex> lock(mtx_);
            return local_book.kalshi_cash + local_book.polymarket_cash + local_book.gemini_cash;
        }

        void addVenueLot(long int market_id, int venue, int yes, int no, float avg_yes, float avg_no) {
            int slot = slotFor(market_id);
            if (slot < 0) return;
            std::lock_guard<std::mutex> lock(mtx_);
            Position& pos = local_book.portfolio_[slot];
            if (yes > 0) {
                blendLot(pos.yes_count, pos.average_yes_price, yes, avg_yes);
                addExposure(venue, yes * avg_yes);
            }
            if (no > 0) {
                blendLot(pos.no_count, pos.average_no_price, no, avg_no);
                addExposure(venue, no * avg_no);
            }
        }

        FillApply updatePortfolio(Fill order_fill) {
            if (order_fill.market_id == 0 || order_fill.venue_id == 0 || order_fill.size <= 0 || !order_fill.buy) {
                return FillApply::Ignored;
            }
            int slot = slotFor(order_fill.market_id);
            if (slot < 0) return FillApply::Unregistered;
            float fees = takerFee(order_fill.venue, order_fill.price, order_fill.size);
            float spent = (order_fill.size * order_fill.price) + fees;
            std::lock_guard<std::mutex> lock(mtx_);
            Position& pos = local_book.portfolio_[slot];
            if (order_fill.side == YesSided) {
                blendLot(pos.yes_count, pos.average_yes_price, order_fill.size, order_fill.price);
            } else if (order_fill.side == NoSided) {
                blendLot(pos.no_count, pos.average_no_price, order_fill.size, order_fill.price);
            }
            addCash(order_fill.venue, -spent);
            addExposure(order_fill.venue, spent);
            return FillApply::Applied;
        }

        void Register(long int market_id) {
            if (local_book.position_count_ >= 16 || slotFor(market_id) >= 0) return;
            int slot = local_book.position_count_;
            local_book.portfolio_[slot] = Position(market_id, 0, 0);
            local_book.position_count_++;
            slot_by_market_id_[market_id] = slot;
        }

        Position getPositionByMarketId(long int market_id) {
            int slot = slotFor(market_id);
            if (slot < 0) return Position();
            std::lock_guard<std::mutex> lock(mtx_);
            return local_book.portfolio_[slot];
        }

        Portfolio getPortfolio() {
            std::lock_guard<std::mutex> lock(mtx_);
            return local_book;
        }
};
