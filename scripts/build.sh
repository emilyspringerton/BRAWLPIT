#!/usr/bin/env bash
# scripts/build.sh — build BRAWLPIT + run the physics smoke test.
#
# Added 2026-08-14: this repo never had a build script at all, unlike
# every sibling repo in this monorepo (REDGARDEN/scripts/build.sh,
# GOLDENBAND/scripts/build_and_test.sh, etc.) -- the only documented build
# path was one raw gcc invocation in README.md's own "Setup" section.
# Founder real-time: "i think tipjar build is failing not sure" -- traced
# live: the actual C code was never broken (fresh-clone gcc build + tests
# both pass clean), but running `make` at the repo root -- the natural
# first thing to try, matching every other repo's own convention -- fails
# with "No targets specified and no makefile found" since no Makefile
# exists either. This script is the fix for that gap, not a fix to any
# broken code.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."

echo "[1/3] building brawlpit (client)..."
# S417-03/04: level_registry.h's own real network+compression path needs lz4_gen.c/lz4_wrapper.c/
# parena_runtime.c (packages/common/lz4/) compiled in as real, separate translation units --
# these are real .c files, not headers, so they can't just be #include'd the way every other
# packages/common/*.h dependency already is above.
# S421-02: ai_opponent.h (pulled in via packages/simulation/local_game.h) calls the real
# PARENA-compiled commander_posture() to build its own observation vector -- commander_mod.c is
# the same real translation unit scripts/build_training.sh already links for the training .so.
gcc -o brawlpit apps/lobby/src/main.c \
    packages/common/lz4/lz4_gen.c packages/common/lz4/lz4_wrapper.c packages/common/lz4/parena_runtime.c \
    packages/common/commander/commander_mod.c \
    -Ipackages/common/lz4 \
    -lSDL2 -lGL -lGLU -lm
echo "      ok -> ./brawlpit"

# BPMM-12441/12442: added 2026-09-04 -- this build script never built the dedicated UDP server
# binary at all (only the client), which is the real reason no BRAWLPIT server was ever actually
# running anywhere in this monorepo despite apps/server/src/main.c existing and compiling clean --
# see that file's own server_net_init doc comment for the full matchmaking root-cause writeup.
echo "[2/3] building brawlpit-server (dedicated UDP server)..."
mkdir -p bin
# S421-03: --level <name> (founder real-time: "can we train on the level called THREE from the
# registry?") needs level_registry.h's own real network+LZ4-decompress fetch path -- same real
# lz4 translation units as the client build above, for the same real reason.
gcc -o bin/brawlpit_server apps/server/src/main.c \
    packages/common/lz4/lz4_gen.c packages/common/lz4/lz4_wrapper.c packages/common/lz4/parena_runtime.c \
    -Ipackages/common/lz4 -lm -O2
echo "      ok -> bin/brawlpit_server"

echo "[3/3] running tests..."
gcc -o /tmp/brawlpit_test_physics tests/test_physics.c -lm
/tmp/brawlpit_test_physics
rm -f /tmp/brawlpit_test_physics

# S419: tests/test_net_protocol.c already existed (real wire-layout lock-down for NetHeader/
# UserCmd/NetPlayer) but was never actually wired into this script before -- a real, pre-existing
# gap found while building the packet-level RL training pipeline, fixed here rather than left.
gcc -o /tmp/brawlpit_test_net_protocol tests/test_net_protocol.c -lm
/tmp/brawlpit_test_net_protocol
rm -f /tmp/brawlpit_test_net_protocol

# S419: the PARENA-compiled fractal-commander decision function (stdlib/brawlpit/
# commander_mod.prn -> packages/common/commander/commander_mod.c).
gcc -o /tmp/brawlpit_test_commander tests/test_commander.c packages/common/commander/commander_mod.c \
    -Ipackages/common/lz4 -lm
/tmp/brawlpit_test_commander
rm -f /tmp/brawlpit_test_commander

# S421-02: the MLP policy loader/forward-pass (packages/common/mlp_policy.h). Synthetic cases
# only here (no real checkpoint/Python available in every build environment) -- see
# scripts/test_mlp_policy_parity.py for the real cross-language parity proof against an actual
# trained checkpoint, run manually where stable_baselines3 is installed.
gcc -o /tmp/brawlpit_test_mlp_policy tests/test_mlp_policy.c -lm
/tmp/brawlpit_test_mlp_policy
rm -f /tmp/brawlpit_test_mlp_policy

# S456: ai_opponent_build_observation's own real S430 relational-feature block -- locks down the
# real, found bug where this function silently stopped at 21 of the real 31 dims (see that test's
# own doc comment for the full incident writeup).
gcc -o /tmp/brawlpit_test_ai_opponent tests/test_ai_opponent.c packages/common/commander/commander_mod.c \
    -Ipackages/common -Ipackages/common/lz4 -lm
/tmp/brawlpit_test_ai_opponent
rm -f /tmp/brawlpit_test_ai_opponent

echo "done."
