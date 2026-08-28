#include <iostream>
#include <memory>
#include <ctime>
#include <thread>
#include <chrono>
#include <mutex>
#include <string>
#include <vector>
#include <cmath>
#include <condition_variable>
#include <unordered_map>
#include <atomic>

#include "DefaultEWrapper.h"
#include "EClientSocket.h"
#include "EReaderOSSignal.h"
#include "EReader.h"
#include "Contract.h"
#include "Order.h"
#include "Decimal.h"

#include "market_data.hpp"
#include "ib_md.hpp"
#include "kalshi_env.hpp"
#include "latency.hpp"

struct FxOrder {
    long int for_id;
    float limit_price;
    int size;
    bool buy_no;
};

static std::mutex fx_order_mtx;
static std::vector<FxOrder> fx_orders;
static std::unordered_map<long int, long int> fx_yes_to_no;
static std::unordered_map<long int, double> fx_min_tick;
static std::mutex fx_wait_mtx;
static std::condition_variable fx_wait_cv;
static int fx_wait_oid = -1;
static bool fx_wait_done = false;
static bool fx_wait_filled = false;
static std::atomic<bool> fx_hard_reject{false};
static std::chrono::steady_clock::time_point fx_t_enq{};
static std::chrono::steady_clock::time_point fx_t_place{};
static std::chrono::steady_clock::time_point fx_t_done{};
static long fx_place_call_us = -1;
static float ib_cash_value = 0;
static float ib_nlv_value = 0;
static bool ib_got_cash = false;
static std::atomic<bool> fx_armed{false};
static void (*fx_lot_handler)(long int, int, float) = nullptr;

struct FxLot {
    long int conid = 0;
    int size = 0;
    float avg = 0;
};

void enqueueForecastExOrder(long int for_id, float limit_price, int size, bool buy_no) {
    {
        std::lock_guard<std::mutex> w(fx_wait_mtx);
        fx_t_enq = std::chrono::steady_clock::now();
        fx_t_place = {};
        fx_t_done = {};
        fx_place_call_us = -1;
        fx_wait_oid = -1;
        fx_wait_done = false;
        fx_wait_filled = false;
    }
    std::lock_guard<std::mutex> lock(fx_order_mtx);
    fx_orders.push_back({for_id, limit_price, size, buy_no});
}

void forecastExResetHardReject() {
    fx_hard_reject.store(false);
}

bool forecastExHardReject() {
    return fx_hard_reject.load();
}

bool forecastExNoReady(long int yes_id) {
    auto it = fx_yes_to_no.find(yes_id);
    return it != fx_yes_to_no.end() && it->second != 0;
}

long int forecastExNoId(long int yes_id) {
    auto it = fx_yes_to_no.find(yes_id);
    if (it == fx_yes_to_no.end()) return 0;
    return it->second;
}

void setForecastExLotHandler(void (*fn)(long int conid, int size, float avg)) {
    fx_lot_handler = fn;
}

void armForecastExTrading() {
    fx_armed.store(true);
}

bool waitForecastExFill(int timeout_ms) {
    std::unique_lock<std::mutex> lock(fx_wait_mtx);
    fx_wait_cv.wait_for(lock, std::chrono::milliseconds(timeout_ms), [] { return fx_wait_done; });
    return fx_wait_filled;
}

long forecastExQueueUs() {
    std::lock_guard<std::mutex> lock(fx_wait_mtx);
    return latUs(fx_t_enq, fx_t_place);
}

long forecastExPlaceCallUs() {
    std::lock_guard<std::mutex> lock(fx_wait_mtx);
    return fx_place_call_us;
}

long forecastExFillUs() {
    std::lock_guard<std::mutex> lock(fx_wait_mtx);
    return latUs(fx_t_place, fx_t_done);
}

float forecastExCash() {
    if (ib_got_cash && ib_cash_value != 0.0f) return ib_cash_value;
    if (ib_nlv_value != 0.0f) return ib_nlv_value;
    if (ib_got_cash) return ib_cash_value;
    return 0;
}

float forecastExAccountValue() {
    if (ib_nlv_value != 0.0f) return ib_nlv_value;
    return forecastExCash();
}

class IbFeed : public DefaultEWrapper {
    EReaderOSSignal m_osSignal;
    EClientSocket* m_pClient;

    public:
    bool got_id = false;
    int next_oid = 1;
    float bid = 0.00, ask = 0.00;
    MarketData& md;
    void (*on_tick)(MarketData&, long int) = nullptr;
    std::vector<long int> for_ids;
    std::unordered_map<int, long int> cd_yes_req;
    std::unordered_map<int, long int> cd_no_req;
    std::vector<FxLot> lots;
    bool pos_end = false;
    bool acct_end = false;

    IbFeed(MarketData& md)
        : m_osSignal(2000)
        , m_pClient(new EClientSocket(this, &m_osSignal))
        , md(md)
    {}
    ~IbFeed() {
        m_pReader.reset();
        delete m_pClient;
    }

    bool connect() {
        m_pClient->setConnectOptions("+PACEAPI");
        if (m_pClient->eConnect("127.0.0.1", ibPort(), 16)) {
            m_pReader.reset(new EReader(m_pClient, &m_osSignal));
            m_pReader->start();
            return true;
        }
        return false;
    }

    void process() {
        m_osSignal.waitForSignal();
        m_pReader->processMsgs();
    }

    void nextValidId(int orderId) {
        std::cout << "nextValidId: " << orderId << " " << m_pClient->EClient::serverVersion() << "\n";
        next_oid = orderId;
        got_id = true;
    }

    void drainOrders() {
        std::vector<FxOrder> pending;
        {
            std::lock_guard<std::mutex> lock(fx_order_mtx);
            pending.swap(fx_orders);
        }
        for (auto& p : pending) {
            if (fx_hard_reject.load()) break;
            long int oid = p.for_id;
            if (p.buy_no) {
                auto it = fx_yes_to_no.find(p.for_id);
                if (it == fx_yes_to_no.end() || it->second == 0) {
                    std::cout << "skip fx: NO contract not resolved\n";
                    continue;
                }
                oid = it->second;
            }
            Contract c;
            c.conId = oid;
            c.exchange = "FORECASTX";
            Order o;
            o.action = "BUY";
            o.orderType = "LMT";
            o.totalQuantity = DecimalFunctions::stringToDecimal(std::to_string(p.size));
            double tick = 0.01;
            auto mt = fx_min_tick.find(oid);
            if (mt != fx_min_tick.end() && mt->second > 0.0) {
                tick = mt->second;
            }
            double px = std::round(static_cast<double>(p.limit_price) / tick) * tick;
            o.lmtPrice = px;
            o.tif = "DAY";
            {
                std::lock_guard<std::mutex> lock(fx_wait_mtx);
                fx_wait_oid = next_oid;
                fx_wait_done = false;
                fx_wait_filled = false;
            }
            std::cout << "fx placeOrder BUY " << p.size << " LMT " << px
                      << (p.buy_no ? " NO\n" : " YES\n");
            auto p0 = std::chrono::steady_clock::now();
            m_pClient->placeOrder(next_oid, c, o);
            {
                std::lock_guard<std::mutex> w(fx_wait_mtx);
                fx_t_place = std::chrono::steady_clock::now();
                fx_place_call_us = latUs(p0, fx_t_place);
            }
            next_oid++;
        }
    }

    void orderStatus(int orderId, const std::string& status, Decimal filled,
                     Decimal remaining, double avgFillPrice, long long permId, int parentId,
                     double lastFillPrice, int clientId, const std::string& whyHeld, double mktCapPrice) {
        (void)remaining;
        (void)avgFillPrice;
        (void)permId;
        (void)parentId;
        (void)lastFillPrice;
        (void)clientId;
        (void)whyHeld;
        (void)mktCapPrice;
        std::cout << "fx orderStatus " << orderId << " " << status
                  << " filled " << DecimalFunctions::decimalToString(filled) << "\n";
        int filled_n = 0;
        try { filled_n = std::stoi(DecimalFunctions::decimalToString(filled)); } catch (...) {}
        std::lock_guard<std::mutex> lock(fx_wait_mtx);
        if (orderId != fx_wait_oid) return;
        if (filled_n > 0 && status == "Filled") {
            fx_t_done = std::chrono::steady_clock::now();
            fx_wait_filled = true;
            fx_wait_done = true;
            fx_wait_cv.notify_all();
        } else if (status == "Inactive" || status == "Cancelled" || status == "ApiCancelled") {
            fx_t_done = std::chrono::steady_clock::now();
            fx_wait_done = true;
            fx_wait_cv.notify_all();
        }
    }

    void tickPrice(int reqId, TickType field, double price, const TickAttrib& attrib) {
        if (field != 1 && field != 2) { return; }
        latencyArrive();
        long int id = for_ids[reqId - 3];
        Snapshot s = md.getSnapshotByVenueId(id, ForecastEx);
        float yes_bid = s.yes_bid;
        float yes_ask = s.yes_ask;
        if (field == 1) { yes_bid = price; }
        else { yes_ask = price; }
        md.price_update(id, ForecastEx, yes_bid, yes_ask, 1 - yes_ask, 1 - yes_bid);
        if (yes_bid == 0.00 || yes_ask == 0.00) { return; }
        latencyParsed();
        if (on_tick) {
            on_tick(md, id);
        }
    }

    void contractDetails(int reqId, const ContractDetails& details) {
        const Contract& c = details.contract;
        auto yes_it = cd_yes_req.find(reqId);
        if (yes_it != cd_yes_req.end()) {
            Contract no;
            no.symbol = c.symbol;
            no.secType = c.secType.empty() ? "OPT" : c.secType;
            no.exchange = "FORECASTX";
            no.currency = c.currency.empty() ? "USD" : c.currency;
            no.strike = c.strike;
            no.right = "P";
            no.lastTradeDateOrContractMonth = c.lastTradeDateOrContractMonth;
            std::string loc = c.localSymbol;
            auto pos = loc.rfind("_YES");
            if (pos != std::string::npos) {
                loc.replace(pos, 4, "_NO");
                no.localSymbol = loc;
            }
            int nreq = 200 + (reqId - 100);
            cd_no_req[nreq] = yes_it->second;
            std::cout << "fx resolve " << loc << "\n";
            if (details.minTick > 0.0) {
                fx_min_tick[c.conId] = details.minTick;
            }
            m_pClient->reqContractDetails(nreq, no);
            return;
        }
        auto no_it = cd_no_req.find(reqId);
        if (no_it != cd_no_req.end()) {
            fx_yes_to_no[no_it->second] = c.conId;
            if (details.minTick > 0.0) {
                fx_min_tick[c.conId] = details.minTick;
            }
            std::cout << "fx NO ready " << c.localSymbol << " tick " << details.minTick << "\n";
        }
    }

    void requestAccount() {
        m_pClient->reqAccountSummary(9002, "All", "TotalCashValue,NetLiquidation,AvailableFunds");
    }

    void requestPositions() {
        lots.clear();
        pos_end = false;
        m_pClient->reqPositions();
    }

    void applyLots() {
        if (!fx_lot_handler) return;
        fx_lot_handler(0, 0, 0);
        for (auto& lot : lots) {
            if (lot.size != 0) fx_lot_handler(lot.conid, lot.size, lot.avg);
        }
    }

    void position(const std::string&, const Contract& contract, Decimal position, double avgCost) {
        const std::string& loc = contract.localSymbol;
        bool fx = contract.exchange == "FORECASTX"
            || loc.find("_YES") != std::string::npos
            || loc.find("_NO") != std::string::npos;
        if (!fx) return;
        int n = 0;
        try { n = std::stoi(DecimalFunctions::decimalToString(position)); } catch (...) { return; }
        if (n == 0) return;
        lots.push_back({contract.conId, n, static_cast<float>(avgCost)});
    }

    void positionEnd() {
        pos_end = true;
        m_pClient->cancelPositions();
    }

    void accountSummaryEnd(int) {
        acct_end = true;
    }

    void requestNoContracts() {
        for (int i = 0; i < (int)for_ids.size(); i++) {
            Contract c;
            c.conId = for_ids[i];
            c.exchange = "FORECASTX";
            int reqId = 100 + i;
            cd_yes_req[reqId] = for_ids[i];
            m_pClient->reqContractDetails(reqId, c);
        }
    }

    void requestQuotes() {
        for(int i = 0; i < for_ids.size(); i++) {
            Contract c;
            c.conId = for_ids[i];
            c.symbol = "UNR";
            c.secType = "OPT";
            c.exchange = "FORECASTX";
            c.currency = "USD";
            std::cout << "send UNR OPT FORECASTX quotes\n";
            m_pClient->reqMktData(3+i, c, "", false, false, TagValueListSPtr());
        }
        
    }
    void accountSummary(int reqId, const std::string&, const std::string& tag, const std::string& value, const std::string&) {
        (void)reqId;
        try {
            if (tag == "TotalCashValue") {
                ib_cash_value = std::stof(value);
                ib_got_cash = true;
            } else if (tag == "NetLiquidation") {
                ib_nlv_value = std::stof(value);
            }
        } catch (...) {}
    }

    void error(int id, time_t errorTime, int errorCode, const std::string& errorString, const std::string& advancedOrderRejectJson) {
        if (errorCode == 2104 || errorCode == 2106 || errorCode == 2158 || errorCode == 2107 || errorCode == 2119 || errorCode == 2148) {
            return;
        }
        (void)errorTime;
        (void)advancedOrderRejectJson;
        if (errorString.find("Account:") != std::string::npos) {
            std::cout << "error " << id << " " << errorCode << "\n";
            return;
        }
        std::cout << "error " << id << " " << errorCode << ": " << errorString << "\n";
        if (errorCode == 201) {
            fx_hard_reject.store(true);
            {
                std::lock_guard<std::mutex> q(fx_order_mtx);
                fx_orders.clear();
            }
        }
        std::lock_guard<std::mutex> lock(fx_wait_mtx);
        if (id == fx_wait_oid || (errorCode == 201 && fx_wait_oid != -1)) {
            fx_t_done = std::chrono::steady_clock::now();
            fx_wait_filled = false;
            fx_wait_done = true;
            fx_wait_cv.notify_all();
        }
    }

    void managedAccounts(const std::string&) {
        std::cout << "accounts ok\n";
    }

    std::unique_ptr<EReader> m_pReader;
};


bool startForecastExFeed(MarketData& md, const std::vector<long int>& for_ids, void (*on_tick)(MarketData&, long int)) { 
    IbFeed feed(md);
    feed.on_tick = on_tick;
    feed.for_ids = for_ids;

    if (!feed.connect()) {
        std::cout << "ib connect failed\n";
        while (!fx_armed.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        return false;
    }

    const auto id_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
    while (!feed.got_id && std::chrono::steady_clock::now() < id_deadline) {
        feed.process();
    }
    if (!feed.got_id) {
        std::cout << "ib handshake timeout\n";
        return false;
    }

    feed.requestAccount();
    feed.requestNoContracts();
    feed.requestPositions();
    {
        const auto wait_until = std::chrono::steady_clock::now() + std::chrono::seconds(20);
        while (std::chrono::steady_clock::now() < wait_until) {
            feed.process();
            bool nos = fx_yes_to_no.size() >= for_ids.size() || for_ids.empty();
            if (feed.pos_end && nos && (ib_got_cash || feed.acct_end)) break;
        }
        std::cout << "fx NO resolved " << fx_yes_to_no.size() << "/" << for_ids.size() << "\n";
        feed.applyLots();
    }

    while (!fx_armed.load()) {
        feed.process();
    }

    if (!assisiClockOnly() && !assisiLiveOrders() && ibPort() == 4002 && !for_ids.empty()) {
        enqueueForecastExOrder(for_ids[0], 0.01f, 1);
    }

    feed.requestQuotes();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::minutes(3);

    while (std::chrono::steady_clock::now() < deadline) {
        feed.drainOrders();
        feed.process();
    }

    return 0; 
}
