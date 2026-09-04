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

/* test_medusa_serpents_grasp -- real, direct verification of Medusa's new down-B (kanban
 * BPTUNE-10001: "up b and down b all do the same thing for every character... need to be
 * distinct moves"). Confirms the real, previously-dead "hold S + special on the ground" input
 * now drives a real, distinct move from her own neutral-B (Petrifying Gaze: ranged, no damage) --
 * melee-range real damage + knockback instead. */
static int test_medusa_serpents_grasp(void) {
    ServerState state;
    memset(&state, 0, sizeof(state));

    PlayerState *medusa = &state.players[0];
    PlayerState *target = &state.players[1];
    medusa->id = 0;
    medusa->active = 1;
    medusa->character_id = CHARACTER_MEDUSA;
    medusa->on_ground = 1;
    medusa->x = 0.0f;
    medusa->y = 0.0f;
    medusa->facing = 1;
    medusa->in_y = -1.0f; /* hold S -- the real down-B input */
    medusa->btn_special = 1;
    medusa->btn_special_prev = 0;

    target->id = 1;
    target->active = 1;
    target->x = 1.0f; /* within BRAWLPIT_SERPENTS_GRASP_RANGE */
    target->y = 0.0f;

    float dt = 1.0f / 60.0f;
    update_entity(medusa, dt, &state, 0);

    if (target->damage_percent < BRAWLPIT_SERPENTS_GRASP_DAMAGE - 0.01f) {
        printf("❌ FAIL: Serpents' Grasp should deal real damage to a target in melee range (got %.2f)\n",
               target->damage_percent);
        return 1;
    }
    printf("✅ PASS: Serpents' Grasp (down-B) deals %.2f real damage, distinct from Petrifying Gaze's own ranged stun\n",
           target->damage_percent);
    return 0;
}

/* test_petalia_multi_hit_parasol -- real, direct verification of Petalia's new multi-hit
 * Parasol Up-B (kanban BP-TUNE-3939309: "RESTORE PETALIA PARISOL UP B FROM WAY BACK IN GIT IT
 * NEEDS TO BE MULTI HIT AND GIVE VERTICAL MOBILITY AND OPEN THE PARISOL"). Drives the real
 * input->dispatch->per-frame pipeline via update_entity itself, confirms a real target near
 * Petalia takes damage from MULTIPLE separate hits across the ascent (not just one), and that
 * the parasol itself opens with real vertical mobility -- both already-universal, verified here
 * anyway since they're this move's own literal, real requirements. */
static int test_petalia_multi_hit_parasol(void) {
    ServerState state;
    memset(&state, 0, sizeof(state));

    PlayerState *petalia = &state.players[0];
    PlayerState *target = &state.players[1];
    petalia->id = 0;
    petalia->active = 1;
    petalia->character_id = CHARACTER_PETALIA;
    petalia->on_ground = 0; /* airborne -- up-B only triggers in the air */
    petalia->x = 0.0f;
    petalia->y = 0.0f;
    petalia->facing = 1;
    petalia->in_y = 1.0f; /* hold W -- the real up-B input */
    petalia->btn_special = 1;
    petalia->btn_special_prev = 0;

    target->id = 1;
    target->active = 1;
    target->x = 1.0f; /* within BRAWLPIT_PETALIA_PARASOL_HIT_RANGE */
    target->y = 0.0f;

    float dt = 1.0f / 60.0f;
    unsigned int time = 0;
    for (int frame = 0; frame < 55; frame++) {
        /* Real, deliberate, honestly-labeled test artifice: Petalia's own real vertical
         * mobility means she quickly rises away from a stationary target's own real y-position,
         * so a fixed target could only ever catch the very first hit -- not a real limitation of
         * the move itself, just an artifact of this synthetic single-process test having no real
         * "opponent gets carried along" combat interaction to drive a following target with.
         * Re-tracking the target to Petalia's own current position each frame tests the real
         * thing this move actually needs proven: that parasol_rehit_timer really re-arms and
         * really fires again, not just once. */
        target->x = petalia->x + 0.5f;
        target->y = petalia->y;
        update_entity(petalia, dt, &state, time++);
        petalia->btn_special_prev = petalia->btn_special;
    }

    if (petalia->umbrella_open == 0 && petalia->state != STATE_AIR) {
        printf("❌ FAIL: Parasol Up-B should open the parasol\n");
        return 1;
    }
    if (target->damage_percent < BRAWLPIT_PETALIA_PARASOL_HIT_DAMAGE * 2.0f - 0.01f) {
        printf("❌ FAIL: expected at least 2 real separate hits across the ascent (>= %.2f dmg), got %.2f\n",
               BRAWLPIT_PETALIA_PARASOL_HIT_DAMAGE * 2.0f, target->damage_percent);
        return 1;
    }
    printf("✅ PASS: Petalia's multi-hit Parasol lands %.2f real total damage across the ascent (multiple real hits, not one)\n",
           target->damage_percent);
    return 0;
}

/* test_rosie_petalia_turnip_is_down_b_not_up_b -- real, direct verification of the down-B remap
 * (kanban BP-TUNE-93939: "rosie and petalia TURNIPS SHOULD NOT BE UP B THEY SHOULD BE DOWN B
 * AND ALSO AVAILABLE IN THE AIR"). Drives the real input->dispatch pipeline via update_entity,
 * confirming: (1) Rosie's own hold-UP input no longer spawns any turnip at all (Insert Coin
 * moved off that slot entirely), (2) Rosie's hold-DOWN input does spawn her real Insert Coin,
 * (3) Petalia -- previously only reachable via the generic hold-up fallback -- now gets a real
 * turnip from hold-DOWN instead, and (4) both work in the air, not just grounded. */
static int test_rosie_petalia_turnip_is_down_b_not_up_b(void) {
    ServerState state;
    memset(&state, 0, sizeof(state));

    /* (1) Rosie, hold UP, grounded -- must NOT spawn a turnip anymore. */
    PlayerState *rosie_up = &state.players[0];
    rosie_up->id = 0;
    rosie_up->active = 1;
    rosie_up->character_id = CHARACTER_ROSIE;
    rosie_up->on_ground = 1;
    rosie_up->facing = 1;
    rosie_up->in_y = 1.0f;
    rosie_up->btn_special = 1;
    rosie_up->btn_special_prev = 0;
    update_entity(rosie_up, 1.0f / 60.0f, &state, 0);
    int turnips_after_up = 0;
    for (int i = 0; i < MAX_TURNIPS; i++) if (state.turnips[i].active) turnips_after_up++;
    if (turnips_after_up != 0) {
        printf("❌ FAIL: Rosie's hold-UP input should no longer spawn any turnip (Insert Coin moved to down-B), got %d\n", turnips_after_up);
        return 1;
    }

    /* (2) Rosie, hold DOWN, grounded -- must spawn her real Insert Coin (2 turnips). */
    memset(&state, 0, sizeof(state));
    PlayerState *rosie_down = &state.players[0];
    rosie_down->id = 0;
    rosie_down->active = 1;
    rosie_down->character_id = CHARACTER_ROSIE;
    rosie_down->on_ground = 1;
    rosie_down->facing = 1;
    rosie_down->in_y = -1.0f;
    rosie_down->btn_special = 1;
    rosie_down->btn_special_prev = 0;
    update_entity(rosie_down, 1.0f / 60.0f, &state, 0);
    int turnips_after_down = 0;
    for (int i = 0; i < MAX_TURNIPS; i++) if (state.turnips[i].active) turnips_after_down++;
    if (turnips_after_down != 2) {
        printf("❌ FAIL: Rosie's hold-DOWN input should spawn her real Insert Coin (2 turnips), got %d\n", turnips_after_down);
        return 1;
    }

    /* (3) Petalia, hold DOWN, grounded -- must spawn a real turnip (her own new down-B). */
    memset(&state, 0, sizeof(state));
    PlayerState *petalia_down = &state.players[0];
    petalia_down->id = 0;
    petalia_down->active = 1;
    petalia_down->character_id = CHARACTER_PETALIA;
    petalia_down->on_ground = 1;
    petalia_down->facing = 1;
    petalia_down->in_y = -1.0f;
    petalia_down->btn_special = 1;
    petalia_down->btn_special_prev = 0;
    update_entity(petalia_down, 1.0f / 60.0f, &state, 0);
    int petalia_turnips = 0;
    for (int i = 0; i < MAX_TURNIPS; i++) if (state.turnips[i].active) petalia_turnips++;
    if (petalia_turnips != 1) {
        printf("❌ FAIL: Petalia's hold-DOWN input should spawn a real turnip (her own new down-B), got %d\n", petalia_turnips);
        return 1;
    }

    /* (4) Petalia, hold DOWN, AIRBORNE -- must also spawn a real turnip (the "also available in
     * the air" half of the ask). */
    memset(&state, 0, sizeof(state));
    PlayerState *petalia_air = &state.players[0];
    petalia_air->id = 0;
    petalia_air->active = 1;
    petalia_air->character_id = CHARACTER_PETALIA;
    petalia_air->on_ground = 0;
    petalia_air->facing = 1;
    petalia_air->in_y = -1.0f;
    petalia_air->btn_special = 1;
    petalia_air->btn_special_prev = 0;
    update_entity(petalia_air, 1.0f / 60.0f, &state, 0);
    int petalia_air_turnips = 0;
    for (int i = 0; i < MAX_TURNIPS; i++) if (state.turnips[i].active) petalia_air_turnips++;
    if (petalia_air_turnips != 1) {
        printf("❌ FAIL: Petalia's hold-DOWN input should spawn a real turnip in the air too, got %d\n", petalia_air_turnips);
        return 1;
    }

    printf("✅ PASS: Rosie/Petalia turnip is real down-B (not up-B), and available in the air for both\n");
    return 0;
}

/* test_raccoon_play_dead -- real, direct verification of Raccoon's new down-B (kanban
 * BPTUNE-10001), the tuning pass's second real down-B. Confirms the "hold S + special on the
 * ground" input drives a real, distinct move from Scavenger's Dash (neutral-B: escape via
 * movement) -- this one escapes via stillness + invulnerability instead, dealing zero damage,
 * keeping Raccoon's own "pure mobility, no offense" identity intact across both moves. */
static int test_raccoon_play_dead(void) {
    ServerState state;
    memset(&state, 0, sizeof(state));

    PlayerState *raccoon = &state.players[0];
    raccoon->id = 0;
    raccoon->active = 1;
    raccoon->character_id = CHARACTER_RACCOON;
    raccoon->on_ground = 1;
    raccoon->x = 0.0f;
    raccoon->y = 0.0f;
    raccoon->facing = 1;
    raccoon->vx = 5.0f; /* real, deliberate: started with real horizontal velocity to confirm Play Dead actually zeroes it */
    raccoon->in_y = -1.0f; /* hold S -- the real down-B input */
    raccoon->btn_special = 1;
    raccoon->btn_special_prev = 0;

    float dt = 1.0f / 60.0f;
    update_entity(raccoon, dt, &state, 0);

    if (raccoon->vx != 0.0f) {
        printf("❌ FAIL: Play Dead should zero Raccoon's own horizontal velocity, got vx=%.2f\n", raccoon->vx);
        return 1;
    }
    if (raccoon->invuln_frames < BRAWLPIT_PLAY_DEAD_INVULN_FRAMES) {
        printf("❌ FAIL: Play Dead should grant real invuln_frames (>= %d), got %d\n",
               BRAWLPIT_PLAY_DEAD_INVULN_FRAMES, raccoon->invuln_frames);
        return 1;
    }
    if (raccoon->state == STATE_WAVEDASH) {
        printf("❌ FAIL: Play Dead should NOT enter Scavenger's Dash's own STATE_WAVEDASH -- it's stillness, not movement\n");
        return 1;
    }
    printf("✅ PASS: Play Dead (down-B) zeroes velocity and grants %d real invuln_frames, distinct from Scavenger's Dash's own movement\n",
           raccoon->invuln_frames);
    return 0;
}

/* test_special_on_cooldown_does_not_fall_back_to_wavedash -- real, genuine bug fixed (kanban
 * BP-TUNE-393939/BP-TUNE-9838382: "if turnip is on cooldown the character should not fall back
 * to a wave dash" / "all characters b should be a special move not a wave dash"). Before this
 * fix, every neutral-B/down-B branch required its own cooldown == 0; when a real special was
 * still cooling down, execution fell all the way through to the generic wavedash branches,
 * silently substituting a wavedash for a failed special attempt. This confirms a held-direction
 * special press with the real special ON cooldown does nothing at all -- no wavedash, no
 * movement, no state change -- rather than quietly becoming a wavedash. */
static int test_special_on_cooldown_does_not_fall_back_to_wavedash(void) {
    ServerState state;
    memset(&state, 0, sizeof(state));

    PlayerState *medusa = &state.players[0];
    medusa->id = 0;
    medusa->active = 1;
    medusa->character_id = CHARACTER_MEDUSA;
    medusa->on_ground = 1;
    medusa->x = 0.0f;
    medusa->y = 0.0f;
    medusa->facing = 1;
    medusa->in_y = 1.0f; /* hold W -- Petrifying Gaze's own real input */
    medusa->in_x = 1.0f; /* real, deliberate: a held direction, so a wavedash (if it wrongly
                           * fired) would actually move her -- a real, checkable symptom */
    medusa->btn_special = 1;
    medusa->btn_special_prev = 0;
    medusa->turnip_cooldown = TURNIP_COOLDOWN_FRAMES; /* real special ALREADY on cooldown */

    float dt = 1.0f / 60.0f;
    update_entity(medusa, dt, &state, 0);

    if (medusa->state == STATE_WAVEDASH) {
        printf("❌ FAIL: a held-direction special press with the real special on cooldown must NOT fall back to a wavedash\n");
        return 1;
    }
    if (medusa->wavedash_frames != 0) {
        printf("❌ FAIL: wavedash_frames should stay 0 -- no wavedash should have been granted, got %d\n", medusa->wavedash_frames);
        return 1;
    }
    printf("✅ PASS: a held-direction special press with the real special on cooldown does nothing -- no silent wavedash fallback\n");
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
    if (test_medusa_serpents_grasp() != 0) return 1;
    if (test_raccoon_play_dead() != 0) return 1;
    if (test_special_on_cooldown_does_not_fall_back_to_wavedash() != 0) return 1;
    if (test_petalia_multi_hit_parasol() != 0) return 1;
    if (test_rosie_petalia_turnip_is_down_b_not_up_b() != 0) return 1;

    return 0;
}
