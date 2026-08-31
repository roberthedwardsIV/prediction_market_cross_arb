struct RiskLimits {
    float max_market_pct;               // % of portfolio per market
    float max_allocation_pct;           // % of cash spent on contracts
    float venue_reserve_pct;            // % of cash to save on each Venue
    int   max_contracts_per_market;     // max count of contracts per market
};

bool approve_pair(const OrderIntent& yes, const OrderIntent& no, const PortfolioManager& book, const RiskLimits& limits) {
    
    // will we own more than the limit of max contracts after doing the two buys?
    int current_yes_count = book.getPositionByMarketId(yes.market_id).yes_count;
    int current_no_count = book.getPositionByMarketId(no.market_id).no_count;
    if((current_yes_count + yes.size) > max_contracts_per_market || (current_no_count + no.size) > max_contracts_per_market) { return false; }

    // will we have enough cash in each venue after doing the two buys?
    float  current_yes_venue_cash;
    if(yes.venue_id == Kalshi) { current_yes_venue_cash = book.kalshi_cash(); }
    else if(yes.venue_id == Polymarket) { current_yes_venue_cash = book.polymarket_cash(); }
    else if(yes.venue_id == Gemini) { current_yes_venue_cash = book.gemini_cash(); }

    float  current_no_venue_cash;
    if(no.venue_id == Kalshi) { current_no_venue_cash = book.kalshi_cash(); }
    else if(no.venue_id == Polymarket) { current_no_venue_cash = book.polymarket_cash(); }
    else if(no.venue_id == Gemini) { current_no_venue_cash = book.gemini_cash(); }

    
    // will cash spent / total cash + positions exceed out limit after the two buys?
    // will we own too much of this one market after the two buys?

    // 

    book.kalshi_cash();
    book.polymarket_cash();
    book.gemini_cash();

    

}