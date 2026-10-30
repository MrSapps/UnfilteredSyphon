#include "common.h"
#include "gameobj.h"
#include "gobj.h"
#include "scratch.h"

SDATA_LOCAL(D_8010F93C)

typedef struct MovementState {
    /* 0x00 */ VECTOR32 pos;
    /* 0x10 */ VECTOR32 delta; /* pos change, << 12 */
    /* 0x20 */ VECTOR32 vel;
    /* 0x30 */ VECTOR32 accel; /* gravity is added here */
} MovementState;

/* Nearly zero, for the "has it stopped" checks. */
#define IN_RANGE(x) ((u32)((x) + 0x2000) <= 0x4000)

/* Objects whose model is hidden/inactive, or that are explicitly frozen, are not moved. */
#define GOBJ_MOVEMENT_FROZEN(gobj)                                                             \
    ((((gobj)->anim->model->flags & GOBJ_MODEL_FLAG_400000) &&                                 \
      (((gobj)->anim->unk00[8] & 0x10) || (gobj)->move == NULL)) ||                            \
     MOVE(gobj)->frozen)

/* Position or orientation bookkeeping (MovementInfo.translation/rotation, 0x80 bytes). */
typedef struct MovementBasicInfo {
    /* 0x00 */ MovementState cur;
    /* 0x40 */ MovementState next; /* becomes cur on MovementBasicInfoUpdateCurrent */
} MovementBasicInfo;

/* Character (CAS driven) movement request (MovementInfo.charMove). */
typedef struct CharacterMovement {
    /* 0x00 */ s32 unk00;
    /* 0x04 */ s32 unk04;
    /* 0x08 */ VECTOR32 input; /* joystick direction, character relative */
    /* 0x18 */ VECTOR32 vel;
    /* 0x28 */ s32 unk28;
    /* 0x2C */ VECTOR32 unk2C;
    /* 0x3C */ s32 type; /* 5, 8, 9: standing still; 6, 7: moving */
    /* 0x40 */ s32 prevType;
    /* 0x44 */ u8 unk44;
    /* 0x45 */ u8 unk45[3];
    /* 0x48 */ VECTOR32 unk48;
    /* 0x58 */ VECTOR32 unk58;
    /* 0x68 */ VECTOR32 unk68;
    /* 0x78 */ s32 unk78;
    /* 0x7C */ VECTOR32 unk7C;
    /* 0x8C */ VECTOR32 unk8C;
    /* 0x9C */ VECTOR32 unk9C;
    /* 0xAC */ VECTOR32 unkAC;
    /* 0xBC */ VECTOR32 unkBC;
    /* 0xCC */ s32 unkCC;
    /* 0xD0 */ VECTOR32 unkD0;
    /* 0xE0 */ VECTOR32 unkE0;
    /* 0xF0 */ VECTOR32 unkF0;
    /* 0x100 */ s32 unk100;
    /* 0x104 */ u8 unk104;
    /* 0x105 */ u8 unk105[0x23];
} CharacterMovement; /* size 0x128 */

/* MovementGobjReset request. */
typedef struct MovementResetInfo {
    /* 0x0 */ u32 type; /* 0: matrix, 1: position + rotation matrix, 2: position + YXZ angles, 3: teleport */
    /* 0x4 */ VECTOR32 *pos;
    /* 0x8 */ VECTOR32 *dir;
    /* 0xC */ s32 unkC;
} MovementResetInfo;

/*
 * The full layout of GameObjectRuntime.move (gobj.h's MoveInfo, 0x1B8 bytes); movement.c
 * owns it. Accessed through MOVE(gobj) until gobj.h names these fields.
 */
typedef struct MovementInfo {
    /* 0x000 */ MovementBasicInfo translation;
    /* 0x080 */ MovementBasicInfo rotation;
    /* 0x100 */ u8 frozen; /* no position/velocity updates while set */
    /* 0x101 */ u8 gravity; /* gravity is applied */
    /* 0x102 */ u8 unk102[6];
    /* 0x108 */ u8 moved;
    /* 0x109 */ u8 collides; /* collision update instead of free movement */
    /* 0x10A */ u8 unk10A[2];
    /* 0x10C */ VECTOR32 lowest;     /* lowest point of the bounding box */
    /* 0x11C */ VECTOR32 unk11C;
    /* 0x12C */ s32 unk12C;
    /* 0x130 */ u8 unk130[0x10];
    /* 0x140 */ s32 unk140;
    /* 0x144 */ s32 unk144;
    /* 0x148 */ s32 unk148;
    /* 0x14C */ u8 unk14C[4];
    /* 0x150 */ s32 unk150;
    /* 0x154 */ s32 unk154;
    /* 0x158 */ s32 unk158;
    /* 0x15C */ u8 unk15C[4];
    /* 0x160 */ s32 unk160;
    /* 0x164 */ s32 unk164;
    /* 0x168 */ s32 unk168;
    /* 0x16C */ u8 unk16C[4];
    /* 0x170 */ u8 stopTicks;   /* removed from the list after being still this long (0: never) */
    /* 0x171 */ u8 stillTicks;
    /* 0x172 */ u8 unk172[2];
    /* 0x174 */ s32 unk174;
    /* 0x178 */ s32 unk178;
    /* 0x17C */ s32 unk17C;
    /* 0x180 */ s32 unk180;
    /* 0x184 */ s32 unk184;
    /* 0x188 */ s32 unk188;
    /* 0x18C */ u8 unk18C[4];
    /* 0x190 */ s32 unk190;
    /* 0x194 */ s32 unk194;
    /* 0x198 */ s32 unk198;
    /* 0x19C */ s32 unk19C;
    /* 0x1A0 */ s32 unk1A0;
    /* 0x1A4 */ u32 edgeFlags; /* collision/edge state bits, 0x40000: jump requested */
    /* 0x1A8 */ CharacterMovement *charMove;
    /* 0x1AC */ s32 physics; /* nonzero: PhysicsGobjUpdate moves it */
    /* 0x1B0 */ s32 friendly; /* nonzero: FriendlyGobjUpdateBeforeCollision runs on it */
    /* 0x1B4 */ Gobj *next;   /* next object in the movement list (D_8010F93C) */
} MovementInfo;

#define MOVE(gobj) ((MovementInfo *)(gobj)->move)

extern Gobj *D_8010F93C; /* objects with movement */
extern s32 gCASJoystickGobjID;

s32 MovementBasicInfoReset(MovementBasicInfo *info, VECTOR32 *pos);
s32 MovementBasicInfoUpdate(MovementBasicInfo *info, VECTOR32 *pos);
s32 MovementBasicInfoUpdateCurrent(MovementBasicInfo *info);
s32 MovementGobjLookAtFrame(Gobj *gobj, EGSFrame *target, s32 roll);
void EGSFrameSetOrientation(EGSFrame *frame, s32 arg1, VECTOR32 *dir, s32 roll);
void EGSGeometryFIX_VectorGetAngleFromXZPlane(VECTOR32 *v, VECTOR32 *out);
s32 ratan2(s32 y, s32 x);
void MovementGobjReset(Gobj *gobj, MovementResetInfo *info);
s32 MovementGobjInitCAS(Gobj *gobj, s32 a1, s32 a2, s32 a3, s32 a4, s32 a5, s32 a6);
void CollisionGobjBasicInit(Gobj *gobj, s32 a, s32 b, s32 c, s32 d, s32 e);
s32 func_8004966C(Gobj *gobj);
void FriendlyGobjUpdateAfterCollision(Gobj *gobj);
s32 MovementCharacterRelative(s16 id, s32 arg1);
s32 MovementWorldRelative(s16 id, s32 arg1);
void func_8004BC70(Gobj *gobj, VECTOR32 *out);

typedef struct MovementSVector {
    s16 vx, vy, vz, pad;
} MovementSVector;

void EGSAnimGetBox(GameObjectAnim *anim, s32 arg1, VECTOR32 *box);
void EGSFrameSetMatrix(EGSFrame *frame, s32 arg1, void *m);
void EGSFrameSetPosition(EGSFrame *frame, s32 arg1, VECTOR32 *pos);
void EGSFrameSetRotationMatrix(EGSFrame *frame, s32 arg1, void *m);
void EGSFrameSetRotationYXZ(EGSFrame *frame, s32 arg1, VECTOR32 *angles);
void EGSGeometry_RotationMatrixToAngleYXZ(GOBJ_MATRIX *m, VECTOR32 *angles);
void func_800498A4(Gobj *gobj);
void EGSGeometry_GetBoxPoints(VECTOR32 *box, MovementSVector *points, GOBJ_MATRIX *m);

#define MOVE_TYPE_STILL(t) ((t) == 5 || (t) == 8 || (t) == 9)
#define MOVE_TYPE_MOVING(t) ((t) == 6 || (t) == 7)
s32 func_8004B058(Gobj *gobj);
void FriendlyGobjUpdateBeforeCollision(Gobj *gobj);
void EGSFrameUpdateHierarchy(void *frame);
void iMovementUpdate7(void);
void iMovementUpdate1(void);
void iMovementUpdate1_2(void);
void iMovementUpdate3(void);
void iMovementUpdate4(void);
void AnimateProcess(s32 arg0);
void *EGSMemMalloc(u32 size);
void PhysicsGobjBasicInit(Gobj *gobj, s32 arg1, s32 arg2);
void PhysicsGobjUpdate(Gobj *gobj);
void CollisionGobjBasicUpdate_Human(Gobj *gobj);
void CollisionGobjBasicUpdate_NonHuman(Gobj *gobj);
void CollisionGobjUpdate(Gobj *gobj);
s32 MovementRemoveGobj(Gobj *gobj);
s32 MovementAddGobj(Gobj *gobj);
extern s32 gPhysicsGravityAcceleration; /* gravity x; y and z follow */
extern s32 D_800FE5E8;
extern s32 D_800FE5EC;

/* Movement has (nearly) stopped. Same test as museum.c's PathIsStopped. */
static inline u8 MovementIsStopped(MovementInfo *move) {
    if (((move->moved && (move->edgeFlags & 0x100000)) || !move->gravity) &&
        IN_RANGE(move->translation.cur.vel.vx) && IN_RANGE(move->translation.cur.vel.vy) &&
        IN_RANGE(move->translation.cur.vel.vz) && IN_RANGE(move->rotation.cur.vel.vx) &&
        IN_RANGE(move->rotation.cur.vel.vy) && IN_RANGE(move->rotation.cur.vel.vz)) {
        return 1;
    }
    return 0;
}
void EGSFrameLookAtPoint(EGSFrame *frame, s32 arg1, VECTOR32 *point, s32 roll);
void EGSFrameLookAt(EGSFrame *frame, EGSFrame *target, s32 roll);
u8 FriendlyGobjGetFoundEdge(Gobj *gobj, s32 kind);
void CASRequestAction(s32 id, s32 action);
s32 MovementGobjUpdatePosition(Gobj *gobj, u8 translation, u8 rotation);
s32 MovementGobjUpdateVelocity(Gobj *gobj, u8 translation, u8 rotation);
s32 MovementGobjUpdateMovement(Gobj *gobj, u8 translation, u8 rotation);

s32 MovementGobjSetOrientation(Gobj *gobj, VECTOR32 *dir, s32 roll) {
    if (gobj == NULL) {
        return 0;
    }
    if (gobj->move == NULL) {
        EGSFrameSetOrientation(gobj->anim->frame, 0, dir, roll);
    } else {
        EGSGeometryFIX_VectorGetAngleFromXZPlane(dir, &MOVE(gobj)->rotation.next.pos);
        MOVE(gobj)->rotation.next.pos.vy = ratan2(dir->vx, dir->vz);
        MOVE(gobj)->rotation.next.pos.vz = roll;
    }
    return 1;
}

s32 MovementGobjLookAtGobj(Gobj *gobj, Gobj *target, s32 roll) {
    if (gobj == NULL || target == NULL) {
        return 0;
    }
    MovementGobjLookAtFrame(gobj, target->anim->frame, roll);
    return 1;
}

s32 MovementGobjLookAtAnim(Gobj *gobj, GameObjectAnim *anim, s32 roll) {
    if (gobj == NULL || anim == NULL) {
        return 0;
    }
    MovementGobjLookAtFrame(gobj, anim->frame, roll);
    return 1;
}

s32 MovementGobjLookAtFrame(Gobj *gobj, EGSFrame *target, s32 roll) {
    VECTOR32 pos;
    VECTOR32 tgt;
    VECTOR32 dir;

    if (gobj == NULL || target == NULL) {
        return 0;
    }
    if (gobj->move == NULL) {
        EGSFrameLookAt(gobj->anim->frame, target, roll);
    } else {
        pos = MOVE(gobj)->translation.cur.pos;
        tgt.vx = target->pos.vx;
        tgt.vy = target->pos.vy;
        tgt.vz = target->pos.vz;
        tgt.vy = -tgt.vy;
        dir.vx = tgt.vx - pos.vx;
        dir.vy = tgt.vy - pos.vy;
        dir.vz = tgt.vz - pos.vz;
        EGSGeometryFIX_VectorGetAngleFromXZPlane(&dir, &MOVE(gobj)->rotation.next.pos);
        MOVE(gobj)->rotation.next.pos.vy = ratan2(dir.vx, dir.vz);
        MOVE(gobj)->rotation.next.pos.vz = roll;
    }
    return 1;
}

s32 MovementGobjLookAtPoint(Gobj *gobj, VECTOR32 *point, s32 roll) {
    VECTOR32 pos;
    VECTOR32 dir;

    if (gobj == NULL) {
        return 0;
    }
    if (gobj->move == NULL) {
        EGSFrameLookAtPoint(gobj->anim->frame, 0, point, roll);
    } else {
        pos = MOVE(gobj)->translation.cur.pos;
        dir.vx = point->vx - pos.vx;
        dir.vy = point->vy - pos.vy;
        dir.vz = point->vz - pos.vz;
        EGSGeometryFIX_VectorGetAngleFromXZPlane(&dir, &MOVE(gobj)->rotation.next.pos);
        MOVE(gobj)->rotation.next.pos.vy = ratan2(dir.vx, dir.vz);
        MOVE(gobj)->rotation.next.pos.vz = roll;
    }
    return 1;
}

s32 MovementGobjGetPosition(Gobj *gobj, VECTOR32 *out) {
    MovementInfo *move;

    if (gobj == NULL) {
        return 0;
    }
    move = MOVE(gobj);
    if (move == NULL) {
        out->vx = gobj->anim->frame->pos.vx;
        out->vy = gobj->anim->frame->pos.vy;
        out->vz = gobj->anim->frame->pos.vz;
        out->vy = -out->vy;
    } else {
        *out = move->translation.cur.pos;
    }
    return 1;
}

s32 MovementBasicInfoReset(MovementBasicInfo *info, VECTOR32 *pos) {
    if (pos != NULL) {
        info->cur.pos = *pos;
        info->next.pos = *pos;
    }
    info->cur.delta.vx = 0;
    info->cur.delta.vy = 0;
    info->cur.delta.vz = 0;
    info->cur.vel.vx = 0;
    info->cur.vel.vy = 0;
    info->cur.vel.vz = 0;
    info->cur.accel.vx = 0;
    info->cur.accel.vy = 0;
    info->cur.accel.vz = 0;
    info->next.delta.vx = 0;
    info->next.delta.vy = 0;
    info->next.delta.vz = 0;
    info->next.vel.vx = 0;
    info->next.vel.vy = 0;
    info->next.vel.vz = 0;
    info->next.accel.vx = 0;
    info->next.accel.vy = 0;
    info->next.accel.vz = 0;
    return 1;
}

s32 MovementBasicInfoUpdate(MovementBasicInfo *info, VECTOR32 *pos) {
    info->next.pos = *pos;
    info->next.delta.vx = info->next.pos.vx - info->cur.pos.vx;
    info->next.delta.vy = info->next.pos.vy - info->cur.pos.vy;
    info->next.delta.vz = info->next.pos.vz - info->cur.pos.vz;
    info->next.delta.vx <<= 12;
    info->next.delta.vy <<= 12;
    info->next.delta.vz <<= 12;
    info->next.vel = info->next.delta;
    info->next.accel.vx = info->next.vel.vx - info->cur.vel.vx;
    info->next.accel.vy = info->next.vel.vy - info->cur.vel.vy;
    info->next.accel.vz = info->next.vel.vz - info->cur.vel.vz;
    return 1;
}

s32 MovementBasicInfoUpdateCurrent(MovementBasicInfo *info) {
    info->cur.pos = info->next.pos;
    info->cur.delta = info->next.delta;
    info->cur.vel = info->next.vel;
    info->cur.accel = info->next.accel;
    return 1;
}

s32 MovementGobjTranslationInfoReset(Gobj *gobj, VECTOR32 *pos) {
    if (gobj == NULL || gobj->move == NULL) {
        return 0;
    }
    MovementBasicInfoReset(&MOVE(gobj)->translation, pos);
    return 1;
}

s32 MovementGobjRotationInfoReset(Gobj *gobj, VECTOR32 *pos) {
    if (gobj == NULL || gobj->move == NULL) {
        return 0;
    }
    MovementBasicInfoReset(&MOVE(gobj)->rotation, pos);
    return 1;
}

s32 MovementGobjTranslationInfoUpdateCurrent(Gobj *gobj) {
    if (gobj == NULL || gobj->move == NULL) {
        return 0;
    }
    MovementBasicInfoUpdateCurrent(&MOVE(gobj)->translation);
    return 1;
}

s32 MovementGobjRotationInfoUpdateCurrent(Gobj *gobj) {
    if (gobj == NULL || gobj->move == NULL) {
        return 0;
    }
    MovementBasicInfoUpdateCurrent(&MOVE(gobj)->rotation);
    return 1;
}

s32 MovementGobjTranslationInfoUpdate(Gobj *gobj, VECTOR32 *pos) {
    if (gobj == NULL || gobj->move == NULL) {
        return 0;
    }
    MovementBasicInfoUpdate(&MOVE(gobj)->translation, pos);
    return 1;
}

s32 MovementGobjRotationInfoUpdate(Gobj *gobj, VECTOR32 *pos) {
    if (gobj == NULL || gobj->move == NULL) {
        return 0;
    }
    MovementBasicInfoUpdate(&MOVE(gobj)->rotation, pos);
    return 1;
}

void MovementGobjReset(Gobj *gobj, MovementResetInfo *info) {
    MovementInfo *move;
    EGSFrame *frame;
    VECTOR32 pos;
    VECTOR32 angles;
    GOBJ_MATRIX m;
    GOBJ_MATRIX *fm;

    if (gobj == NULL || gobj->move == NULL) {
        return;
    }
    move = MOVE(gobj);
    frame = gobj->anim->frame;
    switch (info->type) {
        case 0:
            if (info->pos != NULL) {
                EGSFrameSetMatrix(frame, 0, info->pos);
            }
            break;
        case 1:
            if (info->pos != NULL) {
                EGSFrameSetPosition(frame, 0, info->pos);
            }
            if (info->dir != NULL) {
                EGSFrameSetRotationMatrix(frame, 0, info->dir);
            }
            break;
        case 2:
            if (info->pos != NULL) {
                EGSFrameSetPosition(frame, 0, info->pos);
            }
            if (info->dir != NULL) {
                EGSFrameSetRotationYXZ(frame, 0, info->dir);
            }
            break;
        case 3:
            if (info->pos != NULL) {
                EGSFrameSetPosition(frame, 0, info->pos);
            }
            if (info->dir != NULL) {
                EGSFrameSetOrientation(frame, 0, info->dir, info->unkC);
            }
            break;
    }
    if (*(s32 *)((u8 *)frame + 0x20) != 0) {
        EGSFrameUpdateHierarchy(frame);
    }
    pos.vx = frame->pos.vx;
    pos.vy = frame->pos.vy;
    pos.vz = frame->pos.vz;
    pos.vy = -pos.vy;
    MovementBasicInfoReset(&move->translation, &pos);
    fm = (GOBJ_MATRIX *)frame;
    m.m[0][0] = fm->m[0][0];
    m.m[0][1] = -fm->m[0][1];
    m.m[0][2] = fm->m[0][2];
    m.m[1][0] = -fm->m[1][0];
    m.m[1][1] = fm->m[1][1];
    m.m[1][2] = -fm->m[1][2];
    m.m[2][0] = fm->m[2][0];
    m.m[2][1] = -fm->m[2][1];
    m.m[2][2] = fm->m[2][2];
    EGSGeometry_RotationMatrixToAngleYXZ(&m, &angles);
    MovementBasicInfoReset(&move->rotation, &angles);
    move->frozen = 0;
    func_800498A4(gobj);
    move->moved = 0;
    move->edgeFlags = 0;
    move->unk140 = 0;
    move->unk144 = 0;
    move->unk148 = 0;
    move->unk150 = 0;
    move->unk154 = 0x1000;
    move->unk158 = 0;
    move->unk12C = 0x80000001;
}

s32 MovementGobjInit(Gobj *gobj, VECTOR32 *from, VECTOR32 *pos, VECTOR32 *dir, u8 addToList) {
    MovementInfo **pmove;
    MovementInfo *move;
    MovementResetInfo info;

    if (gobj == NULL) {
        return 0;
    }
    pmove = (MovementInfo **)&gobj->move;
    if (*pmove == NULL && (*pmove = EGSMemMalloc(sizeof(MovementInfo))) == NULL) {
        return 0;
    }
    PhysicsGobjBasicInit(gobj, -1, 0);
    (*pmove)->collides = 0;
    (*pmove)->unk160 = 0;
    (*pmove)->unk164 = 0;
    (*pmove)->unk168 = 0;
    (*pmove)->stopTicks = 0;
    (*pmove)->stillTicks = 0;
    move = *pmove;
    move->unk174 = 0;
    move->unk178 = 0;
    move->unk17C = 0;
    move->unk180 = 0;
    move->unk184 = 0;
    move->unk188 = 0;
    move->unk190 = 0;
    move->unk194 = 0;
    move->unk198 = 0;
    move->unk19C = 0;
    (*pmove)->charMove = NULL;
    (*pmove)->physics = 0;
    (*pmove)->friendly = 0;
    (*pmove)->next = NULL;
    if (addToList == 1) {
        MovementAddGobj(gobj);
    }
    if (from != NULL) {
        info.type = 0;
        info.pos = from;
        MovementGobjReset(gobj, &info);
    } else {
        info.type = 2;
        info.pos = pos;
        info.dir = dir;
        MovementGobjReset(gobj, &info);
    }
    return 1;
}

void SYSMovementInit(void) {
    D_8010F93C = NULL;
}

/* Push onto the head of the movement list. */
#define MOVEMENT_LIST_PUSH(gobj)                \
    do {                                        \
        MOVE(gobj)->next = D_8010F93C;          \
        D_8010F93C = (gobj);                    \
    } while (0)

s32 MovementAddGobj(Gobj *gobj) {
    Gobj *it;

    if (gobj == NULL || gobj->move == NULL) {
        return 0;
    }
    if (D_8010F93C == NULL) {
        D_8010F93C = gobj;
        return 1;
    }
    for (it = D_8010F93C; it != NULL; it = MOVE(it)->next) {
        if (it == gobj) {
            return 0;
        }
    }
    MOVEMENT_LIST_PUSH(gobj);
    return 1;
}

s32 MovementRemoveGobj(Gobj *gobj) {
    Gobj *it;
    Gobj *prev;

    if (gobj == NULL || gobj->move == NULL) {
        return 0;
    }
    for (it = D_8010F93C, prev = NULL; it != gobj && it != NULL; it = MOVE(it)->next) {
        prev = it;
    }
    if (it == NULL) {
        return 0;
    }
    if (prev == NULL) {
        D_8010F93C = MOVE(gobj)->next;
    } else {
        MOVE(prev)->next = MOVE(gobj)->next;
    }
    return 1;
}

/* Bounding heights of a character from its bone frames (lowest / unk11C). */
s32 func_8004966C(Gobj *gobj) {
    VECTOR32 p;
    EGSFrame *f;
    s32 min;
    s32 max;
    s32 i;

    if ((gobj->npcState == 1 || gobj->npcState == 2) &&
        (gobj->anim->model->flags & GOBJ_MODEL_FLAG_400000) && !(gobj->anim->unk00[8] & 0x10) &&
        gobj->move != NULL) {
        f = gobj->anim->frames[0];
        p.vx = f->pos.vx;
        p.vy = f->pos.vy;
        p.vz = f->pos.vz;
        p.vy = -p.vy;
        min = max = p.vy;
        for (i = 1; i < 15; i++) {
            f = gobj->anim->frames[i];
            p.vx = f->pos.vx;
            p.vy = f->pos.vy;
            p.vz = f->pos.vz;
            p.vy = -p.vy;
            if (p.vy < min) {
                min = p.vy;
            } else if (max < p.vy && i != 8 && i != 2 && i != 9 && i != 3 && i != 7 && i != 1) {
                max = p.vy;
            }
        }
        MOVE(gobj)->lowest.vx = MOVE(gobj)->translation.cur.pos.vx;
        MOVE(gobj)->lowest.vy = min;
        MOVE(gobj)->lowest.vz = MOVE(gobj)->translation.cur.pos.vz;
        MOVE(gobj)->unk11C.vx = MOVE(gobj)->translation.cur.pos.vx;
        MOVE(gobj)->unk11C.vy = max;
        MOVE(gobj)->unk11C.vz = MOVE(gobj)->translation.cur.pos.vz;
    } else {
        p = MOVE(gobj)->translation.cur.pos;
        MOVE(gobj)->lowest = p;
        MOVE(gobj)->unk11C = p;
    }
    return 1;
}

#ifdef NON_MATCHING
/* lowest-point loop: the original addresses points[] through two induction pointers */
/*
 * Static inline in the original (inlined into iMovementUpdate1_2); global with an
 * out-of-line copy here since asm MovementGobjReset still calls it.
 */
inline void func_800498A4(Gobj *gobj) {
    MovementInfo *move = MOVE(gobj);
    VECTOR32 lowest;
    VECTOR32 box[2];
    MovementSVector points[8];
    GOBJ_MATRIX m;
    GOBJ_MATRIX *src;
    VECTOR32 *low = &lowest;
    VECTOR32 *b;
    MovementSVector *pt;
    s32 t;
    s32 i;

    if (gobj->npcState == 1 || gobj->npcState == 2) {
        func_8004966C(gobj);
        return;
    }
    if (move->collides != 3) {
        return;
    }
    src = (GOBJ_MATRIX *)gobj->anim->frame;
    m.m[0][0] = src->m[0][0];
    m.m[0][1] = -src->m[0][1];
    m.m[0][2] = src->m[0][2];
    m.m[1][0] = -src->m[1][0];
    m.m[1][1] = src->m[1][1];
    m.m[1][2] = -src->m[1][2];
    m.m[2][0] = src->m[2][0];
    m.m[2][1] = -src->m[2][1];
    m.m[2][2] = src->m[2][2];
    m.t[0] = src->t[0];
    m.t[1] = -src->t[1];
    m.t[2] = src->t[2];
    b = box;
    EGSAnimGetBox(gobj->anim, 0, b);
    t = box[0].vy;
    box[0].vy = -box[1].vy;
    box[1].vy = -t;
    EGSGeometry_GetBoxPoints(b, points, &m);
    lowest.vx = points[0].vx;
    lowest.vy = points[0].vy;
    lowest.vz = points[0].vz;
    for (i = 1; i < 8; i++) {
        if (points[i].vy < low->vy) {
            pt = &points[i];
            low->vx = pt->vx;
            low->vy = pt->vy;
            low->vz = pt->vz;
        }
    }
    move->lowest = lowest;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/movement", func_800498A4);
#endif

extern VECTOR32 D_80011FD0; /* zero vector */

#define FIX_TO_INT(x) ((x) >= 0 ? (x) >> 12 : -(-(x) >> 12))
#define FIX_TO_INT3(x) ((x) >= 0 ? (x) >> 3 : -(-(x) >> 3))
#define WRAP_ANGLE(a)              \
    if ((a) > 0x800) {             \
        (a) -= 0x1000;             \
    } else if ((a) < -0x800) {     \
        (a) += 0x1000;             \
    }

s32 MovementGobjUpdatePosition(Gobj *gobj, u8 translation, u8 rotation) {
    MovementInfo *move = MOVE(gobj);
    VECTOR32 delta;
    VECTOR32 pos;
    VECTOR32 zero = D_80011FD0;

    if (translation) {
        pos = move->translation.next.pos;
        delta = move->translation.next.delta;
        move->translation.cur.delta = delta;
        if (delta.vx != 0 || delta.vy != 0 || delta.vz != 0) {
            delta.vx = FIX_TO_INT(delta.vx);
            delta.vy = FIX_TO_INT(delta.vy);
            delta.vz = FIX_TO_INT(delta.vz);
            pos.vx += delta.vx;
            pos.vy += delta.vy;
            pos.vz += delta.vz;
            move->translation.next.pos = pos;
        }
        if (pos.vx != move->translation.cur.pos.vx || pos.vy != move->translation.cur.pos.vy ||
            pos.vz != move->translation.cur.pos.vz) {
            EGSFrameSetPosition(gobj->anim->frame, 0, &pos);
            move->translation.cur.pos = pos;
        }
        move->translation.next.delta = zero;
    }
    if (rotation) {
        pos = move->rotation.next.pos;
        delta = move->rotation.next.delta;
        move->rotation.cur.delta = delta;
        if (delta.vx != 0 || delta.vy != 0 || delta.vz != 0) {
            delta.vx = FIX_TO_INT(delta.vx);
            delta.vy = FIX_TO_INT(delta.vy);
            delta.vz = FIX_TO_INT(delta.vz);
            pos.vx += delta.vx;
            WRAP_ANGLE(pos.vx);
            pos.vy += delta.vy;
            WRAP_ANGLE(pos.vy);
            pos.vz += delta.vz;
            WRAP_ANGLE(pos.vz);
            move->rotation.next.pos = pos;
        }
        pos = move->rotation.next.pos;
        if (pos.vx != move->rotation.cur.pos.vx || pos.vy != move->rotation.cur.pos.vy ||
            pos.vz != move->rotation.cur.pos.vz) {
            EGSFrameSetRotationYXZ(gobj->anim->frame, 0, &pos);
            move->rotation.cur.pos = pos;
        }
        move->rotation.next.delta = zero;
    }
    return 1;
}

s32 MovementGobjUpdateVelocity(Gobj *gobj, u8 translation, u8 rotation) {
    MovementInfo *move = MOVE(gobj);
    VECTOR32 vel;
    VECTOR32 accel;
    VECTOR32 zero = D_80011FD0;

    if (translation) {
        vel = move->translation.next.vel;
        accel = move->translation.next.accel;
        if (accel.vx != 0 || accel.vy != 0 || accel.vz != 0) {
            vel.vx += accel.vx;
            vel.vy += accel.vy;
            vel.vz += accel.vz;
            move->translation.next.vel = vel;
        }
        move->translation.cur.vel = vel;
        move->translation.cur.accel = accel;
        move->translation.next.accel = zero;
    }
    if (rotation) {
        vel = move->rotation.next.vel;
        accel = move->rotation.next.accel;
        if (accel.vx != 0 || accel.vy != 0 || accel.vz != 0) {
            vel.vx += accel.vx;
            vel.vy += accel.vy;
            vel.vz += accel.vz;
            move->rotation.next.vel = vel;
        }
        move->rotation.cur.vel = vel;
        move->rotation.cur.accel = accel;
        move->rotation.next.accel = zero;
    }
    return 1;
}

s32 MovementGobjUpdateMovement(Gobj *gobj, u8 translation, u8 rotation) {
    MovementInfo *move = MOVE(gobj);

    if (translation) {
        move->translation.next.delta.vx += move->translation.next.vel.vx;
        move->translation.next.delta.vy += move->translation.next.vel.vy;
        move->translation.next.delta.vz += move->translation.next.vel.vz;
        move->translation.next.delta.vx += move->translation.next.accel.vx;
        move->translation.next.delta.vy += move->translation.next.accel.vy;
        move->translation.next.delta.vz += move->translation.next.accel.vz;
    }
    if (rotation) {
        move->rotation.next.delta.vx += move->rotation.next.vel.vx;
        move->rotation.next.delta.vy += move->rotation.next.vel.vy;
        move->rotation.next.delta.vz += move->rotation.next.vel.vz;
        move->rotation.next.delta.vx += move->rotation.next.accel.vx;
        move->rotation.next.delta.vy += move->rotation.next.accel.vy;
        move->rotation.next.delta.vz += move->rotation.next.accel.vz;
    }
    return 1;
}

void iMovementUpdate1(void) {
    Gobj *gobj;
    GameObjectAnim *anim;
    MovementInfo *move;
    s16 frozen;

    for (gobj = D_8010F93C; gobj != NULL; gobj = move->next) {
        anim = gobj->anim;
        move = MOVE(gobj);
        frozen = ((anim->model->flags & GOBJ_MODEL_FLAG_400000) && ((anim->unk00[8] & 0x10) || move == NULL)) ||
                 move->frozen;
        if (!frozen) {
            MovementGobjUpdatePosition(gobj, 0, 1);
            MovementGobjUpdateVelocity(gobj, 0, 1);
            MovementGobjUpdateMovement(gobj, 0, 1);
        } else {
            MovementBasicInfoReset(&move->rotation, NULL);
        }
    }
}

void iMovementUpdate1_1(void) {
    Gobj *gobj;
    MovementInfo *move;
    EGSFrame *frame;

    for (gobj = D_8010F93C; gobj != NULL; gobj = move->next) {
        frame = gobj->anim->frame;
        move = MOVE(gobj);
        if (*(s32 *)((u8 *)frame + 0x20) != 0) {
            EGSFrameUpdateHierarchy(frame);
        }
    }
}

#ifdef NON_MATCHING
/* inlines func_800498A4, which does not match yet */
void iMovementUpdate1_2(void) {
    Gobj *gobj;
    MovementInfo *move;

    for (gobj = D_8010F93C; gobj != NULL; gobj = move->next) {
        move = MOVE(gobj);
        func_800498A4(gobj);
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/movement", iMovementUpdate1_2);
#endif

void iMovementUpdate2(void) {
    Gobj *gobj;
    MovementInfo *move;

    for (gobj = D_8010F93C; gobj != NULL; gobj = move->next) {
        move = MOVE(gobj);
        if (move->friendly != 0) {
            FriendlyGobjUpdateBeforeCollision(gobj);
        }
    }
}

void iMovementUpdate3(void) {
    Gobj *gobj;
    MovementInfo *move;
    s16 frozen;

    for (gobj = D_8010F93C; gobj != NULL; gobj = move->next) {
        move = MOVE(gobj);
        frozen = GOBJ_MOVEMENT_FROZEN(gobj);
        if (!frozen) {
            if (move->collides) {
                if (move->friendly == 0) {
                    if (gobj->npcState == 1 || gobj->npcState == 2) {
                        CollisionGobjBasicUpdate_Human(gobj);
                    } else {
                        CollisionGobjBasicUpdate_NonHuman(gobj);
                    }
                } else {
                    CollisionGobjUpdate(gobj);
                }
            } else {
                MovementGobjUpdatePosition(gobj, 1, 0);
                MovementGobjUpdateVelocity(gobj, 1, 0);
                MovementGobjUpdateMovement(gobj, 1, 0);
            }
            move->moved = 1;
        } else {
            MovementBasicInfoReset(&move->translation, NULL);
            move->edgeFlags = 0;
            move->moved = 0;
        }
        if (move->physics == 0) {
            if (move->gravity) {
                MOVE(gobj)->translation.next.accel.vx += gPhysicsGravityAcceleration;
                MOVE(gobj)->translation.next.accel.vy += D_800FE5E8;
                MOVE(gobj)->translation.next.accel.vz += D_800FE5EC;
            }
        } else {
            PhysicsGobjUpdate(gobj);
        }
    }
}

void iMovementUpdate4(void) {
    Gobj *gobj;
    MovementInfo *move;

    for (gobj = D_8010F93C; gobj != NULL; gobj = move->next) {
        move = MOVE(gobj);
        if (move->friendly != 0) {
            SPAD_ENTER();
            EGSFrameUpdateHierarchy(gobj->anim->frame);
            SPAD_LEAVE();
            SPAD_ENTER();
            func_8004966C(gobj);
            SPAD_LEAVE();
            FriendlyGobjUpdateAfterCollision(gobj);
        }
    }
}

void iMovementUpdate5(void) {
    Gobj *gobj;
    MovementInfo *move;

    for (gobj = D_8010F93C; gobj != NULL; gobj = move->next) {
        move = MOVE(gobj);
        if (gobj->cas != NULL) {
            func_8004B058(gobj);
        }
    }
}

#ifdef NON_MATCHING
/* the original copies move into a separate register for the stopped test; frozen/stillTicks branch layout */
void iMovementUpdate7(void) {
    Gobj *gobj;
    Gobj *next;
    MovementInfo *move;

    for (gobj = D_8010F93C; gobj != NULL; gobj = next) {
        move = MOVE(gobj);
        next = move->next;
        if (move->stopTicks != 0) {
            if (MovementIsStopped(move) && !(s16)GOBJ_MOVEMENT_FROZEN(gobj)) {
                move->stillTicks++;
            } else {
                move->stillTicks = 0;
            }
            if (move->stillTicks >= move->stopTicks) {
                MovementRemoveGobj(gobj);
            }
        }
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/movement", iMovementUpdate7);
#endif

s32 MovementUpdateAfterDisplay(void) {
    SPAD_ENTER();
    iMovementUpdate1();
    SPAD_LEAVE();
    AnimateProcess(1);
    SPAD_ENTER();
    iMovementUpdate1_1();
    SPAD_LEAVE();
    SPAD_ENTER();
    iMovementUpdate1_2();
    SPAD_LEAVE();
    iMovementUpdate2();
    SPAD_ENTER();
    iMovementUpdate3();
    SPAD_LEAVE();
    iMovementUpdate4();
    SPAD_ENTER();
    iMovementUpdate5();
    SPAD_LEAVE();
    AnimateProcess(0);
    return 1;
}

s32 MovementUpdateBeforeDisplay(void) {
    SPAD_ENTER();
    iMovementUpdate7();
    SPAD_LEAVE();
    __asm__ volatile("nop"); /* load delay: $sp is used right away */
    return 1;
}

s32 MovementGobjGetCASInfo(Gobj *gobj, CharacterMovement **out) {
    if (gobj == NULL) {
        return 0;
    }
    *out = MOVE(gobj)->charMove;
    return 1;
}

s32 MovementGobjGetCharacterMovement(Gobj *gobj, VECTOR32 *vel, s32 *type) {
    CharacterMovement *cm;

    if (gobj == NULL || gobj->move == NULL || (cm = MOVE(gobj)->charMove) == NULL) {
        return 0;
    }
    *vel = cm->vel;
    *type = MOVE(gobj)->charMove->type;
    return 1;
}

s32 MovementGobjRegisterCharacterMovement(Gobj *gobj, VECTOR32 *vel, s32 type) {
    CharacterMovement *cm;
    s32 prev;

    if (gobj == NULL || gobj->move == NULL || MOVE(gobj)->charMove == NULL) {
        return 0;
    }
    cm = MOVE(gobj)->charMove;
    if (vel == NULL) {
        cm->vel.vx = 0;
        cm->vel.vy = 0;
        cm->vel.vz = 0;
    } else {
        cm->vel = *vel;
    }
    if (cm->vel.vx == 0 && cm->vel.vy == 0 && cm->vel.vz == 0) {
        type = 5;
    }
    prev = cm->type;
    cm->type = type;
    cm->prevType = prev;
    return 1;
}

s32 MovementGobjInitCAS(Gobj *gobj, s32 a1, s32 a2, s32 a3, s32 a4, s32 a5, s32 a6) {
    MovementInfo *move;
    CharacterMovement **cm;

    if (gobj == NULL) {
        return 0;
    }
    move = MOVE(gobj);
    if (move == NULL) {
        return 0;
    }
    cm = &move->charMove;
    if (move->charMove == NULL) {
        *cm = EGSMemMalloc(sizeof(CharacterMovement));
        if (*cm == NULL) {
            return 0;
        }
    }
    (*cm)->unk00 = a4;
    (*cm)->unk04 = a5;
    (*cm)->input.vx = 0;
    (*cm)->input.vy = 0;
    (*cm)->input.vz = 0;
    (*cm)->vel.vx = 0;
    (*cm)->vel.vy = 0;
    (*cm)->vel.vz = 0;
    (*cm)->unk28 = 0;
    (*cm)->unk2C.vx = 0;
    (*cm)->unk2C.vy = 0;
    (*cm)->unk2C.vz = 0;
    (*cm)->type = 5;
    (*cm)->prevType = 5;
    (*cm)->unk44 = 1;
    (*cm)->unk48.vx = 0;
    (*cm)->unk48.vy = 0;
    (*cm)->unk48.vz = 0;
    (*cm)->unk58.vx = 0;
    (*cm)->unk58.vy = 0;
    (*cm)->unk58.vz = 0;
    (*cm)->unk68.vx = 0;
    (*cm)->unk68.vy = 0;
    (*cm)->unk68.vz = 0;
    (*cm)->unk68.pad = 0;
    (*cm)->unk7C.vx = 0;
    (*cm)->unk7C.vy = 0;
    (*cm)->unk7C.vz = 0;
    (*cm)->unk8C.vx = 0;
    (*cm)->unk8C.vy = 0;
    (*cm)->unk8C.vz = 0;
    (*cm)->unk9C.vx = 0;
    (*cm)->unk9C.vy = 0;
    (*cm)->unk9C.vz = 0;
    (*cm)->unkAC.vx = 0;
    (*cm)->unkAC.vy = 0;
    (*cm)->unkAC.vz = 0;
    (*cm)->unkAC.pad = 0;
    (*cm)->unkBC.vx = 0;
    (*cm)->unkBC.vy = 0;
    (*cm)->unkBC.vz = 0;
    (*cm)->unkBC.pad = 0;
    (*cm)->unkD0.vx = 0;
    (*cm)->unkD0.vy = 0;
    (*cm)->unkD0.vz = 0;
    (*cm)->unkE0.vx = 0;
    (*cm)->unkE0.vy = 0;
    (*cm)->unkE0.vz = 0;
    (*cm)->unkF0.vx = 0;
    (*cm)->unkF0.vy = 0;
    (*cm)->unkF0.vz = 0;
    (*cm)->unk100 = 0x80000001;
    (*cm)->unk104 = 0;
    return 1;
}

void EGSGeometry_AngleInterpolate(s32 from, s32 to, s32 step, u8 mode, s32 *out, s32 *remain);

static inline void MovementValueArctan(s32 y, s32 x, s32 *out) {
    *out = ratan2(y, x);
}

void ShadowRequestDraw(Gobj *gobj);
void func_8004B8E8(Gobj *gobj);
/* Character movement: edge/fall reactions, banking, joystick facing. */
/* Turn the yaw towards a direction (snappier while in active state 8). */
static inline void MovementTurnTowards(Gobj *gobj, VECTOR32 *v, u8 mode) {
    VECTOR32 dir = *v;
    s32 speed;
    s32 target;
    s32 step;
    s32 remain;

    if (dir.vx != 0 || dir.vy != 0 || dir.vz != 0) {
        speed = 0x71;
        if (gobj->cas->active == 8) {
            speed = 0x11C;
        }
        MovementValueArctan(dir.vx, dir.vz, &target);
        EGSGeometry_AngleInterpolate(MOVE(gobj)->rotation.cur.pos.vy, target, speed, mode, &step, &remain);
        MOVE(gobj)->rotation.next.delta.vy = step << 12;
    }
}

#ifdef NON_MATCHING
/* regalloc: original keeps the AngleInterpolate mode (0) in s1 across ratan2, shifting s-regs */
s32 func_8004B058(Gobj *gobj) {
    MovementInfo *move = MOVE(gobj);
    u32 flags = move->edgeFlags;
    CharacterMovement *cm = move->charMove;
    s32 isFirstPerson;

    if (flags & 0x400) {
        CASRequestAction(gobj->id, 0x14);
        MOVE(gobj)->edgeFlags &= ~0x400;
    } else if (flags & 0x200) {
        CASRequestAction(gobj->id, 0x13);
    } else if ((flags & 0xC000) == 0xC000 &&
               ((flags & 0x100) || (gobj->cas->flags & 0x800000) ||
                ((gobj->cas->flags & 0x1000) && move->translation.cur.vel.vy >= 0)) &&
               (!(gobj->cas->flags & 0x10000) || MOVE(gobj)->translation.cur.vel.vy <= -0x1D709) &&
               gobj->cas->active != 8 && FriendlyGobjGetFoundEdge(gobj, 6)) {
        CASRequestAction(gobj->id, 0x1C);
    } else if ((MOVE(gobj)->edgeFlags & 0x3000) == 0x3000 && (gobj->cas->flags & 0x1000) &&
               MOVE(gobj)->translation.cur.vel.vy >= 0 && FriendlyGobjGetFoundEdge(gobj, 5)) {
        CASRequestAction(gobj->id, 0x19);
    } else {
        if (MOVE(gobj)->translation.cur.vel.vy <= 0 &&
            ((MOVE(gobj)->edgeFlags & 0x100) || (gobj->cas->flags & 0x800000) ||
             (MOVE(gobj)->edgeFlags & 0x80000)) &&
            !(gobj->cas->flags & 0x1000) && gobj->cas->active != 8) {
            CASRequestAction(gobj->id, 0x33);
        }
        if ((gobj->cas->flags & 0x8000) && gobj->cas->active != 0xC) {
            if (MOVE(gobj)->edgeFlags & 0x800) {
                CASRequestAction(gobj->id, 0x21);
            } else if (MOVE(gobj)->edgeFlags & 0x100000) {
                CASRequestAction(gobj->id, 0x3E);
            }
        }
    }
    if (gobj->id == gCASJoystickGobjID) {
        if (gobj->cas->active == 2 && (gobj->cas->flags & 0xC) && (MOVE(gobj)->edgeFlags & 8) &&
            FriendlyGobjGetFoundEdge(gobj, 3)) {
            CASRequestAction(gobj->id, 0x5D);
        }
        if (gobj->id == gCASJoystickGobjID && gobj->cas->active == 8 && (MOVE(gobj)->edgeFlags & 0x40)) {
            FriendlyGobjGetFoundEdge(gobj, 6);
        }
    }
    func_8004B8E8(gobj);
    if (gobj->id == gCASJoystickGobjID &&
        ((isFirstPerson = gobj->cas->flags & 0x1000) ||
         (gobj != NULL && gobj->move != NULL && MOVE(gobj)->friendly != 0 &&
          *(s32 *)MOVE(gobj)->friendly == 1)) &&
        gobj != NULL && gobj->move != NULL && MOVE(gobj)->charMove != NULL) {
        if (isFirstPerson) {
            cm->unk2C.vx = -cm->unk9C.vx;
            cm->unk2C.vz = -cm->unk9C.vz;
            cm->unk2C.vy = -cm->unk9C.vy;
        }
        MovementTurnTowards(gobj, &cm->unk2C, 0);
    }
    if (cm->unk104) {
        ShadowRequestDraw(gobj);
    }
    return 1;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/movement", func_8004B058);
#endif

void MovementGobjTeleport(Gobj *gobj, VECTOR32 *pos, VECTOR32 *dir, s32 arg3) {
    MovementResetInfo info;

    if (gobj != NULL && gobj->move != NULL && MOVE(gobj)->charMove != NULL) {
        info.type = 3;
        info.pos = pos;
        info.dir = dir;
        info.unkC = arg3;
        MovementGobjReset(gobj, &info);
        MovementGobjInitCAS(gobj, 0, 0, 0, 0, 0, 0);
        CollisionGobjBasicInit(gobj, 0x80000001, -1, -1, -1, -1);
    }
}

void MovementGobjTeleport_ByXZLookAtPoint(Gobj *gobj, VECTOR32 *pos, VECTOR32 *lookAt, s32 arg3) {
    VECTOR32 cur;
    VECTOR32 dir;

    if (lookAt != NULL) {
        if (pos == NULL) {
            cur = MOVE(gobj)->translation.cur.pos;
            pos = &cur;
        }
        dir.vx = lookAt->vx - pos->vx;
        dir.vy = lookAt->vy - pos->vy;
        dir.vz = lookAt->vz - pos->vz;
        dir.vy = 0;
        MovementGobjTeleport(gobj, pos, &dir, arg3);
    } else {
        MovementGobjTeleport(gobj, pos, NULL, arg3);
    }
}

s32 MovementCharacterRelative(s16 id, s32 arg1) {
    Gobj *gobj = gameObjectsInfo[id].runtime;
    CharacterMovement *cm;
    VECTOR32 input;

    if (gobj == NULL || gobj->move == NULL || MOVE(gobj)->charMove == NULL) {
        return 0;
    }
    cm = MOVE(gobj)->charMove;
    input = cm->input;
    if (input.vx == 0 && input.vy == 0 && input.vz == 0) {
        if (gobj->cas->active == 8) {
            CASRequestAction(gobj->id, 0x1F);
        }
    } else if (MOVE_TYPE_STILL(cm->type)) {
        if (input.vz > 0) {
            if (!(gobj->cas->flags & 0x200000)) {
                CASRequestAction(gobj->id, 0x1A);
            }
        } else if (input.vz < 0) {
            if (!(gobj->cas->flags & 0x400000)) {
                CASRequestAction(gobj->id, 0x44);
            }
        }
    } else if (input.vx < 0) {
        if (gobj->cas->active == 8) {
            if (MOVE(gobj)->edgeFlags & 0x8000) {
                CASRequestAction(gobj->id, 0x1D);
            } else {
                CASRequestAction(gobj->id, 0x1F);
            }
        }
    } else if (input.vx > 0) {
        if (gobj->cas->active == 8) {
            if (MOVE(gobj)->edgeFlags & 0x4000) {
                CASRequestAction(gobj->id, 0x1E);
            } else {
                CASRequestAction(gobj->id, 0x1F);
            }
        }
    }
    return 1;
}

s32 EGSMath_VectorModulus(VECTOR32 *v, s32 *out);

static inline void MovementClampAbs(s32 *v, s32 max) {
    s32 x = *v;

    if (max < x) {
        *v = max;
    } else if (x < -max) {
        *v = -max;
    } else {
        *v = x;
    }
}

/* Bank (roll) a moving object into its turns, or level it out. */
void func_8004B8E8(Gobj *gobj) {
    VECTOR32 angle;
    VECTOR32 add = D_80011FD0;
    VECTOR32 d;
    s32 speed;

    d.vx = FIX_TO_INT(MOVE(gobj)->rotation.next.delta.vx);
    d.vy = FIX_TO_INT(MOVE(gobj)->rotation.next.delta.vy);
    d.vz = FIX_TO_INT(MOVE(gobj)->rotation.next.delta.vz);
    angle.vx = MOVE(gobj)->rotation.next.pos.vx + d.vx;
    WRAP_ANGLE(angle.vx);
    angle.vy = MOVE(gobj)->rotation.next.pos.vy + d.vy;
    WRAP_ANGLE(angle.vy);
    angle.vz = MOVE(gobj)->rotation.next.pos.vz + d.vz;
    WRAP_ANGLE(angle.vz);
    angle.vz <<= 12;
    if ((gobj->cas->flags & 8) && gobj->cas->active == 1 && !(MOVE(gobj)->edgeFlags & 0x100)) {
        d = MOVE(gobj)->translation.cur.vel;
        d.vy = 0;
        EGSMath_VectorModulus(&d, &speed);
        if (angle.vz > 0x4FA4F ? speed >= 0x3321 : speed > 0xB320) {
            VECTOR32 rv;
            s32 bank;

            rv = MOVE(gobj)->rotation.cur.vel;
            bank = -rv.vy * 4;
            MovementClampAbs(&bank, 0xC16C1);
            add.vz = bank - angle.vz;
            add.vz = FIX_TO_INT3(add.vz);
        } else {
            add.vz = angle.vz >= 0 ? -(angle.vz >> 1) : -angle.vz >> 1;
        }
    } else {
        add.vz = angle.vz >= 0 ? -(angle.vz >> 1) : -angle.vz >> 1;
    }
    MOVE(gobj)->rotation.next.delta.vx += add.vx;
    MOVE(gobj)->rotation.next.delta.vy += add.vy;
    MOVE(gobj)->rotation.next.delta.vz += add.vz;
}

void JoystickGobjTurn(VECTOR32 *out);

/* Turn a character towards its movement direction. */
void func_8004BC70(Gobj *gobj, VECTOR32 *out) {
    MovementInfo *move = MOVE(gobj);
    CharacterMovement *cm = move->charMove;
    VECTOR32 angle;
    VECTOR32 d;
    s32 target;
    s32 step;
    s32 remain;
    s32 yaw;
    s32 speed;

    if ((move->edgeFlags & 0x104) != 4) {
        return;
    }
    if (gobj == gameObjectsInfo[gCASJoystickGobjID].runtime) {
        JoystickGobjTurn(out);
        return;
    }
    *out = cm->vel;
    if (out->vx == 0 && out->vy == 0 && out->vz == 0) {
        return;
    }
    d.vx = FIX_TO_INT(MOVE(gobj)->rotation.next.delta.vx);
    d.vy = FIX_TO_INT(MOVE(gobj)->rotation.next.delta.vy);
    d.vz = FIX_TO_INT(MOVE(gobj)->rotation.next.delta.vz);
    angle.vx = MOVE(gobj)->rotation.next.pos.vx + d.vx;
    WRAP_ANGLE(angle.vx);
    angle.vy = MOVE(gobj)->rotation.next.pos.vy + d.vy;
    WRAP_ANGLE(angle.vy);
    angle.vz = MOVE(gobj)->rotation.next.pos.vz + d.vz;
    WRAP_ANGLE(angle.vz);
    yaw = angle.vy;
    MovementValueArctan(out->vx, out->vz, &target);
    if ((gobj->cas->flags & 0x4000000) ||
        (MOVE_TYPE_STILL(cm->type) && !MOVE_TYPE_STILL(cm->prevType)) || gobj->cas->active == 9) {
        speed = 0;
    } else if (MOVE_TYPE_STILL(cm->type) && MOVE_TYPE_STILL(cm->prevType)) {
        speed = cm->unk00;
    } else {
        speed = cm->unk04;
    }
    EGSGeometry_AngleInterpolate(yaw, target, speed, 1, &step, &remain);
    step <<= 12;
    if (speed == 0x1000) {
        MOVE(gobj)->rotation.next.delta.vy += step;
    } else {
        MOVE(gobj)->rotation.next.vel.vy += step;
    }
}

s32 MovementWorldRelative(s16 id, s32 arg1) {
    Gobj *gobj = gameObjectsInfo[id].runtime;
    CharacterMovement *cm;
    VECTOR32 dir;

    if (gobj == NULL) {
        return 0;
    }
    if (gobj->move == NULL || MOVE(gobj)->charMove == NULL) {
        return 0;
    }
    cm = MOVE(gobj)->charMove;
    func_8004BC70(gobj, &dir);
    if (gobj->cas->flags & 0x100000) {
        if (MOVE_TYPE_MOVING(cm->prevType) && MOVE_TYPE_STILL(cm->type)) {
            CASRequestAction(gobj->id, 0x38);
        } else if (MOVE_TYPE_STILL(cm->prevType) && MOVE_TYPE_MOVING(cm->type)) {
            CASRequestAction(gobj->id, 0x39);
        } else if (MOVE_TYPE_STILL(cm->type) && gobj->id != gCASJoystickGobjID) {
            CASRequestAction(gobj->id, 5);
        } else {
            CASRequestAction(gobj->id, cm->type);
        }
    } else if (MOVE_TYPE_STILL(cm->type) && gobj->id != gCASJoystickGobjID) {
        CASRequestAction(gobj->id, 5);
    } else {
        CASRequestAction(gobj->id, cm->type);
    }
    return 1;
}

void MovementGobjMoveCAS(s16 id, s32 arg1) {
    CASState *cas = gameObjectsInfo[id].runtime->cas;

    if (cas->active == 8) {
        SPAD_ENTER();
        MovementCharacterRelative(id, arg1);
        SPAD_LEAVE();
    } else if (!(cas->flags & 0x80000)) {
        SPAD_ENTER();
        MovementWorldRelative(id, arg1);
        SPAD_LEAVE();
    }
    __asm__ volatile("nop"); /* load delay after restoring $sp */
}

void MovementGobjJumpCAS(s16 id) {
    Gobj *gobj = gameObjectsInfo[id].runtime;
    s32 action = 0;

    if (gobj != NULL && gobj->move != NULL && MOVE(gobj)->charMove != NULL) {
        if (!(MOVE(gobj)->edgeFlags & 0x40000)) {
            if ((MOVE(gobj)->edgeFlags & 0xC000) == 0xC000 && FriendlyGobjGetFoundEdge(gobj, 6)) {
                action = 0x1A;
            } else if ((MOVE(gobj)->edgeFlags & 0x3000) == 0x3000 && FriendlyGobjGetFoundEdge(gobj, 5)) {
                action = 0x19;
            } else if ((MOVE(gobj)->edgeFlags & 0x30000) == 0x30000 && FriendlyGobjGetFoundEdge(gobj, 7)) {
                action = 0x18;
            }
            if (action != 0) {
                CASRequestAction(gobj->id, action);
            }
        }
        MOVE(gobj)->edgeFlags &= ~0x40000;
    }
}

void MovementGobjCrouchCAS(s16 id, s32 action) {
    Gobj *gobj = gameObjectsInfo[id].runtime;
    s32 type;

    if (action == 0x4D) {
        if (gobj->cas->flags & 2) {
            if ((MOVE(gobj)->edgeFlags & 8) && FriendlyGobjGetFoundEdge(gobj, 3)) {
                CASRequestAction(id, 0x1B);
            }
        } else if (!(gobj->cas->flags & 0x100000)) {
            CASRequestAction(id, 0xB);
        }
    } else {
        type = MOVE(gobj)->charMove->type;
        if (type == 5 || type == 8 || type == 9) {
            CASRequestAction(id, 0x4D);
        }
    }
}

INCLUDE_RODATA("asm/main/nonmatchings/movement", D_80011FC0);

INCLUDE_RODATA("asm/main/nonmatchings/movement", D_80011FD0);
