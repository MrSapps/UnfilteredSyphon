#include "common.h"
#include "system.h"
#include "gameobj.h"
#include "gobj.h"
#include "scratch.h"

/* friendly.c's data is defined in C (docs/data_migration.md) */

/* Bit-string access that tolerates an unaligned base. */
#define BITS_WORD(arr, n, w, b)           \
    {                                     \
        u32 _m;                           \
        w = (u32 *)(arr);                 \
        b = (n);                          \
        _m = (u32)w & 3;                  \
        if (_m) {                         \
            b = (n) + _m * 8;             \
            w = (u32 *)((u32)w & ~3);     \
        }                                 \
    }

/* SPAD_LEAVE followed by a use of $sp: the assembler put a load-delay nop there. */
#define SPAD_LEAVE_NOP() __asm__ volatile("lui $29, 0x1F80\n\tlw $29, 0($29)\n\tnop")

/* A collision polygon handed to the friendly-collision code: up to 4 vertices + normal. */
typedef struct FriendlyPoly {
    /* 0x00 */ s32 count;
    /* 0x04 */ VECTOR32 v[4];
    /* 0x44 */ s32 nx, ny, nz;
    /* 0x50 */ s32 unk50;
} FriendlyPoly;

typedef struct VEC3 {
    s32 vx, vy, vz;
} VEC3;

typedef struct FakeVertex {
    s16 x, y, z, pad;
} FakeVertex;

/* Collision face as walked by the collision code (0x24 bytes). */
typedef struct CollisionFace {
    /* 0x00 */ s32 count;
    /* 0x04 */ u8 *flags;
    /* 0x08 */ s16 *normalPtr;
    /* 0x0C */ s16 normal[3];
    /* 0x12 */ s16 pad;
    /* 0x14 */ FakeVertex *verts[4];
} CollisionFace;

/* Temporary face linked into a model's face list (0x2C bytes). */
typedef struct FakeFace {
    /* 0x00 */ struct FakeFace *prev;
    /* 0x04 */ struct FakeFace *next;
    /* 0x08 */ CollisionFace face;
} FakeFace;

typedef struct FakeFacePool {
    /* 0x000 */ s32 count;
    /* 0x004 */ FakeFace faces[30];
    /* 0x52C */ FakeVertex verts[30][4];
    /* 0x8EC */ u8 flags[30];
} FakeFacePool;

extern FakeFacePool mCollisionFakeFacePool;

/* The pool's faces+verts and flags as the reset loop addresses them. */
typedef struct FakeFaceBlock {
    /* 0x000 */ FakeFace faces[30];
    /* 0x528 */ FakeVertex verts[30][4];
} FakeFaceBlock;
extern FakeFaceBlock D_8012CEC4; /* == mCollisionFakeFacePool.faces */
extern u8 D_8012D7AC[30];        /* == mCollisionFakeFacePool.flags */
#define COLLISION_BOX_MAX 15
extern FakeVertex mCollisionBoxVertices[COLLISION_BOX_MAX][8];
extern FakeFace mCollisionBoxFaces[COLLISION_BOX_MAX][6];
extern u8 mCollisionBoxFaceFlags[COLLISION_BOX_MAX][6];
static s32 D_8010FAB4 = 0;          /* .sdata: number of vertices in D_801212D8 */
static FakeVertex *D_801212D8[400]; /* .bss: vertices whose y is currently negated */

void CollisionClearFakeFaces(void);

/* A face found under/around an object (fields seen so far). */
typedef struct FoundFace {
    /* 0x00 */ VECTOR32 p0;
    /* 0x10 */ VECTOR32 p1;
    /* 0x20 */ VECTOR32 unk20;
    /* 0x30 */ VECTOR32 normal;
    /* 0x40 */ s32 midY;
    /* 0x44 */ s32 facing; /* normal . collector direction */
    /* 0x48 */ u8 left;      /* probe point inside the face's left edge */
    /* 0x49 */ u8 right;
    /* 0x4A */ u8 unk4A[2];
    /* 0x4C */ u32 kind;
} FoundFace;

/* One class of collected faces (floor/wall/...). */
typedef struct FaceGroup {
    /* 0x00 */ s32 flagBit; /* bit of move->flags1A4 set while best != NULL */
    /* 0x04 */ s32 leftBit;
    /* 0x08 */ s32 rightBit;
    /* 0x0C */ s32 facingMin;
    /* 0x10 */ s32 facingMax;
    /* 0x14 */ s32 distMin;
    /* 0x18 */ s32 distMax;
    /* 0x1C */ s32 count;
    /* 0x20 */ FoundFace *faces[16];
    /* 0x60 */ FoundFace *best;
} FaceGroup; /* size 0x64 */

/* Faces collected around an object (one global instance, D_80106AEC). */
typedef struct FaceCollect {
    /* 0x000 */ FaceGroup groups[5];
    /* 0x1F4 */ VECTOR32 pos;
    /* 0x204 */ s32 dir[3];
    /* 0x210 */ u8 unk210[4];
    /* 0x214 */ s32 vel[3]; /* in whole units (>> 12) */
    /* 0x220 */ u8 unk220[4];
    /* 0x224 */ VECTOR32 ref; /* pos with vy = reference y */
    /* 0x234 */ VECTOR32 unk234;
    /* 0x244 */ VECTOR32 unk244;
    /* 0x254 */ FoundFace found[32];
    /* 0xC54 */ s32 foundCount;
} FaceCollect;

static void func_80087E0C(Gobj *gobj, struct FaceCollect *c, struct FaceGroup *g);
u8 JoystickGobjShouldCollideWithEdges(Gobj *gobj);
s32 EGSMathFIXMul(s32 a, s32 b);
void CollisionGobjAddFakeFace(Gobj *gobj, FriendlyPoly *poly, u8 flags);

/* View of MoveInfo with the fields used here. */
typedef struct FriendlyMove {
    /* 0x000 */ u8 unk000[0x60];
    /* 0x060 */ VECTOR32 unk60;
    /* 0x070 */ VECTOR32 unk70;
    /* 0x080 */ u8 unk080[0x8C];
    /* 0x10C */ VECTOR32 unk10C;
    /* 0x11C */ VECTOR32 unk11C;
    /* 0x12C */ s32 floorY; /* 0x80000001: none */
    /* 0x130 */ u8 unk130[0x10];
    /* 0x140 */ VECTOR32 groundA;
    /* 0x150 */ VECTOR32 groundB;
    /* 0x160 */ u8 unk160[0x44];
    /* 0x1A4 */ u32 flags1A4;
    /* 0x1A8 */ struct FriendlyEdgeInfo *edgeInfo;
    /* 0x1AC */ u8 unk1AC[4];
    /* 0x1B0 */ void *ride; /* FriendlyRide */
} FriendlyMove;

typedef struct FriendlyRideTarget {
    /* 0x00 */ s32 bone;
} FriendlyRideTarget;

/* Collision box of a ride (fields seen so far). */
typedef struct FriendlyRideBox {
    /* 0x00 */ u8 unk00[0x14];
    /* 0x14 */ s32 height;
    /* 0x18 */ u8 unk18[0x24];
    /* 0x3C */ VEC3 offset;
} FriendlyRideBox;

/* What an object is standing on / attached to. */
typedef struct FriendlyRide {
    /* 0x00 */ s32 type;
    /* 0x04 */ u8 unk04[4];
    /* 0x08 */ FriendlyRideTarget *target;
    /* 0x0C */ u8 unk0C[0x1C];
    /* 0x28 */ FriendlyRideBox *box;
    /* 0x2C */ u8 unk2C[0xC];
    /* 0x38 */ struct GroundFace *ground;
} FriendlyRide;

/* The face an object stands on, as returned by func_80087088. */
typedef struct GroundFace {
    /* 0x00 */ s32 type;
    /* 0x04 */ u8 unk04[4];
    /* 0x08 */ s16 *normal;
    /* 0x0C */ u8 unk0C[2];
    /* 0x0E */ s16 unkE;
    /* 0x10 */ u8 unk10[4];
    /* 0x14 */ s16 *point;
} GroundFace;


typedef struct FriendlyEdgeInfo {
    /* 0x00 */ u8 unk00[0x7C];
    /* 0x7C */ VECTOR32 a;
    /* 0x8C */ VECTOR32 b;
    /* 0x9C */ VECTOR32 c;
    /* 0xAC */ u8 unkAC[0x58];
    /* 0x104 */ u8 shadow;
    /* 0x105 */ u8 unk105[3];
    /* 0x108 */ u8 shadowMatrix[1];
} FriendlyEdgeInfo;

/* .data: the face groups' parameters are initialised, the rest is zero */
static FaceCollect D_80106AEC = {
    {
        { 3, 0, 0, 0x800, 0x7FFFFFFF, -0x30, 0x10 },
        { 4, 0, 0, (s32)0x80000001, -0x800, -0x10, 0x30 },
        { 5, 12, 13, (s32)0x80000001, -0x800, -0x10, 0x50 },
        { 6, 14, 15, (s32)0x80000001, -0x800, -0x33, 0x60 },
        { 7, 16, 17, (s32)0x80000001, -0x800, -0x1B, 0x50 },
    },
};
#ifndef _PSYQ
/* fields of D_80106AEC the asm functions address by splat label */
__asm__(".set D_80106CE0, D_80106AEC+0x1F4\n"
        ".set D_80106CF0, D_80106AEC+0x204\n"
        ".set D_80106CF4, D_80106AEC+0x208\n"
        ".set D_80106CF8, D_80106AEC+0x20C\n"
        ".set D_80106D00, D_80106AEC+0x214\n"
        ".set D_80106D04, D_80106AEC+0x218\n"
        ".set D_80106D08, D_80106AEC+0x21C\n"
        ".set D_80106D10, D_80106AEC+0x224\n"
        ".set D_80106D14, D_80106AEC+0x228\n"
        ".set D_80106D20, D_80106AEC+0x234\n"
        ".set D_80106D24, D_80106AEC+0x238\n"
        ".set D_80106D28, D_80106AEC+0x23C\n"
        ".set D_80106D30, D_80106AEC+0x244\n"
        ".set D_80106D34, D_80106AEC+0x248\n"
        ".set D_80106D38, D_80106AEC+0x24C\n");
/* D_80107740 (.foundCount) is an absolute symbol in config/main.syms.ld: lib_gte's
 * rcos reaches its table through the same folded %hi */
#endif
static void func_80088270();
s32 FriendlyGobjGetReferenceY(Gobj *gobj, u8 useMove);
s32 GobjCollideLoop(Gobj *gobj);
static void func_80086D24(Gobj *gobj, u8 unnegate);
static s32 func_80086E94(Gobj *gobj);
static s32 func_80087088(Gobj *gobj, u8 *found, struct GroundFace **face, s32 *floorY, s32 *dist);
s32 EGSGeometryINT_PointProjectOnFacePlaneAlongY(VECTOR32 *p, FriendlyPoly *face, u8 *ok, s32 *y);
u8 ShadowCalculateProjectionMatrix(VECTOR32 *normal, void *out);
static void func_800874C0(Gobj *gobj);
static void func_80088EE4(Gobj *gobj);
static void func_80089544(Gobj *gobj);
void FriendlyGobjNegateFaceListYs(Gobj *gobj);
void FriendlyGobjUnNegateFaceListYs(Gobj *gobj);
void FriendlyGobjCollectGameObjectBoxFaces(Gobj *gobj);

/* One collision test handed to CobjLoop (0x8C bytes). */
typedef struct CobjTest {
    /* 0x00 */ void (*handler)();
    /* 0x04 */ u8 unk04[4];
    /* 0x08 */ VECTOR32 pos;
    /* 0x18 */ Gobj *gobj;
    /* 0x1C */ s32 radius;
    /* 0x20 */ u8 unk20[0x6C];
} CobjTest;

typedef struct CobjRequest {
    /* 0x00 */ s16 sector;
    /* 0x04 */ s32 flags;
    /* 0x08 */ u8 unk08;
    /* 0x0C */ CobjTest tests[2];
} CobjRequest;

extern u16 gGabeSector;
void CobjLoop(CobjRequest *req);
void CobjHandleHotBoxXZ();
void CobjHandleHotBox();

/* GameObjectAnim with its collision face list. */
typedef struct FriendlyAnim {
    /* 0x00 */ u8 unk00[8];
    /* 0x08 */ u8 flag8;
    /* 0x09 */ u8 unk09[2];
    /* 0x0B */ u8 flagB;
    /* 0x0C */ GOBJ_MATRIX *frame;
    /* 0x10 */ GameObjectModel *model;
    /* 0x14 */ u8 unk14[4];
    /* 0x18 */ GOBJ_MATRIX **frames; /* per-bone world matrices */
    /* 0x1C */ u8 unk1C[0xC];
    /* 0x28 */ FakeFace *faceList;
} FriendlyAnim;

/* The camera's anim block, wrapped in a temporary gobj to reuse the face-list code. */
extern FriendlyAnim gCameraAnim;

#ifdef NON_MATCHING
/* loop strength reduction/regalloc differs (original keeps per-side face offsets in t0/t1/t2) */
/* Build the six faces (two per axis) of every collision box. */
void FriendlyInit(void) {
    FakeVertex *quad[4];
    s32 box;
    s32 axis;
    s32 side;

    for (box = 0; box < COLLISION_BOX_MAX; box++) {
        for (axis = 0; axis < 3; axis++) {
            for (side = 0; side < 2; side++) {
                switch (axis) {
                    case 0:
                        quad[0] = &mCollisionBoxVertices[box][side * 4];
                        quad[1] = &mCollisionBoxVertices[box][side * 4 + 1];
                        quad[2] = &mCollisionBoxVertices[box][side * 4 + 3];
                        quad[3] = &mCollisionBoxVertices[box][side * 4 + 2];
                        break;
                    case 1:
                        quad[0] = &mCollisionBoxVertices[box][side * 2];
                        quad[1] = &mCollisionBoxVertices[box][side * 2 + 4];
                        quad[2] = &mCollisionBoxVertices[box][side * 2 + 5];
                        quad[3] = &mCollisionBoxVertices[box][side * 2 + 1];
                        break;
                    case 2:
                        quad[0] = &mCollisionBoxVertices[box][side];
                        quad[1] = &mCollisionBoxVertices[box][side + 2];
                        quad[2] = &mCollisionBoxVertices[box][side + 6];
                        quad[3] = &mCollisionBoxVertices[box][side + 4];
                        break;
                }
                mCollisionBoxFaces[box][axis * 2 + side].face.count = 4;
                switch (side) {
                    case 0:
                        mCollisionBoxFaces[box][axis * 2].face.verts[0] = quad[0];
                        mCollisionBoxFaces[box][axis * 2].face.verts[1] = quad[1];
                        mCollisionBoxFaces[box][axis * 2].face.verts[2] = quad[2];
                        mCollisionBoxFaces[box][axis * 2].face.verts[3] = quad[3];
                        break;
                    case 1:
                        mCollisionBoxFaces[box][axis * 2 + 1].face.verts[0] = quad[0];
                        mCollisionBoxFaces[box][axis * 2 + 1].face.verts[1] = quad[3];
                        mCollisionBoxFaces[box][axis * 2 + 1].face.verts[2] = quad[2];
                        mCollisionBoxFaces[box][axis * 2 + 1].face.verts[3] = quad[1];
                        break;
                }
                mCollisionBoxFaces[box][axis * 2 + side].face.flags = &mCollisionBoxFaceFlags[box][axis * 2 + side];
                mCollisionBoxFaces[box][axis * 2 + side].face.normalPtr =
                    mCollisionBoxFaces[box][axis * 2 + side].face.normal;
            }
        }
    }
    func_80086B24();
}
#else
INCLUDE_ASM("asm/main/nonmatchings/friendly", FriendlyInit);
#endif

void func_80086B24(void) {
    s32 i, j;
    FakeFace *f;

    for (i = 0; i < 30; i++) {
        f = &D_8012CEC4.faces[i];
        f->face.count = 0;
        for (j = 0; j < 4; j++) {
            /* two address forms: 2D index for the pointer, byte offset i*32 + j*8 for the clears */
            f->face.verts[j] = &D_8012CEC4.verts[i][j];
            (*(FakeVertex *)((u8 *)D_8012CEC4.verts + (i * 32 + j * 8))).x = 0;
            (*(FakeVertex *)((u8 *)D_8012CEC4.verts + (i * 32 + j * 8))).y = 0;
            (*(FakeVertex *)((u8 *)D_8012CEC4.verts + (i * 32 + j * 8))).z = 0;
        }
        f->face.normalPtr = f->face.normal;
        f->face.normal[0] = 0;
        f->face.normal[1] = 0;
        f->face.normal[2] = 0;
        f->face.flags = &D_8012D7AC[i];
        D_8012D7AC[i] = 0;
        f->prev = NULL;
        f->next = NULL;
    }
    CollisionClearFakeFaces();
}

void CollisionClearFakeFaces(void) {
    mCollisionFakeFacePool.count = 0;
}

void CollisionGobjAddFakeFace(Gobj *gobj, FriendlyPoly *poly, u8 flags) {
    FakeFace *f;
    CollisionFace *face;
    int i;

    if (mCollisionFakeFacePool.count < 30) {
        f = &mCollisionFakeFacePool.faces[mCollisionFakeFacePool.count];
        face = &f->face;
        face->count = poly->count;
        for (i = 0; i < poly->count; i++) {
            face->verts[i]->x = poly->v[i].vx;
            face->verts[i]->y = poly->v[i].vy;
            face->verts[i]->z = poly->v[i].vz;
        }
        face->normal[0] = poly->nx;
        face->normal[1] = poly->ny;
        face->normal[2] = poly->nz;
        face->normalPtr = face->normal;
        *face->flags = flags;
        f->prev = NULL;
        f->next = ((FriendlyAnim *)gobj->anim)->faceList;
        ((FriendlyAnim *)gobj->anim)->faceList = f;
        if (f->next != NULL) {
            f->next->prev = f;
        }
        mCollisionFakeFacePool.count++;
    }
}

/* Negate (or restore) the y of every vertex and normal in gobj's face list. */
static void func_80086D24(Gobj *gobj, u8 unnegate) {
    FakeFace *f;
    FakeVertex *v;
    int i;
    int j;
    int n;

    f = ((FriendlyAnim *)gobj->anim)->faceList;
    if (unnegate == 1) {
        for (i = 0; i < D_8010FAB4; i++) {
            D_801212D8[i]->y = -D_801212D8[i]->y;
        }
        D_8010FAB4 = 0;
        while (f != NULL) {
            f->face.normalPtr[1] = -f->face.normalPtr[1];
            f = f->next;
        }
    } else {
        while (f != NULL) {
            n = f->face.count;
            for (i = 0; i < n; i++) {
                v = f->face.verts[i];
                for (j = 0; j < D_8010FAB4; j++) {
                    if (D_801212D8[j] == v) {
                        break;
                    }
                }
                if (j >= D_8010FAB4) {
                    if (D_8010FAB4 >= 400) {
                        return;
                    }
                    v->y = -v->y;
                    D_801212D8[D_8010FAB4++] = v;
                }
            }
            f->face.normalPtr[1] = -f->face.normalPtr[1];
            f = f->next;
        }
    }
}

static s32 func_80086E94(Gobj *gobj) {
    VECTOR32 pos;
    CobjRequest req;

    pos = *(VECTOR32 *)gobj->move;
    req.sector = gGabeSector;
    req.flags = 4;
    req.unk08 = 0;
    req.tests[0].handler = CobjHandleHotBoxXZ;
    req.tests[0].pos = pos;
    req.tests[0].radius = 0x2580;
    req.tests[0].gobj = gobj;
    req.tests[1].handler = CobjHandleHotBox;
    req.tests[1].pos = pos;
    req.tests[1].gobj = gobj;
    req.tests[1].radius = 0xA00;
    CobjLoop(&req);
    return 0;
}

static s32 func_80086F60(Gobj *gobj) {
    SCRATCH_CALL1(GobjCollideLoop, gobj);
    return 0;
}

/* Is pt inside the (convex, XZ) polygon? */
static s32 func_80086FAC(VECTOR32 *pt, FriendlyPoly *poly, u8 *inside) {
    VECTOR32 edge;
    VECTOR32 rel;
    struct {
        s32 vx, vy;
    } dot;
    int i;
    int next;

    *inside = 1;
    for (i = 0; i < poly->count; i++) {
        if (i + 1 == poly->count) {
            next = 0;
        } else {
            next = i + 1;
        }
        edge.vx = poly->v[i].vz - poly->v[next].vz;
        edge.vz = poly->v[next].vx - poly->v[i].vx;
        rel.vx = pt->vx - poly->v[i].vx;
        rel.vz = pt->vz - poly->v[i].vz;
        dot.vx = rel.vx * edge.vx + rel.vz * edge.vz;
        if (dot.vx > 0) {
            *inside = 0;
            break;
        }
    }
    return 1;
}

#ifdef NON_MATCHING
/* the original hoists &poly.v (spilled, 8-byte larger frame) and keeps &poly in s0; regalloc follows */
/*
 * Find the highest face of gobj's face list under its feet (within 0x40 above
 * them); *face, *floorY and *dist (feet - floor) describe it.
 */
static s32 func_80087088(Gobj *gobj, u8 *found, GroundFace **face, s32 *floorY, s32 *dist) {
    VECTOR32 pos;
    FriendlyPoly poly;
    u8 near;
    u8 hit;
    s32 y;
    FakeFace *f;
    CollisionFace *cf;
    u32 *words;
    s32 bit;
    s32 top;
    s32 d;
    s32 i;

    f = ((FriendlyAnim *)gobj->anim)->faceList;
    pos = ((FriendlyMove *)gobj->move)->unk10C;
    pos.vy -= 9;
    pos = *(VECTOR32 *)gobj->move;
    pos.vx += ((FriendlyRide *)((FriendlyMove *)gobj->move)->ride)->box->offset.vx;
    pos.vy += ((FriendlyRide *)((FriendlyMove *)gobj->move)->ride)->box->offset.vy;
    pos.vz += ((FriendlyRide *)((FriendlyMove *)gobj->move)->ride)->box->offset.vz;
    pos.vy -= ((FriendlyRide *)((FriendlyMove *)gobj->move)->ride)->box->height;
    ((FriendlyMove *)gobj->move)->floorY = 0x80000001;
    *found = 0;
    *face = NULL;
    *dist = 0x7FFFFFFF;
    top = pos.vy + 0x40;
    for (; f != NULL; f = f->next) {
        cf = &f->face;
        BITS_WORD(&f->face.pad, 0, words, bit);
        if ((*words & (1 << bit)) || cf->normalPtr[1] <= 0x800) {
            continue;
        }
        switch (cf->count) {
            case 3:
                near = 0;
                if (!(top < cf->verts[0]->y && top < cf->verts[1]->y && top < cf->verts[2]->y)) {
                    near = 1;
                }
                break;
            case 4:
                near = 0;
                if (!(top < cf->verts[0]->y && top < cf->verts[1]->y && top < cf->verts[2]->y &&
                      top < cf->verts[3]->y)) {
                    near = 1;
                }
                break;
            default:
                near = 0;
                for (i = 0; i < cf->count; i++) {
                    if (top >= cf->verts[i]->y) {
                        near = 1;
                        break;
                    }
                }
                break;
        }
        if (near) {
            poly.count = cf->count;
            for (i = 0; i < poly.count; i++) {
                poly.v[i].vx = cf->verts[i]->x;
                poly.v[i].vy = cf->verts[i]->y;
                poly.v[i].vz = cf->verts[i]->z;
            }
            func_80086FAC(&pos, &poly, &near);
            if (near) {
                EGSGeometryINT_PointProjectOnFacePlaneAlongY(&pos, &poly, &hit, &y);
                d = pos.vy - y;
                if (hit && d >= -0x3F && d < *dist) {
                    ((FriendlyMove *)gobj->move)->floorY = y;
                    *found = 1;
                    *face = (GroundFace *)cf;
                    *floorY = y;
                    *dist = d;
                }
            }
        }
    }
    return 1;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/friendly", func_80087088);
#endif

static void func_800874C0(Gobj *gobj) {
    u8 found;
    GroundFace *face;
    s32 unk20;
    s32 dist;

    if (gobj == NULL || gobj->move == NULL || ((FriendlyMove *)gobj->move)->ride == NULL) {
        return;
    }
    if (((FriendlyRide *)((FriendlyMove *)gobj->move)->ride)->type == 1) {
        ((FriendlyMove *)gobj->move)->flags1A4 &= ~0x100;
        ((FriendlyMove *)gobj->move)->flags1A4 &= ~0x80000;
    } else {
        if (((FriendlyMove *)gobj->move)->flags1A4 & 4) {
            ((FriendlyMove *)gobj->move)->flags1A4 &= ~0x100;
        }
        if (((FriendlyMove *)gobj->move)->flags1A4 & 0x100000) {
            ((FriendlyMove *)gobj->move)->flags1A4 &= ~0x80000;
        }
    }
    func_80087088(gobj, &found, &face, &unk20, &dist);
    ((FriendlyRide *)((FriendlyMove *)gobj->move)->ride)->ground = face;
    if (face != NULL && face->type == 3) {
        ((FriendlyAnim *)gobj->anim)->flagB |= 0x80;
    } else {
        ((FriendlyAnim *)gobj->anim)->flagB &= 0x7F;
    }
    if (face != NULL) {
        VECTOR32 point;
        VECTOR32 normal;
        FriendlyMove *move;

        point.vx = face->point[0];
        point.vy = face->point[1];
        point.vz = face->point[2];
        normal.vx = face->normal[0];
        normal.vy = face->normal[1];
        normal.vz = face->normal[2];
        move = (FriendlyMove *)gobj->move;
        move->groundA = point;
        move->groundB = normal;
        ((FriendlyMove *)gobj->move)->edgeInfo->shadow = ShadowCalculateProjectionMatrix(
            &normal, ((FriendlyMove *)gobj->move)->edgeInfo->shadowMatrix);
    } else {
        ((FriendlyMove *)gobj->move)->edgeInfo->shadow = 0;
    }
    if (found && dist <= 0x40 && ((FriendlyRide *)((FriendlyMove *)gobj->move)->ride)->type != 1 &&
        !(((FriendlyMove *)gobj->move)->flags1A4 & 0x100000) && face->unkE < 0xB50) {
        ((FriendlyMove *)gobj->move)->flags1A4 &= ~0x100;
        ((FriendlyMove *)gobj->move)->flags1A4 |= 0x80000;
    } else if (!found || dist > 0x40) {
        if (((FriendlyRide *)((FriendlyMove *)gobj->move)->ride)->type != 1 &&
            !(((FriendlyMove *)gobj->move)->flags1A4 & 4)) {
            ((FriendlyMove *)gobj->move)->flags1A4 |= 0x100;
            ((FriendlyMove *)gobj->move)->flags1A4 &= ~0x80000;
        }
    }
}

s32 FriendlyGobjGetReferenceY(Gobj *gobj, u8 useMove) {
    VECTOR32 a;
    VECTOR32 b;
    VECTOR32 c;
    FriendlyRide *ride;
    FriendlyAnim *anim;
    s32 floorY;

    ride = NULL;
    if (gobj != NULL && gobj->move != NULL) {
        if (((FriendlyMove *)gobj->move)->ride != NULL) {
            ride = ((FriendlyMove *)gobj->move)->ride;
        }
    }
    if (gobj->cas->active == 8) {
        if ((u8)(gobj->npcState - 1) < 2 &&
            ((anim = (FriendlyAnim *)gobj->anim)->model->flags & GOBJ_MODEL_FLAG_400000) &&
            !(anim->flag8 & 0x10) && gobj->move != NULL && ride != NULL && ride->type == 1 &&
            ride->target != NULL && (ride->target->bone == 8 || ride->target->bone == 2)) {
            a.vx = anim->frames[ride->target->bone]->t[0];
            a.vy = ((FriendlyAnim *)gobj->anim)->frames[ride->target->bone]->t[1];
            a.vz = ((FriendlyAnim *)gobj->anim)->frames[ride->target->bone]->t[2];
            a.vy = -a.vy;
            return a.vy - 0xD0;
        }
        b.vx = ((FriendlyAnim *)gobj->anim)->frames[8]->t[0];
        b.vy = ((FriendlyAnim *)gobj->anim)->frames[8]->t[1];
        b.vz = ((FriendlyAnim *)gobj->anim)->frames[8]->t[2];
        b.vy = -b.vy;
        c.vx = ((FriendlyAnim *)gobj->anim)->frames[8]->t[0];
        c.vy = ((FriendlyAnim *)gobj->anim)->frames[8]->t[1];
        c.vz = ((FriendlyAnim *)gobj->anim)->frames[8]->t[2];
        c.vy = -c.vy;
        if (b.vy < c.vy) {
            return c.vy - 0xD0;
        }
        return b.vy - 0xD0;
    }
    floorY = ((FriendlyMove *)gobj->move)->floorY;
    a = ((FriendlyMove *)gobj->move)->unk10C;
    a.vy -= 9;
    if ((gobj->cas->flags & 0x9000) || gobj->cas->active == 4 || gobj->cas->active == 10 ||
        (gobj->cas->flags & 0x800000) || (((FriendlyMove *)gobj->move)->flags1A4 & 0x100) ||
        floorY == 0x80000001 || a.vy - floorY > 0x40 || (ride != NULL && ride->type == 1)) {
        if (useMove) {
            return a.vy;
        }
        b = *(VECTOR32 *)gobj->move;
        return b.vy - 0x69;
    }
    return floorY;
}

static void func_80087B8C(void *arg0, FaceCollect *c, FoundFace *face) {
    s32 ref;
    s32 mid;
    s32 sum;
    s32 lo;
    s32 hi;

    ref = c->ref.vy;
    sum = face->p0.vy + face->p1.vy;
    if (sum >= 0) {
        mid = (sum >> 1) - ref;
    } else {
        mid = -(-sum >> 1) - ref;
    }
    if (face->p0.vy < face->p1.vy) {
        lo = face->p0.vy - ref;
        hi = face->p1.vy - ref;
    } else {
        lo = face->p1.vy - ref;
        hi = face->p0.vy - ref;
    }
    if (lo <= 0x20 && hi >= -0x80 && c->groups[0].count < 16) {
        c->groups[0].faces[c->groups[0].count++] = face;
        face->midY = mid;
    }
}

/* Shares its prologue with func_80087B8C. */
#define FACE_LIST_ADD(list)                     \
    if ((list).count >= 16) {                   \
        return;                                 \
    }                                           \
    (list).faces[(list).count++] = face;        \
    face->midY = mid;

static void func_80087C28(void *arg0, FaceCollect *c, FoundFace *face) {
    s32 ref;
    s32 mid;
    s32 sum;
    s32 lo;
    s32 hi;

    ref = c->ref.vy;
    sum = face->p0.vy + face->p1.vy;
    if (sum >= 0) {
        mid = (sum >> 1) - ref;
    } else {
        mid = -(-sum >> 1) - ref;
    }
    if (face->p0.vy < face->p1.vy) {
        lo = face->p0.vy - ref;
        hi = face->p1.vy - ref;
    } else {
        lo = face->p1.vy - ref;
        hi = face->p0.vy - ref;
    }
    if (face->kind >= 3 && ((c->vel[1] >= 0 && hi >= 200 && lo <= c->vel[1] + 0xD8) ||
                            (hi >= c->vel[1] + 200 && lo <= 0xD8))) {
        FACE_LIST_ADD(c->groups[3]);
    } else if (face->kind >= 2 && lo < 0x50) {
        if (lo <= 0x20 && hi >= -0x80) {
            FACE_LIST_ADD(c->groups[0]);
        }
        FACE_LIST_ADD(c->groups[1]);
    } else if (face->kind >= 2 && hi >= 0x50 && lo <= 0xC8) {
        FACE_LIST_ADD(c->groups[2]);
    } else if (face->kind >= 3 && hi >= 0xD8 && lo <= 0x1A0) {
        FACE_LIST_ADD(c->groups[4]);
    }
}

/* FriendlyDot for the facing test. */
static inline void FriendlyFacing(s32 *a, s32 *b, s32 *out) {
    VECTOR32 unused; /* never used, but gives the inline a 16-byte frame: that is what puts dist after it on the stack */
    *out = EGSMathFIXMul(a[0], b[0]) + EGSMathFIXMul(a[1], b[1]) + EGSMathFIXMul(a[2], b[2]);
}

/* *out = a . b (4.12 fixed point). */
static inline void FriendlyDot(s32 *a, s32 *b, s32 *out) {
    *out = EGSMathFIXMul(a[0], b[0]) + EGSMathFIXMul(a[1], b[1]) + EGSMathFIXMul(a[2], b[2]);
}

/* *out = signed distance of pos from the plane through p with normal n. */
static inline void FriendlyPlaneDist(VECTOR32 *pos, VECTOR32 *p, VECTOR32 *n, s32 *out) {
    VECTOR32 rel;

    rel.vx = pos->vx - p->vx;
    rel.vy = pos->vy - p->vy;
    rel.vz = pos->vz - p->vz;
    FriendlyDot(&rel.vx, &n->vx, out);
}

/* Is pos (XZ) on the inner side of the edge a-b of a face with normal n? */
static inline void FriendlyInsideEdge(VECTOR32 *a, VECTOR32 *b, VECTOR32 *pos, VECTOR32 *n, u8 *out) {
    struct {
        s32 edge, rel;
    } cross;

    cross.edge = -n->vx * b->vz - -n->vz * b->vx;
    cross.rel = (pos->vz - a->vz) * -n->vx - (pos->vx - a->vx) * -n->vz;
    if (cross.edge > 0) {
        *out = cross.rel >= 0 && !(cross.edge < cross.rel);
    } else if (cross.edge < 0) {
        *out = cross.rel <= 0 && !(cross.rel < cross.edge);
    } else {
        *out = 0;
    }
}

#define FACE_LIST_DROP(g, i)                              \
    {                                                     \
        FoundFace *_t = (g)->faces[i];                    \
        (g)->faces[i] = (g)->faces[(g)->count - 1];       \
        (g)->faces[(g)->count - 1] = _t;                  \
        (g)->count--;                                     \
    }

/*
 * Keep the faces of g that face c's direction and lie within g's distance
 * range of c->pos; g->best becomes the most facing one.
 */
static void func_80087E0C(Gobj *gobj, FaceCollect *c, FaceGroup *g) {
    VECTOR32 p;
    VECTOR32 n;
    VECTOR32 a;
    VECTOR32 b;
    VECTOR32 pos;
    VECTOR32 n2;
    s32 facing;
    s32 dist;
    u8 inside;
    FoundFace *face;
    FoundFace *best;
    s32 bestFacing;
    s32 i;

    bestFacing = 0x7FFFFFFF;
    best = NULL;
    inside = 0;
    for (i = 0; i < g->count;) {
        face = g->faces[i];
        FriendlyFacing(c->dir, &face->normal.vx, &facing);
        if (facing < g->facingMin || facing > g->facingMax) {
            FACE_LIST_DROP(g, i);
            continue;
        }
        face->facing = facing;
        p = face->p0;
        n = face->normal;
        FriendlyPlaneDist(&c->pos, &p, &n, &dist);
        if (dist < g->distMin || dist > g->distMax) {
            FACE_LIST_DROP(g, i);
            continue;
        }
        i++;
    }
    for (i = 0; i < g->count;) {
        face = g->faces[i];
        a = face->p0;
        b = face->unk20;
        pos = c->pos;
        n2 = face->normal;
        FriendlyInsideEdge(&a, &b, &pos, &n2, &inside);
        if (!inside) {
            FACE_LIST_DROP(g, i);
            continue;
        }
        i++;
    }
    for (i = 0; i < g->count; i++) {
        face = g->faces[i];
        if (best == NULL || face->facing < bestFacing) {
            best = face;
            bestFacing = face->facing;
        }
    }
    if (best != NULL) {
        ((FriendlyMove *)gobj->move)->flags1A4 |= 1 << g->flagBit;
        g->best = best;
    } else {
        ((FriendlyMove *)gobj->move)->flags1A4 &= ~(1 << g->flagBit);
    }
}

/*
 * Like func_80087E0C, but also work out whether c's position is inside each
 * face's left/right edge (from the two probe points when Gabe is climbing,
 * else from points one unit along the face); faces inside neither are dropped.
 * The best face's left/right flags (or those of faces nearly parallel to it)
 * set g's leftBit/rightBit in move->flags1A4.
 */
static void func_80088270(Gobj *gobj, FaceCollect *c, FaceGroup *g) {
    VECTOR32 p;
    VECTOR32 n;
    VECTOR32 a;
    VECTOR32 b;
    VECTOR32 pos;
    VECTOR32 n2;
    VECTOR32 dir;
    s32 facing;
    s32 dist;
    s32 len;
    s32 dot;
    FoundFace *face;
    FoundFace *best;
    s32 bestFacing;
    u8 left;
    u8 right;
    s32 i;

    bestFacing = 0x7FFFFFFF;
    left = 0;
    i = 0;
    right = 0;
    best = NULL;
    while (i < g->count) {
        face = g->faces[i];
        FriendlyFacing(c->dir, &face->normal.vx, &facing);
        if (facing < g->facingMin || facing > g->facingMax) {
            FACE_LIST_DROP(g, i);
            continue;
        }
        face->facing = facing;
        p = face->p0;
        n = face->normal;
        FriendlyPlaneDist(&c->pos, &p, &n, &dist);
        if (dist < g->distMin || dist > g->distMax) {
            FACE_LIST_DROP(g, i);
            continue;
        }
        i++;
    }
    for (i = 0; i < g->count;) {
        face = g->faces[i];
        a = face->p0;
        b = face->unk20;
        if (gobj->cas->active == 8 && g == &c->groups[3]) {
            pos = c->unk234;
            n2 = face->normal;
            FriendlyInsideEdge(&a, &b, &pos, &n2, &face->left);
            pos = c->unk244;
            n2 = face->normal;
            FriendlyInsideEdge(&a, &b, &pos, &n2, &face->right);
        } else {
            dir.vx = b.vx;
            dir.vy = 0;
            dir.vz = b.vz;
            EGSMath_VectorModulus(&dir, &len);
            len = 0x10000 / len;
            dir.vx = EGSMathFIXMul(dir.vx, len);
            dir.vy = EGSMathFIXMul(dir.vy, len);
            dir.vz = EGSMathFIXMul(dir.vz, len);
            pos.vx = c->pos.vx - dir.vx;
            pos.vy = c->pos.vy - dir.vy;
            pos.vz = c->pos.vz - dir.vz;
            n2 = face->normal;
            FriendlyInsideEdge(&a, &b, &pos, &n2, &face->left);
            pos.vx = c->pos.vx + dir.vx;
            pos.vy = c->pos.vy + dir.vy;
            pos.vz = c->pos.vz + dir.vz;
            n2 = face->normal;
            FriendlyInsideEdge(&a, &b, &pos, &n2, &face->right);
        }
        if (*(u16 *)&face->left == 0) { /* neither left nor right */
            FACE_LIST_DROP(g, i);
            continue;
        }
        i++;
    }
    for (i = 0; i < g->count; i++) {
        face = g->faces[i];
        if (best == NULL || face->facing < bestFacing) {
            best = face;
            bestFacing = face->facing;
        }
    }
    if (best != NULL) {
        left = best->left;
        right = best->right;
        for (i = 0; i < g->count && (!left || !right); i++) {
            face = g->faces[i];
            if (face != best) {
                FriendlyDot(&face->normal.vx, &best->normal.vx, &dot);
                if (dot >= 0xB50) {
                    if (!left) {
                        left = face->left;
                    }
                    if (!right) {
                        right = face->right;
                    }
                }
            }
        }
    }
    ((FriendlyMove *)gobj->move)->flags1A4 &= ~(1 << g->leftBit);
    ((FriendlyMove *)gobj->move)->flags1A4 &= ~(1 << g->rightBit);
    if (best != NULL) {
        ((FriendlyMove *)gobj->move)->flags1A4 |= 1 << g->flagBit;
        if (left) {
            ((FriendlyMove *)gobj->move)->flags1A4 |= 1 << g->leftBit;
        }
        if (right) {
            ((FriendlyMove *)gobj->move)->flags1A4 |= 1 << g->rightBit;
        }
        g->best = best;
    } else {
        ((FriendlyMove *)gobj->move)->flags1A4 &= ~(1 << g->flagBit);
    }
}

/* Turn each found edge into a 0x40-high fake wall face pushed 8 units out along its normal. */
static void func_80088BAC(Gobj *gobj, FaceCollect *c) {
    FriendlyPoly poly;
    VECTOR32 a;
    VECTOR32 b;
    VECTOR32 n;
    VECTOR32 off;
    FoundFace **edges;
    int i;
    int count;

    if (JoystickGobjShouldCollideWithEdges(gobj)) {
        count = c->groups[0].count;
        poly.count = 4;
        edges = c->groups[0].faces;
        for (i = 0; i < count; i++) {
            a = edges[i]->p0;
            b = edges[i]->p1;
            n = edges[i]->normal;
            off.vx = EGSMathFIXMul(n.vx, 8);
            off.vy = EGSMathFIXMul(n.vy, 8);
            off.vz = EGSMathFIXMul(n.vz, 8);
            a.vx += off.vx;
            a.vy += off.vy;
            a.vz += off.vz;
            b.vx += off.vx;
            b.vy += off.vy;
            b.vz += off.vz;
            poly.v[0] = b;
            poly.v[1] = a;
            poly.v[2] = a;
            poly.v[3] = b;
            poly.v[2].vy += 0x40;
            poly.v[3].vy += 0x40;
            poly.nx = -n.vx;
            poly.ny = -n.vy;
            poly.nz = -n.vz;
            CollisionGobjAddFakeFace(gobj, &poly, 0);
        }
    }
}

static void func_80088DE4(void *arg0, FaceCollect *c) {
    int i;
    FoundFace *face;

    for (i = 0; i < c->groups[0].count; i++) {
        face = c->groups[0].faces[i];
        if (face->kind < 2) {
            c->groups[0].faces[i] = c->groups[0].faces[c->groups[0].count - 1];
            c->groups[0].faces[c->groups[0].count - 1] = face;
            c->groups[0].count--;
        }
    }
    func_80087E0C(arg0, c, c);
}

static void func_80088E7C(Gobj *gobj, FaceCollect *c) {
}

static void func_80088E84(void *arg0, u8 *arg1) {
    func_80088270(arg0, arg1, arg1 + 0xC8);
}

static void func_80088EA4(void *arg0, u8 *arg1) {
    func_80088270(arg0, arg1, arg1 + 0x12C);
}

static void func_80088EC4(void *arg0, u8 *arg1) {
    func_80088270(arg0, arg1, arg1 + 0x190);
}

#define FIX_TO_INT(x) ((x) >= 0 ? (x) >> 12 : -(-(x) >> 12))
void GTEMath_VectorNormal(VECTOR32 *in, VECTOR32 *out);

#ifdef NON_MATCHING
/* the original keeps &f->face in a register and never folds D_80106AEC.foundCount into c (cf. func_80089544); regalloc follows */
/*
 * Fill D_80106AEC for gobj: position, probes, direction and velocity, then
 * collect the flagged edges of its face list as found faces (deduplicated)
 * and sort them into the first group.
 */
static void func_80088EE4(Gobj *gobj) {
    FaceCollect *c;
    CollisionFace *cf;
    FakeFace *f;
    FoundFace *found;
    FakeVertex **verts;
    VECTOR32 normal;
    VECTOR32 v;
    u8 *edgeFlags;
    u8 flags;
    s32 n;
    s32 i;
    s32 j;

    D_80106AEC.foundCount = 0;
    D_80106AEC.pos = *(VECTOR32 *)gobj->move;
    D_80106AEC.unk234.vx = ((FriendlyAnim *)gobj->anim)->frames[8]->t[0];
    D_80106AEC.unk234.vy = ((FriendlyAnim *)gobj->anim)->frames[8]->t[1];
    D_80106AEC.unk234.vz = ((FriendlyAnim *)gobj->anim)->frames[8]->t[2];
    D_80106AEC.unk234.vy = -D_80106AEC.unk234.vy;
    D_80106AEC.unk244.vx = ((FriendlyAnim *)gobj->anim)->frames[2]->t[0];
    D_80106AEC.unk244.vy = ((FriendlyAnim *)gobj->anim)->frames[2]->t[1];
    D_80106AEC.unk244.vz = ((FriendlyAnim *)gobj->anim)->frames[2]->t[2];
    D_80106AEC.unk244.vy = -D_80106AEC.unk244.vy;
    D_80106AEC.ref = D_80106AEC.pos;
    D_80106AEC.ref.vy = FriendlyGobjGetReferenceY(gobj, 0);
    D_80106AEC.dir[0] = ((FriendlyAnim *)gobj->anim)->frame->m[0][2];
    D_80106AEC.dir[1] = ((FriendlyAnim *)gobj->anim)->frame->m[1][2];
    D_80106AEC.dir[2] = ((FriendlyAnim *)gobj->anim)->frame->m[2][2];
    D_80106AEC.dir[1] = -D_80106AEC.dir[1];
    D_80106AEC.vel[0] = ((FriendlyMove *)gobj->move)->unk60.vx + ((FriendlyMove *)gobj->move)->unk70.vx;
    D_80106AEC.vel[1] = ((FriendlyMove *)gobj->move)->unk60.vy + ((FriendlyMove *)gobj->move)->unk70.vy;
    D_80106AEC.vel[2] = ((FriendlyMove *)gobj->move)->unk60.vz + ((FriendlyMove *)gobj->move)->unk70.vz;
    c = &D_80106AEC;
    c->vel[0] = FIX_TO_INT(c->vel[0]);
    c->vel[1] = FIX_TO_INT(c->vel[1]);
    c->vel[2] = FIX_TO_INT(c->vel[2]);
    for (f = ((FriendlyAnim *)gobj->anim)->faceList; f != NULL; f = f->next) {
        flags = *f->face.flags;
        cf = &f->face;
        if (flags == 0) {
            continue;
        }
        edgeFlags = f->face.flags - 12;
        if (f->face.count == 4) {
            edgeFlags = f->face.flags - 1;
        }
        if (*edgeFlags & 8) {
            continue;
        }
        n = cf->count;
        normal.vx = cf->normalPtr[0];
        normal.vy = cf->normalPtr[1];
        normal.vz = cf->normalPtr[2];
        verts = cf->verts;
        for (i = 0; i < n; i++) {
            if (!((flags >> (i * 2)) & 3)) {
                continue;
            }
            if (c->foundCount >= 32) {
                return;
            }
            found = &c->found[c->foundCount];
            j = i + 1;
            if (j == n) {
                j = 0;
            }
            if (normal.vy > 0x800) {
                found->p0.vx = verts[i]->x;
                found->p0.vy = verts[i]->y;
                found->p0.vz = verts[i]->z;
                found->p1.vx = verts[j]->x;
                found->p1.vy = verts[j]->y;
                found->p1.vz = verts[j]->z;
            } else {
                found->p0.vx = verts[j]->x;
                found->p0.vy = verts[j]->y;
                found->p0.vz = verts[j]->z;
                found->p1.vx = verts[i]->x;
                found->p1.vy = verts[i]->y;
                found->p1.vz = verts[i]->z;
            }
            found->unk20.vx = found->p1.vx - found->p0.vx;
            found->unk20.vy = found->p1.vy - found->p0.vy;
            found->unk20.vz = found->p1.vz - found->p0.vz;
            for (j = 0; j < c->foundCount; j++) {
                if (c->found[j].p0.vx == found->p0.vx && c->found[j].p0.vy == found->p0.vy &&
                    c->found[j].p0.vz == found->p0.vz && c->found[j].p1.vx == found->p1.vx &&
                    c->found[j].p1.vy == found->p1.vy && c->found[j].p1.vz == found->p1.vz) {
                    break;
                }
            }
            if (j < c->foundCount) {
                continue;
            }
            v.vx = -found->unk20.vz;
            v.vy = 0;
            v.vz = found->unk20.vx;
            GTEMath_VectorNormal(&v, &found->normal);
            found->kind = (flags >> (i * 2)) & 3;
            c->foundCount++;
        }
    }
    c->groups[0].count = 0;
    for (i = 0; i < c->foundCount; i++) {
        func_80087B8C(gobj, c, &c->found[i]);
    }
    func_80088BAC(gobj, c);
}
#else
INCLUDE_ASM("asm/main/nonmatchings/friendly", func_80088EE4);
#endif

#ifdef NON_MATCHING
/*
 * Logic matches; the original never CSEs the D_80106AEC field addresses (each store is
 * lui/sw, and c is rematerialised after the call), and its pos block copy moves 3+1 words.
 */
static void func_80089544(Gobj *gobj) {
    FaceCollect *c;
    int i;

    D_80106AEC.pos = *(VECTOR32 *)gobj->move;
    D_80106AEC.unk234.vx = ((FriendlyAnim *)gobj->anim)->frames[8]->t[0];
    D_80106AEC.unk234.vy = ((FriendlyAnim *)gobj->anim)->frames[8]->t[1];
    D_80106AEC.unk234.vz = ((FriendlyAnim *)gobj->anim)->frames[8]->t[2];
    D_80106AEC.unk234.vy = -D_80106AEC.unk234.vy;
    D_80106AEC.unk244.vx = ((FriendlyAnim *)gobj->anim)->frames[2]->t[0];
    D_80106AEC.unk244.vy = ((FriendlyAnim *)gobj->anim)->frames[2]->t[1];
    D_80106AEC.unk244.vz = ((FriendlyAnim *)gobj->anim)->frames[2]->t[2];
    D_80106AEC.unk244.vy = -D_80106AEC.unk244.vy;
    D_80106AEC.ref = D_80106AEC.pos;
    D_80106AEC.ref.vy = FriendlyGobjGetReferenceY(gobj, 0);
    D_80106AEC.dir[0] = ((FriendlyAnim *)gobj->anim)->frame->m[0][2];
    D_80106AEC.dir[1] = ((FriendlyAnim *)gobj->anim)->frame->m[1][2];
    D_80106AEC.dir[2] = ((FriendlyAnim *)gobj->anim)->frame->m[2][2];
    D_80106AEC.dir[1] = -D_80106AEC.dir[1];
    D_80106AEC.vel[0] = ((FriendlyMove *)gobj->move)->unk60.vx + ((FriendlyMove *)gobj->move)->unk70.vx;
    D_80106AEC.vel[1] = ((FriendlyMove *)gobj->move)->unk60.vy + ((FriendlyMove *)gobj->move)->unk70.vy;
    D_80106AEC.vel[2] = ((FriendlyMove *)gobj->move)->unk60.vz + ((FriendlyMove *)gobj->move)->unk70.vz;
    c = &D_80106AEC;
    c->vel[0] = FIX_TO_INT(c->vel[0]);
    c->vel[1] = FIX_TO_INT(c->vel[1]);
    c->vel[2] = FIX_TO_INT(c->vel[2]);
    c->groups[0].count = 0;
    c->groups[1].count = 0;
    c->groups[2].count = 0;
    c->groups[3].count = 0;
    c->groups[4].count = 0;
    for (i = 0; i < c->foundCount; i++) {
        func_80087C28(gobj, c, &c->found[i]);
    }
    func_80088DE4(gobj, c);
    func_80088E7C(gobj, c);
    func_80088E84(gobj, c);
    func_80088EA4(gobj, c);
    func_80088EC4(gobj, c);
}
#else
INCLUDE_ASM("asm/main/nonmatchings/friendly", func_80089544);
#endif

s32 FriendlyGobjGetFoundEdge(Gobj *gobj, s32 kind) {
    FaceGroup *slots;
    FoundFace *edge;
    FriendlyEdgeInfo *info;

    slots = D_80106AEC.groups;
    edge = NULL;
    if (gobj == NULL) {
        goto fail;
    }
    if (gobj->move == NULL) {
        return 0;
    }
    if (((FriendlyMove *)gobj->move)->edgeInfo == NULL) {
        return 0;
    }
    info = ((FriendlyMove *)gobj->move)->edgeInfo;
    switch (kind) {
        case 3:
            edge = slots[0].best;
            break;
        case 4:
            edge = slots[1].best;
            break;
        case 5:
            edge = slots[2].best;
            break;
        case 6:
            edge = slots[3].best;
            break;
        case 7:
            edge = slots[4].best;
            break;
    }
    if (edge == NULL) {
    fail:
        return 0;
    }
    info->a = edge->p0;
    info->b = edge->unk20;
    info->c = edge->normal;
    return 1;
}

void FriendlyGobjNegateFaceListYs(Gobj *gobj) {
    SPAD_ENTER();
    func_80086D24(gobj, 0);
    SPAD_LEAVE_NOP();
}

void FriendlyGobjUnNegateFaceListYs(Gobj *gobj) {
    SPAD_ENTER();
    func_80086D24(gobj, 1);
    SPAD_LEAVE_NOP();
}

void FriendlyGobjCollectGameObjectBoxFaces(Gobj *gobj) {
    func_80086F60(gobj);
}

void FriendlyGobjUpdateBeforeCollision(Gobj *gobj) {
    FriendlyMove *move;
    VECTOR32 v;

    SPAD_ENTER();
    func_80088EE4(gobj);
    SPAD_LEAVE();
    move = (FriendlyMove *)gobj->move;
    if ((move->flags1A4 & 0x100) || (gobj->cas->flags & 0x1000)) {
        v = move->unk10C;
        ((FriendlyMove *)gobj->move)->unk11C = v;
    }
}

void FriendlyGobjUpdateAfterCollision(Gobj *gobj) {
    func_80086E94(gobj);
    SPAD_ENTER();
    func_800874C0(gobj);
    SPAD_LEAVE();
    SPAD_ENTER();
    func_80089544(gobj);
    SPAD_LEAVE();
    SPAD_ENTER();
    CollisionClearFakeFaces();
    SPAD_LEAVE_NOP();
}

void FriendlyPreparePlayerAndCameraFaceListsBeforeUse(u8 collect) {
    Gobj camera;
    FriendlyAnim *anim;

    if (gObjPlayer != NULL) {
        FriendlyGobjNegateFaceListYs(gObjPlayer);
        if (collect) {
            FriendlyGobjCollectGameObjectBoxFaces(gObjPlayer);
        }
    }
    anim = &gCameraAnim;
    if ((u32)anim->faceList > 0x80000000) {
        camera.anim = (GameObjectAnim *)anim;
        FriendlyGobjNegateFaceListYs(&camera);
    }
}

void FriendlyRevertPlayerAndCameraFaceListsAfterUse(void) {
    Gobj camera;
    FriendlyAnim *anim;

    if (gObjPlayer != NULL) {
        FriendlyGobjUnNegateFaceListYs(gObjPlayer);
    }
    anim = &gCameraAnim;
    if ((u32)anim->faceList > 0x80000000) {
        camera.anim = (GameObjectAnim *)anim;
        FriendlyGobjUnNegateFaceListYs(&camera);
    }
}
