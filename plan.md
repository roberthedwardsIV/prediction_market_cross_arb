# Three venues: Kalshi / Polymarket / Gemini

Hitch is at dinner. Execute in this process. No extra Composer panes.

## Locked
- Remove ForecastEx/IB from `assisi`. Replace with Polymarket.
- Replace Nadex with Gemini.
- Clock-ready: WS books + REST cash/positions/orders. Hitch adds `.env` keys only.

## Modules
1. Domain: venue constants, CSV `kal_id,pm_id,gem_id`, strategy across 3 books, 3 cash pools, drop IB 2%.
2. Polymarket: Ed25519 REST + markets WS lite + FOK orders.
3. Gemini: HMAC-SHA384 REST + `bookTicker` WS + FOK orders.
4. Glue: `main`, monitor, UI, CMake. Kalshi WS stays up (no 180s stop).
5. Universe: 3 CSV rows that exist on all three books where the contract is the same question.

## Env Hitch adds
```
POLYMARKET_KEY_ID=
POLYMARKET_SECRET_KEY=
GEMINI_API_KEY=
GEMINI_API_SECRET=
```
Optional: `GEMINI_ENV=sandbox`. Do not write secrets into the repo.
