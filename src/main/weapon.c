#include "common.h"
#include "system.h"
#include "gameobj.h"
#include "gobj.h"
#include "game.h"
#include "egsmath.h"

/* weapon.c's data, defined in original order (docs/data_migration.md). */
static const s32 D_80011E8C[4] = { 1, 1, 1, 0 }; /* starts every game TU's .rdata */

typedef void (*WeaponCallback)();
typedef void (*WeaponFiredCallback)(s16 id, s16 weapon);

/* Master anim of a weapon model, with its dropped ("X") variant */
typedef struct WeaponMasterAnim {
    /* 0x00 */ u8 unk00[0x24];
    /* 0x24 */ void *dropModel;
} WeaponMasterAnim;

/* Per-weapon constant data (gWeaponInfo[], 0x20 bytes each). */
typedef struct WeaponInfo {
    /* 0x00 */ char *name;
    /* 0x04 */ WeaponMasterAnim *masterAnim; /* NULL: no model */
    /* 0x08 */ u8 unk08; /* nonzero: usable weapon */
    /* 0x09 */ u8 clipSize;
    /* 0x0A */ u8 unk0A;
    /* 0x0B */ u8 unk0B;
    /* 0x0C */ u32 clipShift : 3;
    /* 0x0C */ u32 unk0C_3 : 13;
    /* 0x0E */ s8 fireSong; /* SysPlaySong id, negative for none */
    /* 0x0F */ s8 npcFireSnd; /* SysPlayWpnSnd id; -2: use fireSong */
    /* 0x10 */ s16 npcAlertSnd;
    /* 0x12 */ s16 deathAction;
    /* 0x14 */ u32 unk14_0 : 6;
    /* 0x14 */ u32 damageNear : 10;
    /* 0x14 */ u32 damageFar : 10;
    /* 0x14 */ u32 fireDelay : 5;
    /* 0x14 */ u32 unk14_31 : 1;
    /* 0x18 */ u8 unk18[2];
    /* 0x1A */ s8 tipY; /* muzzle offset in the hand */
    /* 0x1B */ s8 tipZ;
    /* 0x1C */ u8 unk1C[4];
} WeaponInfo; /* size 0x20 */

/* Per-weapon ammo (gWeaponState[]). */
typedef struct WeaponState {
    /* 0x00 */ u16 reserve; /* rounds not in the clip */
    /* 0x02 */ u16 loaded;  /* rounds in the clip */
} WeaponState;

#define WEAPON_TASER 0xE

/* .sdata; the "" comes first, so it is a named array the table points at */
static char D_8010F8FC[1] = "";
static WeaponFiredCallback D_8010F900 = NULL; /* weapon fired callback */
static WeaponCallback D_8010F904 = NULL;      /* taser hit NPC callback */
/* .data (the long names go to .rdata, the short ones to .sdata) */
WeaponInfo gWeaponInfo[0x1A] = {
    { D_8010F8FC, NULL, 0, 0, 0, 0, 0, 0, -1, -1, 0, 0, 0xF, 0xF, 0, 0, 0, { 0, 0 }, 0, 0, { 0, 0, 0, 0 } },
    { "GLOKSIL.GMD", NULL, 1, 0xF, 0xF, 5, 5, 0xB01, 0, -2, 0x1E0, 0xF, 0xA, 0x32, 0x19, 4, 0, { 5, 0 }, -0x27, -9, { 8, 6, 4, 0 } },
    { "GLOCK17.GMD", NULL, 0, 0xF, 0xF, 0xA, 5, 0x301, 1, -2, 0x960, 0xF, 0xA, 0x32, 0x19, 0xC, 0, { 6, 0 }, -0x17, -9, { 0x50, 0x40, 0x30, 0 } },
    { D_8010F8FC, NULL, 0, 0, 0, 0, 0, 0, -1, -2, 0, 0, 0xF, 0xF, 0, 0, 0, { 0, 0 }, 0, 0, { 0x50, 0x40, 0x30, 0 } },
    { "COLT45.GMD", NULL, 2, 0xA, 0xA, 0xA, 5, 0x101, 2, -2, 0x960, 0xF, 5, 0x96, 0x4B, 8, 0, { 7, 0 }, -0x18, -8, { 0x50, 0x40, 0x30, 0 } },
    { "GLOCK18.GMD", NULL, 3, 0x21, 0x21, 0x21, 5, 0x241, 0x25, 0x1F, 0x960, 0xF, 5, 0x32, 0x19, 1, 0, { 0xD, 0 }, -0x17, -9, { 0x50, 0x40, 0x30, 0 } },
    { "BERELLI.GMD", NULL, 4, 0x1E, 5, 5, 0, 2, 0xC, -2, 0x960, 0x11, 0x1E, 0x12C, 0x96, 0x12, 0, { 9, 0 }, -0x4E, -0xF, { 0x50, 0x40, 0x30, 0 } },
    { "ITHICA37.GMD", NULL, 5, 0x19, 5, 5, 0, 0x42, 0xD, -2, 0x960, 0x11, 0x1E, 0xC8, 0x4B, 0xF, 0, { 8, 0 }, -0x4E, -0xB, { 0x50, 0x40, 0x30, 0 } },
    { "AK102.GMD", NULL, 6, 0x1E, 0x1E, 0xA, 5, 0x322, 0x2C, 0x26, 0x960, 0x10, 0, 0x32, 0x19, 2, 0, { 0xA, 0 }, -0x44, -8, { 0x50, 0x40, 0x30, 0 } },
    { "M16.GMD", NULL, 7, 0x1E, 0x1E, 0xF, 5, 0x322, 0x25, 0x1F, 0x960, 0x10, 0, 0x32, 0x19, 2, 0, { 0xB, 0 }, -0x53, -0xA, { 0x50, 0x40, 0x30, 0 } },
    { "BIZON2.GMD", NULL, 8, 0x42, 0x42, 0x42, 5, 0x241, 0x33, 0x2D, 0x960, 0x10, 0, 0x4B, 0x23, 2, 0, { 0xE, 0 }, -0x3B, -6, { 0x50, 0x40, 0x30, 0 } },
    { "MP5.GMD", NULL, 9, 0x20, 0x20, 8, 5, 0x341, 0x2C, 0x26, 0x960, 0x10, 0, 0x32, 0x19, 2, 0, { 0xC, 0 }, -0x24, -0x11, { 0x50, 0x40, 0x30, 0 } },
    { "DRAGSVD.GMD", NULL, 0xB, 0xA, 0xA, 0xA, 2, 0x902, 0, -2, 0x1E0, 0xF, 0x3C, 0xC8, 0x64, 0x14, 0, { 0x10, 0 }, -0x5A, -9, { 0x1A, 0x15, 0x10, 0 } },
    { "SUPERG.GMD", NULL, 0xC, 0xA, 0xA, 0xA, 2, 0x902, 0, -2, 0x1E0, 0xF, 0xF, 0x32, 0x19, 4, 0, { 0x11, 0 }, -0x43, -0xC, { 0x1A, 0x15, 0x10, 0 } },
    { "TASER.GMD", NULL, 0xD, 0, 0, 0, 0, 0x801, 0x11, -2, 0x960, 0x10, 0x23, 0x64, 0, 0, 0, { 0x15, 0 }, -0x2C, 0, { 0x50, 0x40, 0x30, 0 } },
    { "FLAMEGDF.GMD", NULL, 0, 0x5A, 0x5A, 0x5A, 5, 0x242, 0x12, -2, 0x960, 0x10, 0, 0x32, 0x32, 2, 0, { 0x15, 0 }, -0x5F, -0x11, { 0x50, 0x40, 0x30, 0 } },
    { "GRENLAUN.GMD", NULL, 0x10, 0xF, 5, 1, 0, 1, 0x1C, -2, 0x1E0, 0x11, 0, 0x258, 0x258, 0x14, 0, { 0x14, 0 }, 0, 0, { 0x50, 0x40, 0x30, 0 } },
    { "G3.GMD", NULL, 0xA, 0x14, 0x14, 0x14, 5, 0x322, 0x33, 0x2D, 0x960, 0x10, 0, 0x32, 0x19, 2, 0, { 0xF, 0 }, -0x53, -0xA, { 0x50, 0x40, 0x30, 0 } },
    { "FLASHLT.GMD", NULL, 0x13, 0, 0, 0, 0, 1, -1, -1, 0, 0, 0x28, 0, 0, 0, 0, { 0x17, 0 }, -0x22, 0, { 0, 0, 0, 0 } },
    { "GRENADE.GMD", NULL, 0xE, 0xA, 5, 1, 0, 4, 0xA, -2, 0x1E0, 0x11, 0, 0x258, 0, 0xF, 0, { 0x12, 0 }, 0, 0, { 0x50, 0x40, 0x30, 0 } },
    { "GRENADE.GMD", NULL, 0xF, 0xA, 5, 1, 0, 0x804, 0xA, -2, 0x1E0, 0x12, 0, 0x258, 0, 0xF, 0, { 0x13, 0 }, 0, 0, { 0x50, 0x40, 0x30, 0 } },
    { "FLASHLT.GMD", NULL, 0x12, 0, 0, 0, 0, 1, -1, -1, 0, 0, 0x28, 0, 0, 0, 0, { 0x16, 0 }, 0x22, 0, { 0, 0, 0, 0 } },
    { "CHNGUN.GMD", NULL, 0, 0x5A, 0x5A, 0x5A, 5, 0x122, 9, -2, 0x960, 0x10, 0, 0x64, 0x64, 2, 0, { 0x15, 0 }, -0x5F, -0x11, { 0xA0, 0x80, 0x60, 0 } },
    { "KEYCARD.ZZZ", NULL, 0x14, 0, 0, 0, 0, 4, -1, -1, 0, 0, 0x28, 0, 0, 0, 0, { 0x19, 0 }, 0x22, 0, { 0, 0, 0, 0 } },
    { "DEVICE.ZZZ", NULL, 0x15, 0, 0, 0, 0, 4, -1, -1, 0, 0, 0x28, 0, 0, 0, 0, { 0x1A, 0 }, 0x22, 0, { 0, 0, 0, 0 } },
    { "SYRINGE.ZZZ", NULL, 0x16, 0, 0, 0, 0, 4, -1, -1, 0, 0, 0x28, 0, 0, 0, 0, { 0x1B, 0 }, 0x22, 0, { 0, 0, 0, 0 } },
};
static u8 D_8010628C[0x1C] = { /* next weapon in the selection order */
    1, 4, 0, 0, 9, 0xA, 5, 6, 7, 0xB, 0x11, 8, 0x13, 0xC, 0x15, 0, 0xE, 0xD, 0x17, 0x14, 0x10, 0x12, 0, 0x18, 0x19, 1, 0, 0,
};
static u8 D_801062A8[0x1C] = { /* previous weapon in the selection order */
    1, 0x19, 0, 0, 1, 6, 7, 8, 0xB, 4, 5, 9, 0xD, 0x11, 0x10, 0, 0x14, 0xA, 0x15, 0xC, 0x13, 0xE, 0, 0x12, 0x17, 0x18, 0, 0,
};
/* .sdata */
s32 gCurrentWeapon = 1;
u32 gWeaponsAvailable[1] = { 0 }; /* bit per weapon */
static s32 D_8010F928 = 0;        /* last allocated weapon anim slot */
s16 gTaserGobjID = -1;
static s16 D_8010F92E = -1;       /* taser hum song, -1 when not playing */
static s32 D_8010F930 = -1;       /* taser shock ticks, -1 before the first */
static char D_8010F934[2] = " ";  /* first used by WeaponMakeRoundsString (still asm) */
/* 0x8010F938 (unreferenced, weapon or movement) stays in the blob */
/* .sbss */
static u8 D_80110028;
static WeaponMasterAnim *D_8011002C; /* body armour (VEST.GMD) */

extern WeaponState gWeaponState[];
extern s32 particleTaserState;
extern s32 gPermHogFile;
s32 EGSHogGetBlockByName(s32 hog, char *name, void **out);
void LevelGetModel(char *name, void **out);
WeaponMasterAnim *EGSAnimCreateMasterAnim(void *model, s32 flags);
int rand(void);

/* Bit-string access that tolerates an unaligned base. */
#define BITS_WORD(arr, n, w, b)                    \
    {                                              \
        u32 _m;                                    \
        w = (u32 *)(arr);                          \
        b = (n);                                   \
        _m = (u32)w & 3;                           \
        if (_m) {                                  \
            b = (n) + _m * 8;                      \
            w = (u32 *)((u32)w & ~3);              \
        }                                          \
    }

extern s32 gPlayerOffset;

/* GameObjectInfo.flags24 viewed as the NPC weapon word: low byte = weapon, 0x3000 = default kind */
#define GOBJINFO_WEAPON(info) (*(u8 *)&(info)->flags24)
#define GOBJINFO_WORD24(info) (*(u32 *)&(info)->flags24)
/* Weapon an object holds (no player special case); a single expression, so the
 * compiler folds scaling/conversion into each arm. */
#define GOBJINFO_NPC_WEAPON(info)                                                            \
    (GOBJINFO_WEAPON(info) != 0                ? GOBJINFO_WEAPON(info)                        \
     : (GOBJINFO_WORD24(info) & 0x3000) != 0x1000 ? ((GOBJINFO_WORD24(info) & 0x3000) == 0x2000 ? 0x14 : 0) \
                                               : 0x13)

static inline s32 WeaponOfGobj(Gobj *gobj) {
    s32 weapon;
    s32 kind;

    if (gobj->id != gPlayerOffset) {
        weapon = GOBJINFO_WEAPON(&gameObjectsInfo[gobj->id]);
        if (weapon == 0) {
            kind = GOBJINFO_WORD24(&gameObjectsInfo[gobj->id]) & 0x3000;
            if (kind != 0x1000) {
                weapon = kind == 0x2000 ? 0x14 : 0;
            } else {
                weapon = 0x13;
            }
        }
    } else {
        weapon = gCurrentWeapon;
    }
    return weapon;
}

#define WEAPON_ANIM_MAX 30

typedef struct WeaponAnim {
    /* 0x00 */ u8 unk00[9];
    /* 0x09 */ u8 unk09;
    /* 0x0A */ u8 unk0A; /* 0x10: held */
    /* 0x0B */ u8 unk0B;
    /* 0x0C */ s32 unk0C; /* nonzero while in use */
    /* 0x10 */ WeaponMasterAnim *masterAnim;
    /* 0x14 */ u8 unk14[2];
    /* 0x16 */ u16 unk16; /* pickup data (low byte) */
    /* 0x18 */ GameObjectAnim *unk18; /* owner's anim while held */
} WeaponAnim;

extern s32 D_80118F40[WEAPON_ANIM_MAX];      /* owner per slot: -1 free, negative = dropped */
extern WeaponAnim *D_80118A8C[WEAPON_ANIM_MAX];

/* Placement frame of each weapon anim slot (0x24 bytes each). */
typedef struct WeaponFrame {
    /* 0x00 */ s16 m[3][3];
    /* 0x12 */ u8 unk12[2];
    /* 0x14 */ s32 x, y, z;
    /* 0x20 */ s32 unk20;
} WeaponFrame; /* size 0x24 */
extern WeaponFrame D_80118B08[WEAPON_ANIM_MAX];

/* Saved dropped weapon (8 bytes). */
typedef struct WeaponSaveInfo {
    /* 0x00 */ s16 x, y, z;
    /* 0x06 */ u16 info; /* anim byte 0x16 | owner << 8, 0 when owned by an object */
} WeaponSaveInfo;

void WeaponDrop(Gobj *gobj, u8 keep);
WeaponAnim *iWeaponAllocateAnim(s32 owner, s32 *slotOut);

#define WEAPON_PREVIOUS 0x1C
#define WEAPON_NEXT 0x1B
/* Weapons that don't use ammo (clip size 0) */
#define WEAPON_NO_AMMO(w) (gWeaponInfo[w].clipSize * (gWeaponInfo[w].clipShift + 1) == 0)

extern Gobj *D_80118FB8[]; /* [0]: first object to select a weapon, -1 until then */

s32 WeaponGetPreviousID(s32 weapon);
void InterfaceWeaponChange(void);
u8 WeaponReload(s32 weapon);
void CASRequestAction(s32 id, s32 action);

u16 TextReplaceString(u16 id, char *str, s32 arg2, s32 arg3);
s32 SysPlaySong(s32 arg0, s32 song, void *arg2, s32 arg3);
s32 WeaponAddAmmo(s32 weapon, s32 rounds);
s32 WeaponGetNextID(s32 weapon);
void WeaponSelect(Gobj *gobj, s32 weapon, s32 arg2);
void InterfaceWeaponDisplayUpdate(void);
void SysPlayWpnSnd(s32 arg0, s32 snd, Gobj *gobj, s32 arg3);
void NPCSoundGobj(s32 arg0, s32 id, void *arg2, s32 snd);

/* NPC state (GameObjectRuntime.npc) fields used here */
typedef struct WeaponNPC {
    /* 0x00 */ u8 unk00[0x20];
    /* 0x20 */ s32 flags;
    /* 0x24 */ u8 unk24[0x12];
    /* 0x36 */ s16 burst; /* shots left before the reload animation */
    /* 0x38 */ u8 unk38[4];
    /* 0x3C */ s32 reloadAction;
    /* 0x40 */ u8 unk40[0xC];
    /* 0x4C */ s8 unk4C;
} WeaponNPC;

/* Terrain sector (gTerrSector[], 0x3C bytes); a dropped weapon shows while its model is loaded */
typedef struct WeaponTerrSector {
    /* 0x00 */ s32 *model;
    /* 0x04 */ u8 unk04[0x38];
} WeaponTerrSector;
extern WeaponTerrSector *gTerrSector;

void EGSDisplayAddAnim(void *display, void *anim);
void EGSDisplayDeleteAnim(void *display, void *anim);
void EGSGeometry_GetFuzzyDistancePoint2Point(VECTOR32 *a, void *b, s32 *dist);
void PickupSetDataIDs(u8 data, s16 *armor, s16 *weapon);

extern MATRIX D_80118A58; /* hand matrix with Y flipped, for WeaponGetTip */
void PickupObjectSet(); /* (s16 objId, u8 armor, u8 weapon, s16 amount, u8 showMsg), called unprototyped */
u8 PickupObjectTake(u8 manual);
void MatrixNormal_0(void *in, void *out);

/* Terrain sector found by EGSDisplayGetPointSector */
typedef struct WeaponDropSectorDisplay {
    /* 0x00 */ u8 unk00[2];
    /* 0x02 */ s16 id;
} WeaponDropSectorDisplay;
typedef struct WeaponDropSectorModel {
    /* 0x00 */ u8 unk00[0x20];
    /* 0x20 */ WeaponDropSectorDisplay *display;
} WeaponDropSectorModel;
typedef struct WeaponDropSector {
    /* 0x00 */ u8 unk00[0x10];
    /* 0x10 */ WeaponDropSectorModel *model;
} WeaponDropSector;
WeaponDropSector *EGSDisplayGetPointSector(s32 *pos);

void EGSAnimSimpleCreate(void *model, s32 arg1, s32 flags, WeaponAnim **out);


void WeaponInit(void) {
    char name[16];
    void *model = NULL;
    s32 i;

    gWeaponsAvailable[0] = 0;
    for (i = 0; i < 0x1A; i++) {
        gWeaponState[i].loaded = 0;
        gWeaponState[i].reserve = 0;
        if (gWeaponInfo[i].name != NULL && gWeaponInfo[i].name[0] != 0 &&
            EGSHogGetBlockByName(gPermHogFile, gWeaponInfo[i].name, &model) == 0) {
            gWeaponInfo[i].masterAnim = EGSAnimCreateMasterAnim(model, 0x04010000);
            strcpy(name, gWeaponInfo[i].name);
            name[0] = 'X';
            if (EGSHogGetBlockByName(gPermHogFile, name, &model) == 0) {
                gWeaponInfo[i].masterAnim->dropModel = model;
            }
        } else {
            gWeaponInfo[i].masterAnim = NULL;
        }
    }
    LevelGetModel("VEST.GMD", &model);
#line 333
    ASSERT(model != NULL);
    D_8011002C = EGSAnimCreateMasterAnim(model, 0x04010000);
    LevelGetModel("XEST.GMD", &model);
#line 336
    ASSERT(model != NULL);
    D_8011002C->dropModel = model;
    /* one past the end: also resets D_80118FB8 */
    for (i = 0; i <= WEAPON_ANIM_MAX; i++) {
        if (model != NULL) {
            EGSAnimSimpleCreate(model, 0, 0x04010000, &D_80118A8C[i]);
            D_80118A8C[i]->unk18 = NULL;
            D_80118A8C[i]->unk09 = 0x11;
        }
        D_80118F40[i] = -1;
    }
    gTaserGobjID = -1;
    D_80110028 = 1;
}

/* Dropped weapons: show them while their sector is visible, and let the player walk over them. */
void WeaponUpdate(void) {
    s32 i;
    WeaponAnim *anim;
    VECTOR32 pos;
    s32 dist;
    s16 armor;
    s16 weapon;
    u8 amount;

    for (i = 0; i < WEAPON_ANIM_MAX; i++) {
        anim = D_80118A8C[i];
        if (D_80118F40[i] >= 0) {
            if (*(s32 *)gTerrSector[D_80118F40[i]].model != 0) {
                if (anim->unk0C == 0) {
                    anim->unk0C = (s32)&D_80118B08[i];
                    EGSDisplayAddAnim(*gCamera, anim);
                }
                pos = gObjPlayer->move->pos;
                pos.vy = -pos.vy;
                EGSGeometry_GetFuzzyDistancePoint2Point(&pos, &((WeaponFrame *)anim->unk0C)->x, &dist);
                if (dist < 0xC0) {
                    PickupSetDataIDs(anim->unk16, &armor, &weapon);
                    if (weapon != 0) {
                        amount = gWeaponInfo[weapon].unk0B;
                    } else {
                        amount = 0;
                    }
                    if (weapon != 0xF) {
                        PickupObjectSet(-1, armor, weapon, amount, 0);
                        if (PickupObjectTake(0)) {
                            D_80118F40[i] = -1;
                            EGSDisplayDeleteAnim(*gCamera, anim);
                        }
                    }
                }
            } else if (anim->unk0C != 0) {
                anim->unk0C = 0;
                EGSDisplayDeleteAnim(*gCamera, anim);
            }
        }
    }
}

void WeaponAdd(s32 weapon, s32 rounds) {
    u32 *words;
    s32 bit;

    s32 word;
    u32 mask;

    BITS_WORD(gWeaponsAvailable, weapon, words, bit);
    word = bit >> 5;
    mask = 1 << (bit & 0x1F);
    words[word] |= mask;
    if (rounds) {
        WeaponAddAmmo(weapon, rounds);
    }
}

void WeaponRemove(s32 weapon) {
    WeaponState *state = &gWeaponState[weapon];
    u32 *words;
    s32 bit;
    s32 word;
    u32 mask;

    if (gCurrentWeapon == weapon) {
        WeaponSelect(gObjPlayer, WeaponGetNextID(weapon), 0);
    }
    state->reserve = 0;
    state->loaded = 0;
    BITS_WORD(gWeaponsAvailable, weapon, words, bit);
    word = bit >> 5;
    mask = 1 << (bit & 0x1F);
    words[word] &= ~mask;
}

void WeaponSpecialUse(s32 weapon) {
    WeaponState *state = &gWeaponState[weapon];

    if (weapon != WEAPON_TASER && gWeaponInfo[weapon].clipSize * (gWeaponInfo[weapon].clipShift + 1) == 0 &&
        state->loaded != 0) {
        state->loaded--;
        if (weapon == gCurrentWeapon) {
            InterfaceWeaponDisplayUpdate();
        }
    }
}

s32 WeaponHasMaxAmmo(s32 weapon) {
    WeaponState *state = &gWeaponState[weapon];
    s32 ret;
    s32 total;

    if (weapon == WEAPON_TASER ||
        gWeaponInfo[weapon].clipSize * (gWeaponInfo[weapon].clipShift + 1) != 0) {
        if (gWeaponInfo[weapon].clipShift != 0) {
            total = state->loaded;
            total += state->reserve;
            ret = total >= gWeaponInfo[weapon].clipSize * (gWeaponInfo[weapon].clipShift + 1);
        } else {
            ret = state->loaded >= gWeaponInfo[weapon].clipSize;
        }
    } else {
        ret = 0;
    }
    return ret;
}

s32 WeaponAddAmmo(s32 weapon, s32 rounds) {
    WeaponState *state = &gWeaponState[weapon];
    s32 shift;
    s32 clipSize;
    s32 reserve;
    s32 loaded;
    s32 max;
    s32 extra;

    if (weapon == WEAPON_TASER || !WEAPON_NO_AMMO(weapon)) {
        shift = gWeaponInfo[weapon].clipShift;
        if (shift != 0) {
            clipSize = gWeaponInfo[weapon].clipSize;
            reserve = state->reserve;
            loaded = state->loaded;
            if (reserve >= clipSize * (shift + 1)) {
                return 0;
            }
            reserve += rounds;
            max = clipSize * shift;
            if (max < reserve) {
                extra = reserve - max;
                reserve = max;
                if (extra != 0) {
                    loaded += extra;
                    if (clipSize < loaded) {
                        rounds -= loaded - clipSize;
                        loaded = clipSize;
                    }
                    state->loaded = loaded;
                }
            }
            state->reserve = reserve;
            if (state->loaded == 0) {
                WeaponReload(weapon);
            }
        } else {
            state->loaded += rounds;
            if (gWeaponInfo[weapon].clipSize < state->loaded) {
                rounds -= state->loaded - gWeaponInfo[weapon].clipSize;
                state->loaded = gWeaponInfo[weapon].clipSize;
            }
        }
    } else {
        state->loaded += rounds;
    }
    if (weapon == gCurrentWeapon) {
        InterfaceWeaponDisplayUpdate();
    }
    return rounds;
}

/* World position of the muzzle of the weapon in gobj's hand; 0 if it holds no valid weapon. */
s32 WeaponGetTip(Gobj *gobj, VECTOR32 *tip) {
    WeaponInfo *info;

    if (GOBJINFO_NPC_WEAPON(&gameObjectsInfo[gobj->id]) >= 0x1A) {
        return 0;
    }
    info = &gWeaponInfo[GOBJINFO_NPC_WEAPON(&gameObjectsInfo[gobj->id])];
    tip->vx = 0;
    tip->vy = info->tipY;
    tip->vz = info->tipZ;
    D_80118A58.m[0][0] = gobj->anim->frames[8]->m[0][0];
    D_80118A58.m[0][1] = -gobj->anim->frames[8]->m[0][1];
    D_80118A58.m[0][2] = gobj->anim->frames[8]->m[0][2];
    D_80118A58.m[1][0] = -gobj->anim->frames[8]->m[1][0];
    D_80118A58.m[1][1] = gobj->anim->frames[8]->m[1][1];
    D_80118A58.m[1][2] = -gobj->anim->frames[8]->m[1][2];
    D_80118A58.m[2][0] = gobj->anim->frames[8]->m[2][0];
    D_80118A58.m[2][1] = -gobj->anim->frames[8]->m[2][1];
    D_80118A58.m[2][2] = gobj->anim->frames[8]->m[2][2];
    D_80118A58.t[0] = gobj->anim->frames[8]->pos.vx;
    D_80118A58.t[1] = -gobj->anim->frames[8]->pos.vy;
    D_80118A58.t[2] = gobj->anim->frames[8]->pos.vz;
    ApplyMatrixLV(&D_80118A58, (VECTOR *)tip, (VECTOR *)tip);
    tip->vx += D_80118A58.t[0];
    tip->vy += D_80118A58.t[1];
    tip->vz += D_80118A58.t[2];
    return 1;
}

s32 WeaponGetPreviousID(s32 weapon) {
    s32 id = D_801062A8[weapon];
    u32 *words;
    s32 bit;
    s32 word;
    u32 mask;

    while (id != gCurrentWeapon) {
        BITS_WORD(gWeaponsAvailable, id, words, bit);
        word = bit >> 5;
        mask = 1 << (bit & 0x1F);
        if (words[word] & mask) {
            break;
        }
        id = D_801062A8[id];
    }
    return id;
}

s32 WeaponGetNextID(s32 weapon) {
    s32 id = D_8010628C[weapon];
    u32 *words;
    s32 bit;
    s32 word;
    u32 mask;

    while (id != gCurrentWeapon) {
        BITS_WORD(gWeaponsAvailable, id, words, bit);
        word = bit >> 5;
        mask = 1 << (bit & 0x1F);
        if (words[word] & mask) {
            break;
        }
        id = D_8010628C[id];
    }
    return id;
}

#ifdef NON_MATCHING
/* Loop setup order: the original initialises p after the hoisted loop constants. */
WeaponAnim *iWeaponAllocateAnim(s32 owner, s32 *slotOut) {
    s32 i = D_8010F928 + 1;
    s32 reuse = -1;
    s32 *p;

    if (i >= WEAPON_ANIM_MAX) {
        i = 0;
    }
    p = &D_80118F40[i];
    while (i != D_8010F928) {
        if (*p == -1) {
            break;
        }
        if (!(*p & 0x80000000) && reuse < 0) {
            reuse = i;
        }
        i++;
        p++;
        if (i >= WEAPON_ANIM_MAX) {
            p = D_80118F40;
            i = 0;
        }
    }
    if (i == D_8010F928 && D_80118A8C[i]->unk0C != 0) {
        i++;
        if (reuse >= 0) {
            i = reuse;
        }
    }
    D_80118F40[i] = owner;
    D_8010F928 = i;
    if (slotOut != NULL) {
        *slotOut = i;
    }
    return D_80118A8C[i];
}
#else
INCLUDE_ASM("asm/main/nonmatchings/weapon", iWeaponAllocateAnim);
#endif

#ifdef NON_MATCHING
/* Scheduling only: the else branch loads gCamera (into a2) before the slot stores */
/* Show the object's current weapon in its hand (or hide it when it has none). */
void WeaponReplace(Gobj *gobj) {
    WeaponInfo *info;
    WeaponAnim *anim;
    s32 i;
    struct EGSFrame **frame;

    info = &gWeaponInfo[GOBJINFO_NPC_WEAPON(&gameObjectsInfo[gobj->id])];
    for (i = WEAPON_ANIM_MAX; i >= 0; i--) {
        if (D_80118F40[i] == (s32)gobj) {
            break;
        }
    }
    if (i >= 0) {
        anim = D_80118A8C[i];
    } else {
        anim = iWeaponAllocateAnim((s32)gobj, NULL);
    }
    if (info->masterAnim != 0) {
        anim->unk18 = NULL;
        anim->unk09 = 0x11;
        anim->unk18 = gobj->anim;
        frame = gobj->anim->frames;
        anim->unk0C = (s32)*(gobj->npcState != 8 ? &frame[8] : &frame[4]);
        anim->masterAnim = info->masterAnim;
        anim->unk16 = GOBJINFO_NPC_WEAPON(&gameObjectsInfo[gobj->id]);
        EGSDisplayAddAnim(*gCamera, anim);
    } else {
        anim->unk0C = 0;
        D_80118F40[D_8010F928] = -1;
        D_8010F928--;
        EGSDisplayDeleteAnim(*gCamera, anim);
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/weapon", WeaponReplace);
#endif

void WeaponSelect(Gobj *gobj, s32 weapon, s32 quiet) {
    s32 next;

    if (gobj == gObjPlayer) {
        if (weapon == WEAPON_PREVIOUS) {
            weapon = WeaponGetPreviousID(gCurrentWeapon);
            gCurrentWeapon = weapon;
        } else if (weapon == WEAPON_NEXT) {
            next = WeaponGetNextID(gCurrentWeapon);
            if (!quiet) {
                while (next != WEAPON_TASER && WEAPON_NO_AMMO(next)) {
                    next = WeaponGetNextID(next);
                }
            }
            weapon = gCurrentWeapon = next;
        } else {
            gCurrentWeapon = weapon;
        }
        if (!quiet) {
            InterfaceWeaponChange();
            WeaponReload(weapon);
            CASRequestAction(gobj->id, 0x25);
        }
        if (D_80118FB8[0] == (Gobj *)-1) {
            D_80118FB8[0] = gobj;
        }
    }
    GOBJINFO_WEAPON(&gameObjectsInfo[gobj->id]) = weapon;
}

#ifdef NON_MATCHING
/* Register allocation: the original keeps one more callee-saved giv (keep spilled to the stack) and inverts the weapon-byte branch */
/* gobj lets go of its weapon: drop it on the floor (keep) or remove it. A player/armoured NPC
 * also drops its body armour. */
void WeaponDrop(Gobj *gobj, u8 keep) {
    s32 i;
    WeaponAnim *anim;
    WeaponAnim *vest;
    WeaponDropSector *sector;
    s32 slot;
    WeaponFrame *frame;

    for (i = 0; i < WEAPON_ANIM_MAX; i++) {
        if (D_80118F40[i] == (s32)gobj) {
            anim = D_80118A8C[i];
            anim->unk0A &= ~0x10;
            if (!keep) {
                D_80118F40[i] = -1;
                anim->unk0C = 0;
                EGSDisplayDeleteAnim(*gCamera, anim);
            } else {
                sector = EGSDisplayGetPointSector(&((WeaponFrame *)anim->unk0C)->x);
                if (sector != NULL) {
                    D_80118F40[i] = sector->model->display->id;
                    frame = &D_80118B08[i];
                    *frame = *(WeaponFrame *)anim->unk0C;
                    D_80118B08[i].unk20 = 0;
                    anim->unk0C = (s32)frame;
                    anim->unk18 = NULL;
                    anim->unk09 = 0x11;
                    anim->unk18 = NULL;
                    frame->m[0][0] = 0;
                    frame->m[2][0] = 0;
                    frame->m[1][1] = 0;
                    frame->m[1][2] = 0;
                    MatrixNormal_0(frame, frame);
                    frame->y = -(((MoveInfo *)gobj->move)->unk12C + 0x28);
                    anim->unk16 = GOBJINFO_NPC_WEAPON(&gameObjectsInfo[gobj->id]);
                    if ((gobj->id == gPlayerOffset || (GOBJINFO_WORD24(&gameObjectsInfo[gobj->id]) & 0x4000)) &&
                        gobj->healthInfo->armor != 0) {
                        vest = iWeaponAllocateAnim(D_80118F40[i], &slot);
                        D_80118B08[slot] = *(WeaponFrame *)gobj->anim->frames[0xD];
                        D_80118B08[slot].unk20 = 0;
                        D_80118B08[slot].y = frame->y;
                        vest->unk16 = 0x80;
                        if (gobj->id != gPlayerOffset) {
                            GOBJINFO_WORD24(&gameObjectsInfo[gobj->id]) &= ~0x4000;
                        }
                        vest->masterAnim = D_8011002C;
                        EGSDisplayAddAnim(*gCamera, vest);
                    }
                } else {
                    D_80118F40[i] = -1;
                    anim->unk0C = 0;
                    EGSDisplayDeleteAnim(*gCamera, anim);
                }
                GOBJINFO_WEAPON(&gameObjectsInfo[gobj->id]) = 0;
            }
        }
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/weapon", WeaponDrop);
#endif

u8 WeaponReload(s32 weapon) {
    WeaponState *state = &gWeaponState[weapon];
    s32 reserve = state->reserve;
    s32 need;

    if (reserve != 0) {
        need = gWeaponInfo[weapon].clipSize - state->loaded;
        if (need > 0) {
            if (need < reserve) {
                state->reserve -= need;
                state->loaded += need;
            } else {
                state->loaded += reserve;
                state->reserve = 0;
            }
            InterfaceWeaponDisplayUpdate();
            return 1;
        }
    }
    return 0;
}

void WeaponPlayReloadSndCASAnimCB(s16 id, s32 arg1, s32 quiet) {
    Gobj *gobj = gameObjectsInfo[id].runtime;
    u32 weapon = WeaponOfGobj(gobj);

    if (weapon - 6 < 2) {
        if (!quiet) {
            SysPlaySong(1, 0xE, gobj, 0);
        }
    } else {
        SysPlaySong(1, 0x13, gobj, 0);
    }
}

void WeaponDoFiredCallback(s16 id, s16 weapon) {
    if (D_8010F900 != NULL) {
        D_8010F900(id, weapon);
    }
}

static inline void WeaponFired(s32 id, s32 weapon) {
    if (D_8010F900 != NULL) {
        D_8010F900(id, weapon);
    }
}

/* A shot was fired: sound, ammo and the reload animation when the clip/burst is empty. */
void WeaponSubRound(Gobj *gobj) {
    s32 weapon;
    u8 reload;
    WeaponState *state;

    if (gobj->id != gPlayerOffset) {
        weapon = GOBJINFO_NPC_WEAPON(&gameObjectsInfo[gobj->id]);
        if (gWeaponInfo[weapon].npcFireSnd == -2) {
            SysPlaySong(1, gWeaponInfo[weapon].fireSong, gobj, 0);
        } else if (gWeaponInfo[weapon].npcFireSnd >= 0) {
            SysPlayWpnSnd(1, gWeaponInfo[weapon].npcFireSnd, gobj, ((WeaponNPC *)gobj->npc)->unk4C);
        }
        WeaponFired(gobj->id, weapon);
        if (!(((WeaponNPC *)gobj->npc)->flags & 0x100000)) {
            if (gWeaponInfo[weapon].clipShift) {
                ((WeaponNPC *)gobj->npc)->burst--;
                if (gWeaponInfo[weapon].clipShift && ((WeaponNPC *)gobj->npc)->burst == 0) {
                    reload = 1;
                    ((WeaponNPC *)gobj->npc)->burst = gWeaponInfo[weapon].clipSize;
                } else {
                    reload = 0;
                }
            } else {
                reload = 1;
            }
            if (gobj->cas != NULL && reload) {
                if ((u32)(weapon - 6) >= 2 && weapon != 0x10) {
                    ((WeaponNPC *)gobj->npc)->reloadAction = 0x3D;
                } else {
                    ((WeaponNPC *)gobj->npc)->reloadAction = 0x54;
                }
                CASRequestAction(gobj->id, ((WeaponNPC *)gobj->npc)->reloadAction);
            }
        }
    } else {
        state = &gWeaponState[gCurrentWeapon];
        if (state->loaded != 0) {
            if (gCurrentWeapon != WEAPON_TASER) {
                state->loaded--;
            }
            if (gWeaponInfo[gCurrentWeapon].fireSong >= 0) {
                SysPlaySong(1, gWeaponInfo[gCurrentWeapon].fireSong, gobj, 0);
                if ((u32)(gCurrentWeapon - 0x13) >= 2) {
                    NPCSoundGobj(0, gPlayerOffset, NULL, gWeaponInfo[gCurrentWeapon].npcAlertSnd);
                    WeaponFired(gPlayerOffset, gCurrentWeapon);
                }
            }
        }
        if (gobj->cas->active != 8 &&
            (!gWeaponInfo[gCurrentWeapon].clipShift || (state->loaded == 0 && state->reserve != 0))) {
            if ((u32)(gCurrentWeapon - 6) >= 2 && gCurrentWeapon != 0x10) {
                if (WeaponReload(gCurrentWeapon)) {
                    CASRequestAction(gobj->id, 0x3D);
                }
            } else {
                CASRequestAction(gobj->id, 0x54);
            }
        }
        InterfaceWeaponDisplayUpdate();
    }
}

void WeaponGetRounds(s32 weapon, s32 *rounds, s32 *clips) {
    if (weapon == WEAPON_TASER) {
        if (rounds != NULL) {
            if (particleTaserState == 0) {
                *rounds = 1;
            } else {
                *rounds = 0;
            }
        }
        if (clips != NULL) {
            *clips = 0;
        }
    } else {
        if (rounds != NULL) {
            *rounds = gWeaponState[weapon].loaded;
        }
        if (clips != NULL) {
            *clips = gWeaponState[weapon].reserve;
        }
    }
}

s32 WeaponGetDeathAction(s32 weapon) {
    return gWeaponInfo[weapon].deathAction;
}

WeaponFiredCallback WeaponSetWeaponFiredCallback(WeaponFiredCallback cb) {
    WeaponFiredCallback old = D_8010F900;
    D_8010F900 = cb;
    return old;
}

WeaponCallback WeaponSetTaserHitNpcCallback(WeaponCallback cb) {
    WeaponCallback old = D_8010F904;
    D_8010F904 = cb;
    return old;
}

/* TargetInfo fields used here */
typedef struct WeaponTargetInfo {
    /* 0x00 */ s16 targetId;
    /* 0x02 */ s16 unk02;
    /* 0x04 */ u32 flags;
    /* 0x08 */ u8 unk08[0x52];
    /* 0x5A */ s16 aimBone; /* negative: aim at the body point */
    /* 0x5C */ u8 unk5C[2];
    /* 0x5E */ s16 windowId; /* window in the line of fire, -1 for none */
    /* 0x60 */ u8 unk60[4];
    /* 0x64 */ VECTOR32 bonePoint;
    /* 0x74 */ VECTOR32 bodyPoint;
} WeaponTargetInfo;

VECTOR32 *SniperGetTarget(s32 *outId);
void ParticleTaser(VECTOR32 *aim);
void ParticleTaserSetTarget(Gobj *target, s32 bone);
void ParticleTaserRetract(void);
void NPCDamagedByPC(s32 id);
void DamageShowFlakJacketMsg(s32 id);
void DamageGOBJ(s16 from, s16 by, s16 to, s16 amount, s16 type);
struct GameCamSlot;
void GameCameraSetModeInteraction(s32 on, Gobj *primary, VECTOR32 *pos, struct GameCamSlot *slot, s32 arg4);
void GameCameraRequestGetBehind(void);
void JoystickDirInputDeactivate(s32 deactivate, u8 arg1);
void SysStopSong(s32 arg0, s32 song);
void AnimateGobjElectrocute(Gobj *gobj);
void EGSGeometry_VectorRotateYXZ(VECTOR32 *in, VECTOR32 *angle, VECTOR32 *out);
void NPCPutFireOnNPCShow(s32 id);
extern s32 gSniperMode;
extern s32 gSniperBonesHit;
extern s32 sWindowCount;
extern Gobj *sWindowArray[];
extern struct GameCamSlot D_80118A78; /* camera slot for the taser close-up */

#define GOBJ_TYPE_WINDOW 0x6D

/* Taser trigger: fire (shoot the darts, then keep shocking) or release. */
void WeaponTaser(Gobj *gobj, u8 fire) {
    Gobj *target;
    Gobj *hit;
    VECTOR32 *aim;
    s32 hitId;
    s32 bone;
    u32 bits;
    s32 b;
    u32 i;
    s32 id;

    if (fire) {
        if (particleTaserState == 0 && D_80110028 != 0) {
            D_8010F930 = -1;
            D_80110028 = 0;
            target = NULL;
            if (gSniperMode != 0) {
                aim = SniperGetTarget(&hitId);
                if (hitId >= 0) {
                    target = gameObjectsInfo[hitId].runtime;
                }
                if (target != NULL &&
                    ((target->id != gPlayerOffset && !(GOBJINFO_WORD24(&gameObjectsInfo[target->id]) & 0x4000)) ||
                     !(gSniperBonesHit & 0x6000)) &&
                    !(target->cas->flags & 0x400) &&
                    (target->npc == NULL || !(((WeaponNPC *)target->npc)->flags & 2)) && GOBJ_TYPE(hitId) != 2 &&
                    target->healthInfo->health > 0) {
                    bone = -1;
                    gTaserGobjID = hitId;
                    bits = gSniperBonesHit;
                    for (b = 0; b < 15; b++) {
                        if (bits & 1) {
                            bone = b;
                            break;
                        }
                        bits >>= 1;
                    }
                    ParticleTaserSetTarget(target, bone);
                    ParticleTaser(aim);
                    NPCDamagedByPC(gTaserGobjID);
                    if (D_8010F904 != NULL) {
                        D_8010F904(target->id);
                    }
                } else {
                    gTaserGobjID = -1;
                    ParticleTaserSetTarget(NULL, -1);
                    ParticleTaser(aim);
                    if (gameObjectsInfo[hitId].runtime->id == gPlayerOffset ||
                        (GOBJINFO_WORD24(&gameObjectsInfo[gameObjectsInfo[hitId].runtime->id]) & 0x4000)) {
                        DamageShowFlakJacketMsg(hitId);
                    }
                }
                if (sWindowCount != 0) {
                    for (i = 0; i < sWindowCount; i++) {
                        id = sWindowArray[i]->id;
                        if (GOBJ_TYPE(id) != GOBJ_TYPE_WINDOW) {
                            DamageGOBJ(gPlayerOffset, gPlayerOffset, id, gWeaponInfo[0xD].damageNear, 0xF);
                        }
                    }
                }
            } else {
                if (gobj->target->targetId >= 0 && gTaserGobjID == -1) {
                    hit = gameObjectsInfo[gobj->target->targetId].runtime;
                    if ((u32)(hit->npcState - 1) < 2 && !(hit->cas->flags & 0x400) &&
                        (hit->npc == NULL || !(((WeaponNPC *)hit->npc)->flags & 2)) && hit->target->targetId >= 0 &&
                        GOBJ_TYPE(hit->target->targetId) != 2) {
                        if (hit->id != gPlayerOffset && !(GOBJINFO_WORD24(&gameObjectsInfo[hit->id]) & 0x4000)) {
                            target = hit;
                            if (D_8010F904 != NULL) {
                                D_8010F904(hit->id);
                            }
                        } else {
                            DamageShowFlakJacketMsg(hit->id);
                        }
                    }
                }
                if (target != NULL) {
                    ParticleTaserSetTarget(target, -1);
                    gTaserGobjID = target->id;
                    DamageGOBJ(gobj->id, gobj->id, gTaserGobjID, 0, 0);
                } else if (((WeaponTargetInfo *)gobj->target)->aimBone >= 0) {
                    ParticleTaser(&((WeaponTargetInfo *)gobj->target)->bonePoint);
                } else {
                    ParticleTaser(&((WeaponTargetInfo *)gobj->target)->bodyPoint);
                }
                id = ((WeaponTargetInfo *)gobj->target)->windowId;
                if (id >= 0 && GOBJ_TYPE(id) != GOBJ_TYPE_WINDOW) {
                    DamageGOBJ(gPlayerOffset, gPlayerOffset, id, gWeaponInfo[0xD].damageNear, 0xF);
                }
            }
            SysPlaySong(1, 0x11, gobj, 0);
        } else if (particleTaserState == 2) {
            if (gTaserGobjID != -1) {
                if (D_8010F930 == -1) {
                    if (gameObjectsInfo[gTaserGobjID].runtime->healthInfo->health > 0) {
                        CASRequestAction(gTaserGobjID, 0x50);
                        NPCSoundGobj(1, gTaserGobjID, NULL, 0xC80);
                        D_8010F930 = 0;
                    }
                } else {
                    if (gSniperMode == 0) {
                        VECTOR32 from;
                        VECTOR32 to;
                        VECTOR32 dir;
                        VECTOR32 out;
                        VECTOR32 cam;
                        VECTOR32 rot = { 0, 0x200, 0, 0 };
                        s32 dist;

                        from = gobj->move->pos;
                        to = gameObjectsInfo[gTaserGobjID].runtime->move->pos;
                        dir.vx = from.vx - to.vx;
                        dir.vy = from.vy - to.vy;
                        dir.vz = from.vz - to.vz;
                        GTEMath_VectorNormal((VECTOR *)&dir, (VECTOR *)&dir);
                        EGSGeometry_VectorRotateYXZ(&dir, &rot, &out);
                        cam.vx = EGSMathFIXMul(out.vx, 0x140);
                        cam.vy = EGSMathFIXMul(out.vy, 0x140);
                        cam.vz = EGSMathFIXMul(out.vz, 0x140);
                        EGSGeometry_GetFuzzyDistancePoint2Point(&from, &to, &dist);
                        if (dist <= 0x280) {
                            cam.vx += from.vx;
                            cam.vy += from.vy;
                            cam.vz += from.vz;
                            cam.vy += 0xA0;
                        } else {
                            cam.vx += to.vx;
                            cam.vy += to.vy;
                            cam.vz += to.vz;
                        }
                        GameCameraSetModeInteraction(1, gameObjectsInfo[gTaserGobjID].runtime, &cam, &D_80118A78, 1);
                    } else {
                        JoystickDirInputDeactivate(-1, 0);
                    }
                    if (++D_8010F930 == 0x29) {
                        NPCPutFireOnNPCShow(gTaserGobjID);
                        NPCSoundGobj(1, gTaserGobjID, NULL, 0x3E80);
                    }
                    if (D_8010F92E == -1) {
                        D_8010F92E = SysPlaySong(1, 0x19, gameObjectsInfo[gTaserGobjID].runtime, 0);
                    }
                    AnimateGobjElectrocute(gameObjectsInfo[gTaserGobjID].runtime);
                    if (!(gobj->target->flags & 2)) {
                        CASRequestAction(gobj->id, 0x16);
                    }
                }
            } else {
                JoystickDirInputDeactivate(0, 0);
                ParticleTaserRetract();
            }
        }
        if (D_80110028 != 0) {
            JoystickDirInputDeactivate(-1, 0);
        }
    }
    if (!fire) {
        if (particleTaserState != 0) {
            if (gSniperMode == 0) {
                GameCameraSetModeInteraction(0, gobj, NULL, &D_80118A78, 1);
                GameCameraRequestGetBehind();
            }
            ParticleTaserRetract();
            if (gTaserGobjID != -1) {
                if (gameObjectsInfo[gTaserGobjID].runtime->healthInfo->health > 0) {
                    if (D_8010F930 >= 0x28) {
                        DamageGOBJ(gobj->id, gobj->id, gTaserGobjID, 0x7FFF, 0x51);
                    } else {
                        CASRequestAction(gTaserGobjID, 0x52);
                    }
                }
                gTaserGobjID = -1;
            }
            SysStopSong(1, D_8010F92E);
            D_8010F92E = -1;
        }
        JoystickDirInputDeactivate(0, 0);
        D_80110028 = 1;
        D_8010F930 = -1;
    }
}

#ifdef NON_MATCHING
/* Loop pointer setup/strength reduction differs (one insn longer). */
s32 WeaponGetSaveInfo(WeaponSaveInfo *out) {
    s32 i;
    s32 owner;
    WeaponFrame *frame;
    s32 *owners;
    WeaponAnim **anims;
    WeaponAnim *anim;

    for (i = 0; i < WEAPON_ANIM_MAX; i++) {
        owner = D_80118F40[i];
        if ((owner & 0x80000000) && owner != -1 && ((Gobj *)owner)->healthInfo->health <= 0) {
            WeaponDrop((Gobj *)owner, 1);
        }
    }
    for (i = 0, frame = D_80118B08, owners = D_80118F40, anims = D_80118A8C; i < WEAPON_ANIM_MAX; i++) {
        owner = *owners;
        anim = *anims;
        if (!(owner & 0x80000000)) {
            out->x = frame->x;
            out->y = frame->y;
            out->z = frame->z;
            out->info = anim->unk16 | ((owner & 0xFF) << 8);
        } else {
            out->info = 0;
        }
        out++;
        frame++;
        owners++;
        anims++;
    }
    return 1;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/weapon", WeaponGetSaveInfo);
#endif

s32 WeaponRestoreSaveInfo(WeaponSaveInfo *in) {
    s32 i;
    WeaponAnim *anim;
    WeaponFrame *frame;
    s16 (*m)[3];
    VECTOR side;
    VECTOR up;
    s32 info;

    for (i = 0; i < WEAPON_ANIM_MAX; i++) {
        anim = D_80118A8C[i];
        if ((s16)in[i].info != 0) {
            frame = &D_80118B08[i];
            frame->x = in[i].x;
            frame->y = in[i].y;
            frame->z = in[i].z;
            anim->unk16 = in[i].info & 0xFF;
            D_80118F40[i] = (s16)in[i].info >> 8;
            m = frame->m;
            if (m[1][0] != 1 && m[1][0] != -1) {
                side.vx = 0;
                up.vx = (rand() & 1) ? 0x1000 : -0x1000;
                up.vz = 0;
                up.vy = 0;
                m[1][0] = up.vx;
                m[1][1] = up.vy;
                m[1][2] = up.vz;
                while ((side.vy = rand() * 2) == 0 && (side.vz = rand() * 2) == 0) {
                }
                GTEMath_VectorNormal(&side, &side);
                OuterProduct12(&side, &up, &up);
                m[0][0] = side.vx;
                m[0][1] = side.vy;
                m[0][2] = side.vz;
                m[2][0] = up.vx;
                m[2][1] = up.vy;
                m[2][2] = up.vz;
            }
            info = in[i].info & 0x3F;
            if (info != 0) {
                anim->masterAnim = gWeaponInfo[info].masterAnim;
            } else {
                anim->masterAnim = D_8011002C;
            }
        } else {
            anim->unk0C = 0;
            D_80118F40[i] = -1;
        }

    }
    return 1;
}

void WeaponUnlimitedAmmo(void) {
    s32 i;
    WeaponState *state;

    for (i = 0; i < 0x1A; i++) {
        if (gWeaponInfo[i].masterAnim != NULL && gWeaponInfo[i].unk08 != 0) {
            WeaponAdd(i, 0);
            state = &gWeaponState[i];
            state->loaded = 0xFFFF;
        }
    }
    InterfaceWeaponDisplayUpdate();
}

void WeaponTorchCASAnimCB(void) {
}

#ifdef NON_MATCHING
/* The original rematerialises the /10 magic constant (0x66666667) in each block instead of
 * keeping it in a register (2 insns longer). */
u16 WeaponMakeRoundsString(u16 textId, s32 weapon) {
    s32 reserve = gWeaponState[weapon].reserve;
    s32 loaded = gWeaponState[weapon].loaded;
    char buf[8];
    char *p;

    if (weapon != 0x18 && WEAPON_NO_AMMO(weapon)) {
        return TextReplaceString(textId, D_8010F934, loaded, reserve);
    }
    if (loaded >= 100) {
        loaded = 99;
    }
    buf[0] = loaded / 10 + '0';
    buf[1] = loaded % 10 + '0';
    if (reserve > 0) {
        buf[2] = '/';
        p = &buf[3];
        if (reserve >= 100) {
            p = &buf[4];
            buf[3] = reserve / 100 + '0';
            reserve %= 100;
        }
        p[2] = '\0';
        p[0] = reserve / 10 + '0';
        p[1] = reserve % 10 + '0';
    } else {
        buf[2] = '\0';
    }
    return TextReplaceString(textId, buf, loaded, reserve);
}
#else
INCLUDE_ASM("asm/main/nonmatchings/weapon", WeaponMakeRoundsString);
#endif
