#include "../core/character.h"

static void fox_on_spawn(Character *c) {
    c->pos.x = -5.0f;
    c->pos.y = 12.0f;
}

static void fox_on_hit(Character *c, HitEvent *hit) {
    if (!hit) return;
    c->vel.x += hit->knockback.x * 0.8f;
    c->vel.y += hit->knockback.y * 0.8f;
}

CharacterDef CHARACTER_FOX = {
    .name = "Fox",
    .icon_path = "icons/fox.png",
    .physics = {
        .gravity = 0.12f,
        .air_speed = 1.4f,
        .max_fall_speed = 3.2f,
    },
    .shield = {
        .type = 0,
        .stun = 6.0f,
        .pushback = 1.2f,
    },
    .vtable = {
        .on_spawn = fox_on_spawn,
        .on_hit = fox_on_hit,
    },
    .custom_data = NULL,
    .custom_size = 0,
};
