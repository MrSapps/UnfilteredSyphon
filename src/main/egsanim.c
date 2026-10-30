#include "common.h"
#include "game.h"

/* egsanim.c's small data, still assembled from asm */
SDATA_LOCAL(D_8010FF08)
SDATA_LOCAL(masterAnimCount)
SDATA_LOCAL(D_80110220)
SDATA_LOCAL(D_80110224)

#define MASTER_HMD 0x400000         /* model is an HMD; register it with the display */
#define MASTER_NO_BOX 0x1000000     /* do not initialise the bounding box */
#define MASTER_HMD_NOSHADE 0x2000000
#define MASTER_MODEL_BOX 0x4000000  /* box comes from the model header */

typedef struct EGSVec4 {
    s32 x, y, z, pad;
} EGSVec4;

typedef struct EGSBox {
    /* 0x00 */ EGSVec4 min;
    /* 0x10 */ EGSVec4 max;
} EGSBox; /* size 0x20 */

typedef struct MasterAnim {
    /* 0x00 */ EGSBox box;
    /* 0x20 */ void *model;
    /* 0x24 */ void *detailModel;
    /* 0x28 */ u32 flags;
    /* 0x2C */ s32 unk2C;
    /* 0x30 */ u8 unk30[0x44];
    /* 0x74 */ s32 unk74;
} MasterAnim; /* size 0x78 */

typedef struct EGSAnim {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 unk4;
    /* 0x08 */ u8 unk8;
    /* 0x09 */ u8 unk9;
    /* 0x0A */ u8 flags; /* 4: showing the detail model */
    /* 0x0B */ u8 unkB;
    /* 0x0C */ void *frame;
    /* 0x10 */ MasterAnim *master;
    /* 0x14 */ s16 unk14;
    /* 0x16 */ s16 unk16;
    /* 0x18 */ s32 unk18;
    /* 0x1C */ struct EGSAnimHMD *hmd;
    /* 0x20 */ s32 unk20;
    /* 0x24 */ s32 unk24;
    /* 0x28 */ s32 unk28;
} EGSAnim; /* size 0x2C (0x1C for simple anims) */

typedef struct EGSAnimHMD {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s16 scale[3];
    /* 0x0A */ s16 unkA;
    /* 0x0C */ s16 rot[3];
    /* 0x12 */ s16 unk12;
} EGSAnimHMD; /* size 0x14 */

/* Model header fields used for the bounding box. */
typedef struct EGSModelHeader {
    /* 0x00 */ u8 unk0[0xA];
    /* 0x0A */ s16 min[3];
    /* 0x10 */ s16 max[3];
} EGSModelHeader;

extern s16 D_8010FF08;       /* max number of master anims */
extern s16 masterAnimCount;
extern void **D_80110220;    /* model of each master anim */
extern MasterAnim **D_80110224;
extern EGSVec4 hmdMin;
extern EGSVec4 hmdMax;

void *EGSMemMalloc(u32 size);
void EGSDisplayRegisterHMD(void *model);
void EGSDisplayGetSectorBox(void *model, MasterAnim *master);
void EGSDisplaySetupGTE(void *frame);
s32 EGSFastCalculateZ(s32 x, s32 y, s32 z);
void EGSFrameCreateWithNode(void **frame, void *node);
void EGSAnimHMDLink(EGSAnim *anim, void *model, s32 arg2, s32 arg3);
void InitMasters(void);
s32 EGSAnimInitBox(MasterAnim *master, EGSModelHeader *model);

MasterAnim *FindMasterAnim(void *model, EGSAnim *anim, u32 flags) {
    s32 idx;
    s32 i;
    MasterAnim *master;

    idx = -1;
    if (masterAnimCount == -1) {
        InitMasters();
    }
    if ((s32)model != idx) {
        for (i = 0; i < D_8010FF08; i++) {
            if (D_80110220[i] == NULL) {
                break;
            }
            if (D_80110220[i] == model) {
                idx = i;
                break;
            }
        }
    }
    if (idx == -1) {
        idx = masterAnimCount;
        master = EGSMemMalloc(sizeof(MasterAnim));
        D_80110224[masterAnimCount] = master;
        D_80110220[masterAnimCount++] = model;
        master->flags = flags;
        master->unk74 = -1;
        if (flags & MASTER_HMD) {
            EGSDisplayRegisterHMD(model);
        }
    }
    master = D_80110224[idx];
    master->model = NULL;
    master->detailModel = NULL;
    master->unk2C = 0;
    if (anim != NULL) {
        anim->master = master;
    }
    return master;
}

s32 EGSAnimSimpleCreate(void *model, void *frame, u32 flags, EGSAnim **out) {
    MasterAnim *master;

    if (model == NULL) {
        return 1;
    }
    *out = EGSMemMalloc(0x1C);
    master = FindMasterAnim(model, *out, flags);
    (*out)->master = master;
    if (!(flags & MASTER_NO_BOX)) {
        EGSAnimInitBox(master, model);
    }
    master->model = model;
    (*out)->unk8 = 1;
    (*out)->unk9 = 0x40;
    (*out)->flags = 0;
    (*out)->unkB = 0;
    (*out)->frame = frame;
    (*out)->unk0 = 0;
    (*out)->unk4 = 0;
    (*out)->unk14 = 0;
    (*out)->unk18 = 0;
    return 0;
}

MasterAnim *EGSAnimCreateMasterAnim(void *model, u32 flags) {
    MasterAnim *master;

    if (model == NULL) {
        return NULL;
    }
    master = FindMasterAnim(model, NULL, flags);
    if (!(flags & MASTER_NO_BOX)) {
        EGSAnimInitBox(master, model);
    }
    master->model = model;
    return master;
}

s32 EGSAnimGetBox(EGSAnim *anim, s32 unused, EGSBox *box) {
    *box = anim->master->box;
    return 0;
}

s32 EGSAnimGetFrame(EGSAnim *anim, void **frame) {
    if (anim == NULL) {
        return 1;
    }
    *frame = anim->frame;
    return 0;
}

s32 EGSAnimInitBox(MasterAnim *master, EGSModelHeader *model) {
    if (master->flags & MASTER_MODEL_BOX) {
        master->box.min.x = model->min[0];
        master->box.min.y = model->min[1];
        master->box.min.z = model->min[2];
        master->box.max.x = model->max[0];
        master->box.max.y = model->max[1];
        master->box.max.z = model->max[2];
    } else if (master->flags & MASTER_NO_BOX) {
        EGSDisplayGetSectorBox(model, master);
    }
    return 0;
}

void EGSAnimShowDetailDef(EGSAnim *anim) {
    MasterAnim *master;

    master = anim->master;
#line 419
    ASSERT(master->detailModel != NULL);
    anim->flags |= 4;
    EGSAnimInitBox(master, master->detailModel);
}

void EGSAnimHideDetailDef(EGSAnim *anim) {
    MasterAnim *master;

    master = anim->master;
    anim->flags &= ~4;
    EGSAnimInitBox(master, master->model);
}

s32 EGSAnimSetBox(EGSAnim *anim, s32 unused, EGSBox *box) {
    anim->master->box = *box;
    return 0;
}

void InitMasters(void) {
    s16 i;

    D_80110220 = EGSMemMalloc(D_8010FF08 * 4);
    D_80110224 = EGSMemMalloc(D_8010FF08 * 4);
    masterAnimCount = 0;
    for (i = 0; i < D_8010FF08; i++) {
        D_80110220[i] = NULL;
    }
}

s32 EGSAnimGetTransformedZ(EGSAnim *anim) {
    if (anim == NULL) {
        return -1;
    }
    EGSDisplaySetupGTE(anim->frame);
    return EGSFastCalculateZ(0, 0, 0);
}

s32 EGSAnimHMDCreate(void *model, s32 arg1, s32 unused, u8 shaded, EGSAnim **out, s32 arg5) {
    EGSAnim *anim;
    EGSAnimHMD *hmd;
    MasterAnim *master;

    anim = EGSMemMalloc(sizeof(EGSAnim));
    hmd = EGSMemMalloc(sizeof(EGSAnimHMD));
    master = FindMasterAnim(model, anim, MASTER_HMD);
    if (!shaded) {
        master->flags |= MASTER_HMD_NOSHADE;
    }
    master->box.min = hmdMin;
    master->box.max = hmdMax;
    EGSFrameCreateWithNode(&anim->frame, NULL);
    anim->unk8 = 1;
    anim->unk9 = 0x80;
    anim->unk16 = 0x29A;
    anim->unk0 = 0;
    anim->unk4 = 0;
    anim->flags = 0;
    anim->unkB = 0;
    anim->master = master;
    anim->unk14 = 0;
    anim->unk28 = 0;
    hmd->scale[2] = 500;
    hmd->scale[1] = 500;
    hmd->scale[0] = 500;
    hmd->rot[2] = 0;
    hmd->rot[1] = 0;
    hmd->rot[0] = 0;
    hmd->unk0 = 0;
    anim->hmd = hmd;
    EGSAnimHMDLink(anim, model, arg1, arg5);
    *out = anim;
    return 0;
}

s32 EGSAnimHMDLinkModel(EGSAnim *anim, void *model) {
    s32 i;

    for (i = 0; i < D_8010FF08; i++) {
        if (D_80110220[i] == NULL) {
            break;
        }
        if (D_80110220[i] == model) {
            anim->master = D_80110224[i];
            return 0;
        }
    }
    return 15;
}

INCLUDE_RODATA("asm/main/nonmatchings/egsanim", gEGSMathIdentityMatrix);
