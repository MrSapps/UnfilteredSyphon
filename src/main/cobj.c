#include "common.h"
#include "game.h"
#include "gameobj.h"
#include "gobj.h"
#include "scratch.h"
#include "egsmath.h"

/* cobj.c's data (docs/data_migration.md). The two zero words after sectorCobj in
 * .sdata are unreferenced and stay in the splat blob: their TU is unclear. */
extern s32 sectorInfo;
static struct CobjNode *D_8010F9C0 = NULL; /* free list */
s32 *sectorCobj = &sectorInfo;
/* .bss: the node pool (D_8011EB98 below; entry 0 is unused) */
#define D_8011EBAC (&D_8011EB98[1])
/* the line count up to the ASSERTs is kept */


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

s16 losEndPointZ; /* common, $gp */

/* Objects the camera may not look through (doors and the like). */
static inline s32 LOSIsCameraBlocker(s32 type, s32 npcState) {
    s32 ret = 0;

    if (type == 0x2C || type == 0x62 || type == 0x38 || type == 0x1E || npcState == 9) {
        ret = 1;
    }
    return ret;
}

#define LOS_HIT_MAX 0x1A

u32 losHitCount; /* common, $gp */
u32 losSortedCount; /* common, $gp */
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

static CobjNode D_8011EB98[COBJ_NODE_MAX]; /* node pool (entry 0 unused) */
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
    /* 0x28 */ s32 maxNormalY;
    /* 0x2C */ s32 wallStatic;
    /* 0x30 */ s32 wallKinetic;
    /* 0x34 */ s32 floorStatic;
    /* 0x38 */ s32 floorKinetic;
} CobjPhysics;

/* Contact found by the collision detector */
typedef struct CobjContact {
    /* 0x00 */ u8 unk00[0x44];
    /* 0x44 */ VECTOR32 normal;
    /* 0x54 */ s32 unk54;
    /* 0x58 */ VECTOR32 motion;
    /* 0x68 */ u8 unk68[0x14];
    /* 0x7C */ s32 staticCoef;
    /* 0x80 */ s32 kineticCoef;
    /* 0x84 */ s32 coef;
} CobjContact;

/* MoveInfo.coll (fields used here) */
struct CollisionInfo {
    /* 0x00 */ u8 unk00[0x10];
    /* 0x10 */ s32 unk10;
    /* 0x14 */ u8 unk14[0x14];
    /* 0x28 */ void *hold;
};

s32 iCollisionPhysicsFaceGetFrictionForceValue(u8 *physics, s32 *out) {
    s32 modulus;

    EGSMath_VectorModulus(physics + 0x68, &modulus);
    *out = EGSMathFIXMul(*(s32 *)(physics + 0x84), modulus);
    return 1;
}

s32 iCollisionPhysicsGetKineticFrictionForce(VECTOR32 *v, s32 force, VECTOR32 *out) {
    s32 *o = &out->vx; /* as an array: o[0] is a plain scalar access */
    s32 modulus;

    EGSMath_VectorModulus(v, &modulus);
    if (force < modulus) {
        o[0] = EGSMathFIXMul(v->vx, -force);
        o[1] = EGSMathFIXMul(v->vy, -force);
        o[2] = EGSMathFIXMul(v->vz, -force);
        o[0] = EGSMathFIXDiv(o[0], modulus);
        o[1] = EGSMathFIXDiv(o[1], modulus);
        o[2] = EGSMathFIXDiv(o[2], modulus);
    } else {
        out->vx = -v->vx;
        out->vy = -v->vy;
        out->vz = -v->vz;
    }
    return 1;
}

INCLUDE_RODATA("asm/main/nonmatchings/cobj", D_800122D4);

/* Push out along the averaged normal of the floor-like contacts. */
void func_80072E34(CobjPhysics *phys, CobjContact **contacts, s32 count, VECTOR32 *out) {
    VECTOR32 sum = { 0, 0, 0, 0 };
    VECTOR32 push;
    s32 len;
    s32 i;
    s32 n = 0;
    s32 scale;
    s32 *v; /* push as an array: v[0] is a plain scalar store, v[1]/v[2] are not */

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
                v = &push.vx;
                v[0] = EGSMathFIXMul(contacts[i]->normal.vx, -scale);
                v[1] = EGSMathFIXMul(contacts[i]->normal.vy, -scale);
                v[2] = EGSMathFIXMul(contacts[i]->normal.vz, -scale);
                out->vx += push.vx;
                out->vy += push.vy;
                out->vz += push.vz;
            }
        }
    }
}

s32 func_80071E04(Gobj *a, Gobj *b, VECTOR32 *rel, VECTOR32 *out);
s32 func_800721EC(Gobj *obj, CobjPhysics *det, VECTOR32 *vel, VECTOR32 *out);
s32 func_8007112C(VECTOR32 *v, VECTOR32 *a1, VECTOR32 *axis, s32 *a3, CobjContact **hits, s32 *count);
void func_80072AE0(VECTOR32 *v, CobjContact **hit, s32 mode, VECTOR32 *axis, VECTOR32 *out, s32 *len);
s32 PhysicsGobjRegisterForce(Gobj *gobj, VECTOR32 *force);
s32 iCollisionPhysicsGetKineticFrictionForce(VECTOR32 *v, s32 force, VECTOR32 *out);

#define COBJ_COEF(hit, a, b)                \
    if ((a) == 0x1000) {                    \
        (hit)->coef = (b);                  \
    } else if ((b) == 0x1000) {             \
        (hit)->coef = (a);                  \
    } else {                                \
        (hit)->coef = EGSMathFIXMul(a, b);  \
    }

static inline u8 CobjOverLimit(s32 len, s32 limit) {
    if (len < limit) {
        return 0;
    }
    return 1;
}

static inline u8 CobjOverLimit2(s32 len, s32 limit) {
    if (len >= limit) {
        return 1;
    }
    return 0;
}

#ifdef NON_MATCHING
/* regalloc only: kinetic gets s5 and hits s7 (original: hits s6, staticSum s5, kinetic s7).
 * kinetic (8 refs, live 194, pri 1237) must drop below hits (736). Reusing kinetic for prev
 * (`kinetic = *kineticFlag; ... kinetic = kinetic == 0 ? CobjOverLimit2(..) : CobjOverLimit(..)`)
 * gives the right s5/s6/s7 (8 lines) but the original keeps prev separate in s2 and sets
 * kinetic directly in each branch; a dead `kinetic = 0` at the top is deleted (no effect). */
s32 CollisionPhysicsGobj_DetectorMotion(Gobj *obj, Gobj *other, CobjPhysics *det, CobjContact **hits, s32 count,
                                        u8 *kineticFlag) {
    VECTOR32 a;
    VECTOR32 b;
    VECTOR32 c;
    VECTOR32 d;
    VECTOR32 e;
    VECTOR32 f;
    VECTOR32 axis;
    VECTOR32 force;
    s32 limit[2];
    VECTOR32 h;
    VECTOR32 fr;
    VECTOR32 kin;
    s32 len;
    s32 friction;
    struct CollisionInfo *coll;
    u8 prev;
    u8 kinetic;
    s32 staticSum;
    s32 i;

    prev = *kineticFlag;
    *kineticFlag = 0;
    coll = obj->move->coll;
    if (count == 0) {
        return 1;
    }
    func_80071E04(obj, other, &a, &b);
    func_800721EC(obj, det, &c, &d);
    if (coll->hold == NULL) {
        a.vx += c.vx;
        a.vy += c.vy;
        a.vz += c.vz;
        b.vx += d.vx;
        b.vy += d.vy;
        b.vz += d.vz;
    } else {
        a.vy += c.vy;
        b.vy += d.vy;
    }
    func_8007112C(&b, &f, &axis, limit, hits, &count);
    e.vx = b.vx - f.vx;
    e.vy = b.vy - f.vy;
    e.vz = b.vz - f.vz;
    if (count == 0) {
        return 1;
    }
    if (coll->hold != NULL && count != 3) {
        c.vy = 0;
        d.vy = 0;
        func_80072AE0(&c, hits, count, &axis, &c, NULL);
        func_80072AE0(&d, hits, count, &axis, &d, NULL);
        a.vx += c.vx;
        a.vy += c.vy;
        a.vz += c.vz;
        b.vx += d.vx;
        b.vy += d.vy;
        b.vz += d.vz;
    }
    force.vx = 0;
    force.vy = 0;
    force.vz = 0;
    func_80072AE0(&a, hits, count, &axis, &h, &len);
    for (i = 0; i < count; i++) {
        hits[i]->motion = h;
    }
    if (prev == 0) {
        kinetic = CobjOverLimit2(len, 0x3321);
    } else {
        kinetic = CobjOverLimit(len, 0x1000);
    }
    staticSum = 0;
    fr.vx = 0;
    fr.vy = 0;
    fr.vz = 0;
    for (i = 0; i < count; i++) {
        hits[i]->staticCoef = 0x1833;
        hits[i]->kineticCoef = 0x872;
        if (hits[i]->normal.vy >= det->minNormalY && hits[i]->normal.vy <= det->maxNormalY) {
            if (kinetic == 0) {
                COBJ_COEF(hits[i], hits[i]->staticCoef, det->floorStatic);
            } else {
                COBJ_COEF(hits[i], hits[i]->kineticCoef, det->floorKinetic);
            }
        } else if (kinetic == 0) {
            COBJ_COEF(hits[i], hits[i]->staticCoef, det->wallStatic);
        } else {
            COBJ_COEF(hits[i], hits[i]->kineticCoef, det->wallKinetic);
        }
        iCollisionPhysicsFaceGetFrictionForceValue((u8 *)hits[i], &friction);
        if (kinetic == 1) {
            iCollisionPhysicsGetKineticFrictionForce(&hits[i]->motion, friction, &kin);
            fr.vx += kin.vx;
            fr.vy += kin.vy;
            fr.vz += kin.vz;
        } else {
            staticSum += friction;
        }
    }
    if (kinetic == 1) {
        force.vx = -e.vx;
        force.vy = -e.vy;
        force.vz = -e.vz;
        force.vx += fr.vx;
        force.vy += fr.vy;
        force.vz += fr.vz;
    } else if (limit[0] < staticSum) {
        force.vx = -b.vx;
        force.vy = -b.vy;
        force.vz = -b.vz;
    } else {
        kinetic = 1;
        force.vx = -e.vx;
        force.vy = -e.vy;
        force.vz = -e.vz;
    }
    if (coll->unk10 == 0) {
        func_80072E34(det, hits, count, &force);
    }
    PhysicsGobjRegisterForce(obj, &force);
    if (other != NULL) {
        force.vx = -force.vx;
        force.vy = -force.vy;
        force.vz = -force.vz;
        PhysicsGobjRegisterForce(other, &force);
    }
    *kineticFlag = kinetic;
    return 1;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/cobj", CollisionPhysicsGobj_DetectorMotion);
#endif

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
        /* offset + base (&gTerrSector[i] adds them the other way round) */
        ts = (CobjTerrSector *)(i * sizeof(CobjTerrSector) + (u32)gTerrSector);
        ts->unk1C = 0;
        ts->unk20 = 0;
        gTerrSector[i].unk24 = 0;
        for (j = 4; j >= 0; j--) {
            gTerrSector[i].unk28[j] = 0;
        }
    }
    return 1;
}

/* Hot box shapes (CobjNode.type): 1 a sector rotated box, 2 a game object, 3 an animated model */
typedef struct CobjRotBox {
    /* 0x00 */ u32 flags;
    /* 0x04 */ VECTOR center;
    /* 0x14 */ MATRIX mtx;
    /* 0x34 */ VECTOR size;
} CobjRotBox;

typedef struct CobjBoxBounds {
    /* 0x00 */ VECTOR min;
    /* 0x10 */ VECTOR max;
} CobjBoxBounds;

typedef struct CobjBoxModel {
    /* 0x00 */ u8 unk00[0xC];
    /* 0x0C */ MATRIX *frame;
    /* 0x10 */ CobjBoxBounds *bounds;
} CobjBoxModel;

typedef struct CobjBoxGobj {
    /* 0x00 */ u8 unk00[2];
    /* 0x02 */ s16 id;
    /* 0x04 */ u8 unk04[4];
    /* 0x08 */ CobjBoxModel *anim;
} CobjBoxGobj;

typedef struct CobjSegment {
    /* 0x00 */ VECTOR p0;
    /* 0x10 */ VECTOR p1;
} CobjSegment;

/* Line of sight request handed to CobjHandleHotBoxLosXZ */
typedef struct CobjLosQuery {
    /* 0x00 */ s32 unk00;
    /* 0x04 */ void *unk04;
    /* 0x08 */ void *unk08;
    /* 0x0C */ void *unk0C;
    /* 0x10 */ s32 unk10;
    /* 0x14 */ CobjSegment seg;
    /* 0x34 */ VECTOR point;
    /* 0x44 */ s32 radius;
    /* 0x48 */ s32 unk48;
    /* 0x4C */ s32 margin;
    /* 0x50 */ s32 unk50;
    /* 0x54 */ u8 hit;
} CobjLosQuery;

int EGSMath_MatrixInvert(MATRIX *in, MATRIX *out);
int EGSGeometry_TransformRay(CobjSegment *in, MATRIX *m, CobjSegment *out);
int EGSGeometry_GetIntersectRaySegment2UnrotBox(CobjSegment *ray, CobjBoxBounds *box, s32 margin, u8 *hit,
                                                VECTOR *point, VECTOR *normal);

#define HB_ROT(node) ((CobjRotBox *)(node)->gobj)
#define HB_GOBJ(node) (((CobjBoxGobj *)(node)->gobj)->anim)
#define HB_MODEL(node) ((CobjBoxModel *)(node)->gobj)
#define HALVE(x) ((x) >= 0 ? (x) >> 1 : -(-(x) >> 1))

static inline void CobjHotBoxGetCenter(CobjNode *node, u8 type, VECTOR *out) {
    CobjBoxBounds *b;

    switch (type) {
    case 1:
        *out = HB_ROT(node)->center;
        break;
    case 2:
        out->vx = HB_GOBJ(node)->frame->t[0];
        out->vy = HB_GOBJ(node)->frame->t[1];
        out->vz = HB_GOBJ(node)->frame->t[2];
        out->vy = -out->vy;
        break;
    case 3:
        b = HB_MODEL(node)->bounds;
        out->vx = b->max.vx + b->min.vx;
        out->vy = b->max.vy + b->min.vy;
        out->vz = b->max.vz + b->min.vz;
        out->vx = HALVE(out->vx);
        out->vy = HALVE(out->vy);
        out->vz = HALVE(out->vz);
        break;
    }
}

static inline void CobjHotBoxGetHalfSize(CobjNode *node, u8 type, VECTOR *size, VECTOR *out) {
    CobjBoxBounds *b;
    CobjBoxBounds *m;

    switch (type) {
    case 1:
        *size = HB_ROT(node)->size;
        out->vx = size->vx >> 1;
        out->vy = size->vy >> 1;
        out->vz = size->vz >> 1;
        break;
    case 2:
        b = HB_GOBJ(node)->bounds;
        out->vx = b->max.vx - b->min.vx;
        out->vy = b->max.vy - b->min.vy;
        out->vz = b->max.vz - b->min.vz;
        break;
    case 3:
        m = HB_MODEL(node)->bounds;
        out->vx = m->max.vx - m->min.vx;
        out->vy = m->max.vy - m->min.vy;
        out->vz = m->max.vz - m->min.vz;
        out->vx = out->vx >> 1;
        out->vy = out->vy >> 1;
        out->vz = out->vz >> 1;
        break;
    }
}

static inline void CobjHotBoxIsUnrotated(CobjNode *node, u8 type, u8 *out) {
    switch (type) {
    case 1:
        *out = 0;
        break;
    case 2:
        *out = 0;
        break;
    case 3:
        *out = 1;
        break;
    }
}

#define HB_FLIP_MATRIX(dst, src)        \
    (dst).m[0][0] = (src)->m[0][0];    \
    (dst).m[0][1] = -(src)->m[0][1];   \
    (dst).m[0][2] = (src)->m[0][2];    \
    (dst).m[1][0] = -(src)->m[1][0];   \
    (dst).m[1][1] = (src)->m[1][1];    \
    (dst).m[1][2] = -(src)->m[1][2];   \
    (dst).m[2][0] = (src)->m[2][0];    \
    (dst).m[2][1] = -(src)->m[2][1];   \
    (dst).m[2][2] = (src)->m[2][2];    \
    (dst).t[0] = (src)->t[0];          \
    (dst).t[1] = -(src)->t[1];         \
    (dst).t[2] = (src)->t[2];

/* Inverse of the box frame (the y axis flipped for game objects and models) */
#define HB_GET_INV_MATRIX(node, type, mtx, out)              \
    switch (type) {                                         \
    case 1:                                                 \
        *(out) = &HB_ROT(node)->mtx;                        \
        break;                                              \
    case 2:                                                 \
        HB_FLIP_MATRIX(mtx, HB_GOBJ(node)->frame);          \
        EGSMath_MatrixInvert(&(mtx), &(mtx));               \
        *(out) = &(mtx);                                    \
        break;                                              \
    case 3:                                                 \
        HB_FLIP_MATRIX(mtx, HB_MODEL(node)->frame);         \
        EGSMath_MatrixInvert(&(mtx), &(mtx));               \
        *(out) = &(mtx);                                    \
        break;                                              \
    }

static inline void CobjHotBoxGetOrigin(CobjNode *node, u8 type, VECTOR *out) {
    CobjBoxBounds *b;

    switch (type) {
    case 1:
        out->vx = -HB_ROT(node)->mtx.t[0];
        out->vy = -HB_ROT(node)->mtx.t[1];
        out->vz = -HB_ROT(node)->mtx.t[2];
        break;
    case 2:
        out->vx = HB_GOBJ(node)->frame->t[0];
        out->vy = HB_GOBJ(node)->frame->t[1];
        out->vz = HB_GOBJ(node)->frame->t[2];
        out->vy = -out->vy;
        break;
    case 3:
        b = HB_MODEL(node)->bounds;
        out->vx = b->max.vx + b->min.vx;
        out->vy = b->max.vy + b->min.vy;
        out->vz = b->max.vz + b->min.vz;
        out->vx = HALVE(out->vx);
        out->vy = HALVE(out->vy);
        out->vz = HALVE(out->vz);
        break;
    }
}

static inline void CobjHotBoxGetBox(CobjNode *node, u8 type, CobjBoxBounds *box) {
    VECTOR c;
    s32 t;
    s32 u;

    switch (type) {
    case 1:
        box->min.vx = 0;
        box->min.vy = 0;
        box->min.vz = 0;
        box->max = HB_ROT(node)->size;
        break;
    case 2:
        *box = *HB_GOBJ(node)->bounds;
        t = box->min.vy;
        box->min.vy = box->max.vy;
        box->max.vy = t;
        box->min.vy = -box->min.vy;
        box->max.vy = -box->max.vy;
        break;
    case 3:
        *box = *HB_MODEL(node)->bounds;
        u = box->min.vy;
        box->min.vy = box->max.vy;
        box->max.vy = u;
        box->min.vy = -box->min.vy;
        box->max.vy = -box->max.vy;
        c.vx = box->max.vx + box->min.vx;
        c.vy = box->max.vy + box->min.vy;
        c.vz = box->max.vz + box->min.vz;
        c.vx = HALVE(c.vx);
        c.vy = HALVE(c.vy);
        c.vz = HALVE(c.vz);
        box->min.vx -= c.vx;
        box->min.vy -= c.vy;
        box->min.vz -= c.vz;
        box->max.vx -= c.vx;
        box->max.vy -= c.vy;
        box->max.vz -= c.vz;
        break;
    }
}

s32 CobjHandleHotBoxLosXZ(CobjNode *node, CobjLosQuery *q, u8 *hit) {
    VECTOR point;
    VECTOR center;
    VECTOR d;
    VECTOR half;
    VECTOR v;
    s32 radius;
    s32 adx;
    s32 adz;
    s32 reach;
    s32 far;
    s32 margin;
    u8 type;

    q->unk04 = q->unk08;
    *hit = 0;
    type = node->type;
    radius = q->radius;
    point = q->point;
    CobjHotBoxGetCenter(node, type, &center);
    d.vx = center.vx - point.vx;
    adx = d.vx >= 0 ? d.vx : -d.vx;
    if (radius < adx) {
        return 1;
    }
    d.vz = center.vz - point.vz;
    adz = d.vz >= 0 ? d.vz : -d.vz;
    if (radius < adz) {
        return 1;
    }
    margin = q->margin;
    CobjHotBoxGetHalfSize(node, type, &v, &half);
    reach = half.vx + half.vz + margin;
    if (reach < adx || reach < adz) {
        return 1;
    }
    far = adz;
    if (adz < adx) {
        far = adx;
    }
    if (reach < far) {
        return 1;
    }
    if (type == 2 && node->gobj == q->unk08) {
        return 1;
    }
    {
        CobjSegment seg;
        MATRIX mtx;
        CobjBoxBounds box;
        VECTOR hitPoint;
        VECTOR hitNormal;
        u8 unrotated;
        MATRIX *m;

        seg = q->seg;
        CobjHotBoxIsUnrotated(node, type, &unrotated);
        if (!unrotated) {
            HB_GET_INV_MATRIX(node, type, mtx, &m);
            EGSGeometry_TransformRay(&seg, m, &seg);
        } else {
            CobjHotBoxGetOrigin(node, type, &v);
            seg.p0.vx -= v.vx;
            seg.p0.vy -= v.vy;
            seg.p0.vz -= v.vz;
        }
        CobjHotBoxGetBox(node, type, &box);
        if (seg.p0.vx >= box.min.vx && seg.p0.vx <= box.max.vx && seg.p0.vz >= box.min.vz &&
            seg.p0.vz <= box.max.vz) {
            *hit = 1;
            hitPoint = seg.p0;
            hitNormal.vx = 0;
            hitNormal.vy = 0;
            hitNormal.vz = 0;
        } else {
            box.max.vy = 0x27100;
            box.min.vy = -0x27100;
            EGSGeometry_GetIntersectRaySegment2UnrotBox(&seg, &box, 0, hit, &hitPoint, &hitNormal);
        }
        if (*hit && (type != 2 || node->gobj != q->unk0C)) {
            q->hit = 1;
        }
    }
    return 1;
}

s32 CobjHandleHotBoxXZ(CobjHotBoxModel **box, s32 arg1, u8 *out) {
    CobjSector *sector = (*box)->display->sector;

    if (sector == (CobjSector *)-1) {
        *out = 0;
    } else {
        *out = mFaceCollisionSectors[sector->id] != 0;
    }
    return 1;
}

void SysPostMessage(u8 id, u8 priority, s32 from, s32 to, s32 d0, s32 d1, s32 d2, s32 d3);

/* Point query handed to CobjHandleHotBox */
typedef struct CobjHotBoxQuery {
    /* 0x00 */ s32 unk00;
    /* 0x04 */ Gobj *unk04;
    /* 0x08 */ VECTOR point;
    /* 0x18 */ Gobj *self;
    /* 0x1C */ s32 radius;
} CobjHotBoxQuery;

s32 CobjHandleHotBox(CobjNode *node, CobjHotBoxQuery *q, u8 *hit) {
    VECTOR point;
    VECTOR center;
    VECTOR d;
    VECTOR half;
    VECTOR v;
    s32 radius;
    s32 adx;
    s32 ady;
    s32 adz;
    s32 sum;
    s32 far;
    u8 type;

    q->unk04 = q->self;
    *hit = 0;
    type = node->type;
    radius = q->radius;
    point = q->point;
    CobjHotBoxGetCenter(node, type, &center);
    d.vx = center.vx - point.vx;
    adx = d.vx >= 0 ? d.vx : -d.vx;
    if (radius < adx) {
        return 1;
    }
    d.vy = center.vy - point.vy;
    ady = d.vy >= 0 ? d.vy : -d.vy;
    if (radius < ady) {
        return 1;
    }
    d.vz = center.vz - point.vz;
    adz = d.vz >= 0 ? d.vz : -d.vz;
    if (radius < adz) {
        return 1;
    }
    CobjHotBoxGetHalfSize(node, type, &v, &half);
    sum = half.vx + half.vy + half.vz;
    if (sum < adx || sum < ady || sum < adz) {
        return 1;
    }
    if (ady < adx) {
        far = adz;
        if (far < adx) {
            far = adx;
        }
    } else {
        far = adz;
        if (far < ady) {
            far = ady;
        }
    }
    if (half.vx > half.vy) {
        if (half.vy < half.vz) {
            sum = half.vx + half.vz;
        } else {
            sum = half.vy + half.vx;
        }
    } else if (half.vx < half.vz) {
        sum = half.vy + half.vz;
    } else {
        sum = half.vy + half.vx;
    }
    if (sum < far) {
        return 1;
    }
    if (type == 2 && node->gobj == q->self) {
        return 1;
    }
    {
        MATRIX mtx;
        CobjBoxBounds box;
        u8 unrotated;
        MATRIX *m;

        CobjHotBoxIsUnrotated(node, type, &unrotated);
        if (!unrotated) {
            HB_GET_INV_MATRIX(node, type, mtx, &m);
            ApplyMatrixLV(m, &point, &point);
            point.vx += m->t[0];
            point.vy += m->t[1];
            point.vz += m->t[2];
        } else {
            CobjHotBoxGetOrigin(node, type, &v);
            point.vx -= v.vx;
            point.vy -= v.vy;
            point.vz -= v.vz;
        }
        CobjHotBoxGetBox(node, type, &box);
        if (point.vx >= box.min.vx && point.vx <= box.max.vx && point.vy >= box.min.vy && point.vy <= box.max.vy &&
            point.vz >= box.min.vz && point.vz <= box.max.vz) {
            *hit = 1;
            if (type == 2 && (node->flags & 2)) {
                SysPostMessage(0x1A, 1, q->self->id, node->gobj->id, 0, 0, 0, 0);
            }
        }
    }
    return 1;
}

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
u32 losSectorHitCount; /* common, $gp */

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
LOSHit *losBlocker; /* common, $gp */
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
/* sectorInfo: declared at the top */
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
void *losInvMatrix; /* common, $gp */
s16 losGCount; /* common, $gp */
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

/* $gp commons: let maspsx use $gp for them (kept after the last function, in .text) */
SDATA_LOCAL(losBlocker)
SDATA_LOCAL(losEndPointZ)
SDATA_LOCAL(losGCount)
SDATA_LOCAL(losHitCount)
SDATA_LOCAL(losInvMatrix)
SDATA_LOCAL(losSectorHitCount)
SDATA_LOCAL(losSortedCount)
