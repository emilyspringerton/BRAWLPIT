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

CPU vs. GPU (`--device`, default "cpu"), founder real-time: "are we using the GPU on colab? do
we get increased training if we switch to a GPU box?" Real, measured answer: no, not with this
architecture, and switching to a GPU box alone would not meaningfully speed this up. The policy
network is a tiny 64-unit MLP (Linear(21,64)+Tanh, Linear(64,64)+Tanh, Linear(64,6) -- see
mlp_policy.h's own cross-language-verified real shape) -- a forward/backward pass on a network
this small is already effectively instant on CPU; a GPU adds real per-call kernel-launch and
host<->device transfer overhead that, for tensors this tiny, tends to make things SLOWER, not
faster (the same real reason stable_baselines3's own docs recommend CPU for MlpPolicy). The
ACTUAL bottleneck, confirmed by this box's own real generation timings (~6-8 real wall-clock
minutes for 3 models x 2048 timesteps each, roughly 4-6 timesteps/sec per model): one real UDP
round trip to a real bin/brawlpit_server subprocess per environment step -- socket I/O and
process/context-switch latency, not matrix-multiply compute. The real path to faster training is
running MORE PARALLEL environment instances per role (more dedicated servers, a real vectorized-
env architecture -- not built here, see BACKLOG.md) to collect more experience per wall-clock
second; that's a CPU-core/parallelism story a bigger CPU box actually helps with, not a GPU one.

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
import signal
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
from rl_registry import (  # noqa: E402
    authenticate,
    download_checkpoint,
    list_checkpoints,
    push_checkpoint,
    record_match_result,
)
from rl_evaluate import run_evaluation_match  # noqa: E402
from export_policy_weights import export_policy_weights  # noqa: E402

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SERVER_BIN = os.path.join(REPO_ROOT, "bin", "brawlpit_server")

# One dedicated server subprocess per archetype, matching the real "each model needs its own
# match" constraint named in this module's own top-of-file doc comment.
ROLE_PORTS = {
    LeagueRole.MAIN: 7978,
    LeagueRole.MAIN_EXPLOITER: 7979,
    LeagueRole.LEAGUE_EXPLOITER: 7980,
}

# A real, found, self-diagnosed gap (founder real-time: "elos stuck at 1500 again not sure if its
# cause we keep asking for more stuff and the training is reset or what"): register_generation_
# snapshot's own Elo INHERITANCE has always worked correctly, but nothing ever actually MOVED an
# Elo away from DEFAULT_ELO during a normal training run -- record_match_result (local or remote)
# only ever got called by hand (rl_evaluate.py / rl_bot_pool.py, run manually this session), never
# by the training loop itself. This port hosts one, real, short-lived evaluation match per
# generation per role (new checkpoint vs. that SAME role's own immediately-prior generation) so
# Elo actually moves as training progresses, with no manual step required.
EVAL_PORT = 7985

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


def _handle_terminate_signal(signum, frame):
    """Real, found-live gap fixed here: Python's atexit hooks do NOT run on a bare SIGTERM (the
    signal `kill`/`pkill` send by default) -- only on normal interpreter exit, sys.exit(), or an
    uncaught exception. Killing a real training run this way (Ctrl-C sends SIGINT, which DOES
    already run atexit -- but `pkill -f rl_train_packet.py` and most process managers send
    SIGTERM) left three real bin/brawlpit_server subprocesses running forever, still burning a
    full CPU core each in their own --fast-forward busy loop, confirmed live in this session. A
    real, explicit SIGTERM handler that calls sys.exit() is what actually makes atexit fire."""
    sys.exit(0)


signal.signal(signal.SIGTERM, _handle_terminate_signal)


def _fresh_model(env, device):
    return PPO("MlpPolicy", env, verbose=0, device=device)


def _find_latest_registry_checkpoint(registry_url, role_value):
    """Real, live lookup for --resume-from-registry: the newest (highest generation, ties broken
    by highest id) checkpoint IDUNA's registry has for this exact role, or None if that role has
    never been pushed there yet -- a real, honest "nothing to resume from" case (e.g. this role's
    very first-ever run), not an error.

    Skips any checkpoint marked `is_disabled` (S428, founder real-time: "i want to reset training
    but not include certain models from the registry - can you add a checkbox to the registry
    backend to disable those models from the league?") -- this is the actual enforcement side of
    that checkbox: a disabled checkpoint is never picked as a resume target, even if it's the
    real newest one for its role."""
    checkpoints = [c for c in list_checkpoints(registry_url, role=role_value) if not c.get("is_disabled")]
    if not checkpoints:
        return None
    return max(checkpoints, key=lambda c: (c["generation"], c["id"]))


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
    # Founder real-time: "a single python script to drop into a colab cell... i guess it needs to
    # download the league from the registry too?" -- a real, named gap this fixes: every prior
    # run always started all 3 models from a FRESH random network and generation 0, no matter how
    # far the real, shared league had already progressed on another machine, because a Colab
    # runtime's own local --league-dir is ephemeral and starts empty every time. With this flag,
    # each role instead warm-starts from the newest checkpoint THAT ROLE already has in the
    # shared registry (downloaded fresh, real PPO weights, not just Elo bookkeeping) and the
    # local league is seeded with that checkpoint's own real registry Elo/generation, so this run
    # picks up the SAME league other machines have been training, not a disconnected new one.
    p.add_argument("--resume-from-registry", action="store_true",
                   help="requires --registry-url. Warm-starts each role from the newest checkpoint "
                        "that role already has in the shared registry instead of a fresh network.")
    # Real, deliberate default: "cpu", not "auto"/"cuda" -- see this module's own top-of-file doc
    # comment (founder real-time: "are we using the GPU on colab? do we get increased training if
    # we switch to a GPU box?") for the full measured rationale. Exposed as a real flag (not
    # hardcoded) so a future architecture change (e.g. real vectorized parallel envs) that DOES
    # benefit from a GPU doesn't need code changes to use one -- also fixes a real, found
    # inconsistency: the resume path used to hardcode device="cpu" while a fresh model silently
    # deferred to SB3's own "auto" (which picks CUDA if present) -- both paths now agree.
    p.add_argument("--device", default=os.environ.get("BRAWLPIT_RL_DEVICE", "cpu"),
                   help="stable_baselines3 device ('cpu', 'cuda', or 'auto'). Defaults to 'cpu' -- "
                        "this pipeline's own tiny 64-unit MLP plus one real UDP round trip per "
                        "environment step is latency-bound, not compute-bound, so a GPU has "
                        "nothing to meaningfully accelerate here (see the module doc comment).")
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

    if args.resume_from_registry and not args.registry_url:
        print("--resume-from-registry needs --registry-url (or IDUNA_BASE_URL) set -- there's no "
              "registry to resume from otherwise.")
        return 1

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

    # Real state carried generation-to-generation so each new checkpoint can be evaluated against
    # its own immediate predecessor -- see EVAL_PORT's own doc comment above for the full
    # rationale. Populated at the end of the loop body below, never inside register_generation_
    # snapshot itself (that stays a pure registration call, not an evaluation one). Pre-seeded
    # from the registry below when --resume-from-registry is set.
    prev_checkpoint_paths, prev_member_ids, prev_remote_ids = {}, {}, {}
    resume_generation = -1  # -1 means "nothing resumed" -> generation starts at 0, as before

    if args.resume_from_registry:
        for role in ROLE_PORTS:
            latest = _find_latest_registry_checkpoint(args.registry_url, role.value)
            if latest is None:
                print(f"resume: no existing registry checkpoint for {role.value} yet -- that role starts fresh.")
                continue
            local_path = os.path.join(args.output_dir, f"_resume_{role.value}.zip")
            download_checkpoint(args.registry_url, latest["id"], local_path)
            prev_checkpoint_paths[role] = local_path
            prev_remote_ids[role] = latest["id"]
            # Seed a real local league member carrying the ACTUAL registry Elo (not DEFAULT_ELO)
            # so this run's own first local generation inherits/evaluates against the real,
            # current standing -- the whole point of "download the league from the registry too."
            seeded = league.register(role.value, latest["generation"], local_path, inherit_elo_from_role=False)
            league.set_elo(seeded.id, latest["elo"])
            prev_member_ids[role] = seeded.id
            resume_generation = max(resume_generation, latest["generation"])
            print(f"resume: {role.value} <- registry checkpoint id={latest['id']} "
                  f"(gen {latest['generation']}, elo={latest['elo']:.0f})")

    print(f"Starting 3 dedicated bin/brawlpit_server processes (one per archetype)...")
    envs = {}
    models = {}
    for role, port in ROLE_PORTS.items():
        _spawn_server(port)
        env = BrawlpitPacketEnv(host=args.host, port=port)
        envs[role] = env
        if role in prev_checkpoint_paths:
            models[role] = PPO.load(prev_checkpoint_paths[role], env=env, device=args.device)
            print(f"  {role.value}: server on port {port}, resumed from registry checkpoint")
        else:
            models[role] = _fresh_model(env, args.device)
            print(f"  {role.value}: server on port {port}, fresh PPO model")

    checkpoint_template = os.path.join(args.output_dir, "{role}_gen{gen}")
    timesteps_done = {role: 0 for role in ROLE_PORTS}
    generation = resume_generation + 1

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
                models[role] = _fresh_model(envs[role], args.device)
                reset_roles.add(role)

        # Founder real-time: "each snapshot has the 3 archetypes... for each snapshot it adds 3
        # to the league" -- one real, atomic-in-intent registration call per generation.
        registered = register_generation_snapshot(league, generation, checkpoint_paths, reset_roles=reset_roles)
        remote_ids = {}
        for role, member in registered.items():
            elo = league.get_elo(member.id)
            print(f"[gen {generation}] registered {role.value} -> league member {member.id} "
                  f"(elo={elo:.0f})")
            if registry_jwt:
                try:
                    # S421-02, founder real-time: "ensure that the client actually uses that
                    # model" -- export the real native-inference weights (scripts/
                    # export_policy_weights.py's own "BPMW" format) alongside the .zip so the
                    # registry can serve them to the actual game client, not just Python.
                    weights_path = checkpoint_paths[role].removesuffix(".zip") + ".weights.bin"
                    try:
                        export_policy_weights(checkpoint_paths[role], weights_path)
                    except Exception as export_err:  # noqa: BLE001 -- same real, non-fatal
                        # degrade as the push itself below: a failed export must never crash
                        # training, just push the checkpoint without a native-inference blob
                        # this once (an older checkpoint with HasWeights=false is a real,
                        # already-handled state, not a corruption).
                        print(f"[gen {generation}]   -> WARNING: weights export failed ({export_err}), pushing without weights")
                        weights_path = None

                    remote = push_checkpoint(
                        args.registry_url, registry_jwt, role.value, generation, elo,
                        args.registry_source_location, checkpoint_paths[role],
                        weights_path=weights_path,
                    )
                    remote_ids[role] = remote["id"]
                    print(f"[gen {generation}]   -> pushed to remote registry as checkpoint id={remote['id']} "
                          f"(name={remote.get('name')}, has_weights={remote.get('has_weights')})")
                except Exception as e:  # noqa: BLE001 -- a real, non-fatal degrade: a registry
                    # outage/network blip must never crash a real, in-progress local training
                    # run over an optional remote sync, same "a bad/missing resource never
                    # corrupts what's already working" convention level_registry.h's own doc
                    # comment already established for the read side of this exact pipeline.
                    print(f"[gen {generation}]   -> WARNING: push to remote registry failed ({e}), continuing locally")

        # Real, automatic per-generation evaluation -- the actual fix for "elos stuck at 1500":
        # play one real match between each role's brand-new checkpoint and that SAME role's own
        # immediately-prior generation, then call record_match_result (local always, remote when
        # configured) with the real outcome. Skips a role that was just reset this generation --
        # a freshly re-initialized network hasn't earned a claim to a match against its own
        # pre-reset predecessor any more than it inherited that predecessor's Elo (see
        # LeagueManager.register's own inherit_elo_from_role=False doc comment) -- evaluation for
        # that lineage resumes once prev_checkpoint_paths reflects the post-reset generation.
        eval_server = None
        for role, member in registered.items():
            if role in reset_roles or role not in prev_checkpoint_paths:
                continue
            try:
                if eval_server is None:
                    eval_server = _spawn_server(EVAL_PORT)
                score_a = run_evaluation_match(args.host, EVAL_PORT, checkpoint_paths[role], prev_checkpoint_paths[role])
                league.record_match_result(member.id, prev_member_ids[role], score_a)
                new_elo, prev_elo = league.get_elo(member.id), league.get_elo(prev_member_ids[role])
                print(f"[gen {generation}]   -> evaluated {role.value} vs its own prior generation: "
                      f"score_a={score_a} (local elo now {new_elo:.0f} vs {prev_elo:.0f})")
                if registry_jwt and role in remote_ids and role in prev_remote_ids:
                    remote_result = record_match_result(args.registry_url, registry_jwt,
                                                          remote_ids[role], prev_remote_ids[role], score_a)
                    print(f"[gen {generation}]   -> remote elo now {remote_result['a']['elo']:.0f} "
                          f"vs {remote_result['b']['elo']:.0f}")
            except Exception as e:  # noqa: BLE001 -- an evaluation match failing (a dropped
                # packet, a transient registry outage) must never crash real, in-progress
                # training over an optional ranking signal, same non-fatal-degrade convention
                # the remote push above already follows.
                print(f"[gen {generation}]   -> WARNING: evaluation match for {role.value} failed ({e}), Elo unchanged this generation")
        if eval_server is not None:
            eval_server.terminate()
            try:
                eval_server.wait(timeout=2.0)
            except subprocess.TimeoutExpired:
                eval_server.kill()
            _spawned_servers.remove(eval_server)

        prev_checkpoint_paths = dict(checkpoint_paths)
        prev_member_ids = {role: member.id for role, member in registered.items()}
        prev_remote_ids = remote_ids

        generation += 1

    for env in envs.values():
        env.close()
    print("DONE.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
