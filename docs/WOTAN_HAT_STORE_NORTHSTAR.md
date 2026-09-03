# NORTHSTAR — WOTAN Hat Store (BRAWLPIT cosmetics, paid in GFD Flow)

Real, unified scoping pass for 3 related priority-queue cards, treated as one feature, not three
separate asks: `WOTAN-999` ("we need to build the hat store WOTAN you can buy upgraded hats for
brawlpit using flow from GFD plan nortgstar it"), `WOTAN-998` ("IMPLEMENT vs0 GO... plan
nortgstar it"), `WOTAN-996` ("ITERATE... make it so users can draw their own hats in a pixel
editor"). Planning only, per `WOTAN-999`'s own explicit "plan northstar it" framing — no code
written for this pass.

## What's actually being asked, made concrete

A cosmetic store, hosted on WOTAN (the real, existing esports/stats hub at
`okemily.com/tournaments.html` — confirmed via prior BACKLOG history to be a real web page, not
its own separate codebase), where a player spends **Flow** (GFD's own real, existing in-game
currency) to buy hats their BRAWLPIT fighter can wear. `WOTAN-996` adds a real pixel editor so
players can draw their own custom hats instead of only picking from a pre-made catalog.

## Real, checked-live foundation — what already exists

- **Flow is real and already live**, but ONLY inside GFD's own MUD server: `GoblinFoxDragon/
  apps2/mud/main.go`'s own `Player.flow` field, already used for real buy/sell/travel-cost
  mechanics (`"You buy %s for %d flow"`, `"Need %d Flow to travel"`). Real, honest, decisive
  finding: **there is no external API exposing a player's Flow balance today** — it only exists
  as in-process server state inside the MUD's own player struct.
- **This exact gap is already named, separately, twice, elsewhere in this backlog** — kanban
  cards `3213432` ("build flow API bindings into papercraft... IDUNA game accounts dont have game
  boundaries so we can use flow from GFD") and `345234` ("build more api mod interfaces to allow
  more programatic access to the flow market in GFD"). **Real, load-bearing consequence for this
  doc**: the hat store's own real prerequisite — a way to check and spend a player's Flow balance
  from OUTSIDE the GFD MUD process — is not a new problem this feature invents, it's the SAME
  real, already-tracked gap those two cards name. This doc does not re-plan that work; it names
  the dependency directly so the hat store isn't built twice against two different, disconnected
  assumptions about how Flow access will eventually work.
- **BRAWLPIT is real and live**: a real 2.5D fighter, 10 real fighters, a real character-select
  screen (`apps/lobby`) — the real, concrete place a "wearing a hat" cosmetic would actually
  render.
- **IDUNA is the real, existing identity boundary** every cross-game system in this monorepo
  already uses — a player's WOTAN session, GFD Flow balance, and BRAWLPIT hat inventory all need
  to resolve to the SAME real IDUNA identity, not three separate account systems.

## Real, phased plan (none started)

**Phase 0 (real, external prerequisite, not this doc's own scope)** — a real Flow balance-query
+ spend API, exposed from GFD (matching cards `3213432`/`345234`'s own already-tracked ask). The
hat store's own Phase 2 below is genuinely blocked until this exists — named honestly, not
silently assumed solved.

**Phase 1 — real hat catalog + inventory data model.** A small, new, real table (candidate home:
`IDUNA_PRO`'s own real store, matching this monorepo's own "cross-game state lives behind IDUNA"
convention already established for `drive`/`blog`/etc.) — `hats` (id, name, flow-cost, image
asset), `player-hats` (IDUNA identity, hat id, acquired-at). Real, deliberate v0 scope: a fixed,
hand-curated hat catalog to start, not user-generated content yet (that's Phase 4, the pixel
editor).

**Phase 2 — the real WOTAN store page.** A real, simple web page (browse catalog, see Flow cost,
a "Buy" button) calling Phase 0's own Flow-spend API + Phase 1's own inventory write, gated on
IDUNA login (WOTAN already has a real identity story to build on, not invented fresh here).

**Phase 3 — real BRAWLPIT-side rendering.** The character-select screen queries the logged-in
player's own real hat inventory (Phase 1) and lets them equip one; the equipped hat renders on
their fighter model in-match. Real, honest, not-yet-resolved question: BRAWLPIT's own real asset
pipeline (how a fighter's sprite/model is composed) needs a real "attach a cosmetic layer" point
that doesn't exist yet — a real, concrete follow-up scoping question for whoever picks up this
phase, not answered here.

**Phase 4 (`WOTAN-996`'s own real ask) — a real pixel editor for user-drawn hats.** A real,
simple, canvas-based pixel-art editor on the WOTAN page itself (the natural home — it's already
where the store lives), producing a real image asset a player can submit alongside Phase 1's own
catalog. Real, honest, deliberately-named open questions, not resolved here: **moderation** (a
public pixel editor producing player-visible in-game content needs a real content-review step
before publishing wide — the same real class of question `EMILY_FOR_BUSINESS_NORTHSTAR.md`'s own
self-signup/abuse-policy question already named for a different feature), and **a real cost
model** for a user-drawn hat (a flat Flow price? free, since the player did the work? — a real,
founder-level product decision, not resolved here).

## Real, honest, explicitly out-of-scope for this pass

No code written. No API contract for Phase 0's own Flow endpoint is designed here — that belongs
to cards `3213432`/`345234` directly, this doc only names the real dependency. BRAWLPIT's own
real cosmetic-layer rendering mechanism (Phase 3) is named as a real, unresolved technical
question, not designed.

## Related

- `GoblinFoxDragon/apps2/mud/main.go` — the real, live `Player.flow` field this whole feature
  spends, currently with no external API.
- `GoblinFoxDragon/docs2/INVENTORY_EQUIPMENT_NORTHSTAR.md` — GFD's own real, existing
  inventory/equipment design, the real precedent this doc's own Phase 1 data model follows.
- Kanban cards `3213432`/`345234` — the real, already-tracked Flow-API-access gap this feature's
  own Phase 0 depends on, not re-planned here.
- `IDUNA/docs/EMILY_FOR_BUSINESS_NORTHSTAR.md` — the real, direct precedent for the same class of
  "public user-generated content needs a moderation story" question Phase 4 names.
