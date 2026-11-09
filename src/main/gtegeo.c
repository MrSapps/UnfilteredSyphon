#include "common.h"
#include "gameobj.h"
#include "egsframe.h"
#include "game.h"
#include "system.h"

/*
 * Entry of the per-type LOS candidate lists (losLists[type]); a hit keeps the
 * entry it came from, and window hits copy it whole into the query result.
 */
typedef struct LOSNode {
    /* 0x00 */ union {
        s32 id; /* windows: portal id */
        GameObjectRuntime *gobj; /* game objects */
    } u;
    /* 0x04 */ s32 data[3];
    /* 0x10 */ struct LOSNode *next;
} LOSNode; /* size 0x14 */

/* View of GameObjectAnim used by the LOS and collision passes */
typedef struct LOSCollideAnim {
    /* 0x00 */ u8 unk00[0xB];
    /* 0x0B */ u8 collideFlags; /* 2: solid box, 4: pushes/blocks the player */
    /* 0x0C */ EGSFrame *frame;
    /* 0x10 */ struct LOSModel *model;
    /* 0x14 */ u16 id; /* gameObjectsInfo index in the low 10 bits */
    /* 0x16 */ s16 collided;
} LOSCollideAnim;

/* Entry of the collision candidate list (D_8012D8CC) */
typedef struct LOSCollideNode {
    /* 0x00 */ LOSCollideAnim *anim;
    /* 0x04 */ s32 unk04;
    /* 0x08 */ struct LOSCollideNode *next;
} LOSCollideNode;

/* View of MoveInfo */
typedef struct LOSMove {
    /* 0x000 */ u8 unk000[0x10C];
    /* 0x10C */ VECTOR pos;
    /* 0x11C */ u8 unk11C[0x78];
    /* 0x194 */ VECTOR pendingMove;
} LOSMove;

typedef struct LOSHit {
    /* 0x00 */ s32 unk00;
    /* 0x04 */ u32 z;
    /* 0x08 */ s32 type; /* 1: game object, 2: window */
    /* 0x0C */ void *box;
    /* 0x10 */ VECTOR normal;
    /* 0x20 */ VECTOR point;
    /* 0x30 */ u8 unk30[0x18];
    /* 0x48 */ LOSCollideAnim *owner;
    /* 0x4C */ LOSNode *window;
} LOSHit; /* size 0x50 */

#define LOS_HIT_MAX 26

/* A 20.12 scalar the plane test keeps in memory (only vx is used) */
typedef struct LOSFixed {
    s32 vx;
    s32 pad;
} LOSFixed;

typedef struct LOSRay {
    /* 0x00 */ VECTOR origin;
    /* 0x10 */ VECTOR dir;
} LOSRay;

/* Line of sight query, as filled in by LOSVerifyHits */
typedef struct LOSQuery {
    /* 0x000 */ u8 unk000[0xA0];
    /* 0x0A0 */ GameObjectRuntime *ignore0; /* objects the ray never hits (shooter, ...) */
    /* 0x0A4 */ GameObjectRuntime *ignore1;
    /* 0x0A8 */ u8 unk0A8[0x40];
    /* 0x0E8 */ u32 hitZ;
    /* 0x0EC */ u8 hit;
    /* 0x0F0 */ VECTOR hitPoint;
    /* 0x100 */ VECTOR hitNormal;
    /* 0x110 */ LOSNode hitWindow;
} LOSQuery;

/* Model bounding box (GameObjectModel + 0) */
typedef struct LOSBounds {
    /* 0x00 */ VECTOR min;
    /* 0x10 */ VECTOR max;
} LOSBounds;

/* Collision box of a model (GameObjectModel + 0x30), built by InitBxv */
typedef struct LOSBox {
    /* 0x00 */ s32 id; /* 0 for see-through-proof objects, -1 otherwise */
    /* 0x04 */ u8 unk04[6];
    /* 0x0A */ s16 radius; /* collision test range */
    /* 0x0C */ u8 unk0C[0x38];
    /* 0x44 */ s32 state; /* -1 until InitBxv ran */
} LOSBox; /* size 0x48 */

typedef struct LOSModel {
    /* 0x00 */ LOSBounds bounds;
    /* 0x20 */ u8 unk20[8];
    /* 0x28 */ u32 flags;
    /* 0x2C */ u8 unk2C[4];
    /* 0x30 */ LOSBox box;
} LOSModel;

typedef struct LOSFace {
    /* 0x00 */ struct LOSFace *prev;
    /* 0x04 */ struct LOSFace *next;
} LOSFace;

typedef struct LOSAnimFace {
    /* 0x00 */ s32 count; /* vertices */
    /* 0x04 */ s32 unk04;
    /* 0x08 */ SVECTOR *normal;
    /* 0x0C */ SVECTOR normalBuf; /* losAnimFace points normal here */
    /* 0x14 */ SVECTOR *v[4];
} LOSAnimFace;

/* Owner tag kept in the pad of a collision box face normal */
typedef union CollisionBoxOwner {
    u16 raw;
    struct {
        u16 unk : 4;
        u16 id : 10; /* gameObjectsInfo index + 1 */
        u16 unk2 : 2;
    } bits;
} CollisionBoxOwner;

/* One face of a registered collision box; linked into the owner's face list */
typedef struct CollisionBoxFace {
    /* 0x00 */ struct CollisionBoxFace *prev;
    /* 0x04 */ struct CollisionBoxFace *next;
    /* 0x08 */ u8 unk08[0xC];
    /* 0x14 */ SVECTOR normal; /* outward normal; pad = CollisionBoxOwner */
    /* 0x1C */ u8 unk1C[0x10];
} CollisionBoxFace; /* size 0x2C */

extern SVECTOR mCollisionBoxVertices[][2][2][2];
extern CollisionBoxFace mCollisionBoxFaces[][3][2];
extern u8 mCollisionBoxFaceFlags[][3][2];
extern LOSNode *D_80128A20[];
extern LOSCollideNode *D_8012D8CC; /* collision candidates */
extern MATRIX losFrame;
extern MATRIX losRevMatrix;
extern MATRIX *losInvMatrix; /* LOS candidate lists, by hit type */
extern LOSAnimFace losAnimFace;
extern LOSHit losHitArray[];
extern LOSHit *losHitsSorted[];
extern u32 losHitCount;
extern u32 losSortedCount;
extern s32 losWindowCount;
extern s32 losWindowArray[];
extern s16 losEndPointZ;
extern LOSHit *losBlocker;
extern MATRIX gEGSIdentityMatrix;
extern u16 losGCount;
extern LOSRay losRay;

void GobjLOSLoop(LOSQuery *query, s32 type, u8 forcedOnly);
void LOSSetupGTE(MATRIX *mtx);
u32 LOSCalculateZ(s32 x, s32 y, s32 z);
void LOSVerifyHits(LOSQuery *query);
void LOSSortHits(void);
void InitBxv(LOSModel *model);
s32 LOSOnScreen(s32 a, s32 b);
s32 LOSRotBoxV(LOSBox *box, s16 endZ, LOSHit *hit, LOSRay *ray);
void OuterProduct0(VECTOR *a, VECTOR *b, VECTOR *out);
void LOSVectorNormal(VECTOR *in, VECTOR *out);
u8 iLOSGeometry_IsIntersectPoint2CoplanarAnimFace(VECTOR *point, LOSAnimFace *face, s32 tolerance);
void PreSortHits(LOSQuery *query);
void GsSetProjection(s32 h);
void EGSDisplaySetAlternateViewpoint(MATRIX *m);
void LOSSetupGTEBoth(MATRIX *m, MATRIX *rev);
void InitBxv2(LOSBox *box, LOSModel *model, s32 id);

#ifndef ABS
#define ABS(x) ((x) < 0 ? -(x) : (x))
#endif
#ifndef MAX
#define MAX(a, b) ((a) < (b) ? (b) : (a))
#endif

/* libgte.h vector macros */
#define setVector(v, _x, _y, _z) (v)->vx = _x, (v)->vy = _y, (v)->vz = _z
#define addVector(v0, v1) (v0)->vx += (v1)->vx, (v0)->vy += (v1)->vy, (v0)->vz += (v1)->vz

/* 20.12 fixed point product to integer, rounding towards zero */
#define LOS_FIX(x) ((x) >= 0 ? (x) >> 12 : -(-(x) >> 12))

/* Octagonal approximation of sqrt(x^2 + z^2), into len */
#define LOS_FUZZY_LENGTH_2D(len, x, z)                 \
    {                                                  \
        s32 _x = (x) >= 0 ? (x) : -(x);                \
        s32 _z = (z) >= 0 ? (z) : -(z);                \
        s32 _len;                                      \
        s32 _diff;                                     \
        s32 _half;                                     \
        s32 _quarter;                                  \
        _len = _x + _z;                                \
        _diff = _x - _z;                               \
        _half = _len >> 1;                             \
        _quarter = _len >> 2;                          \
        if (_diff < 0) {                               \
            _diff = _z - _x;                           \
        }                                              \
        if (_diff < _half) {                           \
            _len -= _quarter;                          \
        } else if (_diff <= _half + _quarter) {        \
            _len -= _len >> 3;                         \
        }                                              \
        len = _len;                                    \
    }

void WindowLOSLoop(LOSQuery *query) {
    GobjLOSLoop(query, 2, 0);
    LOSVerifyHits(query);
}

void GobjLOSLoop(LOSQuery *query, s32 type, u8 forcedOnly) {
    LOSNode *node;
    GameObjectRuntime *gobj;
    LOSModel *model;
    LOSBox *box;
    LOSHit *hit;
    GameObjectRuntime *ignore0;
    GameObjectRuntime *ignore1;
    s32 objType;
    u32 state;
    u8 forced;
    s32 f;

    node = D_80128A20[type];
    ignore0 = query->ignore0;
    ignore1 = query->ignore1;
    losAnimFace.count = 3;
    losAnimFace.normal = &losAnimFace.normalBuf;
    hit = &losHitArray[losHitCount];
    while (node != NULL) {
        gobj = node->u.gobj;
        model = (LOSModel *)gobj->anim->model;
        objType = GOBJ_TYPE(gobj->id);
        state = gobj->npcState;
        f = 0;
        if (objType == 0x2C || objType == 0x62 || objType == 0x38 || objType == 0x1E || state == 9) {
            f = 1;
        }
        forced = f;
        if (model->box.state == -1) {
            InitBxv(model);
        }
        box = &model->box;
        if (!forced) {
            box->id = -1;
        } else {
            box->id = 0;
        }
        if (gobj == ignore1 || gobj == ignore0 || (forcedOnly && !forced)) {
            node = node->next;
            continue;
        }
        LOSSetupGTE(&gobj->anim->frame->matrix);
        if (forced || LOSOnScreen(6000, 150)) {
            losGCount++;
            hit->owner = (LOSCollideAnim *)gobj->anim;
            if (LOSRotBoxV(box, losEndPointZ, hit, &losRay)) {
                hit->window = node;
                hit->type = type;
                hit->box = box;
                if (++losHitCount == LOS_HIT_MAX) {
                    PreSortHits(query);
                }
                hit = &losHitArray[losHitCount];
            }
        }
        node = node->next;
    }
}

u8 iLOSGeometry_IsIntersectPoint2CoplanarAnimFace(VECTOR *point, LOSAnimFace *face, s32 tolerance) {
    VECTOR p; /* the point projected onto the face's dominant plane (vx, vz) */
    VECTOR pts[4]; /* the face vertices, projected the same way */
    VECTOR edge; /* edge normal within the plane */
    VECTOR d;
    VECTOR dot;
    SVECTOR *n;
    SVECTOR **v;
    s32 count;
    s32 ax;
    s32 ay;
    s32 az;
    s32 max;
    s32 i;
    s32 next;
    s32 len;

    n = face->normal;
    count = face->count;
    ax = n->vx >= 0 ? n->vx : -n->vx;
    ay = n->vy >= 0 ? n->vy : -n->vy;
    az = n->vz >= 0 ? n->vz : -n->vz;
    v = face->v;
    max = MAX(ax, MAX(ay, az));
    if (max == ax) {
        if (n->vx >= 0) {
            p.vx = point->vz;
            p.vz = point->vy;
            for (i = 0; i < count; i++) {
                pts[i].vx = v[i]->vz;
                pts[i].vz = v[i]->vy;
            }
        } else {
            p.vx = point->vy;
            p.vz = point->vz;
            for (i = 0; i < count; i++) {
                pts[i].vx = v[i]->vy;
                pts[i].vz = v[i]->vz;
            }
        }
    } else if (max == ay) {
        if (n->vy >= 0) {
            p.vx = point->vx;
            p.vz = point->vz;
            for (i = 0; i < count; i++) {
                pts[i].vx = v[i]->vx;
                pts[i].vz = v[i]->vz;
            }
        } else {
            p.vx = point->vz;
            p.vz = point->vx;
            for (i = 0; i < count; i++) {
                pts[i].vx = v[i]->vz;
                pts[i].vz = v[i]->vx;
            }
        }
    } else {
        if (n->vz >= 0) {
            p.vx = point->vy;
            p.vz = point->vx;
            for (i = 0; i < count; i++) {
                pts[i].vx = v[i]->vy;
                pts[i].vz = v[i]->vx;
            }
        } else {
            p.vx = point->vx;
            p.vz = point->vy;
            for (i = 0; i < count; i++) {
                pts[i].vx = v[i]->vx;
                pts[i].vz = v[i]->vy;
            }
        }
    }
    i = 0;
    if (count > 0) do {
        next = i + 1;
        if (next == count) {
            next = 0;
        }
        edge.vx = pts[i].vz - pts[next].vz;
        edge.vz = pts[next].vx - pts[i].vx;
        LOS_FUZZY_LENGTH_2D(len, edge.vx, edge.vz);
        if (len < 2) {
            len = 1;
        } else {
            LOS_FUZZY_LENGTH_2D(len, edge.vx, edge.vz);
        }
        d.vx = p.vx - pts[i].vx;
        d.vz = p.vz - pts[i].vz;
        dot.vx = d.vx * edge.vx + d.vz * edge.vz;
        if (dot.vx > tolerance * len) {
            return 0;
        }
    } while (++i < count);
    return 1;
}

s32 LOSGeometry_GetIntersectLine2AnimFacePlane(LOSRay *ray, LOSAnimFace *face, VECTOR *out, VECTOR *normal) {
    VECTOR edge1;
    VECTOR edge0;
    LOSFixed denom;
    VECTOR base;
    VECTOR diff;
    VECTOR step;
    LOSFixed num;
    s32 i;
    s32 miss;
    s32 t;
    s32 a;
    s32 b;

    miss = 0;
    for (i = 0; i < face->count; i++) {
        face->v[i]->vy = -face->v[i]->vy;
    }
    edge1.vx = face->v[2]->vx - face->v[1]->vx;
    edge1.vy = face->v[2]->vy - face->v[1]->vy;
    edge1.vz = face->v[2]->vz - face->v[1]->vz;
    edge0.vx = face->v[0]->vx - face->v[1]->vx;
    edge0.vy = face->v[0]->vy - face->v[1]->vy;
    edge0.vz = face->v[0]->vz - face->v[1]->vz;
    OuterProduct0(&edge1, &edge0, normal);
    LOSVectorNormal(normal, normal);
    face->normal->vx = normal->vx;
    face->normal->vy = normal->vy;
    face->normal->vz = normal->vz;
    denom.vx = ray->dir.vx * normal->vx + ray->dir.vy * normal->vy + ray->dir.vz * normal->vz;
    denom.vx = LOS_FIX(denom.vx);
    if (denom.vx != 0) {
        base.vx = face->v[1]->vx;
        base.vy = face->v[1]->vy;
        base.vz = face->v[1]->vz;
        diff.vx = base.vx - ray->origin.vx;
        diff.vy = base.vy - ray->origin.vy;
        diff.vz = base.vz - ray->origin.vz;
        num.vx = diff.vx * normal->vx + diff.vy * normal->vy + diff.vz * normal->vz;
        t = LOS_FIX(num.vx);
        a = ray->dir.vx;
        b = denom.vx;
        num.vx = t;
        t = EGSMathFIXDiv(a, b);
        a = ray->dir.vy;
        b = denom.vx;
        step.vx = t;
        step.vy = EGSMathFIXDiv(a, b);
        a = ray->dir.vz;
        b = denom.vx;
        t = EGSMathFIXDiv(a, b);
        a = step.vx;
        b = num.vx;
        step.vz = t;
        t = EGSMathFIXMul(a, b);
        a = step.vy;
        b = num.vx;
        step.vx = t;
        t = EGSMathFIXMul(a, b);
        a = step.vz;
        b = num.vx;
        step.vy = t;
        step.vz = EGSMathFIXMul(a, b);
        out->vx = ray->origin.vx + step.vx;
        out->vy = ray->origin.vy + step.vy;
        out->vz = ray->origin.vz + step.vz;
    } else {
        *out = ray->origin;
        miss = 1;
    }
    if (!miss) {
        miss = iLOSGeometry_IsIntersectPoint2CoplanarAnimFace(out, face, 4) != 1;
    }
    for (i = 0; i < face->count; i++) {
        face->v[i]->vy = -face->v[i]->vy;
    }
    face->normal->vy = -face->normal->vy;
    return miss;
}

void LosTransformFaceToWorld(LOSAnimFace *face, MATRIX *mtx) {
    VECTOR v;

    v.vx = face->v[0]->vx;
    v.vy = face->v[0]->vy;
    v.vz = face->v[0]->vz;
    ApplyMatrixLV(mtx, &v, &v);
    v.vx += mtx->t[0];
    v.vy += mtx->t[1];
    v.vz += mtx->t[2];
    face->v[0]->vx = v.vx;
    face->v[0]->vy = v.vy;
    face->v[0]->vz = v.vz;
    v.vx = face->v[1]->vx;
    v.vy = face->v[1]->vy;
    v.vz = face->v[1]->vz;
    ApplyMatrixLV(mtx, &v, &v);
    v.vx += mtx->t[0];
    v.vy += mtx->t[1];
    v.vz += mtx->t[2];
    face->v[1]->vx = v.vx;
    face->v[1]->vy = v.vy;
    face->v[1]->vz = v.vz;
    v.vx = face->v[2]->vx;
    v.vy = face->v[2]->vy;
    v.vz = face->v[2]->vz;
    ApplyMatrixLV(mtx, &v, &v);
    v.vx += mtx->t[0];
    v.vy += mtx->t[1];
    v.vz += mtx->t[2];
    face->v[2]->vx = v.vx;
    face->v[2]->vy = v.vy;
    face->v[2]->vz = v.vz;
}

/* hit = &losHitArray[i] per iteration and the sorted store re-takes &losHitArray[i]
 * (one giv for both; a hit++ pointer left a separate &hit->z register) */
void LOSSortHits(void) {
    LOSHit *hit;
    GameObjectRuntime *runtime;
    MATRIX *mtx;
    LOSHit *a;
    LOSHit *b;
    u32 i;
    u32 swapped;
    u8 isGobj;

    losSortedCount = 0;
    LOSSetupGTE(&gEGSIdentityMatrix);
    for (i = 0; i < losHitCount; i++) {
        hit = &losHitArray[i];
        isGobj = hit->type == 1;
        hit->z = LOSCalculateZ(hit->point.vx, hit->point.vy, hit->point.vz);
        if (isGobj) {
            runtime = gameObjectsInfo[hit->owner->id & 0x3FF].runtime;
            if (runtime != NULL && (u32)(runtime->npcState - 1) < 2) {
                mtx = &runtime->anim->frame->matrix;
                hit->z = LOSCalculateZ(mtx->t[0], -mtx->t[1], mtx->t[2]);
            }
        }
        if (hit->z > 32 && isGobj) {
            hit->z -= 32;
        }
        if (hit->z != 0) {
            losHitsSorted[losSortedCount++] = &losHitArray[i];
        }
    }
    if (losSortedCount < 2) {
        return;
    }
    swapped = losSortedCount;
    while (swapped != 0) {
        swapped = 0;
        for (i = 0; i < losSortedCount - 1; i++) {
            a = losHitsSorted[i];
            b = losHitsSorted[i + 1];
            if (a->z > b->z) {
                swapped++;
                losHitsSorted[i] = b;
                losHitsSorted[i + 1] = a;
            }
        }
    }
}

void LOSVerifyHits(LOSQuery *query) {
    u32 i;
    LOSHit *hit;
    LOSHit *first;

    losWindowCount = 0;
    LOSSortHits();
    if (losSortedCount != 0 && losHitsSorted[0]->z < losEndPointZ) {
        first = losHitsSorted[0];
        query->hit = 1;
        query->hitZ = first->z;
        query->hitWindow = *first->window;
        query->hitPoint = first->point;
        query->hitNormal = first->normal;
    }
    for (i = 0; i < losSortedCount; i++) {
        hit = losHitsSorted[i];
        if (hit->z > losEndPointZ || hit->type != 2) {
            losBlocker = hit;
            return;
        }
        if (hit->z < losEndPointZ + 20) {
            losWindowArray[losWindowCount++] = hit->window->u.id;
        }
    }
}

s32 AddFaceToFaceList(GameObjectRuntime *gobj, LOSFace *face) {
    face->prev = NULL;
    face->next = gobj->anim->faceList;
    gobj->anim->faceList = face;
    if (face->next != NULL) {
        face->next->prev = face;
    }
    return 1;
}

/* Store the owner id into a face normal's pad (CollisionBoxOwner.bits.id).
 * Being an inline keeps the pad address in the (twice-set) pointer param, so loop.c
 * doesn't strength-reduce it; `v` set twice keeps the mask off sched1's birthing boost. */
static inline void iSetCollisionBoxOwner(u16 *owner, s32 id) {
    s32 v = *owner & 0xC00F;
    v |= (id & 0x3FF) << 4;
    *owner = v;
}

s32 iCobjHandleCollisionBox_RegisterBox(GameObjectRuntime *gobj, GameObjectRuntime *owner, s32 box) {
    LOSBounds bounds;
    MATRIX m;
    VECTOR axis[3];
    VECTOR minProj[3];
    VECTOR maxProj[3];
    VECTOR corner;
    VECTOR delta[3];
    VECTOR *axisPtr[3];
    s32 t;
    s32 i;
    s32 j;
    s32 k;
    s32 a;
    s32 s;


    bounds = ((LOSModel *)gobj->anim->model)->bounds;
    t = bounds.min.vy;
    bounds.min.vy = bounds.max.vy;
    bounds.max.vy = t;
    bounds.min.vy = -bounds.min.vy;
    bounds.max.vy = -bounds.max.vy;
    m.m[0][0] = gobj->anim->frame->matrix.m[0][0];
    m.m[0][1] = -gobj->anim->frame->matrix.m[0][1];
    m.m[0][2] = gobj->anim->frame->matrix.m[0][2];
    m.m[1][0] = -gobj->anim->frame->matrix.m[1][0];
    m.m[1][1] = gobj->anim->frame->matrix.m[1][1];
    m.m[1][2] = -gobj->anim->frame->matrix.m[1][2];
    m.m[2][0] = gobj->anim->frame->matrix.m[2][0];
    m.m[2][1] = -gobj->anim->frame->matrix.m[2][1];
    m.m[2][2] = gobj->anim->frame->matrix.m[2][2];
    m.t[0] = gobj->anim->frame->matrix.t[0];
    m.t[1] = -gobj->anim->frame->matrix.t[1];
    m.t[2] = gobj->anim->frame->matrix.t[2];
    axis[0].vx = m.m[0][0];
    axis[0].vy = m.m[1][0];
    axis[0].vz = m.m[2][0];
    axis[1].vx = m.m[0][1];
    axis[1].vy = m.m[1][1];
    axis[1].vz = m.m[2][1];
    axis[2].vx = m.m[0][2];
    axis[2].vy = m.m[1][2];
    axis[2].vz = m.m[2][2];
    minProj[0].vx = EGSMathFIXMul(axis[0].vx, bounds.min.vx);
    minProj[0].vy = EGSMathFIXMul(axis[0].vy, bounds.min.vx);
    minProj[0].vz = EGSMathFIXMul(axis[0].vz, bounds.min.vx);
    minProj[1].vx = EGSMathFIXMul(axis[1].vx, bounds.min.vy);
    minProj[1].vy = EGSMathFIXMul(axis[1].vy, bounds.min.vy);
    minProj[1].vz = EGSMathFIXMul(axis[1].vz, bounds.min.vy);
    minProj[2].vx = EGSMathFIXMul(axis[2].vx, bounds.min.vz);
    minProj[2].vy = EGSMathFIXMul(axis[2].vy, bounds.min.vz);
    minProj[2].vz = EGSMathFIXMul(axis[2].vz, bounds.min.vz);
    maxProj[0].vx = EGSMathFIXMul(axis[0].vx, bounds.max.vx);
    maxProj[0].vy = EGSMathFIXMul(axis[0].vy, bounds.max.vx);
    maxProj[0].vz = EGSMathFIXMul(axis[0].vz, bounds.max.vx);
    maxProj[1].vx = EGSMathFIXMul(axis[1].vx, bounds.max.vy);
    maxProj[1].vy = EGSMathFIXMul(axis[1].vy, bounds.max.vy);
    maxProj[1].vz = EGSMathFIXMul(axis[1].vz, bounds.max.vy);
    maxProj[2].vx = EGSMathFIXMul(axis[2].vx, bounds.max.vz);
    maxProj[2].vy = EGSMathFIXMul(axis[2].vy, bounds.max.vz);
    maxProj[2].vz = EGSMathFIXMul(axis[2].vz, bounds.max.vz);
    corner.vx = m.t[0];
    corner.vy = m.t[1];
    corner.vz = m.t[2];
    corner.vx += minProj[0].vx;
    corner.vy += minProj[0].vy;
    corner.vz += minProj[0].vz;
    corner.vx += minProj[1].vx;
    corner.vy += minProj[1].vy;
    corner.vz += minProj[1].vz;
    corner.vx += minProj[2].vx;
    corner.vy += minProj[2].vy;
    corner.vz += minProj[2].vz;
    delta[0].vx = maxProj[0].vx - minProj[0].vx;
    delta[0].vy = maxProj[0].vy - minProj[0].vy;
    delta[0].vz = maxProj[0].vz - minProj[0].vz;
    delta[1].vx = maxProj[1].vx - minProj[1].vx;
    delta[1].vy = maxProj[1].vy - minProj[1].vy;
    delta[1].vz = maxProj[1].vz - minProj[1].vz;
    delta[2].vx = maxProj[2].vx - minProj[2].vx;
    delta[2].vy = maxProj[2].vy - minProj[2].vy;
    delta[2].vz = maxProj[2].vz - minProj[2].vz;
    for (i = 0; i < 2; i++) {
        for (j = 0; j < 2; j++) {
            for (k = 0; k < 2; k++) {
                setVector(&mCollisionBoxVertices[box][i][j][k], corner.vx, corner.vy, corner.vz);
                if (i == 1) {
                    addVector(&mCollisionBoxVertices[box][i][j][k], &delta[0]);
                }
                if (j == 1) {
                    addVector(&mCollisionBoxVertices[box][i][j][k], &delta[1]);
                }
                if (k == 1) {
                    addVector(&mCollisionBoxVertices[box][i][j][k], &delta[2]);
                }
            }
        }
    }
    axisPtr[0] = &axis[0];
    axisPtr[1] = &axis[1];
    axisPtr[2] = &axis[2];
    for (a = 0; a < 3; a++) {
        for (s = 0; s < 2; s++) {
            if (s == 0) {
                setVector(&mCollisionBoxFaces[box][a][0].normal, -axisPtr[a]->vx, -axisPtr[a]->vy, -axisPtr[a]->vz);
            } else {
                setVector(&mCollisionBoxFaces[box][a][s].normal, axisPtr[a]->vx, axisPtr[a]->vy, axisPtr[a]->vz);
            }
            mCollisionBoxFaces[box][a][s].normal.pad = 0;
            iSetCollisionBoxOwner((u16 *)&mCollisionBoxFaces[box][a][s].normal.pad, gobj->id + 1);
        }
    }
    for (a = 0; a < 3; a++) {
        for (s = 0; s < 2; s++) {
            mCollisionBoxFaceFlags[box][a][s] = 0;
            AddFaceToFaceList(owner, (LOSFace *)&mCollisionBoxFaces[box][a][s]);
        }
    }
    return 1;
}

s32 GetFuzzyDistancePoint2Point(VECTOR *a, VECTOR *b) {
    s32 dx;
    s32 dy;
    s32 dz;
    s32 t;

    dx = a->vx - b->vx;
    if (dx < 0) {
        dx = b->vx - a->vx;
    }
    dy = a->vy - b->vy;
    if (dy < 0) {
        dy = b->vy - a->vy;
    }
    dz = a->vz - b->vz;
    if (dz < 0) {
        dz = b->vz - a->vz;
    }
    if (dx < dy) {
        t = dx;
        dx = dy;
        dy = t;
    }
    if (dx < dz) {
        t = dx;
        dx = dz;
        dz = t;
    }
    return dx + ((dy + dz) >> 2);
}

/* __FILE__ of the assert below, kept word-padded for the Psy-Q rodata layout */
static const char D_80012300[12] = "s/gtegeo.c";

/* The early `node = node->next; continue;` exits are what the original had: the duplicated
 * advances give node enough (loop-weighted) refs to get s4 ahead of the other loop values. */
void GobjCollideLoop(GameObjectRuntime *gobj) {
    LOSHit hit;
    LOSHit *hitp;
    LOSHit backHit;
    VECTOR pos;
    VECTOR otherPos;
    LOSBox box;
    LOSCollideNode *node;
    LOSCollideAnim *self;
    LOSCollideAnim *anim;
    LOSModel *model;
    GameObjectRuntime *target;
    u32 modelFlags;
    s32 boxCount;
    u8 flags;
    u8 isCharacter;

    node = D_8012D8CC;
    self = (LOSCollideAnim *)gobj->anim;
    GsSetProjection(20);
    pos.vx = gobj->anim->frame->matrix.t[0];
    pos.vy = gobj->anim->frame->matrix.t[1];
    pos.vz = gobj->anim->frame->matrix.t[2];
    pos.vy = -pos.vy;
    boxCount = 0;
    pos = ((LOSMove *)gobj->move)->pos;
    losFrame = self->frame->matrix;
    losFrame.t[1] = -17 - pos.vy;
    EGSDisplaySetAlternateViewpoint(&losFrame);
    losInvMatrix = &losRevMatrix;
    losAnimFace.count = 3;
    losAnimFace.normal = &losAnimFace.normalBuf;
    losRay.origin.vx = losFrame.t[0];
    losRay.origin.vy = losFrame.t[1];
    losRay.origin.vz = losFrame.t[2];
    losRay.origin.vy = -losRay.origin.vy;
    losRay.dir.vx = losFrame.m[0][2];
    losRay.dir.vy = losFrame.m[1][2];
    losRay.dir.vz = losFrame.m[2][2];
    losRay.dir.vy = -losRay.dir.vy;
    hitp = &hit;
    while (node != NULL) {
        anim = node->anim;
        model = anim->model;
        flags = anim->collideFlags;
        if (anim == self || !(flags & 6)) {
            node = node->next;
            continue;
        }
        {
            if (model->box.state == -1) {
                InitBxv(model);
            }
            otherPos.vx = anim->frame->matrix.t[0];
            otherPos.vy = anim->frame->matrix.t[1];
            otherPos.vz = anim->frame->matrix.t[2];
            otherPos.vy = -otherPos.vy;
            if (GetFuzzyDistancePoint2Point(&pos, &otherPos) > model->box.radius) {
                node = node->next;
                continue;
            }
            {
                if (flags & 2) {
                    LOSBounds saved;

                    target = gameObjectsInfo[anim->id & 0x3FF].runtime;
                    modelFlags = model->flags;
                    isCharacter = (u32)(target->npcState - 1) < 2;
#line 645
                    if (!(boxCount < 15)) EGSDebugAssert(1, 0, (char *)D_80012300, __LINE__);
                    if (isCharacter) {
                        saved = model->bounds;
                        model->bounds.max.vz = 0x80;
                        model->bounds.min.vz = -0x40;
                        model->bounds.max.vx = 0x30;
                        model->bounds.min.vx = -0x30;
                    }
                    iCobjHandleCollisionBox_RegisterBox(target, gobj, boxCount++);
                    if (isCharacter) {
                        model->bounds = saved;
                    }
                    if (!(modelFlags & 0x410000)) {
                        anim->collided = 1;
                    }
                }
                if (flags & 4) {
                    LOSSetupGTEBoth(&anim->frame->matrix, &losRevMatrix);
                    hitp->owner = anim;
                    if (GOBJ_TYPE(anim->id & 0x3FF) == 0x20) {
                        InitBxv2(&box, model, -0x29A);
                    } else {
                        InitBxv2(&box, model, -0x32);
                    }
                    if (LOSRotBoxV(&box, -0x29A, hitp, &losRay)) {
                        LOSSetupGTE(&gEGSIdentityMatrix);
                        if (LOSCalculateZ(hitp->point.vx, hitp->point.vy, hitp->point.vz) != 0) {
                            LOSSetupGTE(&anim->frame->matrix);
                            backHit.owner = anim;
                            if (!LOSRotBoxV(&box, 0x29A, &backHit, &losRay) ||
                                (LOSSetupGTE(&gEGSIdentityMatrix),
                                 LOSCalculateZ(backHit.point.vx, backHit.point.vy, backHit.point.vz) == 0)) {
                                VECTOR push = { 0 };
                                GameObjectRuntime *other;
                                LOSMove *move;

                                other = gameObjectsInfo[anim->id & 0x3FF].runtime;
                                if (other != NULL && other->move != NULL) {
                                    push = ((LOSMove *)other->move)->pendingMove;
                                }
                                if ((flags & 2) && (push.vx != 0 || push.vy != 0 || push.vz != 0)) {
                                    move = (LOSMove *)gobj->move;
                                    move->pendingMove.vx += push.vx;
                                    move->pendingMove.vy += push.vy;
                                    move->pendingMove.vz += push.vz;
                                }
                                SysPostMessage(0x1A, 1, gobj->id, other->id, 0, 0, 0, 0);
                            }
                        }
                    }
                }
            }
        }
        node = node->next;
    }
}
