#include "common.h"

#include "game.h"
#include "egsmath.h"
#include "egsframe.h"

#define ABS(x) ((x) >= 0 ? (x) : -(x))

SDATA_LOCAL(gImagePtr)
SDATA_LOCAL(gInfraredColor)
SDATA_LOCAL(gInfraredModel)
SDATA_LOCAL(gSnifferAnim)
SDATA_LOCAL(gHADoneCallback)
SDATA_LOCAL(gHMDDLA)

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

/* Infrared image slots (0x44 bytes). */
typedef struct EGSImage {
    /* 0x00 */ u8 unk00[4];
    /* 0x04 */ s32 used;
    /* 0x08 */ u8 unk08[4];
    /* 0x0C */ u8 *packets;
    /* 0x10 */ u8 unk10[0x30];
    /* 0x40 */ s32 unk40;
} EGSImage;

#define EGS_IMAGE_MAX 5

extern GsOT hmdOtbl[24];
extern EGSImage gImages[EGS_IMAGE_MAX];
extern s32 gImagePtr;
extern s32 gInfraredColor;
extern s32 gInfraredModes[];
extern void *gInfraredModel;
extern void *gSnifferAnim;
extern void (*gHADoneCallback)(EGSHMD *hmd);
extern void *D_8012D8DC;
extern DamageCache gGabeCache;
extern void *gHMDDLA;
extern u8 gDLA_none[];
extern u8 gDLA_npc_gun_flash[];
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
        gImages[i].used = 0;
        gImages[i].unk40 = 0;
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
        gImages[i].used = 0;
    }
    gImagePtr = 0;
}

INCLUDE_ASM("asm/main/nonmatchings/egshmd", SortHMDs);

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

#ifdef NON_MATCHING
/* regalloc/scheduling in the delta-decode branches; vz store not tail-merged */
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
                    u8 b3 = data[3];
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
                    rot->vx = ((u8)b << 8) + data[1];
                    rot->vy = (data[2] << 8) + data[3];
                    rot->vz = (data[4] << 8) + data[5];
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
#else
INCLUDE_ASM("asm/main/nonmatchings/egshmd", EGSDisplayHAnimate);
#endif


