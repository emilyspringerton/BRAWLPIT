/* tests/test_ai_opponent.c -- S456, real regression test for ai_opponent_build_observation.
 *
 * REAL BUG THIS LOCKS DOWN: the function stopped writing at 21 of the real 31 S430 observation
 * dims -- the posture one-hot block -- even though the 10 relational-feature helpers
 * (ai_opponent_time_to_any_blast_normalized, ai_opponent_facing_toward, etc.) sat right above it
 * in this same file, fully implemented and correct, just never called. obs[21..30] were
 * uninitialized stack garbage fed straight into the live policy net every frame. Training stayed
 * healthy (pure Python, rl_env_packet.py's own build_observation always filled all 31) so Elo
 * genuinely moved while the live C client's own inference was silently broken -- "elos go up and
 * down but when i play them they just stand there" (founder, real-time, 2026-09-13).
 *
 * Expected values below are a direct, hand-verified cross-check against rl_env_packet.py's own
 * real build_observation() for the exact same synthetic own/opp state (see this file's own git
 * history / S456 commit message for the Python-side numbers this was diffed against) -- the same
 * "real checkpoint / real cross-language parity" discipline tests/test_mlp_policy.c's own tier 2
 * already established, just with hand-picked state instead of a trained checkpoint since this is
 * pure arithmetic, not learned weights.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "../packages/common/protocol.h"
#include "../packages/common/ai_opponent.h"

static int failures = 0;

#define CHECK_NEAR(actual, expected, msg) do { \
    float _a = (actual), _e = (expected); \
    if (fabsf(_a - _e) > 1e-4f) { \
        printf("FAIL: %s (got %.6f, want %.6f)\n", msg, _a, _e); \
        failures++; \
    } \
} while (0)

int main(void) {
    PlayerState own; memset(&own, 0, sizeof(own));
    PlayerState opp; memset(&opp, 0, sizeof(opp));

    own.x = 10.0f; own.y = 5.0f; own.vx = 2.0f; own.vy = -1.0f;
    own.damage_percent = 40.0f; own.stocks = 3; own.shield_health = 50.0f; own.facing = 1;

    opp.x = -20.0f; opp.y = 8.0f; opp.vx = -1.5f; opp.vy = 0.5f;
    opp.damage_percent = 70.0f; opp.stocks = 2; opp.shield_health = 30.0f; opp.facing = -1;

    float obs[AI_OPPONENT_OBS_SIZE];
    ai_opponent_build_observation(&own, &opp, obs);

    /* dims 0-20: 8 own + 8 opp raw scalars + 5 posture one-hot -- unchanged by this fix, spot-
     * checked here only to confirm this test's own synthetic state lines up with expectations. */
    CHECK_NEAR(obs[0], 0.125f, "own.x normalized");
    CHECK_NEAR(obs[8], -0.25f, "opp.x normalized");

    /* dims 21-30: the real S430 relational block this fix actually restores -- every one of
     * these was uninitialized garbage before this fix, not just wrong. Values hand-verified
     * against rl_env_packet.py's own build_observation() for this identical own/opp state. */
    CHECK_NEAR(obs[21], -0.375f,   "dx normalized");
    CHECK_NEAR(obs[22], 0.0375f,   "dy normalized");
    CHECK_NEAR(obs[23], 0.376870f, "distance normalized");
    CHECK_NEAR(obs[24], -0.181594f, "closing velocity normalized");
    CHECK_NEAR(obs[25], 0.083333f, "own time-to-blast normalized");
    CHECK_NEAR(obs[26], 0.088889f, "opp time-to-blast normalized");
    CHECK_NEAR(obs[27], -1.0f,     "own facing-toward-opponent");
    CHECK_NEAR(obs[28], -1.0f,     "opp facing-toward-own");
    CHECK_NEAR(obs[29], -0.15f,    "damage differential normalized");
    CHECK_NEAR(obs[30], 0.25f,     "stock differential normalized");

    /* The real, direct regression check: obs[21..30] must never again be silently zero/garbage
     * because the code stopped early -- assert every one of them is non-zero for this state
     * (every hand-picked input above was deliberately chosen so no real term legitimately zeros
     * out, unlike e.g. a symmetric position which would legitimately zero dx/dy). */
    for (int i = 21; i < AI_OPPONENT_OBS_SIZE; i++) {
        if (obs[i] == 0.0f) {
            printf("FAIL: obs[%d] is exactly zero -- the S430 relational block regressed to a no-op again\n", i);
            failures++;
        }
    }

    if (failures == 0) {
        printf("test_ai_opponent: all checks passed\n");
        return 0;
    }
    printf("test_ai_opponent: %d check(s) FAILED\n", failures);
    return 1;
}
