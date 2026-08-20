#ifndef TIPJAR_H
#define TIPJAR_H

/* TIPJAR -- Step 1 (Core single-player loop, SP-SHIFT). See BRAWLPIT/docs/TIPJAR_ROADMAP.md and
 * the TIPJAR wiki's Product-Core-Acceptance.md (both golden-indexed) for the full real spec this
 * implements against. Founder direction (2026-08-04, via AskUserQuestion): a mode inside BRAWLPIT,
 * not a separate fork -- reuses this engine's own real platformer physics (update_entity, AABB
 * collision, the Turnip projectile pipeline) rather than building parallel systems, the same
 * "check for existing infra first" discipline this session already applied elsewhere today.
 *
 * Deliberately NOT wired through update_entity's own Special-button handling (turnip-pull/Up-B
 * recovery/wavedash) -- that logic is real, shared, load-bearing for the existing fighting-game
 * mode and TIPJAR's own Special meaning (throw a bubble) genuinely conflicts with it. TIPJAR calls
 * update_entity for movement only, with btn_special zeroed before the call so none of that
 * fighting-game-specific behavior can fire, and reads the real raw special/attack/shield presses
 * itself for TIPJAR's own actions. Bubbles reuse the Turnip struct/array (position, velocity, TTL)
 * for real projectile movement -- matching the wiki's own "reuse your turnip pipeline" plan -- but
 * are ticked and collision-tested here against Customers, not through update_turnips (which
 * applies real fighting-game knockback/damage to Players, not what a bubble hitting a customer
 * should do).
 */

#include "../common/protocol.h"
#include "../common/physics.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

#define MAX_CUSTOMERS 6
#define TIPJAR_QUOTA 300              /* score needed to win the shift */
#define TIPJAR_SHIFT_MS (180u * 1000u) /* 3-minute shift -- real, timed, not endless */
#define TIPJAR_EJECT_DOOR_X -27.0f     /* far left of stage_fd_geo's own 60-wide main floor (-30..30) */
#define TIPJAR_SEAT_MIN_X -20.0f
#define TIPJAR_SEAT_MAX_X 20.0f
#define TIPJAR_FLOOR_Y 0.0f            /* stage_fd_geo's main floor top (y=-5, h=10 -> top at y=0), matches phys_respawn's own player landing height */
#define TIPJAR_PATIENCE_SECONDS 22.0f
#define TIPJAR_ANGRY_THRESHOLD 0.4f    /* fraction of patience remaining before a customer visibly turns ANGRY */
#define TIPJAR_BUBBLE_MS 7000u    /* real, live-verified value -- 3000ms only ever let a bubbled
                                      customer drift ~2.7 real units before wearing off, nowhere
                                      near enough to reach the eject door from a typical seat
                                      distance (up to ~47 units) -- the eject mechanic was
                                      effectively unreachable at the original number */
#define TIPJAR_PUSH_SPEED 3.2f    /* real player-driven push, matching the wiki's own "pushing
                                      works" spec -- auto-drift alone (below) is a slow fallback,
                                      not the primary way a bubbled customer reaches the door */
#define TIPJAR_RESPAWN_DELAY_MS 2500u
#define TIPJAR_SERVE_RADIUS 3.0f
#define TIPJAR_DEESCALATE_RADIUS 4.0f
#define TIPJAR_BUMP_RADIUS 2.2f
#define TIPJAR_BUMP_COOLDOWN_MS 900u
#define TIPJAR_STYLE_BUBBLE 250        /* Turnip.style value reserved for TIPJAR bubbles -- real CharacterId values are small (0/1 today), well clear of this */

typedef enum {
    CUST_INACTIVE = 0,
    CUST_WAITING_DRINK,
    CUST_HAPPY,
    CUST_BRAWLING,
    CUST_BUBBLED
} CustomerState;

typedef enum { DRINK_SODA = 0, DRINK_ESPRESSO, DRINK_ICED_WATER, DRINK_TYPE_COUNT } DrinkType;

static const char *TIPJAR_DRINK_NAMES[DRINK_TYPE_COUNT] = { "SODA POP", "ESPRESSO", "ICED WATER" };
static const int TIPJAR_DRINK_TIPS[DRINK_TYPE_COUNT] = { 15, 25, 20 };

typedef struct {
    int active;
    int state;
    float x, y;
    float seat_x;
    int order_type;
    float patience;           /* seconds remaining in WAITING_DRINK before escalating to BRAWLING */
    float happy_timer;        /* seconds remaining in HAPPY before the seat clears for a new customer */
    unsigned int bubbled_until_ms;
    unsigned int respawn_at_ms; /* seat is empty (CUST_INACTIVE) until this real time, then a new customer sits */
    int bubbled_by;            /* player_id who landed the bubble that's currently holding this customer, -1 = none */
} Customer;

/* TipjarPlayerInput: one player's real TIPJAR-specific actions for this tick (deliver/bubble are
 * edge-triggered "was this just pressed," shield is a level "is this held right now" -- same
 * convention tipjar_tick's own single-player version already used, just per-player now). */
typedef struct {
    int deliver_pressed;
    int bubble_pressed;
    int shield_held;
} TipjarPlayerInput;

/* TipjarState (2026-08-04, Step 2 -- BRAWLPIT/docs/TIPJAR_ROADMAP.md, founder: "iterate on
 * tipjar"): score/hp/last_bump are now real per-player arrays instead of a single p0-shaped
 * value, and every Customer carries a real bubbled_by owner_id -- the roadmap's own Step 2 exit
 * criteria: "No gameplay system assumes player 0 is the only real human. All gameplay systems
 * accept player_id and work for any 0..3" and "every interactive entity has owner_id, clear
 * rules for who gets credit." The bar itself (customers, vibe, shift timer/quota) stays one
 * shared instance -- a real co-op framing, not yet the separate-per-player instances Competitive
 * Party mode (roadmap Step 4) needs; that's intentionally still ahead, not guessed at here. */
typedef struct {
    Customer customers[MAX_CUSTOMERS];
    int score[MAX_CLIENTS];
    float vibe;                /* 0-100, shared -- one bar, one shift, real co-op framing */
    unsigned int shift_start_ms;
    unsigned int shift_end_ms;
    int shift_over;
    int shift_won;
    int orders_completed;
    int orders_missed;
    int brawls_handled;
    int damage_caused;         /* shared shift-level stat, matches the acceptance criteria's own results-screen field */
    unsigned int last_bump_ms[MAX_CLIENTS];
    int player_hp[MAX_CLIENTS];
} TipjarState;

TipjarState tipjar_state;

/* tipjar_total_score: the real shared win/loss condition still reads one combined number (co-op
 * framing -- see TipjarState's own doc comment) even though credit is tracked per-player now. */
static int tipjar_total_score(void) {
    int total = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) total += tipjar_state.score[i];
    return total;
}

static void tipjar_spawn_customer(Customer *c, int seat_index, unsigned int now_ms) {
    memset(c, 0, sizeof(*c));
    c->active = 1;
    c->state = CUST_WAITING_DRINK;
    float span = TIPJAR_SEAT_MAX_X - TIPJAR_SEAT_MIN_X;
    c->seat_x = TIPJAR_SEAT_MIN_X + span * ((float)seat_index / (float)(MAX_CUSTOMERS - 1));
    c->x = c->seat_x;
    c->y = TIPJAR_FLOOR_Y;
    c->order_type = rand() % DRINK_TYPE_COUNT;
    c->patience = TIPJAR_PATIENCE_SECONDS;
    c->bubbled_by = -1;
    (void)now_ms;
}

/* tipjar_init_players: real per-player state reset, called for every ACTIVE slot (matches
 * local_init_match's own real "which slots exist" set) -- not folded into tipjar_init's own
 * memset-everything since tipjar_init doesn't know which players are active yet at the call site
 * (see tipjar_init's own doc comment). */
static void tipjar_init_player(int player_id) {
    if (player_id < 0 || player_id >= MAX_CLIENTS) return;
    tipjar_state.score[player_id] = 0;
    tipjar_state.player_hp[player_id] = 100;
    tipjar_state.last_bump_ms[player_id] = 0;
}

void tipjar_init(unsigned int now_ms) {
    memset(&tipjar_state, 0, sizeof(tipjar_state));
    tipjar_state.vibe = 60.0f;
    tipjar_state.shift_start_ms = now_ms;
    tipjar_state.shift_end_ms = now_ms + TIPJAR_SHIFT_MS;
    for (int i = 0; i < MAX_CLIENTS; i++) tipjar_init_player(i);
    for (int i = 0; i < MAX_CUSTOMERS; i++) {
        tipjar_spawn_customer(&tipjar_state.customers[i], i, now_ms);
    }
}

/* tipjar_throw_bubble: real projectile, same shape spawn_turnip already uses (position ahead of
 * the player along their facing, initial velocity, TTL) -- reuses the Turnip array directly, just
 * tagged with TIPJAR_STYLE_BUBBLE so tipjar_update_bubbles (not the fighting-game's own
 * update_turnips) is what moves/resolves it. */
static void tipjar_throw_bubble(ServerState *state, PlayerState *p) {
    for (int i = 0; i < MAX_TURNIPS; i++) {
        Turnip *t = &state->turnips[i];
        if (t->active) continue;
        t->active = 1;
        t->owner_id = p->id;
        t->x = p->x + (float)p->facing * 1.5f;
        t->y = p->y + 2.0f;
        t->vx = (float)p->facing * TURNIP_SPEED * 0.75f;
        t->vy = 0.05f; /* a real bubble arcs much less than a thrown turnip -- mostly travels level */
        t->ttl_frames = TURNIP_TTL_FRAMES;
        t->style = TIPJAR_STYLE_BUBBLE;
        break;
    }
}

/* tipjar_update_bubbles: real flight + collision against Customers, deliberately separate from
 * update_turnips (which applies real fighting-game knockback/damage to Players -- not what a
 * bubble hitting a customer should ever do). Only Turnip slots tagged TIPJAR_STYLE_BUBBLE are
 * touched, so a real turnip thrown in any other mode sharing this array is never affected. */
static void tipjar_update_bubbles(ServerState *state) {
    for (int i = 0; i < MAX_TURNIPS; i++) {
        Turnip *t = &state->turnips[i];
        if (!t->active || t->style != TIPJAR_STYLE_BUBBLE) continue;

        t->vy -= TURNIP_GRAVITY * 0.2f; /* a bubble falls much slower than a real turnip */
        t->x += t->vx;
        t->y += t->vy;
        t->ttl_frames--;
        if (t->ttl_frames <= 0 || t->y < 0.0f) { t->active = 0; continue; }

        for (int ci = 0; ci < MAX_CUSTOMERS; ci++) {
            Customer *c = &tipjar_state.customers[ci];
            if (!c->active || c->state != CUST_BRAWLING) continue;
            if (check_aabb(t->x - 0.5f, t->y - 0.5f, 1.0f, 1.0f, c->x - 0.75f, c->y, 1.5f, 3.0f)) {
                c->state = CUST_BUBBLED;
                c->bubbled_until_ms = 0; /* set for real by the caller, which has now_ms */
                c->bubbled_by = t->owner_id; /* real credit -- whoever's bubble actually landed the hit */
                t->active = 0;
                break;
            }
        }
    }
}

/* tipjar_tick: the real per-frame update for TIPJAR mode -- patience decay, escalation to
 * BRAWLING, bubble throws, delivery, de-escalation, brawler-vs-player bumps, win/loss. Movement
 * itself is still driven by update_entity (called by the caller before this, same as every other
 * mode) -- this function only owns TIPJAR's own real game-loop state.
 *
 * Step 2 (2026-08-04, founder: "iterate on tipjar" -- BRAWLPIT/docs/TIPJAR_ROADMAP.md): takes the
 * real player array + one TipjarPlayerInput per slot instead of a single hardcoded p0, and loops
 * every real interaction (delivery, de-escalate, bubble throw, brawler chase/bump, push) over
 * every ACTIVE player -- any of them can serve, bubble, or get bumped, with real credit going to
 * whichever player actually performed the action (Customer.bubbled_by, TipjarState.score[i]).
 * "Nearest active player" stands in for "which of possibly several players is this customer/
 * brawler actually interacting with" -- the real, obvious choice for a shared-space co-op bar,
 * not a placeholder. */
void tipjar_tick(ServerState *state, TipjarPlayerInput *inputs, unsigned int now_ms, float dt) {
    TipjarState *tj = &tipjar_state;
    if (tj->shift_over) return;

    for (int pi = 0; pi < MAX_CLIENTS; pi++) {
        if (!state->players[pi].active) continue;
        if (inputs[pi].bubble_pressed) {
            tipjar_throw_bubble(state, &state->players[pi]);
        }
    }
    tipjar_update_bubbles(state);
    /* Bubbles just spawned/resolved this tick need a real expiry time -- set once, right after
       tipjar_update_bubbles so any bubble that landed a hit this exact frame gets one. */
    for (int ci = 0; ci < MAX_CUSTOMERS; ci++) {
        Customer *c = &tj->customers[ci];
        if (c->active && c->state == CUST_BUBBLED && c->bubbled_until_ms == 0) {
            c->bubbled_until_ms = now_ms + TIPJAR_BUBBLE_MS;
        }
    }

    for (int ci = 0; ci < MAX_CUSTOMERS; ci++) {
        Customer *c = &tj->customers[ci];
        if (!c->active) {
            if (c->respawn_at_ms != 0 && now_ms >= c->respawn_at_ms) {
                tipjar_spawn_customer(c, ci, now_ms);
            }
            continue;
        }

        switch (c->state) {
        case CUST_WAITING_DRINK: {
            c->patience -= dt;
            for (int pi = 0; pi < MAX_CLIENTS; pi++) {
                PlayerState *p = &state->players[pi];
                if (!p->active) continue;
                float dx = p->x - c->x, dy = (p->y + 2.0f) - c->y;
                /* Named is_close, not "near": near is a reserved macro in Windows/MinGW
                   headers (a leftover 16-bit-memory-model keyword still #defined for
                   backward compat), which broke the Windows cross-compile the instant any
                   Windows-related header got pulled in transitively -- compiled fine on
                   Linux the whole time, which is exactly why this went unnoticed until CI's
                   actual mingw build. */
                int is_close = (dx * dx + dy * dy) <= (TIPJAR_SERVE_RADIUS * TIPJAR_SERVE_RADIUS);
                if (!is_close) continue;
                if (inputs[pi].deliver_pressed) {
                    c->state = CUST_HAPPY;
                    c->happy_timer = 2.0f;
                    tj->score[pi] += TIPJAR_DRINK_TIPS[c->order_type];
                    tj->vibe = fminf(100.0f, tj->vibe + 4.0f);
                    tj->orders_completed++;
                    break; /* served -- real, this customer can't be double-served by a second player this tick */
                } else if (inputs[pi].shield_held && c->patience < TIPJAR_PATIENCE_SECONDS) {
                    /* Real de-escalate action (the wiki's own "Shield... reduces anger in a small
                       radius while held") -- restores patience directly rather than just slowing
                       its decay, so a player who's fallen behind can genuinely recover a customer,
                       not just delay the inevitable. */
                    c->patience = fminf(TIPJAR_PATIENCE_SECONDS, c->patience + dt * 3.0f);
                }
            }
            if (c->active && c->state == CUST_WAITING_DRINK && c->patience <= 0.0f) {
                c->state = CUST_BRAWLING;
                tj->vibe = fmaxf(0.0f, tj->vibe - 12.0f);
                tj->orders_missed++;
            }
            break;
        }
        case CUST_HAPPY: {
            c->happy_timer -= dt;
            if (c->happy_timer <= 0.0f) {
                c->active = 0;
                c->respawn_at_ms = now_ms + TIPJAR_RESPAWN_DELAY_MS;
            }
            break;
        }
        case CUST_BRAWLING: {
            /* Real reactive chase, not scripted -- targets whichever active player is currently
               nearest, recomputed every tick (same discipline racer_bot_drive_toward used earlier
               today for WEAKNIGHT_BEDROCK_RACERS' own bots), so it genuinely follows whoever's
               actually closest rather than a fixed player index. */
            int nearest = -1;
            float nearest_d2 = 0.0f;
            for (int pi = 0; pi < MAX_CLIENTS; pi++) {
                if (!state->players[pi].active) continue;
                float dx = state->players[pi].x - c->x, dy = state->players[pi].y - c->y;
                float d2 = dx * dx + dy * dy;
                if (nearest < 0 || d2 < nearest_d2) { nearest = pi; nearest_d2 = d2; }
            }
            tj->vibe = fmaxf(0.0f, tj->vibe - dt * 1.5f); /* ongoing chaos while unresolved */
            if (nearest < 0) break;
            PlayerState *target = &state->players[nearest];
            float dir = (target->x > c->x) ? 1.0f : -1.0f;
            c->x += dir * 1.1f * dt;

            if (nearest_d2 <= (TIPJAR_BUMP_RADIUS * TIPJAR_BUMP_RADIUS) &&
                now_ms - tj->last_bump_ms[nearest] >= TIPJAR_BUMP_COOLDOWN_MS) {
                tj->last_bump_ms[nearest] = now_ms;
                tj->player_hp[nearest] -= 8;
                tj->damage_caused += 8;
                tj->vibe = fmaxf(0.0f, tj->vibe - 3.0f);
            }
            break;
        }
        case CUST_BUBBLED: {
            c->y += 0.03f; /* real slow float, matches the wiki's own "float slowly upward" */
            float auto_dir = (c->x > TIPJAR_EJECT_DOOR_X) ? -1.0f : 1.0f;
            c->x += auto_dir * 1.6f * dt; /* slow fallback drift -- not the primary mechanic, see push below */

            /* Real player-driven push (the wiki's own actual Build-1 spec: "while bubbled: ...
               pushing works") -- any active player standing on the bubble pushes it in their own
               real input direction; credit for the eventual eject still goes to whoever actually
               landed the bubble (c->bubbled_by), not whoever happens to be pushing. */
            for (int pi = 0; pi < MAX_CLIENTS; pi++) {
                PlayerState *p = &state->players[pi];
                if (!p->active) continue;
                if (check_aabb(p->x - 1.0f, p->y, 2.0f, 4.0f, c->x - 0.75f, c->y, 1.5f, 3.0f)
                    && fabsf(p->in_x) > 0.01f) {
                    c->x += p->in_x * TIPJAR_PUSH_SPEED * dt;
                }
            }

            if (c->x <= TIPJAR_EJECT_DOOR_X) {
                int credit = (c->bubbled_by >= 0 && c->bubbled_by < MAX_CLIENTS) ? c->bubbled_by : 0;
                tj->score[credit] += 10; /* eject bonus, per the wiki's own "bonus tips" */
                tj->vibe = fminf(100.0f, tj->vibe + 6.0f);
                tj->brawls_handled++;
                c->active = 0;
                c->bubbled_by = -1;
                c->respawn_at_ms = now_ms + TIPJAR_RESPAWN_DELAY_MS;
            } else if (now_ms >= c->bubbled_until_ms) {
                /* Bubble wore off before reaching the door -- back to brawling, not auto-resolved. */
                c->state = CUST_BRAWLING;
                c->bubbled_by = -1;
            }
            break;
        }
        default: break;
        }
    }

    int total = tipjar_total_score();
    int any_player_dead = 0;
    for (int pi = 0; pi < MAX_CLIENTS; pi++) {
        if (state->players[pi].active && tj->player_hp[pi] <= 0) any_player_dead = 1;
    }
    if (total >= TIPJAR_QUOTA) {
        tj->shift_over = 1; tj->shift_won = 1;
    } else if (tj->vibe <= 0.0f || any_player_dead) {
        tj->shift_over = 1; tj->shift_won = 0;
    } else if (now_ms >= tj->shift_end_ms) {
        tj->shift_over = 1; tj->shift_won = (total >= TIPJAR_QUOTA);
    }
}

#endif
