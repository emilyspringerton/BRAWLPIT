#!/usr/bin/env bash
# scripts/build_training.sh (S419) -- builds the ctypes-callable shared library
# packages/common/commander.h exposes for the Python packet-level RL environment
# (scripts/rl_env_packet.py), mirroring REDGARDEN/ECOWAR's own scripts/build_training.sh
# convention (same "a .so for ctypes to dlopen, kept separate from scripts/build.sh's own game
# binaries" reasoning). Also builds and runs bin/brawlpit_server and bin/probe_client (the real
# training-side native pieces, see docs/RL_TRAINING_NORTHSTAR.md).
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
mkdir -p "${BUILD_DIR}"
cd "${ROOT_DIR}"

echo "[1/2] building libbrawlpit_commander.so (PARENA-compiled commander_posture)..."
# commander_mod.c is real PARENA-compiled output (PARENA/stdlib/brawlpit/commander_mod.prn via
# `parena build`) -- checked in verbatim at packages/common/commander/commander_mod.c, "do not
# edit by hand" per its own generated header. Reuses packages/common/lz4/parena_runtime.h -- the
# same minimal, hand-written, portable Arena/Vec subset S417-03 already built -- rather than a
# second near-duplicate copy, since commander_mod.c needs nothing from it beyond the #include
# itself (it's pure I32 scalar logic, confirmed live: zero arena_/vec_ function calls in the
# generated C).
gcc -shared -fPIC -o "${BUILD_DIR}/libbrawlpit_commander.so" \
    packages/common/commander/commander_mod.c \
    -Ipackages/common/lz4
echo "      ok -> build/libbrawlpit_commander.so"

echo "[2/2] building bin/brawlpit_server (with --fast-forward, --level, --port) for training use..."
mkdir -p bin
# S421-03: --level <name> needs level_registry.h's own real network+LZ4-decompress fetch path --
# same real lz4 translation units scripts/build.sh's own client/server build already needs.
gcc -o bin/brawlpit_server apps/server/src/main.c \
    packages/common/lz4/lz4_gen.c packages/common/lz4/lz4_wrapper.c packages/common/lz4/parena_runtime.c \
    -Ipackages/common/lz4 -lm -O2
echo "      ok -> bin/brawlpit_server (run with --fast-forward for training throughput)"

echo "done."
