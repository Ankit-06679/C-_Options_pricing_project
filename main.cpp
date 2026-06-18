#include <iostream>
#include <memory>
#include <vector>
#include <thread>
#include <atomic>
#include <csignal>
#include <cstdlib>
#include <algorithm>
#include <chrono>
#include <sstream>
#include <optional>

#include "Common.hpp"
#include "ThreadSafeQueue.hpp"
#include "AlphaVantageClient.hpp"
#include "PricingEngine.hpp"
#include "PortfolioManager.hpp"
#include "Strategy.hpp"
#include "Config.hpp"

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/strand.hpp>

#include <nlohmann/json.hpp>

namespace beast  = boost::beast;
namespace http   = beast::http;
namespace net    = boost::asio;
using tcp        = net::ip::tcp;

static std::atomic<bool> shutdown_requested{false};

extern "C" void signal_handler(int) {
    shutdown_requested.store(true);
}

// ── Global Stock Database by Country ──
static const nlohmann::json STOCK_DB = nlohmann::json::array({
    {{"symbol","AAPL"},{"name","Apple Inc."},{"country","United States"},{"exchange","NASDAQ"}},
    {{"symbol","MSFT"},{"name","Microsoft Corp."},{"country","United States"},{"exchange","NASDAQ"}},
    {{"symbol","GOOGL"},{"name","Alphabet Inc."},{"country","United States"},{"exchange","NASDAQ"}},
    {{"symbol","AMZN"},{"name","Amazon.com Inc."},{"country","United States"},{"exchange","NASDAQ"}},
    {{"symbol","TSLA"},{"name","Tesla Inc."},{"country","United States"},{"exchange","NASDAQ"}},
    {{"symbol","NVDA"},{"name","NVIDIA Corp."},{"country","United States"},{"exchange","NASDAQ"}},
    {{"symbol","META"},{"name","Meta Platforms Inc."},{"country","United States"},{"exchange","NASDAQ"}},
    {{"symbol","JPM"},{"name","JPMorgan Chase & Co."},{"country","United States"},{"exchange","NYSE"}},
    {{"symbol","BAC"},{"name","Bank of America Corp."},{"country","United States"},{"exchange","NYSE"}},
    {{"symbol","JNJ"},{"name","Johnson & Johnson"},{"country","United States"},{"exchange","NYSE"}},
    {{"symbol","V"},{"name","Visa Inc."},{"country","United States"},{"exchange","NYSE"}},
    {{"symbol","WMT"},{"name","Walmart Inc."},{"country","United States"},{"exchange","NYSE"}},
    {{"symbol","PG"},{"name","Procter & Gamble Co."},{"country","United States"},{"exchange","NYSE"}},
    {{"symbol","XOM"},{"name","Exxon Mobil Corp."},{"country","United States"},{"exchange","NYSE"}},
    {{"symbol","UNH"},{"name","UnitedHealth Group Inc."},{"country","United States"},{"exchange","NYSE"}},
    {{"symbol","HD"},{"name","Home Depot Inc."},{"country","United States"},{"exchange","NYSE"}},
    {{"symbol","ORCL"},{"name","Oracle Corp."},{"country","United States"},{"exchange","NYSE"}},
    {{"symbol","KO"},{"name","Coca-Cola Co."},{"country","United States"},{"exchange","NYSE"}},
    {{"symbol","DIS"},{"name","Walt Disney Co."},{"country","United States"},{"exchange","NYSE"}},
    {{"symbol","CSCO"},{"name","Cisco Systems Inc."},{"country","United States"},{"exchange","NASDAQ"}},
    {{"symbol","RELIANCE"},{"name","Reliance Industries Ltd."},{"country","India"},{"exchange","NSE"}},
    {{"symbol","TCS"},{"name","Tata Consultancy Services Ltd."},{"country","India"},{"exchange","NSE"}},
    {{"symbol","HDFCBANK"},{"name","HDFC Bank Ltd."},{"country","India"},{"exchange","NSE"}},
    {{"symbol","INFY"},{"name","Infosys Ltd."},{"country","India"},{"exchange","NSE"}},
    {{"symbol","ICICIBANK"},{"name","ICICI Bank Ltd."},{"country","India"},{"exchange","NSE"}},
    {{"symbol","SBIN"},{"name","State Bank of India"},{"country","India"},{"exchange","NSE"}},
    {{"symbol","BHARTIARTL"},{"name","Bharti Airtel Ltd."},{"country","India"},{"exchange","NSE"}},
    {{"symbol","ITC"},{"name","ITC Ltd."},{"country","India"},{"exchange","NSE"}},
    {{"symbol","WIPRO"},{"name","Wipro Ltd."},{"country","India"},{"exchange","NSE"}},
    {{"symbol","HCLTECH"},{"name","HCL Technologies Ltd."},{"country","India"},{"exchange","NSE"}},
    {{"symbol","HSBA"},{"name","HSBC Holdings PLC"},{"country","United Kingdom"},{"exchange","LSE"}},
    {{"symbol","BP"},{"name","BP PLC"},{"country","United Kingdom"},{"exchange","LSE"}},
    {{"symbol","GSK"},{"name","GSK PLC"},{"country","United Kingdom"},{"exchange","LSE"}},
    {{"symbol","SHEL"},{"name","Shell PLC"},{"country","United Kingdom"},{"exchange","LSE"}},
    {{"symbol","AZN"},{"name","AstraZeneca PLC"},{"country","United Kingdom"},{"exchange","LSE"}},
    {{"symbol","LLOY"},{"name","Lloyds Banking Group PLC"},{"country","United Kingdom"},{"exchange","LSE"}},
    {{"symbol","BARC"},{"name","Barclays PLC"},{"country","United Kingdom"},{"exchange","LSE"}},
    {{"symbol","ULVR"},{"name","Unilever PLC"},{"country","United Kingdom"},{"exchange","LSE"}},
    {{"symbol","RIO"},{"name","Rio Tinto Group"},{"country","United Kingdom"},{"exchange","LSE"}},
    {{"symbol","VOD"},{"name","Vodafone Group PLC"},{"country","United Kingdom"},{"exchange","LSE"}},
    {{"symbol","TM"},{"name","Toyota Motor Corp."},{"country","Japan"},{"exchange","TSE"}},
    {{"symbol","SONY"},{"name","Sony Group Corp."},{"country","Japan"},{"exchange","TSE"}},
    {{"symbol","MUFG"},{"name","Mitsubishi UFJ Financial Group"},{"country","Japan"},{"exchange","TSE"}},
    {{"symbol","HMC"},{"name","Honda Motor Co."},{"country","Japan"},{"exchange","TSE"}},
    {{"symbol","SMFG"},{"name","Sumitomo Mitsui Financial Group"},{"country","Japan"},{"exchange","TSE"}},
    {{"symbol","NTT"},{"name","Nippon Telegraph & Telephone Corp."},{"country","Japan"},{"exchange","TSE"}},
    {{"symbol","SAP"},{"name","SAP SE"},{"country","Germany"},{"exchange","FRA"}},
    {{"symbol","SIEGY"},{"name","Siemens AG"},{"country","Germany"},{"exchange","FRA"}},
    {{"symbol","DMLRY"},{"name","Deutsche Bank AG"},{"country","Germany"},{"exchange","FRA"}},
    {{"symbol","BAYN"},{"name","Bayer AG"},{"country","Germany"},{"exchange","FRA"}},
    {{"symbol","ADDYY"},{"name","Adidas AG"},{"country","Germany"},{"exchange","FRA"}},
    {{"symbol","MBG"},{"name","Mercedes-Benz Group AG"},{"country","Germany"},{"exchange","FRA"}},
    {{"symbol","RY"},{"name","Royal Bank of Canada"},{"country","Canada"},{"exchange","TSX"}},
    {{"symbol","TD"},{"name","Toronto-Dominion Bank"},{"country","Canada"},{"exchange","TSX"}},
    {{"symbol","SHOP"},{"name","Shopify Inc."},{"country","Canada"},{"exchange","TSX"}},
    {{"symbol","BNS"},{"name","Bank of Nova Scotia"},{"country","Canada"},{"exchange","TSX"}},
    {{"symbol","CNQ"},{"name","Canadian Natural Resources Ltd."},{"country","Canada"},{"exchange","TSX"}},
    {{"symbol","BHP"},{"name","BHP Group Ltd."},{"country","Australia"},{"exchange","ASX"}},
    {{"symbol","CBA"},{"name","Commonwealth Bank of Australia"},{"country","Australia"},{"exchange","ASX"}},
    {{"symbol","WBC"},{"name","Westpac Banking Corp."},{"country","Australia"},{"exchange","ASX"}},
    {{"symbol","NAB"},{"name","National Australia Bank Ltd."},{"country","Australia"},{"exchange","ASX"}},
    {{"symbol","ANZ"},{"name","ANZ Group Holdings Ltd."},{"country","Australia"},{"exchange","ASX"}},
    {{"symbol","MC"},{"name","LVMH Moet Hennessy Louis Vuitton"},{"country","France"},{"exchange","EPA"}},
    {{"symbol","TTE"},{"name","TotalEnergies SE"},{"country","France"},{"exchange","EPA"}},
    {{"symbol","SAN"},{"name","Sanofi SA"},{"country","France"},{"exchange","EPA"}},
    {{"symbol","AIR"},{"name","Airbus SE"},{"country","France"},{"exchange","EPA"}},
    {{"symbol","BABA"},{"name","Alibaba Group Holding Ltd."},{"country","China"},{"exchange","HKEX"}},
    {{"symbol","TCEHY"},{"name","Tencent Holdings Ltd."},{"country","China"},{"exchange","HKEX"}},
    {{"symbol","JD"},{"name","JD.com Inc."},{"country","China"},{"exchange","HKEX"}},
    {{"symbol","BIDU"},{"name","Baidu Inc."},{"country","China"},{"exchange","HKEX"}},
    {{"symbol","NIO"},{"name","NIO Inc."},{"country","China"},{"exchange","HKEX"}},
    {{"symbol","VALE"},{"name","Vale SA"},{"country","Brazil"},{"exchange","B3"}},
    {{"symbol","ITUB"},{"name","Itau Unibanco Holding SA"},{"country","Brazil"},{"exchange","B3"}},
    {{"symbol","PBR"},{"name","Petrobras SA"},{"country","Brazil"},{"exchange","B3"}},
    {{"symbol","ABEV"},{"name","Ambev SA"},{"country","Brazil"},{"exchange","B3"}},
    {{"symbol","NESN"},{"name","Nestle SA"},{"country","Switzerland"},{"exchange","SIX"}},
    {{"symbol","NOVN"},{"name","Novartis AG"},{"country","Switzerland"},{"exchange","SIX"}},
    {{"symbol","ROG"},{"name","Roche Holding AG"},{"country","Switzerland"},{"exchange","SIX"}},
    {{"symbol","UBS"},{"name","UBS Group AG"},{"country","Switzerland"},{"exchange","SIX"}},
    {{"symbol","SSNLF"},{"name","Samsung Electronics Co."},{"country","South Korea"},{"exchange","KRX"}},
    {{"symbol","HYMTF"},{"name","Hyundai Motor Co."},{"country","South Korea"},{"exchange","KRX"}},
    {{"symbol","KIMTF"},{"name","Kia Corp."},{"country","South Korea"},{"exchange","KRX"}}
});

static std::mutex strategies_mutex;
static std::vector<StrategyDef> user_strategies;

static std::vector<std::string> get_countries_from_db() {
    std::vector<std::string> countries;
    for (const auto& s : STOCK_DB) {
        std::string c = s["country"];
        if (std::find(countries.begin(), countries.end(), c) == countries.end()) {
            countries.push_back(c);
        }
    }
    std::sort(countries.begin(), countries.end());
    return countries;
}

// ── HTML Dashboard ──
static const std::string HTML_PAGE = R"HTML_DELIM(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1.0">
<title>Options Pricing Platform</title>
<script src="https://cdn.jsdelivr.net/npm/chart.js@4"></script>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:'Segoe UI',system-ui,-apple-system,sans-serif;background:#0d1117;color:#c9d1d9;padding:20px;font-size:14px}
header{background:#161b22;border:1px solid #30363d;border-radius:8px;padding:20px;margin-bottom:16px}
header h1{color:#58a6ff;font-size:22px}
header p{color:#8b949e;font-size:13px;margin-top:4px}
.controls{display:flex;gap:8px;align-items:center;flex-wrap:wrap;margin-top:10px}
.controls button{padding:6px 16px;border:1px solid #30363d;border-radius:6px;background:#21262d;color:#c9d1d9;cursor:pointer;font-size:12px;font-weight:600}
.controls button:hover{background:#30363d}
.controls button.running{background:#238636;border-color:#238636;color:#fff}
.controls button.paused{background:#d29922;border-color:#d29922;color:#fff}
.controls .badge{display:inline-block;padding:3px 10px;border-radius:12px;font-size:11px;font-weight:600}
.controls .badge.running{background:#0d5320;color:#3fb950}
.controls .badge.paused{background:#3d2e00;color:#d29922}
.dashboard{display:grid;grid-template-columns:1fr 1fr;gap:16px;margin-bottom:16px}
@media(max-width:900px){.dashboard{grid-template-columns:1fr}}
.card{background:#161b22;border:1px solid #30363d;border-radius:8px;padding:16px;overflow-x:auto}
.card h2{color:#58a6ff;font-size:15px;margin-bottom:10px;border-bottom:1px solid #30363d;padding-bottom:6px}
.card h3{color:#8b949e;font-size:12px;margin:8px 0 4px;text-transform:uppercase;letter-spacing:1px}
.stat-grid{display:grid;grid-template-columns:1fr 1fr;gap:6px}
.stat{display:flex;justify-content:space-between;padding:5px 8px;background:#0d1117;border-radius:4px;font-size:13px}
.stat .label{color:#8b949e}
.stat .value{font-weight:600;font-family:'Consolas','Courier New',monospace}
.stat .value.positive{color:#3fb950}
.stat .value.negative{color:#f85149}
.stat .value.neutral{color:#d29922}
table{width:100%;border-collapse:collapse;font-size:12px}
th,td{padding:5px 6px;text-align:right;border-bottom:1px solid #21262d;white-space:nowrap}
th{color:#8b949e;font-weight:600;position:sticky;top:0;background:#161b22;cursor:pointer;user-select:none}
th:hover{color:#58a6ff}
th .sort-arrow{color:#58a6ff;margin-left:3px}
td:first-child,th:first-child{text-align:left;color:#58a6ff}
tr:hover{background:#1c2128}
tr.expanded-row{background:#1c2128}
tr.expanded-detail td{padding:0}
tr.expanded-detail table{margin:0;border-top:2px solid #30363d}
tr.expanded-detail td:first-child{color:#c9d1d9}
.table-wrap{max-height:280px;overflow-y:auto;border:1px solid #30363d;border-radius:4px}
.clickable{cursor:pointer}
.clickable:hover{color:#58a6ff}
.modal{display:none;position:fixed;top:0;left:0;width:100%;height:100%;background:rgba(0,0,0,0.7);z-index:1000}
.modal-content{background:#161b22;margin:40px auto;padding:20px;border:1px solid #30363d;border-radius:8px;max-width:700px;max-height:80vh;overflow-y:auto}
.modal h2{color:#58a6ff;margin-bottom:12px;font-size:16px}
.modal label{color:#c9d1d9;font-size:13px;display:block;margin-bottom:4px}
.modal select{width:100%;padding:6px;background:#0d1117;color:#c9d1d9;border:1px solid #30363d;border-radius:4px;margin-bottom:10px;font-size:13px}
.modal .stock-list{display:grid;grid-template-columns:1fr 1fr 1fr;gap:3px;max-height:260px;overflow-y:auto;border:1px solid #30363d;border-radius:4px;padding:6px}
.modal .stock-item{display:flex;align-items:center;gap:5px;padding:3px;font-size:12px;cursor:pointer;border-radius:3px}
.modal .stock-item:hover{background:#1c2128}
.modal .stock-item input{cursor:pointer}
.modal .modal-actions{display:flex;gap:8px;justify-content:flex-end;margin-top:12px}
.modal .modal-actions button{padding:6px 20px;border:1px solid #30363d;border-radius:6px;cursor:pointer;font-weight:600;font-size:13px}
.modal .modal-actions .btn-primary{background:#1f6feb;color:#fff;border-color:#1f6feb}
.modal .modal-actions .btn-secondary{background:#21262d;color:#c9d1d9}
.heatmap{display:grid;gap:2px;margin-top:8px}
.heatmap-cell{text-align:center;padding:4px 2px;font-size:10px;border-radius:2px;font-family:'Consolas',monospace;color:#fff;font-weight:600}
.heatmap-label{color:#8b949e;font-size:10px;text-align:center;padding:2px}
.tooltip{position:relative;cursor:help;border-bottom:1px dashed #30363d}
.tooltip:hover::after{content:attr(data-tip);position:absolute;background:#1c2128;color:#c9d1d9;padding:4px 8px;border-radius:4px;font-size:11px;white-space:nowrap;z-index:100;bottom:100%;left:50%;transform:translateX(-50%)}
@media(max-width:600px){body{padding:10px}.dashboard{grid-template-columns:1fr}.modal .stock-list{grid-template-columns:1fr 1fr}}
</style>
</head>
<body>
<header>
<h1>Options Pricing Platform</h1>
<p>Black-Scholes + Monte Carlo Engine &middot; Real-time Risk Dashboard</p>
<div class="controls">
<button id="btnPause" class="running" onclick="togglePause()">Pause</button>
<button onclick="confirmReset()">Reset</button>
<button onclick="openStockModal()">Stocks</button>
<button onclick="openStrategyModal()">Strategies</button>
<button onclick="window.open('/api/export/csv','_blank')">CSV</button>
<button onclick="window.open('/api/export/json','_blank')">JSON</button>
<button onclick="exportPDF()">PDF</button>
<span id="statusBadge" class="badge running">Running</span>
<span style="color:#8b949e;font-size:12px" id="statusInfo">0 pos &middot; 0 sym</span>
</div>
</header>

<div class="tabs">
<button class="tab-btn active" onclick="switchTab('portfolio')">Portfolio</button>
<button class="tab-btn" onclick="switchTab('convergence')">Convergence</button>
<button class="tab-btn" onclick="switchTab('exotic')">Exotic</button>
<button class="tab-btn" onclick="switchTab('strategies')">Strategies</button>
</div>

<!-- Portfolio / Standard Tab -->
<div id="portfolioSection">
<div class="dashboard">
<div class="card">
<h2>Greeks <span id="tabLabel">(BS &mdash; Baseline)</span></h2>
<div class="stat-grid" id="greeksGrid"></div>
</div>
<div class="card">
<h2>P&L Summary</h2>
<div class="stat-grid" id="plGrid"></div>
<div><canvas id="plChart"></canvas></div>
</div>
</div>

<div class="dashboard">
<div class="card">
<h2>Risk Analytics</h2>
<div class="stat-grid" id="analyticsGrid"></div>
<h3>Stress Scenarios</h3>
<div class="stat-grid" id="stressGrid"></div>
</div>
<div class="card">
<h2>Vol Surface <span style="color:#8b949e;font-weight:400;font-size:11px">IV by Strike &times; Expiry</span></h2>
<div id="surfaceHeatmap" style="min-height:80px"></div>
</div>
</div>

<div class="card" style="margin-bottom:12px">
<h2>Greeks Surface &mdash; <span style="font-weight:400;color:#8b949e">click ticker to expand chain</span></h2>
<div class="table-wrap"><table><thead><tr>
<th onclick="sortTable('surface','ticker')">Ticker<span class="sort-arrow"></span></th>
<th onclick="sortTable('surface','count')">#Pos<span class="sort-arrow"></span></th>
<th onclick="sortTable('surface','delta')">Delta<span class="sort-arrow"></span></th>
<th onclick="sortTable('surface','gamma')">Gamma<span class="sort-arrow"></span></th>
<th onclick="sortTable('surface','vega')">Vega<span class="sort-arrow"></span></th>
<th onclick="sortTable('surface','theta')">Theta<span class="sort-arrow"></span></th>
<th onclick="sortTable('surface','iv')">IV<span class="sort-arrow"></span></th>
<th onclick="sortTable('surface','value')">Value<span class="sort-arrow"></span></th>
</tr></thead><tbody id="surfaceBody"></tbody></table></div>
</div>

<div class="card" style="margin-bottom:12px">
<h2>Positions</h2>
<div class="table-wrap"><table><thead><tr>
<th onclick="sortTable('pos','ticker')">Ticker<span class="sort-arrow"></span></th>
<th>Spot</th><th>Strike</th><th>Expiry</th><th>Type</th>
<th onclick="sortTable('pos','delta')">Delta<span class="sort-arrow"></span></th>
<th>Gamma</th><th>Vega</th><th>IV</th><th onclick="sortTable('pos','price')">Price<span class="sort-arrow"></span></th>
</tr></thead><tbody id="positionsBody"></tbody></table></div>
</div>
</div>

<!-- Convergence Tab -->
<div id="convergenceSection" style="display:none">
<div class="card" style="margin-bottom:12px">
<h2>Convergence Analysis &mdash; <span style="font-weight:400;color:#8b949e">MC paths &rarr; BS exact price</span></h2>
<div class="form-row" style="display:flex;gap:12px;flex-wrap:wrap;margin-bottom:12px">
<div><label>Spot ($):</label><input type="number" id="convSpot" value="100" step="0.01" style="width:100px"></div>
<div><label>Strike ($):</label><input type="number" id="convStrike" value="100" step="0.01" style="width:100px"></div>
<div><label>Expiry (yr):</label><input type="number" id="convExpiry" value="0.5" step="0.01" style="width:80px"></div>
<div><label>Rate (%):</label><input type="number" id="convRate" value="4" step="0.1" style="width:70px"></div>
<div><label>Vol (%):</label><input type="number" id="convVol" value="25" step="1" style="width:70px"></div>
<div><label>Type:</label>
<select id="convType"><option value="CALL">Call</option><option value="PUT">Put</option></select></div>
<div><label>Max Paths:</label>
<select id="convMaxPaths"><option value="10000">10K</option><option value="50000">50K</option><option value="100000" selected>100K</option><option value="200000">200K</option></select></div>
<div style="align-self:flex-end"><button class="btn-primary" onclick="runConvergence()">Run</button></div>
</div>
<div style="height:300px"><canvas id="convChart"></canvas></div>
<div class="table-wrap" style="margin-top:12px"><table><thead><tr><th>Paths</th><th>BS Price</th><th>MC Price</th><th>Error</th><th>Std Error</th></tr></thead><tbody id="convBody"></tbody></table></div>
</div>
</div>

<!-- Exotic Tab -->
<div id="exoticSection" style="display:none">
<div class="card" style="margin-bottom:12px">
<h2>Exotic Options &mdash; <span style="font-weight:400;color:#8b949e">MC pricing for path-dependent options</span></h2>
<div class="form-row" style="display:flex;gap:12px;flex-wrap:wrap;margin-bottom:12px">
<div><label>Style:</label>
<select id="exoticStyle" onchange="toggleExoticFields()">
<option value="asian">Asian (Average)</option>
<option value="barrier">Barrier</option>
<option value="lookback">Lookback</option>
</select></div>
<div><label>Option Type:</label>
<select id="exoticType"><option value="CALL">Call</option><option value="PUT">Put</option></select></div>
<div><label>Ticker:</label>
<select id="exoticTicker"></select></div>
<div><label>Spot ($):</label><input type="number" id="exoticSpot" value="100" step="0.01" style="width:100px"></div>
<div><label>Strike ($):</label><input type="number" id="exoticStrike" value="100" step="0.01" style="width:100px"></div>
<div><label>Expiry (yr):</label><input type="number" id="exoticExpiry" value="0.5" step="0.01" style="width:80px"></div>
<div><label>Rate (%):</label><input type="number" id="exoticRate" value="4" step="0.1" style="width:70px"></div>
<div><label>Vol (%):</label><input type="number" id="exoticVol" value="25" step="1" style="width:70px"></div>
<div id="exoticBarrierField" style="display:none">
<label>Barrier ($):</label><input type="number" id="exoticBarrier" value="90" step="0.01" style="width:100px">
<label>Direction:</label>
<select id="exoticBarrierDir"><option value="down">Down-and-Out</option><option value="up">Up-and-Out</option></select>
</div>
<div style="align-self:flex-end"><button class="btn-primary" onclick="priceExotic()">Price</button></div>
</div>
<div id="exoticResult" style="margin-top:12px;padding:12px;background:#161b22;border-radius:6px"></div>
</div>
</div>

<!-- Strategy Tab -->
<div class="card" id="strategyCard" style="margin-bottom:12px;display:none">
<h2>Strategies &mdash; <span style="font-weight:400;color:#8b949e">P&amp;L at Expiry</span></h2>
<div style="display:flex;gap:16px;flex-wrap:wrap">
<div style="flex:1;min-width:300px"><canvas id="strategyChart"></canvas></div>
<div style="flex:1;min-width:200px"><div class="stat-grid" id="strategyGrid"></div></div>
</div>
<div id="strategyLegs" style="margin-top:8px;font-size:12px;color:#8b949e"></div>
</div>

<div class="footer">
Options Pricing Platform v2.0 &middot; <a href="/api/export/csv" style="color:#58a6ff">Export CSV</a> &middot; <a href="/api/export/json" style="color:#58a6ff">Export JSON</a>
</div>

<div id="strategyModal" class="modal">
<div class="modal-content" style="max-width:600px">
<h2>Build Strategy</h2>
<label>Strategy Type:</label>
<select id="stratType" onchange="updateStratForm()">
<option value="Covered Call">Covered Call</option>
<option value="Protective Put">Protective Put</option>
<option value="Straddle">Straddle</option>
<option value="Strangle">Strangle</option>
<option value="Bull Call Spread">Bull Call Spread</option>
<option value="Bear Put Spread">Bear Put Spread</option>
<option value="Butterfly">Butterfly</option>
<option value="Iron Condor">Iron Condor</option>
</select>
<label>Ticker:</label>
<select id="stratTicker"></select>
<label>Spot Price ($):</label>
<input type="number" id="stratSpot" step="0.01" value="100">
<label>Expiry (years):</label>
<input type="number" id="stratExpiry" step="0.01" value="0.5">
<div id="stratFormFields"></div>
<div class="modal-actions">
<button class="btn-secondary" onclick="closeStrategyModal()">Cancel</button>
<button class="btn-primary" onclick="buildStrategy()">Build &amp; View</button>
</div>
<div id="stratResult" style="margin-top:10px;color:#8b949e;font-size:12px"></div>
</div>
</div>

<div id="stockModal" class="modal">
<div class="modal-content">
<h2>Configure Stocks</h2>
<label for="countryFilter">Filter by Country:</label>
<select id="countryFilter" onchange="filterStocks()"><option value="all">All Countries</option></select>
<label>Select stocks:</label>
<div id="stockList" class="stock-list"></div>
<div class="modal-actions">
<button class="btn-secondary" onclick="closeStockModal()">Cancel</button>
<button class="btn-primary" onclick="applyStocks()">Apply</button>
</div>
</div>
</div>

<script>
let plChart = null;
let currentTab = 'portfolio';
let allStocks = [];
let selectedStocks = new Set();
let surfaceSort = {key:null,dir:1};
let posSort = {key:null,dir:1};
let lastData = null;

async function loadStocks() {
    try {
        const r = await fetch('/api/stocks'); allStocks = await r.json();
        const countries = [...new Set(allStocks.map(s=>s.country))].sort();
        const sel = document.getElementById('countryFilter');
        countries.forEach(c=>{const o=document.createElement('option');o.value=c;o.textContent=c;sel.appendChild(o)});
        const sr=await fetch('/api/status'); const st=await sr.json();
        selectedStocks=new Set(st.active_symbols); renderStockList();
    } catch(e){}
}

function renderStockList() {
    const f=document.getElementById('countryFilter').value;
    const c=document.getElementById('stockList'); c.innerHTML='';
    allStocks.filter(s=>f==='all'||s.country===f).forEach(s=>{
        const d=document.createElement('div'); d.className='stock-item';
        const i=document.createElement('input'); i.type='checkbox'; i.checked=selectedStocks.has(s.symbol);
        i.onchange=()=>{if(i.checked)selectedStocks.add(s.symbol);else selectedStocks.delete(s.symbol)};
        const l=document.createElement('span'); l.textContent=s.symbol+' - '+s.name+' ('+s.exchange+')';
        d.appendChild(i); d.appendChild(l); c.appendChild(d);
    });
}
function filterStocks(){renderStockList()}
function openStockModal(){document.getElementById('stockModal').style.display='block';renderStockList()}
function closeStockModal(){document.getElementById('stockModal').style.display='none'}
async function applyStocks(){
    const s=Array.from(selectedStocks); if(!s.length){alert('Select at least one stock.');return}
    try{
        const r=await fetch('/api/configure',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({symbols:s})});
        if(!r.ok){alert('Failed to apply: '+await r.text());return}
        closeStockModal(); await loadStocks(); setTimeout(refresh,500);
    }catch(e){alert('Error: '+e.message)}
}

async function togglePause(){
    try{const r=await fetch('/api/status');const s=await r.json();
        await fetch(s.paused?'/api/resume':'/api/pause',{method:'POST'});updateStatus()
    }catch(e){}
}
async function confirmReset(){if(!confirm('Reset all positions?'))return;try{await fetch('/api/reset',{method:'POST'});setTimeout(refresh,500)}catch(e){}}

async function updateStatus(){
    try{
        const r=await fetch('/api/status');const s=await r.json();
        const b=document.getElementById('statusBadge'),i=document.getElementById('statusInfo'),bp=document.getElementById('btnPause');
        if(s.paused){b.className='badge paused';b.textContent='Paused';bp.className='paused';bp.textContent='Resume'}
        else{b.className='badge running';b.textContent='Running';bp.className='running';bp.textContent='Pause'}
        i.textContent=s.positions+' pos &middot; '+s.symbols+' sym';
    }catch(e){}
}

function switchTab(tab){
    currentTab=tab;
    document.querySelectorAll('.tab-btn').forEach(b=>b.classList.remove('active'));
    document.querySelectorAll('.tab-btn').forEach(b=>{if(b.textContent.toLowerCase().includes(tab))b.classList.add('active')});
    document.getElementById('portfolioSection').style.display=tab==='portfolio'?'':'none';
    document.getElementById('convergenceSection').style.display=tab==='convergence'?'':'none';
    document.getElementById('exoticSection').style.display=tab==='exotic'?'':'none';
    const sc=document.getElementById('strategyCard');
    if(sc)sc.style.display=tab==='strategies'?'block':'none';
    if(tab==='exotic')populateExoticTicker();
    if(lastData&&tab==='portfolio')updateDashboard(lastData);
}

function toggleChain(ticker){
    const row=document.getElementById('chain-'+ticker);
    if(row){row.remove();return}
    const dd=lastData; if(!dd||!dd.ticker_positions||!dd.ticker_positions[ticker])return;
    const table=document.querySelector('#surfaceBody');
    const refRow=Array.from(table.children).find(r=>r.cells[0]&&r.cells[0].textContent.trim()===ticker);
    if(!refRow)return;
    // Find correct insert position (after refRow, before next ticker or end)
    let insertAfter=refRow;
    while(insertAfter.nextElementSibling&&insertAfter.nextElementSibling.id&&insertAfter.nextElementSibling.id.startsWith('chain-')){insertAfter=insertAfter.nextElementSibling}
    const tr=document.createElement('tr'); tr.id='chain-'+ticker; tr.className='expanded-detail';
    const td=document.createElement('td'); td.colSpan=10; td.style.padding='0';
    let html='<table><thead><tr style="background:#0d1117"><th>Strike</th><th>Expiry</th><th>Type</th><th>MC Price</th><th>BS Price</th><th>MC Delta</th><th>BS Delta</th><th>MC IV</th><th>BS IV</th></tr></thead><tbody>';
    dd.ticker_positions[ticker].forEach(p=>{
        html+='<tr><td>$'+p.strike.toFixed(2)+'</td><td>'+p.expiry.toFixed(4)+'yr</td><td>'+p.type+'</td><td>$'+p.mc_price.toFixed(2)+'</td><td>$'+p.bs_price.toFixed(2)+'</td><td>'+p.mc_delta.toFixed(4)+'</td><td>'+p.bs_delta.toFixed(4)+'</td><td>'+(p.mc_iv*100).toFixed(2)+'%</td><td>'+(p.bs_iv*100).toFixed(2)+'%</td></tr>';
    });
    html+='</tbody></table>'; td.innerHTML=html; tr.appendChild(td);
    insertAfter.insertAdjacentElement('afterend',tr);
}

function sortTable(type,key){
    if(type==='surface'){if(surfaceSort.key===key)surfaceSort.dir*=-1;else{surfaceSort.key=key;surfaceSort.dir=1}}
    else{if(posSort.key===key)posSort.dir*=-1;else{posSort.key=key;posSort.dir=1}}
    renderTables();
}

function renderTables(){
    const d=lastData; if(!d)return;
    // Surface table (always show BS values)
    let surfHtml=''; let tickerKeys=[];
    if(d.tickers){tickerKeys=Object.keys(d.tickers).sort();
        if(surfaceSort.key){
            tickerKeys.sort((a,b)=>{
                const sa=d.tickers[a],sb=d.tickers[b]; let va,vb;
                if(surfaceSort.key==='ticker'){va=a;vb=b}
                else if(surfaceSort.key==='count'){va=sa.count;vb=sb.count}
                else if(surfaceSort.key==='delta'){va=sa.bs_delta;vb=sb.bs_delta}
                else if(surfaceSort.key==='gamma'){va=sa.bs_gamma;vb=sb.bs_gamma}
                else if(surfaceSort.key==='vega'){va=sa.bs_vega;vb=sb.bs_vega}
                else if(surfaceSort.key==='theta'){va=sa.bs_theta;vb=sb.bs_theta}
                else if(surfaceSort.key==='iv'){va=sa.bs_iv;vb=sb.bs_iv}
                else if(surfaceSort.key==='value'){va=sa.bs_value;vb=sb.bs_value}
                if(typeof va==='string')return va.localeCompare(vb)*surfaceSort.dir;
                return (va-vb)*surfaceSort.dir;
            });
        }
        tickerKeys.forEach(t=>{
            const s=d.tickers[t];
            surfHtml+='<tr><td class="clickable" onclick="toggleChain(\''+t+'\')">'+t+' <span style="color:#30363d;font-size:10px">&#9654;</span></td><td>'+s.count+'</td><td>'+fmt(s.bs_delta,2)+'</td><td>'+fmt(s.bs_gamma,4)+'</td><td>'+fmt(s.bs_vega,1)+'</td><td>'+fmt(s.bs_theta,1)+'</td><td>'+fmtPct(s.bs_iv)+'</td><td>$'+fmt(s.bs_value,2)+'</td></tr>';
        });
        if(tickerKeys.length>1){
            surfHtml+='<tr style="font-weight:700;border-top:2px solid #58a6ff"><td>TOTAL</td><td>'+d.positions_total+'</td><td>'+fmt(d.bs_delta,2)+'</td><td>'+fmt(d.bs_gamma,4)+'</td><td>'+fmt(d.bs_vega,1)+'</td><td>'+fmt(d.bs_theta,1)+'</td><td>'+fmtPct(d.bs_avg_iv)+'</td><td>$'+fmt(d.bs_value,2)+'</td></tr>';
        }
    }
    document.getElementById('surfaceBody').innerHTML=surfHtml;

    // Positions table (show BS values)
    let posHtml=''; let posArr=[];
    if(d.recent_positions){posArr=d.recent_positions.slice();
        if(posSort.key){
            posArr.sort((a,b)=>{
                let va,vb;
                if(posSort.key==='ticker'){va=a.ticker;vb=b.ticker}
                else if(posSort.key==='delta'){va=a.bs_delta;vb=b.bs_delta}
                else if(posSort.key==='price'){va=a.bs_price;vb=b.bs_price}
                if(typeof va==='string')return va.localeCompare(vb)*posSort.dir;
                return (va-vb)*posSort.dir;
            });
        }
        posArr.forEach(p=>{
            posHtml+='<tr><td>'+p.ticker+'</td><td>$'+fmt(p.spot)+'</td><td>$'+fmt(p.strike)+'</td><td>'+p.expiry.toFixed(4)+'yr</td><td>'+p.type+'</td><td>'+fmt(p.bs_delta,4)+'</td><td>'+fmt(p.bs_gamma,4)+'</td><td>'+fmt(p.bs_vega,1)+'</td><td>'+fmtPct(p.bs_iv)+'</td><td>$'+fmt(p.bs_price,2)+'</td></tr>';
        });
    }
    document.getElementById('positionsBody').innerHTML=posHtml;
}

let strategyChart = null;
let strategyData = null;

function updateDashboard(d) {
    lastData = d;
    const tab = currentTab;
    if (tab !== 'portfolio') { updateStatus(); return; }

    function greekRow(label, key, decimals) {
        const v = d[key] || 0;
        return '<div class="stat"><span class="label">' + label + '</span><span class="value ' + statusClass(v, label.toLowerCase()) + '">' + fmt(v, decimals) + '</span></div>';
    }
    document.getElementById('greeksGrid').innerHTML =
        '<div class="stat"><span class="label">Positions</span><span class="value">' + (d.positions_total || 0) + '</span></div>' +
        '<div class="stat"><span class="label">Last Spot</span><span class="value">$' + fmt(d.last_spot) + '</span></div>' +
        greekRow('Delta', 'bs_delta', 4) +
        greekRow('Gamma', 'bs_gamma', 4) +
        greekRow('Vega', 'bs_vega', 1) +
        greekRow('Theta', 'bs_theta', 1) +
        greekRow('Rho', 'bs_rho', 1) +
        '<div class="stat"><span class="label">IV (avg)</span><span class="value neutral">' + fmtPct(d.bs_avg_iv || 0) + '</span></div>' +
        '<div class="stat"><span class="label">HV (annual)</span><span class="value neutral">' + fmtPct(d.hv || 0) + '</span></div>';

    document.getElementById('plGrid').innerHTML =
        '<div class="stat"><span class="label">Total Cost</span><span class="value">$' + fmt(d.total_cost) + '</span></div>' +
        '<div class="stat"><span class="label">Portfolio Value (BS)</span><span class="value">$' + fmt(d.bs_value) + '</span></div>' +
        '<div class="stat"><span class="label">Unrealized P&L</span><span class="value ' + (d.unrealized_pl >= 0 ? 'positive' : 'negative') + '">' + (d.unrealized_pl >= 0 ? '+' : '') + '$' + fmt(d.unrealized_pl) + '</span></div>' +
        '<div class="stat"><span class="label">Ticker</span><span class="value">' + (d.last_ticker || '') + '</span></div>';

    const a = d.analytics || {};
    document.getElementById('analyticsGrid').innerHTML =
        '<div class="stat"><span class="label tooltip" data-tip="Annualized return/risk">Sharpe Ratio</span><span class="value ' + (a.sharpe > 1 ? 'positive' : a.sharpe > 0 ? 'neutral' : 'negative') + '">' + fmt(a.sharpe, 2) + '</span></div>' +
        '<div class="stat"><span class="label tooltip" data-tip="95% Value at Risk">VaR (95%)</span><span class="value ' + (a.var95 < 0 ? 'negative' : 'positive') + '">$' + fmt(a.var95, 2) + '</span></div>' +
        '<div class="stat"><span class="label tooltip" data-tip="99% Value at Risk">VaR (99%)</span><span class="value ' + (a.var99 < 0 ? 'negative' : 'positive') + '">$' + fmt(a.var99, 2) + '</span></div>' +
        '<div class="stat"><span class="label tooltip" data-tip="Peak-to-trough decline">Max Drawdown</span><span class="value negative">' + (a.max_drawdown * 100).toFixed(2) + '%</span></div>';

    document.getElementById('stressGrid').innerHTML =
        '<div class="stat"><span class="label tooltip" data-tip="P&L impact if spot drops 10%">Market Crash (-10%)</span><span class="value ' + (a.stress_crash < 0 ? 'negative' : 'positive') + '">$' + fmt(a.stress_crash, 2) + '</span></div>' +
        '<div class="stat"><span class="label tooltip" data-tip="P&L impact if IV rises 20%">Vol Spike (+20%)</span><span class="value ' + (a.stress_vol > 0 ? 'positive' : 'negative') + '">$' + fmt(a.stress_vol, 2) + '</span></div>' +
        '<div class="stat"><span class="label tooltip" data-tip="P&L impact if rates rise 1%">Rate Shock (+1%)</span><span class="value ' + (a.stress_rate > 0 ? 'positive' : 'negative') + '">$' + fmt(a.stress_rate, 2) + '</span></div>';

    if (d.ticker_positions) {
        const tickers = Object.keys(d.ticker_positions).sort();
        let heatHtml = '';
        tickers.forEach(t => {
            const positions = d.ticker_positions[t];
            if (!positions || !positions.length) return;
            const strikes = [...new Set(positions.map(p => p.strike))].sort((a, b) => a - b);
            const expiries = [...new Set(positions.map(p => p.expiry))].sort((a, b) => a - b);
            heatHtml += '<div style="margin-bottom:8px"><div style="color:#58a6ff;font-size:12px;font-weight:600;margin-bottom:3px">' + t + '</div>';
            heatHtml += '<div style="display:grid;grid-template-columns:80px repeat(' + expiries.length + ',60px);gap:2px;align-items:center">';
            heatHtml += '<div class="heatmap-label">Strike \\ Exp</div>';
            expiries.forEach(e => { heatHtml += '<div class="heatmap-label">' + (e * 12).toFixed(1) + 'mo</div>' });
            strikes.forEach(s => {
                heatHtml += '<div class="heatmap-label">$' + s.toFixed(0) + '</div>';
                expiries.forEach(e => {
                    const pos = positions.find(p => Math.abs(p.strike - s) < 0.01 && Math.abs(p.expiry - e) < 0.001);
                    if (pos) {
                        const iv = pos.bs_iv || pos.mc_iv || 0;
                        const r = Math.min(255, Math.round(255 * (iv / 0.5)));
                        const g = Math.min(255, Math.round(255 * (1 - iv / 0.5)));
                        const bg = 'rgb(' + Math.max(0, 255 - r * 2) + ',' + Math.max(0, 255) + ',50)';
                        heatHtml += '<div class="heatmap-cell" style="background:' + bg + '">' + (iv * 100).toFixed(1) + '%</div>';
                    } else {
                        heatHtml += '<div class="heatmap-cell" style="background:#0d1117;color:#30363d">-</div>';
                    }
                });
            });
            heatHtml += '</div></div>';
        });
        document.getElementById('surfaceHeatmap').innerHTML = heatHtml || '<div style="color:#30363d;padding:12px;text-align:center">No data yet</div>';
    }

    plHistory.push(d.unrealized_pl);
    if (plHistory.length > 50) plHistory.shift();
    if (!plChart) {
        const ctx = document.getElementById('plChart').getContext('2d');
        plChart = new Chart(ctx, { type: 'line', data: { labels: [], datasets: [{ label: 'P&L', data: [], borderColor: '#3fb950', backgroundColor: 'rgba(63,185,80,0.1)', fill: true, tension: 0.3 }] }, options: { responsive: true, maintainAspectRatio: false, plugins: { legend: { labels: { color: '#8b949e' } } }, scales: { x: { display: false }, y: { grid: { color: '#30363d' }, ticks: { color: '#8b949e' } } } } });
    }
    if (plChart) {
        plChart.data.labels = plHistory.map((_, i) => i + 1);
        plChart.data.datasets[0].data = plHistory;
        plChart.update();
    }

    renderTables();
    updateStatus();
}

// ── SSE Streaming ──
let sseSource = null;
function startSSE() {
    if (sseSource) { sseSource.close(); }
    sseSource = new EventSource('/api/stream');
    sseSource.onmessage = function(e) {
        try { const d = JSON.parse(e.data); updateDashboard(d); } catch (x) { console.error(x); }
    };
    sseSource.onerror = function() {
        console.warn('SSE disconnected, falling back to polling');
        sseSource.close();
        sseSource = null;
        setInterval(function() {
            fetch('/api/dashboard').then(r => r.json()).then(d => updateDashboard(d)).catch(() => {});
        }, 5000);
    };
}

async function refresh() {
    try { const r = await fetch('/api/dashboard'); const d = await r.json(); updateDashboard(d); } catch (e) { setTimeout(refresh, 5000); }
}

// ── Convergence Analysis ──
let convChart = null;
async function runConvergence() {
    const body = {
        spot: parseFloat(document.getElementById('convSpot').value),
        strike: parseFloat(document.getElementById('convStrike').value),
        expiry: parseFloat(document.getElementById('convExpiry').value),
        rate: parseFloat(document.getElementById('convRate').value) / 100,
        vol: parseFloat(document.getElementById('convVol').value) / 100,
        type: document.getElementById('convType').value,
        max_paths: parseInt(document.getElementById('convMaxPaths').value)
    };
    try {
        const r = await fetch('/api/convergence', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify(body)
        });
        const data = await r.json();
        // Table
        let html = '';
        data.forEach(p => {
            html += '<tr><td>' + p.paths + '</td><td>$' + fmt(p.bs_price) + '</td><td>$' + fmt(p.mc_price) + '</td><td>' + fmt(p.error, 4) + '</td><td>' + fmt(p.std_error, 4) + '</td></tr>';
        });
        document.getElementById('convBody').innerHTML = html;
        // Chart
        const ctx = document.getElementById('convChart');
        if (convChart) convChart.destroy();
        convChart = new Chart(ctx, {
            type: 'scatter',
            data: {
                datasets: [{
                    label: 'MC Price',
                    data: data.map(p => ({x: p.paths, y: p.mc_price})),
                    backgroundColor: '#58a6ff',
                    showLine: true, tension: 0.3
                }, {
                    label: 'BS Price (exact)',
                    data: data.map(p => ({x: p.paths, y: p.bs_price})),
                    borderColor: '#d29922',
                    backgroundColor: 'rgba(210,153,34,0.1)',
                    showLine: true, borderDash: [5,5],
                    pointRadius: 0
                }]
            },
            options: {
                responsive: true, maintainAspectRatio: false,
                plugins: { legend: { labels: { color: '#8b949e' } } },
                scales: {
                    x: { title: { display: true, text: 'Number of Paths', color: '#8b949e' }, ticks: { color: '#8b949e' } },
                    y: { title: { display: true, text: 'Option Price ($)', color: '#8b949e' }, ticks: { color: '#8b949e' }, grid: { color: '#30363d' } }
                }
            }
        });
    } catch (e) {
        document.getElementById('convBody').innerHTML = '<tr><td colspan="5" style="color:#f85149">Error: ' + e.message + '</td></tr>';
    }
}

// ── Exotic Options ──
function toggleExoticFields() {
    const style = document.getElementById('exoticStyle').value;
    document.getElementById('exoticBarrierField').style.display = style === 'barrier' ? 'block' : 'none';
}
function populateExoticTicker() {
    const sel = document.getElementById('exoticTicker');
    sel.innerHTML = '';
    allStocks.filter(s => selectedStocks.has(s.symbol)).forEach(s => {
        const o = document.createElement('option');
        o.value = s.symbol;
        o.textContent = s.symbol + ' - ' + s.name;
        sel.appendChild(o);
    });
}
async function priceExotic() {
    const body = {
        style: document.getElementById('exoticStyle').value,
        type: document.getElementById('exoticType').value,
        ticker: document.getElementById('exoticTicker').value,
        spot: parseFloat(document.getElementById('exoticSpot').value),
        strike: parseFloat(document.getElementById('exoticStrike').value),
        expiry: parseFloat(document.getElementById('exoticExpiry').value),
        rate: parseFloat(document.getElementById('exoticRate').value) / 100,
        vol: parseFloat(document.getElementById('exoticVol').value) / 100
    };
    if (body.style === 'barrier') {
        body.barrier = parseFloat(document.getElementById('exoticBarrier').value);
        body.down_and_out = document.getElementById('exoticBarrierDir').value === 'down';
    }
    try {
        const r = await fetch('/api/exotic', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify(body)
        });
        const result = await r.json();
        let styleLabels = {asian: 'Asian', barrier: 'Barrier', lookback: 'Lookback'};
        document.getElementById('exoticResult').innerHTML =
            '<h3 style="margin-top:0;color:#58a6ff">' + styleLabels[body.style] + ' ' + body.type + ' &mdash; ' + body.ticker + '</h3>' +
            '<div class="stat-grid">' +
            '<div class="stat"><span class="label">Price (MC)</span><span class="value positive">$' + fmt(result.price, 2) + '</span></div>' +
            '<div class="stat"><span class="label">Delta</span><span class="value">' + fmt(result.delta, 4) + '</span></div>' +
            '<div class="stat"><span class="label">Spot</span><span class="value">$' + fmt(body.spot) + '</span></div>' +
            '<div class="stat"><span class="label">Strike</span><span class="value">$' + fmt(body.strike) + '</span></div>' +
            '<div class="stat"><span class="label">Expiry</span><span class="value">' + body.expiry + ' yr</span></div>' +
            '<div class="stat"><span class="label">Vol</span><span class="value">' + (body.vol * 100).toFixed(0) + '%</span></div>' +
            (body.barrier ? '<div class="stat"><span class="label">Barrier</span><span class="value">$' + fmt(body.barrier) + ' (' + (body.down_and_out ? 'Down-and-Out' : 'Up-and-Out') + ')</span></div>' : '') +
            '</div>';
    } catch (e) {
        document.getElementById('exoticResult').innerHTML = '<span style="color:#f85149">Error: ' + e.message + '</span>';
    }
}

// ── Strategy Builder ──
function openStrategyModal() {
    document.getElementById('strategyModal').style.display = 'block';
    // Populate ticker dropdown from stock list
    const sel = document.getElementById('stratTicker');
    sel.innerHTML = '';
    allStocks.filter(s => selectedStocks.has(s.symbol)).forEach(s => { const o = document.createElement('option'); o.value = s.symbol; o.textContent = s.symbol + ' - ' + s.name; sel.appendChild(o); });
    updateStratForm();
}
function closeStrategyModal() { document.getElementById('strategyModal').style.display = 'none'; }
function updateStratForm() {
    const type = document.getElementById('stratType').value;
    const fields = document.getElementById('stratFormFields');
    const twoStrikes = ['Strangle', 'Bull Call Spread', 'Bear Put Spread'];
    const threeStrikes = ['Butterfly'];
    const fourStrikes = ['Iron Condor'];
    let html = '';
    if (type === 'Covered Call' || type === 'Protective Put' || type === 'Straddle') {
        html = '<label>Strike ($):</label><input type="number" id="stratStrike" step="0.01" value="100">';
    } else if (twoStrikes.includes(type)) {
        html = '<label>Low Strike ($):</label><input type="number" id="stratLowStrike" step="0.01" value="95">';
        html += '<label>High Strike ($):</label><input type="number" id="stratHighStrike" step="0.01" value="105">';
    } else if (threeStrikes.includes(type)) {
        html = '<label>Low Strike ($):</label><input type="number" id="stratLowStrike" step="0.01" value="90">';
        html += '<label>Mid Strike ($):</label><input type="number" id="stratMidStrike" step="0.01" value="100">';
        html += '<label>High Strike ($):</label><input type="number" id="stratHighStrike" step="0.01" value="110">';
    } else if (fourStrikes.includes(type)) {
        html = '<label>Put Low ($):</label><input type="number" id="stratPutLow" step="0.01" value="85">';
        html += '<label>Put High ($):</label><input type="number" id="stratPutHigh" step="0.01" value="95">';
        html += '<label>Call Low ($):</label><input type="number" id="stratCallLow" step="0.01" value="105">';
        html += '<label>Call High ($):</label><input type="number" id="stratCallHigh" step="0.01" value="115">';
    }
    fields.innerHTML = html;
}
async function buildStrategy() {
    const type = document.getElementById('stratType').value;
    const ticker = document.getElementById('stratTicker').value;
    const spot = parseFloat(document.getElementById('stratSpot').value);
    const expiry = parseFloat(document.getElementById('stratExpiry').value);
    const body = { type, ticker, spot, expiry };
    const twoStrikes = ['Strangle', 'Bull Call Spread', 'Bear Put Spread'];
    const threeStrikes = ['Butterfly'];
    const fourStrikes = ['Iron Condor'];
    if (document.getElementById('stratStrike')) body.strike = parseFloat(document.getElementById('stratStrike').value);
    if (document.getElementById('stratLowStrike')) body.low_strike = parseFloat(document.getElementById('stratLowStrike').value);
    if (document.getElementById('stratHighStrike')) body.high_strike = parseFloat(document.getElementById('stratHighStrike').value);
    if (document.getElementById('stratMidStrike')) body.mid_strike = parseFloat(document.getElementById('stratMidStrike').value);
    if (document.getElementById('stratPutLow')) body.put_low = parseFloat(document.getElementById('stratPutLow').value);
    if (document.getElementById('stratPutHigh')) body.put_high = parseFloat(document.getElementById('stratPutHigh').value);
    if (document.getElementById('stratCallLow')) body.call_low = parseFloat(document.getElementById('stratCallLow').value);
    if (document.getElementById('stratCallHigh')) body.call_high = parseFloat(document.getElementById('stratCallHigh').value);
    try {
        const r = await fetch('/api/strategy/build', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) });
        const result = await r.json();
        document.getElementById('stratResult').innerHTML = '<span style="color:#3fb950">Built: ' + result.name + ' (' + result.ticker + ')</span>';
        strategyData = result;
        renderStrategies();
        closeStrategyModal();
        switchTab('strategies');
    } catch (e) { document.getElementById('stratResult').innerHTML = '<span style="color:#f85149">Error building strategy</span>'; }
}
async function renderStrategies() {
    if (strategyData && strategyData.pnl_data) {
        const ctx = document.getElementById('strategyChart');
        if (!ctx) return;
        const pd = strategyData.pnl_data;
        if (strategyChart) strategyChart.destroy();
        strategyChart = new Chart(ctx, {
            type: 'line',
            data: {
                labels: pd.map(p => '$' + p.spot.toFixed(0)),
                datasets: [{
                    label: strategyData.name + ' P&L',
                    data: pd.map(p => p.pnl),
                    borderColor: '#58a6ff',
                    backgroundColor: 'rgba(88,166,255,0.1)',
                    fill: true, tension: 0.3
                }, {
                    label: 'Delta',
                    data: pd.map(p => p.delta),
                    borderColor: '#d29922',
                    backgroundColor: 'rgba(210,153,34,0.05)',
                    fill: false, tension: 0.3, yAxisID: 'y1'
                }]
            },
            options: {
                responsive: true, maintainAspectRatio: false,
                interaction: { mode: 'index', intersect: false },
                plugins: { legend: { labels: { color: '#8b949e' } } },
                scales: {
                    x: { ticks: { color: '#8b949e', maxTicksLimit: 10 } },
                    y: { grid: { color: '#30363d' }, ticks: { color: '#8b949e' } },
                    y1: { position: 'right', grid: { display: false }, ticks: { color: '#d29922' } }
                }
            }
        });
        document.getElementById('strategyGrid').innerHTML =
            '<div class="stat"><span class="label">Name</span><span class="value">' + strategyData.name + '</span></div>' +
            '<div class="stat"><span class="label">Ticker</span><span class="value">' + strategyData.ticker + '</span></div>' +
            '<div class="stat"><span class="label">Spot</span><span class="value">$' + fmt(strategyData.spot) + '</span></div>' +
            '<div class="stat"><span class="label">Total Cost</span><span class="value">$' + fmt(strategyData.total_cost) + '</span></div>' +
            '<div class="stat"><span class="label">Delta</span><span class="value">' + fmt(strategyData.delta, 4) + '</span></div>' +
            '<div class="stat"><span class="label">Gamma</span><span class="value">' + fmt(strategyData.gamma, 4) + '</span></div>' +
            '<div class="stat"><span class="label">Vega</span><span class="value">' + fmt(strategyData.vega, 1) + '</span></div>' +
            '<div class="stat"><span class="label">Theta</span><span class="value">' + fmt(strategyData.theta, 1) + '</span></div>';
        let legHtml = strategyData.legs.map(l => l.qty + ' x ' + l.type + ' @ $' + l.strike.toFixed(2) + ' (entry: $' + l.entry_price.toFixed(2) + ')').join(' | ');
        document.getElementById('strategyLegs').innerHTML = '<strong>Legs:</strong> ' + legHtml;
    }
    // Load all strategies
    try {
        const r = await fetch('/api/strategies/current');
        const all = await r.json();
        if (all.length > 0) { strategyData = all[all.length - 1]; }
    } catch (e) {}
}

// ── PDF Export ──
function exportPDF() {
    const w = window.open('', '_blank');
    if (!w) { alert('Please allow popups for PDF export'); return; }
    let html = '<!DOCTYPE html><html><head><meta charset="UTF-8"><title>Options Portfolio Report</title>';
    html += '<style>body{font-family:Arial,sans-serif;padding:20px}h1{color:#333}h2{color:#555}table{width:100%;border-collapse:collapse;margin:10px 0}th,td{border:1px solid #ccc;padding:6px;text-align:right}th{background:#f5f5f5}td:first-child{text-align:left}.pos{color:green}.neg{color:red}</style></head><body>';
    html += '<h1>Options Portfolio Report</h1><p>Generated: ' + new Date().toLocaleString() + '</p>';
    fetch('/api/export/json').then(r => r.json()).then(data => {
        if (data.summary) {
            html += '<h2>Summary</h2><table><tr><th>Metric</th><th>Value</th></tr>';
            html += '<tr><td>Total Positions</td><td>' + (data.summary.positions_total || 0) + '</td></tr>';
            html += '<tr><td>Portfolio Value (MC)</td><td>$' + fmt(data.summary.mc_value || 0) + '</td></tr>';
            html += '<tr><td>Portfolio Value (BS)</td><td>$' + fmt(data.summary.bs_value || 0) + '</td></tr>';
            html += '<tr><td>Delta (MC)</td><td>' + fmt(data.summary.mc_delta || 0, 4) + '</td></tr>';
            html += '<tr><td>Gamma (MC)</td><td>' + fmt(data.summary.mc_gamma || 0, 4) + '</td></tr>';
            html += '<tr><td>Vega (MC)</td><td>' + fmt(data.summary.mc_vega || 0, 1) + '</td></tr>';
            html += '<tr><td>Theta (MC)</td><td>' + fmt(data.summary.mc_theta || 0, 1) + '</td></tr>';
            html += '</table>';
        }
        if (data.positions && data.positions.length) {
            html += '<h2>Positions</h2><table><tr><th>Ticker</th><th>Spot</th><th>Strike</th><th>Expiry</th><th>Type</th><th>MC Price</th><th>BS Price</th><th>MC Delta</th><th>BS IV</th></tr>';
            data.positions.forEach(p => {
                html += '<tr><td>' + p.ticker + '</td><td>$' + fmt(p.spot) + '</td><td>$' + fmt(p.strike) + '</td><td>' + (p.expiry || 0).toFixed(4) + '</td><td>' + p.type + '</td><td>$' + fmt(p.mc_price) + '</td><td>$' + fmt(p.bs_price) + '</td><td>' + fmt(p.mc_delta, 4) + '</td><td>' + fmtPct(p.bs_iv || 0) + '</td></tr>';
            });
            html += '</table>';
        }
        if (data.analytics) {
            html += '<h2>Risk Analytics</h2><table><tr><th>Metric</th><th>Value</th></tr>';
            html += '<tr><td>Sharpe Ratio</td><td>' + fmt(data.analytics.sharpe, 2) + '</td></tr>';
            html += '<tr><td>VaR (95%)</td><td>$' + fmt(data.analytics.var95, 2) + '</td></tr>';
            html += '<tr><td>VaR (99%)</td><td>$' + fmt(data.analytics.var99, 2) + '</td></tr>';
            html += '<tr><td>Max Drawdown</td><td>' + (data.analytics.max_drawdown * 100).toFixed(2) + '%</td></tr>';
            html += '<tr><td>Crash Stress</td><td>$' + fmt(data.analytics.stress_crash, 2) + '</td></tr>';
            html += '<tr><td>Vol Spike Stress</td><td>$' + fmt(data.analytics.stress_vol, 2) + '</td></tr>';
            html += '</table>';
        }
        html += '<p style="text-align:center;color:#999;margin-top:30px">Generated by Options Pricing Platform v2.0</p></body></html>';
        w.document.write(html);
        w.document.close();
        w.print();
    }).catch(() => { w.document.write('<p>Error loading data</p>'); w.document.close(); });
}

const STATUS = { LONG: 'status-long', SHORT: 'status-short', NEUTRAL: 'status-neutral' };
function statusClass(v, t) {
    if (t === 'delta') return v > 0.5 ? 'status-long' : v < -0.5 ? 'status-short' : 'status-neutral';
    if (t === 'gamma') return v > 0.01 ? 'status-long' : v < -0.01 ? 'status-short' : 'status-neutral';
    if (t === 'vega' || t === 'rho') return v > 1 ? 'status-long' : v < -1 ? 'status-short' : 'status-neutral';
    if (t === 'theta') return v < -1 ? 'status-short' : v > 1 ? 'status-long' : 'status-neutral';
    return 'status-neutral';
}
function fmt(v, d) { return typeof v === 'number' ? v.toFixed(d || 2) : v }
function fmtPct(v) { return (v * 100).toFixed(2) + '%' }
let plHistory = [];

startSSE();
loadStocks();
refresh();
</script>
</body>
</html>
)HTML_DELIM";

// ── SSE Session ──
class SSESession : public std::enable_shared_from_this<SSESession> {
public:
    SSESession(beast::tcp_stream&& stream, std::shared_ptr<PortfolioManager> pm)
        : stream_(std::move(stream)), timer_(stream_.get_executor()), pm_(std::move(pm)) {}

    void start() {
        http::response<http::empty_body> res{http::status::ok, 11};
        res.set(http::field::content_type, "text/event-stream");
        res.set(http::field::cache_control, "no-cache");
        res.set(http::field::access_control_allow_origin, "*");
        res.keep_alive(true);
        res.prepare_payload();
        http::async_write(stream_, res,
            [self = shared_from_this()](beast::error_code ec, std::size_t) {
                if (!ec) self->send_events();
            });
    }

    void stop() {
        beast::error_code ec;
        stream_.socket().shutdown(tcp::socket::shutdown_both, ec);
        stream_.close();
        timer_.cancel();
    }

private:
    void send_events() {
        if (shutdown_requested.load()) { stop(); return; }
        timer_.expires_after(std::chrono::seconds(3));
        timer_.async_wait([self = shared_from_this()](beast::error_code ec) {
            if (ec || shutdown_requested.load()) { self->stop(); return; }
            std::string body = self->pm_->to_json().dump();
            std::string data = "data: " + body + "\n\n";
            net::async_write(self->stream_, net::buffer(data),
                [self](beast::error_code ec, std::size_t) {
                    if (!ec) self->send_events();
                    else self->stop();
                });
        });
    }

    beast::tcp_stream stream_;
    net::steady_timer timer_;
    std::shared_ptr<PortfolioManager> pm_;
};

// ── HTTP Server ──
class HttpServer : public std::enable_shared_from_this<HttpServer> {
public:
    HttpServer(net::io_context& ioc, tcp::endpoint ep,
               std::shared_ptr<PortfolioManager> pm,
               std::shared_ptr<AlphaVantageClient> client)
        : ioc_(ioc), acceptor_(ioc), pm_(std::move(pm)), client_(std::move(client)) {
        beast::error_code ec;
        acceptor_.open(ep.protocol(), ec);
        if (ec) { std::cerr << "acceptor open: " << ec.message() << std::endl; return; }
        // Intentionally NOT setting reuse_address — prevents multiple instances on same port.
        acceptor_.bind(ep, ec);
        if (ec) { std::cerr << "acceptor bind: " << ec.message() << std::endl; return; }
        acceptor_.listen(net::socket_base::max_listen_connections, ec);
        if (ec) { std::cerr << "acceptor listen: " << ec.message() << std::endl; return; }
    }

    void run() { do_accept(); }

    void stop() {
        beast::error_code ec;
        acceptor_.close(ec);
    }

private:
    void do_accept() {
        auto socket = std::make_shared<tcp::socket>(ioc_);
        acceptor_.async_accept(*socket, [this, socket](beast::error_code ec) {
            if (!ec) {
                std::make_shared<HttpSession>(std::move(*socket), pm_, client_)->run();
            }
            if (!shutdown_requested.load()) {
                do_accept();
            }
        });
    }

    class HttpSession : public std::enable_shared_from_this<HttpSession> {
    public:
        HttpSession(tcp::socket socket,
                    std::shared_ptr<PortfolioManager> pm,
                    std::shared_ptr<AlphaVantageClient> client)
            : stream_(std::move(socket)), pm_(std::move(pm)), client_(std::move(client)) {}

        void run() { do_read(); }

    private:
        void do_read() {
            req_ = {};
            stream_.expires_after(std::chrono::seconds(30));
            http::async_read(stream_, buffer_, req_,
                [self = shared_from_this()](beast::error_code ec, std::size_t) {
                    if (!ec) self->handle_request();
                });
        }

        void handle_request() {
            auto const make_response = [this](http::status status, std::string body, std::string content_type) {
                http::response<http::string_body> res{status, req_.version()};
                res.set(http::field::content_type, content_type);
                res.set(http::field::access_control_allow_origin, "*");
                res.keep_alive(req_.keep_alive());
                res.body() = std::move(body);
                res.prepare_payload();
                return res;
            };

            auto const json_response = [&](nlohmann::json j) {
                return make_response(http::status::ok, j.dump(), "application/json");
            };

            auto const bad_request = [&](std::string what) {
                return make_response(http::status::bad_request, what, "text/plain");
            };

            http::response<http::string_body> res;

            if (req_.method() == http::verb::get) {
                std::string target(req_.target());

                if (target == "/") {
                    res = make_response(http::status::ok, HTML_PAGE, "text/html");
                } else if (target == "/api/stream") {
                    // SSE streaming - move stream to SSESession
                    auto sse = std::make_shared<SSESession>(std::move(stream_), pm_);
                    sse->start();
                    return; // Don't go through normal do_write
                } else if (target == "/api/dashboard") {
                    res = json_response(pm_->to_json());
                } else if (target == "/api/status") {
                    auto syms = client_->get_symbols();
                    nlohmann::json j = {
                        {"paused", client_->is_paused()},
                        {"positions", pm_->to_json()["positions_total"]},
                        {"symbols", syms.size()},
                        {"active_symbols", syms}
                    };
                    res = json_response(j);
                } else if (target == "/api/stocks") {
                    res = json_response(STOCK_DB);
                } else if (target == "/api/countries") {
                    res = json_response(get_countries_from_db());
                } else if (target == "/api/export/json") {
                    res = json_response(pm_->export_json());
                } else if (target == "/api/export/csv") {
                    std::string csv = pm_->export_csv();
                    http::response<http::string_body> r{http::status::ok, req_.version()};
                    r.set(http::field::content_type, "text/csv");
                    r.set(http::field::content_disposition, "attachment; filename=portfolio.csv");
                    r.set(http::field::access_control_allow_origin, "*");
                    r.keep_alive(req_.keep_alive());
                    r.body() = std::move(csv);
                    r.prepare_payload();
                    res = std::move(r);
                } else if (target.find("/api/chain?ticker=") == 0) {
                    std::string ticker = target.substr(18);
                    res = json_response(pm_->chain_for_ticker(ticker));
                } else if (target.find("/api/chain/") == 0) {
                        std::string ticker = target.substr(11);
                        res = json_response(pm_->chain_for_ticker(ticker));
                } else if (target == "/api/strategies") {
                    nlohmann::json arr = nlohmann::json::array();
                    arr.push_back("Covered Call"); arr.push_back("Protective Put");
                    arr.push_back("Straddle"); arr.push_back("Strangle");
                    arr.push_back("Bull Call Spread"); arr.push_back("Bear Put Spread");
                    arr.push_back("Butterfly"); arr.push_back("Iron Condor");
                    res = json_response(arr);
                } else if (target == "/api/strategies/current") {
                    std::lock_guard<std::mutex> lock(strategies_mutex);
                    nlohmann::json jarr = nlohmann::json::array();
                    for (const auto& s : user_strategies) {
                        try { jarr.push_back(StrategyBuilder::to_json(s)); } catch(...) {}
                    }
                    res = json_response(jarr);
                } else if (target == "/api/pl-diagram") {
                    auto dash = pm_->to_json();
                    nlohmann::json positions = dash["recent_positions"];
                    double spot_min = 1e9, spot_max = 0;
                    for (const auto& p : positions) {
                        double s = p["spot"].get<double>();
                        spot_min = std::min(spot_min, s * 0.5);
                        spot_max = std::max(spot_max, s * 1.5);
                    }
                    if (spot_min > spot_max) { spot_min = 50; spot_max = 200; }
                    double step = (spot_max - spot_min) / 100;
                    nlohmann::json points = nlohmann::json::array();
                    for (int i = 0; i <= 100; ++i) {
                        double S = spot_min + i * step;
                        double pnl = 0;
                        for (const auto& p : positions) {
                            double strike = p["strike"].get<double>();
                            double mc_price = p["mc_price"].get<double>();
                            std::string type = p["type"].get<std::string>();
                            double intrinsic = (type == "CALL")
                                ? std::max(S - strike, 0.0)
                                : std::max(strike - S, 0.0);
                            pnl += intrinsic - mc_price;
                        }
                        points.push_back({{"spot", S}, {"pnl", pnl}});
                    }
                    res = json_response(points);
                } else {
                    res = make_response(http::status::not_found, "Not found", "text/plain");
                }
            } else if (req_.method() == http::verb::post) {
                std::string target(req_.target());
                if (target == "/api/pause") {
                    client_->pause();
                    res = json_response({{"status", "ok"}, {"paused", true}});
                } else if (target == "/api/resume") {
                    client_->resume();
                    res = json_response({{"status", "ok"}, {"paused", false}});
                } else if (target == "/api/reset") {
                    client_->request_reset();
                    pm_->reset();
                    res = json_response({{"status", "ok"}, {"reset", true}});
                } else if (target == "/api/configure") {
                    try {
                        auto body = nlohmann::json::parse(req_.body());
                        if (!body.contains("symbols") || !body["symbols"].is_array()) {
                            res = bad_request("Missing 'symbols' array");
                        } else {
                            std::vector<std::string> symbols;
                            for (const auto& s : body["symbols"]) {
                                symbols.push_back(s.get<std::string>());
                            }
                            client_->set_symbols(symbols);
                            client_->request_reset();
                            pm_->reset();
                            res = json_response({{"status", "ok"}, {"symbols", symbols}});
                        }
                    } catch (const std::exception& e) {
                        res = bad_request(std::string("Bad request: ") + e.what());
                    }
                } else if (target == "/api/strategy/build") {
                    auto build_strategy = [&]() -> std::optional<StrategyDef> {
                        try {
                            auto body = nlohmann::json::parse(req_.body());
                            std::string type = body["type"].get<std::string>();
                            std::string ticker = body["ticker"].get<std::string>();
                            double spot = body["spot"].get<double>();
                            double expiry = body["expiry"].get<double>();
                            if (type == "Covered Call")
                                return StrategyBuilder::covered_call(ticker, spot, body["strike"].get<double>(), expiry);
                            if (type == "Protective Put")
                                return StrategyBuilder::protective_put(ticker, spot, body["strike"].get<double>(), expiry);
                            if (type == "Straddle")
                                return StrategyBuilder::straddle(ticker, spot, body["strike"].get<double>(), expiry);
                            if (type == "Strangle")
                                return StrategyBuilder::strangle(ticker, spot, body["low_strike"].get<double>(), body["high_strike"].get<double>(), expiry);
                            if (type == "Bull Call Spread")
                                return StrategyBuilder::bull_call_spread(ticker, spot, body["low_strike"].get<double>(), body["high_strike"].get<double>(), expiry);
                            if (type == "Bear Put Spread")
                                return StrategyBuilder::bear_put_spread(ticker, spot, body["low_strike"].get<double>(), body["high_strike"].get<double>(), expiry);
                            if (type == "Butterfly")
                                return StrategyBuilder::butterfly(ticker, spot, body["low_strike"].get<double>(), body["mid_strike"].get<double>(), body["high_strike"].get<double>(), expiry);
                            if (type == "Iron Condor")
                                return StrategyBuilder::iron_condor(ticker, spot, body["put_low"].get<double>(), body["put_high"].get<double>(), body["call_low"].get<double>(), body["call_high"].get<double>(), expiry);
                            return std::nullopt;
                        } catch (...) { return std::nullopt; }
                    }();
                    if (!build_strategy.has_value()) {
                        res = bad_request("Unknown strategy type or bad request");
                    } else {
                        { std::lock_guard<std::mutex> lock(strategies_mutex); user_strategies.push_back(*build_strategy); }
                        res = json_response(StrategyBuilder::to_json(*build_strategy));
                    }
                } else if (target == "/api/backtest") {
                    try {
                        auto body = nlohmann::json::parse(req_.body());
                        std::string ticker = body["ticker"].get<std::string>();
                        double spot = body["spot"].get<double>();
                        double strike = body["strike"].get<double>();
                        double expiry = body["expiry"].get<double>();
                        OptionType type = body["type"].get<std::string>() == "CALL" ? OptionType::CALL : OptionType::PUT;
                        int paths = body.value("paths", 10000);
                        nlohmann::json results = nlohmann::json::array();
                        BlackScholesEngine bs_engine;
                        MonteCarloEngine mc_engine(paths);

                        std::vector<double> vol_range = {0.15, 0.20, 0.25, 0.30, 0.35, 0.40};
                        for (double v : vol_range) {
                            double bs_price = StrategyBuilder::bs_price(spot, strike, expiry, 0.04, v, type);
                            MarketTick vt{ticker, "", spot, strike, expiry, 0.04, bs_price, type};
                            auto mc_g = mc_engine.calculate(vt);
                            auto bs_g = bs_engine.calculate(vt);
                            results.push_back({
                                {"vol", v}, {"mc_price", mc_g.price}, {"bs_price", bs_g.price},
                                {"mc_delta", mc_g.delta}, {"bs_delta", bs_g.delta},
                                {"mc_iv", mc_g.impliedVol}, {"bs_iv", bs_g.impliedVol}
                            });
                        }
                        res = json_response({{"ticker", ticker}, {"spot", spot}, {"strike", strike},
                                             {"results", results}});
                    } catch (const std::exception& e) {
                        res = bad_request(std::string("Bad request: ") + e.what());
                    }
                } else if (target == "/api/strategies/clear") {
                    { std::lock_guard<std::mutex> lock(strategies_mutex); user_strategies.clear(); }
                    res = json_response({{"status", "ok"}, {"cleared", true}});
                } else if (target == "/api/convergence") {
                    try {
                        auto body = nlohmann::json::parse(req_.body());
                        double S = body["spot"].get<double>();
                        double K = body["strike"].get<double>();
                        double T = body["expiry"].get<double>();
                        double r = body["rate"].get<double>();
                        double sigma = body["vol"].get<double>();
                        OptionType type = body["type"].get<std::string>() == "CALL" ? OptionType::CALL : OptionType::PUT;
                        unsigned int max_paths = body.value("max_paths", 100000);
                        MonteCarloEngine mc_conv(max_paths);
                        auto points = mc_conv.convergence(S, K, T, r, sigma, type, max_paths);
                        nlohmann::json arr = nlohmann::json::array();
                        for (const auto& p : points) {
                            arr.push_back({
                                {"paths", p.paths},
                                {"bs_price", p.bs_price},
                                {"mc_price", p.mc_price},
                                {"error", p.error},
                                {"std_error", p.std_error}
                            });
                        }
                        res = json_response(arr);
                    } catch (const std::exception& e) {
                        res = bad_request(std::string("Convergence error: ") + e.what());
                    }
                } else if (target == "/api/exotic") {
                    try {
                        auto body = nlohmann::json::parse(req_.body());
                        std::string style = body["style"].get<std::string>();
                        std::string ticker = body.value("ticker", "");
                        double S = body["spot"].get<double>();
                        double K = body["strike"].get<double>();
                        double T = body["expiry"].get<double>();
                        double r = body["rate"].get<double>();
                        double sigma = body["vol"].get<double>();
                        OptionType type = body["type"].get<std::string>() == "CALL" ? OptionType::CALL : OptionType::PUT;

                        MonteCarloEngine mc_exotic(100000);
                        double price = 0.0, delta = 0.0;

                        if (style == "asian") {
                            auto g = mc_exotic.calculate_asian(S, K, T, r, sigma, type, 52);
                            price = g.price;
                            delta = g.delta;
                        } else if (style == "barrier") {
                            double barrier = body["barrier"].get<double>();
                            bool down_and_out = body.value("down_and_out", true);
                            auto g = mc_exotic.calculate_barrier(S, K, T, r, sigma, type, barrier, down_and_out);
                            price = g.price;
                            delta = g.delta;
                        } else if (style == "lookback") {
                            auto g = mc_exotic.calculate_lookback(S, K, T, r, sigma, type);
                            price = g.price;
                            delta = g.delta;
                        }

                        res = json_response({
                            {"style", style}, {"ticker", ticker},
                            {"price", price}, {"delta", delta},
                            {"spot", S}, {"strike", K}, {"expiry", T}
                        });
                    } catch (const std::exception& e) {
                        res = bad_request(std::string("Exotic pricing error: ") + e.what());
                    }
                } else {
                    res = make_response(http::status::not_found, "Not found", "text/plain");
                }
            } else {
                res = bad_request("Unknown HTTP method");
            }

            do_write(std::move(res));
        }

        void do_write(http::response<http::string_body> res) {
            auto sp = std::make_shared<http::response<http::string_body>>(std::move(res));
            http::async_write(stream_, *sp,
                [self = shared_from_this(), sp](beast::error_code, std::size_t) {
                    beast::error_code ignored;
                    self->stream_.socket().shutdown(tcp::socket::shutdown_both, ignored);
                    self->stream_.close();
                });
        }

        beast::tcp_stream stream_;
        beast::flat_buffer buffer_;
        http::request<http::string_body> req_;
        std::shared_ptr<PortfolioManager> pm_;
        std::shared_ptr<AlphaVantageClient> client_;
    };

    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    std::shared_ptr<PortfolioManager> pm_;
    std::shared_ptr<AlphaVantageClient> client_;
};

// ── Main ──
int main(int argc, char* argv[]) {
    AppConfig config = AppConfig::from_cli(argc, argv);
    std::string api_key = (argc > 1) ? argv[1] : "demo";
    int port = static_cast<int>(config.port);

    std::signal(SIGINT,  signal_handler);
    std::signal(SIGTERM, signal_handler);

    unsigned int num_workers = std::max(1u, std::thread::hardware_concurrency());

    std::cout << "=== Options Pricing Platform v2.0 (Web) ===" << std::endl;
    std::cout << "Symbols: ";
    for (size_t i = 0; i < config.symbols.size(); ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << config.symbols[i];
    }
    std::cout << std::endl;
    std::cout << "MC paths: " << config.mc_paths << std::endl;
    std::cout << "Dashboard: http://localhost:" << port << std::endl;
    std::cout << "API:       http://localhost:" << port << "/api/dashboard" << std::endl;
    std::cout << "Press Ctrl+C to stop." << std::endl;

    auto queue     = std::make_shared<ThreadSafeQueue<MarketTick>>();
    auto pm        = std::make_shared<PortfolioManager>();
    auto bs_engine = std::make_shared<BlackScholesEngine>();
    auto mc_engine = std::make_shared<MonteCarloEngine>(config.mc_paths);
    auto client    = std::make_shared<AlphaVantageClient>(config.symbols, api_key, config, queue);

    net::io_context ioc{1};

    auto server = std::make_shared<HttpServer>(
        ioc, tcp::endpoint(tcp::v4(), static_cast<unsigned short>(port)), pm, client);
    std::clog << "Server starting on port " << port << std::endl;
    server->run();

    std::vector<std::thread> workers;

    client->start();

    for (unsigned int i = 0; i < num_workers; ++i) {
        workers.emplace_back([queue, bs = bs_engine.get(), mc = mc_engine.get(), pm]() {
            while (!shutdown_requested.load()) {
                MarketTick tick = queue->wait_and_pop();
                if (shutdown_requested.load()) break;
                if (tick.ticker.empty()) continue;
                Greeks mc_g = mc->calculate(tick);
                Greeks bs_g = bs->calculate(tick);
                pm->update(mc_g, bs_g, tick);
            }
        });
    }

    std::thread ioc_thread([&ioc]() {
        ioc.run();
    });

    while (!shutdown_requested.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "\nShutting down gracefully..." << std::endl;
    ioc.stop();
    server->stop();
    queue->shutdown();
    client->stop();
    pm->stop();

    if (ioc_thread.joinable()) ioc_thread.join();

    for (auto& w : workers) {
        if (w.joinable()) w.join();
    }

    std::cout << "Platform stopped." << std::endl;
    return 0;
}
