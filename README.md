# Assisi — Prediction Market Cross-Exchange Arbitrage Engine

[![ci](https://github.com/roberthedwardsIV/prediction_market_cross_arb/actions/workflows/ci.yml/badge.svg)](https://github.com/roberthedwardsIV/prediction_market_cross_arb/actions/workflows/ci.yml)

Assisi watches Kalshi, Polymarket, and Gemini prediction-market books for yes+no pairs that still pay under $1 after taker fees. C++17, ixwebsocket, no kernel bypass. Paper and replay are the default; live orders require `ASSISI_LIVE=1`.

## Quick Start

Needs CMake, a C++17 compiler, OpenSSL, and network on first configure (fetches ixwebsocket, nlohmann/json, googletest).

```
cmake -S . -B build && cmake --build build
```

Default build type is **Release** (`-O3`). Override with `-DCMAKE_BUILD_TYPE=Debug` if you need symbols.

```
cp markets_sample.csv markets.csv
./build/assisi_tests
./build/assisi --replay ticks_replay_sample.csv
./build/bench
```

Run from the repo root. The process reads `markets.csv` and `ui/index.html` relative to the working directory.

## Modes

| Mode | CLI | UI | Orders |
| --- | --- | --- | --- |
| Replay | `--replay <tape.csv>` | Mode → Replay, pick a tape, Start | Never. Paper fills on a recorded tape. |
| Clock | `--clock` | Mode → Clock, Start | Never. Live public feeds, no sends, even if `ASSISI_LIVE=1`. |
| Paper | `--trade` with `ASSISI_LIVE` unset | Mode → Paper, Start | Simulated fills only. |
| Live | `--trade` with `ASSISI_LIVE=1` | Mode → Live, Start | Real REST orders. Same sizing path as paper. |

No flags = start the control page on `http://127.0.0.1:8787` and wait. `--record [path]` writes a tape while feeds run (default `ticks.csv`).

Live is off unless `ASSISI_LIVE` is exactly `1` at launch. The UI cannot enable it; without that env var the Live option is disabled and the server returns 403. `--clock` wins over `ASSISI_LIVE` and never places an order. Stop on the page refuses new pairs (same halt as the kill switch). After a UI replay the portfolio holds paper fills, so Paper / Clock / Live are refused (409) until the process restarts.

Keys are read from `.env` in the working directory (not in this repo; see `.env.example`). Names only: `KALSHI_API_KEY`, `KALSHI_PRIVATE_KEY_PATH`, `KALSHI_ENV` (`prod` vs demo), `POLYMARKET_KEY_ID`, `POLYMARKET_SECRET_KEY`, `GEMINI_API_KEY`, `GEMINI_API_SECRET`, optional `GEMINI_ACCOUNT` (defaults to `primary`) and `GEMINI_ENV=sandbox`.

## How it decides size

Same path for paper, replay, and live.

1. **Strategy** — If cheapest yes + cheapest no + taker fees at **n = 1** is under 100 cents, propose `size = 70%` of the thinner top-of-book ask (`min(yes_ask_n, no_ask_n)`), same on both legs. Below one contract → no order. (Kalshi’s ceil fee at n = 1 overestimates per-contract cost for larger n, so this screen is conservative on that venue.) Prices, cash, and fees are **integer cents** (`src/money.hpp`). Wire/tape dollars are parsed on the way in.
2. **Risk** — Shrinks both legs together (closed-form per limit, then a cent-rounding guard) against, in order: per-market contract cap, venue cash (gross − in-flight reserved), venue reserve, total allocation, per-market equity share. Then **rechecks** `cost(yes) + cost(no) < size` at the final n; shrinks until it holds or rejects `BelowMinEdge`. Every reject is logged with a `RiskReason`.
3. **Live only** — One busy flag per market: House GOP and Senate GOP can both send in the same second; the same market cannot start a second job while the first is still going. Before the send, we set aside that market’s cash on each exchange and give it back when the job ends (at the pre-chase prices). Two markets still cannot spend the same Kalshi (or Polymarket, or Gemini) dollars twice. Kalshi and Polymarket request clocks bump if two signs land in the same millisecond (Gemini already did this). The control page shows `Working (N)`. Paper and replay do not set aside cash and do not use the busy flags.
4. **Second-leg chase, then flatten** — If the second buy misses, bump that buy +$0.01 only up to a chase ceiling. We do not buy a larger missing side later. If the pair is still one-sided (live or paper), sell the leftover at 1 cent. Kalshi leftover NO is closed by buying YES at up to 99 cents (Kalshi is a yes book). If that dump misses, the log says we are still directional.
5. **Fills** — Venue responses must report a fill count; missing/zero count is no fill (parsers never invent `idea.size`). Paper fills come only from the local `Execute` path. Buys debit cash and raise counts; sells credit cash and drop counts (Kalshi NO flatten spends the YES price plus fee). Fills for unregistered markets are rejected and logged (`Unregistered`), with no cash change.
6. **Kill switch** — Cumulative taker fees on applied fills trip `max_drawdown` (default $10). Sticky until process restart; also sets the UI halt. Manual Stop does the same halt.

Cash is venue-derived only: Kalshi + Polymarket + Gemini (HTTP-refreshed live; seeded at $100 each in replay). No separate internal ledger.

### Risk env (`ASSISI_RISK_*`)

Resolved at startup and logged (`risk limits ...`). Defaults match the values below if unset.

| Variable | Default | Meaning |
| --- | --- | --- |
| `ASSISI_RISK_MAX_MARKET_PCT` | `0.70` or `70` | Max share of total equity in one market |
| `ASSISI_RISK_MAX_ALLOCATION_PCT` | `0.80` or `80` | Max (exposure + new spend) / cash |
| `ASSISI_RISK_VENUE_RESERVE_PCT` | `0.10` or `10` | Keep this fraction of venue equity unspent |
| `ASSISI_RISK_MAX_CONTRACTS` | `100` | Cap on yes+no contracts per market |
| `ASSISI_RISK_CHASE_RESERVE` | `0.01` | Dollars of edge to keep when chasing (stored as 1 cent) |
| `ASSISI_RISK_MAX_DRAWDOWN` | `10.0` | Kill switch on cumulative fees (stored as 1000 cents) |

## Latency

On-box only. `std::chrono::steady_clock` from the websocket callback (book update arrived) to strategy intent (about to send or paper-fill). Hot-path logging is async (background thread); intent stamps do not block on `localtime` / console flush. Summarized as p50 / p99 over a 128-sample ring and written to `latency.csv` after replay.

Not in that number: HTTP / order POST, NIC, kernel, or exchange matching. Live sends are handed to a detached thread after intent.

This is not exchange-to-exchange latency and not HFT.

## Benchmark

`./build/bench` loads `ticks_replay_sample.csv` and `markets.csv`, then times arrival → `price_update` → `strategize` → intent per tick in nanoseconds. Output is `bench_latency.csv` (p50 / p99 / n). Reflects local CPU timing on the sample tape, not exchange RTT.

Local result (Intel i5-8257U laptop, macOS, Release): bench p50 ≈ 183 ns, p99 ≈ 199 ns, n = 5442; replay sample tape p50 ≈ 350 ns, p99 ≈ 370–400 ns.

## Market Universe & Linking

`markets.csv`, one row per internal market:

`market_id,expiration,kal_id,pm_id,gem_id`

`expiration` is Unix seconds UTC. Kalshi ticker, Polymarket slug, Gemini `instrumentSymbol`; use `0` if that venue is absent. Max 16 rows, registered at startup only. You decide that the three ids are the same contract. Duplicate venue ids collide. Expired rows stop trading without a restart; they stay in the file until you delete them.

## Tests

gtest (fees, strategy, risk, portfolio, tape, busy flags, request clocks, reconnect backoff, WS JSON fixtures). Risk covers shrink-to-fit per venue, two markets reserving at once, reservation vs double-spend, chase ceiling, final-size edge, kill switch, and `ASSISI_RISK_*` loading. Portfolio covers sell cash-back and Kalshi NO cover. `tests/fixtures/` holds raw Kalshi/Polymarket book JSON so nested Polymarket qty parsing cannot silently regress to inventing size 1. GitHub Actions builds `assisi` and `assisi_tests` on Ubuntu and runs the suite on every push.

## Layout

- `src/main.cpp` — wire-up, flags, paper vs live send, replay, flatten leftover, fill apply
- `src/money.hpp` — integer cents parse/format; no `float` on the money path
- `src/strategy.hpp` — cross-book yes+no, fees at n=1, 70% book participation
- `src/live_busy.hpp` — one busy flag per market (max 16)
- `src/ws_backoff.hpp` / `src/ws_reconnect.hpp` — wait-and-retry for dropped price feeds
- `src/risk.hpp` — limits, fit-to-cap, final-size edge, reserve/release, chase ceiling, kill switch, env load
- `src/portfolio.hpp` — positions, buys and sells, venue cash / exposure / reserved
- `src/parse_book.hpp` / `src/json_find.hpp` — WS book/ticker parse (shared with tests)
- `src/latency.hpp` / `src/tape.hpp` — clocks, tape record and parse
- `src/market_data.hpp` / `src/execution.hpp` — book state (slot maps), paper fills
- `src/kalshi/` / `src/polymarket/` / `src/gemini/` — per-venue websocket, REST, orders
- `src/monitor.cpp` + `ui/index.html` — local control page on `:8787`
- `src/bench.cpp` — hot-path benchmark
- `tests/` + `tests/fixtures/` — gtest + raw JSON fixtures
- `markets_sample.csv` / `ticks_replay_sample.csv` — clone-able universe and tape

## Limitations

- **One live send per market** — Different markets can send together. The same market cannot. Cash set-aside is still shared across markets on the same exchange.
- **Feeds reconnect** — If a price feed drops, we wait 1s then 2s, 4s, … up to 30s and connect again. Kalshi and Polymarket sign a fresh clock each try (an old signature would be rejected). On open we subscribe again.
- **Top-of-book only** — Sizing uses displayed ask size × 70%, not deeper book.
- **Half fill is sold, not chased** — If only one side of a pair fills, we dump that leftover at 1 cent (a market-style take). Kalshi leftover NO is closed by buying YES at up to 99 cents (Kalshi is a yes book). If that dump misses, the log says we are still directional.
- **Kill switch is fee-based** — Trips on cumulative taker fees vs `max_drawdown`, not mark-to-market P&L (cash + cost-basis exposure is flat on fills).
- **No volatility / inventory sizer** — Caps are contract, cash, reserve, allocation, and market %.
- **Integer cents, not a settlement ledger** — Prices, cash, fees, and averages are `int` cents. Venue JSON is still decimal dollars on the wire.
- **Universe is static** — `markets.csv`, max 16 rows, no hot reload; linking across venues is manual.
- **Detached live threads** — No join on shutdown; intent latency excludes the HTTP order path.
- **Public websockets / consumer hardware** — On-box intent time is not exchange RTT.
