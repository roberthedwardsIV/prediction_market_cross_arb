cmake_minimum_required(VERSION 3.16)
project(assisi LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

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
  src/kalshi_ws.cpp
  src/kalshi_order.cpp
  src/kalshi_http.cpp
  src/polymarket_ws.cpp
  src/polymarket_order.cpp
  src/polymarket_http.cpp
  src/gemini_ws.cpp
  src/gemini_order.cpp
  src/gemini_http.cpp
  src/monitor.cpp
)
target_link_libraries(assisi PRIVATE
  ixwebsocket
  OpenSSL::Crypto
  nlohmann_json::nlohmann_json
  Threads::Threads
)

add_executable(kalshi_feed src/kalshi_feed.cpp src/kalshi_ws.cpp)
target_link_libraries(kalshi_feed PRIVATE
  ixwebsocket
  OpenSSL::Crypto
  nlohmann_json::nlohmann_json
)
