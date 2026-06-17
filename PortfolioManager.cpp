#include "PortfolioManager.hpp"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <nlohmann/json.hpp>

PortfolioManager::PortfolioManager() {
    load();
}

void PortfolioManager::clear_screen() {
#if defined(_WIN32)
    std::system("cls");
#else
    std::system("clear");
#endif
}

double PortfolioManager::compute_hv() const {
    if (log_returns_.size() < 5) return 0.0;

    double sum = 0.0;
    for (double r : log_returns_) sum += r;
    double mean = sum / static_cast<double>(log_returns_.size());

    double sq_sum = 0.0;
    for (double r : log_returns_) {
        double dev = r - mean;
        sq_sum += dev * dev;
    }

    double variance = sq_sum / static_cast<double>(log_returns_.size() - 1);
    double daily_vol = std::sqrt(variance);
    return daily_vol * std::sqrt(252.0);
}

TickerSummary PortfolioManager::compute_summary(const std::string& ticker) const {
    TickerSummary s;
    int count = 0;
    for (const auto& p : positions_) {
        if (p.ticker != ticker) continue;
        ++count;
        s.mcDelta += p.mc.delta;
        s.mcGamma += p.mc.gamma;
        s.mcVega  += p.mc.vega;
        s.mcTheta += p.mc.theta;
        s.mcRho   += p.mc.rho;
        s.mcValue += p.mc.price;
        s.mcAvgIv += p.mc.impliedVol;
        s.bsDelta += p.bs.delta;
        s.bsGamma += p.bs.gamma;
        s.bsVega  += p.bs.vega;
        s.bsTheta += p.bs.theta;
        s.bsRho   += p.bs.rho;
        s.bsValue += p.bs.price;
        s.bsAvgIv += p.bs.impliedVol;
        s.totalCost += p.entryMarketPrice;
    }
    s.count = count;
    if (count > 0) {
        s.mcAvgIv /= static_cast<double>(count);
        s.bsAvgIv /= static_cast<double>(count);
    }
    return s;
}

void PortfolioManager::update(const Greeks& mc_greeks, const Greeks& bs_greeks, const MarketTick& tick) {
    if (!active_.load()) return;

    {
        std::lock_guard<std::mutex> lock(mutex_);

        Position pos;
        pos.ticker           = tick.ticker;
        pos.spotPrice        = tick.spotPrice;
        pos.strikePrice      = tick.strikePrice;
        pos.timeToExpiry     = tick.timeToExpiry;
        pos.riskFreeRate     = tick.riskFreeRate;
        pos.optionType       = tick.optionType;
        pos.entryMarketPrice = tick.optionMarketPrice;
        pos.mc               = mc_greeks;
        pos.bs               = bs_greeks;

        positions_.push_back(std::move(pos));

        lastSpot_   = tick.spotPrice;
        lastTicker_ = tick.ticker;

        running_mc_value_ += tick.optionMarketPrice;
        running_total_cost_ += tick.optionMarketPrice;

        // Track P&L history every 10 positions
        if (positions_.size() % 10 == 0) {
            double mc_sum = 0.0, cost_sum = 0.0;
            for (const auto& pos : positions_) {
                mc_sum += pos.mc.price;
                cost_sum += pos.entryMarketPrice;
            }
            double pl = mc_sum - cost_sum;
            pl_history_.push_back(pl);
            if (pl_history_.size() > 100) pl_history_.pop_front();
        }

        history_.push_back({tick.spotPrice});
        if (history_.size() > max_history_) {
            history_.pop_front();
        }

        if (history_.size() >= 2) {
            double prev = history_[history_.size() - 2].spot;
            double curr = history_.back().spot;
            if (prev > 0.0) {
                double log_ret = std::log(curr / prev);
                log_returns_.push_back(log_ret);
                if (log_returns_.size() > max_history_) {
                    log_returns_.pop_front();
                }
            }
        }
    }

    if (positions_.size() % 30 == 0) {
        save();
    }
}

void PortfolioManager::reset() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        positions_.clear();
        history_.clear();
        log_returns_.clear();
        pl_history_.clear();
        lastSpot_ = 0.0;
        lastTicker_.clear();
    }
    save();
    std::cout << "[PortfolioManager] Reset complete" << std::endl;
}

void PortfolioManager::print_ticker_section(const std::string& ticker, const TickerSummary& s) const {
    std::ostringstream oss;
    oss << "| " << std::left << std::setw(7) << ticker
        << "| " << std::right << std::setw(4) << s.count
        << "  | " << std::setw(9) << std::fixed << std::setprecision(2) << s.mcDelta
        << " | " << std::setw(9) << std::fixed << std::setprecision(4) << s.mcGamma
        << " | " << std::setw(9) << std::fixed << std::setprecision(1) << s.mcVega
        << " | " << std::setw(9) << std::fixed << std::setprecision(1) << s.mcTheta
        << " | " << std::setw(6) << std::fixed << std::setprecision(2) << s.mcAvgIv * 100.0 << "%"
        << "  |\n";
    std::cout << oss.str();
}

void PortfolioManager::print_dashboard() const {
    std::lock_guard<std::mutex> lock(mutex_);

    clear_screen();

    double hv = compute_hv();

    double totalMCValue = 0.0, totalBSValue = 0.0, totalCost = 0.0;
    double mcDelta = 0.0, mcGamma = 0.0, mcVega = 0.0, mcTheta = 0.0, mcRho = 0.0, mcAvgIv = 0.0;
    double bsDelta = 0.0, bsGamma = 0.0, bsVega = 0.0, bsTheta = 0.0, bsRho = 0.0, bsAvgIv = 0.0;
    int totalCount = static_cast<int>(positions_.size());

    std::unordered_map<std::string, TickerSummary> summaries;

    for (const auto& p : positions_) {
        mcDelta += p.mc.delta; mcGamma += p.mc.gamma; mcVega += p.mc.vega;
        mcTheta += p.mc.theta; mcRho += p.mc.rho; mcAvgIv += p.mc.impliedVol;
        bsDelta += p.bs.delta; bsGamma += p.bs.gamma; bsVega += p.bs.vega;
        bsTheta += p.bs.theta; bsRho += p.bs.rho; bsAvgIv += p.bs.impliedVol;
        totalCost += p.entryMarketPrice;
        totalMCValue += p.mc.price;
        totalBSValue += p.bs.price;

        auto& s = summaries[p.ticker];
        s.count += 1;
        s.mcDelta += p.mc.delta; s.mcGamma += p.mc.gamma; s.mcVega += p.mc.vega;
        s.mcTheta += p.mc.theta; s.mcRho += p.mc.rho; s.mcAvgIv += p.mc.impliedVol;
        s.mcValue += p.mc.price;
        s.bsDelta += p.bs.delta; s.bsGamma += p.bs.gamma; s.bsVega += p.bs.vega;
        s.bsTheta += p.bs.theta; s.bsRho += p.bs.rho; s.bsAvgIv += p.bs.impliedVol;
        s.bsValue += p.bs.price;
        s.totalCost += p.entryMarketPrice;
    }

    if (totalCount > 0) {
        mcAvgIv /= totalCount;
        bsAvgIv /= totalCount;
    }
    for (auto& [_, s] : summaries) {
        if (s.count > 0) {
            s.mcAvgIv /= s.count;
            s.bsAvgIv /= s.count;
        }
    }

    double unrealizedPl = totalMCValue - totalCost;

    std::cout << "+======================================================================+\n"
              << "|               REAL-TIME PORTFOLIO RISK DASHBOARD                      |\n"
              << "+======================================================================+\n"
              << "| Total Positions: " << std::setw(4) << totalCount
              << "  | Last Spot: $" << std::right << std::setw(8) << std::fixed << std::setprecision(2) << lastSpot_
              << "  | HV: " << std::setw(6) << hv * 100.0 << "%                       |\n"
              << "+------------------------------------------+-----------------------------+------------------+\n";

    auto print_row = [&](const std::string& name, double mc, double bs, const std::string& unit) {
        std::cout << "| " << std::left << std::setw(40) << name
                  << "| MC:" << std::right << std::setw(10) << std::fixed << std::setprecision(4) << mc
                  << " BS:" << std::setw(10) << std::fixed << std::setprecision(4) << bs
                  << " | " << std::left << std::setw(16) << unit << "|\n";
    };

    std::cout << "| " << std::left << std::setw(40) << "Greek"
              << "| MC         BS         | Status           |\n"
              << "+------------------------------------------+-----------------------------+------------------+\n";
    print_row("Delta",  mcDelta,  bsDelta,  mcDelta > 0.5 ? "LONG" : mcDelta < -0.5 ? "SHORT" : "NEUTRAL");
    print_row("Gamma",  mcGamma,  bsGamma,  mcGamma > 0.01 ? "CONVEX" : mcGamma < -0.01 ? "CONCAVE" : "FLAT");
    print_row("Vega",   mcVega,   bsVega,   mcVega > 1.0 ? "LONG VOL" : mcVega < -1.0 ? "SHORT VOL" : "FLAT");
    print_row("Theta",  mcTheta,  bsTheta,  mcTheta < -1.0 ? "TIME DECAY" : mcTheta > 1.0 ? "TIME GAIN" : "FLAT");
    print_row("Rho",    mcRho,    bsRho,    mcRho > 1.0 ? "LONG RATE" : mcRho < -1.0 ? "SHORT RATE" : "FLAT");

    std::cout << "+------------------------------------------+-----------------------------+------------------+\n"
              << "| MC Implied Vol (avg)                     | " << std::right << std::setw(26) << std::fixed << std::setprecision(2)
              << mcAvgIv * 100.0 << "%"
              << "     |                  |\n"
              << "| BS Implied Vol (avg)                     | " << std::right << std::setw(26) << std::fixed << std::setprecision(2)
              << bsAvgIv * 100.0 << "%"
              << "     |                  |\n"
              << "| Historical Volatility (annual)           | " << std::right << std::setw(26) << std::fixed << std::setprecision(2)
              << hv * 100.0 << "%"
              << "     |                  |\n"
              << "+------------------------------------------+-----------------------------+------------------+\n"
              << "| P&L Summary                                                                       |\n"
              << "+------------------------------------------+-----------------------------+------------------+\n"
              << "| Total Cost Basis (entry)                 | " << std::right << std::setw(24) << std::fixed << std::setprecision(2)
              << totalCost
              << "     |                  |\n"
              << "| Portfolio Value (MC)                     | " << std::right << std::setw(24) << std::fixed << std::setprecision(2)
              << totalMCValue
              << "     |                  |\n"
              << "| Portfolio Value (BS)                     | " << std::right << std::setw(24) << std::fixed << std::setprecision(2)
              << totalBSValue
              << "     |                  |\n"
              << "| Unrealized P&L (MC)                      | " << std::right << std::setw(24) << std::fixed << std::setprecision(2)
              << unrealizedPl
              << "     | " << (unrealizedPl >= 0.0 ? "PROFIT        " : "LOSS          ") << "|\n"
              << "+==========================================+=============================+==================+\n"
              << std::endl;

    if (!summaries.empty()) {
        std::cout << "  GREEKS SURFACE (MC) - Position Summary by Ticker\n"
                  << "  " << std::string(76, '=') << "\n"
                  << "  " << std::left << std::setw(7) << "Ticker"
                  << "| " << std::right << std::setw(4) << "#Pos"
                  << "  | " << std::setw(9) << "Delta"
                  << " | " << std::setw(9) << "Gamma"
                  << " | " << std::setw(9) << "Vega"
                  << " | " << std::setw(9) << "Theta"
                  << " | " << std::setw(6) << "IV"
                  << "  |\n"
                  << "  " << std::string(76, '-') << "\n";

        for (const auto& [ticker, s] : summaries) {
            std::cout << "  ";
            print_ticker_section(ticker, s);
        }

        std::cout << "  " << std::string(76, '-') << "\n"
                  << "  " << std::left << std::setw(7) << "TOTAL"
                  << "| " << std::right << std::setw(4) << totalCount
                  << "  | " << std::setw(9) << std::fixed << std::setprecision(2) << mcDelta
                  << " | " << std::setw(9) << std::fixed << std::setprecision(4) << mcGamma
                  << " | " << std::setw(9) << std::fixed << std::setprecision(1) << mcVega
                  << " | " << std::setw(9) << std::fixed << std::setprecision(1) << mcTheta
                  << " | " << std::setw(6) << std::fixed << std::setprecision(2) << mcAvgIv * 100.0 << "%"
                  << "  |\n"
                  << "  " << std::string(76, '=') << "\n"
                  << std::endl;
    }

    if (!history_.empty()) {
        std::cout << "  Price History (last " << std::setw(2) << history_.size() << " ticks, spot):\n  ";
        size_t count = 0;
        for (const auto& rec : history_) {
            std::cout << std::fixed << std::setprecision(2) << std::setw(7) << rec.spot;
            if (++count % 10 == 0 && count < history_.size()) {
                std::cout << "\n  ";
            } else if (count < history_.size()) {
                std::cout << " ";
            }
        }
        std::cout << "\n" << std::endl;
    }

    // Analytics section
    if (!pl_history_.empty()) {
        std::vector<double> pl_vals(pl_history_.begin(), pl_history_.end());
        double sum = std::accumulate(pl_vals.begin(), pl_vals.end(), 0.0);
        double mean = sum / pl_vals.size();
        double sq_sum = 0.0;
        for (double v : pl_vals) sq_sum += (v - mean) * (v - mean);
        double stddev = std::sqrt(sq_sum / pl_vals.size());

        std::sort(pl_vals.begin(), pl_vals.end());
        double var95 = pl_vals[static_cast<size_t>(pl_vals.size() * 0.05)];
        double var99 = pl_vals[static_cast<size_t>(pl_vals.size() * 0.01)];

        double sharpe = stddev > 0.0 ? (mean - 0.04) / stddev * std::sqrt(252.0) : 0.0;

        std::cout << "+======================================================================+\n"
                  << "| PORTFOLIO ANALYTICS                                                    |\n"
                  << "+======================================================================+\n"
                  << "| Sharpe Ratio (annual)  | " << std::right << std::setw(12) << std::fixed << std::setprecision(2) << sharpe
                  << "                                |\n"
                  << "| VaR (95%%)             | " << std::right << std::setw(12) << std::fixed << std::setprecision(2) << var95
                  << "                                |\n"
                  << "| VaR (99%%)             | " << std::right << std::setw(12) << std::fixed << std::setprecision(2) << var99
                  << "                                |\n"
                  << "+======================================================================+\n" << std::endl;
    }

    std::cout << "+======================================================================+\n" << std::endl;
}

nlohmann::json PortfolioManager::to_json() const {
    std::lock_guard<std::mutex> lock(mutex_);

    double mcDelta = 0.0, mcGamma = 0.0, mcVega = 0.0, mcTheta = 0.0, mcRho = 0.0;
    double bsDelta = 0.0, bsGamma = 0.0, bsVega = 0.0, bsTheta = 0.0, bsRho = 0.0;
    double totalCost = 0.0, mcValue = 0.0, bsValue = 0.0, mcAvgIv = 0.0, bsAvgIv = 0.0;
    int totalCount = static_cast<int>(positions_.size());

    std::unordered_map<std::string, nlohmann::json> tickerData;
    std::unordered_map<std::string, std::vector<nlohmann::json>> tickerPositions;

    for (const auto& p : positions_) {
        mcDelta += p.mc.delta; mcGamma += p.mc.gamma; mcVega += p.mc.vega;
        mcTheta += p.mc.theta; mcRho += p.mc.rho; mcAvgIv += p.mc.impliedVol;
        bsDelta += p.bs.delta; bsGamma += p.bs.gamma; bsVega += p.bs.vega;
        bsTheta += p.bs.theta; bsRho += p.bs.rho; bsAvgIv += p.bs.impliedVol;
        totalCost += p.entryMarketPrice;
        mcValue += p.mc.price;
        bsValue += p.bs.price;

        nlohmann::json td = nlohmann::json::object();
        auto it = tickerData.find(p.ticker);
        if (it != tickerData.end()) td = it->second;
        td["count"]          = td.value("count", 0) + 1;
        td["mc_delta"]       = td.value("mc_delta", 0.0) + p.mc.delta;
        td["mc_gamma"]       = td.value("mc_gamma", 0.0) + p.mc.gamma;
        td["mc_vega"]        = td.value("mc_vega", 0.0) + p.mc.vega;
        td["mc_theta"]       = td.value("mc_theta", 0.0) + p.mc.theta;
        td["mc_rho"]         = td.value("mc_rho", 0.0) + p.mc.rho;
        td["mc_value"]       = td.value("mc_value", 0.0) + p.mc.price;
        td["mc_iv_sum"]      = td.value("mc_iv_sum", 0.0) + p.mc.impliedVol;
        td["bs_delta"]       = td.value("bs_delta", 0.0) + p.bs.delta;
        td["bs_gamma"]       = td.value("bs_gamma", 0.0) + p.bs.gamma;
        td["bs_vega"]        = td.value("bs_vega", 0.0) + p.bs.vega;
        td["bs_theta"]       = td.value("bs_theta", 0.0) + p.bs.theta;
        td["bs_rho"]         = td.value("bs_rho", 0.0) + p.bs.rho;
        td["bs_value"]       = td.value("bs_value", 0.0) + p.bs.price;
        td["bs_iv_sum"]      = td.value("bs_iv_sum", 0.0) + p.bs.impliedVol;
        td["cost"]           = td.value("cost", 0.0) + p.entryMarketPrice;
        tickerData[p.ticker] = td;

        tickerPositions[p.ticker].push_back({
            {"spot",       p.spotPrice},
            {"strike",     p.strikePrice},
            {"expiry",     p.timeToExpiry},
            {"type",       p.optionType == OptionType::CALL ? "CALL" : "PUT"},
            {"mc_price",   p.mc.price},   {"mc_delta", p.mc.delta},
            {"mc_gamma",   p.mc.gamma},   {"mc_vega",  p.mc.vega},
            {"mc_theta",   p.mc.theta},   {"mc_iv",    p.mc.impliedVol},
            {"bs_price",   p.bs.price},   {"bs_delta", p.bs.delta},
            {"bs_gamma",   p.bs.gamma},   {"bs_vega",  p.bs.vega},
            {"bs_theta",   p.bs.theta},   {"bs_iv",    p.bs.impliedVol},
            {"entry_price", p.entryMarketPrice}
        });
    }

    if (totalCount > 0) {
        mcAvgIv /= static_cast<double>(totalCount);
        bsAvgIv /= static_cast<double>(totalCount);
    }
    double hv = compute_hv();
    double unrealizedPl = mcValue - totalCost;

    nlohmann::json tickers;
    for (auto& [sym, td] : tickerData) {
        int cnt = td["count"].get<int>();
        tickers[sym] = {
            {"count",   cnt},
            {"mc_delta", td["mc_delta"]}, {"mc_gamma", td["mc_gamma"]},
            {"mc_vega",  td["mc_vega"]},  {"mc_theta", td["mc_theta"]},
            {"mc_rho",   td["mc_rho"]},   {"mc_value", td["mc_value"]},
            {"mc_iv",    cnt > 0 ? td["mc_iv_sum"].get<double>() / cnt : 0.0},
            {"bs_delta", td["bs_delta"]}, {"bs_gamma", td["bs_gamma"]},
            {"bs_vega",  td["bs_vega"]},  {"bs_theta", td["bs_theta"]},
            {"bs_rho",   td["bs_rho"]},   {"bs_value", td["bs_value"]},
            {"bs_iv",    cnt > 0 ? td["bs_iv_sum"].get<double>() / cnt : 0.0},
            {"cost",     td["cost"]}
        };
    }

    nlohmann::json priceHistory = nlohmann::json::array();
    for (const auto& rec : history_) {
        priceHistory.push_back(rec.spot);
    }

    nlohmann::json positions_json = nlohmann::json::array();
    int start = std::max(0, totalCount - 10);
    for (int i = start; i < totalCount; ++i) {
        const auto& p = positions_[i];
        positions_json.push_back({
            {"ticker",     p.ticker},
            {"spot",       p.spotPrice},
            {"strike",     p.strikePrice},
            {"expiry",     p.timeToExpiry},
            {"type",       p.optionType == OptionType::CALL ? "CALL" : "PUT"},
            {"mc_price",   p.mc.price},   {"mc_delta", p.mc.delta},
            {"mc_gamma",   p.mc.gamma},   {"mc_vega",  p.mc.vega},
            {"mc_theta",   p.mc.theta},   {"mc_iv",    p.mc.impliedVol},
            {"bs_price",   p.bs.price},   {"bs_delta", p.bs.delta},
            {"bs_gamma",   p.bs.gamma},   {"bs_vega",  p.bs.vega},
            {"bs_theta",   p.bs.theta},   {"bs_iv",    p.bs.impliedVol},
            {"entry_price", p.entryMarketPrice}
        });
    }

    // ── Analytics ──
    double sharpe = 0.0, var95 = 0.0, var99 = 0.0, max_dd = 0.0;
    double stress_crash = 0.0, stress_vol = 0.0, stress_rate = 0.0;

    if (pl_history_.size() >= 5) {
        std::vector<double> pl_vals(pl_history_.begin(), pl_history_.end());
        double sum = std::accumulate(pl_vals.begin(), pl_vals.end(), 0.0);
        double mean = sum / pl_vals.size();
        double sq_sum = 0.0;
        for (double v : pl_vals) sq_sum += (v - mean) * (v - mean);
        double stddev = std::sqrt(sq_sum / pl_vals.size());

        std::sort(pl_vals.begin(), pl_vals.end());
        var95 = pl_vals[static_cast<size_t>(pl_vals.size() * 0.05)];
        var99 = pl_vals[static_cast<size_t>(pl_vals.size() * 0.01)];

        sharpe = stddev > 0.0 ? (mean - 0.04) / stddev * std::sqrt(252.0) : 0.0;

        // Max drawdown
        double peak = -1e18;
        for (double v : pl_vals) {
            if (v > peak) peak = v;
            double dd = (peak - v) / std::max(std::abs(peak), 1.0);
            if (dd > max_dd) max_dd = dd;
        }

        // Stress scenarios
        double crash_pnl = 0.0, vol_pnl = 0.0, rate_pnl = 0.0;
        for (const auto& p : positions_) {
            double crash_spot = p.spotPrice * 0.9;
            double intrinsic = p.optionType == OptionType::CALL
                ? std::max(crash_spot - p.strikePrice, 0.0)
                : std::max(p.strikePrice - crash_spot, 0.0);
            crash_pnl += intrinsic - p.mc.price;
            vol_pnl += p.mc.vega * 0.2;
            rate_pnl += p.mc.rho * 0.01;
        }
        stress_crash = crash_pnl;
        stress_vol = vol_pnl;
        stress_rate = rate_pnl;
    }

    nlohmann::json analytics = {
        {"sharpe",         sharpe},
        {"var95",          var95},
        {"var99",          var99},
        {"max_drawdown",   max_dd},
        {"stress_crash",   stress_crash},
        {"stress_vol",     stress_vol},
        {"stress_rate",    stress_rate},
        {"pl_history",     std::vector<double>(pl_history_.begin(), pl_history_.end())}
    };

    return {
        {"positions_total", totalCount},
        {"last_spot",       lastSpot_},
        {"last_ticker",     lastTicker_},
        {"mc_delta", mcDelta}, {"mc_gamma", mcGamma}, {"mc_vega", mcVega},
        {"mc_theta", mcTheta}, {"mc_rho",   mcRho},
        {"bs_delta", bsDelta}, {"bs_gamma", bsGamma}, {"bs_vega", bsVega},
        {"bs_theta", bsTheta}, {"bs_rho",   bsRho},
        {"total_cost",   totalCost},
        {"mc_value",     mcValue},
        {"bs_value",     bsValue},
        {"unrealized_pl", unrealizedPl},
        {"mc_avg_iv",    mcAvgIv},
        {"bs_avg_iv",    bsAvgIv},
        {"hv",           hv},
        {"tickers",      tickers},
        {"ticker_positions", tickerPositions},
        {"price_history", priceHistory},
        {"recent_positions", positions_json},
        {"analytics",    analytics}
    };
}

nlohmann::json PortfolioManager::chain_for_ticker(const std::string& ticker) const {
    std::lock_guard<std::mutex> lock(mutex_);
    nlohmann::json positions_json = nlohmann::json::array();
    for (const auto& p : positions_) {
        if (p.ticker != ticker) continue;
        positions_json.push_back({
            {"ticker",     p.ticker},
            {"spot",       p.spotPrice},
            {"strike",     p.strikePrice},
            {"expiry",     p.timeToExpiry},
            {"type",       p.optionType == OptionType::CALL ? "CALL" : "PUT"},
            {"mc_price",   p.mc.price},   {"mc_delta", p.mc.delta},
            {"mc_gamma",   p.mc.gamma},   {"mc_vega",  p.mc.vega},
            {"mc_theta",   p.mc.theta},   {"mc_iv",    p.mc.impliedVol},
            {"bs_price",   p.bs.price},   {"bs_delta", p.bs.delta},
            {"bs_gamma",   p.bs.gamma},   {"bs_vega",  p.bs.vega},
            {"bs_theta",   p.bs.theta},   {"bs_iv",    p.bs.impliedVol},
            {"entry_price", p.entryMarketPrice}
        });
    }
    return positions_json;
}

nlohmann::json PortfolioManager::export_json() const {
    nlohmann::json j = to_json();
    j["export_version"] = "2.0";
    j["export_timestamp"] = std::to_string(std::time(nullptr));
    return j;
}

std::string PortfolioManager::export_csv() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream csv;
    csv << "Ticker,Spot,Strike,Expiry,Type,"
        << "MC_Price,MC_Delta,MC_Gamma,MC_Vega,MC_Theta,MC_IV,"
        << "BS_Price,BS_Delta,BS_Gamma,BS_Vega,BS_Theta,BS_IV,"
        << "EntryPrice\n";
    for (const auto& p : positions_) {
        csv << p.ticker << ","
            << p.spotPrice << ","
            << p.strikePrice << ","
            << p.timeToExpiry << ","
            << (p.optionType == OptionType::CALL ? "CALL" : "PUT") << ","
            << p.mc.price << "," << p.mc.delta << "," << p.mc.gamma << ","
            << p.mc.vega << "," << p.mc.theta << "," << p.mc.impliedVol << ","
            << p.bs.price << "," << p.bs.delta << "," << p.bs.gamma << ","
            << p.bs.vega << "," << p.bs.theta << "," << p.bs.impliedVol << ","
            << p.entryMarketPrice << "\n";
    }
    return csv.str();
}

void PortfolioManager::save(const std::string& path) const {
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        nlohmann::json j = nlohmann::json::array();
        for (const auto& p : positions_) {
            j.push_back({
                {"ticker", p.ticker},
                {"spot", p.spotPrice},
                {"strike", p.strikePrice},
                {"expiry", p.timeToExpiry},
                {"rate", p.riskFreeRate},
                {"type", p.optionType == OptionType::CALL ? "CALL" : "PUT"},
                {"entry_price", p.entryMarketPrice},
                {"mc", {{"price", p.mc.price}, {"delta", p.mc.delta}, {"gamma", p.mc.gamma},
                        {"vega", p.mc.vega}, {"theta", p.mc.theta}, {"rho", p.mc.rho},
                        {"iv", p.mc.impliedVol}}},
                {"bs", {{"price", p.bs.price}, {"delta", p.bs.delta}, {"gamma", p.bs.gamma},
                        {"vega", p.bs.vega}, {"theta", p.bs.theta}, {"rho", p.bs.rho},
                        {"iv", p.bs.impliedVol}}}
            });
        }
        std::ofstream ofs(path);
        ofs << j.dump(2);
        std::cout << "[PortfolioManager] Saved " << positions_.size() << " positions to " << path << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[PortfolioManager] Save error: " << e.what() << std::endl;
    }
}

void PortfolioManager::load(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        std::ifstream ifs(path);
        if (!ifs.good()) return;
        nlohmann::json j;
        ifs >> j;
        if (!j.is_array()) return;
        positions_.clear();
        for (const auto& item : j) {
            Position p;
            p.ticker           = item.value("ticker", "");
            p.spotPrice        = item.value("spot", 0.0);
            p.strikePrice      = item.value("strike", 0.0);
            p.timeToExpiry     = item.value("expiry", 0.0);
            p.riskFreeRate     = item.value("rate", 0.04);
            p.entryMarketPrice = item.value("entry_price", 0.0);
            p.optionType       = item.value("type", "CALL") == "CALL" ? OptionType::CALL : OptionType::PUT;
            if (item.contains("mc")) {
                p.mc.price      = item["mc"].value("price", 0.0);
                p.mc.delta      = item["mc"].value("delta", 0.0);
                p.mc.gamma      = item["mc"].value("gamma", 0.0);
                p.mc.vega       = item["mc"].value("vega", 0.0);
                p.mc.theta      = item["mc"].value("theta", 0.0);
                p.mc.rho        = item["mc"].value("rho", 0.0);
                p.mc.impliedVol = item["mc"].value("iv", 0.0);
                p.mc.valid      = true;
            }
            if (item.contains("bs")) {
                p.bs.price      = item["bs"].value("price", 0.0);
                p.bs.delta      = item["bs"].value("delta", 0.0);
                p.bs.gamma      = item["bs"].value("gamma", 0.0);
                p.bs.vega       = item["bs"].value("vega", 0.0);
                p.bs.theta      = item["bs"].value("theta", 0.0);
                p.bs.rho        = item["bs"].value("rho", 0.0);
                p.bs.impliedVol = item["bs"].value("iv", 0.0);
                p.bs.valid      = true;
            }
            positions_.push_back(std::move(p));
        }
        if (!positions_.empty()) {
            lastSpot_   = positions_.back().spotPrice;
            lastTicker_ = positions_.back().ticker;
        }
        std::cout << "[PortfolioManager] Loaded " << positions_.size() << " positions from " << path << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[PortfolioManager] Load error: " << e.what() << std::endl;
    }
}

void PortfolioManager::stop() {
    save();
    active_.store(false);
}

bool PortfolioManager::is_active() const {
    return active_.load();
}
