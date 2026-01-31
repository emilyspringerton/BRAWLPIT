#ifndef LOCAL_GAME_H
#define LOCAL_GAME_H

#include "../common/protocol.h"
#include "../common/physics.h"
#include <string.h>

ServerState local_state;

// Bot Logic: Simple chase and smash
void bot_think(int id, PlayerState *players) {
    PlayerState *me = &players[id];
    if (me->state == STATE_DEAD || me->state == STATE_STUNNED) return;

    // Find nearest target
    int target = -1;
    float min_d = 9999.0f;
    for (int i=0; i<MAX_CLIENTS; i++) {
        if (i==id || !players[i].active || players[i].state == STATE_DEAD) continue;
        float d = fabs(players[i].x - me->x);
        if (d < min_d) { min_d = d; target = i; }
    }

    if (target != -1) {
        float dx = players[target].x - me->x;
        float dy = players[target].y - me->y;

        // Move towards
        me->in_x = (dx > 0) ? 1.0f : -1.0f;
        
        // Jump if target is higher or need recovery
        if (dy > 5.0f || me->y < 0) {
             if (rand()%100 < 5) me->btn_jump = 1;
        }

        // Attack if close
        if (fabs(dx) < 4.0f && fabs(dy) < 3.0f) {
            if (rand()%100 < 10) me->btn_attack = 1;
        }
    } else {
        // Recover to center
        if (me->x < -5.0f) me->in_x = 1.0f;
        else if (me->x > 5.0f) me->in_x = -1.0f;
        else me->in_x = 0;
    }
}

void local_update(float sx, float sy, int jump, int attack, int shield, int special, void *ctx, unsigned int cmd_time) {
    PlayerState *p0 = &local_state.players[0];
    
    // Map inputs
    p0->in_x = sx;
    p0->in_y = sy;
    
    // Simple edge trigger for jump (prevent hold-to-fly)
    static int last_jump = 0;
    if (jump && !last_jump) p0->btn_jump = 1;
    else p0->btn_jump = 0;
    last_jump = jump;

    p0->btn_attack = attack;
    p0->btn_shield = shield;
    p0->btn_special = special;

    // Simulation Loop
    for(int i=0; i<MAX_CLIENTS; i++) {
        PlayerState *p = &local_state.players[i];
        if (!p->active) continue;
        
        if (i > 0 && p->is_bot) bot_think(i, local_state.players);

        // Resolve Attacks (Attackers vs All)
        if (p->btn_attack && p->attack_cooldown == 0 && p->state != STATE_STUNNED) {
            for (int j=0; j<MAX_CLIENTS; j++) {
                if (i==j) continue;
                check_attack_hitbox(p, &local_state.players[j]);
            }
        }

        update_entity(p, 0.016f, ctx, cmd_time);
    }
}

void local_init_match(int num_players, int mode) {
    memset(&local_state, 0, sizeof(ServerState));
    local_state.game_mode = mode;
    
    // Initialize Players
    for(int i=0; i<num_players; i++) {
        local_state.players[i].active = 1;
        local_state.players[i].id = i;
        local_state.players[i].stocks = STOCK_COUNT;
        local_state.players[i].shield_health = SHIELD_MAX;
        local_state.players[i].is_bot = (i > 0);
        local_state.players[i].ground_platform_type = -1;
        phys_respawn(&local_state.players[i], 0);
        
        // Spread out spawns
        local_state.players[i].x = (i % 2 == 0) ? -10.0f : 10.0f;
    }
}

#endif
