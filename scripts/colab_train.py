#!/usr/bin/env python3
"""
scripts/colab_train.py -- the real, single "drop into one Colab cell" bootstrap the founder
asked for: "a single python script to drop into a colab cell to download the repo and start the
league would be awesome i guess it needs to download the league from the registry too?"

Paste this whole file's contents into one Colab cell and run it (or, once BRAWLPIT is already
cloned somewhere, `!python3 scripts/colab_train.py`) -- it clones/updates the repo, builds the
real training binary, authenticates against IDUNA, downloads each league role's newest checkpoint
from the shared registry so this run continues the SAME real, ongoing league instead of starting
three fresh random networks from generation 0 every time a Colab runtime recycles, and then kicks
off a real, long training run that keeps pushing new generations back to that same registry.

On "oauth into IDUNA to get the token used for training": this system's real token exchange for a
MACHINE (not a human) is IDUNA's M2M agent-secret grant (POST /api/v1/auth/agent, exchanging a
pre-provisioned agent_name/agent_secret pair for a short-lived Bearer JWT) -- the same mechanism
every other automated agent in this monorepo already uses (REDGARDEN-BOTS, ECOWAR-BOTS, ...), not
a browser OAuth redirect (that's IDUNA's real HUMAN login flow, for a person's own Google/local
account -- a Colab training run isn't a person). This script's own IDUNA_AGENT_SECRET input IS
that real "auth to get the token used for training" step -- rl_registry.authenticate() does the
actual exchange, honestly named here rather than pretending this is literal OAuth.

Prereqs: a GitHub personal access token with `repo` read scope (this repo is private), and a real
BRAWLPIT-RL agent secret from IDUNA/var/agent-secrets.env (IDUNA_SECRET_BRAWLPIT_RL) if you want
this run to join the shared registry -- leave the secret blank for a local-only smoke run.
"""

import getpass
import os
import subprocess
import sys

REPO_URL_TEMPLATE = "https://{token}@github.com/emilyspringerton/BRAWLPIT.git"
IDUNA_BASE_URL = os.environ.get("IDUNA_BASE_URL", "https://okemily.com")


def _run(cmd, **kwargs):
    print(f"$ {' '.join(cmd)}")
    subprocess.run(cmd, check=True, **kwargs)


def _bootstrap_repo(github_token):
    """Clones BRAWLPIT if it isn't here yet, or pulls the latest if it already is -- makes
    re-running this exact cell in the same Colab runtime (e.g. after a crash) safe, not just a
    first-run script. commander_mod.c (PARENA's own compiled output) is already checked in, so
    no need to clone or build PARENA itself."""
    if os.path.isdir("BRAWLPIT"):
        print("BRAWLPIT already present -- pulling latest instead of cloning fresh.")
        _run(["git", "-C", "BRAWLPIT", "pull", "--ff-only"])
    else:
        _run(["git", "clone", REPO_URL_TEMPLATE.format(token=github_token), "BRAWLPIT"])
    os.chdir("BRAWLPIT")


def _bootstrap_build():
    # Colab's own base image already ships gcc/build-essential -- this is a real, harmless
    # no-op safety net for a from-scratch machine, not assumed-necessary busywork.
    _run(["apt-get", "-qq", "update"])
    _run(["apt-get", "-qq", "install", "-y", "build-essential"])
    _run(["chmod", "+x", "scripts/build_training.sh"])
    _run(["./scripts/build_training.sh"])
    _run([sys.executable, "-m", "pip", "install", "-q", "gymnasium", "stable-baselines3"])


def main():
    # getpass falls back cleanly to a real, already-set env var (e.g. a Colab "Secret") so this
    # script also runs non-interactively -- matches the existing notebook's own real convention.
    github_token = os.environ.get("GITHUB_TOKEN") or getpass.getpass("GitHub personal access token (repo read scope): ")
    iduna_agent_secret = os.environ.get("IDUNA_AGENT_SECRET") or getpass.getpass(
        "BRAWLPIT-RL agent secret (blank to skip the shared registry -- local-only run): "
    )

    _bootstrap_repo(github_token)
    del github_token  # real, deliberate -- don't keep the token in memory longer than the clone needs it
    _bootstrap_build()

    sys.path.insert(0, "scripts")
    from rl_registry import authenticate, list_checkpoints  # noqa: E402  -- only importable after the clone above

    if iduna_agent_secret:
        jwt = authenticate(IDUNA_BASE_URL, "BRAWLPIT-RL", iduna_agent_secret)
        print(f"Authenticated with the shared registry at {IDUNA_BASE_URL} -- got a real, short-lived Bearer JWT.")
        print("-- current league standings (before this run adds anything) --")
        for c in sorted(list_checkpoints(IDUNA_BASE_URL), key=lambda c: (c["role"], -c["generation"]))[:20]:
            print(f"  id={c['id']:4d}  {c['role']:18s} gen={c['generation']:3d}  elo={c['elo']:7.1f}  {c.get('name', '')}")
        del jwt
    else:
        print("No agent secret given -- this run will train locally only and NOT join the shared league.")

    cmd = [
        sys.executable, "scripts/rl_train_packet.py",
        "--total-timesteps", os.environ.get("BRAWLPIT_TOTAL_TIMESTEPS", "1000000"),
        # S431, founder real-time: "can we also check in the model half as frequently?" --
        # doubled from the prior 2048 default so a real checkpoint push (and the S424 automatic
        # evaluation match that comes with it) happens half as often.
        "--save-freq", os.environ.get("BRAWLPIT_SAVE_FREQ", "4096"),
        "--league-dir", "league_data",
        "--output-dir", "rl_packet_checkpoints",
    ]
    # S431, founder real-time: "can we switch the training level to the one called 4" -- optional,
    # unset by default (falls back to bin/brawlpit_server's own STAGE_FD default) so this script
    # doesn't silently change behavior for anyone not setting BRAWLPIT_LEVEL.
    level = os.environ.get("BRAWLPIT_LEVEL")
    if level:
        cmd += ["--level", level]
    env = os.environ.copy()
    if iduna_agent_secret:
        env["IDUNA_AGENT_SECRET"] = iduna_agent_secret
        cmd += [
            "--registry-url", IDUNA_BASE_URL,
            "--registry-source-location", "colab",
            # The real fix for "i guess it needs to download the league from the registry too?"
            # -- warm-starts each role from its own newest real registry checkpoint (actual PPO
            # weights, not just an Elo number) instead of three fresh random networks, so this
            # runtime continues the SAME shared league other machines have been training.
            "--resume-from-registry",
        ]
    del iduna_agent_secret

    print("\nStarting real training -- this runs until --total-timesteps or the cell/runtime is stopped.")
    subprocess.run(cmd, env=env, check=True)


if __name__ == "__main__":
    main()
