#ifndef GAMEOBJ_H
#define GAMEOBJ_H

#include "common.h"

/* Per-level table of placed game objects (gameObjectsInfo[], 0x4C bytes each). */
typedef struct GameObjectInfo {
    /* 0x00 */ s32 modelIndex; /* index into gameObjectsModelInfo */
    /* 0x04 */ u8 unk04[0x2C];
    /* 0x30 */ s32 unk30;
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

typedef struct GameObjectHealth {
    /* 0x00 */ u8 unk00[6];
    /* 0x06 */ s16 health;
    /* 0x08 */ s16 maxHealth;
} GameObjectHealth;

typedef struct GameObjectRuntime {
    /* 0x00 */ u8 flags0;
    /* 0x01 */ u8 flags1;
    /* 0x02 */ s16 linkedId; /* related object (e.g. a bomb's target) */
    /* 0x04 */ u8 unk04[4];
    /* 0x08 */ GameObjectAnim *anim;
    /* 0x0C */ u8 unk0C[0xC];
    /* 0x18 */ GameObjectHealth *healthInfo;
    /* 0x1C */ u8 unk1C[7];
    /* 0x23 */ u8 flags23;
} GameObjectRuntime;

#define GOBJ_MODEL_FLAG_400000 0x400000

/* Object id 666 is a fixed object with its own dispatch slot rather than a placed one. */
#define GOBJ_ID_666 0x29A
#define GOBJ_TYPE(id) \
    ((id) != GOBJ_ID_666 ? gameObjectsModelInfo[gameObjectsInfo[id].modelIndex].type : GOBJ_ID_666)

/* Object types seen so far */
#define GOBJ_TYPE_BOMB 0x2E
#define GOBJ_TYPE_COP 0x35

extern GameObjectInfo *gameObjectsInfo;
extern GameObjectModelInfo *gameObjectsModelInfo;
extern s32 gLevelGameObjCount;

#endif /* GAMEOBJ_H */
