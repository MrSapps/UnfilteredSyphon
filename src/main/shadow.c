#include "common.h"

/* This TU's data is defined in C (docs/data_migration.md): .rdata D_80013360 and
 * D_80013414; mShadowRequestList (.sbss) and the pools (.bss) are commons. */

typedef struct EGSListNode {
    /* 0x00 */ void *data;
    /* 0x04 */ struct EGSListNode *prev;
    /* 0x08 */ struct EGSListNode *next;
} EGSListNode;

typedef struct ShadowRect {
    s16 x, y, w, h;
} ShadowRect;

/* One shadow polygon (0x30 bytes); data is nonzero while it is in the display list */
typedef struct ShadowPoly {
    /* 0x00 */ s32 data;
    /* 0x04 */ s32 z;
    /* 0x08 */ u8 unk08[7];
    /* 0x0F */ u8 flags;
    /* 0x10 */ u8 unk10[0x20];
} ShadowPoly;

#define SHADOW_POOL_SIZE 6
#define SHADOW_POLYS 4

typedef struct Shadow {
    /* 0x00 */ ShadowPoly polys[SHADOW_POLYS];
} Shadow; /* size 0xC0 */

typedef struct ShadowModel {
    /* 0x00 */ u8 unk00[0x28];
    /* 0x28 */ u32 flags;
} ShadowModel;

typedef struct ShadowBox {
    /* 0x00 */ u8 unk00[4];
    /* 0x04 */ s16 x;
    /* 0x06 */ s16 y;
    /* 0x08 */ s16 z;
} ShadowBox;

typedef struct ShadowFrame {
    /* 0x00 */ s16 m[3][3];
    /* 0x12 */ s16 pad;
    /* 0x14 */ s32 t[3];
} ShadowFrame;

/* View of GameObjectAnim */
typedef struct ShadowAnim {
    /* 0x00 */ u8 unk00[8];
    /* 0x08 */ u32 flags; /* 0x10: no shadow */
    /* 0x0C */ ShadowFrame *frame;
    /* 0x10 */ ShadowModel *model;
    /* 0x14 */ u8 unk14[4];
    /* 0x18 */ ShadowFrame **frames; /* per-bone frames */
    /* 0x1C */ ShadowBox *box;
} ShadowAnim;

typedef struct ShadowGobj {
    /* 0x00 */ u8 unk00[8];
    /* 0x08 */ ShadowAnim *anim;
} ShadowGobj;

EGSListNode *mShadowRequestList; /* common, $gp */
SDATA_LOCAL(mShadowRequestList)
extern Shadow mShadowPool[SHADOW_POOL_SIZE];
extern u16 mShadowTexture[32][16]; /* 16x32 15-bit shadow blob, built by ShadowInit */
extern void **gCamera;

void EGSListAdd(EGSListNode **list, void *data);
void EGSListRemove(EGSListNode **list, EGSListNode *node);
void EGSDisplayDeletePoly(void *camera, void *poly);
void LoadImage(ShadowRect *rect, void *data);
void EGSMath_VectorModulus(void *v, s32 *out);
typedef struct ShadowVec {
    /* 0x00 */ s32 vx;
    /* 0x04 */ s32 vy;
    /* 0x08 */ s32 vz;
    /* 0x0C */ s32 pad;
} ShadowVec;

/* Shadow caster state hanging off the object at 0x1A8 */
typedef struct ShadowCaster {
    /* 0x000 */ u8 unk000[0x108];
    /* 0x108 */ u8 matrix[0x14];
    /* 0x11C */ s32 pos[3];
} ShadowCaster;

typedef struct ShadowObjState {
    /* 0x000 */ ShadowVec pos;
    /* 0x010 */ u8 unk010[0x11C];
    /* 0x12C */ s32 groundY;
    /* 0x130 */ u8 unk130[0x78];
    /* 0x1A8 */ ShadowCaster *caster;
} ShadowObjState;

typedef struct ShadowObj {
    /* 0x00 */ u8 unk00[8];
    /* 0x08 */ ShadowAnim *anim;
    /* 0x0C */ ShadowObjState *state;
    /* 0x10 */ u8 unk10[0x12];
    /* 0x22 */ u8 npcState; /* 1, 2: characters */
} ShadowObj; /* GameObjectRuntime */

#define SHADOW_MAX(a, b) ((a) > (b) ? (a) : (b))
#define SHADOW_MIN(a, b) ((a) > (b) ? (b) : (a))
#define SHADOW_MAX3(a, b, c) ((a) < SHADOW_MAX2(b, c) ? (s16)SHADOW_MAX2(b, c) : (a))
#define SHADOW_MAX2(a, b) ((a) < (b) ? (b) : (a))
#define SHADOW_ABS(x) ((x) < 0 ? -(x) : (x))
/* 20.12 fixed point to integer, rounding towards zero */
#define SHADOW_FIX(x) ((x) >= 0 ? (x) >> 12 : -(-(x) >> 12))

extern s32 gSniperMode;
extern void *gSniperCam;

void EGSDisplaySetupTransformLocalToScreen(void *camera, void *matrix);
void EGSDisplayTransformWorldToScreen(ShadowVec *in, ShadowVec *out);
u8 func_800918C0(ShadowObj *obj, ShadowVec *center, ShadowVec *points, s32 count);
u8 func_800915DC(ShadowObj *obj, u8 *rgb);
static void func_800916CC(Shadow *shadow, ShadowVec *center, ShadowVec *points, s32 count, u8 *rgb);

/* Texture description handed to EGSDisplayBuildPoly4FT */
typedef struct ShadowTex {
    /* 0x00 */ u8 unk00[0xC];
    /* 0x0C */ s32 mode;
    /* 0x10 */ ShadowRect rect;
    /* 0x18 */ u8 unk18[4];
    /* 0x1C */ s16 clutX;
    /* 0x1E */ s16 clutY;
} ShadowTex;

void EGSDisplayBuildPoly4FT(Shadow *shadow, s32 xy0, s32 xy1, s32 xy2, s32 xy3, s32 color, ShadowTex *tex, s32 arg7);
void EGSDisplayAddPoly(void *camera, void *poly);
static u8 func_80092468(ShadowObj *obj, Shadow *shadow, s32 count);

typedef struct ShadowMatrix {
    /* 0x00 */ s16 m[3][3];
    /* 0x12 */ s16 pad;
    /* 0x14 */ s32 t[3];
} ShadowMatrix;

/* Plane through point with the given normal */
typedef struct ShadowPlane {
    /* 0x00 */ ShadowVec point;
    /* 0x10 */ ShadowVec normal;
} ShadowPlane;

#define setVector(v, x, y, z) ((v)->vx = (x), (v)->vy = (y), (v)->vz = (z))

void EGSGeometryINT_PointProjectOnPlaneAlongY(ShadowVec *p, ShadowPlane *plane, u8 *ok, s32 *y);
int EGSMathFIX_VectorScaleToLength(ShadowVec *v, s32 length, ShadowVec *out);
static const ShadowVec D_80013414; /* {0, 0x1000, 0}: up, shared with func_800918C0 */

typedef struct ShadowSVec {
    s16 vx, vy, vz, pad;
} ShadowSVec;

void TransposeMatrix(ShadowMatrix *in, ShadowMatrix *out);
void SetRotMatrix(ShadowMatrix *m);
void SetTransMatrix(ShadowMatrix *m);
void ApplyRotMatrix(ShadowSVec *in, ShadowVec *out);
void GTEMath_VectorNormal(ShadowVec *in, ShadowVec *out);

#define SHADOW_HALF(x) ((x) >= 0 ? (x) >> 1 : -(-(x) >> 1))
/* Mirror a footprint corner through the centre (y kept) */
#define SHADOW_MIRROR(dst, src) ((dst)->vx = -(src)->vx, (dst)->vy = (src)->vy, (dst)->vz = -(src)->vz)

/* The MAX of the two abs values must stay a COND_EXPR (fold then distributes the multiply and the
 * FIX test into both arms, and v is reloaded per copy); SHADOW_MIN(SHADOW_MAX(a, 0), 31) keeps
 * the clamp as a MAX_EXPR inside an unfolded COND_EXPR. */
void ShadowInit(void) {
    ShadowVec v;
    s32 len;
    s32 i;
    s32 j;
    s32 a;

    mShadowRequestList = NULL;
    for (i = 0; i < SHADOW_POOL_SIZE; i++) {
        for (j = SHADOW_POLYS - 1; j >= 0; j--) {
            mShadowPool[i].polys[j].data = 0;
        }
    }
    for (i = 0; i < 32; i++) {
        for (j = 0; j < 16; j++) {
            v.vx = (j - 7) << 9;
            v.vy = (i - 15) << 8;
            v.vz = 0;
            EGSMath_VectorModulus(&v, &len);
            a = SHADOW_FIX((0x1000 - len) * 155 / 4);
            a = SHADOW_FIX(a * (0x1000 - (__builtin_abs(v.vx) < __builtin_abs(v.vy) ? (s32)__builtin_abs(v.vy)
                                                                                     : (s32)__builtin_abs(v.vx)) / 4));
            a = SHADOW_MIN(SHADOW_MAX(a, 0), 31);
            mShadowTexture[i][j] = 0x8000 | a << 10 | a << 5 | a;
        }
    }
}

u8 func_800915DC(ShadowObj *obj, u8 *rgb) {
    ShadowBox *box;
    s32 level;
    s32 c;

    if (obj->anim->model->flags & 0x400000) {
        box = obj->anim->box;
        level = SHADOW_MAX3(box->x, box->y, box->z);
        level /= 16;
        c = SHADOW_MAX(SHADOW_MIN(level, 255), 0);
        rgb[2] = c;
        rgb[1] = c;
        rgb[0] = c;
        return 1;
    }
    return 0;
}

static void func_800916CC(Shadow *shadow, ShadowVec *center, ShadowVec *points, s32 count, u8 *rgb) {
    ShadowTex tex;
    ShadowVec unused; /* allocated but never used */
    s32 z;
    s32 i;

    if (count == 4) {
        i = 0;
        if (points[0].vz > 0 && points[1].vz > 0 && points[2].vz > 0 && points[3].vz > 0) {
            tex.mode = 2;
            tex.rect.x = 0x300;
            tex.rect.y = 0xA0;
            tex.rect.w = 0x10;
            tex.rect.h = 0x20;
            tex.clutX = 0x300;
            tex.clutY = 0x1E3;
            EGSDisplayBuildPoly4FT(shadow, points[0].vx - (points[0].vy << 16), points[1].vx - (points[1].vy << 16),
                                   points[3].vx - (points[3].vy << 16), points[2].vx - (points[2].vy << 16),
                                   ((rgb[2] >> 2) << 16) + ((rgb[1] >> 2) << 8) + (rgb[0] >> 2), &tex, 2);
            shadow->polys[0].flags |= 2;
            z = (center->vz - 0x80) * 3 / 16;
            if (z < 0) {
                z = 0;
            }
            shadow->polys[0].z = z;
            if (shadow->polys[0].data == 0) {
                EGSDisplayAddPoly(*gCamera, &shadow->polys[0]);
            }
            i = 1;
        }
        for (; i < SHADOW_POLYS; i++) {
            if (shadow->polys[i].data != 0) {
                EGSDisplayDeletePoly(*gCamera, &shadow->polys[i]);
            }
        }
    }
}

static const s32 D_80013360[4] = { 1, 1, 1, 0 }; /* starts every game TU's .rdata */

#ifdef NON_MATCHING
/* stack layout: the original overlays the bone tables with the half/centre vectors
 * r6H: orig frame is 0x168 (ours 0x1C0) and it reloads obj->anim at every use (no anim local;
 * `#define anim (obj->anim)` reproduces those loads, 139). 2.7.2 never reuses block-scope slots,
 * but inline-function frames are temp slots that later inlines reuse: the bone loop and the
 * half/centre math were probably static inlines. */
u8 func_800918C0(ShadowObj *obj, ShadowVec *center, ShadowVec *points, s32 count) {
    ShadowAnim *anim;
    s32 i;

    if ((u32)(obj->npcState - 1) < 2) {
        anim = obj->anim;
        if ((anim->model->flags & 0x400000) && !(anim->flags & 0x10) && obj->state != NULL) {
            ShadowVec bounds[2] = { { -32, 0, -16 }, { 32, 0, 16 } };
            ShadowMatrix rot;
            s32 bones[5] = { 0, 7, 1, 9, 3 };
            ShadowVec radii[5] = { { 32, 0, 16 }, { 12, 0, 16 }, { 12, 0, 16 }, { 3, 0, 3 }, { 3, 0, 3 } };
            ShadowVec pos;
            ShadowMatrix inv = { 0 };
            ShadowVec fwd;
            ShadowVec halfX;
            ShadowVec halfZ;
            ShadowVec centerX;
            ShadowVec centerZ;
            ShadowVec extX;
            ShadowVec extZ;
            s32 hx;
            s32 hz;

            pos = obj->state->pos;
            {
                ShadowVec up = D_80013414;
                ShadowVec side;

                fwd.vx = anim->frame->m[0][2];
                fwd.vy = anim->frame->m[1][2];
                fwd.vy = -fwd.vy;
                fwd.vz = anim->frame->m[2][2];
                if (fwd.vy != 0) {
                    fwd.vy = 0;
                    GTEMath_VectorNormal(&fwd, &fwd);
                }
                side.vx = fwd.vz;
                side.vy = 0;
                side.vz = -fwd.vx;
                rot.m[0][0] = side.vx;
                rot.m[1][0] = side.vy;
                rot.m[2][0] = side.vz;
                rot.m[0][1] = up.vx;
                rot.m[1][1] = up.vy;
                rot.m[2][1] = up.vz;
                rot.m[0][2] = fwd.vx;
                rot.m[1][2] = fwd.vy;
                rot.m[2][2] = fwd.vz;
            }
            TransposeMatrix(&rot, &inv);
            inv.t[0] = rot.t[0];
            inv.t[1] = rot.t[1];
            inv.t[2] = rot.t[2];
            SetRotMatrix(&inv);
            SetTransMatrix(&inv);
            for (i = 0; i < 5; i++) {
                ShadowVec p;
                ShadowSVec sv;
                ShadowVec lo;
                ShadowVec hi;

                p.vx = anim->frames[bones[i]]->t[0];
                p.vy = anim->frames[bones[i]]->t[1];
                p.vz = anim->frames[bones[i]]->t[2];
                p.vy = -p.vy;
                p.vx -= pos.vx;
                p.vy -= pos.vy;
                p.vz -= pos.vz;
                sv.vx = p.vx;
                sv.vy = p.vy;
                sv.vz = p.vz;
                ApplyRotMatrix(&sv, &p);
                hi.vx = p.vx + radii[i].vx;
                hi.vz = p.vz + radii[i].vz;
                lo.vx = p.vx - radii[i].vx;
                lo.vz = p.vz - radii[i].vz;
                if (bounds[1].vx < hi.vx) {
                    bounds[1].vx = hi.vx;
                }
                if (lo.vx < bounds[0].vx) {
                    bounds[0].vx = lo.vx;
                }
                if (bounds[1].vz < hi.vz) {
                    bounds[1].vz = hi.vz;
                }
                if (lo.vz < bounds[0].vz) {
                    bounds[0].vz = lo.vz;
                }
            }
            {
                ShadowVec axisX;
                ShadowVec axisZ;

                axisX.vx = rot.m[0][0];
                axisX.vy = rot.m[1][0];
                axisX.vz = rot.m[2][0];
                axisZ.vx = rot.m[0][2];
                axisZ.vy = rot.m[1][2];
                axisZ.vz = rot.m[2][2];
                hx = (bounds[1].vx - bounds[0].vx) / 2;
                extX.vx = axisX.vx * hx;
                extX.vy = axisX.vy;
                extX.vz = axisX.vz * hx;
                extX.vx = SHADOW_FIX(extX.vx);
                extX.vy = SHADOW_FIX(extX.vy);
                extX.vz = SHADOW_FIX(extX.vz);
                hz = (bounds[1].vz - bounds[0].vz) / 2;
                extZ.vx = axisZ.vx * hz;
                extZ.vy = axisZ.vy;
                extZ.vz = axisZ.vz * hz;
                extZ.vx = SHADOW_FIX(extZ.vx);
                extZ.vy = SHADOW_FIX(extZ.vy);
                extZ.vz = SHADOW_FIX(extZ.vz);
                hx = (bounds[1].vx + bounds[0].vx) / 2;
                centerX.vx = axisX.vx * hx;
                centerX.vy = axisX.vy;
                centerX.vz = axisX.vz * hx;
                centerX.vx = SHADOW_FIX(centerX.vx);
                centerX.vy = SHADOW_FIX(centerX.vy);
                centerX.vz = SHADOW_FIX(centerX.vz);
                hz = (bounds[1].vz + bounds[0].vz) / 2;
                centerZ.vx = axisZ.vx * hz;
                centerZ.vy = axisZ.vy;
                centerZ.vz = axisZ.vz * hz;
                centerZ.vx = SHADOW_FIX(centerZ.vx);
                centerZ.vy = SHADOW_FIX(centerZ.vy);
                centerZ.vz = SHADOW_FIX(centerZ.vz);
            }
            center->vx = centerX.vx + centerZ.vx;
            center->vy = centerX.vy + centerZ.vy;
            center->vz = centerX.vz + centerZ.vz;
            halfX.vx = SHADOW_HALF(extX.vx);
            halfX.vy = extX.vy;
            halfX.vz = SHADOW_HALF(extX.vz);
            halfZ.vx = SHADOW_HALF(extZ.vx);
            halfZ.vy = extZ.vy;
            halfZ.vz = SHADOW_HALF(extZ.vz);
            switch (count) {
                case 4:
                    points[0].vx = extZ.vx - extX.vx;
                    points[0].vy = extZ.vy - extX.vy;
                    points[0].vz = extZ.vz - extX.vz;
                    points[1].vx = extZ.vx + extX.vx;
                    points[1].vy = extZ.vy + extX.vy;
                    points[1].vz = extZ.vz + extX.vz;
                    SHADOW_MIRROR(&points[2], &points[0]);
                    SHADOW_MIRROR(&points[3], &points[1]);
                    break;
                case 6:
                    points[0].vx = extZ.vx - halfX.vx;
                    points[0].vy = extZ.vy - halfX.vy;
                    points[0].vz = extZ.vz - halfX.vz;
                    points[1].vx = extZ.vx + halfX.vx;
                    points[1].vy = extZ.vy + halfX.vy;
                    points[1].vz = extZ.vz + halfX.vz;
                    points[2] = extX;
                    SHADOW_MIRROR(&points[3], &points[0]);
                    SHADOW_MIRROR(&points[4], &points[1]);
                    SHADOW_MIRROR(&points[5], &points[2]);
                    break;
                case 8:
                    points[0].vx = extZ.vx - halfX.vx;
                    points[0].vy = extZ.vy - halfX.vy;
                    points[0].vz = extZ.vz - halfX.vz;
                    points[1].vx = extZ.vx + halfX.vx;
                    points[1].vy = extZ.vy + halfX.vy;
                    points[1].vz = extZ.vz + halfX.vz;
                    points[2].vx = extX.vx + halfZ.vx;
                    points[2].vy = extX.vy + halfZ.vy;
                    points[2].vz = extX.vz + halfZ.vz;
                    points[3].vx = extX.vx - halfZ.vx;
                    points[3].vy = extX.vy - halfZ.vy;
                    points[3].vz = extX.vz - halfZ.vz;
                    SHADOW_MIRROR(&points[4], &points[0]);
                    SHADOW_MIRROR(&points[5], &points[1]);
                    SHADOW_MIRROR(&points[6], &points[2]);
                    SHADOW_MIRROR(&points[7], &points[3]);
                    break;
                default:
                    return 0;
            }
            for (i = 0; i < count; i++) {
                points[i].vx += center->vx;
                points[i].vy += center->vy;
                points[i].vz += center->vz;
            }
            return 1;
        }
    }
    return 0;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/shadow", func_800918C0);
#endif

static u8 func_80092468(ShadowObj *obj, Shadow *shadow, s32 count) {
    ShadowVec center;
    ShadowVec pos;
    ShadowVec screenCenter;
    ShadowVec points[8];
    ShadowVec screenPoints[8];
    u8 rgb[4];
    ShadowCaster *caster;
    void *camera;
    s32 i;

    if (obj != NULL && obj->state != NULL && obj->state->caster != NULL && func_800918C0(obj, &center, points, count)) {
        if (!func_800915DC(obj, rgb)) {
            return 0;
        }
        pos = obj->state->pos;
        pos.vy = obj->state->groundY;
        caster = obj->state->caster;
        caster->pos[0] = pos.vx;
        caster->pos[1] = pos.vy;
        caster->pos[2] = pos.vz;
        if (gSniperMode != 2 || (camera = gSniperCam) == NULL) {
            camera = *gCamera;
        }
        EGSDisplaySetupTransformLocalToScreen(camera, caster->matrix);
        EGSDisplayTransformWorldToScreen(&center, &screenCenter);
        if (screenCenter.vz > 0) {
            for (i = 0; i < count; i++) {
                EGSDisplayTransformWorldToScreen(&points[i], &screenPoints[i]);
            }
            func_800916CC(shadow, &screenCenter, screenPoints, count, rgb);
            return 1;
        }
    }
    return 0;
}

static void func_80092620(ShadowPoly *polys) {
    s32 i;

    for (i = 0; i < SHADOW_POLYS; i++) {
        if (polys[i].data != 0) {
            EGSDisplayDeletePoly(*gCamera, &polys[i]);
        }
    }
}

void ShadowProcess(void) {
    ShadowRect rect;
    EGSListNode *node;
    EGSListNode *next;
    Shadow *shadow;
    s32 count;

    rect.x = 0x300;
    rect.y = 0xA0;
    rect.w = 0x10;
    rect.h = 0x20;
    LoadImage(&rect, mShadowTexture);
    node = mShadowRequestList;
    count = 0;
    if (node != NULL) {
        shadow = mShadowPool;
        do {
            next = node->next;
            if (count < SHADOW_POOL_SIZE && func_80092468(node->data, shadow, SHADOW_POLYS)) {
                shadow++;
                count++;
            }
            EGSListRemove(&mShadowRequestList, node);
            node = next;
        } while (node != NULL);
    }
    for (; count < SHADOW_POOL_SIZE; count++) {
        func_80092620(mShadowPool[count].polys);
    }
}

static const ShadowVec D_80013414 = { 0, 0x1000, 0 }; /* up; emitted after func_800918C0's templates */

u8 ShadowCalculateProjectionMatrix(ShadowVec *normal, ShadowMatrix *out) {
    ShadowVec ax = { 0x1000, 0, 0 };
    ShadowVec ay = D_80013414;
    ShadowVec az = { 0, 0, 0x1000 };
    ShadowVec t = { 0, 0, 0 };
    static const ShadowPlane ground = { { 0, 0, 0 }, { 0, 0, 0 } };
    ShadowPlane plane;
    u8 ok;
    s32 y0;
    s32 y1;

    ax.vx /= 32;
    ax.vy /= 32;
    ax.vz /= 32;
    plane = ground;
    plane.normal = *normal;
    EGSGeometryINT_PointProjectOnPlaneAlongY(&ax, &plane, &ok, &y0);
    setVector(&ax, ax.vx, y0, ax.vz);
    ax.vx <<= 5;
    ax.vy <<= 5;
    ax.vz <<= 5;
    if (ok) {
        az.vx /= 32;
        az.vy /= 32;
        az.vz /= 32;
        plane = ground;
        plane.normal = *normal;
        EGSGeometryINT_PointProjectOnPlaneAlongY(&az, &plane, &ok, &y1);
        setVector(&az, az.vx, y1, az.vz);
        az.vx <<= 5;
        az.vy <<= 5;
        az.vz <<= 5;
        if (ok) {
            if (ax.vx < -0x7E66 || ax.vx > 0x7E66 || ax.vy < -0x7E66 || ax.vy > 0x7E66 || ax.vz < -0x7E66 || ax.vz > 0x7E66) {
                EGSMathFIX_VectorScaleToLength(&ax, 0x7E66, &ax);
            }
            if (az.vx < -0x7E66 || az.vx > 0x7E66 || az.vy < -0x7E66 || az.vy > 0x7E66 || az.vz < -0x7E66 || az.vz > 0x7E66) {
                EGSMathFIX_VectorScaleToLength(&az, 0x7E66, &az);
            }
            out->m[0][0] = ax.vx;
            out->m[1][0] = ax.vy;
            out->m[2][0] = ax.vz;
            out->m[0][1] = ay.vx;
            out->m[1][1] = ay.vy;
            out->m[2][1] = ay.vz;
            out->m[0][2] = az.vx;
            out->m[1][2] = az.vy;
            out->m[2][2] = az.vz;
            out->t[0] = t.vx;
            out->t[1] = t.vy;
            out->t[2] = t.vz;
        }
    }
    return ok;
}

void ShadowRequestDraw(void *gobj) {
    EGSListAdd(&mShadowRequestList, gobj);
}
