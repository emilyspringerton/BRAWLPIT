#include "../core/stage.h"

static const float fd_points[] = {
    -30.0f, 0.0f,
    30.0f, 0.0f,
};

StageDef STAGE_FINAL_DESTINATION = {
    .name = "Final Destination",
    .mesh = {
        .points = fd_points,
        .point_count = 2,
    },
    .vtable = {0},
};
