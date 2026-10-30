#include "common.h"
#include "system.h"
#include "level.h"
#include "gameobj.h"
#include "gobj.h"

/* A collision probe (0x178 bytes); objects keep lists of them for pushing/grabbing. */
typedef struct CollisionDetector {
    /* 0x000 */ u8 unk000[0x14];
    /* 0x014 */ s32 unk14;
    /* 0x018 */ u8 unk18[0xC];
    /* 0x024 */ s32 unk24;
    /* 0x028 */ u8 unk28[4];
    /* 0x02C */ s32 unk2C;
    /* 0x030 */ s32 unk30;
    /* 0x034 */ u8 unk34[4];
    /* 0x038 */ u8 unk38;
    /* 0x039 */ u8 unk39[7];
    /* 0x040 */ s32 unk40;
    /* 0x044 */ u8 unk44[0x130];
    /* 0x174 */ struct CollisionDetector *next;
} CollisionDetector; /* size 0x178 */

/* Per-object collision state (MoveInfo.coll, 0x40 bytes). */
typedef struct CollisionInfo {
    /* 0x00 */ s32 mode; /* 1: grabbing */
    /* 0x04 */ s32 prevMode;
    /* 0x08 */ CollisionDetector *grab;
    /* 0x0C */ s32 unk0C;
    /* 0x10 */ s32 unk10;
    /* 0x14 */ s32 unk14;
    /* 0x18 */ u8 unk18[8];
    /* 0x20 */ s32 unk20;
    /* 0x24 */ CollisionDetector *push;
    /* 0x28 */ s32 unk28;
    /* 0x2C */ u8 unk2C[4];
    /* 0x30 */ s32 unk30;
    /* 0x34 */ CollisionDetector *pushList;
    /* 0x38 */ s32 unk38;
    /* 0x3C */ s32 unk3C;
} CollisionInfo; /* size 0x40 */

void *EGSMemMalloc(u32 size);
void FriendlyInit(void);
void func_8006D9F4(void);
s32 CollisionDetectorInitParams(CollisionDetector *det, s32 arg1, s32 arg2);
s32 CollisionDetectorAddToList(CollisionDetector *det, s32 pos, CollisionDetector **list);
s32 CollisionDetectorRemoveFromList(CollisionDetector *det, CollisionDetector **list);
s32 CollisionGobjSetModeGrab(Gobj *obj, CollisionDetector *grab);
s32 CollisionGobjSetMode(Gobj *obj, s32 *mode);
u8 CollisionGobjGetInfo(Gobj *obj, CollisionInfo **out);

INCLUDE_ASM("asm/main/nonmatchings/collision", func_8006B860);

INCLUDE_ASM("asm/main/nonmatchings/collision", func_8006BB88);

INCLUDE_ASM("asm/main/nonmatchings/collision", func_8006BD48);

INCLUDE_ASM("asm/main/nonmatchings/collision", func_8006C2D0);

INCLUDE_ASM("asm/main/nonmatchings/collision", func_8006C4B0);

INCLUDE_ASM("asm/main/nonmatchings/collision", func_8006C510);

INCLUDE_ASM("asm/main/nonmatchings/collision", func_8006C7E8);

INCLUDE_ASM("asm/main/nonmatchings/collision", func_8006CB88);

INCLUDE_ASM("asm/main/nonmatchings/collision", func_8006D9F4);

#ifdef NON_MATCHING
/* regalloc/branch layout; see permuter */
void CollisionGobjSetPushDetector(Gobj *obj, s32 which) {
    CollisionInfo *info;
    CollisionDetector *det;
    s32 best;
    s32 i;

    if (obj != NULL && obj->move != NULL && (info = obj->move->coll) != NULL) {
        if (which < 0) {
            best = 0x7FFFFFFF;
            for (det = info->pushList; det != NULL; det = det->next) {
                if (det->unk40 - det->unk14 < best) {
                    best = det->unk40 - det->unk14;
                    info->push = det;
                }
            }
        } else if (which == 0) {
            info->push = NULL;
        } else {
            det = info->pushList;
            for (i = 1; i != which && det != NULL; i++) {
                det = det->next;
            }
            info->push = det;
        }
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/collision", CollisionGobjSetPushDetector);
#endif

INCLUDE_ASM("asm/main/nonmatchings/collision", func_8006DDB4);

INCLUDE_ASM("asm/main/nonmatchings/collision", func_8006E0BC);

s32 CollisionInit(void) {
    FriendlyInit();
    return 1;
}

#ifdef NON_MATCHING
/* regalloc/branch layout; see permuter */
u8 CollisionGobjGetInfo(Gobj *obj, CollisionInfo **out) {
    u8 ok = 0;

    if (obj != NULL) {
        MoveInfo *move = obj->move;

        ok = 1;
        if (move == NULL) {
            return 0;
        }
        *out = move->coll;
    }
    return ok;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/collision", CollisionGobjGetInfo);
#endif

#ifdef NON_MATCHING
/* regalloc/branch layout; see permuter */
s32 CollisionGobjGetGrabInfo(Gobj *obj, CollisionDetector **out) {
    s32 ok = 0;

    if (obj != NULL && obj->move != NULL) {
        if (obj->move->coll == NULL) {
            return 0;
        }
        *out = obj->move->coll->grab;
        ok = 1;
    }
    return ok;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/collision", CollisionGobjGetGrabInfo);
#endif

#ifdef NON_MATCHING
/* regalloc/branch layout; see permuter */
s32 CollisionGobjSetGrabInfo(Gobj *obj, CollisionDetector *grab) {
    s32 ok = 0;

    if (obj != NULL && obj->move != NULL) {
        if (obj->move->coll == NULL) {
            return 0;
        }
        obj->move->coll->grab = grab;
        ok = 1;
    }
    return ok;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/collision", CollisionGobjSetGrabInfo);
#endif

s32 CollisionGobjSetMode(Gobj *obj, s32 *mode) {
    obj->move->coll->mode = *mode;
    return 1;
}

#ifdef NON_MATCHING
/* regalloc/branch layout; see permuter */
s32 CollisionGobjBasicInit(Gobj *obj, s32 height, s32 a, s32 b, s32 c, s32 d) {
    s32 ok = 0;
    MoveInfo *move;

    if (obj != NULL) {
        move = obj->move;
        if (move == NULL) {
            return 0;
        }
        move->unk109 = 2;
        move->unk12C = height;
        if (a < 0) {
            a = 0;
        }
        if (b < 0) {
            b = 0x1000;
        }
        if (c < 0) {
            c = 0;
        }
        if (d < 0) {
            d = 0x1000;
        }
        move->unk130 = a;
        move->unk134 = b;
        move->unk138 = c;
        move->unk13C = d;
        move->collFlags = 0;
        ok = 1;
    }
    return ok;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/collision", CollisionGobjBasicInit);
#endif

#ifdef NON_MATCHING
/* regalloc/branch layout; see permuter */
s32 CollisionGobjInit(Gobj *obj, s32 *mode) {
    MoveInfo *move;

    if (obj != NULL && obj->move != NULL) {
        CollisionGobjBasicInit(obj, 0x80000001, -1, -1, -1, -1);
        move = obj->move;
        move->coll = EGSMemMalloc(sizeof(CollisionInfo));
        if (move->coll != NULL) {
            move->coll->grab = NULL;
            move->coll->unk0C = 0;
            move->coll->unk10 = 0;
            move->coll->unk14 = 0;
            move->coll->unk20 = 0;
            move->coll->push = NULL;
            move->coll->unk28 = 0;
            move->coll->unk30 = 0;
            move->coll->pushList = NULL;
            move->coll->unk38 = 0;
            move->coll->unk3C = 0;
            CollisionGobjSetMode(obj, mode);
            return 1;
        }
    }
    return 0;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/collision", CollisionGobjInit);
#endif

INCLUDE_ASM("asm/main/nonmatchings/collision", CollisionDetectorInitParams);

s32 CollisionDetectorCreate(CollisionDetector **out, s32 arg1, s32 arg2) {
    if (out != NULL) {
        *out = EGSMemMalloc(sizeof(CollisionDetector));
        if (*out != NULL) {
            CollisionDetectorInitParams(*out, arg1, arg2);
            return 1;
        }
    }
    return 0;
}

s32 CollisionGobjGetPushDetectorList(Gobj *obj, CollisionDetector **out) {
    CollisionInfo *info;
    u8 ok = CollisionGobjGetInfo(obj, &info);

    if (info == NULL || !ok) {
        return 0;
    }
    *out = info->pushList;
    return 1;
}

s32 CollisionGobjSetPushDetectorList(Gobj *obj, CollisionDetector *list) {
    CollisionInfo *info;
    u8 ok = CollisionGobjGetInfo(obj, &info);

    if (info == NULL || !ok) {
        return 0;
    }
    info->pushList = list;
    return 1;
}

s32 CollisionDetectorAddToList(CollisionDetector *det, s32 pos, CollisionDetector **list) {
    CollisionDetector *prev = NULL;
    CollisionDetector *cur = *list;
    s32 i = 0;

    while (cur != NULL && (i < pos || pos < 0)) {
        prev = cur;
        cur = cur->next;
        i++;
    }
    if (prev != NULL) {
        prev->next = det;
    } else {
        *list = det;
    }
    det->next = cur;
    return 1;
}

#ifdef NON_MATCHING
/* regalloc/branch layout; see permuter */
s32 CollisionDetectorRemoveFromList(CollisionDetector *det, CollisionDetector **list) {
    CollisionDetector *cur = *list;

    if (cur == det) {
        *list = det->next;
    } else {
        while (cur != NULL && cur->next != det) {
            cur = cur->next;
        }
        if (cur == NULL) {
            return 0;
        }
        cur->next = det->next;
    }
    det->next = NULL;
    return 1;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/collision", CollisionDetectorRemoveFromList);
#endif

s32 CollisionDetectorReorderInList(CollisionDetector *det, s32 pos, CollisionDetector **list) {
    CollisionDetectorRemoveFromList(det, list);
    CollisionDetectorAddToList(det, pos, list);
    return 1;
}

#ifdef NON_MATCHING
/* regalloc/branch layout; see permuter */
s32 CollisionGobjGetMode(Gobj *obj, s32 *out) {
    s32 ok = 0;

    if (obj != NULL && obj->move != NULL) {
        if (obj->move->coll == NULL) {
            return 0;
        }
        *out = obj->move->coll->mode;
        ok = 1;
    }
    return ok;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/collision", CollisionGobjGetMode);
#endif

#ifdef NON_MATCHING
/* regalloc/branch layout; see permuter */
s32 CollisionGobjSetModeGrab(Gobj *obj, CollisionDetector *grab) {
    s32 ok = 0;
    CollisionInfo *info;

    if (obj != NULL) {
        if (obj->move == NULL || obj->move->coll == NULL) {
            return 0;
        }
        obj->move->coll->grab = grab;
        if (grab == NULL) {
            info = obj->move->coll;
            if (info->mode == 1) {
                info->mode = info->prevMode;
            }
        } else {
            grab->unk2C = 0;
            info = obj->move->coll;
            if (info->mode != 1) {
                info->prevMode = info->mode;
            }
            obj->move->coll->mode = 1;
        }
        ok = 1;
    }
    return ok;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/collision", CollisionGobjSetModeGrab);
#endif

INCLUDE_ASM("asm/main/nonmatchings/collision", func_8006EABC);

INCLUDE_ASM("asm/main/nonmatchings/collision", CollisionGobjSetModeEdgeGrab);

void CollisionGobjReleaseModeGrabCAS(s16 id) {
    CollisionGobjSetModeGrab(gameObjectsInfo[id].runtime, NULL);
}

s32 func_8006F49C(void) {
    func_8006D9F4();
    return 1;
}

INCLUDE_ASM("asm/main/nonmatchings/collision", func_8006F4BC);

void func_8006F738(Gobj *obj, s32 speed) {
    if (obj->move->collFlags & 4) {
        obj->move->collFlags &= ~0x100;
    }
    if (obj->move->collFlags & 0x100000) {
        obj->move->collFlags &= ~0x80000;
    }
    if (speed <= 0x40000 || (obj->move->collFlags & 4)) {
        if (!(obj->move->collFlags & 0x100000)) {
            obj->move->collFlags |= 0x80000;
        }
    }
    if (speed > 0x40000) {
        if (!(obj->move->collFlags & 4)) {
            obj->move->collFlags |= 0x100;
        }
    }
}

INCLUDE_ASM("asm/main/nonmatchings/collision", func_8006F810);

INCLUDE_ASM("asm/main/nonmatchings/collision", CollisionGobjBasicUpdate_Human);

INCLUDE_ASM("asm/main/nonmatchings/collision", CollisionGobjBasicUpdate_NonHuman);

INCLUDE_ASM("asm/main/nonmatchings/collision", CollisionGobjUpdate);

INCLUDE_ASM("asm/main/nonmatchings/collision", func_8007112C);

INCLUDE_ASM("asm/main/nonmatchings/collision", func_80071E04);

INCLUDE_ASM("asm/main/nonmatchings/collision", func_800721EC);

INCLUDE_ASM("asm/main/nonmatchings/collision", func_80072AE0);

INCLUDE_RODATA("asm/main/nonmatchings/collision", D_800122C4);
