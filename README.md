# Options Pricing Platform

A **production-grade C++20 web application** for pricing financial options using **Black-Scholes** (analytical baseline for European options) and **Monte Carlo simulation** (exotic path-dependent options: Asian, Barrier, Lookback). Features a real-time web dashboard with live SSE streaming, Greeks tracking, volatility surface heatmaps, convergence analysis, and 8 built-in option strategies.

---

## Objective

Build a real-time, interactive options pricing and risk analytics platform accessible via web browser, demonstrating:

- **Black-Scholes** closed-form analytical pricing as the exact baseline for European options
- **Monte Carlo** simulation for exotic options where no closed-form solution exists
- **Convergence Analysis** — visual proof that MC prices approach BS prices as simulation paths increase
- **Greeks calculation** — risk sensitivities (Delta, Gamma, Vega, Theta, Rho) for position management
- **Option Strategies** — 8 multi-leg strategies with P&L at expiry diagrams
- **Live streaming** — Server-Sent Events push dashboard updates every 3 seconds
- **Real-world ingestion** — Alpha Vantage API integration with automatic synthetic pricing fallback

---

## Architecture

```
┌─────────────────────────────────────────────────────┐
│                   Web Browser                        │
│  Portfolio │ Convergence │ Exotic │ Strategies       │
│  (Chart.js dashboard with SSE live updates)          │
└─────────────────────┬───────────────────────────────┘
                      │ HTTP / SSE
┌─────────────────────▼───────────────────────────────┐
│           Boost.Beast HTTP Server (main.cpp)         │
│  REST API: /api/dashboard, /api/convergence,         │
│  /api/exotic, /api/chain, /api/strategies, ...       │
│  SSE: /api/stream (pushes JSON every 3s)             │
└──┬──────────────┬──────────────┬───────────────────┘
   │              │              │
   ▼              ▼              ▼
┌────────┐ ┌────────────┐ ┌──────────────┐
│Alpha   │ │Portfolio   │ │PricingEngine │
│Vantage │ │Manager     │ │              │
│Client  │ │            │ │• Black-      │
│        │ │• Positions │ │  Scholes     │
│• HTTPS │ │• Greeks    │ │• Monte Carlo │
│• Rate  │ │• Analytics │ │  (European)  │
│  Limit │ │• Save/Load │ │• MC (Asian)  │
│• Synth │ │• Export    │ │• MC (Barrier)│
│  Price │ │  (CSV/JSON)│ │• MC (Lookbk) │
└────────┘ └────────────┘ │• Convergence │
                          └──────────────┘
```

### File Structure

| File | Purpose |
|------|---------|
| `main.cpp` | Boost.Beast HTTP server, HTML dashboard, 17+ REST endpoints, SSE streaming |
| `Common.hpp` | `OptionType`, `OptionStyle` enums, `MarketTick`, `Greeks` structs |
| `PricingEngine.hpp/.cpp` | Black-Scholes engine + Monte Carlo engine (European, Asian, Barrier, Lookback) |
| `AlphaVantageClient.hpp/.cpp` | HTTPS data ingestion with rate limiting and synthetic pricing fallback |
| `PortfolioManager.hpp/.cpp` | Position tracking, BS Greeks, P&L analytics, save/load, CSV/JSON export |
| `Strategy.hpp/.cpp` | 8 option strategies with P&L at expiry and combined Greeks |
| `Config.hpp` | CLI argument parsing (`--mc-paths`, `--port`, `--calls-only`, etc.) |
| `ThreadSafeQueue.hpp` | Thread-safe producer-consumer queue with shutdown support |
| `Dockerfile` | Multi-stage build (gcc:14.2 → debian:bookworm-slim), port 8080 |

---

## Black-Scholes Model

The **Black-Scholes model** provides a closed-form analytical solution for pricing **European options** (exercisable only at expiry).

### Assumptions
- Underlying price follows **geometric Brownian motion** with constant drift and volatility
- **Lognormal** distribution of returns
- Constant risk-free interest rate
- No dividends
- No transaction costs or taxes
- Continuous trading
- European exercise (no early exercise)

### Formulas

**Call option price:**
```
C = S₀ · N(d₁) - K · e^(-rT) · N(d₂)
```

**Put option price:**
```
P = K · e^(-rT) · N(-d₂) - S₀ · N(-d₁)
```

Where:
```
d₁ = [ln(S₀/K) + (r + σ²/2)T] / (σ√T)
d₂ = d₁ - σ√T
```

| Variable | Meaning |
|----------|---------|
| `S₀` | Current spot price of underlying asset |
| `K` | Strike price of the option |
| `T` | Time to expiry (in years) |
| `r` | Risk-free interest rate |
| `σ` | Volatility of underlying returns |
| `N(·)` | Cumulative distribution function of standard normal |

### Implementation

In `PricingEngine::BlackScholesEngine`:
- `price()` — computes exact BS price using `std::erfc` for the normal CDF
- `greeks()` — computes analytical partial derivatives (delta, gamma, vega, theta, rho)
- `implied_volatility()` — inverts BS price using Newton-Raphson with bisection fallback

---

## Monte Carlo Simulation

The **Monte Carlo engine** simulates thousands of random price paths and computes option payoffs, averaging them to estimate the fair price. It handles both European and exotic options.

### Variance Reduction: Antithetic Variates

For each random normal `z`, we also use `-z` to generate a paired path. This creates negative correlation between pairs, reducing estimator variance by ~50% for the same number of paths.

### European Options (Monte Carlo)

```
S_T = S₀ · exp((r - σ²/2) · T + σ · √T · z)
Payoff(CALL) = max(S_T - K, 0)
Payoff(PUT)  = max(K - S_T, 0)
Price = e^(-rT) · average(payoff)
```

### Asian Options

Payoff is based on the **arithmetic average** of prices along the path, not just the final price. Useful for commodities and averaging contracts.

```
For each path (52 steps):
    avg = (S₀ + S₁ + ... + S₅₂) / 53
    Payoff(CALL) = max(avg - K, 0)
    Payoff(PUT)  = max(K - avg, 0)
```

### Barrier Options

Payoff depends on whether the underlying price **touches a barrier level** during the option's life.

- **Down-and-Out**: Option is **knocked out** (becomes worthless) if price falls below barrier
- **Up-and-Out**: Option is knocked out if price rises above barrier

```
For each path (52 steps):
    If any step price crosses the barrier → payoff = 0
    Else → payoff = max(S_T - K, 0) for call
```

### Lookback Options

Payoff is based on the **maximum or minimum** price reached during the option's life.

- **Floating Strike Lookback Call**: `payoff = max(S_max - K, 0)`
- **Floating Strike Lookback Put**: `payoff = max(K - S_min, 0)`

### Convergence Analysis

The `/api/convergence` endpoint runs MC at multiple path counts `[1K, 2K, 5K, 10K, 20K, 50K, 100K]` and compares each result to the exact BS price, demonstrating that **MC price → BS price** as `paths → ∞`, with standard error decreasing as `1/√N`.

```
error = mc_price - bs_price
std_error = σ(payoffs) / √N
```

---

## The Greeks

The **Greeks** measure the sensitivity of an option's price to various market parameters. All Greeks are stored per-position as BS analytical values (the baseline).

| Greek | Symbol | Definition | Interpretation |
|-------|--------|------------|----------------|
| **Delta** | Δ | ∂V/∂S | Price change per $1 move in underlying |
| **Gamma** | Γ | ∂²V/∂S² | Delta change per $1 move in underlying (convexity) |
| **Vega** | ν | ∂V/∂σ | Price change per 1% change in volatility |
| **Theta** | Θ | ∂V/∂T | Time decay — price change per day closer to expiry |
| **Rho** | ρ | ∂V/∂r | Price change per 1% change in interest rate |
| **Implied Vol** | IV | σ such that BS price = market price | Market's expectation of future volatility |

### Portfolio-Level Analytics

| Metric | Formula | Purpose |
|--------|---------|---------|
| **Sharpe Ratio** | (R̄ - Rf) / σ(R) | Risk-adjusted return (annualized) |
| **VaR (95%/99%)** | Percentile of P&L distribution | Maximum loss at given confidence |
| **Max Drawdown** | Peak-to-trough decline | Worst historical loss |
| **Stress Crash** | P&L if spot drops 10% | Market crash scenario |
| **Vol Spike** | P&L if IV rises 20% | Volatility shock scenario |
| **Rate Shock** | P&L if rates rise 1% | Interest rate scenario |

---

## Option Strategies

8 built-in multi-leg option strategies, each with P&L at expiry diagrams and combined Greeks.

| Strategy | Composition | Outlook |
|----------|-------------|---------|
| **Covered Call** | Long stock + Short call | Neutral to slightly bullish (income) |
| **Protective Put** | Long stock + Long put | Bullish with downside protection |
| **Straddle** | Long call + Long put (same strike) | High volatility / big move expected |
| **Strangle** | Long call + Long put (different strikes) | High volatility / wider range |
| **Bull Call Spread** | Long low-strike call + Short high-strike call | Moderately bullish (limited risk/reward) |
| **Bear Put Spread** | Long high-strike put + Short low-strike put | Moderately bearish (limited risk/reward) |
| **Butterfly** | Combination at 3 strikes (OTM call + 2×ATM call + ITM call) | Low volatility / range-bound |
| **Iron Condor** | OTM put spread + OTM call spread | Low volatility / range-bound (credit) |

---

## API Endpoints

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/dashboard` | Full portfolio dashboard JSON |
| GET | `/api/status` | Server status (symbols, positions, paused state) |
| GET | `/api/stocks` | Global stock database (82 stocks, 12 countries) |
| GET | `/api/countries` | List of available countries for filtering |
| GET | `/api/stream` | SSE — pushes dashboard JSON every 3 seconds |
| GET | `/api/chain?ticker=X` | Option chain for a given ticker |
| GET | `/api/pl-diagram` | Portfolio-wide P&L at expiry curve |
| GET | `/api/export/csv` | Download portfolio as CSV |
| GET | `/api/export/json` | Download portfolio as JSON |
| POST | `/api/configure` | Update symbols, strikes, expiries, calls/puts |
| POST | `/api/pause` | Pause data ingestion |
| POST | `/api/resume` | Resume data ingestion |
| POST | `/api/reset` | Clear all positions |
| POST | `/api/convergence` | Run MC convergence analysis (returns chart data) |
| POST | `/api/exotic` | Price exotic options (Asian/Barrier/Lookback) |
| POST | `/api/strategy/build` | Build a multi-leg strategy |
| GET | `/api/strategies/current` | Get built strategies |
| POST | `/api/strategies/clear` | Clear all strategies |
| POST | `/api/backtest` | Run MC vs BS backtest across volatility range |

---

## Terminology Glossary

| Term | Definition |
|------|------------|
| **Option** | Financial contract giving the holder the right (not obligation) to buy/sell an asset at a specified price |
| **Call Option** | Right to **buy** the underlying asset at the strike price |
| **Put Option** | Right to **sell** the underlying asset at the strike price |
| **European Option** | Exercisable only at expiry date |
| **Asian Option** | Payoff determined by average price over the option's life |
| **Barrier Option** | Activated/terminated when spot price hits a predetermined level |
| **Lookback Option** | Payoff based on maximum or minimum price during the option's life |
| **Strike Price (K)** | Pre-determined price at which the option can be exercised |
| **Spot Price (S₀)** | Current market price of the underlying asset |
| **Time to Expiry (T)** | Time remaining until option expiration (in years) |
| **Risk-Free Rate (r)** | Theoretical return on a risk-free investment (government bond yield) |
| **Volatility (σ)** | Standard deviation of the underlying asset's returns (annualized) |
| **Implied Volatility (IV)** | Volatility implied by the market price via inverse Black-Scholes |
| **Historical Volatility (HV)** | Actual realized volatility of the underlying over a past period |
| **In-the-Money (ITM)** | Option with intrinsic value (call: S > K, put: S < K) |
| **At-the-Money (ATM)** | Option where S ≈ K |
| **Out-of-the-Money (OTM)** | Option with no intrinsic value (call: S < K, put: S > K) |
| **Premium** | Price paid to purchase an option |
| **P&L** | Profit and Loss on a position |
| **SSE (Server-Sent Events)** | Unidirectional real-time data push over HTTP |

---

## Quick Start

### Prerequisites
- C++20 compiler (GCC 13+/Clang 16+)
- Boost 1.83+ (system, asio, beast)
- OpenSSL 3+
- CMake 3.20+
- nlohmann/json (fetched automatically by CMake)

### Build
```bash
git clone https://github.com/Ankit-06679/C-_Options_pricing_project
cd C-_Options_pricing_project
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel $(nproc)
```

### Run
```bash
# Basic usage
./options_pricer API_KEY AAPL MSFT TSLA

# With options
./options_pricer API_KEY AAPL --mc-paths 50000 --port 9090 --calls-only

# Help
./options_pricer --help
```

### Docker
```bash
docker build -t options-pricer .
docker run -d -p 8080:8080 options-pricer API_KEY AAPL TSLA
```

Then open **http://localhost:8080** in your browser.

---

## Dashboard Tabs

| Tab | Description |
|-----|-------------|
| **Portfolio** | BS Greeks grid, P&L history chart, risk analytics, volatility surface heatmap, expandable option chain |
| **Convergence** | MC→BS convergence scatter plot + error table across path counts 1K–100K |
| **Exotic** | Price Asian, Barrier, and Lookback options with MC simulation |
| **Strategies** | Build 8 multi-leg strategies with interactive P&L diagram |

---

## Technologies

- **C++20** — `std::jthread`, smart pointers, structured bindings, `constexpr`
- **Boost.Beast** — HTTP/1.1 server, WebSocket, SSL/TLS
- **Boost.Asio** — networking, timers, `io_context`
- **nlohmann/json** — JSON serialization/deserialization
- **OpenSSL** — HTTPS client connections
- **Chart.js** — Client-side charts (realtime P&L, convergence scatter, P&L diagrams)
- **Server-Sent Events** — Real-time dashboard updates
- **Docker** — Multi-stage build for deployment
- **CMake** — Build system with `FetchContent`
