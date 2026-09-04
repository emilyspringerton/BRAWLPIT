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
gcc -o brawlpit apps/lobby/src/main.c -lSDL2 -lGL -lGLU -lm
echo "      ok -> ./brawlpit"

# BPMM-12441/12442: added 2026-09-04 -- this build script never built the dedicated UDP server
# binary at all (only the client), which is the real reason no BRAWLPIT server was ever actually
# running anywhere in this monorepo despite apps/server/src/main.c existing and compiling clean --
# see that file's own server_net_init doc comment for the full matchmaking root-cause writeup.
echo "[2/3] building brawlpit-server (dedicated UDP server)..."
mkdir -p bin
gcc -o bin/brawlpit_server apps/server/src/main.c -lm -O2
echo "      ok -> bin/brawlpit_server"

echo "[3/3] running physics smoke test..."
gcc -o /tmp/brawlpit_test_physics tests/test_physics.c -lm
/tmp/brawlpit_test_physics
rm -f /tmp/brawlpit_test_physics

echo "done."
