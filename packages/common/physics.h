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

        if (smash_possible && p->btn_special && p->smash_charge_timer == 0 &&
            p->smash_active_timer == 0 && p->smash_release_timer == 0 &&
            p->attack_timer == 0 && p->attack_cooldown == 0) {
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
            } else if (!p->on_ground) {
                p->umbrella_open = !p->umbrella_open;
            } else if (p->in_y > 0.5f && p->turnip_cooldown == 0 && ctx != NULL) {
                spawn_turnip((ServerState *)ctx, p);
                p->turnip_cooldown = (p->character_id == CHARACTER_VEXAR) ? (TURNIP_COOLDOWN_FRAMES - 10) : TURNIP_COOLDOWN_FRAMES;
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
                float dmg = (t->style == CHARACTER_VEXAR) ? 10.0f : 8.0f;
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
