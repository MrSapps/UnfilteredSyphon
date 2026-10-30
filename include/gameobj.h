#ifndef GAMEOBJ_H
#define GAMEOBJ_H

#include "common.h"

/* Per-level table of placed game objects (gameObjectsInfo[], 0x4C bytes each). */
typedef struct GameObjectInfo {
    /* 0x00 */ s32 modelIndex; /* index into gameObjectsModelInfo */
    /* 0x04 */ u8 unk04[0x30];
    /* 0x34 */ struct GameObjectRuntime *runtime;
    /* 0x38 */ u8 unk38[0x14];
} GameObjectInfo; /* size 0x4C */

/* Per-level model/type table (gameObjectsModelInfo[], 0x14 bytes each). */
typedef struct GameObjectModelInfo {
    /* 0x00 */ s16 type; /* index into gDispatchFunctions */
    /* 0x02 */ u8 unk02[0x12];
} GameObjectModelInfo; /* size 0x14 */

typedef struct GameObjectDisplay {
    /* 0x00 */ u8 unk00[0x10];
    /* 0x10 */ s32 unk10;
} GameObjectDisplay;

typedef struct GameObjectModel {
    /* 0x00 */ u8 unk00[0x20];
    /* 0x20 */ GameObjectDisplay *display;
    /* 0x24 */ u8 unk24[4];
    /* 0x28 */ u32 flags;
} GameObjectModel;

typedef struct GameObjectAnim {
    /* 0x00 */ u8 unk00[0x10];
    /* 0x10 */ GameObjectModel *model;
} GameObjectAnim;

typedef struct GameObjectRuntime {
    /* 0x00 */ u8 unk00[8];
    /* 0x08 */ GameObjectAnim *anim;
} GameObjectRuntime;

#define GOBJ_MODEL_FLAG_400000 0x400000

extern GameObjectInfo *gameObjectsInfo;
extern GameObjectModelInfo *gameObjectsModelInfo;
extern s32 gLevelGameObjCount;

#endif /* GAMEOBJ_H */
