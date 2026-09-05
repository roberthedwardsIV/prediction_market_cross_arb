#pragma once

#include <string>
#include "json_find.hpp"

struct ParsedBook {
    std::string id;
    float yes_bid = 0;
    float yes_ask = 0;
    int yes_bid_n = 0;
    int yes_ask_n = 0;
    bool ok = false;
};

inline bool levelPx(const std::string& obj, std::string& out) {
    return jsonField(obj, "px", out) || jsonField(obj, "price", out)
        || jsonNestedValue(obj, "px", out) || jsonNestedValue(obj, "price", out);
}

inline bool levelQty(const std::string& obj, std::string& out) {
    return jsonField(obj, "qty", out) || jsonField(obj, "size", out)
        || jsonNestedValue(obj, "qty", out) || jsonNestedValue(obj, "size", out);
}

inline ParsedBook parsePolymarketBookJson(const std::string& body) {
    ParsedBook out;
    if (!jsonField(body, "marketSlug", out.id)) return out;
    std::string bid_s, ask_s, bid_n_s, ask_n_s, bid_obj, ask_obj;
    bool have_bid_n = false, have_ask_n = false;
    bool got_book =
        (jsonFirstObjectSlice(body, "bids", bid_obj)
            && (jsonFirstObjectSlice(body, "offers", ask_obj)
                || jsonFirstObjectSlice(body, "asks", ask_obj)));
    if (got_book) {
        if (!levelPx(bid_obj, bid_s) || !levelPx(ask_obj, ask_s)) return out;
        have_bid_n = levelQty(bid_obj, bid_n_s);
        have_ask_n = levelQty(ask_obj, ask_n_s);
    } else {
        if (!(jsonNestedValue(body, "bestBid", bid_s) || jsonField(body, "bestBid", bid_s))
            || !(jsonNestedValue(body, "bestAsk", ask_s) || jsonField(body, "bestAsk", ask_s))) {
            return out;
        }
    }
    try {
        out.yes_bid = std::stof(bid_s);
        out.yes_ask = std::stof(ask_s);
    } catch (...) {
        return out;
    }
    out.yes_bid_n = have_bid_n ? jsonContracts(bid_n_s) : 0;
    out.yes_ask_n = have_ask_n ? jsonContracts(ask_n_s) : 0;
    out.ok = true;
    return out;
}

inline ParsedBook parseKalshiTickerJson(const std::string& body) {
    ParsedBook out;
    std::string kind;
    if (!jsonField(body, "type", kind) || kind != "ticker") return out;
    std::string bid_s, ask_s, bid_n_s, ask_n_s;
    if (!jsonField(body, "yes_bid_dollars", bid_s) ||
        !jsonField(body, "yes_ask_dollars", ask_s) ||
        !jsonField(body, "market_ticker", out.id)) {
        return out;
    }
    try {
        out.yes_bid = std::stof(bid_s);
        out.yes_ask = std::stof(ask_s);
    } catch (...) {
        return out;
    }
    out.yes_bid_n = jsonSizeOrUnknown(jsonField(body, "yes_bid_size_fp", bid_n_s), bid_n_s);
    out.yes_ask_n = jsonSizeOrUnknown(jsonField(body, "yes_ask_size_fp", ask_n_s), ask_n_s);
    out.ok = true;
    return out;
}
