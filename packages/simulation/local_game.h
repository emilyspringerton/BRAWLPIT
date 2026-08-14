#ifndef LOCAL_GAME_H
#define LOCAL_GAME_H

#include "../common/protocol.h"
#include "../common/physics.h"
#include "../common/characters.h"
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

/* local_set_player_input: TIPJAR Step 3 (2026-08-14) -- real second-local-player input path.
 * local_update itself only ever mapped raw input onto players[0]; every other active slot fell
 * through to bot_think (or, for a non-bot slot, sat with stale/zeroed buttons -- nothing else
 * ever wrote them). Split out so a caller can feed a second real human's input onto players[1]
 * (or any slot) BEFORE calling local_update, which still runs the shared simulation tick for
 * every active player each frame -- as long as that slot's is_bot is cleared, bot_think skips it
 * (`if (i > 0 && p->is_bot)`) and the freshly-set real input drives it instead. Zero change to
 * local_update's own signature or behavior for player 0 -- existing call sites are unaffected. */
void local_set_player_input(int player_id, float sx, float sy, int jump, int attack, int shield, int special) {
    if (player_id < 0 || player_id >= MAX_CLIENTS) return;
    PlayerState *p = &local_state.players[player_id];
    p->in_x = sx;
    p->in_y = sy;
    p->btn_jump = jump ? 1 : 0;
    p->btn_attack = attack;
    p->btn_shield = shield;
    p->btn_special = special;
}

void local_update(float sx, float sy, int jump, int attack, int shield, int special, void *ctx, unsigned int cmd_time) {
    void *sim_ctx = ctx ? ctx : &local_state;

    if (local_state.match_over) {
        update_edge_ko_effects(&local_state);
        return;
    }

    local_set_player_input(0, sx, sy, jump, attack, shield, special);

    // Simulation Loop
    for(int i=0; i<MAX_CLIENTS; i++) {
        PlayerState *p = &local_state.players[i];
        if (!p->active) continue;
        
        if (i > 0 && p->is_bot) bot_think(i, local_state.players);

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

void local_init_match(int num_players, int mode, int stage_id, CharacterId p0_char, CharacterId p1_char) {
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
        local_state.players[i].ground_platform_type = -1;
        local_state.players[i].character_id = (i == 0) ? p0_char : p1_char;
        phys_respawn(&local_state.players[i], 0);
        
        // Spread out spawns
        local_state.players[i].x = (i % 2 == 0) ? -10.0f : 10.0f;
    }
}

#endif
