#include "character.h"
#include <stdlib.h>
#include <string.h>

static void apply_physics(Character *c) {
    if (!c || !c->def) return;
    c->vel.y -= c->def->physics.gravity;
    if (c->vel.y < -c->def->physics.max_fall_speed) {
        c->vel.y = -c->def->physics.max_fall_speed;
    }
    c->pos.x += c->vel.x;
    c->pos.y += c->vel.y;
}

Character *spawn_character(CharacterDef *def) {
    if (!def) return NULL;
    Character *c = calloc(1, sizeof(Character));
    if (!c) return NULL;
    c->def = def;
    c->shield.stun = def->shield.stun;
    c->shield.pushback = def->shield.pushback;
    if (def->custom_size > 0) {
        c->custom = calloc(1, def->custom_size);
    } else {
        c->custom = def->custom_data;
    }
    if (def->vtable.on_spawn) {
        def->vtable.on_spawn(c);
    }
    return c;
}

void update_character(Character *c) {
    if (!c || !c->def) return;
    c->frame++;
    apply_physics(c);
    if (c->def->vtable.on_frame) {
        c->def->vtable.on_frame(c);
    }
}

void character_on_hit(Character *c, HitEvent *hit) {
    if (!c || !c->def) return;
    if (c->def->vtable.on_hit) {
        c->def->vtable.on_hit(c, hit);
    }
}

void character_on_shield(Character *c) {
    if (!c || !c->def) return;
    if (c->def->vtable.on_shield) {
        c->def->vtable.on_shield(c);
    }
}

void character_on_special(Character *c, int special_id) {
    if (!c || !c->def) return;
    if (c->def->vtable.on_special) {
        c->def->vtable.on_special(c, special_id);
    }
}

void destroy_character(Character *c) {
    if (!c) return;
    if (c->def && c->def->vtable.on_death) {
        c->def->vtable.on_death(c);
    }
    if (c->def && c->def->custom_size > 0) {
        free(c->custom);
    }
    free(c);
}
