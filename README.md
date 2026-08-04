# BRAWLPIT

**Codename:** BRAWLPIT
**Base:** SHANKPIT (Build 178)
**Genre:** 2.5D Platform Fighter (Cursed Vibe Coding)

"Where momentum meets mayhem in 2.5D combat."

## Current Status (2026-08-04)

TIPJAR (a real bartender/bouncer game mode, see `docs/TIPJAR_ROADMAP.md`) now lives inside this
engine — press **T** from the lobby. Step 1 (core single-player shift loop) and Step 2
(player-indexed simulation, real entity ownership) are both shipped and live-verified; Steps 3-7
(split-screen, competitive/co-op, content, polish) are next. Also fixed: character select getting
permanently stuck on a rematch (a stale cursor variable silently edited the wrong slot). See
`CHANGELOG.md`.

## Environment
- **Render:** OpenGL Immediate Mode (Legacy)
- **Physics:** Custom 2.5D Momentum Engine
- **Net:** UDP / Lag Compensation

## Setup
Dependencies: `libsdl2-dev`
Build: `gcc -o brawlpit apps/lobby/src/main.c -lSDL2 -lGL -lGLU -lm`

## Controls
- **Move:** A/D
- **Aim/Direction:** W/S (hold **S** on passthrough platforms to drop through)
- **Jump:** Space
- **Attack:** J
- **Shield:** Left Shift (shows bubble)
- **Dodge/Wavedash:** K (directional; briefly disables friction for slide)
- **Parasol Up-B:** K + W (ground or air)
- **Turnip Toss:** K on ground + hold S

## Construct Build Artifact
To generate the construct artifact used for build snapshots:
```bash
python scripts/build_construct.py
```
This writes `BRAWLPIT_CONSTRUCT` in the repo root, containing every tracked file
with a `--- FILE START`/`--- FILE END` block for archival and diffing.
