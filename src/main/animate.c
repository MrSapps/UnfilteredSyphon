#include "common.h"

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
    /* 0x0C */ s32 unk0C;
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

INCLUDE_ASM("asm/main/nonmatchings/animate", func_80079950);

#define OBJ_CLASS(id) (*(u8 *)&gameObjectsInfo[id].flags24)
#define OBJ_SEX(id) (gameObjectsInfo[id].flags24 & 0x3000)

#ifdef NON_MATCHING
/*
 * Logically equivalent; cse folds the original's redundant third
 * "(sex == 0x1000)" test of the first class switch, so this is one insn short.
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

#ifdef NON_MATCHING
/* Run on the scratchpad stack (old sp saved at 0x1F800000). */
#define SPAD_STACK_ENTER() __asm__ volatile("sw $29, 0x1F800000\n\tli $29, 0x1F8003E0")
#define SPAD_STACK_LEAVE() __asm__ volatile("lw $29, 0x1F800000")
/* same, spelled differently so jump.c does not cross-jump the two call sites */
#define SPAD_STACK_ENTER2() __asm__ volatile("sw $29,0x1F800000\n\tli $29,0x1F8003E0")
#define SPAD_STACK_LEAVE2() __asm__ volatile("lw $29,0x1F800000")

/*
 * Control flow matches; differs in register allocation ("paused & 0xFF" is not
 * hoisted out of the loop) and the original leaves the jal delay slots of the
 * scratchpad calls empty.
 */
void AnimateProcess(u8 paused) {
    s32 i;
    AnimGobj *gobj;
    u8 action;

    for (i = 0; i < ANIM_SLOT_MAX; i++) {
        gobj = D_8011F810[i].gobj;
        if (!D_8011F810[i].active) {
            continue;
        }
        if (D_8011F810[i].aimRequest) {
            if ((gobj->unk22 == 1 || gobj->unk22 == 2) && (gobj->anim->master->flags & 0x400000) &&
                !(gobj->anim->flags & 0x10) && gobj->unk0C) {
                if (D_8011F810[i].unk14 == 1) {
                    LOSGetGobjLOSTestPoint((void *)D_8011F810[i].aimPoint.vx, &D_8011F810[i].aimPoint);
                }
                if (func_8007D120(&D_8011F810[i]) == 1) {
                    if (D_8011F810[i].mode != 0) {
                        D_8011F810[i].unk00 = paused;
                        SPAD_STACK_ENTER();
                        func_8007E650(&D_8011F810[i]);
                        SPAD_STACK_LEAVE();
                    } else {
                        goto simple;
                    }
                }
            }
        } else {
            action = gobj->state->action;
            if (action == 5 || action == 6 || action == 9) {
            } else if (D_8011F810[i].mode == 0) {
            simple:
                if (!paused) {
                    func_80079950(&D_8011F810[i]);
                }
            } else {
                D_8011F810[i].unk00 = paused;
                SPAD_STACK_ENTER2();
                func_8007E650(&D_8011F810[i]);
                SPAD_STACK_LEAVE2();
            }
        }
        if (paused) {
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
#else
INCLUDE_ASM("asm/main/nonmatchings/animate", AnimateProcess);
#endif

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
        !(gobj->anim->flags & 0x10) && gobj->unk0C) {
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

#define WRAP_ANGLE(a)          \
    if ((a) > 0x800) {         \
        (a) -= 0x1000;         \
    } else if ((a) < -0x800) { \
        (a) += 0x1000;         \
    }

#ifdef NON_MATCHING
/*
 * Same length and stack layout; differs in global register allocation
 * (aiming/gobj/aimType land in s3/s1/s2 instead of s1/s2/s3) and a few
 * temporaries.
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
          !(gobj->anim->flags & 0x10) && gobj->unk0C)) {
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
                slot->aimAngle = ang;
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

            ang = slot->aimAngle;
            bone = NULL;
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


