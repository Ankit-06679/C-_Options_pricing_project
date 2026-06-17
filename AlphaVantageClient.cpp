#include "AlphaVantageClient.hpp"

#include <iostream>
#include <sstream>
#include <cmath>
#include <thread>
#include <chrono>
#include <random>
#include <algorithm>

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <openssl/ssl.h>
#include <nlohmann/json.hpp>

namespace beast = boost::beast;
namespace http  = beast::http;
namespace net   = boost::asio;
namespace ssl   = net::ssl;
using tcp       = net::ip::tcp;

AlphaVantageClient::AlphaVantageClient(
    const std::vector<std::string>& symbols,
    const std::string& api_key,
    const AppConfig& config,
    std::shared_ptr<ThreadSafeQueue<MarketTick>> queue)
    : symbols_(symbols)
    , api_key_(api_key)
    , config_(config)
    , queue_(std::move(queue))
{
}

AlphaVantageClient::~AlphaVantageClient() {
    stop();
}

void AlphaVantageClient::start() {
    if (running_.exchange(true)) return;
    thread_ = std::make_unique<std::thread>([this] { polling_loop(); });
}

void AlphaVantageClient::stop() {
    running_.store(false);
    if (thread_ && thread_->joinable()) {
        thread_->join();
    }
}

void AlphaVantageClient::pause() {
    paused_.store(true);
    std::cout << "[AlphaVantage] Paused" << std::endl;
}

void AlphaVantageClient::resume() {
    paused_.store(false);
    std::cout << "[AlphaVantage] Resumed" << std::endl;
}

bool AlphaVantageClient::is_paused() const {
    return paused_.load();
}

void AlphaVantageClient::request_reset() {
    reset_requested_.store(true);
    paused_.store(false);
    std::cout << "[AlphaVantage] Reset requested" << std::endl;
}

void AlphaVantageClient::set_symbols(const std::vector<std::string>& symbols) {
    std::lock_guard<std::mutex> lock(symbols_mutex_);
    symbols_ = symbols;
    symbol_index_.store(0);
    std::cout << "[AlphaVantage] Symbols updated:";
    for (const auto& s : symbols_) std::cout << " " << s;
    std::cout << std::endl;
}

std::vector<std::string> AlphaVantageClient::get_symbols() const {
    std::lock_guard<std::mutex> lock(symbols_mutex_);
    return symbols_;
}

void AlphaVantageClient::polling_loop() {
    while (running_.load()) {
        if (paused_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            continue;
        }

        if (reset_requested_.exchange(false)) {
            queue_->clear();
            symbol_index_.store(0);
            std::cout << "[AlphaVantage] Queue cleared after reset" << std::endl;
        }

        std::string symbol;
        {
            std::lock_guard<std::mutex> lock(symbols_mutex_);
            if (symbols_.empty()) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }
            symbol = symbols_[symbol_index_.fetch_add(1) % symbols_.size()];
        }

        try {
            MarketTick base = fetch_quote(symbol);
            std::vector<MarketTick> chain = expand_option_chain(base);

            for (auto& tick : chain) {
                queue_->push(std::move(tick));
            }

            std::cout << "[AlphaVantage] " << symbol
                      << " spot=" << base.spotPrice
                      << " -> " << chain.size() << " options queued"
                      << std::endl;
        } catch (const std::exception& ex) {
            std::cerr << "[AlphaVantage] " << symbol << " error: " << ex.what() << std::endl;
        }

        for (int i = 0; i < static_cast<int>(config_.rate_limit_sec) && running_.load(); ++i) {
            if (paused_.load() || reset_requested_.load()) break;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

std::vector<MarketTick> AlphaVantageClient::expand_option_chain(const MarketTick& base) {
    std::vector<MarketTick> chain;
    chain.reserve(config_.strike_pct.size() * config_.expiries.size() * ((config_.use_calls ? 1 : 0) + (config_.use_puts ? 1 : 0)));

    for (double strike_pct : config_.strike_pct) {
        double strike = std::round(base.spotPrice * strike_pct);

        for (double expiry : config_.expiries) {
            if (config_.use_calls) {
                MarketTick tick = base;
                tick.strikePrice   = strike;
                tick.timeToExpiry  = expiry;
                tick.optionType    = OptionType::CALL;
                tick.optionMarketPrice = compute_option_market_price(
                    base.spotPrice, strike, expiry, base.riskFreeRate, OptionType::CALL);
                chain.push_back(std::move(tick));
            }

            if (config_.use_puts) {
                MarketTick tick = base;
                tick.strikePrice   = strike;
                tick.timeToExpiry  = expiry;
                tick.optionType    = OptionType::PUT;
                tick.optionMarketPrice = compute_option_market_price(
                    base.spotPrice, strike, expiry, base.riskFreeRate, OptionType::PUT);
                chain.push_back(std::move(tick));
            }
        }
    }

    return chain;
}

MarketTick AlphaVantageClient::fetch_quote(const std::string& symbol) {
    std::string host = "www.alphavantage.co";
    std::string port = "443";
    std::string target = "/query?function=GLOBAL_QUOTE&symbol=" + symbol + "&apikey=" + api_key_;

    net::io_context ioc;
    ssl::context ctx(ssl::context::tls_client);
    ctx.set_default_verify_paths();
    ctx.set_verify_mode(ssl::verify_none);
    ctx.set_options(
        ssl::context::default_workarounds |
        ssl::context::no_sslv2 |
        ssl::context::no_sslv3 |
        ssl::context::single_dh_use
    );

    beast::ssl_stream<beast::tcp_stream> stream(ioc, ctx);
    beast::get_lowest_layer(stream).expires_after(std::chrono::seconds(30));

    tcp::resolver resolver(ioc);
    auto const results = resolver.resolve(host, port);
    beast::get_lowest_layer(stream).connect(results);

    if (!SSL_set_tlsext_host_name(stream.native_handle(), host.c_str())) {
        throw std::runtime_error("SSL SNI setup failed");
    }

    stream.handshake(ssl::stream_base::client);

    http::request<http::string_body> req{http::verb::get, target, 11};
    req.set(http::field::host, host);
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    http::write(stream, req);

    beast::flat_buffer buffer;
    http::response<http::dynamic_body> res;
    http::read(stream, buffer, res);

    beast::error_code ec;
    stream.shutdown(ec);

    auto body = beast::buffers_to_string(res.body().data());
    auto json = nlohmann::json::parse(body);

    std::string price_str = "0.0";
    if (json.contains("Global Quote") && json["Global Quote"].contains("05. price")) {
        price_str = json["Global Quote"]["05. price"].get<std::string>();
    } else if (json.contains("Note")) {
        std::cerr << "[AlphaVantage] Rate-limit note: " << json["Note"] << std::endl;
    } else if (json.contains("Information")) {
        std::cerr << "[AlphaVantage] Info: " << json["Information"] << std::endl;
    }

    double spot = 0.0;
    try {
        spot = std::stod(price_str);
    } catch (...) {
        spot = 0.0;
    }
    if (spot <= 0.0) {
        spot = synthetic_spot(symbol);
        static thread_local std::mt19937_64 noise_rng(std::random_device{}());
        std::normal_distribution<double> noise(0.0, spot * 0.01);
        spot += noise(noise_rng);
        spot = std::max(spot, 1.0);
        std::cout << "[AlphaVantage] " << symbol << " using synthetic spot $" << spot << std::endl;
    }

    auto now = std::chrono::system_clock::now();
    auto tt  = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&tt), "%Y-%m-%d %H:%M:%S");
    std::string timestamp = ss.str();

    MarketTick tick;
    tick.ticker           = symbol;
    tick.timestamp        = timestamp;
    tick.spotPrice        = spot;
    tick.strikePrice      = std::round(spot);
    tick.timeToExpiry     = 0.25;
    tick.riskFreeRate     = 0.04;
    tick.optionType       = OptionType::CALL;
    tick.optionMarketPrice = compute_option_market_price(spot, tick.strikePrice, tick.timeToExpiry, tick.riskFreeRate, OptionType::CALL);

    return tick;
}

double AlphaVantageClient::synthetic_spot(const std::string& symbol) {
    std::hash<std::string> hasher;
    size_t h = hasher(symbol);
    std::seed_seq seed{static_cast<int>(h & 0xffffffff), static_cast<int>((h >> 32) & 0xffffffff)};
    std::mt19937_64 local_rng(seed);
    std::uniform_real_distribution<double> dist(20.0, 500.0);
    return std::round(dist(local_rng) * 100.0) / 100.0;
}

double AlphaVantageClient::compute_option_market_price(
    double spot, double strike, double T, double r, OptionType type) const
{
    double sigma = 0.25;
    double d1 = (std::log(spot / strike) + (r + sigma * sigma * 0.5) * T)
              / (sigma * std::sqrt(T));
    double d2 = d1 - sigma * std::sqrt(T);
    double nd1  = 0.5 * (1.0 + std::erf( d1 / std::sqrt(2.0)));
    double nd2  = 0.5 * (1.0 + std::erf( d2 / std::sqrt(2.0)));
    double n_nd1 = 0.5 * (1.0 + std::erf(-d1 / std::sqrt(2.0)));
    double n_nd2 = 0.5 * (1.0 + std::erf(-d2 / std::sqrt(2.0)));

    double price;
    if (type == OptionType::CALL) {
        price = spot * nd1 - strike * std::exp(-r * T) * nd2;
    } else {
        price = strike * std::exp(-r * T) * n_nd2 - spot * n_nd1;
    }

    static thread_local std::mt19937_64 rng(std::random_device{}());
    static thread_local std::normal_distribution<double> noise(0.0, 1.0);
    price *= (1.0 + noise(rng) * 0.03);
    price = std::max(price, 0.01);

    return price;
}
