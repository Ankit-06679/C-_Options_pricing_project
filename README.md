---
title: Options Pricing Platform
sdk: docker
pinned: false
---

# Options Pricing Platform — Complete Guide

## What Is This Project?

This is a **real-time web application** that calculates the price of **financial options** — contracts that give you the right to buy or sell a stock at a fixed price in the future. It uses two mathematical methods:

1. **Black-Scholes** (a precise formula for simple options)
2. **Monte Carlo simulation** (runs thousands of random scenarios for complex options)

The app fetches live stock prices (or generates realistic synthetic prices) and displays everything on an interactive dashboard in your browser.

---

## Objective

Build a working, real-time options pricing and risk-analysis tool accessible from any web browser, to demonstrate:

- How option prices are calculated using math and simulation
- How risk is measured (Delta, Gamma, Vega, etc.)
- How different trading strategies behave
- How Monte Carlo accuracy improves with more simulation paths
- All with live updating data and interactive charts

---

# How to Use the Platform
.

## The Toolbar (Top Bar)

| Button | What It Does |
|--------|-------------|
| **Pause / Resume** | Stops or restarts the live data feed |
| **Reset** | Deletes all positions and starts fresh |
| **Stocks** | Opens a window to choose which stocks to track |
| **Strategies** | Opens a window to build multi-leg option strategies |
| **CSV** | Downloads all your data as a spreadsheet file |
| **JSON** | Downloads all your data as a JSON file (for developers) |
| **PDF** | Generates a printable PDF report |

The badge on the right shows **Running** or **Paused**, and the number of positions and symbols tracked.

## Tab 1: Portfolio (Main Dashboard)

This is the default tab. It shows the current state of all your option positions.

### Greeks Cards (top-left box)
Shows the overall risk numbers for your entire portfolio:

- **Positions** — total number of option contracts
- **Last Spot** — most recent stock price received
- **Delta / Gamma / Vega / Theta / Rho** — risk measurements (explained in glossary)
- **IV (avg)** — average implied volatility across all positions
- **HV (annual)** — historical volatility (how much the stock actually moved)

> Numbers are color-coded: green = positive, red = negative, gray = neutral.

### P&L Summary (top-right box)
- **Total Cost** — how much you paid for all positions
- **Portfolio Value (BS)** — what your positions are worth now (Black-Scholes estimate)
- **Unrealized P&L** — profit or loss (green = profit, red = loss)
- **Ticker** — the last stock that was updated

Below this is a **P&L chart** that updates live — a line showing how your profit/loss changes over time.

### Risk Analytics (bottom-left)
- **Sharpe Ratio** — risk-adjusted return (higher is better)
- **VaR (95%)** — Value at Risk: the maximum you could lose 95% of the time
- **VaR (99%)** — Value at Risk at 99% confidence
- **Max Drawdown** — biggest drop from peak to trough

#### Stress Scenarios
- **Market Crash (-10%)** — what happens to your portfolio if all stocks drop 10%
- **Vol Spike (+20%)** — what happens if volatility jumps 20%
- **Rate Shock (+1%)** — what happens if interest rates rise 1%

### Vol Surface Heatmap (bottom-right)
A colored grid showing **implied volatility** across different strike prices and expiry dates for each stock. Darker colors = higher volatility. Helps you see which options are expensive or cheap.

### Greeks Surface Table (full-width table)
A sortable table showing each stock's total risk numbers. **Click any stock name** to expand its full option chain (all strikes, expiries, and their individual prices and Greeks).

### Positions Table (below surface table)
Shows the most recent 10 option positions with all details: ticker, spot price, strike price, expiry, type (CALL/PUT), Delta, Gamma, Vega, IV, and price. **Click column headers** to sort.

> The data refreshes automatically every 3–5 seconds.

## Tab 2: Convergence Analysis

This tab demonstrates that **Monte Carlo simulation gets more accurate as you run more paths**.

### How to use:
1. Enter: Spot price, Strike price, Expiry (years), Interest rate (%), Volatility (%), Option type (Call/Put)
2. Choose max paths (10K, 50K, 100K, 200K)
3. Click **Run**

### What you'll see:
- A **table** showing MC price vs BS exact price at 7 different path counts (1K to 100K)
- A **chart** showing MC prices converging toward the BS price line as paths increase
- The **error** column shows MC price minus BS price (should approach zero)
- The **std error** column shows the statistical uncertainty (should shrink)

## Tab 3: Exotic Options

Prices **path-dependent options** that can't be priced with a simple formula.

### How to use:
1. **Style** — choose Asian (average), Barrier, or Lookback
2. **Option Type** — Call or Put
3. **Ticker** — select a stock
4. **Spot, Strike, Expiry, Rate, Vol** — enter the option parameters
5. If **Barrier** is selected, also enter the Barrier price and direction (Down-and-Out / Up-and-Out)
6. Click **Price**

### What you'll see:
- The calculated price (MC simulation result)
- Delta risk measure
- All input parameters shown for reference

## Tab 4: Strategies

Build and visualize **multi-leg option strategies** — combinations of multiple options that create specific profit/loss patterns.

### How to use:
1. Click **Build Strategy**
2. Choose a **Strategy Type** from the dropdown
3. Enter the required parameters (they change based on strategy type)
4. Click **Build & View**

### Strategy Types Explained:

| Strategy | What It Is | When To Use |
|----------|-----------|-------------|
| **Covered Call** | Own stock + sell a call option | Earning extra income when you think the stock won't rise much |
| **Protective Put** | Own stock + buy a put option | Protecting against a price drop (like insurance) |
| **Straddle** | Buy both a call and a put at same strike | Expecting a big move but unsure which direction |
| **Strangle** | Buy a put and a call at different strikes | Same as straddle but cheaper, needs bigger move |
| **Bull Call Spread** | Buy a cheap call + sell an expensive call | Betting on a moderate price increase |
| **Bear Put Spread** | Buy an expensive put + sell a cheap put | Betting on a moderate price decrease |
| **Butterfly** | Complex 3-strike combination | Betting the price stays exactly where it is |
| **Iron Condor** | Complex 4-strike combination | Betting the price stays within a range |

### What you'll see:
- A **P&L diagram** (chart showing profit/loss at different stock prices at expiry)
- **Strategy info** — name, ticker, cost, risk numbers
- **Legs** — each individual option in the strategy

---

# Terminology Glossary

## Basic Stock Market Terms

| Term | Simple Explanation |
|------|--------------------|
| **Stock** | A tiny piece of ownership in a company. If you own 1 share of Apple, you own a tiny fraction of Apple. |
| **Ticker** | The short code for a stock (e.g., AAPL = Apple, TSLA = Tesla, MSFT = Microsoft) |
| **Spot Price** | The current market price of a stock |
| **Exchange** | The marketplace where stocks are traded (NASDAQ, NYSE, etc.) |

## Option Terms

| Term | Simple Explanation |
|------|--------------------|
| **Option** | A contract that gives you the right (but not obligation) to buy or sell a stock at a fixed price before a certain date |
| **Call Option** | The right to **buy** a stock at a fixed price. You profit if the stock price goes up. |
| **Put Option** | The right to **sell** a stock at a fixed price. You profit if the stock price goes down. |
| **Strike Price (K)** | The fixed price at which you can buy (call) or sell (put) the stock |
| **Expiry (T)** | The date when the option contract ends. After this, the option is worthless. |
| **Premium** | The price you pay to buy an option |
| **In-the-Money (ITM)** | An option that has value if exercised right now (call: stock price > strike, put: stock price < strike) |
| **At-the-Money (ATM)** | Stock price is roughly equal to the strike price |
| **Out-of-the-Money (OTM)** | An option with no value if exercised right now (call: stock price < strike, put: stock price > strike) |

## Exotic Option Terms

| Term | Simple Explanation |
|------|--------------------|
| **European Option** | A standard option — can only be used on the expiry date |
| **Asian Option** | Price depends on the **average** stock price over the option's life, not just the final price |
| **Barrier Option** | An option that **activates or disappears** if the stock price hits a certain level |
| **Down-and-Out** | A barrier option that **becomes worthless** if the stock price falls below the barrier |
| **Up-and-Out** | A barrier option that becomes worthless if the stock price rises above the barrier |
| **Lookback Option** | Price is based on the **best** price reached during the option's life (the highest for a call, lowest for a put) |

## Risk Measurement Terms (The Greeks)

| Term | Symbol | Simple Explanation |
|------|--------|--------------------|
| **Delta** | Δ | How much the option price changes if the stock moves $1. A Delta of 0.5 means the option price moves $0.50 for every $1 move in the stock. |
| **Gamma** | Γ | How much Delta itself changes when the stock moves. High Gamma means Delta changes quickly. |
| **Vega** | ν | How much the option price changes if volatility goes up by 1%. Higher Vega = more sensitive to volatility. |
| **Theta** | Θ | **Time decay** — how much value the option loses each day. Options lose value as expiry approaches. |
| **Rho** | ρ | How much the option price changes if interest rates change by 1% |
| **Implied Volatility (IV)** | IV | What the market thinks the future volatility will be. Higher IV = more expensive options. Think of it as "how uncertain is the market?" |
| **Historical Volatility (HV)** | — | How much the stock actually moved in the past (measured from price history) |

## Portfolio Analytics Terms

| Term | Simple Explanation |
|------|--------------------|
| **P&L** | Profit and Loss — how much money you've made or lost |
| **Sharpe Ratio** | A score that measures **return relative to risk**. Above 1 is good, above 2 is very good. Negative means you're losing money. |
| **VaR (Value at Risk)** | The **worst expected loss** at a given confidence level. VaR 95% = $100 means there's a 5% chance you'll lose more than $100. |
| **Max Drawdown** | The biggest drop from a peak to a low. A 40% max drawdown means at some point you were down 40% from the high. |
| **Stress Scenario** | A "what if" calculation. What happens to your portfolio in a market crash, a volatility spike, or a rate hike? |

## Pricing Method Terms

| Term | Simple Explanation |
|------|--------------------|
| **Black-Scholes** | A mathematical formula that gives the exact theoretical price of a European option. It won a Nobel Prize. |
| **Monte Carlo Simulation** | A computer runs **thousands of random scenarios** (simulated stock price paths) and averages the results to find the price. Named after the casino. |
| **Convergence** | The phenomenon where Monte Carlo gets closer to the exact Black-Scholes price as you run more random scenarios |
| **Standard Error** | A measure of how uncertain the Monte Carlo estimate is. Shrinks as you add more paths. |
| **Antithetic Variates** | A trick to make Monte Carlo more accurate: for each random number, also use its negative. Gives ~2x accuracy for the same number of paths. |

## Tech Terms

| Term | Simple Explanation |
|------|--------------------|
| **API** | A way for programs to talk to each other. The app has an API that the web page talks to. |
| **Endpoint** | A specific URL path that does one thing (e.g., `/api/dashboard` returns the portfolio data) |
| **SSE (Server-Sent Events)** | A technology where the server pushes updates to the browser automatically, like a live feed |
| **JSON** | A text format for data. Looks like: `{"price": 150.25, "delta": 0.65}` |
| **CSV** | A spreadsheet format you can open in Excel |
| **Docker** | A way to package the app so it runs the same way everywhere |
| **Port** | A numbered "channel" the app listens on. Like a TV channel number but for internet traffic. |

---

# Technical Details (For Developers)

## Architecture

```
Web Browser (Chart.js dashboard)
    ↕ HTTP / SSE
Boost.Beast HTTP Server
    ↕
Alpha Vantage Client ← → ThreadSafe Queue ← → Worker Threads → Portfolio Manager
    (fetches data)       (holds pending     (compute prices)   (stores results)
                          ticks)
```

- **Alpha Vantage Client** fetches stock prices from the Alpha Vantage API every 12 seconds, generates 30 option positions per stock, and puts them in a queue
- **Worker threads** pick up positions from the queue, compute prices using both Black-Scholes and Monte Carlo, and save the results
- **HTTP Server** serves the web page and API data from the Portfolio Manager

## CLI Arguments

```
options_pricer API_KEY [SYMBOLS...] [options]

Options:
  --mc-paths N      Number of Monte Carlo paths (default: 5000 on HF, 100000 local)
  --rate-limit S    Seconds between API calls (default: 12)
  --port P          HTTP server port (default: 8080)
  --calls-only      Only generate CALL options
  --puts-only       Only generate PUT options
  --help            Show help

Examples:
  options_pricer API_KEY AAPL MSFT TSLA
  options_pricer API_KEY AAPL --mc-paths 50000 --port 9090
```

## API Endpoints

| Method | Path | What it returns |
|--------|------|-----------------|
| GET | `/` | The HTML dashboard page |
| GET | `/api/dashboard` | All portfolio data as JSON |
| GET | `/api/status` | Running status (positions count, symbols, paused?) |
| GET | `/api/stocks` | List of all 82 available stocks |
| GET | `/api/countries` | List of 12 countries for filtering stocks |
| GET | `/health` | Health check with uptime |
| POST | `/api/configure` | Change tracked stocks (send JSON like `{"symbols":["AAPL","TSLA"]}`) |
| POST | `/api/pause` | Pause data ingestion |
| POST | `/api/resume` | Resume data ingestion |
| POST | `/api/reset` | Clear all positions |
| POST | `/api/convergence` | Run convergence analysis |
| POST | `/api/exotic` | Price exotic options |
| POST | `/api/strategy/build` | Build a multi-leg strategy |
| POST | `/api/strategies/clear` | Clear built strategies |
| POST | `/api/backtest` | Run MC vs BS comparison across volatilities |

## Live Deployed URL

**https://ankit3445-options-pricing.hf.space**

## GitHub Repository

**https://github.com/Ankit-06679/C-_Options_pricing_project**

---

## Source Files

| File | What It Contains |
|------|------------------|
| `main.cpp` | HTTP server, web page HTML/JS, REST API handlers, SSE streaming, threading setup |
| `Common.hpp` | Shared data types (OptionType, MarketTick, Greeks) |
| `Config.hpp` | CLI argument parsing |
| `PricingEngine.hpp/.cpp` | Black-Scholes formula, Monte Carlo simulation (European, Asian, Barrier, Lookback) |
| `PortfolioManager.hpp/.cpp` | Position storage, aggregated Greeks, P&L analytics, save/load, CSV/JSON export |
| `Strategy.hpp/.cpp` | 8 option strategies with P&L calculation |
| `AlphaVantageClient.hpp/.cpp` | HTTPS data ingestion from Alpha Vantage API, rate limiting, synthetic price fallback |
| `ThreadSafeQueue.hpp` | Thread-safe queue with shutdown support |
| `Dockerfile` | Build and deployment configuration for Docker / Hugging Face Spaces |
| `CMakeLists.txt` | Build system configuration |

---

## Technologies Used

- **C++20** — latest C++ standard
- **Boost.Beast + Boost.Asio** — HTTP server and networking
- **OpenSSL** — secure HTTPS connections to Alpha Vantage
- **nlohmann/json** — JSON parsing and generation
- **Chart.js** — browser-based interactive charts
- **Docker** — containerized deployment
- **CMake** — build system

