# BP-LOBBY-001: Matchmaking Portal — Scoping (Principle 19)

**Kanban card #159**, priority queue: "brawlpit lobby have like a portal you jump in to find
matchmaking and have auto lobbies get filled with 8 random players no chat no lives and combat
abilities work but dont damage other characters."

This is a big, unscoped ask (`EMILY/docs/THE_EMILY_WAY.md` Principle 19) — real investigation
performed before any code was written, real phased plan below, real sub-tasks returned to
`EMILY/BACKLOG.md` rather than the card being marked done on the strength of a plan alone.

## Real, current state (investigated directly, not assumed)

- `apps/server/src/main.c` is a real, live UDP server; `MAX_CLIENTS` is already `8`
  (`packages/common/protocol.h:10`) — the "8 random players" number needs no protocol change.
- Client-side arena networking is **stubbed, not working**: `STATE_GAME_NET` exists and
  `net_connect()` runs, but the actual per-frame traffic is commented out —
  `apps/lobby/src/main.c:983-985`:
  ```c
  // net_send_cmd(cmd);
  // net_tick(); // Receive snapshots
  // TODO(net): apply server-authoritative stage_id from welcome/snapshot before simulation.
  ```
  Today, picking "online" from the lobby predicts locally but never actually sends/receives
  real input or snapshots. **This is the real, foundational blocker underneath BP-LOBBY-001** —
  a matchmaking portal that drops players into a lobby that doesn't actually network them yet is
  not a real feature.
- `docs/net_plan.md` is TIPJAR's own networking plan (server-authoritative sim, client
  prediction, snapshot deltas, late-join) — real and well-reasoned, but written for TIPJAR's
  shift loop, not the arena fighting mode this card needs, and not implemented for either mode
  yet at the transport level.
- No real matchmaking exists anywhere: no queue, no auto-fill, no lobby-of-N discovery. A
  "portal you jump in to find matchmaking" is a real, new client-side interaction (a physical
  trigger volume in the lobby scene) wired to a real, new server-side matchmaking queue —
  neither exists.
- No "no lives, no damage, abilities still work" mode variant exists. Every real special move
  shipped so far (Insert Coin, High Score Rush, Serpents' Grasp, Petrifying Gaze, etc.) calls
  `apply_knockback`/sets `damage_percent`/`hitstun_frames` unconditionally — a real, new
  server-side mode flag is needed to let ability *logic* run (spawn projectiles, trigger dash
  state, etc.) while suppressing the damage/knockback side effects specifically.

## Real, phased plan

**Phase 0 — make arena netcode actually work (blocking, not optional).**
Wire up the real `net_send_cmd`/`net_tick` calls that are currently commented out, following
`docs/net_plan.md`'s own already-agreed shape (server-authoritative sim, client prediction,
reconciliation) applied to the arena `ServerState` instead of TIPJAR's. Definition of done: two
real processes (one `apps/server` instance, two `apps/lobby` clients) fight in real time over
loopback UDP with correct, reconciled state on both ends. Nothing below this line is buildable
without Phase 0.

**Phase 1 — server-side matchmaking queue.**
A real queue on the server: a client sends a "find match" request, the server holds it until
either 8 players have queued or a real timeout fires (then fills remaining slots with bots —
this repo already has real `bot_think` logic in `packages/simulation/local_game.h` to reuse,
not invent). Emits a real "match found" message with connection info once full.

**Phase 2 — client-side portal.**
A real, physical trigger volume in the lobby scene (the existing OpenGL immediate-mode lobby
render, matching how `T` already opens TIPJAR) — walking into it calls the Phase 1 queue
request instead of a keypress. Shows real, live queue status (players found / 8) while waiting.

**Phase 3 — "no lives, no damage" sandbox mode.**
A new real `ServerState.mode` flag (`MODE_SANDBOX` alongside whatever the existing default is
called). When set: `apply_knockback` and any direct `damage_percent`/`hitstun_frames` mutation
is gated on `state->mode != MODE_SANDBOX` (a real, single choke point — `apply_knockback`
already is one for damage; the handful of specials that touch `hitstun_frames` directly, e.g.
`special_petrify_gaze`, need the same guard added individually). Explicitly **not** touching
stock/KO logic removal beyond "no lives" meaning stocks don't deplete — real, minimal: keep
`phys_respawn`'s existing blast-zone/respawn behavior so falling off-stage still resets a
player's position, it just never actually costs a stock in this mode.

**Phase 4 — "no chat."**
Real, honest non-task: there is no chat system in this repo today (checked — no chat UI, no
chat packet type in `protocol.h`). "No chat" for this card means *don't build one*, not remove
one — worth stating explicitly so a future pass doesn't misread the card as asking for chat
moderation work.

## Why this isn't done in one pass

Phase 0 alone (real client-server arena netcode) is a multi-day, high-risk piece of work on its
own — it touches the live prediction/reconciliation model for every existing physics special
built this session (Rosie's dash, Medusa's grasp, etc.), each of which needs to behave
correctly under real network jitter, not just in the synthetic single-process tests that
verified them so far. Phases 1-4 are each real, bounded follow-on slices once Phase 0 lands.
Real sub-tasks are logged in `EMILY/BACKLOG.md` under this card's own section rather than folded
into a single, unscoped "build the lobby" checkbox.
