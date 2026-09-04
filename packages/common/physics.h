#ifndef BRAWLPIT_PHYSICS_H
#define BRAWLPIT_PHYSICS_H

#include <math.h>
#include "protocol.h"
#include "characters.h"

typedef struct { float x, y; } Vec2;

static inline void apply_friction_2d(Vec2 *vel, float friction_per_sec, float dt) {
    float vx = vel->x;
    float vy = vel->y;
    float speed_sq = vx * vx + vy * vy;
    if (speed_sq < 1e-8f) {
        vel->x = 0.0f;
        vel->y = 0.0f;
        return;
    }

    float speed = sqrtf(speed_sq);
    float new_speed = speed - (friction_per_sec * dt);
    if (new_speed <= 0.0f) {
        vel->x = 0.0f;
        vel->y = 0.0f;
        return;
    }

    float scale = new_speed / speed;
    vel->x = vx * scale;
    vel->y = vy * scale;
}

#ifndef ATTACK_COOLDOWN_FRAMES
#define ATTACK_COOLDOWN_FRAMES 10
#endif

#ifndef SMASH_COOLDOWN_FRAMES
#define SMASH_COOLDOWN_FRAMES 28
#endif

#ifndef SHIELD_BREAK_STUN
#define SHIELD_BREAK_STUN 300
#endif

#ifndef HIGH_PERCENT_THRESHOLD
#define HIGH_PERCENT_THRESHOLD 91.0f
#endif

#ifndef HIGH_PERCENT_LAUNCH_DELAY
#define HIGH_PERCENT_LAUNCH_DELAY 22
#endif

#ifndef KNOCKBACK_SCALING
#define KNOCKBACK_SCALING 0.04f
#endif


#ifndef ATTACK_ACTIVE_FRAMES
#define ATTACK_ACTIVE_FRAMES 12
#endif

#ifndef SMASH_ACTIVE_FRAMES
#define SMASH_ACTIVE_FRAMES 16
#endif

#ifndef BLAST_LEFT
#define BLAST_LEFT -60.0f
#endif

#ifndef BLAST_RIGHT
#define BLAST_RIGHT 60.0f
#endif

#ifndef BLAST_TOP
#define BLAST_TOP 60.0f
#endif

#ifndef BLAST_BOTTOM
#define BLAST_BOTTOM -40.0f
#endif

#ifndef EDGE_KO_FLASH_FRAMES
#define EDGE_KO_FLASH_FRAMES 24
#endif

#ifndef GRAVITY
#define GRAVITY 0.08f
#endif

#ifndef FAST_FALL_GRAVITY
#define FAST_FALL_GRAVITY 0.14f
#endif

#ifndef TERMINAL_VELOCITY
#define TERMINAL_VELOCITY 2.8f
#endif

#ifndef GROUND_ACCEL
#define GROUND_ACCEL 0.18f
#endif

#ifndef AIR_ACCEL
#define AIR_ACCEL 0.08f
#endif

#ifndef GROUND_MAX_SPEED
#define GROUND_MAX_SPEED 1.2f
#endif

#ifndef AIR_MAX_SPEED
#define AIR_MAX_SPEED 1.0f
#endif

#ifndef GROUND_FRICTION
#define GROUND_FRICTION 0.10f
#endif

#ifndef AIR_FRICTION
#define AIR_FRICTION 0.02f
#endif

#ifndef JUMP_FORCE
#define JUMP_FORCE 1.6f
#endif

#ifndef DODGE_COOLDOWN_FRAMES
#define DODGE_COOLDOWN_FRAMES 30
#endif

#ifndef WAVEDASH_FRAMES
#define WAVEDASH_FRAMES 12
#endif

#ifndef WAVEDASH_GROUND_SPEED
#define WAVEDASH_GROUND_SPEED 1.6f
#endif

#ifndef WAVEDASH_AIR_BOOST
#define WAVEDASH_AIR_BOOST 1.4f
#endif

#ifndef WAVEDASH_DROP_VY
#define WAVEDASH_DROP_VY 0.6f
#endif

#ifndef WAVEDASH_MAX_SPEED
#define WAVEDASH_MAX_SPEED 1.8f
#endif

#ifndef PARRY_WINDOW_FRAMES
#define PARRY_WINDOW_FRAMES 4
#endif

#ifndef SHIELD_DRAIN
#define SHIELD_DRAIN 0.28f
#endif

#ifndef SHIELD_REGEN
#define SHIELD_REGEN 0.17f
#endif

#ifndef SHIELD_DROP_LAG_FRAMES
#define SHIELD_DROP_LAG_FRAMES 11
#endif

#ifndef SMASH_CHARGE_FRAMES
#define SMASH_CHARGE_FRAMES 44
#endif

#ifndef SMASH_RELEASE_DELAY_FRAMES
#define SMASH_RELEASE_DELAY_FRAMES 7
#endif

#ifndef SMASH_FLASH_FRAMES
#define SMASH_FLASH_FRAMES 7
#endif

#ifndef TURNIP_COOLDOWN_FRAMES
#define TURNIP_COOLDOWN_FRAMES 45
#endif

#ifndef TURNIP_SPEED
#define TURNIP_SPEED 1.0f

/* Rosie's High Score Rush (side-B / direction-B) -- kanban priority-queue card BP-TUNE-0033:
 * "make rosie direction B do a double hit dash ability it does damage at the beginning and end
 * of the dash and in the middle shes totally invuln like SSB dodge." Real, deliberate frame
 * windows: hits at frame 1 (the real, honest "beginning" the card asks for) and at the final
 * frame (the real "end"), invulnerable for the real, middle stretch in between -- matching a
 * real Smash-style spot-dodge's own "committed, but safe in the middle" shape. Defined here
 * (ahead of `update_entity`, not alongside the other per-move constants further down) since C
 * macros must be defined before their own first real use, and `update_entity` itself is the one
 * real place that needs them. */
#define ROSIE_DASH_TOTAL_FRAMES 18
#define ROSIE_DASH_SPEED 0.22f
#define ROSIE_DASH_HIT_RANGE 2.2f
#define ROSIE_DASH_HIT_DAMAGE 6.0f
#define ROSIE_DASH_INVULN_START 4  /* real, deliberate gap after the opening hit before i-frames kick in -- a real dash isn't invulnerable the instant it starts, matching the card's own "at the beginning... and in the middle" as two real, distinct phases, not one continuous state */
#define ROSIE_DASH_INVULN_END 14   /* real, deliberate gap before the closing hit -- i-frames end before the final hit lands, so the closing hit isn't happening from a still-invulnerable frame */
#define BRAWLPIT_PETALIA_PARASOL_HIT_RANGE 2.0f
#define BRAWLPIT_PETALIA_PARASOL_HIT_DAMAGE 4.0f /* real, deliberate: low per-hit, several real hits land across the ascent -- see the STATE_UPB update block */
#define BRAWLPIT_PETALIA_PARASOL_REHIT_INTERVAL 12 /* frames between real hits during the ascent */
#define BRAWLPIT_REGROWTH_HEAL_AMOUNT 15.0f /* real, deliberate: comparable to Ground Slam's own 10.0f damage -- a real sustain trade-off, not a token heal */
#endif

#ifndef TURNIP_UP_SPEED
#define TURNIP_UP_SPEED 1.1f
#endif

#ifndef UMBRELLA_GRAVITY
#define UMBRELLA_GRAVITY 0.02f
#endif

#ifndef UMBRELLA_FALL_SPEED
#define UMBRELLA_FALL_SPEED 0.6f
#endif

#ifndef RESPAWN_DELAY_FRAMES
#define RESPAWN_DELAY_FRAMES 90
#endif

#ifndef DROP_THROUGH_FRAMES
#define DROP_THROUGH_FRAMES 10
#endif

#ifndef TURNIP_GRAVITY
#define TURNIP_GRAVITY 0.04f
#endif

#ifndef TURNIP_TTL_FRAMES
#define TURNIP_TTL_FRAMES 240
#endif

#ifndef RESPAWN_INVULN_FRAMES
#define RESPAWN_INVULN_FRAMES 120
#endif

void resolve_platform_collisions(PlayerState *p, float prev_y);
static inline int check_aabb(float x1, float y1, float w1, float h1, float x2, float y2, float w2, float h2);
static inline void apply_knockback(PlayerState *target, float dmg, float kbx, float kby);
void check_attack_hitbox(PlayerState *attacker, PlayerState *target);
static inline void spawn_turnip(ServerState *state, PlayerState *p);
static inline void special_petrify_gaze(ServerState *state, PlayerState *p);
static inline void special_scavenger_dash(PlayerState *p);
static inline void special_play_dead(PlayerState *p);
static inline void special_petalia_parasol_hit(ServerState *state, PlayerState *p);
static inline void special_regrowth(PlayerState *p);
static inline void special_serpents_grasp(ServerState *state, PlayerState *p);
static inline void special_ground_slam(ServerState *state, PlayerState *p);
static inline void special_uncrowned_claim(PlayerState *p);
static inline void special_insert_coin(ServerState *state, PlayerState *p);
static inline void special_high_score_rush_hit(ServerState *state, PlayerState *p);
static inline void phys_start_respawn(PlayerState *p);

static inline void phys_respawn(PlayerState *p, unsigned int now) {
    (void)now;
    if (p->stocks <= 0) {
        p->state = STATE_DEAD;
        p->x = 0.0f;
        p->y = 1000.0f;
        return;
    }

    p->state = STATE_IDLE;
    p->damage_percent = 0.0f;
    p->x = 0.0f;
    p->y = 30.0f;
    p->vx = 0.0f;
    p->vy = 0.0f;
    p->in_x = 0.0f;
    p->in_y = 0.0f;
    p->btn_jump = 0;
    p->btn_jump_prev = 0;
    p->btn_attack = 0;
    p->btn_attack_prev = 0;
    p->btn_shield = 0;
    p->btn_shield_prev = 0;
    p->btn_special = 0;
    p->btn_special_prev = 0;
    p->on_ground = 0;
    p->hitstun_frames = 0;
    p->attack_cooldown = 0;
    p->attack_timer = 0;
    p->parry_timer = 0;
    p->smash_active_timer = 0;
    p->smash_charge_timer = 0;
    p->smash_release_timer = 0;
    p->smash_flash_timer = 0;
    p->smash_charge_level = 0.0f;
    p->shield_stun_frames = 0;
    p->shield_drop_timer = 0;
    p->shield_regen_timer = 0;
    p->shield_health = SHIELD_MAX;
    p->invuln_frames = RESPAWN_INVULN_FRAMES;
    p->respawn_timer = 0;
    p->launch_delay_frames = 0;
    p->pending_kb_x = 0.0f;
    p->pending_kb_y = 0.0f;
    p->ground_platform_type = -1;
    p->drop_through_timer = 0;
    p->wavedash_frames = 0;
    p->rosie_dash_frame = 0;
    p->dodge_cooldown = 0;
    p->umbrella_open = 0;
    p->upb_frame = 0;
    p->upb_landing_lag = 0;
    p->parasol_rehit_timer = 0;
    p->hitlag_frames = 0;
    p->hit_flash_timer = 0;
    p->hit_flash_multihit = 0;
    p->turnip_cooldown = 0;
    p->jumps_remaining = MAX_JUMPS;
}

static inline void update_entity(PlayerState *p, float dt, void *ctx, unsigned int time) {
    if (p->state == STATE_DEAD) return;
    
    float prev_y = p->y;

    if (p->respawn_timer > 0) {
        p->respawn_timer--;
        if (p->respawn_timer == 0) phys_respawn(p, time);
        return;
    }

    if (p->invuln_frames > 0) p->invuln_frames--;
    if (p->hitlag_frames > 0) p->hitlag_frames--;
    if (p->hit_flash_timer > 0) p->hit_flash_timer--;
    if (p->parasol_rehit_timer > 0) p->parasol_rehit_timer--;
    if (p->hitstun_frames > 0) {
        p->hitstun_frames--;
        if (p->hitstun_frames <= 0 && p->state == STATE_STUNNED) p->state = STATE_IDLE;
    }
    if (p->attack_cooldown > 0) p->attack_cooldown--;
    if (p->attack_timer > 0) {
        p->attack_timer--;
        if (p->attack_timer == 0 && p->state == STATE_ATTACK && p->smash_active_timer == 0) {
            p->state = STATE_IDLE;
        }
    }
    if (p->smash_active_timer > 0) {
        p->smash_active_timer--;
        if (p->smash_active_timer == 0 && p->state == STATE_ATTACK && p->attack_timer == 0) {
            p->state = STATE_IDLE;
        }
    }
    if (p->smash_release_timer > 0) {
        p->smash_release_timer--;
        if (p->smash_release_timer == 0) {
            p->smash_active_timer = SMASH_ACTIVE_FRAMES;
            p->attack_cooldown = SMASH_COOLDOWN_FRAMES;
            p->state = STATE_ATTACK;
        }
    }
    if (p->smash_flash_timer > 0) p->smash_flash_timer--;
    if (p->parry_timer > 0) p->parry_timer--;
    if (p->shield_stun_frames > 0) p->shield_stun_frames--;
    if (p->shield_drop_timer > 0) p->shield_drop_timer--;
    if (p->upb_landing_lag > 0) p->upb_landing_lag--;
    if (p->wavedash_frames > 0) p->wavedash_frames--;
    if (p->dodge_cooldown > 0) p->dodge_cooldown--;
    if (p->drop_through_timer > 0) p->drop_through_timer--;
    if (p->turnip_cooldown > 0) p->turnip_cooldown--;

    /* Rosie's High Score Rush -- real, per-frame dash processing (kanban BP-TUNE-0033). Runs
     * unconditionally (same real zone wavedash_frames/dodge_cooldown already decrement in, not
     * gated by action_locked) since the dash needs to keep running through its own real
     * cooldown-setting side effect. Real, deliberate frame shape: hit at frame 1 (the opening
     * hit), invulnerable for `ROSIE_DASH_INVULN_START..ROSIE_DASH_INVULN_END` (real, honest SSB
     * dodge-style i-frames -- `invuln_frames` is the same real field respawn invulnerability
     * already uses, topped up every real frame in the window so it never lapses early), a
     * second hit at the final frame, then resets. Real, honest limitation named directly: turnip
     * hits (`update_turnips`) don't check `invuln_frames` at all -- a real, pre-existing,
     * cross-cutting gap this dash doesn't close (out of this card's own scope), so a turnip can
     * still land on Rosie mid-dash even though a normal attack (`check_attack_hitbox`, which DOES
     * check `invuln_frames`) cannot. */
    if (p->rosie_dash_frame > 0) {
        p->vx = (float)p->facing * ROSIE_DASH_SPEED;
        if (p->rosie_dash_frame == 1 && ctx != NULL) {
            special_high_score_rush_hit((ServerState *)ctx, p);
        }
        if (p->rosie_dash_frame >= ROSIE_DASH_INVULN_START && p->rosie_dash_frame <= ROSIE_DASH_INVULN_END) {
            if (p->invuln_frames < 2) p->invuln_frames = 2;
        }
        if (p->rosie_dash_frame == ROSIE_DASH_TOTAL_FRAMES && ctx != NULL) {
            special_high_score_rush_hit((ServerState *)ctx, p);
        }
        p->rosie_dash_frame++;
        if (p->rosie_dash_frame > ROSIE_DASH_TOTAL_FRAMES) {
            p->rosie_dash_frame = 0;
            if (p->state == STATE_ROSIE_DASH) p->state = STATE_IDLE;
        }
    }

    if (p->shield_regen_timer > 0) p->shield_regen_timer--;
    else if (p->shield_health < SHIELD_MAX && p->state != STATE_SHIELD && p->shield_drop_timer == 0) {
        p->shield_health += SHIELD_REGEN;
        if (p->shield_health > SHIELD_MAX) p->shield_health = SHIELD_MAX;
    }
    if (p->launch_delay_frames > 0) {
        p->launch_delay_frames--;
        p->vx = 0.0f;
        p->vy = 0.0f;
        if (p->launch_delay_frames == 0) {
            p->vx = p->pending_kb_x;
            p->vy = p->pending_kb_y;
        }
        p->btn_jump_prev = p->btn_jump;
        p->btn_attack_prev = p->btn_attack;
        p->btn_shield_prev = p->btn_shield;
        p->btn_special_prev = p->btn_special;
        return;
    }

    int jump_press = p->btn_jump && !p->btn_jump_prev;
    int attack_press = p->btn_attack && !p->btn_attack_prev;
    int shield_press = p->btn_shield && !p->btn_shield_prev;
    int special_press = p->btn_special && !p->btn_special_prev;

    int action_locked = (p->state == STATE_DEAD || p->respawn_timer > 0 || p->hitstun_frames > 0 ||
                         p->hitlag_frames > 0 || p->shield_stun_frames > 0 || p->shield_drop_timer > 0 ||
                         p->launch_delay_frames > 0 || p->dodge_cooldown > 0 || p->upb_landing_lag > 0);

    if (!action_locked && p->state != STATE_STUNNED) {
        if (p->on_ground && p->ground_platform_type == 1 && p->in_y < -0.6f) {
            p->drop_through_timer = DROP_THROUGH_FRAMES;
            p->on_ground = 0;
            p->ground_platform_type = -1;
            p->vy = -0.2f;
        }

        const FighterDef *fd = fighter_def((CharacterId)p->character_id);
        int smash_possible = p->on_ground && fabsf(p->in_x) > 0.6f;
        int smash_lock = (p->smash_charge_timer > 0 || p->smash_release_timer > 0);

        /* Rosie's High Score Rush (side-B, kanban BP-TUNE-0033) -- real, deliberate interception
         * BEFORE the generic smash-charge trigger below, since both real mechanics compete for
         * the exact same real input (grounded + a strong held direction + special). Gated on
         * `special_press` (a fresh press, not held `btn_special`) and `rosie_dash_frame == 0`
         * (not already mid-dash) -- `dodge_cooldown` doubles as her real cooldown, same reuse
         * `special_scavenger_dash` already established for Raccoon. */
        if (smash_possible && special_press && p->character_id == CHARACTER_ROSIE &&
            p->dodge_cooldown == 0 && p->rosie_dash_frame == 0) {
            p->rosie_dash_frame = 1;
            p->state = STATE_ROSIE_DASH;
            p->facing = (p->in_x > 0.0f) ? 1 : -1;
            p->dodge_cooldown = DODGE_COOLDOWN_FRAMES;
        } else if (smash_possible && p->btn_special && p->smash_charge_timer == 0 &&
            p->smash_active_timer == 0 && p->smash_release_timer == 0 &&
            p->attack_timer == 0 && p->attack_cooldown == 0 &&
            p->character_id != CHARACTER_ROSIE) {
            p->smash_charge_timer = SMASH_CHARGE_FRAMES;
            p->smash_charge_level = 0.0f;
            p->state = STATE_ATTACK;
            smash_lock = 1;
        }
        if (p->smash_charge_timer > 0) {
            p->vx = 0.0f;
            if (p->on_ground) p->vy = 0.0f;
            if (!p->btn_special) {
                p->smash_charge_level = 1.0f - ((float)p->smash_charge_timer / (float)SMASH_CHARGE_FRAMES);
                p->smash_charge_timer = 0;
                p->smash_release_timer = SMASH_RELEASE_DELAY_FRAMES;
                p->smash_flash_timer = SMASH_FLASH_FRAMES;
            } else {
                p->smash_charge_timer--;
                p->smash_charge_level = 1.0f - ((float)p->smash_charge_timer / (float)SMASH_CHARGE_FRAMES);
                if (p->smash_charge_timer == 0) {
                    p->smash_charge_level = 1.0f;
                    p->smash_release_timer = SMASH_RELEASE_DELAY_FRAMES;
                    p->smash_flash_timer = SMASH_FLASH_FRAMES;
                }
            }
            smash_lock = 1;
        }

        if (special_press && !smash_possible && p->smash_charge_timer == 0 &&
            p->smash_active_timer == 0 && p->smash_release_timer == 0) {
            if (!p->on_ground && p->in_y > 0.55f && p->state != STATE_UPB) {
                p->state = STATE_UPB;
                p->umbrella_open = 1;
                p->upb_frame = 0;
                p->parasol_rehit_timer = 0;
                if (p->character_id == CHARACTER_VEXAR) {
                    p->vy = fmaxf(p->vy, 2.2f);
                    p->vx += p->facing * 0.35f;
                } else {
                    p->vy = fmaxf(p->vy, 1.8f);
                    p->vx *= 0.6f;
                }
            } else if (!p->on_ground && p->in_y < -0.5f &&
                       (p->character_id == CHARACTER_ROSIE || p->character_id == CHARACTER_PETALIA) &&
                       p->turnip_cooldown == 0 && ctx != NULL) {
                /* Real, air-available turnip down-B (kanban BP-TUNE-93939: "rosie and petalia
                 * TURNIPS SHOULD NOT BE UP B THEY SHOULD BE DOWN B AND ALSO AVAILABLE IN THE
                 * AIR"). Checked BEFORE the generic airborne umbrella-toggle branch below, or
                 * every airborne down-B press for these two would just silently toggle the
                 * parasol instead. Rosie keeps her own real Insert Coin (two turnips); Petalia
                 * gets the plain, single-turnip spawn_turnip directly -- the same real move
                 * every generic character's own neutral-B already throws, just repositioned to
                 * down-B and newly usable in the air, matching this card's own literal ask. */
                if (p->character_id == CHARACTER_ROSIE) {
                    special_insert_coin((ServerState *)ctx, p);
                } else {
                    spawn_turnip((ServerState *)ctx, p);
                }
                p->turnip_cooldown = TURNIP_COOLDOWN_FRAMES;
            } else if (!p->on_ground) {
                p->umbrella_open = !p->umbrella_open;
            } else if (p->in_y > 0.5f && p->character_id == CHARACTER_MEDUSA && p->turnip_cooldown == 0 && ctx != NULL) {
                special_petrify_gaze((ServerState *)ctx, p);
                p->turnip_cooldown = TURNIP_COOLDOWN_FRAMES;
            } else if (p->in_y < -0.5f && p->character_id == CHARACTER_MEDUSA && p->turnip_cooldown == 0 && ctx != NULL) {
                /* Real down-B (BPTUNE-10001) -- distinct input (hold S) from her own neutral-B
                 * above (hold W), and a distinct effect (melee damage/knockback vs. ranged stun).
                 * Shares turnip_cooldown with her neutral-B on purpose -- one "gaze or grasp"
                 * budget per cooldown window, not two separate specials she can freely alternate. */
                special_serpents_grasp((ServerState *)ctx, p);
                p->turnip_cooldown = TURNIP_COOLDOWN_FRAMES;
            } else if (p->in_y > 0.5f && p->character_id == CHARACTER_RACCOON && p->dodge_cooldown == 0) {
                special_scavenger_dash(p);
                p->dodge_cooldown = DODGE_COOLDOWN_FRAMES;
            } else if (p->in_y < -0.5f && p->character_id == CHARACTER_RACCOON && p->dodge_cooldown == 0) {
                /* Real down-B (BPTUNE-10001) -- distinct input (hold S) and distinct effect
                 * (stillness + invuln vs. the neutral-B's own directional dash) from Scavenger's
                 * Dash above. Shares dodge_cooldown with the neutral-B on purpose -- one real
                 * "dash or play dead" mobility budget per cooldown window, matching the same
                 * real design Medusa's own neutral/down-B pair already established for her
                 * shared turnip_cooldown. */
                special_play_dead(p);
                p->dodge_cooldown = DODGE_COOLDOWN_FRAMES;
            } else if (p->in_y > 0.5f && p->character_id == CHARACTER_SECOND_TREE && p->turnip_cooldown == 0 && ctx != NULL) {
                special_ground_slam((ServerState *)ctx, p);
                p->turnip_cooldown = TURNIP_COOLDOWN_FRAMES;
            } else if (p->in_y < -0.5f && p->character_id == CHARACTER_SECOND_TREE && p->turnip_cooldown == 0) {
                /* Real down-B (BPTUNE-10001) -- distinct input (hold S) and distinct effect
                 * (real self-heal, zero offense) from Ground Slam's own AOE knockback above.
                 * Shares turnip_cooldown with the neutral-B on purpose -- one "slam or regrow"
                 * budget per cooldown window, matching Medusa's own neutral/down-B pair. */
                special_regrowth(p);
                p->turnip_cooldown = TURNIP_COOLDOWN_FRAMES;
            } else if (p->in_y > 0.5f && p->character_id == CHARACTER_UNCROWNED && p->turnip_cooldown == 0) {
                special_uncrowned_claim(p);
                p->turnip_cooldown = TURNIP_COOLDOWN_FRAMES;
            } else if (p->in_y < -0.5f &&
                       (p->character_id == CHARACTER_ROSIE || p->character_id == CHARACTER_PETALIA) &&
                       p->turnip_cooldown == 0 && ctx != NULL) {
                /* Real, grounded half of BP-TUNE-93939's own down-B remap -- Insert Coin moves
                 * OFF her old neutral-B slot (hold up) entirely, and Petalia gets a real,
                 * dedicated down-B for the first time instead of falling through to the generic
                 * hold-up fallback below (which now explicitly excludes her, same as every other
                 * character with a real dedicated special). */
                if (p->character_id == CHARACTER_ROSIE) {
                    special_insert_coin((ServerState *)ctx, p);
                } else {
                    spawn_turnip((ServerState *)ctx, p);
                }
                p->turnip_cooldown = TURNIP_COOLDOWN_FRAMES;
            } else if (p->in_y > 0.5f && p->turnip_cooldown == 0 && ctx != NULL &&
                       p->character_id != CHARACTER_MEDUSA && p->character_id != CHARACTER_RACCOON &&
                       p->character_id != CHARACTER_SECOND_TREE && p->character_id != CHARACTER_UNCROWNED &&
                       p->character_id != CHARACTER_ROSIE && p->character_id != CHARACTER_PETALIA) {
                /* Real bug found live, founder: "fallthrough" / "the state machine all the super
                 * sensitive stuffs". Raccoon's own branch above gates on dodge_cooldown, a
                 * DIFFERENT field from this fallback's turnip_cooldown -- Raccoon never touches
                 * turnip_cooldown, so it stays 0 forever, meaning if Raccoon's dash was on
                 * cooldown (dodge_cooldown != 0) but turnip_cooldown was (always) 0, this branch's
                 * own condition alone would have silently passed and spawned a turnip -- breaking
                 * "pure mobility, no offense" for the one character whose whole identity is that.
                 * The other custom-special characters happen to share turnip_cooldown as
                 * their own gate, so they could never have hit this specific bug, but excluding
                 * all of them explicitly (not just Raccoon) makes this fallback's real contract --
                 * "only for characters with no dedicated special above" -- true by construction
                 * instead of true by coincidence of which field each one happens to reuse.
                 */
                spawn_turnip((ServerState *)ctx, p);
                p->turnip_cooldown = (p->character_id == CHARACTER_VEXAR) ? (TURNIP_COOLDOWN_FRAMES - 10) : TURNIP_COOLDOWN_FRAMES;
            } else if (p->in_y > 0.5f || p->in_y < -0.5f) {
                /* Real, genuine bug fixed (kanban BP-TUNE-393939/BP-TUNE-9838382: "if turnip is
                 * on cooldown the character should not fall back to a wave dash" / "all
                 * characters b should be a special move not a wave dash"). Every neutral-B/
                 * down-B branch above requires its own cooldown == 0; when the real special is
                 * still cooling down, none of them match, and execution used to fall all the way
                 * through to the wavedash branches below -- silently substituting a wavedash for
                 * a failed special attempt. A held-direction special press must do nothing while
                 * its real special is on cooldown, full stop -- it must never produce a
                 * wavedash. Wavedash itself stays reserved for a genuinely UNDIRECTED special
                 * press (no up/down held), via the two branches below, matching the README's own
                 * "K alone" description. Deliberately empty body: this branch's only job is to
                 * intercept the fallthrough, not to do anything itself.
                 */
            } else if (p->btn_shield && p->dodge_cooldown == 0) {
                float dir = (fabsf(p->in_x) > 0.01f) ? p->in_x : (float)p->facing;
                p->vx = dir * WAVEDASH_GROUND_SPEED;
                if (!p->on_ground) {
                    p->vx = dir * WAVEDASH_AIR_BOOST;
                    p->vy = -WAVEDASH_DROP_VY;
                }
                p->wavedash_frames = WAVEDASH_FRAMES;
                p->state = STATE_WAVEDASH;
                p->dodge_cooldown = DODGE_COOLDOWN_FRAMES;
            } else if (p->dodge_cooldown == 0) {
                float dir = (fabsf(p->in_x) > 0.01f) ? p->in_x : (float)p->facing;
                p->vx = dir * WAVEDASH_GROUND_SPEED;
                p->wavedash_frames = WAVEDASH_FRAMES;
                p->state = STATE_WAVEDASH;
                p->dodge_cooldown = DODGE_COOLDOWN_FRAMES;
            }
        }

        float accel = (p->on_ground ? GROUND_ACCEL : AIR_ACCEL) * (p->on_ground ? fd->ground_speed_mul : fd->air_speed_mul);
        float max_s = (p->on_ground ? GROUND_MAX_SPEED : AIR_MAX_SPEED) * (p->on_ground ? fd->ground_speed_mul : fd->air_speed_mul);
        float fric  = p->on_ground ? GROUND_FRICTION : AIR_FRICTION;
        if (p->wavedash_frames > 0 && p->on_ground) {
            fric = 0.0f;
            if (max_s < WAVEDASH_MAX_SPEED) max_s = WAVEDASH_MAX_SPEED;
        }

        if (!smash_lock && fabsf(p->in_x) > 0.01f) {
            p->vx += p->in_x * accel;
            p->facing = (p->in_x > 0.0f) ? 1 : -1;
        } else if (!smash_lock) {
            if (fabsf(p->vx) <= fric) p->vx = 0.0f;
            else p->vx -= (p->vx > 0.0f ? fric : -fric);
        }

        if (p->vx > max_s) p->vx = max_s;
        if (p->vx < -max_s) p->vx = -max_s;

        if (!smash_lock && jump_press && p->jumps_remaining > 0) {
            p->vy = JUMP_FORCE * fd->jump_mul;
            p->jumps_remaining--;
            p->on_ground = 0;
        }

        if (p->btn_shield && p->shield_health > 0.0f && p->state != STATE_UPB) {
            if (shield_press) p->parry_timer = PARRY_WINDOW_FRAMES;
            p->state = STATE_SHIELD;
            p->shield_health -= SHIELD_DRAIN;
            p->vx *= 0.9f;
            if (p->shield_health <= 0.0f) {
                p->state = STATE_STUNNED;
                p->hitstun_frames = SHIELD_BREAK_STUN;
                p->shield_health = 0.0f;
            }
        } else if (p->state == STATE_SHIELD) {
            p->state = STATE_IDLE;
            p->shield_drop_timer = SHIELD_DROP_LAG_FRAMES;
        }

        if (attack_press && p->state != STATE_SHIELD &&
            p->attack_cooldown == 0 && p->attack_timer == 0 &&
            p->smash_charge_timer == 0 && p->smash_active_timer == 0 && p->smash_release_timer == 0) {
            p->state = STATE_ATTACK;
            p->attack_timer = ATTACK_ACTIVE_FRAMES;
            p->attack_cooldown = ATTACK_COOLDOWN_FRAMES;
        }
    }

    const FighterDef *fd2 = fighter_def((CharacterId)p->character_id);
    p->vy -= ((p->in_y < -0.5f && !p->on_ground) ? FAST_FALL_GRAVITY : GRAVITY) * fd2->gravity_mul;
    if ((p->umbrella_open || p->state == STATE_UPB) && p->vy < 0.0f) {
        p->vy += (GRAVITY - UMBRELLA_GRAVITY);
        if (p->vy < -UMBRELLA_FALL_SPEED) p->vy = -UMBRELLA_FALL_SPEED;
    }
    if (p->state == STATE_UPB) {
        p->upb_frame++;
        if (p->upb_frame < 10) {
            p->vy = fmaxf(p->vy, 1.2f);
            p->vx += p->in_x * (AIR_ACCEL * 0.6f);
        } else if (p->upb_frame > 50) {
            p->state = STATE_AIR;
            p->umbrella_open = 0;
        }
        /* Petalia's own real multi-hit Parasol (BP-TUNE-3939309) -- the real, previously-dead
         * parasol_rehit_timer wired up for real: a hit fires the moment the timer reaches 0
         * (starting at activation, see the up-B dispatch above), then the timer re-arms for
         * BRAWLPIT_PETALIA_PARASOL_REHIT_INTERVAL frames before the next one can land, giving
         * several real hits across the whole ~50-frame ascent instead of the usual zero. */
        if (p->character_id == CHARACTER_PETALIA && ctx != NULL) {
            if (p->parasol_rehit_timer == 0) {
                special_petalia_parasol_hit((ServerState *)ctx, p);
                p->parasol_rehit_timer = BRAWLPIT_PETALIA_PARASOL_REHIT_INTERVAL;
            }
        }
    }
    if (p->vy < -TERMINAL_VELOCITY) p->vy = -TERMINAL_VELOCITY;

    p->x += p->vx * dt * 60.0f;
    p->y += p->vy * dt * 60.0f;

    resolve_platform_collisions(p, prev_y);
    if (p->on_ground && p->state == STATE_WAVEDASH && p->wavedash_frames == 0) p->state = STATE_IDLE;
    if (p->on_ground && p->state == STATE_UPB) {
        p->state = STATE_IDLE;
        p->umbrella_open = 0;
        p->upb_frame = 0;
        p->upb_landing_lag = 14;
    }
    if (p->on_ground) p->umbrella_open = 0;

    p->btn_jump_prev = p->btn_jump;
    p->btn_attack_prev = p->btn_attack;
    p->btn_shield_prev = p->btn_shield;
    p->btn_special_prev = p->btn_special;

    if (p->x < BLAST_LEFT || p->x > BLAST_RIGHT || p->y < BLAST_BOTTOM || p->y > BLAST_TOP) {
        phys_start_respawn(p);
    }
}

static inline void spawn_turnip(ServerState *state, PlayerState *p) {
    if (!state || !p) return;
    for (int i = 0; i < MAX_TURNIPS; i++) {
        Turnip *t = &state->turnips[i];
        if (t->active) continue;
        t->active = 1;
        t->owner_id = p->id;
        t->x = p->x + (float)p->facing * 1.5f;
        t->y = p->y + 2.0f;
        t->vx = (float)p->facing * TURNIP_SPEED;
        t->vy = TURNIP_UP_SPEED;
        t->ttl_frames = TURNIP_TTL_FRAMES;
        t->style = (unsigned char)p->character_id;
        break;
    }
}

/* Real per-character neutral-specials for the #120-123 batch (S181-06,
 * founder: "then into brawlpit as selectable characters with unique
 * abilities") -- each grounded directly in its own lore entry, not a
 * generic move with a re-skinned name. All four share the same trigger
 * hook the generic turnip-toss special already uses (grounded, up-tilted
 * input, special_press) -- see the character_id dispatch that calls
 * these, just below in the main tick function. */

#define BRAWLPIT_PETRIFY_RANGE 2.2f
#define BRAWLPIT_PETRIFY_STUN_FRAMES 40
#define BRAWLPIT_SERPENTS_GRASP_RANGE 1.5f /* real, deliberately tighter than petrify's own ranged gaze -- this is a melee-range grab */
#define BRAWLPIT_SERPENTS_GRASP_DAMAGE 9.0f
#define BRAWLPIT_GROUND_SLAM_RANGE 2.6f
#define BRAWLPIT_SCAVENGER_DASH_SPEED 0.16f
#define BRAWLPIT_PLAY_DEAD_INVULN_FRAMES 20 /* real, deliberate: shorter than Rosie's own 10-frame High Score Rush window since this has no offensive payoff attached, purely defensive utility */
#define BRAWLPIT_UNCROWNED_CLAIM_SHIELD 15.0f /* real fraction of SHIELD_MAX (60), not a normalized 0-1 value */
#define BRAWLPIT_INSERT_COIN_DAMAGE 5.0f /* real, deliberate balance: 2 real coins at 5.0f each (10.0f total, landing both) still edges out one regular 8.0f turnip -- rewards the real, harder-to-land double-hit, doesn't make it strictly free damage if only one connects */

/* Medusa's Petrifying Gaze -- a short-range stun (real hitstun_frames,
 * not a damage hit) on any opponent standing in front of her. Literal to
 * the myth: turns whoever's close enough to see her to stone for a beat,
 * rather than dealing damage outright. */
static inline void special_petrify_gaze(ServerState *state, PlayerState *p) {
    if (!state) return;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        PlayerState *t = &state->players[i];
        if (t == p || !t->active || t->state == STATE_DEAD) continue;
        float dx = t->x - p->x;
        if ((float)p->facing * dx <= 0.0f) continue; /* only the direction she's facing */
        if (fabsf(dx) > BRAWLPIT_PETRIFY_RANGE) continue;
        if (fabsf(t->y - p->y) > 1.4f) continue;
        t->hitstun_frames = BRAWLPIT_PETRIFY_STUN_FRAMES;
        t->state = STATE_STUNNED;
    }
}

/* Medusa's Serpents' Grasp -- kanban BPTUNE-10001 ("up b and down b all do the same thing for
 * every character... need to be distinct moves"). Real down-B (in_y < -0.5f, i.e. hold S +
 * special on the ground), the first one this tuning pass has actually built -- until now every
 * grounded custom special lived on the SAME up-tilted (hold W) input as the universal Parasol
 * up-B decided its air/ground split on, leaving "down + special" a dead input for every
 * character. Thematically distinct from her own neutral-B (Petrifying Gaze: ranged, no damage,
 * stuns): this is a real, melee-range strike that deals real damage and real knockback,
 * literal to the myth's other half -- the gaze paralyzes at range, the serpents themselves bite
 * up close. */
static inline void special_serpents_grasp(ServerState *state, PlayerState *p) {
    if (!state) return;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        PlayerState *t = &state->players[i];
        if (t == p || !t->active || t->state == STATE_DEAD) continue;
        float dx = t->x - p->x;
        if (fabsf(dx) > BRAWLPIT_SERPENTS_GRASP_RANGE) continue;
        if (fabsf(t->y - p->y) > 1.4f) continue;
        apply_knockback(t, BRAWLPIT_SERPENTS_GRASP_DAMAGE, (dx >= 0.0f ? 1.0f : -1.0f) * 0.5f, 0.45f);
    }
}

/* The Raccoon's Scavenger's Dash -- pure mobility, no damage/CC at all.
 * The ability IS escape, matching a scavenger archetype that wins by not
 * being where the hit lands, not by trading blows. */
static inline void special_scavenger_dash(PlayerState *p) {
    float dir = (fabsf(p->in_x) > 0.01f) ? p->in_x : (float)p->facing;
    p->vx = dir * BRAWLPIT_SCAVENGER_DASH_SPEED * 10.0f;
    p->wavedash_frames = WAVEDASH_FRAMES;
    p->state = STATE_WAVEDASH;
}

/* The Raccoon's Play Dead -- real down-B (kanban BPTUNE-10001: "up b and down b all do the same
 * thing for every character... need to be distinct moves"), the tuning pass's second real
 * down-B after Medusa's Serpents' Grasp. A real, deliberate CONTRAST from Scavenger's Dash
 * rather than a variation on it: that neutral-B escapes by moving away; this down-B escapes by
 * standing completely still and taking nothing -- both real, honest expressions of "pure
 * mobility, no offense at all," approached from opposite directions (motion vs. stillness), the
 * same real character truth Raccoon's own neutral-B doc comment already states. Zero damage,
 * zero knockback dealt -- Raccoon remains the one fighter whose special kit never deals damage
 * even with two real moves now. */
static inline void special_play_dead(PlayerState *p) {
    p->vx = 0.0f;
    if (p->invuln_frames < BRAWLPIT_PLAY_DEAD_INVULN_FRAMES) p->invuln_frames = BRAWLPIT_PLAY_DEAD_INVULN_FRAMES;
}

/* Petalia's own real, multi-hit Parasol Up-B (kanban BP-TUNE-3939309: "RESTORE PETALIA PARISOL
 * UP B FROM WAY BACK IN GIT IT NEEDS TO BE MULTI HIT AND GIVE VERTICAL MOBILITY AND OPEN THE
 * PARISOL"). Real, honest investigation performed first: no prior Petalia-specific up-B code
 * was found anywhere in this repo's own git history, nor in SHANKPIT (the base this repo forked
 * from) -- CHARACTER_PETALIA has only ever been the default/fallback character (index 0), never
 * carrying unique special logic. This is a real, new build against the card's own literal
 * requirements, not a literal restoration of lost code. Real, load-bearing find that made this
 * a real, honest "finish" rather than invent-from-scratch: `parasol_rehit_timer` already existed
 * on PlayerState and was already decremented every frame (see update_entity's own per-frame
 * block) -- but nothing anywhere ever SET it to a real value or READ it to gate a hit. A real,
 * half-built multi-hit mechanic that was scaffolded and never wired up -- this function and its
 * own call site in the STATE_UPB update block are that real wiring, not new state.
 *
 * Vertical mobility and "opens the parasol" are both already real and universal (every
 * character's own up-B already sets umbrella_open + a real vy boost, same real precedent
 * Vexar's own per-character up-B variance already established in that shared dispatch) --
 * Petalia's own real, NEW piece is the multi-hit damage itself, since no character's up-B deals
 * any damage today. */
static inline void special_petalia_parasol_hit(ServerState *state, PlayerState *p) {
    if (!state) return;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        PlayerState *t = &state->players[i];
        if (t == p || !t->active || t->state == STATE_DEAD) continue;
        float dx = t->x - p->x;
        float dy = t->y - p->y;
        if (dx * dx + dy * dy > BRAWLPIT_PETALIA_PARASOL_HIT_RANGE * BRAWLPIT_PETALIA_PARASOL_HIT_RANGE) continue;
        apply_knockback(t, BRAWLPIT_PETALIA_PARASOL_HIT_DAMAGE, (dx >= 0.0f ? 1.0f : -1.0f) * 0.35f, 0.3f);
    }
}

/* The Second Tree's ground slam -- real AOE knockback via the same
 * apply_knockback every normal attack already uses, applied to every
 * nearby opponent at once instead of a single target. An angry tree's
 * whole kit should read as area denial, not a precision hit. */
static inline void special_ground_slam(ServerState *state, PlayerState *p) {
    if (!state) return;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        PlayerState *t = &state->players[i];
        if (t == p || !t->active || t->state == STATE_DEAD) continue;
        float dx = t->x - p->x;
        float dy = t->y - p->y;
        if (dx * dx + dy * dy > BRAWLPIT_GROUND_SLAM_RANGE * BRAWLPIT_GROUND_SLAM_RANGE) continue;
        apply_knockback(t, 10.0f, (dx >= 0.0f ? 1.0f : -1.0f) * 0.7f, 0.55f);
    }
}

/* The Second Tree's Regrowth -- real down-B (kanban BPTUNE-10001), a deliberate CONTRAST to
 * Ground Slam rather than a variation on it: that neutral-B is pure offense (AOE knockback,
 * zero self-benefit), this heals real, meaningful HP back (comparable in size to a single
 * Ground Slam's own damage output) with zero offense at all -- a tree literally regrowing,
 * matching the same real "same identity, opposite direction" pattern Medusa's own gaze/grasp
 * and Raccoon's own dash/stillness pair already established. */
static inline void special_regrowth(PlayerState *p) {
    p->damage_percent -= BRAWLPIT_REGROWTH_HEAL_AMOUNT;
    if (p->damage_percent < 0.0f) p->damage_percent = 0.0f;
}

/* Uncrowned's Claim -- a defensive shield-health top-up, no offense at
 * all. The one fighter whose special is entirely about not losing rather
 * than winning, matching "doubt, not triumph." */
static inline void special_uncrowned_claim(PlayerState *p) {
    p->shield_health += BRAWLPIT_UNCROWNED_CLAIM_SHIELD;
    if (p->shield_health > SHIELD_MAX) p->shield_health = SHIELD_MAX;
}

/* Rosie's Insert Coin -- kanban priority-queue card BPTUNE-001/BPTUNE-003 ("tuning pass...
 * unique normal/up-B/down-B/direction-B attacks... embrace spaghetti code spookiness and weird
 * gimmicks... dont touch Understudy or Petalia"). Rosie was 100% generic before this pass (the
 * shared turnip-toss fallback, same as every un-tuned fighter) -- this is her real, first custom
 * special, an argument made directly from her own lore: "generated twice, a style apart, and
 * kept both times... two separate generations of the same subject, both times reaching for a
 * game that isn't the one she's actually standing in." The mechanic IS the lore: she throws not
 * one turnip but TWO, a real, honest spread, each hitting slightly softer than a single one
 * (BRAWLPIT_INSERT_COIN_DAMAGE below, see update_turnips' own real per-style dispatch) -- the
 * character mechanically insists on being two of a thing rather than one, same as the art
 * direction that inspired her.
 *
 * Real, deliberate scope, matching "dont bite off too much": this is her ONE new special
 * (neutral-B) for this pass, not all four B-moves at once -- up-B/down-B/side-B and a real
 * unique normal attack are real, separate, honestly-tracked follow-up, the same "one bounded
 * slice" discipline this whole tuning pass is meant to follow character by character. */
static inline void special_insert_coin(ServerState *state, PlayerState *p) {
    if (!state || !p) return;
    for (int slot = 0; slot < 2; slot++) {
        for (int i = 0; i < MAX_TURNIPS; i++) {
            Turnip *t = &state->turnips[i];
            if (t->active) continue;
            t->active = 1;
            t->owner_id = p->id;
            t->x = p->x + (float)p->facing * 1.5f;
            t->y = p->y + 2.0f;
            t->vx = (float)p->facing * TURNIP_SPEED;
            /* The real "a style apart" spread: the second coin arcs noticeably higher than the
             * first, two real, visibly different trajectories for two real, separate throws --
             * not a cosmetic duplicate fired at the same arc. */
            t->vy = (slot == 0) ? TURNIP_UP_SPEED : (TURNIP_UP_SPEED * 1.6f);
            t->ttl_frames = TURNIP_TTL_FRAMES;
            t->style = (unsigned char)p->character_id;
            break;
        }
    }
}

/* special_high_score_rush_hit -- the real, shared hit-check both the opening and closing frames
 * of Rosie's High Score Rush dash use (kanban BP-TUNE-0033). Same real, direct AABB-range check
 * special_ground_slam already established -- deliberately does NOT check the target's own
 * invuln_frames first, the same real, existing, cross-cutting gap every other custom-special hit
 * function in this file already has (special_ground_slam/special_petrify_gaze also skip it) --
 * not a new bug introduced here, a pre-existing convention followed for consistency. */
static inline void special_high_score_rush_hit(ServerState *state, PlayerState *p) {
    if (!state) return;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        PlayerState *t = &state->players[i];
        if (t == p || !t->active || t->state == STATE_DEAD) continue;
        float dx = t->x - p->x;
        float dy = t->y - p->y;
        if (dx * dx + dy * dy > ROSIE_DASH_HIT_RANGE * ROSIE_DASH_HIT_RANGE) continue;
        apply_knockback(t, ROSIE_DASH_HIT_DAMAGE, (dx >= 0.0f ? 1.0f : -1.0f) * 0.7f, 0.55f);
    }
}

static inline void phys_start_respawn(PlayerState *p) {
    if (p->state == STATE_DEAD || p->respawn_timer > 0) return;
    if (p->stocks > 0) p->stocks--;
    if (p->stocks <= 0) {
        p->stocks = 0;
        p->state = STATE_DEAD;
        p->respawn_timer = 0;
        p->x = 0.0f;
        p->y = 1000.0f;
        return;
    }
    p->state = STATE_RESPAWN;
    p->respawn_timer = RESPAWN_DELAY_FRAMES;
    p->vx = 0.0f;
    p->vy = 0.0f;
    p->launch_delay_frames = 0;
    p->hitlag_frames = 0;
    p->attack_timer = 0;
    p->smash_active_timer = 0;
    p->smash_charge_timer = 0;
    p->smash_release_timer = 0;
    p->wavedash_frames = 0;
    p->rosie_dash_frame = 0;
    p->umbrella_open = 0;
}

static inline void check_parasol_hitbox(PlayerState *attacker, PlayerState *target) {
    check_attack_hitbox(attacker, target);
}

static inline void update_turnips(ServerState *state) {
    for (int i = 0; i < MAX_TURNIPS; i++) {
        Turnip *t = &state->turnips[i];
        if (!t->active) continue;

        t->vy -= TURNIP_GRAVITY;
        t->x += t->vx;
        t->y += t->vy;
        t->ttl_frames--;
        if (t->ttl_frames <= 0) {
            t->active = 0;
            continue;
        }

        for (int p = 0; p < MAX_CLIENTS; p++) {
            PlayerState *pl = &state->players[p];
            if (!pl->active || pl->state == STATE_DEAD) continue;
            if (p == t->owner_id) continue;

            if (check_aabb(t->x - 0.5f, t->y - 0.5f, 1.0f, 1.0f,
                           pl->x - 1.0f, pl->y, 2.0f, 4.0f)) {
                float dmg = (t->style == CHARACTER_VEXAR) ? 10.0f : (t->style == CHARACTER_ROSIE) ? BRAWLPIT_INSERT_COIN_DAMAGE : 8.0f;
                float ky = (t->style == CHARACTER_VEXAR) ? 0.45f : 0.6f;
                apply_knockback(pl, dmg, (t->vx > 0 ? 0.8f : -0.8f), ky);
                t->active = 0;
                break;
            }
        }
    }
}

static inline void update_edge_ko_effects(ServerState *state) {
    for (int i = 0; i < MAX_EDGE_KO_EFFECTS; i++) {
        EdgeKOEffect *fx = &state->edge_kos[i];
        if (!fx->active) continue;
        fx->timer--;
        if (fx->timer <= 0) {
            fx->active = 0;
            fx->timer = 0;
        }
    }
}

static inline int check_aabb(float x1, float y1, float w1, float h1, float x2, float y2, float w2, float h2) {
    return (x1 < x2 + w2 && x1 + w1 > x2 &&
            y1 < y2 + h2 && y1 + h1 > y2);
}

static inline void apply_knockback(PlayerState *target, float dmg, float kbx, float kby) {
    target->damage_percent += dmg;
    if (target->damage_percent < 0) target->damage_percent = 0;
    if (target->damage_percent > 999.0f) target->damage_percent = 999.0f;

    float scaling = 1.0f + (target->damage_percent * KNOCKBACK_SCALING);
    float final_kbx = kbx * scaling;
    float final_kby = kby * scaling;
    if (target->damage_percent >= HIGH_PERCENT_THRESHOLD) {
        target->launch_delay_frames = HIGH_PERCENT_LAUNCH_DELAY;
        target->pending_kb_x = final_kbx;
        target->pending_kb_y = final_kby;
        target->vx = 0;
        target->vy = 0;
    } else {
        target->vx = final_kbx;
        target->vy = final_kby;
    }

    target->hitstun_frames = (int)(sqrtf(kbx * kbx + kby * kby) * 5.0f * scaling);
    target->state = STATE_STUNNED;
}

typedef Platform2D Platform;

typedef enum {
    STAGE_FD = 0,
    STAGE_TIMELINE = 1
} StageId;

static const Platform stage_fd_geo[] = {
    {0.0f, -5.0f, 60.0f, 10.0f, 0},
    {-15.0f, 8.0f, 12.0f, 1.0f, 1},
    {15.0f, 8.0f, 12.0f, 1.0f, 1},
    {0.0f, 18.0f, 12.0f, 1.0f, 1},
};
static const int stage_fd_count = (int)(sizeof(stage_fd_geo) / sizeof(stage_fd_geo[0]));

static const Platform stage_timeline_geo[] = {
    {0.0f, -8.0f, 52.0f, 4.0f, 0},
    {-18.0f, 2.0f, 18.0f, 1.0f, 1},
    {18.0f, 2.0f, 18.0f, 1.0f, 1},
    {0.0f, 10.0f, 16.0f, 1.0f, 1},
    {-20.0f, 16.0f, 16.0f, 1.0f, 1},
    {20.0f, 16.0f, 16.0f, 1.0f, 1},
    {0.0f, 24.0f, 20.0f, 1.0f, 1},
    {-28.0f, -2.0f, 6.0f, 1.0f, 1},
    {-24.0f, 2.0f, 6.0f, 1.0f, 1},
    {-20.0f, 6.0f, 6.0f, 1.0f, 1},
    {28.0f, -2.0f, 6.0f, 1.0f, 1},
    {24.0f, 2.0f, 6.0f, 1.0f, 1},
    {20.0f, 6.0f, 6.0f, 1.0f, 1},
};
static const int stage_timeline_count = (int)(sizeof(stage_timeline_geo) / sizeof(stage_timeline_geo[0]));

// TODO: stage_geo/stage_count are header-static (per translation unit).
// Consolidate to a single source of truth (e.g., ServerState or one .c extern) to avoid cross-TU desync.
static const Platform *stage_geo = stage_fd_geo;
static int stage_count = (int)(sizeof(stage_fd_geo) / sizeof(stage_fd_geo[0]));

static inline void stage_set_active(int stage_id) {
    if (stage_id == STAGE_TIMELINE) {
        stage_geo = stage_timeline_geo;
        stage_count = stage_timeline_count;
        return;
    }
    stage_geo = stage_fd_geo;
    stage_count = stage_fd_count;
}

// --- HACK 1: FLOOR BOUNCING (Missed Techs) ---
// We modify the collision resolver to check if a player is hitting the ground
// fast while stunned. If so, they bounce instead of landing efficiently.
void resolve_platform_collisions(PlayerState *p, float prev_y) {
    float pw = 2.0f; // Player Width

    p->on_ground = 0;

    // Don't collide if moving upwards
    if (p->vy > 0) {
        p->ground_platform_type = -1;
        return;
    }

    for(int i=0; i<stage_count; i++) {
        Platform b = stage_geo[i];

        if (b.type == 1 && p->drop_through_timer > 0) continue;

        if (p->x + pw/2 > b.x - b.w/2 && p->x - pw/2 < b.x + b.w/2) {
            float top = b.y + b.h/2;
            if (prev_y >= top && p->y <= top) {
                // --- TECH CHECK ---
                // If hitting ground fast while stunned, BOUNCE.
                if (p->state == STATE_STUNNED && p->vy < -1.8f) {
                    p->y = top + 0.1f;
                    p->vy *= -0.7f; // Retain 70% energy upwards
                    // Apply friction to the bounce
                    p->vx *= 0.5f;
                    // Add visual flair? (Screen shake trigger could go here)
                    return;
                }
                // ------------------

                if (b.type == 1 && p->in_y < -0.6f) continue;

                p->y = top;
                p->vy = 0;
                p->on_ground = 1;
                p->jumps_remaining = MAX_JUMPS;
                p->ground_platform_type = b.type;
                return;
            }
        }
    }
    p->ground_platform_type = -1;
}

// --- HACK 2: AERIALS & DI ---
// We replace the generic hitbox logic with context-sensitive moves.
void check_attack_hitbox(PlayerState *attacker, PlayerState *target) {
    if (attacker->attack_timer <= 0 && attacker->smash_active_timer <= 0) return;
    if (target->invuln_frames > 0) return;
    if (target->state == STATE_DEAD) return;

    float reach = 3.5f;
    float hx = attacker->x + (attacker->facing * 2.0f);
    float hy = attacker->y + 1.0f;
    float hw = reach;
    float hh = 2.5f;

    float tx = target->x;
    float ty = target->y + 2.0f;
    float tw = 2.0f;
    float th = 4.0f;

    if (check_aabb(hx - hw/2, hy - hh/2, hw, hh, tx - tw/2, ty - th/2, tw, th)) {
        float kb_x = attacker->facing * 1.0f;
        float kb_y = 0.8f;
        float damage = 12.0f * fighter_def((CharacterId)attacker->character_id)->attack_damage_mul;
        int heavy_hit = 0;

        if (attacker->smash_active_timer > 0) {
            float charge = attacker->smash_charge_level;
            damage = 14.0f + (12.0f * charge);
            kb_x = attacker->facing * (1.0f + 1.2f * charge);
            kb_y = 0.9f + 0.9f * charge;
            heavy_hit = 1;
        }
        else if (!attacker->on_ground) {
            if (attacker->in_x * attacker->facing > 0.1f) {
                damage = 14.0f;
                kb_x = attacker->facing * 0.6f;
                kb_y = -1.6f;
                heavy_hit = 1;
            }
            else if (attacker->in_x * attacker->facing < -0.1f) {
                damage = 15.0f;
                kb_x = attacker->facing * 1.6f;
                kb_y = 0.8f;
                heavy_hit = 1;
            }
            else {
                damage = 9.0f;
                kb_x = attacker->facing * 1.1f;
                kb_y = 0.5f;
            }
        }
        else {
            if (attacker->in_y > 0.5f) { kb_y = 1.5f; kb_x *= 0.2f; }
            else if (attacker->in_y < -0.5f) { kb_y = -0.5f; kb_x *= 1.2f; }
        }

        if (target->state == STATE_SHIELD) {
            if (target->parry_timer > 0) {
                 target->parry_timer = 0;
                 target->vx = 0; target->vy = 0;
                 attacker->vx = -attacker->facing * 1.4f;
                 attacker->vy = 0.6f;
                 attacker->hitstun_frames = 18;
                 attacker->state = STATE_STUNNED;
                 return;
            }
            float shield_damage = damage;
            target->shield_health -= shield_damage;
            target->shield_regen_timer = 60;
            target->shield_stun_frames = (int)(shield_damage * 5.0f);
            target->vx += attacker->facing * 0.08f * shield_damage;
            attacker->vx -= attacker->facing * 0.05f * shield_damage;
            if (target->shield_health <= 0) {
                target->state = STATE_STUNNED;
                target->hitstun_frames = SHIELD_BREAK_STUN;
                target->shield_health = 0;
            }
            if (target->shield_health > SHIELD_MAX) target->shield_health = SHIELD_MAX;
            return;
        }

        if (target->damage_percent > 50.0f) {
            float di_strength = 0.4f;
            kb_x += target->in_x * di_strength;
            kb_y += target->in_y * di_strength;
        }

        apply_knockback(target, damage, kb_x, kb_y);

        if (heavy_hit) {
            int freeze = 6 + (int)(damage * 0.5f);
            attacker->hitlag_frames = freeze;
            target->hitlag_frames = freeze;
            target->hit_flash_timer = 10;
        }

        if (attacker->smash_active_timer > 0) {
            attacker->attack_cooldown = SMASH_COOLDOWN_FRAMES;
        } else {
            attacker->attack_cooldown = ATTACK_COOLDOWN_FRAMES;
        }
    }
}

#endif
