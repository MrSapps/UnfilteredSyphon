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

/* Collision physics state (fields used here) */
typedef struct CobjPhysics {
    /* 0x00 */ u8 unk00[0x24];
    /* 0x24 */ s32 minNormalY; /* contacts with a flatter normal count as floor */
} CobjPhysics;

/* Contact found by the collision detector */
typedef struct CobjContact {
    /* 0x00 */ u8 unk00[0x44];
    /* 0x44 */ VECTOR32 normal;
} CobjContact;

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

#ifdef NON_MATCHING
/* Scheduling only: the first EGSMathFIXMul result is stored in the next call's delay slot */
/* Push out along the averaged normal of the floor-like contacts. */
void func_80072E34(CobjPhysics *phys, CobjContact **contacts, s32 count, VECTOR32 *out) {
    VECTOR32 sum = { 0, 0, 0, 0 };
    VECTOR32 push;
    s32 len;
    s32 i;
    s32 n = 0;
    s32 scale;

    for (i = 0; i < count; i++) {
        if (contacts[i]->normal.vy >= phys->minNormalY) {
            sum.vx += contacts[i]->normal.vx;
            sum.vy += contacts[i]->normal.vy;
            n++;
            sum.vz += contacts[i]->normal.vz;
        }
    }
    if (n > 0) {
        scale = 0xCCCC;
        if (n >= 2) {
            EGSMath_VectorModulus(&sum, &len);
            scale = 0x0CCCC000 / len;
        }
        for (i = 0; i < count; i++) {
            if (contacts[i]->normal.vy >= phys->minNormalY) {
                push.vx = EGSMathFIXMul(contacts[i]->normal.vx, -scale);
                push.vy = EGSMathFIXMul(contacts[i]->normal.vy, -scale);
                push.vz = EGSMathFIXMul(contacts[i]->normal.vz, -scale);
                out->vx += push.vx;
                out->vy += push.vy;
                out->vz += push.vz;
            }
        }
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/cobj", func_80072E34);
#endif

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

typedef struct LOSMtx {
    /* 0x00 */ s16 m[3][3];
    /* 0x12 */ s16 pad;
    /* 0x14 */ s32 t[3];
} LOSMtx;

/* HMD model header: boxes per part */
typedef struct HMDHeader {
    /* 0x00 */ s32 unk00;
    /* 0x04 */ s32 last; /* number of boxes - 1 */
    /* 0x08 */ u8 unk08[0xC];
    /* 0x14 */ s32 boxOffset; /* from the header, 0x20 bytes per box */
} HMDHeader;

typedef struct HMDLOSModel {
    /* 0x00 */ u8 unk00[0x20];
    /* 0x20 */ HMDHeader *hmd;
} HMDLOSModel;

/* GameObjectAnim as seen by the LOS code */
typedef struct HMDLOSAnim {
    /* 0x00 */ u8 unk00[0xC];
    /* 0x0C */ LOSMtx *frame;
    /* 0x10 */ HMDLOSModel *model;
    /* 0x14 */ u16 id; /* gameObjectsInfo index in the low 10 bits */
    /* 0x16 */ u8 unk16[2];
    /* 0x18 */ LOSMtx **frames;
} HMDLOSAnim;

/* Face handed to the GTE intersection code */
typedef struct LOSAnimFaceState {
    /* 0x00 */ s32 kind; /* 3: triangle, 4: quad */
    /* 0x04 */ s32 unk04;
    /* 0x08 */ void *unk08;
    /* 0x0C */ s32 unk0C;
    /* 0x10 */ s32 unk10;
    /* 0x14 */ s16 *v[4];
} LOSAnimFaceState;

/* Terrain face (negative first word: quad) */
typedef struct LOSTerrFace {
    /* 0x00 */ s32 flags;
    /* 0x04 */ u8 unk04[3];
    /* 0x07 */ u8 v0;
    /* 0x08 */ u8 v3;
    /* 0x09 */ u8 unk09;
    /* 0x0A */ u8 v1;
    /* 0x0B */ u8 v2;
} LOSTerrFace;

/* Terrain face hit by the sector pass */
typedef struct LOSSectorHit {
    /* 0x00 */ LOSTerrFace *face;
    /* 0x04 */ u8 *verts;
} LOSSectorHit; /* size 8 */

extern LOSSectorHit losSectorHitArray[];
extern u32 losSectorHitCount;

void GobjLOSLoop(void *query, s32 type, u8 arg2);
void LOSVerifyHits(void *query);
s32 LOSGeometry_GetIntersectLine2AnimFacePlane(void *ray, LOSAnimFaceState *face, LOSVec *point, LOSVec *normal);

/* Only the field used here; see weapon.c for the full WeaponInfo. */
typedef struct CobjWeaponInfo {
    /* 0x00 */ char *name;
    /* 0x04 */ void *masterAnim;
    /* 0x08 */ u8 unk08[0x18];
} CobjWeaponInfo; /* size 0x20 */

extern CobjWeaponInfo gWeaponInfo[];
extern LOSAnimFaceState losAnimFace;
extern LOSHit *losBlocker;
extern u8 losRay[];
void LOSSetupGTE(LOSMtx *mtx);
s32 LOSRotBoxV(void *bxv, s32 z, LOSHit *hit, void *ray);

/* Weapon an object holds, by its GameObjectInfo weapon word */
#define HMDLOS_WEAPON(info)                                                                    \
    (*(u8 *)&(info)->flags24 != 0 ? *(u8 *)&(info)->flags24                                    \
     : (*(u32 *)&(info)->flags24 & 0x3000) != 0x1000                                           \
         ? ((*(u32 *)&(info)->flags24 & 0x3000) == 0x2000 ? 0x14 : 0)                          \
         : 0x13)

/* Line of sight against an HMD model's boxes (last box: the held weapon); bit per box hit. */
s32 HMDLOSLoop(HMDLOSAnim *anim) {
    u8 bxv[0x48];
    LOSHit hit;
    HMDLOSAnim owner;
    s32 last = anim->model->hmd->last;
    s32 i;
    s32 hits;
    LOSMtx *mtx;
    void *box;

    hit.owner = (LOSHitOwner *)&owner;
    losAnimFace.kind = 3;
    losAnimFace.unk08 = &losAnimFace.unk0C;
    hits = 0;
    for (i = last; i >= 0; i--) {
        mtx = anim->frames[i];
        box = (u8 *)anim->model->hmd + anim->model->hmd->boxOffset + i * 32;
        if (i == last) {
            box = gWeaponInfo[HMDLOS_WEAPON(&gameObjectsInfo[gameObjectsInfo[anim->id & 0x3FF].runtime->id])].masterAnim;
            if (box == NULL) {
                hits <<= 1;
                continue;
            }
            mtx = anim->frames[8];
        }
        owner.frame = mtx;
        InitBxv2(bxv, box, 0);
        LOSSetupGTE(mtx);
        hits <<= 1;
        if (LOSRotBoxV(bxv, losEndPointZ, &hit, &losRay)) {
            losBlocker->point = hit.point;
            hits |= 1;
            losBlocker->normal = hit.normal;
        }
    }
    return hits;
}

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

void GTELOSFinish(void *query, u8 windows, u8 arg2) {
    u32 i;
    LOSSectorHit *sh;
    LOSTerrFace *face;
    LOSHit *hit;
    s32 kind;

    for (i = 0; i < losSectorHitCount; i++) {
        sh = &losSectorHitArray[i];
        face = sh->face;
        kind = 3;
        if (face->flags < 0) {
            kind = 4;
        }
        losAnimFace.kind = kind;
        losAnimFace.unk08 = &losAnimFace.unk0C;
        hit = &losHitArray[losHitCount];
        if (kind == 3) {
            losAnimFace.v[0] = (s16 *)(sh->verts + (face->v0 << 3) / 3);
            losAnimFace.v[1] = (s16 *)(sh->verts + (face->v1 << 3) / 3);
            losAnimFace.v[2] = (s16 *)(sh->verts + (face->v2 << 3) / 3);
        } else if (kind == 4) {
            losAnimFace.v[0] = (s16 *)(sh->verts + (face->v0 << 3) / 3);
            losAnimFace.v[1] = (s16 *)(sh->verts + (face->v1 << 3) / 3);
            losAnimFace.v[2] = (s16 *)(sh->verts + (face->v3 << 3) / 3);
            losAnimFace.v[3] = (s16 *)(sh->verts + (face->v2 << 3) / 3);
        }
        if (LOSGeometry_GetIntersectLine2AnimFacePlane(losRay, &losAnimFace, &hit->point, &hit->normal) == 0) {
            hit->window = face;
            hit->type = 0;
            hit->box = NULL;
            losHitCount++;
#line 1724
            ASSERT(losHitCount < LOS_HIT_MAX);
        }
    }
    GobjLOSLoop(query, 1, arg2);
    if (windows) {
        GobjLOSLoop(query, 2, 0);
    }
    LOSVerifyHits(query);
}

INCLUDE_ASM("asm/main/nonmatchings/cobj", GTELOSLoop);

/* D_800122F4: "s/cobj.c", now emitted by PreSortHits (see config/syms.d/main/cobj.ld) */
