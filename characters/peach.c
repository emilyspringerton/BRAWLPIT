#include "../core/character.h"

static void peach_on_spawn(Character *c) {
    c->pos.x = 0.0f;
    c->pos.y = 10.0f;
}

static void peach_on_special(Character *c, int special_id) {
    if (special_id == 0) {
        c->vel.y = 2.4f;
    }
}

CharacterDef CHARACTER_PEACH = {
    .name = "Peach",
    .icon_path = "icons/peach.png",
    .physics = {
        .gravity = 0.07f,
        .air_speed = 1.0f,
        .max_fall_speed = 2.6f,
    },
    .shield = {
        .type = 0,
        .stun = 8.0f,
        .pushback = 1.0f,
    },
    .vtable = {
        .on_spawn = peach_on_spawn,
        .on_special = peach_on_special,
    },
    .custom_data = NULL,
    .custom_size = 0,
};
