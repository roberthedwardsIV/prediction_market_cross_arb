#include <gtest/gtest.h>
#include <fstream>
#include <sstream>
#include <string>

#include "parse_book.hpp"
#include "json_find.hpp"
#include "execution.hpp"

#ifndef ASSISI_SOURCE_DIR
#define ASSISI_SOURCE_DIR "."
#endif

static std::string readFixture(const char* name) {
    std::string path = std::string(ASSISI_SOURCE_DIR) + "/tests/fixtures/" + name;
    std::ifstream in(path);
    EXPECT_TRUE(in.good()) << "missing fixture " << path;
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

TEST(WsParse, PolymarketNestedQtyNotInvented) {
    std::string body = readFixture("polymarket_book_nested.json");
    std::string slice;
    ASSERT_TRUE(jsonFirstObjectSlice(body, "bids", slice));
    EXPECT_NE(slice.find("qty"), std::string::npos)
        << "brace-match must keep qty inside nested px object slice";

    ParsedBook book = parsePolymarketBookJson(body);
    ASSERT_TRUE(book.ok);
    EXPECT_EQ(book.id, "paccc-usse-midterms-2026-11-03-rep");
    EXPECT_FLOAT_EQ(book.yes_bid, 0.42f);
    EXPECT_FLOAT_EQ(book.yes_ask, 0.44f);
    EXPECT_EQ(book.yes_bid_n, 12);
    EXPECT_EQ(book.yes_ask_n, 7);
    EXPECT_NE(book.yes_bid_n, 1);
    EXPECT_NE(book.yes_ask_n, 1);
}

TEST(WsParse, PolymarketMissingSizeIsZeroNotOne) {
    ParsedBook book = parsePolymarketBookJson(readFixture("polymarket_best_no_size.json"));
    ASSERT_TRUE(book.ok);
    EXPECT_FLOAT_EQ(book.yes_bid, 0.31f);
    EXPECT_FLOAT_EQ(book.yes_ask, 0.33f);
    EXPECT_EQ(book.yes_bid_n, 0);
    EXPECT_EQ(book.yes_ask_n, 0);
}

TEST(WsParse, KalshiTickerSizes) {
    ParsedBook book = parseKalshiTickerJson(readFixture("kalshi_ticker.json"));
    ASSERT_TRUE(book.ok);
    EXPECT_EQ(book.id, "CONTROLS-2026-R");
    EXPECT_FLOAT_EQ(book.yes_bid, 0.40f);
    EXPECT_FLOAT_EQ(book.yes_ask, 0.42f);
    EXPECT_EQ(book.yes_bid_n, 10);
    EXPECT_EQ(book.yes_ask_n, 5);
}

TEST(WsParse, KalshiMissingSizeUnknownIsOne) {
    std::string body =
        "{\"type\":\"ticker\",\"market_ticker\":\"X\","
        "\"yes_bid_dollars\":\"0.40\",\"yes_ask_dollars\":\"0.42\"}";
    ParsedBook book = parseKalshiTickerJson(body);
    ASSERT_TRUE(book.ok);
    EXPECT_EQ(book.yes_bid_n, 1);
    EXPECT_EQ(book.yes_ask_n, 1);
}

TEST(WsParse, OrderMissingFillCountDoesNotInvent) {
    int size = 99;
    float price = 1.0f;
    EXPECT_FALSE(takeReportedFill(0.0f, 0.50f, 0.40f, size, price));
    EXPECT_EQ(size, 0);
}
