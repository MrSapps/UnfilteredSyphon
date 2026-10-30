#include "common.h"
#include "level.h"

/* Object types specific to this level */
#define SUBWAY2_TYPE_BOMB_SWITCH 0x57
#define SUBWAY2_TYPE_BOMB 0x58
#define SUBWAY2_TYPE_DEVICE 0x76

/* subway2.ovl statics, still defined in the overlay's asm .data/.bss */
extern GameObjectRuntime *D_801420AC; /* cop linked to the bomb */
extern s16 D_801420B0;                /* bomb object id */
extern s16 D_801420B4;                /* device object id */
extern u8 D_801420B8;                 /* countdown running */
extern u8 D_801420BC;                 /* bomb armed */
extern void *D_80142070;              /* status messages */
extern s32 D_80142074;
extern s32 D_80142078;
extern s32 D_8014207C;
extern s32 D_801077FC;

static void func_801412D0();

INCLUDE_ASM("asm/subway2/nonmatchings/subway2", func_801412D0);

static void func_80141400(s16 id, s16 by) {
    if (by == gPlayerOffset) {
        NPCCopDamagedCallback(id, by);
        ObjectBombDamagedCallback(id, by);
    }
}

INCLUDE_ASM("asm/subway2/nonmatchings/subway2", func_8014145C);

static s32 func_801415A0(s32 arg0, s32 arg1, s32 arg2, s32 id) {
    s32 ret;

    if (!gMissionFailed && gObjectives != NULL && (gObjectives->completed & 2)) {
        ret = 1;
        SysPlayXA(0x126, 0, 1);
    } else {
        ObjectBombDeath(1, 0x97, 1, id);
        ret = 0;
    }
    return ret;
}

static void func_80141624(void) {
    D_801420BC = 0;
    SysPlayXA(0x5A, 0, 1);
    LevelObjectiveComplete(1);
}

static void func_8014165C(s16 id) {
    GameObjectRuntime *rt;
    int type;

    type = GOBJ_TYPE(id);
    if (type == SUBWAY2_TYPE_BOMB_SWITCH) {
        rt = gameObjectsInfo[D_801420B0].runtime;
        rt->flags0 &= ~0x10;
        WeaponAdd(0x18, 1);
        TextStatusMessage(D_80142070, 0);
        SysPlayXA(0x99, 0, 1);
        LevelObjectiveComplete(0);
    } else if (type == SUBWAY2_TYPE_BOMB) {
        if (gObjectives != NULL && (gObjectives->completed & 1)) {
            WeaponRemove(0x18);
            D_801420B8 = 1;
            InterfaceStartCountdown(5, 0, func_801412D0);
        }
    }
}

INCLUDE_ASM("asm/subway2/nonmatchings/subway2", func_801417B0);

INCLUDE_ASM("asm/subway2/nonmatchings/subway2", func_801418BC);

INCLUDE_ASM("asm/subway2/nonmatchings/subway2", func_80141AC0);

static s32 func_80141B78(void) {
    return D_80142074;
}

static s32 func_80141B88(s16 id, s16 kind) {
    if (kind == 3) {
        if (id != GOBJ_ID_666 && gameObjectsModelInfo[gameObjectsInfo[id].modelIndex].type == SUBWAY2_TYPE_BOMB_SWITCH) {
            return D_801077FC;
        }
        return D_80142078;
    }
    if (kind == 4) {
        return D_8014207C;
    }
    return 0;
}

static void func_80141C34(void) {
    SysPlaySong(0, 7, 0, 0);
    SysPlaySong(0, 13, 0, 0);
}

INCLUDE_ASM("asm/subway2/nonmatchings/subway2", iSubway2Init);

INCLUDE_RODATA("asm/subway2/nonmatchings/subway2", D_80140FE4);
