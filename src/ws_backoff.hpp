#pragma once

// 1s, 2s, 4s, ... capped at 30s.
inline int nextWsBackoffMs(int current_ms) {
    if (current_ms < 1000) return 1000;
    if (current_ms >= 30000) return 30000;
    int next = current_ms * 2;
    return next > 30000 ? 30000 : next;
}
