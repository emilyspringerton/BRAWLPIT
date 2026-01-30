/* tests/test_physics.c */
#include <stdio.h>
#include <math.h>

/* Defines to make physics.h compile standalone */
#define MAX_CLIENTS 8
#define MAX_PROJECTILES 64
#define LAG_HISTORY 64
#define STATE_DEAD 1

typedef struct {
    unsigned char type; unsigned char client_id; unsigned short sequence; unsigned int timestamp; unsigned char entity_count;
} NetHeader;
typedef struct { unsigned int sequence; unsigned int timestamp; unsigned short msec; float stick_x; float stick_y; unsigned int buttons; int weapon_idx; } UserCmd;
typedef struct { int active; int is_bot; float x, y, vx, vy; int on_ground; int facing; float in_x, in_y; int btn_jump, btn_attack, btn_shield; int state; float damage_percent; int stocks; float shield_health; int shield_regen_timer; int jumps_remaining; int hitstun_frames; int attack_cooldown; int invuln_frames; int respawn_timer; int kills; int deaths; unsigned int last_hit_time; } PlayerState;
typedef struct { int active; unsigned int timestamp; float x, y; float vx, vy; } LagRecord;
typedef struct { PlayerState players[MAX_CLIENTS]; LagRecord history[MAX_CLIENTS][LAG_HISTORY]; int server_tick; int game_mode; int client_active[MAX_CLIENTS]; } ServerState;

#include "../packages/common/physics.h"

int main() {
    printf("BRAWLPIT Phase 1 Physics Smoke Test\n");
    Vec2 vel = {10.0f, 0.0f};
    float dt = 1.0f / 60.0f;
    float friction = 0.5f;
    
    printf("Frame | Vx      | Vy\n");
    for(int i = 0; i < 60; i++) {
        apply_friction_2d(&vel, friction, dt);
        if (i % 10 == 0) printf("%5d | %7.4f | %7.4f\n", i, vel.x, vel.y);
    }
    
    if (vel.x < 10.0f && vel.x > 0.0f) {
        printf("\n✅ PASS: Friction applied correctly\n");
        return 0;
    }
    return 1;
}
