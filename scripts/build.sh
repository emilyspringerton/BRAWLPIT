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

echo "[1/2] building brawlpit..."
gcc -o brawlpit apps/lobby/src/main.c -lSDL2 -lGL -lGLU -lm
echo "      ok -> ./brawlpit"

echo "[2/2] running physics smoke test..."
gcc -o /tmp/brawlpit_test_physics tests/test_physics.c -lm
/tmp/brawlpit_test_physics
rm -f /tmp/brawlpit_test_physics

echo "done."
