#pragma once
#include <mutex>
#include <unordered_map>
#include "execution.hpp"

struct Position {
    long int market_id;
    int yes_count;
    int no_count;
    int average_yes_price;
    int average_no_price;
    int yes_venue;
    int no_venue;

    Position() {
        market_id = yes_count = no_count = 0;
        average_yes_price = average_no_price = 0;
        yes_venue = no_venue = 0;
    }

    Position(long int id, int yes, int no) {
        market_id = id;
        yes_count = yes;
        no_count = no;
        average_yes_price = average_no_price = 0;
        yes_venue = no_venue = 0;
    }
};

struct Portfolio {
    Position portfolio_[16];
    int position_count_;
    int kalshi_cash, kalshi_exposure;
    int polymarket_cash, polymarket_exposure;
    int gemini_cash, gemini_exposure;
    int kalshi_reserved, polymarket_reserved, gemini_reserved;
    long int getPositionMarketId(long int marketId) { return marketId; }

    Position positionFor(long int market_id) const {
        for (int i = 0; i < position_count_; i++) {
            if (portfolio_[i].market_id == market_id) return portfolio_[i];
        }
        return Position();
    }

    int venueCashGross(int venue) const {
        if (venue == Kalshi) return kalshi_cash;
        if (venue == Polymarket) return polymarket_cash;
        if (venue == Gemini) return gemini_cash;
        return 0;
    }

    int venueReserved(int venue) const {
        if (venue == Kalshi) return kalshi_reserved;
        if (venue == Polymarket) return polymarket_reserved;
        if (venue == Gemini) return gemini_reserved;
        return 0;
    }

    int venueCash(int venue) const {
        int spendable = venueCashGross(venue) - venueReserved(venue);
        return spendable > 0 ? spendable : 0;
    }

    int venueExposure(int venue) const {
        if (venue == Kalshi) return kalshi_exposure;
        if (venue == Polymarket) return polymarket_exposure;
        if (venue == Gemini) return gemini_exposure;
        return 0;
    }

    int totalCash() const { return kalshi_cash + polymarket_cash + gemini_cash; }
    int totalReserved() const { return kalshi_reserved + polymarket_reserved + gemini_reserved; }
    int totalExposure() const { return kalshi_exposure + polymarket_exposure + gemini_exposure; }
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

    void addExposure(int venue, int amount) {
        if (venue == Kalshi) local_book.kalshi_exposure += amount;
        else if (venue == Polymarket) local_book.polymarket_exposure += amount;
        else if (venue == Gemini) local_book.gemini_exposure += amount;
    }

    void addCash(int venue, int amount) {
        if (venue == Kalshi) local_book.kalshi_cash += amount;
        else if (venue == Polymarket) local_book.polymarket_cash += amount;
        else if (venue == Gemini) local_book.gemini_cash += amount;
    }

    int* reservedPtr(int venue) {
        if (venue == Kalshi) return &local_book.kalshi_reserved;
        if (venue == Polymarket) return &local_book.polymarket_reserved;
        if (venue == Gemini) return &local_book.gemini_reserved;
        return nullptr;
    }

    int cashOf(int venue) const {
        if (venue == Kalshi) return local_book.kalshi_cash;
        if (venue == Polymarket) return local_book.polymarket_cash;
        if (venue == Gemini) return local_book.gemini_cash;
        return 0;
    }

    bool tryReserveUnlocked(int venue, int amount) {
        if (amount <= 0) return true;
        int* reserved = reservedPtr(venue);
        if (!reserved) return false;
        if (cashOf(venue) - *reserved < amount) return false;
        *reserved += amount;
        return true;
    }

    void releaseReserveUnlocked(int venue, int amount) {
        if (amount <= 0) return;
        int* reserved = reservedPtr(venue);
        if (!reserved) return;
        *reserved -= amount;
        if (*reserved < 0) *reserved = 0;
    }

    static void blendLot(int& count, int& avg_price, int add, int add_price) {
        if (add <= 0) return;
        long long total = static_cast<long long>(count) + add;
        long long num = static_cast<long long>(count) * avg_price
            + static_cast<long long>(add) * add_price;
        avg_price = static_cast<int>((num + total / 2) / total);
        count += add;
    }

    public:
        PortfolioManager() {
            local_book.kalshi_cash = local_book.kalshi_exposure = 0;
            local_book.polymarket_cash = local_book.polymarket_exposure = 0;
            local_book.gemini_cash = local_book.gemini_exposure = 0;
            local_book.kalshi_reserved = local_book.polymarket_reserved = local_book.gemini_reserved = 0;
            local_book.position_count_ = 0;
        }

        bool tryReserve(int venue, int amount) {
            if (amount <= 0) return true;
            std::lock_guard<std::mutex> lock(mtx_);
            return tryReserveUnlocked(venue, amount);
        }

        bool tryReserveTwo(int venue_a, int amount_a, int venue_b, int amount_b) {
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

        void releaseReserve(int venue, int amount) {
            if (amount <= 0) return;
            std::lock_guard<std::mutex> lock(mtx_);
            releaseReserveUnlocked(venue, amount);
        }

        void releaseReserveTwo(int venue_a, int amount_a, int venue_b, int amount_b) {
            std::lock_guard<std::mutex> lock(mtx_);
            if (venue_a == venue_b) {
                releaseReserveUnlocked(venue_a, amount_a + amount_b);
                return;
            }
            releaseReserveUnlocked(venue_a, amount_a);
            releaseReserveUnlocked(venue_b, amount_b);
        }

        void setKalshiCash(int v) {
            std::lock_guard<std::mutex> lock(mtx_);
            local_book.kalshi_cash = v;
        }

        void setPolymarketCash(int v) {
            std::lock_guard<std::mutex> lock(mtx_);
            local_book.polymarket_cash = v;
        }

        void setGeminiCash(int v) {
            std::lock_guard<std::mutex> lock(mtx_);
            local_book.gemini_cash = v;
        }

        int kalshiCash() {
            std::lock_guard<std::mutex> lock(mtx_);
            return local_book.kalshi_cash;
        }

        int polymarketCash() {
            std::lock_guard<std::mutex> lock(mtx_);
            return local_book.polymarket_cash;
        }

        int geminiCash() {
            std::lock_guard<std::mutex> lock(mtx_);
            return local_book.gemini_cash;
        }

        int totalCash() {
            std::lock_guard<std::mutex> lock(mtx_);
            return local_book.kalshi_cash + local_book.polymarket_cash + local_book.gemini_cash;
        }

        void addVenueLot(long int market_id, int venue, int yes, int no, int avg_yes, int avg_no) {
            int slot = slotFor(market_id);
            if (slot < 0) return;
            std::lock_guard<std::mutex> lock(mtx_);
            Position& pos = local_book.portfolio_[slot];
            if (yes > 0) {
                blendLot(pos.yes_count, pos.average_yes_price, yes, avg_yes);
                pos.yes_venue = venue;
                addExposure(venue, yes * avg_yes);
            }
            if (no > 0) {
                blendLot(pos.no_count, pos.average_no_price, no, avg_no);
                pos.no_venue = venue;
                addExposure(venue, no * avg_no);
            }
        }

        FillApply updatePortfolio(Fill order_fill) {
            if (order_fill.market_id == 0 || order_fill.venue_id == 0 || order_fill.size <= 0) {
                return FillApply::Ignored;
            }
            int slot = slotFor(order_fill.market_id);
            if (slot < 0) return FillApply::Unregistered;
            int fees = takerFee(order_fill.venue, order_fill.price, order_fill.size);
            std::lock_guard<std::mutex> lock(mtx_);
            Position& pos = local_book.portfolio_[slot];
            if (order_fill.buy) {
                int spent = (order_fill.size * order_fill.price) + fees;
                if (order_fill.side == YesSided) {
                    blendLot(pos.yes_count, pos.average_yes_price, order_fill.size, order_fill.price);
                    pos.yes_venue = order_fill.venue;
                } else if (order_fill.side == NoSided) {
                    blendLot(pos.no_count, pos.average_no_price, order_fill.size, order_fill.price);
                    pos.no_venue = order_fill.venue;
                }
                addCash(order_fill.venue, -spent);
                addExposure(order_fill.venue, spent);
                return FillApply::Applied;
            }
            int& count = (order_fill.side == YesSided) ? pos.yes_count : pos.no_count;
            int& avg = (order_fill.side == YesSided) ? pos.average_yes_price : pos.average_no_price;
            int& held_venue = (order_fill.side == YesSided) ? pos.yes_venue : pos.no_venue;
            int sell = order_fill.size;
            if (sell > count) sell = count;
            if (sell < 1) return FillApply::Ignored;
            int cost_basis = sell * avg;
            count -= sell;
            if (count == 0) {
                avg = 0;
                held_venue = 0;
            }
            bool kalshi_cover_no = (order_fill.venue == Kalshi && order_fill.side == NoSided);
            if (kalshi_cover_no) {
                addCash(order_fill.venue, -(sell * order_fill.price + fees));
            } else {
                addCash(order_fill.venue, sell * order_fill.price - fees);
            }
            addExposure(order_fill.venue, -cost_basis);
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
