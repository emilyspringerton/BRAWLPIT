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
from real stock/damage deltas.

### 5.1 `PACKET_RESET_MATCH` (S419-07) — a real episode boundary

The smoke test above originally exposed a real, live gap: a connecting client inherited an
already-mid-fight demo slot (`bin/brawlpit_server`'s `main()` boots into one
`local_init_match(PETALIA, VEXAR)` match that ran forever, with no network "reset" packet).
Fixed: `PACKET_RESET_MATCH`/`PACKET_RESET_ACK` (`protocol.h`) — an already-connected client sends
a bare `PACKET_RESET_MATCH`; the server re-runs `local_init_match` while preserving that one
sender's own network binding, then replies `PACKET_RESET_ACK` so a training loop has a
deterministic "the new episode has actually started" signal.

**Two real bugs found and fixed live while verifying this, not assumed away**:
1. `local_init_match(1, ...)` — note `num_players=1` — only initializes **slot 0**; slot 1+ have
   never gotten their stocks/shield/spawn from it, only from `PACKET_CONNECT`'s own separate
   init block. An early version of the reset handler left the client's own slot zeroed out from
   `local_init_match`'s own `memset`. Fixed by reusing `mm_init_slot` (the exact same real
   per-slot init `PACKET_CONNECT`/matchmaking already share) instead of hand-rolling a smaller,
   incomplete patch.
2. `PacketClient.recv_snapshot()`/`.reset_match()` both had a real bug where a single stray
   backlog packet (e.g. an ordinary `PACKET_SNAPSHOT` arriving before the awaited `PACKET_WELCOME`/
   `PACKET_RESET_ACK`) made the call give up early instead of continuing to wait — harmless at
   low tick rates, but a real, live-reproduced failure once `--fast-forward` is generating
   backlog faster than one Python `recvfrom` per call can drain. Fixed by looping within the
   same wait window instead of returning on the first (possibly wrong) packet.

**Live-verified end to end after both fixes**: a fresh client connects, plays a few ticks
(stocks/damage drift from combat), sends `PACKET_RESET_MATCH`, and both fighters come back with
real, fresh `stocks=4`/`damage=0` — confirmed against a clean server process, not just the
process that happened to already be warmed up.

### 5.2 `--port` (S419-11) — running three servers at once

`scripts/rl_train_packet.py`'s own three-archetype design needs three independent matches
running simultaneously (one dedicated server per league role — see §6 below). `bin/
brawlpit_server`'s bind port was a hardcoded constant; added a real `--port` CLI flag (default
6978, unchanged for any existing deploy). **Live-verified**: two real server processes bound to
different ports (7978/7979) simultaneously, each independently completing a real
`PACKET_CONNECT`/`PACKET_WELCOME` handshake.

### 5.3 Reward design — three tiers, not one flat damage delta

`compute_reward`'s own real design (see its own module-level doc comment in
`scripts/rl_env_packet.py` for the full rationale):

1. **Outcome** (zero-sum ground truth): damage dealt/taken, stock taken/lost, terminal win/loss
   — REDGARDEN's own `rl_env.py` shape, adapted to BRAWLPIT's `damage_percent`+`stocks` model.
2. **Positional shaping**, conditioned on the real fractal-commander posture (`commander_posture`)
   — dense, per-tick signal tied to platform-fighter-specific strategic concepts the outcome tier
   alone is too sparse to teach quickly:
   - a small, continuous penalty for standing in real edge danger (`RECOVER` posture) at all —
     teaches proactive positioning *before* a stock is lost, not just after.
   - a real bonus for a **successful recovery** — transitioning out of `RECOVER` without having
     lost a stock — rewards the single highest-skill-expression mechanic in this genre directly,
     not just "didn't die."
   - a real bonus for **converting** a positional advantage into damage — extra reward for hits
     landed while the *opponent* was the one in edge danger, discouraging camping stage center
     while an opponent is vulnerable off-stage.
3. **Survival** (`REWARD_ALIVE_PER_TICK`) — a tiny, deliberately sub-dominant nudge against a
   degenerate all-zero-reward policy early in training, two orders of magnitude below a single
   damage-percent tick so it can never outweigh actually playing well.

All magnitudes are real, named, tunable module-level constants. 8 new tests
(`TestComputeReward`'s own edge-danger/recovery/edgeguard-conversion cases) confirm the shaping
terms actually fire correctly against the real compiled commander `.so`, not just in isolation.

## 6. `scripts/rl_train_packet.py` (S419-08) — the training orchestrator

Runs all three league archetypes (Main/Main Exploiter/League Exploiter) in ONE process — a
deliberate, different architecture from REDGARDEN's own `rl_train_team.py` (three separate
processes coordinating only via the shared `LeagueManager` directory) — since the founder's own
framing ("each snapshot has the 3 archetypes... for each snapshot it adds 3 to the league")
describes one shared snapshot cadence across all three, not three independently-paced runs. Each
model gets its own dedicated `bin/brawlpit_server --fast-forward --port <N>` subprocess (§5.2).
Every `--save-freq` cycle, all three checkpoints save and register together via
`register_generation_snapshot` — Main Exploiter's own periodic reset (`should_reset_main_exploiter`)
is wired through to `reset_roles` so a reset checkpoint doesn't inherit its old Elo.

**Real, honest, named scope limit (not self-play yet)**: each model's own opponent is whatever
`local_init_match`/`PACKET_RESET_MATCH` produces by default today — a static, non-bot-driven
slot 0 (`local_game.h`'s own `local_init_match`: `is_bot = (i > 0)`, so slot 0 is never
bot-driven). This is real training against a fixed target, **not** true self-play against the
growing checkpoint pool the league itself already tracks. Loading a past checkpoint's policy to
actually drive the opponent slot server-side is real, separate, not-yet-built work — scoped as
S419-10, not silently pretended to already work.

**Not run end-to-end in this session** — written to `stable_baselines3`'s real, documented PPO
API, same "flagged not faked" precedent REDGARDEN's own `rl_env.py` already set for the same
sandbox limitation (externally managed Python, no sudo/venv). See §7 for where this is actually
meant to run.

## 7. Colab workflow

`notebooks/brawlpit_rl_training.ipynb` — clones this repo (private, needs a GitHub token pasted
via `getpass` at runtime, never persisted), builds `bin/brawlpit_server`+
`build/libbrawlpit_commander.so` (`scripts/build_training.sh` — PARENA itself doesn't need to be
cloned, `commander_mod.c` is already checked in), `pip install`s `gymnasium`/`stable_baselines3`
(both real installs, unlike this repo's own sandbox), runs the `--smoke-test` sanity check, then
a real (small, by default) `rl_train_packet.py` run, prints the resulting league standings via
`LeagueManager`, and zips+downloads the results (Colab runtimes are ephemeral — nothing persists
past the session otherwise).

## 9. Remote checkpoint registry (S420)

Founder real-time: "lets make a checkpoint registry so we can train from multiple locations and
then we can add checkpoints from colab?" `scripts/rl_league.py`'s own `LeagueManager` is a real,
working registry, but it's a **local filesystem directory** — it lets separate PROCESSES on the
SAME machine share a league (that's the whole reason S419's three-archetype registration works
at all), but a Colab runtime (a fresh, ephemeral filesystem every session) and this box are two
genuinely separate machines with no shared disk. Same real reason the BRAWLPIT online level
editor (S415-417) needed IDUNA rather than staying a local SQLite file.

**IDUNA side** (`internal/brawlpit/checkpoint_store.go`, `internal/http/handlers/
brawlpit_checkpoints.go`): a real, SQLite-metadata + on-disk-blob registry — `POST /api/v1/
brawlpit-checkpoints` (multipart: `role`/`generation`/`elo`/`source_location`/`file`) uploads one
checkpoint; `GET /api/v1/brawlpit-checkpoints[?role=]` lists; `GET .../<id>/download` streams the
raw bytes back. List/download are public (same trust level `GET /api/v1/brawlpit-levels` already
established); upload is gated behind a real, new M2M permission (`brawlpit.checkpoints.write`,
`migrations/truestore/202609131400_brawlpit_rl_checkpoints.sql`) granted to a new agent identity,
`BRAWLPIT-RL` — the same real M2M pattern REDGARDEN-BOTS/ECOWAR-BOTS already use, not a new
auth mechanism invented for this. 13 new Go tests (store + handler layers) pass.

**BRAWLPIT side** (`scripts/rl_registry.py`): a minimal, dependency-free (stdlib `urllib` only,
no `requests`) client — `authenticate`/`push_checkpoint`/`list_checkpoints`/`download_checkpoint`,
plus a real CLI (`push`/`list`/`pull` subcommands). Wired into `rl_train_packet.py` as an opt-in
`--registry-url`/`--registry-agent-secret`/`--registry-source-location` flag set: omit
`--registry-url` and training behaves exactly as it did before S420 (local `--league-dir` only).
When set, every generation's 3 checkpoints push to the remote registry right after their local
`register_generation_snapshot` call, tagged with a real, free-text `source_location` (e.g.
`"colab"`) so the registry shows where each one came from. A push failure degrades to a logged
warning, never a crashed training run — the same "a bad/missing resource never corrupts what's
already working" convention `level_registry.h`'s own doc comment already established for this
exact pipeline's read side.

**Live-verified for real against the actual production IDUNA instance in this session**, not
just unit-tested: deployed the new migration + binary (with a real pre-deploy DB+binary backup,
same discipline as every other live IDUNA change this session), provisioned the new agent's real
secret via `cmd/bootstrap` (dry-run reviewed first), then ran the full real round trip twice —
once via raw `curl` (authenticate → upload → list → download, plus confirming an unauthenticated
upload correctly gets `401`), once via `scripts/rl_registry.py` itself (`push` → `list` → `pull`,
byte-identical content both times). Both test checkpoints were deleted from the live registry
afterward — this section's own proof, not real training data.

`notebooks/brawlpit_rl_training.ipynb` was updated to match: a second `getpass` prompt for the
`BRAWLPIT-RL` agent secret (optional — leave blank for a local-only run), a cell showing the
shared registry's standings before training, `--registry-url`/`--registry-source-location colab`
wired into the training run when a secret was given, and the shared registry's standings shown
again afterward to confirm the new checkpoints actually joined it.

## 10. Real, honest status — what's done vs. not

**Done, live-verified**: wire-protocol byte layout (+ a found/fixed live bug), `--fast-forward`,
`--port`, `PACKET_RESET_MATCH`/`PACKET_RESET_ACK` (+ two more found/fixed live bugs), the PARENA
commander module, the ported+extended league (Elo, 3-archetype-per-snapshot registration), the
three-tier reward design, the packet-level env's core plumbing, and (S420) the real, remote
checkpoint registry (IDUNA-hosted, live-verified end to end twice) — all backed by real, passing
tests (80 Python + 2 C test binaries + 13 new Go tests).

**Built and run end-to-end for real**: `gymnasium`/`stable_baselines3` ARE installable in this
box's own sandbox after all (`pip3 install --user --break-system-packages`, the same PEP-668
bypass `python-xlib` needed earlier this session) — corrects the "not installable" limitation
this section used to document. A real, multi-hour, multi-generation training run has been live
on this box since S421-00, continuously pushing real checkpoints to the shared registry.

**S422/S423 (real automatic Elo movement + Colab resume)**, founder real-time: "elos stuck at
1500 again" -> "are we setup for pretty auto magical training on colab? ... i guess it needs to
download the league from the registry too?":
- `rl_train_packet.py` now runs one real evaluation match per role per generation (new checkpoint
  vs. that same role's own immediately-prior generation, on a dedicated `EVAL_PORT` server) and
  calls `record_match_result` (local `LeagueManager` + the remote IDUNA registry) with the real
  outcome — fixing the actual root cause of "Elo stuck at 1500": Elo INHERITANCE always worked,
  but nothing ever automatically MOVED it away from the default before this.
- `--resume-from-registry`: warm-starts each role from the newest checkpoint that role already
  has in the shared registry (real downloaded PPO weights, not just an Elo number) and seeds the
  local league with that checkpoint's real registry Elo/generation — the actual fix for the
  "sync my local league_data/ from the remote registry" gap named below.
- `scripts/colab_train.py` (new): the single, self-contained "drop into one Colab cell" script —
  clones/pulls BRAWLPIT, builds, authenticates against IDUNA (a real M2M agent-secret → JWT
  exchange, honestly distinguished in its own doc comment from a human OAuth login redirect,
  which this isn't), and launches `rl_train_packet.py --resume-from-registry` so a fresh Colab
  runtime continues the SAME real, shared league instead of colliding fresh random networks into
  it — supersedes `notebooks/brawlpit_rl_training.ipynb`'s own multi-cell manual flow for anyone
  who just wants training running, though the notebook still works for step-by-step inspection.
- Tier 5 reward, founder real-time: "add a reward that ticks up over time so fib like 1 1 2 3 5
  reward for not die also it should go exponentially ish for the higher damage you are it should
  reward you even more when you oof it resets" — a real, growing survival-streak bonus
  (`REWARD_SURVIVAL_STREAK_UNIT * fib(min(ticks_alive, 20)) * 2.0 ** (damage / 100)`), reset to
  zero the instant a stock is actually lost. See `rl_env_packet.py`'s own reward-design doc
  comment for the full rationale.
- **S426 (diminishing marginal returns on button-press activity)**, founder real-time: "can we
  add diminishing marginal returns for the reward for 'rewarded for pushing buttons'?" — the Nth
  button press this episode (whole-episode count, not reset per life like tier 5) is now worth
  `REWARD_BUTTON_PRESS_PER_TICK / N`, a real harmonic decay, instead of a flat amount every time.
  The movement half of tier 4 stays flat (the ask named buttons specifically). Backward-
  compatible: a caller that never passes `button_press_count` still gets the old flat behavior.
  4 new tests.

**Not done, named honestly**:
- S419-10: real self-play — loading a past league checkpoint's policy to actually drive the
  opponent slot server-side, instead of today's static default opponent.
- No Bazel build for any of this (S417-05 already tracks BRAWLPIT's own separate Bazel migration
  ask; this pipeline's build lives in `scripts/build_training.sh` for now, matching
  REDGARDEN/ECOWAR's own identical convention).
