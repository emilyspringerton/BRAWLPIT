#include "../core/stage.h"

static void void_on_collision(Stage *s, Character *c) {
    (void)s;
    if (c) {
        c->vel.y = -5.0f;
    }
}

static const float void_points[] = {
    0.0f, -50.0f,
};

StageDef STAGE_WEIRD_VOID = {
    .name = "Weird Void",
    .mesh = {
        .points = void_points,
        .point_count = 1,
    },
    .vtable = {
        .on_collision = void_on_collision,
    },
};
