# Changelog

## 2026-08-04 (2)

- feat(tipjar): Step 1 core single-player shift loop -- real bar + bouncer loop. Founder: "iterate
  tipjar." New `STATE_TIPJAR` mode (T from the lobby), new `packages/simulation/tipjar.h`: real
  customer state machine (WAITING_DRINK -> HAPPY / BRAWLING -> BUBBLED) with real per-tick patience
  decay and seat respawn cycle; delivery (serve a waiting customer for a real tip); a real bouncer
  de-escalation action (Shield restores patience); bubbles reusing the existing Turnip pipeline but
  resolved separately from `update_turnips` so real fighting-game player-knockback logic never
  touches them; real player-driven pushing of bubbled customers to an eject-door zone (the wiki's
  actual spec, not just auto-drift); real win/loss (quota, vibe collapse, or shift timer) with a
  results screen. Deliberately doesn't route through `update_entity`'s own Special-button handling
  (turnip-pull/Up-B/wavedash) -- TIPJAR reads a raw K-press for "throw bubble" instead. Live-
  verified end-to-end under Xvfb: delivery, escalation, bubble-hit, real push-to-door ejection, and
  seat respawn all confirmed via live state logging; also caught and fixed a real balance bug --
  the original 3000ms bubble duration made the eject mechanic practically unreachable (up to 47
  units to the door vs. ~2.7 units of real drift), fixed by raising duration to 7000ms and adding
  the real push mechanic. `tests/test_physics.c` still passes.

## 2026-08-04

- fix(select): reset `select_cursor` on every real entry into character select. Founder: "in
  single player mode the first game works but the next game it says character select and it
  seems lijk i cant selecti the character to play again." Real root cause, traced via
  `select_cursor`'s own mutation sites: every return-to-character-select path already reset
  `select_confirmed` back to `{0,0}`, but left `select_cursor` wherever match 1's own selection
  ended it (slot 1 -- confirming slot 0 auto-advances the cursor there). On the second trip
  through character select, the player's left/right/confirm inputs were silently editing
  `selected_chars[1]` (the bot's slot) instead of their own -- `select_confirmed[0]` could never
  become true again, so `local_init_match`'s own start condition never fired. Looked and felt
  exactly like character select stopped responding. Live-verified under Xvfb: forced the exact
  end-of-match1 state and dispatched a real `SDLK_RETURN` event through the actual `STATE_RESULTS`
  handler -- confirmed cursor resets to 0 and confirmed resets to `[0,0]`. `tests/test_physics.c`
  still passes.
