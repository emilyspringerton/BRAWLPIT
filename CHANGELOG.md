# Changelog

## 2026-09-04 (9)
- refactor(physics): readability pass on `update_entity`'s special-move dispatch, per founder
  request ("the control flow is fucking terrible... a crazy gauntlet of ifs"). Extracted the
  15-branch else-if chain into a standalone `dispatch_special_move` function, one guard clause
  per branch with an early return instead of an accumulating else-if chain -- same behavior
  (conditions were already mutually exclusive), easier to read one branch at a time. Renamed two
  overloaded `PlayerState` fields that were confusingly reused across unrelated moves:
  `turnip_cooldown` -> `special_b_cooldown` (gates Medusa/Second Tree/Uncrowned/Vexar's specials
  too, not just turnip throws) and `dodge_cooldown` -> `dash_cooldown` (gates Raccoon's specials
  and Rosie's High Score Rush, not just the universal wavedash). Stripped ticket-number/lore
  comment bloat throughout the file down to real gotchas only. Also folded in a real balance
  change: Vexar's Relic Warp is now usable airborne (previously grounded-only), and its distance
  is retuned to 28.4 units. New test (`test_vexar_relic_warp_airborne`); all 12 physics tests
  pass, `apps/server` still builds clean against the renamed fields.

## 2026-09-04 (8)
- feat: Vexar's Relic Warp -- the tuning pass's fifth real down-B, and Vexar's own FIRST real
  custom special ability (kanban `BPTUNE-10001`). Unlike every other tuned character, Vexar's
  own neutral-B was never a unique move -- just the shared Turnip Toss with a real, slightly
  faster cooldown. New down-B: an instant short-range warp (`BRAWLPIT_RELIC_WARP_DISTANCE`, 3.5
  units) in the held or facing direction, distinct in KIND from every turnip-throwing
  neutral-B (no projectile, pure positioning) -- real, deliberate fit for his own "COSMIC RELIC
  HUNTER" compendium title and his own already-established real up-B variance (a stronger vy
  boost + facing-direction kick, the only other character-specific up-B tweak in this file).
  Shares `turnip_cooldown` with the neutral-B. New `test_vexar_relic_warp` confirms the real
  positional move and that no turnip spawns. `bash scripts/build.sh` clean, all 11 physics tests
  pass. README updated. Real, honest, still open: Sunlit Draw and Sequel Duck each still need
  their own real, distinct down-B -- the last two fully-generic characters in the roster.

## 2026-09-04 (7)
- feat: Uncrowned's Cast Doubt -- the tuning pass's fourth real down-B (kanban `BPTUNE-10001`,
  continuing from Medusa/Raccoon/Second Tree). Real, deliberate contrast to Uncrowned's Claim
  (purely self-facing shield top-up, no offense at all): drains a nearby opponent's own
  shield_health (`BRAWLPIT_CAST_DOUBT_SHIELD_DAMAGE`, 20.0) -- the first Uncrowned move that
  reaches another player at all -- while still dealing real, deliberate zero damage_percent, a
  design boundary kept on purpose (Uncrowned remains the one fighter whose kit never lands a
  real damage/knockback hit, even now that it reaches other players). "Doubt, not triumph"
  turned outward for the first time. Shares `turnip_cooldown` with the neutral-B. New
  `test_uncrowned_cast_doubt` confirms the real shield drain and zero damage dealt. `bash
  scripts/build.sh` clean, all 10 physics tests pass. README updated. Real, honest, still open:
  Vexar, Sunlit Draw, Sequel Duck each still need their own real, distinct down-B.

## 2026-09-04 (6)
- feat: Second Tree's Regrowth -- the tuning pass's third real down-B (kanban `BPTUNE-10001`,
  continuing from Medusa's Serpents' Grasp and Raccoon's Play Dead). Real, deliberate contrast
  to Ground Slam (pure AOE offense, zero self-benefit): heals a real, meaningful amount of
  `damage_percent` back (`BRAWLPIT_REGROWTH_HEAL_AMOUNT`, 15.0 -- comparable in size to a single
  Ground Slam's own 10.0 damage output), zero offense at all. Shares `turnip_cooldown` with the
  neutral-B on purpose. New `test_second_tree_regrowth` confirms the real heal amount and floor
  at 0. `bash scripts/build.sh` clean, all 9 physics tests pass. README updated. Real, honest,
  still open: Uncrowned, Vexar, Sunlit Draw, Sequel Duck each still need their own real,
  distinct down-B (Rosie and Petalia already have real down-Bs -- Insert Coin/Turnip Toss,
  see the `BP-TUNE-93939` entry above).

## 2026-09-04 (5)
- docs(WOTAN_HAT_STORE_NORTHSTAR): Phase 4.5's own synchronous-purchase gap resolved by real,
  direct founder clarification (kanban `HS-GFD-2223`): "a surprise box does not need to
  generate the image at the time of purchase, it needs to get generated when the player uses
  the item in GFD — it is actually like a tradable token." The box is a real GFD item (fast,
  ordinary purchase); the slow promptoverse generation only happens later at real use time,
  genuinely async, no longer needing to fit inside a single purchase transaction. Real, new
  remaining gaps named, not solved: the MUD server has no real background-job runner, and no
  IDUNA endpoint exists yet to create-a-new-hat-and-grant-it in one step (needed at generation
  completion). No code written this pass -- planning only, real sub-tasks logged to
  `EMILY/BACKLOG.md`.

## 2026-09-04 (4)
- feat: Rosie and Petalia's turnip toss is real down-B now, not up-B, and air-available (kanban
  `BP-TUNE-93939`: "rosie and petalia TURNIPS SHOULD NOT BE UP B THEY SHOULD BE DOWN B AND ALSO
  AVAILABLE IN THE AIR"). Rosie's own Insert Coin moved entirely off her old neutral-B slot
  (hold W) onto a real down-B (hold S) -- her hold-W input now does nothing at all where Insert
  Coin used to fire. Petalia gets a real, dedicated down-B for the first time: previously she
  only ever reached the generic neutral-B turnip fallback (hold W, grounded only, same as every
  untuned character); now `spawn_turnip` is wired to her own real down-B, explicitly excluded
  from the generic fallback (same real "has a dedicated special, excluded from the generic one"
  pattern every other tuned character already follows). Both are now real, genuinely usable in
  the air too, via a new branch checked before the existing airborne umbrella-toggle fallback
  (which otherwise would have silently swallowed every airborne down-B press for these two).
  New `test_rosie_petalia_turnip_is_down_b_not_up_b`: confirms Rosie's hold-up spawns nothing,
  her hold-down spawns real Insert Coin, Petalia's hold-down spawns a real turnip, and Petalia's
  air hold-down does too. `bash scripts/build.sh` clean, all 8 physics tests pass. README
  updated (both entries moved to their own real Down-B lines).

## 2026-09-04 (3)
- feat: Petalia's Parasol Up-B is now real, multi-hit (kanban `BP-TUNE-3939309`: "RESTORE
  PETALIA PARISOL UP B FROM WAY BACK IN GIT IT NEEDS TO BE MULTI HIT AND GIVE VERTICAL MOBILITY
  AND OPEN THE PARISOL"). Real, honest investigation performed first: no prior Petalia-specific
  up-B code was found anywhere in this repo's own git history, nor in SHANKPIT (the base this
  repo forked from) -- this is a real, new build against the card's own literal requirements,
  not a literal restoration of lost code. Real, load-bearing find that made it more "finish"
  than "invent": `parasol_rehit_timer` already existed on `PlayerState` and was already
  decremented every frame, but nothing anywhere ever armed or read it -- a real, half-built
  multi-hit mechanic, scaffolded and never wired up. New `special_petalia_parasol_hit` +
  a real per-frame check in the `STATE_UPB` update block: a hit fires the moment the timer hits
  0 (immediately on activation), then re-arms for `BRAWLPIT_PETALIA_PARASOL_REHIT_INTERVAL` (12
  frames) before the next can land -- several real hits across the ~50-frame ascent. Vertical
  mobility and "opens the parasol" were both already real/universal (every character's own up-B
  already does both) -- Petalia's real, new piece is the damage itself, since no character's
  up-B deals any today. This is the one, explicit, deliberate exception to the standing "don't
  touch Petalia" tuning-pass rule -- the card asked for her by name. New
  `test_petalia_multi_hit_parasol` confirms multiple separate real hits land (not just one) and
  the parasol still opens. `bash scripts/build.sh` clean, all 7 physics tests pass. README
  updated (Petalia moved out of the "untouched" list into the real per-character specials list).

## 2026-09-04 (2)
- fix: real, genuine bug fixed -- a held-direction special press no longer silently falls back
  to a wavedash when the real special is on cooldown (kanban `BP-TUNE-393939`/`BP-TUNE-9838382`:
  "if turnip is on cooldown the character should not fall back to a wave dash" / "all characters
  b should be a special move not a wave dash"). Every neutral-B/down-B branch in the dispatch
  chain requires its own `cooldown == 0`; when the real special was still cooling down, none of
  them matched and execution fell all the way through to the generic wavedash branches at the
  bottom of the chain -- silently substituting a wavedash for a failed special attempt, on every
  character, every time their special was on cooldown. New real, explicit catch-all branch
  (`p->in_y > 0.5f || p->in_y < -0.5f`) intercepts this case with an intentionally empty body --
  a held-direction special press with a cooling-down special now does nothing at all, never a
  wavedash. Wavedash itself stays reserved for the genuinely undirected "K alone" input, matching
  the README's own documented control. New `test_special_on_cooldown_does_not_fall_back_to_
  wavedash` (drives the real input->dispatch pipeline via `update_entity`, confirms no
  `STATE_WAVEDASH`/`wavedash_frames` when a held direction hits an on-cooldown special).
  `bash scripts/build.sh` clean, all 6 physics tests pass.

## 2026-09-04
- feat: Raccoon's Play Dead -- the tuning pass's second real down-B (kanban `BPTUNE-10001`,
  continuing from Medusa's own Serpents' Grasp). Real, deliberate contrast, not a variation, on
  her own neutral-B: Scavenger's Dash escapes by moving away, Play Dead escapes by standing
  completely still with real invulnerability (`BRAWLPIT_PLAY_DEAD_INVULN_FRAMES`, 20 frames) --
  both real, honest expressions of "pure mobility, no offense at all" from opposite directions.
  Zero damage dealt, keeping Raccoon's own established identity intact across both moves. Shares
  `dodge_cooldown` with the neutral-B on purpose (one real "dash or play dead" budget per
  cooldown window). New `test_raccoon_play_dead` (confirms velocity zeroed, real invuln granted,
  and the move does NOT enter Scavenger's Dash's own `STATE_WAVEDASH`). `bash scripts/build.sh`
  clean, all 5 physics tests pass. README's Moves & Combos section updated. Real, honest, still
  open: Second Tree, Uncrowned, Rosie, Vexar, Sunlit Draw, Sequel Duck each still need their own
  real, distinct down-B.

## 2026-09-03 (10)
- docs(WOTAN_HAT_STORE_NORTHSTAR): new Phase 4.5 scoped, per Principle 19 (kanban `BPHS-00001`,
  "a surprise box with a lot of flow allows you to generate your own hat with the promptoverse
  hat gen"). Real prerequisites now exist and are named directly: the real, live `promptoverse
  hat` generation style (`HSG-000`) and the real hat catalog/buy API (Phase 1). Real, concrete
  gaps named, not solved: a real promptoverse generation is slow (tens of seconds to minutes
  between requests, live-verified), incompatible with `handleBuyHat`'s own synchronous
  one-transaction design; the generated hat's subject isn't decided; the card's own "inflation
  sync" phrase is unexplained (read as "price should scale with Flow supply," no real supply
  metric found to key off); moderation carries over from Phase 4 unchanged; the `hats` table
  needs a new `user_generated` column. No code written this pass — planning only, real
  sub-tasks logged to `EMILY/BACKLOG.md`.

## 2026-09-03 (9)
- fix(input): real controller parity bug fixed (kanban `BP-TUNE-CP-001`: "BP CONTROLLER PARITY -
  keyboard controll can drop down through the platforms controller cant (fall through)"). Found
  live: `g_pad.ly` (the real left-stick vertical axis) was polled every frame but never once
  merged into the arena's own `sy` anywhere in `apps/lobby/src/main.c` — `dpad_up`/`dpad_down`
  were in the same boat. A controller player had no real way to set `in_y` at all, meaning every
  W/S-gated mechanic was silently unreachable on a pad — not just drop-through platforms, but
  every neutral-special's own "hold up + special" dispatch too (Medusa/Raccoon/Second Tree/
  Uncrowned/Rosie's Insert Coin all gate on `p->in_y > 0.5f`). Fixed with the same real deadzone
  + snap-to-full-value shape the existing horizontal `pad_x` merge already uses (no hysteresis
  engage/release state machine needed — `in_y` is only ever read as a threshold gesture, not
  smoothed continuous locomotion the way horizontal movement is). Real, honest sign-convention
  note: SDL's own `SDL_CONTROLLER_AXIS_LEFTY` is positive when the stick is pushed DOWN, negated
  here to match `sy`'s own "hold up is positive" convention. `gcc -fsyntax-only -Wall` clean;
  `bash scripts/build.sh` clean, all 4 physics tests still pass (this fix is input-layer only,
  doesn't touch `physics.h`). README's own Controls section updated to document real gamepad
  parity.

## 2026-09-03 (8)
- docs(WOTAN_HAT_STORE_NORTHSTAR): new Phase 2.5 DONE (kanban `WTHS-012010`). Real GFD Town
  proxy shipped -- `hatshop`/`hatshop buy`/`hatshop mine` MUD commands buy real BRAWLPIT hats
  with real Flow from inside GFD's own Town (GoblinFoxDragon commit `dafccba`), a real, separate
  purchase surface alongside the eventual WOTAN web page (Phase 2), not a replacement for it.

## 2026-09-03 (7)
- docs(WOTAN_HAT_STORE_NORTHSTAR): Phases 0-1 updated to DONE (kanban `WTHS-0000`/`WTHS-0010`).
  Real, decisive correction to this doc's own original Phase 0 claim: a real, external Flow
  balance-query + spend API already existed in IDUNA (`GET /api/v1/characters/by-player/
  :player_id` -> `gold_balance`, synced from GFD's own `apps2/mud/main.go`) -- no new work
  needed. Phase 1 (real hat catalog + `character_hats` ownership, `IDUNA` commit `5bf170c`)
  shipped for real, seeded from `OKEMILY/hats.html`'s own already-designed mockup. Corrected the
  doc's own earlier "candidate home: IDUNA_PRO" guess to the real, actual home (IDUNA's own
  existing MMO schema, matching `characters`/`items` convention directly). Phases 2-4 (store
  page, BRAWLPIT rendering, pixel editor) still not built.

## 2026-09-03 (6)
- docs: `docs/BP_LOBBY_MATCHMAKING_NORTHSTAR.md` — real scoping pass for kanban `BP-LOBBY-001`
  ("portal you jump in to find matchmaking, auto-fill 8 random players, no chat/no lives, combat
  abilities work but don't damage"), per Principle 19 (big, unscoped ask gets scoped, not
  swallowed whole). Real investigation, not assumption: `MAX_CLIENTS` is already 8, but the
  actual arena client netcode is stubbed — `net_send_cmd`/`net_tick` are commented out
  (`apps/lobby/src/main.c:983-985`), meaning "online" mode predicts locally but never really
  networks players today. Named this the real, blocking Phase 0 underneath the whole ask, then
  phased the rest (server matchmaking queue w/ bot-fill timeout reusing existing `bot_think`,
  client-side portal trigger volume, a new `MODE_SANDBOX` damage/knockback choke-point flag,
  and "no chat" as an explicit non-task since no chat system exists to remove). Registered in
  `EMILY/context/golden-docs-index.md`; real sub-tasks logged in `EMILY/BACKLOG.md`.

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
