#include "common.h"
#include "system.h"
#include "level.h"
#include "gameobj.h"

/* level.c's small data, still assembled from asm */
SDATA_LOCAL(D_8010F654)
SDATA_LOCAL(D_8010F658)
SDATA_LOCAL(D_8010F65C)
SDATA_LOCAL(gAddingObjectiveViaQuip)
SDATA_LOCAL(gHanHogFile)
SDATA_LOCAL(gHanLvlHogFile)
SDATA_LOCAL(gInEOLState)
SDATA_LOCAL(gLevelGameObjCount)
SDATA_LOCAL(gLevelSpecificNpcID)
SDATA_LOCAL(gLevelSpecificNpcID2)
SDATA_LOCAL(gMissionFailed)
SDATA_LOCAL(gObjectives)
SDATA_LOCAL(gPermHogFile)
SDATA_LOCAL(gStartedEOLFade)
SDATA_LOCAL(gameObjectsInfo)
SDATA_LOCAL(gameObjectsModelInfo)
SDATA_LOCAL(levelLoadHeader)
SDATA_LOCAL(mGameOverMsgFlag)
SDATA_LOCAL(mLastTickAddedObjectives)
SDATA_LOCAL(mMissionFailedMsgFlag)
SDATA_LOCAL(mNextTickAllowMsgIncompleteObjectives)
SDATA_LOCAL(mNextTickAllowMsgMissionFailed)

extern u8 (*D_8010F654)();    /* level EOL callback: nonzero allows end of level */
extern void *(*D_8010F658)(void);  /* objective instructions callback */
extern u8 D_8010F65C;              /* direction pointer flag */
extern u8 gAddingObjectiveViaQuip;
extern s32 gHanHogFile;
extern s32 gHanLvlHogFile;
extern u8 gStartedEOLFade;
extern s32 gPermHogFile;
extern u8 mGameOverMsgFlag;
extern u8 mMissionFailedMsgFlag;
extern s32 mLastTickAddedObjectives;
extern u32 mNextTickAllowMsgIncompleteObjectives;
extern u32 mNextTickAllowMsgMissionFailed;
typedef struct LevelLoadHeader {
    /* 0x00 */ u8 unk00[0x30];
    /* 0x30 */ u8 *base;
    /* 0x34 */ u8 unk34[0x34];
    /* 0x68 */ s32 menuHogOffset;
} LevelLoadHeader;
extern LevelLoadHeader *levelLoadHeader;
typedef struct LevelData {
    /* 0x00 */ u8 fogOn;
    /* 0x01 */ u8 unk01[3];
    /* 0x04 */ u8 fogColor[8];
    /* 0x0C */ s32 zoomPoints[1][4]; /* x, y, z, ? per zoom point */
} LevelData;
extern LevelData *gLevData;
extern s32 gExeTick;
extern void *gGameOverMsg;
extern void *gObjectiveAddedMsg;
extern char D_8010F6EC[]; /* "\\SOL\\%s" (.sdata) */
extern char D_8010F6F4[]; /* "\\EOL\\%s" (.sdata) */

s32 EGSHogGetBlockByName(s32 hog, s32 name, void *out);
void EGSDisplayFogOn(void *camera);
void EGSDisplaySetFogColor(void *camera, void *color);
void InterfaceSetChasePointerTarget(void *pos);
int strlen(const char *);
u8 TextAddWindow(s32 x, s32 y, s32 w, s32 h);
void TextSetWindowJustification(u8 window, s32 just);
u16 TextAddString(u8 window, char *str, s32 arg2, s32 arg3);
void TextSetStringIntensity(u16 str, s32 r, s32 g, s32 b);
void SysPlayMovie(MovieInfo *info);
void TerrainZoom(s32 *point, s32 sector);
void InterfaceDeleteBomb(void);
void SetNextTickAllowMsgLongText(u32 tick);
void StringObjectiveMsg(char *buf, s16 objective, u8 state, s32 arg3);
void SysExeTickQueueTimer(void (*cb)(s32), s32 ticks, s32 arg);
void StringReplaceContactMeiXingMsg(void *msg);
s32 LevelGetGameObjectStartPosition(s32 id, s32 *pos);
void LevelDoEOLMovie(s32 unused);
void LevelDoRestart(void);
static void func_80017544(s32 arg0);
static void func_80017348(void);
void SavePlayerPreserve(void);
void PlayerFailMission(void);
void StringMissionCompletedMsg(char *buf);
void JoystickDeactivateMenu(s32);
void GameCameraRequestCutToBehind(void);
u16 ObjectDoQuip(s16 id, s32 bit, LevelObjectives *objectives);
void LevelObjectiveAdd(s32 objective, s32 quipFrom);

s32 LevelGetGameObjectMatrix(s32 id, GOBJ_MATRIX *out) {
    if (id >= gLevelGameObjCount || id < 0) {
        return 0;
    }
    *out = gameObjectsInfo[id].matrix;
    return 1;
}

#ifdef NON_MATCHING
/* addu operand order of the gameObjectsInfo index differs (register lifetimes) */
s32 LevelGetGameObjectStartPosition(s32 id, s32 *pos) {
    GameObjectInfo *info;

    if (id >= gLevelGameObjCount || id < 0) {
        return 0;
    }
    info = &gameObjectsInfo[id];
    pos[0] = info->matrix.t[0];
    pos[1] = info->matrix.t[1];
    pos[2] = info->matrix.t[2];
    return 1;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/level", LevelGetGameObjectStartPosition);
#endif

s32 LevelGetPathInfo(s32 id, s32 *start, s32 *path) {
    if (id >= gLevelGameObjCount || id < 0) {
        return 0;
    }
    *path = gameObjectsInfo[id].path;
    *start = gameObjectsInfo[id].pathStart;
    return 1;
}

void LevelLoadHMD(s32 *names, void **out, s32 fromLevelHog) {
    s32 hog;
    int i;

    if (fromLevelHog == 0) {
        hog = gHanHogFile;
    } else {
        hog = gHanLvlHogFile;
    }
    for (i = 0; names[i] != 0; i++) {
        EGSHogGetBlockByName(hog, names[i], &out[fromLevelHog + i]);
    }
}

void LevelGetHMDModel(s32 name, void **out) {
    EGSHogGetBlockByName(gPermHogFile, name, out);
}

void LevelGetModel(s32 name, void **out) {
    if (EGSHogGetBlockByName(gPermHogFile, name, out) != 0) {
        *out = NULL;
    }
}

s32 LevelGameObjectIsInvisibleClass(s16 id) {
    switch (GOBJ_TYPE(id)) {
    case 0x0E:
    case 0x18:
    case 0x22:
    case 0x5D:
    case 0x5F:
    case 0x73:
    case 0x74:
    case 0x77:
        return 1;
    }
    return 0;
}

s32 LevelGameObjectHasModel(s32 id) {
    return *gameObjectsModelInfo[gameObjectsInfo[id].modelIndex].model != 0;
}

void LevelSetupLights(void) {
    void *camera;

    camera = *gCamera;
    if (gLevData->fogOn) {
        EGSDisplayFogOn(camera);
        EGSDisplaySetFogColor(camera, gLevData->fogColor);
    }
}

void LevelPlaySOLMovie(void) {
    MovieInfo info;

    sprintf(info.name, D_8010F6EC, gLevelNames[gSysInfo.level]);
    info.width = 320;
    info.height = 240;
    info.halfSize = 40;
    info.unk84 = 0;
    info.trueColor = 1;
    info.doneMsgId = 0;
    SysPlayMovie(&info);
}

void LevelSetDirectionPointerFlag(u8 flag) {
    D_8010F65C = flag;
}

u8 LevelGetDirectionPointerFlag(void) {
    return D_8010F65C;
}

void LevelSetPointer(s32 id) {
    s32 pos[4];

    LevelGetGameObjectStartPosition(id, pos);
    InterfaceSetChasePointerTarget(pos);
}

s32 (*LevelSetEOLCallback(s32 (*cb)()))() {
    s32 (*prev)();

    prev = (s32 (*)())D_8010F654;
    D_8010F654 = (u8 (*)())cb;
    return prev;
}

void *LevelSetObjectiveInstructionsCallback(void *(*cb)(void)) {
    void *(*prev)(void);

    prev = D_8010F658;
    D_8010F658 = cb;
    return prev;
}

void LevelDoEOLMovie(s32 unused) {
    MovieInfo info;

    SysStatePop();
    sprintf(info.name, D_8010F6F4, gLevelNames[gSysInfo.level]);
    info.width = 320;
    info.height = 240;
    info.halfSize = 40;
    info.trueColor = 1;
    info.doneMsgId = SYS_MSG_END_OF_LEVEL;
    info.unk84 = 0;
    info.doneMsgFrom = SYS_MSG_LEVEL;
    info.doneMsgTo = SYS_MSG_LEVEL;
    SysPlayMovie(&info);
}

#ifdef NON_MATCHING
/* s0/s1 allocation of extraTicks vs ticks swapped */
/* Centred mission message; returns how long it stays up. */
s32 LevelMissionCtrMsg(char *msg, s32 extraTicks) {
    s32 ticks;
    u8 window;

    ticks = (extraTicks + 24) + strlen(msg) * 2;
    window = TextAddWindow(-140, -6, 280, 25);
    TextSetWindowJustification(window, 0);
    TextSetStringIntensity(TextAddString(window, msg, ticks, 0), 255, 255, 255);
    return ticks;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/level", LevelMissionCtrMsg);
#endif

/* Wait for any XA voice-over to finish, then roll the end-of-level movie. */
static void func_800172FC(void) {
    if (SysSoundIsXAPlaying()) {
        EGSDisplayQueueInterrupt(func_800172FC, 1, 0xE0C00000);
        return;
    }
    LevelDoEOLMovie(0);
}

/* End-of-level fade finished. */
static void func_80017348(void) {
    if (gObjPlayer->healthInfo->health > 0) {
        gInEOLState = 1;
        SysStatePush(5);
        EGSDisplayQueueInterrupt(func_800172FC, 1, 0xE0C00000);
        return;
    }
    LevelDoRestart();
}

s32 LevelDoEOL(u8 force) {
    char buf[32];

    if (force || D_8010F654 == NULL || D_8010F654()) {
        if (!mGameOverMsgFlag && !mMissionFailedMsgFlag) {
            mMissionFailedMsgFlag = 1;
            mGameOverMsgFlag = 1;
            InterfaceSetMode(0);
            SavePlayerPreserve();
            if (!force) {
                StringMissionCompletedMsg(buf);
                LevelMissionCtrMsg(buf, 16);
            }
            JoystickDeactivateMenu(1);
            gStartedEOLFade = 1;
            EGSDisplayFade(1, 4, func_80017348);
            return 1;
        }
    }
    return 0;
}

void LevelDoRestart(void) {
    SysHalt(3);
}

static void LevelZoom2(s32 *point) {
    TerrainZoom(point, point[3]);
}

s32 LevelZoom(s32 n) {
    s32 *point;

    point = gLevData->zoomPoints[n];
    if (point[0] != 0 || point[1] != 0 || point[2] != 0) {
        EGSDisplayQueueInterrupt((void (*)(void))LevelZoom2, 1, (s32)point);
        GameCameraRequestCutToBehind();
        return 1;
    }
    return 0;
}

static void func_80017544(s32 arg0) {
    if (!mGameOverMsgFlag) {
        mGameOverMsgFlag = 1;
        InterfaceDeleteBomb();
        InterfaceSetMode(0);
        LevelMissionCtrMsg(gGameOverMsg, (arg0 == -1) * 16);
        EGSDisplayFade(1, 4, LevelDoRestart);
    }
}

void LevelGameOver(s32 arg0) {
    if (!mGameOverMsgFlag) {
        EGSDisplayQueueInterrupt((void (*)(void))func_80017544, 1, arg0);
    }
}

#ifdef NON_MATCHING
/* logic matches; argument registers are allocated differently (one extra move) */
void LevelMissionFailed(s32 reason, s32 xa, u8 secondary) {
    char buf[32];

    if (gObjPlayer->healthInfo->health <= 0) {
        return;
    }
    if (!secondary) {
        if (gObjectives != NULL) {
            if (gObjectives->unk20 & (1 << reason)) {
                return;
            }
            gObjectives->unk20 |= 1 << reason;
        }
    } else if (gObjectives != NULL) {
        if (gObjectives->unk14 & (1 << reason)) {
            return;
        }
        gObjectives->unk14 |= 1 << reason;
    }
    if (!gMissionFailed) {
        gMissionFailed = 1;
    }
    if (!mMissionFailedMsgFlag) {
        mMissionFailedMsgFlag = 1;
    }
    if (xa == -2) {
        switch ((u16)gSysInfo.level) {
        case 10:
        case 11:
        case 12:
        case 13:
            PlayerFailMission();
            break;
        default:
            SysPlayXA(0x16C, 0, 1);
            break;
        }
    } else if (xa != -1) {
        SysPlayXA(xa, 0, 1);
    }
    StringObjectiveMsg(buf, reason, secondary, 0);
    if (mNextTickAllowMsgMissionFailed < (u32)gExeTick) {
        mNextTickAllowMsgMissionFailed = gExeTick + LevelMissionCtrMsg(buf, 0) + 16;
        SetNextTickAllowMsgLongText(mNextTickAllowMsgMissionFailed);
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/level", LevelMissionFailed);
#endif

void LevelObjectiveComplete(s16 objective) {
    char buf[32];
    s32 bit;
    s32 ticks;

    if (gObjPlayer->healthInfo->health > 0) {
        if (gObjectives != NULL) {
            bit = 1 << objective;
            if (!(gObjectives->unk18 & bit)) {
                LevelObjectiveAdd(objective, -1);
            }
            gObjectives->completed |= bit;
        }
        StringObjectiveMsg(buf, objective, 1, 1);
        ticks = strlen(buf) * 2 + 24;
        SetNextTickAllowMsgLongText(gExeTick + ticks + 16);
        TextStatusMessage(buf, ticks);
        if (!gMissionFailed) {
            LevelMissionCtrMsg(gCheckpointMsg, 0);
            SaveCreateContinue();
        }
    }
}

void LevelObjectivesIncomplete(void *msg) {
    s32 ticks;

    if ((u32)gExeTick > mNextTickAllowMsgIncompleteObjectives) {
        ticks = strlen(msg) * 2 + 24;
        mNextTickAllowMsgIncompleteObjectives = gExeTick + ticks + 16;
        SetNextTickAllowMsgLongText(mNextTickAllowMsgIncompleteObjectives);
        TextStatusMessage(msg, ticks);
    }
}

void LevelAddingObjectiveInstruction(void) {
    if (D_8010F658 != NULL) {
        TextStatusMessage(D_8010F658(), 100);
    }
}

void func_80017920(void) {
    TextStatusMessage(gObjectiveAddedMsg, 0);
    LevelAddingObjectiveInstruction();
}

void func_80017950(void) {
    if (gPlayerRadioFlag) {
        SysExeTickQueueTimer((void (*)(s32))func_80017950, 1, 0);
        return;
    }
    TextStatusMessage(gObjectiveAddedMsg, 0);
    LevelAddingObjectiveInstruction();
}

#ifdef NON_MATCHING
/* register allocation: original keeps quipFrom in a1 and bit in a2 */
void LevelObjectiveAdd(s32 objective, s32 quipFrom) {
    u32 added;
    s32 bit;
    u16 quip;

    if (gObjectives != NULL) {
        added = gObjectives->unk18;
        bit = 1 << objective;
        if (!(added & bit)) {
            gObjectives->unk18 = added | bit;
            gObjectives->unk1C |= bit;
            if (mLastTickAddedObjectives != gExeTick) {
                gAddingObjectiveViaQuip = 0;
                if ((s16)quipFrom != -1) {
                    quip = ObjectDoQuip(quipFrom, bit, gObjectives);
                } else {
                    quip = 0xFFFF;
                }
                if (quip == 0xFFFF) {
                    EGSDisplayQueueInterrupt(func_80017920, 1, 0);
                } else {
                    gAddingObjectiveViaQuip = 1;
                }
                mLastTickAddedObjectives = gExeTick;
            }
        }
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/level", LevelObjectiveAdd);
#endif

void LevelRemoveContactMeiXingMsgForRadio(void) {
    StringReplaceContactMeiXingMsg(NULL);
    if (gAddingObjectiveViaQuip) {
        gAddingObjectiveViaQuip = 0;
        SysExeTickQueueTimer((void (*)(s32))func_80017950, 40, 0);
    }
}

void LevelRemoveContactMeiXingMsgForAddingObjectiveMsg(void) {
    if (gAddingObjectiveViaQuip) {
        StringReplaceContactMeiXingMsg(gObjectiveAddedMsg);
        LevelAddingObjectiveInstruction();
    }
}

/* Index of an objective among those currently shown. */
#ifdef NON_MATCHING
/* loop register allocation differs */
s16 LevelObjectiveID2DisplayedObjectiveID(s16 objective) {
    s16 shown;
    int i;
    s32 count;

    count = *(s32 *)gObjectives;
    shown = 0;
    for (i = 0; i < count; i++) {
        if (gObjectives != NULL && (gObjectives->unk18 & (1 << i))) {
            if (i == objective) {
                break;
            }
            shown++;
        }
    }
    return shown;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/level", LevelObjectiveID2DisplayedObjectiveID);
#endif

#ifdef NON_MATCHING
/* loop register allocation differs */
s16 LevelParameterID2DisplayedParameterID(s16 param) {
    s16 shown;
    int i;
    s32 count;

    count = gObjectives->count;
    shown = 0;
    for (i = 0; i < count; i++) {
        if (gObjectives != NULL && (gObjectives->unk24 & (1 << i))) {
            if (i == param) {
                break;
            }
            shown++;
        }
    }
    return shown;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/level", LevelParameterID2DisplayedParameterID);
#endif

void LevelSetLevelSpecificNpcID(s16 id) {
    gLevelSpecificNpcID = id;
}

void LevelSetLevelSpecificNpcID2(s16 id) {
    gLevelSpecificNpcID2 = id;
}

INCLUDE_RODATA("asm/main/nonmatchings/level", D_80010140);

u8 *LevelGetMenuHogHeader(void) {
    s32 offset;

    offset = levelLoadHeader->menuHogOffset;
#line 2385
    ASSERT(offset != 0);
    return levelLoadHeader->base + offset;
}
