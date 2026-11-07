#include "common.h"
#include "game.h"
#include "system.h"
#include "level.h"
#include "scratch.h"
#include "gobj.h"
#include "egsmath.h"

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
    /* 0x14 */ s16 unk14;
    /* 0x16 */ u8 unk16[2];
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
    /* 0x14 */ u8 unk14;
    /* 0x15 */ u8 unk15;
    /* 0x18 */ u32 unk18;
    /* 0x1C */ void (*unk1C)();
} InitAnimSlot; /* size 0x20 */

/* Level data loaded from \<LEVEL>\<LEVEL>.DAT */
typedef struct LevelData {
    /* 0x00 */ u8 unk00;
    /* 0x01 */ u8 fogColor;
    /* 0x02 */ s16 fogDist;
    /* 0x04 */ u8 unk04[7];
    /* 0x0B */ u8 terrain;
    /* 0x0C */ u8 unk0C[0x42];
    /* 0x4E */ s16 collideX;
    /* 0x50 */ s16 collideZ;
    /* 0x52 */ u8 unk52[0x2A];
    /* 0x7C */ s32 gabeSector;
} LevelData;

extern s32 gInterfaceCurrentMode;
void TextUpdate(void);
void InterfaceAnimate(void);
void EGSDisplaySort(s32 arg0, s32 arg1);
void TextSetStringIntensity(u16 str, s32 r, s32 g, s32 b);

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
u8 IsWideDoor(void *rt);
s32 PathGetVelocity(void *rt);
s32 PathGetNumPoints(void *rt);
void InitBxv(void *box);
u8 PickupGetData(void);
u8 LevelGameObjectHasModel(s16 id);

void LevelGetModel(char *name, s32 *model);
char *strcat(char *, const char *);
extern s32 gPermHogFile;
s32 EGSHogGetBlockByName(s32 hog, char *name, s32 *block);
void EGSAnimHMDCreate(s32 model, s32 hmd, s32 arg2, s32 arg3, InitAnim **anim, s32 arg5);
void FixupEMDGameObject(s32 model, InitAnim *anim);
void EGSAnimSimpleCreate(s32 model, GOBJ_MATRIX *mat, s32 flags, InitAnim **anim);
void LevelGetGameObjectStartPosition(s16 id, VECTOR32 *pos);
void LevelGetGameObjectMatrix(s16 id, GOBJ_MATRIX *mat);
void EGSFrameSetPosition(void *frame, s32 arg1, VECTOR32 *pos);
void EGSFrameSetRotationMatrix(void *frame, s32 arg1, GOBJ_MATRIX *mat);
void EGSFrameUpdateHierarchy(void *frame);

void UseObjectTeleportCameraSetup(InitRuntime *rt);
void ObjectMakeMinimumBox(InitRuntime *rt, s32 w, s32 h, s32 d);
void ObjectRaiseTopOfBoxIfTooShort(InitRuntime *rt, s32 h);
static void func_80147F38(s16 id);


static const s32 initHeader[5] = {0, 1, 1, 1, 0};

/* overlay name per level */
static char *D_8014BA30[20] = {
    "SUBWAY", "SUBWAY2", "SUBWAY",  "PARK",    "PARK2",   "MUSEUM",  "MUSEUM",
    "BASEEXT", "BASEEXT", "CHOPPER", "BASEEXT", "LEVSPEC", "LEVSPEC", "LEVSPEC",
    "WHOUSE", "WHOUSE", "CAVE",    "CAVE",    "CAVE",    "SUBWAY",
};

/* briefing per level, in the level overlays (shared addresses: absolute) */
static Briefing *D_8014BA80[20] = {
    (Briefing *)0x80145584, /* subwayBriefings */
    (Briefing *)0x80142048, /* subway2Briefings */
    (Briefing *)0x801455A0, /* subway3Briefings */
    (Briefing *)0x80142848, /* parkBriefings */
    (Briefing *)0x80143354, /* park2Briefings */
    (Briefing *)0x80143DC4, /* museumBriefings */
    (Briefing *)0x80143DF4, /* museum2Briefings */
    (Briefing *)0x80144A0C, /* baseextBriefings */
    (Briefing *)0x80144A28, /* bunkerBriefings */
    (Briefing *)0x801454D8, /* chopperPreSwitchBriefings */
    (Briefing *)0x80144A44, /* baseext2Briefings */
    (Briefing *)0x801429A0, /* churchBriefings */
    (Briefing *)0x801429B8, /* church2Briefings */
    (Briefing *)0x801429D4, /* catacombsBriefings */
    (Briefing *)0x801426B4, /* whouseBriefings */
    (Briefing *)0x801426CC, /* inwhouseBriefings */
    (Briefing *)0x80143C00, /* caveBriefings */
    (Briefing *)0x80143C1C, /* cave2Briefings */
    (Briefing *)0x80143C38, /* siloBriefings */
    (Briefing *)0x80145584, /* subwayBriefings */
};

u16 gLoadingString = 0xFFFF;
static s32 D_8014BAD4 = 0; /* in LevelLoadCallback */

/* level init function per level, in the level overlays (absolute) */
static void (*const D_80145C54[20])(s32) = {
    (void (*)(s32))0x80142E90, /* iSubwayInit */
    (void (*)(s32))0x80141C74, /* iSubway2Init */
    (void (*)(s32))0x80144814, /* iSubway3Init */
    (void (*)(s32))0x801420F0, /* iParkInit */
    (void (*)(s32))0x80141578, /* iPark2Init */
    (void (*)(s32))0x80141F94, /* iMuseumInit */
    (void (*)(s32))0x80142E88, /* iMuseum2Init */
    (void (*)(s32))0x80142B0C, /* iBaseextInit */
    (void (*)(s32))0x80143CBC, /* iBunkerInit */
    (void (*)(s32))0x80144BD0, /* iChopperLevelInit */
    (void (*)(s32))0x80143258, /* iBaseext2Init */
    (void (*)(s32))0x80142674, /* iChurchInit */
    (void (*)(s32))0x80142674, /* iChurchInit */
    (void (*)(s32))0x80141F28, /* iCatacombsInit */
    (void (*)(s32))0x80141E18, /* iWhouseInit */
    (void (*)(s32))0x80141620, /* iInwhouseInit */
    (void (*)(s32))0x80141D48, /* iCaveInit */
    (void (*)(s32))0x80141F48, /* iCave2Init */
    (void (*)(s32))0x80142EFC, /* iSiloInit */
    (void (*)(s32))0x80142E90, /* iSubwayInit */
};

/* Opens \<dir>\<name>.HOG;1; returns nonzero on success. */
static s32 func_80145F34(char *dir, char *name, s32 arg2, s32 arg3) {
    char path[32];

    sprintf(path, "\\%s\\%s.HOG;1", dir, name);
    return EGSHogOpen(path, arg3, arg2) == 0;
}

/* Header of \<LEVEL>\<LEVEL>.BIN; the table fields are offsets from the start of the file. */
typedef struct WorldBin {
    /* 0x00 */ s32 unk00;
    /* 0x04 */ s32 numSectors;
    /* 0x08 */ s32 objCount;
    /* 0x0C */ s32 uniqueObjCount;
    /* 0x10 */ s32 modelInfo;
    /* 0x14 */ s32 objInfo;
    /* 0x18 */ s32 objActivation;
    /* 0x1C */ s32 playerOffset;
    /* 0x20 */ s32 generatorCount;
    /* 0x24 */ s32 generators;
    /* 0x28 */ s32 generatorActivation;
} WorldBin;

typedef struct LevelLoadHeader {
    /* 0x00 */ s32 permHog;
    /* 0x04 */ u8 unk04[8];
    /* 0x0C */ s32 worldOffset;
    /* 0x10 */ u8 unk10[0x20];
    /* 0x30 */ s32 base;
} LevelLoadHeader;

/* GameObjectModelInfo as relocated at load time. */
typedef struct InitModelRelocs {
    /* 0x00 */ s32 unk00;
    /* 0x04 */ s32 name;
    /* 0x08 */ s32 unk08;
    /* 0x0C */ s32 unk0C;
    /* 0x10 */ s32 unk10;
} InitModelRelocs; /* size 0x14 */

typedef struct NPCGenerator {
    /* 0x00 */ s32 unk00;
    /* 0x04 */ u8 count;
    /* 0x05 */ u8 unk05[3];
    /* 0x08 */ s32 *list;
    /* 0x0C */ s32 unk0C;
} NPCGenerator; /* size 0x10 */

typedef struct InitActivationList {
    /* 0x00 */ s32 count;
    /* 0x04 */ s32 *ids;
} InitActivationList;

extern LevelLoadHeader *levelLoadHeader;
extern WorldBin *gWorldBin;
extern s32 gNumSectors;
extern s32 gLevelUniqueGameObjCount;
extern s32 gNPCGeneratorCount;
extern NPCGenerator *generatorInfo;
extern InitActivationList *objActivationList;
extern InitActivationList *generatorActivationList;
void NPCInitTerrainGenerator(s32 index);

#ifdef NON_MATCHING
/* scheduling: generatorInfo = NULL store hoisted above the objActivationList store; inner j in a1 instead of a0 */
/* Loads (or locates, when preloaded) the level's world file and relocates its tables. */
static void func_80145F98(u8 preloaded) {
    char path[32];
    s32 base;
    s32 i;
    s32 j;
    InitModelRelocs *mi;
    GameObjectInfo *info;
    NPCGenerator *gen;
    InitActivationList *obj;
    InitActivationList *genAct;
    char *name;

    if (preloaded) {
        gWorldBin = (WorldBin *)(levelLoadHeader->base + levelLoadHeader->worldOffset);
        base = (s32)gWorldBin;
    } else {
        name = gLevelNames[gSysInfo.level];
        sprintf(path, "\\%s\\%s.BIN;1", name, name);
        SysLoadData(path, &gWorldBin, 0);
        base = (s32)gWorldBin;
    }
    gNumSectors = gWorldBin->numSectors;
    gLevelGameObjCount = gWorldBin->objCount;
    gLevelUniqueGameObjCount = gWorldBin->uniqueObjCount;
    gPlayerOffset = gWorldBin->playerOffset;
    gameObjectsModelInfo = (GameObjectModelInfo *)(base + gWorldBin->modelInfo);
    for (i = 0; i < gLevelUniqueGameObjCount; i++) {
        mi = &((InitModelRelocs *)gameObjectsModelInfo)[i];
        mi->name = base + mi->name;
        mi->unk0C = base + mi->unk0C;
    }
    gameObjectsInfo = (GameObjectInfo *)(base + gWorldBin->objInfo);
    for (i = 0; i < gLevelGameObjCount; i++) {
        info = &gameObjectsInfo[i];
        if (info->path != 0) {
            info->path = base + info->path;
        }
    }
    objActivationList = (InitActivationList *)(base + gWorldBin->objActivation);
    generatorInfo = NULL;
    gNPCGeneratorCount = gWorldBin->generatorCount;
    generatorActivationList = (InitActivationList *)(base + gWorldBin->generatorActivation);
    if (gNPCGeneratorCount > 0) {
        generatorInfo = (NPCGenerator *)(base + gWorldBin->generators);
    }
    for (i = 0; i < gNPCGeneratorCount; i++) {
        gen = &generatorInfo[i];
        NPCInitTerrainGenerator(i);
        gen->list = (s32 *)(base + (s32)gen->list);
        gen->unk0C = base + gen->unk0C;
        for (j = 0; j < gen->count; j++) {
            gen->list[j] = base + gen->list[j];
        }
    }
    for (i = 0; i < gNumSectors; i++) {
        obj = &objActivationList[i];
        genAct = &generatorActivationList[i];
        obj->ids = (s32 *)(base + (s32)obj->ids);
        genAct->ids = (s32 *)(base + (s32)genAct->ids);
        for (j = 0; j < obj->count; j++) {
            obj->ids[j] = (s32)&gameObjectsInfo[obj->ids[j]];
        }
    }
}
#else
INCLUDE_ASM("asm/init/nonmatchings/init", func_80145F98);
#endif

/* Loads the level data and the level overlay. */
static void func_801462CC(void) {
    char path[32];
    char ovl[16];
    char *name;

    name = gLevelNames[gSysInfo.level];
    sprintf(path, "\\%s\\%s.DAT;1", name, name);
    gLevData = &levdata;
    SysLoadData(path, &gLevData, 0x800);
    sprintf(ovl, "%s.OVL", D_8014BA30[gSysInfo.level]);
    SysLoadOverlay(ovl, D_80140FE0, 1);
    EGSDisplaySetCollideXZ(gLevData->collideX, gLevData->collideZ);
}

/* GameObjectModelInfo as seen by LevelCreateGOBJ. */
typedef struct InitModelInfo {
    /* 0x00 */ s16 type;
    /* 0x02 */ u8 unk02[2];
    /* 0x04 */ char *name;
    /* 0x08 */ s32 model;
    /* 0x0C */ char *xname;
    /* 0x10 */ s32 xmodel;
} InitModelInfo;

/* offset + base in that order: the original adds the scaled index first */
#define GOBJ_MODEL_AT(i) ((GameObjectModelInfo *)((i) * sizeof(GameObjectModelInfo) + (u32)gameObjectsModelInfo))

/* Creates the model ("gobj" anim) of a placed object; hmd is the HMD block for animated models, or 0. */
void LevelCreateGOBJ(s16 id, s32 hmd) {
    GOBJ_MATRIX mat;
    VECTOR32 pos;
    s32 han;
    InitAnim *anim;
    InitRuntime *rt;
    GameObjectInfo *info;
    GameObjectModelInfo *mi;
    s32 model;
    s32 xmodel;
    s32 idx;
    void *frame;
    char c;

    idx = gameObjectsInfo[id].modelIndex;
    mi = GOBJ_MODEL_AT(idx);
    rt = (InitRuntime *)gameObjectsInfo[id].runtime;
    if (mi->model[0] == 0) {
        rt->anim = NULL;
        return;
    }
    model = ((InitModelInfo *)mi)->model;
    xmodel = ((InitModelInfo *)mi)->xmodel;
    if (model != 0) {
        c = mi->model[strlen(mi->model) - 3];
        if (c == 'H') {
            if (hmd == 0) {
                GameObjectModelInfo *m = GOBJ_MODEL_AT(idx);

                m->model[strlen(m->model) - 4] = 0;
                strcat(gameObjectsModelInfo[idx].model, ".HAN");
                EGSHogGetBlockByName(gPermHogFile, gameObjectsModelInfo[idx].model, &han);
                EGSAnimHMDCreate(model, han, 0, 0, &anim, 0);
                anim->flags08 |= 0x20;
            } else {
                EGSAnimHMDCreate(model, hmd, 0, 1, &anim, 5);
            }
        } else if (c == 'E') {
            EGSAnimSimpleCreate(model, &gameObjectsInfo[id].matrix, 0x01100000, &anim);
            FixupEMDGameObject(model, anim);
            if (xmodel != 0) {
                FixupEMDGameObject(xmodel, anim);
            }
        } else {
            info = &gameObjectsInfo[id];
            *(s32 *)&info->flags24 = 0;
            EGSAnimSimpleCreate(model, &info->matrix, 0x04000000, &anim);
        }
        if (anim != NULL) {
            frame = anim->frame;
            anim->unk14 = id + 0x2000;
            LevelGetGameObjectStartPosition(id, &pos);
            EGSFrameSetPosition(frame, 0, &pos);
            LevelGetGameObjectMatrix(id, &mat);
            EGSFrameSetRotationMatrix(frame, 0, &mat);
            EGSFrameUpdateHierarchy(frame);
        }
    } else {
        anim = NULL;
    }
    rt->anim = anim;
    if (anim != NULL && xmodel != 0) {
        anim->box[9] = xmodel;
        rt->anim->unk0A |= 8;
    }
    rt->healthInfo = (GameObjectHealth *)gameObjectsInfo[rt->id].unk38;
}

void EGSDisplayRegisterInfraredModel(s32 model);

/* Finds each model in the permanent hog: tries <name>.GMD, then .HMD, then .EMD. */
#define FIND_MODEL(name, model)                                                              \
    mi = MODEL_AT(i);                                                                        \
    if (*mi->name != 0) {                                                                    \
        mi->name[strlen(mi->name) - 4] = 0;                                                  \
        strcat(MODELS[i].name, ".GMD");                                                      \
        if (EGSHogGetBlockByName(gPermHogFile, MODELS[i].name, &MODELS[i].model) != 0) {     \
            InitModelInfo *m = MODEL_AT(i);                                                  \
            m->name[strlen(m->name) - 4] = 0;                                                \
            strcat(MODELS[i].name, ".HMD");                                                  \
            if (EGSHogGetBlockByName(gPermHogFile, MODELS[i].name, &MODELS[i].model) != 0) { \
                InitModelInfo *m = MODEL_AT(i);                                              \
                m->name[strlen(m->name) - 4] = 0;                                            \
                strcat(MODELS[i].name, ".EMD");                                              \
                EGSHogGetBlockByName(gPermHogFile, MODELS[i].name, &MODELS[i].model);        \
            }                                                                                \
        }                                                                                    \
    }

/* Looks up the level's models and resets the level specific NPCs. */
static void func_801466C8(void) {
    s32 ir;
    s32 err;
    InitModelInfo *mi;
    s16 i;

    gLevelSpecificNpcID = -1;
    gLevelSpecificNpcID2 = -1;
    gLevelSpecificNpcName = NULL;
    gLevelSpecificNpc2Name = NULL;
    if (gSysInfo.level == 3) {
        err = EGSHogGetBlockByName(gPermHogFile, "TERROX.HMD", &ir);
    } else {
        err = EGSHogGetBlockByName(gPermHogFile, "IRMODEL.HMD", &ir);
    }
    if (err == 0) {
        EGSDisplayRegisterInfraredModel(ir);
    }
#define MODELS ((InitModelInfo *)gameObjectsModelInfo)
/* offset + base in that order: the original adds the scaled index first */
#define MODEL_AT(i) ((InitModelInfo *)((i) * sizeof(InitModelInfo) + (u32)gameObjectsModelInfo))
    for (i = 0; i < gLevelUniqueGameObjCount; i++) {
        FIND_MODEL(name, model);
        FIND_MODEL(xname, xmodel);
    }
#undef MODELS
    for (i = 0; i < gLevelGameObjCount; i++) {
        SysPostMessage(2, 5, 0xFFFE, i, 0, 0, 0, 0);
    }
}

/* Allocates the runtime ("gobj") of every visible placed object. */
/* named: the asm func_80146C6C uses them too */
static const char D_80145D14[] = "bin/init.c";
static const char D_80145D20[] = "\\COMMON\\INTRFACE";
static const char D_80145D34[] = "\\COMMON\\SPFX";

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

/* Loads every texture of a common hog (\COMMON\INTRFACE or \COMMON\SPFX). */
static void func_80146C6C(char *name) {
    char unused[32];
    EGSTex tex;
    EGSHog hog;
    EGSHog *h;
    s32 count;
    s32 i;

    if (name[8] == 'I') {
        hog.data = gINTRFACEHog;
    } else {
        hog.data = gSPFXHog;
    }
    h = &hog;
    count = EGSHogGetBlockCount(h);
    i = 0;
    if (count > 0) {
        do {
            if (EGSHogGetBlock(h, i, &tex) != 0) {
                EGSDebugAssert(1, 0, D_80145D14, 0x306);
            }
            EGSTextureCreate(&tex);
            EGSTextureLoad(&tex);
        } while (++i < count);
    }
    if (strcmp(name, D_80145D20) == 0) {
        SpriteSetupInterface(h);
    } else if (strcmp(name, D_80145D34) == 0) {
        SetupMultiTextures(h, 5, gMultiTextures);
        SpriteCreateFootFx(h);
        ObjectTextureAnimationsHack();
    }
}

/* Per-tick callback while the level loads: animates the interface and the "loading" text. */
void LevelLoadCallback(void) {
    if (D_8014BAD4 == 0) {
        D_8014BAD4 = 1;
        gSystemTick++;
        TextUpdate();
        if (gInterfaceCurrentMode == 9) {
            SPAD_ENTER();
            InterfaceAnimate();
            SPAD_LEAVE();
        }
        SPAD_ENTER();
        EGSDisplaySort(1, 0x29A);
        SPAD_LEAVE();
        if ((gSystemTick & 0xF) == 0 && gLoadingString != 0xFFFF) {
            if (gSystemTick & 0x10) {
                TextSetStringIntensity(gLoadingString, 0x20, 0xFF, 0x90);
            } else {
                TextSetStringIntensity(gLoadingString, 0x50, 0x70, 0xFF);
            }
        }
        D_8014BAD4 = 0;
    }
}

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

/* Zeroes the memory between the heap and the overlay, top down. */
void ClearHeapBelow(s32 *top) {
    s32 *p;
    s32 count;
    s32 i;

    p = top - 1;
    count = ((s32)EGSMemAvail() >> 2) - 2;
    count -= (u32)D_5EE8 >> 2;
    count -= 0x1D4C0;
    for (i = 0; i < count; i++) {
        *p-- = 0;
    }
}


typedef struct EnterArgs {
    /* 0x00 */ u8 unk00[0xC];
    /* 0x0C */ s32 mode; /* 1: front end, 3: restart the level */
} EnterArgs;

typedef struct InitRect {
    s16 x, y, w, h;
} InitRect;

extern u8 D_8012D934[];
extern u8 gPCHan[];
extern EGSHog gFakeHanHog;
extern EGSHog *gHanHogFile;
extern EGSHog hanLvlHogFile;
extern EGSHog *gHanLvlHogFile;
extern EGSHog permHogFile;
extern void *gLevelStartHeap;
extern char *gLoadedStringFmt;
extern char *gLoadingStringFmt;
extern char *gLevelPrettyNames[];
extern u8 gStartedEOLFade;
extern s16 gGabeSector;
void GameCameraInit(void);
void TextInitialize(s32 arg0);
void InterfaceSetup(void);
void EGSDisplayCameraMalloc(void *buf, s32 size);
void LoadPlayerAnimations(s32 arg0, ...);
void SetupSound(s32 arg0);
void SysInitMenuSoundBank(void);
void *EGSMemGetHeap(void);
void InitStreamRequests(void);
void EGSFileOpenFog(char *path);
void InitStreaming(void);
u16 TextAddPersistentString(char *str, EGSSprite *sprites, s32 x, s32 y);
u16 TextReplaceString(u16 id, char *str);
void EGSDeviceCounterSetPeriodicCallback(void (*cb)(void), s32 period, s32 *id);
void EGSDeviceCounterUnsetPeriodicCallback(s32 id);
void SysInitSound(char *path, s32 arg1, void *arg2, s32 arg3);
void LoadImage(InitRect *rect, void *data);
void EGSHogClose(void *hog);
void EGSMemFree(void *p);
void EGSDisplaySetTerrainFog(s32 dist, s32 color);
void ParticleInit(void);
void CASAnimInit(void);
void NPCInit(void);
void TargetInit(void);
void SetupTerrain(s32 arg0, s32 arg1);
void GameCameraScrimInit(void);
void DamageInit(void);
void NPCInitData(void);
void PickupInitData(void);
void FixupTerrain(void);
void CobjInit(s32 arg0);
void SysExeTickQueueInit(void);
void LevelSetupLights(void);
void CollisionInit(void);
void AnimateInit(void);
void StringInitMessages(void);
void GameCameraActivateAndDisplay(s32 arg0, GameObjectRuntime *rt);
void LevelPlaySOLMovie(void);

/* Level overlay entry: loads (or reloads) the level and sets up every subsystem. */
void ENTEROverlay(EnterArgs *args) {
    char path[32];
    u8 sound[32];
    char str[80];
    s32 callback;
    s32 *hog;
    s32 mode;
    s32 i;
    u32 *data;
    u32 pages;
    s32 page;
    s32 *file;
    u8 reload;
    EGSSprite *sprites;

    callback = 0;
    mode = args->mode;
    gSystemTick += 10;
    GameCameraInit();
    TextInitialize(1);
    InterfaceSetup();
    EGSDisplayCameraMalloc(D_8012D934, 500);
    SysStatePush(1);
    EGSDisplayFadeOff();
    gSysMovieDoneCallback = NULL;
    gBriefings = D_8014BA80[gSysInfo.level];
    if (mode == 1) {
        func_80146C6C(D_80145D20);
        func_80146C6C(D_80145D34);
        gFakeHanHog.data = gPCHan;
        gHanHogFile = &gFakeHanHog;
        LoadPlayerAnimations(1);
        SetupSound(1);
        SysInitMenuSoundBank();
        gLevelStartHeap = EGSMemGetHeap();
    } else if (mode == 3) {
        if (gLevelRestartHeap == NULL) {
            EGSDebugAssert(1, 0, D_80145D14, 0x3E4);
        }
        func_801462CC();
        SysPlaySong(3, 0, 0, 0);
        EGSMemSetHeap(gLevelRestartHeap);
        InitStreamRequests();
        gLevelReady = 1;
    } else {
        sprintf(str, "\\FOG\\%s.FOG;1", gLevelNames[gSysInfo.level]);
        EGSFileOpenFog(str);
        sprintf(str, "%s.OVL", D_8014BA30[gSysInfo.level]);
        SysLoadOverlay(str, D_80140FE0, 1);
        InitStreaming();
        sprites = gInitHeap;
        sprintf(str, gLoadedStringFmt, gLevelPrettyNames[gSysInfo.level]);
        TextSpriteInitialize(sprites, strlen(str));
        gLoadingString = TextAddPersistentString(str, sprites, -170, 90);
        sprites += TextStringLength(gLoadingString, 1);
        gInitHeap = sprites;
        TextSetStringIntensity(gLoadingString, 0x20, 0xFF, 0x90);
        sprintf(str, gLoadingStringFmt, gLevelPrettyNames[gSysInfo.level]);
        gLoadingString = TextReplaceString(gLoadingString, str);
        func_80146EA4();
        EGSDeviceCounterSetPeriodicCallback(LevelLoadCallback, 3, &callback);
        EGSMemSetHeap(gLevelStartHeap);
        ClearHeapBelow(gLevelStartHeap);
        sprintf(path, "\\%s\\SLF.RFF;1", gLevelNames[gSysInfo.level]);
        SysInitSound(path, 0, sound, 0);
        SysPlaySong(3, 0, 0, 0);
        sprintf(path, "\\%s\\VLF.RFF;1", gLevelNames[gSysInfo.level]);
        if (EGSHogOpen(path, 0, (s32)&hog) != 0) {
            EGSDebugAssert(1, 0, D_80145D14, 0x444);
        }
        pages = *(u32 *)*hog;
        page = 0;
        data = (u32 *)*hog;
        do {
            InitRect rect = { 0, 0, 64, 256 };

            if (pages & 1) {
                i = page;
                if ((page & 0xF) < 6) {
                    i = page + 6;
                }
                rect.x = (i & 0xF) << 6;
                if (i >= 0x10) {
                    rect.y = 256;
                } else {
                    rect.y = 0;
                }
                LoadImage(&rect, data);
                DrawSync(0);
                data += 0x2000;
                gTexturePageMap[page] = 1;
            }
            page++;
            pages >>= 1;
        } while (page < 32);
        {
            InitRect rect = { 0x300, 0x1E0, 0x100, 0x20 };

            LoadImage(&rect, data);
            DrawSync(0);
        }
        EGSHogClose(&hog);
        sprintf(path, "\\%s\\DLF.RFF;1", gLevelNames[gSysInfo.level]);
        if (EGSHogOpen(path, 0, (s32)&hog) != 0) {
            EGSDebugAssert(1, 0, D_80145D14, 0x470);
        }
        file = (s32 *)*hog;
        levelLoadHeader = (LevelLoadHeader *)((u8 *)file + file[11]);
        EGSMiscMemcpy(levelLoadHeader, file, 0x2E);
        levelLoadHeader->base = (s32)file;
        EGSMiscMemcpy(&levdata, (u8 *)file + file[4], 0x200);
        gLevData = &levdata;
        EGSDisplaySetCollideXZ(levdata.collideX, levdata.collideZ);
        permHogFile.data = (u8 *)file + file[0];
        gPermHogFile = (s32)&permHogFile;
        if (file[1] != file[2]) {
            hanLvlHogFile.data = (u8 *)file + file[1];
            gHanLvlHogFile = &hanLvlHogFile;
            LoadPlayerAnimations(0);
        }
        gLevelRestartHeap = (void *)(levelLoadHeader->base + levelLoadHeader->permHog);
        EGSMemFree(gLevelRestartHeap);
    }
    EGSDisplaySetTerrainFog(gLevData->fogDist != 0 ? gLevData->fogDist : 0x7FF, gLevData->fogColor);
    if (mode != 1) {
        if (mode == 3) {
            ClearHeapBelow(gLevelRestartHeap);
        }
        reload = mode != 3;
        func_80145F98(reload);
        EGSMemFree(gWorldBin);
        ClearHeapBelow((s32 *)gWorldBin);
        ParticleInit();
        CASAnimInit();
        NPCInit();
        TargetInit();
        SetupTerrain(gLevData->terrain, reload);
        GameCameraScrimInit();
        DamageInit();
        NPCInitData();
        PickupInitData();
        if (mode == 3) {
            FixupTerrain();
        }
        gGabeSector = gLevData->gabeSector;
        CobjInit(0);
        gExeTick = 0;
        SysExeTickQueueInit();
        func_80146B14();
        func_801466C8();
        LevelSetupLights();
        CollisionInit();
        gStartedEOLFade = 0;
        gInEOLState = 0;
        AnimateInit();
        StringInitMessages();
        GameCameraActivateAndDisplay(1, gameObjectsInfo[gPlayerOffset].runtime);
        GameCameraRequestCutToBehind();
        gLevelAlerted = 0;
        gMissionFailed = 0;
        SysPostMessage(1, 5, 0xFFFE, 0xFFFE, 0, 0, 0, 0);
    }
    if (callback != 0) {
        EGSDeviceCounterUnsetPeriodicCallback(callback);
        callback = 0;
    }
    SysStatePop();
    if (mode != 1) {
        LevelSpecificInit(mode);
        GameSysSetLetterBoxMode(3);
        if (mode != 3) {
            EGSDisplayQueueInterrupt(LevelPlaySOLMovie, 2, 0);
        } else {
            EGSDisplayFadeFull();
            EGSDisplayFade(-1, 15, NULL);
        }
    }
    gSystemTick = 0;
    if (gLoadingString != 0xFFFF) {
        char done[80];

        sprintf(done, gLoadedStringFmt, gLevelPrettyNames[gSysInfo.level]);
        gLoadingString = TextReplaceString(gLoadingString, done);
        TextSetStringIntensity(gLoadingString, 200, 100, 100);
        SysStatePush(8);
    } else {
        EGSDisplayMalloc(EGSMemAvail());
    }
    if (mode == 3) {
        InterfaceSetMode(1);
    } else {
        InterfaceSetMode(0);
    }
}

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

/* Grows the collision box to at least w x h x d. */
void ObjectMakeMinimumBox(InitRuntime *rt, s32 w, s32 h, s32 d) {
    if (rt->anim != NULL) {
        s32 *box = rt->anim->box;
        s32 sx = box[4] - box[0];
        s32 sy, sz;
        if (sx < w) {
            sx = (w - sx) >> 1;
            box[0] -= sx;
            box[4] += sx;
        }
        sy = box[5] - box[1];
        if (sy < h) {
            sy = (h - sy) >> 1;
            box[1] -= sy;
            box[5] += sy;
        }
        sz = box[6] - box[2];
        if (sz < d) {
            sz = (d - sz) >> 1;
            box[2] -= sz;
            box[6] += sz;
        }
    }
}

/* Shrinks the collision box to at most w x h x d. */
void ObjectMakeMaximumBox(InitRuntime *rt, s32 w, s32 h, s32 d) {
    if (rt->anim != NULL) {
        s32 *box = rt->anim->box;
        s32 sx = box[4] - box[0];
        s32 sy, sz;
        if (w < sx) {
            sx = (sx - w) >> 1;
            box[0] += sx;
            box[4] -= sx;
        }
        sy = box[5] - box[1];
        if (h < sy) {
            sy = (sy - h) >> 1;
            box[1] += sy;
            box[5] -= sy;
        }
        sz = box[6] - box[2];
        if (d < sz) {
            sz = (sz - d) >> 1;
            box[2] += sz;
            box[6] -= sz;
        }
    }
}

#define INIT_MAX(a, b) ((a) < (b) ? (b) : (a))

/* Makes the collision box at least h tall along whichever local axis points up. */
void ObjectRaiseTopOfBoxIfTooShort(InitRuntime *rt, s32 h) {
    MATRIX m;
    VECTOR axes[3];
    InitAnim *anim;
    s32 *box;
    s32 ax;
    s32 ay;
    s32 az;
    s32 up;
    s32 grow;
    s32 len;

    anim = rt->anim;
    if (anim != NULL) {
        box = anim->box;
        m.m[0][0] = ((s16 *)rt->anim->frame)[0];
        m.m[0][1] = -((s16 *)rt->anim->frame)[1];
        m.m[0][2] = ((s16 *)rt->anim->frame)[2];
        m.m[1][0] = -((s16 *)rt->anim->frame)[3];
        m.m[1][1] = ((s16 *)rt->anim->frame)[4];
        m.m[1][2] = -((s16 *)rt->anim->frame)[5];
        m.m[2][0] = ((s16 *)rt->anim->frame)[6];
        m.m[2][1] = -((s16 *)rt->anim->frame)[7];
        m.m[2][2] = ((s16 *)rt->anim->frame)[8];
        axes[0].vx = m.m[0][0];
        axes[0].vy = m.m[1][0];
        axes[0].vz = m.m[2][0];
        axes[1].vx = m.m[0][1];
        axes[1].vy = m.m[1][1];
        axes[1].vz = m.m[2][1];
        axes[2].vx = m.m[0][2];
        axes[2].vy = m.m[1][2];
        axes[2].vz = m.m[2][2];
        ax = axes[0].vy >= 0 ? axes[0].vy : -axes[0].vy;
        ay = axes[1].vy >= 0 ? axes[1].vy : -axes[1].vy;
        az = axes[2].vy >= 0 ? axes[2].vy : -axes[2].vy;
        up = INIT_MAX(ax, INIT_MAX(ay, az));
        if (up == ax) {
            len = box[4] - box[0];
            grow = h - len;
            if (grow < 0) {
                grow = 0;
            }
            if (axes[0].vy >= 0) {
                box[4] += grow;
            } else {
                box[0] -= grow;
            }
        } else if (up == ay) {
            len = box[5] - box[1];
            grow = h - len;
            if (grow < 0) {
                grow = 0;
            }
            if (axes[1].vy >= 0) {
                box[1] -= grow;
            } else {
                box[5] += grow;
            }
        } else if (up == az) {
            len = box[6] - box[2];
            grow = h - len;
            if (grow < 0) {
                grow = 0;
            }
            if (axes[2].vy >= 0) {
                box[6] += grow;
            } else {
                box[2] -= grow;
            }
        }
    }
}

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

/* Creates the keycard model (and its X-ray variant) for a pickup object. */
static void func_80147F38(s16 id) {
    GOBJ_MATRIX mat;
    VECTOR32 pos;
    s32 model;
    s32 xmodel;
    InitAnim *anim;
    InitRuntime *rt;
    void *frame;
    GameObjectInfo *info;

    rt = (InitRuntime *)gameObjectsInfo[id].runtime;
    frame = &model; /* reused register; matches the original allocation */
    LevelGetModel("KEYCARD.GMD", frame);
    LevelGetModel("XEYCARD.GMD", &xmodel);
    if (model == 0) {
        EGSDebugAssert(1, 0, "bin/..\\s\\objinit.c", 0xFA);
    }
    if (xmodel == 0) {
        EGSDebugAssert(1, 0, "bin/..\\s\\objinit.c", 0xFB);
    }
    if (model != 0) {
        info = &gameObjectsInfo[id];
        *(s32 *)&info->flags24 = 0;
        EGSAnimSimpleCreate(model, &info->matrix, 0x04010000, &anim);
        if (anim != NULL) {
            anim->unk18 = 0;
            anim->unk09 = 0x11;
            anim->unk14 = id + 0x2000;
            frame = anim->frame;
            LevelGetGameObjectStartPosition(id, &pos);
            EGSFrameSetPosition(frame, 0, &pos);
            LevelGetGameObjectMatrix(id, &mat);
            EGSFrameSetRotationMatrix(frame, 0, &mat);
            EGSFrameUpdateHierarchy(frame);
        }
    } else {
        anim = NULL;
    }
    rt->anim = anim;
    if (anim != NULL && xmodel != 0) {
        anim->box[9] = xmodel;
        rt->anim->unk0A |= 8;
    }
    rt->healthInfo = (GameObjectHealth *)gameObjectsInfo[rt->id].unk38;
}

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

/* Finds the camera an object is linked to through at most two pass-through objects. */
static void func_801482D4(InitRuntime *rt) {
    GameObjectInfo *info = gameObjectsInfo;
    GameObjectRuntime *runtime;
    s16 id;
    s32 type;
    s32 depth;

    rt->unk04 = -1;
    id = info[rt->id].refId;
    depth = 1;
    while (id != -1) {
        runtime = info[id].runtime;
        type = id != GOBJ_ID_666 ? gameObjectsModelInfo[info[id].modelIndex].type : GOBJ_ID_666;
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
        info = gameObjectsInfo;
        id = info[id].refId;
    }
}

void DOORObjInit(InitRuntime *rt, s32 type) {
    s32 hi;

    rt->npcState = 9;
    LevelCreateGOBJ(rt->id, 0);
    rt->anim->unk09 = 0x40;
    PathSetupEX(rt, 1);
    if (rt->move != NULL) {
        PathSetPositionToPoint(rt, 0);
        rt->flags0 &= ~0x40;
    } else {
        rt->flags0 |= 0x40;
    }
    {
        u8 a = rt->flags1 & 0xF8;
        rt->flags0 &= 0xD0;
        rt->flags1 = a;
    }
    if (type != 0xA && type != 0x60 && type != 0x61 && type != 0x54) {
        rt->flags0 &= ~0x10;
    } else {
        rt->flags0 |= 0x10;
    }
    if (type != 0x49 && type != 0x60 && type != 0x3B && type != 0x76 && type != 0x54 && type != 0x3E) {
        rt->flags20 |= 0x80;
        rt->flags1 &= 0x7F;
    } else {
        rt->flags1 |= 0x80;
        if (type == 0x3E) {
            u8 f = rt->flags1;
            f &= 0xF8;
            rt->flags1 = f;
            MovementGobjInit(rt, 0, 0, 0, 0);
            PhysicsGobjBasicInit(rt, 0x2000, 1);
            CollisionGobjBasicInit(rt, rt->move[1], 0x666, 0x800, 1, 0xC00);
        }
    }
    if (type == 0x71 || type == 0x76) {
        TargetInitGobj(rt, target, 0, 0);
        rt->flags20 |= 0x10;
    }
    ObjectMakeMinimumBox(rt, 0x20, 0x10, 0x20);
    rt->anim->unk0A |= 0x20;
    rt->anim->box[10] |= 0x8000;
    rt->flags1 &= 0x80;
    hi = rt->flags1;
    if (type != 0x3B && type != 7) {
        rt->flags1 = IsWideDoor(rt) ? hi | 0xF : hi | 6;
    } else {
        rt->flags1 = hi;
    }
    rt->unk04 = -1;
    if (type != 0x3E) {
        func_801482D4(rt);
    }
}

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
/* regalloc: flag bit computed straight into s1 instead of via v0.
 * Ours: the SI and-result is the multi-block pseudo (CSE folds the u8 hi copy into it).
 * Tried: hi = flags1 &= 0x80, block-local u8/s32 temps, two-step hi = x; hi &= 0x80, goto layouts,
 * bitfield struct, u8/s32 hi. `static inline u8 f(rt, u8 v){rt->flags1 = v; return v;}` gets
 * the v0->s1 copy right but adds andi 0xFF and folds hi|1 to li 1 (3 diffs). */
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

void INTERACTObjInit(InitRuntime *rt, s32 type) {
    GameObjectHealth *health;
    InitAnim *anim;

    rt->npcState = 0;
    LevelCreateGOBJ(rt->id, 0);
    UseObjectTeleportCameraSetup(rt);
    PathSetupEX(rt, 1);
    rt->flags0 &= 0x97;
    switch (type) {
    case 0x57:
        ObjectRaiseTopOfBoxIfTooShort(rt, 0x30);
        rt->anim->unk18 = 0;
        rt->anim->unk09 = 0x10;
        health = rt->healthInfo;
        health->armor = 0x7FFF;
        health->health = 0x7FFF;
        break;
    default:
        anim = rt->anim;
        anim->unk09 = 0x40;
        health = rt->healthInfo;
        health->armor = 0x7FFF;
        health->health = 0x7FFF;
        break;
    case 0x58:
        rt->anim->flags08 |= 8;
        TargetInitGobj(rt, target, 0, 0);
        break;
    case 0x59:
        anim = rt->anim;
        anim->flags08 |= 8;
        health = rt->healthInfo;
        health->armor = 0x7FFF;
        health->health = 0x7FFF;
        SysExeTickQueueTimer(UseBeaconUpdate, 1, rt);
        break;
    case 0x64:
        TargetInitGobj(rt, target, 0, 0);
        if (rt->move != NULL) {
            rt->anim->unk18 = 0;
            rt->anim->unk09 = 0x16;
            rt->anim->unk0A |= 0x20;
            rt->anim->box[10] |= 0x8000;
            rt->flags0 |= 0x20;
        } else {
            rt->anim->flags08 |= 8;
        }
        break;
    case 0x6C:
        TargetInitGobj(rt, target, 0, 0);
        break;
    }
}

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

void LIGHTObjInit(InitRuntime *rt, s32 type) {
    GameObjectHealth *health;
    s32 v;

    LevelCreateGOBJ(rt->id, 0);
    rt->npcState = 4;
    rt->unk21 = 0xA0;
    rt->cas = NULL;
    rt->anim->unk18 = 0;
    rt->anim->unk09 = 0x17;
    rt->flags0 = (rt->flags0 | 2) & 0x37;
    rt->flags1 &= 0x7F;
    PathSetupEX(rt, type == 0x6D);
    if (type == 0x34) {
        rt->flags20 |= 0x80;
    } else {
        rt->flags20 &= 0x7F;
    }
    ObjectMakeMinimumBox(rt, 0x20, 0x20, 0x20);
    if (type == 0x15 || type == 0x23 || type == 0x6B || type == 0x6F || type == 0x28) {
        rt->flags20 |= 8;
        ObjectMakeMaximumBox(rt, 0x40, 0x40, 0x20);
        if (type == 0x6B || type == 0x6F) {
            v = PathGetVelocity(rt);
            rt->unk04 = v;
            rt->flags1 = v;
            rt->flags0 |= 8;
            if (type == 0x6F) {
                health = rt->healthInfo;
                health->armor = 0x7FFF;
                health->health = 0x7FFF;
            }
        }
    } else if (type == 0x6D) {
        MovementGobjInit(rt, 0, 0, 0, 0);
        PhysicsGobjBasicInit(rt, 0x10000, 1);
        CollisionGobjBasicInit(rt, rt->move[1], 0, 0x1000, 0, 0x1000);
    } else if (type == 0x34) {
        rt->unk04 = 0;
        rt->unk21 = 0xA0;
    }
    if (type != 0x6F) {
        TargetInitGobj(rt, target, 0, 0);
    }
}

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

void EGSFrameCreateWithNode(void **frame, void *parent);
void EGSFrameSetMatrix(void *frame, s32 arg1, MATRIX *m);

/* Re-creates rt's frame as a child of parent, keeping its matrix (y flipped). */
#define INIT_ATTACH_FRAME(rt, parent)                                              \
    if (&(rt)->anim->frame != NULL && (rt)->anim->frame != NULL) {                 \
        m.m[0][0] = ((MATRIX *)(rt)->anim->frame)->m[0][0];                        \
        m.m[0][1] = -((MATRIX *)(rt)->anim->frame)->m[0][1];                       \
        m.m[0][2] = ((MATRIX *)(rt)->anim->frame)->m[0][2];                        \
        m.m[1][0] = -((MATRIX *)(rt)->anim->frame)->m[1][0];                       \
        m.m[1][1] = ((MATRIX *)(rt)->anim->frame)->m[1][1];                        \
        m.m[1][2] = -((MATRIX *)(rt)->anim->frame)->m[1][2];                       \
        m.m[2][0] = ((MATRIX *)(rt)->anim->frame)->m[2][0];                        \
        m.m[2][1] = -((MATRIX *)(rt)->anim->frame)->m[2][1];                       \
        m.m[2][2] = ((MATRIX *)(rt)->anim->frame)->m[2][2];                        \
        m.t[0] = ((MATRIX *)(rt)->anim->frame)->t[0];                              \
        m.t[1] = -((MATRIX *)(rt)->anim->frame)->t[1];                             \
        m.t[2] = ((MATRIX *)(rt)->anim->frame)->t[2];                              \
        EGSFrameCreateWithNode(&(rt)->anim->frame, parent);                        \
        EGSFrameSetMatrix((rt)->anim->frame, 0, &m);                               \
        EGSFrameUpdateHierarchy((rt)->anim->frame);                                \
    }

void ObjectSetupCar(s16 id) {
    VECTOR carPos;
    VECTOR pos;
    MATRIX m;
    InitRuntime *rt;
    InitRuntime *car;
    s16 type;
    s16 next;

    rt = (InitRuntime *)gameObjectsInfo[id].runtime;
    next = gameObjectsInfo[rt->id].refId;
    if (next == -1) {
        return;
    }
    type = GOBJ_TYPE(next);
    if (type == 0x2C) {
        car = (InitRuntime *)gameObjectsInfo[next].runtime;
        carPos.vx = ((MATRIX *)car->anim->frame)->t[0];
        carPos.vy = ((MATRIX *)car->anim->frame)->t[1];
        carPos.vz = ((MATRIX *)car->anim->frame)->t[2];
        carPos.vy = -carPos.vy;
        pos.vx = ((MATRIX *)rt->anim->frame)->t[0];
        pos.vy = ((MATRIX *)rt->anim->frame)->t[1];
        pos.vz = ((MATRIX *)rt->anim->frame)->t[2];
        pos.vy = -pos.vy;
        car->anim->box[1] = carPos.vy - pos.vy + rt->anim->box[5];
        INIT_ATTACH_FRAME(car, NULL);
        INIT_ATTACH_FRAME(rt, car->anim->frame);
        next = gameObjectsInfo[car->id].refId;
        if (next != -1) {
            rt = (InitRuntime *)gameObjectsInfo[next].runtime;
            INIT_ATTACH_FRAME(rt, car->anim->frame);
        }
    }
}

void TGTObjInit(InitRuntime *rt, s32 type) {
    if (type == 0x2C && *(s32 *)&gameObjectsInfo[rt->id].flags24 != 0) {
        rt->flags1 = PickupGetData();
        *(s32 *)&gameObjectsInfo[rt->id].flags24 = 0;
    }
    LevelCreateGOBJ(rt->id, 0);
    rt->npcState = 3;
    TargetInitGobj(rt, target, 0, 0);
    rt->cas = NULL;
    rt->flags0 &= 0x37;
    ObjectMakeMinimumBox(rt, 0x10, 0x10, 0x10);
    switch (type) {
    case 0x67:
        rt->anim->flags08 |= 8;
        break;
    case 0x3D:
    case 0x66:
        MovementGobjInit(rt, 0, 0, 0, 0);
        PhysicsGobjBasicInit(rt, 0x3000, 1);
        CollisionGobjBasicInit(rt, rt->move[1], 0x666, 0x1000, 0x800, 0);
        rt->anim->unk09 = 0x40;
        rt->flags20 |= 0x80;
        break;
    case 0x2C:
        rt->unk21 = 0x8C;
        rt->unk04 = 0;
        rt->flags0 &= ~0x20;
        rt->flags1 = (rt->flags1 & 0x80) | 1;
        rt->anim->unk18 = 0;
        rt->anim->unk09 = 0x18;
        rt->anim->unk0A |= 0x20;
        rt->anim->box[10] |= 0x8000;
        MovementGobjInit(rt, 0, 0, 0, 0);
        PhysicsGobjBasicInit(rt, 0x10000, 1);
        CollisionGobjBasicInit(rt, rt->move[1], 0x666, 0x800, -1, 0xC00);
        rt->flags20 |= 0x80;
        break;
    case 0x38:
        rt->unk04 = 0;
        rt->unk21 = 0x8C;
        rt->anim->flags08 |= 8;
        rt->flags20 |= 0x80;
        break;
    case 0x44:
        rt->anim->unk09 = 0x40;
        rt->flags20 |= 0x20;
        break;
    case 0x42:
        rt->anim->unk09 = 0x40;
        break;
    case 0x21:
    default:
        rt->anim->unk09 = 0x40;
        rt->flags20 |= 0x80;
        break;
    }
}

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

void iCallbackLAND();
void iCallbackAIM();
void iCallbackPul4();
void iCallbackPul8();
void iCallbackClimbA();
void iCallbackSTLOB();
void iCallbackWKLOB();
void iCallbackRNLOB();
void iCallbackKNLOB();
void iCallbackCWLOB();
void iCallbackJP1();
void iCallbackReloadSnd_STRLD1B();
void iCallbackReloadSnd_WK1RLD_WK2RLD();
void iCallbackReloadSnd_RN1RLD();
void iCallbackReloadSnd_ST2RLD_RN2RLD_KN2RLD();
void iCallbackReloadSnd_CW2RLD();
void iCallbackIdle13();
void iCallbackShimL();
void iCallbackShimR();
void iCallbackWK();
void iCallbackRN();
void iCallbackCW();
void iCallbackDeathCamGunshot();
void iCallbackKIKDR();
void iCallbackSurrender();
extern InitAnimSlot gCASAnimInfo[];

#ifdef NON_MATCHING
/* statements in store order; the original reaches some slots through the CSE'd &gCASAnimInfo in s0 and copies with 3 registers (~150 insns longer) */
/* Sets up the per-animation callbacks and timing of the CAS animation table. */
void CASAnimInit(void) {
    InitAnimStruct(gCASAnimInfo, 0xA3);
    gCASAnimInfo[149].unk1C = iCallbackLAND;
    gCASAnimInfo[151].unk1C = iCallbackLAND;
    gCASAnimInfo[99].unk1C = iCallbackLAND;
    gCASAnimInfo[101].unk1C = iCallbackLAND;
    gCASAnimInfo[43].unk1C = iCallbackLAND;
    gCASAnimInfo[149].active = 1;
    gCASAnimInfo[151].active = 1;
    gCASAnimInfo[99].active = 1;
    gCASAnimInfo[101].active = 1;
    gCASAnimInfo[43].active = 1;
    gCASAnimInfo[91].active = 1;
    gCASAnimInfo[91].unk08 = 0x20;
    gCASAnimInfo[91].unk1C = iCallbackAIM;
    gCASAnimInfo[94].active = 1;
    gCASAnimInfo[94].unk08 = 0x20;
    gCASAnimInfo[94].unk1C = iCallbackAIM;
    gCASAnimInfo[114].active = 1;
    gCASAnimInfo[114].unk08 = 0x20;
    gCASAnimInfo[114].unk1C = iCallbackAIM;
    gCASAnimInfo[115].active = 1;
    gCASAnimInfo[115].unk08 = 0x20;
    gCASAnimInfo[115].unk1C = iCallbackAIM;
    gCASAnimInfo[54].active = 1;
    gCASAnimInfo[54].unk08 = 0x20;
    gCASAnimInfo[54].unk1C = iCallbackAIM;
    gCASAnimInfo[55].active = 1;
    gCASAnimInfo[55].unk08 = 0x20;
    gCASAnimInfo[55].unk1C = iCallbackAIM;
    gCASAnimInfo[4].active = 1;
    gCASAnimInfo[4].unk08 = 0x20;
    gCASAnimInfo[4].unk1C = iCallbackAIM;
    gCASAnimInfo[6].active = 1;
    gCASAnimInfo[6].unk08 = 0x20;
    gCASAnimInfo[6].unk1C = iCallbackAIM;
    gCASAnimInfo[140].active = 1;
    gCASAnimInfo[140].unk08 = 0x20;
    gCASAnimInfo[140].unk1C = iCallbackAIM;
    gCASAnimInfo[143].active = 1;
    gCASAnimInfo[143].unk08 = 0x20;
    gCASAnimInfo[143].unk1C = iCallbackAIM;
    gCASAnimInfo[118].active = 1;
    gCASAnimInfo[118].unk08 = 0x1000000;
    gCASAnimInfo[118].unk10 = 2;
    gCASAnimInfo[119] = gCASAnimInfo[118];
    gCASAnimInfo[146] = gCASAnimInfo[118];
    gCASAnimInfo[147] = gCASAnimInfo[118];
    gCASAnimInfo[97] = gCASAnimInfo[118];
    gCASAnimInfo[98] = gCASAnimInfo[118];
    gCASAnimInfo[62] = gCASAnimInfo[118];
    gCASAnimInfo[63] = gCASAnimInfo[118];
    gCASAnimInfo[64] = gCASAnimInfo[118];
    gCASAnimInfo[10] = gCASAnimInfo[118];
    gCASAnimInfo[11] = gCASAnimInfo[118];
    gCASAnimInfo[12] = gCASAnimInfo[118];
    gCASAnimInfo[125].active = 1;
    gCASAnimInfo[125].unk04 = 0x1000000;
    gCASAnimInfo[125].unk0C = 2;
    gCASAnimInfo[126] = gCASAnimInfo[125];
    gCASAnimInfo[153] = gCASAnimInfo[125];
    gCASAnimInfo[154] = gCASAnimInfo[125];
    gCASAnimInfo[106] = gCASAnimInfo[125];
    gCASAnimInfo[107] = gCASAnimInfo[125];
    gCASAnimInfo[69] = gCASAnimInfo[125];
    gCASAnimInfo[70] = gCASAnimInfo[125];
    gCASAnimInfo[71] = gCASAnimInfo[125];
    gCASAnimInfo[14] = gCASAnimInfo[125];
    gCASAnimInfo[15] = gCASAnimInfo[125];
    gCASAnimInfo[16] = gCASAnimInfo[125];
    gCASAnimInfo[53].active = 1;
    gCASAnimInfo[122].active = 1;
    gCASAnimInfo[122].unk14 = 1;
    gCASAnimInfo[122].unk18 = 0x80000000;
    gCASAnimInfo[122].unk04 = 0x80000;
    gCASAnimInfo[122].unk08 = 0x80000;
    gCASAnimInfo[122].unk0C = 6;
    gCASAnimInfo[122].unk10 = 6;
    gCASAnimInfo[123] = gCASAnimInfo[122];
    gCASAnimInfo[75].active = 1;
    gCASAnimInfo[75].unk14 = 1;
    gCASAnimInfo[75].unk04 = 0x80000;
    gCASAnimInfo[75].unk18 = 0x80000000;
    gCASAnimInfo[75].unk08 = 0x180000;
    gCASAnimInfo[75].unk0C = 6;
    gCASAnimInfo[75].unk10 = 6;
    gCASAnimInfo[76] = gCASAnimInfo[75];
    gCASAnimInfo[59].active = 1;
    gCASAnimInfo[59].unk0C = 6;
    gCASAnimInfo[59].unk10 = 6;
    gCASAnimInfo[17].active = 1;
    gCASAnimInfo[17].unk0C = 6;
    gCASAnimInfo[17].unk10 = 6;
    gCASAnimInfo[128].active = 1;
    gCASAnimInfo[128].unk04 = 0x8000008;
    gCASAnimInfo[128].unk08 = 0x8000000;
    gCASAnimInfo[128].unk14 = 1;
    gCASAnimInfo[128].unk18 = 0x80000000;
    gCASAnimInfo[129] = gCASAnimInfo[128];
    gCASAnimInfo[108].active = 1;
    gCASAnimInfo[108].unk04 = 0x4000008;
    gCASAnimInfo[108].unk08 = 0x4000008;
    gCASAnimInfo[108].unk0C = 2;
    gCASAnimInfo[108].unk10 = 2;
    gCASAnimInfo[108].unk14 = 1;
    gCASAnimInfo[108].unk18 = 0x80000000;
    gCASAnimInfo[109] = gCASAnimInfo[108];
    gCASAnimInfo[132].active = 1;
    gCASAnimInfo[132].unk04 = 0x80;
    gCASAnimInfo[132].unk08 = 0x80;
    gCASAnimInfo[77] = gCASAnimInfo[132];
    gCASAnimInfo[83].active = 1;
    gCASAnimInfo[83].unk1C = iCallbackPul4;
    gCASAnimInfo[84].unk1C = iCallbackPul8;
    gCASAnimInfo[83].unk04 = 0x82000;
    gCASAnimInfo[83].unk08 = 0x882000;
    gCASAnimInfo[83].unk18 = 0x80000000;
    gCASAnimInfo[83].unk14 = 1;
    gCASAnimInfo[84].active = 1;
    gCASAnimInfo[84].unk04 = 0x82000;
    gCASAnimInfo[84].unk08 = 0x882000;
    gCASAnimInfo[84].unk18 = 0x80000000;
    gCASAnimInfo[84].unk14 = 1;
    gCASAnimInfo[0].active = 1;
    gCASAnimInfo[0].unk1C = iCallbackClimbA;
    gCASAnimInfo[0].unk0C = 2;
    gCASAnimInfo[0].unk10 = 2;
    gCASAnimInfo[113].active = 1;
    gCASAnimInfo[48].active = 1;
    gCASAnimInfo[46].active = 1;
    gCASAnimInfo[45].active = 1;
    gCASAnimInfo[49].active = 1;
    gCASAnimInfo[47].active = 1;
    gCASAnimInfo[124].active = 1;
    gCASAnimInfo[124].unk04 = 0x800;
    gCASAnimInfo[124].unk08 = 0x800;
    gCASAnimInfo[124].unk0C = 2;
    gCASAnimInfo[124].unk10 = 2;
    gCASAnimInfo[124].unk1C = iCallbackSTLOB;
    gCASAnimInfo[152] = gCASAnimInfo[124];
    gCASAnimInfo[152].unk1C = iCallbackWKLOB;
    gCASAnimInfo[102] = gCASAnimInfo[124];
    gCASAnimInfo[102].unk1C = iCallbackRNLOB;
    gCASAnimInfo[65] = gCASAnimInfo[124];
    gCASAnimInfo[65].unk1C = iCallbackKNLOB;
    gCASAnimInfo[13] = gCASAnimInfo[124];
    gCASAnimInfo[13].unk1C = iCallbackCWLOB;
    gCASAnimInfo[103].active = 1;
    gCASAnimInfo[103].unk04 = 0x400;
    gCASAnimInfo[103].unk08 = 0x400;
    gCASAnimInfo[104].active = 1;
    gCASAnimInfo[104].unk04 = 0x400;
    gCASAnimInfo[104].unk08 = 0x400;
    gCASAnimInfo[105].active = 1;
    gCASAnimInfo[105].unk04 = 0x400;
    gCASAnimInfo[105].unk08 = 0x400;
    gCASAnimInfo[130].active = 1;
    gCASAnimInfo[130].unk04 = 0x400;
    gCASAnimInfo[130].unk08 = 0x400;
    gCASAnimInfo[131].active = 1;
    gCASAnimInfo[131].unk04 = 0x400;
    gCASAnimInfo[131].unk08 = 0x400;
    gCASAnimInfo[72].active = 1;
    gCASAnimInfo[72].unk04 = 0x400;
    gCASAnimInfo[72].unk08 = 0x400;
    gCASAnimInfo[73].active = 1;
    gCASAnimInfo[73].unk04 = 0x400;
    gCASAnimInfo[73].unk08 = 0x400;
    gCASAnimInfo[74].active = 1;
    gCASAnimInfo[74].unk04 = 0x400;
    gCASAnimInfo[74].unk08 = 0x400;
    gCASAnimInfo[51].active = 1;
    gCASAnimInfo[51].unk08 = 0x1000;
    gCASAnimInfo[51].unk1C = iCallbackJP1;
    gCASAnimInfo[51].unk04 = 0x801000;
    gCASAnimInfo[127].active = 1;
    gCASAnimInfo[127].unk0C = 0x22;
    gCASAnimInfo[127].unk10 = 0x22;
    gCASAnimInfo[127].unk04 = 0x1000000;
    gCASAnimInfo[127].unk08 = 0x1000000;
    gCASAnimInfo[127].unk14 = 1;
    gCASAnimInfo[127].unk18 = 0x80000000;
    gCASAnimInfo[127].unk1C = iCallbackReloadSnd_STRLD1B;
    gCASAnimInfo[141] = gCASAnimInfo[127];
    gCASAnimInfo[141].unk1C = iCallbackReloadSnd_WK1RLD_WK2RLD;
    gCASAnimInfo[92] = gCASAnimInfo[127];
    gCASAnimInfo[92].unk1C = iCallbackReloadSnd_RN1RLD;
    gCASAnimInfo[117] = gCASAnimInfo[127];
    gCASAnimInfo[117].unk1C = iCallbackReloadSnd_ST2RLD_RN2RLD_KN2RLD;
    gCASAnimInfo[116] = gCASAnimInfo[127];
    gCASAnimInfo[116].unk0C |= 0x10;
    gCASAnimInfo[116].unk10 |= 0x10;
    gCASAnimInfo[144] = gCASAnimInfo[127];
    gCASAnimInfo[144].unk0C |= 0x10;
    gCASAnimInfo[144].unk10 |= 0x10;
    gCASAnimInfo[145] = gCASAnimInfo[127];
    gCASAnimInfo[145].unk1C = iCallbackReloadSnd_WK1RLD_WK2RLD;
    gCASAnimInfo[96] = gCASAnimInfo[127];
    gCASAnimInfo[96].unk1C = iCallbackReloadSnd_ST2RLD_RN2RLD_KN2RLD;
    gCASAnimInfo[95] = gCASAnimInfo[127];
    gCASAnimInfo[95].unk0C = 0x12;
    gCASAnimInfo[95].unk10 = 0x12;
    gCASAnimInfo[95].unk1C = iCallbackReloadSnd_ST2RLD_RN2RLD_KN2RLD;
    gCASAnimInfo[58] = gCASAnimInfo[127];
    gCASAnimInfo[58].unk1C = iCallbackReloadSnd_ST2RLD_RN2RLD_KN2RLD;
    gCASAnimInfo[57] = gCASAnimInfo[127];
    gCASAnimInfo[57].unk1C = iCallbackReloadSnd_ST2RLD_RN2RLD_KN2RLD;
    gCASAnimInfo[56] = gCASAnimInfo[127];
    gCASAnimInfo[56].unk1C = iCallbackReloadSnd_ST2RLD_RN2RLD_KN2RLD;
    gCASAnimInfo[56].unk0C |= 0x10;
    gCASAnimInfo[56].unk10 |= 0x10;
    gCASAnimInfo[7] = gCASAnimInfo[127];
    gCASAnimInfo[7].unk1C = iCallbackReloadSnd_ST2RLD_RN2RLD_KN2RLD;
    gCASAnimInfo[7].unk0C |= 0x10;
    gCASAnimInfo[7].unk10 |= 0x10;
    gCASAnimInfo[9] = gCASAnimInfo[127];
    gCASAnimInfo[9].unk1C = iCallbackReloadSnd_CW2RLD;
    gCASAnimInfo[8] = gCASAnimInfo[127];
    gCASAnimInfo[8].unk1C = iCallbackReloadSnd_CW2RLD;
    gCASAnimInfo[44].unk1C = iCallbackIdle13;
    gCASAnimInfo[44].active = 1;
    gCASAnimInfo[110].active = 1;
    gCASAnimInfo[110].unk1C = iCallbackShimL;
    gCASAnimInfo[110].unk04 = 0x20000;
    gCASAnimInfo[110].unk08 = 0x20000;
    gCASAnimInfo[110].unk18 = 0x80000000;
    gCASAnimInfo[110].unk14 = 1;
    gCASAnimInfo[111].active = 1;
    gCASAnimInfo[111].unk04 = 0x20000;
    gCASAnimInfo[111].unk08 = 0x20000;
    gCASAnimInfo[111].unk1C = iCallbackShimR;
    gCASAnimInfo[111].unk18 = 0x80000000;
    gCASAnimInfo[111].unk14 = 1;
    gCASAnimInfo[138].active = 1;
    gCASAnimInfo[138].unk1C = iCallbackWK;
    gCASAnimInfo[138].unk0C = 8;
    gCASAnimInfo[138].unk10 = 8;
    gCASAnimInfo[89].active = 1;
    gCASAnimInfo[89].unk1C = iCallbackRN;
    gCASAnimInfo[89].unk0C = 8;
    gCASAnimInfo[89].unk10 = 8;
    gCASAnimInfo[1].active = 1;
    gCASAnimInfo[1].unk1C = iCallbackCW;
    gCASAnimInfo[1].unk0C = 8;
    gCASAnimInfo[1].unk10 = 8;
    gCASAnimInfo[26].active = 1;
    gCASAnimInfo[26].unk18 = 0x80000000;
    gCASAnimInfo[26].unk14 = 1;
    gCASAnimInfo[26].unk15 = 1;
    gCASAnimInfo[26].unk1C = iCallbackDeathCamGunshot;
    gCASAnimInfo[27] = gCASAnimInfo[26];
    gCASAnimInfo[28] = gCASAnimInfo[26];
    gCASAnimInfo[29] = gCASAnimInfo[26];
    gCASAnimInfo[30] = gCASAnimInfo[26];
    gCASAnimInfo[31] = gCASAnimInfo[26];
    gCASAnimInfo[32] = gCASAnimInfo[26];
    gCASAnimInfo[33] = gCASAnimInfo[26];
    gCASAnimInfo[18] = gCASAnimInfo[26];
    gCASAnimInfo[19] = gCASAnimInfo[26];
    gCASAnimInfo[20] = gCASAnimInfo[26];
    gCASAnimInfo[21] = gCASAnimInfo[26];
    gCASAnimInfo[22] = gCASAnimInfo[26];
    gCASAnimInfo[23] = gCASAnimInfo[26];
    gCASAnimInfo[24] = gCASAnimInfo[26];
    gCASAnimInfo[25] = gCASAnimInfo[26];
    gCASAnimInfo[37] = gCASAnimInfo[26];
    gCASAnimInfo[25] = gCASAnimInfo[26];
    gCASAnimInfo[36] = gCASAnimInfo[26];
    gCASAnimInfo[38] = gCASAnimInfo[26];
    gCASAnimInfo[39] = gCASAnimInfo[26];
    gCASAnimInfo[34] = gCASAnimInfo[26];
    gCASAnimInfo[134].active = 1;
    gCASAnimInfo[134].unk14 = 1;
    gCASAnimInfo[134].unk18 = 0x80000000;
    gCASAnimInfo[135] = gCASAnimInfo[134];
    gCASAnimInfo[52].active = 1;
    gCASAnimInfo[52].unk14 = 1;
    gCASAnimInfo[52].unk18 = 0x80000000;
    gCASAnimInfo[52].unk1C = iCallbackKIKDR;
    gCASAnimInfo[87].active = 1;
    gCASAnimInfo[87].unk15 = 1;
    gCASAnimInfo[87].unk18 = 0x80000000;
    gCASAnimInfo[88] = gCASAnimInfo[87];
    gCASAnimInfo[133].active = 1;
    gCASAnimInfo[133].unk1C = iCallbackSurrender;
}
#else
INCLUDE_ASM("asm/init/nonmatchings/init", CASAnimInit);
#endif
