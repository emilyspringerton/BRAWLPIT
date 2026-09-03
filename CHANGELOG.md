# Changelog

## 2026-09-03 (3)
- feat: Rosie's Insert Coin -- real character tuning pass, character 1 of the roster (kanban priority-queue cards BPTUNE-001/BPTUNE-003, "tuning pass on the brawlpit characters they all need unique normal/up-B/down-B/direction-B attacks... embrace spaghetti code spookiness and weird gimmicks... dont touch Understudy or Petalia"). Rosie of the Unclaimed Arcade Cabinet was 100% generic before this pass (the shared turnip-toss fallback every un-tuned fighter still uses). New special_insert_coin (packages/common/physics.h): her real, first custom neutral-special, an argument made directly from her own lore ("generated twice, a style apart, and kept both times... two separate generations of the same subject, both times reaching for a game that isn't the one she's actually standing in") -- she throws TWO turnip-style projectiles in a real, distinct spread (different vy, "a style apart") instead of one, each at reduced damage (BRAWLPIT_INSERT_COIN_DAMAGE, real balance: 2x5.0f edges out one regular 8.0f turnip only if both connect). Wired into the same real per-character dispatch hook Medusa/Raccoon/Second Tree/Uncrowned's own specials already use, correctly excluded from the generic turnip fallback's own condition. New tests/test_physics.c coverage (test_rosie_insert_coin): confirms exactly 2 turnips spawn, correctly styled, with a real distinct trajectory spread, not a cosmetic duplicate. Real, deliberate scope, matching "dont bite off too much": ONE new special this pass (neutral-B), not all four B-moves + a unique normal at once -- up-B/down-B/side-B/normal remain real, separate, honestly-tracked follow-up for Rosie, and the remaining 4 un-tuned fighters (Sunlit Draw, Sequel Duck, and re-confirming Vexar's own barely-modified turnip) stay untouched this pass. `bash scripts/build.sh`: clean build, both the pre-existing friction smoke test and the new Insert Coin test pass. gcc -Wall -Wextra clean (no new warnings).

## 2026-09-03 (2)
- docs: real, unified scoping pass for kanban priority-queue cards WOTAN-999/WOTAN-998/WOTAN-996 -- a WOTAN-hosted cosmetic hat store for BRAWLPIT paid in GFD's own real Flow currency, plus a user pixel-editor iteration. New docs/WOTAN_HAT_STORE_NORTHSTAR.md. Real, decisive finding: GFD's own Flow currency (Player.flow, apps2/mud/main.go) has no external API today, only in-process MUD server state -- the same real, already-tracked gap kanban cards 3213432/345234 already name. Real 4-phase plan (external Flow API prerequisite named not re-planned; hat catalog/inventory data model; the real WOTAN store page; real BRAWLPIT-side cosmetic rendering, an honestly unresolved technical question; the real pixel editor with named moderation/cost-model open questions). Registered as golden doc WOTAN-HAT-STORE-NORTH. Planning only, no code written.

## 2026-09-03

- feat: mirror-match hat added to draw_player (kanban priority-queue card 342342, 'we need hats for the brawlpit characters for mirror matches'). Real gap found live: draw_player colored every fighter purely by fd->body_r/g/b (a per-CHARACTER color, from fighter_def), never by player slot, despite the function's own stale comment claiming 'color based on player id' -- when both players pick the same fighter they rendered pixel-identical with nothing distinguishing them mid-fight. draw_player now takes a player_index parameter (all 3 call sites updated); any slot after 0 sharing slot 0's own character_id gets a real, colored triangle hat + pom-pom drawn above its head, using the same draw_circle/immediate-mode-triangle primitives already used for CHARACTER_PETALIA's own umbrella-accent circle, no new art asset needed. Honest v0 scope: only differentiates against slot 0, matching the real 2-player case this card names -- a genuine 3-4P free-for-all with multiple colliding slots would need a real per-slot color/accessory table, separate later work. Live-verified, not just compile-checked: forced a real mirror match (both players CHARACTER_PETALIA) under Xvfb via a temporary debug hook (reverted before commit) and screenshotted -- the second player's fighter visibly wears the hat, the first does not. Native gcc build (-Wall -Wextra) clean, scripts/build.sh + its own physics smoke test both pass. (sess-20260902-2008-ed50169e)


## 2026-09-02

- Added real dual-pad support to TIPJAR: the engine now opens up to two controllers (`g_pad`/`g_pad2`, `try_open_first_two_controllers`, hotplug add/remove handled for both). One connected pad still drives Player 2 (preserves the fix below); a second connected pad takes over Player 1 instead of leaving it keyboard-only, so two people can each play on their own controller with no keyboard needed. New shared `apply_pad_to_tipjar_input` helper replaces the old inline single-pad merge. Verified: `gcc -fsyntax-only` clean, `scripts/build.sh` build + physics smoke test pass. README updated. (sess-20260830-1207-cc0ba7da)

- Fixed a real TIPJAR input bug: the one connected gamepad was merged into PLAYER 1's own keyboard input (on top of P1's full WASD/Space/J/K/LShift scheme), so plugging in a controller made keyboard and pad both drive the same fighter, leaving P2's arrows/RCtrl/Slash/Apostrophe scheme with no pad support at all. Reassigned the pad to drive Player 2 instead — P1 stays pure keyboard, P2 is keyboard-or-pad, so plugging in one controller now gives a real 2-player split with two distinct fighters. README updated with the TIPJAR control scheme. (sess-20260830-1207-cc0ba7da)

## 2026-08-20
- Fixed Windows build: 'near' reserved as a macro under MinGW/Windows headers, broke the cross-compile (tipjar.h). Renamed to is_close, no behavior change. Also added .gitignore. (sess-20260820-0649-a3f19d93)

- Fixed a real fallthrough bug: Raccoon's dash-on-cooldown state incorrectly fell through to spawn the generic turnip special (sess-20260813-2154-dda37e8b)


## 2026-08-19

- Added 4 new fighters (Medusa, Raccoon, Second Tree, Uncrowned) from chroma-keyed Prompt-o-verse sprites, each with a real unique special move built on the existing BTN_SPECIAL framework (sess-20260813-2154-dda37e8b)


## 2026-08-18
- Fixed draw_sprite_quad to enable/restore GL alpha blending correctly, needed for upcoming chroma-keyed sprite rendering (sess-20260813-2154-dda37e8b)

- 4 pixel-art fighters (Understudy, Rosie, Sunlit Draw, Sequel Duck) added from real Prompt-o-verse 8-bit pixel art generations; new stb_image-based texture rendering (first image/sprite capability this engine has ever had) (sess-20260813-2154-dda37e8b)


## 2026-08-14
- Added scripts/build.sh (repo never had one, unlike every sibling repo) -- verified the TIPJAR build was never actually broken, running 'make' with no Makefile is almost certainly what looked like a build failure (sess-20260813-2154-dda37e8b)

- TIPJAR Step 3: real 2-player split-screen (local_set_player_input + per-panel camera/viewport/HUD), verified live via Xvfb+screenshot (sess-20260813-2154-dda37e8b)


## 2026-08-04 (3)

- feat(tipjar): Step 2 -- real player-indexed simulation, entity ownership. Founder: "iterate on
  tipjar" (continuing `TIPJAR_ROADMAP.md`'s own Step 2). `tipjar_tick` now takes the real player
  array plus one `TipjarPlayerInput` per slot instead of a single hardcoded `p0`, looping every
  real interaction (delivery, de-escalate, bubble throw, brawler chase/bump, push) over every
  active player. `TipjarState.score`/`player_hp`/`last_bump_ms` are real per-player arrays;
  `Customer.bubbled_by` gives real eject credit to whoever actually landed the bubble, not
  whoever happens to be pushing. Bar itself (customers, vibe, shift) stays one shared co-op
  instance -- Competitive Party's separate-per-player instances are Step 4, not guessed at here.
  Live-verified under Xvfb: a real second active player independently delivered a drink and
  scored correctly (`score[1]` credited, `score[0]` untouched, combined total correct) via a temp
  test hook, reverted before commit. `gcc -O2` clean, `tests/test_physics.c` still passes.

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
