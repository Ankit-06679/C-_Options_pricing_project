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

COPY --from=builder /src/build/options_pricer /usr/local/bin/options_pricer

EXPOSE 7860

ENV API_KEY="demo"
ENV SYMBOLS="AAPL"

CMD options_pricer "$API_KEY" $SYMBOLS --port 7860
