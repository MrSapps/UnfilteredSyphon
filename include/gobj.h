#ifndef GOBJ_H
#define GOBJ_H

#include "common.h"
#include "gameobj.h"

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

typedef struct GameObjectRuntime Gobj;

typedef struct CASState {
    /* 0x00 */ u32 flags;
    /* 0x04 */ u32 flags2;
    /* 0x08 */ u8 active;
    /* 0x09 */ u8 mode;
    /* 0x0A */ u8 unk0A[2];
    /* 0x0C */ void *queue;
    /* 0x10 */ u8 unk10[4];
    /* 0x14 */ void *actionTable;
} CASState;

#define GOBJ_POOL_SIZE 100
#define GOBJ_MOVE_REQUEST_MAX 30

#endif /* GOBJ_H */
