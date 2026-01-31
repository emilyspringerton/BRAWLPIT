# BRAWLPIT

**Codename:** BRAWLPIT
**Base:** SHANKPIT (Build 178)
**Genre:** 2.5D Platform Fighter (Cursed Vibe Coding)

"Where momentum meets mayhem in 2.5D combat."

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

## Construct Build Artifact
To generate the construct artifact used for build snapshots:
```bash
python scripts/build_construct.py
```
This writes `BRAWLPIT_CONSTRUCT` in the repo root, containing every tracked file
with a `--- FILE START`/`--- FILE END` block for archival and diffing.
