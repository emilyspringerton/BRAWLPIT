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
import math
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
# physics.h's own real BLAST_TOP/BLAST_BOTTOM -- deliberately asymmetric (falling off the bottom
# is a real, shorter, riskier window than a horizontal edge or the top). Used only by the S430
# hand-tailored time-to-blast feature below.
STAGE_FD_BLAST_TOP = 60.0
STAGE_FD_BLAST_BOTTOM = -40.0


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
# normalized to roughly [-1, 1]/[0, 1] ranges, plus a one-hot commander posture block, plus (S430)
# 10 real, hand-tailored relational features -- see HAND_TAILORED_FEATURE_COUNT's own doc comment
# below. Real, deliberate simplification from REDGARDEN's own much richer sim_get_obs (no
# per-hero one-hot -- BRAWLPIT's v0 training target is a fixed local_init_match(PETALIA, VEXAR)
# matchup, not REDGARDEN's own arbitrary-hero-pool problem).
#
# S430, founder real-time: "how can we switch to RNN with hand tailored features and a critic
# with access to privledged info like opponent health" -- this is the hand-tailored-features half
# of that ask (the RNN swap and the asymmetric privileged critic are real, separate, larger,
# not-yet-built pieces -- see this module's own top-level doc comment / RL_TRAINING_NORTHSTAR.md
# for the full three-part plan and why each piece was sequenced the way it was). These 10 terms
# are relational/derived quantities a network COULD in principle learn to infer from the 21 raw
# scalars above given enough data, but handing them over pre-computed is real, standard reward-
# shaping-adjacent practice (the same reasoning commander_posture's own one-hot block already
# established) -- it turns "learn to notice you're closing distance" into "read one number."
#
# REAL, DELIBERATE BREAKING CHANGE: this changes OBS_SIZE (21 -> 31), so the observation_space
# shape changes. Every checkpoint already in the registry was trained against the OLD 21-dim
# shape -- PPO.load(..., env=<new 31-dim env>) will fail on a shape mismatch, so
# --resume-from-registry cannot warm-start from any pre-S430 checkpoint; a fresh run starts over.
# packages/common/ai_opponent.h's own ai_opponent_build_observation is updated in lockstep (same
# 10 new terms, same order) so native C inference still matches this file exactly -- an OLD,
# still-active checkpoint (in_dim=21) loaded against the NEW client build is safe, not a crash:
# mlp_policy_forward's own existing `obs_size != p->layers[0].in_dim` check (mlp_policy.h) catches
# the mismatch and ai_opponent_drive already no-ops (leaves input untouched) rather than feeding
# garbage -- the same real, pre-existing "checked mismatch degrades safely" contract this file's
# own doc comment already promised, just now actually exercised by a real obs-size bump.
OBS_SIZE = 8 * 2 + COMMANDER_POSTURE_COUNT + 10

POS_NORM = 1.0 / 80.0  # STAGE_FD's own real default width
VEL_NORM = 1.0 / 20.0  # a real, generous fixed-speed bound -- clipped, not exact
DAMAGE_NORM = 1.0 / 200.0  # damage_percent realistically climbs well past 100 before a kill
EDGE_TIME_CAP_TICKS = 300.0  # ~5 real seconds at the confirmed 60Hz tick rate -- a real, generous "danger horizon"; not heading toward a wall within this reads as fully safe


def _time_to_blast_1d(pos, vel, low, high, cap_ticks=EDGE_TIME_CAP_TICKS):
    """Real, hand-engineered danger signal: a constant-velocity estimate (in real ticks, `vel`
    already being units-per-tick per physics.h's own `v * dt * 60.0f` scaling) of when `pos`
    would cross either `low` or `high`. Not moving, or moving away from both bounds, reads as the
    full cap -- "no real danger on this horizon," not zero/undefined."""
    if vel > 1e-6:
        ticks = (high - pos) / vel
    elif vel < -1e-6:
        ticks = (pos - low) / -vel
    else:
        ticks = cap_ticks
    return max(0.0, min(cap_ticks, ticks))


def _time_to_any_blast_normalized(x, y, vx, vy):
    """The real minimum of the horizontal and vertical time-to-blast estimates, normalized to
    [0, 1] -- 0.0 means "about to fly off right now," 1.0 means genuinely safe on the horizon."""
    tx = _time_to_blast_1d(x, vx, STAGE_FD_BLAST_LEFT, STAGE_FD_BLAST_RIGHT)
    ty = _time_to_blast_1d(y, vy, STAGE_FD_BLAST_BOTTOM, STAGE_FD_BLAST_TOP)
    return min(tx, ty) / EDGE_TIME_CAP_TICKS


def _facing_toward(p, dx_from_p):
    """1.0 if `p`'s own facing direction points toward the opponent (`dx_from_p` = opponent.x -
    p.x, from p's own perspective), -1.0 if facing away, 0.0 for the real, honest degenerate case
    of standing exactly on top of each other (no real "toward" direction exists)."""
    if dx_from_p == 0:
        return 0.0
    facing_right = bool(p.facing)
    return 1.0 if facing_right == (dx_from_p > 0) else -1.0


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

    # S430: 10 real, hand-tailored relational features -- see OBS_SIZE's own doc comment above
    # for the full rationale and the real breaking-change note.
    dx = opp.x - own.x
    dy = opp.y - own.y
    distance_raw = math.sqrt(dx * dx + dy * dy)
    if distance_raw > 1e-6:
        rel_vx, rel_vy = opp.vx - own.vx, opp.vy - own.vy
        # Closing speed = -(d(distance)/dt): positive means the gap is shrinking.
        closing_velocity = -(dx * rel_vx + dy * rel_vy) / distance_raw
    else:
        closing_velocity = 0.0

    hand_tailored = [
        max(-2.0, min(2.0, dx * POS_NORM)),
        max(-2.0, min(2.0, dy * POS_NORM)),
        max(0.0, min(2.0, distance_raw * POS_NORM)),
        max(-1.0, min(1.0, closing_velocity * VEL_NORM)),
        _time_to_any_blast_normalized(own.x, own.y, own.vx, own.vy),
        _time_to_any_blast_normalized(opp.x, opp.y, opp.vx, opp.vy),
        _facing_toward(own, dx),
        _facing_toward(opp, -dx),
        max(-1.0, min(1.0, (own.damage - opp.damage) * DAMAGE_NORM)),
        max(-1.0, min(1.0, (own.stocks - opp.stocks) / 4.0)),
    ]
    return obs + one_hot + hand_tailored


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
#     The button-press half of this term is REAL TOKEN-BUCKET RATE LIMITED (S442, replacing an
#     earlier harmonic-decay design -- founder real-time: "it should be token bag based though so
#     spamming as much as possible only gets you so much reward and pausing 20 ish percent of the
#     time you should still get the same reward output for button pushing so at a certain APM you
#     just dont get any more reward anymore for going faster"). ActivityTokenBucket refills at
#     ACTIVITY_TOKEN_REFILL_RATE=0.8 tokens/tick and spends 1 token per real button press (only
#     when a token is actually available) -- 0.8 is exactly the founder's own literal "pausing
#     20% of the time" framing: pressing at or below an 80% duty cycle NEVER runs the bucket dry
#     (each press always finds a token, since refill keeps pace with spend), so it earns the
#     exact same total reward as pressing every single tick would; only EXCEEDING that
#     sustainable rate wastes presses on an empty bucket, capping the real maximum achievable
#     reward regardless of raw press count/APM -- "at a certain APM you just dont get any more
#     reward anymore for going faster," addressed structurally instead of by a decaying formula.
#     `BrawlpitPacketEnv` owns the real bucket instance (reset fresh each episode) and passes in
#     whether THIS tick's press actually spent a real token as `activity_token_spent` -- optional
#     and backward-compatible (None keeps the button-press bonus flat at
#     REWARD_BUTTON_PRESS_PER_TICK for any caller with no bucket to track, matching every other
#     optional-degrade convention this module already establishes).
#
#  An INACTIVITY PENALTY (S442, founder real-time, after directly observing a real trained
#  checkpoint go completely inert -- stick in the deadzone, zero button presses, for the rest of
#  its life after one early stock loss: "do we introduce a strong negative reward that ticks down
#  if no key is pressed for say 4 seconds?") sits alongside tier 4: if NEITHER the stick moves
#  past the deadzone NOR any button is pressed for more than INACTIVITY_TICKS_THRESHOLD=240
#  consecutive ticks (4 real seconds at the confirmed 60Hz tick rate -- generous enough that any
#  real, brief strategic pause never triggers it), a real, flat REWARD_INACTIVITY_PENALTY_PER_TICK
#  applies every tick past that threshold. This is what actually breaks "freezing is the safest
#  strategy" -- tier 4's own activity bonus alone is never negative, so it could never explain or
#  fix a policy that has learned total inaction is safer than the small risk of acting; a real,
#  and eventually large, cost for doing literally nothing is what forces the policy back toward
#  exploring instead of freezing. `BrawlpitPacketEnv` tracks the real whole-episode inactivity
#  streak (reset the instant any real input occurs) and passes it in as `inactivity_ticks`.
#
#  5. A SURVIVAL-STREAK term (REWARD_SURVIVAL_STREAK_UNIT), founder real-time: "add a reward that
#     ticks up over time so fib like 1 1 2 3 5 reward for not die also it should go exponentially
#     ish for the higher damage you are it should reward you even more when you oof it resets."
#     Unlike tier 3's own flat per-tick survival nudge, this one deliberately GROWS the longer the
#     current life goes on -- the per-tick unit is scaled by the real Fibonacci sequence (1, 1, 2,
#     3, 5, 8, 13, ...) indexed by how many consecutive ticks this life has lasted, capped at
#     SURVIVAL_STREAK_FIB_CAP=7 so the PER-TICK value itself stays genuinely tiny once capped
#     (0.013 at 0 damage) -- a real, found, fixed bug (see SURVIVAL_STREAK_FIB_CAP's own doc
#     comment): this cap only bounds the per-tick value, NOT the total across a long life (the
#     term still applies every tick a life continues), so an earlier, much higher cap (20) let a
#     long-surviving life rack up tens of thousands of total reward for simply not dying -- a real
#     reward-hacking incentive, caught live by the founder noticing a real, measured quality
#     regression across successive checkpoints ("walked up a gradient of stupidity"). And further
#     scaled EXPONENTIALLY by the own player's current damage percent
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
REWARD_BUTTON_PRESS_PER_TICK = 0.0005  # per real token spent (see ActivityTokenBucket below) -- any of jump/attack/shield/special pressed
ACTIVITY_STICK_DEADZONE = 0.15  # matches a real, typical analog-stick deadzone -- not every tiny drift counts as "moving"

# S442: real token-bucket rate limiter for the button-press activity reward -- see the module doc
# comment above for the full founder-quoted rationale.
ACTIVITY_TOKEN_REFILL_RATE = 0.8  # tokens/tick -- exactly the founder's own "pausing 20% of the time" framing
ACTIVITY_TOKEN_BUCKET_CAPACITY = 3.0  # a real, modest reserve -- enough to smooth a short pause, not enough to bank an unlimited future burst


class ActivityTokenBucket:
    """A real, standard token-bucket rate limiter. Starts full (an idle bot's very first press
    should count -- no "warm-up" penalty for a fresh episode). Call `try_spend(pressed)` once per
    tick: refills first (capped at capacity), then spends one token if `pressed` is True AND a
    token is actually available, returning whether a token was really spent this tick."""

    def __init__(self):
        self.level = ACTIVITY_TOKEN_BUCKET_CAPACITY

    def try_spend(self, pressed):
        self.level = min(ACTIVITY_TOKEN_BUCKET_CAPACITY, self.level + ACTIVITY_TOKEN_REFILL_RATE)
        if pressed and self.level >= 1.0:
            self.level -= 1.0
            return True
        return False


# S442: real inactivity penalty -- see the module doc comment above for the full rationale.
INACTIVITY_TICKS_THRESHOLD = 240  # 4 real seconds at the confirmed 60Hz tick rate
REWARD_INACTIVITY_PENALTY_PER_TICK = -0.01  # applied every tick PAST the threshold -- bounded overall by MATCH_TIME_LIMIT_TICKS's own real episode-length cap, not by a separate cap here

# S449, founder real-time, citing a real, distinct project's own documented technique ("Hyperbot",
# a Melee-playing bot): "are we doing reward discounting in some way?" Real, checked answer: no --
# _fresh_model() calls PPO("MlpPolicy", env, ...) with no gamma set, so the only discounting
# anywhere is SB3's own plain default gamma=0.99, applied per env.step() call, not per real
# elapsed second. Hyperbot's own real fix (event-driven packet timing makes step-COUNT a bad
# proxy for real time) discounts a terminal draw penalty continuously over real time via a
# half-life, so a future draw's PERCEIVED cost starts small (safe to stall) and grows to its full
# value right at the buzzer (worth taking a real risk rather than eating a near-certain draw).
# BRAWLPIT's own version of the SAME irregularity Hyperbot names, for a different reason:
# recv_snapshot()'s own "drain to freshest" behavior (see its own doc comment) means a training
# client's env.step() cadence can fall behind the server's real tick rate under --fast-forward,
# so multiple real simulated ticks can elapse between two consecutive steps -- yet
# BrawlpitPacketEnv._episode_ticks only ever increments once per step() call (see step()'s own
# `self._episode_ticks += 1`), not by however many real ticks actually passed. This module does
# NOT fix that deeper tick-accounting gap here (a real, separate, larger undertaking -- it would
# need the server to tick in lockstep with every connected client's own input, not free-run) --
# named honestly as a real limitation on how precisely "elapsed real seconds" below tracks true
# server time specifically under --fast-forward, not assumed away.
#
# What IS fixed here: unlike Hyperbot's own literal value-function surgery (no hook for that in
# SB3 PPO's stock GAE/rollout buffer -- a per-transition, wall-clock-dependent gamma isn't
# something `model.learn()` exposes), this reproduces the SAME qualitative incentive via ordinary
# reward shaping, this module's own established pattern (token-bucket activity reward, inactivity
# penalty): REWARD_INACTIVITY_PENALTY_PER_TICK gets scaled up by time_pressure_multiplier() as the
# real match clock runs down, so pure stalling is tolerated early (same real intent as Hyperbot's
# own "safer to stall" finding) but becomes progressively MORE costly as the timeout approaches --
# directly addressing the flat REWARD_LOSS-on-timeout's own real gap: a draw was already scored
# exactly as badly as a loss (S429), but nothing previously made STANDING STILL more costly than
# ENGAGING as the deadline approached, so an agent that judged its own win chance as low had no
# reason to ever stop stalling, for the WHOLE match, not just the start.
TIME_PRESSURE_HALF_LIFE_SECONDS = MATCH_TIME_LIMIT_SECONDS * (139.0 / 240.0)  # ratio-preserving port of Hyperbot's own 139s-half-life/4min-cap pairing onto BRAWLPIT's own real 150s (2.5min) cap -- ~86.9s
TIME_PRESSURE_MULTIPLIER_CEILING = 3.0  # real, named cap: time_pressure_multiplier() alone only ever reaches 1.0x (at the exact buzzer) -- this lets the inactivity penalty grow BEYOND its own S442 baseline severity as urgency rises, not just back up to 1x


def time_pressure_multiplier(elapsed_seconds, match_limit_seconds=MATCH_TIME_LIMIT_SECONDS, half_life_seconds=TIME_PRESSURE_HALF_LIFE_SECONDS, ceiling=TIME_PRESSURE_MULTIPLIER_CEILING):
    """Hyperbot's own real half-life formula, evaluated here as a plain multiplier rather than a
    literal discount applied to a future value estimate (see this module's own doc comment above
    for why that's the honest, buildable analog on top of SB3 PPO's stock machinery). Real math,
    directly ported: the perceived cost of a future timeout-draw penalty at real elapsed time `t`
    is |D| * 0.5^((T-t)/half_life) -- ~0.30x at match start (t=0, matching Hyperbot's own cited
    "~one-third"), ~0.90x with a proportionally-scaled "21 seconds left" remaining (matching
    Hyperbot's own cited "10% discount"), exactly 1.0x at the buzzer (t=T). Scaled by `ceiling` so
    callers can push a penalty ABOVE its own undiscounted baseline severity as urgency rises, not
    just back up to parity with it -- at t=T this returns exactly `ceiling`, not 1.0.
    `elapsed_seconds` past `match_limit_seconds` clamps to the ceiling (a timed-out episode has
    zero real time left, i.e. maximum urgency, not undefined negative "remaining" time)."""
    remaining = max(0.0, match_limit_seconds - elapsed_seconds)
    return ceiling * (0.5 ** (remaining / half_life_seconds))

# Tier 5: real, growing survival-streak shaping (see the module doc comment above for the full
# founder-quoted rationale). REWARD_SURVIVAL_STREAK_UNIT is deliberately the same tiny order of
# magnitude as REWARD_ALIVE_PER_TICK so an EARLY streak tick stays negligible; the whole point is
# that the Fibonacci/exponential multipliers below are what make it grow into something real.
REWARD_SURVIVAL_STREAK_UNIT = 0.001
SURVIVAL_STREAK_FIB_CAP = 14  # REAL, FOUND, FIXED BUG (founder real-time: "we spiked in model
# quality... then the newer ones are all pretty dumb... like they walked up a gradient of
# stupidity... i think i introduced some perverted incentives"): this term used to be applied
# EVERY TICK for as long as a life continued, with the Fibonacci index merely clamped at the cap
# -- so once a life survived past SURVIVAL_STREAK_FIB_CAP ticks, it kept earning
# REWARD_SURVIVAL_STREAK_UNIT * fib(cap) EVERY SUBSEQUENT TICK, forever, for the rest of a match.
# At the old cap of 20, fib(20)=6765 -> 6.765 reward PER TICK sustained for up to ~9000 ticks --
# tens of thousands of total reward for simply not dying, versus REWARD_WIN=10 for actually
# winning. A real, severe reward-hacking incentive to stall/avoid combat, exactly matching the
# founder's own observed symptom: gradual policy drift toward passivity as more gradient steps
# accumulated under this term, not a single sharp break.
#
# REAL FIX: compute_reward now stops paying this term entirely once survival_ticks exceeds this
# cap (see the `survival_ticks > SURVIVAL_STREAK_FIB_CAP` guard below) instead of paying the
# capped value forever -- the total this term can ever pay out over one life is now a real,
# fixed, bounded constant: REWARD_SURVIVAL_STREAK_UNIT * sum(fib(1..cap)) * (damage scale at the
# time each tick was paid), not an unbounded per-tick-forever plateau. sum(fib(1..14)) = 986, so
# the real worst-case total (sustained at 200% damage the whole time) is
# 0.001 * 986 * 2.0**2.0 = 3.944 -- safely under REWARD_STOCK_TAKEN=5, so surviving passively can
# never out-earn actually taking a stock, let alone winning.
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


def compute_reward(prev_own, prev_opp, cur_own, cur_opp, done, edge_danger_threshold=EDGE_DANGER_THRESHOLD_DEFAULT, action=None, survival_ticks=None, activity_token_spent=None, timed_out=False, inactivity_ticks=None, episode_ticks=None):
    """Delta-based dense reward -- see this module's own "Reward design" doc comment above for
    the full tier rationale (outcome / positional-shaping / survival / activity / survival-streak
    / inactivity).

    `action` is the real 6-element action just taken this tick ([stick_x, stick_y, jump, attack,
    shield, special], the exact shape BrawlpitPacketEnv.step's own action space uses) -- optional
    and backward-compatible (None skips tier 4's movement half entirely, e.g. for a caller that
    only has game state and no action to report, matching every other optional-degrade
    convention this module already establishes).

    `survival_ticks` is the real count of consecutive ticks this life has lasted (including this
    one), maintained by the caller and reset to 0 the tick a stock is lost -- optional and
    backward-compatible the same way `action` is (None skips tier 5 entirely).

    `activity_token_spent` (S442, replacing the earlier `button_press_count` harmonic-decay
    design) is a real bool: did THIS tick's button press actually spend a real token from the
    caller's own ActivityTokenBucket? None keeps the button-press bonus flat at
    REWARD_BUTTON_PRESS_PER_TICK on any real press, matching this module's own established
    optional-degrade convention (equivalent to a caller with an infinite, unlimited bucket).

    `timed_out` (S429, founder real-time: "add a timer - 2.5 minutes - if time expires it's a
    draw and thats counted the same as a loss in terms of negative reward") is True when `done`
    became True because MATCH_TIME_LIMIT_TICKS was reached, not because either side actually ran
    out of stocks. A timeout is a real draw -- deliberately NOT scored via the normal stock-
    comparison outcome below (whoever happens to be ahead on stocks when the clock runs out does
    NOT get REWARD_WIN): both sides get REWARD_LOSS, exactly as bad as an outright loss, so a
    policy can never learn to stall out a lead until the clock saves it.

    `inactivity_ticks` (S442, founder real-time, after directly observing a real trained
    checkpoint go completely inert: "do we introduce a strong negative reward that ticks down if
    no key is pressed for say 4 seconds?") is the real count of consecutive ticks with NEITHER
    real stick movement NOR any button press, maintained by the caller and reset to 0 the instant
    any real input occurs. Optional and backward-compatible: None skips this term entirely.

    `episode_ticks` (S449, founder real-time: "are we doing reward discounting in some way?",
    citing Hyperbot's own real half-life-over-real-time technique) is the real count of ticks
    elapsed in the WHOLE match so far (not reset per life, unlike `survival_ticks`) -- used only
    to scale the inactivity penalty above by time_pressure_multiplier() (see that function's own
    doc comment for the full rationale and math). None keeps the inactivity penalty at its old,
    flat S442 severity for the whole match, matching this module's own optional-degrade
    convention."""
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
            # S442: real token-bucket rate limiting (see the module doc comment above) --
            # activity_token_spent=None (no bucket to track) keeps the old flat bonus on any
            # real press; otherwise only a press that actually spent a real token earns it.
            if activity_token_spent is None or activity_token_spent:
                reward += REWARD_BUTTON_PRESS_PER_TICK

    # S442: real inactivity penalty (see the module doc comment above for the full rationale) --
    # a real, flat cost for total inaction sustained past a generous 4-real-second threshold, the
    # actual fix for a policy that has learned "never act" is safer than the small risk of acting.
    #
    # S449: scaled by real match-clock urgency when episode_ticks is given (see
    # time_pressure_multiplier's own doc comment) -- Hyperbot's own real technique, ported as
    # reward shaping: standing still stays cheap early in a match (matching Hyperbot's own real
    # "safer to stall" finding at low win-probability) but gets progressively MORE expensive as
    # the real 150s cap approaches, so a policy that has judged its own win chance as low still
    # has a real, growing reason to eventually engage rather than stall for the WHOLE match.
    if inactivity_ticks is not None and inactivity_ticks > INACTIVITY_TICKS_THRESHOLD:
        penalty = REWARD_INACTIVITY_PENALTY_PER_TICK
        if episode_ticks is not None:
            penalty *= time_pressure_multiplier(episode_ticks / TICK_RATE_HZ)
        reward += penalty

    # Tier 5: survival streak (see the module doc comment above for the full rationale, including
    # the real bug found and fixed here). Refuses to apply on the exact tick a stock was lost,
    # even if the caller passes a stale/positive `survival_ticks` -- "it resets" is enforced here,
    # not just trusted to the caller. REAL FIX: stops paying entirely once survival_ticks exceeds
    # the cap (a real, one-time-per-life bounded total), rather than paying the capped value
    # every tick forever -- see SURVIVAL_STREAK_FIB_CAP's own doc comment for the full math.
    if (survival_ticks is not None and 0 < survival_ticks <= SURVIVAL_STREAK_FIB_CAP
            and cur_own.stocks == prev_own.stocks):
        damage_scale = SURVIVAL_STREAK_DAMAGE_EXP_BASE ** (cur_own.damage / 100.0)
        reward += REWARD_SURVIVAL_STREAK_UNIT * _fibonacci(survival_ticks) * damage_scale

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


# --- Real matchmaking (MATCHMAKING_MODE_1V1) -- moved here from rl_evaluate.py (S443) so
# BrawlpitPacketEnv's own real self-play mode (see class doc comment below) can use it without a
# circular import (rl_evaluate.py already imports FROM this module). rl_evaluate.py/rl_bot_pool.py
# both still import these exact names from wherever they used to live -- see rl_evaluate.py's own
# real re-export of them, kept for backward compatibility. ---

MATCHMAKING_MODE_1V1 = 1  # protocol.h's own real MATCHMAKING_MODE_1V1


def encode_find_match_1v1():
    """Real PACKET_FIND_MATCH request targeting the 1v1 queue -- entity_count carries which
    queue (protocol.h's own real convention), 1 = MATCHMAKING_MODE_1V1."""
    h = NetHeader(type=PACKET_FIND_MATCH, client_id=0, sequence=0, timestamp=0, entity_count=MATCHMAKING_MODE_1V1)
    return bytes(h)


def decode_match_found(data):
    """Returns the real client_id the server assigned this connection for the match, or None if
    `data` isn't a real PACKET_MATCH_FOUND."""
    if len(data) < ctypes.sizeof(NetHeader):
        return None
    h = NetHeader.from_buffer_copy(data[: ctypes.sizeof(NetHeader)])
    if h.type != PACKET_MATCH_FOUND:
        return None
    return h.client_id


def find_match_1v1_both(client_a, client_b, timeout=10.0):
    """Queues BOTH real clients into the SAME server's 1v1 queue and waits for both to receive a
    real PACKET_MATCH_FOUND -- sending both FIND_MATCH requests interleaved (not one client fully
    blocking before the other starts) so they land in the queue together, within
    MATCHMAKING_1V1_TIMEOUT_MS, and get matched with EACH OTHER rather than one of them getting
    bot-filled after the real 5s timeout."""
    client_a.sock.settimeout(0.2)
    client_b.sock.settimeout(0.2)
    deadline = time.time() + timeout
    a_id = b_id = None
    while time.time() < deadline and (a_id is None or b_id is None):
        if a_id is None:
            client_a.sock.sendto(encode_find_match_1v1(), client_a.addr)
        if b_id is None:
            client_b.sock.sendto(encode_find_match_1v1(), client_b.addr)
        for client, current in ((client_a, a_id), (client_b, b_id)):
            if current is not None:
                continue
            try:
                data, _ = client.sock.recvfrom(2048)
            except socket.timeout:
                continue
            cid = decode_match_found(data)
            if cid is not None:
                client.client_id = cid
                if client is client_a:
                    a_id = cid
                else:
                    b_id = cid
    if a_id is None or b_id is None:
        raise ConnectionError("failed to queue both evaluation clients into the same real 1v1 match")
    return a_id, b_id


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
        uses).

        S443, founder real-time: "no no no sir its supposed to fight it self and evolve via the
        league" -- REAL SELF-PLAY, not just a static opponent. Pass `opponent_checkpoint_path`
        to make this env queue into BRAWLPIT's own real MATCHMAKING_MODE_1V1 (the exact same
        mechanism rl_evaluate.py's own evaluation matches already use, no server change needed)
        against a SECOND real network client driven by a frozen (never-trained-during-this-env)
        past checkpoint of the same role -- instead of the old default path (a direct-connect,
        PACKET_RESET_MATCH-driven match against local_init_match's own real, honestly-named,
        completely UNDRIVEN "opponent" slot -- no bot_think, no AI, nothing, just a character
        sitting at its spawn point the entire match). Leaving `opponent_checkpoint_path` unset
        keeps the exact old, single-client behavior (full backward compatibility -- every
        existing test in this file constructs this class with no opponent and still gets the old
        static-dummy path)."""

        metadata = {"render_modes": []}

        def __init__(self, host="127.0.0.1", port=DEFAULT_PORT, opponent_checkpoint_path=None):
            super().__init__()
            self.observation_space = spaces.Box(low=-2.0, high=2.0, shape=(OBS_SIZE,), dtype="float32")
            self.action_space = spaces.Box(low=-1.0, high=1.0, shape=(6,), dtype="float32")
            self.host, self.port = host, port
            self.client = None
            self._prev_own, self._prev_opp = None, None
            self._survival_ticks = 0  # tier 5: real, consecutive-tick life counter, reset on every stock loss
            self._episode_ticks = 0  # S429: real, whole-episode tick counter for the 2.5-minute match timer
            self._activity_bucket = ActivityTokenBucket()  # S442: real token-bucket state for the button-press activity reward
            self._inactivity_ticks = 0  # S442: real, whole-episode "ticks since any real input" counter

            # S443: real self-play state -- see the class doc comment above.
            self.opponent_checkpoint_path = opponent_checkpoint_path
            self._opponent_model = None
            self._opponent_client = None
            self._opponent_prev_own, self._opponent_prev_opp = None, None
            if opponent_checkpoint_path:
                from stable_baselines3 import PPO  # imported lazily -- only self-play mode needs SB3 loaded here
                self._opponent_model = PPO.load(opponent_checkpoint_path, device="cpu")

        def reset(self, *, seed=None, options=None):
            super().reset(seed=seed)
            if self.client is None:
                self.client = PacketClient(self.host, self.port)
            if self.opponent_checkpoint_path:
                # S443: real self-play -- queue BOTH real clients into the same server's own
                # 1v1 matchmaking queue instead of the old single-client PACKET_RESET_MATCH path.
                if self._opponent_client is None:
                    self._opponent_client = PacketClient(self.host, self.port)
                find_match_1v1_both(self.client, self._opponent_client)
                _, opp_players = self._opponent_client.recv_snapshot()
                self._opponent_prev_own, self._opponent_prev_opp = find_self_and_opponent(opp_players, self._opponent_client.client_id)
            else:
                if self.client.client_id is None:
                    self.client.connect()
                # S419-07: a real, server-confirmed episode boundary (fresh spawns/stocks/damage
                # for both slots) -- fixes what was a real, named gap (episodes used to be
                # observational only, since the server had no network "reset this match" packet).
                self.client.reset_match()
            header, players = self.client.recv_snapshot()
            own, opp = find_self_and_opponent(players, self.client.client_id)
            self._prev_own, self._prev_opp = own, opp
            self._survival_ticks = 0  # a fresh episode is a fresh life
            self._episode_ticks = 0  # a fresh episode gets a fresh 2.5-minute clock
            self._activity_bucket = ActivityTokenBucket()  # a fresh episode gets a fresh bucket
            self._inactivity_ticks = 0  # a fresh episode gets a fresh inactivity clock
            obs = build_observation(own, opp) if own and opp else [0.0] * OBS_SIZE
            return _as_obs_array(obs), {}

        def step(self, action):
            # S443: drive the frozen self-play opponent's own real action FIRST, from its own
            # last-known observation (the same real lag-one-tick pattern this env's own training
            # side already uses) -- a genuine second network client, not a scripted heuristic.
            if self.opponent_checkpoint_path and self._opponent_prev_own is not None and self._opponent_prev_opp is not None:
                opp_action, _ = self._opponent_model.predict(
                    build_observation(self._opponent_prev_own, self._opponent_prev_opp), deterministic=True)
                self._opponent_client.send_action(
                    float(opp_action[0]), float(opp_action[1]),
                    jump=opp_action[2] > 0, attack=opp_action[3] > 0, shield=opp_action[4] > 0, special=opp_action[5] > 0,
                )

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
                # S442: real activity/inactivity bookkeeping -- re-derives "was there any real
                # input this tick" the same way compute_reward itself checks, so the two can
                # never drift apart.
                moved = abs(action[0]) > ACTIVITY_STICK_DEADZONE or abs(action[1]) > ACTIVITY_STICK_DEADZONE
                pressed = action[2] > 0 or action[3] > 0 or action[4] > 0 or action[5] > 0
                token_spent = self._activity_bucket.try_spend(pressed)
                self._inactivity_ticks = 0 if (moved or pressed) else self._inactivity_ticks + 1
                reward = compute_reward(self._prev_own, self._prev_opp, own, opp, done,
                                         action=action, survival_ticks=self._survival_ticks,
                                         activity_token_spent=token_spent, timed_out=timed_out,
                                         inactivity_ticks=self._inactivity_ticks,
                                         episode_ticks=self._episode_ticks)
            obs = build_observation(own, opp) if own and opp else [0.0] * OBS_SIZE
            self._prev_own, self._prev_opp = own, opp

            if self.opponent_checkpoint_path:
                _, opp_players = self._opponent_client.recv_snapshot()
                new_opp_own, new_opp_opp = find_self_and_opponent(opp_players, self._opponent_client.client_id)
                if new_opp_own is not None and new_opp_opp is not None:
                    self._opponent_prev_own, self._opponent_prev_opp = new_opp_own, new_opp_opp

            return _as_obs_array(obs), reward, done, False, {}

        def close(self):
            if self.client:
                self.client.close()
                self.client = None
            if self._opponent_client:
                self._opponent_client.close()
                self._opponent_client = None


def _smoke_test(host, port, steps):
    """Real, live rollout against an ACTUAL running bin/brawlpit_server -- no gymnasium needed.
    Run `./bin/brawlpit_server --fast-forward &` first."""
    client = PacketClient(host, port)
    client_id = client.connect()
    print(f"connected, assigned client_id={client_id}")
    prev_own, prev_opp = None, None
    total_reward = 0.0
    survival_ticks = 0
    activity_bucket = ActivityTokenBucket()
    inactivity_ticks = 0
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
            moved = abs(action[0]) > ACTIVITY_STICK_DEADZONE or abs(action[1]) > ACTIVITY_STICK_DEADZONE
            pressed = action[2] > 0 or action[3] > 0 or action[4] > 0 or action[5] > 0
            token_spent = activity_bucket.try_spend(pressed)
            inactivity_ticks = 0 if (moved or pressed) else inactivity_ticks + 1
            r = compute_reward(prev_own, prev_opp, own, opp, done=False, action=action,
                                survival_ticks=survival_ticks, activity_token_spent=token_spent,
                                inactivity_ticks=inactivity_ticks)
            total_reward += r
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
