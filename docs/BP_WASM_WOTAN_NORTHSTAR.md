# BRAWLPIT on WOTAN — native wasm port, NORTHSTAR

Founder real-time, 2026-10-16: "we need to port BRAWLPIT to WASM via PARENA and put on WOTAN,
ALL THE WOTAN AFFORDANCES, IDUNA SSO LOGIN, A TOP TAB ON THE TOP, GFD name is same as BRAWLPIT
name, use PARENA, ensure hats show up in BRAWLPIT WEB, CONTROLLER SUPPORT IF YOU CAN DO IT IN
THE BROWSER." Routed through `emily observe` before work started (Principle 1a). This closes
the real, honestly-named open question left by `WOTAN_HAT_STORE_NORTHSTAR.md` Phase 3
("BRAWLPIT-side rendering") — the real blocker there was that no BRAWLPIT web client existed at
all to render into.

## The real precedent this follows

`GFD/` (`WOTAN/GFD/index.html` + `gfd.js` + `term.wasm`) is the only existing "native PARENA ->
wasm32, hosted on WOTAN, IDUNA SSO, hat overlay" build in this monorepo, and it is the template
for this doc, not a fresh design:

- Engine logic lives in PARENA (`PARENA/stdlib/gfd/term.prn`), compiled via the native toolchain
  `PARENA/scripts/build_gfd_term_wasm.sh` uses: `parena build -> opt -> llc -> wasm-ld`
  (`-mtriple=wasm32-unknown-unknown`, no Emscripten — same family DEADWEIGHT's
  `NATIVE_WASM_CLIENT_NORTHSTAR.md` chose after the founder explicitly rejected Emscripten).
- Host glue is a plain `<page>.js` that owns the WebSocket/canvas/DOM/clipboard/a11y transcript —
  everything the wasm module itself cannot touch.
- SSO is the shared `js/iduna-sso.js` session; no page renders its own sign-in form
  (`wotan-nav.js`'s own standing rule).
- The equipped-hat badge is `WOTAN/GFD/hat.js`: `GET /characters/by-player/:id` ->
  `GET /characters/:id/hats` -> find `equipped:true` -> set an `<img>` from `hat.image_url`.
  This is the exact, already-proven answer to Phase 3's "BRAWLPIT-side rendering" question for a
  2D web view — no new backend work needed, the IDUNA hats API Phase 1 shipped already covers it.

## Real, checked capability audit

- **BRAWLPIT's actual engine is not PARENA and is not close to portable yet.** `BRAWLPIT/core/`
  is 237 total lines across `character_runtime.c`/`stage_runtime.c`/`selection_screen.c` +
  headers — real but thin: no full match loop, no netcode, no wasm-safe platform layer. This is
  nowhere near GFD's `term.prn` (641 lines, pure PARENA, already wasm-compiled and shipped).
  Per `CLAUDE.md`'s "Core Deps Are PARENA-First" standing rule, the actual core engine logic
  (hitstun/knockback/shield/parry/smash-charge, per `TIPJAR-ACCEPTANCE`'s own description of what
  BRAWLPIT's engine already assumes exists) needs a real PARENA port before a wasm build of real
  gameplay is possible — this is the same shape of blocking Phase 0 `BP_LEVEL_EDITOR_NORTHSTAR.md`
  already found for level data ("zero data-driven format exists").
- **No wasm toolchain is present in this sandbox** (checked directly: no `clang`/`llc`/`wasm-ld`
  on `$PATH`, and `$HOME/.local/opt/llvm-toolchain` referenced by `build_gfd_term_wasm.sh` does
  not exist here). GFD's `term.wasm` was built and committed from a box that had it; this sandbox
  cannot reproduce that build today. Named honestly rather than claiming a build that didn't run.
- **WOTAN-side affordances (SSO, nav, hat overlay) are NOT blocked by the above** — they're pure
  JS/HTML following GFD's exact pattern, reusing already-shipped IDUNA endpoints
  (`/api/v1/characters/by-player/:id`, `/api/v1/characters/:id/hats`) and already-shipped
  `js/iduna-sso.js`/`js/wotan-nav.js`. These are real, buildable now, independent of the engine
  port.
- **Controller support is genuinely easy in-browser**: the Gamepad API (`navigator.getGamepads()`,
  polled per animation frame) needs no server/wasm changes — it is host-glue JS exactly like
  `gfd.js`'s existing keyboard handling, mapping gamepad buttons/axes to the same input events the
  wasm module already receives from keyboard/touch once that module exists. It has no engine
  dependency and can be wired into the host glue stub now, tested against the real browser API,
  ahead of the engine port landing.

## Phased plan

- **Phase 0 (blocking, not started)** — Port BRAWLPIT's real core engine (character state
  machine, stage geometry, hit/hurt resolution) from `core/*.c` into `PARENA/stdlib/brawlpit/`,
  with tests, same standard GFD's `term.prn` met. This is the real, substantial piece; everything
  below can scaffold ahead of it but can't play a real match until it lands.
- **Phase 1 (this pass, done)** — WOTAN page scaffold: `WOTAN/BRAWLPIT/index.html` + `brawlpit.js`
  mirroring `GFD/`'s structure (canvas stage, SSO via shared session, nav tab, equipped-hat badge
  via a direct adaptation of `GFD/hat.js`). Wasm module not yet wired in (Phase 0 blocks it) — the
  page honestly shows a "Core engine porting to PARENA — not playable yet" placeholder state in
  the stage, matching `index.html`'s own established "visibly disabled, not faked as live"
  convention, instead of pretending to play.
- **Phase 2 (this pass, done)** — Nav integration: `BRAWLPIT` added to `wotan-nav.js`'s `GAMES`
  top bar (the "top tab on the top") and its own `TABS.bp` section list, `data-skin="bp"`. Named
  as `Brawlpit` to match the GFD convention of using the repo's own name as the label (not an
  invented marketing name).
  `WOTAN_HAT_STORE_NORTHSTAR.md`'s own already-shipped `/store.html` already links back here once
  Phase 0/3 below exist.
- **Phase 3 (this pass, done — closes the old open question)** — Hat rendering in BRAWLPIT web:
  `WOTAN/BRAWLPIT/hat.js`, a direct structural port of `GFD/hat.js` (same two-call chain, same
  `equipped:true` filter, same `image_url` badge). This resolves
  `WOTAN_HAT_STORE_NORTHSTAR.md` Phase 3's "BRAWLPIT-side rendering, an honestly-named unresolved
  technical question" for the 2D web case specifically — it was unresolved because no BRAWLPIT
  web page existed to answer it in, not because the mechanism was unclear once GFD proved it.
- **Phase 4 (scaffolded, inert until Phase 0)** — Gamepad/controller host glue: polling loop and
  button/axis-to-input mapping stubbed in `brawlpit.js`, feeding the same input path keyboard
  uses once the wasm module defines one. Verified only that `navigator.getGamepads` support
  detection runs without throwing; no real gamepad was available to test button mapping end to
  end in this sandbox.

## What this pass does NOT claim

No wasm module exists yet, no real match can be played in a browser yet, and the PARENA core
port (Phase 0) has not started. Everything shipped this pass is page/nav/SSO/hat/controller-glue
scaffold, verified by direct code reading and Node syntax-check only (no headless browser
available in this sandbox) — not a live-verified playable build. Follow-up: Phase 0 PARENA engine
port, then wire `brawlpit.js` to the resulting `.wasm`, then live-verify same as GFD's own
headless-Chromium pass.
