#include "Strategy.hpp"
#include <cmath>
#include <sstream>
#include <algorithm>
#include <numeric>

double StrategyBuilder::norm_cdf(double x) {
    return 0.5 * (1.0 + std::erf(x / std::sqrt(2.0)));
}

double StrategyBuilder::bs_price(double S, double K, double T, double r, double sigma, OptionType type) {
    double d1 = (std::log(S / K) + (r + sigma * sigma * 0.5) * T) / (sigma * std::sqrt(T));
    double d2 = d1 - sigma * std::sqrt(T);
    if (type == OptionType::CALL)
        return S * norm_cdf(d1) - K * std::exp(-r * T) * norm_cdf(d2);
    else
        return K * std::exp(-r * T) * norm_cdf(-d2) - S * norm_cdf(-d1);
}

Greeks StrategyBuilder::bs_greeks(double S, double K, double T, double r, double sigma, OptionType type) {
    Greeks g;
    if (T <= 0.0 || sigma <= 0.0) { g.valid = false; return g; }
    double d1 = (std::log(S / K) + (r + sigma * sigma * 0.5) * T) / (sigma * std::sqrt(T));
    double d2 = d1 - sigma * std::sqrt(T);
    double pdf = std::exp(-0.5 * d1 * d1) / std::sqrt(2.0 * M_PI);
    if (type == OptionType::CALL) {
        g.price = S * norm_cdf(d1) - K * std::exp(-r * T) * norm_cdf(d2);
        g.delta = norm_cdf(d1);
        g.gamma = pdf / (S * sigma * std::sqrt(T));
        g.vega  = S * pdf * std::sqrt(T) / 100.0;
        g.theta = (-S * pdf * sigma / (2.0 * std::sqrt(T)) - r * K * std::exp(-r * T) * norm_cdf(d2)) / 365.0;
        g.rho   = K * T * std::exp(-r * T) * norm_cdf(d2) / 100.0;
    } else {
        g.price = K * std::exp(-r * T) * norm_cdf(-d2) - S * norm_cdf(-d1);
        g.delta = norm_cdf(d1) - 1.0;
        g.gamma = pdf / (S * sigma * std::sqrt(T));
        g.vega  = S * pdf * std::sqrt(T) / 100.0;
        g.theta = (-S * pdf * sigma / (2.0 * std::sqrt(T)) + r * K * std::exp(-r * T) * norm_cdf(-d2)) / 365.0;
        g.rho   = -K * T * std::exp(-r * T) * norm_cdf(-d2) / 100.0;
    }
    g.impliedVol = sigma;
    g.valid = true;
    return g;
}

// ── Strategy Factories ──
StrategyDef StrategyBuilder::covered_call(const std::string& ticker, double spot, double strike, double expiry) {
    double sigma = 0.25;
    StrategyDef s;
    s.name = "Covered Call"; s.ticker = ticker; s.spot = spot; s.rate = 0.04;
    s.legs = {{strike, expiry, OptionType::CALL, -1, bs_price(spot, strike, expiry, 0.04, sigma, OptionType::CALL)}};
    return s;
}

StrategyDef StrategyBuilder::protective_put(const std::string& ticker, double spot, double strike, double expiry) {
    double sigma = 0.25;
    StrategyDef s;
    s.name = "Protective Put"; s.ticker = ticker; s.spot = spot; s.rate = 0.04;
    s.legs = {{strike, expiry, OptionType::PUT, 1, bs_price(spot, strike, expiry, 0.04, sigma, OptionType::PUT)}};
    return s;
}

StrategyDef StrategyBuilder::straddle(const std::string& ticker, double spot, double strike, double expiry) {
    double sigma = 0.25;
    StrategyDef s;
    s.name = "Straddle"; s.ticker = ticker; s.spot = spot; s.rate = 0.04;
    s.legs = {
        {strike, expiry, OptionType::CALL, 1, bs_price(spot, strike, expiry, 0.04, sigma, OptionType::CALL)},
        {strike, expiry, OptionType::PUT,  1, bs_price(spot, strike, expiry, 0.04, sigma, OptionType::PUT)}
    };
    return s;
}

StrategyDef StrategyBuilder::strangle(const std::string& ticker, double spot, double low_strike, double high_strike, double expiry) {
    double sigma = 0.25;
    StrategyDef s;
    s.name = "Strangle"; s.ticker = ticker; s.spot = spot; s.rate = 0.04;
    s.legs = {
        {low_strike,  expiry, OptionType::PUT,  1, bs_price(spot, low_strike, expiry, 0.04, sigma, OptionType::PUT)},
        {high_strike, expiry, OptionType::CALL, 1, bs_price(spot, high_strike, expiry, 0.04, sigma, OptionType::CALL)}
    };
    return s;
}

StrategyDef StrategyBuilder::bull_call_spread(const std::string& ticker, double spot, double low_strike, double high_strike, double expiry) {
    double sigma = 0.25;
    StrategyDef s;
    s.name = "Bull Call Spread"; s.ticker = ticker; s.spot = spot; s.rate = 0.04;
    s.legs = {
        {low_strike,  expiry, OptionType::CALL, 1, bs_price(spot, low_strike, expiry, 0.04, sigma, OptionType::CALL)},
        {high_strike, expiry, OptionType::CALL, -1, bs_price(spot, high_strike, expiry, 0.04, sigma, OptionType::CALL)}
    };
    return s;
}

StrategyDef StrategyBuilder::bear_put_spread(const std::string& ticker, double spot, double low_strike, double high_strike, double expiry) {
    double sigma = 0.25;
    StrategyDef s;
    s.name = "Bear Put Spread"; s.ticker = ticker; s.spot = spot; s.rate = 0.04;
    s.legs = {
        {high_strike, expiry, OptionType::PUT, 1, bs_price(spot, high_strike, expiry, 0.04, sigma, OptionType::PUT)},
        {low_strike,  expiry, OptionType::PUT, -1, bs_price(spot, low_strike, expiry, 0.04, sigma, OptionType::PUT)}
    };
    return s;
}

StrategyDef StrategyBuilder::butterfly(const std::string& ticker, double spot, double low, double mid, double high, double expiry) {
    double sigma = 0.25;
    double r = 0.04;
    StrategyDef s;
    s.name = "Butterfly"; s.ticker = ticker; s.spot = spot; s.rate = r;
    s.legs = {
        {low,  expiry, OptionType::CALL, 1,  bs_price(spot, low, expiry, r, sigma, OptionType::CALL)},
        {mid,  expiry, OptionType::CALL, -2, bs_price(spot, mid, expiry, r, sigma, OptionType::CALL)},
        {high, expiry, OptionType::CALL, 1,  bs_price(spot, high, expiry, r, sigma, OptionType::CALL)}
    };
    return s;
}

StrategyDef StrategyBuilder::iron_condor(const std::string& ticker, double spot, double put_low, double put_high, double call_low, double call_high, double expiry) {
    double sigma = 0.25;
    double r = 0.04;
    StrategyDef s;
    s.name = "Iron Condor"; s.ticker = ticker; s.spot = spot; s.rate = r;
    s.legs = {
        {put_low,  expiry, OptionType::PUT,  -1, bs_price(spot, put_low, expiry, r, sigma, OptionType::PUT)},
        {put_high, expiry, OptionType::PUT,  1,  bs_price(spot, put_high, expiry, r, sigma, OptionType::PUT)},
        {call_low, expiry, OptionType::CALL, 1,  bs_price(spot, call_low, expiry, r, sigma, OptionType::CALL)},
        {call_high,expiry, OptionType::CALL, -1, bs_price(spot, call_high, expiry, r, sigma, OptionType::CALL)}
    };
    return s;
}

Greeks StrategyBuilder::greeks_at_spot(const StrategyDef& strat, double spot) {
    Greeks combined;
    double sigma = 0.25;
    for (const auto& leg : strat.legs) {
        Greeks g = bs_greeks(spot, leg.strike, leg.expiry, strat.rate, sigma, leg.type);
        combined.price  += g.price * leg.quantity;
        combined.delta  += g.delta * leg.quantity;
        combined.gamma  += g.gamma * leg.quantity;
        combined.vega   += g.vega * leg.quantity;
        combined.theta  += g.theta * leg.quantity;
        combined.rho    += g.rho * leg.quantity;
    }
    combined.valid = true;
    return combined;
}

nlohmann::json StrategyBuilder::pnl_at_expiry(const StrategyDef& strat, double spot_min, double spot_max, int steps) {
    double total_cost = 0.0;
    for (const auto& leg : strat.legs)
        total_cost += leg.entry_price * leg.quantity;

    nlohmann::json data = nlohmann::json::array();
    double step = (spot_max - spot_min) / steps;
    for (int i = 0; i <= steps; ++i) {
        double S = spot_min + i * step;
        // At expiry, option value = intrinsic value
        double pnl = -total_cost;
        for (const auto& leg : strat.legs) {
            double intrinsic = 0.0;
            if (leg.type == OptionType::CALL)
                intrinsic = std::max(S - leg.strike, 0.0);
            else
                intrinsic = std::max(leg.strike - S, 0.0);
            pnl += intrinsic * leg.quantity;
        }
        Greeks g = greeks_at_spot(strat, S);
        data.push_back({{"spot", S}, {"pnl", pnl}, {"delta", g.delta}, {"gamma", g.gamma}});
    }
    return data;
}

nlohmann::json StrategyBuilder::to_json(const StrategyDef& strat) {
    nlohmann::json legs = nlohmann::json::array();
    double total_cost = 0.0;
    for (const auto& leg : strat.legs) {
        total_cost += leg.entry_price * leg.quantity;
        legs.push_back({
            {"strike", leg.strike}, {"expiry", leg.expiry},
            {"type", leg.type == OptionType::CALL ? "CALL" : "PUT"},
            {"qty", leg.quantity}, {"entry_price", leg.entry_price}
        });
    }
    Greeks g = greeks_at_spot(strat, strat.spot);
    return {
        {"name", strat.name}, {"ticker", strat.ticker}, {"spot", strat.spot},
        {"legs", legs}, {"total_cost", total_cost},
        {"price", g.price}, {"delta", g.delta}, {"gamma", g.gamma},
        {"vega", g.vega}, {"theta", g.theta}, {"rho", g.rho},
        {"pnl_data", pnl_at_expiry(strat, strat.spot * 0.5, strat.spot * 1.5, 50)}
    };
}
