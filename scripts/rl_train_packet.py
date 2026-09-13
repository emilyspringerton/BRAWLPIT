#!/usr/bin/env python3
"""
scripts/rl_train_packet.py (S419-08) -- the real training orchestrator: runs THREE simultaneous
PPO models (Main, Main Exploiter, League Exploiter -- scripts/rl_league.py's own real AlphaStar
league roles) against BRAWLPIT's real packet-level env (scripts/rl_env_packet.py), and, on every
checkpoint-save cycle, registers all three as one real "snapshot" into the shared league
(register_generation_snapshot) -- matching the founder's own explicit clarification: "each
snapshot has the 3 archetypes the normal the exploiter and the league exploiter so for each
snapshot it adds 3 to the league."

Real, deliberate architecture choice, distinct from REDGARDEN's own rl_train_team.py (which runs
each of the 3 roles as a SEPARATE process, coordinating only through the shared LeagueManager
directory on disk): here, ONE process drives all three models in the same training loop, since
the founder's own framing ("for each snapshot it adds 3 to the league") describes one shared
snapshot cadence across all three archetypes, not three independently-paced processes. Each
model gets its OWN dedicated bin/brawlpit_server subprocess (a real UDP server can only usefully
host one packet-level RL client against BRAWLPIT's own current single-match-at-boot design --
see docs/RL_TRAINING_NORTHSTAR.md's own §5) on its own port, all three spawned and torn down by
this script.

Real, honest, named scope limit (NOT a self-play opponent pool yet): each model's own opponent is
whatever bin/brawlpit_server's own local_init_match/PACKET_RESET_MATCH produces by default today
-- a static, non-bot-driven slot 0 (see local_game.h's own local_init_match: `is_bot = (i > 0)`,
so slot 0 is never bot-driven). This is real training against a fixed, static target, not true
self-play against the growing checkpoint pool the league itself tracks -- loading a past
checkpoint's policy to actually DRIVE the opponent slot server-side is real, separate, not-yet-
built work (see BACKLOG.md S419-10). register_generation_snapshot/LeagueManager/Elo are still
real and fully functional today for tracking and ranking the checkpoints this script produces;
what's missing is feeding them back in as live opponents.

NOTE ON VERIFICATION: same documented limitation as scripts/rl_env_packet.py -- gymnasium/
stable_baselines3 are not installable in the sandbox this file was written in (externally
managed Python, no sudo/venv). This file is written to stable_baselines3's real, documented PPO
API and was NOT run end-to-end in this session. See docs/RL_TRAINING_NORTHSTAR.md's own Colab
workflow for where this is actually meant to run (a normal Python environment with pip access).

Usage:
    python3 scripts/rl_train_packet.py --total-timesteps 200000 --save-freq 20000 \\
        --league-dir league_data --reset-every-n-generations 5
"""

import argparse
import atexit
import os
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from rl_league import (  # noqa: E402
    LeagueManager,
    LeagueRole,
    register_generation_snapshot,
    should_reset_main_exploiter,
)

try:
    from stable_baselines3 import PPO
    _HAVE_SB3 = True
except ImportError:
    _HAVE_SB3 = False

from rl_env_packet import BrawlpitPacketEnv, _HAVE_GYM  # noqa: E402
from rl_registry import authenticate, push_checkpoint  # noqa: E402

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SERVER_BIN = os.path.join(REPO_ROOT, "bin", "brawlpit_server")

# One dedicated server subprocess per archetype, matching the real "each model needs its own
# match" constraint named in this module's own top-of-file doc comment.
ROLE_PORTS = {
    LeagueRole.MAIN: 7978,
    LeagueRole.MAIN_EXPLOITER: 7979,
    LeagueRole.LEAGUE_EXPLOITER: 7980,
}

_spawned_servers = []


def _spawn_server(port):
    """Starts one real bin/brawlpit_server --fast-forward --port <port> subprocess. apps/server/
    src/main.c gained a real --port flag (S419-11) specifically so three of these can run
    simultaneously, one per league archetype, without colliding on the old hardcoded 6978 --
    live-verified in this session: two real server processes bound to different ports (7978/
    7979) both answered a real PACKET_CONNECT handshake independently and correctly."""
    proc = subprocess.Popen([SERVER_BIN, "--fast-forward", "--port", str(port)], cwd=REPO_ROOT,
                             stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    _spawned_servers.append(proc)
    time.sleep(0.5)  # real, minimal startup grace period -- server_net_init binds synchronously
    return proc


def _cleanup_servers():
    for proc in _spawned_servers:
        proc.terminate()
    for proc in _spawned_servers:
        try:
            proc.wait(timeout=2.0)
        except subprocess.TimeoutExpired:
            proc.kill()


atexit.register(_cleanup_servers)


def _fresh_model(env):
    return PPO("MlpPolicy", env, verbose=0)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--total-timesteps", type=int, default=200_000)
    p.add_argument("--save-freq", type=int, default=20_000)
    p.add_argument("--league-dir", default=os.environ.get("BRAWLPIT_LEAGUE_DIR", "league_data"))
    p.add_argument("--reset-every-n-generations", type=int, default=5,
                   help="Main Exploiter's own periodic full reset cadence (NORTHSTAR §25.4.1, "
                        "video 13:55). <= 0 disables resetting entirely.")
    p.add_argument("--output-dir", default=os.environ.get("BRAWLPIT_RL_OUTPUT_DIR", "rl_packet_checkpoints"))
    p.add_argument("--host", default="127.0.0.1")
    # S420: pushing to the real, remote, shared registry (IDUNA) is opt-in -- omit
    # --registry-url and this behaves exactly as it did before S420, a purely local run against
    # --league-dir only. Founder real-time: "lets make a checkpoint registry so we can train
    # from multiple locations and then we can add checkpoints from colab?"
    p.add_argument("--registry-url", default=os.environ.get("IDUNA_BASE_URL"),
                   help="e.g. https://okemily.com -- if set, every generation's 3 checkpoints "
                        "also push to IDUNA's real, shared checkpoint registry, not just the "
                        "local --league-dir.")
    p.add_argument("--registry-agent-name", default=os.environ.get("IDUNA_AGENT_NAME", "BRAWLPIT-RL"))
    p.add_argument("--registry-agent-secret", default=os.environ.get("IDUNA_AGENT_SECRET"))
    p.add_argument("--registry-source-location", default=os.environ.get("BRAWLPIT_SOURCE_LOCATION", "unknown"),
                   help="a real, free-text label for where this training run is happening "
                        "(e.g. 'colab', a hostname) -- recorded on every pushed checkpoint so "
                        "the registry can show where each one came from.")
    args = p.parse_args()

    registry_jwt = None
    if args.registry_url:
        if not args.registry_agent_secret:
            print("--registry-url was set but --registry-agent-secret (or IDUNA_AGENT_SECRET) "
                  "wasn't -- refusing to silently skip the registry push. Pass the secret or "
                  "drop --registry-url for a local-only run.")
            return 1
        registry_jwt = authenticate(args.registry_url, args.registry_agent_name, args.registry_agent_secret)
        print(f"Authenticated with the remote checkpoint registry at {args.registry_url}.")

    if not _HAVE_SB3 or not _HAVE_GYM:
        print("stable_baselines3 and/or gymnasium are not installed. This orchestrator needs a "
              "real Python environment with pip access (this repo's own sandbox is externally "
              "managed and has neither) -- see docs/RL_TRAINING_NORTHSTAR.md's own Colab "
              "workflow. Nothing was run.")
        return 1
    if not os.path.exists(SERVER_BIN):
        print(f"{SERVER_BIN} not found -- run ./scripts/build_training.sh first.")
        return 1

    os.makedirs(args.output_dir, exist_ok=True)
    league = LeagueManager(args.league_dir)

    print(f"Starting 3 dedicated bin/brawlpit_server processes (one per archetype)...")
    envs = {}
    models = {}
    for role, port in ROLE_PORTS.items():
        _spawn_server(port)
        env = BrawlpitPacketEnv(host=args.host, port=port)
        envs[role] = env
        models[role] = _fresh_model(env)
        print(f"  {role.value}: server on port {port}, fresh PPO model")

    checkpoint_template = os.path.join(args.output_dir, "{role}_gen{gen}")
    timesteps_done = {role: 0 for role in ROLE_PORTS}
    generation = 0

    while min(timesteps_done.values()) < args.total_timesteps:
        checkpoint_paths = {}
        reset_roles = set()

        for role in ROLE_PORTS:
            model = models[role]
            chunk = min(args.save_freq, args.total_timesteps - timesteps_done[role])
            if chunk <= 0:
                checkpoint_paths[role] = checkpoint_template.format(role=role.value, gen=generation) + ".zip"
                continue
            before = model.num_timesteps
            model.learn(total_timesteps=chunk, reset_num_timesteps=False)
            timesteps_done[role] += model.num_timesteps - before

            ckpt_path = checkpoint_template.format(role=role.value, gen=generation)
            model.save(ckpt_path)
            checkpoint_paths[role] = ckpt_path + ".zip"
            print(f"[gen {generation}] {role.value}: saved {ckpt_path}.zip "
                  f"({timesteps_done[role]}/{args.total_timesteps} timesteps)")

            if role == LeagueRole.MAIN_EXPLOITER and should_reset_main_exploiter(
                    generation, args.reset_every_n_generations):
                print(f"[gen {generation}] Main Exploiter: resetting to a freshly initialized network.")
                models[role] = _fresh_model(envs[role])
                reset_roles.add(role)

        # Founder real-time: "each snapshot has the 3 archetypes... for each snapshot it adds 3
        # to the league" -- one real, atomic-in-intent registration call per generation.
        registered = register_generation_snapshot(league, generation, checkpoint_paths, reset_roles=reset_roles)
        for role, member in registered.items():
            elo = league.get_elo(member.id)
            print(f"[gen {generation}] registered {role.value} -> league member {member.id} "
                  f"(elo={elo:.0f})")
            if registry_jwt:
                try:
                    remote = push_checkpoint(
                        args.registry_url, registry_jwt, role.value, generation, elo,
                        args.registry_source_location, checkpoint_paths[role],
                    )
                    print(f"[gen {generation}]   -> pushed to remote registry as checkpoint id={remote['id']}")
                except Exception as e:  # noqa: BLE001 -- a real, non-fatal degrade: a registry
                    # outage/network blip must never crash a real, in-progress local training
                    # run over an optional remote sync, same "a bad/missing resource never
                    # corrupts what's already working" convention level_registry.h's own doc
                    # comment already established for the read side of this exact pipeline.
                    print(f"[gen {generation}]   -> WARNING: push to remote registry failed ({e}), continuing locally")

        generation += 1

    for env in envs.values():
        env.close()
    print("DONE.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
