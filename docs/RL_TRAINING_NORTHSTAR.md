# BRAWLPIT RL Training — Packet-Level Autocurriculum League (S419)

Founder real-time (routed via `emily observe`, Apple #19326), verbatim: "can we build a training
pipeline reinforcement learning on the packet level take autocurriculum doctrine (alpha star
league) find the recent additions to the REDGARDEN docs to add that take the spicy AI stuff from
REDGARDEN and ECOWAR especially in ECOWAR the ability to FAST FORWARD fractal commander but not
necessary fractal squad commander (no team coordination) use PARENA when possible." Followed by
two real clarifications, addressed directly below: "and we need to save all the snapshots adding
them to the league" and "so each snapshot has the 3 archetypes the normal the exploiter and the
league exploiter so for each snapshot it adds 3 to the league", then "we need to implement elo i
guess".

## 1. Why "packet level" is not a metaphor here

REDGARDEN's own `scripts/rl_env.py` (NORTHSTAR §21) wraps `apps/arena_training/src/headless.c`'s
ctypes API — a purpose-built, in-process training harness with no real network path at all. This
pipeline is architecturally different, on purpose: the observation IS the literal bytes
`bin/brawlpit_server`'s own `server_broadcast()` sends every real tick over UDP
(`packages/common/protocol.h`'s `NetHeader`/`NetPlayer`), and the action IS the literal bytes
`apps/lobby/src/main.c`'s own `net_send_cmd` sends (`UserCmd`). A trained policy is therefore a
genuine drop-in bot that speaks the real wire protocol — it could run as a totally separate
process or machine with zero code sharing beyond `scripts/rl_env_packet.py`.

Real, byte-exact wire layout — verified live against this repo's own compiled toolchain (a
throwaway `sizeof`/`offsetof` C probe), not guessed: `NetHeader`=12 bytes, `UserCmd`=28 bytes,
`NetPlayer`=32 bytes. `scripts/rl_env_packet.py`'s `ctypes.Structure` definitions self-check this
at import time (`assert ctypes.sizeof(...) == N`) so a future `protocol.h` change that breaks the
byte layout fails loudly instead of silently desyncing observations.

**Real, found-and-fixed live bug** (found while building this): `server_broadcast()` never set
`NetPlayer.jump_count`/`.hit_stun` — an uninitialized stack local, so every real snapshot this
server has ever sent shipped raw stack garbage in those two of twelve wire fields. Fixed in
`apps/server/src/main.c` before this pipeline's observation vector was built on top of it.

## 2. Fast-forward (ECOWAR precedent)

`bin/brawlpit_server` gained `--fast-forward` (`apps/server/src/main.c`), mirroring
`ECOWAR/apps/arena_server/src/main.c`'s own real `--fast-forward` flag exactly: skips the
real-time `usleep(16000)` pacing so ticks run back-to-back as fast as the CPU allows.

**Real, honest, named scope cut from ECOWAR's own sibling flag pair**: no `--tick-ms` here.
ECOWAR's `arena_update(dt_ms)` takes an explicit simulated-time-per-tick parameter;
`local_update` (BRAWLPIT's own equivalent, `packages/simulation/local_game.h`) has no such
parameter — its physics stepping assumes a fixed real tick internally. Changing that means
touching core physics timing, a real, separate, riskier change than what `--fast-forward` alone
needs to deliver (raw wall-clock training throughput), so it's left undone.

Also unlike ECOWAR's own `ARENA_PHASE_WAITING`/`LIVE` split, BRAWLPIT's server has no "waiting
for a real UDP handshake" phase to preserve real-time pacing for — `local_init_match` runs once
at boot regardless of client connections, so there's no equivalent gotcha here.

## 3. Fractal commander — single-agent only, no squad layer

REDGARDEN NORTHSTAR §26.3 names a "fractal commander/soldier hierarchy" (nested commander →
commander-soldier → soldier policies, real hierarchical/feudal MARL) and its own real first step:
"a real, rule-based (not learned) team-wide 'Commander' signal that genuinely changes individual
squad decisions... smaller in scope than the full commander/soldier hierarchy... a genuine
structural step in the same direction." Founder direction here is explicit: build that same
single-agent commander layer, but **not** §26.3's own squad-coordination nesting — BRAWLPIT has
no teams.

`PARENA/stdlib/brawlpit/commander_mod.prn` is that layer: a real, pure, PARENA-compiled I32
decision function (`commander_posture`) computing a 5-way posture (NEUTRAL/AGGRESSIVE/PATIENT/
EDGEGUARD/RECOVER) from own/opponent stocks, damage%, and edge distance — edge-safety takes
priority over stock/damage (a stock lead means nothing if you're about to die off-stage), a real
strategic hierarchy specific to platform fighters that a MOBA's own lane-based posture signal
never had to consider. Compiled via `parena build` to `packages/common/commander/
commander_mod.c` ("do not edit by hand"), wrapped as `build/libbrawlpit_commander.so`
(`scripts/build_training.sh`) for `scripts/rl_env_packet.py` to call via ctypes and append as a
one-hot observation feature — one real decision function, shared by the training pipeline and any
future in-game hybrid bot wanting the same signal.

I32-only the whole way (no F32/struct/Vec crossing the `#target` boundary VS0 still can't do —
same real constraint `stdlib/ecowar/frontier_village_mod.prn`'s own header names).

## 4. Autocurriculum / league doctrine (ported from REDGARDEN)

`scripts/rl_league.py` and `scripts/test_rl_league.py` are REDGARDEN's own `rl_league.py`/
`test_rl_league.py` (NORTHSTAR §25.4.1, the real AlphaStar three-role league: MAIN/
MAIN_EXPLOITER/LEAGUE_EXPLOITER, PFSP weighting, a permanent append-only `LeagueManager` JSON
registry) **ported verbatim** — the file has zero REDGARDEN-specific coupling (pure Python,
generic checkpoint paths), matching this monorepo's own established precedent for a related game
reusing the same RL infra wholesale (ECOWAR's own `apps/arena_training/src/headless.c` was itself
ported from SHANKPIT/REDGARDEN's identical shape). All 29 original tests pass unmodified in
BRAWLPIT.

### 4.1 "Save all the snapshots, adding them to the league" — three archetypes per snapshot

Founder clarification: REDGARDEN's own `rl_train_team.py` runs each of the three roles as a
**separate process**, each registering its own checkpoints independently and asynchronously.
BRAWLPIT's own real requirement is different and more literal: **one training "snapshot" cycle
registers all three archetypes together**, as a real, atomic-in-intent group of 3. New
`register_generation_snapshot(league, generation, checkpoint_paths, reset_roles=...)` in
`rl_league.py` is the real, tested mechanism — it raises rather than partially registers if any
of the three (`ALL_ROLES = (MAIN, MAIN_EXPLOITER, LEAGUE_EXPLOITER)`) is missing, and each new
checkpoint **inherits its own role's current Elo** (skill carries forward generation to
generation) unless explicitly flagged in `reset_roles` (Main Exploiter's own real periodic
"reset to a freshly initialized network" moment, which must *not* inherit the old rating).

### 4.2 Elo (new, not in REDGARDEN's own copy yet)

Founder: "we need to implement elo i guess." `rl_league.py` gained a real, standard Elo
implementation (`elo_expected`, `elo_update`, `DEFAULT_ELO=1500`, `ELO_K=32`) plus
`LeagueManager.get_elo`/`.set_elo`/`.record_match_result` — kept deliberately separate from the
existing permanent, append-only checkpoint-registration files (`members/*.json`, which must stay
immutable for the "no locking needed" concurrency design to hold): Elo lives in its own small
per-member `elo/<id>.json` file, rewritten atomically (temp+rename) on every update. Real, named
concurrency limit: the read-modify-write pair in `record_match_result` is not atomic across
processes (only each individual write is) — an accepted, low-probability risk for a training
pipeline, not glossed over. Not yet ported back to REDGARDEN's own copy of `rl_league.py` — a
real, worthwhile follow-up, not done in this pass.

## 5. `scripts/rl_env_packet.py` — the real env

Real UDP `PacketClient` (connect handshake → `PACKET_WELCOME` → send `UserCmd` / recv
`PACKET_SNAPSHOT` each step), a `gymnasium.Env` subclass (`BrawlpitPacketEnv`, optional-import
guarded the same way REDGARDEN's own `rl_env.py` is), and a `--smoke-test` mode needing no
gymnasium at all.

**Live-verified for real in this session**: built `bin/brawlpit_server --fast-forward` and
`build/libbrawlpit_commander.so`, ran the server, and ran `rl_env_packet.py --smoke-test` against
it — real UDP handshake succeeded (assigned a real `client_id`), real snapshots decoded
correctly, real commander posture computed via the actual compiled `.so`, real reward computed
from real stock/damage deltas. All 22 offline wire/observation/reward unit tests
(`scripts/test_rl_env_packet.py`, mirroring `tests/test_net_protocol.c`'s own real C-side proof
from the Python side) and all 46 league/Elo tests (`scripts/test_rl_league.py`) pass.

**Real, honest gap found live, not glossed over**: the smoke test's own connecting client
immediately inherited an already-mid-fight demo slot (`bin/brawlpit_server`'s `main()` boots
directly into one `local_init_match(PETALIA, VEXAR)` match that runs forever — there is no
network "reset this match" packet). The connecting training client's stocks visibly dropped
3→2→1→0 within the first few real ticks, because it was thrown into combat already in progress
rather than a clean, fresh match. **This is the real, current blocker on running an actual
multi-episode PPO training loop**: `BrawlpitPacketEnv.reset()` names this gap directly in its own
docstring rather than pretending episodes actually reset game state. A real fix needs a new
server-side packet (e.g. `PACKET_RESET_MATCH`) that re-runs `local_init_match`/respawns both
fighters on request — scoped as S419's own next real step, not built in this pass (matching
REDGARDEN's own S405-02 precedent of not running a full multi-hour training pass in the same
session that built the mechanism).

## 6. Real, honest status — what's done vs. not

**Done, live-verified**: wire-protocol byte layout (verified + a real found/fixed bug), 
`--fast-forward`, the PARENA commander module (compiled + linked + tested), the ported+extended
league (Elo, 3-archetype-per-snapshot registration), the packet-level env's core plumbing (real
socket round trip, observation, reward) — all backed by real, passing tests (68 Python + 2 new C
test binaries).

**Not done, named honestly**:
- No `PACKET_RESET_MATCH` — see §5's own real, live-found blocker.
- No `rl_train_packet.py` orchestrator actually driving 3 simultaneous PPO models through
  `stable_baselines3` — blocked on the reset gap above (a training loop with no real episode
  boundary can't produce a meaningful policy), and, same as REDGARDEN's own documented
  precedent, `gymnasium`/`stable_baselines3` aren't installable in this sandbox (externally
  managed Python, no sudo/venv).
- No actual multi-hour/multi-generation training run.
- No Bazel build for any of this (S417-05 already tracks BRAWLPIT's own separate Bazel migration
  ask; this pipeline's build lives in `scripts/build_training.sh` for now, matching
  REDGARDEN/ECOWAR's own identical convention).
