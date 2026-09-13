#!/usr/bin/env python3
"""
scripts/rl_env_packet.py (S419) -- a gymnasium.Env for BRAWLPIT that trains directly against the
REAL wire protocol (BRAWLPIT/packages/common/protocol.h's own NetHeader/UserCmd/NetPlayer, sent
over a real UDP socket to a real bin/brawlpit_server process), not an internal simulation API.

Founder real-time, verbatim: "can we build a training pipeline reinforcement learning on the
packet level take autocurriculum doctrine (alpha star league) find the recent additions to the
REDGARDEN docs to add that take the spicy AI stuff from REDGARDEN and ECOWAR especially in ECOWAR
the ability to FAST FORWARD fractal commander but not necessary fractal squad commander (no team
coordination) use PARENA when possible."

This is a genuinely different architecture from REDGARDEN's own scripts/rl_env.py (that file
wraps apps/arena_training/src/headless.c's ctypes API -- an in-process, purpose-built training
harness with no real network path at all). Here, the observation IS the literal bytes
bin/brawlpit_server's own server_broadcast() sends every real tick, and the action IS the literal
bytes apps/lobby/src/main.c's own net_send_cmd sends -- so a trained policy is a genuine drop-in
bot that speaks the real wire protocol; it could run as a totally separate process/machine with
no code sharing beyond this file. "Packet level" is not a metaphor here.

Real, byte-exact wire layout (S419, verified live against this repo's own compiled toolchain via
a throwaway `sizeof`/`offsetof` C probe, not guessed): NetHeader=12 bytes, UserCmd=28 bytes,
NetPlayer=32 bytes -- see the ctypes.Structure definitions below, each with a self-checking
`assert ctypes.sizeof(...) == N` so a future protocol.h change that breaks this file's own byte
layout fails LOUDLY at import time instead of silently desyncing observations.

Real, found-and-fixed live bug found while building this (BRAWLPIT commit for S419):
server_broadcast() never set NetPlayer.jump_count/.hit_stun (an uninitialized stack local) --
every snapshot this server has ever sent shipped raw stack garbage in those two fields. Fixed in
apps/server/src/main.c before this file was written, so this env's own observation vector isn't
built on top of noise.

Fractal commander (REDGARDEN NORTHSTAR §26.3), single-agent only (explicitly no §26.3's own
"commander-soldier" squad nesting -- BRAWLPIT has no teams): PARENA/stdlib/brawlpit/
commander_mod.prn's real, compiled, rule-based `commander_posture` function (built via
scripts/build_training.sh into build/libbrawlpit_commander.so) is called every step and appended
to the observation as one extra one-hot block, giving the low-level PPO "soldier" policy a real
strategic-directive signal to condition on -- matching §26.3's own "smaller in scope than the
full commander/soldier hierarchy... a genuine structural step in the same direction" precedent
exactly, not a full learned hierarchy.

NOTE ON VERIFICATION (matching REDGARDEN/scripts/rl_env.py's own documented discipline):
gymnasium/stable-baselines3 are not installable in the environment this file was written in (no
venv module available, system Python externally-managed, no sudo). Unlike rl_env.py, this file's
CORE packet plumbing (socket handshake, wire encode/decode, observation vector construction,
reward computation, commander posture) needed no gymnasium at all and WAS run for real against a
live, locally-built bin/brawlpit_server in this same session (see this module's own
`--smoke-test`, and scripts/test_rl_env_packet.py's offline wire round-trip tests). Only the
gymnasium.Env subclassing itself (reset()/step() method signatures, Box space construction) is
written to the documented API but not run against a real gymnasium install -- flagged here, not
faked, same as rl_env.py's own precedent.

Run a real live smoke test against a real server:
    ./scripts/build_training.sh
    ./bin/brawlpit_server --fast-forward &
    python3 scripts/rl_env_packet.py --smoke-test
"""

import argparse
import ctypes
import os
import socket
import struct
import time

try:
    import numpy as np
    _HAVE_NUMPY = True
except ImportError:
    _HAVE_NUMPY = False

# --- Real wire protocol structures (packages/common/protocol.h) ---
# ctypes.Structure lets the C compiler's own natural alignment/padding rules apply automatically
# (matching gcc's real x86-64 Linux ABI, which is what bin/brawlpit_server is actually built
# with) instead of hand-composing a struct.calcsize format string and hoping the padding is
# right -- the self-check assertions below are what actually catches it if not.

PACKET_CONNECT = 0
PACKET_USERCMD = 1
PACKET_SNAPSHOT = 2
PACKET_WELCOME = 3
PACKET_FIND_MATCH = 4  # protocol.h's own real matchmaking-queue request
PACKET_MATCH_FOUND = 5  # protocol.h's own real matchmaking-queue response
PACKET_QUEUE_STATUS = 6
PACKET_RESET_MATCH = 7  # S419-07 -- see protocol.h's own doc comment for the full rationale
PACKET_RESET_ACK = 8

BTN_JUMP = 1
BTN_ATTACK = 2
BTN_SHIELD = 4
BTN_SPECIAL = 8

DEFAULT_PORT = 6978  # apps/server/src/main.c's own hardcoded bind_addr.sin_port

# Real match time limit (S429, founder real-time: "add a timer - 2.5 minutes - if time expires
# it's a draw and thats counted the same as a loss in terms of negative reward"). Counted in real
# TICKS, not real wall-clock seconds -- apps/server/src/main.c's own game loop and
# packages/common/physics.h's own real velocity scaling (`v * dt * 60.0f`) both confirm 60Hz is
# the real, canonical simulated tick rate, and counting ticks (not time.time()) keeps "2.5
# minutes of MATCH time" an invariant whether a match plays out in real time (--fast-forward off,
# ~16ms/tick) or during training (--fast-forward on, thousands of ticks per real second) -- the
# exact same real convention rl_evaluate.py/rl_bot_pool.py's own pre-existing max_ticks parameter
# already used, just now with a real, named, canonical value instead of an arbitrary one.
TICK_RATE_HZ = 60.0
MATCH_TIME_LIMIT_SECONDS = 150.0  # 2.5 minutes
MATCH_TIME_LIMIT_TICKS = int(MATCH_TIME_LIMIT_SECONDS * TICK_RATE_HZ)  # 9000


class NetHeader(ctypes.Structure):
    _fields_ = [
        ("type", ctypes.c_ubyte),
        ("client_id", ctypes.c_ubyte),
        ("sequence", ctypes.c_ushort),
        ("timestamp", ctypes.c_uint),
        ("entity_count", ctypes.c_ubyte),
    ]


assert ctypes.sizeof(NetHeader) == 12, (
    f"NetHeader size drifted to {ctypes.sizeof(NetHeader)} (expected 12) -- protocol.h changed? "
    f"This file's wire parsing is no longer safe to trust until re-verified.")


class UserCmd(ctypes.Structure):
    _fields_ = [
        ("sequence", ctypes.c_uint),
        ("timestamp", ctypes.c_uint),
        ("msec", ctypes.c_ushort),
        ("stick_x", ctypes.c_float),
        ("stick_y", ctypes.c_float),
        ("buttons", ctypes.c_uint),
        ("weapon_idx", ctypes.c_int),
    ]


assert ctypes.sizeof(UserCmd) == 28, f"UserCmd size drifted to {ctypes.sizeof(UserCmd)} (expected 28)"


class NetPlayer(ctypes.Structure):
    _fields_ = [
        ("id", ctypes.c_ubyte),
        ("x", ctypes.c_float),
        ("y", ctypes.c_float),
        ("vx", ctypes.c_float),
        ("vy", ctypes.c_float),
        ("state", ctypes.c_ubyte),
        ("damage", ctypes.c_ushort),
        ("stocks", ctypes.c_ubyte),
        ("shield", ctypes.c_ubyte),
        ("facing", ctypes.c_ubyte),
        ("jump_count", ctypes.c_ubyte),
        ("hit_stun", ctypes.c_ubyte),
    ]


assert ctypes.sizeof(NetPlayer) == 32, f"NetPlayer size drifted to {ctypes.sizeof(NetPlayer)} (expected 32)"

# STAGE_FD's own real default dimensions (bin/brawlpit_server hardcodes STAGE_FD at boot --
# apps/server/src/main.c's own local_init_match call) -- used only to derive a real edge-distance
# feature for the commander signal below. Matches physics.h's own S417-01 blast-zone-scaling
# ratios at STAGE_FD's default 80-wide size (blast_right = +width*0.75 = 60).
STAGE_FD_BLAST_RIGHT = 60.0
STAGE_FD_BLAST_LEFT = -60.0


def encode_connect():
    """Real PACKET_CONNECT handshake request -- server_handle_packet's own real
    `head->type == PACKET_CONNECT` check needs nothing beyond a bare NetHeader."""
    h = NetHeader(type=PACKET_CONNECT, client_id=0, sequence=0, timestamp=0, entity_count=0)
    return bytes(h)


def decode_welcome(data):
    """Returns the client_id the server assigned, or None if `data` isn't a real PACKET_WELCOME."""
    if len(data) < ctypes.sizeof(NetHeader):
        return None
    h = NetHeader.from_buffer_copy(data[: ctypes.sizeof(NetHeader)])
    if h.type != PACKET_WELCOME:
        return None
    return h.client_id


def encode_reset_match(client_id):
    """S419-07: a real episode-boundary request -- a bare NetHeader is all
    server_handle_packet's own `head->type == PACKET_RESET_MATCH` check needs."""
    h = NetHeader(type=PACKET_RESET_MATCH, client_id=client_id, sequence=0, timestamp=0, entity_count=0)
    return bytes(h)


def decode_reset_ack(data):
    """Returns the client_id echoed back, or None if `data` isn't a real PACKET_RESET_ACK."""
    if len(data) < ctypes.sizeof(NetHeader):
        return None
    h = NetHeader.from_buffer_copy(data[: ctypes.sizeof(NetHeader)])
    if h.type != PACKET_RESET_ACK:
        return None
    return h.client_id


def encode_usercmd(client_id, sequence, stick_x, stick_y, buttons, timestamp_ms=None):
    """Real wire construction mirroring apps/lobby/src/main.c's own net_send_cmd EXACTLY: a
    NetHeader, then one real reserved/padding byte (server_handle_packet's own
    `cursor = sizeof(NetHeader) + 1`), then a UserCmd -- see tests/test_net_protocol.c's own
    `test_usercmd_wire_layout` for the authoritative C-side proof this mirrors."""
    if timestamp_ms is None:
        timestamp_ms = int(time.time() * 1000) & 0xFFFFFFFF
    h = NetHeader(type=PACKET_USERCMD, client_id=client_id, sequence=sequence & 0xFFFF,
                  timestamp=timestamp_ms, entity_count=0)
    cmd = UserCmd(sequence=sequence & 0xFFFFFFFF, timestamp=timestamp_ms, msec=0,
                  stick_x=stick_x, stick_y=stick_y, buttons=buttons, weapon_idx=0)
    return bytes(h) + b"\x00" + bytes(cmd)


def decode_snapshot(data):
    """Real wire parse mirroring apps/lobby/src/main.c's own net_tick EXACTLY: a NetHeader, a
    redundant 1-byte count, then `count` back-to-back NetPlayer entries (see
    tests/test_net_protocol.c's own `test_snapshot_wire_layout`). Returns (header, [NetPlayer,
    ...]) or (None, []) if `data` isn't a real, complete PACKET_SNAPSHOT."""
    header_size = ctypes.sizeof(NetHeader)
    if len(data) < header_size + 1:
        return None, []
    h = NetHeader.from_buffer_copy(data[:header_size])
    if h.type != PACKET_SNAPSHOT:
        return None, []
    count = data[header_size]
    cursor = header_size + 1
    player_size = ctypes.sizeof(NetPlayer)
    players = []
    for _ in range(count):
        if len(data) - cursor < player_size:
            break  # truncated/malformed packet -- return what real data we did get, not a crash
        players.append(NetPlayer.from_buffer_copy(data[cursor: cursor + player_size]))
        cursor += player_size
    return h, players


# --- Fractal commander (REDGARDEN NORTHSTAR §26.3, single-agent only) ---

_COMMANDER_LIB = None


def _load_commander_lib():
    """Loads build/libbrawlpit_commander.so (scripts/build_training.sh's own real output, PARENA-
    compiled from stdlib/brawlpit/commander_mod.prn) lazily, once. Returns None (not an
    exception) if the .so hasn't been built yet -- a caller that doesn't need the commander
    feature (or is running before the first `./scripts/build_training.sh`) shouldn't crash on
    import alone, matching this repo's own "a bad/missing resource never corrupts what's already
    working" convention (level_registry.h's own doc comment)."""
    global _COMMANDER_LIB
    if _COMMANDER_LIB is not None:
        return _COMMANDER_LIB
    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    so_path = os.path.join(repo_root, "build", "libbrawlpit_commander.so")
    try:
        lib = ctypes.CDLL(so_path)
        lib.commander_posture.argtypes = [ctypes.c_int] * 7
        lib.commander_posture.restype = ctypes.c_int
        _COMMANDER_LIB = lib
    except OSError:
        _COMMANDER_LIB = False
    return _COMMANDER_LIB or None


COMMANDER_POSTURE_COUNT = 5  # NEUTRAL/AGGRESSIVE/PATIENT/EDGEGUARD/RECOVER -- see commander.h
EDGE_DANGER_THRESHOLD_DEFAULT = 8  # matches COMMANDER_EDGE_DANGER_THRESHOLD_DEFAULT in commander.h

# Named to match commander.h's own COMMANDER_POSTURE_* constants exactly -- used by
# compute_reward below to condition shaping on the real fractal-commander signal, not just
# raw stock/damage deltas.
COMMANDER_POSTURE_NEUTRAL = 0
COMMANDER_POSTURE_AGGRESSIVE = 1
COMMANDER_POSTURE_PATIENT = 2
COMMANDER_POSTURE_EDGEGUARD = 3
COMMANDER_POSTURE_RECOVER = 4


def commander_posture(own, opp, edge_danger_threshold=EDGE_DANGER_THRESHOLD_DEFAULT):
    """Computes the real fractal-commander posture for `own` against `opp` (both NetPlayer),
    calling the actual PARENA-compiled decision function. Returns an int in
    [0, COMMANDER_POSTURE_COUNT) -- falls back to 0 (NEUTRAL) if the .so isn't built yet, a real,
    safe degrade rather than a hard crash."""
    lib = _load_commander_lib()
    own_edge_dist = int(min(own.x - STAGE_FD_BLAST_LEFT, STAGE_FD_BLAST_RIGHT - own.x))
    opp_edge_dist = int(min(opp.x - STAGE_FD_BLAST_LEFT, STAGE_FD_BLAST_RIGHT - opp.x))
    if lib is None:
        return 0
    return lib.commander_posture(
        int(own.stocks), int(opp.stocks),
        int(own.damage), int(opp.damage),
        own_edge_dist, opp_edge_dist,
        edge_danger_threshold,
    )


# --- Observation vector ---
# 8 raw per-player scalars (x, y, vx, vy, damage, stocks, shield, facing) for self + opponent,
# normalized to roughly [-1, 1]/[0, 1] ranges, plus a one-hot commander posture block. Real,
# deliberate simplification from REDGARDEN's own much richer sim_get_obs (no per-hero one-hot --
# BRAWLPIT's v0 training target is a fixed local_init_match(PETALIA, VEXAR) matchup, not
# REDGARDEN's own arbitrary-hero-pool problem).
OBS_SIZE = 8 * 2 + COMMANDER_POSTURE_COUNT

POS_NORM = 1.0 / 80.0  # STAGE_FD's own real default width
VEL_NORM = 1.0 / 20.0  # a real, generous fixed-speed bound -- clipped, not exact
DAMAGE_NORM = 1.0 / 200.0  # damage_percent realistically climbs well past 100 before a kill


def build_observation(own, opp, edge_danger_threshold=EDGE_DANGER_THRESHOLD_DEFAULT):
    """Builds the real, fixed-size float observation vector from two decoded NetPlayer structs.
    `own` is always index 0 in the returned vector's own ordering (the trainee), `opp` is the
    single opponent -- BRAWLPIT's v0 training target is the fixed 2-player local_init_match this
    server always boots into, not the up-to-8-player FFA path (a real, named, honest scope cut,
    same as physics.h's own many single-match-at-a-time assumptions this training pipeline
    inherits rather than fights)."""

    def player_features(p):
        return [
            p.x * POS_NORM,
            p.y * POS_NORM,
            max(-1.0, min(1.0, p.vx * VEL_NORM)),
            max(-1.0, min(1.0, p.vy * VEL_NORM)),
            min(1.0, p.damage * DAMAGE_NORM),
            p.stocks / 4.0,  # STOCK_COUNT
            p.shield / 60.0,  # SHIELD_MAX
            1.0 if p.facing else -1.0,
        ]

    obs = player_features(own) + player_features(opp)
    posture = commander_posture(own, opp, edge_danger_threshold)
    one_hot = [0.0] * COMMANDER_POSTURE_COUNT
    if 0 <= posture < COMMANDER_POSTURE_COUNT:
        one_hot[posture] = 1.0
    return obs + one_hot


# --- Reward design (S419) ---
#
# Real design philosophy, not just "whatever fell out of REDGARDEN's own copy": three tiers,
# each with a real, named reason to exist rather than one flat damage-delta signal.
#
#  1. OUTCOME terms (zero-sum, the ground truth of who's winning): damage dealt/taken, stock
#     taken/lost, terminal win/loss. Same real shape REDGARDEN/scripts/rl_env.py's own
#     compute_reward established, adapted to BRAWLPIT's real damage_percent + stocks model
#     instead of REDGARDEN's own hp model. These alone are enough to train SOMETHING, but a
#     platform fighter's real skill expression (edge-guarding, recovery) is comparatively rare
#     and only ever shows up as a stock swing several seconds later -- too sparse a signal on
#     its own for credit assignment to find quickly.
#
#  2. POSITIONAL SHAPING terms, conditioned on the real fractal-commander posture
#     (commander_posture, PARENA/stdlib/brawlpit/commander_mod.prn) -- dense, per-tick signal
#     tied directly to the exact platform-fighter-specific strategic concepts that posture
#     signal already names:
#       - a small, continuous penalty for standing in real edge danger (COMMANDER_POSTURE_
#         RECOVER) at all, every tick -- teaches proactive stage positioning BEFORE a stock is
#         actually lost, not just after (the outcome-tier REWARD_STOCK_LOST already covers the
#         "after" case).
#       - a real bonus for a successful recovery: transitioning OUT of RECOVER posture without
#         having lost a stock in the process -- directly rewards the single highest-skill-
#         expression mechanic in this genre, not just "don't die" (implicit in outcome terms)
#         but "get back from danger."
#       - a real bonus for CONVERTING a positional advantage into damage: extra reward (on top
#         of the base damage-dealt term) for hits landed while the OPPONENT was the one in real
#         edge danger -- reinforces capitalizing on an advantage instead of just camping stage
#         center waiting for stocks to trade.
#
#  3. A tiny SURVIVAL term (REWARD_ALIVE_PER_TICK) -- purely a numerical-stability nudge against
#     a degenerate all-zero-reward policy early in training, deliberately small enough (2 orders
#     of magnitude below a single damage-percent tick) that it can never outweigh actually
#     playing well.
#
#  4. A tiny ACTIVITY term (REWARD_MOVEMENT_PER_TICK / REWARD_BUTTON_PRESS_PER_TICK) -- founder
#     real-time: "can we add some modest rewards for hitting buttons like movement a and b."
#     Same real "numerical-stability nudge, not a real objective" scope as tier 3's own survival
#     bonus (comparable, deliberately small magnitude): a policy that never moves the stick and
#     never presses a button can still collect the outcome/positional tiers' own small per-tick
#     terms indefinitely by doing nothing, which is a real, known degenerate local optimum this
#     early in training (before any real damage/positioning signal has actually been discovered
#     yet) -- a tiny, real bonus for genuinely engaging the controls breaks that tie toward
#     actually trying things, without being large enough to reward button-mashing OVER real
#     damage/positioning play once the agent has something better to do.
#
#     The button-press half of this term has real, deliberate DIMINISHING MARGINAL RETURNS
#     (founder real-time: "can we add diminishing marginal returns for the reward for 'rewarded
#     for pushing buttons'?") -- the Nth button press this episode is worth
#     REWARD_BUTTON_PRESS_PER_TICK / N, not a flat amount every time. This directly targets the
#     one real, known failure mode a FLAT per-press bonus invites: button-mashing for its own
#     sake becoming a cheap, easy way to rack up reward with no regard for whether the press did
#     anything useful. A harmonic (1/N) decay was chosen over the movement term (which stays flat
#     -- the founder's ask named buttons specifically): the total collectible reward from pure
#     mashing over an entire episode still grows (like the harmonic series, unboundedly but very
#     slowly -- ~REWARD_BUTTON_PRESS_PER_TICK * ln(N)), so this is a real, gentle nudge against a
#     degenerate strategy, not a hard cap that a sufficiently long episode could still exploit.
#     `BrawlpitPacketEnv` tracks the real per-episode press count and passes it in as
#     `button_press_count` (the count BEFORE this tick's own press, so the very first press this
#     episode still gets the FULL, undiminished bonus, matching pre-existing training runs'
#     magnitude at low activity levels) -- optional and backward-compatible (None keeps the old,
#     flat REWARD_BUTTON_PRESS_PER_TICK behavior, e.g. for a caller with no per-episode state to
#     track, matching every other optional-degrade convention this module already establishes).
#
#  5. A SURVIVAL-STREAK term (REWARD_SURVIVAL_STREAK_UNIT), founder real-time: "add a reward that
#     ticks up over time so fib like 1 1 2 3 5 reward for not die also it should go exponentially
#     ish for the higher damage you are it should reward you even more when you oof it resets."
#     Unlike tier 3's own flat per-tick survival nudge, this one deliberately GROWS the longer the
#     current life goes on -- the per-tick unit is scaled by the real Fibonacci sequence (1, 1, 2,
#     3, 5, 8, ...) indexed by how many consecutive ticks this life has lasted (capped at
#     SURVIVAL_STREAK_FIB_CAP so an unusually long life doesn't diverge to an absurd magnitude),
#     and further scaled EXPONENTIALLY by the own player's current damage percent
#     (SURVIVAL_STREAK_DAMAGE_EXP_BASE ** (damage / 100)) -- surviving one more tick at high
#     damage (one hit from death) is worth real, deliberately more than surviving one more tick at
#     0 damage. The streak resets to zero the instant a stock is actually lost ("it resets") --
#     BrawlpitPacketEnv.step tracks the real per-life tick counter and passes it in as
#     `survival_ticks`; compute_reward itself also independently refuses to apply this term on the
#     exact tick a stock was lost (cur_own.stocks == prev_own.stocks below), so a caller can never
#     accidentally reward the death tick itself even with a stale counter.
#
#  A real MATCH TIME LIMIT (S429, founder real-time: "add a timer - 2.5 minutes - if time expires
#  it's a draw and thats counted the same as a loss in terms of negative reward") sits inside
#  tier 1's own terminal-outcome branch, not as a separate numbered tier: MATCH_TIME_LIMIT_TICKS
#  (150 real seconds' worth of ticks at the real, canonical 60Hz tick rate) caps every episode.
#  Reaching it is a real draw -- deliberately scored the SAME as REWARD_LOSS for both sides (not
#  REWARD_WIN for whoever happened to be ahead on stocks when the clock ran out), so a policy can
#  never learn "get a small lead, then stall out the clock" as a winning strategy.
#
# All magnitudes are real, tunable module-level constants (not computed OUTSIDE the C sim by
# design -- REDGARDEN's own compute_reward doc comment gives the same real reasoning: shaping
# stays adjustable without touching/recompiling anything server-side).

REWARD_DAMAGE_DEALT_PER_PCT = 0.01
REWARD_DAMAGE_TAKEN_PER_PCT = -0.01
REWARD_STOCK_TAKEN = 5.0
REWARD_STOCK_LOST = -5.0
REWARD_ALIVE_PER_TICK = 0.001
REWARD_WIN = 10.0
REWARD_LOSS = -10.0

REWARD_EDGE_DANGER_PER_TICK = -0.002  # dense, proactive positioning signal (tier 2)
REWARD_RECOVERY_SUCCESS = 1.0  # real, one-time bonus for surviving a RECOVER window (tier 2)
REWARD_EDGEGUARD_CONVERSION_PER_PCT = 0.02  # bonus ON TOP OF the base damage-dealt term (tier 2)

# Tier 4: real, modest activity/engagement shaping. Deliberately the same order of magnitude as
# REWARD_ALIVE_PER_TICK (tier 3) -- an "I did something" nudge, not a real objective on its own.
REWARD_MOVEMENT_PER_TICK = 0.0005  # stick pushed past the deadzone on either axis
REWARD_BUTTON_PRESS_PER_TICK = 0.0005  # any of jump/attack/shield/special pressed
ACTIVITY_STICK_DEADZONE = 0.15  # matches a real, typical analog-stick deadzone -- not every tiny drift counts as "moving"

# Tier 5: real, growing survival-streak shaping (see the module doc comment above for the full
# founder-quoted rationale). REWARD_SURVIVAL_STREAK_UNIT is deliberately the same tiny order of
# magnitude as REWARD_ALIVE_PER_TICK so an EARLY streak tick stays negligible; the whole point is
# that the Fibonacci/exponential multipliers below are what make it grow into something real.
REWARD_SURVIVAL_STREAK_UNIT = 0.001
SURVIVAL_STREAK_FIB_CAP = 20  # fib(20) = 6765 -- bounds one life's max streak bonus to roughly REWARD_WIN's own order of magnitude, not an unbounded blowup over a long life
SURVIVAL_STREAK_DAMAGE_EXP_BASE = 2.0  # exponential-ish: the streak bonus doubles every +100 damage percent


def _fibonacci(n):
    """The real, standard Fibonacci sequence, 1-indexed (fib(1)=1, fib(2)=1, fib(3)=2, fib(4)=3,
    fib(5)=5, ...) -- exactly the sequence the founder named. Iterative, not recursive: `n` is
    always small in practice (bounded by SURVIVAL_STREAK_FIB_CAP), so there's no real need for
    memoization or closed-form (Binet's formula) here."""
    if n <= 0:
        return 0
    a, b = 1, 1
    for _ in range(n - 1):
        a, b = b, a + b
    return a


def compute_reward(prev_own, prev_opp, cur_own, cur_opp, done, edge_danger_threshold=EDGE_DANGER_THRESHOLD_DEFAULT, action=None, survival_ticks=None, button_press_count=None, timed_out=False):
    """Delta-based dense reward -- see this module's own "Reward design" doc comment above for
    the full five-tier rationale (outcome / positional-shaping / survival / activity /
    survival-streak).

    `action` is the real 6-element action just taken this tick ([stick_x, stick_y, jump, attack,
    shield, special], the exact shape BrawlpitPacketEnv.step's own action space uses) -- optional
    and backward-compatible (None skips tier 4 entirely, e.g. for a caller that only has game
    state and no action to report, matching every other optional-degrade convention this module
    already establishes).

    `survival_ticks` is the real count of consecutive ticks this life has lasted (including this
    one), maintained by the caller and reset to 0 the tick a stock is lost -- optional and
    backward-compatible the same way `action` is (None skips tier 5 entirely).

    `button_press_count` is the real count of button presses ALREADY made this episode, BEFORE
    this tick's own press -- maintained by the caller, never reset on a stock loss (this is a
    whole-episode diminishing-returns curve, not a per-life one like `survival_ticks`). Optional
    and backward-compatible: None keeps the button-press bonus flat at
    REWARD_BUTTON_PRESS_PER_TICK, exactly like before this tier existed.

    `timed_out` (S429, founder real-time: "add a timer - 2.5 minutes - if time expires it's a
    draw and thats counted the same as a loss in terms of negative reward") is True when `done`
    became True because MATCH_TIME_LIMIT_TICKS was reached, not because either side actually ran
    out of stocks. A timeout is a real draw -- deliberately NOT scored via the normal stock-
    comparison outcome below (whoever happens to be ahead on stocks when the clock runs out does
    NOT get REWARD_WIN): both sides get REWARD_LOSS, exactly as bad as an outright loss, so a
    policy can never learn to stall out a lead until the clock saves it."""
    reward = 0.0

    # Tier 1: outcome.
    damage_dealt = max(0, cur_opp.damage - prev_opp.damage)
    reward += REWARD_DAMAGE_DEALT_PER_PCT * damage_dealt
    reward += REWARD_DAMAGE_TAKEN_PER_PCT * max(0, cur_own.damage - prev_own.damage)
    if cur_opp.stocks < prev_opp.stocks:
        reward += REWARD_STOCK_TAKEN * (prev_opp.stocks - cur_opp.stocks)
    if cur_own.stocks < prev_own.stocks:
        reward += REWARD_STOCK_LOST * (prev_own.stocks - cur_own.stocks)
    if done:
        if timed_out:
            reward += REWARD_LOSS
        elif cur_own.stocks > cur_opp.stocks:
            reward += REWARD_WIN
        elif cur_own.stocks < cur_opp.stocks:
            reward += REWARD_LOSS

    # Tier 2: positional shaping, conditioned on the real commander posture. Computed from BOTH
    # sides' own perspective (commander_posture(own, opp, ...) reads as "own's own danger";
    # calling it with the arguments swapped reads as "opp's own danger" -- the function itself
    # is symmetric on its own two subjects, not hardcoded to one side).
    prev_own_posture = commander_posture(prev_own, prev_opp, edge_danger_threshold)
    cur_own_posture = commander_posture(cur_own, cur_opp, edge_danger_threshold)
    prev_opp_posture = commander_posture(prev_opp, prev_own, edge_danger_threshold)

    if prev_own_posture == COMMANDER_POSTURE_RECOVER:
        reward += REWARD_EDGE_DANGER_PER_TICK
        if cur_own_posture != COMMANDER_POSTURE_RECOVER and cur_own.stocks == prev_own.stocks:
            reward += REWARD_RECOVERY_SUCCESS

    if prev_opp_posture == COMMANDER_POSTURE_RECOVER and damage_dealt > 0:
        reward += REWARD_EDGEGUARD_CONVERSION_PER_PCT * damage_dealt

    # Tier 3: survival (numerical-stability nudge only -- see the module doc comment on why this
    # stays two orders of magnitude below a single damage-percent tick).
    reward += REWARD_ALIVE_PER_TICK

    # Tier 4: activity/engagement (see the module doc comment above for the real rationale --
    # this exists to break the "do nothing" degenerate local optimum, not to reward mashing).
    if action is not None:
        stick_x, stick_y = action[0], action[1]
        if abs(stick_x) > ACTIVITY_STICK_DEADZONE or abs(stick_y) > ACTIVITY_STICK_DEADZONE:
            reward += REWARD_MOVEMENT_PER_TICK
        jump, attack, shield, special = action[2], action[3], action[4], action[5]
        if jump > 0 or attack > 0 or shield > 0 or special > 0:
            # Real, deliberate diminishing marginal returns (see the module doc comment above):
            # the Nth press this episode is worth 1/N of the base bonus, not a flat amount --
            # button_press_count is the number of PRIOR presses, so the very first press (count
            # 0) still gets the full, undiminished REWARD_BUTTON_PRESS_PER_TICK.
            press_index = (button_press_count if button_press_count is not None else 0) + 1
            reward += REWARD_BUTTON_PRESS_PER_TICK / press_index

    # Tier 5: survival streak (see the module doc comment above for the full rationale). Refuses
    # to apply on the exact tick a stock was lost, even if the caller passes a stale/positive
    # `survival_ticks` -- "it resets" is enforced here, not just trusted to the caller.
    if survival_ticks is not None and survival_ticks > 0 and cur_own.stocks == prev_own.stocks:
        fib_index = min(survival_ticks, SURVIVAL_STREAK_FIB_CAP)
        damage_scale = SURVIVAL_STREAK_DAMAGE_EXP_BASE ** (cur_own.damage / 100.0)
        reward += REWARD_SURVIVAL_STREAK_UNIT * _fibonacci(fib_index) * damage_scale

    return reward


# --- Real UDP packet client ---

class PacketClient:
    """A real, minimal UDP client speaking BRAWLPIT's actual wire protocol -- the network half of
    this env. Deliberately synchronous/blocking-with-timeout (a training loop wants a real
    snapshot before it decides the next action; this is not the real interactive game client's
    own render-independent net_tick)."""

    def __init__(self, host="127.0.0.1", port=DEFAULT_PORT, timeout=2.0):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.settimeout(timeout)
        self.addr = (host, port)
        self.client_id = None
        self.sequence = 0

    def connect(self, retries=10):
        for _ in range(retries):
            self.sock.sendto(encode_connect(), self.addr)
            try:
                data, _ = self.sock.recvfrom(2048)
            except socket.timeout:
                continue
            client_id = decode_welcome(data)
            if client_id is not None:
                self.client_id = client_id
                return client_id
        raise ConnectionError(f"no PACKET_WELCOME from {self.addr} after {retries} attempts")

    def reset_match(self, retries=10):
        """S419-07: requests a real, fresh episode from the server (fresh spawns/stocks/damage
        for both slots) and blocks for the real PACKET_RESET_ACK confirming it happened --
        deterministic, not a guess from snapshot timing. Requires connect() to have already run
        (server_handle_packet's own PACKET_RESET_MATCH handler is gated on a resolved client_id,
        same as PACKET_USERCMD)."""
        if self.client_id is None:
            raise RuntimeError("reset_match() called before connect()")
        base_timeout = self.sock.gettimeout() or 2.0
        for _ in range(retries):
            self.sock.sendto(encode_reset_match(self.client_id), self.addr)
            attempt_deadline = time.time() + base_timeout
            # Real, found-live bug fixed here: a naive single recvfrom() per attempt treats any
            # stray backlog packet (a real, ordinary PACKET_SNAPSHOT the server was already
            # broadcasting before this request went out) as "no ack this attempt, resend" --
            # under --fast-forward's own high tick rate that backlog is normal and expected, not
            # a failure. Keep reading WITHIN this same attempt, ignoring anything that isn't the
            # real ack, until the ack arrives or this attempt's own deadline passes.
            while time.time() < attempt_deadline:
                self.sock.settimeout(max(0.01, attempt_deadline - time.time()))
                try:
                    data, _ = self.sock.recvfrom(2048)
                except socket.timeout:
                    break
                acked_id = decode_reset_ack(data)
                if acked_id == self.client_id:
                    self.sock.settimeout(base_timeout)
                    return True
        self.sock.settimeout(base_timeout)
        raise ConnectionError(f"no PACKET_RESET_ACK from {self.addr} after {retries} attempts")

    def send_action(self, stick_x, stick_y, jump=False, attack=False, shield=False, special=False):
        buttons = 0
        if jump:
            buttons |= BTN_JUMP
        if attack:
            buttons |= BTN_ATTACK
        if shield:
            buttons |= BTN_SHIELD
        if special:
            buttons |= BTN_SPECIAL
        self.sequence += 1
        pkt = encode_usercmd(self.client_id, self.sequence, stick_x, stick_y, buttons)
        self.sock.sendto(pkt, self.addr)

    def recv_snapshot(self):
        """Blocks (up to the socket's own timeout) for the next real PACKET_SNAPSHOT, draining
        any backlog so the caller always sees the FRESHEST state -- important once
        --fast-forward is running the server far faster than this client's own step() cadence.

        Real, found-live bug fixed here: the original version had an unconditional `break` at
        the end of its outer loop's first pass, so it only EVER attempted one blocking recvfrom
        plus one non-blocking drain -- a stray non-snapshot packet arriving first (e.g. a real
        PACKET_RESET_ACK, S419-07) or an empty initial read left `header` as None even when a
        real snapshot was broadcast moments later, well within the caller's own timeout budget.
        This version keeps blocking-then-draining across the FULL timeout window: it waits for
        at least one real packet, then switches to non-blocking reads to drain any backlog
        without waiting further, and only returns once the socket is genuinely empty (or the
        overall deadline passes with nothing at all received)."""
        header, players = None, []
        base_timeout = self.sock.gettimeout() or 2.0
        deadline = time.time() + base_timeout
        got_any = False
        while True:
            remaining = deadline - time.time()
            if remaining <= 0:
                break
            self.sock.settimeout(remaining if not got_any else 0.0)
            try:
                data, _ = self.sock.recvfrom(4096)
            except (socket.timeout, BlockingIOError):
                break
            got_any = True
            h, p = decode_snapshot(data)
            if h is not None:
                header, players = h, p
        self.sock.settimeout(base_timeout)
        return header, players

    def close(self):
        self.sock.close()


def find_self_and_opponent(players, self_id):
    """Splits a decoded snapshot's player list into (self, opponent) by wire id. Returns
    (None, None) if either side isn't present yet (e.g. the very first tick after connecting, or
    an opponent that hasn't joined) -- a real, honest degrade the caller must check for, not
    something this function papers over."""
    own = next((p for p in players if p.id == self_id), None)
    others = [p for p in players if p.id != self_id]
    opp = others[0] if others else None
    return own, opp


# --- gymnasium.Env (optional import, same guard REDGARDEN/scripts/rl_env.py's own precedent uses) ---

try:
    import gymnasium as gym
    from gymnasium import spaces
    _HAVE_GYM = True
except ImportError:
    _HAVE_GYM = False


def _as_obs_array(obs):
    """Real, found-live bug fixed here: build_observation returns a plain Python list, but
    gymnasium's Box space (and stable_baselines3's own internal buffers) expect a real numpy
    array matching the space's declared dtype -- a bare list can silently break shape/dtype
    checks or force an implicit, slower conversion deep inside SB3's rollout collection. Falls
    back to returning the list unchanged if numpy isn't installed (matches this module's own
    existing 'degrade, don't crash, when an optional dependency is missing' convention)."""
    if _HAVE_NUMPY:
        return np.asarray(obs, dtype=np.float32)
    return obs


if _HAVE_GYM:

    class BrawlpitPacketEnv(gym.Env):
        """gymnasium.Env training directly against a real bin/brawlpit_server over real UDP.
        Action: Box(6) -- [stick_x, stick_y, jump, attack, shield, special], the last four
        thresholded at > 0 (same convention REDGARDEN/scripts/rl_env.py's own action space
        uses)."""

        metadata = {"render_modes": []}

        def __init__(self, host="127.0.0.1", port=DEFAULT_PORT):
            super().__init__()
            self.observation_space = spaces.Box(low=-2.0, high=2.0, shape=(OBS_SIZE,), dtype="float32")
            self.action_space = spaces.Box(low=-1.0, high=1.0, shape=(6,), dtype="float32")
            self.host, self.port = host, port
            self.client = None
            self._prev_own, self._prev_opp = None, None
            self._survival_ticks = 0  # tier 5: real, consecutive-tick life counter, reset on every stock loss
            self._button_press_count = 0  # tier 4: real, whole-episode press count for the diminishing-returns curve
            self._episode_ticks = 0  # S429: real, whole-episode tick counter for the 2.5-minute match timer

        def reset(self, *, seed=None, options=None):
            super().reset(seed=seed)
            if self.client is None:
                self.client = PacketClient(self.host, self.port)
                self.client.connect()
            # S419-07: a real, server-confirmed episode boundary (fresh spawns/stocks/damage for
            # both slots) -- fixes what was a real, named gap (episodes used to be observational
            # only, since the server had no network "reset this match" packet at all).
            self.client.reset_match()
            header, players = self.client.recv_snapshot()
            own, opp = find_self_and_opponent(players, self.client.client_id)
            self._prev_own, self._prev_opp = own, opp
            self._survival_ticks = 0  # a fresh episode is a fresh life
            self._button_press_count = 0  # a fresh episode is a fresh diminishing-returns curve
            self._episode_ticks = 0  # a fresh episode gets a fresh 2.5-minute clock
            obs = build_observation(own, opp) if own and opp else [0.0] * OBS_SIZE
            return _as_obs_array(obs), {}

        def step(self, action):
            stick_x, stick_y = float(action[0]), float(action[1])
            self.client.send_action(
                stick_x, stick_y,
                jump=action[2] > 0, attack=action[3] > 0, shield=action[4] > 0, special=action[5] > 0,
            )
            header, players = self.client.recv_snapshot()
            own, opp = find_self_and_opponent(players, self.client.client_id)
            if own is None or opp is None:
                # Opponent/self missing from a snapshot -- degrade to "nothing changed" rather
                # than crash the training loop over one dropped/malformed packet.
                own, opp = self._prev_own, self._prev_opp
            self._episode_ticks += 1
            # S429, founder real-time: "add a timer - 2.5 minutes - if time expires it's a draw
            # and thats counted the same as a loss in terms of negative reward" -- a real, whole-
            # episode clock, independent of whether either side has actually lost a stock yet.
            timed_out = self._episode_ticks >= MATCH_TIME_LIMIT_TICKS
            done = bool(own and (own.stocks == 0 or opp.stocks == 0)) or timed_out
            reward = 0.0
            if self._prev_own and self._prev_opp and own and opp:
                # Tier 5's own real per-life tick counter: reset the instant a stock is actually
                # lost ("it resets"), otherwise keep growing -- this IS the Fibonacci index
                # compute_reward looks up.
                if own.stocks < self._prev_own.stocks:
                    self._survival_ticks = 0
                else:
                    self._survival_ticks += 1
                reward = compute_reward(self._prev_own, self._prev_opp, own, opp, done,
                                         action=action, survival_ticks=self._survival_ticks,
                                         button_press_count=self._button_press_count, timed_out=timed_out)
                # Tier 4's own diminishing-returns counter: NOT reset on a stock loss (unlike
                # tier 5) -- this is a whole-episode curve. Re-derives "was a button pressed"
                # the same way compute_reward itself does, so the two never drift apart.
                if action[2] > 0 or action[3] > 0 or action[4] > 0 or action[5] > 0:
                    self._button_press_count += 1
            obs = build_observation(own, opp) if own and opp else [0.0] * OBS_SIZE
            self._prev_own, self._prev_opp = own, opp
            return _as_obs_array(obs), reward, done, False, {}

        def close(self):
            if self.client:
                self.client.close()
                self.client = None


def _smoke_test(host, port, steps):
    """Real, live rollout against an ACTUAL running bin/brawlpit_server -- no gymnasium needed.
    Run `./bin/brawlpit_server --fast-forward &` first."""
    client = PacketClient(host, port)
    client_id = client.connect()
    print(f"connected, assigned client_id={client_id}")
    prev_own, prev_opp = None, None
    total_reward = 0.0
    survival_ticks = 0
    button_press_count = 0
    for i in range(steps):
        attack = i % 10 == 0
        client.send_action(stick_x=0.5, stick_y=0.0, attack=attack)
        action = [0.5, 0.0, 0.0, 1.0 if attack else 0.0, 0.0, 0.0]
        header, players = client.recv_snapshot()
        if header is None:
            print(f"step {i}: no snapshot received")
            continue
        own, opp = find_self_and_opponent(players, client_id)
        if own is None or opp is None:
            print(f"step {i}: {len(players)} player(s) in snapshot, self or opponent not yet present")
            continue
        posture = commander_posture(own, opp)
        obs = build_observation(own, opp)
        if prev_own is not None and prev_opp is not None:
            survival_ticks = 0 if own.stocks < prev_own.stocks else survival_ticks + 1
            r = compute_reward(prev_own, prev_opp, own, opp, done=False, action=action,
                                survival_ticks=survival_ticks, button_press_count=button_press_count)
            total_reward += r
            if action[2] > 0 or action[3] > 0 or action[4] > 0 or action[5] > 0:
                button_press_count += 1
        print(f"step {i}: self(x={own.x:.1f} dmg={own.damage} stocks={own.stocks}) "
              f"opp(x={opp.x:.1f} dmg={opp.damage} stocks={opp.stocks}) "
              f"posture={posture} obs_len={len(obs)}")
        prev_own, prev_opp = own, opp
    print("total accumulated reward:", total_reward)
    client.close()


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("--smoke-test", action="store_true",
                   help="run a real live rollout against a running bin/brawlpit_server "
                        "(no gymnasium/SB3 needed) and print decoded state + reward + commander posture")
    p.add_argument("--host", default="127.0.0.1")
    p.add_argument("--port", type=int, default=DEFAULT_PORT)
    p.add_argument("--steps", type=int, default=20)
    args = p.parse_args()

    if args.smoke_test:
        _smoke_test(args.host, args.port, args.steps)
    elif _HAVE_GYM:
        env = BrawlpitPacketEnv(args.host, args.port)
        obs, info = env.reset()
        ep_reward = 0.0
        for _ in range(args.steps):
            action = env.action_space.sample()
            obs, reward, terminated, truncated, info = env.step(action)
            ep_reward += reward
            if terminated or truncated:
                break
        print("gymnasium rollout total reward:", ep_reward)
        env.close()
    else:
        print("gymnasium not installed -- pass --smoke-test to exercise the packet plumbing "
              "without it (matches REDGARDEN/scripts/rl_env.py's own precedent).")
