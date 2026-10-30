#include "common.h"
#include "game.h"
#include "gameobj.h"
#include "gobj.h"
#include "scratch.h"

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
/* Model bounds (first 0x20 bytes of a box model) */
typedef struct CobjBounds {
    /* 0x00 */ LOSVec min;
    /* 0x10 */ LOSVec max;
} CobjBounds;

typedef struct BxvVert {
    s16 vx, vy, vz, pad;
} BxvVert;

/* Rotated box for LOS/collision tests */
typedef struct CobjBxv {
    /* 0x00 */ s32 unk00;
    /* 0x04 */ BxvVert verts[8]; /* verts[0].pad: radius */
    /* 0x44 */ s32 unk44;
} CobjBxv;

void InitBxv2(CobjBxv *bxv, CobjBounds *model, s32 pad);
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

typedef void (*CobjNodeCallback)(CobjNode *node, s32 kind, s32 arg);

/* Collision query run by CobjLoop */
typedef struct CobjQuery {
    /* 0x00 */ s16 sector;
    /* 0x04 */ s32 kind;
    /* 0x08 */ u8 stopOnHit;
    /* 0x0C */ void (*sectorFilter)(void *model, void *self, u8 *ok);
    /* 0x10 */ u8 unk10[0x88];
    /* 0x98 */ void (*nodeFilter)(CobjNode *node, void *self, u8 *ok);
    /* 0x9C */ s32 arg;
} CobjQuery;

s32 HotboxCallback();
extern u8 *gLevData;
void EGSDisplayGetSectorRotBoxList(void *model, u32 *count, u32 **boxes);

#ifdef NON_MATCHING
/* Sector selection: the original tests -3/-2/-1 as a linear chain and indexes links without strength reduction */
static inline CobjTerrSector *CobjGetTerrSector(s32 sector) {
    CobjTerrSector *ts;

    if (sector < gTerrSectorCount && sector >= 0) {
        ts = &gTerrSector[sector];
    } else {
        ts = NULL;
    }
    return ts;
}

/* Run a collision query over the dummy sectors (game objects, rotated boxes), the query's
 * sector and its neighbours. Kinds 0 and 4 of a terrain sector walk its rotated boxes. */
s32 CobjLoop(CobjQuery *q) {
    u8 ok;
    CobjNode node;
    u32 count;
    u32 *boxes;
    s32 i = -3;
    u8 *links = gLevData + q->sector * 15 + 0x80;
    CobjTerrSector *ts;
    CobjNode *list;
    u8 hasList;
    u8 done;
    u32 idx;
    s32 off;
    s32 kind;
    u32 *box;

    for (;;) {
        switch (i) {
        case -3:
            ts = &gCobjDummySector_Gobj;
            break;
        case -2:
            ts = &gCobjDummySector_RotBox;
            break;
        case -1:
            ts = CobjGetTerrSector(q->sector);
            break;
        default:
            ts = CobjGetTerrSector(links[i]);
            break;
        }
        if (ts == NULL) {
            return 1;
        }
        if (q->sectorFilter != NULL && ts->collisionModel != NULL) {
            q->sectorFilter(&ts->collisionModel, &q->sectorFilter, &ok);
        } else {
            ok = 1;
        }
        hasList = 0;
        idx = 0;
        if (ok) {
            kind = q->kind;
            node.gobj = NULL;
            node.type = 1;
            node.unk05 = 1;
            node.flags = 0;
            node.pooled = 0;
            node.unk0C = 0;
            node.unk08 = 0;
            node.next = NULL;
            if (i >= -1 && (kind == 4 || kind == 0)) {
                hasList = 1;
                list = &node;
                EGSDisplayGetSectorRotBoxList(ts->model, &count, &boxes);
            } else {
                list = (CobjNode *)ts->unk28[kind];
            }
            if (list != NULL) {
                off = idx * 0x48;
                do {
                    done = 0;
                    if (hasList) {
                        for (; idx < count; idx++, off += 0x48) {
                            box = (u32 *)((u8 *)boxes + off);
                            node.gobj = (Gobj *)box;
                            if (kind == 0) {
                                if (*box & 1) {
                                    node.unk0C = 0;
                                    break;
                                }
                            } else if (kind == 4 && (*box & 0x10)) {
                                node.unk0C = (s32)HotboxCallback;
                                list->unk08 = (*box >> 9) & 1;
                                break;
                            }
                        }
                        if (idx >= count) {
                            done = 1;
                        }
                    }
                    if (done) {
                        break;
                    }
                    if (q->nodeFilter != NULL) {
                        ok = 0;
                        q->nodeFilter(list, &q->nodeFilter, &ok);
                        if ((CobjNodeCallback)list->unk0C != NULL) {
                            if (ok ? list->unk08 == 0 : list->unk08 == 1) {
                                ((CobjNodeCallback)list->unk0C)(list, q->kind, q->arg);
                            }
                        }
                        if (ok && q->stopOnHit == 1) {
                            return 1;
                        }
                    }
                    if (hasList) {
                        off += 0x48;
                        idx++;
                    } else {
                        list = list->next;
                    }
                } while (list != NULL);
            }
        }
        i++;
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/cobj", CobjLoop);
#endif

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
        InitBxv2((CobjBxv *)bxv, box, 0);
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

/* Build a box's 8 corners from its model bounds. pad > 0 grows the box on every axis,
 * pad < 0 only along the thinnest axis (-666: doors, taller); 0/666: just the radius. */
void InitBxv2(CobjBxv *bxv, CobjBounds *model, s32 pad) {
    CobjBounds b;
    LOSVec size;
    BxvVert *v = bxv->verts;
    s32 small;
    u8 all;
    s32 m;

    b = *model;
    small = 0;
    if (pad != 0 && pad != GOBJ_ID_666) {
        all = pad > 0;
        if (!all) {
            size.vx = b.max.vx - b.min.vx;
            size.vy = b.max.vy - b.min.vy;
            size.vz = b.max.vz - b.min.vz;
            pad = -pad;
            if (pad == GOBJ_ID_666) {
                b.max.vy += 0xA0;
                pad = 0x50;
                all = 1;
            }
            m = size.vy;
            if (size.vz < m) {
                m = size.vz;
            }
            small = size.vx;
            if (m < size.vx) {
                small = m;
            }
        }
        if (all || small == size.vx) {
            b.max.vx += pad;
            b.min.vx -= pad;
        }
        if (all || small == size.vy) {
            b.max.vy += pad;
            b.min.vy -= pad;
        }
        if (all || small == size.vz) {
            b.max.vz += pad;
            b.min.vz -= pad;
        }
    } else {
        size.vx = b.max.vx - b.min.vx;
        size.vy = b.max.vy - b.min.vy;
        size.vz = b.max.vz - b.min.vz;
        if (size.vy >= size.vz ? size.vx < size.vy : size.vx < size.vz) {
            small = size.vy < size.vz ? size.vz : size.vy;
        } else {
            small = size.vx;
        }
        if (pad == 0) {
            v[0].pad = (small >> 1) + 0xA0;
        } else {
            v[0].pad = small + 0xA0;
        }
    }
    bxv->unk00 = 0x80;
    bxv->unk44 = 0;
    v[4].vx = b.min.vx;
    v[4].vy = b.min.vy;
    v[4].vz = b.min.vz;
    v[5].vx = b.max.vx;
    v[5].vy = b.min.vy;
    v[5].vz = b.min.vz;
    v[6].vx = b.min.vx;
    v[6].vy = b.min.vy;
    v[6].vz = b.max.vz;
    v[7].vx = b.max.vx;
    v[7].vy = b.min.vy;
    v[7].vz = b.max.vz;
    v[0].vx = b.min.vx;
    v[0].vy = b.max.vy;
    v[0].vz = b.min.vz;
    v[1].vx = b.max.vx;
    v[1].vy = b.max.vy;
    v[1].vz = b.min.vz;
    v[2].vx = b.min.vx;
    v[2].vy = b.max.vy;
    v[2].vz = b.max.vz;
    v[3].vx = b.max.vx;
    v[3].vy = b.max.vy;
    v[3].vz = b.max.vz;
}

void InitBxv(u8 *model) {
    InitBxv2((CobjBxv *)(model + 0x30), (CobjBounds *)model, (*(u32 *)(model + 0x28) & 0x8000) ? 0x29A : 0);
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

/* Line of sight query (fields used here) */
typedef struct GTELOSQuery {
    /* 0x00 */ s16 sector;
    /* 0x02 */ u8 unk02[0xA2];
    /* 0xA4 */ s32 unkA4;
    /* 0xA8 */ u8 unkA8[4];
    /* 0xAC */ LOSVec origin;
    /* 0xBC */ LOSVec dir;
} GTELOSQuery;

typedef struct LOSRay {
    /* 0x00 */ LOSVec origin;
    /* 0x10 */ LOSVec dir;
} LOSRay;

/* Terrain sector bounds and box (sector model display) */
typedef struct LOSSectorBounds {
    /* 0x00 */ s32 minX;
    /* 0x04 */ s32 minY;
    /* 0x08 */ s32 minZ;
    /* 0x0C */ s32 pad0C;
    /* 0x10 */ s32 maxX;
    /* 0x14 */ s32 maxY;
    /* 0x18 */ s32 maxZ;
    /* 0x1C */ s32 pad1C;
    /* 0x20 */ s32 unk20; /* display sector */
} LOSSectorBounds;

typedef struct LOSTerrSectorModel {
    /* 0x00 */ s32 unk00; /* loaded */
    /* 0x04 */ u8 unk04[0xC];
    /* 0x10 */ LOSSectorBounds *bounds;
} LOSTerrSectorModel;

/* Sector visibility state for EGSDisplayLOSSectorPolys */
extern s32 sectorInfo;
extern u8 D_80127F84;
extern u8 D_80127F85;
extern u8 D_80127F86;
extern u8 D_80127F87;
extern u8 D_80127F88;
extern s32 D_80127F8C;
extern s32 D_80127F90;
extern LOSVec losEndPoint;
extern u8 losFrame[];
extern u8 gInvWSMatrix[];
extern void *losInvMatrix;
extern s16 losGCount;
void GsSetProjection(s32 h);
void EGSFrameSetPosition(void *frame, s32 arg1, LOSVec *pos);
void EGSFrameSetOrientation(void *frame, s32 arg1, LOSVec *dir, s32 arg3);
void LOSVectorNormal(LOSVec *in, LOSVec *out);
void EGSDisplaySetAlternateViewpoint(void *frame);
s16 EGSFastCalculateZ(s16 x, s16 y, s16 z);
void EGSDisplayLOSSectorPolys(s32 sector, u8 steep, u8 arg2);

/* GTELOSFinish(q, w, z) called on the scratchpad stack */
#define GTELOS_FINISH(q, w, z)                                                            \
    __asm__ volatile(SCRATCH_ENTER "move $4, %0\n\tmove $5, %1\n\tmove $6, %2\n\tjal GTELOSFinish\n\t" \
                     SCRATCH_LEAVE                                                         \
                     :                                                                     \
                     : "r"(q), "r"(w), "r"(z)                                              \
                     : SCRATCH_CLOBBERS)

#ifdef NON_MATCHING
/* Reload scheduling only: the endZ spill and its reload in the loop are placed differently */
typedef struct GTELOSLevData {
    /* 0x00 */ u8 unk00[0x80];
    /* 0x80 */ u8 links[1][15]; /* per sector: linked sectors */
} GTELOSLevData;

static inline void GTELOSSectors(GTELOSQuery *query, LOSVec *origin, u8 windows, u8 steep, u8 arg2, s16 endX, s16 endZ,
                                 s16 x, s16 z) {
    s32 i;
    u8 *link;
    s32 sector;
    LOSTerrSectorModel *model;
    LOSHit *hit;

    if (losEndPointZ != 0) {
        sectorInfo = 0;
        D_80127F84 = 1;
        D_80127F85 = 1;
        D_80127F86 = 0;
        D_80127F87 = 0;
        D_80127F8C = 0;
        D_80127F88 = 0;
        D_80127F90 = 0;
        link = ((GTELOSLevData *)gLevData)->links[query->sector] - 1;
        for (i = -1;; link++, i++) {
            if (i < 0) {
                sector = query->sector;
            } else {
                sector = *link;
            }
            if (sector > gTerrSectorCount) {
                break;
            }
            model = (LOSTerrSectorModel *)gTerrSector[sector].model;
            if (model->unk00 != 0 &&
                ((model->bounds->minX < x && x < model->bounds->maxX && model->bounds->minZ < z && z < model->bounds->maxZ) ||
                 (model->bounds->minX < endX && endX < model->bounds->maxX && model->bounds->minZ < endZ &&
                  endZ < model->bounds->maxZ) ||
                 (hit = &losHitArray[losHitCount], hit->owner = (LOSHitOwner *)model,
                  LOSRotBoxV((u8 *)model->bounds + 0x30, losEndPointZ, hit, origin)))) {
                EGSDisplayLOSSectorPolys(model->bounds->unk20, steep, arg2);
            }
        }
        GTELOS_FINISH(query, windows, 0);
    }
}

/* Line of sight: test the ray against the current sector and its neighbours, then objects. */
void GTELOSLoop(GTELOSQuery *query, u8 windows, u8 arg2) {
    LOSVec normal;
    u8 steep = 0;
    s32 endZ;
    s32 endX;
    s32 flag;
    s32 i;
    u8 *link;
    s32 sector;
    s16 x;
    s16 z;
    LOSTerrSectorModel *model;
    LOSHit *hit;
    LOSVec *origin;
    LOSVec *dir;

    flag = query->unkA4;
    losSectorHitCount = 0;
    GsSetProjection(0x1F4);
    origin = &query->origin;
    EGSFrameSetPosition(losFrame, 0, origin);
    dir = &query->dir;
    EGSFrameSetOrientation(losFrame, 0, dir, 0);
    LOSVectorNormal(dir, &normal);
    EGSDisplaySetAlternateViewpoint(losFrame);
    losInvMatrix = gInvWSMatrix;
    losEndPoint.vx = query->origin.vx + query->dir.vx;
    losEndPoint.vy = query->origin.vy + query->dir.vy;
    losEndPoint.vz = query->origin.vz + query->dir.vz;
    losEndPointZ = EGSFastCalculateZ(losEndPoint.vx, losEndPoint.vy, losEndPoint.vz);
    losHitCount = 0;
    losGCount = 0;
    *(LOSRay *)losRay = *(LOSRay *)&query->origin;
    losBlocker = NULL;
    if ((normal.vy >= 0 ? normal.vy : -normal.vy) >= 0xA29 || (flag == 0 && losEndPointZ < 0xC0)) {
        steep = 1;
    }
    GTELOSSectors(query, origin, windows, steep, arg2, losEndPoint.vx, losEndPoint.vz, origin->vx, origin->vz);
    /* load delay slot after restoring $sp; the shared exit starts with it */
    __asm__ volatile("nop");
}
#else
INCLUDE_ASM("asm/main/nonmatchings/cobj", GTELOSLoop);
#endif

/* D_800122F4: "s/cobj.c", now emitted by PreSortHits (see config/syms.d/main/cobj.ld) */
