# ── Build Stage ──
FROM gcc:14.2-bookworm AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake libboost-system-dev libssl-dev ca-certificates git \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src

COPY CMakeLists.txt ./
COPY Common.hpp ThreadSafeQueue.hpp Config.hpp \
     AlphaVantageClient.hpp AlphaVantageClient.cpp \
     PricingEngine.hpp PricingEngine.cpp \
     PortfolioManager.hpp PortfolioManager.cpp \
     Strategy.hpp Strategy.cpp \
     main.cpp ./

RUN mkdir build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    cmake --build . --parallel 2

# ── Runtime Stage ──
FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
    libboost-system1.74.0 libssl3 ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Copy libstdc++ from builder (GCC 14.2) since bookworm has GCC 12.2
COPY --from=builder /usr/local/lib64/libstdc++.so.6* /usr/lib/x86_64-linux-gnu/
COPY --from=builder /src/build/options_pricer /usr/local/bin/options_pricer

EXPOSE 7860

ENV API_KEY="demo"
ENV SYMBOLS="AAPL"

CMD options_pricer "$API_KEY" $SYMBOLS --port 7860 --mc-paths 5000
