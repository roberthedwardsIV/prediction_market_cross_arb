#pragma once

#include "ws_backoff.hpp"

#include <chrono>
#include <thread>
#include <ixwebsocket/IXWebSocket.h>

inline void waitUntilWsClosed(ix::WebSocket& ws) {
    while (ws.getReadyState() != ix::ReadyState::Closed) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}
