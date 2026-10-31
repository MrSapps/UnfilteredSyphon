#include "common.h"

/*
 * strings.c's data. cc1 outputs a pointer's string literal while it reads the
 * initialiser, so each short literal lands in .sdata just before its pointer and each
 * long one in .rdata, in definition order; a table's literals come out in reverse
 * element order. Identical literals are shared ("%s%s %s", "Shotgun").
 */
static const s32 D_80012874[4] = { 1, 1, 1, 0 };

static char D_8010FAB8[1] = "";
static char *D_8010FAC4 = "%s%s %s";       /* take object */
static char *D_8010FAC8 = "%s%s%s %s";     /* take ammo */
static char *D_8010FACC = "%s%ss maxed";   /* ammo maxed */
static char *D_8010FAD0 = "%s undamaged";  /* undamaged flak jacket */
static char *D_8010FADC = "%s%s";
static char *D_8010FAE8 = "taken";
static char *D_8010FAF4 = " bullet";
static char *D_8010FB00 = " shell";
static char *D_8010FB0C = "%s %s";
static char *D_8010FB10 = "%s%s %s";
static char *D_8010FB14 = "%s %s %s";
static char *D_8010FB20 = "Mission";
static char *D_8010FB24 = "Objective";
static char *D_8010FB28 = "Parameter";
static char *D_8010FB34 = "Failed";
static char *D_8010FB38 = "Complete";
static char *D_8010FB3C = "Markinson";
static char *D_8010FB40 = "Press %s to Contact %s";
static char *D_8010FB44 = "Eliminate %s";

char *gLevelPrettyNames[20] = {
    "Georgia Street Subway District",
    "Subway District: Disaster Area",
    "Subway District: Tunnel Chase",
    "Washington Park",
    "Freedom Memorial",
    "Pharcom Exposition Center: The Hunt",
    "Pharcom Exposition Center: The Search",
    "Rozovka Terrorist Base",
    "Terrorist Base: Bunker",
    "Terrorist Base: Radar-Comm Tower",
    "Terrorist Base: Escape",
    "Uzhorod Secret Prison: Upper Level",
    "Uzhorod Secret Prison: Lower Level",
    "Uzhorod Catacombs",
    "Kazakhstan Warehouse District",
    "Warehouse 76",
    "Kazakhstan Mining Tunnels",
    "Silo Access Tunnels",
    "Soviet Missile Silo",
    "TRANSTUN",
};

char *gUseMsgs[28] = {
    "Bomb",        "Computer",    "Elevator",      "*",          "Switch",            "Silenced 9mm",
    "9mm",         ".45",         "Shotgun",       "Shotgun",    "PK-102",            "M-16",
    "HK-5",        "G-18",        "BIZ-2",         "K3G4",       "Nightvision rifle", "Sniper rifle",
    "Grenade",     "Gas Grenade", "M-79",          "Taser",      "Flashlight",        "Viral Scanner",
    "Flak Jacket", "Keycard",     "C4 Explosives", "Antigen",
};

char *gObjectiveInstructionsMsg = "Press START to see objectives";
char *gHealthMsg = "health";
char *gArmorMsg = "armor";
char *gTargetMsg = "target";
char *gDangerMsg = "danger";
char *gCountdownFmt = "00:00:00";
char *gTargetLockMsg = "Target Lock";
char *gHeadShotMsg = "Head Shot";
char *gHeadMsg = "head";
char *gWeaponMsg = "weapon";
char *gHandMsg = "hand";
char *gBodyMsg = "body";
char *gLimbMsg = "limb";
char *gTargetFmt = "tgt: %s";
char *gZoomFmt = "zoom %dx";
char *gGameOverMsg = "Game Over";
char *gObjectiveAddedMsg = "Objective Added";
char *gCheckpointMsg = "Checkpoint";
char *gObjectivesIncompleteMsg = "Objectives Incomplete";
char *gLevelAlertedMsg = "Level alerted";
char *gLockedMsg = "Locked";
char *gElectronicallyLockedMsg = "Electronically Locked";
char *gLockCodeRequiredMsg = "Lock Code Required";
char *gElevatorCallSwitchMsg = "Elevator Call Switch";
char *gPowerSwitchMsg = "Power Switch";
char *gCommArrayMsg = "Communications array";
char *gNoTgtMsg = "No Target Available";
char *gRemainingMsg = " remaining";
char *gBodyArmorHitMsg = "Body Armor";
char *gCBCDAgentMsg = "CBDC Agent";
char *gMeiXingMsg = "Lian Xing";
char *gPhaganMsg = "Phagan";
char *gAramovMsg = "Aramov";
char *gRhoemerMsg = "Rhoemer";
char *gLoadingStringFmt = "Loading:\n%s";
char *gLoadedStringFmt = "%s loaded!\nPress %%x to continue";
s32 gStringNumFillers = 4;
char *gStringFillerText[4] = { "Status: ok", "diagnostic: ok", "power: ok", "monitoring..." };
char *gStringSniperSpareText[4] = { "SuperScope", "Dest LMF", "Castor OK", "Mag On" };
char *gStringSnifferSpareText[3] = { "Viral detector", "v 1.0c", "Init OK" };
char *gStringSnifferLock[4] = { "Scanning...", "Virus detected", "analysis complete", "notify hq" };
u16 gContactMeiXingMsgH = 0xFFFF;
static char D_8010FCE4[2] = "s";
static char D_8010FCE8[2] = " ";

/* .bss */
static char D_80121918[0x40]; /* contact message buffer */
static char D_80121958[0x20]; /* weapon name buffer */

/* Weapon table entry (0x20 bytes) */
typedef struct WeaponDesc {
    /* 0x00 */ u8 unk00[0x1E];
    /* 0x1E */ u8 count;
    /* 0x1F */ u8 unk1F;
} WeaponDesc;

typedef struct SubObjectiveInfo {
    /* 0x00 */ u16 handle; /* text string, 0xFFFF when free */
    /* 0x04 */ char *fmt;
} SubObjectiveInfo;

typedef struct SysInfo {
    /* 0x00 */ u16 level;
} SysInfo;

extern WeaponDesc D_80105F58[];
extern char *gButtonStrings[];
extern s32 gJoyMap[];
extern SysInfo gSysInfo;
extern char gEliminateRhoemerMsg[];
extern SubObjectiveInfo mStringSubObjectiveInfo[2];

int sprintf(char *, const char *, ...);
s32 TextIsStringShowing(u16 handle);
void TextRemoveString(u16 handle);
u16 TextStatusMessage(char *msg, s32 ticks);
void LevelObjectivesIncomplete(char *msg);
void LevelRemoveContactMeiXingMsgForAddingObjectiveMsg(void);
void EGSDisplayQueueInterrupt(void (*fn)(void), s32 arg1, s32 arg2);

char *StringSetUseMsg(s16 id) {
    char *msg = gUseMsgs[id];

    if (id == 0x12) {
        sprintf(D_80121958, D_8010FADC, msg, D_80105F58[0x12].count != 1 ? D_8010FCE4 : D_8010FAB8);
        msg = D_80121958;
    } else if (id == 0x13) {
        sprintf(D_80121958, D_8010FADC, msg, D_80105F58[0x13].count != 1 ? D_8010FCE4 : D_8010FAB8);
        msg = D_80121958;
    }
    return msg;
}

static char *func_80089D00(s16 weapon, u8 arg1, u8 arg2) {
    sprintf(D_80121958, D_8010FADC, D_8010FCE8, gUseMsgs[0x12]);
    return arg1 ? (arg2 ? (weapon == 0x14 ? D_80121958 : D_8010FAF4) : D_8010FB00) : D_8010FAB8;
}

void StringTakeAmmoMsg(char *buf, s16 count, s16 weapon, u8 arg3, u8 arg4) {
    char *s = func_80089D00(weapon, arg3, arg4);

    sprintf(buf, D_8010FAC8, gUseMsgs[weapon], s, count != 1 ? D_8010FCE4 : D_8010FAB8, D_8010FAE8);
}

void StringMaxedAmmoMsg(char *buf, s16 weapon, u8 arg2, u8 arg3) {
    sprintf(buf, D_8010FACC, gUseMsgs[weapon], func_80089D00(weapon, arg2, arg3));
}

void StringTakeObjectMsg(char *buf, s32 count, s16 id) {
    sprintf(buf, D_8010FAC4, gUseMsgs[id],
            (id == 0x12 || id == 0x13) ? (count != 1 ? D_8010FCE4 : D_8010FAB8) : D_8010FAB8, D_8010FAE8);
}

void StringUndamagedFlakJacket(char *buf) {
    sprintf(buf, D_8010FAD0, gUseMsgs[0x18]);
}

void StringObjectiveMsg(char *buf, s16 objective, u8 state, u8 complete) {
    if (state && complete) {
        sprintf(buf, D_8010FB0C, D_8010FB24, D_8010FB38);
    } else {
        sprintf(buf, D_8010FB14, D_8010FB20, state ? D_8010FB24 : D_8010FB28, complete ? D_8010FB38 : D_8010FB34);
    }
}

void StringMissionCompletedMsg(char *buf) {
    sprintf(buf, D_8010FB0C, D_8010FB20, D_8010FB38);
}

void StringMissionFailedMsg(void) {
    char buf[0x20];

    sprintf(buf, D_8010FB10, D_8010FB20, D_8010FAB8, D_8010FB34);
    LevelObjectivesIncomplete(buf);
}

void StringInteractObjectsRemaining(char *fmt, s32 left) {
    char buf[0x80];
    s32 i;
    s32 free = 2;

    sprintf(buf, fmt, left, left != 1 ? D_8010FCE4 : D_8010FAB8);
    for (i = 0; i < 2; i++) {
        if (!TextIsStringShowing(mStringSubObjectiveInfo[i].handle)) {
            free = i;
        } else if (mStringSubObjectiveInfo[i].fmt == fmt) {
            TextRemoveString(mStringSubObjectiveInfo[i].handle);
        }
    }
    if (free != 2) {
        mStringSubObjectiveInfo[free].fmt = fmt;
        mStringSubObjectiveInfo[free].handle = TextStatusMessage(buf, 0);
    }
}

void StringObjectiveObjectsRemaining2(char *buf, char *fmt, char *what, char *arg3, s32 done, s32 total) {
    s32 left = total;

    if (done != total) {
        left = total - done;
    }
    sprintf(buf, fmt, left, left == total ? D_8010FAB8 : what, left != 1 ? D_8010FCE4 : D_8010FAB8, arg3);
}

void StringInitMessages(void) {
    s32 i;

    for (i = 0; i < 2; i++) {
        mStringSubObjectiveInfo[i].handle = 0xFFFF;
        mStringSubObjectiveInfo[i].fmt = NULL;
    }
    sprintf(gEliminateRhoemerMsg, D_8010FB44, gRhoemerMsg);
}

#ifdef NON_MATCHING
/* Matches except that the loop's constant 1 and i = 0 are emitted in swapped order. */
u16 StringContactMeiXingMsg(void) {
    u32 i;
    char *button;

    for (i = 0; i < 9; i++) {
        if (gJoyMap[i] == 1) {
            button = gButtonStrings[i];
            goto found;
        }
    }
    button = NULL;
found:
    sprintf(D_80121918, D_8010FB40, button,
            (gSysInfo.level != 10 && gSysInfo.level != 11 && gSysInfo.level != 12 && gSysInfo.level != 13)
                ? gMeiXingMsg
                : D_8010FB3C);
    gContactMeiXingMsgH = TextStatusMessage(D_80121918, 100);
    EGSDisplayQueueInterrupt(LevelRemoveContactMeiXingMsgForAddingObjectiveMsg, 99, 0xE0C00000);
    return gContactMeiXingMsgH;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/strings", StringContactMeiXingMsg);
#endif

void StringReplaceContactMeiXingMsg(char *msg) {
    if (gContactMeiXingMsgH != 0xFFFF) {
        TextRemoveString(gContactMeiXingMsgH);
        if (msg != NULL) {
            TextStatusMessage(msg, 0);
        }
        gContactMeiXingMsgH = 0xFFFF;
    }
}

