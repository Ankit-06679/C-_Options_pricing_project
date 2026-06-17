#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <cstring>

struct AppConfig {
    std::vector<std::string> symbols          = {"AAPL", "MSFT", "GOOGL", "AMZN", "TSLA"};
    std::vector<double>      strike_pct        = {0.95, 0.975, 1.0, 1.025, 1.05};
    std::vector<double>      expiries           = {0.0833, 0.25, 0.5};
    bool                     use_calls          = true;
    bool                     use_puts           = true;
    unsigned int             mc_paths           = 100000;
    unsigned int             rate_limit_sec     = 12;
    unsigned int             port               = 8080;

    static AppConfig from_cli(int argc, char* argv[]) {
        AppConfig cfg;
        cfg.symbols.clear();

        for (int i = 2; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--mc-paths" && i + 1 < argc) {
                cfg.mc_paths = static_cast<unsigned int>(std::stoul(argv[++i]));
            } else if (arg == "--rate-limit" && i + 1 < argc) {
                cfg.rate_limit_sec = static_cast<unsigned int>(std::stoul(argv[++i]));
            } else if (arg == "--port" && i + 1 < argc) {
                cfg.port = static_cast<unsigned int>(std::stoul(argv[++i]));
            } else if (arg == "--calls-only") {
                cfg.use_calls = true;
                cfg.use_puts  = false;
            } else if (arg == "--puts-only") {
                cfg.use_calls = false;
                cfg.use_puts  = true;
            } else if (arg == "--help") {
                std::cout << "Usage: options_pricer.exe API_KEY [SYMBOLS...] [options]\n"
                          << "Options:\n"
                          << "  --mc-paths N    Monte Carlo paths (default: 100000)\n"
                          << "  --rate-limit S  API rate limit in seconds (default: 12)\n"
                          << "  --port P        HTTP server port (default: 8080)\n"
                          << "  --calls-only    Only generate CALL options\n"
                          << "  --puts-only     Only generate PUT options\n"
                          << "  --help          Show this help\n"
                          << "Examples:\n"
                          << "  options_pricer.exe API_KEY AAPL MSFT GOOGL\n"
                          << "  options_pricer.exe API_KEY AAPL --mc-paths 50000 --port 9090\n";
                std::exit(0);
            } else if (!arg.empty() && arg[0] != '-') {
                cfg.symbols.push_back(arg);
            }
        }
        if (cfg.symbols.empty()) cfg.symbols = {"AAPL"};
        return cfg;
    }
};
