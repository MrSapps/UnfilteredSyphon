#include "common.h"
#include "gobj.h"

typedef struct PhysVector {
    s32 vx, vy, vz;
} PhysVector;

/* The physics view of MoveInfo (gobj.h) */
typedef struct PhysMove {
    /* 0x000 */ u8 unk000[0x70];
    /* 0x070 */ PhysVector velocity;
    /* 0x07C */ u8 unk07C[0x85];
    /* 0x101 */ u8 enabled;
    /* 0x102 */ u8 unk102[2];
    /* 0x104 */ s32 mass; /* 0x1000 = 1.0 */
    /* 0x108 */ u8 unk108[0xA4];
    /* 0x1AC */ void *physics;
} PhysMove;

typedef struct PhysGobj {
    /* 0x00 */ u8 unk00[0xC];
    /* 0x0C */ PhysMove *move;
} PhysGobj;

/* .data (docs/data_migration.md) */
VECTOR32 gPhysicsGravityAcceleration = { 0, -10518, 0, 0 };

void *EGSMemMalloc(u32 size);
s32 EGSMathFIXMul(s32 a, s32 b);
s32 EGSMathFIXDiv(s32 a, s32 b);
void EGSMath_VectorModulus(PhysVector *v, s32 *out);
void EGSMath_VectorNegate(PhysVector *v, PhysVector *out);
void EGSGeometry_VectorProjectOnPlane(PhysVector *v, PhysVector *normal, PhysVector *out);

#define VECTOR_IS_ZERO(v) ((v)->vx == 0 && (v)->vy == 0 && (v)->vz == 0)

s32 PhysicsGobjBasicInit(PhysGobj *gobj, s32 mass, u8 enabled) {
    if (gobj == NULL || gobj->move == NULL) {
        return 0;
    }
    gobj->move->mass = mass;
    gobj->move->enabled = enabled;
    return 1;
}

s32 PhysicsGobjInit(PhysGobj *gobj, s32 mass, u8 enabled) {
    PhysMove *move;

    if (gobj == NULL) {
        return 0;
    }
    move = gobj->move;
    if (move == NULL) {
        return 0;
    }
    if (move->physics == NULL) {
        if ((move->physics = EGSMemMalloc(8)) == NULL) {
            return 0;
        }
    }
    PhysicsGobjBasicInit(gobj, mass, enabled);
    return 1;
}

s32 PhysicsGobjUpdate(PhysGobj *gobj) {
    if (gobj == NULL || gobj->move == NULL) {
        return 0;
    }
    if (gobj->move->physics == NULL) {
        return 0;
    }
    if (gobj->move->enabled == 1) {
        gobj->move->velocity.vx += gPhysicsGravityAcceleration.vx;
        gobj->move->velocity.vy += gPhysicsGravityAcceleration.vy;
        gobj->move->velocity.vz += gPhysicsGravityAcceleration.vz;
    }
    return 1;
}

s32 PhysicsGobjRegisterForce(PhysGobj *gobj, PhysVector *force) {
    PhysVector accel;
    s32 *v; /* accel as an array: v[0] is a plain scalar store */
    s32 mass;

    if (gobj == NULL || gobj->move == NULL) {
        return 0;
    }
    mass = gobj->move->mass;
    if (mass > 0 && mass != 0x1000) {
        v = &accel.vx;
        v[0] = EGSMathFIXDiv(force->vx, mass);
        v[1] = EGSMathFIXDiv(force->vy, mass);
        v[2] = EGSMathFIXDiv(force->vz, mass);
        gobj->move->velocity.vx += accel.vx;
        gobj->move->velocity.vy += accel.vy;
        gobj->move->velocity.vz += accel.vz;
    } else {
        gobj->move->velocity.vx += force->vx;
        gobj->move->velocity.vy += force->vy;
        gobj->move->velocity.vz += force->vz;
    }
    return 1;
}

/* Case 2's deceleration lives in memory (reloaded and negated before each call): it is passed
 * by address to the scaling helper. */
static inline void iFrictionScale(PhysVector *vel, s32 *decel, PhysVector *out) {
    out->vx = EGSMathFIXMul(vel->vx, -*decel);
    out->vy = EGSMathFIXMul(vel->vy, -*decel);
    out->vz = EGSMathFIXMul(vel->vz, -*decel);
}

s32 PhysicsApplyFrictionToVelocity(s32 friction, PhysVector *vel, s32 mode, PhysVector *normal, PhysVector *out) {
    s32 speed;
    s32 decel;
    s32 *o;

    if (friction <= 0 || (normal != NULL && VECTOR_IS_ZERO(normal) == 1) ||
        (vel != NULL && VECTOR_IS_ZERO(vel) == 1)) {
        out->vx = 0;
        out->vy = 0;
        out->vz = 0;
    } else {
        switch (mode) {
            case 0:
                EGSMath_VectorModulus(vel, &speed);
                if (friction < speed) {
                    friction = -friction;
                    out->vx = EGSMathFIXMul(vel->vx, friction);
                    out->vy = EGSMathFIXMul(vel->vy, friction);
                    out->vz = EGSMathFIXMul(vel->vz, friction);
                    /* through a plain s32 pointer: the stores may alias speed, so each
                     * out->v? store stays ahead of the next speed reload */
                    o = &out->vx;
                    o[0] = EGSMathFIXDiv(o[0], speed);
                    o[1] = EGSMathFIXDiv(o[1], speed);
                    o[2] = EGSMathFIXDiv(o[2], speed);
                } else {
                    EGSMath_VectorNegate(vel, out);
                }
                break;
            case 1:
                if (friction < 0x1000) {
                    friction = -friction;
                    out->vx = EGSMathFIXMul(vel->vx, friction);
                    out->vy = EGSMathFIXMul(vel->vy, friction);
                    out->vz = EGSMathFIXMul(vel->vz, friction);
                } else {
                    EGSMath_VectorNegate(vel, out);
                }
                break;
            case 2:
                EGSMath_VectorModulus(vel, &speed);
                decel = EGSMathFIXMul(friction, speed);
                if (decel < 0x1000) {
                    iFrictionScale(vel, &decel, out);
                } else {
                    EGSMath_VectorNegate(vel, out);
                }
                break;
    }
    /* never leave a component exactly zero */
    if (out->vx == 0) {
        out->vx = vel->vx > 0 ? -1 : vel->vx < 0;
    }
    if (out->vy == 0) {
        out->vy = vel->vy > 0 ? -1 : vel->vy < 0;
    }
    if (out->vz == 0) {
        out->vz = vel->vz > 0 ? -1 : vel->vz < 0;
    }
    if (normal != NULL) {
        EGSGeometry_VectorProjectOnPlane(out, normal, out);
    }
    }
    return 1;
}
