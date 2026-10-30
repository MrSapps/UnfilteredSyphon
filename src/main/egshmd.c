#include "common.h"

#include "game.h"
#include "egsmath.h"

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

typedef struct DamageCache {
    /* 0x00 */ s32 count;
    /* 0x04 */ DamageEntry *entries;
} DamageCache;

typedef struct HMDAnimObj {
    /* 0x00 */ u8 unk00[0x28];
    /* 0x28 */ s32 type;
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
    /* 0x18 */ void *anim;
    /* 0x1C */ u8 unk1C[4];
    /* 0x20 */ HMDAnimNode *animList;
    /* 0x24 */ void *unk24;
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
void EGSDisplayHAnimate(EGSHMD *hmd, HMDAnimObj *obj, s32 arg2);

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

    if (hmd->anim == NULL || hmd->unk24 == NULL) {
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

INCLUDE_ASM("asm/main/nonmatchings/egshmd", EGSDisplayHAnimate);

INCLUDE_ASM("asm/main/nonmatchings/egshmd", EGSRotMatYXZ);

INCLUDE_ASM("asm/main/nonmatchings/egshmd", EGSDisplayOnScreen);

INCLUDE_ASM("asm/main/nonmatchings/egshmd", EGSDisplaySetGTEAmbient);

INCLUDE_ASM("asm/main/nonmatchings/egshmd", EGSDisplaySetupGTE_SP);

INCLUDE_ASM("asm/main/nonmatchings/egshmd", EGSDisplaySortHMD);

INCLUDE_ASM("asm/main/nonmatchings/egshmd", func_800CC418);

INCLUDE_ASM("asm/main/nonmatchings/egshmd", EGSDisplaySortSmallHMD);

INCLUDE_ASM("asm/main/nonmatchings/egshmd", EGSDisplayGetSectorBox);

INCLUDE_ASM("asm/main/nonmatchings/egshmd", EGSDisplayPointColor);

