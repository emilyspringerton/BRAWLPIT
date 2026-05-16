#ifndef BRAWLPIT_CHARACTERS_H
#define BRAWLPIT_CHARACTERS_H

#include "protocol.h"

typedef enum {
    CHARACTER_PETALIA = 0,
    CHARACTER_VEXAR = 1,
    CHARACTER_COUNT
} CharacterId;

typedef struct {
    CharacterId id;
    const char *name;
    const char *descriptor;
    float body_r, body_g, body_b;
    float accent_r, accent_g, accent_b;
    float gravity_mul;
    float air_speed_mul;
    float ground_speed_mul;
    float jump_mul;
    float attack_damage_mul;
    float projectile_speed_mul;
} FighterDef;

static const FighterDef g_fighters[CHARACTER_COUNT] = {
    { CHARACTER_PETALIA, "PETALIA", "SKY BLOOM DUELIST",
      1.00f, 0.45f, 0.86f, 1.00f, 0.80f, 0.95f,
      0.85f, 0.95f, 0.95f, 1.10f, 0.95f, 0.90f },
    { CHARACTER_VEXAR, "VEXAR", "COSMIC RELIC HUNTER",
      0.34f, 0.38f, 0.43f, 0.12f, 0.95f, 1.00f,
      1.08f, 1.20f, 1.15f, 0.95f, 1.12f, 1.20f },
};

static inline const FighterDef *fighter_def(CharacterId id) {
    if (id < 0 || id >= CHARACTER_COUNT) return &g_fighters[CHARACTER_PETALIA];
    return &g_fighters[id];
}

#endif
