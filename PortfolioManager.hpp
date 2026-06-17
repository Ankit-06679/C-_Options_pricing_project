#pragma once

#include <mutex>
#include <atomic>
#include <deque>
#include <vector>
#include <string>
#include <unordered_map>

#include "Common.hpp"
#include <nlohmann/json.hpp>

struct Position {
    std::string ticker;
    double spotPrice;
    double strikePrice;
    double timeToExpiry;
    double riskFreeRate;
    OptionType optionType;
    double entryMarketPrice;
    Greeks mc;
    Greeks bs;
};

struct TickerSummary {
    int    count        = 0;
    double totalCost    = 0.0;
    double mcDelta      = 0.0;
    double mcGamma      = 0.0;
    double mcVega       = 0.0;
    double mcTheta      = 0.0;
    double mcRho        = 0.0;
    double mcValue      = 0.0;
    double mcAvgIv      = 0.0;
    double bsDelta      = 0.0;
    double bsGamma      = 0.0;
    double bsVega       = 0.0;
    double bsTheta      = 0.0;
    double bsRho        = 0.0;
    double bsValue      = 0.0;
    double bsAvgIv      = 0.0;
};

class PortfolioManager {
public:
    PortfolioManager();

    void update(const Greeks& mc_greeks, const Greeks& bs_greeks, const MarketTick& tick);
    void reset();
    void print_dashboard() const;
    nlohmann::json to_json() const;
    nlohmann::json chain_for_ticker(const std::string& ticker) const;
    nlohmann::json export_json() const;
    std::string    export_csv() const;
    void save(const std::string& path = "portfolio_snapshot.json") const;
    void load(const std::string& path = "portfolio_snapshot.json");
    void stop();
    bool is_active() const;

private:
    struct TickRecord {
        double spot;
    };

    static void clear_screen();
    double compute_hv() const;
    TickerSummary compute_summary(const std::string& ticker) const;
    void print_ticker_section(const std::string& ticker, const TickerSummary& s) const;

    mutable std::mutex mutex_;
    std::atomic<bool> active_{true};

    std::vector<Position> positions_;

    double lastSpot_ = 0.0;
    std::string lastTicker_;

    std::deque<TickRecord> history_;
    static constexpr size_t max_history_ = 40;

    std::deque<double> log_returns_;
    std::deque<double> pl_history_;
    double running_mc_value_ = 0.0;
    double running_total_cost_ = 0.0;
};
