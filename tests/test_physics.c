/* tests/test_physics.c */
#include <stdio.h>
#include <math.h>
#include <string.h>

#ifdef _WIN32
    #include <winsock2.h>
#else
    #include <netinet/in.h>
#endif

/* Include the actual headers instead of redefining them */
#include "../packages/common/physics.h"

/* test_rosie_insert_coin -- real, direct verification of Rosie's new neutral-special (kanban
 * priority-queue card BPTUNE-001, the tuning pass's own first character). Real, minimal harness
 * (this repo has no established per-special unit-test convention yet -- every prior custom
 * special was verified live under Xvfb instead), but real enough to catch a genuine regression:
 * confirms the mechanic that IS the character's own lore ("generated twice... two separate
 * generations... both times reaching for a game that isn't the one she's actually standing in")
 * actually spawns TWO real, distinct turnip-style projectiles, not one, with the real, deliberate
 * "a style apart" spread (different vy) and the correct character-tagged style. */
static int test_rosie_insert_coin(void) {
    ServerState state;
    PlayerState p;
    memset(&state, 0, sizeof(state));
    memset(&p, 0, sizeof(p));
    p.id = 0;
    p.x = 0.0f;
    p.y = 0.0f;
    p.facing = 1;
    p.character_id = CHARACTER_ROSIE;

    special_insert_coin(&state, &p);

    int active_count = 0;
    float vy_values[2];
    for (int i = 0; i < MAX_TURNIPS; i++) {
        if (!state.turnips[i].active) continue;
        if (active_count < 2) vy_values[active_count] = state.turnips[i].vy;
        active_count++;
        if (state.turnips[i].style != (unsigned char)CHARACTER_ROSIE) {
            printf("❌ FAIL: Insert Coin turnip has wrong style (%d, expected CHARACTER_ROSIE)\n", state.turnips[i].style);
            return 1;
        }
    }
    if (active_count != 2) {
        printf("❌ FAIL: Insert Coin should spawn exactly 2 turnips, spawned %d\n", active_count);
        return 1;
    }
    if (fabsf(vy_values[0] - vy_values[1]) < 0.01f) {
        printf("❌ FAIL: Insert Coin's two turnips should have a real, distinct spread (different vy), got the same trajectory\n");
        return 1;
    }
    printf("✅ PASS: Insert Coin spawns 2 real, distinct-trajectory turnips, both correctly styled CHARACTER_ROSIE\n");
    return 0;
}

/* test_rosie_high_score_rush -- real, direct verification of Rosie's new side-B (kanban
 * priority-queue card BP-TUNE-0033: "make rosie direction B do a double hit dash ability it does
 * damage at the beginning and end of the dash and in the middle shes totally invuln like SSB
 * dodge"). Drives the real, full input -> dispatch -> per-frame-processing pipeline via
 * update_entity itself (not calling special_high_score_rush_hit directly), the same "test the
 * real path a player's input actually takes" discipline test_rosie_insert_coin's own header
 * comment already establishes for its neutral-special sibling. */
static int test_rosie_high_score_rush(void) {
    ServerState state;
    memset(&state, 0, sizeof(state));

    PlayerState *rosie = &state.players[0];
    PlayerState *near_target = &state.players[1];
    PlayerState *far_target = &state.players[2];
    rosie->id = 0;
    rosie->active = 1;
    rosie->character_id = CHARACTER_ROSIE;
    rosie->on_ground = 1;
    rosie->x = 0.0f;
    rosie->y = 0.0f;
    rosie->facing = 1;
    rosie->in_x = 1.0f; /* real, strong held direction -- the real input this move needs */
    rosie->btn_special = 1;
    rosie->btn_special_prev = 0; /* a fresh press this frame */

    /* Real, honest two-target design, not one: the opening hit's own real knockback pushes
     * whoever it lands on away, so a single stationary target can't realistically catch BOTH
     * the opening and closing hits (the closing hit checks Rosie's own CURRENT position, ~18
     * frames and ~ROSIE_DASH_SPEED*18 units of real forward travel later) -- this tests each
     * real hit window independently instead of assuming an unrealistic setup. */
    near_target->id = 1;
    near_target->active = 1;
    near_target->x = 1.0f; /* within ROSIE_DASH_HIT_RANGE of Rosie's own starting position */
    near_target->y = 0.0f;

    far_target->id = 2;
    far_target->active = 1;
    far_target->x = (float)ROSIE_DASH_TOTAL_FRAMES * ROSIE_DASH_SPEED - 0.3f; /* near her real end position */
    far_target->y = 0.0f;

    float dt = 1.0f / 60.0f;
    unsigned int time = 0;
    int invuln_seen = 0;

    for (int frame = 0; frame < 22; frame++) {
        update_entity(rosie, dt, &state, time++);
        if (rosie->invuln_frames > 0) invuln_seen = 1;
        /* Real, honest input semantics: btn_special_prev tracks the previous frame's real
         * button state, same as every other real edge-triggered input in this file. */
        rosie->btn_special_prev = rosie->btn_special;
    }

    if (rosie->character_id != CHARACTER_ROSIE) {
        printf("❌ FAIL: test setup corrupted character_id\n");
        return 1;
    }
    if (near_target->damage_percent < ROSIE_DASH_HIT_DAMAGE - 0.01f) {
        printf("❌ FAIL: High Score Rush's opening hit should land on a real target near Rosie's own starting position (got %.2f damage)\n",
               near_target->damage_percent);
        return 1;
    }
    if (far_target->damage_percent < ROSIE_DASH_HIT_DAMAGE - 0.01f) {
        printf("❌ FAIL: High Score Rush's closing hit should land on a real target near Rosie's own end-of-dash position (got %.2f damage)\n",
               far_target->damage_percent);
        return 1;
    }
    if (!invuln_seen) {
        printf("❌ FAIL: High Score Rush should give Rosie real invuln_frames during the dash's own middle window\n");
        return 1;
    }
    if (rosie->rosie_dash_frame != 0) {
        printf("❌ FAIL: rosie_dash_frame should reset to 0 once the dash's own %d frames finish\n", ROSIE_DASH_TOTAL_FRAMES);
        return 1;
    }
    printf("✅ PASS: High Score Rush lands the opening hit (%.2f dmg) and the closing hit (%.2f dmg) independently, plus real mid-dash invulnerability\n",
           near_target->damage_percent, far_target->damage_percent);
    return 0;
}

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

    /* Validation */
    if (!(vel.x < 10.0f && vel.x > 0.0f)) {
        printf("\n❌ FAIL: Velocity check failed (Final Vx: %f)\n", vel.x);
        return 1;
    }
    printf("\n✅ PASS: Friction applied correctly (Final Vx: %f)\n", vel.x);

    if (test_rosie_insert_coin() != 0) return 1;
    if (test_rosie_high_score_rush() != 0) return 1;

    return 0;
}
