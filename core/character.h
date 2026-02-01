#ifndef BRAWLPIT_CORE_CHARACTER_H
#define BRAWLPIT_CORE_CHARACTER_H

#include <stddef.h>

typedef struct Character Character;
typedef struct HitEvent HitEvent;

typedef struct {
    float gravity;
    float air_speed;
    float max_fall_speed;
} PhysicsParams;

typedef struct {
    int type;
    float stun;
    float pushback;
} ShieldParams;

typedef struct {
    float stun;
    float pushback;
} ShieldRuntime;

typedef struct CharacterVTable {
    void (*on_spawn)(Character *c);
    void (*on_frame)(Character *c);
    void (*on_hit)(Character *c, HitEvent *hit);
    void (*on_shield)(Character *c);
    void (*on_special)(Character *c, int special_id);
    void (*on_death)(Character *c);
} CharacterVTable;

typedef struct CharacterDef {
    const char *name;
    const char *icon_path;
    PhysicsParams physics;
    ShieldParams shield;
    CharacterVTable vtable;
    void *custom_data;
    size_t custom_size;
} CharacterDef;

typedef struct {
    float x;
    float y;
} Vec2;

struct HitEvent {
    float damage;
    Vec2 knockback;
    int hit_id;
};

struct Character {
    CharacterDef *def;
    Vec2 pos;
    Vec2 vel;
    int state;
    int frame;
    ShieldRuntime shield;
    void *custom;
};

Character *spawn_character(CharacterDef *def);
void update_character(Character *c);
void character_on_hit(Character *c, HitEvent *hit);
void character_on_shield(Character *c);
void character_on_special(Character *c, int special_id);
void destroy_character(Character *c);

#endif
