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
} Customer;

typedef struct {
    Customer customers[MAX_CUSTOMERS];
    int score;
    float vibe;                /* 0-100 */
    unsigned int shift_start_ms;
    unsigned int shift_end_ms;
    int shift_over;
    int shift_won;
    int orders_completed;
    int orders_missed;
    int brawls_handled;
    int damage_caused;         /* real -- taken from bumps a BRAWLING customer lands on the player */
    unsigned int last_bump_ms;
    int player_hp;
} TipjarState;

TipjarState tipjar_state;

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
    (void)now_ms;
}

void tipjar_init(unsigned int now_ms) {
    memset(&tipjar_state, 0, sizeof(tipjar_state));
    tipjar_state.vibe = 60.0f;
    tipjar_state.shift_start_ms = now_ms;
    tipjar_state.shift_end_ms = now_ms + TIPJAR_SHIFT_MS;
    tipjar_state.player_hp = 100;
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
                t->active = 0;
                break;
            }
        }
    }
}

/* tipjar_tick: the real per-frame update for TIPJAR mode -- patience decay, escalation to
 * BRAWLING, bubble throws, delivery, de-escalation, brawler-vs-player bumps, win/loss. Movement
 * itself is still driven by update_entity (called by the caller before this, same as every other
 * mode) -- this function only owns TIPJAR's own real game-loop state. */
void tipjar_tick(ServerState *state, PlayerState *p0, int deliver_pressed, int bubble_pressed,
                  int shield_held, unsigned int now_ms, float dt) {
    TipjarState *tj = &tipjar_state;
    if (tj->shift_over) return;

    if (bubble_pressed) {
        tipjar_throw_bubble(state, p0);
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
            float dx = p0->x - c->x, dy = (p0->y + 2.0f) - c->y;
            int near = (dx * dx + dy * dy) <= (TIPJAR_SERVE_RADIUS * TIPJAR_SERVE_RADIUS);
            if (near && deliver_pressed) {
                c->state = CUST_HAPPY;
                c->happy_timer = 2.0f;
                tj->score += TIPJAR_DRINK_TIPS[c->order_type];
                tj->vibe = fminf(100.0f, tj->vibe + 4.0f);
                tj->orders_completed++;
            } else if (shield_held && near && c->patience < TIPJAR_PATIENCE_SECONDS) {
                /* Real de-escalate action (the wiki's own "Shield... reduces anger in a small
                   radius while held") -- restores patience directly rather than just slowing its
                   decay, so a player who's fallen behind can genuinely recover a customer, not
                   just delay the inevitable. */
                c->patience = fminf(TIPJAR_PATIENCE_SECONDS, c->patience + dt * 3.0f);
            }
            if (c->patience <= 0.0f) {
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
            /* Real reactive chase, not scripted -- recomputed off the player's own current
               position every tick, same discipline racer_bot_drive_toward used earlier today for
               WEAKNIGHT_BEDROCK_RACERS' own bots. */
            float dir = (p0->x > c->x) ? 1.0f : -1.0f;
            c->x += dir * 1.1f * dt;
            tj->vibe = fmaxf(0.0f, tj->vibe - dt * 1.5f); /* ongoing chaos while unresolved */

            float dx = p0->x - c->x, dy = p0->y - c->y;
            if ((dx * dx + dy * dy) <= (TIPJAR_BUMP_RADIUS * TIPJAR_BUMP_RADIUS) &&
                now_ms - tj->last_bump_ms >= TIPJAR_BUMP_COOLDOWN_MS) {
                tj->last_bump_ms = now_ms;
                tj->player_hp -= 8;
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
               pushing works" -- not automatic delivery to the door). Real AABB contact against the
               player's own real hitbox, moved in the player's real current input direction, not
               just toward the door -- a player can push a bubbled customer the wrong way too,
               same as a real physical push would allow. */
            if (check_aabb(p0->x - 1.0f, p0->y, 2.0f, 4.0f, c->x - 0.75f, c->y, 1.5f, 3.0f)
                && fabsf(p0->in_x) > 0.01f) {
                c->x += p0->in_x * TIPJAR_PUSH_SPEED * dt;
            }

            if (c->x <= TIPJAR_EJECT_DOOR_X) {
                tj->score += 10; /* eject bonus, per the wiki's own "bonus tips" */
                tj->vibe = fminf(100.0f, tj->vibe + 6.0f);
                tj->brawls_handled++;
                c->active = 0;
                c->respawn_at_ms = now_ms + TIPJAR_RESPAWN_DELAY_MS;
            } else if (now_ms >= c->bubbled_until_ms) {
                /* Bubble wore off before reaching the door -- back to brawling, not auto-resolved. */
                c->state = CUST_BRAWLING;
            }
            break;
        }
        default: break;
        }
    }

    if (tj->score >= TIPJAR_QUOTA) {
        tj->shift_over = 1; tj->shift_won = 1;
    } else if (tj->vibe <= 0.0f || tj->player_hp <= 0) {
        tj->shift_over = 1; tj->shift_won = 0;
    } else if (now_ms >= tj->shift_end_ms) {
        tj->shift_over = 1; tj->shift_won = (tj->score >= TIPJAR_QUOTA);
    }
}

#endif
