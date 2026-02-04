# TIPJAR Roadmap (Step-by-Step)

This roadmap translates the product goals and acceptance criteria into an ordered
set of build milestones. Each step is scoped to be shippable and leaves the
simulation architecture in a multiplayer-ready state.

## Step 1 — Core single-player loop (SP-SHIFT)
- Implement the bar loop: customers spawn, place orders, patience timers tick,
  and the player can fulfill + deliver orders.
- Add a visible order queue with timers and a simple vibe/chaos meter.
- Add a minimal bouncer loop: impatience escalates to arguments/fights and the
  player can de-escalate using a non-lethal control action.
- End-of-shift outcomes: win/loss state and a results screen with tips, orders
  completed/missed, brawls handled, damage caused.

**Exit criteria**
- A new player understands “serve + keep order” within 30 seconds.
- A full 10-minute shift can be completed without crashes.

## Step 2 — Player-indexed systems (SP-PARTY framework)
- Refactor gameplay systems to accept `player_id` for scoring, order credit,
  brawler handling, and ownership (items/bubbles/throws).
- Introduce an objective/scoring system that is mode-agnostic (usable for
  single-player and multiplayer without redesign).
- Add deterministic tick boundaries and entity snapshot metadata suitable for
  future networking.

**Exit criteria**
- 2–4 human players can be spawned locally and each can score independently.
- No gameplay system assumes “player 0 is the only real human.”

## Step 3 — Split-screen foundations
- Add 1P/2P/3P/4P split layouts (fixed choices), with consistent per-panel HUD.
- Ensure each panel shows: score, vibe/chaos, order queue + timers, warnings.
- Maintain readability at 1280×720 in 4-up layout (icons + numbers prioritized).

**Exit criteria**
- 4 players can act simultaneously with no input conflicts.
- Every panel’s order queue is readable in 4-up view.

## Step 4 — Competitive Party scaffolding
- Implement a “party ruleset” layer for competitive settings (score timer,
  quotas, deterministic tiebreakers).
- Add at least two sabotage actions (e.g., spawn heckler, ingredient denial),
  with clear feedback and counterplay windows.
- Add a comeback mechanic (e.g., last-call bonus for trailing players).

**Exit criteria**
- A winner is deterministically calculated and displayed.
- Sabotage can be triggered and countered during a match.

## Step 5 — Co-op gameplay layer
- Implement 2P co-op mechanics that are strictly stronger together (assist/
  revive, item handoff, or team bubble combo).
- Implement 2v2 team scoring with teammate readability (outline/color tag) and
  consistent friendly collision rules.

**Exit criteria**
- A player can help a struggling teammate within 2 seconds.
- Team score updates correctly in 2v2.

## Step 6 — Content minimums
- Add 3+ customer archetypes, 2+ trouble archetypes, 3+ drink types.
- Provide at least one full “shift” scenario and one party ruleset.

**Exit criteria**
- A full shift scenario is playable end-to-end with varied content.

## Step 7 — Polish & stability
- Reduce input-to-action latency to arcade feel.
- Validate all acceptance tests in the checklist.

**Exit criteria**
- v0 DoD (sections 0–2, 5, 6, 7, 9, 10) passes.
- v1 DoD (sections 3–4 + co-op assist + stable results screen) passes.
