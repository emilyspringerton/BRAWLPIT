#include "../core/character.h"

static void yoshi_on_shield(Character *c) {
    c->shield.stun = 0.0f;
    c->shield.pushback = 0.0f;
}

static void yoshi_on_frame(Character *c) {
    if (c->state == 5) {
        c->vel.x *= 0.9f;
    }
}

CharacterDef CHARACTER_YOSHI = {
    .name = "Yoshi",
    .icon_path = "icons/yoshi.png",
    .physics = {
        .gravity = 0.09f,
        .air_speed = 1.2f,
        .max_fall_speed = 2.8f,
    },
    .shield = {
        .type = 1,
        .stun = 0.0f,
        .pushback = 0.0f,
    },
    .vtable = {
        .on_frame = yoshi_on_frame,
        .on_shield = yoshi_on_shield,
    },
    .custom_data = NULL,
    .custom_size = 0,
};
