#ifndef PHYSICS_H
#define PHYSICS_H
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include "protocol.h"

// --- BRAWLPIT: 2D Helpers ---
typedef struct { float x, y; } Vec2;

static inline void apply_friction_2d(Vec2 *vel, float friction_per_sec, float dt) {
    float vx = vel->x;
    float vy = vel->y;
    float speed_sq = vx * vx + vy * vy;
    if (speed_sq <= 1e-8f) {
        vel->x = 0.0f;
        vel->y = 0.0f;
        return;
    }

    float speed = sqrtf(speed_sq);
    float reduction = friction_per_sec * dt;
    float new_speed = speed - reduction;
    if (new_speed <= 0.0f) {
        vel->x = 0.0f;
        vel->y = 0.0f;
        return;
    }
    float scale = new_speed / speed;
    vel->x = vx * scale;
    vel->y = vy * scale;
}

// --- SMASH PHYSICS TUNING ---
#define GRAVITY 0.065f
#define FAST_FALL_GRAVITY 0.14f
#define TERMINAL_VELOCITY 2.5f

#define GROUND_ACCEL 0.25f
#define GROUND_FRICTION 0.2f
#define GROUND_MAX_SPEED 1.2f

#define AIR_ACCEL 0.08f
#define AIR_FRICTION 0.02f
#define AIR_MAX_SPEED 1.0f

#define DODGE_COOLDOWN_FRAMES 30
#define WAVEDASH_FRAMES 12
#define WAVEDASH_GROUND_SPEED 1.6f
#define WAVEDASH_AIR_BOOST 1.4f
#define WAVEDASH_DROP_VY 0.6f
#define WAVEDASH_MAX_SPEED 1.8f
#define DROP_THROUGH_FRAMES 10
#define RESPAWN_DELAY_FRAMES 90
#define RESPAWN_INVULN_FRAMES 120
#define EDGE_KO_FLASH_FRAMES 24
#define EDGE_KO_SPEED 3.8f
#define ATTACK_ACTIVE_FRAMES 30
#define ATTACK_COOLDOWN_FRAMES 20
#define TURNIP_COOLDOWN_FRAMES 45
#define TURNIP_TTL_FRAMES 240
#define TURNIP_SPEED 1.0f
#define TURNIP_UP_SPEED 1.1f
#define TURNIP_GRAVITY 0.04f
#define UMBRELLA_GRAVITY 0.02f
#define UMBRELLA_FALL_SPEED 0.6f

#define JUMP_FORCE 1.6f
#define SHORT_HOP_FORCE 0.9f
#define DOUBLE_JUMP_FORCE 1.4f

#define SHIELD_DRAIN 0.5f
#define SHIELD_REGEN 0.2f
#define SHIELD_STUN_BASE 20

#define HITSTUN_FACTOR 0.4f
#define KNOCKBACK_SCALING 0.04f

// --- STAGE GEOMETRY (2.5D) ---
// X, Y, W, H, Type (0=Solid, 1=Passthrough)
typedef struct { float x, y, w, h; int type; } Platform;

static Platform stage_geo[] = {
    {0.0f, -5.0f, 60.0f, 10.0f, 0},   // Main Stage (FD)
    {-15.0f, 8.0f, 12.0f, 1.0f, 1},   // Left Plat
    {15.0f, 8.0f, 12.0f, 1.0f, 1},    // Right Plat
    {0.0f, 18.0f, 12.0f, 1.0f, 1},    // Top Plat
};
static int stage_count = 4;

// Blast Zones
#define BLAST_LEFT -60.0f
#define BLAST_RIGHT 60.0f
#define BLAST_TOP 60.0f
#define BLAST_BOTTOM -40.0f

float phys_rand_f() { return ((float)(rand()%1000)/500.0f) - 1.0f; }

static inline void spawn_edge_ko_effect(ServerState *server, float x, float y, float speed) {
    if (!server) return;
    int slot = -1;
    for (int i = 0; i < MAX_EDGE_KO_EFFECTS; i++) {
        if (!server->edge_kos[i].active) {
            slot = i;
            break;
        }
    }
    if (slot == -1) slot = 0;
    EdgeKOEffect *fx = &server->edge_kos[slot];
    fx->active = 1;
    fx->x = x;
    fx->y = y;
    fx->intensity = fminf(speed / EDGE_KO_SPEED, 2.0f);
    fx->timer = EDGE_KO_FLASH_FRAMES;
}

static inline void update_edge_ko_effects(ServerState *server) {
    if (!server) return;
    for (int i = 0; i < MAX_EDGE_KO_EFFECTS; i++) {
        EdgeKOEffect *fx = &server->edge_kos[i];
        if (!fx->active) continue;
        fx->timer--;
        if (fx->timer <= 0) {
            fx->active = 0;
            fx->timer = 0;
        }
    }
}

// --- COLLISION ---
int check_aabb(float x1, float y1, float w1, float h1, float x2, float y2, float w2, float h2) {
    return (x1 < x2 + w2 && x1 + w1 > x2 &&
            y1 < y2 + h2 && y1 + h1 > y2);
}

void resolve_platform_collisions(PlayerState *p, float prev_y) {
    float pw = 2.0f; // Player Width
    float ph = 4.0f; // Player Height
    
    p->on_ground = 0;
    
    // Don't collide if moving upwards (pass through everything from bottom)
    if (p->vy > 0) {
        p->ground_platform_type = -1;
        return;
    }

    for(int i=0; i<stage_count; i++) {
        Platform b = stage_geo[i];

        if (b.type == 1 && p->drop_through_timer > 0) continue;

        // Simple AABB for feet
        if (p->x + pw/2 > b.x - b.w/2 && p->x - pw/2 < b.x + b.w/2) {
            // Check vertical overlap
            float top = b.y + b.h/2;
            if (prev_y >= top && p->y <= top) {
                // Landed
                // Passthrough check: if holding down, fall through passthroughs
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

void apply_knockback(PlayerState *target, float dmg, float kbx, float kby);

void update_turnips(ServerState *state) {
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

        for (int s = 0; s < stage_count; s++) {
            Platform b = stage_geo[s];
            if (t->x > b.x - b.w/2 && t->x < b.x + b.w/2) {
                float top = b.y + b.h/2;
                if (t->y <= top && t->y + t->vy >= top - b.h) {
                    t->y = top;
                    t->vy = 0;
                }
            }
        }

        for (int p = 0; p < MAX_CLIENTS; p++) {
            PlayerState *pl = &state->players[p];
            if (!pl->active || pl->state == STATE_DEAD) continue;
            if (p == t->owner_id) continue;

            float tw = 1.0f;
            float th = 1.0f;
            float pw = 2.0f;
            float ph = 4.0f;
            if (check_aabb(t->x - tw/2, t->y - th/2, tw, th,
                           pl->x - pw/2, pl->y, pw, ph)) {
                apply_knockback(pl, 8.0f, (t->vx > 0 ? 0.8f : -0.8f), 0.6f);
                t->active = 0;
                break;
            }
        }
    }
}

void spawn_turnip(ServerState *state, PlayerState *p) {
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
        break;
    }
}

void apply_knockback(PlayerState *target, float dmg, float kbx, float kby) {
    target->damage_percent += dmg;
    if (target->damage_percent < 0) target->damage_percent = 0;
    if (target->damage_percent > 999.0f) target->damage_percent = 999.0f;
    
    // Smash Knockback Formula (Simplified)
    float scaling = 1.0f + (target->damage_percent * KNOCKBACK_SCALING);
    target->vx = kbx * scaling;
    target->vy = kby * scaling;
    
    target->hitstun_frames = (int)(sqrtf(kbx*kbx + kby*kby) * 5.0f * scaling);
    target->state = STATE_STUNNED;
}

void check_attack_hitbox(PlayerState *attacker, PlayerState *target) {
    if (attacker->attack_cooldown > 0) return;
    if (target->invuln_frames > 0) return;
    if (target->state == STATE_DEAD) return;

    // Hitbox definition (Front of player)
    float reach = 3.5f;
    float hx = attacker->x + (attacker->facing * 2.0f);
    float hy = attacker->y + 1.0f;
    float hw = reach; 
    float hh = 2.5f;

    // Target Hurtbox
    float tx = target->x; 
    float ty = target->y + 2.0f;
    float tw = 2.0f; 
    float th = 4.0f;

    if (check_aabb(hx - hw/2, hy - hh/2, hw, hh, tx - tw/2, ty - th/2, tw, th)) {
        // HIT CONFIRMED
        // Directional Influence could go here
        
        float kb_x = attacker->facing * 1.0f;
        float kb_y = 0.8f;
        
        if (attacker->in_y > 0.5f) { kb_y = 1.5f; kb_x *= 0.2f; } // Up Tilt
        else if (attacker->in_y < -0.5f) { kb_y = -0.5f; kb_x *= 1.2f; } // Down Smash
        
        // Base damage
        float damage = 12.0f;
        
        // Shield Check
        if (target->state == STATE_SHIELD) {
            target->shield_health -= damage * 1.5f;
            target->shield_regen_timer = 60;
            if (target->shield_health <= 0) {
                target->state = STATE_STUNNED;
                target->hitstun_frames = 120; // SHIELD BREAK
                target->shield_health = 0;
            }
            if (target->shield_health > SHIELD_MAX) target->shield_health = SHIELD_MAX;
            return; 
        }

        apply_knockback(target, damage, kb_x, kb_y);
        attacker->attack_cooldown = ATTACK_COOLDOWN_FRAMES; // Hitlag/Recovery
    }
}

void phys_respawn(PlayerState *p, unsigned int now) {
    (void)now;
    if (p->stocks <= 0) {
        p->state = STATE_DEAD;
        p->x = 0; p->y = 1000; // Skybox
        return;
    }
    p->state = STATE_IDLE;
    p->damage_percent = 0;
    p->x = 0; p->y = 30; // Drop from top
    p->vx = 0; p->vy = 0;
    p->in_x = 0; p->in_y = 0;
    p->btn_jump = 0; p->btn_attack = 0; p->btn_shield = 0; p->btn_special = 0;
    p->attack_cooldown = 0;
    p->attack_timer = 0;
    p->hitstun_frames = 0;
    p->invuln_frames = RESPAWN_INVULN_FRAMES;
    p->shield_health = SHIELD_MAX;
    p->shield_regen_timer = 0;
    p->jumps_remaining = MAX_JUMPS;
    p->on_ground = 0;
    p->ground_platform_type = -1;
    p->drop_through_timer = 0;
    p->respawn_timer = 0;
    p->umbrella_open = 0;
    p->turnip_cooldown = 0;
    p->special_prev = 0;
}

void phys_start_respawn(PlayerState *p) {
    if (p->state == STATE_DEAD || p->respawn_timer > 0) return;
    if (p->stocks <= 0) {
        p->state = STATE_DEAD;
        p->x = 0; p->y = 1000;
        return;
    }
    p->stocks--;
    if (p->stocks <= 0) {
        p->stocks = 0;
        p->state = STATE_DEAD;
        p->respawn_timer = 0;
        p->x = 0; p->y = 1000;
        return;
    }
    p->state = STATE_RESPAWN;
    p->respawn_timer = RESPAWN_DELAY_FRAMES;
    p->vx = 0; p->vy = 0;
    p->in_x = 0; p->in_y = 0;
    p->btn_jump = 0; p->btn_attack = 0; p->btn_shield = 0; p->btn_special = 0;
    p->attack_cooldown = 0;
    p->attack_timer = 0;
    p->hitstun_frames = 0;
    p->x = 0; p->y = 1000;
    p->on_ground = 0;
    p->ground_platform_type = -1;
    p->drop_through_timer = 0;
    p->wavedash_frames = 0;
    p->dodge_cooldown = 0;
}

void update_entity(PlayerState *p, float dt, void *ctx, unsigned int time) {
    if (p->state == STATE_DEAD) return;
    float prev_x = p->x;
    float prev_y = p->y;

    // --- TIMERS ---
    if (p->invuln_frames > 0) p->invuln_frames--;
    if (p->respawn_timer > 0) {
        p->respawn_timer--;
        if (p->respawn_timer == 0) {
            phys_respawn(p, time);
        }
        return;
    }
    if (p->hitstun_frames > 0) {
        p->hitstun_frames--;
        if (p->hitstun_frames <= 0) p->state = STATE_IDLE;
    }
    if (p->attack_cooldown > 0) p->attack_cooldown--;
    if (p->attack_timer > 0) {
        p->attack_timer--;
        if (p->attack_timer == 0 && p->state == STATE_ATTACK) {
            p->state = STATE_IDLE;
        }
    }
    if (p->shield_regen_timer > 0) p->shield_regen_timer--;
    else if (p->shield_health < SHIELD_MAX && p->state != STATE_SHIELD) {
        p->shield_health += SHIELD_REGEN;
        if (p->shield_health > SHIELD_MAX) p->shield_health = SHIELD_MAX;
    }
    if (p->drop_through_timer > 0) p->drop_through_timer--;
    if (p->wavedash_frames > 0) p->wavedash_frames--;
    if (p->dodge_cooldown > 0) p->dodge_cooldown--;
    if (p->turnip_cooldown > 0) p->turnip_cooldown--;

    // --- INPUT PROCESSING (Physics) ---
    if (p->state != STATE_STUNNED) {
        if (p->on_ground && p->ground_platform_type == 1 && p->in_y < -0.6f) {
            p->drop_through_timer = DROP_THROUGH_FRAMES;
            p->on_ground = 0;
            p->vy = -0.2f;
            p->ground_platform_type = -1;
        }

        int special_edge = p->btn_special && !p->special_prev;
        if (special_edge) {
            if (!p->on_ground) {
                p->umbrella_open = !p->umbrella_open;
            } else if (p->in_y > 0.5f && p->turnip_cooldown == 0 && ctx != NULL) {
                spawn_turnip((ServerState *)ctx, p);
                p->turnip_cooldown = TURNIP_COOLDOWN_FRAMES;
            } else if (p->dodge_cooldown == 0) {
                float dir = (p->in_x != 0.0f) ? p->in_x : (float)p->facing;
                p->vx = dir * WAVEDASH_GROUND_SPEED;
                p->wavedash_frames = WAVEDASH_FRAMES;
                p->state = STATE_WAVEDASH;
                p->dodge_cooldown = DODGE_COOLDOWN_FRAMES;
            }
        }

        // Movement
        float accel = p->on_ground ? GROUND_ACCEL : AIR_ACCEL;
        float max_s = p->on_ground ? GROUND_MAX_SPEED : AIR_MAX_SPEED;
        float fric  = p->on_ground ? GROUND_FRICTION : AIR_FRICTION;

        if (p->wavedash_frames > 0 && p->on_ground) {
            fric = 0.0f;
            if (max_s < WAVEDASH_MAX_SPEED) max_s = WAVEDASH_MAX_SPEED;
        }

        if (p->in_x != 0) {
            p->vx += p->in_x * accel;
            p->facing = (p->in_x > 0) ? 1 : -1;
        } else {
            // Friction
            if (fabs(p->vx) < fric) p->vx = 0;
            else p->vx -= (p->vx > 0 ? 1 : -1) * fric;
        }

        // Cap Speed
        if (p->vx > max_s) p->vx = max_s;
        if (p->vx < -max_s) p->vx = -max_s;

        // Jump
        if (p->btn_jump && p->jumps_remaining > 0) {
            p->vy = JUMP_FORCE;
            p->jumps_remaining--;
            p->on_ground = 0;
            p->btn_jump = 0; // Consume input
            if (p->state == STATE_WAVEDASH) p->state = STATE_AIR;
        }

        // Shield
        if (p->btn_shield && p->shield_health > 0) {
            p->state = STATE_SHIELD;
            p->shield_health -= SHIELD_DRAIN;
            p->vx *= 0.9f; // Slow down in shield
            if (p->shield_health <= 0) {
                p->state = STATE_STUNNED;
                p->hitstun_frames = 60;
            }
        } else if (p->state == STATE_SHIELD) {
            p->state = STATE_IDLE;
        }

        // Attack
        if (p->btn_attack && p->attack_cooldown == 0) {
            p->state = STATE_ATTACK;
            p->attack_timer = ATTACK_ACTIVE_FRAMES;
            p->attack_cooldown = ATTACK_COOLDOWN_FRAMES;
            // Check hits against all others (naive O(N^2) but fine for N=8)
            // In a real loop we'd pass the list of targets
        }
    }

    // --- INTEGRATION ---
    p->vy -= (p->in_y < -0.5f && !p->on_ground) ? FAST_FALL_GRAVITY : GRAVITY;
    if (p->umbrella_open && p->vy < 0.0f) {
        p->vy += (GRAVITY - UMBRELLA_GRAVITY);
        if (p->vy < -UMBRELLA_FALL_SPEED) p->vy = -UMBRELLA_FALL_SPEED;
    }
    if (p->vy < -TERMINAL_VELOCITY) p->vy = -TERMINAL_VELOCITY;

    p->x += p->vx;
    p->y += p->vy;

    resolve_platform_collisions(p, prev_y);
    if (p->on_ground && p->state == STATE_WAVEDASH && p->wavedash_frames == 0) {
        p->state = STATE_IDLE;
    }
    if (p->on_ground) p->umbrella_open = 0;
    p->special_prev = p->btn_special;
    if (p->invuln_frames > 0) {
        if (p->x < BLAST_LEFT) { p->x = BLAST_LEFT + 1.0f; p->vx = 0; }
        if (p->x > BLAST_RIGHT) { p->x = BLAST_RIGHT - 1.0f; p->vx = 0; }
        if (p->y < BLAST_BOTTOM) { p->y = BLAST_BOTTOM + 1.0f; p->vy = 0; }
        if (p->y > BLAST_TOP) { p->y = BLAST_TOP - 1.0f; p->vy = 0; }
    }

    // --- BLAST ZONES ---
    if (p->invuln_frames == 0 &&
        (p->x < BLAST_LEFT || p->x > BLAST_RIGHT || p->y < BLAST_BOTTOM || p->y > BLAST_TOP)) {
        float speed = sqrtf((p->vx * p->vx) + (p->vy * p->vy));
        if (speed >= EDGE_KO_SPEED) {
            float impact_x = fminf(fmaxf(prev_x, BLAST_LEFT), BLAST_RIGHT);
            float impact_y = fminf(fmaxf(prev_y, BLAST_BOTTOM), BLAST_TOP);
            spawn_edge_ko_effect((ServerState *)ctx, impact_x, impact_y, speed);
        }
        phys_start_respawn(p);
    }
}

// History stuff kept for netcode compilation
void phys_store_history(ServerState *server, int client_id, unsigned int now) {
    if (client_id < 0 || client_id >= MAX_CLIENTS) return;
    int slot = (now / 16) % LAG_HISTORY; 
    server->history[client_id][slot].active = 1;
    server->history[client_id][slot].timestamp = now;
    server->history[client_id][slot].x = server->players[client_id].x;
    server->history[client_id][slot].y = server->players[client_id].y;
}
#endif
