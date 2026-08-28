# Decisions

## 2026-08-24 — Two-venue BTC is clock/paper only
- Hitch waived Polymarket. Added 128–130: 5pm ET Aug 24 overlapping “≥ $79k / $79.5k / $80k” on Kalshi `KXBTCD-26AUG2417` and Gemini `BTC2608242100`. 131: 12:00pm EDT 15m up (`KXBTC15M-26AUG241200-00` / `GEMI-BTC15M2608241600-UP`).
- Still not the same contract: Kalshi CF BRTI vs Gemini Kaiko 60s. Do not live-send these. CSV is static; 15m rows die at `expiration`.

## 2026-08-24 — No three-venue hourly BTC
- Gemini short crypto is only `05M` / `15M` `UP` (plus daily `HI{PRICE}`). There is no hourly duration marker.
- Kalshi live BTC short books are `KXBTC15M` (up/down vs prior 15m BRTI TWAP) and `KXBTCD` 12pm/5pm EDT strike ladders (BRTI 60s TWAP). Not a rolling hourly UP.
- Same clock + same dollar strike is still not equivalent: Kalshi settles CF BRTI; Gemini daily settles `GRR-KAIKO_RFR_BTCUSD_60S`. 15m windows also do not line up (Kalshi 11:00–11:15 vs Gemini 11:15–11:30).
- CSV cannot intern a new 15m ticker every window without a rotating linker. Do not add BTC rows until Hitch picks a triple that shares index, window, and strike.

## 2026-08-22 — Three more rows; only House GOP is three-venue
- Added 125 House GOP (`CONTROLH-2026-R` / `paccc-usho-midterms-2026-11-03-rep` / `GEMI-CTRLUSHOU-GOP`), same close as the other chamber rows.
- Added 126/127 CA governor Becerra and Hilton. Kalshi and Gemini use the same “elected to the governorship … 2026” rule. Polymarket US slug not confirmed; `pm_id=0`.
- Do not pair Kalshi `KXPRESPERSON-28-*` (inauguration 2029) with Gemini `GEMI-PRES2028-*` (election-night sources).

## 2026-08-21 — Clock-only skips private REST
- `--clock` does not pull positions/balances. Those 401s were noise on a record run. WS still needs a fresh signed timestamp.
- Kalshi and Polymarket 401 together + Gemini public `open` is almost always a stale Mac clock. `to_sign` `1787284254075` was 2026-08-21 03:50 UTC while wall clock was ~Aug 22 02:27 UTC.

## 2026-08-21 — Size on the book; backtest is our tape
- Snapshot stores `yes_bid_n` / `yes_ask_n` / `no_bid_n` / `no_ask_n`. Derived NO: `no_ask_n = yes_bid_n`, `no_bid_n = yes_ask_n`.
- Strategy skips a side if ask is 0 or `*_ask_n < 1`, then caps size to `min(yes_ask_n, no_ask_n)`. Missing-leg chase uses the same gate.
- Missing size field is unknown, treated as 1 (live size). Present 0 skips that side.
- Kalshi: `yes_bid_size_fp` / `yes_ask_size_fp`. Gemini bookTicker: `B` / `A`. Polymarket full book `bids[]`/`offers[]` first object `qty` (lite `bidDepth` is not contract qty).
- There is no synchronized historical L1 for Kalshi + Polymarket + Gemini on these midterms. Do not invent one. Record live ticks, replay the same strategy paper.
- `--record [ticks.csv]` with `--clock` or `--trade` appends after each tick. `--replay file` loads `markets.csv`, seeds $100/venue, paper-fills, never live, never WS.
- Replay is optimistic: fill at printed ask if `n >= size`, no RTT, no one-leg, same snapshot for both legs.

## 2026-08-19 — ForecastEx out, Polymarket in; Nadex out, Gemini in
- `assisi` no longer links TWS. CSV is `kal_id,pm_id,gem_id`. Universe: Senate GOP, Senate DEM, House DEM on all three books. Expiration `1801494000` (Kalshi close 2027-02-01 15:00 UTC).
- Env: `POLYMARKET_KEY_ID` / `POLYMARKET_SECRET_KEY`; `GEMINI_API_KEY` / `GEMINI_API_SECRET`; optional `GEMINI_ENV=sandbox`.
- Fees: Kalshi `ceil(0.07*C*p*(1-p)*100)/100`; Polymarket `round(0.06*C*p*(1-p)*100)/100`; Gemini Titan taker `round(0.05*C*min(p,1-p)*100)/100`.
- Kalshi WS no longer dies at 180s. Gemini private REST is POST with HMAC-SHA384 payload headers. Polymarket REST/WS is Ed25519 `X-PM-*`.
- Gemini terms accept is not wired; needed before first live Gemini order. Clock can skip.
- Master API keys (`master-…`) require `"account"` in the signed JSON (`MissingAccounts`). Default shortname `primary`; override with `GEMINI_ACCOUNT`. Account-scoped keys must omit it.

## 2026-08-19 — Three venues locked
- Kalshi, Polymarket, Gemini Predictions. IB ForecastEx and CDNA stay out. Do not reopen the venue hunt unless Hitch asks.

## 2026-08-19 — Server colo does not change trader eligibility
- Account holder / US-person status and KYC, not VPS country. Colo is latency/ops for the three locked venues. It does not unlock unregistered foreign boards of trade. OFAC countries are worse, not better.

## 2026-08-19 — Bot-usable US venues are Kalshi, Polymarket, Gemini
- Filter: REST+WS for market data and orders, no IB-style 2% NAV cap. Kalshi and Polymarket pass. Gemini Predictions (Gemini Titan DCM, live Dec 2025) also passes: REST events/orders/positions + WS bookTicker/depth and `order.place`/`order.cancel`. Docs: https://developer.gemini.com/prediction-markets/getting-started
- Still out: CDNA (FIX/app, REST/WS coming soon), ForecastEx (IB 2% or Robinhood no API), Rothera (FIX/FCM/ECP MM), CME/FanDuel/DK (no retail REST/WS), Coinbase/Webull (Kalshi front-ends), PredictIt (MD only).

## 2026-08-19 — CDNA prediction orders are not on Exchange REST/WS
- Official matrix: https://crypto.com/exchange-pro/en-US/api — DCM/FCM Predictions REST and WebSocket are **Coming soon**. FIX is **Available**. Exchange `private/create-order` is spot/margin/perps/futures, not CDNA event contracts.
- Predictions REST at https://data.crypto.com / `https://data-api.crypto.com/api/v1/predictions/` is **market data only** (5 GETs: events, search, event, contracts, price). No book, no orders, no positions. WS “Coming soon”. Personal use is keyless; commercial needs MDLA.
- CDNA rulebook: Executing Broker Members get FIX; Market Makers get FIX gateway (fee). Retail Trading Members: Crypto.com app after Nadex.com retired Dec 2025. Do not treat Exchange v1 `product_type=EVENTS` as CDNA US prediction trading.

## 2026-08-19 — IB event capital is 2% of account
- ForecastEx only. Cap = 0.02 × NLV (else cash). Used = open FX premium from reconcile + live FX fills. Strategy and FX send require the FX leg to fit in remaining. IB 201 clears the order queue and stops chase (wait flags reset on enqueue).

## 2026-08-19 — Live clocks cover tick and fill
- Tick: parse / intent / dispatch / tick_to_send + src= (also live). p50/p99 tick_to_intent every 100 ticks.
- Fill: `kalshi_http_us` (POST RTT). FX: `fx_enqueue_us` (queue push), `fx_queue_us` (enqueue → placeOrder), `fx_place_us` (placeOrder call), `fx_fill_us` (placeOrder → Filled/reject). `leg_gap_us` is Kalshi fill → FX enqueue. `pair_total_us` / `missing_total_us` wrap the worker.

## 2026-08-19 — Missing-leg chase is per-market
- `yes_count != no_count` on that row: no new YES+NO pair. Buy the short side (cheapest Kalshi/FX ask, size 1, +1¢ × 15). Other rows still trade. Halt still blocks new missing-leg sends; in-flight chase is not aborted.

## 2026-08-19 — STOP
- UI STOP → `/api/stop`. No new live/paper pairs. In-flight `liveSendPair` chase is not aborted. Feeds stay up. Restart the process to send again.

## 2026-08-19 — Live size 1
- `liveSendPair` and the live arm path force both legs to size 1. Strategy max 50 stays for paper only.

## 2026-08-19 — In-process clocks closed
- After string-find: `src=fx` parse=0 tick_to_send=3; warm `src=kalshi` parse=24 tick_to_send=33 (was 260–280 parse). Stop in-process speed. Wire time only on live send.

## 2026-08-19 — Kalshi tick parse is string-find, not nlohmann
- Warm nlohmann ticker parse stayed 260–280µs. Subscribe still uses nlohmann. Incoming ticker: find `"type"` / `"yes_bid_dollars"` / `"yes_ask_dollars"` / `"market_ticker"` in the raw frame. Same four fields as before. Re-clock `src=kalshi`.

## 2026-08-19 — Re-clock after UI off tick
- Fire lines: parse=73 intent=0–1 dispatch=25/69 tick_to_send=100/144. Intent cut was real. Parse is now the largest real segment. Dispatch spread is stdout/`unitbuf`, not strategy.

## 2026-08-19 — Intent was UI JSON
- First `--clock` fire: parse=19 intent=427 dispatch=27 tick_to_send=474. `monitorRefresh` (full status JSON + 80 log lines) ran on every tick. Now `/api/status` builds that; tick path only strategies. Re-clock.

## 2026-08-19 — `--clock` is dry latency
- `./build/assisi --clock` starts feeds like `--trade` but never `liveSendPair`, paper `Executioner`, Kalshi smoke, or IB paper 0.01. Flag is argv-only so a live `.env` cannot accidentally send. UI shows CLOCK. `--trade` / START with `ASSISI_LIVE=1` still live.

## 2026-08-19 — Tick clocks
- `latency.hpp`: arrive (WS/IB tick in) → parsed → intent → dispatch. Log `tick_to_send` on fire; p50/p99 of tick_to_intent every 100 ticks. Kalshi HTTP and FX enqueue timed inside live send. Colocation only if these numbers say so.

## 2026-08-19 — Do not execute Kalshi through IB
- TWS listing Kalshi is for lookup/eyeball only. IB’s prediction-markets UI smart-routes across Kalshi / ForecastEx / CME. Assisi needs two pinned books and Kalshi-native FOK + fees. Kalshi via IBKR also adds IB commission. Keep `kal_id` as the Kalshi ticker; `for_id` as FORECASTX YES ConId.

## 2026-08-19 — CSV daily + mid-session expiry
- Format and AM restart ritual: `markets.md`. Max 5 rows. `for_id` is IB YES ConId from TWS; Kalshi ticker from the site.
- `strategize` and the tick path refuse `expiration <= now`. No restart required for a clock that hits during the day. Positions stay; no new orders.

## 2026-08-19 — CSV expiration is unix seconds
- `strategize` compares `expiration` to `time(nullptr)` (seconds). Do not paste millisecond epoch (13 digits) or the market never expires. Hitch picks the clock; code does not infer Kalshi vs IB last-trade.

## 2026-08-19 — Linker is manual
- Hitch does not trust regex, LLM, or a code matcher to pair Kalshi and ForecastEx. `markets.csv` is the link table. Automate only after hand-linking proves mechanical.
- Exact match still means same event, strike, and resolution — Hitch decides that, not a fuzzy lookup.
- Remaining “3” is universe policy: which series he will add, expiration definition, no live NBA/in-game markets (half-leg risk).

## 2026-08-19 — Reconcile on start
- No local DB. On process start: Kalshi GET `/portfolio/positions` into the book; IB `reqPositions` + `TotalCashValue` (do not let AvailableFunds overwrite). IB connects for cash/positions during UI-only; quotes and orders wait for START / `--trade`.

## 2026-08-19 — UI without trading
- `./build/assisi` serves `http://127.0.0.1:8787` and waits. No IB, Kalshi WS, or demo smoke until `--trade` or POST `/api/start` (START button). Demo smoke stays gated to trading start.

## 2026-08-19 — Cash, size, chase, monitor (Hitch away)
- Sizing: `contracts = edge% * venue_cash / pair_cost`, min 1, max 50, still require yes+no+fees < $1 per contract. Venue cash = Kalshi GET `/portfolio/balance` + IB `TotalCashValue` when present; else internal 100.
- Missing leg: bump that side +$0.01 (max 15) until filled or the pair is no longer locked. Chase does not cross through $1.
- Monitor: `http://127.0.0.1:8787` serves `ui/index.html` and `/api/status`. Do not log IB account ids (skip 2148; redact Account:).
- Live pair runs on a worker thread so IB `process()` can still drain `placeOrder`.

## 2026-08-19 — Post-live roadmap (Hitch)
- Locked small arb is the point. Do not optimize cents of edge in the fee model next.
- Order: (5) true Kalshi+IB cash/positions, then (3) sizing, then (2) exact-match linker, then (4) monitor UI, then (1) speed. Speed without a measured tick-to-order time is guesswork. Edge% of portfolio as size is the starting rule; cap by book depth and one-leg risk, not edge% alone. Internal `cash = 100` is a lie now that live fills exist.

## 2026-08-19 — Drop the $0.86 Kalshi YES
- Hitch wrote it off. Do not bring that fill into later tests, hedges, or run-again warnings. Next concern is whether IB ForecastEx `placeOrder` works after the email token.

## 2026-08-19 — Do not call live ready until the send cannot one-leg on a Kalshi fail
- First "ready" was wrong: FX still placed after Kalshi 403, two markets, GTC, IB 110. No fill that time. That is not ready.
- Live: one pair per process. Kalshi `fill_or_kill` first. ForecastEx only if Kalshi 201 and fill_count > 0. REST Origin is `https://kalshi.com` (ix default Origin was `https://host:443`, same class as WS 403). Disable HTTP gzip. `sendKalshiOrder` error paths return false, not 1. IB price rounded to contract `minTick`.
- Irreducible: Kalshi FOK fills then FX rejects → long YES size 1. Two-venue is not atomic.
- 403 empty body: WAF/Origin or key without `write::trade`. Do not rerun until Hitch says the key can trade.

## 2026-08-19 — Live first send: 403 then 110, no fill
- Kalshi POST 403 empty body: authenticated WS works, order write forbidden. Typical cause is a view/read key without `write::trade`. 401 would be bad signature.
- IB error 110 is a reject (min tick), not a fill. Round `lmtPrice` to 2 decimals before `placeOrder` (float 0.11/0.64).
- Do not enqueue ForecastEx if a Kalshi leg was required and did not return 201. Otherwise a later 110-fix would buy FX NO unhedged.

## 2026-08-19 — ForecastEx NO is a put / `_NO` contract
- Live arb on U3 was Kalshi YES + ForecastEx NO. Buying the YES conid at the NO price is the wrong book. Resolve NO at IB start: `reqContractDetails` on YES, then same strike/date with right `P` and `_NO` localSymbol. Map stays in process memory; do not print conids.
- Live sends once per `market_id` per process (`live_sent`). Paper portfolio is not updated on live, so without this every tick would re-fire.
- Kalshi POST is sync on the tick thread; IB `placeOrder` is queued on the IB thread. One-leg is still possible. Size stays 1.

## 2026-08-19 — Order plumbing + first live gate
- `KALSHI_ENV=prod` / demo hosts in `kalshi_env.hpp`. `ASSISI_LIVE=1` sends Kalshi REST + IB `placeOrder` and skips paper `Executioner`. Default stays demo + paper 4002.
- Startup smokes (Kalshi 0.32 bid, FX 0.01 BUY) only on demo + paper. Never when prod or live — flipping keys without `ASSISI_LIVE` must not fire real orders.
- Kalshi V2 POST `/trade-api/v2/portfolio/events/orders` signed like WS. Demo returned 400 `insufficient_balance` — auth and body reached Kalshi.
- IB `placeOrder` uses conId + `FORECASTX` only (official sample). Paper returned 10345 "You cannot trade a Option" — quotes work; paper lacks Forecast Contracts / options permission. Live account must have that permission.
- ForecastEx NO is a separate `_NO`/put contract. CSV `for_id` is YES. Live skips the pair if the cheap NO is ForecastEx. Do not BUY the YES conid at the NO price.
- Paper still pretends both legs fill. Live does not. Half-fill handling is still unfinished.
- Size stays 1. ClientId 16. Do not paste conids in chat.

## 2026-08-19 — Assistant copies boilerplate
- User does not copy WS/auth/sign/CMake glue back and forth. Assistant pastes that. User writes new domain logic (strategy, books, order fields). Ping the user when the next edit is actually new.
- Do **not** sneak extra JSON/API fields into their file unasked. "I copy boilerplate" is not a license to add `time_in_force` etc. mid-step. Ask or wait.

## 2026-08-19 — One edit per reply
- User is done with multi-step tickPrice dumps. After each "done": check, then exactly one edit (one place, one action). Do not combine "add id" with snapshot/bid/ask/prints. Do not wait to be asked for the next single edit.
- User asked to add 3 markets. Do not assign a different next step (orders, reverse intern, for_id vector). Do not assume prior homework was done. Answer the requested thing.

## 2026-08-19 — Per-leg taker fees
- `takerFee(venue, price, contracts)` in `strategy.hpp`. Kalshi: `ceil(0.07 * C * P * (1-P) * 100) / 100` (M=1 taker; INX/Nasdaq 0.035 not modeled). ForecastEx: `$0.01 * C`. Else 0 (Nadex idle).
- Strategy: sum two calls after cheapest YES and NO books are known. Do not compute fees before venue selection. Stub `0.02` was charged once on the pair; that was wrong.
- Portfolio: one fill = one `takerFee`. Cash is `(size * price) + fees`. Not `size * (price + fees)` — `takerFee` already includes size.
- Official Kalshi also has maker rates, series multiplier M, and centicent/balance rounding. Ceil-to-cent is a conservative hurdle, not a ticket clone.

## 2026-08-19 — Two-venue Kalshi + ForecastEx; Nadex idle
- Cross-venue arb needs the same event on two books. U3 already overlaps Kalshi and ForecastEx. Nadex crypto does not help that book.
- Feed + identity is the cost (same as IB conids). The `Nadex` snapshot / CSV column / strategy `yes_ask != 0` branches already no-op. Do not rip them out. Do not fake a Nadex book.
- Add Nadex later only for a specific overlapping contract, not “because we have a third slot.”

## 2026-08-19 — Live Kalshi + ForecastEx in assisi
- Helper `startForecastExFeed` owns `IbFeed`. `assisi` registers CSV, starts IB on a thread, Kalshi on main, `join`.
- Kalshi ticker needs `send_initial_snapshot: true` or you get `subscribed` and no book.
- `on_kalshi_tick` must look up ForecastEx if Kalshi snapshot is empty (same callback, two venue ids).
- `lock_guard` only locks. Do not construct a second guard named unlock. Do not lock `Register` if it calls `getMarketById` (same mutex).
- Farm 2104/2106/2158/2107/2119 are status. Skip them.

## 2026-08-19 — Do not implement on “let’s do it”
- User writes the code. Assistant teaches one slice. Unsolicited `ib_md`, mutex on `MarketData`, and linking TWS into `assisi` was a contract breach. Reverted. Stay in `ib_feed` until quotes print.

## 2026-08-19 — UNR chain works; intern YES 0826 4.2 not the index
- 2104/2106/2158 are farm-up notices. 2158 `secdefil` is what `reqContractDetails` needs; without it the request is silent. That is what a Gateway cycle actually resets — not a superstition.
- Duplicate `conid:` lines: `contractDetails` and `contractDataProtoBuf` both fire. Use one.
- ForecastEx localSymbol is `UNR_MMYY_K_YES|NO`. Kalshi `KXU3-26AUG-T4.2` = `UNR_0826_4.2_YES` (C), last trade `20260904` (BLS), not `20260826`. `UNR_1226_4.2_*` is a different month.
- Matching-symbols UNR IND is the underlying. Do not put it in `for_id`.
- Handshake at 223 works. Empty protobuf (`reqCurrentTime`) works. `reqMatchingSymbols` works. Nested `ContractDataRequest` is what Gateway uses for conid lookup at 223 (`REQ_CONTRACT_DATA+200`).
- Forcing classic encode at 223 (`if (false && useProtoBuf)`) is ignored: zero bytes back, no error 200. Do not do that again.
- Skip `conId=0` in `createContractProto` (proto3 optional would serialize 0). C++ `Contract` defaults `conId=0`; `isValidValue(int)` is only `!= INT_MAX`.
- Official Python ibapi  + protobuf 5.29.5 behaves the same as our C++ 7.35 stubs. This is not a protoc-35 wire-format bug.
- `UNR OPT FORECASTX 20260826 C 4.2` and localSymbol `UNR_0826_4.2` return error 200 (no security definition). Kalshi `26AUG` is not IB's last trade date. IB ForecastEx example uses full `YYYYMMDD`. Matching finds UNR as IND on FORECASTX (underlying), not the 4.2 option.
- After those 200s, **all** `reqContractDetails` (including AAPL conid 265598) send nothing — not even 200. Decoder never sees a message. Same Gateway still answers matching + currentTime. Matches the earlier FIX NPE on security lookup. Restart Gateway; then one chain query: UNR / OPT / FORECASTX / USD, no strike. Filter 4.2 into `markets.csv` `for_id`.
- `+PACEAPI` before `eConnect`. 2s after `nextValidId`. Similar CD queries are paced ≥1 min.

## 2026-08-18 — Kalshi WS helper shared by both binaries
- `kalshi_ws.hpp` / `kalshi_ws.cpp`: `startKalshiWebsocket(MarketData&, intern map&)`. No `main`.
- `kalshi_feed` is smoke. `assisi` loads CSV, calls helper, paper TEST 1 after 15s.
- `#pragma once` on headers that get included from more than one place (`market_data.hpp`).
- Feed stays ignorant of strategy. Strategy-on-tick is a callback from main, not includes in `kalshi_ws.cpp`.

## 2026-08-18 — markets.csv is the intern table
- Kalshi column is ticker string. Load intern → long. `Register` uses kal_venue_id ≠ market_id.
- `getSnapshotByVenueId(venue_id, venue)` matches `price_update`.
- `kalshi_feed` is the live process. `assisi` TEST 1 uses `kalshi_ids[kal_id]`.

- MLB ticker in CSV expired overnight. User swapped to a live market; feed worked.
- We only log `type==ticker`, so subscribe errors are invisible. Print `type==error` next.
- Do not treat HTTP 401 as "market expired" without checking; 401 is handshake. Still: always verify the ticker is live.

- `unordered_map<string, long>` ticker → kal_id. `price_update` still takes `long`. Unknown ticker: skip.

- `venue` (KAL/FOR/NAD) stays a small int — finite set, enum is right.
- `venue_id` in Snapshot/strategy stays `long`. Do not put ticker strings on the hot path (same reason we dropped `char venue[4]`).
- Do not hash tickers (collisions). Do not enum every Kalshi market (they churn).
- Feed/CSV: ticker string → interned long, once at Register. Ticks look up the map, then `price_update(long, …)`.

- Parsed `yes_bid_dollars`/`yes_ask_dollars`; NO derived as `1 - yes_ask` / `1 - yes_bid`.
- Stand-in `Register(1, …, kal_id=1)` because venue_id is still `long`, ticker is a string.
- Lambda must capture `&md` by reference.

- Subscribed `ticker` for one market; received `subscribed` + ticker JSON (`yes_bid_dollars` / `yes_ask_dollars`).
- Next: parse into existing `price_update` (not yet).

- Prod key from kalshi.com. Demo host 401s those keys.
- IXWebSocket default `Host: host:443` + `Origin: wss://host:443` → 403. Fix: Host without port, Origin `https://kalshi.com`, disable deflate/reconnect.
- Sign path remains `/trade-api/ws/v2`.

- Keys from kalshi.com are production. Demo keys only exist if you log into demo.kalshi.co.
- 401 on demo WS with a prod key. Sign path is still `/trade-api/ws/v2`; only the host changes.
- Leading `/secrets/...` is filesystem root, not the repo. Use `secrets/kalshi_private_key.pem`.
- `source .env` is a shell trick; CMake is build-time. ws_hello reads `.env` from cwd (repo root).

- Leading `/secrets/...` is filesystem root, not the repo. Use `secrets/kalshi_private_key.pem`.
- `source .env` is a shell trick; CMake is build-time. ws_hello reads `.env` from cwd (repo root).

- `source .env` is a shell trick; CMake is build-time. ws_hello reads `.env` from cwd (repo root).

- IXWebSocket + TLS connected to `wss://ws.postman-echo.com/raw`. echo.websocket.events is dead (DNS).
- Teacher: one-step walkthrough of ws_hello.cpp for first-time WS.
- Data file lives next to CMakeLists; run `./build/assisi` from repo root. Don't bake `src/` into the binary.
- Learning path: IXWebSocket for live Kalshi MD.
- Beast only if client proves hot under measurement.

## 2026-08-17 — Live data: WebSocket first
- User: MD via WebSocket; REST deferred to execution (Kalshi orders).
- No split Composer panes; continue Socratic one-step build in-chat.

## 2026-08-16 — End-to-end skeleton green
- Full loop: MD updates → strategy min-ask → execution fill → portfolio (size * price).
- Even slate for next session; optimization not started.

## 2026-08-16 — No code unless asked (reinforced)
- Base chat rule. Violating it (including “hint” code blocks) breaks the learning contract.
- Feedback = what’s wrong / what to think about / what done looks like in words. No snippets.

## 2026-08-14 — Teaching mode over implementation
- Prefer Socratic prompts and review-style feedback over writing code for the user.
- Build competence in this order: C++ fluency → clear module boundaries → correct market/exchange model → then latency.

## 2026-08-16 — Pivot: refine market_data before finishing portfolio
- User struggling with skeleton-vs-real line; agreed to deepen MD next.
- Endgame venues: Kalshi, ForecastEx, Nadex — trade exact-match markets across venues.
- Cross-venue identity (same event/contract) is a first-class problem; latency comes after correct books.

## 2026-08-14 — Scope realism
- Paper/sim exchange first; no real money, no claiming production colocation latency until measured.
- “HFT” is a performance goal after correctness; don’t optimize before multi-venue model exists.
