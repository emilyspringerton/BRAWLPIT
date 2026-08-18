#ifndef BRAWLPIT_CHARACTERS_H
#define BRAWLPIT_CHARACTERS_H

#include "protocol.h"

typedef enum {
    CHARACTER_PETALIA = 0,
    CHARACTER_VEXAR = 1,
    // Founder, real-time: "can we add pixel art to the brawlpit engine?
    // ... add at least 4 characters to brawlpit via the promtoverse
    // pixel art gens." Lore for all four written first, into
    // TYLER/multiverse_heroes.md #116-119 ("Later addition, 2026-08-18"),
    // real generated portraits pulled from Prompt-o-verse's "8-bit pixel
    // art" style -- stats below are an argument made from that lore, not
    // invented separately from it (same doctrine that document itself
    // states: "a hero's mechanics should be an argument someone can make
    // from their history").
    CHARACTER_UNDERSTUDY = 2,   // The Arabesque Understudy -- balletic, airy, precise
    CHARACTER_ROSIE = 3,        // Rosie of the Unclaimed Arcade Cabinet -- quick, generated twice, well-rounded
    CHARACTER_SUNLIT_DRAW = 4,  // The Sunlit Draw -- grounded, sturdy, "cleared and aware it wasn't guaranteed"
    CHARACTER_SEQUEL_DUCK = 5,  // The Tuxedo Duck, Second Casting -- tricky, floaty, theatrical
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
    // sprite_path is optional -- "" means no sprite, fall back to the
    // existing colored-primitive rendering (draw_player already handles
    // this: an empty path is simply never handed to draw_sprite_quad).
    const char *sprite_path;
} FighterDef;

static const FighterDef g_fighters[CHARACTER_COUNT] = {
    { CHARACTER_PETALIA, "PETALIA", "SKY BLOOM DUELIST",
      1.00f, 0.45f, 0.86f, 1.00f, 0.80f, 0.95f,
      0.85f, 0.95f, 0.95f, 1.10f, 0.95f, 0.90f, "" },
    { CHARACTER_VEXAR, "VEXAR", "COSMIC RELIC HUNTER",
      0.34f, 0.38f, 0.43f, 0.12f, 0.95f, 1.00f,
      1.08f, 1.20f, 1.15f, 0.95f, 1.12f, 1.20f, "" },
    // Balletic and airy -- arabesque precision reads as low ground speed,
    // strong air control, light landing (low gravity), and a jump multiplier
    // that rewards commitment to an aerial line, per the lore's own "held
    // an arabesque against a scene it never asked for."
    { CHARACTER_UNDERSTUDY, "UNDERSTUDY", "THE ARABESQUE UNDERSTUDY",
      0.85f, 0.20f, 0.30f, 0.90f, 0.75f, 0.80f,
      0.75f, 1.25f, 0.85f, 1.20f, 0.85f, 1.00f,
      "apps/lobby/assets/sprites/understudy.png" },
    // Generated twice, kept both times -- reads as a well-rounded,
    // slightly-favored-toward-speed kit; no glaring weakness, matching a
    // subject the taxonomy engine reached for on its own more than once.
    { CHARACTER_ROSIE, "ROSIE", "ROSIE OF THE UNCLAIMED ARCADE CABINET",
      0.95f, 0.55f, 0.15f, 1.00f, 0.20f, 0.55f,
      1.00f, 1.05f, 1.10f, 1.00f, 1.00f, 1.05f,
      "apps/lobby/assets/sprites/rosie.png" },
    // Grounded and sturdy -- "cleared, and aware it wasn't guaranteed"
    // reads as a character who plays like she knows the ground could be
    // taken away: heavier, harder-hitting, less mobile in the air.
    { CHARACTER_SUNLIT_DRAW, "SUNLIT DRAW", "THE SUNLIT DRAW",
      0.15f, 0.75f, 0.85f, 1.00f, 0.80f, 0.10f,
      1.15f, 0.80f, 1.05f, 0.90f, 1.15f, 0.90f,
      "apps/lobby/assets/sprites/sunlit_draw.png" },
    // Tricky and theatrical -- a duck generated wearing a tuxedo despite
    // the engine's own judgment layer concluding it probably shouldn't
    // be a mashup; plays floaty and unpredictable, projectile-leaning
    // (the bow, the hat, thrown like a beat) over raw damage.
    { CHARACTER_SEQUEL_DUCK, "SEQUEL DUCK", "THE TUXEDO DUCK, SECOND CASTING",
      0.10f, 0.10f, 0.12f, 0.85f, 0.65f, 0.15f,
      0.70f, 1.10f, 0.90f, 1.05f, 0.90f, 1.25f,
      "apps/lobby/assets/sprites/sequel_duck.png" },
};

static inline const FighterDef *fighter_def(CharacterId id) {
    if (id < 0 || id >= CHARACTER_COUNT) return &g_fighters[CHARACTER_PETALIA];
    return &g_fighters[id];
}

#endif
