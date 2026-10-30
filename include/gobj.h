#ifndef GOBJ_H
#define GOBJ_H

#include "common.h"

typedef struct VECTOR32 {
    s32 vx, vy, vz, pad;
} VECTOR32;

/* Per-object movement bookkeeping (0x1B8 bytes, EGSMemMalloc'd). */
typedef struct MoveInfo {
    /* 0x000 */ u8 unk000[0x174];
    /* 0x174 */ s32 unk174;
    /* 0x178 */ u8 unk178[4];
    /* 0x17C */ s32 unk17C;
    /* 0x180 */ u8 unk180[0x10];
    /* 0x190 */ s32 unk190;
    /* 0x194 */ VECTOR32 pendingMove; /* applied by GameSysProcessMoveRequests */
    /* 0x1A4 */ u8 unk1A4[0x14];
} MoveInfo; /* size 0x1B8 */

typedef struct EGSFrame {
    /* 0x00 */ u8 unk00[0x14];
    /* 0x14 */ VECTOR32 pos; /* y grows downwards */
} EGSFrame;

typedef struct EGSAnimNode {
    /* 0x00 */ u8 unk00[0xC];
    /* 0x0C */ EGSFrame *frame;
} EGSAnimNode;

/* Live game object ("gobj"), pooled in gGobjPool. */
typedef struct Gobj {
    /* 0x00 */ u8 unk00[2];
    /* 0x02 */ s16 id; /* gameObjectsInfo index, -1 when free */
    /* 0x04 */ u8 unk04[4];
    /* 0x08 */ EGSAnimNode *anim;
    /* 0x0C */ MoveInfo *move;
    /* 0x10 */ struct CASState *cas; /* character animation system state */
    /* 0x14 */ u8 unk14[0x10];
} Gobj; /* size 0x24 */

typedef struct CASState {
    /* 0x00 */ u8 unk00[8];
    /* 0x08 */ u8 active;
    /* 0x09 */ u8 mode;
} CASState;

#define GOBJ_POOL_SIZE 100
#define GOBJ_MOVE_REQUEST_MAX 30

#endif /* GOBJ_H */
