cmake_minimum_required(VERSION 3.20)
project(OptionsPricingPlatform VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/W4 /O2)
else()
    add_compile_options(-Wall -Wextra -O3)
endif()

find_package(Boost REQUIRED COMPONENTS system)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

include(FetchContent)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)

set(CROW_USE_BOOST ON CACHE BOOL "Use Boost Asio")
FetchContent_MakeAvailable(nlohmann_json)

add_executable(options_pricer
    main.cpp
    AlphaVantageClient.cpp
    PricingEngine.cpp
    PortfolioManager.cpp
    Strategy.cpp
)

target_include_directories(options_pricer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(options_pricer PRIVATE
    Boost::system
    OpenSSL::SSL
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
    Threads::Threads
    ws2_32
    wsock32
)
