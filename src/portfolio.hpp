#pragma once
#include <mutex>
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
    float cash;
    float kalshi_cash;
    float polymarket_cash;
    float gemini_cash;
    long int getPositionMarketId(long int marketId) { return marketId; }
};

class PortfolioManager {
    Portfolio local_book;
    std::mutex mtx_;

    public:
        PortfolioManager() {
            local_book.cash = 100.00;
            local_book.kalshi_cash = 0.00;
            local_book.polymarket_cash = 0.00;
            local_book.gemini_cash = 0.00;
            local_book.position_count_ = 0;
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

        float sizingCash() {
            float venue = kalshiCash() + polymarketCash() + geminiCash();
            if (venue > 0.00f) return venue;
            std::lock_guard<std::mutex> lock(mtx_);
            return local_book.cash;
        }

        void addVenueLot(long int market_id, int yes, int no, float avg_yes, float avg_no) {
            std::lock_guard<std::mutex> lock(mtx_);
            for (int i = 0; i < local_book.position_count_; i++) {
                if (local_book.portfolio_[i].market_id != market_id) continue;
                if (yes > 0) {
                    float old_count = static_cast<float>(local_book.portfolio_[i].yes_count);
                    float total = old_count + static_cast<float>(yes);
                    if (total > 0) {
                        local_book.portfolio_[i].average_yes_price =
                            ((old_count / total) * local_book.portfolio_[i].average_yes_price)
                            + ((static_cast<float>(yes) / total) * avg_yes);
                    }
                    local_book.portfolio_[i].yes_count += yes;
                }
                if (no > 0) {
                    float old_count = static_cast<float>(local_book.portfolio_[i].no_count);
                    float total = old_count + static_cast<float>(no);
                    if (total > 0) {
                        local_book.portfolio_[i].average_no_price =
                            ((old_count / total) * local_book.portfolio_[i].average_no_price)
                            + ((static_cast<float>(no) / total) * avg_no);
                    }
                    local_book.portfolio_[i].no_count += no;
                }
                return;
            }
        }

        void updatePortfolio(Fill order_fill) {
            std::lock_guard<std::mutex> lock(mtx_);
            float fees = takerFee(order_fill.venue, order_fill.price, order_fill.size);
            if (order_fill.market_id != 0 && order_fill.venue_id != 0 && order_fill.size > 0) {
                for (int i = 0; i < local_book.position_count_; i++) {
                    if (local_book.portfolio_[i].market_id == order_fill.market_id && order_fill.buy) {
                        if (order_fill.side == YesSided) {
                            float old_count = local_book.portfolio_[i].yes_count;
                            float new_size = order_fill.size;
                            float total_weight = old_count + new_size;
                            local_book.portfolio_[i].average_yes_price = ((old_count / total_weight) * local_book.portfolio_[i].average_yes_price) + ((new_size / total_weight) * order_fill.price);
                            local_book.portfolio_[i].yes_count += order_fill.size;
                        } else if (order_fill.side == NoSided) {
                            float old_count = local_book.portfolio_[i].no_count;
                            float new_size = order_fill.size;
                            float total_weight = old_count + new_size;
                            local_book.portfolio_[i].average_no_price = ((old_count / total_weight) * local_book.portfolio_[i].average_no_price) + ((new_size / total_weight) * order_fill.price);
                            local_book.portfolio_[i].no_count += order_fill.size;
                        }
                        float spent = (order_fill.size * order_fill.price) + fees;
                        local_book.cash -= spent;
                        if (order_fill.venue == Kalshi) local_book.kalshi_cash -= spent;
                        else if (order_fill.venue == Polymarket) local_book.polymarket_cash -= spent;
                        else if (order_fill.venue == Gemini) local_book.gemini_cash -= spent;
                    }
                }
            }
        }

        void Register(long int market_id) {
            if (local_book.position_count_ < 16 && getPositionByMarketId(market_id).market_id == 0) {
                local_book.portfolio_[local_book.position_count_] = Position(market_id, 0, 0);
                local_book.position_count_++;
            }
        }

        Position getPositionByMarketId(long int market_id) {
            std::lock_guard<std::mutex> lock(mtx_);
            for (int i = 0; i < local_book.position_count_; i++) {
                if (local_book.portfolio_[i].market_id == market_id) {
                    return local_book.portfolio_[i];
                }
            }
            return Position();
        }

        Portfolio getPortfolio() {
            std::lock_guard<std::mutex> lock(mtx_);
            return local_book;
        }
};
