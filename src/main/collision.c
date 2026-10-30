#include "common.h"
#include "system.h"
#include "level.h"
#include "gameobj.h"
#include "gobj.h"
#include "egsmath.h"

/* Same layout as movement.c's MovementState/MovementBasicInfo. */
typedef struct MovementState {
    /* 0x00 */ VECTOR32 pos;
    /* 0x10 */ VECTOR32 delta;
    /* 0x20 */ VECTOR32 vel;
    /* 0x30 */ VECTOR32 accel;
} MovementState;

typedef struct MovementBasicInfo {
    /* 0x00 */ MovementState cur;
    /* 0x40 */ MovementState next;
} MovementBasicInfo;

/* A terrain polygon as the collision probes see it. */
typedef struct CollPoly {
    /* 0x00 */ s32 count; /* vertices */
    /* 0x04 */ s32 unk4;
    /* 0x08 */ SVECTOR *normal;
    /* 0x0C */ s32 unkC;
    /* 0x10 */ s32 unk10;
    /* 0x14 */ SVECTOR *verts[4];
} CollPoly;

/* A polygon flattened onto its dominant plane (x/z hold the two kept axes). */
typedef struct CollPoly2D {
    /* 0x00 */ s32 count;
    /* 0x04 */ VECTOR32 v[5];
} CollPoly2D;

/* A moving sphere tested against one polygon. */
typedef struct CollisionProbe {
    /* 0x00 */ VECTOR32 pos;
    /* 0x10 */ s32 radius;
    /* 0x14 */ VECTOR32 move;
    /* 0x24 */ CollPoly *poly;
    /* 0x28 */ s32 unk28;
    /* 0x2C */ s32 unk2C;
    /* 0x30 */ s32 unk30;
    /* 0x34 */ s32 unk34;
    /* 0x38 */ u8 hit;
    /* 0x3C */ s32 dist;      /* signed distance of pos from the plane, << 12 */
    /* 0x40 */ s32 planeDist; /* -(move . normal) */
    /* 0x44 */ VECTOR32 contact;
    /* 0x54 */ VECTOR32 unk54;
    /* 0x64 */ VECTOR32 unk64;
    /* 0x74 */ VECTOR32 unk74;
} CollisionProbe;

/* CollisionDetectorCreate request (0x3C bytes). */
typedef struct CollisionDetectorParams {
    /* 0x00 */ EGSFrame *frame;
    /* 0x04 */ VECTOR32 offset; /* in frame space */
    /* 0x14 */ s32 unk14;
    /* 0x18 */ s32 unk18;
    /* 0x1C */ s32 unk1C;
    /* 0x20 */ s32 unk20;
    /* 0x24 */ s32 unk24;
    /* 0x28 */ s32 unk28;
    /* 0x2C */ s32 unk2C;
    /* 0x30 */ s32 unk30;
    /* 0x34 */ s32 unk34;
    /* 0x38 */ s32 *unk38;
} CollisionDetectorParams;

/* A collision probe (0x178 bytes); objects keep lists of them for pushing/grabbing. */
typedef struct CollisionDetector {
    /* 0x000 */ EGSFrame *frame;
    /* 0x004 */ VECTOR32 offset;
    /* 0x014 */ s32 unk14;
    /* 0x018 */ s32 unk18;
    /* 0x01C */ s32 unk1C;
    /* 0x020 */ s32 unk20;
    /* 0x024 */ s32 unk24;
    /* 0x028 */ s32 unk28;
    /* 0x02C */ s32 unk2C;
    /* 0x030 */ s32 unk30;
    /* 0x034 */ s32 unk34;
    /* 0x038 */ s32 unk38;
    /* 0x03C */ MovementBasicInfo translation;
    /* 0x0BC */ MovementBasicInfo rotation;
    /* 0x13C */ VECTOR32 unk13C;
    /* 0x14C */ VECTOR32 unk14C;
    /* 0x15C */ VECTOR32 unk15C;
    /* 0x16C */ s32 unk16C;
    /* 0x170 */ s32 *unk170;
    /* 0x174 */ struct CollisionDetector *next;
} CollisionDetector; /* size 0x178 */

/* Grab request (MoveInfo.coll->grab): pulls the object towards a bone-relative target. */
typedef struct CollisionGrab {
    /* 0x00 */ s32 bone; /* -1: the model frame */
    /* 0x04 */ VECTOR32 offset; /* bone space, added to the target every tick */
    /* 0x14 */ s32 duration; /* ticks, < 0: until released */
    /* 0x18 */ u8 keepTarget; /* keep the preset target on the first tick */
    /* 0x19 */ u8 pad19[3];
    /* 0x1C */ VECTOR32 target;
    /* 0x2C */ s32 ticks;
} CollisionGrab;

/* Per-object collision state (MoveInfo.coll, 0x40 bytes). */
typedef struct CollisionInfo {
    /* 0x00 */ s32 mode; /* 1: grabbing */
    /* 0x04 */ s32 prevMode;
    /* 0x08 */ CollisionGrab *grab;
    /* 0x0C */ s32 unk0C;
    /* 0x10 */ s32 unk10;
    /* 0x14 */ s32 unk14;
    /* 0x18 */ s32 unk18;
    /* 0x1C */ s8 unk1C; /* nonzero: blocked along x (relative to the facing) */
    /* 0x1D */ s8 unk1D;
    /* 0x1E */ s8 unk1E;
    /* 0x1F */ u8 unk1F;
    /* 0x20 */ CollisionDetector *unk20; /* detector the object currently rests on */
    /* 0x24 */ CollisionDetector *push;
    /* 0x28 */ CollisionDetector *hold;      /* detector attached to the hips (CAS) */
    /* 0x2C */ CollisionDetector *holdSaved; /* hold while detached */
    /* 0x30 */ CollisionDetector *unk30; /* detector following the model frame */
    /* 0x34 */ CollisionDetector *pushList;
    /* 0x38 */ s32 unk38;
    /* 0x3C */ u8 unk3C;
    /* 0x3D */ u8 pad3D[3];
} CollisionInfo; /* size 0x40 */

/* Full view of MoveInfo (gobj.h), same layout as movement.c's MovementInfo. */
typedef struct CollMoveInfo {
    /* 0x000 */ MovementBasicInfo translation;
    /* 0x080 */ MovementBasicInfo rotation;
    /* 0x100 */ u8 frozen;
    /* 0x101 */ u8 gravity;
    /* 0x102 */ u8 unk102[2];
    /* 0x104 */ s32 mass; /* 0x1000: 1.0 */
    /* 0x108 */ u8 moved;
    /* 0x109 */ u8 collides;
    /* 0x10A */ u8 unk10A[2];
    /* 0x10C */ VECTOR32 lowest;
    /* 0x11C */ VECTOR32 unk11C;
    /* 0x12C */ s32 height;
    /* 0x130 */ s32 unk130;
    /* 0x134 */ s32 unk134;
    /* 0x138 */ s32 unk138;
    /* 0x13C */ s32 unk13C;
    /* 0x140 */ u8 unk140[0x20];
    /* 0x160 */ VECTOR32 unk160; /* added to attached detectors positions */
    /* 0x170 */ u8 unk170[0x24];
    /* 0x194 */ VECTOR32 pendingMove;
    /* 0x1A4 */ u32 collFlags;
    /* 0x1A8 */ void *charMove;
    /* 0x1AC */ s32 physics;
    /* 0x1B0 */ CollisionInfo *coll;
    /* 0x1B4 */ Gobj *next;
} CollMoveInfo;

#define CMOVE(obj) ((CollMoveInfo *)(obj)->move)
/* Ledge found by the edge probes (lives at charMove + 0x7C). */
typedef struct CollEdge {
    VECTOR32 pt;
    VECTOR32 dir;
    VECTOR32 normal;
} CollEdge;
/* Edge grab request (0x48 bytes, tables in the player code). */
typedef struct CollEdgeGrab {
    CollisionGrab grab;
    s32 side;    /* 0x30 */
    s32 up;      /* 0x34 */
    s32 out;     /* 0x38 */
    s32 unk3C;
    u8 isEdge;   /* 0x40 */
    u8 pad41[3];
    s32 face;    /* 0x44: 0 keep, 1 against the edge normal, 2 along it */
} CollEdgeGrab;
s32 EGSFrameSetPosition(EGSFrame *frame, EGSFrame *ref, VECTOR32 *pos);
s32 EGSFrameSetOrientation(EGSFrame *frame, s32 arg1, VECTOR32 *dir, s32 roll);
s32 EGSMathINT_SolveLinear2VectorEquationForParameters(VECTOR32 *v, s32 *out);

/* The camera frame. */
#define CAM_FRAME (*(EGSFrame **)*gCamera)
u8 JoystickGobjIsInPureStrafe(Gobj *gobj);
/* 12.12 fixed point dot product. */
#define DOT_FIX(a, b) (EGSMathFIXMul((a).vx, (b).vx) + EGSMathFIXMul((a).vy, (b).vy) + EGSMathFIXMul((a).vz, (b).vz))

s32 EGSMathFIX_VectorClipToLength(VECTOR32 *v, s32 length, VECTOR32 *out);
#define HALVE(x) ((x) >= 0 ? (x) >> 1 : -(-(x) >> 1))


/* MIN(m, 0) - r, written so that it does not fold to a MIN_EXPR */
#define EXTEND_DOWN(m, r) ((m) <= 0 ? (m) - (r) : -(r))
#define MAX(a, b) ((a) < (b) ? (b) : (a))

/* Axis-aligned box swept by a probe. */
typedef struct CollBox {
    VECTOR32 min;
    VECTOR32 max;
} CollBox;


/* A surface hit handed to the detector-motion helpers (only the normal is known). */
typedef struct CollHit {
    u8 unk0[0x44];
    VECTOR32 normal;
} CollHit;
s32 EGSMath_VectorModulus(VECTOR32 *v, s32 *out);


/* MoveInfo.charMove as collision.c sees it. */
typedef struct CollCharMove {
    u8 unk0[0x100];
    s32 groundY; /* height of the last ground stood on, 0x80000001: none */
} CollCharMove;
#define GOBJ_GRABBING(obj) \
    ((obj) != NULL && CMOVE(obj) != NULL && CMOVE(obj)->coll != NULL && CMOVE(obj)->coll->mode == 1)


#define FIX_TO_INT(x) ((x) >= 0 ? (x) >> 12 : -(-(x) >> 12))
#define IABS(x) ((x) >= 0 ? (x) : -(x)) /* folds to an abs insn */

/* Octagonal approximation of the length of (x, z). */
#define APPROX_LEN(vx, vz) ({                   \
    s32 x_ = IABS(vx);                           \
    s32 z_ = IABS(vz);                           \
    s32 sum_ = x_ + z_;                         \
    s32 diff_ = x_ - z_;                        \
    s32 half_ = sum_ >> 1;                      \
    s32 quarter_ = sum_ >> 2;                   \
    if (diff_ < 0) {                            \
        diff_ = z_ - x_;                        \
    }                                           \
    if (diff_ < half_) {                        \
        sum_ -= quarter_;                       \
    } else if (diff_ <= half_ + quarter_) {     \
        sum_ -= sum_ >> 3;                      \
    }                                           \
    sum_;                                       \
})

extern VECTOR32 D_800122C4; /* {0, 0, 0}, rodata */

void *EGSMemMalloc(u32 size);
void EGSFrameTransformPoint(VECTOR32 *in, EGSFrame *frame, EGSFrame *ref, VECTOR32 *out);
s32 MovementGobjGetPosition(Gobj *gobj, VECTOR32 *out);
s32 MovementBasicInfoReset(MovementBasicInfo *info, VECTOR32 *pos);
void func_8006B860(CollisionProbe *p);
static u8 func_8006BD48(VECTOR32 *pt, CollPoly *poly, s32 arg2);
void func_8006C2D0(CollisionProbe *p);
void FriendlyInit(void);
s8 *CASGetHipMotion(s16 id);
u8 JoystickGobjIsInFullRetreat(Gobj *gobj);
u8 JoystickGobjIsWalkKeyPressed(Gobj *gobj);
s32 EGSFrameTransformVector(VECTOR32 *in, EGSFrame *frame, EGSFrame *ref, VECTOR32 *out);
s32 MovementBasicInfoUpdate(MovementBasicInfo *info, VECTOR32 *pos);
s32 MovementBasicInfoUpdateCurrent(MovementBasicInfo *info);
s32 EGSMath_VectorModulus(VECTOR32 *v, s32 *out);
s32 EGSMathINT_VectorScaleToLength(VECTOR32 *v, s32 length, VECTOR32 *out);
void CollisionPhysicsGobj_DetectorMotion(Gobj *obj, s32 arg1, CollisionDetector *det, s32 *arg3, s32 arg4, void *arg5);
s32 MovementGobjUpdateVelocity(Gobj *gobj, u8 translation, u8 rotation);
s32 MovementGobjUpdateMovement(Gobj *gobj, u8 translation, u8 rotation);
void CollisionGobjSetPushDetector(Gobj *obj, s32 which);
s32 func_8006DDB4(Gobj *obj);
s32 func_8006E0BC(Gobj *obj, u8 attach);
void func_8006CB88(Gobj *obj, CollisionDetector *det, s32 arg2, s32 *arg3, s32 *arg4);
static s32 func_8006EABC(Gobj *obj);
static s32 func_8006F49C(Gobj *obj);
s32 func_8006F4BC(Gobj *obj);
void func_8006F810(Gobj *obj);
s32 MovementGobjUpdatePosition(Gobj *obj, s32 a, s32 b);
s32 CollisionDetectorInitParams(CollisionDetector *det, CollisionDetectorParams *params, Gobj *ref);
s32 CollisionDetectorAddToList(CollisionDetector *det, s32 pos, CollisionDetector **list);
s32 CollisionDetectorRemoveFromList(CollisionDetector *det, CollisionDetector **list);
s32 CollisionGobjSetModeGrab(Gobj *obj, CollisionGrab *grab);
s32 CollisionGobjSetMode(Gobj *obj, s32 *mode);
u8 CollisionGobjGetInfo(Gobj *obj, CollisionInfo **out);

/* The detector the object is carried by: the held one, else the one it rests on. */
static inline CollisionDetector *CollisionInfoActiveDetector(CollisionInfo *info) {
    CollisionDetector *d = info->hold;
    if (d == NULL) {
        d = info->unk20;
    }
    return d;
}

void func_8006B860(CollisionProbe *p) {
    VECTOR32 n;
    VECTOR32 m;
    VECTOR32 rel;
    VECTOR32 a;
    VECTOR32 b;
    s32 planeDist;
    s32 dist;
    s32 ratio;
    s32 r;

    /* memory-resident scalars: the original took their addresses somewhere */
    (void)&planeDist;
    (void)&dist;
    (void)&ratio;
    p->hit = 0;
    n.vx = p->poly->normal->vx;
    n.vy = p->poly->normal->vy;
    n.vz = p->poly->normal->vz;
    m.vx = FIX_TO_INT(p->move.vx);
    m.vy = FIX_TO_INT(p->move.vy);
    m.vz = FIX_TO_INT(p->move.vz);
    planeDist = -(m.vx * n.vx + m.vy * n.vy + m.vz * n.vz);
    if (planeDist >= -0x54C0) {
        rel.vx = p->pos.vx - p->poly->verts[0]->vx;
        rel.vy = p->pos.vy - p->poly->verts[0]->vy;
        rel.vz = p->pos.vz - p->poly->verts[0]->vz;
        dist = rel.vx * n.vx + rel.vy * n.vy + rel.vz * n.vz;
        rel.vx <<= 12;
        rel.vy <<= 12;
        rel.vz <<= 12;
        r = p->radius << 12;
        if (dist - planeDist - r <= p->unk28 && dist >= -p->unk2C) {
            p->hit = 1;
            p->dist = dist;
            p->planeDist = planeDist;
            if (p->unk34 >= planeDist) {
                a.vx = EGSMathFIXMul(n.vx, planeDist - dist);
                a.vy = EGSMathFIXMul(n.vy, planeDist - dist);
                a.vz = EGSMathFIXMul(n.vz, planeDist - dist);
                b.vx = p->move.vx + a.vx;
                b.vy = p->move.vy + a.vy;
                b.vz = p->move.vz + a.vz;
            } else {
                ratio = EGSMathFIXDiv(dist, planeDist);
                b.vx = EGSMathFIXMul(p->move.vx, ratio);
                /* the const read lets the b.vx store sink past this load, as in the original */
                b.vy = EGSMathFIXMul(((const VECTOR32 *)&p->move)->vy, ratio);
                b.vz = EGSMathFIXMul(p->move.vz, ratio);
            }
            p->contact.vx = rel.vx + b.vx;
            p->contact.vy = rel.vy + b.vy;
            p->contact.vz = rel.vz + b.vz;
        }
    }
}

static s32 func_8006BB88(VECTOR32 *pt, CollPoly2D *poly, s32 tol) {
    VECTOR32 edge;
    VECTOR32 rel;
    s32 dot;
    s32 len;
    s32 i;
    s32 j;

    (void)&dot;
    tol = FIX_TO_INT(tol);
    for (i = 0; i < poly->count; i++) {
        j = i + 1;
        if (j == poly->count) {
            j = 0;
        }
        edge.vx = poly->v[i].vz - poly->v[j].vz;
        edge.vz = poly->v[j].vx - poly->v[i].vx;
        len = APPROX_LEN(edge.vx, edge.vz) > 1 ? APPROX_LEN(edge.vx, edge.vz) : 1;
        rel.vx = pt->vx - poly->v[i].vx;
        rel.vz = pt->vz - poly->v[i].vz;
        dot = rel.vx * edge.vx + rel.vz * edge.vz;
        if (tol * len < dot) {
            return 0;
        }
    }
    return 1;
}

/* Flattens poly onto its dominant axis plane and tests pt against it. */
static u8 func_8006BD48(VECTOR32 *pt, CollPoly *poly, s32 tol) {
    VECTOR32 p;
    CollPoly2D flat;
    s32 ax;
    s32 ay;
    s32 az;
    s32 max;
    s32 i;

    ax = IABS(poly->normal->vx);
    ay = IABS(poly->normal->vy);
    az = IABS(poly->normal->vz);
    max = MAX(ax, MAX(ay, az));
    flat.count = poly->count;
    if (max == ax) {
        if (poly->normal->vx < 0) {
            p.vx = FIX_TO_INT(pt->vy);
            p.vz = FIX_TO_INT(pt->vz);
            flat.v[0].vx = 0;
            flat.v[0].vz = 0;
            for (i = 1; i < poly->count; i++) {
                flat.v[i].vx = poly->verts[i]->vy - poly->verts[0]->vy;
                flat.v[i].vz = poly->verts[i]->vz - poly->verts[0]->vz;
            }
        } else {
            p.vx = -FIX_TO_INT(pt->vy);
            p.vz = FIX_TO_INT(pt->vz);
            flat.v[0].vx = 0;
            flat.v[0].vz = 0;
            for (i = 1; i < poly->count; i++) {
                flat.v[i].vx = poly->verts[0]->vy - poly->verts[i]->vy;
                flat.v[i].vz = poly->verts[i]->vz - poly->verts[0]->vz;
            }
        }
    } else if (max == ay) {
        if (poly->normal->vy >= 0) {
            p.vx = FIX_TO_INT(pt->vx);
            p.vz = FIX_TO_INT(pt->vz);
            flat.v[0].vx = 0;
            flat.v[0].vz = 0;
            for (i = 1; i < poly->count; i++) {
                flat.v[i].vx = poly->verts[i]->vx - poly->verts[0]->vx;
                flat.v[i].vz = poly->verts[i]->vz - poly->verts[0]->vz;
            }
        } else {
            p.vx = -FIX_TO_INT(pt->vx);
            p.vz = FIX_TO_INT(pt->vz);
            flat.v[0].vx = 0;
            flat.v[0].vz = 0;
            for (i = 1; i < poly->count; i++) {
                flat.v[i].vx = poly->verts[0]->vx - poly->verts[i]->vx;
                flat.v[i].vz = poly->verts[i]->vz - poly->verts[0]->vz;
            }
        }
    } else {
        if (poly->normal->vz < 0) {
            p.vx = FIX_TO_INT(pt->vx);
            p.vz = FIX_TO_INT(pt->vy);
            flat.v[0].vx = 0;
            flat.v[0].vz = 0;
            for (i = 1; i < poly->count; i++) {
                flat.v[i].vx = poly->verts[i]->vx - poly->verts[0]->vx;
                flat.v[i].vz = poly->verts[i]->vy - poly->verts[0]->vy;
            }
        } else {
            p.vx = -FIX_TO_INT(pt->vx);
            p.vz = FIX_TO_INT(pt->vy);
            flat.v[0].vx = 0;
            flat.v[0].vz = 0;
            for (i = 1; i < poly->count; i++) {
                flat.v[i].vx = poly->verts[0]->vx - poly->verts[i]->vx;
                flat.v[i].vz = poly->verts[i]->vy - poly->verts[0]->vy;
            }
        }
    }
    return func_8006BB88(&p, &flat, tol);
}

#ifdef NON_MATCHING
/* regalloc: radius << 12 is computed into a1 and copied to s3 in the original */
void func_8006C2D0(CollisionProbe *p) {
    s32 r = p->radius << 12;
    s32 plane = p->planeDist;
    SVECTOR *nrm = p->poly->normal;
    s32 dist = p->dist;

    if (p->unk34 >= plane) {
        VECTOR32 n;

        n.vx = nrm->vx;
        n.vy = nrm->vy;
        n.vz = nrm->vz;
        p->unk54.vx = EGSMathFIXMul(n.vx, plane - dist + r);
        p->unk54.vy = EGSMathFIXMul(n.vy, plane - dist + r);
        p->unk54.vz = EGSMathFIXMul(n.vz, plane - dist + r);
        p->unk64.vx = EGSMathFIXMul(n.vx, plane);
        p->unk64.vy = EGSMathFIXMul(n.vy, plane);
        p->unk64.vz = EGSMathFIXMul(n.vz, plane);
        p->unk74.vx = p->unk54.vx - p->unk64.vx;
        p->unk74.vy = p->unk54.vy - p->unk64.vy;
        p->unk74.vz = p->unk54.vz - p->unk64.vz;
    } else {
        VECTOR32 t;
        VECTOR32 d;

        d.vx = EGSMathFIXDiv(p->move.vx, plane);
        /* the const read lets the previous store sink past this load, as in the original */
        d.vy = EGSMathFIXDiv(((const VECTOR32 *)&p->move)->vy, plane);
        d.vz = EGSMathFIXDiv(p->move.vz, plane);
        t.vx = EGSMathFIXMul(d.vx, dist - r);
        t.vy = EGSMathFIXMul(d.vy, dist - r);
        t.vz = EGSMathFIXMul(d.vz, dist - r);
        p->unk54.vx = t.vx - p->move.vx;
        p->unk54.vy = t.vy - p->move.vy;
        p->unk54.vz = t.vz - p->move.vz;
        p->unk64 = p->unk54;
        p->unk74.vx = 0;
        p->unk74.vy = 0;
        p->unk74.vz = 0;
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/collision", func_8006C2D0);
#endif

void func_8006C4B0(CollisionProbe *p) {
    func_8006B860(p);
    if (p->hit) {
        p->hit = func_8006BD48(&p->contact, p->poly, p->unk30);
        if (p->hit) {
            func_8006C2D0(p);
        }
    }
}

/* Box covering a sphere of radius rad/radY moving by move (<< 12), grown by r. */
void func_8006C510(VECTOR32 *pos, s32 r, s32 bottom, s32 top, VECTOR32 *move, s32 rad, s32 radY, CollBox *box) {
    box->min.vx = pos->vx + FIX_TO_INT(EXTEND_DOWN(move->vx, rad)) - r;
    box->min.vy = bottom + FIX_TO_INT(EXTEND_DOWN(move->vy, radY));
    box->min.vz = pos->vz + FIX_TO_INT(EXTEND_DOWN(move->vz, rad)) - r;
    box->max.vx = pos->vx + FIX_TO_INT(MAX(move->vx, 0) + rad) + r;
    box->max.vy = top + FIX_TO_INT(MAX(move->vy, 0) + radY);
    box->max.vz = pos->vz + FIX_TO_INT(MAX(move->vz, 0) + rad) + r;
}

s32 func_8006C7E8(CollBox *box, CollPoly *poly) {
    SVECTOR *a = poly->verts[0];
    SVECTOR *b = poly->verts[1];
    SVECTOR *c = poly->verts[2];
    SVECTOR *d;

    if (poly->count == 3) {
        if (a->vy < box->min.vy && b->vy < box->min.vy && c->vy < box->min.vy) {
            return 0;
        }
        if (a->vy > box->max.vy && b->vy > box->max.vy && c->vy > box->max.vy) {
            return 0;
        }
        if (a->vx < box->min.vx && b->vx < box->min.vx && c->vx < box->min.vx) {
            return 0;
        }
        if (a->vx > box->max.vx && b->vx > box->max.vx && c->vx > box->max.vx) {
            return 0;
        }
        if (a->vz < box->min.vz && b->vz < box->min.vz && c->vz < box->min.vz) {
            return 0;
        }
        if (a->vz > box->max.vz && b->vz > box->max.vz && c->vz > box->max.vz) {
            return 0;
        }
    } else if (poly->count == 4) {
        d = poly->verts[3];
        if (a->vy < box->min.vy && b->vy < box->min.vy && c->vy < box->min.vy && d->vy < box->min.vy) {
            return 0;
        }
        if (a->vy > box->max.vy && b->vy > box->max.vy && c->vy > box->max.vy && d->vy > box->max.vy) {
            return 0;
        }
        if (a->vx < box->min.vx && b->vx < box->min.vx && c->vx < box->min.vx && d->vx < box->min.vx) {
            return 0;
        }
        if (a->vx > box->max.vx && b->vx > box->max.vx && c->vx > box->max.vx && d->vx > box->max.vx) {
            return 0;
        }
        if (a->vz < box->min.vz && b->vz < box->min.vz && c->vz < box->min.vz && d->vz < box->min.vz) {
            return 0;
        }
        if (a->vz > box->max.vz && b->vz > box->max.vz && c->vz > box->max.vz && d->vz > box->max.vz) {
            return 0;
        }
    } else {
        return 0;
    }
    return 1;
}

INCLUDE_ASM("asm/main/nonmatchings/collision", func_8006CB88);

#ifdef NON_MATCHING
/* after the CB88 call the target re-tests a constant-0 reg (v0=0; bnez) before the delta update */
s32 func_8006D9F4(Gobj *obj) {
    VECTOR32 pos;
    VECTOR32 delta;
    CollisionInfo *info;
    CollisionDetector *det;
    CollisionDetector *cur;

    info = CMOVE(obj)->coll;
    func_8006DDB4(obj);
    func_8006E0BC(obj, 1);
    det = info->unk20;
    if (det != NULL && CMOVE(obj)->coll->mode == 3) {
        CollisionPhysicsGobj_DetectorMotion(obj, 0, det, det->unk170, det->unk16C, &info->unk3C);
    }
    det = info->unk30;
    if (det != NULL) {
        CollisionPhysicsGobj_DetectorMotion(obj, 0, det, det->unk170, det->unk16C, &info->unk3C);
    }
    MovementGobjUpdateVelocity(obj, 1, 0);
    MovementGobjUpdateMovement(obj, 1, 0);
    if (CMOVE(obj)->coll->mode == 3) {
        if (info->push == NULL) {
            CollisionGobjSetPushDetector(obj, -1);
        }
        info->unk20 = info->push;
        CollisionGobjSetPushDetector(obj, 0);
    }
    func_8006E0BC(obj, 0);
    pos = CMOVE(obj)->translation.cur.pos;
    delta = CMOVE(obj)->translation.next.delta;
    det = CollisionInfoActiveDetector(info);
    cur = info->unk20;
    if (det != NULL) {
        det->unk13C.vx = pos.vx + det->translation.cur.pos.vx;
        det->unk13C.vy = pos.vy + det->translation.cur.pos.vy;
        det->unk13C.vz = pos.vz + det->translation.cur.pos.vz;
        det->unk14C.vx = delta.vx + det->translation.next.delta.vx;
        det->unk14C.vy = delta.vy + det->translation.next.delta.vy;
        det->unk14C.vz = delta.vz + det->translation.next.delta.vz;
        det->unk15C.vx = 0;
        det->unk15C.vy = 0;
        det->unk15C.vz = 0;
        if (det != info->hold) {
            det->unk16C = 0;
            cur->unk16C = 0;
        } else {
            func_8006CB88(obj, det, 0x7FFFFFFF, cur->unk170, &cur->unk16C);
        }
        if (cur->unk16C != 0) {
            delta.vx += det->unk15C.vx;
            delta.vy += det->unk15C.vy;
            delta.vz += det->unk15C.vz;
            CMOVE(obj)->translation.next.delta = delta;
        }
    }
    return 1;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/collision", func_8006D9F4);
#endif

void CollisionGobjSetPushDetector(Gobj *obj, s32 which) {
    CollisionInfo *info;
    CollisionDetector *det;
    s32 best;
    s32 i;

    if (obj != NULL && obj->move != NULL && (info = obj->move->coll) != NULL) {
        if (which < 0) {
            best = 0x7FFFFFFF;
            for (det = info->pushList; det != NULL; det = det->next) {
                if (det->translation.cur.pos.vy - det->unk14 < best) {
                    best = det->translation.cur.pos.vy - det->unk14;
                    info->push = det;
                }
            }
        } else if (which == 0) {
            info->push = NULL;
        } else {
            det = info->pushList;
            i = 1;
            while (i != which && det != NULL) {
                i++;
                det = det->next;
            }
            info->push = det;
        }
    }
}

s32 func_8006DDB4(Gobj *obj) {
    VECTOR32 pos;
    VECTOR32 off;
    VECTOR32 world;
    VECTOR32 rel;
    VECTOR32 d;
    s32 len;
    CollisionInfo *info;
    CollisionDetector *det;

    info = CMOVE(obj)->coll;
    pos = CMOVE(obj)->translation.cur.pos;
    off = CMOVE(obj)->unk160;
    det = info->unk20;
    if (det != NULL) {
        EGSFrameTransformPoint(&det->offset, det->frame, NULL, &world);
        rel.vx = world.vx - pos.vx;
        rel.vy = world.vy - pos.vy;
        rel.vz = world.vz - pos.vz;
        rel.vx += off.vx;
        rel.vy += off.vy;
        rel.vz += off.vz;
        MovementBasicInfoUpdateCurrent(&det->translation);
        MovementBasicInfoUpdate(&det->translation, &rel);
        if (CMOVE(obj)->coll->mode == 3) {
            d.vx = det->translation.next.pos.vx - det->translation.cur.pos.vx;
            d.vy = det->translation.next.pos.vy - det->translation.cur.pos.vy;
            d.vz = det->translation.next.pos.vz - det->translation.cur.pos.vz;
            EGSMath_VectorModulus(&d, &len);
            if (len > 0x10) {
                EGSMathINT_VectorScaleToLength(&det->translation.next.delta, 0x10, &d);
            }
            rel.vx = det->translation.cur.pos.vx + d.vx;
            rel.vy = det->translation.cur.pos.vy + d.vy;
            rel.vz = det->translation.cur.pos.vz + d.vz;
            MovementBasicInfoUpdate(&det->translation, &rel);
        }
    }
    det = info->unk30;
    if (det != NULL) {
        EGSFrameTransformPoint(&det->offset, obj->anim->frame, NULL, &world);
        rel.vx = world.vx - pos.vx;
        rel.vy = world.vy - pos.vy;
        rel.vz = world.vz - pos.vz;
        MovementBasicInfoUpdateCurrent(&det->translation);
        MovementBasicInfoUpdate(&det->translation, &rel);
    }
    for (det = info->pushList; det != NULL; det = det->next) {
        if (det != info->unk20) {
            EGSFrameTransformPoint(&det->offset, det->frame, NULL, &world);
            rel.vx = world.vx - pos.vx;
            rel.vy = world.vy - pos.vy;
            rel.vz = world.vz - pos.vz;
            rel.vx += off.vx;
            rel.vy += off.vy;
            rel.vz += off.vz;
            MovementBasicInfoUpdateCurrent(&det->translation);
            MovementBasicInfoUpdate(&det->translation, &rel);
        }
    }
    return 1;
}

s32 func_8006E0BC(Gobj *obj, u8 attach) {
    VECTOR32 v;
    VECTOR32 unused1; /* unused locals, they only size the frame */
    SVECTOR unused2;
    CollisionInfo *info;
    CollisionDetector *det;
    s8 *hip;
    s32 i;

    info = CMOVE(obj)->coll;
    hip = CASGetHipMotion(obj->id);
    if (hip != NULL) {
        if (attach == 1) {
            if (info->hold == NULL) {
                info->hold = info->holdSaved;
                info->holdSaved = NULL;
            }
            det = info->hold;
            if (det != NULL) {
                v.vx = 0;
                v.vy = det->unk14 - hip[1];
                v.vz = 0;
                EGSFrameTransformVector(&v, obj->anim->frame, NULL, &v);
                MovementBasicInfoUpdateCurrent(&det->translation);
                MovementBasicInfoUpdate(&det->translation, &v);
                info->unk0C = -(hip[0] << 12);
                info->unk10 = 0;
                info->unk14 = -(hip[2] << 12);
                if (JoystickGobjIsInFullRetreat(obj)) {
                    if ((obj->cas->flags & 0x100000) ||
                        ((obj->cas->flags & 4) && JoystickGobjIsWalkKeyPressed(obj))) {
                        info->unk14 = -info->unk14;
                    } else if (obj->cas->flags & 4) {
                        info->unk14 = -(info->unk14 * 2);
                    }
                }
                if (info->unk20 != NULL) {
                    det->unk16C = info->unk20->unk16C;
                    for (i = 0; i < det->unk16C; i++) {
                        det->unk170[i] = info->unk20->unk170[i];
                    }
                } else {
                    det->unk16C = 0;
                }
                info->unk20 = det;
                info->unk1C = 0;
                info->unk1D = 0;
                info->unk1E = 0;
                det->unk18 = CMOVE(obj)->unk11C.vy - CMOVE(obj)->lowest.vy;
            }
        }
    } else if (attach == 0) {
        if (info->holdSaved == NULL) {
            info->holdSaved = info->hold;
            info->hold = NULL;
        }
        det = info->hold;
        if (det != NULL && info->unk20 != NULL) {
            v.vx = 0;
            v.vy = 0;
            v.vz = 0;
            v.vy = info->unk20->translation.next.pos.vy - info->unk20->unk14 + det->unk14;
            MovementBasicInfoUpdateCurrent(&det->translation);
            MovementBasicInfoUpdate(&det->translation, &v);
            det->unk18 = CMOVE(obj)->unk11C.vy - CMOVE(obj)->lowest.vy;
        }
    }
    return 1;
}

s32 CollisionInit(void) {
    FriendlyInit();
    return 1;
}

u8 CollisionGobjGetInfo(Gobj *obj, CollisionInfo **out) {
    if (obj == NULL) {
        return 0;
    }
    if (obj->move != NULL) {
        *out = obj->move->coll;
        return 1;
    }
    return 0;
}

s32 CollisionGobjGetGrabInfo(Gobj *obj, CollisionGrab **out) {
    if (obj == NULL || obj->move == NULL) {
        return 0;
    }
    if (obj->move->coll != NULL) {
        *out = obj->move->coll->grab;
        return 1;
    }
    return 0;
}

s32 CollisionGobjSetGrabInfo(Gobj *obj, CollisionGrab *grab) {
    s16 ok = 0;

    if (obj != NULL && obj->move != NULL) {
        if (obj->move->coll == NULL) {
            return 0;
        }
        obj->move->coll->grab = grab;
        ok = 1;
    }
    return ok;
}

s32 CollisionGobjSetMode(Gobj *obj, s32 *mode) {
    obj->move->coll->mode = *mode;
    return 1;
}

s32 CollisionGobjBasicInit(Gobj *obj, s32 height, s32 a, s32 b, s32 c, s32 d) {
    s16 ok = 0;
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

s32 CollisionGobjInit(Gobj *obj, s32 *mode) {
    MoveInfo *move;

    if (obj == NULL || obj->move == NULL) {
        return 0;
    }
    CollisionGobjBasicInit(obj, 0x80000001, -1, -1, -1, -1);
    move = obj->move;
    move->coll = EGSMemMalloc(sizeof(CollisionInfo));
    if (move->coll == NULL) {
        return 0;
    }
    move->coll->grab = NULL;
    move->coll->unk0C = 0;
    move->coll->unk10 = 0;
    move->coll->unk14 = 0;
    move->coll->unk20 = NULL;
    move->coll->push = NULL;
    move->coll->hold = NULL;
    move->coll->unk30 = NULL;
    move->coll->pushList = NULL;
    move->coll->unk38 = 0;
    move->coll->unk3C = 0;
    CollisionGobjSetMode(obj, mode);
    return 1;
}

s32 CollisionDetectorInitParams(CollisionDetector *det, CollisionDetectorParams *params, Gobj *ref) {
    VECTOR32 pos;
    VECTOR32 refPos;
    VECTOR32 zero = D_800122C4;

    if (det == NULL) {
        return 0;
    }
    det->frame = params->frame;
    det->offset = params->offset;
    det->unk14 = params->unk14;
    det->unk1C = params->unk18;
    det->unk20 = params->unk1C;
    det->unk24 = params->unk20;
    det->unk28 = params->unk24;
    det->unk2C = params->unk28;
    det->unk30 = params->unk2C;
    det->unk34 = params->unk30;
    det->unk38 = params->unk34;
    EGSFrameTransformPoint(&det->offset, det->frame, NULL, &pos);
    if (ref != NULL) {
        MovementGobjGetPosition(ref, &refPos);
        pos.vx -= refPos.vx;
        pos.vy -= refPos.vy;
        pos.vz -= refPos.vz;
    }
    MovementBasicInfoReset(&det->translation, &pos);
    MovementBasicInfoReset(&det->rotation, &zero);
    det->unk13C.vx = 0;
    det->unk13C.vy = 0;
    det->unk13C.vz = 0;
    det->unk14C.vx = 0;
    det->unk14C.vy = 0;
    det->unk14C.vz = 0;
    det->unk15C.vx = 0;
    det->unk15C.vy = 0;
    det->unk15C.vz = 0;
    det->unk16C = 0;
    det->unk170 = params->unk38;
    det->next = NULL;
    return 1;
}

s32 CollisionDetectorCreate(CollisionDetector **out, CollisionDetectorParams *params, Gobj *ref) {
    if (out != NULL) {
        *out = EGSMemMalloc(sizeof(CollisionDetector));
        if (*out != NULL) {
            CollisionDetectorInitParams(*out, params, ref);
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

s32 CollisionDetectorRemoveFromList(CollisionDetector *det, CollisionDetector **list) {
    CollisionDetector *cur;

    if (*list == det) {
        *list = det->next;
    } else {
        cur = *list;
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

s32 CollisionDetectorReorderInList(CollisionDetector *det, s32 pos, CollisionDetector **list) {
    CollisionDetectorRemoveFromList(det, list);
    CollisionDetectorAddToList(det, pos, list);
    return 1;
}

s32 CollisionGobjGetMode(Gobj *obj, s32 *out) {
    if (obj == NULL || obj->move == NULL) {
        return 0;
    }
    if (obj->move->coll != NULL) {
        *out = obj->move->coll->mode;
        return 1;
    }
    return 0;
}

s32 CollisionGobjSetModeGrab(Gobj *obj, CollisionGrab *grab) {
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
            grab->ticks = 0;
            if (obj->move->coll->mode != 1) {
                obj->move->coll->prevMode = obj->move->coll->mode;
            }
            obj->move->coll->mode = 1;
        }
        ok = 1;
    }
    return ok;
}

static s32 func_8006EABC(Gobj *obj) {
    VECTOR32 pos;
    VECTOR32 d;
    VECTOR32 fpos;
    VECTOR32 off;
    s32 len;
    EGSFrame *frame;
    CollisionGrab *grab;

    if (obj == NULL || CMOVE(obj) == NULL) {
        return 0;
    }
    if (CMOVE(obj)->coll == NULL) {
        return 0;
    }
    func_8006DDB4(obj);
    func_8006E0BC(obj, 1);
    func_8006E0BC(obj, 0);
    CMOVE(obj)->coll->hold->unk13C.vx = CMOVE(obj)->translation.cur.pos.vx + CMOVE(obj)->coll->hold->translation.cur.pos.vx;
    CMOVE(obj)->coll->hold->unk13C.vy = CMOVE(obj)->translation.cur.pos.vy + CMOVE(obj)->coll->hold->translation.cur.pos.vy;
    CMOVE(obj)->coll->hold->unk13C.vz = CMOVE(obj)->translation.cur.pos.vz + CMOVE(obj)->coll->hold->translation.cur.pos.vz;
    grab = CMOVE(obj)->coll->grab;
    if (grab == NULL) {
        return 0;
    }
    {
        if (grab->duration >= 0 && grab->ticks >= grab->duration) {
            CollisionGobjSetModeGrab(obj, NULL);
            return 1;
        }
        MovementBasicInfoReset(&CMOVE(obj)->translation, NULL);
        MovementBasicInfoReset(&CMOVE(obj)->rotation, NULL);
        pos = CMOVE(obj)->translation.cur.pos;
        if (grab->bone == -1) {
            frame = obj->anim->frame;
        } else {
            frame = obj->anim->frames[grab->bone];
        }
        fpos.vx = frame->pos.vx;
        fpos.vy = frame->pos.vy;
        fpos.vz = frame->pos.vz;
        fpos.vy = -fpos.vy;
        if (grab->ticks == 0) {
            if (!grab->keepTarget) {
                grab->target = fpos;
            } else {
                grab->keepTarget = 0;
            }
        }
        if (grab->offset.vx != 0 || grab->offset.vy != 0 || grab->offset.vz != 0) {
            EGSFrameTransformVector(&grab->offset, frame, NULL, &off);
            grab->target.vx += off.vx;
            grab->target.vy += off.vy;
            grab->target.vz += off.vz;
        }
        d.vx = grab->target.vx - fpos.vx;
        d.vy = grab->target.vy - fpos.vy;
        d.vz = grab->target.vz - fpos.vz;
        d.vx <<= 12;
        d.vy <<= 12;
        d.vz <<= 12;
        if (obj->cas->active == 8) {
            EGSMath_VectorModulus(&d, &len);
            if (len > 0xB320) {
                d.vx = HALVE(d.vx);
                d.vy = HALVE(d.vy);
                d.vz = HALVE(d.vz);
            }
        } else {
            EGSMathFIX_VectorClipToLength(&d, 0xB320, &d);
        }
        CMOVE(obj)->translation.next.delta.vx += d.vx;
        CMOVE(obj)->translation.next.delta.vy += d.vy;
        CMOVE(obj)->translation.next.delta.vz += d.vz;
        grab->ticks++;
        return 1;
    }
}

s32 CollisionGobjSetModeEdgeGrab(Gobj *obj, CollEdgeGrab *eg) {
    VECTOR32 n;
    VECTOR32 p;
    VECTOR32 fp;
    EGSFrame local;
    VECTOR32 d;
    VECTOR32 eq[3];
    s32 res[4];
    VECTOR32 along;
    VECTOR32 target;
    CollEdge *edge;
    EGSFrame *frame;
    s32 zero;

    if (eg == NULL || obj == NULL || CMOVE(obj) == NULL) {
        return 0;
    }
    edge = (CollEdge *)((u8 *)CMOVE(obj)->charMove + 0x7C);
    if (eg->face != 0) {
        if (eg->face == 1) {
            n.vx = -edge->normal.vx;
            n.vy = -edge->normal.vy;
            n.vz = -edge->normal.vz;
        } else {
            n = edge->normal;
        }
    }
    if (eg->isEdge == 1) {
        frame = obj->anim->frames[eg->grab.bone];
        zero = eg->grab.offset.vx == 0 && eg->grab.offset.vy == 0 && eg->grab.offset.vz == 0;
        if (zero == 1) {
            p.vx = frame->pos.vx;
            p.vy = frame->pos.vy;
            p.vz = frame->pos.vz;
            p.vy = -p.vy;
        } else {
            EGSFrameTransformPoint(&eg->grab.offset, frame, NULL, &p);
        }
        if (eg->face != 0) {
            local.pos.pad = 0;
            fp.vx = obj->anim->frame->pos.vx;
            fp.vy = obj->anim->frame->pos.vy;
            fp.vz = obj->anim->frame->pos.vz;
            fp.vy = -fp.vy;
            EGSFrameSetPosition(&local, NULL, &fp);
            EGSFrameSetOrientation(&local, 0, &n, 0);
            EGSFrameTransformPoint(&p, NULL, obj->anim->frame, &p);
            EGSFrameTransformPoint(&p, &local, NULL, &p);
        }
        d.vx = edge->pt.vx - p.vx;
        d.vy = edge->pt.vy - p.vy;
        d.vz = edge->pt.vz - p.vz;
        eq[0] = edge->normal;
        eq[1] = edge->dir;
        eq[2] = d;
        eq[0].vx = -eq[0].vx;
        eq[0].vy = -eq[0].vy;
        eq[0].vz = -eq[0].vz;
        eq[1].vx = -eq[1].vx;
        eq[1].vy = -eq[1].vy;
        eq[1].vz = -eq[1].vz;
        eq[0].vy = 0;
        eq[1].vy = 0;
        eq[2].vy = 0;
        if (EGSMathINT_SolveLinear2VectorEquationForParameters(eq, res) != 0) {
            return 0;
        }
        {
            s32 t;

            (void)&t;
            t = EGSMathFIXDiv(res[3], res[2]);
            along.vx = EGSMathFIXMul(edge->dir.vx, t);
            /* the const read lets the previous store sink past this load, as in the original */
            along.vy = EGSMathFIXMul(((const VECTOR32 *)&edge->dir)->vy, t);
            along.vz = EGSMathFIXMul(edge->dir.vz, t);
            target.vx = edge->pt.vx + along.vx;
            target.vy = edge->pt.vy + along.vy;
            target.vz = edge->pt.vz + along.vz;
            target.vy += eg->up;
            if (eg->side != 0 || eg->out != 0) {
                VECTOR32 dirn;
                VECTOR32 tmp;

                dirn = edge->dir;
                dirn.vy = 0;
                GTEMath_VectorNormal((VECTOR *)&dirn, (VECTOR *)&dirn);
                tmp.vx = EGSMathFIXMul(dirn.vx, eg->side);
                /* the const read lets the previous store sink past this load, as in the original */
                tmp.vy = EGSMathFIXMul(dirn.vy, ((const CollEdgeGrab *)eg)->side);
                tmp.vz = EGSMathFIXMul(dirn.vz, eg->side);
                target.vx += tmp.vx;
                target.vy += tmp.vy;
                target.vz += tmp.vz;
                if (eg->out != 0) {
                    VECTOR32 perp;
                    VECTOR32 tmp2;

                    perp.vx = -dirn.vz;
                    perp.vy = 0;
                    perp.vz = dirn.vx;
                    tmp2.vx = EGSMathFIXMul(perp.vx, eg->out);
                    /* the const read lets the previous store sink past this load, as in the original */
                    tmp2.vy = EGSMathFIXMul(perp.vy, ((const CollEdgeGrab *)eg)->out);
                    tmp2.vz = EGSMathFIXMul(perp.vz, eg->out);
                    target.vx += tmp2.vx;
                    target.vy += tmp2.vy;
                    target.vz += tmp2.vz;
                }
            }
        }
        eg->grab.target = target;
        eg->grab.keepTarget = 1;
    }
    if (eg->face != 0 && obj != NULL && CMOVE(obj) != NULL && CMOVE(obj)->charMove != NULL) {
        *(VECTOR32 *)((u8 *)CMOVE(obj)->charMove + 0x2C) = n;
    }
    CollisionGobjSetModeGrab(obj, &eg->grab);
    return 1;
}

void CollisionGobjReleaseModeGrabCAS(s16 id) {
    CollisionGobjSetModeGrab(gameObjectsInfo[id].runtime, NULL);
}

static s32 func_8006F49C(Gobj *obj) {
    func_8006D9F4(obj);
    return 1;
}

s32 func_8006F4BC(Gobj *obj) {
    VECTOR32 vel;
    VECTOR32 low;
    u32 flags;
    u8 b9;
    u8 b19;
    u8 b21;
    s32 groundY;
    s32 drop;

    flags = CMOVE(obj)->collFlags;
    CMOVE(obj)->collFlags &= ~0x200;
    CMOVE(obj)->collFlags &= ~0x400;
    CMOVE(obj)->collFlags &= ~0x800;
    CMOVE(obj)->collFlags &= ~0x200000;
    b9 = (flags >> 9) & 1;
    b19 = (flags >> 19) & 1;
    vel = CMOVE(obj)->translation.cur.vel;
    b21 = (flags >> 21) & 1;
    groundY = ((CollCharMove *)CMOVE(obj)->charMove)->groundY;
    low = CMOVE(obj)->lowest;
    if (GOBJ_GRABBING(obj) || groundY == 0x80000001) {
        drop = 0;
    } else {
        drop = groundY - low.vy;
    }
    if (b9 || (drop > 0x288 && b21 && !b19)) {
        if (CMOVE(obj)->collFlags & 0x100000) {
            CMOVE(obj)->collFlags |= 0x400;
        } else {
            CMOVE(obj)->collFlags |= 0x200000;
            if (drop > 0x510) {
                CMOVE(obj)->collFlags |= 0x200;
            }
        }
    } else if (b21 || vel.vy < -0x1D709) {
        if ((CMOVE(obj)->collFlags & 0x100000) && drop > 0x120) {
            CMOVE(obj)->collFlags |= 0x800;
        } else {
            CMOVE(obj)->collFlags |= 0x200000;
        }
    }
    if ((CMOVE(obj)->collFlags & 0x100000) || GOBJ_GRABBING(obj) || groundY == 0x80000001) {
        ((CollCharMove *)CMOVE(obj)->charMove)->groundY = low.vy;
    }
    return 1;
}

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

void func_8006F810(Gobj *obj) {
    VECTOR32 v;
    VECTOR32 w;
    s32 f;

    if (obj->npcState == 1 || obj->npcState == 2) {
        v = CMOVE(obj)->rotation.cur.vel;
        v.vx = 0;
        v.vz = 0;
        v.vy = -v.vy;
        CMOVE(obj)->rotation.next.accel.vx += v.vx;
        CMOVE(obj)->rotation.next.accel.vy += v.vy;
        CMOVE(obj)->rotation.next.accel.vz += v.vz;
    } else if (CMOVE(obj)->collFlags & 4) {
        v.vx = CMOVE(obj)->rotation.next.vel.vx + CMOVE(obj)->rotation.next.accel.vx;
        v.vy = CMOVE(obj)->rotation.next.vel.vy + CMOVE(obj)->rotation.next.accel.vy;
        v.vz = CMOVE(obj)->rotation.next.vel.vz + CMOVE(obj)->rotation.next.accel.vz;
        w = v;
        f = CMOVE(obj)->unk13C;
        if (f != 0x1000) {
            if (f != 0) {
                w.vx = EGSMathFIXMul(w.vx, f);
                w.vy = EGSMathFIXMul(w.vy, f);
                w.vz = EGSMathFIXMul(w.vz, f);
            } else {
                w.vx = 0;
                w.vy = 0;
                w.vz = 0;
            }
        }
        f = CMOVE(obj)->unk138;
        if (f != 0) {
            if (f != 0x1000) {
                f += 0x1000;
                w.vx = EGSMathFIXMul(w.vx, f);
                w.vy = EGSMathFIXMul(w.vy, f);
                w.vz = EGSMathFIXMul(w.vz, f);
            } else {
                w.vx *= 2;
                w.vy *= 2;
                w.vz *= 2;
            }
        }
        w.vx = -w.vx;
        w.vy = -w.vy;
        w.vz = -w.vz;
        CMOVE(obj)->rotation.next.accel.vx += w.vx;
        CMOVE(obj)->rotation.next.accel.vy += w.vy;
        CMOVE(obj)->rotation.next.accel.vz += w.vz;
    }
}

INCLUDE_ASM("asm/main/nonmatchings/collision", CollisionGobjBasicUpdate_Human);

void CollisionGobjBasicUpdate_NonHuman(Gobj *obj) {
    VECTOR32 push;
    VECTOR32 base;
    VECTOR32 v;
    VECTOR32 vel;
    VECTOR32 acc;
    s32 t;
    s32 k;
    CollMoveInfo *move;

    (void)&t;
    move = CMOVE(obj);
    push = D_800122C4;
    CMOVE(obj)->collFlags &= ~4;
    CMOVE(obj)->collFlags &= ~0x100000;
    if (move->collides == 3) {
        base = move->lowest;
    } else {
        base = move->translation.cur.pos;
    }
    push.vy = move->height - base.vy - FIX_TO_INT(move->translation.next.delta.vy);
    if (IABS(push.vy) <= 0x7FFFE) {
        push.vy <<= 12;
        if (push.vy >= -0x4CC0 && move->translation.next.delta.vy <= 0x54C0) {
            v.vx = CMOVE(obj)->translation.next.vel.vx + CMOVE(obj)->translation.next.accel.vx;
            v.vy = CMOVE(obj)->translation.next.vel.vy + CMOVE(obj)->translation.next.accel.vy;
            v.vz = CMOVE(obj)->translation.next.vel.vz + CMOVE(obj)->translation.next.accel.vz;
            if (v.vy <= 0x54C0) {
                CMOVE(obj)->collFlags |= 4;
                CMOVE(obj)->collFlags |= 0x100000;
            }
            move->translation.next.delta.vx += push.vx;
            move->translation.next.delta.vy += push.vy;
            move->translation.next.delta.vz += push.vz;
            if (CMOVE(obj)->collFlags & 4) {
                v = D_800122C4;
                vel = move->translation.next.vel;
                acc = move->translation.next.accel;
                vel.vx += acc.vx;
                vel.vy += acc.vy;
                vel.vz += acc.vz;
                t = move->unk130;
                k = move->unk134;
                v.vy = 0;
                if (vel.vy < 0) {
                    v.vy = -vel.vy;
                }
                if (t != 0 && vel.vy < 0) {
                    t = EGSMathFIXMul(vel.vy, -t);
                    v.vy += t;
                }
                if (k != 0 && vel.vy < 0) {
                    vel.vy = 0;
                    acc.vx = EGSMathFIXMul(vel.vx, -k);
                    acc.vy = EGSMathFIXMul(vel.vy, -k);
                    acc.vz = EGSMathFIXMul(vel.vz, -k);
                    v.vx += acc.vx;
                    v.vy += acc.vy;
                    v.vz += acc.vz;
                }
                move->translation.next.accel.vx += v.vx;
                move->translation.next.accel.vy += v.vy;
                move->translation.next.accel.vz += v.vz;
            }
        }
    }
    MovementGobjUpdatePosition(obj, 1, 0);
    MovementGobjUpdateVelocity(obj, 1, 0);
    MovementGobjUpdateMovement(obj, 1, 0);
    func_8006F810(obj);
}

s32 CollisionGobjUpdate(Gobj *obj) {
    CollisionInfo *info;
    VECTOR32 *pm;

    if (obj == NULL || CMOVE(obj) == NULL || CMOVE(obj)->coll == NULL) {
        return 0;
    }
    CMOVE(obj)->collFlags &= ~0x100000;
    CMOVE(obj)->collFlags &= ~4;
    CMOVE(obj)->collFlags &= ~2;
    switch (CMOVE(obj)->coll->mode) {
        case 1:
            func_8006EABC(obj);
            break;
        case 3:
            func_8006F49C(obj);
            break;
        default:
            return 0;
    }
    func_8006F4BC(obj);
    info = CMOVE(obj)->coll;
    info->unk0C = 0;
    info->unk10 = 0;
    info->unk14 = 0;
    info->unk1C = 1;
    info->unk1D = 1;
    info->unk1E = 1;
    pm = &CMOVE(obj)->pendingMove;
    if (pm->vx != 0 || pm->vy != 0 || pm->vz != 0) {
        pm->vx <<= 12;
        pm->vy <<= 12;
        pm->vz <<= 12;
        CMOVE(obj)->translation.next.delta.vx += pm->vx;
        CMOVE(obj)->translation.next.delta.vy += pm->vy;
        CMOVE(obj)->translation.next.delta.vz += pm->vz;
        pm->vx = 0;
        pm->vy = 0;
        pm->vz = 0;
    }
    MovementGobjUpdatePosition(obj, 1, 0);
    func_8006F810(obj);
    return 1;
}

INCLUDE_ASM("asm/main/nonmatchings/collision", func_8007112C);

/* Also called from cobj.c (CollisionPhysicsGobj_DetectorMotion). */
s32 func_80071E04(Gobj *a, Gobj *b, VECTOR32 *rel, VECTOR32 *out) {
    VECTOR32 va;
    VECTOR32 vb;
    VECTOR32 aa;
    VECTOR32 ab;
    VECTOR32 pa;
    VECTOR32 pb;
    VECTOR32 sum;
    VECTOR32 vc;
    VECTOR32 dv;
    s32 ratio;
    s32 ma;
    s32 mb;

    (void)&ratio;
    va = CMOVE(a)->translation.next.vel;
    aa = CMOVE(a)->translation.next.accel;
    ma = CMOVE(a)->mass;
    if (ma == 0x1000) {
        pa = va;
    } else {
        pa.vx = EGSMathFIXMul(va.vx, ma);
        pa.vy = EGSMathFIXMul(va.vy, ma);
        pa.vz = EGSMathFIXMul(va.vz, ma);
    }
    if (b != NULL && CMOVE(b) != NULL) {
        vb = CMOVE(b)->translation.next.vel;
        ab = CMOVE(b)->translation.next.accel;
        rel->vx = va.vx - vb.vx;
        rel->vy = va.vy - vb.vy;
        rel->vz = va.vz - vb.vz;
        mb = CMOVE(b)->mass;
        if (mb > 0) {
            if (mb == 0x1000) {
                pb = vb;
            } else {
                pb.vx = EGSMathFIXMul(vb.vx, mb);
                pb.vy = EGSMathFIXMul(vb.vy, mb);
                pb.vz = EGSMathFIXMul(vb.vz, mb);
            }
            sum.vx = pa.vx + pb.vx;
            sum.vy = pa.vy + pb.vy;
            sum.vz = pa.vz + pb.vz;
            ratio = EGSMathFIXDiv(ma, ma + mb);
            vc.vx = EGSMathFIXMul(sum.vx, ratio);
            vc.vy = EGSMathFIXMul(sum.vy, ratio);
            vc.vz = EGSMathFIXMul(sum.vz, ratio);
        } else if (ma == 0x1000) {
            vc = vb;
        } else {
            vc.vx = EGSMathFIXMul(vb.vx, ma);
            vc.vy = EGSMathFIXMul(vb.vy, ma);
            vc.vz = EGSMathFIXMul(vb.vz, ma);
        }
    } else {
        ab.vx = 0;
        ab.vy = 0;
        ab.vz = 0;
        *rel = va;
        vc.vx = 0;
        vc.vy = 0;
        vc.vz = 0;
    }
    dv.vx = vc.vx - pa.vx;
    dv.vy = vc.vy - pa.vy;
    dv.vz = vc.vz - pa.vz;
    if (ma == 0x1000) {
        *out = aa;
    } else {
        out->vx = EGSMathFIXMul(aa.vx, ma);
        out->vy = EGSMathFIXMul(aa.vy, ma);
        out->vz = EGSMathFIXMul(aa.vz, ma);
    }
    out->vx -= dv.vx;
    out->vy -= dv.vy;
    out->vz -= dv.vz;
    return 1;
}

s32 func_800721EC(Gobj *obj, CollisionDetector *det, VECTOR32 *vel, VECTOR32 *out) {
    VECTOR32 fwd;
    VECTOR32 right;
    VECTOR32 t;
    VECTOR32 g;
    s8 free[3];
    s32 mass;
    CollisionInfo *info;

    mass = CMOVE(obj)->mass;
    info = CMOVE(obj)->coll;
    vel->vx = 0;
    vel->vy = 0;
    vel->vz = 0;
    out->vx = 0;
    out->vy = 0;
    out->vz = 0;
    if (det == info->unk20) {
        fwd.vx = obj->anim->frame->m[0][2];
        fwd.vy = obj->anim->frame->m[1][2];
        fwd.vz = obj->anim->frame->m[2][2];
        fwd.vy = -fwd.vy;
        right.vx = fwd.vz;
        right.vy = 0;
        right.vz = -fwd.vx;
        if (info->unk0C != 0) {
            t.vx = EGSMathFIXMul(right.vx, info->unk0C);
            /* the const read lets the previous store sink past this load, as in the original */
            t.vy = EGSMathFIXMul(right.vy, ((const CollisionInfo *)info)->unk0C);
            t.vz = EGSMathFIXMul(right.vz, info->unk0C);
            out->vx += t.vx;
            out->vy += t.vy;
            out->vz += t.vz;
        }
        if (info->unk14 != 0) {
            t.vx = EGSMathFIXMul(fwd.vx, info->unk14);
            /* the const read lets the previous store sink past this load, as in the original */
            t.vy = EGSMathFIXMul(fwd.vy, ((const CollisionInfo *)info)->unk14);
            t.vz = EGSMathFIXMul(fwd.vz, info->unk14);
            out->vx += t.vx;
            out->vy += t.vy;
            out->vz += t.vz;
        }
        out->vy += info->unk10;
        *vel = *out;
        if (mass != 0x1000 && mass != 0) {
            vel->vx = EGSMathFIXDiv(vel->vx, mass);
            vel->vy = EGSMathFIXDiv(vel->vy, mass);
            vel->vz = EGSMathFIXDiv(vel->vz, mass);
        }
        free[0] = info->unk1C == 0;
        free[1] = info->unk1D == 0;
        free[2] = info->unk1E == 0;
        if (free[0] || free[1] || free[2]) {
            g = info->unk20->translation.next.vel;
            if (free[0]) {
                if (free[2]) {
                    out->vx += g.vx;
                    out->vz += g.vz;
                } else {
                    s32 d1;

                    (void)&d1;
                    d1 = DOT_FIX(g, right);
                    t.vx = EGSMathFIXMul(right.vx, d1);
                    t.vy = EGSMathFIXMul(right.vy, d1);
                    t.vz = EGSMathFIXMul(right.vz, d1);
                    out->vx += t.vx;
                    out->vy += t.vy;
                    out->vz += t.vz;
                }
            } else {
                s32 d2;

                (void)&d2;
                d2 = DOT_FIX(g, fwd);
                t.vx = EGSMathFIXMul(fwd.vx, d2);
                t.vy = EGSMathFIXMul(fwd.vy, d2);
                t.vz = EGSMathFIXMul(fwd.vz, d2);
                out->vx += t.vx;
                out->vy += t.vy;
                out->vz += t.vz;
            }
            if (free[1]) {
                out->vy += g.vy;
            }
            if (mass != 0x1000 && mass != 0) {
                out->vx = EGSMathFIXMul(out->vx, mass);
                out->vy = EGSMathFIXMul(out->vy, mass);
                out->vz = EGSMathFIXMul(out->vz, mass);
            }
            if (out->vy <= 0) {
                out->vy = 0;
            }
            g = info->unk20->translation.cur.vel;
            if (free[0]) {
                if (free[2]) {
                    vel->vx += g.vx;
                    vel->vz += g.vz;
                } else {
                    s32 d3;

                    (void)&d3;
                    d3 = DOT_FIX(g, right);
                    t.vx = EGSMathFIXMul(right.vx, d3);
                    t.vy = EGSMathFIXMul(right.vy, d3);
                    t.vz = EGSMathFIXMul(right.vz, d3);
                    vel->vx += t.vx;
                    vel->vy += t.vy;
                    vel->vz += t.vz;
                }
            } else {
                s32 d4;

                (void)&d4;
                d4 = DOT_FIX(g, fwd);
                t.vx = EGSMathFIXMul(fwd.vx, d4);
                t.vy = EGSMathFIXMul(fwd.vy, d4);
                t.vz = EGSMathFIXMul(fwd.vz, d4);
                vel->vx += t.vx;
                vel->vy += t.vy;
                vel->vz += t.vz;
            }
            if (free[1]) {
                vel->vy += g.vy;
            }
            if (vel->vy <= 0) {
                vel->vy = 0;
            }
        }
    }
    if (JoystickGobjIsInPureStrafe(obj)) {
        VECTOR32 axis;
        VECTOR32 proj;
        MATRIX cam;
        s32 d5;
        s32 d6;

        (void)&d5;
        (void)&d6;
        cam.m[0][0] = CAM_FRAME->m[0][0];
        cam.m[0][1] = -CAM_FRAME->m[0][1];
        cam.m[0][2] = CAM_FRAME->m[0][2];
        cam.m[1][0] = -CAM_FRAME->m[1][0];
        cam.m[1][1] = CAM_FRAME->m[1][1];
        cam.m[1][2] = -CAM_FRAME->m[1][2];
        cam.m[2][0] = CAM_FRAME->m[2][0];
        cam.m[2][1] = -CAM_FRAME->m[2][1];
        cam.m[2][2] = CAM_FRAME->m[2][2];
        proj.vx = cam.m[0][0];
        proj.vy = cam.m[1][0];
        proj.vz = cam.m[2][0];
        axis.vx = -proj.vz;
        axis.vy = 0;
        axis.vz = proj.vx;
        d5 = DOT_FIX(*out, axis);
        proj.vx = EGSMathFIXMul(axis.vx, d5);
        proj.vy = EGSMathFIXMul(axis.vy, d5);
        proj.vz = EGSMathFIXMul(axis.vz, d5);
        out->vx -= proj.vx;
        out->vy -= proj.vy;
        out->vz -= proj.vz;
        d6 = DOT_FIX(*vel, axis);
        proj.vx = EGSMathFIXMul(axis.vx, d6);
        proj.vy = EGSMathFIXMul(axis.vy, d6);
        proj.vz = EGSMathFIXMul(axis.vz, d6);
        vel->vx -= proj.vx;
        vel->vy -= proj.vy;
        vel->vz -= proj.vz;
    }
    return 1;
}

/* Also called from cobj.c (CollisionPhysicsGobj_DetectorMotion). */
void func_80072AE0(VECTOR32 *v, CollHit **hit, s32 mode, VECTOR32 *axis, VECTOR32 *out, s32 *len) {
    VECTOR32 n;
    s32 dot;
    s32 dot2;

    (void)&dot;
    (void)&dot2;
    if (mode == 1) {
        dot = EGSMathFIXMul(v->vx, (*hit)->normal.vx) + EGSMathFIXMul(v->vy, (*hit)->normal.vy) +
              EGSMathFIXMul(v->vz, (*hit)->normal.vz);
        n.vx = EGSMathFIXMul((*hit)->normal.vx, dot);
        n.vy = EGSMathFIXMul((*hit)->normal.vy, dot);
        n.vz = EGSMathFIXMul((*hit)->normal.vz, dot);
        out->vx = v->vx - n.vx;
        out->vy = v->vy - n.vy;
        out->vz = v->vz - n.vz;
        if (len != NULL) {
            EGSMath_VectorModulus(out, len);
        }
    } else if (mode == 2) {
        dot2 = EGSMathFIXMul(v->vx, axis->vx) + EGSMathFIXMul(v->vy, axis->vy) + EGSMathFIXMul(v->vz, axis->vz);
        out->vx = EGSMathFIXMul(axis->vx, dot2);
        out->vy = EGSMathFIXMul(axis->vy, dot2);
        out->vz = EGSMathFIXMul(axis->vz, dot2);
        if (len != NULL) {
            *len = IABS(dot2);
        }
    } else if (mode == 3) {
        out->vx = 0;
        out->vy = 0;
        out->vz = 0;
        if (len != NULL) {
            *len = 0;
        }
    }
}



INCLUDE_RODATA("asm/main/nonmatchings/collision", D_800122C4);
