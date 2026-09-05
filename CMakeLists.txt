cmake_minimum_required(VERSION 3.16)
project(assisi LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

include(FetchContent)
set(USE_TLS ON CACHE BOOL "TLS for wss://" FORCE)
FetchContent_Declare(
  ixwebsocket
  GIT_REPOSITORY https://github.com/machinezone/IXWebSocket.git
  GIT_TAG        v12.0.1
)
FetchContent_MakeAvailable(ixwebsocket)

FetchContent_Declare(
  json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(json)

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

add_executable(assisi
  src/main.cpp
  src/kalshi/kalshi_ws.cpp
  src/kalshi/kalshi_order.cpp
  src/kalshi/kalshi_http.cpp
  src/polymarket/polymarket_ws.cpp
  src/polymarket/polymarket_order.cpp
  src/polymarket/polymarket_http.cpp
  src/gemini/gemini_ws.cpp
  src/gemini/gemini_order.cpp
  src/gemini/gemini_http.cpp
  src/monitor.cpp
)
target_link_libraries(assisi PRIVATE
  ixwebsocket
  OpenSSL::Crypto
  nlohmann_json::nlohmann_json
  Threads::Threads
)

target_include_directories(assisi PRIVATE src src/kalshi src/polymarket src/gemini)

enable_testing()

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(assisi_tests
  tests/smoke_test.cpp
  tests/test_fees.cpp
  tests/test_risk.cpp
  tests/test_portfolio.cpp
  tests/test_tape.cpp
  tests/test_strategy.cpp
  tests/test_ws_parse.cpp
)
target_include_directories(assisi_tests PRIVATE src)
target_compile_definitions(assisi_tests PRIVATE ASSISI_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

target_link_libraries(assisi_tests PRIVATE
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(assisi_tests)

add_executable(bench src/bench.cpp)
target_include_directories(bench PRIVATE src)