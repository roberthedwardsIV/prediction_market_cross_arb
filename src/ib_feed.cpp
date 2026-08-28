#include <iostream>
#include <memory>
#include <ctime>
#include <thread>
#include <chrono>
#include <vector>

#include "DefaultEWrapper.h"
#include "EClientSocket.h"
#include "EReaderOSSignal.h"
#include "EReader.h"
#include "Contract.h"
#include "ErrorMessage.pb.h"
#include "ContractData.pb.h"

#include "market_data.hpp"
#include "ib_md.hpp"

int main() {
    std::cout << std::unitbuf;
    MarketData md;
    long int for_id = 890098180;
    std::vector<long int> for_venue_ids;
    for_venue_ids.push_back(for_id);

    md.Register(123, 11111111111, 0, for_id, 0);
    armForecastExTrading();
    std::cout << startForecastExFeed(md, for_venue_ids);
    
    return 0;
}
