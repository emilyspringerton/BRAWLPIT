# BRAWLPIT on WOTAN -- native wasm port, NORTHSTAR

Founder real-time, 2026-10-16: "we need to port BRAWLPIT to WASM via PARENA and put on WOTAN,
ALL THE WOTAN AFFORDANCES, IDUNA SSO LOGIN, A TOP TAB ON THE TOP, GFD name is same as BRAWLPIT
name, use PARENA, ensure hats show up in BRAWLPIT WEB, CONTROLLER SUPPORT IF YOU CAN DO IT IN
THE BROWSER." Then, same day: "get brawlpit web shipped; PARENA powered; no malloc; wasmdaddy" and
"make sure its actually wasm not faked with a bunch of garbage js; whatever we do need to use js for
use parena into TS".

**Status: shipped** at wotan.okemily.com/BRAWLPIT/. This replaces the earlier version of this doc, which
was a scaffold plan written when no wasm toolchain was present in the sandbox and the engine port had
not been started; both statements are now false.

## What exists

- **The game is one wasm32 module** (`WOTAN/BRAWLPIT/brawlpit.wasm`, ~128 KB), compiled from PARENA
  (`PARENA/stdlib/brawlpit/bp_*.prn`) by `parena build -> opt -> llc -> wasm-ld`, no Emscripten, the same
  toolchain GFD's terminal uses. It contains the simulation, the CPU fighters, the menus and a software
  renderer (960x540 RGBA). **No malloc**: every record is a fixed-address array in linear memory, the module
  imports nothing, its 8 MB memory cannot grow, and the build fails if an allocator symbol, an undefined
  symbol or a `memory.grow` shows up in the object code.
- **The physics is the real engine, ported line for line.** The earlier audit looked at `core/*.c`
  (237 lines of `CharacterDef`/`StageDef` vtables) -- that turned out to be dead code the shipped client never
  compiles. The real engine is `packages/common/physics.h` + `packages/simulation/local_game.h`; that is what
  was ported, on PARENA's new F32 wasm backend so single-precision float behaviour matches C exactly.
  `PARENA/tests/brawlpit_parity_*` run both engines through 74 scripted scenarios (all 10 characters, 3 stages,
  1v1 / 3-way / 4-player / two humans / sandbox) and require **bit-identical state after every one of 174,600
  ticks**, with a coverage gate (KOs, high-percent launches, shield breaks, parries, every special, both Duck
  projectiles). The CPU levels EASY/NORMAL/HARD (`bp_ai.prn`) are new web code behind a switch; CLASSIC is the
  C bot's own `bot_think` and is inside the parity-tested path.
- **The only JavaScript is generated from PARENA.** `bp-host.js` is `PARENA/stdlib/brawlpit/bp_host.prn`
  compiled to TypeScript by PARENA's `emit_ts` backend (grown for this: statements, locals, loops, module
  state, a typed JS bridge) and type-stripped. It does what only a browser can: instantiate the wasm, blit the
  framebuffer (an `ImageData` view onto wasm memory, no copy), forward keyboard / pointer / gamepad state, and
  decode images (the equipped hat PNG, the portrait atlas) into wasm memory. No game logic.
- **WOTAN affordances**: IDUNA SSO via the shared `js/iduna-sso.js` session (the nav owns sign-in), the `bp`
  nav skin and the top `GAMES` bar entry, the equipped-hat badge in the page header, and **the equipped hat
  worn in matches** -- **by default, the moment it loads, with nothing to pick** (up/down on the fighter-select
  screen only swaps it for no hat or a colour hat, and a deliberate choice is respected). `image_url` hats are
  decoded by the browser; curated hats have no PNG and IDUNA stores their art as an HTML-entity string
  (`&#127914;`), which the host decodes, draws as a glyph and fits onto the head. A status line under the canvas
  says what it found (signed out / nothing equipped / expired sign-in / missing image). Closes
  `WOTAN_HAT_STORE_NORTHSTAR.md` Phase 3's "BRAWLPIT-side rendering" question. (It first shipped opt-in -- NO HAT
  by default -- which was wrong; corrected the same day on the founder's "the hat store hat thats equipped should
  be worn by the player in brawlpit".)
- **Controllers**: standard-mapping Gamepad API, polled per frame by the host; stick deadzone / snap, d-pad,
  A jump, X/RT attack, LB/LT shield, B/RB special, Start pause -- the native client's own scheme -- plus menu
  navigation. One pad plays player 2 in a local match, two pads play one each.
- **Modes**: vs CPU, local 2-player (WASD / arrows), free-for-all (you + 3 CPUs, last one standing), practice
  (nothing hurts). Stages: Final Destination, Timeline, TIPJAR Bar.

## Tests (PARENA repo, `make test-brawlpit`)

emitter tests (wasm 112, TypeScript 55), parity (75), engine (69), generated host under a DOM stub (38);
`WOTAN/scripts/brawlpit-browser-test.mjs` drives the real page in headless Chromium (13).

## What is not done, honestly

- Only Node and headless Chromium have run it. Not Firefox, not Safari, not a phone; **no touch controls**.
- The gamepad was driven through a shim of the Gamepad API (and the wasm's pad path directly); no physical
  controller was attached.
- The curated-hat glyph path (decode entity -> draw -> fit onto the head) was verified with a text symbol in headless
  Chromium; the build machine has no emoji font, so a real colour emoji (and Apple/Segoe emoji metrics) has not been
  looked at. Pixel hats (PNG) are verified end to end in the page, in the fight and on the select screen.
- No audio. No online play (the web client is local; the UDP server / matchmaking / lobbies are unchanged
  and not wired to it). No custom-level browser or RL-checkpoint opponent in the web client.
- The portraits are the native client's own generated images (some with scene backgrounds, not matted), packed
  into one 128x128-per-tile atlas; Petalia and Vexar have none (flat-colour bodies, as in the native client).
- Rebuilding: `WOTAN/scripts/build-brawlpit.sh` regenerates `brawlpit.wasm` and `bp-host.js` from PARENA; both are
  committed (the deploy image is built from git-tracked files), so a `.prn` change is not live until that is run.
