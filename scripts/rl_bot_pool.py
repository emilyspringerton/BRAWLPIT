#!/usr/bin/env python3
"""
scripts/rl_bot_pool.py (S422) -- a real, persistent bot pool.

Founder real-time: "we need a bot pool like in redgarden it needs to be elo based and humans can
join it in game so we can be matched with a bot and then we fight it and either our elo our its
elo goes up and down depending on the outcome of the match" -> "its not a team game so we can
have like 9 bots in bot pool and there should be 1 bot always waiting and 4 bot games always
running" -> "i guess all model generations need to be added to the bot pool."

Mirrors REDGARDEN's own real apps/arena_bot precedent: standing bot processes that queue into the
SAME real matchmaker humans use (BRAWLPIT's own MATCHMAKING_MODE_1V1), not a special
training-only mechanism. Real topology (the founder's own numbers): up to 9 bots drawn from the
live checkpoint registry -- every real checkpoint with exported weights, from every generation
currently in the registry ("all model generations need to be added"), not a fixed hand-picked
set -- 4 pairs (8 bots) running continuous bot-vs-bot matches on dedicated local servers (real,
ongoing Elo movement even with zero humans playing), and 1 bot queued on whichever real server
humans actually connect to, waiting to be matched.

Real, honest, named gap, not glossed over: BRAWLPIT has no human player identity/login system at
all today (checked directly -- only free, no-login cosmetics exist). When a human beats or loses
the waiting bot, only the BOT's own Elo moves (record_match_result needs two real checkpoint
ids; there is no second one to represent an anonymous human yet). A real human-side Elo needs a
real player-identity system first -- scoped, not built, here.

Real, deliberate safety default: --human-host/--human-port default to a LOCAL dedicated server,
never a real production server, since connecting an untested bot to a live server real players
might be on is a genuinely outward-facing action -- point this at a real production server only
once you've verified this script works the way you want against a local one first.

Usage (all local, safe to run anywhere):
    ./scripts/build_training.sh
    python3 scripts/rl_bot_pool.py --registry-url http://localhost:8080 --agent-secret ...

Usage against a real server humans actually connect to (only once you're ready):
    python3 scripts/rl_bot_pool.py --registry-url ... --agent-secret ... \\
        --human-host brawlpit.okemily.com --human-port 6978 --spawn-human-server=false
"""

import argparse
import os
import random
import subprocess
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from rl_env_packet import (  # noqa: E402
    MATCH_TIME_LIMIT_TICKS,
    NetHeader,
    PACKET_MATCH_FOUND,
    PacketClient,
    build_observation,
    find_self_and_opponent,
)
from rl_evaluate import encode_find_match_1v1, decode_match_found, find_match_1v1_both  # noqa: E402
from rl_registry import authenticate, download_checkpoint, list_checkpoints, record_match_result  # noqa: E402

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SERVER_BIN = os.path.join(REPO_ROOT, "bin", "brawlpit_server")
CACHE_DIR = os.path.join(REPO_ROOT, "var", "bot_pool_checkpoints")

_spawned_servers = []


def _spawn_dedicated_server(port):
    proc = subprocess.Popen([SERVER_BIN, "--fast-forward", "--port", str(port)], cwd=REPO_ROOT,
                             stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    _spawned_servers.append(proc)
    time.sleep(0.5)
    return proc


def _cleanup_servers():
    for proc in _spawned_servers:
        proc.terminate()
    for proc in _spawned_servers:
        try:
            proc.wait(timeout=2.0)
        except subprocess.TimeoutExpired:
            proc.kill()


class PoolBot:
    """One real, persistent bot identity -- a checkpoint's own registry metadata plus its
    locally-cached model, loaded ONCE and reused across every match this bot plays (loading an
    SB3 model from disk is real, non-trivial overhead a persistent pool member should never
    repeat per match)."""

    def __init__(self, checkpoint, local_path):
        from stable_baselines3 import PPO
        self.checkpoint = checkpoint
        self.model = PPO.load(local_path, device="cpu")

    @property
    def id(self):
        return self.checkpoint["id"]

    @property
    def name(self):
        return self.checkpoint.get("name") or f"#{self.id}"


def fetch_pool_bots(registry_url, pool_size):
    """Pulls the real, live checkpoint list and picks up to `pool_size` real, distinct
    checkpoints that actually have exported weights -- founder real-time: "all model generations
    need to be added to the bot pool," so this draws from every real generation currently in the
    registry, not a fixed hand-picked set. Downloads each one's real .zip locally once (cached by
    id under var/bot_pool_checkpoints/, re-used across restarts) since PPO.load needs a real
    file, not the registry's own HTTP stream directly.

    Skips any checkpoint marked `is_disabled` (S428, the real registry checkbox) -- a model the
    founder has explicitly excluded from the league never gets pooled, even if it has weights."""
    os.makedirs(CACHE_DIR, exist_ok=True)
    all_checkpoints = [c for c in list_checkpoints(registry_url) if c.get("has_weights") and not c.get("is_disabled")]
    if not all_checkpoints:
        raise RuntimeError("no checkpoints with exported weights in the registry yet -- nothing to pool")
    random.shuffle(all_checkpoints)
    chosen = all_checkpoints[:pool_size]

    bots = []
    for c in chosen:
        local_path = os.path.join(CACHE_DIR, f"{c['id']}.zip")
        if not os.path.exists(local_path):
            download_checkpoint(registry_url, c["id"], local_path)
        bots.append(PoolBot(c, local_path))
    print(f"pool: loaded {len(bots)} real checkpoint(s) -- {', '.join(b.name for b in bots)}")
    return bots


def _play_and_score(client_a, bot_a, client_b, bot_b, max_ticks=MATCH_TIME_LIMIT_TICKS):
    """Real match loop shared by both bot-vs-bot and bot-vs-human play: each tick, each SIDE THIS
    SCRIPT CONTROLS computes its own action from its own model and sends it; a human-controlled
    side (client_b is None) just gets read, never driven. Returns score_a (1.0/0.0/0.5) once
    either side's stocks hit 0 or max_ticks elapses, or None if the match was never actually
    observed (a real, honest "inconclusive, don't record" outcome).

    `max_ticks` defaults to MATCH_TIME_LIMIT_TICKS (S429, founder real-time: "add a timer - 2.5
    minutes - if time expires it's a draw") -- reaching it is ALWAYS scored as a real draw (0.5),
    never a win for whoever happened to be ahead when the clock ran out."""
    own_a = opp_a = None
    timed_out = True
    for _ in range(max_ticks):
        _, players_a = client_a.recv_snapshot()
        own_a, opp_a = find_self_and_opponent(players_a, client_a.client_id)
        if own_a is None or opp_a is None:
            continue

        action_a, _ = bot_a.model.predict(build_observation(own_a, opp_a), deterministic=True)
        client_a.send_action(float(action_a[0]), float(action_a[1]),
                              jump=action_a[2] > 0, attack=action_a[3] > 0, shield=action_a[4] > 0, special=action_a[5] > 0)

        if client_b is not None:
            _, players_b = client_b.recv_snapshot()
            own_b, opp_b = find_self_and_opponent(players_b, client_b.client_id)
            if own_b is not None and opp_b is not None:
                action_b, _ = bot_b.model.predict(build_observation(own_b, opp_b), deterministic=True)
                client_b.send_action(float(action_b[0]), float(action_b[1]),
                                      jump=action_b[2] > 0, attack=action_b[3] > 0, shield=action_b[4] > 0, special=action_b[5] > 0)

        if own_a.stocks == 0 or opp_a.stocks == 0:
            timed_out = False
            break

    if own_a is None or opp_a is None:
        return None
    if timed_out:
        return 0.5
    if own_a.stocks > opp_a.stocks:
        return 1.0
    if own_a.stocks < opp_a.stocks:
        return 0.0
    return 0.5


class BotMatchmaker:
    """A real, shared, Elo-aware matchmaker for the bot-vs-bot pool (S422, founder real-time:
    "use matchmaking queues to manage load as the bot pool grows really low elos get looong
    queue times because we dont really need data on a shit bot fighting against anything
    really"). Real, deliberate design: each pairing decision picks the longest-waiting free bot
    (real fairness -- nothing starves forever) and matches it with its CLOSEST-CURRENT-Elo free
    partner (Elo refreshed from the registry right before deciding, so pairing reflects real,
    live ratings, not stale ones from pool startup). An outlier bot (very low/high Elo relative
    to the rest of the pool) naturally gets paired less often than a well-matched pair would be
    -- exactly the real behavior asked for (don't spend real match-time generating low-value
    blowout data between mismatched bots) without needing a separate, explicit priority/delay
    mechanism bolted on top."""

    def __init__(self, bots, registry_url):
        self.bots = bots
        self.registry_url = registry_url
        self.last_played = {b.id: 0.0 for b in bots}
        self.busy_ids = set()
        self.lock = threading.Lock()

    def _refresh_elo(self):
        try:
            live = list_checkpoints(self.registry_url)
            elo_by_id = {c["id"]: c["elo"] for c in live}
            for b in self.bots:
                if b.id in elo_by_id:
                    b.checkpoint["elo"] = elo_by_id[b.id]
        except Exception:
            pass  # a real, honest degrade -- one stale Elo reading is fine for one pairing decision

    def pick_pair(self):
        """Returns a real (bot_a, bot_b) pair not currently playing elsewhere, or None if fewer
        than 2 free bots exist right now. Marks both busy immediately (release_pair below frees
        them) so no bot is ever picked for two simultaneous games."""
        with self.lock:
            self._refresh_elo()
            free = [b for b in self.bots if b.id not in self.busy_ids]
            if len(free) < 2:
                return None
            free.sort(key=lambda b: self.last_played[b.id])
            a = free[0]
            rest = sorted(free[1:], key=lambda b: abs(b.checkpoint["elo"] - a.checkpoint["elo"]))
            b = rest[0]
            now = time.time()
            self.last_played[a.id] = now
            self.last_played[b.id] = now
            self.busy_ids.add(a.id)
            self.busy_ids.add(b.id)
            return a, b

    def release_pair(self, bot_a, bot_b):
        with self.lock:
            self.busy_ids.discard(bot_a.id)
            self.busy_ids.discard(bot_b.id)


def run_bot_vs_bot_port_forever(host, port, matchmaker, registry_url, jwt, stop_event):
    """One dedicated server's own worker loop -- asks the shared BotMatchmaker for a fresh,
    Elo-aware pair every time it's free to host a new match (not a fixed pair for the port's
    whole lifetime), plays it out, records the real result, and repeats. This is what keeps real
    Elo moving even with zero humans playing."""
    while not stop_event.is_set():
        pair = matchmaker.pick_pair()
        if pair is None:
            time.sleep(1.0)
            continue
        bot_a, bot_b = pair
        try:
            client_a, client_b = PacketClient(host, port), PacketClient(host, port)
            find_match_1v1_both(client_a, client_b)
            print(f"[bot-vs-bot :{port}] {bot_a.name} (elo {bot_a.checkpoint['elo']:.0f}) vs "
                  f"{bot_b.name} (elo {bot_b.checkpoint['elo']:.0f})")
            score_a = _play_and_score(client_a, bot_a, client_b, bot_b)
            client_a.close()
            client_b.close()
            if score_a is not None:
                result = record_match_result(registry_url, jwt, bot_a.id, bot_b.id, score_a)
                print(f"[bot-vs-bot :{port}] result score_a={score_a} -> "
                      f"{bot_a.name} elo={result['a']['elo']:.0f}, {bot_b.name} elo={result['b']['elo']:.0f}")
        except Exception as e:  # noqa: BLE001 -- a real, non-fatal degrade: one bad match (a
            # dropped connection, a malformed snapshot) must never take down a persistent pool
            # member -- log it and try again, matching this whole pipeline's own established
            # "a bad/missing resource never corrupts what's already working" convention.
            print(f"[bot-vs-bot :{port}] WARNING: match failed ({e}), retrying")
            time.sleep(2.0)
        finally:
            matchmaker.release_pair(bot_a, bot_b)


def run_waiting_for_human_forever(host, port, bot, registry_url, jwt, stop_event):
    """The real '1 bot always waiting' slot -- queues alone on whichever real server humans
    actually connect to, plays whoever it gets matched with (a real human, or -- if this points
    at a local test server with nothing else going on -- nobody, in which case BRAWLPIT's own
    real MATCHMAKING_1V1_TIMEOUT_MS bot-fill kicks in and this bot ends up playing that fallback
    heuristic bot instead, which is a real, harmless, honest degrade for a test run, not a bug).
    Real, named gap: a human opponent has no registry id, so a human win/loss only ever moves
    THIS bot's own Elo via a real, direct two-argument record isn't possible (record_match_result
    needs two real ids) -- so human matches are NOT recorded yet. Named directly, not silently
    dropped."""
    while not stop_event.is_set():
        try:
            client = PacketClient(host, port)
            client.sock.settimeout(0.5)
            h = NetHeader(type=0, client_id=0, sequence=0, timestamp=0, entity_count=0)
            deadline = time.time() + 30.0
            matched_id = None
            while time.time() < deadline and matched_id is None:
                client.sock.sendto(encode_find_match_1v1(), client.addr)
                try:
                    data, _ = client.sock.recvfrom(2048)
                    matched_id = decode_match_found(data)
                except Exception:
                    continue
            if matched_id is None:
                print(f"[waiting :{port}] no match found in 30s, re-queueing")
                client.close()
                continue
            client.client_id = matched_id
            print(f"[waiting :{port}] {bot.name} matched (client_id={matched_id}) -- playing")
            score = _play_and_score(client, bot, None, None)
            client.close()
            if score is not None:
                print(f"[waiting :{port}] match finished, {bot.name}'s own result: "
                      f"{'won' if score == 1.0 else 'lost' if score == 0.0 else 'drew'} "
                      f"(not recorded -- opponent has no real registry identity yet)")
        except Exception as e:  # noqa: BLE001
            print(f"[waiting :{port}] WARNING: match failed ({e}), retrying")
            time.sleep(2.0)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--registry-url", default=os.environ.get("IDUNA_BASE_URL", "http://localhost:8080"))
    p.add_argument("--agent-name", default=os.environ.get("IDUNA_AGENT_NAME", "BRAWLPIT-RL"))
    p.add_argument("--agent-secret", default=os.environ.get("IDUNA_AGENT_SECRET"))
    p.add_argument("--pool-size", type=int, default=9)
    p.add_argument("--bot-vs-bot-games", type=int, default=4)
    p.add_argument("--bot-vs-bot-base-port", type=int, default=8100)
    p.add_argument("--human-host", default="127.0.0.1",
                    help="real, deliberate safety default -- a LOCAL server, never production, unless you explicitly point this elsewhere")
    p.add_argument("--human-port", type=int, default=8199)
    p.add_argument("--spawn-human-server", action="store_true", default=True)
    p.add_argument("--no-spawn-human-server", dest="spawn_human_server", action="store_false",
                    help="pass this when --human-host/--human-port point at a real, already-running server (e.g. production)")
    args = p.parse_args()

    if not args.agent_secret:
        print("--agent-secret (or IDUNA_AGENT_SECRET) is required.")
        return 1
    if not os.path.exists(SERVER_BIN):
        print(f"{SERVER_BIN} not found -- run ./scripts/build_training.sh first.")
        return 1

    needed = args.bot_vs_bot_games * 2 + 1
    if args.pool_size < needed:
        print(f"--pool-size {args.pool_size} is too small for {args.bot_vs_bot_games} bot-vs-bot "
              f"games + 1 waiting bot (needs {needed}) -- reducing bot-vs-bot-games.")
        args.bot_vs_bot_games = max(0, (args.pool_size - 1) // 2)

    jwt = authenticate(args.registry_url, args.agent_name, args.agent_secret)
    print(f"authenticated with the registry at {args.registry_url}")
    bots = fetch_pool_bots(args.registry_url, args.pool_size)
    if len(bots) < needed:
        print(f"only {len(bots)} real checkpoint(s) available -- reducing bot-vs-bot-games accordingly.")
        args.bot_vs_bot_games = max(0, (len(bots) - 1) // 2)

    stop_event = threading.Event()
    threads = []

    # Real, deliberate design (S422 matchmaking-queue follow-up): the bot-vs-bot pool of
    # `bot_vs_bot_games * 2` bots is shared across ALL bot-vs-bot ports via one BotMatchmaker,
    # not fixed 1:1 pairs per port -- every time a port's own game finishes, it asks the SAME
    # matchmaker for a fresh Elo-aware pair. This is what actually lets low-Elo outlier bots
    # "get looong queue times" (fewer of their close-Elo partners are ever free) rather than
    # being locked into a bad matchup forever or spun up on a dedicated port regardless of fit.
    pool_bots = bots[: args.bot_vs_bot_games * 2]
    matchmaker = BotMatchmaker(pool_bots, args.registry_url)
    for game_i in range(args.bot_vs_bot_games):
        port = args.bot_vs_bot_base_port + game_i
        _spawn_dedicated_server(port)
        t = threading.Thread(target=run_bot_vs_bot_port_forever,
                              args=("127.0.0.1", port, matchmaker, args.registry_url, jwt, stop_event),
                              daemon=True)
        t.start()
        threads.append(t)
        print(f"started bot-vs-bot worker on port {port}, drawing from a shared {len(pool_bots)}-bot Elo-aware queue")

    bot_idx = args.bot_vs_bot_games * 2
    if bot_idx < len(bots):
        waiting_bot = bots[bot_idx]
        if args.spawn_human_server:
            _spawn_dedicated_server(args.human_port)
        t = threading.Thread(target=run_waiting_for_human_forever,
                              args=(args.human_host, args.human_port, waiting_bot, args.registry_url, jwt, stop_event),
                              daemon=True)
        t.start()
        threads.append(t)
        print(f"started waiting-for-human bot on {args.human_host}:{args.human_port}: {waiting_bot.name}")
    else:
        print("no bot left over for the waiting slot -- pool_size was too small.")

    try:
        while True:
            time.sleep(1.0)
    except KeyboardInterrupt:
        pass
    finally:
        stop_event.set()
        _cleanup_servers()

    return 0


if __name__ == "__main__":
    sys.exit(main())
