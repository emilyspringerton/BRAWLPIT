# BPLE-12441 / BPLE-13331: Online Level Editor — Scoping (Principle 19)

**Kanban priority-queue cluster**, 9 real cards, investigated together as one epic (queue
position order, `BPMM-x-123` aside — that one's parked separately, see `BP_SOCIAL_LOBBY_
NORTHSTAR.md`):

- `#285 BPLE-12441`: "online BrawlPit level editor"
- `#286 BPLE-12441`: "A user can log in with an account"
- `#287 BPLE-12441`: "a user can design a brawl pit level (simple drag and drop affordances)"
- `#288 BPLE-12441`: "a user can choose the size of the level"
- `#289 BPLE-12441`: "a user can place platforms"
- `#290 BPLE-12441`: "a user can browse the user contributed maps after character selection to
  choose the map (default at top)"
- `#292/#293 BPLE-13331` (duplicate text, same real kanban-duplicate-ID quirk already documented
  for `BPSW-1216` this session): "a user can select the map on the online registry and hit play
  and it launches brawlpit client with that level (ROBLOX STYLE AFFORDANCE FOR WORLD SELECTION
  WEB->NATIVE)"
- `#294 BPLE-13331`: "we can use the web interface actually in the game for the level browser
  like roblox seems to use an embedded browser to allow for level selection in game or via web"

This is a real, large, multi-subsystem ask (`EMILY/docs/THE_EMILY_WAY.md` Principle 19) —
investigated directly before any code was written, real phased plan below, real sub-tasks
returned to `EMILY/BACKLOG.md` rather than guessed at and built blind.

## Real, current state (investigated directly, not assumed)

- **Levels are compiled C code, not data.** `levels/*.c` (`battlefield.c`, `final_destination.c`,
  `weird_void.c`) each define a `StageDef` (`core/stage.h`) — a `const float *points` array and
  C function pointers, compiled straight into the binary. Separately, and more importantly, the
  REAL gameplay collision geometry `apply_knockback`/`check_attack_hitbox`/movement actually use
  is a completely different, also-hardcoded set: `packages/common/physics.h`'s own
  `stage_fd_geo`/`stage_timeline_geo` static `Platform2D` arrays, selected via a hardcoded
  `stage_set_active(stage_id)` switch. **There is no file format, no runtime loader, no
  serialization anywhere in this codebase for level geometry.** A user-designed level — from any
  editor, web or native — cannot be loaded today without a real, new format and loader. This is
  the exact same shape of finding `BP_LOBBY_MATCHMAKING_NORTHSTAR.md` made for netcode: a real,
  blocking Phase 0 underneath the whole ask, not a corner of it.
- **No account system exists.** Checked directly: zero `IDUNA`/`idunaclient` references anywhere
  in this repo. `#286` ("a user can log in with an account") is a real, net-new integration —
  BRAWLPIT has never talked to IDUNA (this monorepo's own real, established IAM) at all. Every
  sibling repo that has accounts (`IDUNA_PRO`, `apps2/server-go`'s own JWT verify) does so via
  IDUNA directly; that's the obvious real integration point, not a new auth system from scratch.
- **No web presence exists.** Checked directly: no `www/`, no HTTP server, no web assets
  anywhere in this repo. `#290`'s "browse... online registry" and `#294`'s "embedded browser...
  web interface actually in the game" both require a real, new web service (map storage,
  browsing UI, an API the native client can query) that doesn't exist in any form.
- **The native client has no embedded-browser capability.** BRAWLPIT's client
  (`apps/lobby/src/main.c`) is plain SDL2 + immediate-mode OpenGL — no webview/CEF/native-browser
  integration of any kind. `#294`'s own "like roblox... embedded browser" is a real, named,
  nontrivial technical ask (Roblox's own in-client browser is a substantial engineering
  investment) — this is not a small feature.
- **Real, existing precedent to build on, found and worth reusing directly:**
  - `WOTAN` (this monorepo's own real esports/stats + planned hat-store web hub) already has a
    real Cloudflare-backed subdomain pattern (`docs/WOTAN_HAT_STORE_NORTHSTAR.md`) — a natural
    real home for a "map registry" web page (`maps.wotan...` or a WOTAN page), not a brand-new
    site.
  - `IDUNA` already issues real ES256 JWTs and already has a real character/player-identity model
    (`apps2/server-go`'s own `authVerifier.Verify`) — `#286`'s login should be "BRAWLPIT gets an
    IDUNA login flow," the same real pattern every other account-bearing service in this
    monorepo already uses, not a new bespoke account system.
  - `GOLDENBAND`'s own `.gband` format (this monorepo's own precedent for "define a real binary
    asset format + a loader + a sampler," HQ-SPEC-SIM-100) is the closest real sibling precedent
    for what a `.brawllvl` (or similar) level format + loader would need to look like.

## Real, phased plan

**Phase 0 — a real, data-driven level format + runtime loader (blocking, not optional).**
Define a real, versioned level file format (JSON is the honest, simplest real choice given
`Platform2D` is already just `{x, y, w, h, type}` arrays — no need for `GOLDENBAND`'s own binary
complexity here) and a real loader that populates `stage_fd_geo`-shaped data at runtime instead
of switching between hardcoded compiled arrays. Definition of done: the existing 3 shipped
levels (`battlefield`, `final_destination`, `weird_void`) are re-expressed as real level files
and load correctly through the new runtime path, with zero regression to existing gameplay
tests. Nothing below this line is buildable without Phase 0 — `#287`/`#288`/`#289` all produce
level DATA that has nowhere real to go without it.

**Phase 1 — real IDUNA login (`#286`).**
A real BRAWLPIT login flow against IDUNA (the established pattern, not a new system) — device
flow or a simple username/password exchange for an ES256 JWT, matching `apps2/server-go`'s own
`idunaauth.NewVerifier()` convention. Real open question for the founder: does this live in the
native SDL2 client itself (a real, new in-client login UI, this client's font/button work from
`BPUX-12444` this session would need to extend to text INPUT, not just display) or only in the
web registry (Phase 3) with the native client trusting a token handed to it at launch (Roblox's
own actual model — the web side authenticates, native launches with a session token)? This
changes Phase 1's own scope significantly and should be decided before building either.

**Phase 2 — native, in-client level editor (`#287`/`#288`/`#289`).**
A real new `STATE_LEVEL_EDITOR` client mode: choose a size (a real, bounded canvas — this game's
existing `BLAST_LEFT/RIGHT/TOP/BOTTOM` blast-zone constants already define a real playable
bounding box to default/clamp against), place/resize/delete platforms via "simple drag and drop"
(the card's own words — real mouse-driven affordances, a first for this client; today's UI is
100% keyboard/gamepad, see `BPUX-12444`'s own finding that `STATE_LOBBY` has zero mouse handling
today), save to Phase 0's real level format. Real, bounded scope per the card's own "simple"
qualifier: platform placement/sizing only, not slopes/passthrough-authoring nuance/background art
— matches this repo's own established "vertical slice, not everything at once" discipline.

**Phase 3 — real web map registry (`#290` half, `#292`/`#293`).**
A real, new web service (recommend hosting under `WOTAN`'s own existing real subdomain/Cloudflare
pattern rather than standing up a brand-new site): upload a Phase 0-format level file (from
Phase 2's editor export), a real browsable list ("user contributed maps," "default at top" per
`#290`'s own wording), and a real "select map → hit play → launches the native client with that
level" handoff (`#292`/`#293`) — the real, concrete mechanism for that handoff (a custom URI
scheme the OS hands to the installed client, matching how Steam/Discord/Roblox's own "protocol
launch" links work; the client fetches the chosen level file by ID on startup) needs a real
technical spike before committing, not assumed.

**Phase 4 — embedded browser in-client (`#294`, `#290`'s other half: "after character
selection").**
The single most open-ended, expensive real ask in this whole cluster: a real, native-embedded
web browser INSIDE the SDL2/OpenGL client (Roblox's own real, substantial engineering investment,
named honestly as such) so map browsing can happen without leaving the game. Real, honest
alternative worth real founder consideration: Phase 3's web registry reached via the user's own
system browser (already real and free, zero new client engineering) covers `#290`'s literal
"browse... to choose the map" ask on its own; `#294`'s specific embedded-in-client ask is a real,
separate, much bigger investment (a CEF/WebView native integration) that should be its own scoped
decision, not bundled into "finish the level editor."

## Why this isn't done in one pass

Phase 0 alone (a real level format + loader, replacing every hardcoded compiled-C stage this
game has ever shipped) is real, structural, blocking work touching how every existing level
loads today — the same real category of risk `BP_LOBBY_MATCHMAKING_NORTHSTAR.md`'s own Phase 0
named for netcode. Phases 1-4 each depend on real, founder-level decisions (native vs. web login;
which map-registry host; whether an embedded browser is worth its real cost vs. the system
browser) that change the actual code substantially. Real sub-tasks are logged in
`EMILY/BACKLOG.md` under this cluster's own section rather than folded into a single, unscoped
"build the level editor" checkbox.
