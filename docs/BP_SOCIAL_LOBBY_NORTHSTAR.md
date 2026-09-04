# BPMM-x-123: Social Lobby — Scoping (Principle 19)

**Kanban card `BPMM-x-123`, priority queue**: "new mode social lobby either you join a lobby or
it starts a new one if it cant find one for you to join even if its just you the lobby opens
when another player joins they appear in lobby."

This is a real, meaningfully-sized new feature with genuine open design questions (`EMILY/docs/
THE_EMILY_WAY.md` Principle 19) — investigated before any code was written, real options below,
real sub-tasks returned to `EMILY/BACKLOG.md` rather than guessed at and built blind. Distinct
from `BPMM-12441/12442`/`BPMM-1202020` (matchmaking, shipped this session, commits `cb82a5c`/
`5993b9c`) — those are one-shot queues that end in a real fight starting; this card describes a
persistent, joinable **space**, not a queue.

## Real, current state (investigated directly, not assumed)

- `STATE_LOBBY` (`apps/lobby/src/main.c:38`) has always been a flat, **local, offline** 2D text
  menu — no networking, no other players visible, just keybind hints (confirmed via the existing
  mirror-match-hat card's own doc comment at the same call site: "STATE_LOBBY has always been a
  flat 2D text menu... no walkable 3D avatar/scene exists here"). There is zero existing
  infrastructure for players to see or be near each other outside an active match.
- The server (`apps/server/src/main.c`) has exactly two real multiplayer shapes today, both of
  which end in an active fight the instant they resolve: `PACKET_CONNECT` (immediate, direct
  join into whatever match is already running) and the two matchmaking queues just shipped this
  session (`mm_queue`/`mm_queue_1v1`) — both are one-shot: queue, wait, `mm_start_match*()`
  fires, the queue empties back to 0. Neither queue is a space players linger in by choice; it's
  a waiting room for a specific timer/headcount, not a real persistent lobby.
- `local_state.clients[MAX_CLIENTS]`/`client_active[MAX_CLIENTS]` (protocol.h) already model "up
  to 8 connected slots" and `server_broadcast()`/`PacketSnapshot` already sync position/state
  between whoever is in those slots today — the real transport and snapshot-sync mechanism this
  card would need already exists and works (proven live this session for both matchmaking
  modes). What's missing is a *pre-match* state that uses that same sync without kicking off
  `mm_start_match`.
- This is a genuinely single-process, single-`ServerState` server (`BP_LOBBY_MATCHMAKING_
  NORTHSTAR.md`'s own Phase 0 section: "two real processes fight over" the one shared world) —
  there is no existing multi-room/multi-instance concept anywhere in this codebase. "A lobby"
  and "a match" would, today, have to share the same single `local_state` unless a real
  multi-instance model is built first.

## Real open questions (why this needs a founder decision, not a guess)

1. **Does the social lobby ever become a match, or is it a pure hangout space?** The card's own
   wording ("the lobby opens when another player joins they appear in lobby") only describes
   players becoming visible to each other — it never says a fight starts. If a real duel/FFA is
   still meant to follow, how does that trigger (a ready-up? walking into something? automatic
   once N players are in)? If it's meant to stay a pure social space, does it ever end?
2. **One global lobby, or many discoverable lobbies?** "Either you join a lobby or it starts a
   new one if it can't find one for you to join" implies real lobby *discovery* — multiple
   concurrent lobbies can exist, and a joining player finds an open one or creates their own.
   That's a real, new multi-instance concept this single-`ServerState`-process architecture does
   not have today (see above) — every match/queue right now is process-wide, singular.
   Alternative reading: there's only ever one social lobby at a time (matching this server's
   existing one-queue-at-a-time model) and "starts a new one" just means "the first person to
   show up creates it" in the trivial, single-instance sense. These are very different builds.
3. **Does the existing offline `STATE_LOBBY` menu get replaced/extended into this, or is this a
   new, separate networked state (`STATE_SOCIAL_LOBBY`) reached some other way (a keypress, like
   `M`/`N` already are for the two matchmaking modes)?** The menu's own real screen space is
   already getting crowded (`D`/`F`/`J`/`T`/`M`/`N` are all real, live keybinds now) —
   `BPUX-12444` (next priority-queue card, "the brawlpit UX... cant really read the words... can
   we make the font nicer and have clear buttons") is directly relevant here and arguably should
   land *before* adding a 7th menu option to a screen already flagged as hard to read.
4. **What do players actually see/do while "in lobby together"?** Just names/portraits in a
   list (cheapest, reuses zero new rendering)? Or real, walking-around 3D avatars sharing the
   existing stage geometry (a real new client-render mode, more work, but matches "they appear
   in lobby" literally)? `BP_LOBBY_MATCHMAKING_NORTHSTAR.md`'s own Phase 4 already named "no
   chat" as real and deliberate for the matchmaking flow — does that same constraint apply here?

## Real options (not built yet — pick one before code starts)

**Option A — Minimal: a persistent pre-match waiting room, single global instance.**
Reuse the existing snapshot-sync transport exactly as-is. Add a third server-side "queue" that
behaves like `mm_queue`/`mm_queue_1v1` but with no timeout and no bot-fill — players just sit in
it, visible to each other via `PacketSnapshot` (real movement, so this doubles as the "they
appear in lobby" ask literally, on the exact same stage geometry a match already uses), until a
ready-up or headcount trigger starts a real match using the FFA or 1v1 start-match path already
built. Smallest real diff from what exists today; answers open question 1 by making it a
pre-match room, not a permanent hangout. Still needs question 4 (portraits list vs. walking
avatars) answered — walking avatars is the cheaper build given `PacketSnapshot` already does
exactly that.

**Option B — Real multi-instance lobbies (matches the card's literal "find one to join" text
most closely).** A real lobby-list/discovery layer: the server tracks N independent lobby
instances (each its own mini `ServerState`-shaped player set), a join request either seats the
sender in an existing open lobby or spins up a new one. This is the one option that needs a real
architecture change (today's server is one shared `ServerState`, not N of them) — the same real,
larger lift `BP_LOBBY_MATCHMAKING_NORTHSTAR.md` already flagged for "two real processes fight
over" the one shared world, now generalized to N. Real, honest: this is the most faithful read
of the card's own words, and the most expensive to build correctly.

**Option C — Punt the "lobby" framing, extend the existing matchmaking queues instead.** Given
Option A already answers "you appear together before a match starts," the cheapest real
interpretation is: don't build a new mode at all, just remove the bot-fill timeout from the
*existing* FFA/1v1 queues so players can idle in them together indefinitely and see each other
via the queue's own existing `PACKET_QUEUE_STATUS`/eventual `PACKET_MATCH_FOUND` -- but this
doesn't give visible, walking-around presence (`PacketSnapshot` isn't sent to queued-but-not-
yet-matched clients today), so it under-delivers on "they appear in lobby" unless that snapshot
wiring is added anyway, which converges back to most of Option A's real work.

## Recommendation, not yet actioned

Option A is the real, honest "smallest real thing that satisfies the card's own words" — it
reuses 100% of the transport/snapshot code already shipped and tested this session, and only
needs one open question answered before code starts: **what starts the actual match** (a ready-
up keypress per player, or an automatic timer once ≥2 are present, mirroring the matchmaking
timeout convention already established). Option B is real and more faithful to "find a lobby to
join" but is a genuinely larger architecture change (multi-instance) that deserves its own
scoping pass if chosen. Sequencing: `BPUX-12444` (menu readability) touches the exact same lobby
screen this card would add a 7th option to — real, likely worth landing first so the new option
doesn't get added to a screen already flagged as hard to read.

## Why this isn't done in one pass

Real, open design questions above (does a lobby ever start a match and how; one global instance
or real multi-instance discovery; portraits vs. walking avatars) change the actual code written
enough that building any one interpretation blind risks shipping the wrong shape entirely — the
same real risk `BP_LOBBY_MATCHMAKING_NORTHSTAR.md` named for its own Phase 0. Real sub-tasks
(implement Option A once question 1's trigger is decided; or scope Option B's multi-instance
model separately if that's the founder's real intent) are logged in `EMILY/BACKLOG.md` under
this card's own section rather than folded into a single, unscoped "build the social lobby"
checkbox.
