#pragma once

#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <thread>
#include <random>
#include <mutex>
#include <unordered_map>

#include "Common.hpp"
#include "ThreadSafeQueue.hpp"
#include "Config.hpp"

class AlphaVantageClient {
public:
    AlphaVantageClient(const std::vector<std::string>& symbols,
                       const std::string& api_key,
                       const AppConfig& config,
                       std::shared_ptr<ThreadSafeQueue<MarketTick>> queue);
    ~AlphaVantageClient();

    void start();
    void stop();
    void pause();
    void resume();
    bool is_paused() const;
    void request_reset();
    void set_symbols(const std::vector<std::string>& symbols);
    std::vector<std::string> get_symbols() const;

private:
    void polling_loop();
    MarketTick fetch_quote(const std::string& symbol);
    std::vector<MarketTick> expand_option_chain(const MarketTick& base);
    double compute_option_market_price(double spot, double strike, double T, double r, OptionType type) const;
    double synthetic_spot(const std::string& symbol);

    std::vector<std::string> symbols_;
    std::string api_key_;
    AppConfig config_;
    std::shared_ptr<ThreadSafeQueue<MarketTick>> queue_;
    std::atomic<bool> running_{false};
    std::unique_ptr<std::thread> thread_;
    std::atomic<size_t> symbol_index_{0};
    std::atomic<bool> paused_{false};
    std::atomic<bool> reset_requested_{false};
    mutable std::mutex symbols_mutex_;
};
