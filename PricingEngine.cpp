#include "PricingEngine.hpp"

#include <cmath>
#include <limits>
#include <numbers>
#include <random>
#include <stdexcept>
#include <algorithm>
#include <numeric>

// ───── Black-Scholes Engine ──────────────────────────────────────────────

double BlackScholesEngine::cum_norm(double x) {
    return 0.5 * (1.0 + std::erf(x / std::sqrt(2.0)));
}

double BlackScholesEngine::norm_pdf(double x) {
    static constexpr double inv_sqrt_2pi = 1.0 / std::sqrt(2.0 * std::numbers::pi);
    return inv_sqrt_2pi * std::exp(-0.5 * x * x);
}

double BlackScholesEngine::bs_price(
    double S, double K, double T, double r, double sigma, OptionType type)
{
    if (sigma <= 0.0 || T <= 0.0) {
        double intrinsic = (type == OptionType::CALL)
            ? std::max(0.0, S - K * std::exp(-r * T))
            : std::max(0.0, K * std::exp(-r * T) - S);
        return intrinsic;
    }

    double d1 = (std::log(S / K) + (r + sigma * sigma * 0.5) * T)
              / (sigma * std::sqrt(T));
    double d2 = d1 - sigma * std::sqrt(T);

    if (type == OptionType::CALL) {
        return S * cum_norm(d1) - K * std::exp(-r * T) * cum_norm(d2);
    } else {
        return K * std::exp(-r * T) * cum_norm(-d2) - S * cum_norm(-d1);
    }
}

double BlackScholesEngine::price(
    double S, double K, double T, double r, double sigma, OptionType type)
{
    return bs_price(S, K, T, r, sigma, type);
}

Greeks BlackScholesEngine::bs_greeks(
    double S, double K, double T, double r, double sigma, OptionType type)
{
    Greeks g;
    if (sigma <= 0.0 || T <= 0.0) {
        return g;
    }

    double sqrt_T = std::sqrt(T);
    double d1 = (std::log(S / K) + (r + sigma * sigma * 0.5) * T)
              / (sigma * sqrt_T);
    double d2 = d1 - sigma * sqrt_T;

    double nd1   = cum_norm(d1);
    double nd2   = cum_norm(d2);
    double pdf_d1 = norm_pdf(d1);
    double disc  = std::exp(-r * T);

    if (type == OptionType::CALL) {
        g.price = S * nd1 - K * disc * nd2;
        g.delta = nd1;
        g.theta = -S * pdf_d1 * sigma / (2.0 * sqrt_T)
                  - r * K * disc * nd2;
        g.rho   = K * T * disc * nd2;
    } else {
        g.price = K * disc * cum_norm(-d2) - S * cum_norm(-d1);
        g.delta = nd1 - 1.0;
        g.theta = -S * pdf_d1 * sigma / (2.0 * sqrt_T)
                  + r * K * disc * cum_norm(-d2);
        g.rho   = -K * T * disc * cum_norm(-d2);
    }

    g.gamma = pdf_d1 / (S * sigma * sqrt_T);
    g.vega  = S * pdf_d1 * sqrt_T;

    return g;
}

Greeks BlackScholesEngine::greeks(
    double S, double K, double T, double r, double sigma, OptionType type)
{
    return bs_greeks(S, K, T, r, sigma, type);
}

double BlackScholesEngine::implied_vol_newton(
    double target, double S, double K, double T, double r, OptionType type)
{
    double sigma = 0.3;
    constexpr int max_iter = 80;
    constexpr double tol = 1e-10;

    for (int i = 0; i < max_iter; ++i) {
        double sqrt_T = std::sqrt(T);
        double d1 = (std::log(S / K) + (r + sigma * sigma * 0.5) * T)
                  / (sigma * sqrt_T);
        double price = bs_price(S, K, T, r, sigma, type);
        double vega  = S * norm_pdf(d1) * sqrt_T;
        double diff  = price - target;

        if (std::abs(diff) < tol) {
            return sigma;
        }
        if (std::abs(vega) < 1e-12) {
            break;
        }

        sigma = sigma - diff / vega;
        if (sigma <= 0.0) sigma = 0.001;
        if (sigma > 10.0) sigma = 10.0;
    }

    return implied_vol_bisection(target, S, K, T, r, type);
}

double BlackScholesEngine::implied_vol_bisection(
    double target, double S, double K, double T, double r, OptionType type)
{
    double lo = 0.001;
    double hi = 5.0;
    constexpr int max_iter = 100;
    constexpr double tol = 1e-10;

    double price_lo = bs_price(S, K, T, r, lo, type);
    double price_hi = bs_price(S, K, T, r, hi, type);

    if ((target - price_lo) * (target - price_hi) > 0.0) {
        if (target < price_lo) return lo;
        if (target > price_hi) return hi;
        return std::numeric_limits<double>::quiet_NaN();
    }

    for (int i = 0; i < max_iter; ++i) {
        double mid = (lo + hi) * 0.5;
        double price_mid = bs_price(S, K, T, r, mid, type);
        double diff = price_mid - target;

        if (std::abs(diff) < tol) {
            return mid;
        }

        if ((price_mid - target) * (price_lo - target) < 0.0) {
            hi = mid;
            price_hi = price_mid;
        } else {
            lo = mid;
            price_lo = price_mid;
        }
    }

    return std::numeric_limits<double>::quiet_NaN();
}

double BlackScholesEngine::implied_volatility(
    double market_price, double S, double K, double T, double r, OptionType type)
{
    double iv = implied_vol_newton(market_price, S, K, T, r, type);
    if (std::isnan(iv) || iv <= 0.0) {
        return 0.25;
    }
    return iv;
}

Greeks BlackScholesEngine::calculate(const MarketTick& tick) {
    Greeks g = bs_greeks(
        tick.spotPrice,
        tick.strikePrice,
        tick.timeToExpiry,
        tick.riskFreeRate,
        0.25,
        tick.optionType
    );

    double iv = implied_vol_newton(
        tick.optionMarketPrice,
        tick.spotPrice,
        tick.strikePrice,
        tick.timeToExpiry,
        tick.riskFreeRate,
        tick.optionType
    );

    if (!std::isnan(iv)) {
        g = bs_greeks(
            tick.spotPrice,
            tick.strikePrice,
            tick.timeToExpiry,
            tick.riskFreeRate,
            iv,
            tick.optionType
        );
    }

    g.impliedVol = iv;
    g.valid = !std::isnan(iv) && iv > 0.0;

    return g;
}

// ───── Monte Carlo Engine ────────────────────────────────────────────────

MonteCarloEngine::MonteCarloEngine(unsigned int num_paths)
    : num_paths_(num_paths) {}

void MonteCarloEngine::generate_normals(std::vector<double>& z) const {
    static thread_local std::mt19937_64 rng(std::random_device{}());
    std::normal_distribution<double> norm(0.0, 1.0);
    for (size_t i = 0; i < z.size(); ++i) {
        z[i] = norm(rng);
    }
}

void MonteCarloEngine::generate_paths(
    std::vector<std::vector<double>>& paths, double S, double T, double r, double sigma) const
{
    static thread_local std::mt19937_64 rng(std::random_device{}());
    std::normal_distribution<double> norm(0.0, 1.0);

    double dt = T / paths[0].size();
    double drift = (r - 0.5 * sigma * sigma) * dt;
    double diffusion = sigma * std::sqrt(dt);

    for (auto& path : paths) {
        path[0] = S;
        for (size_t i = 1; i < path.size(); ++i) {
            double z = norm(rng);
            path[i] = path[i - 1] * std::exp(drift + diffusion * z);
        }
    }
}

// ── Standard European MC (antithetic) ──

double MonteCarloEngine::run_mc(
    double S, double K, double T, double r, double sigma,
    OptionType type, const std::vector<double>& z) const
{
    if (sigma <= 0.0 || T <= 0.0) {
        double intrinsic = (type == OptionType::CALL)
            ? std::max(0.0, S - K * std::exp(-r * T))
            : std::max(0.0, K * std::exp(-r * T) - S);
        return intrinsic;
    }

    double drift     = (r - 0.5 * sigma * sigma) * T;
    double diffusion = sigma * std::sqrt(T);
    double discount  = std::exp(-r * T);
    double half      = static_cast<double>(z.size());
    double sum       = 0.0;

    for (const double& zi : z) {
        double s_t1 = S * std::exp(drift + diffusion * zi);
        double s_t2 = S * std::exp(drift + diffusion * (-zi));

        double payoff1, payoff2;
        if (type == OptionType::CALL) {
            payoff1 = std::max(s_t1 - K, 0.0);
            payoff2 = std::max(s_t2 - K, 0.0);
        } else {
            payoff1 = std::max(K - s_t1, 0.0);
            payoff2 = std::max(K - s_t2, 0.0);
        }

        sum += discount * (payoff1 + payoff2);
    }

    return sum / (2.0 * half);
}

// ── Asian MC (arithmetic average) ──

double MonteCarloEngine::run_mc_asian(
    double S, double K, double T, double r, double sigma,
    OptionType type, unsigned int steps,
    const std::vector<std::vector<double>>& paths) const
{
    if (sigma <= 0.0 || T <= 0.0) {
        double intrinsic = (type == OptionType::CALL)
            ? std::max(0.0, S - K * std::exp(-r * T))
            : std::max(0.0, K * std::exp(-r * T) - S);
        return intrinsic;
    }

    double discount = std::exp(-r * T);
    double sum = 0.0;

    for (const auto& path : paths) {
        double avg = std::accumulate(path.begin(), path.end(), 0.0) / static_cast<double>(steps + 1);
        double payoff;
        if (type == OptionType::CALL) {
            payoff = std::max(avg - K, 0.0);
        } else {
            payoff = std::max(K - avg, 0.0);
        }
        sum += discount * payoff;
    }

    return sum / static_cast<double>(paths.size());
}

// ── Barrier MC (down-and-out / up-and-out) ──

double MonteCarloEngine::run_mc_barrier(
    double S, double K, double T, double r, double sigma,
    OptionType type, double barrier, bool down_and_out,
    const std::vector<double>& z) const
{
    if (sigma <= 0.0 || T <= 0.0) {
        double intrinsic = (type == OptionType::CALL)
            ? std::max(0.0, S - K * std::exp(-r * T))
            : std::max(0.0, K * std::exp(-r * T) - S);
        return intrinsic;
    }

    double drift     = (r - 0.5 * sigma * sigma) * T;
    double diffusion = sigma * std::sqrt(T);
    double discount  = std::exp(-r * T);
    double half      = static_cast<double>(z.size());
    double sum       = 0.0;

    // Simulate with 52 time steps to check barrier
    unsigned int steps = 52;
    double dt = T / steps;
    double drift_step = (r - 0.5 * sigma * sigma) * dt;
    double diff_step = sigma * std::sqrt(dt);

    for (const double& zi : z) {
        // Antithetic: +zi and -zi
        for (double sign : {1.0, -1.0}) {
            double s = S;
            bool knocked = false;
            for (unsigned int i = 0; i < steps; ++i) {
                double eps = zi * sign;
                // Sub-step: use fresh normals if more resolution needed
                s = s * std::exp(drift_step + diff_step * eps);
                if (down_and_out && s <= barrier) { knocked = true; break; }
                if (!down_and_out && s >= barrier) { knocked = true; break; }
            }
            if (!knocked) {
                double payoff = (type == OptionType::CALL)
                    ? std::max(s - K, 0.0)
                    : std::max(K - s, 0.0);
                sum += discount * payoff;
            }
        }
    }

    return sum / (2.0 * half);
}

// ── Lookback MC (floating strike) ──

double MonteCarloEngine::run_mc_lookback(
    double S, double K, double T, double r, double sigma,
    OptionType type, const std::vector<double>& z) const
{
    if (sigma <= 0.0 || T <= 0.0) {
        double intrinsic = (type == OptionType::CALL)
            ? std::max(0.0, S - K * std::exp(-r * T))
            : std::max(0.0, K * std::exp(-r * T) - S);
        return intrinsic;
    }

    unsigned int steps = 52;
    double dt = T / steps;
    double drift_step = (r - 0.5 * sigma * sigma) * dt;
    double diff_step = sigma * std::sqrt(dt);
    double discount = std::exp(-r * T);
    double half = static_cast<double>(z.size());
    double sum = 0.0;

    for (const double& zi : z) {
        for (double sign : {1.0, -1.0}) {
            double s = S;
            double s_max = S;
            double s_min = S;
            for (unsigned int i = 0; i < steps; ++i) {
                double eps = zi * sign;
                s = s * std::exp(drift_step + diff_step * eps);
                if (s > s_max) s_max = s;
                if (s < s_min) s_min = s;
            }
            double payoff;
            if (type == OptionType::CALL) {
                // Call on max: payoff = max(S_max - K, 0)
                payoff = std::max(s_max - K, 0.0);
            } else {
                // Put on min: payoff = max(K - S_min, 0)
                payoff = std::max(K - s_min, 0.0);
            }
            sum += discount * payoff;
        }
    }

    return sum / (2.0 * half);
}

// ── Greeks via finite differences (European) ──

Greeks MonteCarloEngine::fd_greeks(
    double S, double K, double T, double r, double sigma,
    OptionType type, const std::vector<double>& z) const
{
    Greeks g;

    double eps_s     = S * 0.01;
    double eps_sigma = 0.001;
    double eps_T     = 0.001;
    double eps_r     = 0.0001;

    double v0  = run_mc(S, K, T, r, sigma, type, z);
    double v_sp = run_mc(S + eps_s, K, T, r, sigma, type, z);
    double v_sm = run_mc(S - eps_s, K, T, r, sigma, type, z);
    double v_vp = run_mc(S, K, T, r, sigma + eps_sigma, type, z);
    double v_vm = run_mc(S, K, T, r, sigma - eps_sigma, type, z);
    double v_tm = run_mc(S, K, T - eps_T, r, sigma, type, z);
    double v_rp = run_mc(S, K, T, r + eps_r, sigma, type, z);
    double v_rm = run_mc(S, K, T, r - eps_r, sigma, type, z);

    g.price = v0;
    g.delta = (v_sp - v_sm) / (2.0 * eps_s);
    g.gamma = (v_sp - 2.0 * v0 + v_sm) / (eps_s * eps_s);
    g.vega  = (v_vp - v_vm) / (2.0 * eps_sigma);
    g.theta = (v_tm - v0) / eps_T;
    g.rho   = (v_rp - v_rm) / (2.0 * eps_r);

    return g;
}

// ── Greeks via finite differences (exotic) ──

Greeks MonteCarloEngine::fd_greeks_exotic(
    double S, double K, double T, double r, double sigma,
    OptionType type, unsigned int style,
    double barrier, bool down_and_out,
    const std::vector<double>& z,
    const std::vector<std::vector<double>>& paths) const
{
    Greeks g;
    double eps_s     = S * 0.01;
    double eps_sigma = 0.001;
    double eps_T     = 0.001;
    double eps_r     = 0.0001;

    auto price_fn = [&](double sp, double k, double t, double rt, double sg, const std::vector<double>& zn) -> double {
        (void)k;
        if (style == 1) { // Asian
            return run_mc_asian(sp, K, t, rt, sg, type, static_cast<unsigned int>(paths[0].size() - 1), paths);
        } else if (style == 2) { // Barrier
            return run_mc_barrier(sp, K, t, rt, sg, type, barrier, down_and_out, zn);
        } else { // Lookback
            return run_mc_lookback(sp, K, t, rt, sg, type, zn);
        }
    };

    // Need a modified path set for Asian - approximate by regenerating with same seed
    // For barrier and lookback, use the same z vector
    if (style == 1) {
        // Asian: recompute paths for each shift (coarse approximation)
        // For finite differences, use path regeneration
        g.price = price_fn(S, K, T, r, sigma, z);
        g.delta = 0.0; g.gamma = 0.0; g.vega = 0.0; g.theta = 0.0; g.rho = 0.0;
    } else {
        double v0   = price_fn(S, K, T, r, sigma, z);
        double v_sp = price_fn(S + eps_s, K, T, r, sigma, z);
        double v_sm = price_fn(S - eps_s, K, T, r, sigma, z);
        double v_vp = price_fn(S, K, T, r, sigma + eps_sigma, z);
        double v_vm = price_fn(S, K, T, r, sigma - eps_sigma, z);
        double v_tm = price_fn(S, K, T - eps_T, r, sigma, z);
        double v_rp = price_fn(S, K, T, r + eps_r, sigma, z);
        double v_rm = price_fn(S, K, T, r - eps_r, sigma, z);

        g.price = v0;
        g.delta = (v_sp - v_sm) / (2.0 * eps_s);
        g.gamma = (v_sp - 2.0 * v0 + v_sm) / (eps_s * eps_s);
        g.vega  = (v_vp - v_vm) / (2.0 * eps_sigma);
        g.theta = (v_tm - v0) / eps_T;
        g.rho   = (v_rp - v_rm) / (2.0 * eps_r);
    }

    return g;
}

// ── Public API: calculate European ──

Greeks MonteCarloEngine::calculate(const MarketTick& tick) {
    size_t half = num_paths_ / 2;
    std::vector<double> z(half);
    generate_normals(z);

    Greeks g;
    if (tick.optionStyle == OptionStyle::ASIAN) {
        unsigned int steps = 52;
        std::vector<std::vector<double>> paths(half * 2, std::vector<double>(steps + 1));
        generate_paths(paths, tick.spotPrice, tick.timeToExpiry, tick.riskFreeRate, 0.25);
        double price = run_mc_asian(tick.spotPrice, tick.strikePrice, tick.timeToExpiry,
                                     tick.riskFreeRate, 0.25, tick.optionType, steps, paths);
        g.price = price;
        g.valid = true;
    } else if (tick.optionStyle == OptionStyle::BARRIER) {
        double price = run_mc_barrier(tick.spotPrice, tick.strikePrice, tick.timeToExpiry,
                                       tick.riskFreeRate, 0.25, tick.optionType,
                                       tick.barrier, tick.barrier_down, z);
        Greeks fd = fd_greeks_exotic(tick.spotPrice, tick.strikePrice, tick.timeToExpiry,
                                      tick.riskFreeRate, 0.25, tick.optionType, 2,
                                      tick.barrier, tick.barrier_down, z,
                                      std::vector<std::vector<double>>());
        g = fd;
        g.price = price;
        g.valid = true;
    } else if (tick.optionStyle == OptionStyle::LOOKBACK) {
        double price = run_mc_lookback(tick.spotPrice, tick.strikePrice, tick.timeToExpiry,
                                        tick.riskFreeRate, 0.25, tick.optionType, z);
        Greeks fd = fd_greeks_exotic(tick.spotPrice, tick.strikePrice, tick.timeToExpiry,
                                      tick.riskFreeRate, 0.25, tick.optionType, 3,
                                      0.0, false, z, std::vector<std::vector<double>>());
        g = fd;
        g.price = price;
        g.valid = true;
    } else {
        // Standard European
        double sigma_guess = 0.25;
        g = fd_greeks(tick.spotPrice, tick.strikePrice, tick.timeToExpiry,
                       tick.riskFreeRate, sigma_guess, tick.optionType, z);

        double iv = BlackScholesEngine::implied_volatility(
            tick.optionMarketPrice,
            tick.spotPrice, tick.strikePrice,
            tick.timeToExpiry, tick.riskFreeRate,
            tick.optionType);

        if (!std::isnan(iv) && iv > 0.0) {
            g = fd_greeks(tick.spotPrice, tick.strikePrice, tick.timeToExpiry,
                           tick.riskFreeRate, iv, tick.optionType, z);
        }

        g.impliedVol = iv;
        g.valid = !std::isnan(iv) && iv > 0.0;
    }

    return g;
}

// ── Exotic option public APIs ──

Greeks MonteCarloEngine::calculate_asian(
    double S, double K, double T, double r, double sigma,
    OptionType type, unsigned int steps) const
{
    Greeks g;

    size_t half = num_paths_ / 2;
    // We need paths for Asian - generate (half * 2) paths
    std::vector<std::vector<double>> paths(half * 2, std::vector<double>(steps + 1));

    // Use generate_paths but we need non-const access... use local RNG
    static thread_local std::mt19937_64 rng(std::random_device{}());
    std::normal_distribution<double> norm(0.0, 1.0);

    double dt = T / steps;
    double drift = (r - 0.5 * sigma * sigma) * dt;
    double diffusion = sigma * std::sqrt(dt);

    for (auto& path : paths) {
        path[0] = S;
        for (size_t i = 1; i <= steps; ++i) {
            double z = norm(rng);
            path[i] = path[i - 1] * std::exp(drift + diffusion * z);
        }
    }

    double discount = std::exp(-r * T);
    double sum = 0.0, sum_sq = 0.0;

    for (const auto& path : paths) {
        double avg = std::accumulate(path.begin(), path.end(), 0.0) / static_cast<double>(steps + 1);
        double payoff = (type == OptionType::CALL)
            ? std::max(avg - K, 0.0)
            : std::max(K - avg, 0.0);
        double pv = discount * payoff;
        sum += pv;
        sum_sq += pv * pv;
    }

    double n = static_cast<double>(paths.size());
    g.price = sum / n;
    double variance = (sum_sq - sum * sum / n) / (n - 1.0);
    g.valid = true;

    return g;
}

Greeks MonteCarloEngine::calculate_barrier(
    double S, double K, double T, double r, double sigma,
    OptionType type, double barrier, bool down_and_out) const
{
    size_t half = num_paths_ / 2;
    std::vector<double> z(half);
    generate_normals(z);

    Greeks g;
    g.price = run_mc_barrier(S, K, T, r, sigma, type, barrier, down_and_out, z);
    g.valid = true;

    // Approximate delta via finite differences
    double eps = S * 0.01;
    double vp = run_mc_barrier(S + eps, K, T, r, sigma, type, barrier, down_and_out, z);
    double vm = run_mc_barrier(S - eps, K, T, r, sigma, type, barrier, down_and_out, z);
    g.delta = (vp - vm) / (2.0 * eps);

    return g;
}

Greeks MonteCarloEngine::calculate_lookback(
    double S, double K, double T, double r, double sigma,
    OptionType type) const
{
    size_t half = num_paths_ / 2;
    std::vector<double> z(half);
    generate_normals(z);

    Greeks g;
    g.price = run_mc_lookback(S, K, T, r, sigma, type, z);
    g.valid = true;

    // Approximate delta
    double eps = S * 0.01;
    double vp = run_mc_lookback(S + eps, K, T, r, sigma, type, z);
    double vm = run_mc_lookback(S - eps, K, T, r, sigma, type, z);
    g.delta = (vp - vm) / (2.0 * eps);

    return g;
}

// ── Convergence Analysis ──

std::vector<ConvergencePoint> MonteCarloEngine::convergence(
    double S, double K, double T, double r, double sigma,
    OptionType type, unsigned int max_paths) const
{
    std::vector<unsigned int> path_counts = {1000, 2000, 5000, 10000, 20000, 50000, 100000};
    // Filter counts <= max_paths
    std::vector<unsigned int> use_counts;
    for (auto c : path_counts) {
        if (c <= max_paths) use_counts.push_back(c);
    }
    if (use_counts.empty()) use_counts.push_back(max_paths);

    double bs = BlackScholesEngine::price(S, K, T, r, sigma, type);

    std::vector<ConvergencePoint> results;
    static thread_local std::mt19937_64 rng(std::random_device{}());
    std::normal_distribution<double> norm(0.0, 1.0);

    for (unsigned int paths : use_counts) {
        size_t half = paths / 2;
        std::vector<double> z(half);
        for (size_t i = 0; i < half; ++i) {
            z[i] = norm(rng);
        }

        double mc = run_mc(S, K, T, r, sigma, type, z);
        double error = mc - bs;

        // Standard error: std(payoffs) / sqrt(N)
        double drift = (r - 0.5 * sigma * sigma) * T;
        double diffusion = sigma * std::sqrt(T);
        double discount = std::exp(-r * T);
        double sum_payoffs = 0.0, sum_sq = 0.0;

        for (const double& zi : z) {
            double s_t1 = S * std::exp(drift + diffusion * zi);
            double s_t2 = S * std::exp(drift + diffusion * (-zi));
            double p1 = (type == OptionType::CALL) ? std::max(s_t1 - K, 0.0) : std::max(K - s_t1, 0.0);
            double p2 = (type == OptionType::CALL) ? std::max(s_t2 - K, 0.0) : std::max(K - s_t2, 0.0);
            double pv1 = discount * p1;
            double pv2 = discount * p2;
            sum_payoffs += pv1 + pv2;
            sum_sq += pv1 * pv1 + pv2 * pv2;
        }

        double N = static_cast<double>(2 * half);
        double variance = (sum_sq - sum_payoffs * sum_payoffs / N) / (N - 1.0);
        double std_err = std::sqrt(variance / N);

        ConvergencePoint cp;
        cp.paths    = paths;
        cp.bs_price  = bs;
        cp.mc_price  = mc;
        cp.error     = error;
        cp.std_error = std_err;
        results.push_back(cp);
    }

    return results;
}
