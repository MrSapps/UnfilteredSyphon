#ifndef GOBJ_H
#define GOBJ_H

#include "common.h"
#include "gameobj.h"

typedef struct VECTOR32 {
    s32 vx, vy, vz, pad;
} VECTOR32;

/* Per-object movement bookkeeping (0x1B8 bytes, EGSMemMalloc'd). */
typedef struct MoveInfo {
    /* 0x000 */ VECTOR32 pos;
    /* 0x010 */ u8 unk010[0x70];
    /* 0x080 */ VECTOR32 rot; /* rotation angles */
    /* 0x090 */ u8 unk090[0x60];
    /* 0x0F0 */ VECTOR32 unkF0; /* angular push, applied by the movement update */
    /* 0x100 */ u8 unk100[9];
    /* 0x109 */ u8 unk109;
    /* 0x10A */ u8 unk10A[2];
    /* 0x10C */ VECTOR32 unk10C; /* position (y grows downwards) */
    /* 0x11C */ u8 unk11C[0x10];
    /* 0x12C */ s32 unk12C; /* height */
    /* 0x130 */ s32 unk130;
    /* 0x134 */ s32 unk134;
    /* 0x138 */ s32 unk138;
    /* 0x13C */ s32 unk13C;
    /* 0x140 */ u8 unk140[0x34];
    /* 0x174 */ s32 unk174;
    /* 0x178 */ u8 unk178[4];
    /* 0x17C */ s32 unk17C;
    /* 0x180 */ u8 unk180[0x10];
    /* 0x190 */ s32 unk190;
    /* 0x194 */ VECTOR32 pendingMove; /* applied by GameSysProcessMoveRequests */
    /* 0x1A4 */ u32 collFlags;
    /* 0x1A8 */ u8 unk1A8[8];
    /* 0x1B0 */ struct CollisionInfo *coll;
    /* 0x1B4 */ u8 unk1B4[4];
} MoveInfo; /* size 0x1B8 */

typedef struct EGSFrame {
    /* 0x00 */ s16 m[3][3]; /* rotation; column 2 is the facing direction */
    /* 0x12 */ s16 pad;
    /* 0x14 */ VECTOR32 pos; /* y grows downwards */
} EGSFrame;

typedef struct GameObjectRuntime Gobj;

/* Pending CAS action requests (also used for the global per-frame list). */
typedef struct CASQueue {
    /* 0x00 */ u8 count;
    /* 0x01 */ u8 max;
    /* 0x02 */ u8 pad02[2];
    /* 0x04 */ s32 *items;
    /* 0x08 */ struct CASSlots {
        s32 current; /* slot in use, -1 when idle */
        s32 prio[3];
        s32 state[3];
    } slots;
    /* 0x24 */ u8 unk24[0x48];
    /* 0x6C */ s16 unk6C;
} CASQueue;

typedef struct CASState {
    /* 0x00 */ u32 flags;
    /* 0x04 */ u32 flags2;
    /* 0x08 */ u8 active;
    /* 0x09 */ u8 mode;
    /* 0x0A */ u8 unk0A[2];
    /* 0x0C */ struct CASQueue *queue;
    /* 0x10 */ u8 unk10[4];
    /* 0x14 */ void *actionTable;
} CASState;

#define GOBJ_POOL_SIZE 100
#define GOBJ_MOVE_REQUEST_MAX 30

#endif /* GOBJ_H */
