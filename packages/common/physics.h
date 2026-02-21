#ifndef BRAWLPIT_PHYSICS_H
#define BRAWLPIT_PHYSICS_H

#include <math.h>
#include "protocol.h"

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
    p->hitstun_frames = 0;
    p->attack_cooldown = 0;
    p->attack_timer = 0;
    p->smash_active_timer = 0;
    p->smash_charge_timer = 0;
    p->smash_release_timer = 0;
    p->smash_charge_level = 0.0f;
    p->shield_stun_frames = 0;
    p->shield_drop_timer = 0;
    p->shield_regen_timer = 0;
    p->shield_health = SHIELD_MAX;
    p->invuln_frames = 120;
    p->respawn_timer = 0;
    p->launch_delay_frames = 0;
    p->pending_kb_x = 0.0f;
    p->pending_kb_y = 0.0f;
    p->ground_platform_type = -1;
    p->jumps_remaining = MAX_JUMPS;
}

static inline void update_entity(PlayerState *p, float dt, void *ctx, unsigned int time) {
    (void)ctx;
    (void)time;
    if (p->state == STATE_DEAD || p->respawn_timer > 0) return;

    if (p->hitstun_frames > 0) p->hitstun_frames--;
    if (p->attack_cooldown > 0) p->attack_cooldown--;
    if (p->attack_timer > 0) p->attack_timer--;
    if (p->smash_active_timer > 0) p->smash_active_timer--;
    if (p->shield_stun_frames > 0) p->shield_stun_frames--;
    if (p->shield_drop_timer > 0) p->shield_drop_timer--;
    if (p->invuln_frames > 0) p->invuln_frames--;
    if (p->turnip_cooldown > 0) p->turnip_cooldown--;

    p->x += p->vx * dt * 60.0f;
    p->y += p->vy * dt * 60.0f;

    if (p->x < BLAST_LEFT || p->x > BLAST_RIGHT || p->y < BLAST_BOTTOM || p->y > BLAST_TOP) {
        if (p->stocks > 0) p->stocks--;
        phys_respawn(p, time);
    }
}

void check_attack_hitbox(PlayerState *attacker, PlayerState *target);

static inline void check_parasol_hitbox(PlayerState *attacker, PlayerState *target) {
    check_attack_hitbox(attacker, target);
}

static inline void update_turnips(ServerState *state) {
    (void)state;
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
        float damage = 12.0f;
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
