#include <string>

enum class RiskReason {
    Ok,
    ContractCap,
    VenueCash,
    VenueReserve,
    NoCash,
    Allocation,
    NoEquity,
    MarketPct
};

std::string ReasonLogger(RiskReason reason) {
    switch(reason) {
        case RiskReason::Ok:
            return "Ok";
        case RiskReason::ContractCap:
            return "Contract cap reached";
        case RiskReason::VenueCash:
            return "Venue cash insufficient";
        case RiskReason::VenueReserve:
            return "Venue reserves too low";
        case RiskReason::NoCash:
            return "No cash";
        case RiskReason::Allocation:
            return "Maximum cash allocation reached";
        case RiskReason::NoEquity:
            return "No equity";
        case RiskReason::MarketPct:
            return "Market percentage limit reached";
    }
    return "Unknown";
}
struct RiskLimits {
    float max_market_pct;               // % of portfolio per market
    float max_allocation_pct;           // % of cash spent on contracts
    float venue_reserve_pct;            // % of cash to save on each Venue
    int   max_contracts_per_market;     // max count of contracts per market
};

RiskReason approve_pair(const OrderIntent& yes, const OrderIntent& no, PortfolioManager& book, const RiskLimits& limits) {
    // does this intent exceed the max # of contracts per market?
    int current_contract_count = book.getPositionByMarketId(yes.market_id).yes_count + book.getPositionByMarketId(no.market_id).no_count;
    if((current_contract_count + yes.size + no.size) > limits.max_contracts_per_market) { return RiskReason::ContractCap; }

    // does this intent cause venue-specific cash reserve to be violated?
    float kalshi_cash = book.kalshiCash();
    float polymarket_cash = book.polymarketCash();
    float gemini_cash = book.geminiCash();

    float kalshi_exposure = book.getPortfolio().kalshi_exposure;
    float polymarket_exposure = book.getPortfolio().polymarket_exposure;
    float gemini_exposure = book.getPortfolio().gemini_exposure;

    float yes_cost = (yes.size * yes.limit_price) + takerFee(yes.venue, yes.limit_price, yes.size);
    if(yes.venue == Kalshi) { 
        if(yes_cost > kalshi_cash) { return RiskReason::VenueCash; }
        else if(kalshi_exposure + yes_cost > (kalshi_cash + kalshi_exposure) * (1 - limits.venue_reserve_pct)) { return RiskReason::VenueReserve; }
    }
    else if(yes.venue == Polymarket) {
        if(yes_cost > polymarket_cash) { return RiskReason::VenueCash; }
        else if(polymarket_exposure + yes_cost > (polymarket_cash + polymarket_exposure) * (1 - limits.venue_reserve_pct)) { return RiskReason::VenueReserve; }
    }
    else if(yes.venue == Gemini) {
        if(yes_cost > gemini_cash) { return RiskReason::VenueCash; }
        else if(gemini_exposure + yes_cost > (gemini_cash + gemini_exposure) * (1 - limits.venue_reserve_pct)) { return RiskReason::VenueReserve; }
    }


    bool same_venue = (no.venue == yes.venue);
    float no_cost = (no.size * no.limit_price) + takerFee(no.venue, no.limit_price, no.size);
    if(no.venue == Kalshi) { 
        if(no_cost > kalshi_cash) { return RiskReason::VenueCash; }
        else if(kalshi_exposure + no_cost > (kalshi_cash + kalshi_exposure) * (1 - limits.venue_reserve_pct)) { return RiskReason::VenueReserve; }
        else if (same_venue && yes_cost + no_cost > kalshi_cash) { return RiskReason::VenueCash; }
        else if (same_venue && kalshi_exposure + yes_cost + no_cost > (kalshi_cash + kalshi_exposure) * (1 - limits.venue_reserve_pct)) { return RiskReason::VenueReserve; }

    }

    else if(no.venue == Polymarket) {
        if(no_cost > polymarket_cash) { return RiskReason::VenueCash; }
        else if(polymarket_exposure + no_cost > (polymarket_cash + polymarket_exposure) * (1 - limits.venue_reserve_pct)) { return RiskReason::VenueReserve; }
        else if (same_venue && yes_cost + no_cost > polymarket_cash) { return RiskReason::VenueCash; }
        else if (same_venue && polymarket_exposure + yes_cost + no_cost > (polymarket_cash + polymarket_exposure) * (1 - limits.venue_reserve_pct)) { return RiskReason::VenueReserve; }
    }
    else if(no.venue == Gemini) {
        if(no_cost > gemini_cash) { return RiskReason::VenueCash; }
        else if(gemini_exposure + no_cost > (gemini_cash + gemini_exposure) * (1 - limits.venue_reserve_pct)) { return RiskReason::VenueReserve; }
        else if (same_venue && yes_cost + no_cost > gemini_cash) { return RiskReason::VenueCash; }
        else if (same_venue && gemini_exposure + yes_cost + no_cost > (gemini_cash + gemini_exposure) * (1 - limits.venue_reserve_pct)) { return RiskReason::VenueReserve; }

    }
 
    // does this intent cause total cash allocation limit to be violated?
    float total_exposure = kalshi_exposure + polymarket_exposure + gemini_exposure;
    float total_cash = kalshi_cash + polymarket_cash + gemini_cash;
    float proposed_spend = yes_cost + no_cost;
    if(total_cash == 0) { return RiskReason::NoCash; }
    if(((total_exposure + proposed_spend) / total_cash) > limits.max_allocation_pct) { return RiskReason::Allocation; }


    // will we own too much of this one market after the two buys?
    float market_specific_exposure = (book.getPositionByMarketId(yes.market_id).average_yes_price * book.getPositionByMarketId(yes.market_id).yes_count) +
                                        (book.getPositionByMarketId(no.market_id).average_no_price * book.getPositionByMarketId(no.market_id).no_count);
    float total_equity = total_cash + total_exposure;
    if(total_equity == 0) { return RiskReason::NoEquity; }
    if(((market_specific_exposure + proposed_spend) / total_equity) > limits.max_market_pct) { return RiskReason::MarketPct; }
     
    



    return RiskReason::Ok;

}