#include "market_data.hpp"

#include <vector>

void enqueueForecastExOrder(long int for_id, float limit_price, int size, bool buy_no = false);
void forecastExResetHardReject();
bool forecastExHardReject();
bool forecastExNoReady(long int yes_id);
long int forecastExNoId(long int yes_id);
bool waitForecastExFill(int timeout_ms);
long forecastExQueueUs();
long forecastExPlaceCallUs();
long forecastExFillUs();
float forecastExCash();
float forecastExAccountValue();
void setForecastExLotHandler(void (*fn)(long int conid, int size, float avg));
void armForecastExTrading();
bool startForecastExFeed(MarketData& md, const std::vector<long int>& for_ids, void (*on_tick)(MarketData&, long int) = nullptr);
