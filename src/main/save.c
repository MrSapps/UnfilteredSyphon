#include "common.h"
#include "game.h"
#include "gameobj.h"

/* save.c's small data, still assembled from asm */
SDATA_LOCAL(gContinueLevel)

extern u8 gContinueLevel; /* nonzero once a continue point has been stored */

/* Per-weapon inventory entry; slot 0 is not written to the memory card */
typedef struct WeaponSlot {
    /* 0x00 */ u8 data[4];
} WeaponSlot;

#define WEAPON_SLOTS 26

/* Memory card save block (gSaveData) */
typedef struct SaveData {
    /* 0x00 */ u16 health;
    /* 0x02 */ u16 armor;
    /* 0x04 */ u8 level : 8;
    /* 0x05 */ u8 weapon : 8;
    /* 0x06 */ u8 radar : 1;
    u8 flightControls : 1;
    u8 highestLevel : 5;
    /* 0x07 */ u8 ambient;
    /* 0x08 */ u8 volume[3];
    /* 0x0B */ u8 joyMap[8];
    /* 0x13 */ u8 unk13;
    /* 0x14 */ u32 weaponsAvailable;
    /* 0x18 */ WeaponSlot weapons[WEAPON_SLOTS - 1];
    /* 0x7C */ u16 position;
    /* 0x7E */ u16 position2;
    /* 0x80 */ u8 unk80[0x78];
    /* 0xF8 */ u32 xorSum;
    /* 0xFC */ u32 sum;
} SaveData; /* size 0x100 */

/* Per-object state stored with a continue point */
typedef struct ContinueObject {
    /* 0x00 */ s16 id;
    /* 0x02 */ s16 data0;
    /* 0x04 */ s16 data1;
    /* 0x06 */ s16 pad;
} ContinueObject;

#define CONTINUE_OBJECTS_MAX 0x82

/* Continue point (gContinueData) */
typedef struct ContinueData {
    /* 0x000 */ u8 sector;
    /* 0x002 */ s16 objCount;
    /* 0x004 */ s16 savedObjCount;
    /* 0x006 */ s16 generatorCount;
    /* 0x008 */ s32 weapon;
    /* 0x00C */ u32 weaponsAvailable;
    /* 0x010 */ WeaponSlot weapons[WEAPON_SLOTS];
    /* 0x078 */ u32 objectives[6];
    /* 0x090 */ s32 countdown;
    /* 0x094 */ s32 internalTimer;
    /* 0x098 */ s32 exeTick;
    /* 0x09C */ s32 orient[3];
    /* 0x0A8 */ s32 unkA8;
    /* 0x0AC */ s32 position[4];
    /* 0x0BC */ u16 health;
    /* 0x0BE */ u16 armor;
    /* 0x0C0 */ u16 generators[100];
    /* 0x188 */ u32 objAlive[20];
    /* 0x1D8 */ ContinueObject objects[CONTINUE_OBJECTS_MAX];
    /* 0x5E8 */ u8 weaponInfo[0xF0];
    /* 0x6D8 */ s32 levelData[1];
} ContinueData;

/* Player state at the start of the level */
typedef struct PlayerSave {
    /* 0x00 */ s32 weapon;
    /* 0x04 */ u32 weaponsAvailable;
    /* 0x08 */ WeaponSlot weapons[WEAPON_SLOTS];
    /* 0x70 */ u16 health;
    /* 0x72 */ u16 armor;
} PlayerSave;

typedef struct SaveObjectives {
    /* 0x00 */ u8 unk00[0x10];
    /* 0x10 */ u32 bits[6];
} SaveObjectives;

/* Weapon table entry (0x20 bytes) */
typedef struct WeaponDesc {
    /* 0x00 */ u32 unk0 : 3;
    u32 cas : 3;
    /* 0x04 */ u8 unk04[0x1C];
} WeaponDesc;

/* gameObjectsInfo[] entry seen through its flags word at 0x24 */
typedef struct GameObjectWeaponInfo {
    /* 0x00 */ u8 unk00[0x24];
    /* 0x24 */ u32 weapon : 8;
    u32 unk24 : 4;
    u32 kind : 2;
} GameObjectWeaponInfo;

#define GOBJ_WEAPON_INFO(id) ((GameObjectWeaponInfo *)&gameObjectsInfo[id])

typedef struct SysInfo {
    /* 0x00 */ s16 level;
} SysInfo;

extern SaveData D_80121A08;     /* gSaveData */
extern ContinueData D_80121B08; /* gContinueData */
extern PlayerSave mSOLState;
extern WeaponSlot gWeaponState[WEAPON_SLOTS];
extern WeaponDesc D_80105F58[];
extern SysInfo gSysInfo;
extern SaveObjectives *gObjectives;
extern GameObjectRuntime *gObjPlayer;
extern u32 gWeaponsAvailable[];
extern s32 gCurrentWeapon;
extern s32 gPlayerOffset;
extern s32 gJoyMap[8];
extern s32 gAmbientLight;
extern u16 POSITION;
extern u16 D_8012C7FA;

void InterfaceCountdownAbort(void);
void InterfaceCountdownReset(s32 countdown);
void EGSDisplayQueueInterrupt(void (*fn)(void), s32 arg1, s32 arg2);
void WeaponSelect(GameObjectRuntime *obj, s32 weapon, s32 arg2);
void WeaponReplace(GameObjectRuntime *obj);
void PlayerCASInit(GameObjectRuntime *obj, s32 cas);
void InterfaceWeaponChange(void);
u8 JoystickGetFlightControls(void);
void JoystickSetFlightControls(s32 on);
u8 InterfaceRadarIsOn(void);
s32 SysSoundGetMasterVolume(s32 channel);
void SysSoundSetMasterVolume(s32 channel, s32 vol);
void SysChangeLevel(u32 level);

void SaveObjectivesNTimersClear(void) {
    D_80121B08.objectives[0] = 0;
    D_80121B08.objectives[1] = 0;
    D_80121B08.objectives[2] = 0;
    D_80121B08.objectives[3] = 0;
    D_80121B08.objectives[4] = 0;
    D_80121B08.objectives[5] = 0;
    D_80121B08.countdown = 0x7FFFFFFF;
    D_80121B08.internalTimer = 0;
}

void SaveObjectivesNTimersReinit(SaveObjectives *objectives, s32 *timer) {
    gObjectives = objectives;
    if (objectives != NULL) {
        objectives->bits[0] = D_80121B08.objectives[0];
        objectives->bits[1] = D_80121B08.objectives[1];
        objectives->bits[2] = D_80121B08.objectives[2];
        objectives->bits[3] = D_80121B08.objectives[3];
        objectives->bits[4] = D_80121B08.objectives[4];
        objectives->bits[5] = D_80121B08.objectives[5];
    }
    if (D_80121B08.countdown == 0) {
        InterfaceCountdownAbort();
    } else if (D_80121B08.countdown != 0x7FFFFFFF) {
        InterfaceCountdownReset(D_80121B08.countdown);
    }
    if (timer != NULL) {
        *timer = D_80121B08.internalTimer;
    }
}

void SaveInternalTimer(s32 timer) {
    D_80121B08.internalTimer = timer;
}

void SaveReturnToSOL(void) {
    s32 i;

    D_80121B08.health = mSOLState.health;
    D_80121B08.armor = mSOLState.armor;
    D_80121B08.weapon = mSOLState.weapon;
    D_80121B08.weaponsAvailable = mSOLState.weaponsAvailable;
    for (i = 0; i < WEAPON_SLOTS; i++) {
        D_80121B08.weapons[i] = mSOLState.weapons[i];
    }
}

s32 SavePlayerPreserve(void) {
    s32 i;

    if (D_80121A08.level != gSysInfo.level) {
        D_80121A08.level = gSysInfo.level;
        if (D_80121A08.highestLevel < D_80121A08.level) {
            D_80121A08.highestLevel = D_80121A08.level;
        }
        mSOLState.health = gObjPlayer->healthInfo->health;
        mSOLState.armor = gObjPlayer->healthInfo->armor;
        mSOLState.weapon = gCurrentWeapon;
        mSOLState.weaponsAvailable = gWeaponsAvailable[0];
        for (i = 0; i < WEAPON_SLOTS; i++) {
            mSOLState.weapons[i] = gWeaponState[i];
        }
    }
    D_80121B08.health = gObjPlayer->healthInfo->health;
    D_80121B08.armor = gObjPlayer->healthInfo->armor;
    D_80121B08.weapon = gCurrentWeapon;
    D_80121B08.weaponsAvailable = gWeaponsAvailable[0];
    for (i = 0; i < WEAPON_SLOTS; i++) {
        D_80121B08.weapons[i] = gWeaponState[i];
    }
    return 1;
}

s32 SavePlayerRestore(void) {
    s32 i;

    gObjPlayer->healthInfo->health = D_80121B08.health;
    gObjPlayer->healthInfo->armor = D_80121B08.armor;
    gWeaponsAvailable[0] = D_80121B08.weaponsAvailable;
    for (i = 0; i < WEAPON_SLOTS; i++) {
        gWeaponState[i] = D_80121B08.weapons[i];
    }
    WeaponSelect(gObjPlayer, D_80121B08.weapon, 1);
    WeaponReplace(gObjPlayer);
    /* the player's CAS comes from the weapon of the object it is linked to */
    PlayerCASInit(gObjPlayer,
                  D_80105F58[gObjPlayer->id != gPlayerOffset
                                 ? (GOBJ_WEAPON_INFO(gObjPlayer->id)->weapon
                                        ?: (GOBJ_WEAPON_INFO(gObjPlayer->id)->kind == 1   ? 0x13
                                            : GOBJ_WEAPON_INFO(gObjPlayer->id)->kind == 2 ? 0x14
                                                                                                 : 0))
                                 : gCurrentWeapon]
                      .cas);
    InterfaceWeaponChange();
    return 1;
}

void func_80090724(void); /* still asm */

s32 SaveCreateContinue(void) {
    EGSDisplayQueueInterrupt(func_80090724, 2, 0xE0C00000);
    return 1;
}

INCLUDE_ASM("asm/main/nonmatchings/save", func_80090724);

INCLUDE_ASM("asm/main/nonmatchings/save", SaveRestoreContinue);

void SavePrepareMemoryCardSave(void) {
    u32 xorSum = 0;
    u32 sum = 0;
    s32 i;
    u32 *p;

    for (i = 0; i < 8; i++) {
        D_80121A08.joyMap[i] = gJoyMap[i];
    }
    D_80121A08.weaponsAvailable = D_80121B08.weaponsAvailable;
    for (i = 0; i < WEAPON_SLOTS - 1; i++) {
        D_80121A08.weapons[i] = D_80121B08.weapons[i + 1];
    }
    D_80121A08.health = D_80121B08.health;
    D_80121A08.armor = D_80121B08.armor;
    D_80121A08.weapon = D_80121B08.weapon;
    D_80121A08.flightControls = JoystickGetFlightControls() != 0;
    D_80121A08.radar = InterfaceRadarIsOn() != 0;
    D_80121A08.ambient = gAmbientLight;
    D_80121A08.volume[0] = SysSoundGetMasterVolume(0);
    D_80121A08.volume[1] = SysSoundGetMasterVolume(1);
    D_80121A08.volume[2] = SysSoundGetMasterVolume(2);
    D_80121A08.level = gSysInfo.level;
    if (D_80121A08.highestLevel < D_80121A08.level) {
        D_80121A08.highestLevel = D_80121A08.level;
    }
    D_80121A08.position = POSITION;
    D_80121A08.position2 = D_8012C7FA;
    for (p = (u32 *)&D_80121A08; p < &D_80121A08.xorSum; p++) {
        sum += *p;
        xorSum ^= *p;
    }
    D_80121A08.xorSum = xorSum;
    D_80121A08.sum = sum;
}

void SaveGetDataBuffer(SaveData **buf, s32 *size) {
    *buf = &D_80121A08;
    *size = sizeof(SaveData);
}

s32 SaveMemoryCardLoaded(void) {
    u32 sum = 0;
    u32 xorSum = 0;
    s32 i;
    u32 *p;

    for (p = (u32 *)&D_80121A08; p < &D_80121A08.xorSum; p++) {
        sum += *p;
        xorSum ^= *p;
    }
    if (D_80121A08.xorSum != xorSum) {
        return 0;
    }
    if (D_80121A08.sum != sum) {
        return 0;
    }
    D_80121B08.weapon = D_80121A08.weapon;
    D_80121B08.weaponsAvailable = D_80121A08.weaponsAvailable;
    for (i = 0; i < WEAPON_SLOTS - 1; i++) {
        D_80121B08.weapons[i + 1] = D_80121A08.weapons[i];
    }
    D_80121B08.health = D_80121A08.health;
    D_80121B08.armor = D_80121A08.armor;
    for (i = 0; i < 8; i++) {
        gJoyMap[i] = D_80121A08.joyMap[i];
    }
    JoystickSetFlightControls(D_80121A08.flightControls);
    gAmbientLight = (D_80121A08.ambient << 16) + (D_80121A08.ambient << 8) + D_80121A08.ambient;
    SysSoundSetMasterVolume(0, D_80121A08.volume[0]);
    SysSoundSetMasterVolume(1, D_80121A08.volume[1]);
    SysSoundSetMasterVolume(2, D_80121A08.volume[2]);
    POSITION = D_80121A08.position;
    D_8012C7FA = D_80121A08.position2;
    SysChangeLevel(D_80121A08.level);
    return 1;
}

s32 SaveGetHighestLevel(void) {
    if (D_80121A08.highestLevel < gSysInfo.level) {
        D_80121A08.highestLevel = gSysInfo.level;
    }
    return D_80121A08.highestLevel;
}

s32 SaveIsPlayerVirginal(void) {
    return D_80121B08.weapon == 0;
}

INCLUDE_RODATA("asm/main/nonmatchings/save", D_80013344);

INCLUDE_RODATA("asm/main/nonmatchings/save", D_80013354);
