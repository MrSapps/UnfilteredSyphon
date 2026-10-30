#include "common.h"
#include "game.h"
#include "system.h"
#include "level.h"

/*
 * init.ovl: level loading (overlay, data and hog files), game object creation and
 * the per-type object init functions (objinit.c is #included into this file).
 */

/* GameObjectRuntime.anim as seen by the object init functions. */
typedef struct InitAnim {
    /* 0x00 */ u8 unk00[8];
    /* 0x08 */ u8 flags08;
    /* 0x09 */ u8 unk09;
    /* 0x0A */ u8 unk0A;
    /* 0x0B */ u8 flags0B;
    /* 0x0C */ void *frame;
    /* 0x10 */ s32 *box; /* min x,y,z at 0x0, max x,y,z at 0x10 */
    /* 0x14 */ u8 unk14[4];
    /* 0x18 */ s32 unk18;
} InitAnim;

/* GameObjectRuntime as seen by the object init functions. */
typedef struct InitRuntime {
    /* 0x00 */ u8 flags0;
    /* 0x01 */ u8 flags1;
    /* 0x02 */ s16 id;
    /* 0x04 */ s32 unk04;
    /* 0x08 */ InitAnim *anim;
    /* 0x0C */ s32 *move;
    /* 0x10 */ void *cas;
    /* 0x14 */ void *target;
    /* 0x18 */ GameObjectHealth *healthInfo;
    /* 0x1C */ void *npc;
    /* 0x20 */ u8 flags20;
    /* 0x21 */ u8 unk21;
    /* 0x22 */ u8 npcState;
    /* 0x23 */ u8 flags23;
} InitRuntime;

typedef struct InitAnimSlot {
    /* 0x00 */ u8 active;
    /* 0x04 */ s32 unk04;
    /* 0x08 */ s32 unk08;
    /* 0x0C */ s32 unk0C;
    /* 0x10 */ s32 unk10;
    /* 0x14 */ s32 unk14;
    /* 0x18 */ s32 unk18;
    /* 0x1C */ s32 unk1C;
} InitAnimSlot; /* size 0x20 */

/* Level data loaded from \<LEVEL>\<LEVEL>.DAT */
typedef struct LevelData {
    /* 0x00 */ u8 unk00[0x4E];
    /* 0x4E */ s16 collideX;
    /* 0x50 */ s16 collideZ;
} LevelData;

/* init.ovl rodata, still in asm */
extern void (*D_80145C54[])(s32); /* level init function per level */
extern char D_80145CD4[];         /* "%s.OVL" */
extern char D_80145D14[];         /* "bin/init.c" */

/* init.ovl data, still in asm */
extern char *D_8014BA30[]; /* overlay name per level */
extern s32 D_8014BAD4;     /* in LevelLoadCallback */

/* main.exe */
extern u8 D_5EE8[]; /* linker constant */
/* Mission briefing shown while the level loads. */
typedef struct Briefing {
    /* 0x00 */ char *lines[3];
    /* 0x0C */ s32 count;
    /* 0x10 */ char **objectives;
} Briefing;

typedef struct EGSSprite {
    u8 unk00[0x2C];
} EGSSprite;

extern Briefing *gBriefings;
extern EGSSprite *gInitHeap;
extern u8 gSpareTextWindow;
u8 LevelGameObjectIsInvisibleClass(s16 id);
void InterfaceSetMode(s32 mode);
u8 TextAddWindow(s32 x, s32 y, s32 w, s32 h);
void TextSetWindowColor(u8 window, s32 r, s32 g, s32 b);
u16 TextAddString(u8 window, char *str, s32 arg2, EGSSprite *sprites);
s32 TextStringLength(u16 id, s32 arg1);
void TextSpriteInitialize(void *buf, s32 count);
int strlen(const char *);
int strcmp(const char *, const char *);
extern u8 gINTRFACEHog[];
extern u8 gSPFXHog[];
extern u8 gMultiTextures[];
extern char D_80145D20[]; /* "\\COMMON\\INTRFACE" */
extern char D_80145D34[]; /* "\\COMMON\\SPFX" */
typedef struct EGSHog {
    /* 0x00 */ void *data;
    /* 0x04 */ u8 unk04[0xC];
} EGSHog;

typedef struct EGSTex {
    /* 0x00 */ u8 unk00[0x30];
} EGSTex;

s32 EGSHogGetBlockCount(EGSHog *hog);
s32 EGSHogGetBlock(EGSHog *hog, s32 index, EGSTex *tex);
void EGSTextureCreate(EGSTex *tex);
void EGSTextureLoad(EGSTex *tex);
void SpriteSetupInterface(EGSHog *hog);
void SetupMultiTextures(EGSHog *hog, s32 count, void *textures);
void SpriteCreateFootFx(EGSHog *hog);
void ObjectTextureAnimationsHack(void);
extern LevelData *gLevData;
extern LevelData levdata;
extern u8 target[];
extern u8 gAddingObjectiveViaQuip;
extern u8 gElectricFenceFlag;
extern void *gGameCamScriptEndCallback;
extern void *gHitNPCCallback;
extern u8 mGameOverMsgFlag;
extern u8 mMissionFailedMsgFlag;
extern s32 mLastTickAddedObjectives;
extern s32 mNextTickAllowMsgIncompleteObjectives;
extern s32 mNextTickAllowMsgMissionFailed;
s32 EGSHogOpen(char *path, s32 arg1, s32 arg2);
void EGSDisplaySetCollideXZ(s32 x, s32 z);
void SysLoadData(char *path, void *addr, s32 size);
void LevelCreateGOBJ(s16 id, s32 arg1);
void TargetInitGobj(void *rt, void *profile, s32 arg2, s32 arg3);
void SysExeTickQueueTimer(void (*cb)(), s32 ticks, void *arg);
void UseBeaconUpdate();
void PathSetupEX(void *rt, s32 arg1);
void MovementGobjInit(void *rt, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
void PhysicsGobjBasicInit(void *rt, s32 arg1, s32 arg2);
void CollisionGobjBasicInit(void *rt, s32 arg1, s32 arg2, s32 arg3, s32 arg4, s32 arg5);
void LevelSetObjectiveInstructionsCallback(void (*cb)());
void LevelSetDirectionPointerFlag(u8 flag);
void ObjectInitData(void);
void WeaponSetTaserHitNpcCallback(void (*cb)());
void GameCameraSetDefaultPitchAngle(s32 angle);

void PathSetPositionToPoint(void *rt, s32 point);
s32 PathGetNumPoints(void *rt);
void InitBxv(void *box);
u8 PickupGetData(void);
u8 LevelGameObjectHasModel(s16 id);

void UseObjectTeleportCameraSetup(InitRuntime *rt);
void ObjectMakeMinimumBox(InitRuntime *rt, s32 w, s32 h, s32 d);
void ObjectRaiseTopOfBoxIfTooShort(InitRuntime *rt, s32 h);
static void func_80147F38(s16 id);


INCLUDE_RODATA("asm/init/nonmatchings/init", D_80145BF0);

INCLUDE_RODATA("asm/init/nonmatchings/init", D_80145C54);

/* Opens \<dir>\<name>.HOG;1; returns nonzero on success. */
static s32 func_80145F34(char *dir, char *name, s32 arg2, s32 arg3) {
    char path[32];

    sprintf(path, "\\%s\\%s.HOG;1", dir, name);
    return EGSHogOpen(path, arg3, arg2) == 0;
}

INCLUDE_ASM("asm/init/nonmatchings/init", func_80145F98);

/* Loads the level data and the level overlay. */
static void func_801462CC(void) {
    char path[32];
    char ovl[16];
    char *name;

    name = gLevelNames[gSysInfo.level];
    sprintf(path, "\\%s\\%s.DAT;1", name, name);
    gLevData = &levdata;
    SysLoadData(path, &gLevData, 0x800);
    sprintf(ovl, D_80145CD4, D_8014BA30[gSysInfo.level]);
    SysLoadOverlay(ovl, D_80140FE0, 1);
    EGSDisplaySetCollideXZ(gLevData->collideX, gLevData->collideZ);
}

INCLUDE_RODATA("asm/init/nonmatchings/init", D_80145CD4);

INCLUDE_ASM("asm/init/nonmatchings/init", LevelCreateGOBJ);

INCLUDE_ASM("asm/init/nonmatchings/init", func_801466C8);

/* Allocates the runtime ("gobj") of every visible placed object. */
static void func_80146B14(void) {
    InitRuntime *rt;
    s32 i;
    GameObjectInfo *info;

    rt = EGSMemMalloc(gLevelGameObjCount * sizeof(InitRuntime));
    if (rt == NULL) {
        EGSDebugAssert(1, 0, D_80145D14, 0x2AF);
    }
    for (i = 0; i < gLevelGameObjCount; i++) {
        rt->cas = NULL;
        rt->target = NULL;
        rt->anim = NULL;
        rt->move = NULL;
        rt->healthInfo = (GameObjectHealth *)gameObjectsInfo[i].unk38;
        if (!LevelGameObjectIsInvisibleClass(i)) {
            rt->id = i;
            rt->flags0 = 0;
            rt->flags1 = 0;
            rt->flags23 = 0;
            rt->flags20 = 6;
            rt->unk21 = 0;
            rt->npcState = 0;
            rt->unk04 = 0;
            rt->npc = NULL;
            gameObjectsInfo[i].runtime = (GameObjectRuntime *)rt;
            rt++;
        } else {
            info = &gameObjectsInfo[(s16)i];
            *(u16 *)info->unk26 = 0;
            info->flags24 = 0;
            info->runtime = NULL;
        }
    }
}

#ifdef NON_MATCHING
/* &hog is rematerialised from sp instead of kept in s3 */
/* Loads every texture of a common hog (\COMMON\INTRFACE or \COMMON\SPFX). */
static void func_80146C6C(char *name) {
    char unused[32];
    EGSTex tex;
    EGSHog hog;
    s32 count;
    s32 i;

    if (name[8] == 'I') {
        hog.data = gINTRFACEHog;
    } else {
        hog.data = gSPFXHog;
    }
    count = EGSHogGetBlockCount(&hog);
    for (i = 0; i < count; i++) {
        if (EGSHogGetBlock(&hog, i, &tex) != 0) {
            EGSDebugAssert(1, 0, D_80145D14, 0x306);
        }
        EGSTextureCreate(&tex);
        EGSTextureLoad(&tex);
    }
    if (strcmp(name, D_80145D20) == 0) {
        SpriteSetupInterface(&hog);
    } else if (strcmp(name, D_80145D34) == 0) {
        SetupMultiTextures(&hog, 5, gMultiTextures);
        SpriteCreateFootFx(&hog);
        ObjectTextureAnimationsHack();
    }
}
#else
INCLUDE_ASM("asm/init/nonmatchings/init", func_80146C6C);
#endif

INCLUDE_ASM("asm/init/nonmatchings/init", LevelLoadCallback);

/* Shows the mission briefing while the level loads. */
static void func_80146EA4(void) {
    s32 i;
    s32 count;
    s32 len;
    EGSSprite *sprites;

    InterfaceSetMode(6);
    gSpareTextWindow = TextAddWindow(-150, -100, 300, 200);
    TextSetWindowColor(gSpareTextWindow, 0x50, 0x82, 0xF0);
    for (i = 0; i < 3; i++) {
        TextAddString(gSpareTextWindow, gBriefings->lines[i], -1, NULL);
    }
    if (gInitHeap != NULL) {
        count = gBriefings->count;
        len = 0;
        if (count >= 3) {
            count = 2;
        }
        for (i = 0; i < count; i++) {
            len += strlen(gBriefings->objectives[i]);
        }
        sprites = gInitHeap;
        TextSpriteInitialize(sprites, len);
        for (i = 0; i < count; i++) {
            sprites += TextStringLength(TextAddString(gSpareTextWindow, gBriefings->objectives[i], -1, sprites), 0);
        }
        gInitHeap = sprites;
    }
}

#ifdef NON_MATCHING
/* constant folding: -2 and the end symbol are combined before the subtract */
/* Zeroes the memory between the heap and the overlay, top down. */
void ClearHeapBelow(s32 *top) {
    s32 *p;
    s32 count;
    s32 i;

    p = top - 1;
    count = ((s32)EGSMemAvail() >> 2) - 2 - (s32)((u32)D_5EE8 >> 2) - 0x1D4C0;
    for (i = 0; i < count; i++) {
        *p-- = 0;
    }
}
#else
INCLUDE_ASM("asm/init/nonmatchings/init", ClearHeapBelow);
#endif

INCLUDE_RODATA("asm/init/nonmatchings/init", D_80145D14);

INCLUDE_RODATA("asm/init/nonmatchings/init", D_80145D20);

INCLUDE_RODATA("asm/init/nonmatchings/init", D_80145D34);

INCLUDE_ASM("asm/init/nonmatchings/init", ENTEROverlay);

void LevelInitObjectives(void) {
    gBriefings = NULL;
    gObjectives = NULL;
}

void LevelSpecificInit(s32 arg0) {
    LevelSetEOLCallback(NULL);
    LevelSetObjectiveInstructionsCallback(NULL);
    LevelSetDirectionPointerFlag(0);
    ObjectInitData();
    WeaponSetWeaponFiredCallback(NULL);
    WeaponSetTaserHitNpcCallback(NULL);
    gAddingObjectiveViaQuip = 0;
    gHitNPCCallback = NULL;
    gGameCamScriptEndCallback = NULL;
    gLevelInnocentsKilled = 0;
    mMissionFailedMsgFlag = 0;
    mGameOverMsgFlag = 0;
    mNextTickAllowMsgMissionFailed = gExeTick;
    mNextTickAllowMsgIncompleteObjectives = gExeTick;
    mLastTickAddedObjectives = gExeTick;
    GameCameraSetDefaultPitchAngle(0x62);
    gDefaultLevelDmgFX = 2;
    gElectricFenceFlag = 1;
    D_80145C54[gSysInfo.level](arg0);
}

#ifdef NON_MATCHING
/* regalloc: box/size swap a0 and t0 */
/* Grows the collision box to at least w x h x d. */
void ObjectMakeMinimumBox(InitRuntime *rt, s32 w, s32 h, s32 d) {
    InitAnim *anim;
    s32 *box;
    s32 grow;

    anim = rt->anim;
    if (anim != NULL) {
        box = anim->box;
        if (box[4] - box[0] < w) {
            grow = (w - (box[4] - box[0])) >> 1;
            box[0] -= grow;
            box[4] += grow;
        }
        if (box[5] - box[1] < h) {
            grow = (h - (box[5] - box[1])) >> 1;
            box[1] -= grow;
            box[5] += grow;
        }
        if (box[6] - box[2] < d) {
            grow = (d - (box[6] - box[2])) >> 1;
            box[2] -= grow;
            box[6] += grow;
        }
    }
}
#else
INCLUDE_ASM("asm/init/nonmatchings/init", ObjectMakeMinimumBox);
#endif

#ifdef NON_MATCHING
/* regalloc: box/size swap a0 and t0 */
/* Shrinks the collision box to at most w x h x d. */
void ObjectMakeMaximumBox(InitRuntime *rt, s32 w, s32 h, s32 d) {
    InitAnim *anim;
    s32 *box;
    s32 shrink;

    anim = rt->anim;
    if (anim != NULL) {
        box = anim->box;
        if (w < box[4] - box[0]) {
            shrink = ((box[4] - box[0]) - w) >> 1;
            box[0] += shrink;
            box[4] -= shrink;
        }
        if (h < box[5] - box[1]) {
            shrink = ((box[5] - box[1]) - h) >> 1;
            box[1] += shrink;
            box[5] -= shrink;
        }
        if (d < box[6] - box[2]) {
            shrink = ((box[6] - box[2]) - d) >> 1;
            box[2] += shrink;
            box[6] -= shrink;
        }
    }
}
#else
INCLUDE_ASM("asm/init/nonmatchings/init", ObjectMakeMaximumBox);
#endif

INCLUDE_ASM("asm/init/nonmatchings/init", ObjectRaiseTopOfBoxIfTooShort);

/* Links a use object to the camera of its teleport target, if any. */
void UseObjectTeleportCameraSetup(InitRuntime *rt) {
    s16 teleport;
    s16 camera;

    rt->unk04 = -1;
    teleport = ObjectInvisibleMatchToRefID(rt->id, 0x77);
    if (rt->id != teleport && rt->npcState != 2) {
        camera = ObjectInvisibleMatchToRefID(teleport, 0x22);
        if (camera != teleport) {
            rt->unk04 = camera;
        }
    }
}

INCLUDE_ASM("asm/init/nonmatchings/init", func_80147F38);

void AUTOPICKUPObjInit(InitRuntime *rt) {
    GameObjectHealth *health;

    rt->npcState = 0;
    func_80147F38(rt->id);
    rt->flags1 = 0x40;
    rt->anim->flags08 |= 8;
    health = rt->healthInfo;
    rt->flags0 &= 0x9D;
    health->armor = 0x7FFF;
    health->health = 0x7FFF;
    gLevelKeycards++;
}

void BOMBObjInit(InitRuntime *rt) {
    rt->npcState = 0;
    LevelCreateGOBJ(rt->id, 0);
    TargetInitGobj(rt, target, 0, 0);
    rt->anim->unk09 = 0x40;
    rt->flags0 &= ~0x20;
    rt->flags1 &= ~0x80;
    rt->flags20 |= 0x80;
    SysExeTickQueueTimer(UseBeaconUpdate, 1, rt);
    UseObjectTeleportCameraSetup(rt);
}

void COMPUTERObjInit(InitRuntime *rt, s32 type) {
    rt->npcState = 0;
    LevelCreateGOBJ(rt->id, 0);
    rt->anim->unk09 = 0x40;
    rt->flags0 &= 0x16;
    if (type != 5) {
        rt->flags0 &= ~0x10;
    } else {
        rt->flags0 |= 0x10;
    }
    rt->flags20 |= 0x80;
    TargetInitGobj(rt, target, 0, 0);
    rt->cas = NULL;
    UseObjectTeleportCameraSetup(rt);
}

#ifdef NON_MATCHING
/* gameObjectsInfo load is hoisted out of the loop instead of reused from the refId read */
/* Finds the camera an object is linked to through at most two pass-through objects. */
static void func_801482D4(InitRuntime *rt) {
    GameObjectInfo *info;
    GameObjectRuntime *runtime;
    s16 id;
    s32 type;
    s32 depth;

    rt->unk04 = -1;
    id = gameObjectsInfo[rt->id].refId;
    depth = 1;
    while (id != -1) {
        info = &gameObjectsInfo[id];
        runtime = info->runtime;
        type = id != GOBJ_ID_666 ? gameObjectsModelInfo[info->modelIndex].type : GOBJ_ID_666;
        if (type == 0x22) {
            rt->unk04 = id;
            return;
        }
        if (runtime != NULL && type != 6 && type != 0x49 && type != 0x71 && type != 0x76 && type != 0x3E &&
            type != 0xA && type != 0x60 && type != 0x61 && type != 0x54 && type != 7 && type != 0x3B) {
            return;
        }
        if (++depth >= 3) {
            return;
        }
        id = gameObjectsInfo[id].refId;
    }
}
#else
INCLUDE_ASM("asm/init/nonmatchings/init", func_801482D4);
#endif

INCLUDE_ASM("asm/init/nonmatchings/init", DOORObjInit);

void EFFECTSObjInit(InitRuntime *rt, s32 type) {
    s32 n;

    rt->npcState = 0;
    LevelCreateGOBJ(rt->id, 0);
    rt->anim->flags08 |= 8;
    if (type == 0x75) {
        if (gameObjectsInfo[rt->id].pathStart != 0) {
            n = gameObjectsInfo[rt->id].pathStart;
        } else {
            n = 1;
        }
        rt->flags1 = n;
    }
}

void EFFECTS2ObjInit(InitRuntime *rt) {
    rt->npcState = 0;
    LevelCreateGOBJ(rt->id, 0);
    rt->flags0 &= ~0x40;
    rt->unk04 = gExeTick;
}

#ifdef NON_MATCHING
/* regalloc: flag bit computed straight into s1 instead of via v0 */
void ELEVATORObjInit(InitRuntime *rt, s32 type) {
    u8 hi;

    rt->npcState = 0xD;
    LevelCreateGOBJ(rt->id, 0);
    rt->anim->unk09 = 0x40;
    PathSetupEX(rt, 1);
    if (rt->move != NULL) {
        rt->flags1 |= 0x80;
        if (type != 0x40) {
            PathSetPositionToPoint(rt, 0);
        } else {
            PathSetPositionToPoint(rt, PathGetNumPoints(rt) - 1);
            rt->move[0x174 / 4] = 0;
            rt->move[0x17C / 4] = 1;
        }
        hi = rt->flags1 & 0x80;
        rt->flags1 = hi;
        if (hi) {
            rt->flags1 = hi | (PathGetNumPoints(rt) & 0x7F);
        } else {
            rt->flags1 = hi | 1;
        }
    } else {
        rt->flags1 &= ~0x80;
    }
    rt->flags0 &= ~0x0A;
    rt->flags0 |= 1;
    rt->flags0 &= ~0x80;
    if (type != 0xC) {
        rt->flags0 &= ~0x10;
        if (type == 0xD) {
            rt->flags0 &= ~4;
        }
    } else {
        rt->flags0 |= 0x10;
    }
    rt->unk04 = 0;
    rt->flags23 |= 4;
}
#else
INCLUDE_ASM("asm/init/nonmatchings/init", ELEVATORObjInit);
#endif

void FIREObjInit(InitRuntime *rt, s32 type) {
    rt->npcState = 0;
    LevelCreateGOBJ(rt->id, 0);
    rt->flags0 &= ~0x80;
    switch (type) {
    case 0x25:
    case 0x27:
    case 0x30:
    case 0x5A:
        rt->anim->flags0B |= 0x10;
        rt->anim->flags08 |= 8;
        break;
    }
    rt->unk04 = 0;
    rt->flags0 &= ~0x40;
}

void GENObjInit(InitRuntime *rt) {
    s32 type;

    type = GOBJ_TYPE(rt->id);
    rt->npcState = 0;
    LevelCreateGOBJ(rt->id, 0);
    switch (type) {
    case 0x11:
        rt->anim->unk09 = 0x40;
        rt->flags20 |= 0x80;
        break;
    case 0x4E:
        rt->anim->unk09 = 0x40;
        rt->flags20 |= 0x20;
        break;
    case 0x19:
        rt->anim->unk18 = 0;
        rt->anim->unk09 = 0x18;
        rt->flags20 |= 0x80;
        break;
    }
}

void GLASSObjInit(InitRuntime *rt, s32 type) {
    GameObjectHealth *health;

    LevelCreateGOBJ(rt->id, 0);
    rt->npcState = 0xA;
    rt->cas = NULL;
    rt->flags0 &= 0x7D;
    rt->flags1 &= 0x7F;
    if (type == 0x20) {
        InitBxv(rt->anim->box);
        *(s16 *)((u8 *)rt->anim->box + 0x3A) = 0x180;
    }
    if (type != 0x4D && type != 0x70) {
        if (type != 0x51 && type != 0x52) {
            rt->flags0 &= ~1;
        } else {
            rt->flags0 |= 1;
        }
        rt->anim->unk18 = 0;
        rt->anim->unk09 = 0x14;
    } else {
        rt->flags0 |= 1;
        rt->anim->unk18 = 0;
        rt->anim->unk09 = 0x17;
        rt->flags20 |= 0x20;
    }
    if (rt->anim->unk0A & 8) {
        rt->flags20 |= 0x80;
    } else {
        rt->flags20 &= 0x7F;
    }
    if (type != 0x56) {
        TargetInitGobj(rt, target, 0, 0);
    } else {
        health = rt->healthInfo;
        health->armor = 0x7FFF;
        health->health = 0x7FFF;
    }
    ObjectMakeMinimumBox(rt, 0x20, 0x10, 0x20);
}

INCLUDE_ASM("asm/init/nonmatchings/init", INTERACTObjInit);

void LASERObjInit(InitRuntime *rt) {
    GameObjectHealth *health;

    rt->npcState = 5;
    LevelCreateGOBJ(rt->id, 0);
    PathSetupEX(rt, 1);
    health = rt->healthInfo;
    rt->flags0 &= 0x3F;
    health->armor = 0x7FFF;
    health->health = 0x7FFF;
    rt->anim->unk18 = 0;
    rt->anim->unk09 = 0x17;
}

INCLUDE_ASM("asm/init/nonmatchings/init", LIGHTObjInit);

void PICKUPObjInit(InitRuntime *rt) {
    GameObjectHealth *health;

    rt->npcState = 0xB;
    if (*(s32 *)&gameObjectsInfo[rt->id].flags24 != 0) {
        rt->flags1 = PickupGetData();
        *(s32 *)&gameObjectsInfo[rt->id].flags24 = 0;
    }
    if (LevelGameObjectHasModel(rt->id)) {
        LevelCreateGOBJ(rt->id, 0);
        if (GOBJ_TYPE(rt->id) != 0x4F) {
            rt->anim->unk09 = 0x40;
        } else {
            rt->anim->unk18 = 0;
            rt->anim->unk09 = 0x10;
        }
        ObjectRaiseTopOfBoxIfTooShort(rt, 0x30);
    }
    health = rt->healthInfo;
    rt->flags0 &= ~0x20;
    health->armor = 0x7FFF;
    health->health = 0x7FFF;
    UseObjectTeleportCameraSetup(rt);
}

void ROTATINGObjInit(InitRuntime *rt) {
    rt->npcState = 0;
    LevelCreateGOBJ(rt->id, 0);
    rt->anim->unk09 = 0x40;
    MovementGobjInit(rt, 0, 0, 0, 0);
    PhysicsGobjBasicInit(rt, 0x2000, 1);
    CollisionGobjBasicInit(rt, rt->move[1], 0x666, 0x1000, 0x800, 0);
}

void SWITCHObjInit(InitRuntime *rt, s32 type) {
    GameObjectHealth *health;

    rt->npcState = 0;
    LevelCreateGOBJ(rt->id, 0);
    if (type != 0x79 && type != 0x7A && type != 0x7B && type != 0x7C) {
        rt->anim->unk09 = 0x40;
    } else {
        rt->anim->unk18 = 0;
        rt->anim->unk09 = 0x10;
    }
    rt->flags0 &= 0x9F;
    if (type != 0x1C && type != 0x7A) {
        rt->flags0 &= ~0x10;
    } else {
        rt->flags0 |= 0x10;
    }
    if (type != 0x3A) {
        health = rt->healthInfo;
        rt->flags20 |= 0x80;
        health->armor = 0x7FFF;
        health->health = 0x7FFF;
    }
    if (type == 0x3A || (type != 0x1C && type != 0x7A && type != 0x24 && type != 0x7B && type != 0x79 &&
                         type != 0x7C && type != 0x6A)) {
        TargetInitGobj(rt, target, 0, 0);
    }
    ObjectMakeMinimumBox(rt, 0x20, 0x20, 0x10);
    if (type != 0x41 && type != 0x6A && type != 0x3A) {
        UseObjectTeleportCameraSetup(rt);
    } else {
        rt->unk04 = -1;
    }
}

INCLUDE_ASM("asm/init/nonmatchings/init", ObjectSetupCar);

INCLUDE_ASM("asm/init/nonmatchings/init", TGTObjInit);

/* Clears count animation slots. */
void InitAnimStruct(InitAnimSlot *slots, s32 count) {
    s32 i;

    for (i = 0; i < count; i++) {
        slots[i].active = 0;
        slots[i].unk0C = 0;
        slots[i].unk10 = 0;
        slots[i].unk04 = 0;
        slots[i].unk08 = 0;
        slots[i].unk18 = 0;
        slots[i].unk1C = 0;
    }
}

INCLUDE_ASM("asm/init/nonmatchings/init", CASAnimInit);
