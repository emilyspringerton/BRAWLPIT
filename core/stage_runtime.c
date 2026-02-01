#include "stage.h"
#include <stdlib.h>

Stage *load_stage(StageDef *def) {
    if (!def) return NULL;
    Stage *stage = calloc(1, sizeof(Stage));
    if (!stage) return NULL;
    stage->def = def;
    if (def->vtable.on_load) {
        def->vtable.on_load(stage);
    }
    return stage;
}

void update_stage(Stage *stage) {
    if (!stage || !stage->def) return;
    stage->frame++;
    if (stage->def->vtable.on_frame) {
        stage->def->vtable.on_frame(stage);
    }
}

void stage_on_collision(Stage *stage, Character *c) {
    if (!stage || !stage->def) return;
    if (stage->def->vtable.on_collision) {
        stage->def->vtable.on_collision(stage, c);
    }
}

void unload_stage(Stage *stage) {
    free(stage);
}
