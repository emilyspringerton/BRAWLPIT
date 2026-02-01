#ifndef BRAWLPIT_CORE_STAGE_H
#define BRAWLPIT_CORE_STAGE_H

typedef struct Stage Stage;
typedef struct Character Character;

typedef struct {
    const float *points;
    int point_count;
} CollisionMesh;

typedef struct StageVTable {
    void (*on_load)(Stage *s);
    void (*on_frame)(Stage *s);
    void (*on_collision)(Stage *s, Character *c);
} StageVTable;

typedef struct StageDef {
    const char *name;
    CollisionMesh mesh;
    StageVTable vtable;
} StageDef;

struct Stage {
    StageDef *def;
    int frame;
    void *custom;
};

Stage *load_stage(StageDef *def);
void update_stage(Stage *stage);
void stage_on_collision(Stage *stage, Character *c);
void unload_stage(Stage *stage);

#endif
