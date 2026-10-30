#ifndef GAMEOBJ_H
#define GAMEOBJ_H

#include "common.h"

/* Per-level table of placed game objects (gameObjectsInfo[], 0x4C bytes each). */
typedef struct GOBJ_MATRIX {
    s16 m[3][3];
    s16 pad;
    s32 t[3];
} GOBJ_MATRIX; /* layout of the PsyQ MATRIX */

typedef struct GameObjectInfo {
    /* 0x00 */ s32 modelIndex; /* index into gameObjectsModelInfo */
    /* 0x04 */ GOBJ_MATRIX matrix; /* placement; t[] is the start position */
    /* 0x24 */ u16 flags24;
    /* 0x26 */ u8 unk26[2];
    /* 0x28 */ s32 pathStart;
    /* 0x2C */ s32 path;
    /* 0x30 */ s32 refId; /* linked object id, -1 if none */
    /* 0x34 */ struct GameObjectRuntime *runtime;
    /* 0x38 */ u8 unk38[0x14];
} GameObjectInfo; /* size 0x4C */

/* Per-level model/type table (gameObjectsModelInfo[], 0x14 bytes each). */
typedef struct GameObjectModelInfo {
    /* 0x00 */ s16 type; /* index into gDispatchFunctions */
    /* 0x02 */ u8 unk02[2];
    /* 0x04 */ u8 *model;
    /* 0x08 */ u8 unk08[0xC];
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
    /* 0x00 */ u8 unk00[0xC];
    /* 0x0C */ struct EGSFrame *frame;
    /* 0x10 */ GameObjectModel *model;
    /* 0x14 */ u8 unk14[4];
    /* 0x18 */ struct EGSFrame **frames; /* per-bone frames (5 handles) */
    /* 0x1C */ u8 unk1C[0xC];
    /* 0x28 */ void *faceList; /* collision faces gathered this frame */
} GameObjectAnim;

typedef struct GameObjectHealth {
    /* 0x00 */ u8 unk00[6];
    /* 0x06 */ s16 armor; /* flak jacket, 600 when undamaged */
    /* 0x08 */ s16 health;
} GameObjectHealth;

/* Live game object ("gobj"): GameObjectInfo.runtime, pooled by GameGetGobj (0x24 bytes). */
typedef struct GameObjectRuntime {
    /* 0x00 */ u8 flags0;
    /* 0x01 */ u8 flags1;
    /* 0x02 */ s16 id; /* gameObjectsInfo index, -1 when the pool entry is free */
    /* 0x04 */ s32 unk04; /* per-type: last-use tick (doors), state, ... */
    /* 0x08 */ GameObjectAnim *anim;
    /* 0x0C */ struct MoveInfo *move;
    /* 0x10 */ struct CASState *cas; /* character animation system; flags first */
    /* 0x14 */ struct TargetInfo *target;
    /* 0x18 */ GameObjectHealth *healthInfo;
    /* 0x1C */ struct NPCInfo *npc;
    /* 0x20 */ u8 unk20[2];
    /* 0x22 */ u8 npcState; /* 2: NPC */
    /* 0x23 */ u8 flags23;
} GameObjectRuntime;

#define GOBJ_MODEL_FLAG_400000 0x400000

/* Who/what an object is targeting (GameObjectRuntime.target), 0xD8 bytes. */
typedef struct TargetInfo {
    /* 0x00 */ s16 targetId; /* gameObjectsInfo index, -1 for none */
    /* 0x02 */ s16 unk02;
    /* 0x04 */ u32 flags;
    /* 0x08 */ void (*actions[5])(); /* per-kind targeting steps, NULL for none */
    /* 0x1C */ void *unk1C;
    /* 0x20 */ u8 unk20[0x10];
    /* 0x30 */ void *profile; /* entry in the per-kind profile table (0x36 bytes each) */
    /* 0x34 */ s16 unk34[15];
    /* 0x52 */ s16 unk52;
    /* 0x54 */ s16 unk54;
    /* 0x56 */ s16 unk56;
    /* 0x58 */ s16 unk58;
    /* 0x5A */ s16 unk5A;
    /* 0x5C */ u8 unk5C[2];
    /* 0x5E */ s16 unk5E;
    /* 0x60 */ s16 unk60;
    /* 0x62 */ u8 unk62[0x42];
    /* 0xA4 */ s32 unkA4;
    /* 0xA8 */ s32 unkA8;
    /* 0xAC */ s32 unkAC;
    /* 0xB0 */ s32 unkB0;
    /* 0xB4 */ u8 unkB4[4];
    /* 0xB8 */ s32 unkB8;
    /* 0xBC */ s16 unkBC;
    /* 0xBE */ u8 unkBE[2];
    /* 0xC0 */ s32 unkC0;
    /* 0xC4 */ u8 unkC4[8];
    /* 0xCC */ s32 startTick;
    /* 0xD0 */ s32 lastTick;
    /* 0xD4 */ s32 threat; /* 0..0x1000 */
} TargetInfo; /* size 0xD8 */

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
