#pragma once

#include <cstdio>
#include <string>

// Contract prices, cash, fees, and average prices are integer cents.
// $1.00 = 100, $0.40 = 40. Pair payout is kDollarCents per contract.
constexpr int kDollarCents = 100;

inline int clampPriceCents(int p) {
    if (p < 1) return 1;
    if (p > 99) return 99;
    return p;
}

inline int complementCents(int px) {
    if (px <= 0) return 0;
    return kDollarCents - px;
}

// Wire/tape dollars ("0.40", "100", "3.25") to cents. Rounds half up on the third decimal.
inline int parseCents(const std::string& s) {
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '"')) i++;
    if (i >= s.size()) return 0;
    bool neg = false;
    if (s[i] == '-') {
        neg = true;
        i++;
    } else if (s[i] == '+') {
        i++;
    }
    long long whole = 0;
    bool any = false;
    while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
        any = true;
        whole = whole * 10 + (s[i] - '0');
        if (whole > 1000000000LL) return 0;
        i++;
    }
    int frac = 0;
    int frac_digits = 0;
    int rest = 0;
    if (i < s.size() && s[i] == '.') {
        i++;
        while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
            int d = s[i] - '0';
            if (frac_digits < 2) {
                frac = frac * 10 + d;
                frac_digits++;
            } else if (frac_digits == 2) {
                rest = d;
                frac_digits++;
            }
            i++;
        }
        if (frac_digits == 1) frac *= 10;
    }
    if (!any && frac_digits == 0) return 0;
    int cents = static_cast<int>(whole * 100 + frac);
    if (rest >= 5) cents++;
    return neg ? -cents : cents;
}

inline std::string formatCents(int cents) {
    bool neg = cents < 0;
    if (neg) cents = -cents;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%s%d.%02d", neg ? "-" : "", cents / 100, cents % 100);
    return buf;
}

inline std::string formatCents4(int cents) {
    bool neg = cents < 0;
    if (neg) cents = -cents;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%s%d.%02d00", neg ? "-" : "", cents / 100, cents % 100);
    return buf;
}

// "0.70" or "70" to 70. Used for ASSISI_RISK_*_PCT.
inline int parsePct(const std::string& s, int fallback) {
    if (s.empty()) return fallback;
    if (s.find('.') != std::string::npos) {
        int v = parseCents(s);
        if (v < 0 || v > 100) return fallback;
        return v;
    }
    int v = 0;
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) i++;
    if (i >= s.size() || s[i] < '0' || s[i] > '9') return fallback;
    while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
        v = v * 10 + (s[i] - '0');
        i++;
    }
    if (v < 0 || v > 100) return fallback;
    return v;
}
