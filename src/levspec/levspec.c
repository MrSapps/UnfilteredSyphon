#include "common.h"
#include "weapon.h"
#include "level.h"
#include "system.h"

/* Catacombs: object types */
#define CATA_TYPE_SCIENTIST 0x65
#define CATA_TYPE_CELL_DOOR 0x22
/* Church: object types */
#define CHURCH_TYPE_SCIENTIST 0x65
#define CHURCH_TYPE_SUBJECT 0x5C

/* levspec.ovl statics, still defined in the overlay's asm .data/.bss */
extern u8 D_80142A30[];   /* catacombs saved objective state */
extern u8 D_80142A58[];   /* church saved objective state */
extern void **D_80142A5C; /* the two objective lines shown for this level */
extern u8 D_80142A80[];   /* objective line buffers */
extern u8 D_80142AC0[];
extern void *D_80142A14;  /* "Eliminate ... scientist" format */
extern void *D_80142A18;  /* "Administer antigen ..." format */
extern void *D_80142A1C;
extern void *D_80142A20;
extern void *D_80142A24;
extern void *D_80142A28;
extern void *D_80142A2C;
extern u8 D_80140FE4[];
extern s32 D_80142B00;    /* scientists total */
extern s32 D_80142B04;    /* scientists killed */
extern s32 D_80142B08;    /* subjects total */
extern s32 D_80142B0C;    /* subjects inoculated */

static void func_801418E0();
static void func_80141D5C(s16 id);
static s32 func_80141AA4(s32 arg0);
static void func_80141A40(s16 id);
static void func_80141B70(s16 id);
static s32 func_80141BA4(s16 id);
static void func_80141EB4(s32 arg0, s16 weapon);
static void func_80141EFC(void);
static void func_801421F8(void);
static void func_8014224C(void);

void CatacombRescueCheatZoomPoint(void) {
    s32 ref;

    if (gObjectives != NULL) {
        if (!(gObjectives->unk18 & 0x10)) {
            LevelObjectiveAdd(4, -1);
        }
        gObjectives->completed |= 0x10;
    }
    if (gObjectives != NULL) {
        gObjectives->unk18 |= 0x20;
        gObjectives->unk1C |= 0x20;
        if (!(gObjectives->unk18 & 0x20)) {
            LevelObjectiveAdd(5, -1);
        }
        gObjectives->completed |= 0x20;
        if (gObjectives != NULL) {
            gObjectives->unk18 |= 0x40;
            gObjectives->unk1C |= 0x40;
        }
    }
    if (gLevelSpecificNpcID != -1) {
        ref = gameObjectsInfo[gLevelSpecificNpcID].refId;
        if (ref != -1) {
            SysPostMessage(0x14, 4, gLevelSpecificNpcID, ref, 0, 0, 0, 0);
        }
    }
}

static void func_801418E0() {
    if (!gLevelAlerted) {
        gLevelAlerted = 1;
        TextStatusMessage(gLevelAlertedMsg, 0);
        npcSetLevelAlert();
        WeaponSetWeaponFiredCallback(NULL);
    }
}

static void func_80141930(s16 id) {
    s32 ref;

    if (gMissionFailed) {
        return;
    }
    if (id != GOBJ_ID_666 && gameObjectsModelInfo[gameObjectsInfo[id].modelIndex].type == CATA_TYPE_SCIENTIST) {
        ref = gameObjectsInfo[id].refId;
        if (ref != -1 && !(gameObjectsInfo[ref].runtime->flags0 & 0x20)) {
            LevelMissionFailed(0, -2, 0);
        }
    } else if (id == gLevelSpecificNpcID) {
        LevelMissionFailed(1, -2, 0);
    } else if (id == gLevelSpecificNpcID2) {
        LevelMissionFailed(2, -2, 0);
    }
}

static void func_80141A40(s16 id) {
    if (id == gLevelSpecificNpcID2) {
        SysPlayXA(5, (s32)gameObjectsInfo[id].runtime, 0);
        InterfaceSetAddLevelSpecificNPCCallback(NULL);
    }
}

#ifdef NON_MATCHING
/* GCC threads the first gMissionFailed test past the second one; original does not. */
static s32 func_80141AA4(s32 arg0) {
    u32 completed;

    if (!gMissionFailed && gObjectives != NULL) {
        completed = gObjectives->completed;
        if (completed & 0x10) {
            arg0 = 7;
            if (completed & 0x20) {
                SysPlayXA(arg0, (s32)gameObjectsInfo[gLevelSpecificNpcID2].runtime, 0);
                return 1;
            }
        }
    }
    if (gMissionFailed) {
        StringMissionFailedMsg(arg0);
    } else {
        LevelObjectivesIncomplete(gObjectivesIncompleteMsg);
    }
    return 0;
}
#else
INCLUDE_ASM("asm/levspec/nonmatchings/levspec", func_80141AA4);
#endif

static void func_80141B70(s16 id) {
    if (id == gLevelSpecificNpcID2) {
        DamageDestroyImmediately(-1, id);
    }
}

static s32 func_80141BA4(s16 id) {
    if (id != ObjectInvisibleMatchToRefID(id, 0x5F)) {
        SysPlayXA(4, 0, 1);
    }
    return 1;
}

INCLUDE_RODATA("asm/levspec/nonmatchings/levspec", D_80140FE4);

static void func_80141BF0(s16 id) {
    char *msg;

    if (id != GOBJ_ID_666 && gameObjectsModelInfo[gameObjectsInfo[id].modelIndex].type == CATA_TYPE_CELL_DOOR) {
        gameObjectsInfo[gLevelSpecificNpcID].runtime->flags23 &= ~2;
        ObjectDoMovieEx("TEMP", 0x2D, id, id);
        msg = "MOVIE FOR FINDING PHAGAN";
    } else if (!gMissionFailed && gObjectives != NULL && (gObjectives->completed & 0x10)) {
        gameObjectsInfo[gLevelSpecificNpcID].runtime->flags23 |= 2;
        ObjectDoMovieEx("TEMP", 0x2D, id, id);
        msg = "MOVIE FOR FINDING MEI";
    } else {
        return;
    }
    LevelMissionCtrMsg(msg, 0);
}

#ifdef NON_MATCHING
/* delay slot: original fills the gLevelAlerted branch slot with the id move */
static void func_80141D5C(s16 id) {
    s32 objective;

    if (id != GOBJ_ID_666 && gameObjectsModelInfo[gameObjectsInfo[id].modelIndex].type == CATA_TYPE_CELL_DOOR) {
        if (!gLevelAlerted) {
            func_801418E0(id);
        }
        ObjectTeleportPlayer(id, 0x39);
        GameCameraRequestCutToBehind();
        LevelObjectiveAdd(5, -1);
        LevelObjectiveAdd(6, -1);
        objective = 4;
        if (gObjectives != NULL) {
            gObjectives->unk24 |= 4;
        }
    } else {
        InterfaceSetAddLevelSpecificNPCCallback(func_80141A40);
        gameObjectsInfo[gLevelSpecificNpcID2].runtime->flags23 &= ~2;
        NPCSetCopDeactivateCallback(func_80141B70);
        objective = 5;
    }
    LevelObjectiveComplete(objective);
}
#else
INCLUDE_ASM("asm/levspec/nonmatchings/levspec", func_80141D5C);
#endif

static void func_80141EB4(s32 arg0, s16 weapon) {
    if (!((gWeaponInfo[weapon].flags >> 14) & 1)) {
        func_801418E0();
    }
}

static void func_80141EFC(void) {
    SysPlaySong(0, 12, 0, 0);
}

void iCatacombsInit(s32 mode) {
    GameObjectRuntime *phagan;
    GameObjectRuntime *mei;
    u32 completed;

    SaveObjectivesNTimersReinit(D_80142A30, 0);
    if (gObjectives->unk18 == 0) {
        LevelObjectiveAdd(4, -1);
        if (gObjectives != NULL) {
            gObjectives->unk24 |= 3;
        }
    }
    if (mode == 3) {
        func_80141EFC();
    } else {
        gSysMovieDoneCallback = func_80141EFC;
    }
    DamageSetDestroyedCallback((void (*)(s16, s16))func_80141930);
    LevelSetEOLCallback(func_80141AA4);
    UseSetSwitchCallback(func_80141BA4);
    ObjectSetSpecialObjectCallback(func_80141BF0);
    ObjectSetMovieDoneCallback(func_80141D5C);
    if (gObjectives == NULL || !(gObjectives->completed & 0x10)) {
        WeaponSetWeaponFiredCallback(func_80141EB4);
    } else {
        npcSetLevelAlert();
    }

    if (gLevelSpecificNpcID != -1) {
        phagan = gameObjectsInfo[gLevelSpecificNpcID].runtime;
        gLevelSpecificNpcName = gPhaganMsg;
        phagan->npc->nameMsgId = 0x55;
        if (gObjectives == NULL || ((completed = gObjectives->completed) & 0x10) == 0 || (completed & 0x20)) {
            phagan->flags23 |= 2;
        }
        phagan->npc->flags |= 1;
        phagan->npc->unk48 = 2;
    }
    if (gLevelSpecificNpcID2 != -1) {
        mei = gameObjectsInfo[gLevelSpecificNpcID2].runtime;
        gLevelSpecificNpc2Name = gMeiXingMsg;
        mei->npc->nameMsgId = 0x54;
        if (gObjectives == NULL || !(gObjectives->completed & 0x20)) {
            mei->flags23 |= 2;
        } else {
            NPCSetCopDeactivateCallback(func_80141B70);
        }
        mei->npc->flags |= 1;
        mei->npc->unk48 = 2;
        CASRequestAction(mei->id, 0x59);
    }
    gDefaultLevelDmgFX = 11;
}

static void func_801421F8(void) {
    StringObjectiveObjectsRemaining2(D_80142A80, D_80142A14, gRemainingMsg, D_80140FE4, D_80142B04, D_80142B00);
}

static void func_8014224C(void) {
    StringObjectiveObjectsRemaining2(D_80142AC0, D_80142A18, gRemainingMsg, D_80140FE4, D_80142B0C, D_80142B08);
}

static void func_801422A0(s16 id, s16 by) {
    int type;

    type = GOBJ_TYPE(id);
    if (type == CHURCH_TYPE_SCIENTIST) {
        D_80142B04++;
        if (D_80142B04 < D_80142B00) {
            StringInteractObjectsRemaining(D_80142A1C, D_80142B00 - D_80142B04);
            func_801421F8();
        } else {
            func_801421F8();
            LevelObjectiveComplete(0);
        }
    } else if (type == CHURCH_TYPE_SUBJECT) {
        if (gMissionFailed) {
            return;
        }
        if (by == gPlayerOffset) {
            if (gObjectives == NULL || !(gObjectives->unk20 & 1)) {
                LevelMissionFailed(0, -2, 0);
            }
            if (gObjectives != NULL) {
                gObjectives->unk14 |= 2;
            }
        } else if (gObjectives == NULL || !(gObjectives->unk14 & 2)) {
            LevelMissionFailed(1, -2, 1);
        }
    }
}

static s32 func_80142444(void) {
    if (!gMissionFailed && gObjectives != NULL && (gObjectives->completed & 1) && (gObjectives->completed & 2)) {
        return 1;
    }
    if (gMissionFailed) {
        StringMissionFailedMsg();
    } else {
        LevelObjectivesIncomplete(gObjectivesIncompleteMsg);
    }
    return 0;
}

static void func_801424D0(s16 id) {
    D_80142B0C++;
    if (D_80142B0C < D_80142B08) {
        StringInteractObjectsRemaining(D_80142A20, D_80142B08 - D_80142B0C);
        func_8014224C();
    } else {
        func_8014224C();
        LevelObjectiveComplete(1);
    }
    CASRequestAction(id, 10);
}

static s32 func_80142558(s16 id) {
    TextStatusMessage(D_80142A2C, 0);
    ObjectRefMultiSYSPOSTMSG0(id, 0x1B, 3);
    return 0;
}

static void *func_801425A4(s16 id, s16 kind) {
    if (kind != 10) {
        return gLockCodeRequiredMsg;
    }
    return gElectronicallyLockedMsg;
}

static void *func_801425D8(s16 id, s16 kind) {
    if (kind != 3) {
        return D_80142A28;
    }
    return D_80142A24;
}

static void func_8014260C(void) {
    if (gLevelKeycards != gLevelKeycardsTaken) {
        if (!gMissionFailed) {
            LevelMissionCtrMsg(gCheckpointMsg, 0);
            SaveCreateContinue();
        }
    } else {
        LevelObjectiveComplete(2);
    }
}

void iChurchInit(void) {
    int i;
    int bit;
    s32 count;
    u32 *words;
    u32 misalign;
    s32 word;
    u32 mask;
    GameObjectRuntime *rt;
    int type;

    D_80142B00 = 0;
    D_80142B04 = 0;
    D_80142B08 = 0;
    D_80142B0C = 0;
    D_80142A5C[0] = D_80142A80;
    D_80142A5C[1] = D_80142AC0;
    SaveObjectivesNTimersReinit(D_80142A58, 0);
    if (gObjectives->unk18 == 0) {
        LevelObjectiveAdd(0, -1);
        LevelObjectiveAdd(1, -1);
        LevelObjectiveAdd(2, -1);
        LevelObjectiveAdd(3, -1);
        if (gObjectives != NULL) {
            count = gObjectives->count;
            for (bit = 0; bit < count; bit++) {
                if (gObjectives != NULL) {
                    gObjectives->unk24 |= 1 << bit;
                }
            }
        }
    }
    DamageSetDestroyedCallback(func_801422A0);
    LevelSetEOLCallback(func_80142444);
    UseSetLevSpecObjCallback(func_801424D0);
    UseSetSwitchCallback(func_80142558);
    SetLockedStringCallback(func_801425A4);
    SetUseStringCallback(func_801425D8);
    PickupSetKeyCallback(func_8014260C);

    for (i = 0; i < gLevelGameObjCount; i++) {
        rt = gameObjectsInfo[i].runtime;
        type = GOBJ_TYPE(i);
        if (type == CHURCH_TYPE_SCIENTIST) {
            D_80142B00++;
            if (rt->healthInfo->health <= 0) {
                D_80142B04++;
            }
        } else if (type == CHURCH_TYPE_SUBJECT) {
            D_80142B08++;
            CASRequestAction(i, 0x57);
            if (rt->flags0 & 0x20) {
                D_80142B0C++;
            }
        }
    }
    /* the player always gets weapon 0x19 here (bit test tolerant of an unaligned base) */
    words = (u32 *)gWeaponsAvailable;
    bit = 0x19;
    misalign = (u32)words & 3;
    if (misalign) {
        bit = misalign * 8 + 0x19;
        words = (u32 *)((u32)words & ~3);
    }
    word = bit >> 5;
    mask = 1 << (bit & 0x1F);
    if (!(words[word] & mask)) {
        WeaponAdd(0x19, 1);
    }
    func_801421F8();
    func_8014224C();
    gDefaultLevelDmgFX = 11;
}
