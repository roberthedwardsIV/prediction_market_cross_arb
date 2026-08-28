#pragma once

#include <chrono>
#include <mutex>
#include <algorithm>
#include <string>
#include <sstream>
#include <cstdint>

inline std::chrono::steady_clock::time_point& latArrive() {
    thread_local std::chrono::steady_clock::time_point t;
    return t;
}
inline std::chrono::steady_clock::time_point& latParsed() {
    thread_local std::chrono::steady_clock::time_point t;
    return t;
}
inline std::chrono::steady_clock::time_point& latIntent() {
    thread_local std::chrono::steady_clock::time_point t;
    return t;
}

inline long latUs(std::chrono::steady_clock::time_point a, std::chrono::steady_clock::time_point b) {
    if (a.time_since_epoch().count() == 0 || b.time_since_epoch().count() == 0) return -1;
    return static_cast<long>(std::chrono::duration_cast<std::chrono::microseconds>(b - a).count());
}

inline void latencyArrive() {
    latArrive() = std::chrono::steady_clock::now();
    latParsed() = {};
    latIntent() = {};
}

inline void latencyParsed() {
    latParsed() = std::chrono::steady_clock::now();
}

inline void latencyIntent() {
    latIntent() = std::chrono::steady_clock::now();
}

struct LatencyStats {
    std::mutex mtx;
    long us[128]{};
    int n = 0;
    int ticks = 0;
};

inline LatencyStats& latStats() {
    static LatencyStats s;
    return s;
}

inline void latencyRecordTick() {
    long us = latUs(latArrive(), latIntent());
    if (us < 0) us = latUs(latParsed(), latIntent());
    if (us < 0) return;
    LatencyStats& s = latStats();
    std::lock_guard<std::mutex> lock(s.mtx);
    s.us[s.n % 128] = us;
    s.n++;
    s.ticks++;
}

inline long latPctLocked(LatencyStats& s, int pct) {
    int have = s.n < 128 ? s.n : 128;
    if (have <= 0) return -1;
    long tmp[128];
    for (int i = 0; i < have; i++) tmp[i] = s.us[i];
    int idx = (have - 1) * pct / 100;
    std::nth_element(tmp, tmp + idx, tmp + have);
    return tmp[idx];
}

inline bool latencyShouldLogSummary() {
    LatencyStats& s = latStats();
    std::lock_guard<std::mutex> lock(s.mtx);
    return s.ticks > 0 && s.ticks % 100 == 0;
}

inline std::string latencySummaryLine() {
    LatencyStats& s = latStats();
    std::lock_guard<std::mutex> lock(s.mtx);
    std::ostringstream o;
    o << "lat tick_to_intent us p50=" << latPctLocked(s, 50)
      << " p99=" << latPctLocked(s, 99)
      << " n=" << s.n;
    return o.str();
}

inline std::string latField(const char* name, long us) {
    std::ostringstream o;
    o << "lat " << name << "=" << us;
    return o.str();
}

inline std::string latencySendLine() {
    auto now = std::chrono::steady_clock::now();
    std::ostringstream o;
    o << "lat us parse=" << latUs(latArrive(), latParsed())
      << " intent=" << latUs(latParsed(), latIntent())
      << " dispatch=" << latUs(latIntent(), now)
      << " tick_to_send=" << latUs(latArrive(), now);
    return o.str();
}
