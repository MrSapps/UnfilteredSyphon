#include "common.h"

#include "game.h"
#include "egsmath.h"
#include "egsframe.h"
#include "scratch.h"

#define ABS(x) ((x) >= 0 ? (x) : -(x))

/* This TU's .data/.sdata are defined below in original order (docs/data_migration.md). */

typedef struct GsOT {
    u32 length;
    void *org;
    u32 offset;
    u32 point;
    void *tag;
} GsOT;

/* Per-damage-id lists of values copied to the scratchpad (0x3C bytes). */
typedef struct DamageEntry {
    /* 0x00 */ void *obj;
    /* 0x04 */ u8 unk04[8];
    /* 0x0C */ s32 count;
    /* 0x10 */ s32 values[11];
} DamageEntry;

typedef struct HMDSkeleton {
    /* 0x00 */ u8 unk00[4];
    /* 0x04 */ s32 boneCount;
} HMDSkeleton;

typedef struct DamageCache {
    /* 0x00 */ s32 count;
    /* 0x04 */ DamageEntry *entries;
    /* 0x08 */ u8 unk08[0x18];
    /* 0x20 */ HMDSkeleton *skeleton;
} DamageCache;

struct EGSHMD;

/*
 * One running hierarchical animation. The keyframe stream at cur starts with an
 * 0xFn marker byte and a frame code (0xFC = end, 1 = translation included), then a
 * big-endian 16-bit bone mask, then per masked bone a delta-packed rotation.
 */
typedef struct HMDAnimObj {
    /* 0x00 */ u8 unk00[4];
    /* 0x04 */ s16 id;
    /* 0x06 */ u8 unk06[2];
    /* 0x08 */ union {
        u32 all; /* bit 28: loop, bit 29: blending */
        u8 steps; /* blend steps left */
    } flags;
    /* 0x0C */ u8 *start;
    /* 0x10 */ u8 *cur;
    /* 0x14 */ s32 code;
    /* 0x18 */ u8 unk18[4];
    /* 0x1C */ void (*callback)(struct EGSHMD *hmd, s32 event, s32 arg);
    /* 0x20 */ u32 callbackFlags; /* 0x04000000 | code: on that frame, 0x10000000: on loop */
    /* 0x24 */ s32 callbackArg;
    /* 0x28 */ s32 type;
    /* 0x2C */ s32 loops;
    /* 0x30 */ u8 unk30[4];
    /* 0x34 */ SVECTOR targets[1]; /* blend target rotation per bone */
} HMDAnimObj;

typedef struct HMDAnimNode {
    /* 0x00 */ HMDAnimObj *obj;
    /* 0x04 */ u8 unk04[4];
    /* 0x08 */ struct HMDAnimNode *next;
} HMDAnimNode;

typedef struct EGSHMD {
    /* 0x00 */ u32 flags0; /* bit 0: small model */
    /* 0x04 */ u8 unk04[2];
    /* 0x06 */ u16 flags;
    /* 0x08 */ u8 unk08[8];
    /* 0x10 */ DamageCache *damage;
    /* 0x14 */ u8 unk14[4];
    /* 0x18 */ EGSFrame **frames; /* one per bone */
    /* 0x1C */ u8 unk1C[4];
    /* 0x20 */ HMDAnimNode *animList;
    /* 0x24 */ SVECTOR *rotations; /* one per bone */
} EGSHMD;

/* Display object handed to the HMD sorters (cf. GameObjectAnim). */
typedef struct EGSDisplayObj {
    /* 0x00 */ u8 unk00[8];
    /* 0x08 */ union {
        u32 all;
        u8 b[4]; /* b[0]: draw flags, b[2]: light flags */
    } flags;
    /* 0x0C */ void *frame; /* EGSFrame */
    /* 0x10 */ u8 unk10[8];
    /* 0x18 */ MATRIX **coord;
    /* 0x1C */ void **dla;
} EGSDisplayObj;

/* Infrared image slots (0x44 bytes): packets of up to 6 models drawn into a private area. */
typedef struct EGSImage {
    /* 0x00 */ u8 unk00[4];
    /* 0x04 */ u8 *start;
    /* 0x08 */ u8 *end;
    /* 0x0C */ u8 *packets;
    /* 0x10 */ u8 *marks[6]; /* packet start per model */
    /* 0x28 */ s32 depths[6];
    /* 0x40 */ s32 levels; /* models in the image */
} EGSImage;

#define EGS_IMAGE_MAX 5

extern GsOT hmdOtbl[24];
extern EGSImage gImages[EGS_IMAGE_MAX];
extern void *D_8012D8DC;
extern DamageCache gGabeCache;

/* .data */
VECTOR gNPCGouraudLight = { 0x960, 0, 0x960, 0 };
u8 gDLA_none[0xB4] = { 0 };
u8 gDLA_npc_gun_flash[0xB4] = {
    0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x80, 0x0C, 0x40, 0x0B, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x60, 0x04, 0xC0, 0x03, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x40, 0x06, 0x00, 0x00, 0x00, 0x00, 0xA0, 0x05, 0x00, 0x05, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x80, 0x0C, 0x40, 0x0B, 0x00, 0x00,
    0xC0, 0x0D, 0x00, 0x00, 0x00, 0x00, 0xE0, 0x0B, 0xA0, 0x0A, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x0A, 0x00, 0x00, 0x00, 0x00, 0xC0, 0x08, 0x80, 0x07, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x0A, 0x00, 0x00, 0x00, 0x00, 0xC0, 0x08, 0x80, 0x07, 0x00, 0x00,
    0x40, 0x06, 0x00, 0x00, 0x00, 0x00, 0xA0, 0x05, 0x00, 0x05, 0x00, 0x00,
};
/* gImageColors (0x80107B18, 0x18 bytes, unreferenced; egsdisp reads it from +4) follows
 * in the splat blob: it may be this file's or egssort's (hand-written asm). */

/* .sdata */
void *gHMDDLA = gDLA_none;
void *gInfraredModel = NULL;
struct HMDSortAnim *gSnifferAnim = NULL;
s32 gGabeZ = 0;
u8 *gImageEnd = NULL;
s32 gGouraudLightLevel = 0;
void (*gHADoneCallback)(EGSHMD *hmd) = NULL;
s32 gImagePtr = 0;
s32 gInfraredColorNormal = 0xFF00FF00;
s32 gInfraredColorSniffer = 0x360087F9;
s32 gInfraredModes[2] = { 0xFF00FF00, 0x360087F9 };
s32 gHMDSortCount = 0;
static u8 D_8010FEA4 = 1; /* reset the infrared images */
static char D_8010FEA8[7] = "IR ERR"; /* SortHMDs (asm) still refers to it by name */

/* commons */
s32 gInfraredColor;
extern MATRIX GsLIGHTWSMATRIX;

void EGSFrameUpdateHierarchy(void *frame);
s32 EGSDisplaySortHMD(EGSHMD *hmd, s32 arg1, s32 arg2, EGSDisplayObj *obj);
s32 EGSDisplaySortSmallHMD(EGSHMD *hmd, s32 arg1, s32 arg2, EGSDisplayObj *obj);

void *EGSMemMalloc(u32 size);
void GsClearOt(u16 offset, u16 point, GsOT *ot);
void *EGSDisplayPacketAreaMalloc(s32 size);
void EGSDisplayPacketAreaFree(EGSHMD *hmd);
void EGSDisplayHAnimate(EGSHMD *hmd, HMDAnimObj *obj, u8 apply);
s32 Fooble(s32 a, s32 b);
void EGSRotMatYXZ(SVECTOR *rot, MATRIX *mat);
void EGSDisplayHAStop(EGSHMD *hmd, s32 id);

s32 HMDInitOT(void)
{
    int i;

    for (i = 0; i < 24; i++) {
        hmdOtbl[i].length = 8;
        hmdOtbl[i].org = EGSMemMalloc(0x400);
        GsClearOt(0, 0, &hmdOtbl[i]);
    }
    return 0x6000;
}

void EGSDisplayInfraredOn(EGSHMD *hmd, s32 mode)
{
    u8 *area;
    int i;

    area = EGSDisplayPacketAreaMalloc(40000);
#line 261
    ASSERT(area != NULL);
    ASSERT(mode < 2);
    hmd->flags |= 0x10;
    gInfraredColor = gInfraredModes[mode];
    for (i = 0; i < EGS_IMAGE_MAX; i++) {
        gImages[i].packets = area;
        area += 8000;
        gImages[i].start = NULL;
        gImages[i].levels = 0;
    }
    gImagePtr = 0;
}

void EGSDisplayInfraredOff(EGSHMD *hmd)
{
    hmd->flags &= ~0x10;
    EGSDisplayPacketAreaFree(hmd);
}

void egsDisplaySetupDamage(EGSHMD *hmd, EGSDisplayObj *obj)
{
    s32 *dst = (s32 *)0x1F8000C0;
    DamageCache *cache = hmd->damage;
    DamageEntry *entry;
    int i, j, n;

    if (obj == D_8012D8DC) {
        cache = &gGabeCache;
    }
    if (cache != NULL) {
        for (i = 0; i < cache->count; i++) {
            entry = &cache->entries[i];
            if (entry->obj == obj) {
                n = entry->count;
                for (j = 0; j < n; j++) {
                    *dst++ = entry->values[j];
                }
                break;
            }
        }
    }
    *dst = 0;
}

s32 DoSortHMD(EGSHMD *hmd, s32 arg1, s32 arg2, EGSDisplayObj *obj)
{
    VECTOR dir;
    MATRIX *coord;
    void *dla;
    u8 small = hmd->flags0 & 1;
    s32 ret;

    if (obj->flags.all & 0xA0) {
        EGSFrameUpdateHierarchy(obj->frame);
        obj->flags.b[0] &= 0x7F;
    }
    if (obj->flags.b[0] & 0x14) {
        if (obj->flags.b[0] & 4) {
            obj->flags.b[0] &= ~4;
        }
        return 0;
    }
    if (obj->flags.b[2] & 0x10) {
        coord = *obj->coord;
        dla = *obj->dla;
        dir.vx = coord->m[0][2];
        dir.vy = coord->m[1][2];
        dir.vz = coord->m[2][2];
        dir.vy = -dir.vy;
        if (dla == gDLA_npc_gun_flash) {
            dir.vx = -dir.vx;
            dir.vy = -dir.vy;
            dir.vz = -dir.vz;
        }
        gHMDDLA = dla;
        GsLIGHTWSMATRIX.m[0][0] = dir.vx;
        GsLIGHTWSMATRIX.m[0][1] = dir.vy;
        GsLIGHTWSMATRIX.m[0][2] = dir.vz;
    }
    egsDisplaySetupDamage(hmd, obj);
    if (small) {
        ret = EGSDisplaySortSmallHMD(hmd, arg1, arg2, obj);
    } else {
        ret = EGSDisplaySortHMD(hmd, arg1, arg2, obj);
    }
    obj->flags.b[0] |= 0x40;
    gHMDDLA = gDLA_none;
    return ret;
}

void InitImages(void)
{
    int i;

    for (i = 0; i < EGS_IMAGE_MAX; i++) {
        gImages[i].start = NULL;
    }
    gImagePtr = 0;
}


/* Display list node (camera->anims) */
typedef struct EGSListNode {
    /* 0x00 */ void *data;
    /* 0x04 */ struct EGSListNode *prev;
    /* 0x08 */ struct EGSListNode *next;
} EGSListNode;

/* View of EGSCamera */
typedef struct HMDCamera {
    /* 0x00 */ u8 unk00[6];
    /* 0x06 */ u16 flags; /* 0x10: infrared view */
    /* 0x08 */ u8 unk08;
    /* 0x09 */ u8 index; /* non-zero for the secondary (rear view) camera */
} HMDCamera;

/* View of GameObjectModel */
typedef struct HMDSortModel {
    /* 0x00 */ u8 unk00[0x20];
    /* 0x20 */ EGSHMD *hmd;
    /* 0x24 */ EGSHMD *farHmd; /* low detail model */
    /* 0x28 */ u32 flags; /* 0x400000: visible, 0x2000000: only in sniffer view */
} HMDSortModel;

/* Per-object light parameters (anim + 0x1C) */
typedef struct HMDSortLight {
    /* 0x00 */ void *dla;
    /* 0x04 */ s16 r, g, b;
    /* 0x0A */ u8 unk0A[8];
    /* 0x12 */ s16 fog; /* 0..100 */
} HMDSortLight;

/* View of GameObjectAnim as seen by the sorter */
typedef struct HMDSortAnim {
    /* 0x00 */ u8 unk00[8];
    /* 0x08 */ u8 flags8; /* 8: hidden */
    /* 0x09 */ u8 flags9;
    /* 0x0A */ u8 flagsA; /* 0x10: no fog cull, 0x40: far clip */
    /* 0x0B */ u8 flagsB;
    /* 0x0C */ void *frame;
    /* 0x10 */ HMDSortModel *model;
    /* 0x14 */ u8 unk14[2];
    /* 0x16 */ s16 side; /* +-1: z bias direction */
    /* 0x18 */ EGSFrame **frames;
    /* 0x1C */ HMDSortLight *light;
    /* 0x20 */ void *animList;
    /* 0x24 */ SVECTOR *rotations;
} HMDSortAnim;

/* View of EGSPointSource (0x2C bytes) */
typedef struct HMDPointSource {
    /* 0x00 */ u8 unk00[8];
    /* 0x08 */ HMDSortAnim *anim;
    /* 0x0C */ u8 unk0C[0x1C];
    /* 0x28 */ HMDSortAnim *anim2;
} HMDPointSource;

typedef struct Str7 {
    char c[7];
} Str7;

extern s32 gTerrainFog; /* near distance | fog shift << 16 */
extern s32 gFogFlag;
extern HMDSortAnim *gHMDSortArray[];
extern HMDPointSource gPointSourceArray[];
extern s32 gPointSourceCount;
extern char gErrorString[];
extern EGSListNode *D_8012D8CC;
extern u16 D_8012DB22;
extern s16 gCurrentDrawBuffer;
extern GsOT gOtbl[];
extern u8 *GsOUT_PACKET_P;
extern VECTOR gPCGouraudLight;

void EGSDisplaySetupGTE(void *frame);
s32 EGSDisplayOnScreen(s32 dist, s32 arg1);
void GsSetFlatLight(s32 id, VECTOR *light);
void GsSortOt(GsOT *src, GsOT *dst);
void InitImages(void);
void EGSDisplayAnimateHMD(EGSHMD *hmd);

/* GTE far (back) colour: RBK, GBK, BBK */
#define gte_SetBackColor(r, g, b) \
    __asm__ volatile("ctc2 %0, $13\n\tctc2 %1, $14\n\tctc2 %2, $15" : : "r"(r), "r"(g), "r"(b))

/* Four-argument call on the scratchpad stack (see scratch.h) */
#define SCRATCH_CALL4(fn, a, b, c, d)                                                    \
    __asm__ volatile(SCRATCH_ENTER "move $4, %0\n\tmove $5, %1\n\tmove $6, %2\n\tmove $7, %3\n\t" \
                     "jal " #fn "\n\t" SCRATCH_LEAVE                                     \
                     :                                                                   \
                     : "r"(a), "r"(b), "r"(c), "r"(d)                                    \
                     : SCRATCH_CLOBBERS)

#define SORT_MAX 6

#ifdef NON_MATCHING
/* register allocation: different pseudos spill and the reload registers are s0/s5/s7 instead of s3/s5/s7.
 * r11AC: the original's spill slots run 0x38 camera, 0x40 sortBuffer, 0x48 gabe, 0x50 count,
 * 0x58 index, 0x60 fogNear, 0x68 fogFar, 0x70 fogOn, 0x78 savedPacket, 0x80 infrared,
 * 0x88 sniffer, 0x90 snifferModel (pseudo = declaration order, now matched); fogRange lives in fp
 * and image in s6 there, ours spills fogRange (frame 0x120 vs 0x118). fogNear's initialiser goes
 * through a separate pseudo (andi -> 0xD8 slot -> copied to 0x60).
 * r12AD: 434 -> 274 by giving the first loop its own anim pointer (one pseudo spanning both loops
 * conflicted with everything). The 0xD8 slot is shared by the andi temp and the &sorted giv: both
 * were global-alloc'd to s3 and kicked out when reload took s3 as a spill reg (alter_reg reuses
 * spill_stack_slot[from_reg]), so the original's reload picks s3/s5/s7, ours s0/s5/s7. Tried with no
 * gain: splitting isSniffer/z/dist per loop, a distinct i/t/d in the later loops, gabe/count/index
 * assigned after the decls, (u16)/&= forms for fogNear. Forcing fogRange into $fp: 434 -> 365
 * before the split, but 274 -> 307 after it, so the fp/s6 choice is not the main remaining cost. */
void SortHMDs(HMDCamera *camera, EGSListNode *anims, s32 sortBuffer)
{
    HMDSortAnim *sorted[SORT_MAX];
    s16 depths[SORT_MAX];
    s32 slot = 0;
    HMDSortAnim *gabe = gPointSourceArray[0].anim;
    s32 count = 0;
    s32 index = camera->index;
    s32 fogNear = gTerrainFog & 0xFFFF;
    s32 fogFar;
    s32 fogRange;
    s32 fogOn;
    u8 *savedPacket;
    u8 infrared;
    u8 sniffer;
    HMDSortModel *snifferModel;
    EGSImage *image;
    HMDSortAnim *anim;
    HMDSortModel *model;
    HMDSortLight *light;
    EGSHMD *hmd;
    u8 isSniffer;
    u8 isGabe;
    s16 bias;
    s16 z;
    s16 r, g, b;
    s32 dist;
    s32 flags;
    s32 ot;
    s32 i;
    s32 t;
    s32 level;
    s32 d;

    if (index) {
        fogNear += fogNear >> 3;
    }
    gHMDDLA = gDLA_none;
    savedPacket = NULL;
    infrared = 0;
    sniffer = 0;
    fogFar = fogNear + ((0x1000 - fogNear) >> (gTerrainFog >> 16));
    fogRange = fogFar - fogNear;
    fogOn = gFogFlag;
    image = NULL;
    gHMDSortCount = 0;
    snifferModel = gSnifferAnim->model;
    if (camera->flags & 0x10) {
        if (gInfraredColor == gInfraredColorSniffer) {
            sniffer = 1;
        } else {
            infrared = 1;
        }
    }
    if (anims != NULL && camera->index) {
        anims = D_8012D8CC;
    }
    if (D_8010FEA4) {
        D_8010FEA4 = 0;
        InitImages();
    }
    if (camera->flags & 0x10) {
        i = gImagePtr++;
        if (gImagePtr == EGS_IMAGE_MAX) {
            gImagePtr = 0;
        }
        image = &gImages[i];
        image->levels = 0;
    }

    for (; anims != NULL; anims = anims->next) {
        HMDSortAnim *a;

        a = anims->data;
        isSniffer = 0;
        if (sniffer) {
            isSniffer = a->model == snifferModel;
        }
        if ((a->model->flags & 0x400000) && a->frames != NULL && a->rotations != NULL &&
            (!(a->flags8 & 8) || isSniffer) && (!(a->model->flags & 0x2000000) || isSniffer)) {
            if (a->flagsA & 0x40) {
                dist = 20000;
            } else {
                dist = 4500;
            }
            EGSDisplaySetupGTE(a->frame);
            if (!index && !isSniffer) {
                EGSDisplayAnimateHMD((EGSHMD *)a);
            }
            z = EGSDisplayOnScreen(dist, 0xF0);
            if (z > 1000 && !index) {
                z = EGSDisplayOnScreen(dist, 0xCD);
            }
            if (a == gabe) {
                gGabeZ = z;
                if (index || (camera->flags & 0x10) || (D_8012DB22 & 1)) {
                    goto next;
                }
            }
            if (z != 0) {
                if (count >= SORT_MAX) {
                    count = SORT_MAX - 1;
                }
                sorted[count++] = a;
                if (!isSniffer) {
                    depths[slot++] = z;
                } else {
                    depths[slot++] = 200;
                }
            } else {
                a->light->fog = 100;
            }
        }
    next:
        a->flags8 &= ~0x40;
        a->flags9 &= ~0x20;
    }

    for (i = 0; i < count; i++) {
        anim = sorted[i];
        model = anim->model;
        light = anim->light;
        if (index) {
            ot = i * 4 + gCurrentDrawBuffer + 2;
        } else {
            ot = i * 4 + gCurrentDrawBuffer;
        }
        bias = 0;
        isSniffer = 0;
        hmd = model->hmd;
        z = depths[i];
        if (sniffer) {
            isSniffer = model == snifferModel;
        }
        isGabe = anim == gabe;
        flags = camera->flags;
        r = light->r;
        if (r >= 0x1000) {
            r = 0xFFF;
        }
        g = light->g;
        if (g >= 0x1000) {
            g = 0xFFF;
        }
        b = light->b;
        if (b >= 0x1000) {
            b = 0xFFF;
        }
        if (anim->flagsB & 0x20) {
            bias = -24;
        } else {
            d = 24;
            if (isGabe && !(anim->flagsB & 1)) {
                if (gabe->flagsB & 0x80) {
                    d = 18;
                } else {
                    d = 12;
                }
            }
            if (anim->side == 1) {
                bias = d;
            } else if (anim->side == -1) {
                bias = -d;
            }
        }
        light->fog = 0;
        gGouraudLightLevel = 0x600;
        if (infrared) {
            gte_SetBackColor(0, 3000, 0);
        } else {
            if (fogOn && fogFar < z && !index && !(anim->flagsA & 0x10)) {
                light->fog = 100;
                continue;
            }
            if (fogOn && fogNear < z) {
                t = fogFar - z;
                d = t * 100 / fogRange;
                level = t * 1536 / fogRange;
                r = r * t / fogRange;
                g = g * t / fogRange;
                light->fog = 100 - d;
                gGouraudLightLevel = level;
                gte_SetBackColor(r, g, b * t / fogRange);
            } else if (z >= 0x200 && model->farHmd != NULL) {
                level = 0x400 - z;
                if (level < 0) {
                    level = 0;
                }
                gGouraudLightLevel = level * 3;
                d = 0x600 - level * 3;
                if (d > 0) {
                    r = r + d < 0x1000 ? r + d : 0xFFF;
                    g = g + d < 0x1000 ? g + d : 0xFFF;
                    b = b + d < 0x1000 ? b + d : 0xFFF;
                }
                gte_SetBackColor(r, g, b);
            } else {
                gte_SetBackColor(r, g, b);
            }
        }
        GsClearOt(0, 0, &hmdOtbl[ot]);
        if (model->farHmd != NULL && !index && z >= 0x400) {
            hmd = model->farHmd;
        }
        if (flags & 0x10) {
            if (infrared || isSniffer) {
                savedPacket = GsOUT_PACKET_P;
                gImageEnd = image->packets + 8000;
                if (image->levels == 0) {
                    image->start = image->packets;
                    GsOUT_PACKET_P = image->packets;
                } else {
                    GsOUT_PACKET_P = image->end;
                }
                image->marks[image->levels] = GsOUT_PACKET_P;
                image->depths[image->levels++] = z >> 2;
                if (model != snifferModel) {
                    hmd = gInfraredModel;
#line 640
                    ASSERT(hmd != NULL);
                }
            } else {
                flags = (flags & ~0x10) | 0x8000;
            }
        }
        if (z < 70 && isGabe && !(anim->flagsB & 0x20)) {
            flags |= 0x8000;
            anim->flags9 |= 0x20;
        }
        GsSetFlatLight(1, isGabe ? &gPCGouraudLight : &gNPCGouraudLight);
        if (isSniffer) {
            gte_SetBackColor(0, 0, 0xC00);
            DoSortHMD(hmd, 0x100000, (s32)gOtbl[sortBuffer].org, (EGSDisplayObj *)anim);
        } else if (index || !(D_8012DB22 & 1)) {
            if (hmd->flags0 & 1) {
                DoSortHMD(hmd, (flags << 16) + z, (s32)hmdOtbl[ot].org + 0x200, (EGSDisplayObj *)anim);
            } else {
                SCRATCH_CALL4(DoSortHMD, hmd, (flags << 16) + z, (s32)hmdOtbl[ot].org + 0x200, anim);
            }
        }
        if (infrared || isSniffer) {
            image->end = GsOUT_PACKET_P;
            if (GsOUT_PACKET_P - image->start > 8000) {
                *(Str7 *)gErrorString = *(Str7 *)D_8010FEA8;
            }
            GsOUT_PACKET_P = savedPacket;
            if (isSniffer) {
                continue;
            }
        }
        if (!index && !isGabe) {
            gHMDSortArray[gHMDSortCount++] = anim;
        }
        d = (z >> 2) + bias;
        if (d <= 0) {
            d = 1;
        }
        hmdOtbl[ot].point = d;
        GsSortOt(&hmdOtbl[ot], &gOtbl[sortBuffer]);
    }

    if (!index) {
        for (i = 0; i < gHMDSortCount; i++) {
            if (gPointSourceCount >= 10) {
                break;
            }
            gPointSourceArray[gPointSourceCount].anim = NULL;
            gPointSourceArray[gPointSourceCount++].anim2 = gHMDSortArray[i];
        }
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/egshmd", SortHMDs);
#endif

void EGSDisplayRegisterInfraredModel(void *model)
{
    gInfraredModel = model;
}

void EGSDisplayRegisterSnifferAnim(void *anim)
{
    gSnifferAnim = anim;
}

void EGSDisplayAnimateHMD(EGSHMD *hmd)
{
    EGSHMD *h = hmd;
    HMDAnimNode *node = hmd->animList;
    HMDAnimObj *obj;

    if (hmd->frames == NULL || hmd->rotations == NULL) {
        return;
    }
    while (node != NULL) {
        obj = node->obj;
        node = node->next;
        if (obj->type == 0 || obj->type == 0x4000) {
            EGSDisplayHAnimate(hmd, obj, 1);
        }
    }
    for (node = h->animList; node != NULL;) {
        obj = node->obj;
        node = node->next;
        if (obj->type == 0x8000) {
            EGSDisplayHAnimate(hmd, obj, 1);
        }
    }
    if (gHADoneCallback != NULL) {
        gHADoneCallback(hmd);
    }
}

/* moves angle a a quarter of the way towards angle b (4096 units per turn) */
s32 Fooble(s32 a, s32 b)
{
    if (ABS(a - b) > 0x800) {
        if (a < 0) {
            b -= 0x1000;
        } else {
            a -= 0x1000;
        }
    }
    a += (b - a) >> 2;
    if (a < -0x800) {
        a += 0x1000;
    }
    if (a > 0x800) {
        a -= 0x1000;
    }
    return a;
}

/* s32 temps (and an s32 b3) keep the halfword sums out of HImode narrowing; otherwise the
 * add/or ties its result to the zero-extended byte operand and the vz stores of the four
 * delta-decode branches are not cross-jumped into one tail. */
void EGSDisplayHAnimate(EGSHMD *hmd, HMDAnimObj *obj, u8 apply)
{
    MATRIX mat;
    EGSHMD *h;
    HMDSkeleton *skel;
    u8 blend;
    s32 code;
    u16 mask;
    u16 weight;
    EGSFrame **frames;
    u8 *data;
    u8 *start;
    SVECTOR *rot;
    EGSFrame *frame;
    EGSFrameNode *node;
    EGSFrameNode *cur;
    s32 b;
    s32 n;
    s32 i;

    weight = 0x7FFF;
    data = obj->cur;
    blend = (obj->flags.all >> 29) & 1;
    h = hmd;
    frames = h->frames;
    skel = h->damage->skeleton;
    if ((data[0] & 0xF0) != 0xF0) {
        return;
    }
    code = data[1];
    obj->code = code;
    if (!blend) {
        if (code == 0xFC && (obj->flags.all & 0x10000000)) {
            data = obj->cur = obj->start;
            code = data[1];
            obj->code = code;
        }
        if ((obj->callbackFlags & 0x04000000) && (obj->callbackFlags & 0xFF) == code && obj->callback != NULL) {
            start = obj->start;
            obj->callback(hmd, code | 0x04000000, obj->callbackArg);
            if (obj->start != start) {
                return;
            }
        }
    } else {
        n = obj->flags.steps;
        if (code == 0xFC) {
            data = obj->cur = obj->start;
            code = data[1];
        }
        if (obj->type == 0x8000) {
            weight = 0x2B2D;
        } else if (obj->type == 0x4000) {
            weight = 0x54D2;
        }
        if (--n == 0) {
            obj->flags.all &= 0xDFFFFF00;
        } else {
            obj->flags.all = (obj->flags.all & ~0xFF) | n;
        }
    }
    data += 2;
    mask = (data[0] << 8) | data[1];
    data += 2;
    for (i = 0; i < skel->boneCount; i++, mask >>= 1) {
        b = data[0];
        rot = &h->rotations[i];
        frame = frames[i];
        if (blend) {
            node = frame->node;
            if (weight & 1) {
                rot->vx = Fooble(rot->vx, obj->targets[i].vx);
                rot->vy = Fooble(rot->vy, obj->targets[i].vy);
                rot->vz = Fooble(rot->vz, obj->targets[i].vz);
                EGSRotMatYXZ(rot, &mat);
                mat.t[0] = node->local.t[0];
                mat.t[1] = node->local.t[1];
                mat.t[2] = node->local.t[2];
                frame->node->local = mat;
                frame->node->dirty = 1;
            }
            weight >>= 1;
        }
        if (mask & 1) {
            if (blend) {
                if (!(b & 0x80)) {
                    data += 2;
                } else if (b & 0x40) {
                    data += 4;
                } else if (!(b & 0x20)) {
                    data += 3;
                } else {
                    data += 6;
                }
                if (code == 1 || i == 0) {
                    data += 3;
                }
            } else {
                if (!(b & 0x80)) {
                    u8 b1 = data[1];
                    data += 2;
                    rot->vx += (s32)(b << 25) >> 27;
                    rot->vy += (s32)((b << 30) | ((b1 & 0xE0) << 22)) >> 27;
                    rot->vz += (s32)(b1 << 27) >> 27;
                } else if (b & 0x40) {
                    u8 b1 = data[1];
                    u8 b2 = data[2];
                    s32 b3 = data[3];
                    data += 4;
                    rot->vx += (s32)((b << 26) | ((b1 & 0xF0) << 18)) >> 22;
                    rot->vy += (s32)((b1 << 28) | ((b2 & 0xFC) << 20)) >> 22;
                    rot->vz += ((s32)(b2 << 30) >> 22) | b3;
                } else if (!(b & 0x20)) {
                    u8 b1 = data[1];
                    u8 b2 = data[2];
                    data += 3;
                    rot->vx += (s32)((b << 27) | ((b1 & 0xC0) << 19)) >> 25;
                    rot->vy += (s32)((b1 << 26) | ((b2 & 0x80) << 18)) >> 25;
                    rot->vz += (s32)(b2 << 25) >> 25;
                } else {
                    if (b & 0x10) {
                        b |= 0xE0;
                    } else {
                        b &= 0x1F;
                    }
                    { s32 t = ((u8)b << 8) + data[1]; rot->vx = t; }
                    { s32 t = (data[2] << 8) + data[3]; rot->vy = t; }
                    { s32 t = (data[4] << 8) + data[5]; rot->vz = t; }
                    data += 6;
                }
                if (code == 1) {
                    mat.t[0] = (s8)data[0];
                    mat.t[1] = -(data[1] << 24) >> 24;
                    mat.t[2] = (s8)data[2];
                    data += 3;
                } else {
                    cur = frame->node;
                    if (i == 0) {
                        data += 3;
                    }
                    mat.t[0] = cur->local.t[0];
                    mat.t[1] = cur->local.t[1];
                    mat.t[2] = cur->local.t[2];
                }
                if (apply) {
                    EGSRotMatYXZ(rot, &mat);
                    frame->node->local = mat;
                    frame->node->dirty = 1;
                }
            }
        }
    }
    obj->cur = data;
    if (!blend && data[1] == 0xFC) {
        if ((obj->flags.all & 0x10000000) && --obj->loops != 0) {
            if ((obj->callbackFlags & 0x10000000) && obj->callback != NULL) {
                obj->callback(hmd, 0x10000000, obj->callbackArg);
            }
        } else {
            EGSDisplayHAStop(hmd, obj->id);
        }
    }
}

/* $gp commons: let maspsx use $gp for them (kept after the last function, in .text) */
SDATA_LOCAL(gInfraredColor)
