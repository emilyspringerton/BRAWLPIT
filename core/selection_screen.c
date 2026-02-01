#include "character.h"
#include "stage.h"

extern CharacterDef CHARACTER_PEACH;
extern CharacterDef CHARACTER_YOSHI;
extern CharacterDef CHARACTER_FOX;

extern StageDef STAGE_BATTLEFIELD;
extern StageDef STAGE_FINAL_DESTINATION;
extern StageDef STAGE_WEIRD_VOID;

CharacterDef *CHARACTER_REGISTRY[] = {
    &CHARACTER_FOX,
    &CHARACTER_PEACH,
    &CHARACTER_YOSHI,
};

StageDef *STAGE_REGISTRY[] = {
    &STAGE_BATTLEFIELD,
    &STAGE_FINAL_DESTINATION,
    &STAGE_WEIRD_VOID,
};

int CHARACTER_REGISTRY_COUNT = (int)(sizeof(CHARACTER_REGISTRY) / sizeof(CHARACTER_REGISTRY[0]));
int STAGE_REGISTRY_COUNT = (int)(sizeof(STAGE_REGISTRY) / sizeof(STAGE_REGISTRY[0]));
