# Assisi — Project Memory

## What this is
Portfolio C++ project: Kalshi / Polymarket / Gemini Predictions cross-venue arb.

## How we work
- User writes new domain logic. Assistant copies auth/sign/HTTP/CMake/host glue.
- No extra Composer panes. Do not paste account numbers or IB conids in chat.
- Do not read `.env` or `secrets/` in chat.

## Current focus (SINGLE)
- Added Kalshi+Gemini BTC 5pm strikes ($79k/$79.5k/$80k) and one 15m noon window. Polymarket is 0. Indexes differ (BRTI vs Kaiko) — clock/paper only. 131 expires 12:00pm EDT today.

## Live switch (user does this)
- `.env`: `KALSHI_ENV=prod`, `ASSISI_LIVE=1`, plus Polymarket and Gemini keys below.
- Prod Kalshi key + pem. Size is hardcoded `1`. Startup smokes do **not** run when prod or live.

## Credentials
- Kalshi: `KALSHI_API_KEY`, `KALSHI_PRIVATE_KEY_PATH`, `KALSHI_ENV=prod`
- Polymarket: `POLYMARKET_KEY_ID`, `POLYMARKET_SECRET_KEY`
- Gemini: `GEMINI_API_KEY`, `GEMINI_API_SECRET` (optional `GEMINI_ENV=sandbox`, optional `GEMINI_ACCOUNT` shortname; master keys default to `primary`)
