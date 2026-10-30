#include "common.h"
#include "game.h"
#include "gobj.h"

/* camera.c's small data, still assembled from asm */
SDATA_LOCAL(D_8010F70A)
SDATA_LOCAL(gDefaultDisplayPriority)
SDATA_LOCAL(gCamera)

/* Display-side camera owned by the EGS display code (EGSCameraCreate). */
typedef struct EGSCamera {
    /* 0x00 */ EGSFrame *frame;
    /* 0x04 */ u16 projection; /* projection distance */
    /* 0x06 */ u16 flags; /* 1: displayed, 8: game camera */
    /* 0x08 */ u8 priority;
    /* 0x09 */ u8 unk09[3];
    /* 0x0C */ void *region;
    /* 0x10 */ u8 unk10[4];
    /* 0x14 */ s32 id;
} EGSCamera;

/* A value with its per-frame velocity and acceleration (0x30 bytes). */
typedef struct CameraMotion {
    /* 0x00 */ VECTOR32 value;
    /* 0x10 */ VECTOR32 vel;
    /* 0x20 */ VECTOR32 accel;
} CameraMotion;

/* A frame the camera tracks: its own, the lead frame or the watch frame (0x8C bytes). */
typedef struct CameraFrameInfo {
    /* 0x00 */ EGSFrame *frame;
    /* 0x04 */ CameraMotion pos;
    /* 0x34 */ CameraMotion angle;
    /* 0x64 */ VECTOR32 orientation;
    /* 0x74 */ VECTOR32 offset;
    /* 0x84 */ EGSFrame *parent;
    /* 0x88 */ u8 unk88[4];
} CameraFrameInfo; /* size 0x8C */

/* Request passed to the move/frame accessors (Camera.access). */
typedef struct CameraAccess {
    /* 0x00 */ s32 value;
    /* 0x04 */ s32 *ptr;
    /* 0x08 */ VECTOR32 vec;
    /* 0x18 */ void *src;
    /* 0x1C */ s32 mask[3]; /* which of vec's components to use */
    /* 0x28 */ u8 unk28[4];
    /* 0x2C */ u8 apply;
    /* 0x2D */ u8 unk2D[7];
} CameraAccess; /* size 0x34 */

/* One animated camera parameter (0x144 bytes). */
typedef struct CameraMoveInfo {
    /* 0x000 */ u8 active;
    /* 0x001 */ u8 unk001[3];
    /* 0x004 */ s32 type; /* 0: scalar, else vector */
    /* 0x008 */ u8 unk008[4];
    /* 0x00C */ CameraMotion input;
    /* 0x03C */ VECTOR32 value;
    /* 0x04C */ VECTOR32 target;
    /* 0x05C */ CameraMotion motion;
    /* 0x08C */ s32 changed[3];
    /* 0x098 */ u8 unk098[4];
    /* 0x09C */ VECTOR32 min;
    /* 0x0AC */ VECTOR32 max;
    /* 0x0BC */ VECTOR32 nextMin;
    /* 0x0CC */ VECTOR32 nextMax;
    /* 0x0DC */ VECTOR32 limit[3];
    /* 0x10C */ VECTOR32 nextLimit[3];
    /* 0x13C */ VECTOR32 *origin;
    /* 0x140 */ EGSFrame **frameRef;
} CameraMoveInfo; /* size 0x144 */

#define CAMERA_MOVE_MAX 9

typedef struct Camera {
    /* 0x000 */ EGSCamera *display;
    /* 0x004 */ s32 mode;
    /* 0x008 */ u8 unk008[4];
    /* 0x00C */ CameraFrameInfo self;
    /* 0x098 */ CameraFrameInfo lead;
    /* 0x124 */ CameraFrameInfo watch;
    /* 0x1B0 */ CameraMoveInfo move[CAMERA_MOVE_MAX];
    /* 0xD14 */ CameraAccess access;
} Camera; /* size 0xD48 */

extern u16 D_8010F70A; /* next camera id */
extern u8 gDefaultDisplayPriority;
extern Camera *gCamera;
extern Camera *gCameraList[2];

u8 CameraActivate(Camera *cam, u8 on);
u8 CameraDisplay(Camera *cam, u8 on);
u8 func_80018BF8(CameraMoveInfo *move, CameraAccess *access);
u8 func_80018C6C(CameraMoveInfo *move, CameraAccess *access);
u8 func_80018E30(CameraMoveInfo *move, CameraAccess *access);
u8 func_8001893C(VECTOR32 *src, VECTOR32 *unused, u32 count, CameraAccess *access);
u8 func_8001902C(CameraMoveInfo *move, CameraAccess *access);
u8 func_80019324(CameraMoveInfo *move, s32 *mask);
void func_80019270(s32 angle, s32 *out);
void func_800192EC(s32 a, s32 b, s32 *out);
u8 func_800186B0(s32 op, CameraMoveInfo *move, u32 which, CameraAccess *access);
u8 func_8001884C(s32 op, VECTOR32 *a1, VECTOR32 *a2, u32 a3, CameraAccess *access);
u8 func_800188AC(s32 op, CameraMoveInfo *move, CameraAccess *access);
u8 func_8001880C(s32 op, s32 *src, VECTOR32 *arg2, u32 count, CameraAccess *access);
u8 func_800193A8(s32 op, CameraFrameInfo *info, u32 which, CameraAccess *access);
u8 func_800194BC(s32 op, s32 *p, u8 dir, CameraAccess *access);
u8 func_8001950C(s32 op, VECTOR32 *p, u8 dir, CameraAccess *access);
u8 func_800189A8(VECTOR32 *dst, VECTOR32 *dst2, u32 count, CameraAccess *access);
u8 func_80018AB8(CameraMoveInfo *move, CameraAccess *access);
u8 func_80018B34(CameraMoveInfo *move, CameraAccess *access);
u8 func_80018B7C(CameraMoveInfo *move, CameraAccess *access);
u8 func_80019190(CameraMoveInfo *move, CameraAccess *access);
u8 func_800191C0(CameraMoveInfo *move, CameraAccess *access);
u8 func_80019218(CameraMoveInfo *move, CameraAccess *access);
void func_800197D4(Camera *cam);
void func_80019834(Camera *cam);
void func_8001967C(CameraMotion *m, VECTOR32 *value, u8 full);
void func_8001ACA0(VECTOR32 *v, void *arg1, Camera *cam);
void EGSMath_ValueModulus(s32 value, s32 mod, s32 *out);
void EGSFrameTransformPoint(VECTOR32 *point, EGSFrame *frame, s32 arg2, VECTOR32 *out);
void EGSFrameGetOrientation(EGSFrame *frame, s32 arg1, VECTOR32 *out);

extern u8 D_80113BB4[];
extern VECTOR32 gDefaultPosition;
extern VECTOR32 gDefaultAngle;
extern u8 gDefaultInitParams[];

void *memset(void *dst, s32 c, u32 n);
void EGSDisplayProjectionAngleToDistance(s32 angle, u16 *dist);
void EGSCameraCreate(s32 arg0, VECTOR32 *pos, u16 dist, s32 arg3, u8 priority, s32 arg5, s32 arg6, Camera *cam);
void EGSFrameSetRotationYXZ(EGSFrame *frame, s32 arg1, VECTOR32 *angle);
void CameraSetLeadFrame(Camera *cam, EGSFrame *frame, VECTOR32 *offset);
void CameraSetWatchFrame(Camera *cam, EGSFrame *frame, VECTOR32 *offset);
void CameraSetMode(Camera *cam, s32 mode, void *params);
u8 CameraAccessMoveInfo(Camera *cam, s32 op, s32 index, s32 arg3);
extern GOBJ_MATRIX gCameraOriginalMatrices[2];
extern s32 gCameraOriginalProjections[2];

void EGSFrameGetMatrix(EGSFrame *frame, s32 arg1, GOBJ_MATRIX *m);
void EGSFrameSetMatrix(EGSFrame *frame, s32 arg1, GOBJ_MATRIX *m);
void EGSFrameGetLocalAxes(EGSFrame *frame, s32 arg1, VECTOR32 *x, VECTOR32 *y, VECTOR32 *z);
void EGSFrameSetLocalAxes(EGSFrame *frame, s32 arg1, VECTOR32 *x, VECTOR32 *y, VECTOR32 *z);
void EGSFrameGetLocalY(EGSFrame *frame, s32 arg1, VECTOR32 *out);
void EGSFrameGetLocalZ(EGSFrame *frame, s32 arg1, VECTOR32 *out);
void EGSGeometryFIX_VectorGetAngleFromXZPlane(VECTOR32 *v, s32 *out);
void EGSGeometry_VectorGetXZAngleFromZAxis(VECTOR32 *v, s32 *out);
void EGSMath_VectorINTtoFIX(VECTOR32 *in, VECTOR32 *out);
s32 EGSMathFIXMul(s32 a, s32 b);
u8 func_8001AB9C(Camera *cam);
void func_80019594(Camera *cam);
void func_80019B20(CameraMoveInfo *move);
void EGSMiscMemcpy(void *dst, void *src, s32 size);
void EGSGeometry_ReverseTransformVector(VECTOR32 *in, GOBJ_MATRIX *m, VECTOR32 *out);

void SYSCameraInit(void) {
    D_8010F70A = 0;
}

#ifdef NON_MATCHING
/* camera id counter is reloaded (lh) instead of kept zero-extended in a3 */
u8 CameraInit(Camera *cam, EGSCamera *display, VECTOR32 *pos, VECTOR32 *angle, s32 fov, s32 arg5,
              u8 priority, s32 arg7, s32 mode, void *params, u8 activate, u8 show) {
    u8 ret = 1;
    u16 dist;
    s32 id;

    if (cam == NULL || arg5 == 0) {
        ret = 0;
    } else {
        memset(cam, 0, sizeof(Camera));
        if (pos == NULL) {
            pos = &gDefaultPosition;
        }
        if (fov < 0) {
            fov = 0x2AA;
        }
        if (priority >= 5) {
            priority = gDefaultDisplayPriority;
        }
        if (angle == NULL) {
            angle = &gDefaultAngle;
        }
        if (params == NULL) {
            params = &gDefaultInitParams;
        }
        EGSDisplayProjectionAngleToDistance(fov, &dist);
        if (display == NULL) {
            EGSCameraCreate(0, pos, dist, arg5, priority, 11, arg7, cam);
            id = D_8010F70A;
            cam->display->id = (s16)id;
            cam->display->flags |= 8;
            D_8010F70A = id + 1;
            EGSFrameSetRotationYXZ(cam->display->frame, 0, angle);
        } else {
            cam->display = display;
        }
        cam->self.frame = cam->display->frame;
        cam->self.pos.value = *pos;
        cam->self.pos.vel.vx = 0;
        cam->self.pos.vel.vy = 0;
        cam->self.pos.vel.vz = 0;
        cam->self.pos.accel.vx = 0;
        cam->self.pos.accel.vy = 0;
        cam->self.pos.accel.vz = 0;
        cam->self.angle.value = *angle;
        cam->self.angle.vel.vx = 0;
        cam->self.angle.vel.vy = 0;
        cam->self.angle.vel.vz = 0;
        cam->self.angle.accel.vx = 0;
        cam->self.angle.accel.vy = 0;
        cam->self.angle.accel.vz = 0;
        CameraSetLeadFrame(cam, NULL, NULL);
        CameraSetWatchFrame(cam, NULL, NULL);
        CameraSetMode(cam, mode, params);
        cam->access.vec.vx = fov;
        CameraAccessMoveInfo(cam, 1, 8, 0);
        cam->access.vec.vx = fov;
        cam->access.apply = 1;
        CameraAccessMoveInfo(cam, 1, 8, 1);
        if (activate == 1) {
            CameraActivate(cam, 1);
        }
        CameraDisplay(cam, show);
    }
    return ret;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/camera", CameraInit);
#endif

u8 CameraActivate(Camera *cam, u8 on) {
    u8 ret = 1;
    s32 i;

    if (on == 1) {
        CameraActivate(cam, 0);
    }
    i = 0;
    if (cam == NULL) {
        ret = 0;
    } else {
        for (; i < 2; i++) {
            if (on == 1) {
                if (gCameraList[i] == NULL) {
                    gCameraList[i] = cam;
                    break;
                }
            } else if (gCameraList[i] == cam) {
                gCameraList[i] = NULL;
                break;
            }
        }
        if (i >= 2) {
            ret = 0;
        }
    }
    return ret;
}

u8 CameraDisplay(Camera *cam, u8 on) {
    s32 ret = 1;
    EGSCamera *display;

    if (cam == NULL || (display = cam->display) == NULL) {
        ret = 0;
    } else if (on == 1) {
        display->flags |= 1;
    } else {
        display->flags &= ~1;
    }
    return ret;
}

u8 CameraActivateAndDisplay(Camera *cam, u8 on) {
    s32 ret = CameraActivate(cam, on);

    if (ret == 1) {
        ret = CameraDisplay(cam, on);
    }
    return ret;
}

#ifdef NON_MATCHING
/* return value and loaded display swap v0/v1 */
u8 CameraGetDisplay(Camera *cam, EGSCamera **out) {
    u8 ret = 0;

    if (cam != NULL) {
        ret = 1;
        if (out == NULL) {
            ret = 0;
        } else {
            *out = cam->display;
        }
    }
    return ret;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/camera", CameraGetDisplay);
#endif

u8 CameraSetDisplayRegion(Camera *cam, void *region) {
    u8 ret = 0;
    EGSCamera *display;

    if (cam != NULL && region != NULL) {
        display = cam->display;
        ret = 1;
        if (display == NULL) {
            ret = 0;
        } else {
            display->region = region;
        }
    }
    return ret;
}

u8 CameraSetDisplayPriority(Camera *cam, u8 priority) {
    u8 ret = 0;
    EGSCamera *display;

    if (cam != NULL) {
        display = cam->display;
        ret = 1;
        if (display == NULL) {
            ret = 0;
        } else {
            display->priority = priority;
        }
    }
    return ret;
}

INCLUDE_ASM("asm/main/nonmatchings/camera", CameraSetMode);

void CameraSetLeadFrame(Camera *cam, EGSFrame *frame, VECTOR32 *offset) {
    if (cam != NULL) {
        cam->lead.frame = frame;
        if (offset != NULL) {
            cam->lead.offset = *offset;
        }
        func_800197D4(cam);
        cam->lead.pos.vel.vx = 0;
        cam->lead.pos.vel.vy = 0;
        cam->lead.pos.vel.vz = 0;
        cam->lead.pos.accel.vx = 0;
        cam->lead.pos.accel.vy = 0;
        cam->lead.pos.accel.vz = 0;
        cam->lead.angle.vel.vx = 0;
        cam->lead.angle.vel.vy = 0;
        cam->lead.angle.vel.vz = 0;
        cam->lead.angle.accel.vx = 0;
        cam->lead.angle.accel.vy = 0;
        cam->lead.angle.accel.vz = 0;
    }
}

void CameraSetWatchFrame(Camera *cam, EGSFrame *frame, VECTOR32 *offset) {
    if (cam != NULL) {
        cam->watch.frame = frame;
        if (offset != NULL) {
            cam->watch.offset = *offset;
        }
        func_80019834(cam);
        cam->watch.pos.vel.vx = 0;
        cam->watch.pos.vel.vy = 0;
        cam->watch.pos.vel.vz = 0;
        cam->watch.pos.accel.vx = 0;
        cam->watch.pos.accel.vy = 0;
        cam->watch.pos.accel.vz = 0;
        cam->watch.angle.vel.vx = 0;
        cam->watch.angle.vel.vy = 0;
        cam->watch.angle.vel.vz = 0;
        cam->watch.angle.accel.vx = 0;
        cam->watch.angle.accel.vy = 0;
        cam->watch.angle.accel.vz = 0;
    }
}

void CameraSetLeadFrameOffset(Camera *cam, VECTOR32 *offset) {
    cam->lead.offset = *offset;
}

void CameraSetWatchFrameOffset(Camera *cam, VECTOR32 *offset) {
    cam->watch.offset = *offset;
}

u8 CameraSetParent(Camera *cam, Camera *parent) {
    u8 ret = 0;

    if (cam != NULL) {
        if (parent == NULL) {
            cam->self.parent = NULL;
        } else {
            cam->self.parent = parent->self.frame;
        }
        ret = 1;
    }
    return ret;
}

u8 CameraAccessMoveInfo(Camera *cam, s32 op, s32 index, s32 arg3) {
    CameraMoveInfo *move = &cam->move[index];
    u8 ret;

    if (move != NULL && move->active == 1) {
        ret = func_800186B0(op, move, arg3, &cam->access);
    } else {
        ret = 0;
    }
    return ret;
}

u8 CameraAccessFrameInfo(Camera *cam, s32 op, s32 which, s32 arg3) {
    u8 ret;

    cam->access.value = (s32)cam->self.frame;
    switch (which) {
    case 0:
        ret = func_800193A8(op, &cam->self, arg3, &cam->access);
        break;
    case 1:
        ret = func_800193A8(op, &cam->lead, arg3, &cam->access);
        break;
    case 2:
        ret = func_800193A8(op, &cam->watch, arg3, &cam->access);
        break;
    default:
        ret = 0;
        break;
    }
    return ret;
}

INCLUDE_RODATA("asm/main/nonmatchings/camera", D_8001018C);

u8 func_800186B0(s32 op, CameraMoveInfo *move, u32 which, CameraAccess *access) {
    u8 ret;

    switch (which) {
    case 0:
        ret = func_8001884C(op, &move->input.value, NULL, move->type, access);
        if (op == 1) {
            move->input.vel.vx = 0;
            move->input.vel.vy = 0;
            move->input.vel.vz = 0;
            move->input.accel.vx = 0;
            move->input.accel.vy = 0;
            move->input.accel.vz = 0;
        }
        return ret;
    case 1:
        ret = func_800188AC(op, move, access);
        break;
    case 2:
        ret = func_8001884C(op, &move->min, &move->nextMin, move->type, access);
        break;
    case 3:
        ret = func_8001884C(op, &move->max, &move->nextMax, move->type, access);
        break;
    case 4:
        ret = func_8001884C(op, &move->limit[0], &move->nextLimit[0], move->type, access);
        break;
    case 5:
        ret = func_8001884C(op, &move->limit[1], &move->nextLimit[1], move->type, access);
        break;
    case 6:
        ret = func_8001884C(op, &move->limit[2], &move->nextLimit[2], move->type, access);
        break;
    case 7:
        ret = func_8001880C(op, move->changed, NULL, move->type, access);
        break;
    default:
        ret = 0;
        break;
    }
    return ret;
}

u8 func_8001880C(s32 op, s32 *src, VECTOR32 *arg2, u32 count, CameraAccess *access) {
    u8 ret;

    if (op == 0) {
        ret = func_8001893C((VECTOR32 *)src, arg2, count, access);
    } else {
        ret = 0;
    }
    return ret;
}

u8 func_8001884C(s32 op, VECTOR32 *a1, VECTOR32 *a2, u32 a3, CameraAccess *access) {
    u8 ret;

    switch ((u32)op) {
    case 0:
        ret = func_8001893C(a1, a2, a3, access);
        break;
    case 1:
        ret = func_800189A8(a1, a2, a3, access);
        break;
    default:
        ret = 0;
        break;
    }
    return ret;
}

u8 func_800188AC(s32 op, CameraMoveInfo *move, CameraAccess *access) {
    u8 ret;

    switch ((u32)op) {
    case 0:
        ret = func_8001893C(&move->value, NULL, move->type, access);
        break;
    case 1:
        ret = func_80018AB8(move, access);
        break;
    case 2:
        ret = func_80018B34(move, access);
        break;
    case 3:
        ret = func_80018B7C(move, access);
        break;
    default:
        ret = 0;
        break;
    }
    return ret;
}

u8 func_8001893C(VECTOR32 *src, VECTOR32 *unused, u32 count, CameraAccess *access) {
    s8 ok = 1;
    u8 ret;

    switch (count) {
    case 0:
        access->vec.vx = src->vx;
        break;
    case 1:
    case 2:
        access->vec = *src;
        break;
    default:
        ok = 0;
        break;
    }
    ret = ok;
    if (ret == 1) {
        access->src = src;
    }
    return ret;
}

u8 func_800189A8(VECTOR32 *dst, VECTOR32 *dst2, u32 count, CameraAccess *access) {
    u8 ret = 1;

    switch (count) {
    case 0:
        dst->vx = access->vec.vx;
        if (access->apply == 1 && dst2 != NULL) {
            dst2->vx = access->vec.vx;
        }
        break;
    case 1:
    case 2:
        if (access->mask[0] == 1) {
            dst->vx = access->vec.vx;
        }
        if (access->mask[1] == 1) {
            dst->vy = access->vec.vy;
        }
        if (access->mask[2] == 1) {
            dst->vz = access->vec.vz;
        }
        if (access->apply == 1 && dst2 != NULL) {
            if (access->mask[0] == 1) {
                dst2->vx = access->vec.vx;
            }
            if (access->mask[1] == 1) {
                dst2->vy = access->vec.vy;
            }
            if (access->mask[2] == 1) {
                dst2->vz = access->vec.vz;
            }
        }
        break;
    default:
        ret = 0;
        break;
    }
    return ret;
}

u8 func_80018AB8(CameraMoveInfo *move, CameraAccess *access) {
    u8 ret = 1;

    switch ((u32)move->type) {
    case 0:
        func_80018BF8(move, access);
        break;
    case 1:
        func_80018C6C(move, access);
        break;
    case 2:
        func_80018E30(move, access);
        break;
    default:
        ret = 0;
        break;
    }
    return ret;
}

u8 func_80018B34(CameraMoveInfo *move, CameraAccess *access) {
    u8 ret = 1;

    if (move->type == 2) {
        func_8001902C(move, access);
    } else {
        ret = 0;
    }
    return ret;
}

u8 func_80018B7C(CameraMoveInfo *move, CameraAccess *access) {
    u8 ret = 1;

    switch ((u32)move->type) {
    case 0:
        func_80019190(move, access);
        break;
    case 1:
        func_800191C0(move, access);
        break;
    case 2:
        func_80019218(move, access);
        break;
    default:
        ret = 0;
        break;
    }
    return ret;
}

u8 func_80018BF8(CameraMoveInfo *move, CameraAccess *access) {
    s32 v = access->vec.vx;
    u8 ret = 1;

    move->value.vx = v < move->min.vx ? move->min.vx : (v > move->max.vx ? move->max.vx : v);
    move->changed[0] = 1;
    if (access->apply == 1) {
        ret = func_80019324(move, NULL);
    }
    return ret;
}

u8 func_80018C6C(CameraMoveInfo *move, CameraAccess *access) {
    u8 ret = 1;
    GOBJ_MATRIX m;
    s32 v;

    if (move->frameRef != NULL) {
        EGSFrameGetMatrix(*move->frameRef, 0, &m);
        EGSGeometry_ReverseTransformVector(&access->vec, &m, &access->vec);
    }
    if (access->mask[0] == 1) {
        v = access->vec.vx;
        move->value.vx = v < move->min.vx ? move->min.vx : (v > move->max.vx ? move->max.vx : v);
        move->changed[0] = 1;
    }
    if (access->mask[1] == 1) {
        v = access->vec.vy;
        move->value.vy = v < move->min.vy ? move->min.vy : (v > move->max.vy ? move->max.vy : v);
        move->changed[1] = 1;
    }
    if (access->mask[2] == 1) {
        v = access->vec.vz;
        move->value.vz = v < move->min.vz ? move->min.vz : (v > move->max.vz ? move->max.vz : v);
        move->changed[2] = 1;
    }
    if (move->origin != NULL) {
        access->vec.vx -= move->origin->vx;
        access->vec.vy -= move->origin->vy;
        access->vec.vz -= move->origin->vz;
    }
    if (access->apply == 1) {
        ret = func_80019324(move, access->mask);
    }
    return ret;
}

u8 func_80018E30(CameraMoveInfo *move, CameraAccess *access) {
    u8 ret = 1;
    s32 v;

    if (access->mask[0] == 1) {
        v = access->vec.vx;
        access->vec.vx = v < move->min.vx ? move->min.vx : (v > move->max.vx ? move->max.vx : v);
        if (access->vec.vx != move->value.vx) {
            func_80019270(access->vec.vx, &move->value.vx);
            func_80019270(move->input.value.vx, &move->input.value.vx);
            func_800192EC(move->input.value.vx, move->value.vx, &move->input.value.vx);
        }
        move->changed[0] = 1;
    }
    if (access->mask[1] == 1) {
        v = access->vec.vy;
        access->vec.vy = v < move->min.vy ? move->min.vy : (v > move->max.vy ? move->max.vy : v);
        if (access->vec.vy != move->value.vy) {
            func_80019270(access->vec.vy, &move->value.vy);
            func_80019270(move->input.value.vy, &move->input.value.vy);
            func_800192EC(move->input.value.vy, move->value.vy, &move->input.value.vy);
        }
        move->changed[1] = 1;
    }
    if (access->mask[2] == 1) {
        v = access->vec.vz;
        access->vec.vz = v < move->min.vz ? move->min.vz : (v > move->max.vz ? move->max.vz : v);
        if (access->vec.vz != move->value.vz) {
            func_80019270(access->vec.vz, &move->value.vz);
            func_80019270(move->input.value.vz, &move->input.value.vz);
            func_800192EC(move->input.value.vz, move->value.vz, &move->input.value.vz);
        }
        move->changed[2] = 1;
    }
    if (access->apply == 1) {
        ret = func_80019324(move, access->mask);
    }
    return ret;
}

u8 func_8001902C(CameraMoveInfo *move, CameraAccess *access) {
    GOBJ_MATRIX m;
    VECTOR32 v;

    if (move->frameRef != NULL) {
        EGSFrameGetMatrix(*move->frameRef, 0, &m);
        EGSGeometry_ReverseTransformVector(&access->vec, &m, &v);
    } else {
        v = access->vec;
    }
    if (access->mask[0] == 1) {
        EGSGeometryFIX_VectorGetAngleFromXZPlane(&v, &access->vec.vx);
    }
    if (access->mask[1] == 1) {
        if (v.vx == 0 && v.vz == 0) {
            access->vec.vy = move->value.vy;
        } else {
            EGSGeometry_VectorGetXZAngleFromZAxis(&v, &access->vec.vy);
        }
    }
    if (access->mask[2] == 1) {
        access->vec.vz = move->value.vz;
    }
    if (move->origin != NULL) {
        access->vec.vx -= move->origin->vx;
        access->vec.vy -= move->origin->vy;
        access->vec.vz -= move->origin->vz;
    }
    return func_80018E30(move, access);
}

u8 func_80019190(CameraMoveInfo *move, CameraAccess *access) {
    access->vec.vx += move->value.vx;
    return func_80018BF8(move, access);
}

u8 func_800191C0(CameraMoveInfo *move, CameraAccess *access) {
    access->vec.vx += move->value.vx;
    access->vec.vy += move->value.vy;
    access->vec.vz += move->value.vz;
    return func_80018C6C(move, access);
}

u8 func_80019218(CameraMoveInfo *move, CameraAccess *access) {
    access->vec.vx += move->value.vx;
    access->vec.vy += move->value.vy;
    access->vec.vz += move->value.vz;
    return func_80018E30(move, access);
}

void func_80019270(s32 angle, s32 *out) {
    if ((angle < 0 ? -angle : angle) > 0x1000) {
        EGSMath_ValueModulus(angle, 0x1000, &angle);
    }
    if (angle > 0x800) {
        angle -= 0x1000;
    } else if (angle < -0x800) {
        angle += 0x1000;
    }
    *out = angle;
}

void func_800192EC(s32 a, s32 b, s32 *out) {
    s32 d = a - b;

    if (d > 0x800) {
        *out = a - 0x1000;
    } else if (d < -0x800) {
        *out = a + 0x1000;
    } else {
        *out = a;
    }
}

u8 func_80019324(CameraMoveInfo *move, s32 *mask) {
    u8 ret = 1;

    if (move->type == 0) {
        move->target.vx = move->value.vx;
    } else if (mask != NULL) {
        if (mask[0] == 1) {
            move->target.vx = move->value.vx;
        }
        if (mask[1] == 1) {
            move->target.vy = move->value.vy;
        }
        if (mask[2] == 1) {
            move->target.vz = move->value.vz;
        }
    } else {
        ret = 0;
    }
    return ret;
}

u8 func_800193A8(s32 op, CameraFrameInfo *info, u32 which, CameraAccess *access) {
    u8 ret = 1;

    if (info->frame != NULL || which == 0) {
        switch (which) {
        case 0:
            func_800194BC(op, (s32 *)&info->frame, 0, access);
            break;
        case 1:
            func_8001950C(op, &info->pos.value, 0, access);
            break;
        case 2:
            func_8001950C(op, &info->pos.vel, 0, access);
            break;
        case 3:
            func_8001950C(op, &info->pos.accel, 0, access);
            break;
        case 7:
            func_8001950C(op, &info->orientation, 0, access);
            break;
        case 9:
            func_800194BC(op, (s32 *)&info->parent, 1, access);
            break;
        default:
            if ((s32)info->frame == access->value) {
                switch ((s32)which) {
                case 4:
                    func_8001950C(op, &info->angle.value, 0, access);
                    break;
                case 5:
                    func_8001950C(op, &info->angle.vel, 0, access);
                    break;
                case 6:
                    func_8001950C(op, &info->angle.accel, 0, access);
                    break;
                default:
                    ret = 0;
                    break;
                }
            } else if (which == 8) {
                func_8001950C(op, &info->offset, 0, access);
            } else {
                ret = 0;
            }
            break;
        }
    } else {
        ret = 0;
    }
    return ret;
}

u8 func_800194BC(s32 op, s32 *p, u8 dir, CameraAccess *access) {
    u8 ret = 1;

    switch (op) {
    case 0:
        access->value = *p;
        access->ptr = p;
        break;
    case 1:
        if (dir == 1) {
            *p = access->value;
        } else {
            ret = 0;
        }
        break;
    default:
        ret = 0;
        break;
    }
    return ret;
}

u8 func_8001950C(s32 op, VECTOR32 *p, u8 dir, CameraAccess *access) {
    u8 ret = 1;

    switch (op) {
    case 0:
        access->vec = *p;
        access->src = p;
        break;
    case 1:
        if (dir == 1) {
            *p = access->vec;
        } else {
            ret = 0;
        }
        break;
    default:
        ret = 0;
        break;
    }
    return ret;
}

INCLUDE_ASM("asm/main/nonmatchings/camera", func_80019594);

void func_8001967C(CameraMotion *m, VECTOR32 *value, u8 full) {
    VECTOR32 oldValue;
    VECTOR32 oldVel;

    if (!full) {
        oldValue.vx = m->value.vx;
        oldVel.vx = m->vel.vx;
        m->value.vx = value->vx;
        m->vel.vx = (m->value.vx - oldValue.vx) << 12;
        m->accel.vx = m->vel.vx - oldVel.vx;
    } else {
        oldValue = m->value;
        oldVel = m->vel;
        m->value = *value;
        m->vel.vx = m->value.vx - oldValue.vx;
        m->vel.vy = m->value.vy - oldValue.vy;
        m->vel.vz = m->value.vz - oldValue.vz;
        EGSMath_VectorINTtoFIX(&m->vel, &m->vel);
        m->accel.vx = m->vel.vx - oldVel.vx;
        m->accel.vy = m->vel.vy - oldVel.vy;
        m->accel.vz = m->vel.vz - oldVel.vz;
    }
}

void func_800197D4(Camera *cam) {
    VECTOR32 p;

    if (cam->lead.frame != NULL) {
        EGSFrameTransformPoint(&cam->lead.offset, cam->lead.frame, 0, &p);
        func_8001967C(&cam->lead.pos, &p, 1);
        EGSFrameGetOrientation(cam->lead.frame, 0, &cam->lead.orientation);
    }
}

void func_80019834(Camera *cam) {
    VECTOR32 p;

    if (cam->watch.frame != NULL) {
        EGSFrameTransformPoint(&cam->watch.offset, cam->watch.frame, 0, &p);
        func_8001967C(&cam->watch.pos, &p, 1);
        EGSFrameGetOrientation(cam->watch.frame, 0, &cam->watch.orientation);
    }
}

void func_80019894(Camera *cam) {
    VECTOR32 pos;
    VECTOR32 angle;
    VECTOR32 y;

    pos.vx = cam->self.frame->pos.vx;
    pos.vy = cam->self.frame->pos.vy;
    pos.vz = cam->self.frame->pos.vz;
    pos.vy = -pos.vy;
    func_8001967C(&cam->self.pos, &pos, 1);
    EGSFrameGetLocalZ(cam->self.frame, 0, &cam->self.orientation);
    EGSGeometryFIX_VectorGetAngleFromXZPlane(&cam->self.orientation, &angle.vx);
    EGSGeometry_VectorGetXZAngleFromZAxis(&cam->self.orientation, &angle.vy);
    angle.vz = 0;
    EGSFrameGetLocalY(cam->self.frame, 0, &y);
    if (y.vy < 0) {
        if (angle.vx >= 0) {
            angle.vx = 0x800 - angle.vx;
        } else {
            angle.vx = -0x800 - angle.vx;
        }
        if (angle.vy >= 0) {
            angle.vy -= 0x800;
        } else {
            angle.vy += 0x800;
        }
    }
    func_8001967C(&cam->self.angle, &angle, 1);
}

u8 func_800199A0(Camera *cam) {
    u8 ret = 1;
    s32 i;
    CameraMoveInfo *move;
    s32 full;

    for (i = 0; i < CAMERA_MOVE_MAX; i++) {
        move = &cam->move[i];
        if (move != NULL && move->active == 1) {
            full = move->type != 0;
            func_8001967C(&move->motion, &move->value, full);
            func_80019B20(move);
            if (!full) {
                if (move->changed[0] == 0) {
                    move->value.vx = move->target.vx;
                } else {
                    move->changed[0] = 0;
                }
            } else {
                if (move->changed[0] == 0) {
                    move->value.vx = move->target.vx;
                } else {
                    move->changed[0] = 0;
                }
                if (move->changed[1] == 0) {
                    move->value.vy = move->target.vy;
                } else {
                    move->changed[1] = 0;
                }
                if (move->changed[2] == 0) {
                    move->value.vz = move->target.vz;
                } else {
                    move->changed[2] = 0;
                }
            }
            move->min = move->nextMin;
            move->max = move->nextMax;
            EGSMiscMemcpy(move->limit, move->nextLimit, 0xC);
        }
    }
    return ret;
}

INCLUDE_ASM("asm/main/nonmatchings/camera", func_80019B20);

INCLUDE_ASM("asm/main/nonmatchings/camera", func_8001A228);

INCLUDE_ASM("asm/main/nonmatchings/camera", func_8001A3A0);

INCLUDE_ASM("asm/main/nonmatchings/camera", func_8001A660);

#ifdef NON_MATCHING
/* projection index is strength-reduced to a pointer instead of recomputed per iteration */
s32 CameraUpdateBeforeDisplay(void) {
    s32 i;

    for (i = 0; i < 2; i++) {
        if (gCameraList[i] != NULL) {
            EGSFrameGetMatrix(gCameraList[i]->display->frame, 0, &gCameraOriginalMatrices[i]);
            gCameraOriginalProjections[i] = gCameraList[i]->display->projection;
        }
    }
    for (i = 0; i < 2; i++) {
        if (gCameraList[i] != NULL) {
            func_8001AB9C(gCameraList[i]);
        }
    }
    return 1;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/camera", CameraUpdateBeforeDisplay);
#endif

s32 CameraUpdateAfterDisplay(void) {
    s32 i;

    for (i = 0; i < 2; i++) {
        if (gCameraList[i] != NULL) {
            EGSFrameSetMatrix(gCameraList[i]->display->frame, 0, &gCameraOriginalMatrices[i]);
            gCameraList[i]->display->projection = gCameraOriginalProjections[i];
        }
    }
    for (i = 0; i < 2; i++) {
        if (gCameraList[i] != NULL) {
            func_80019594(gCameraList[i]);
        }
    }
    return 1;
}

u8 func_8001AB9C(Camera *cam) {
    VECTOR32 x;
    VECTOR32 y;
    VECTOR32 z;
    EGSFrame *frame = cam->display->frame;

    EGSFrameGetLocalAxes(frame, 0, &x, &y, &z);
    x.vx = EGSMathFIXMul(x.vx, 0x6C00);
    x.vy = EGSMathFIXMul(x.vy, 0x6C00);
    x.vz = EGSMathFIXMul(x.vz, 0x6C00);
    y.vx = EGSMathFIXMul(y.vx, 0x6C00);
    y.vy = EGSMathFIXMul(y.vy, 0x6C00);
    y.vz = EGSMathFIXMul(y.vz, 0x6C00);
    EGSFrameSetLocalAxes(frame, 0, &x, &y, &z);
    cam->display->projection = (cam->display->projection << 12) / 0x6C00;
    return 1;
}

INCLUDE_ASM("asm/main/nonmatchings/camera", func_8001ACA0);

void func_8001AF98(void) {
    VECTOR32 v;
    Camera *cam = gCamera;

    if (cam->move[1].type == 0) {
        v.vx = cam->move[1].input.value.vx;
    } else {
        v = cam->move[1].input.value;
    }
    func_8001ACA0(&v, &D_80113BB4, cam);
}
