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

    return 0;
}
