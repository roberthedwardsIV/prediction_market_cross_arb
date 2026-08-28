# Prediction Markets Cross Exchange Arbitrage Engine

Assisi - an engine that trades upon pricing discrepancies between Kalshi, Polymarket (US) and Gemini prediction market exchanges. 

### Strategy
The 'cheapest yes' and the 'cheapest no' are purchased in cases where two exchanges exhibit price differences (after fees).

### External Resources
Market data is obtained from public WebSockets and REST APIs, and orders use these same resources. 

### Build Language
The engine runs almost entirely on C++, with a light frontend UI for more user-friendly controls and observation.

## Latency
The engine runs with a latency of ***.

## Position Sizing
The engine sizes positions ***.

## Build & Run Instructions

## Market Universe & Linking

'markets.csv' holds the market universe as listed below: 

Header Required:
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

If a row’s `expiration` is reached **during** the session, you do not need to restart. That market is dropped from trading immediately (`expiration <= now`). Quotes may still print; no new orders are sent. Delete the row the next morning so it is gone from the file too.

### Where the ids come from

- **Kalshi:** ticker on the market page (and in the URL). Copy it exactly.
- **Polymarket:** market slug from the market page / URL.
- **Gemini:** `instrumentSymbol` from the contract (REST `GET /v1/prediction-markets/events`, or the symbol on the Gemini Predictions page).

### Expiration helper

```bash
date -u -j -f "%Y-%m-%d %H:%M:%S" "2027-02-01 15:00:00" "+%s"
```

Check “now” with `date +%s`.

## Live vs. Paper vs. Replay

## Layout

## Known Limitations





