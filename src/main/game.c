#include "common.h"
#include "system.h"
#include "gameobj.h"
#include "gobj.h"
#include "scratch.h"

/* game.c's data, defined in original order (docs/data_migration.md). */
static void func_80016690(void);
static void func_80016764(void);
static void func_80016840(void);
void GameSysJoystick(void);
void InterfaceUpdate(void);
void InterfaceCountdownUpdate(void);
void ParticleUpdate(void);
void TextUpdate(void);

/* .data: this game's level-message table */
static SysMsgDesc D_800FCB34[46] = {
    { 0, NULL, NULL }, { 1, (SysMsgHandler)func_80016764, NULL }, { 2, NULL, NULL },
    { 3, (SysMsgHandler)func_80016840, NULL }, { 4, (SysMsgHandler)GameSysJoystick, NULL },
    { 5, NULL, NULL }, { 6, NULL, NULL }, { 7, NULL, NULL }, { 8, NULL, NULL }, { 9, NULL, NULL },
    { 10, NULL, NULL }, { 11, NULL, NULL }, { 12, NULL, NULL }, { 13, NULL, NULL },
    { 14, (SysMsgHandler)InterfaceUpdate, NULL },
    { 15, (SysMsgHandler)InterfaceCountdownUpdate, NULL },
    { 16, (SysMsgHandler)ParticleUpdate, NULL }, { 17, (SysMsgHandler)func_80016690, NULL },
    { 18, NULL, NULL }, { 19, NULL, NULL }, { 20, NULL, NULL }, { 21, NULL, NULL },
    { 22, NULL, NULL }, { 23, NULL, NULL }, { 24, NULL, NULL }, { 25, NULL, NULL },
    { 26, NULL, NULL }, { 27, NULL, NULL }, { 28, NULL, NULL }, { 29, NULL, NULL },
    { 30, NULL, NULL }, { 31, (SysMsgHandler)TextUpdate, NULL }, { 32, NULL, NULL },
    { 33, (SysMsgHandler)GameSysEOL, NULL }, { 34, NULL, NULL }, { 35, NULL, NULL },
    { 36, NULL, NULL }, { 37, NULL, NULL }, { 38, NULL, NULL }, { 39, NULL, NULL },
    { 40, NULL, NULL }, { 41, NULL, NULL }, { 42, NULL, NULL }, { 43, NULL, NULL },
    { 44, NULL, NULL }, { 45, NULL, NULL },
};

/* .sdata */
static s16 D_8010F644 = 0; /* unreferenced */
s16 gMoveRequestCount = 0;
s32 gCheatEnabled = 1;
static s32 D_8010F64C = -1; /* gGobjPool allocation cursor */
static s32 D_8010F650 = 0;  /* unreferenced */

/* .bss */
static Gobj D_80111EB8[GOBJ_POOL_SIZE]; /* gobj pool */
static Gobj *D_80112CC8[GOBJ_MOVE_REQUEST_MAX];


extern s32 gExeTick;
extern s32 gInterfaceCurrentMode;

int ratan2(int y, int x);
void EGSFrameSetPosition(EGSFrame *frame, s32 arg1, VECTOR32 *pos, VECTOR32 *move);
void SysChangeLevel(u32 level);
void TextInitialize(s32);
void SysSaveScreen(void);
void InterfaceSetMode(s32 mode);
void EGSDisplayFromLetterbox(void (*done)(void));
void EGSDisplayToLetterbox(void (*done)(void));
static void func_80016C58(void);
void GameSysJoystickInit(void);
void WeaponInit(void);
u8 SaveIsPlayerVirginal(void);
void SaveObjectivesNTimersClear(void);
void SaveRestoreContinue(void);
void SavePlayerRestore(void);
void PlayerReinit(void);

void SYSGameInit(void) {
    gMoveRequestCount = 0;
}

s32 GameSysInitMoveInfo(MoveInfo **out) {
    *out = EGSMemMalloc(sizeof(MoveInfo));
    if (*out == NULL) {
        return 0;
    }
    (*out)->unk174 = 0;
    (*out)->unk17C = 0;
    (*out)->unk190 = 0;
    (*out)->pendingMove.vx = 0;
    (*out)->pendingMove.vy = 0;
    (*out)->pendingMove.vz = 0;
    return 1;
}

s32 GameSysIsTime(s32 since, u32 ticks) {
    return (u32)(gExeTick - since) >= ticks;
}

void GameSysTableHook(void) {
    gSysInfo.levelMsgTable = D_800FCB34;
}

/* Octagonal approximation of the XZ distance between two points. */
s32 GameSysFuzzyMathXZDistance(VECTOR32 *a, VECTOR32 *b) {
    s32 dx;
    s32 dz;
    s32 dist;
    s32 diff;
    s32 half;
    s32 quarter;

    dx = a->vx - b->vx;
    if (dx < 0) {
        dx = b->vx - a->vx;
    }
    dz = a->vz - b->vz;
    dist = dx + dz;
    if (dz < 0) {
        dz = b->vz - a->vz;
        dist = dx + dz;
    }
    diff = dx - dz;
    half = dist >> 1;
    quarter = dist >> 2;
    if (diff < 0) {
        diff = dz - dx;
    }
    if (diff < half) {
        dist -= quarter;
    } else if (half + quarter >= diff) {
        dist -= dist >> 3;
    }
    return dist;
}

/* Re-activate terrain for the current level (on the scratchpad stack), then notify the level. */
static void func_80016690(void) {
    SCRATCH_CALL_DS(TerrainUpdateActivation, "addu $4, $0, $0");
    SysPostMessage(0x11, 0, SYS_MSG_LEVEL, SYS_MSG_LEVEL, 0, 0, 0, 0);
}

void GameSysEOL(void) {
    EGSDeviceControllerVibrate(2, 0, 0);
    if (gSysInfo.level < 18) {
        gSysInfo.level++;
        SysChangeLevel(gSysInfo.level);
        TextInitialize(0);
        SysSaveScreen();
        return;
    }
    SysHalt(5);
}

static const s32 D_800100D4[4] = { 1, 1, 1, 0 }; /* starts every game TU's .rdata */

/* Level loaded: set up the player either fresh, from the continue point, or carried over. */
static void func_80016764(void) {
    GameSysJoystickInit();
    WeaponInit();
    if (SaveIsPlayerVirginal()) {
        SaveObjectivesNTimersClear();
        PlayerReinit();
    } else if (gContinueLevel[0]) {
        SaveRestoreContinue();
    } else {
        SaveObjectivesNTimersClear();
        switch (gSysInfo.level) {
        /* levels that start a new mission: no carried-over loadout */
        case 0:
        case 5:
        case 7:
        case 11:
        case 14:
            PlayerReinit();
            break;
        default:
            SavePlayerRestore();
            break;
        }
    }
    SysPostMessage(0x11, 0, SYS_MSG_LEVEL, SYS_MSG_LEVEL, 0, 0, 0, 0);
}

static void func_80016840(void) {
}

s32 GameSysGetYRotationFromZ(VECTOR32 *dir, s32 *rotY) {
    *rotY = ratan2(dir->vx, dir->vz);
    return 0;
}

void GameSysRestartLevel(void) {
}

void GameSysRequestMove(Gobj *gobj) {
#line 402
    ASSERT(gMoveRequestCount < GOBJ_MOVE_REQUEST_MAX);
    D_80112CC8[gMoveRequestCount++] = gobj;
}

/* follows "s/game.c" in .rdata, so it comes after the first assert */
static const VECTOR32 D_800100F0 = { 0, 0, 0, 0 }; /* zero vector */

static void func_800168FC(Gobj *gobj) {
    VECTOR32 pos;
    MoveInfo *move;
    VECTOR32 *pending;
    EGSFrame *frame;

    move = gobj->move;
    if (move != NULL && ((pending = &move->pendingMove, move->pendingMove.vx != 0) || move->pendingMove.vy != 0 || move->pendingMove.vz != 0)) {
        frame = gobj->anim->frame;
        pos.vx = frame->pos.vx;
        pos.vy = frame->pos.vy;
        pos.vz = frame->pos.vz;
        pos.vy = -pos.vy;
        pos.vx += pending->vx;
        pos.vy += pending->vy;
        pos.vz += pending->vz;
        EGSFrameSetPosition(frame, 0, &pos, pending);
    }
}

void GameSysProcessMoveRequests(void) {
    s16 i;

    for (i = 0; i < gMoveRequestCount; i++) {
        func_800168FC(D_80112CC8[i]);
    }
}

void GameSysClearMoveRequests(void) {
    VECTOR32 zero;
    s16 i;

    zero = D_800100F0;
    for (i = 0; i < gMoveRequestCount; i++) {
        D_80112CC8[i]->move->pendingMove = zero;
    }
    gMoveRequestCount = 0;
}

Gobj *GameGetGobj(s16 id) {
    s32 start;
    Gobj *gobj;

    start = D_8010F64C;
    if (++D_8010F64C >= GOBJ_POOL_SIZE) {
        D_8010F64C = 0;
    }
    while (D_8010F64C != start) {
        gobj = &D_80111EB8[D_8010F64C];
        if (gobj->id == -1) {
            gobj->id = id;
            return gobj;
        }
        if (++D_8010F64C >= GOBJ_POOL_SIZE) {
            D_8010F64C = 0;
        }
    }
#line 502
    ASSERT(0);
    return NULL;
}

s32 GameFreeGobj(s16 id) {
    GameObjectInfo *info;

    info = &gameObjectsInfo[id];
#line 514
    ASSERT(info->runtime != NULL);
#line 515
    ASSERT(info->runtime->id == id);
    info->runtime->id = -1;
    info->runtime = NULL;
    return 1;
}

static void func_80016C58(void) {
}

void GameSysSetLetterBoxMode(u32 mode) {
    switch (mode) {
    case 0:
        InterfaceSetMode(1);
        EGSDisplayFromLetterbox(func_80016C58);
        break;
    case 1:
        InterfaceSetMode(0);
        EGSDisplayToLetterbox(func_80016C58);
        break;
    case 2:
        InterfaceSetMode(0);
        EGSDisplayToLetterbox(NULL);
        break;
    case 3:
        EGSDisplayFromLetterbox(NULL);
        if (gInterfaceCurrentMode == 0) {
            InterfaceSetMode(1);
        }
        break;
    }
}
