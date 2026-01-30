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
