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

# --- Real wire protocol structures (packages/common/protocol.h) ---
# ctypes.Structure lets the C compiler's own natural alignment/padding rules apply automatically
# (matching gcc's real x86-64 Linux ABI, which is what bin/brawlpit_server is actually built
# with) instead of hand-composing a struct.calcsize format string and hoping the padding is
# right -- the self-check assertions below are what actually catches it if not.

PACKET_CONNECT = 0
PACKET_USERCMD = 1
PACKET_SNAPSHOT = 2
PACKET_WELCOME = 3

BTN_JUMP = 1
BTN_ATTACK = 2
BTN_SHIELD = 4
BTN_SPECIAL = 8

DEFAULT_PORT = 6978  # apps/server/src/main.c's own hardcoded bind_addr.sin_port


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


# --- Reward (dense per-tick shaping, same real shape REDGARDEN/scripts/rl_env.py's own
# compute_reward established -- damage/kill/death/alive/win/loss deltas -- adapted to BRAWLPIT's
# real damage_percent + stocks model instead of REDGARDEN's own hp model). ---

REWARD_DAMAGE_DEALT_PER_PCT = 0.01
REWARD_DAMAGE_TAKEN_PER_PCT = -0.01
REWARD_STOCK_TAKEN = 5.0
REWARD_STOCK_LOST = -5.0
REWARD_ALIVE_PER_TICK = 0.001
REWARD_WIN = 10.0
REWARD_LOSS = -10.0


def compute_reward(prev_own, prev_opp, cur_own, cur_opp, done):
    """Delta-based dense reward, computed OUTSIDE the C sim (same real reasoning REDGARDEN's own
    compute_reward doc comment gives: reward shaping stays tunable without touching/recompiling
    anything server-side)."""
    reward = 0.0
    reward += REWARD_DAMAGE_DEALT_PER_PCT * max(0, cur_opp.damage - prev_opp.damage)
    reward += REWARD_DAMAGE_TAKEN_PER_PCT * max(0, cur_own.damage - prev_own.damage)
    if cur_opp.stocks < prev_opp.stocks:
        reward += REWARD_STOCK_TAKEN * (prev_opp.stocks - cur_opp.stocks)
    if cur_own.stocks < prev_own.stocks:
        reward += REWARD_STOCK_LOST * (prev_own.stocks - cur_own.stocks)
    reward += REWARD_ALIVE_PER_TICK
    if done:
        if cur_own.stocks > cur_opp.stocks:
            reward += REWARD_WIN
        elif cur_own.stocks < cur_opp.stocks:
            reward += REWARD_LOSS
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
        --fast-forward is running the server far faster than this client's own step() cadence."""
        header, players = None, []
        deadline = time.time() + (self.sock.gettimeout() or 2.0)
        while time.time() < deadline:
            try:
                data, _ = self.sock.recvfrom(4096)
            except socket.timeout:
                break
            h, p = decode_snapshot(data)
            if h is not None:
                header, players = h, p
            # Deliberately not returning on the first packet -- drain the socket's recv buffer
            # (non-blocking check via a zero-ish remaining budget) so a fast-forwarded server's
            # backlog doesn't make this client fall further and further behind real time.
            self.sock.settimeout(0.0)
            try:
                while True:
                    data, _ = self.sock.recvfrom(4096)
                    h, p = decode_snapshot(data)
                    if h is not None:
                        header, players = h, p
            except (BlockingIOError, socket.timeout):
                pass
            finally:
                self.sock.settimeout(deadline - time.time() if deadline > time.time() else 0.001)
            break
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

        def reset(self, *, seed=None, options=None):
            super().reset(seed=seed)
            if self.client is None:
                self.client = PacketClient(self.host, self.port)
                self.client.connect()
            # Real, honest, named gap: bin/brawlpit_server has no network "reset this match"
            # packet today (it boots into exactly one local_init_match and runs forever) -- an
            # RL episode boundary here is therefore observational only (stocks/damage keep
            # climbing across what a training loop calls separate "episodes") until a real
            # PACKET_RESET_MATCH is added server-side. Flagged directly rather than silently
            # pretending episodes actually reset game state.
            header, players = self.client.recv_snapshot()
            own, opp = find_self_and_opponent(players, self.client.client_id)
            self._prev_own, self._prev_opp = own, opp
            obs = build_observation(own, opp) if own and opp else [0.0] * OBS_SIZE
            return obs, {}

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
            done = bool(own and (own.stocks == 0 or opp.stocks == 0))
            reward = 0.0
            if self._prev_own and self._prev_opp and own and opp:
                reward = compute_reward(self._prev_own, self._prev_opp, own, opp, done)
            obs = build_observation(own, opp) if own and opp else [0.0] * OBS_SIZE
            self._prev_own, self._prev_opp = own, opp
            return obs, reward, done, False, {}

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
    for i in range(steps):
        client.send_action(stick_x=0.5, stick_y=0.0, attack=(i % 10 == 0))
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
            r = compute_reward(prev_own, prev_opp, own, opp, done=False)
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
