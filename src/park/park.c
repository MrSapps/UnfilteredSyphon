#include "common.h"
#include "level.h"
#include "system.h"

/* Object types specific to this level */
#define PARK_TYPE_HOSTAGE 0x01
#define PARK_TYPE_MEI_XING 0x04
#define PARK_TYPE_EXIT 0x0E

/*
 * The bss items sit 4 bytes apart, as aspsx 2.56's .lcomm rule lays them out
 * (docs/data_migration.md).  aspsx 2.77 and maspsx pack them by size instead, so
 * both builds get explicit padding.
 */
#define BSS_PAD3(n) static u8 n; static s16 n##_2;

/* Mission briefing shown while the level loads (init.ovl reads it through gBriefings). */
typedef struct Briefing {
    /* 0x00 */ char *lines[3];
    /* 0x0C */ s32 count;
    /* 0x10 */ char **objectives;
} Briefing;

/* Fog scrim mesh handed to EGSDisplayRegisterFogScrim. */
typedef struct ParkFogScrim {
    /* 0x00 */ u32 *verts;   /* 121 SVECTORs */
    /* 0x04 */ u32 *colors;  /* 121 CVECTORs (code 0x3A) */
    /* 0x08 */ u32 *indices; /* 136 quads */
    /* 0x0C */ s16 quadCount;
    /* 0x0E */ s16 vertCount;
    /* 0x10 */ s32 unk10;
    /* 0x14 */ s32 unk14;
} ParkFogScrim;

static const s32 D_80140FE4[5] = {0, 1, 1, 1, 0};

static char *D_80142840[2] = {
    "INCOMING FROM BENTON:\n\nYour new orders are to find the viral bombs in Washington Park, set "
    "beacons on them, and then protect the CBDC when they come to disarm them.  Your primary "
    "objective remains eliminating Rhoemer's team.  Rhoemer is nowhere to be found, but Girdeux is "
    "nearby, probably guarding the final bomb.  \n",
    "Intel indicates there may be other key terrorists in the park as well.  Eliminate any you "
    "encounter.  According to our CBDC contact, the viral bombs in the park are set to detonate "
    "within the hour, so speed is essential.",
};
Briefing parkBriefings = {
    {"Washington DC", "Washington Park", "08:24 00:45\n"}, 2, D_80142840,
};
static char *D_8014285C[5] = {
    (char *)D_80140FE4, /* replaced by the bombs objective text (D_801430A4) */
    "Rescue CBDC hostages",
    "Secure terrorist comm. array",
    "Eliminate trigger man Marcos",
    "Reach Freedom Memorial",
};
static char *D_80142870[4] = {
    "Do not kill any member of the strike team",
    "Do not damage any bomb",
    "All bombs must be defused in under 20 minutes",
    "Do not damage the comm. array",
};
static char *D_80142880 = "Locate and disarm %d%s viral bomb%s"; /* objective format string */
static char *D_80142884 = "them";                /* "bombs" (plural) */
static char *D_80142888 = "it";                  /* "bomb" (last one) */
static char *D_8014288C = "%d Bomb%s Remaining"; /* bombs-remaining status format */
static char *D_80142890 = "Communications array damaged"; /* computer already used message */
static char *D_80142894 = "Marcos";              /* Mei Xing's name */
/* the original .rdata ends in a stray byte of linker padding */
static const char D_801413B7 = 0x8F;
LevelObjectives parkObjectives = {5, D_8014285C, 4, D_80142870}; /* saved objective/timer state */
static u32 D_801428C0[242] = {
    0xF001FFDE, 0x0000003C, 0xF11F01E9, 0x0000FA71, 0xF48401A6, 0x0000F4FC,
    0xF9ACFF84, 0x0000F150, 0xFFCDFC9D, 0x0000F05D, 0x05F9FA54, 0x0000F247,
    0x0B3AF9E1, 0x0000F660, 0x0EC0FBE1, 0x0000FB5D, 0x0FFF0022, 0x0000FFC4,
    0xF123FFB2, 0x0000FA18, 0xF48DFD82, 0x0000F51C, 0xF9BBFA3A, 0x0000F275,
    0xFFE1F736, 0x0000F2A1, 0x060DF5CF, 0x0000F53F, 0x0B4BF6E5, 0x0000F940,
    0x0ECAFA93, 0x0000FD30, 0xF129FD81, 0x0000FA92, 0xF49BF9B1, 0x0000F6B5,
    0xF9D0F5B5, 0x0000F56D, 0xFFFAF2FD, 0x0000F6B0, 0x0625F2A8, 0x0000F9A5,
    0x0B5DF523, 0x0000FCFF, 0x0ED3FA03, 0x0000FF5B, 0xF132FBA2, 0x0000FBCC,
    0xF4ACF6B5, 0x0000F994, 0xF9E7F28E, 0x0000F9D3, 0x0013F082, 0x0000FBFD,
    0x063BF14D, 0x0000FEE3, 0x0B6DF4DA, 0x00000121, 0x0EDBFA45, 0x00000195,
    0xF13CFA54, 0x0000FD9F, 0xF4BEF4F3, 0x0000FD54, 0xF9FEF133, 0x0000FF11,
    0x002AF01B, 0x000001D5, 0x064EF1ED, 0x00000442, 0x0B79F613, 0x00000516,
    0x0EE0FB50, 0x00000390, 0xF145F9C4, 0x0000FFCA, 0xF4CEF4AA, 0x00000175,
    0xFA11F1D2, 0x00000470, 0x003BF1D5, 0x0000076E, 0x065AF470, 0x0000090C,
    0x0B80F8A4, 0x00000856, 0x0EE2FCFF, 0x0000050B, 0xF14DFA06, 0x00000204,
    0xF4DAF5E3, 0x0000056B, 0xFA1DF456, 0x0000093A, 0x0044F575, 0x00000C07,
    0x065FF882, 0x00000C9E, 0x0B80FC32, 0x00000A72, 0x0EE0FF1A, 0x000005D0,
    0xF152FB12, 0x000003FF, 0xF4E1F873, 0x000008AB, 0xFA22F868, 0x00000CCC,
    0x0044FA7E, 0x00000F04, 0x065BFD97, 0x00000E7A, 0x0B7A0049, 0x00000B23,
    0x0EDB0157, 0x000005C6, 0xF154FCC0, 0x0000057A, 0xF4E1FC02, 0x00000AC7,
    0xFA1EFD7D, 0x00000EA8, 0x003C0045, 0x00000FFE, 0x064F0300, 0x00000E62,
    0x0B6E0457, 0x00000A50, 0x0ED4036C, 0x000004EF, 0xF152FEDB, 0x0000063E,
    0xF4DB0019, 0x00000B78, 0xFA1202E6, 0x00000E90, 0x002B0602, 0x00000ED3,
    0x063C0804, 0x00000C5B, 0x0B5E07D4, 0x00000815, 0x0ECA050E, 0x00000367,
    0xF14D0118, 0x00000635, 0xF4CF0427, 0x00000AA5, 0xF9FF07EA, 0x00000C89,
    0x00150AF1, 0x00000BAB, 0x06260BF6, 0x000008A7, 0x0B4C0A48, 0x000004BF,
    0x0EC10607, 0x00000162, 0xF146032D, 0x0000055E, 0xF4BF07A4, 0x00000869,
    0xF9E90BDC, 0x000008D5, 0xFFFB0E68, 0x000006F3, 0x060F0E50, 0x000003C9,
    0x0B3B0B5E, 0x000000C0, 0x0EB80636, 0x0000FF27, 0xF13C04CF, 0x000003D6,
    0xF4AE0A17, 0x00000514, 0xF9D20E36, 0x000003F7, 0xFFE30FF2, 0x0000014D,
    0x05FA0EC1, 0x0000FE65, 0x0B2C0AF0, 0x0000FCA1, 0x0EB10593, 0x0000FD01,
    0xF13305C8, 0x000001D1, 0xF49C0B2E, 0x00000115, 0xF9BD0EA7, 0x0000FE93,
    0xFFCF0F58, 0x0000FB79, 0x05EA0D38, 0x0000F934, 0x0B23090F, 0x0000F8F1,
    0x0EAE0435, 0x0000FB3A, 0xF12A05F7, 0x0000FF96, 0xF48E0AC0, 0x0000FCF6,
    0xF9AC0D1E, 0x0000F962, 0xFFC10CAF, 0x0000F641, 0x05E109EC, 0x0000F4EA,
    0x0B1F05FC, 0x0000F62D, 0x0EAD024C, 0x0000FA0F, 0xF1230554, 0x0000FD70,
    0xF48408DF, 0x0000F946, 0xF9A409D2, 0x0000F518, 0xFFBC0854, 0x0000F258,
    0x05E0054D, 0x0000F219, 0x0B22021D, 0x0000F4B3, 0x0EB10018, 0x0000F9A9,
    0xF12003F6, 0x0000FBA9, 0xF48005CC, 0x0000F682, 0xF9A30533, 0x0000F246,
    0xFFC002DB, 0x0000F042, 0x05E8FFFC, 0x0000F121, 0x0B2BFDF9, 0x0000F4B8,
    0x0EB7FDE4, 0x0000FA15,
};
static u32 D_80142C88[121] = {
    0x3A869C85, 0x3A839982, 0x3A829881, 0x3A637563, 0x3A1A1F19, 0x3A0A0C0A,
    0x3A060706, 0x3A050605, 0x3A040404, 0x3A839982, 0x3A788C77, 0x3A5F705F,
    0x3A191C18, 0x3A0A0C0A, 0x3A070807, 0x3A050605, 0x3A839982, 0x3A778B76,
    0x3A647564, 0x3A191D18, 0x3A0A0C0A, 0x3A060706, 0x3A050605, 0x3A839982,
    0x3A829881, 0x3A657765, 0x3A1A1E19, 0x3A0B0D0B, 0x3A070807, 0x3A050605,
    0x3A839982, 0x3A768A75, 0x3A607060, 0x3A1A1E19, 0x3A0B0D0B, 0x3A060706,
    0x3A050605, 0x3A839982, 0x3A819780, 0x3A5F6F5F, 0x3A191D18, 0x3A0B0D0B,
    0x3A070807, 0x3A050605, 0x3A839982, 0x3A798D78, 0x3A5D6E5D, 0x3A1A1E19,
    0x3A0B0D0B, 0x3A070807, 0x3A050605, 0x3A839982, 0x3A7E937D, 0x3A5F6F5F,
    0x3A1A1E19, 0x3A0A0C0A, 0x3A070807, 0x3A050605, 0x3A839982, 0x3A7E937D,
    0x3A5D6D5D, 0x3A191D18, 0x3A0B0D0B, 0x3A060706, 0x3A050605, 0x3A839982,
    0x3A80967F, 0x3A667866, 0x3A191D18, 0x3A0A0C0A, 0x3A070807, 0x3A050605,
    0x3A839982, 0x3A7C917B, 0x3A5F6F5F, 0x3A191D18, 0x3A0B0D0B, 0x3A070807,
    0x3A050605, 0x3A839982, 0x3A819680, 0x3A657665, 0x3A191C18, 0x3A0A0C0A,
    0x3A070807, 0x3A050605, 0x3A839982, 0x3A798D78, 0x3A627362, 0x3A1A1E19,
    0x3A0A0C0A, 0x3A060706, 0x3A050605, 0x3A839982, 0x3A80967F, 0x3A647664,
    0x3A1B1F1A, 0x3A0B0D0B, 0x3A060706, 0x3A050605, 0x3A839982, 0x3A7E937D,
    0x3A637463, 0x3A1A1E19, 0x3A0A0C0A, 0x3A070807, 0x3A050605, 0x3A839982,
    0x3A7B8F7A, 0x3A5E6F5E, 0x3A191D18, 0x3A0B0D0B, 0x3A070807, 0x3A050605,
    0x3A839982, 0x3A798D78, 0x3A637463, 0x3A1A1E19, 0x3A0B0D0B, 0x3A060706,
    0x3A050605,
};
static u32 D_80142E6C[136] = {
    0x09010000, 0x0A020901, 0x0B030A02, 0x0C040B03, 0x0D050C04, 0x0E060D05,
    0x0F070E06, 0x08080F07, 0x10090000, 0x110A1009, 0x120B110A, 0x130C120B,
    0x140D130C, 0x150E140D, 0x160F150E, 0x0808160F, 0x17100000, 0x18111710,
    0x19121811, 0x1A131912, 0x1B141A13, 0x1C151B14, 0x1D161C15, 0x08081D16,
    0x1E170000, 0x1F181E17, 0x20191F18, 0x211A2019, 0x221B211A, 0x231C221B,
    0x241D231C, 0x0808241D, 0x251E0000, 0x261F251E, 0x2720261F, 0x28212720,
    0x29222821, 0x2A232922, 0x2B242A23, 0x08082B24, 0x2C250000, 0x2D262C25,
    0x2E272D26, 0x2F282E27, 0x30292F28, 0x312A3029, 0x322B312A, 0x0808322B,
    0x332C0000, 0x342D332C, 0x352E342D, 0x362F352E, 0x3730362F, 0x38313730,
    0x39323831, 0x08083932, 0x3A330000, 0x3B343A33, 0x3C353B34, 0x3D363C35,
    0x3E373D36, 0x3F383E37, 0x40393F38, 0x08084039, 0x413A0000, 0x423B413A,
    0x433C423B, 0x443D433C, 0x453E443D, 0x463F453E, 0x4740463F, 0x08084740,
    0x48410000, 0x49424841, 0x4A434942, 0x4B444A43, 0x4C454B44, 0x4D464C45,
    0x4E474D46, 0x08084E47, 0x4F480000, 0x50494F48, 0x514A5049, 0x524B514A,
    0x534C524B, 0x544D534C, 0x554E544D, 0x0808554E, 0x564F0000, 0x5750564F,
    0x58515750, 0x59525851, 0x5A535952, 0x5B545A53, 0x5C555B54, 0x08085C55,
    0x5D560000, 0x5E575D56, 0x5F585E57, 0x60595F58, 0x615A6059, 0x625B615A,
    0x635C625B, 0x0808635C, 0x645D0000, 0x655E645D, 0x665F655E, 0x6760665F,
    0x68616760, 0x69626861, 0x6A636962, 0x08086A63, 0x6B640000, 0x6C656B64,
    0x6D666C65, 0x6E676D66, 0x6F686E67, 0x70696F68, 0x716A7069, 0x0808716A,
    0x726B0000, 0x736C726B, 0x746D736C, 0x756E746D, 0x766F756E, 0x7770766F,
    0x78717770, 0x08087871, 0x01720000, 0x02730172, 0x03740273, 0x04750374,
    0x05760475, 0x06770576, 0x07780677, 0x08080778,
};
static ParkFogScrim D_8014308C = {D_801428C0, D_80142C88, D_80142E6C, 0x88, 0x79, 0, 0x004D702F}; /* fog scrim */

static char D_801430A4[0x60];         /* objective text buffer */
static GameObjectRuntime *D_80143104; /* hostage */
static u8 D_80143108; BSS_PAD3(D_80143108_pad) /* bombs total */
static u8 D_8014310C; BSS_PAD3(D_8014310C_pad) /* bombs defused */
static u8 D_80143110; BSS_PAD3(D_80143110_pad)
static u8 D_80143114; BSS_PAD3(D_80143114_pad) /* Mei Xing hit warning played */
static u8 D_80143118; BSS_PAD3(D_80143118_pad) /* hostage lost */

extern s32 xaLevelStartIndex[];
extern void *gCommArrayMsg;
extern void *gHitNPCCallback;

typedef struct ParkCamera {
    /* 0x00 */ u8 unk00[0x8C];
    /* 0x8C */ EGSListNode *objects;
} ParkCamera;

typedef struct ParkDisplayObj {
    /* 0x00 */ u8 unk00[0x14];
    /* 0x14 */ u16 id; /* low 10 bits object id, 0x2000 = game object */
} ParkDisplayObj;

void LevelSetDirectionPointerFlag(s32 flag);
void LevelSetPointer(s32 id);
void ObjectDetonateBomb(GameObjectRuntime *rt);
void NPCCombatModeCallback(void (*cb)());
void NPCInitHostageTaker(s32 id);
void NPCShootHostage(s16 id);
void SysSoundQueueXA(s32 arg0, s32 track);
void JoystickDeactivate(void);
void StringContactMeiXingMsg(void);
void InterfaceCountdownReset(s32 arg0);
s16 InterfaceCountdownGet(void);
void InterfaceStartCountdownAfterGameCam(s16 seconds, s32 arg1, void (*done)());
void EGSDisplayRegisterFogScrim(void *scrim);
void ParticleStartRain(void);
GameObjectRuntime *ObjectMatchToRefIDByCatID(s16 id, s32 cat);
void SysSoundPlayPeriodicSong(s32 arg0, s32 song);

/* Written as separate tests so cc1 doesn't merge the bit checks into one mask. */
#define OBJECTIVE_DONE(n) (gObjectives != NULL && (gObjectives->completed & (1 << (n))))
#define OBJECTIVE_UNK20(n) (gObjectives != NULL ? (gObjectives->unk20 & (1 << (n))) : 0)

/* park calls ObjectBombDeath with only three arguments ($a3 is left unset). */
#define ObjectBombDeath3 ((void (*)(s32, s32, s32))ObjectBombDeath)

static void func_801413B8(void);
static void func_80141434(void);
static void func_801417C4(void);
static void func_801420B8(void);

/* Rebuild the "N bombs remaining" objective text. */
static void func_801413B8(void) {
    StringObjectiveObjectsRemaining2(D_801430A4, D_80142880, gRemainingMsg,
                                     D_8014310C == D_80143108 - 1 ? D_80142888 : D_80142884, D_8014310C,
                                     D_80143108);
}

/* Once all objectives are done, point the player at the exit. */
static void func_80141434(void) {
    int i;

    if (!gMissionFailed && OBJECTIVE_DONE(0) && OBJECTIVE_DONE(1) && OBJECTIVE_DONE(3)) {
        for (i = 0; i < gLevelGameObjCount; i++) {
            if (i != GOBJ_ID_666 && gameObjectsModelInfo[gameObjectsInfo[i].modelIndex].type == PARK_TYPE_EXIT) {
                LevelSetDirectionPointerFlag(1);
                LevelSetPointer(i);
                return;
            }
        }
    }
}

/* Countdown expired: every visible armed bomb goes off. */
static void func_8014152C(void) {
    EGSListNode *node;
    int id;
    GameObjectRuntime *rt;

    if (gObjectives == NULL || !(gObjectives->unk20 & 2)) {
        for (node = ((ParkCamera *)*gCamera)->objects; node != NULL; node = node->next) {
            id = ((ParkDisplayObj *)node->data)->id;
            if ((id & 0x2000) && (id &= 0x3FF) != GOBJ_ID_666) {
                if (gameObjectsModelInfo[gameObjectsInfo[id].modelIndex].type == GOBJ_TYPE_BOMB) {
                    rt = gameObjectsInfo[id].runtime;
                    if (!(rt->flags1 & 0x80) && rt->healthInfo->health > 0) {
                        ObjectDetonateBomb(rt);
                    }
                }
            }
        }
        ObjectBombDeath3(2, 0x54, 0);
    }
}

static void func_8014165C(s16 id, s16 by) {
    if (by == gPlayerOffset) {
        if (id != GOBJ_ID_666 && gameObjectsModelInfo[gameObjectsInfo[id].modelIndex].type == PARK_TYPE_MEI_XING) {
            if (!D_80143114 && (gObjectives == NULL || !(gObjectives->completed & 4))) {
                D_80143114 = 1;
                SysPlayXA(0x5E, 0, 1);
            }
        } else {
            NPCCopDamagedCallback(id, by);
            ObjectBombDamagedCallback(id, by);
        }
    }
}

static void func_80141788(void) {
    D_80143104->flags23 |= 8;
    LevelObjectiveComplete(1);
    func_80141434();
}

/* Hostage rescued. */
static void func_801417C4(void) {
    NPCCombatModeCallback(NULL);
    gHitNPCCallback = NULL;
    NPCInitHostageTaker(D_80143104->id);
    if (!D_80143118 && (gObjectives == NULL || !(gObjectives->completed & 2))) {
        SysSoundQueueXA(-1, 0x56);
        GameSysSetLetterBoxMode(1);
        JoystickDeactivate();
        CASRequestAction(gPlayerOffset, 0x5B);
        func_80141788();
    }
}

static void func_80141870(s16 id, s16 by) {
    int type;
    s32 ref;

    type = GOBJ_TYPE((s16)id);
    if (type == PARK_TYPE_HOSTAGE) {
        if (!gDeviceDetonated) {
            if ((s16)id == gLevelSpecificNpcID) {
                SysSoundQueueXA(-1, 0x62);
                StringContactMeiXingMsg();
                LevelObjectiveComplete(3);
                func_80141434();
            } else if ((s16)id == D_80143104->id) {
                func_801417C4();
            }
        }
    } else if (type == GOBJ_TYPE_COP) {
        if (!gDeviceDetonated) {
            ref = gameObjectsInfo[(s16)id].refId;
            if (ref != -1 && ref != GOBJ_ID_666 &&
                gameObjectsModelInfo[gameObjectsInfo[ref].modelIndex].type == GOBJ_TYPE_BOMB) {
                NPCCopKilledWashington(id, by, ref, 0, 0);
            } else if (!D_80143118) {
                D_80143118 = 1;
                LevelMissionFailed(1, 0x58, 1);
            }
        }
    } else if (type == GOBJ_TYPE_BOMB) {
        if (!OBJECTIVE_UNK20(1) && !OBJECTIVE_UNK20(2) &&
            !(gameObjectsInfo[(s16)id].runtime->flags1 & 0x80)) {
            InterfaceCountdownReset(0);
            ObjectBombDeath3(1, 0x82, 0);
        }
    } else if (type == PARK_TYPE_MEI_XING) {
        if (gObjectives == NULL || (!(gObjectives->completed & 4) && !(gObjectives->unk20 & 8))) {
            SysSoundQueueXA(-1, 0x5F);
            StringContactMeiXingMsg();
            LevelMissionFailed(3, -1, 0);
        }
    }
}

static s32 func_80141B58(void) {
    if (!gMissionFailed && OBJECTIVE_DONE(0) && OBJECTIVE_DONE(1) && OBJECTIVE_DONE(3)) {
        SysPlayXA(0x63, 0, 1);
        return 1;
    }
    if (gMissionFailed) {
        StringMissionFailedMsg();
    } else {
        SysPlayXA(0x64, 0, 1);
        LevelObjectivesIncomplete(gObjectivesIncompleteMsg);
    }
    return 0;
}

static void func_80141C0C(s16 id) {
    if (!D_80143118 && id == D_80143104->id) {
        NPCCombatModeCallback(NULL);
        NPCShootHostage(D_80143104->id);
    }
}

/* Bomb defused. */
static void func_80141C6C(s16 id, s16 by) {
    GameObjectRuntime *cop;
    s16 linked;
    u8 defused;

    cop = ObjectMatchToRefID(id, GOBJ_TYPE_COP);
    if (by == gPlayerOffset) {
        linked = cop->id;
        if (linked != id && linked != GOBJ_ID_666 &&
            gameObjectsModelInfo[gameObjectsInfo[linked].modelIndex].type == GOBJ_TYPE_COP) {
            SysPlayXA(0x59, 0, 1);
            NPCHazmatStart(cop->id);
            return;
        }
    }
    defused = ++D_8014310C;
    if (gObjectives == NULL || !(gObjectives->unk20 & 4)) {
        if (defused < D_80143108) {
            StringInteractObjectsRemaining(D_8014288C, D_80143108 - defused);
            func_801413B8();
            SysPlayXA(0x5A, 0, 1);
            if (!gMissionFailed) {
                LevelMissionCtrMsg(gCheckpointMsg, 0);
                SaveCreateContinue();
            }
        } else {
            InterfaceCountdownAbort();
            func_801413B8();
            SysPlayXA(0x6B, 0, 1);
            LevelObjectiveComplete(0);
            func_80141434();
        }
    }
}

static s32 func_80141E30(void) {
    if (gObjectives == NULL || !(gObjectives->unk20 & 8)) {
        LevelObjectiveAdd(3, -1);
        gameObjectsInfo[gLevelSpecificNpcID].runtime->flags23 &= ~2;
        SysSoundQueueXA(-1, 0x60);
        GameSysSetLetterBoxMode(1);
        JoystickDeactivate();
        CASRequestAction(gPlayerOffset, 0x5B);
        LevelObjectiveComplete(2);
    } else {
        TextStatusMessage(D_80142890, 0);
    }
    return 1;
}

static s32 func_80141EFC(s16 id) {
    s32 xa;

    xa = *(s32 *)((u8 *)&gameObjectsInfo[id] + 0x28);
    if (xa >= 100) {
        xa -= 100;
    }
    if (0x5D - xaLevelStartIndex[gSysInfo.level] == xa && gObjectives != NULL) {
        gObjectives->unk1C |= 4;
    }
    return 1;
}

void iMuseumInit(s16 id) {
    s16 ref;
    s16 type;

    ref = gameObjectsInfo[id].refId;
    if (ref != -1) {
        type = GOBJ_TYPE(ref);
        if (type == PARK_TYPE_MEI_XING) {
            LevelObjectiveAdd(2, id);
            if (gObjectives != NULL) {
                gObjectives->unk24 |= 8;
                gObjectives->unk1C &= ~4;
            }
        }
    } else {
        LevelObjectiveAdd(1, id);
    }
}

static void *func_80142094(s16 id, s16 kind) {
    if (kind == 1) {
        return gCommArrayMsg;
    }
    return NULL;
}

static void func_801420B8(void) {
    SysPlaySong(0, 0x16, 0, 0);
    SysSoundPlayPeriodicSong(0, 0x17);
}

#ifndef _PSYQ
/* iParkInit's asm names the objectives' text list pointer by its own label */
__asm__(".set D_8014289C, parkObjectives+4\n");
#endif

/*
 * The original keeps `move v1,v0` after ObjectMatchToRefIDByCatID: something stopped combine from
 * folding the copy into the healthInfo load. A named label (deleted by jump.c), asm_input(""),
 * and duplicated call arms (45 lines: loop.c strength-reduces a third i<<16) don't reproduce it;
 * a volatile insn between the copy and the load does (can_combine_p). The asm below is FAKE.
 */
void iParkInit(s32 mode) {
    int i;
    int type;
    int refType;
    s32 ref;
    GameObjectRuntime *npc; /* hostage, or the cop holding one */
    GameObjectRuntime *rt;
    GameObjectRuntime *cop;

    EGSDisplayRegisterFogScrim(&D_8014308C);
    D_80143104 = NULL;
    D_80143108 = 0;
    D_8014310C = 0;
    D_80143110 = 0;
    D_80143114 = 0;
    D_80143118 = 0;
    InterfaceViralRadarInit();
    ParticleStartRain();
    parkObjectives.primaryTexts[0] = D_801430A4;
    SaveObjectivesNTimersReinit(&parkObjectives, 0);
    if (gObjectives->unk18 == 0) {
        LevelObjectiveAdd(0, -1);
        LevelObjectiveAdd(4, -1);
        if (gObjectives != NULL) {
            gObjectives->unk24 |= 7;
        }
        InterfaceStartCountdownAfterGameCam(1200, 0, func_8014152C);
    } else if (gObjectives == NULL || !(gObjectives->completed & 1)) {
        InterfaceStartCountdownAfterGameCam(InterfaceCountdownGet() / 20, 0, func_8014152C);
    }
    if (mode == 3) {
        func_801420B8();
    } else {
        gSysMovieDoneCallback = func_801420B8;
    }
    DamageSetDamagedCallback(func_8014165C);
    DamageSetDestroyedCallback((void (*)(s16, s16))func_80141870);
    LevelSetEOLCallback(func_80141B58);
    if (gObjectives == NULL || !(gObjectives->completed & 2)) {
        NPCCombatModeCallback(func_80141C0C);
    }
    UseSetBombCallback(func_80141C6C);
    UseSetComputerCallback(func_80141E30);
    ObjectSetQuipObjectCallback(func_80141EFC);
    ObjectSetSpecialObjectCallback(iMuseumInit);
    SetUseStringCallback(func_80142094);

    for (i = 0; i < gLevelGameObjCount; i++) {
        type = GOBJ_TYPE(i);
        if (type == PARK_TYPE_HOSTAGE) {
            ref = gameObjectsInfo[i].refId;
            if (ref != -1 && GOBJ_TYPE(ref) == GOBJ_TYPE_COP) {
                npc = gameObjectsInfo[i].runtime;
                D_80143104 = npc;
                if (gObjectives == NULL || !(gObjectives->completed & 2)) {
                    NPCInitHostageTaker(npc->id);
                } else {
                    do {
                        do { refType = GOBJ_TYPE(ref); } while (0);
                        if (refType == GOBJ_TYPE_COP) {
                            gameObjectsInfo[ref].runtime->flags23 |= 2;
                        }
                        ref = gameObjectsInfo[ref].refId;
                    } while (ref != -1);
                }
            }
        } else if (type == GOBJ_TYPE_COP) {
            ref = gameObjectsInfo[i].refId;
            if (ref == -1 || GOBJ_TYPE(ref) == GOBJ_TYPE_COP) {
                npc = ObjectMatchToRefIDByCatID(i, 2);
                __asm__ volatile("" : : : "memory"); /* FAKE: keeps the result copy (combine stops at a volatile insn) */
                if (npc->healthInfo->health <= 0) {
                    D_80143118 = 1;
                }
            }
        } else if (type == GOBJ_TYPE_BOMB) {
            rt = gameObjectsInfo[i].runtime;
            D_80143108++;
            if (rt->flags1 & 0x80) {
                D_8014310C++;
            }
            cop = ObjectMatchToRefID(i, GOBJ_TYPE_COP);
            refType = GOBJ_TYPE(cop->id);
            if (cop->id != i && refType == GOBJ_TYPE_COP && (*(u32 *)rt & 0x8020) != 0x20) {
                cop->flags23 |= 2;
            }
            if (cop->healthInfo->health <= 0) {
                gLevelInnocentsKilled++;
            }
        }
    }
    D_80143108--;
    func_801413B8();
    if (gLevelSpecificNpcID != -1) {
        if (gameObjectsInfo[gLevelSpecificNpcID].runtime->unk20[2] == 2) {
            gLevelSpecificNpcName = D_80142894;
            gameObjectsInfo[gLevelSpecificNpcID].runtime->npc->nameMsgId = 0xB;
            if (gObjectives == NULL || !(gObjectives->completed & 4)) {
                gameObjectsInfo[gLevelSpecificNpcID].runtime->flags23 |= 2;
            }
        }
    }
    gDefaultLevelDmgFX = 0xB;
}

