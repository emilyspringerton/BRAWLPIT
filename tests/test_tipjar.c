/* tests/test_tipjar.c -- TJ-124: STAGE_TIPJAR (2 floors + basement, 120 wide). Real headless
 * checks against the actual physics/tipjar code: geometry loads from data/levels/tipjar.json,
 * a player lands on each of the three floors, seats are on all three floors and clear of the
 * stairwell, and a bubbled customer is ejected through either end door. Run from repo root. */
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <netinet/in.h>
#include "../packages/common/physics.h"
#include "../packages/simulation/local_game.h"
#include "../packages/simulation/tipjar.h"

static int fails = 0;
#define CHECK(c, ...) do { if (c) printf("PASS: " __VA_ARGS__); else { printf("FAIL: " __VA_ARGS__); fails++; } printf("\n"); } while (0)

static float settle(float x, float y) {
    local_init_match(1, MODE_STOCK, STAGE_TIPJAR, 0, 0);
    PlayerState *p = &local_state.players[0];
    p->x = x; p->y = y; p->vx = 0; p->vy = 0; p->invuln_frames = 0;
    for (int i = 0; i < 400; i++) local_update(0, 0, 0, 0, 0, 0, NULL, (unsigned)i * 16);
    return p->y;
}

int main(void) {
    stage_set_active(STAGE_TIPJAR);
    CHECK(stage_count == 13, "tipjar stage has 13 platforms (got %d)", stage_count);
    CHECK(stage_blast_right >= 100.0f, "blast zone scaled for the wide level (%.0f)", stage_blast_right);

    float yg = settle(-34.0f, 3.0f), yu = settle(-34.0f, 27.0f), yb = settle(-34.0f, -20.0f);
    CHECK(fabsf(yg - 0.0f) < 0.6f, "ground floor lands at y~0 (%.2f)", yg);
    CHECK(fabsf(yu - 24.0f) < 0.6f, "upper floor lands at y~24 (%.2f)", yu);
    CHECK(fabsf(yb + 24.0f) < 0.6f, "basement lands at y~-24 (%.2f)", yb);

    local_init_match(2, MODE_STOCK, STAGE_TIPJAR, 0, 0);
    tipjar_init(0);
    int per_floor[3] = {0, 0, 0}, seat_ok = 1;
    for (int i = 0; i < MAX_CUSTOMERS; i++) {
        Customer *c = &tipjar_state.customers[i];
        for (int f = 0; f < 3; f++) if (c->floor_y == TIPJAR_FLOOR_YS[f]) per_floor[f]++;
        if (fabsf(c->seat_x) < 9.0f || fabsf(c->seat_x) > 56.0f) seat_ok = 0;
    }
    CHECK(per_floor[0] == 3 && per_floor[1] == 3 && per_floor[2] == 3, "3 customers on each floor");
    CHECK(seat_ok, "seats clear of the stairwell and inside the doors");

    for (int side = 0; side < 2; side++) {
        local_init_match(2, MODE_STOCK, STAGE_TIPJAR, 0, 0);
        local_state.players[0].x = 0; local_state.players[0].y = 1;
        local_state.players[1].x = 0; local_state.players[1].y = 1;
        tipjar_init(0);
        Customer *c = &tipjar_state.customers[1]; /* upper floor */
        c->state = CUST_BUBBLED; c->bubbled_by = 1;
        c->x = side ? 55.8f : -55.8f; c->y = c->floor_y;
        c->bubbled_until_ms = 99999;
        TipjarPlayerInput in[MAX_CLIENTS]; memset(in, 0, sizeof(in));
        int before = tipjar_state.score[1];
        for (int t = 0; t < 120 && c->active; t++) tipjar_tick(&local_state, in, 1000 + t * 16, 0.016f);
        CHECK(!c->active && tipjar_state.score[1] == before + 10, "bubbled customer ejected via %s door, credit to bubbler", side ? "right" : "left");
    }
    printf(fails ? "\n%d FAILED\n" : "\nALL PASS\n", fails);
    return fails ? 1 : 0;
}
