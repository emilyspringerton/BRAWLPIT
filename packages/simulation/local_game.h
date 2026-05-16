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
    me->btn_jump = 0;
    me->btn_attack = 0;
    me->btn_shield = 0;
    me->btn_special = 0;

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
    void *sim_ctx = ctx ? ctx : &local_state;

    if (local_state.match_over) {
        update_edge_ko_effects(&local_state);
        return;
    }
    
    // Map inputs
    p0->in_x = sx;
    p0->in_y = sy;
    
    p0->btn_jump = jump ? 1 : 0;
    p0->btn_attack = attack;
    p0->btn_shield = shield;
    p0->btn_special = special;

    // Simulation Loop
    for(int i=0; i<MAX_CLIENTS; i++) {
        PlayerState *p = &local_state.players[i];
        if (!p->active) continue;
        
        if (i > 0 && p->is_bot) {
            bot_think(i, local_state.players);
            if (p->character == CHAR_SAMUS) {
                PlayerState *foe = &local_state.players[0];
                if (p->charge_shot_level < 140 && fabsf(foe->x - p->x) > 8.0f) p->btn_special = 1;
                else if (p->charge_shot_level >= 120 && fabsf(foe->x - p->x) < 14.0f) p->btn_special = 0;
            }
        }

        // Resolve Attacks (Attackers vs All)
        if (p->state == STATE_ATTACK && (p->attack_timer > 0 || p->smash_active_timer > 0) && p->state != STATE_STUNNED) {
            for (int j=0; j<MAX_CLIENTS; j++) {
                if (i==j) continue;
                check_attack_hitbox(p, &local_state.players[j]);
            }
        }
        if (p->state == STATE_UPB && p->state != STATE_STUNNED) {
            for (int j=0; j<MAX_CLIENTS; j++) {
                if (i==j) continue;
                check_parasol_hitbox(p, &local_state.players[j]);
            }
        }

        update_entity(p, 0.016f, sim_ctx, cmd_time);
    }

    update_turnips(&local_state);
    update_edge_ko_effects(&local_state);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        PlayerState *p = &local_state.players[i];
        if (p->active && p->stocks == 0 && p->state == STATE_DEAD) {
            local_state.match_over = 1;
            break;
        }
    }
}

void local_init_match(int num_players, int mode, int stage_id, int p0_char, int p1_char) {
    memset(&local_state, 0, sizeof(ServerState));
    stage_set_active(stage_id);
    local_state.game_mode = mode;
    local_state.match_over = 0;
    
    // Initialize Players
    for(int i=0; i<num_players; i++) {
        local_state.players[i].active = 1;
        local_state.players[i].id = i;
        local_state.players[i].stocks = STOCK_COUNT;
        local_state.players[i].shield_health = SHIELD_MAX;
        local_state.players[i].is_bot = (i > 0);
        local_state.players[i].character = (i==0)?p0_char:((i==1)?p1_char:CHAR_DEFAULT);
        local_state.players[i].ground_platform_type = -1;
        phys_respawn(&local_state.players[i], 0);
        
        // Spread out spawns
        local_state.players[i].x = (i % 2 == 0) ? -10.0f : 10.0f;
    }
}

#endif
