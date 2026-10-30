#include "common.h"
#include "game.h"
#include "gameobj.h"
#include "gobj.h"

/* cobj.c's small data, still assembled from asm */
SDATA_LOCAL(D_8010F9C0)
SDATA_LOCAL(losBlocker)
SDATA_LOCAL(losEndPointZ)
SDATA_LOCAL(losGCount)
SDATA_LOCAL(losHitCount)
SDATA_LOCAL(losInvMatrix)
SDATA_LOCAL(losSectorHitCount)
SDATA_LOCAL(losSortedCount)

/* Entry of a collision box list (0x14 bytes). */
typedef struct CobjNode {
    /* 0x00 */ Gobj *gobj;
    /* 0x04 */ u8 type;
    /* 0x05 */ u8 unk05;
    /* 0x06 */ u8 flags;
    /* 0x07 */ u8 pooled; /* 1: allocated from the free list */
    /* 0x08 */ u8 unk08;
    /* 0x09 */ u8 pad09[3];
    /* 0x0C */ s32 unk0C;
    /* 0x10 */ struct CobjNode *next;
} CobjNode;

extern CobjNode *D_8010F9C0; /* free list */
extern CobjNode *D_80128A24; /* bullet collision boxes */
extern CobjNode *D_80128A28; /* windows */
extern u8 mFaceCollisionSectors[];

typedef struct CobjSector {
    /* 0x00 */ u8 unk00[2];
    /* 0x02 */ s16 id;
} CobjSector;

typedef struct CobjHotBoxDisplay {
    /* 0x00 */ u8 unk00[0x20];
    /* 0x20 */ CobjSector *sector; /* -1 for none */
} CobjHotBoxDisplay;

typedef struct CobjHotBoxModel {
    /* 0x00 */ u8 unk00[0x10];
    /* 0x10 */ CobjHotBoxDisplay *display;
} CobjHotBoxModel;

typedef struct LOSVec {
    /* 0x00 */ s32 vx;
    /* 0x04 */ s32 vy;
    /* 0x08 */ s32 vz;
    /* 0x0C */ s32 pad;
} LOSVec;

typedef struct LOSHitOwner {
    /* 0x00 */ u8 unk00[0x14];
    /* 0x14 */ u16 id; /* gameObjectsInfo index in the low 10 bits */
} LOSHitOwner;

/* Line of sight hit (same layout as in gtegeo.c) */
typedef struct LOSHit {
    /* 0x00 */ s32 unk00;
    /* 0x04 */ u32 z;
    /* 0x08 */ s32 type; /* 0: terrain, 1: game object, 2: window */
    /* 0x0C */ void *box;
    /* 0x10 */ LOSVec normal;
    /* 0x20 */ LOSVec point;
    /* 0x30 */ u8 unk30[0x18];
    /* 0x48 */ LOSHitOwner *owner;
    /* 0x4C */ void *window;
} LOSHit; /* size 0x50 */

extern s16 losEndPointZ;

/* Objects the camera may not look through (doors and the like). */
static inline s32 LOSIsCameraBlocker(s32 type, s32 npcState) {
    s32 ret = 0;

    if (type == 0x2C || type == 0x62 || type == 0x38 || type == 0x1E || npcState == 9) {
        ret = 1;
    }
    return ret;
}

#define LOS_HIT_MAX 0x1A

extern u32 losHitCount;
extern u32 losSortedCount;
extern LOSHit losHitArray[LOS_HIT_MAX];
extern LOSHit *losHitsSorted[LOS_HIT_MAX];

void LOSSortHits(void);
s32 EGSMathFIXDiv(s32 a, s32 b);
/* Terrain sector (gTerrSector[], 0x3C bytes) with its collision state */
typedef struct CobjTerrSector {
    /* 0x00 */ void *model;
    /* 0x04 */ u8 unk04[0x10];
    /* 0x14 */ void *collisionModel;
    /* 0x18 */ u8 unk18;
    /* 0x19 */ u8 unk19;
    /* 0x1A */ u8 unk1A;
    /* 0x1B */ u8 unk1B;
    /* 0x1C */ u8 unk1C;
    /* 0x1D */ u8 pad1D[3];
    /* 0x20 */ s32 unk20;
    /* 0x24 */ s32 unk24;
    /* 0x28 */ s32 unk28[5];
} CobjTerrSector; /* size 0x3C */

#define COBJ_NODE_MAX 150

extern CobjNode D_8011EBAC[COBJ_NODE_MAX - 1]; /* node pool (entry 0 unused) */
extern CobjTerrSector gCobjDummySector_RotBox;
extern CobjTerrSector gCobjDummySector_Gobj;
extern CobjTerrSector *gTerrSector;
extern s32 gTerrSectorCount;

void EGSMath_VectorModulus(void *v, s32 *out);
s32 EGSMathFIXMul(s32 a, s32 b);
void InitBxv2(void *bxv, void *model, s32 id);
s32 func_8007369C(CobjNode *node, CobjNode **list);
s32 func_800736EC(CobjNode *src, CobjNode **list);
s32 func_800737B8(Gobj *gobj, CobjNode **list);

s32 iCollisionPhysicsFaceGetFrictionForceValue(u8 *physics, s32 *out) {
    s32 modulus;

    EGSMath_VectorModulus(physics + 0x68, &modulus);
    *out = EGSMathFIXMul(*(s32 *)(physics + 0x84), modulus);
    return 1;
}

#ifdef NON_MATCHING
/* Scheduling only: the original stores each EGSMathFIXDiv result after the next
 * call's first argument load (same pattern as los.c). */
s32 iCollisionPhysicsGetKineticFrictionForce(VECTOR32 *v, s32 force, VECTOR32 *out) {
    s32 modulus;

    EGSMath_VectorModulus(v, &modulus);
    if (force < modulus) {
        out->vx = EGSMathFIXMul(v->vx, -force);
        out->vy = EGSMathFIXMul(v->vy, -force);
        out->vz = EGSMathFIXMul(v->vz, -force);
        out->vx = EGSMathFIXDiv(out->vx, modulus);
        out->vy = EGSMathFIXDiv(out->vy, modulus);
        out->vz = EGSMathFIXDiv(out->vz, modulus);
    } else {
        out->vx = -v->vx;
        out->vy = -v->vy;
        out->vz = -v->vz;
    }
    return 1;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/cobj", iCollisionPhysicsGetKineticFrictionForce);
#endif

INCLUDE_RODATA("asm/main/nonmatchings/cobj", D_800122D4);

INCLUDE_ASM("asm/main/nonmatchings/cobj", func_80072E34);

INCLUDE_ASM("asm/main/nonmatchings/cobj", CollisionPhysicsGobj_DetectorMotion);

s32 func_8007369C(CobjNode *node, CobjNode **list) {
    CobjNode *n;

    for (n = *list; n != NULL && n != node; n = n->next) {
    }
    if (n == node) {
        return 0;
    }
    node->next = *list;
    *list = node;
    return 1;
}

s32 func_800736EC(CobjNode *src, CobjNode **list) {
    CobjNode *n;
    CobjNode *node;

    if (D_8010F9C0 != NULL) {
        for (n = *list; n != NULL && n->gobj != src->gobj; n = n->next) {
        }
        if (n->gobj != src->gobj) {
            node = D_8010F9C0;
            D_8010F9C0 = node->next;
            *node = *src;
            node->pooled = 1;
            func_8007369C(node, list);
            return 1;
        }
    }
    return 0;
}

s32 func_800737B8(Gobj *gobj, CobjNode **list) {
    CobjNode *n;
    CobjNode *prev = NULL;

    for (n = *list; n != NULL && n->gobj != gobj; n = n->next) {
        prev = n;
    }
    if (n == NULL) {
        return 0;
    }
    if (prev == NULL) {
        *list = n->next;
    } else {
        prev->next = n->next;
    }
    if (n->pooled == 1) {
        n->next = D_8010F9C0;
        D_8010F9C0 = n;
    } else {
        n->next = NULL;
    }
    return 1;
}

s32 CobjAddBulletCollisionBoxGobj(Gobj *gobj, u8 arg1, s32 arg2, u8 arg3) {
    CobjNode node;
    u8 flags = (arg3 != 0) << 1;

    node.gobj = gobj;
    node.unk05 = arg1;
    node.type = 2;
    node.flags = flags;
    node.pooled = 0;
    node.unk0C = arg2;
    node.unk08 = 0;
    node.next = NULL;
    func_800736EC(&node, &D_80128A24);
    return 1;
}

s32 CobjAddWindowGobj(Gobj *gobj, u8 arg1, s32 arg2, u8 arg3) {
    CobjNode node;
    u8 flags = (arg3 != 0) << 1;

    node.gobj = gobj;
    node.unk05 = arg1;
    node.type = 2;
    node.flags = flags;
    node.pooled = 0;
    node.unk0C = arg2;
    node.unk08 = 0;
    node.next = NULL;
    func_800736EC(&node, &D_80128A28);
    return 1;
}

s32 CobjAddCollisionBoxGobj(Gobj *gobj, s32 arg1, s32 arg2, u8 solid, u8 bullets) {
    if (solid) {
        gobj->anim->unk00[0xB] |= 2;
    }
    if (bullets) {
        gobj->anim->unk00[0xB] |= 4;
    }
    return 1;
}

s32 CobjRemoveCollisionBoxGobj(Gobj *gobj) {
    gobj->anim->unk00[0xB] &= ~6;
    return 1;
}

s32 CobjRemoveBulletCollisionBoxGobj(Gobj *gobj) {
    func_800737B8(gobj, &D_80128A24);
    return 1;
}

s32 CobjRemoveWindowGobj(Gobj *gobj) {
    func_800737B8(gobj, &D_80128A28);
    return 1;
}

#ifdef NON_MATCHING
/* One addu operand order (ts = base + i*0x3C) differs; the per-field gTerrSector reloads
 * suggest the original grouped some fields differently. */
s32 CobjInit(void) {
    s32 i;
    s32 j;
    CobjNode *node;
    CobjTerrSector *ts;

    D_8010F9C0 = NULL;
    for (i = 1, node = D_8011EBAC; i < COBJ_NODE_MAX; i++) {
        func_8007369C(node, &D_8010F9C0);
        node++;
    }
    gCobjDummySector_RotBox.model = NULL;
    gCobjDummySector_RotBox.collisionModel = NULL;
    for (i = 4; i >= 0; i--) {
        gCobjDummySector_RotBox.unk28[i] = 0;
    }
    gCobjDummySector_Gobj.model = NULL;
    gCobjDummySector_Gobj.collisionModel = NULL;
    for (i = 4; i >= 0; i--) {
        gCobjDummySector_Gobj.unk28[i] = 0;
    }
    for (i = 0; i < gTerrSectorCount; i++) {
        gTerrSector[i].collisionModel = gTerrSector[i].model;
        gTerrSector[i].unk18 = 3;
        gTerrSector[i].unk19 = 1;
        gTerrSector[i].unk1A = 0;
        gTerrSector[i].unk1B = 0;
        ts = &gTerrSector[i];
        ts->unk1C = 0;
        ts->unk20 = 0;
        gTerrSector[i].unk24 = 0;
        for (j = 4; j >= 0; j--) {
            gTerrSector[i].unk28[j] = 0;
        }
    }
    return 1;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/cobj", CobjInit);
#endif

INCLUDE_ASM("asm/main/nonmatchings/cobj", CobjHandleHotBoxLosXZ);

s32 CobjHandleHotBoxXZ(CobjHotBoxModel **box, s32 arg1, u8 *out) {
    CobjSector *sector = (*box)->display->sector;

    if (sector == (CobjSector *)-1) {
        *out = 0;
    } else {
        *out = mFaceCollisionSectors[sector->id] != 0;
    }
    return 1;
}

INCLUDE_ASM("asm/main/nonmatchings/cobj", CobjHandleHotBox);

INCLUDE_ASM("asm/main/nonmatchings/cobj", CobjLoop);

INCLUDE_ASM("asm/main/nonmatchings/cobj", HMDLOSLoop);

s32 LOSGetCameraBlocker(LOSVec *point, LOSVec *normal) {
    u32 i;
    LOSHit *hit;
    LOSHit *blocker = NULL;
    s16 id;

    for (i = 0; i < losSortedCount; i++) {
        hit = losHitsSorted[i];
        if (hit->z >= losEndPointZ) {
            break;
        }
        if (hit->type == 0) {
            blocker = hit;
            break;
        }
        if (hit->type == 1 && (id = hit->owner->id) != 0 &&
            LOSIsCameraBlocker(GOBJ_TYPE(gameObjectsInfo[id & 0x3FF].runtime->id),
                               gameObjectsInfo[hit->owner->id & 0x3FF].runtime->npcState)) {
            blocker = hit;
            break;
        }
    }
    if (blocker != NULL) {
        *point = blocker->point;
        *normal = blocker->normal;
        return 1;
    }
    return 0;
}

s32 LOSGetSortOrder(void) {
    s32 nearZ = -1;
    s32 farZ = -1;
    u8 nearClose = 0;
    u8 farClose = 0;
    u32 i = 0;
    s32 endZ = losEndPointZ - 0x10;
    LOSHit *hit;
    Gobj *gobj;
    u8 blocks;

    for (; i < losSortedCount; i++) {
        hit = losHitsSorted[i];
        if (endZ < (s32)hit->z) {
            if (hit->normal.vy < 500) {
                farZ = hit->z;
                break;
            }
        } else if ((s32)hit->z < endZ) {
            blocks = 1;
            if (hit->type == 1) {
                gobj = gameObjectsInfo[hit->owner->id & 0x3FF].runtime;
                blocks = LOSIsCameraBlocker(GOBJ_TYPE(gobj->id), gobj->npcState);
            }
            if (blocks) {
                nearZ = hit->z;
            }
        }
    }
    if (nearZ >= 0 && endZ - nearZ < 200) {
        nearClose = 1;
    }
    if (farZ >= 0 && farZ - endZ < 200) {
        farClose = 1;
    }
    if (farClose && nearClose) {
        return 0;
    }
    if (nearClose) {
        return 1;
    }
    return -1;
}

INCLUDE_ASM("asm/main/nonmatchings/cobj", InitBxv2);

void InitBxv(u8 *model) {
    InitBxv2(model + 0x30, model, (*(u32 *)(model + 0x28) & 0x8000) ? 0x29A : 0);
}

void PreSortHits(void) {
    u32 i;

    LOSSortHits();
    losHitCount = 0;
    for (i = 0; i < losSortedCount; i++) {
        losHitArray[i] = *losHitsSorted[i];
    }
#line 1658
    ASSERT(losHitCount < LOS_HIT_MAX);
}

INCLUDE_ASM("asm/main/nonmatchings/cobj", GTELOSFinish);

INCLUDE_ASM("asm/main/nonmatchings/cobj", GTELOSLoop);

/* D_800122F4: "s/cobj.c", now emitted by PreSortHits (see config/syms.d/main/cobj.ld) */
