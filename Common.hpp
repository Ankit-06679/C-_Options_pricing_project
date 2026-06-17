#pragma once

#include <string>
#include <chrono>

enum class OptionType {
    CALL,
    PUT
};

enum class OptionStyle {
    EUROPEAN,
    ASIAN,
    BARRIER,
    LOOKBACK
};

struct MarketTick {
    std::string        ticker;
    std::string        timestamp;
    double             spotPrice;
    double             strikePrice;
    double             timeToExpiry;
    double             riskFreeRate;
    double             optionMarketPrice;
    OptionType         optionType;
    OptionStyle        optionStyle = OptionStyle::EUROPEAN;
    double             barrier     = 0.0;  // for barrier options
    bool               barrier_down = true; // down-and-out vs up-and-out
};

struct Greeks {
    double price       = 0.0;
    double delta       = 0.0;
    double gamma       = 0.0;
    double vega        = 0.0;
    double theta       = 0.0;
    double rho         = 0.0;
    double impliedVol  = 0.0;
    bool   valid       = false;
};
