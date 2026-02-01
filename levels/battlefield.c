#include "../core/stage.h"

static void battlefield_on_frame(Stage *s) {
    (void)s;
}

static const float battlefield_points[] = {
    -20.0f, 0.0f,
    20.0f, 0.0f,
    -10.0f, 8.0f,
    10.0f, 8.0f,
};

StageDef STAGE_BATTLEFIELD = {
    .name = "Battlefield",
    .mesh = {
        .points = battlefield_points,
        .point_count = 4,
    },
    .vtable = {
        .on_frame = battlefield_on_frame,
    },
};
