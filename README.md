# Prediction Markets Cross Exchange Arbitrage Engine

Assisi watches Kalshi, Polymarket (US), and Gemini prediction-market books for yes+no pairs that still pay under $1 after taker fees. Paper and replay are the default; live orders require `ASSISI_LIVE=1`.

## Latency

On-box only. `std::chrono::steady_clock` from the websocket callback (book update arrived) to strategy intent (about to send or paper-fill). Summarized as p50 / p99 over a 128-sample ring.

Not in that number: HTTP / order POST, NIC, kernel, or exchange matching. Live sends are handed to a detached thread after intent.

This is not exchange-to-exchange latency and not HFT.

## Position Sizing

Paper / replay: if cheapest yes + cheapest no + taker fees is under $1, size scales with that edge times venue cash, capped at 50, limited by both books, and cut until it fits cash.

Live (`ASSISI_LIVE=1`): pair size is hardcoded to 1. Completing an unmatched leg is also size 1.

There is no inventory, volatility, or drawdown sizer. That is a known limit, not a feature.

## Build & Run Instructions

Needs CMake, C++17, OpenSSL, and network on first configure (fetches ixwebsocket and nlohmann/json).

Commands:
- `cmake -S . -B build && cmake --build build`
- `cp markets_sample.csv markets.csv`
- `./build/assisi --clock`
- `./build/assisi --replay ticks_replay_sample.csv`

`--clock` = feeds, no orders. `--replay` = tape, paper, then exit. `--trade` = feeds; live still gated by `ASSISI_LIVE`. `--record [path]` writes a tape (default `ticks.csv`). No flags = wait on `http://127.0.0.1:8787` until START. The process reads `markets.csv` in the cwd, not the sample file.

## Market Universe & Linking

The process reads `markets.csv` at start (copy `markets_sample.csv`). One row per internal market:

`market_id,expiration,kal_id,pm_id,gem_id`

`expiration` is Unix seconds UTC. Kalshi ticker, Polymarket slug, Gemini `instrumentSymbol`; use `0` if that venue is absent. Max 16 rows. You decide that the three ids are the same contract. Duplicate venue ids collide. Expired rows stop trading without a restart; they stay in the file until you delete them.

## Live vs. Paper vs. Replay

| Mode | How | Orders |
| --- | --- | --- |
| Replay | `--replay ticks_replay_sample.csv` | Never. Paper fills on a recorded tape, then exit. |
| Clock | `--clock` | Never. Live public feeds, no sends. |
| Paper | `--trade` (or START in the UI) with `ASSISI_LIVE` unset | Simulated fills only. |
| Live | `--trade` and `ASSISI_LIVE=1` | Real REST orders. Pair size 1. |

Live is off unless that env var is exactly `1`. Replay ignores it.

Keys are read from `.env` in the working directory (not in this repo). Names only: `KALSHI_API_KEY`, `KALSHI_PRIVATE_KEY_PATH`, `KALSHI_ENV` (`prod` vs demo), `POLYMARKET_KEY_ID`, `POLYMARKET_SECRET_KEY`, `GEMINI_API_KEY`, `GEMINI_API_SECRET`, optional `GEMINI_ENV=sandbox`.

## Layout

- `src/main.cpp` — wire-up, flags, paper vs live send, replay loop
- `src/strategy.hpp` — cross-book yes+no, fees, paper size
- `src/latency.hpp` / `src/tape.hpp` / `src/portfolio.hpp` — clocks, record/replay, positions
- `src/*_ws.cpp` / `*_http.cpp` / `*_order.cpp` — Kalshi, Polymarket, Gemini
- `src/monitor.cpp` + `ui/` — local control page on `:8787`
- `markets_sample.csv` / `ticks_replay_sample.csv` — clone-able universe and tape
- `CMakeLists.txt` — `assisi` binary (ixwebsocket). Header-only pieces are included from `main.cpp`.

## Known Limitations

- Public websockets, no kernel bypass, consumer hardware. On-box intent time is not exchange RTT.
- Live pair size is 1. Paper size is a cap-50 cash/book heuristic, not a risk model.
- Venue linking is manual: you assert that a Kalshi ticker, Polymarket slug, and Gemini symbol are the same contract.
- Live sends run on a detached thread after intent. No guaranteed two-leg atomic fill; one leg can miss.
- Universe is `markets.csv`, max 16 rows, no hot reload.





