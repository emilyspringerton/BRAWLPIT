/* tests/test_commander.c -- S419, real tests for the PARENA-compiled fractal-commander posture
 * function (PARENA/stdlib/brawlpit/commander_mod.prn -> packages/common/commander/
 * commander_mod.c). Exercises the exact real priority ordering that module's own doc comment
 * names: edge-safety beats stock/damage. */
#include <stdio.h>
#include "../packages/common/commander.h"

static int failures = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } \
} while (0)

int main(void) {
    int T = COMMANDER_EDGE_DANGER_THRESHOLD_DEFAULT;

    /* Own fighter dangerously close to the blast zone -- RECOVER overrides everything, even a
     * commanding stock/damage lead. */
    CHECK(commander_posture(3, 0, 0, 150, 2, 40, T) == COMMANDER_POSTURE_RECOVER,
          "own edge danger must force RECOVER regardless of stock/damage lead");

    /* Opponent is the one in edge danger, we are safe -- EDGEGUARD, even while we're behind on
     * stocks (killing them now matters more than our own deficit). */
    CHECK(commander_posture(0, 2, 50, 10, 40, 2, T) == COMMANDER_POSTURE_EDGEGUARD,
          "opponent edge danger (while we're safe) must yield EDGEGUARD even if we're behind");

    /* No edge danger either side -- falls through to the real stock/damage read. */
    CHECK(commander_posture(3, 1, 20, 80, 40, 40, T) == COMMANDER_POSTURE_PATIENT,
          "a real stock lead (both safe) must yield PATIENT");
    CHECK(commander_posture(1, 3, 20, 80, 40, 40, T) == COMMANDER_POSTURE_AGGRESSIVE,
          "a real stock deficit (both safe) must yield AGGRESSIVE");

    /* Tied stocks -- falls through to damage percent as the tiebreaker. */
    CHECK(commander_posture(2, 2, 10, 90, 40, 40, T) == COMMANDER_POSTURE_PATIENT,
          "tied stocks + lower own damage must yield PATIENT");
    CHECK(commander_posture(2, 2, 90, 10, 40, 40, T) == COMMANDER_POSTURE_AGGRESSIVE,
          "tied stocks + higher own damage must yield AGGRESSIVE");
    CHECK(commander_posture(2, 2, 50, 50, 40, 40, T) == COMMANDER_POSTURE_NEUTRAL,
          "a real dead-even read must yield NEUTRAL");

    /* Boundary: exactly at the threshold is NOT yet danger (strict less-than in the real logic). */
    CHECK(commander_posture(2, 2, 50, 50, T, 40, T) == COMMANDER_POSTURE_NEUTRAL,
          "own edge_dist exactly at the threshold must not trigger RECOVER");

    if (failures == 0) {
        printf("test_commander: all checks passed\n");
        return 0;
    }
    printf("test_commander: %d check(s) FAILED\n", failures);
    return 1;
}
