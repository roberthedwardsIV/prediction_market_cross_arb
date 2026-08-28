# Prediction Markets Cross Exchange Arbitrage Engine

Assisi - an engine that detects & trades upon pricing discrepancies between Kalshi, Polymarket (US) and Gemini prediction market exchanges. 

The engine seeks to purchase the 'cheapest yes' and the 'cheapest no' in cases where two exchanges exhibit price differences (after fees).
Market data is obtained from public WebSockets and REST APIs, and orders use these same resources. 
The engine runs almost entirely on C++, with a light frontend UI for more user-friendly controls and observation.

## Latency
The engine runs with a latency of ***.

## Position Sizing
The engine sizes positions ***.

## Build & Run Instructions

## Market Universe & Linking

## Live vs. Paper vs. Replay

## Layout

## Known Limitations

## Daily ritual

1. Stop trading (`Ctrl+C` on `./build/assisi`).
2. Edit `markets.csv` (add, drop, or fix rows).
3. Start `./build/assisi` from the repo root, then START when you want feeds.

If a row’s `expiration` is reached **during** the session, you do not need to restart. That market is dropped from trading immediately (`expiration <= now`). Quotes may still print; no new orders are sent. Delete the row the next morning so it is gone from the file too.

## Format

Header required:

```
market_id,expiration,kal_id,pm_id,gem_id
```

One internal market per row. Comma-separated. **No spaces and no commas inside fields** (the loader turns commas into spaces, then splits on whitespace).

| Column | What it is |
| --- | --- |
| `market_id` | Session-only label. Unique in this file. Safe to change across restarts. |
| `expiration` | Unix **seconds** (about 10 digits), UTC. Last moment this row may trade. Not milliseconds. |
| `kal_id` | Kalshi ticker from the market page / URL. Put `0` if this row has no Kalshi leg. |
| `pm_id` | Polymarket market slug. Put `0` if this row has no Polymarket leg. |
| `gem_id` | Gemini `instrumentSymbol` (example shape: `GEMI-CTRLUSSEN-GOP`). Put `0` if this row has no Gemini leg. |

Max **16** rows. Extra lines are ignored. Two rows with the same `market_id`: the second is skipped. Two rows with the same `kal_id`, `pm_id`, or `gem_id`: quotes collide — that is the real mix-up, not a renamed `market_id`.

## `market_id` across mornings

Nothing is saved under `market_id`. Orders, books, and startup reconcile key off Kalshi ticker, Polymarket slug, and Gemini symbol. After a restart, a new number on the same triple is fine. Duplicate `market_id` in one file is not.

## Where the ids come from

- **Kalshi:** ticker on the market page (and in the URL). Copy it exactly.
- **Polymarket:** market slug from the market page / URL.
- **Gemini:** `instrumentSymbol` from the contract (REST `GET /v1/prediction-markets/events`, or the symbol on the Gemini Predictions page).

The linker is still you. Same event, strike, and resolution — you decide that. Do not put a Kalshi rate-level Fed contract next to a Gemini hold/cut/hike contract.

## Current universe (verified 2026-08-22)

Chamber-control rows expire `1801494000` (Kalshi close `2027-02-01 15:00:00 UTC`). CA governor rows expire `1825254000` (`2027-11-03 15:00:00 UTC`).

| Row | Question | Kalshi | Polymarket | Gemini |
| --- | --- | --- | --- | --- |
| 122 | Senate GOP | `CONTROLS-2026-R` | `paccc-usse-midterms-2026-11-03-rep` | `GEMI-CTRLUSSEN-GOP` |
| 123 | Senate DEM | `CONTROLS-2026-D` | `paccc-usse-midterms-2026-11-03-dem` | `GEMI-CTRLUSSEN-DEM` |
| 124 | House DEM | `CONTROLH-2026-D` | `paccc-usho-midterms-2026-11-03-dem` | `GEMI-CTRLUSHOU-DEM` |
| 125 | House GOP | `CONTROLH-2026-R` | `paccc-usho-midterms-2026-11-03-rep` | `GEMI-CTRLUSHOU-GOP` |
| 126 | CA Gov Becerra | `KXGOVCA-26-XBEC` | `0` | `GEMI-CAGOV26-BECERRA` |
| 127 | CA Gov Hilton | `KXGOVCA-26-SHIL` | `0` | `GEMI-CAGOV26-HILTON` |
| 128 | BTC ≥ $79,000 at 5pm ET Aug 24 | `KXBTCD-26AUG2417-T78999.99` | `0` | `GEMI-BTC2608242100-HI79000` |
| 129 | BTC ≥ $79,500 at 5pm ET Aug 24 | `KXBTCD-26AUG2417-T79499.99` | `0` | `GEMI-BTC2608242100-HI79500` |
| 130 | BTC ≥ $80,000 at 5pm ET Aug 24 | `KXBTCD-26AUG2417-T79999.99` | `0` | `GEMI-BTC2608242100-HI80000` |
| 131 | BTC 15m up at 12:00pm EDT | `KXBTC15M-26AUG241200-00` | `0` | `GEMI-BTC15M2608241600-UP` |

128–130 expire `1787605200` (`2026-08-24 21:00:00 UTC` / 5pm ET). 131 expires `1787587200` (`16:00 UTC` / 12pm EDT). Polymarket is `0` on purpose.

**BTC is two-venue only and the indexes are not the same.** Kalshi is CF BRTI 60s TWAP. Gemini is Kaiko `GRR-KAIKO_RFR_BTCUSD_60S`. Same clock and dollar strike; a print that locks can still settle YES on one book and NO on the other. Use for `--clock --record` / paper replay, not live. 131 dies at noon — delete it after.

## Expiration helper

```bash
date -u -j -f "%Y-%m-%d %H:%M:%S" "2027-02-01 15:00:00" "+%s"
```

Check “now” with `date +%s`.
