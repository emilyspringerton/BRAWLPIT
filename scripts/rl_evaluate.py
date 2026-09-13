#!/usr/bin/env python3
"""
scripts/rl_evaluate.py (S421-04) -- the real evaluation mechanism founder real-time asked for:
"can we start recording the match results with the actual outcomes?"

Runs a genuine, synchronous head-to-head match between two saved PPO checkpoints, using
BRAWLPIT's own existing MATCHMAKING_MODE_1V1 (apps/server/src/main.c's own mm_start_match_1v1) --
no server change needed, since a real 1v1 duel seating two real network clients into one match is
already exactly what that queue does. Determines a winner by final stock count and calls
IDUNA's real POST /api/v1/brawlpit-checkpoints/match-result (rl_registry.py's own
record_match_result), which is the ONLY thing that ever moves a checkpoint's Elo off its
inherited value (see IDUNA/internal/brawlpit/checkpoint_store.go's own RecordMatchResult doc
comment).

Real, honest, named scope: matchmaking's own mm_start_match_1v1 unconditionally calls
stage_set_active(STAGE_FD) -- an evaluation match always plays on STAGE_FD regardless of any
--level a dedicated training server might have loaded. A real, separate follow-up if evaluation
on a custom level is ever wanted.

Usage:
    python3 scripts/rl_evaluate.py --host 127.0.0.1 --port 7978 \\
        --checkpoint-a path/to/a.zip --checkpoint-b path/to/b.zip \\
        --registry-url http://localhost:8080 --agent-secret ... \\
        --a-id 16 --b-id 18
"""

import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from rl_env_packet import (  # noqa: E402
    MATCH_TIME_LIMIT_TICKS,
    MATCHMAKING_MODE_1V1,
    NetHeader,
    PACKET_FIND_MATCH,
    PACKET_MATCH_FOUND,
    PacketClient,
    build_observation,
    decode_match_found,
    encode_find_match_1v1,
    find_match_1v1_both,
    find_self_and_opponent,
)
from rl_registry import authenticate, record_match_result  # noqa: E402

# S443: encode_find_match_1v1/decode_match_found/find_match_1v1_both/MATCHMAKING_MODE_1V1 all
# moved to rl_env_packet.py (so BrawlpitPacketEnv's own real self-play mode can use them without
# a circular import) -- re-imported above and re-exported here unchanged so rl_bot_pool.py's own
# existing `from rl_evaluate import encode_find_match_1v1, ...` keeps working with zero changes.


def run_evaluation_match(host, port, checkpoint_a_path, checkpoint_b_path, max_ticks=MATCH_TIME_LIMIT_TICKS):
    """Runs one real, synchronous 1v1 duel between two saved PPO checkpoints. Returns score_a
    (1.0 A won, 0.0 A lost, 0.5 a real draw) -- the exact shape record_match_result expects.

    `max_ticks` defaults to MATCH_TIME_LIMIT_TICKS (S429, founder real-time: "add a timer - 2.5
    minutes - if time expires it's a draw and thats counted the same as a loss in terms of
    negative reward") -- the same real, canonical 2.5-minute match clock BrawlpitPacketEnv now
    enforces during training, so an evaluation match plays out under the exact same real time
    limit a real match would. A timeout is ALWAYS scored as a real draw (0.5), never a win for
    whoever happened to be ahead on stocks when the clock ran out -- the same "time expiring is
    always a draw" rule the reward function enforces, applied here to Elo instead."""
    from stable_baselines3 import PPO  # imported lazily -- this module needs SB3, callers that

    model_a = PPO.load(checkpoint_a_path, device="cpu")
    model_b = PPO.load(checkpoint_b_path, device="cpu")

    client_a = PacketClient(host, port)
    client_b = PacketClient(host, port)
    find_match_1v1_both(client_a, client_b)
    print(f"matched: A=client_id {client_a.client_id}, B=client_id {client_b.client_id}")

    own_a = opp_a = None
    timed_out = True
    for tick in range(max_ticks):
        _, players_a = client_a.recv_snapshot()
        _, players_b = client_b.recv_snapshot()
        own_a, opp_a = find_self_and_opponent(players_a, client_a.client_id)
        own_b, opp_b = find_self_and_opponent(players_b, client_b.client_id)
        if own_a is None or opp_a is None or own_b is None or opp_b is None:
            continue

        action_a, _ = model_a.predict(build_observation(own_a, opp_a), deterministic=True)
        action_b, _ = model_b.predict(build_observation(own_b, opp_b), deterministic=True)
        client_a.send_action(float(action_a[0]), float(action_a[1]),
                              jump=action_a[2] > 0, attack=action_a[3] > 0, shield=action_a[4] > 0, special=action_a[5] > 0)
        client_b.send_action(float(action_b[0]), float(action_b[1]),
                              jump=action_b[2] > 0, attack=action_b[3] > 0, shield=action_b[4] > 0, special=action_b[5] > 0)

        if own_a.stocks == 0 or opp_a.stocks == 0:
            timed_out = False
            break

    client_a.close()
    client_b.close()

    if own_a is None or opp_a is None:
        raise RuntimeError("evaluation match never observed both fighters -- treat as inconclusive, don't record a result")

    if timed_out:
        return 0.5
    if own_a.stocks > opp_a.stocks:
        return 1.0
    if own_a.stocks < opp_a.stocks:
        return 0.0
    return 0.5


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--host", default="127.0.0.1")
    p.add_argument("--port", type=int, required=True, help="a real bin/brawlpit_server already running with its matchmaking loop live")
    p.add_argument("--checkpoint-a", required=True)
    p.add_argument("--checkpoint-b", required=True)
    p.add_argument("--max-ticks", type=int, default=1800)
    p.add_argument("--registry-url", default=os.environ.get("IDUNA_BASE_URL"))
    p.add_argument("--agent-name", default=os.environ.get("IDUNA_AGENT_NAME", "BRAWLPIT-RL"))
    p.add_argument("--agent-secret", default=os.environ.get("IDUNA_AGENT_SECRET"))
    p.add_argument("--a-id", type=int, help="the real registry checkpoint id for --checkpoint-a")
    p.add_argument("--b-id", type=int, help="the real registry checkpoint id for --checkpoint-b")
    args = p.parse_args()

    score_a = run_evaluation_match(args.host, args.port, args.checkpoint_a, args.checkpoint_b, args.max_ticks)
    print(f"result: score_a={score_a}")

    if args.registry_url and args.a_id and args.b_id:
        if not args.agent_secret:
            print("--registry-url/--a-id/--b-id were given but --agent-secret wasn't -- not recording.")
        else:
            jwt = authenticate(args.registry_url, args.agent_name, args.agent_secret)
            result = record_match_result(args.registry_url, jwt, args.a_id, args.b_id, score_a)
            print(f"recorded: a elo={result['a']['elo']:.0f}, b elo={result['b']['elo']:.0f}")
