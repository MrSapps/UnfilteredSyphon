#include "common.h"
#include "game.h"
#include "gobj.h"

typedef struct PathPoint {
    s16 x, y, z, pad;
} PathPoint;

typedef struct PathData {
    /* 0x00 */ s32 numPoints;
    /* 0x04 */ PathPoint *points; /* fixed up to point at data[] */
    /* 0x08 */ PathPoint data[1];
} PathData;

u8 LevelGetPathInfo(s32 id, s32 *velocity, PathData **path);
u8 GameSysInitMoveInfo(MoveInfo **out);
void GameSysRequestMove(Gobj *gobj);
s32 EGSAnimGetFrame(GameObjectAnim *anim, EGSFrame **frame);
s32 EGSFrameGetPosition(EGSFrame *frame, s32 arg1, VECTOR32 *pos);
void EGSFrameSetPosition(EGSFrame *frame, s32 arg1, VECTOR32 *pos);
void EGSFrameSetOrientation(EGSFrame *frame, s32 arg1, VECTOR32 *dir, s32 arg3);
void EGSGeometryINT_GetDistancePointToPoint(VECTOR32 *a, VECTOR32 *b, s32 *dist);
void GTEMath_VectorNormal(VECTOR32 *in, VECTOR32 *out);
s32 EGSMathFIXMul(s32 a, s32 b);

void PathGetPoint(Gobj *gobj, s32 index, VECTOR32 *out);
u8 PathHandleEx(Gobj *gobj, s32 unused, s32 orient, u8 pingPong, s16 first, s16 last, u8 *reachedEnd);

/* MoveInfo.unk178 is an s32 (gobj.h still declares it as u8[4]) */
typedef struct PathMoveInfo {
    /* 0x000 */ u8 unk000[0x178];
    /* 0x178 */ s32 stallCount;
} PathMoveInfo;
#define PATH_STALL_COUNT(m) (((PathMoveInfo *)(m))->stallCount)

#define ABS_DIFF(a, b) ({ s32 _d = (b) > (a) ? (b) - (a) : (a) - (b); _d; })

s32 PathSetupEX(Gobj *gobj, u8 force) {
    MoveInfo *move;
    s32 velocity;
    PathData *path = NULL;

    if (gobj->move != NULL) {
        return 1;
    }
#line 136
    ASSERT(LevelGetPathInfo(gobj->id, &velocity, &path) == 1);
    if (path == NULL) {
        return 1;
    }
    if (velocity == 0) {
        return 1;
    }
    if (force != 0 && path->numPoints < 2) {
        return 1;
    }
    path->points = path->data;
#line 168
    ASSERT(GameSysInitMoveInfo(&gobj->move));
    move = gobj->move;
    move->unk174 = 0;
    PATH_STALL_COUNT(move) = 0;
    move->unk190 = velocity;
    return 1;
}

s32 PathGetVelocity(Gobj *gobj) {
    s32 velocity;
    PathData *path = NULL;

#line 283
    ASSERT(LevelGetPathInfo(gobj->id, &velocity, &path) == 1);
    return velocity;
}

s32 PathGetNumPoints(Gobj *gobj) {
    s32 velocity;
    PathData *path = NULL;

#line 306
    ASSERT(LevelGetPathInfo(gobj->id, &velocity, &path) == 1);
    return path->numPoints;
}

void PathGetPoint(Gobj *gobj, s32 index, VECTOR32 *out) {
    s32 velocity;
    PathData *path = NULL;

#line 328
    ASSERT(LevelGetPathInfo(gobj->id, &velocity, &path) == 1);
    out->vx = path->points[index].x;
    out->vy = path->points[index].y;
    out->vz = path->points[index].z;
}

void PathSetPositionToPoint(Gobj *gobj, s32 index) {
    VECTOR32 pos;

    PathGetPoint(gobj, index, &pos);
    EGSFrameSetPosition(gobj->anim->frame, 0, &pos);
}

u8 PathObjectIsAtPoint(Gobj *gobj, s32 index) {
    VECTOR32 pos;
    VECTOR32 point;

    pos.vx = gobj->anim->frame->pos.vx;
    pos.vy = gobj->anim->frame->pos.vy;
    pos.vz = gobj->anim->frame->pos.vz;
    pos.vy = -pos.vy;
    PathGetPoint(gobj, index, &point);
    return pos.vy == point.vy;
}

u8 PathObjectIsAtClosestPoint(Gobj *gobj, Gobj *target, s16 indexA, s16 indexB, u8 notAtPoint) {
    VECTOR32 pos;
    VECTOR32 pointA;
    VECTOR32 pointB;
    VECTOR32 targetPos;
    s32 distA;
    s32 distB;
    s32 targetDistA;
    s32 targetDistB;

    pos.vx = gobj->anim->frame->pos.vx;
    pos.vy = gobj->anim->frame->pos.vy;
    pos.vz = gobj->anim->frame->pos.vz;
    pos.vy = -pos.vy;
    PathGetPoint(gobj, indexA, &pointA);
    PathGetPoint(gobj, indexB, &pointB);
    if (pos.vy != pointA.vy && pos.vy != pointB.vy) {
        return notAtPoint;
    }
    targetPos.vx = target->anim->frame->pos.vx;
    targetPos.vy = target->anim->frame->pos.vy;
    targetPos.vz = target->anim->frame->pos.vz;
    targetPos.vy = -targetPos.vy;
    distA = ABS_DIFF(pos.vy, pointA.vy);
    distB = ABS_DIFF(pos.vy, pointB.vy);
    targetDistA = ABS_DIFF(targetPos.vy, pointA.vy);
    targetDistB = ABS_DIFF(targetPos.vy, pointB.vy);
    return (distA < distB) == (targetDistA < targetDistB);
}

u8 PathHandleEx(Gobj *gobj, s32 unused, s32 orient, u8 pingPong, s16 first, s16 last, u8 *reachedEnd) {
    VECTOR32 cur;
    VECTOR32 target;
    VECTOR32 dir;
    s32 velocity;
    PathData *path = NULL;
    EGSFrame *frame = NULL;
    s32 dist;
    MoveInfo *move = gobj->move;
    s32 lo;
    s32 hi;

#line 466
    ASSERT(move != NULL);
#line 470
    ASSERT(LevelGetPathInfo(gobj->id, &velocity, &path) == 1);
    velocity = move->unk190;
    if (EGSAnimGetFrame(gobj->anim, &frame) != 0) {
        return 0;
    }
    if (EGSFrameGetPosition(frame, 0, &cur) != 0) {
        return 0;
    }
    target.vx = path->points[move->unk174].x;
    target.vy = path->points[move->unk174].y;
    target.vz = path->points[move->unk174].z;
    EGSGeometryINT_GetDistancePointToPoint(&cur, &target, &dist);
    lo = (first != -1) ? first : 0;
    *reachedEnd = 0;
    /* if/else (not `hi = last; if (hi == -1)`): the join label ends cse2's
     * extended block, so the ASSERT's constant 1 is not reused for unk17C = 1 */
    if (last == -1) {
        hi = path->numPoints - 1;
    } else {
        hi = last;
    }
    if (dist <= velocity) {
        if (pingPong) {
            if (move->unk17C == 0) {
                if (++move->unk174 > hi) {
                    *reachedEnd = 1;
                    move->unk174 = hi;
                    move->unk17C = 1;
                }
            } else {
                if (--move->unk174 < lo) {
                    *reachedEnd = 1;
                    move->unk174 = lo;
                    move->unk17C = 0;
                }
            }
        } else {
            if (move->unk17C == 0) {
                if (++move->unk174 > hi) {
                    *reachedEnd = 1;
                    move->unk174 = lo;
                    PathSetPositionToPoint(gobj, lo);
                }
            } else {
                if (--move->unk174 < lo) {
                    *reachedEnd = 1;
                    move->unk174 = hi;
                    PathSetPositionToPoint(gobj, hi);
                }
            }
            if (EGSFrameGetPosition(frame, 0, &cur) != 0) {
                return 0;
            }
        }
        EGSGeometryINT_GetDistancePointToPoint(&cur, &target, &dist);
        PATH_STALL_COUNT(move) = 0;
    } else {
        PATH_STALL_COUNT(move)++;
    }
    if (*reachedEnd) {
        move->pendingMove.vx = target.vx - cur.vx;
        move->pendingMove.vy = target.vy - cur.vy;
        move->pendingMove.vz = target.vz - cur.vz;
    } else {
        dir.vx = target.vx - cur.vx;
        dir.vy = target.vy - cur.vy;
        dir.vz = target.vz - cur.vz;
        GTEMath_VectorNormal(&dir, &dir);
        move->pendingMove.vx = EGSMathFIXMul(dir.vx, (velocity >= dist ? dist : velocity));
        move->pendingMove.vy = EGSMathFIXMul(dir.vy, (velocity >= dist ? dist : velocity));
        move->pendingMove.vz = EGSMathFIXMul(dir.vz, (velocity >= dist ? dist : velocity));
    }
    if (move->pendingMove.vx != 0 || move->pendingMove.vy != 0 || move->pendingMove.vz != 0) {
        if (orient == 2) {
            EGSFrameSetOrientation(gobj->anim->frame, 0, &move->pendingMove, 0);
        }
        GameSysRequestMove(gobj);
    }
    return 1;
}

/* Register allocation only: the original gives done/reachedEnd $s2/$s3 and
 * arg1/orient $s4/$s5. */
u8 PathHandleOneWay(Gobj *gobj, s32 arg1, s32 orient, u8 *done, u8 *reachedEnd) {
    MoveInfo *move;
    u8 ret;

    if (gobj != NULL && (move = gobj->move) != NULL) {
        if (move->unk174 == PathGetNumPoints(gobj) - 1 && move->unk17C == 1) {
            ret = 1;
        } else {
            ret = PathHandleEx(gobj, arg1, orient, 1, -1, -1, reachedEnd);
            *done = *reachedEnd;
            goto end;
        }
    } else {
        ret = 0;
    }
    /* both arms identical; the test is needed for a match */
    if (reachedEnd) {
        *reachedEnd = 0;
        *done = 1;
    } else {
        *reachedEnd = 0;
        *done = 1;
    }
end:
    return ret;
}
