#pragma once

#include <mutex>

// One busy flag per market. Different markets can send at the same time.
// The same market cannot start a second job while the first is still going.
struct LiveBusy {
    static constexpr int kMax = 16;

    bool tryAcquire(long int market_id) {
        if (market_id == 0) return false;
        std::lock_guard<std::mutex> lock(mtx_);
        int free = -1;
        for (int i = 0; i < kMax; i++) {
            if (ids_[i] == market_id) {
                if (on_[i]) return false;
                on_[i] = true;
                return true;
            }
            if (free < 0 && ids_[i] == 0) free = i;
        }
        if (free < 0) return false;
        ids_[free] = market_id;
        on_[free] = true;
        return true;
    }

    void release(long int market_id) {
        if (market_id == 0) return;
        std::lock_guard<std::mutex> lock(mtx_);
        for (int i = 0; i < kMax; i++) {
            if (ids_[i] == market_id) {
                on_[i] = false;
                return;
            }
        }
    }

    void clearForTests() {
        std::lock_guard<std::mutex> lock(mtx_);
        for (int i = 0; i < kMax; i++) {
            ids_[i] = 0;
            on_[i] = false;
        }
    }

    private:
        std::mutex mtx_;
        long int ids_[kMax]{};
        bool on_[kMax]{};
};
