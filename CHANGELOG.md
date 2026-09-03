# Changelog

## 2026-09-03 (5)
- fix(docs): README.md's own "hold S" for Turnip Toss and every tuned neutral-B (Medusa, Raccoon,
  Second Tree, Uncrowned, Rosie's Insert Coin) was backwards -- real investigation, kanban
  `BPTUNE-10001` ("up b and down b all do the same thing"), traced the actual arena input read
  (`apps/lobby/src/main.c`: `if(k[SDL_SCANCODE_W]) sy += 1.0f;`) against the dispatch condition
  (`p->in_y > 0.5f`) and confirmed it's **W** (up), not S. A player following the old docs by
  holding S could never trigger any neutral-special including plain Turnip Toss -- a real,
  plausible, previously-unconsidered contributor to the `BP-fix` "turnips seem broken" report.
  Fixed every occurrence in README.md (Controls + Moves & Combos sections); the actual drop-through
  platform doc ("hold S" for that, a real, separate, unrelated mechanic gated on `in_y < -0.6f`)
  was already correct and untouched.
- feat: Medusa's Serpents' Grasp -- real, first down-B of the tuning pass (kanban `BPTUNE-10001`:
  "continue reworking the characters... up b and down b all do the same thing for every
  character... they need to be distinct moves"). Real, honest root finding: up-B (Parasol) really
  is meant to be the same universal move for every character (by design) -- the actual bug was
  that down-B (hold S + special, grounded) was a dead input for literally everyone, since nothing
  in the special-dispatch chain in `packages/common/physics.h` ever checked `in_y < -0.5f`. New
  `special_serpents_grasp` (physics.h): real melee-range damage + knockback
  (`BRAWLPIT_SERPENTS_GRASP_RANGE`/`_DAMAGE`), thematically distinct from her own ranged,
  no-damage neutral-B (the gaze paralyzes at range, the serpents bite up close) and sharing the
  same `turnip_cooldown` gate as her neutral-B on purpose (one "gaze or grasp" per cooldown
  window). New `tests/test_physics.c` coverage (`test_medusa_serpents_grasp`), drives the real
  input->dispatch->per-frame pipeline via `update_entity`. `gcc -fsyntax-only apps/lobby/src/
  main.c` clean; `bash scripts/build.sh` equivalent (`gcc tests/test_physics.c`) all 4 tests pass.
  Remaining roster (Raccoon/Second Tree/Uncrowned/Rosie/Vexar/Sunlit Draw/Sequel Duck) each still
  need their own real, distinct down-B -- real, honest, not done in this pass; logged as follow-up
  in `EMILY/BACKLOG.md`, kanban card left open (not moved to done).

## 2026-09-03 (4)
- feat: Rosie's High Score Rush -- real side-B (direction-B) special (kanban BP-TUNE-0033: "make rosie direction B do a double hit dash ability it does damage at the beginning and end of the dash and in the middle shes totally invuln like SSB dodge"). New STATE_ROSIE_DASH + PlayerState.rosie_dash_frame (packages/common/protocol.h). New special_high_score_rush_hit (packages/common/physics.h): a real, 18-frame committed dash, real hit at frame 1 (opening) and the final frame (closing), real invulnerability (reusing the existing invuln_frames field, same mechanic post-respawn already uses) through the real middle window (frames 4-14). Real, deliberate input design: intercepts the SAME real input (grounded + strong held direction + special) the universal smash-charge system already claims -- Rosie is explicitly excluded from smash-charging so her own dash takes over that input instead, matching the same real "repurpose a specific input combination per-character" precedent Medusa/Raccoon/Second Tree/Uncrowned's own neutral-specials already established for "hold down + special." Real, honest limitation named directly: turnip hits don't check invuln_frames at all (a real, pre-existing, cross-cutting gap every other custom special's hit function already has, not introduced here) -- a turnip can still land on Rosie mid-dash even though normal attacks can't. New tests/test_physics.c coverage (test_rosie_high_score_rush): drives the real input->dispatch->per-frame pipeline via update_entity itself, confirms both the opening and closing hits land independently (two real targets, since the opening hit's own knockback naturally displaces a single stationary target before the closing hit's window arrives) and that real invuln_frames appear during the dash. `bash scripts/build.sh`: clean, all 3 tests pass. `gcc -Wall -Wextra` clean (no new warnings).
- docs: real moves & combo list added to README.md (kanban BP-fix's own second ask: "put the full moves and combo list into the readme"), covering every real per-character special that currently exists -- universal moves (turnip toss, smash, parasol up-B, wavedash), Medusa/Raccoon/Second Tree/Uncrowned/Vexar's own already-shipped specials, and Rosie's own two new moves (Insert Coin, High Score Rush). Honestly lists Sunlit Draw/Sequel Duck as still fully generic, and names Understudy/Petalia as deliberately untouched per BPTUNE-003.
- investigation: kanban BP-fix's own first ask ("i think turnip projectiles are broken for petalia and for rosie") -- real, direct investigation, not a blind guess-and-patch. Wrote a real, standalone repro harness calling spawn_turnip/special_insert_coin + update_turnips directly for Petalia, Vexar, and Rosie: all three correctly spawn, travel, and land real hits with correct damage (confirmed live, not assumed). Checked live for character-ID-0-as-fallback bugs (fighter_def's own out-of-range fallback IS Petalia, CHARACTER_PETALIA=0 -- a real, plausible bug CLASS, but no actual out-of-range character_id path was found triggering it), checked player.id-vs-array-index assumptions in update_turnips' own hit-exclusion check (confirmed id is always assigned equal to array index in both local_game.h and apps/server, ruling that out), and confirmed FighterDef's own real projectile_speed_mul stat is defined but never actually applied anywhere (a real, separate, pre-existing dead-stat gap, uniform across all characters, not specific to Petalia/Rosie). Real, honest conclusion: the core server-side turnip mechanic is NOT broken for either character, verified directly -- if a real, live bug exists, it's most likely client-rendering or live-multiplayer-timing related, outside what a synthetic single-process physics test can reproduce. Not closed as fixed; flagged honestly as investigated-but-not-reproduced.

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
