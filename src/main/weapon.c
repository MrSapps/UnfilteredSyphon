#include "common.h"
#include "system.h"
#include "gameobj.h"
#include "gobj.h"

/* weapon.c's small data, still assembled from asm */
SDATA_LOCAL(D_8010F900)
SDATA_LOCAL(D_8010F904)
SDATA_LOCAL(gCurrentWeapon)
SDATA_LOCAL(gWeaponsAvailable)
SDATA_LOCAL(D_8010F928)
SDATA_LOCAL(gTaserGobjID)
SDATA_LOCAL(D_8010F92E)
SDATA_LOCAL(D_8010F930)
SDATA_LOCAL(D_80110028)
SDATA_LOCAL(D_8011002C)

typedef void (*WeaponCallback)();

/* Per-weapon constant data (gWeaponInfo[], 0x20 bytes each). */
typedef struct WeaponInfo {
    /* 0x00 */ char *name;
    /* 0x04 */ s32 unk04;
    /* 0x08 */ u8 unk08; /* nonzero: usable weapon */
    /* 0x09 */ u8 clipSize;
    /* 0x0A */ u8 unk0A;
    /* 0x0B */ u8 unk0B;
    /* 0x0C */ u32 clipShift : 3;
    /* 0x0C */ u32 unk0C_3 : 13;
    /* 0x0E */ s8 unk0E;
    /* 0x0F */ s8 unk0F;
    /* 0x10 */ s16 unk10;
    /* 0x12 */ s16 deathAction;
    /* 0x14 */ u8 unk14[0xC];
} WeaponInfo; /* size 0x20 */

/* Per-weapon ammo (gWeaponState[]). */
typedef struct WeaponState {
    /* 0x00 */ u16 reserve; /* rounds not in the clip */
    /* 0x02 */ u16 loaded;  /* rounds in the clip */
} WeaponState;

#define WEAPON_TASER 0xE

extern WeaponCallback D_8010F900; /* weapon fired callback */
extern WeaponCallback D_8010F904; /* taser hit NPC callback */
extern s32 gCurrentWeapon;
extern u32 gWeaponsAvailable[]; /* bit per weapon */
extern WeaponInfo gWeaponInfo[];
extern WeaponState gWeaponState[];
extern u8 D_8010628C[]; /* next weapon in the selection order */
extern u8 D_801062A8[]; /* previous weapon in the selection order */
extern s32 particleTaserState;

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
    /* 0x00 */ u8 unk00[0xC];
    /* 0x0C */ s32 unk0C; /* nonzero while in use */
    /* 0x10 */ u8 unk10[6];
    /* 0x16 */ u8 unk16;
} WeaponAnim;

extern s32 D_8010F928;                       /* last allocated weapon anim slot */
extern s32 D_80118F40[WEAPON_ANIM_MAX];      /* owner per slot: -1 free, negative = dropped */
extern WeaponAnim *D_80118A8C[WEAPON_ANIM_MAX];

/* Placement frame of each weapon anim slot (0x24 bytes each). */
typedef struct WeaponFrame {
    /* 0x00 */ u8 unk00[0x14];
    /* 0x14 */ s32 x, y, z;
    /* 0x20 */ u8 unk20[4];
} WeaponFrame; /* size 0x24 */
extern WeaponFrame D_80118B08[WEAPON_ANIM_MAX];

/* Saved dropped weapon (8 bytes). */
typedef struct WeaponSaveInfo {
    /* 0x00 */ s16 x, y, z;
    /* 0x06 */ u16 info; /* anim byte 0x16 | owner << 8, 0 when owned by an object */
} WeaponSaveInfo;

void WeaponDrop(Gobj *gobj, s32 arg1);

#define WEAPON_PREVIOUS 0x1C
#define WEAPON_NEXT 0x1B
/* Weapons that don't use ammo (clip size 0) */
#define WEAPON_NO_AMMO(w) (gWeaponInfo[w].clipSize * (gWeaponInfo[w].clipShift + 1) == 0)

extern Gobj *D_80118FB8; /* first object to select a weapon, -1 until then */

s32 WeaponGetPreviousID(s32 weapon);
void InterfaceWeaponChange(void);
s32 WeaponReload(s32 weapon);
void CASRequestAction(s32 id, s32 action);
extern char D_8010F934[]; /* " " (.sdata) */

u16 TextReplaceString(u16 id, char *str, s32 arg2, s32 arg3);
void SysPlaySong(s32 arg0, s32 song, void *arg2, s32 arg3);
s32 WeaponAddAmmo(s32 weapon, s32 rounds);
s32 WeaponGetNextID(s32 weapon);
void WeaponSelect(Gobj *gobj, s32 weapon, s32 arg2);
void InterfaceWeaponDisplayUpdate(void);

INCLUDE_RODATA("asm/main/nonmatchings/weapon", D_80011E8C);

INCLUDE_ASM("asm/main/nonmatchings/weapon", WeaponInit);

INCLUDE_ASM("asm/main/nonmatchings/weapon", WeaponUpdate);

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

#ifdef NON_MATCHING
/* v0/v1 swapped when loading loaded/reserve. */
s32 WeaponHasMaxAmmo(s32 weapon) {
    WeaponState *state = &gWeaponState[weapon];
    s32 ret;

    if (weapon == WEAPON_TASER ||
        gWeaponInfo[weapon].clipSize * (gWeaponInfo[weapon].clipShift + 1) != 0) {
        if (gWeaponInfo[weapon].clipShift != 0) {
            ret = state->loaded + state->reserve >=
                  gWeaponInfo[weapon].clipSize * (gWeaponInfo[weapon].clipShift + 1);
        } else {
            ret = state->loaded >= gWeaponInfo[weapon].clipSize;
        }
    } else {
        ret = 0;
    }
    return ret;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/weapon", WeaponHasMaxAmmo);
#endif

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

INCLUDE_ASM("asm/main/nonmatchings/weapon", WeaponGetTip);

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

INCLUDE_ASM("asm/main/nonmatchings/weapon", WeaponReplace);

#ifdef NON_MATCHING
/* Only the D_80118FB8 access differs: the original materialises its address once (la + 0(reg)). */
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
        if (D_80118FB8 == (Gobj *)-1) {
            D_80118FB8 = gobj;
        }
    }
    GOBJINFO_WEAPON(&gameObjectsInfo[gobj->id]) = weapon;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/weapon", WeaponSelect);
#endif

INCLUDE_ASM("asm/main/nonmatchings/weapon", WeaponDrop);

s32 WeaponReload(s32 weapon) {
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

INCLUDE_ASM("asm/main/nonmatchings/weapon", WeaponSubRound);

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

WeaponCallback WeaponSetWeaponFiredCallback(WeaponCallback cb) {
    WeaponCallback old = D_8010F900;
    D_8010F900 = cb;
    return old;
}

WeaponCallback WeaponSetTaserHitNpcCallback(WeaponCallback cb) {
    WeaponCallback old = D_8010F904;
    D_8010F904 = cb;
    return old;
}

INCLUDE_ASM("asm/main/nonmatchings/weapon", WeaponTaser);

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

INCLUDE_ASM("asm/main/nonmatchings/weapon", WeaponRestoreSaveInfo);

void WeaponUnlimitedAmmo(void) {
    s32 i;
    WeaponState *state;

    for (i = 0; i < 0x1A; i++) {
        if (gWeaponInfo[i].unk04 != 0 && gWeaponInfo[i].unk08 != 0) {
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
