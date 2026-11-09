#include "common.h"
#include "scratch.h"

typedef struct AnimVec {
    s32 vx, vy, vz, pad;
} AnimVec;

typedef struct AnimMatrix {
    /* 0x00 */ s16 m[3][3];
    /* 0x12 */ s16 pad;
    /* 0x14 */ s32 t[3];
} AnimMatrix; /* size 0x20 */

typedef struct AnimMaster {
    /* 0x00 */ u8 unk00[0x28];
    /* 0x28 */ u32 flags;
} AnimMaster;

/* Hierarchy frame: local matrix plus a pointer to the world matrix */
typedef struct AnimFrame {
    /* 0x00 */ AnimMatrix local;
    /* 0x20 */ AnimMatrix *world;
} AnimFrame;

typedef struct AnimNode {
    /* 0x00 */ u8 unk00[8];
    /* 0x08 */ u8 flags;
    /* 0x09 */ u8 unk09[3];
    /* 0x0C */ AnimMatrix *frame;
    /* 0x10 */ AnimMaster *master;
    /* 0x14 */ s32 unk14;
    /* 0x18 */ AnimFrame **bones;
    /* 0x1C */ u8 unk1C[8];
    /* 0x24 */ s16 (*restRot)[4]; /* per-bone rest rotation */
} AnimNode;

/* Character state as seen by the animation code */
typedef struct AnimCharState {
    /* 0x00 */ u32 flags;
    /* 0x04 */ u8 unk04[4];
    /* 0x08 */ u8 action;
    /* 0x09 */ u8 weaponPose;
} AnimCharState;

/* Game object as seen by the animation code */
typedef struct AnimGobj {
    /* 0x00 */ u8 unk00[2];
    /* 0x02 */ s16 objId; /* gameObjectsInfo index */
    /* 0x04 */ u8 unk04[4];
    /* 0x08 */ AnimNode *anim;
    /* 0x0C */ struct AnimRuntime *runtime;
    /* 0x10 */ AnimCharState *state;
    /* 0x14 */ s16 *id; /* gameObjectsInfo index */
    /* 0x18 */ u8 unk18[0xA];
    /* 0x22 */ u8 unk22;
} AnimGobj;

/* Per-object animation request slot */
typedef struct AnimSlot {
    /* 0x00 */ u8 unk00;
    /* 0x01 */ u8 active;
    /* 0x04 */ AnimGobj *gobj;
    /* 0x08 */ u8 mode;
    /* 0x0C */ s32 aimRequest;
    /* 0x10 */ u8 aimArg;
    /* 0x14 */ s32 unk14;
    /* 0x18 */ AnimVec aimPoint;
    /* 0x28 */ s32 aimType;
    /* 0x2C */ u8 aimTwoHanded;
    /* 0x30 */ AnimVec aimAngle;
    /* 0x40 */ u8 recoil;
    /* 0x44 */ s32 recoilTime;
    /* 0x48 */ u8 hit;
    /* 0x4C */ s32 hitTime;
    /* 0x50 */ AnimVec hitDir;
} AnimSlot; /* size 0x60 */

#define ANIM_SLOT_MAX 7

typedef struct AnimGameObjectInfo {
    /* 0x00 */ u8 unk00[0x24];
    /* 0x24 */ u32 flags24; /* low byte: object class */
    /* 0x28 */ u8 unk28[0xC];
    /* 0x34 */ void *runtime;
    /* 0x38 */ u8 unk38[0x14];
} AnimGameObjectInfo; /* size 0x4C */

extern AnimSlot D_8011F810[ANIM_SLOT_MAX];
extern AnimGameObjectInfo *gameObjectsInfo;

INCLUDE_RODATA("asm/main/nonmatchings/animate", D_8001231C);

INCLUDE_RODATA("asm/main/nonmatchings/animate", D_8001232C);

extern const AnimVec D_8001231C; /* {1, 1, 1} */
extern const AnimVec D_8001232C; /* {0, 0, 0} */

typedef struct AnimSVec {
    s16 vx, vy, vz, pad;
} AnimSVec;

void RotMatrixYXZ(AnimSVec *r, AnimMatrix *m);
s32 ratan2(s32 y, s32 x);
void EGSGeometryINT_VectorGetAngleFromXZPlane(AnimVec *v, AnimVec *angle);
void EGSFrameSetRotationMatrix(AnimFrame *frame, s32 arg1, AnimMatrix *m);

void LOSGetGobjLOSTestPoint(void *runtime, AnimVec *out);
s32 EGSMathFIXMul(s32 a, s32 b);
void GTEMath_VectorNormal(AnimVec *in, AnimVec *out);
void EGSFrameSetRotationMatrixParent(AnimFrame *frame, AnimMatrix *m);
void EGSFrameSetPositionParent(AnimFrame *frame, AnimVec *pos);
void EGSFrameSetRotationParentYXZ(AnimFrame *frame, AnimVec *rot);
void EGSFrameSetMatrix(AnimFrame *frame, s32 arg1, AnimMatrix *m);
void EGSFrameUpdateHierarchy(AnimMatrix *frame);
int rand(void);
void func_80079950(struct AnimSlot *slot);
void func_8007E650(struct AnimSlot *slot);
u8 func_8007D120(struct AnimSlot *slot);

/* Per-character aim tracking state (gobj->runtime->aim) */
typedef struct AnimAimState {
    /* 0x00 */ u8 unk00[0xB4];
    /* 0xB4 */ s32 tick[3];   /* gSystemTick of the last spine / head / arm update */
    /* 0xC0 */ AnimVec ang[4]; /* smoothed spine, head, upper-arm and forearm angles */
} AnimAimState;

typedef struct AnimRuntime {
    /* 0x000 */ u8 unk000[0x1A8];
    /* 0x1A8 */ AnimAimState *aim;
} AnimRuntime;

enum {
    AIM_SPINE,
    AIM_HEAD,
    AIM_UPPER_ARM,
    AIM_FOREARM,
};

extern s32 gSystemTick;

void EGSFrameUpdateNode(AnimFrame *frame);
void MulMatrix0(AnimMatrix *a, AnimMatrix *b, AnimMatrix *out);
void EGSGeometry_VectorReverseRotateYXZ(AnimVec *v, AnimVec *angle, AnimVec *out);
void EGSGeometryINT_GetXZDistancePoint2Point(AnimVec *a, AnimVec *b, s32 *out);
void EGSGeometry_GetFuzzyDistancePoint2Point(AnimVec *a, AnimVec *b, s32 *out);
void EGSGeometry_VectorToRotationMatrix(AnimVec *v, s32 arg1, AnimMatrix *m);
void EGSFrameSetRotationParentYXZ(AnimFrame *frame, AnimVec *rot);

#define WRAP_ANGLE(a)          \
    if ((a) > 0x800) {         \
        (a) -= 0x1000;         \
    } else if ((a) < -0x800) { \
        (a) += 0x1000;         \
    }

#ifdef NON_MATCHING
/* local matrix converted to the y-up coordinate system */
#define GET_MATRIX_YUP(in, out)                \
    {                                          \
        (out)->m[0][0] = (in)->m[0][0];        \
        (out)->m[0][1] = -(in)->m[0][1];       \
        (out)->m[0][2] = (in)->m[0][2];        \
        (out)->m[1][0] = -(in)->m[1][0];       \
        (out)->m[1][1] = (in)->m[1][1];        \
        (out)->m[1][2] = -(in)->m[1][2];       \
        (out)->m[2][0] = (in)->m[2][0];        \
        (out)->m[2][1] = -(in)->m[2][1];       \
        (out)->m[2][2] = (in)->m[2][2];        \
        (out)->t[0] = (in)->t[0];              \
        (out)->t[1] = -(in)->t[1];             \
        (out)->t[2] = (in)->t[2];              \
    }

/* translation of a matrix in y-up coordinates */
#define GET_POS_YUP(m, v)      \
    {                          \
        (v).vx = (m)->t[0];    \
        (v).vy = (m)->t[1];    \
        (v).vz = (m)->t[2];    \
        (v).vy = -(v).vy;      \
    }

#define GET_COLUMNS(mat, x, y, z) \
    {                             \
        (x).vx = (mat)->m[0][0];  \
        (x).vy = (mat)->m[1][0];  \
        (x).vz = (mat)->m[2][0];  \
        (y).vx = (mat)->m[0][1];  \
        (y).vy = (mat)->m[1][1];  \
        (y).vz = (mat)->m[2][1];  \
        (z).vx = (mat)->m[0][2];  \
        (z).vy = (mat)->m[1][2];  \
        (z).vz = (mat)->m[2][2];  \
    }

#define SET_COLUMNS(mat, x, y, z) \
    {                             \
        (mat)->m[0][0] = (x).vx;  \
        (mat)->m[1][0] = (x).vy;  \
        (mat)->m[2][0] = (x).vz;  \
        (mat)->m[0][1] = (y).vx;  \
        (mat)->m[1][1] = (y).vy;  \
        (mat)->m[2][1] = (y).vz;  \
        (mat)->m[0][2] = (z).vx;  \
        (mat)->m[1][2] = (z).vy;  \
        (mat)->m[2][2] = (z).vz;  \
    }

#define NEGATE_VEC(v)       \
    {                       \
        (v).vx = -(v).vx;   \
        (v).vy = -(v).vy;   \
        (v).vz = -(v).vz;   \
    }

/* d = a - b, each component wrapped to [-0x800, 0x800] */
#define ANGLE_SUB(d, a, b)          \
    {                               \
        (d).vx = (a).vx - (b).vx;   \
        WRAP_ANGLE((d).vx);         \
        (d).vy = (a).vy - (b).vy;   \
        WRAP_ANGLE((d).vy);         \
        (d).vz = (a).vz - (b).vz;   \
        WRAP_ANGLE((d).vz);         \
    }

/* d = a + b, each component wrapped to [-0x800, 0x800] */
#define ANGLE_ADD(d, a, b)          \
    {                               \
        (d).vx = (a).vx + (b).vx;   \
        WRAP_ANGLE((d).vx);         \
        (d).vy = (a).vy + (b).vy;   \
        WRAP_ANGLE((d).vy);         \
        (d).vz = (a).vz + (b).vz;   \
        WRAP_ANGLE((d).vz);         \
    }

#define CLAMP(x, lo, hi)  \
    if ((x) < (lo)) {     \
        (x) = (lo);       \
    } else if ((x) > (hi)) { \
        (x) = (hi);       \
    }

/* arithmetic shift rounding toward zero */
#define SHIFT_DOWN(x, n) ((x) >= 0 ? (x) >> (n) : -(-(x) >> (n)))

#define ANGLE_TO_DEG(a) SHIFT_DOWN((a) * 360, 12)
#define DEG_TO_ANGLE(d) ((d) * 4096 / 360)
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

/* Euler (y-up) angles to a rotation matrix */
static inline void AnimRotMatrix(AnimVec *ang, AnimMatrix *m) {
    AnimSVec r;

    r.vx = -ang->vx;
    r.vy = ang->vy;
    r.vz = -ang->vz;
    RotMatrixYXZ(&r, m);
    m->m[0][1] = -m->m[0][1];
    m->m[1][0] = -m->m[1][0];
    m->m[1][2] = -m->m[1][2];
    m->m[2][1] = -m->m[2][1];
}

/* same, for the smoothed aim angles */
static inline void AnimAimRotMatrix(AnimVec *ang, AnimMatrix *m) {
    AnimSVec r;

    r.vx = -ang->vx;
    r.vy = ang->vy;
    r.vz = -ang->vz;
    RotMatrixYXZ(&r, m);
    m->m[1][0] = -m->m[1][0];
    m->m[0][1] = -m->m[0][1];
    m->m[2][1] = -m->m[2][1];
    m->m[1][2] = -m->m[1][2];
}

/* rotate 180 degrees about y: negate the x and z axes */
static inline void AnimMatrixFlipXZ(AnimMatrix *m) {
    AnimVec x, y, z;

    GET_COLUMNS(m, x, y, z);
    NEGATE_VEC(x);
    NEGATE_VEC(z);
    SET_COLUMNS(m, x, y, z);
}

/* permute the axes into an arm bone's frame */
static inline void AnimMatrixToArm(AnimMatrix *m) {
    AnimVec x, y, z;

    GET_COLUMNS(m, x, y, z);
    NEGATE_VEC(y);
    NEGATE_VEC(z);
    SET_COLUMNS(m, y, z, x);
}

static inline void AnimMatrixFromArm(AnimMatrix *m) {
    AnimVec x, y, z;

    GET_COLUMNS(m, x, y, z);
    NEGATE_VEC(x);
    NEGATE_VEC(y);
    SET_COLUMNS(m, z, x, y);
}

/* permute the axes into the hand bone's frame */
static inline void AnimMatrixToHand(AnimMatrix *m) {
    AnimVec x, y, z;

    GET_COLUMNS(m, x, y, z);
    NEGATE_VEC(x);
    NEGATE_VEC(z);
    SET_COLUMNS(m, y, z, x);
}

static inline void AnimMatrixFromHand(AnimMatrix *m) {
    AnimVec x, y, z;

    GET_COLUMNS(m, x, y, z);
    NEGATE_VEC(y);
    NEGATE_VEC(z);
    SET_COLUMNS(m, z, x, y);
}

/* heading/pitch of the matrix z axis */
static inline void AnimMatrixFacing(AnimMatrix *m, AnimVec *ang) {
    AnimVec z;

    z.vx = m->m[0][2];
    z.vy = m->m[1][2];
    z.vz = m->m[2][2];
    ang->vy = ratan2(z.vx, z.vz);
    EGSGeometryINT_VectorGetAngleFromXZPlane(&z, ang);
    ang->vz = 0;
}

/* full YXZ euler angles of a matrix */
static inline void AnimMatrixAngles(AnimMatrix *m, AnimVec *ang) {
    AnimVec x, z, y;

    x.vx = m->m[0][0];
    x.vy = m->m[1][0];
    x.vz = m->m[2][0];
    y.vx = m->m[0][1];
    y.vy = m->m[1][1];
    y.vz = m->m[2][1];
    z.vx = m->m[0][2];
    z.vy = m->m[1][2];
    z.vz = m->m[2][2];
    ang->vy = ratan2(z.vx, z.vz);
    EGSGeometryINT_VectorGetAngleFromXZPlane(&z, ang);
    ang->vz = 0;
    EGSGeometry_VectorReverseRotateYXZ(&x, ang, &x);
    ang->vz = ratan2(x.vy, x.vx);
}

/* vector from a bone to the aim point */
#define AIM_DELTA(bone, pos, delta, target)   \
    {                                         \
        GET_POS_YUP(&(bone)->local, pos);     \
        (delta).vx = (target)->vx - (pos).vx; \
        (delta).vy = (target)->vy - (pos).vy; \
        (delta).vz = (target)->vz - (pos).vz; \
    }

/* heading/pitch of a vector */
#define VECTOR_ANGLES(v, ang)                                   \
    {                                                           \
        (ang).vy = ratan2((v).vx, (v).vz);                      \
        EGSGeometryINT_VectorGetAngleFromXZPlane(&(v), &(ang)); \
        (ang).vz = 0;                                           \
    }

#define AIM_ANGLES(bone, pos, delta, ang, target) \
    {                                             \
        AIM_DELTA(bone, pos, delta, target);      \
        VECTOR_ANGLES(delta, ang);                \
    }

/*
 * Aim tracking for the simple (non-scratchpad) animation mode: turns the
 * spine, head and arms of a character towards slot->aimPoint, smoothing the
 * angles kept in gobj->runtime->aim, and reacts to recoil and hits.
 *
 * NON_MATCHING: same control flow and instruction count (+1) as the original,
 * 99.8% identical mnemonic sequence. The stack slots of the temporaries differ:
 * in the original most of the vector/matrix temporaries live in inline-function
 * frames that cc1 frees and coalesces (block locals are never freed), so their
 * offsets (and with them the spill slots at 0x308+) depend on the original's
 * exact inline/block structure. Register allocation also differs in places.
 */
void func_80079950(AnimSlot *slot) {
    AnimVec targetAng;
    AnimVec step;
    AnimVec rest;
    AnimMatrix rootM;
    AnimMatrix baseM;
    AnimMatrix boneM;
    AnimMatrix armM;
    AnimVec bonePos;
    AnimVec toTarget;
    AnimVec rootAng;
    AnimVec spineAng;
    AnimVec armAng;
    AnimVec spineOfs;
    AnimVec local;
    AnimMatrix rotM;
    AnimMatrix m;
    AnimMatrix restM;
    AnimVec spineAim;
    s32 aimType;
    u8 twoHanded;
    u8 aiming;
    u8 recoil;
    u8 hit;
    AnimFrame *headBone;
    AnimFrame *foreArm;
    AnimFrame *hand;
    u8 turnBody;
    u8 crouched;
    AnimGobj *gobj;
    AnimVec *target;
    AnimAimState *aim;
    AnimMatrix *root;
    AnimFrame *base;
    AnimFrame *spine;
    AnimFrame *upperArm;
    AnimCharState *state;

    crouched = 0;
    aiming = slot->aimRequest != 0;
    gobj = slot->gobj;
    aimType = slot->aimType;
    twoHanded = slot->aimTwoHanded;
    state = gobj->state;
    target = &slot->aimPoint;
    recoil = slot->recoil;
    hit = slot->hit;
    if (state->action == 1) {
        crouched = (state->flags >> 3) & 1;
    }
    spineAim = D_8001232C;
    if (gobj == NULL || gobj->runtime == NULL || gobj->runtime->aim == NULL) {
        return;
    }
    aim = gobj->runtime->aim;

    EGSFrameUpdateHierarchy(gobj->anim->frame);
    root = gobj->anim->frame;
    base = gobj->anim->bones[14];
    spine = gobj->anim->bones[13];
    headBone = gobj->anim->bones[0];
    upperArm = gobj->anim->bones[11];
    foreArm = gobj->anim->bones[9];
    hand = gobj->anim->bones[8];
    GET_MATRIX_YUP(root, &rootM);
    AnimMatrixFacing(&rootM, &rootAng);
    rest.vx = -gobj->anim->restRot[13][0];
    rest.vy = gobj->anim->restRot[13][1];
    rest.vz = -gobj->anim->restRot[13][2];
    AnimRotMatrix(&rest, &restM);

    /* restart the smoothing of anything not updated last frame */
    if (gSystemTick != aim->tick[0] + 1) {
        aim->ang[AIM_SPINE] = rootAng;
    }
    if (gSystemTick != aim->tick[1] + 1) {
        aim->ang[AIM_HEAD] = rootAng;
    }
    if (gSystemTick != aim->tick[2] + 1) {
        aim->ang[AIM_UPPER_ARM] = rootAng;
        aim->ang[AIM_FOREARM] = rootAng;
    }

    /* always 0 as written; the original tests it in several places */
    turnBody = 0;
    if (twoHanded && aimType != 2) {
        turnBody = aimType == 2;
    }

    GET_MATRIX_YUP(&base->local, &baseM);
    MulMatrix0(&baseM, &restM, &boneM);
    AnimMatrixFlipXZ(&baseM);
    AnimMatrixFlipXZ(&boneM);

    /* spine */
    if (twoHanded || !aiming) {
        AnimVec cur;
        AnimVec want;

        aim->tick[0] = gSystemTick;
        AnimMatrixAngles(&boneM, &spineAng);
        AIM_ANGLES(upperArm, bonePos, toTarget, targetAng, target);
        targetAng.vx = 0;
        ANGLE_SUB(cur, aim->ang[AIM_SPINE], rootAng);
        ANGLE_SUB(want, targetAng, rootAng);
        if (aiming == 1) {
            if (crouched) {
                s32 yaw;

                if (want.vy >= 0) {
                    yaw = want.vy - 0x2E3;
                    if (yaw < 0) {
                        yaw = 0;
                    }
                } else {
                    yaw = want.vy + 0x2E3;
                    if (yaw > 0) {
                        yaw = 0;
                    }
                }
                want.vy = yaw;
            } else if (aimType == aiming) {
                if (want.vy > 0) {
                    want.vy = want.vy / 3;
                } else if (want.vy < 0) {
                    want.vy = want.vy * 120 / 180;
                }
            }
        }
        spineAim = want;
        ANGLE_SUB(spineOfs, spineAng, rootAng);
        cur.vx += spineOfs.vx;
        cur.vy += spineOfs.vy;
        cur.vz += spineOfs.vz;
        want.vx += spineOfs.vx;
        want.vy += spineOfs.vy;
        want.vz += spineOfs.vz;
        if (recoil) {
            if (aimType == 2) {
                cur.vx -= 0x40;
            } else {
                cur.vx -= 0x38;
            }
            cur.vy += 0x63;
        }
        if (!aiming) {
            want = spineOfs;
        }
        if (hit) {
            AnimVec ang;
            AnimSVec r;
            AnimMatrix hitM;
            AnimVec fwd;
            AnimVec side;
            AnimVec dir;
            s32 dot[2];

            ang.vx = aim->ang[AIM_SPINE].vx + spineOfs.vx;
            ang.vy = aim->ang[AIM_SPINE].vy + spineOfs.vy;
            ang.vz = aim->ang[AIM_SPINE].vz + spineOfs.vz;
            r.vx = -ang.vx;
            r.vy = ang.vy;
            r.vz = -ang.vz;
            RotMatrixYXZ(&r, &hitM);
            side.vy = 0;
            hitM.m[0][1] = -hitM.m[0][1];
            hitM.m[1][2] = -hitM.m[1][2];
            hitM.m[1][0] = -hitM.m[1][0];
            hitM.m[2][1] = -hitM.m[2][1];
            fwd.vx = hitM.m[0][2];
            fwd.vy = hitM.m[1][2];
            fwd.vz = hitM.m[2][2];
            side.vx = hitM.m[2][2];
            side.vz = -hitM.m[0][2];
            dir = slot->hitDir;
            dot[0] = dir.vx * fwd.vx + dir.vy * fwd.vy + dir.vz * fwd.vz;
            dot[0] = SHIFT_DOWN(dot[0], 12);
            dot[1] = dir.vx * side.vx + dir.vy * side.vy + dir.vz * side.vz;
            dot[1] = SHIFT_DOWN(dot[1], 12);
            if (dot[0] <= 0) {
                cur.vx -= 0x38;
                if (dot[1] > 0) {
                    cur.vy -= 0x71;
                } else {
                    cur.vy += 0x71;
                }
            } else {
                cur.vx += 0x71;
                if (dot[1] <= 0) {
                    cur.vy -= 0x71;
                } else {
                    cur.vy += 0x71;
                }
            }
        }
        CLAMP(cur.vx, -0x2AA, 0x2AA);
        CLAMP(cur.vy, -0x271, 0x271);
        CLAMP(want.vx, -0x2AA, 0x2AA);
        CLAMP(want.vy, -0x271, 0x271);
        step.vx = want.vx - cur.vx;
        step.vy = want.vy - cur.vy;
        step.vz = want.vz - cur.vz;
        step.vx = SHIFT_DOWN(step.vx, 2);
        step.vy = SHIFT_DOWN(step.vy, 2);
        cur.vx += step.vx;
        cur.vy += step.vy;
        cur.vz += step.vz;
        aim->ang[AIM_SPINE].vx = cur.vx - spineOfs.vx;
        aim->ang[AIM_SPINE].vy = cur.vy - spineOfs.vy;
        aim->ang[AIM_SPINE].vz = cur.vz - spineOfs.vz;
        ANGLE_ADD(aim->ang[AIM_SPINE], aim->ang[AIM_SPINE], rootAng);
        aim->ang[AIM_SPINE].vz = 0;
        local.vz = 0;
        local.vx = cur.vx - spineOfs.vx;
        local.vy = cur.vy - spineOfs.vy;
        if (!turnBody) {
            AnimRotMatrix(&local, &rotM);
            MulMatrix0(&rootM, &rotM, &m);
        } else {
            local.vy += rootAng.vy;
            AnimRotMatrix(&local, &m);
        }
        {
            AnimMatrix headRest;

            rest.vx = -gobj->anim->restRot[14][0];
            rest.vy = gobj->anim->restRot[14][1];
            rest.vz = -gobj->anim->restRot[14][2];
            AnimRotMatrix(&rest, &headRest);
            MulMatrix0(&m, &headRest, &m);
        }
        MulMatrix0(&m, &restM, &m);
        AnimMatrixFlipXZ(&m);
        AnimMatrixFlipXZ(&m);
        EGSFrameSetRotationMatrix(spine, 0, &m);
        AnimMatrixFlipXZ(&m);
    } else {
        EGSFrameSetRotationMatrixParent(spine, &restM);
        aim->ang[AIM_SPINE] = rootAng;
    }

    EGSFrameUpdateNode(spine);
    GET_MATRIX_YUP(&spine->local, &boneM);
    AnimMatrixFlipXZ(&boneM);
    AnimMatrixFacing(&boneM, &spineAng);

    /* head */
    if (aiming) {
        AnimVec cur;
        AnimVec want;
        AnimVec ofs;

        aim->tick[1] = gSystemTick;
        EGSFrameUpdateNode(headBone);
        AIM_ANGLES(headBone, bonePos, toTarget, targetAng, target);
        ANGLE_SUB(cur, aim->ang[AIM_HEAD], spineAng);
        ANGLE_SUB(ofs, spineAng, aim->ang[AIM_SPINE]);
        ANGLE_SUB(want, targetAng, aim->ang[AIM_SPINE]);
        want.vx -= ofs.vx;
        want.vy -= ofs.vy;
        want.vz -= ofs.vz;
        if (turnBody && spineAng.vx > 0) {
            AnimVec pos;
            s32 distTarget;
            s32 distHead;

            GET_POS_YUP(root, pos);
            EGSGeometryINT_GetXZDistancePoint2Point(&pos, target, &distTarget);
            EGSGeometryINT_GetXZDistancePoint2Point(&pos, &bonePos, &distHead);
            if (distTarget < distHead + 0x20) {
                want.vx = 0;
                want.vy = 0;
                want.vz = 0;
                want.vy = rootAng.vy - spineAng.vy;
                WRAP_ANGLE(want.vy);
            }
        }
        if (!slot->aimArg && slot->aimType) {
            cur.vx += 0x38;
        }
        if (recoil) {
            if (aimType != 2) {
                cur.vx += 0x1C;
                cur.vy += 0x31;
            } else {
                cur.vx += 0x40;
                cur.vy += 0x31;
            }
        }
        CLAMP(cur.vx, -0x2AA, 0x2AA);
        CLAMP(cur.vy, -0x31C, 0x31C);
        CLAMP(want.vx, -0x2AA, 0x2AA);
        CLAMP(want.vy, -0x31C, 0x31C);
        step.vx = want.vx - cur.vx;
        step.vy = want.vy - cur.vy;
        step.vz = want.vz - cur.vz;
        step.vx = SHIFT_DOWN(step.vx, 2);
        step.vy = SHIFT_DOWN(step.vy, 2);
        cur.vx += step.vx;
        cur.vy += step.vy;
        cur.vz += step.vz;
        ANGLE_ADD(aim->ang[AIM_HEAD], cur, spineAng);
        aim->ang[AIM_HEAD].vz = 0;
        AnimAimRotMatrix(&aim->ang[AIM_HEAD], &m);
        AnimMatrixFlipXZ(&m);
        EGSFrameSetRotationMatrix(headBone, 0, &m);
        AnimMatrixFlipXZ(&m);
    }

    /* arms */
    if (aiming) {
        if (aimType == 1) {
            AnimVec unused;
            AnimVec cur;
            AnimVec want;
            AnimVec ofs;
            AnimVec elbow;
            AnimVec elbowDir;
            AnimVec elbowAng;
            s32 bend;
            s32 dist;

            /* one-handed: upper arm, forearm and hand */
            AIM_DELTA(upperArm, bonePos, toTarget, target);
            toTarget.vy -= 0xC0;
            VECTOR_ANGLES(toTarget, targetAng);
            ANGLE_SUB(unused, targetAng, rootAng);
            aim->tick[2] = gSystemTick;
            EGSFrameUpdateNode(upperArm);
            AIM_ANGLES(upperArm, bonePos, toTarget, targetAng, target);
            ANGLE_SUB(cur, aim->ang[AIM_UPPER_ARM], spineAng);
            ANGLE_SUB(ofs, spineAng, aim->ang[AIM_SPINE]);
            ANGLE_SUB(want, targetAng, aim->ang[AIM_SPINE]);
            want.vx -= ofs.vx;
            want.vy -= ofs.vy;
            want.vz -= ofs.vz;
            AIM_DELTA(upperArm, bonePos, elbowDir, target);
            elbowDir.vy -= 0x40;
            VECTOR_ANGLES(elbowDir, elbowAng);
            ANGLE_SUB(elbow, elbowAng, aim->ang[AIM_SPINE]);
            {
                s32 v = DEG_TO_ANGLE(MIN(40, MAX(20, 30 - ANGLE_TO_DEG(elbow.vy / 5))));

                if (slot->aimArg) {
                    bend = v;
                } else {
                    bend = 0x71;
                    if (elbow.vx < 0 && crouched) {
                        bend = 0xE3;
                    }
                }
            }
            if (elbow.vx < 0 && elbow.vy < 0) {
                s32 t = -elbow.vy * 80 / 90 - 0x31C;
                s32 lim;

                lim = 0x71;
                if (t < 0x72) {
                    lim = t;
                }
                if (want.vx + bend < lim) {
                    bend = lim - want.vx;
                }
            }
            want.vx += bend;
            want.vy += 0xAA;
            if (recoil) {
                cur.vx += 0xE;
                cur.vy += 0x1A;
            }
            CLAMP(cur.vx, -0x31C, 0x31C);
            CLAMP(cur.vy, -0x238, 0x638);
            CLAMP(want.vx, -0x31C, 0x31C);
            CLAMP(want.vy, -0x238, 0x638);
            step.vx = want.vx - cur.vx;
            step.vy = want.vy - cur.vy;
            step.vz = want.vz - cur.vz;
            step.vx = step.vx * 7 / 16;
            step.vy = step.vy * 7 / 16;
            cur.vx += step.vx;
            cur.vy += step.vy;
            cur.vz += step.vz;
            ANGLE_ADD(aim->ang[AIM_UPPER_ARM], cur, spineAng);
            aim->ang[AIM_UPPER_ARM].vz = -0x155;
            AnimAimRotMatrix(&aim->ang[AIM_UPPER_ARM], &m);
            AnimMatrixToArm(&m);
            EGSFrameSetRotationMatrix(upperArm, 0, &m);
            AnimMatrixFromArm(&m);

            EGSFrameUpdateNode(upperArm);
            GET_MATRIX_YUP(&upperArm->local, &armM);
            AnimMatrixFromArm(&armM);
            AnimMatrixFacing(&armM, &armAng);
            EGSFrameUpdateNode(foreArm);
            AIM_ANGLES(foreArm, bonePos, toTarget, targetAng, target);
            {
                AnimVec cur;
                AnimVec want;
                AnimVec pos;

                ANGLE_SUB(cur, aim->ang[AIM_FOREARM], armAng);
                ANGLE_SUB(want, targetAng, armAng);
                GET_POS_YUP(&upperArm->local, pos);
                EGSGeometry_GetFuzzyDistancePoint2Point(target, &pos, &dist);
                if (dist < 0x61) {
                    want.vx = -bend;
                    want.vy = -0xAA;
                }
                if (recoil) {
                    cur.vx -= 0x55;
                    cur.vy += 0x1A;
                }
                CLAMP(cur.vx, -0x6AA, 0);
                CLAMP(cur.vy, -0x400, 0);
                CLAMP(want.vx, -0x6AA, 0);
                CLAMP(want.vy, -0x400, 0);
                step.vx = want.vx - cur.vx;
                step.vy = want.vy - cur.vy;
                step.vz = want.vz - cur.vz;
                step.vx = step.vx * 7 / 16;
                step.vy = step.vy * 7 / 16;
                cur.vx += step.vx;
                cur.vy += step.vy;
                cur.vz += step.vz;
                ANGLE_ADD(aim->ang[AIM_FOREARM], cur, armAng);
                aim->ang[AIM_FOREARM].vz = -0x2AA;
                AnimAimRotMatrix(&aim->ang[AIM_FOREARM], &m);
                AnimMatrixToArm(&m);
                EGSFrameSetRotationMatrix(foreArm, 0, &m);
                AnimMatrixFromArm(&m);
            }
            EGSFrameUpdateNode(hand);
            {
                AnimVec handAng;

                handAng = aim->ang[AIM_FOREARM];
                handAng.vz = -0x400;
                AnimAimRotMatrix(&handAng, &m);
                AnimMatrixToArm(&m);
                EGSFrameSetRotationMatrix(hand, 0, &m);
                AnimMatrixFromArm(&m);
            }
        } else if (aimType == 2) {
            AnimVec cur;
            AnimVec want;
            AnimFrame *wrist;
            AnimVec wristPos;
            AnimVec handPos;
            AnimVec dir;
            AnimVec axis;
            s32 dist;

            /* two-handed weapon: the hand bone leads, the wrist follows */
            aim->tick[2] = gSystemTick;
            EGSFrameUpdateNode(hand);
            AIM_ANGLES(hand, bonePos, toTarget, targetAng, target);
            ANGLE_SUB(cur, aim->ang[AIM_UPPER_ARM], rootAng);
            ANGLE_SUB(want, targetAng, rootAng);
            if (want.vy < 0 && spineAim.vy > 0x71) {
                want.vy += 0x1000;
            } else if (want.vy > 0 && spineAim.vy < -0x71) {
                want.vy -= 0x1000;
            }
            if (recoil) {
                cur.vx += 0;
                cur.vy += 0;
            }
            CLAMP(cur.vx, -0x31C, 0x31C);
            CLAMP(cur.vy, -0x400, 0x400);
            CLAMP(want.vx, -0x31C, 0x31C);
            CLAMP(want.vy, -0x400, 0x400);
            step.vx = want.vx - cur.vx;
            step.vy = want.vy - cur.vy;
            step.vz = want.vz - cur.vz;
            step.vx = step.vx * 7 / 16;
            step.vy = step.vy * 7 / 16;
            cur.vx += step.vx;
            cur.vy += step.vy;
            cur.vz += step.vz;
            ANGLE_ADD(aim->ang[AIM_UPPER_ARM], cur, rootAng);
            aim->ang[AIM_UPPER_ARM].vz = -0x400;
            AnimAimRotMatrix(&aim->ang[AIM_UPPER_ARM], &m);
            AnimMatrixToArm(&m);
            EGSFrameSetRotationMatrix(hand, 0, &m);
            AnimMatrixFromArm(&m);

            wrist = gobj->anim->bones[3];
            EGSFrameUpdateNode(wrist);
            GET_POS_YUP(&wrist->local, wristPos);
            GET_POS_YUP(&hand->local, handPos);
            axis.vx = m.m[0][2];
            axis.vy = m.m[1][2];
            axis.vz = m.m[2][2];
            axis.vx = EGSMathFIXMul(axis.vx, 0x26);
            axis.vy = EGSMathFIXMul(axis.vy, 0x26);
            axis.vz = EGSMathFIXMul(axis.vz, 0x26);
            handPos.vx += axis.vx;
            handPos.vy += axis.vy;
            handPos.vz += axis.vz;
            EGSGeometry_GetFuzzyDistancePoint2Point(&wristPos, &handPos, &dist);
            if (dist < 3) {
                handPos.vx -= axis.vx;
                handPos.vy -= axis.vy;
                handPos.vz -= axis.vz;
            }
            dir.vx = handPos.vx - wristPos.vx;
            dir.vy = handPos.vy - wristPos.vy;
            dir.vz = handPos.vz - wristPos.vz;
            EGSGeometry_VectorToRotationMatrix(&dir, 0, &m);
            AnimMatrixToHand(&m);
            EGSFrameSetRotationMatrix(wrist, 0, &m);
            AnimMatrixFromHand(&m);
        } else {
            rest.vx = -gobj->anim->restRot[11][0];
            rest.vy = gobj->anim->restRot[11][1];
            rest.vz = -gobj->anim->restRot[11][2];
            EGSFrameSetRotationParentYXZ(upperArm, &rest);
            rest.vx = -gobj->anim->restRot[9][0];
            rest.vy = gobj->anim->restRot[9][1];
            rest.vz = -gobj->anim->restRot[9][2];
            EGSFrameSetRotationParentYXZ(foreArm, &rest);
            rest.vx = -gobj->anim->restRot[8][0];
            rest.vy = gobj->anim->restRot[8][1];
            rest.vz = -gobj->anim->restRot[8][2];
            EGSFrameSetRotationParentYXZ(hand, &rest);
        }
    }
    EGSFrameUpdateHierarchy(gobj->anim->frame);
}
#else
INCLUDE_ASM("asm/main/nonmatchings/animate", func_80079950);
#endif

#define OBJ_CLASS(id) (*(u8 *)&gameObjectsInfo[id].flags24)
#define OBJ_SEX(id) (gameObjectsInfo[id].flags24 & 0x3000)

#ifdef NON_MATCHING
/*
 * Logically equivalent; cse folds the original's redundant third
 * "(sex == 0x1000)" test of the first class switch, so this is one insn short.
 * The fold happens in cse (the test is still present after jump1); splitting the third test
 * into its own if, a user label before it, or a switch on the sex bits do not stop it.
 * r4F: cse records both the `== 0x1000` fall-through (as a known-false comparison) and the
 * `!= 0x2000` fall-through (x == 0x2000) and folds the third test from either; (u16)/(s16)/u32
 * casts, a do-while(0) around the first two tests, a `sex` local with `else sex = sex;` and a
 * volatile local do not keep it.
 * r4J: `if (==0x1000) goto third; if (!=0x2000) goto two; third: if (==0x1000) goto two;`
 * does not help either: cse's follow_jumps path takes the first jump to `third`, folds the test
 * and rethreads the jump straight to two_handed.  (x ^ 0x2000) != 0 / (x - 0x1000) == 0 forms
 * fold too (cse knows x != 0x1000 from the first test's fall-through).
 */
u8 func_8007D120(AnimSlot *slot) {
    AnimCharState *state;
    AnimCharState *pose;

    state = slot->gobj->state;
    if (state->action == 7) {
        return 0;
    }
    if (state->flags & 0x2000) {
        return 0;
    }
    if (state->action == 9 || (state->flags & 0x400)) {
        return 0;
    }
    if (state->action == 8) {
        if (state->flags & 0x200000) {
            slot->aimType = 1;
            slot->aimTwoHanded = 0;
        } else {
            slot->aimType = 0;
            slot->aimTwoHanded = 0;
        }
    } else {
        switch (OBJ_CLASS(slot->gobj->objId)) {
        case 0:
            if (OBJ_SEX(slot->gobj->objId) == 0x1000 || OBJ_SEX(slot->gobj->objId) != 0x2000 ||
                OBJ_SEX(slot->gobj->objId) == 0x1000) {
                goto two_handed;
            }
            break;
        case 0x13:
            goto two_handed;
        }
        switch (OBJ_CLASS(slot->gobj->objId)) {
        case 0:
            if (OBJ_SEX(slot->gobj->objId) != 0x1000 && OBJ_SEX(slot->gobj->objId) == 0x2000) {
                goto two_handed;
            }
            break;
        case 0x14:
        two_handed:
            if (slot->gobj->state->action == 2 && !(slot->gobj->state->flags & 2)) {
                slot->aimType = 0;
                slot->aimTwoHanded = 0;
            } else {
                slot->aimType = 0;
                slot->aimTwoHanded = 1;
            }
            goto done;
        }
        pose = slot->gobj->state;
        if (pose->weaponPose == 2 || (pose->weaponPose == 6 && (pose->flags & 2))) {
            slot->aimType = 1;
            slot->aimTwoHanded = 1;
        } else if (pose->weaponPose == 6) {
            slot->aimType = 1;
            slot->aimTwoHanded = 0;
        } else if (pose->weaponPose == 3 || (pose->weaponPose == 7 && (pose->flags & 2))) {
            slot->aimType = 2;
            slot->aimTwoHanded = 1;
        } else if (pose->weaponPose == 7) {
            slot->aimType = 2;
            slot->aimTwoHanded = 0;
        } else {
            slot->aimType = 0;
            slot->aimTwoHanded = 0;
        }
    }
done:
    if ((slot->gobj->state->flags & 0x1000000) && slot->aimType == 1) {
        slot->aimType = 0;
    }
    return 1;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/animate", func_8007D120);
#endif

/* Calls made on the scratchpad stack (SCRATCH_CALL1, scratch.h): the original leaves
 * the jal delay slot empty.  A second spelling of the same asm keeps jump.c from
 * cross-jumping the two call sites. */
#define SCRATCH_CALL1_B(fn, arg)                                                       \
    __asm__ volatile(SCRATCH_ENTER "move $4,%0\n\tjal " #fn "\n\t" SCRATCH_LEAVE  \
                     :                                                                 \
                     : "r"(arg)                                                        \
                     : SCRATCH_CLOBBERS)

/*
 * `s32 p = paused;` gives the prologue andi (the unk00 stores use the raw param).
 * `s32 one = 1;` (a user variable set after a conditional jump) keeps loop.c from
 * hoisting the constant shared by the two `== 1` compares.  The `if (!p)
 * func_80079950()` call is written out in both places (jump2 cross-jumps them into
 * one): the extra refs lift p above the loop's D_8011F810 base in global-alloc.
 */
void AnimateProcess(u8 paused) {
    s32 i;
    AnimGobj *gobj;
    u8 action;
    s32 p = paused;

    for (i = 0; i < ANIM_SLOT_MAX; i++) {
        gobj = D_8011F810[i].gobj;
        if (!D_8011F810[i].active) {
            continue;
        }
        if (D_8011F810[i].aimRequest) {
            if ((gobj->unk22 == 1 || gobj->unk22 == 2) && (gobj->anim->master->flags & 0x400000) &&
                !(gobj->anim->flags & 0x10) && gobj->runtime) {
                s32 one = 1;
                if (D_8011F810[i].unk14 == one) {
                    LOSGetGobjLOSTestPoint((void *)D_8011F810[i].aimPoint.vx, &D_8011F810[i].aimPoint);
                }
                if (func_8007D120(&D_8011F810[i]) == one) {
                    if (D_8011F810[i].mode != 0) {
                        D_8011F810[i].unk00 = paused;
                        SCRATCH_CALL1(func_8007E650, &D_8011F810[i]);
                    } else if (!p) {
                        func_80079950(&D_8011F810[i]);
                    }
                }
            }
        } else {
            action = gobj->state->action;
            if (action == 5 || action == 6 || action == 9) {
            } else if (D_8011F810[i].mode == 0) {
                if (!p) {
                    func_80079950(&D_8011F810[i]);
                }
            } else {
                D_8011F810[i].unk00 = paused;
                SCRATCH_CALL1_B(func_8007E650, &D_8011F810[i]);
            }
        }
        if (p) {
            continue;
        }
        if (D_8011F810[i].aimRequest > 0) {
            D_8011F810[i].aimRequest--;
        }
        if (D_8011F810[i].recoilTime > 0) {
            D_8011F810[i].recoilTime--;
        }
        if (D_8011F810[i].hitTime > 0) {
            D_8011F810[i].hitTime--;
        }
        D_8011F810[i].recoil = 0;
        D_8011F810[i].hit = 0;
        if (D_8011F810[i].aimRequest == 0 &&
            (D_8011F810[i].mode == 0 || (D_8011F810[i].recoilTime == 0 && D_8011F810[i].hitTime == 0))) {
            D_8011F810[i].active = 0;
        }
    }
}

void AnimateInit(void) {
    s32 i;

    for (i = 0; i < ANIM_SLOT_MAX; i++) {
        D_8011F810[i].active = 0;
    }
}

static s32 func_8007D6CC(AnimGobj *gobj, u8 *mode) {
    s32 i;
    u8 m;

    if (gobj == NULL) {
        return -1;
    }
    for (i = 0; i < ANIM_SLOT_MAX; i++) {
        if (gobj == D_8011F810[i].gobj && D_8011F810[i].active) {
            if (D_8011F810[i].mode == 1) {
                if (*mode == 0) {
                    D_8011F810[i].mode = *mode;
                    D_8011F810[i].recoilTime = 0;
                    D_8011F810[i].hitTime = 0;
                }
                return i;
            }
            *mode = D_8011F810[i].mode;
            return i;
        }
    }
    for (i = 0; i < ANIM_SLOT_MAX; i++) {
        if (!D_8011F810[i].active) {
            break;
        }
    }
    if (i >= ANIM_SLOT_MAX) {
        return -1;
    }
    D_8011F810[i].active = 1;
    D_8011F810[i].gobj = gobj;
    m = *mode;
    D_8011F810[i].aimRequest = 0;
    D_8011F810[i].aimArg = 1;
    D_8011F810[i].recoil = 0;
    D_8011F810[i].recoilTime = 0;
    D_8011F810[i].hit = 0;
    D_8011F810[i].hitTime = 0;
    D_8011F810[i].mode = m;
    return i;
}


void AnimateGobjRequestAim(AnimGobj *gobj, AnimVec *target, u8 arg, u8 mode) {
    AnimVec aim;
    AnimVec pos;
    AnimVec dir;
    u8 m;
    s32 i;

    m = mode;
    i = func_8007D6CC(gobj, &m);
    if (i < 0) {
        return;
    }
    D_8011F810[i].aimRequest = 1;
    D_8011F810[i].aimArg = arg;
    D_8011F810[i].unk14 = 2;
    if (target != NULL) {
        aim = *target;
    } else if (gobj->id != NULL && *gobj->id >= 0) {
        LOSGetGobjLOSTestPoint(gameObjectsInfo[*gobj->id].runtime, &aim);
    } else {
        pos.vx = gobj->anim->frame->t[0];
        pos.vy = gobj->anim->frame->t[1];
        pos.vz = gobj->anim->frame->t[2];
        pos.vy = -pos.vy;
        dir.vx = gobj->anim->frame->m[0][2];
        dir.vy = gobj->anim->frame->m[1][2];
        dir.vz = gobj->anim->frame->m[2][2];
        dir.vy = -dir.vy;
        dir.vx = EGSMathFIXMul(dir.vx, 0x640);
        dir.vy = EGSMathFIXMul(dir.vy, 0x640);
        dir.vz = EGSMathFIXMul(dir.vz, 0x640);
        aim.vx = pos.vx + dir.vx;
        aim.vy = pos.vy + dir.vy;
        aim.vz = pos.vz + dir.vz;
    }
    D_8011F810[i].aimPoint = aim;
}

void AnimateGobjRequestShotRecoil(AnimGobj *gobj, u8 mode) {
    u8 m;
    s32 i;

    m = mode;
    i = func_8007D6CC(gobj, &m);
    if (i < 0) {
        return;
    }
    if (m) {
        D_8011F810[i].recoilTime = 10;
    } else {
        D_8011F810[i].recoilTime = 0;
    }
    D_8011F810[i].recoil = 1;
}

void AnimateGobjRequestHit(AnimGobj *gobj, AnimVec *dir, u8 mode) {
    u8 m;
    s32 i;

    m = mode;
    i = func_8007D6CC(gobj, &m);
    if (i < 0) {
        return;
    }
    if (m) {
        D_8011F810[i].hitTime = 10;
    } else {
        D_8011F810[i].hitTime = 0;
    }
    D_8011F810[i].hit = 1;
    D_8011F810[i].hitDir = *dir;
    GTEMath_VectorNormal(&D_8011F810[i].hitDir, &D_8011F810[i].hitDir);
}

void AnimateGobj(AnimGobj *gobj, u8 active, u8 mode) {
    u8 m;
    s32 i;

    m = mode;
    i = func_8007D6CC(gobj, &m);
    if (i >= 0) {
        D_8011F810[i].active = active;
    }
}

/* World matrix converted to the y-up coordinate system */
#define GET_WORLD_MATRIX_YUP(frame, out)     \
    {                                        \
        *(out) = *(frame)->world;            \
        (out)->m[0][0] = (out)->m[0][0];     \
        (out)->m[0][1] = -(out)->m[0][1];    \
        (out)->m[0][2] = (out)->m[0][2];     \
        (out)->m[1][0] = -(out)->m[1][0];    \
        (out)->m[1][1] = (out)->m[1][1];     \
        (out)->m[1][2] = -(out)->m[1][2];    \
        (out)->m[2][0] = (out)->m[2][0];     \
        (out)->m[2][1] = -(out)->m[2][1];    \
        (out)->m[2][2] = (out)->m[2][2];     \
        (out)->t[0] = (out)->t[0];           \
        (out)->t[1] = -(out)->t[1];          \
        (out)->t[2] = (out)->t[2];           \
    }

static void func_8007DC58(AnimFrame *frame, AnimMatrix *out) {
    GET_WORLD_MATRIX_YUP(frame, out);
}

static void func_8007DD18(AnimFrame *node, AnimMatrix *ref, AnimMatrix *out) {
    AnimMatrix m;
    AnimVec pos;
    AnimVec refPos;
    AnimVec outPos;

    GET_WORLD_MATRIX_YUP(node, &m);
    if (*(s32 *)&m.m[0][0] == *(s32 *)&ref->m[0][0] && *(s32 *)&m.m[0][2] == *(s32 *)&ref->m[0][2] &&
        *(s32 *)&m.m[1][1] == *(s32 *)&ref->m[1][1] && *(s32 *)&m.m[2][0] == *(s32 *)&ref->m[2][0] &&
        m.m[2][2] == ref->m[2][2]) {
        EGSFrameSetRotationMatrixParent(node, out);
    } else {
        out->m[0][0] = m.m[0][0];
        out->m[0][1] = m.m[0][1];
        out->m[0][2] = m.m[0][2];
        out->m[1][0] = m.m[1][0];
        out->m[1][1] = m.m[1][1];
        out->m[1][2] = m.m[1][2];
        out->m[2][0] = m.m[2][0];
        out->m[2][1] = m.m[2][1];
        out->m[2][2] = m.m[2][2];
    }
    pos.vx = m.t[0];
    pos.vy = m.t[1];
    pos.vz = m.t[2];
    refPos.vx = ref->t[0];
    refPos.vy = ref->t[1];
    refPos.vz = ref->t[2];
    outPos.vx = out->t[0];
    outPos.vy = out->t[1];
    outPos.vz = out->t[2];
    if (pos.vx == refPos.vx || pos.vy == refPos.vy || pos.vz == refPos.vz) {
        if (pos.vx == refPos.vx) {
            pos.vx = outPos.vx;
        }
        if (pos.vy == refPos.vy) {
            pos.vy = outPos.vy;
        }
        if (pos.vz == refPos.vz) {
            pos.vz = outPos.vz;
        }
        EGSFrameSetPositionParent(node, &pos);
    }
    out->t[0] = pos.vx;
    out->t[1] = pos.vy;
    out->t[2] = pos.vz;
}

/* world matrices of the head and hands saved around the electrocution jitter */
extern AnimMatrix D_8011F750;
extern AnimMatrix D_8011F770;
extern AnimMatrix D_8011F790;
extern AnimMatrix D_8011F7B0;
extern AnimMatrix D_8011F7D0;
extern AnimMatrix D_8011F7F0;

void AnimateGobjElectrocute(AnimGobj *gobj) {
    AnimVec rot;
    AnimVec jitter;
    AnimMatrix m1;
    AnimMatrix m2;
    s32 i;
    AnimFrame *bone;

    if ((gobj->unk22 == 1 || gobj->unk22 == 2) && (gobj->anim->master->flags & 0x400000) &&
        !(gobj->anim->flags & 0x10) && gobj->runtime) {
        i = 0;
        func_8007DD18(gobj->anim->bones[14], &D_8011F750, &D_8011F7B0);
        func_8007DD18(gobj->anim->bones[6], &D_8011F770, &D_8011F7D0);
        func_8007DD18(gobj->anim->bones[12], &D_8011F790, &D_8011F7F0);
        EGSFrameUpdateHierarchy(gobj->anim->frame);
        m1.m[0][0] = gobj->anim->bones[6]->local.m[0][0];
        m1.m[0][1] = -gobj->anim->bones[6]->local.m[0][1];
        m1.m[0][2] = gobj->anim->bones[6]->local.m[0][2];
        m1.m[1][0] = -gobj->anim->bones[6]->local.m[1][0];
        m1.m[1][1] = gobj->anim->bones[6]->local.m[1][1];
        m1.m[1][2] = -gobj->anim->bones[6]->local.m[1][2];
        m1.m[2][0] = gobj->anim->bones[6]->local.m[2][0];
        m1.m[2][1] = -gobj->anim->bones[6]->local.m[2][1];
        m1.m[2][2] = gobj->anim->bones[6]->local.m[2][2];
        m1.t[0] = gobj->anim->bones[6]->local.t[0];
        m1.t[1] = -gobj->anim->bones[6]->local.t[1];
        m1.t[2] = gobj->anim->bones[6]->local.t[2];
        m2.m[0][0] = gobj->anim->bones[12]->local.m[0][0];
        m2.m[0][1] = -gobj->anim->bones[12]->local.m[0][1];
        m2.m[0][2] = gobj->anim->bones[12]->local.m[0][2];
        m2.m[1][0] = -gobj->anim->bones[12]->local.m[1][0];
        m2.m[1][1] = gobj->anim->bones[12]->local.m[1][1];
        m2.m[1][2] = -gobj->anim->bones[12]->local.m[1][2];
        m2.m[2][0] = gobj->anim->bones[12]->local.m[2][0];
        m2.m[2][1] = -gobj->anim->bones[12]->local.m[2][1];
        m2.m[2][2] = gobj->anim->bones[12]->local.m[2][2];
        m2.t[0] = gobj->anim->bones[12]->local.t[0];
        m2.t[1] = -gobj->anim->bones[12]->local.t[1];
        m2.t[2] = gobj->anim->bones[12]->local.t[2];
        for (; i < 15; i++) {
            bone = gobj->anim->bones[i];
            jitter.vx = (rand() & 0xFF) - 0x80;
            jitter.vy = (rand() & 0xFF) - 0x80;
            jitter.vz = (rand() & 0xFF) - 0x80;
            if (i == 1 || i == 7 || i == 4 || i == 10 || i == 6 || i == 12) {
                continue;
            }
            rot.vx = -gobj->anim->restRot[i][0];
            rot.vy = gobj->anim->restRot[i][1];
            rot.vz = -gobj->anim->restRot[i][2];
            rot.vx += jitter.vx;
            rot.vy += jitter.vy;
            rot.vz += jitter.vz;
            EGSFrameSetRotationParentYXZ(bone, &rot);
        }
        EGSFrameUpdateHierarchy(gobj->anim->frame);
        EGSFrameSetMatrix(gobj->anim->bones[6], 0, &m1);
        EGSFrameSetMatrix(gobj->anim->bones[12], 0, &m2);
        EGSFrameUpdateHierarchy(gobj->anim->frame);
        func_8007DC58(gobj->anim->bones[14], &D_8011F750);
        func_8007DC58(gobj->anim->bones[6], &D_8011F770);
        func_8007DC58(gobj->anim->bones[12], &D_8011F790);
    }
}

#ifdef NON_MATCHING
/*
 * Same length and stack layout; differs in global register allocation
 * (aiming/gobj/aimType land in s3/s1/s2 instead of s1/s2/s3) and a few
 * temporaries.
 * r4F: the s-reg order comes from the else-branch `bone` (refs 14, life ~104, pri 4077) beating
 * slot (pri 3049): bone takes s0, slot shares it.  The original has slot s0, bone s1, gobj s2,
 * aimType s3, aiming s1 -- i.e. slot allocated before bone and the two conflict.  `bone = NULL`
 * before `ang = slot->aimAngle` gives the conflict (gobj/aimType then land in s2/s3, 71 lines)
 * but bone still outranks slot; one function-scope bone for both branches is worse.
 * r4J: using the AnimRotMatrix/AnimMatrixToArm inlines for the else branch scores the same (77;
 * the stack layout still matches) and an inline for the world==NULL stores does not lower bone's
 * refs: integrate.c maps a user-variable pseudo argument straight onto the parameter, only
 * expressions / non-user regs get a fresh pseudo.  bone needs live > ~137 insns (now 103) or
 * fewer refs for slot (3063) to outrank it.
 * r7N (77 -> 28): `bone = NULL;` before `ang = slot->aimAngle;` (else branch; makes bone conflict
 * with slot) + `slot->aimAngle = ang;` inside 6 nested do-while(0) (flow.c weights that slot ref
 * x7: slot 23 refs, pri 4144 > bone 4077) gives slot s0, bone s1, gobj s2, aimType s3, aiming s1.
 * Left: local-alloc temps in the recoil/hit /10 divisions and the bone pointer, the ang sum order,
 * AND the frame: ours is 0xE0 vs 0xD0 (masked by score.py; vs.py shows it). The two `AnimVec rot`
 * blocks land at sp+0x68 instead of 0x48 (bonePos/delta slots of the aiming block are not reused),
 * which also shifts the else-branch m/ang/u; `unused` only papers over part of it.
 */
void func_8007E650(AnimSlot *slot) {
    AnimVec spine;
    AnimVec head;
    AnimVec fwd;
    s32 dot[2];
    u8 aiming;
    AnimGobj *gobj;
    s32 aimType;
    s32 k;
    u8 moving;

    gobj = slot->gobj;
    moving = slot->unk00;
    if (!((gobj->unk22 == 1 || gobj->unk22 == 2) && (gobj->anim->master->flags & 0x400000) &&
          !(gobj->anim->flags & 0x10) && gobj->runtime)) {
        return;
    }
    spine = D_8001232C;
    head = D_8001232C;
    aimType = slot->aimType;
    aiming = 0;
    if (slot->aimRequest) {
        aiming = aimType == 1 || aimType == 2;
    }
    fwd.vx = gobj->anim->frame->m[0][2];
    fwd.vy = gobj->anim->frame->m[1][2];
    fwd.vz = gobj->anim->frame->m[2][2];
    fwd.vy = -fwd.vy;
    if (moving) {
        if (slot->recoilTime) {
            k = -0x4F;
            if (aimType == 1) {
                k = -0x38;
            }
            spine.vx += (-k * slot->recoilTime) / 10;
            if (aimType == 1) {
                k = 0x4F;
                head.vx += (slot->recoilTime * k) / 10;
            }
        }
        if (slot->hitTime) {
            k = -0xAA;
            dot[0] = slot->hitDir.vx * fwd.vx + slot->hitDir.vz * fwd.vz;
            if (dot[0] > 0) {
                k = 0xAA;
            }
            spine.vx += (-k * slot->hitTime) / 10;
        }
        if (aiming) {
            if (slot->unk14 == 2) {
                AnimVec bonePos;
                AnimVec delta;
                AnimVec ang;
                AnimVec cur;
                AnimVec diff;
                AnimFrame *bone;

                cur = D_8001232C;
                bone = NULL;
                if (aimType == 1) {
                    bone = gobj->anim->bones[11];
                } else if (aimType == 2) {
                    bone = gobj->anim->bones[8];
                }
                bonePos.vx = bone->local.t[0];
                bonePos.vy = bone->local.t[1];
                bonePos.vz = bone->local.t[2];
                bonePos.vy = -bonePos.vy;
                delta.vx = slot->aimPoint.vx - bonePos.vx;
                delta.vy = slot->aimPoint.vy - bonePos.vy;
                delta.vz = slot->aimPoint.vz - bonePos.vz;
                ang.vy = ratan2(delta.vx, delta.vz);
                EGSGeometryINT_VectorGetAngleFromXZPlane(&delta, &ang);
                ang.vz = 0;
                cur.vy = ratan2(fwd.vx, fwd.vz);
                diff.vx = ang.vx - cur.vx;
                WRAP_ANGLE(diff.vx);
                diff.vy = ang.vy - cur.vy;
                WRAP_ANGLE(diff.vy);
                diff.vz = ang.vz - cur.vz;
                WRAP_ANGLE(diff.vz);
                if (diff.vy < -0x200) {
                    diff.vy = -0x200;
                } else if (diff.vy > 0x200) {
                    diff.vy = 0x200;
                }
                if (aimType == 2) {
                    spine.vx += -diff.vx / 2;
                }
                ang.vx = cur.vx + diff.vx;
                ang.vz = cur.vz + diff.vz;
                ang.vy = cur.vy + diff.vy;
                if (aimType == 1) {
                    ang.vx += 0x38;
                }
                ang.vz -= 0x400;
                do {
                do {
                do {
                do {
                do {
                do {
                slot->aimAngle = ang;
                } while (0);
                } while (0);
                } while (0);
                } while (0);
                } while (0);
                } while (0);
            } else {
                aiming = 0;
            }
        }
        if (spine.vx != 0 || spine.vy != 0 || spine.vz != 0) {
            AnimVec rot;

            rot.vx = -gobj->anim->restRot[13][0];
            rot.vy = gobj->anim->restRot[13][1];
            rot.vz = -gobj->anim->restRot[13][2];
            rot.vx += spine.vx;
            rot.vy += spine.vy;
            rot.vz += spine.vz;
            EGSFrameSetRotationParentYXZ(gobj->anim->bones[13], &rot);
        }
        if (head.vx != 0 || head.vy != 0 || head.vz != 0 || (aiming && aimType == 1)) {
            AnimVec rot;

            rot.vx = 0x71;
            rot.vy = 0;
            rot.vz = 0;
            rot.vx += head.vx;
            rot.vy += head.vy;
            rot.vz += head.vz;
            EGSFrameSetRotationParentYXZ(gobj->anim->bones[9], &rot);
        }
    } else {
        if (aiming) {
            AnimVec unused; /* keeps m/ang at the original stack offsets */
            AnimMatrix m;
            AnimVec ang;
            union {
                AnimSVec r;
                AnimVec c[3];
            } u;
            AnimFrame *bone;

            bone = NULL;
            ang = slot->aimAngle;
            if (aimType == 1) {
                bone = gobj->anim->bones[11];
            } else if (aimType == 2) {
                bone = gobj->anim->bones[8];
            }
            u.r.vx = -ang.vx;
            u.r.vy = ang.vy;
            u.r.vz = -ang.vz;
            RotMatrixYXZ(&u.r, &m);
            m.m[0][1] = -m.m[0][1];
            m.m[1][0] = -m.m[1][0];
            m.m[1][2] = -m.m[1][2];
            m.m[2][1] = -m.m[2][1];
            if (aimType == 1) {
                goto transpose;
            }
            if (aimType == 2) {
            transpose:
                u.c[0].vx = m.m[0][0];
                u.c[0].vy = m.m[1][0];
                u.c[0].vz = m.m[2][0];
                u.c[1].vx = m.m[0][1];
                u.c[1].vy = m.m[1][1];
                u.c[1].vz = m.m[2][1];
                u.c[2].vx = m.m[0][2];
                u.c[2].vy = m.m[1][2];
                u.c[2].vz = m.m[2][2];
                u.c[1].vx = -u.c[1].vx;
                u.c[1].vy = -u.c[1].vy;
                u.c[1].vz = -u.c[1].vz;
                u.c[2].vx = -u.c[2].vx;
                u.c[2].vy = -u.c[2].vy;
                u.c[2].vz = -u.c[2].vz;
                m.m[0][0] = u.c[1].vx;
                m.m[1][0] = u.c[1].vy;
                m.m[2][0] = u.c[1].vz;
                m.m[0][1] = u.c[2].vx;
                m.m[1][1] = u.c[2].vy;
                m.m[2][1] = u.c[2].vz;
                m.m[0][2] = u.c[0].vx;
                m.m[1][2] = u.c[0].vy;
                m.m[2][2] = u.c[0].vz;
            }
            if (bone->world == NULL) {
                bone->local.m[0][0] = m.m[0][0];
                bone->local.m[0][1] = -m.m[0][1];
                bone->local.m[0][2] = m.m[0][2];
                bone->local.m[1][0] = -m.m[1][0];
                bone->local.m[1][1] = m.m[1][1];
                bone->local.m[1][2] = -m.m[1][2];
                bone->local.m[2][0] = m.m[2][0];
                bone->local.m[2][1] = -m.m[2][1];
                bone->local.m[2][2] = m.m[2][2];
            } else {
                EGSFrameSetRotationMatrix(bone, 0, &m);
            }
        }
        EGSFrameUpdateHierarchy(gobj->anim->frame);
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/animate", func_8007E650);
#endif


