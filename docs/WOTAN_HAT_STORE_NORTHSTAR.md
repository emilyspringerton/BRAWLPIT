# NORTHSTAR — WOTAN Hat Store (BRAWLPIT cosmetics, paid in GFD Flow)

Real, unified scoping pass for 3 related priority-queue cards, treated as one feature, not three
separate asks: `WOTAN-999` ("we need to build the hat store WOTAN you can buy upgraded hats for
brawlpit using flow from GFD plan nortgstar it"), `WOTAN-998` ("IMPLEMENT vs0 GO... plan
nortgstar it"), `WOTAN-996` ("ITERATE... make it so users can draw their own hats in a pixel
editor"). Originally planning-only, per `WOTAN-999`'s own explicit "plan northstar it" framing —
**updated 2026-09-03**: Phases 0 and 1 are now real and shipped, see below.

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

## Real, phased plan

**Phase 0 — DONE, real, already existed (corrected 2026-09-03).** This doc originally named "a
real Flow balance-query + spend API" as a blocking external prerequisite, matching kanban cards
`3213432`/`345234`'s own framing. Checked directly while building Phase 1 below: it already
exists, and did not need new work. `apps2/mud/main.go`'s own `runHeadlessCommand` already syncs
every real Flow delta to IDUNA's `characters.gold_balance` column via `idunaclient.CreditGold`/
`DeductGold` on each headless-command tick. IDUNA's own `GET /api/v1/characters/by-player/:id`
already returns that `gold_balance` (its own doc comment already called this "a real, WOTAN
player_id" resolution route), and `PATCH /api/v1/characters/:id/gold` already spends it
atomically (409 on insufficient funds) — the exact real balance-query + spend contract this
doc asked for. Real, honest caveat: the sync only happens on a headless-command tick, so a
read could be stale if a player hasn't issued a MUD command recently — a real, secondary
refinement, not a blocker for Phase 1/2. Cards `3213432`/`345234` may still have real, separate
asks (cross-game currency swaps with GTA7, broader mod-interface access) — this correction is
scoped to the hat store's own narrow need, not a claim those cards are fully resolved.

**Phase 1 — DONE, real hat catalog + inventory data model (shipped 2026-09-03).** Real home:
IDUNA's own existing MMO schema (`IDUNA/migrations/truestore`), matching `characters`/`items`/
`character_equipment`'s own established convention directly — not `IDUNA_PRO` as an earlier
draft of this doc guessed (`IDUNA_PRO` is a separate, newer product extraction unrelated to
GFD/BRAWLPIT's own MMO backend). New `hats` (hat_id, name, description, flow_cost, image_asset)
+ `character_hats` (character_id, hat_id, acquired_at, equipped) tables
(`202609030001_hats.sql`), seeded with a real, hand-curated 6-hat catalog drawn directly from
`OKEMILY/hats.html`'s own already-designed mockup (not invented fresh) — Top Hat/Uncrowned's
Doubt/Joystick Cap/Second Growth Wreath/Scavenger's Vest Cap/Most-Summoned Circlet, each
lore-grounded in an already-tuned BRAWLPIT character. New handlers
(`IDUNA/internal/http/handlers/hats.go`): `GET /api/v1/hats` (catalog), `GET /api/v1/characters/
:id/hats` (owned), `POST /api/v1/characters/:id/hats/buy` (atomic Flow-deduct + ownership grant
in one real DB transaction, reusing `handleDeductGold`'s own conditional-UPDATE pattern), `PATCH
/api/v1/characters/:id/hats/equip` (exclusive single-hat equip). Real bug found and fixed
live, test-driven: the buy handler's own "character not found vs. insufficient funds"
disambiguation query ran on `h.DB` instead of the open `tx`, which for a `:memory:` SQLite test
DB lands on a different, empty connection — always misreporting a real character as "not
found." 8 real tests (catalog list+cost-ordering, successful buy, insufficient-Flow rejection,
duplicate-purchase rejection with a real rollback-doesn't-double-spend assertion, unknown-hat
404, owned-hats listing, exclusive equip-swap, equip-not-owned rejection) plus a real migration
test confirming the MySQL-flavored DDL survives the SQLite translation path intact (apostrophes,
`TINYINT(1)`, composite primary keys) and re-applies idempotently. `go build/vet/test ./...`
clean.

**Phase 2 — the real WOTAN store page.** Not built this pass. A real, simple web page (browse
catalog, see Flow cost, a "Buy" button) calling Phase 1's own now-real endpoints, gated on IDUNA
login (WOTAN already has a real identity story to build on, not invented fresh here). The real
placeholder `WOTAN/index.html` (see `WOTAN-REPO-001`) is where this page belongs once built.

**Phase 2.5 — DONE, real GFD Town proxy (kanban `WTHS-012010`, shipped 2026-09-03).** A real,
separate real purchase surface, in parallel with the eventual WOTAN web page above, not a
replacement for it: a new `hatshop`/`hatshop buy <hat-id>`/`hatshop mine` MUD command in
`GoblinFoxDragon/apps2/mud/main.go` calls Phase 1's own real endpoints directly (new
`server/idunaclient.ListHats`/`BuyHat`/`ListCharacterHats`), letting a player buy a real
BRAWLPIT hat with real Flow from inside GFD's own Town, no separate web login needed (the MUD
session's own already-resolved IDUNA character identity carries through). Real, live,
end-to-end verified: redeployed the live IDUNA instance (it predated Phase 1's own commit),
then bought a real hat as the real `DRAGONSNSHIT-MUD` agent, confirmed ownership, confirmed a
duplicate purchase correctly fails. GFD commit `dafccba`.

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

Phases 0-1 shipped (see above); Phases 2-4 not built. No real WOTAN store web page exists yet
(Phase 2). BRAWLPIT's own real cosmetic-layer rendering mechanism (Phase 3) is still a real,
unresolved technical question, not designed. Cards `3213432`/`345234`'s own broader asks
(cross-game currency swaps, general mod-interface access beyond this specific hat-store need)
are not resolved by Phase 0's correction above — only the narrow Flow-balance-query-and-spend
need this feature has.

## Related

- `GoblinFoxDragon/apps2/mud/main.go` — the real, live `Player.flow` field this whole feature
  spends, currently with no external API.
- `GoblinFoxDragon/docs2/INVENTORY_EQUIPMENT_NORTHSTAR.md` — GFD's own real, existing
  inventory/equipment design, the real precedent this doc's own Phase 1 data model follows.
- Kanban cards `3213432`/`345234` — the real, already-tracked Flow-API-access gap this feature's
  own Phase 0 depends on, not re-planned here.
- `IDUNA/docs/EMILY_FOR_BUSINESS_NORTHSTAR.md` — the real, direct precedent for the same class of
  "public user-generated content needs a moderation story" question Phase 4 names.
