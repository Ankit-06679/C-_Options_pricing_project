#pragma once

#include "Common.hpp"
#include <vector>

struct ConvergencePoint {
    unsigned int paths;
    double       bs_price;
    double       mc_price;
    double       error;
    double       std_error;
};

class PricingEngine {
public:
    virtual ~PricingEngine() = default;
    virtual Greeks calculate(const MarketTick& tick) = 0;
};

class BlackScholesEngine : public PricingEngine {
public:
    Greeks calculate(const MarketTick& tick) override;

    static double price(double S, double K, double T, double r, double sigma, OptionType type);
    static Greeks greeks(double S, double K, double T, double r, double sigma, OptionType type);
    static double implied_volatility(double market_price, double S, double K,
                                     double T, double r, OptionType type);

private:
    static double cum_norm(double x);
    static double norm_pdf(double x);
    static double bs_price(double S, double K, double T, double r, double sigma, OptionType type);
    static Greeks  bs_greeks(double S, double K, double T, double r, double sigma, OptionType type);
    static double implied_vol_newton(double target, double S, double K, double T, double r, OptionType type);
    static double implied_vol_bisection(double target, double S, double K, double T, double r, OptionType type);
};

class MonteCarloEngine : public PricingEngine {
public:
    explicit MonteCarloEngine(unsigned int num_paths = 100000);
    Greeks calculate(const MarketTick& tick) override;

    // Exotic option pricing
    Greeks calculate_asian(double S, double K, double T, double r, double sigma, OptionType type, unsigned int steps = 52) const;
    Greeks calculate_barrier(double S, double K, double T, double r, double sigma, OptionType type, double barrier, bool down_and_out) const;
    Greeks calculate_lookback(double S, double K, double T, double r, double sigma, OptionType type) const;

    // Convergence analysis: run MC at multiple path counts
    std::vector<ConvergencePoint> convergence(double S, double K, double T, double r, double sigma,
                                               OptionType type, unsigned int max_paths = 100000) const;

private:
    unsigned int num_paths_;

    // Standard European MC
    double run_mc(double S, double K, double T, double r, double sigma,
                  OptionType type, const std::vector<double>& z) const;

    // Asian MC: payoff = max(avg(S) - K, 0) for call
    double run_mc_asian(double S, double K, double T, double r, double sigma,
                        OptionType type, unsigned int steps,
                        const std::vector<std::vector<double>>& paths) const;

    // Barrier MC: payoff if barrier not touched
    double run_mc_barrier(double S, double K, double T, double r, double sigma,
                          OptionType type, double barrier, bool down_and_out,
                          const std::vector<double>& z) const;

    // Lookback MC: payoff = max(max(S) - K, 0) for call
    double run_mc_lookback(double S, double K, double T, double r, double sigma,
                           OptionType type, const std::vector<double>& z) const;

    Greeks fd_greeks(double S, double K, double T, double r, double sigma,
                     OptionType type, const std::vector<double>& z) const;

    Greeks fd_greeks_exotic(double S, double K, double T, double r, double sigma,
                             OptionType type, unsigned int style,
                             double barrier, bool down_and_out,
                             const std::vector<double>& z,
                             const std::vector<std::vector<double>>& paths) const;

    // Helper: generate standard normal random numbers
    void generate_normals(std::vector<double>& z) const;
    // Helper: generate correlated normals for multi-step paths
    void generate_paths(std::vector<std::vector<double>>& paths, double S, double T, double r, double sigma) const;
};
