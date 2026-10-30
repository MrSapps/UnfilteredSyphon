#ifndef WEAPON_H
#define WEAPON_H

#include "common.h"

/*
 * Per-weapon constant data: gWeaponInfo[] at 0x80105F4C, 0x20 bytes per weapon id.
 * (weapon.c and interface.c still use their own local views of this table.)
 */
typedef struct GameWeaponInfo {
    /* 0x00 */ char *name;   /* icon file, "NAME.xxx" */
    /* 0x04 */ s32 unk04;
    /* 0x08 */ u8 sprite;    /* nonzero for usable weapons; HUD sprite index */
    /* 0x09 */ u8 clipSize;
    /* 0x0A */ u8 unk0A;
    /* 0x0B */ u8 unk0B;
    /* 0x0C */ u32 flags;    /* bits 0-2: clips per pickup - 1; 8-10: kind; 14: silenced */
    /* 0x10 */ s16 unk10;
    /* 0x12 */ s16 deathAction;
    /* 0x14 */ u32 unk14;    /* bits 6-15: blast damage */
    /* 0x18 */ s16 nameMsg;  /* index into gUseMsgs */
    /* 0x1A */ u8 unk1A[6];
} GameWeaponInfo; /* size 0x20 */

#define WEAPON_FLAG_SILENCED (1 << 14)

extern GameWeaponInfo gWeaponInfo[];

#endif /* WEAPON_H */
