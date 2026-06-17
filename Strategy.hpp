#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

#include "Common.hpp"

struct StrategyLeg {
    double strike;
    double expiry;
    OptionType type;
    int quantity; // +1 long, -1 short
    double entry_price = 0.0;
};

struct StrategyDef {
    std::string name;
    std::string ticker;
    double spot = 0.0;
    double rate = 0.04;
    std::vector<StrategyLeg> legs;
};

class StrategyBuilder {
public:
    static StrategyDef covered_call(const std::string& ticker, double spot, double strike, double expiry);
    static StrategyDef protective_put(const std::string& ticker, double spot, double strike, double expiry);
    static StrategyDef straddle(const std::string& ticker, double spot, double strike, double expiry);
    static StrategyDef strangle(const std::string& ticker, double spot, double low_strike, double high_strike, double expiry);
    static StrategyDef bull_call_spread(const std::string& ticker, double spot, double low_strike, double high_strike, double expiry);
    static StrategyDef bear_put_spread(const std::string& ticker, double spot, double low_strike, double high_strike, double expiry);
    static StrategyDef butterfly(const std::string& ticker, double spot, double low, double mid, double high, double expiry);
    static StrategyDef iron_condor(const std::string& ticker, double spot, double put_low, double put_high, double call_low, double call_high, double expiry);

    static nlohmann::json pnl_at_expiry(const StrategyDef& strat, double spot_min, double spot_max, int steps);
    static Greeks greeks_at_spot(const StrategyDef& strat, double spot);
    static nlohmann::json to_json(const StrategyDef& strat);
    static double bs_price(double S, double K, double T, double r, double sigma, OptionType type);
    static Greeks bs_greeks(double S, double K, double T, double r, double sigma, OptionType type);

private:
    static double norm_cdf(double x);
};
