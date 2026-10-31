#include "common.h"
#include "system.h"
#include "level.h"
#include "gameobj.h"
#include "gobj.h"
#include "game.h"

/* pickup.c's data is defined below its types */

/* "Use" prompts shown for objects the player is standing at. */
typedef struct UseMsg {
    /* 0x0 */ u16 textId; /* 0xFFFF when no string is showing */
    /* 0x2 */ s16 msgId;
    /* 0x4 */ s16 objId;
} UseMsg;

/* Pickups the player is standing on, waiting to be taken. */
typedef struct PickupPending {
    /* 0x0 */ s16 objId;
    /* 0x2 */ u8 armor;
    /* 0x3 */ u8 weapon;
    /* 0x4 */ s16 amount;
    /* 0x6 */ s16 showMsg;
} PickupPending;

#define USE_MSG_MAX 4
#define PICKUP_PENDING_MAX 4
#define PICKUP_TAKEN_MSG_COUNT 27 /* one per weapon/item id, 0x1A is the flak jacket */

/* .data */
static VECTOR32 D_80107840 = { 0, 0, 0, 0 };

/* .sdata */
static char D_8010FCEC[1] = "";
static s16 D_8010FCF0[4] = { -1, 0x18, 0x19, 0 }; /* armor pickup message ids */
static char *(*D_8010FCF8)() = NULL;  /* locked-door string callback */
static char *(*D_8010FCFC)() = NULL;  /* use string callback */
static void (*D_8010FD00)() = NULL;   /* key callback */
static void (*D_8010FD04)() = NULL;   /* bomb callback */
static s32 (*D_8010FD08)() = NULL;    /* computer callback */
static s32 (*D_8010FD0C)() = NULL;    /* switch callback */
static s32 D_8010FD10 = 0xAA;         /* unreferenced */
static s32 D_8010FD14 = 0x44;         /* unreferenced */
void (*gObjectInteractionCallback)() = NULL; /* level specific object callback */

/* .sbss: the original assembler gave every .lcomm 4 bytes (see particle.c) */
static s32 D_801101B8;
static s32 D_801101BC;                /* UseMsg count */
static s32 D_801101C0;                /* PickupPending count */
static u8 D_801101C4;
static u8 D_801101C4_pad;              /* aspsx 2.77 packs by size: pad */
static s16 D_801101C4_pad2;


static u8 D_801101C8;

/* .bss */
#ifdef _PSYQ
/* separate .lcomms would be 16-aligned by aspsx 2.77: laid out by hand below */
extern UseMsg D_80121978[USE_MSG_MAX];
extern PickupPending D_80121990[PICKUP_PENDING_MAX];
extern u16 D_801219B0[PICKUP_TAKEN_MSG_COUNT]; /* taken-message text ids */
extern u8 D_801219E8[PICKUP_TAKEN_MSG_COUNT];  /* checksums of those messages */
#else
/* maspsx's 2.7x layout would 16-align the arrays; one block, named with .set below
 * (psylink 8-aligns save's .bss after it: the 4-byte gap is a splat piece) */
static u8 D_80121978_bss[0x8C];
extern UseMsg D_80121978[USE_MSG_MAX];
extern PickupPending D_80121990[PICKUP_PENDING_MAX];
extern u16 D_801219B0[PICKUP_TAKEN_MSG_COUNT];
extern u8 D_801219E8[PICKUP_TAKEN_MSG_COUNT];
#endif

/* commons this TU reaches through $gp */
u8 gDeviceDetonated;
s32 gLevelKeycards;
s32 gLevelKeycardsTaken;
u32 gNextTickAllowMsgLongText;

void TextRemoveString();
s32 TextIsStringShowing(u16 str);
u8 TextComputeChecksum(char *str);
void TextChangeStringDuration(u16 str, s32 ticks);
s32 strlen(char *str);
u8 LevelGameObjectHasModel(s32 id);
void EGSDisplayRemoveDamage(Gobj *obj);
void EGSAnimShowDetailDef(GameObjectAnim *anim);
char *StringSetUseMsg(s16 msgId);
u16 TextAddMarker(u16 objId, char *str, s32 arg2, s32 arg3);
void DamageKillAllPeople(s32 by, s32 type);
void ParticleSmoke(s32 arg0, VECTOR32 *pos, void *dir, s32 arg3, s32 arg4);
void InterfaceDeleteBomb(void);

Gobj *ObjectMatchToRefID(s16 id, s32 type);
void PlayerUseCAS();
Gobj *iObjectFindNearbyLight(Gobj *obj, s32 type);

void iPickupObjectTakenMsg(char *msg, s32 idx);
void MatrixNormal_0(GOBJ_MATRIX *in, GOBJ_MATRIX *out);
void EGSFrameSetPosition(EGSFrame *frame, s32 arg1, VECTOR32 *pos);
s32 LevelDoEOL(u8 force);
void func_8008E418(Gobj *door, s32 type);
u8 PickupObjectTake(u8 arg0);
void CobjAddBulletCollisionBoxGobj(Gobj *obj, s32 arg1, s32 arg2, s32 arg3);
void CobjAddCollisionBoxGobj(Gobj *obj, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
void CobjRemoveBulletCollisionBoxGobj(Gobj *obj);
void CobjRemoveCollisionBoxGobj(Gobj *obj);
void func_80149118(Gobj *obj, SysMsg *msg); /* in an overlay */
void func_8014930C(Gobj *obj, s16 type);    /* in an overlay */
void func_8014819C(Gobj *obj);              /* in an overlay */
void func_80148440(Gobj *obj, s32 type);    /* in an overlay */
void func_8008D548(Gobj *door, s32 type);
s32 WeaponAddAmmo(s32 weapon, s32 rounds);
void StringTakeAmmoMsg(char *buf, s16 count, s16 weapon, u8 arg3, u8 arg4);
void StringTakeObjectMsg(char *buf, s32 count, s16 id);
void PickUpInventory(s16 objId);
s32 IsWideDoor(Gobj *obj);
void EGSFrameRotateLocalYXZ(EGSFrame *frame, VECTOR32 *rot);
void MovementRemoveGobj(Gobj *obj);
void func_8008C194(Gobj *obj);
void func_8008C058(Gobj *obj, s32 openerId);
void ObjectRemoveAndIgnoreBulletHoles(Gobj *obj);
void MovementAddGobj(Gobj *obj);
void PathSetPositionToPoint(Gobj *obj, s32 point);
void EGSFrameSetRotationYXZ(EGSFrame *frame, s32 arg1, VECTOR32 *rot);
void func_801487D4(Gobj *obj, s32 type);    /* in an overlay */
u8 PathObjectIsAtClosestPoint(Gobj *obj, Gobj *other, s32 arg2, s32 arg3, s32 arg4);
void func_80143424(Gobj *obj);              /* in an overlay */
void func_8014236C(Gobj *obj);              /* in an overlay */
void func_801422A0(s16 id);                 /* in an overlay */
void func_80148CB4(Gobj *obj, s16 type);    /* in an overlay */
s32 PathGetNumPoints(Gobj *obj);
u8 PathObjectIsAtPoint(Gobj *obj, s32 point);
void func_80148120(Gobj *obj);              /* in an overlay */
Gobj *ObjectMatchToRefIDByCatID(s16 id, s32 cat);
void EGSGeometry_GetFuzzyDistancePoint2Point(VECTOR32 *a, VECTOR32 *b, s32 *dist);
void EGSFrameSetOrientation(EGSFrame *frame, s32 arg1, VECTOR32 *dir, s32 arg3);
void KeyDroppedByNPC(Gobj *npc);
void iKeyRestore(Gobj *obj);

typedef struct SVECTOR16 {
    s16 vx, vy, vz;
} SVECTOR16;
void func_80148234(Gobj *obj, s16 type);    /* in an overlay */
void ObjectElectronicDestruction(Gobj *obj, s32 by);

extern void (*gObjectMovieDoneCallback)();
void InterfaceAddBomb(s16 id);
void NPCEnableGenerator(s16 id);
void NPCHazmatStop(s32 id);
void ObjectDetonateBomb(Gobj *obj);
void UseBeaconUpdate(Gobj *obj);
void WeaponRemove(s32 weapon);
void SysExeTickQueueTimer(void (*fn)(), s32 ticks, void *arg);
void iSwitchReactivate(Gobj *obj);

u8 PathHandleEx(Gobj *gobj, s32 scale, s32 orient, u8 pingPong, s16 first, s16 last, u8 *reachedEnd);
void SysSetSongVolPan(s32 arg0, s32 song, Gobj *obj, s32 vol, s32 pan);

extern VECTOR32 gObjPathScaleVector;
u8 WeaponHasMaxAmmo(s32 weapon);
void StringMaxedAmmoMsg(char *buf, s16 weapon, u8 arg2, u8 arg3);
void StringUndamagedFlakJacket(char *buf);

/* Weapon table, 0x20 bytes per weapon id; fields are addressed through these symbols. */
#define WEAPON_FIELD(sym, type, w) (*(type *)((u8 *)(sym) + ((w) << 5)))
extern u8 D_80105F55[]; /* rounds per clip */
extern u8 D_80105F56[]; /* pickup amount for single-item weapons */
extern u8 D_80105F58[]; /* flags; bits 0-2: clips per pickup - 1 */
extern u8 D_80105F64[]; /* s16 name message id */

extern s32 gCurrentWeapon;
extern s32 gExploDir;

void PickupAddUseMsg(s16 objId, s16 msgId);
void PickupRemoveUseMsg(s16 objId);
void UseObject(s16 objId);
void *UseSetComputerCallback(void (*cb)());

/* flags1 bits 0-6: countdown timer */
#define GOBJ_TIMER(obj) ((obj)->flags1 & 0x7F)
#define GOBJ_SET_TIMER(obj, t) ((obj)->flags1 = ((obj)->flags1 & 0x80) | ((t) & 0x7F))
#define GOBJ_WORD20(obj) (*(u32 *)&(obj)->unk20)
#define INFO_WORD24(info) (*(u32 *)&(info)->flags24)
#define INFO_PICKUP(info) (*(u8 *)&(info)->flags24) /* bit 7: armor, bits 0-5: weapon id */

void PickupInitData(void) {
    s32 i;

    for (i = 0; i < USE_MSG_MAX; i++) {
        D_80121978[i].textId = 0xFFFF;
        D_80121978[i].msgId = -1;
        D_80121978[i].objId = -1;
        D_80121990[i].objId = -1;
        D_80121990[i].armor = 0;
        D_80121990[i].weapon = 0;
        D_80121990[i].amount = 0;
        D_80121990[i].showMsg = 0;
    }
    D_801101BC = 0;
    D_801101C0 = 0;
    gLevelKeycards = 0;
    gLevelKeycardsTaken = 0;
    gNextTickAllowMsgLongText = gExeTick;
    for (i = 0; i < PICKUP_TAKEN_MSG_COUNT; i++) {
        D_801219B0[i] = 0xFFFF;
        D_801219E8[i] = 0;
    }
    D_801101B8 = 0;
    D_801101C4 = 0;
    D_801101C8 = 0;
    gDeviceDetonated = 0;
    SetLockedStringCallback(NULL);
    SetUseStringCallback(NULL);
    PickupSetKeyCallback(NULL);
    UseSetBombCallback(NULL);
    UseSetComputerCallback(NULL);
    UseSetLevSpecObjCallback(NULL);
    UseSetSwitchCallback(NULL);
}

#ifdef _PSYQ
__asm__("\t.bss\nD_80121978:\t.space\t24\nD_80121990:\t.space\t32\n"
        "D_801219B0:\t.space\t56\nD_801219E8:\t.space\t27\n\t.text");
#else
__asm__(".set D_80121978, D_80121978_bss\n"
        ".set D_8012197A, D_80121978_bss+0x2\n"
        ".set D_8012197C, D_80121978_bss+0x4\n"
        ".set D_80121990, D_80121978_bss+0x18\n"
        ".set D_80121992, D_80121978_bss+0x1A\n"
        ".set D_80121993, D_80121978_bss+0x1B\n"
        ".set D_80121994, D_80121978_bss+0x1C\n"
        ".set D_80121996, D_80121978_bss+0x1E\n"
        ".set D_801219B0, D_80121978_bss+0x38\n"
        ".set D_801219E8, D_80121978_bss+0x70\n"
        /* snddrv's SoundSeqCallback (INCLUDE_ASM) indexes its own array from this
         * folded address */
        ".globl D_801101C8\n");
#endif

void SetNextTickAllowMsgLongText(u32 tick) {
    if (gNextTickAllowMsgLongText < tick) {
        gNextTickAllowMsgLongText = tick;
    }
}

void *SetLockedStringCallback(void (*cb)()) {
    void *old = D_8010FCF8;
    D_8010FCF8 = cb;
    return old;
}

void *SetUseStringCallback(void (*cb)()) {
    void *old = D_8010FCFC;
    D_8010FCFC = cb;
    return old;
}

s32 UseObjectAlreadyInList(s16 objId) {
    s32 i;

    if (objId == -1) {
        return 0;
    }
    for (i = 0; i < D_801101BC; i++) {
        if (objId == D_80121978[i].objId) {
            return 1;
        }
    }
    return 0;
}

static void func_8008A550(s32 idx) {
    s32 i;
    s32 j;

    for (D_801101BC--, j = idx, i = idx + 1; j < D_801101BC; j++, i++) {
        D_80121978[j].textId = D_80121978[i].textId;
        D_80121978[j].msgId = D_80121978[i].msgId;
        D_80121978[j].objId = D_80121978[i].objId;
    }
    D_80121978[D_801101BC].textId = 0xFFFF;
    D_80121978[D_801101BC].msgId = -1;
    D_80121978[D_801101BC].objId = -1;
}

/* global until PickupObjectTake, DOORObjDispatch and ELEVATORObjDispatch are C */
void func_8008A62C(s32 idx) {
    s32 i;
    s32 j;

    D_801101C0--;
    if (idx < D_801101C0) {
        j = idx;
        i = idx + 1;
        do {
            D_80121990[j].objId = D_80121990[i].objId;
            D_80121990[j].armor = D_80121990[i].armor;
            D_80121990[j].weapon = D_80121990[i].weapon;
            D_80121990[j].amount = D_80121990[i].amount;
            D_80121990[j].showMsg = D_80121990[i].showMsg;
            j++;
            i++;
        } while (j < D_801101C0);
    }
    D_80121990[D_801101C0].objId = -1;
    D_80121990[D_801101C0].armor = 0;
    D_80121990[D_801101C0].weapon = 0;
    D_80121990[D_801101C0].amount = 0;
    D_80121990[D_801101C0].showMsg = 0;
}

void PickupAddUseMsg(s16 objId, s16 msgId) {
    char *str = NULL;
    s32 i;
    Gobj *obj;
    s32 type;
    u8 state;

    for (i = 0; i < D_801101BC; i++) {
        if (objId == D_80121978[i].objId) {
            return;
        }
    }
    if (D_801101BC < USE_MSG_MAX) {
        D_80121978[D_801101BC].objId = objId;
        D_80121978[D_801101BC].msgId = msgId;
        obj = gameObjectsInfo[objId].runtime;
        type = GOBJ_TYPE(objId);
        state = obj->npcState;
        if (state != 0xB) {
            if ((state != 0xD || (*(u32 *)obj & 0xA) != 0xA) &&
                (gPlayerFlashlight ||
                 (type != 0x78 && type != 0x57 && type != 0x79 && type != 0x7A && type != 0x7B && type != 0x7C))) {
                if (state != 9 || !(obj->flags0 & 0x10)) {
                    if (D_8010FCFC != NULL) {
                        str = D_8010FCFC(objId, msgId, obj);
                    }
                } else if (D_8010FCF8 != NULL) {
                    str = D_8010FCF8(objId, GOBJ_TYPE(objId), obj);
                }
            } else {
                str = D_8010FCEC;
            }
        } else if (!gPlayerFlashlight && type == 0x4F) {
            str = D_8010FCEC;
        }
        if (str == NULL) {
            str = StringSetUseMsg(msgId);
        }
        D_80121978[D_801101BC].textId = strlen(str) ? TextAddMarker(objId, str, -1, 0) : 0xFFFF;
        D_801101BC++;
    }
}

void PickupRemoveUseMsg(s16 objId) {
    s32 i;
    s16 type;

    for (i = 0; i < D_801101BC; i++) {
        if (objId == D_80121978[i].objId) {
            if (D_80121978[i].textId != 0xFFFF) {
                TextRemoveString(D_80121978[i].textId);
            }
            func_8008A550(i);
            break;
        }
    }
    type = GOBJ_TYPE(objId);
    if (type == 0x4F || type == 0x50 || type == 0x2C) {
        for (i = 0; i < D_801101C0; i++) {
            if (objId == D_80121990[i].objId) {
                func_8008A62C(i);
                return;
            }
        }
    }
}

void UseMsgUpdate4FlashlightChange(void) {
    s32 i;
    s32 type;

    for (i = 0; i < D_801101BC; i++) {
        type = GOBJ_TYPE(D_80121978[i].objId);
        if (type == 0x4F || type == 0x78 || type == 0x57 || type == 0x79 || type == 0x7A || type == 0x7B ||
            type == 0x7C) {
            s32 objId = D_80121978[i].objId;
            s32 msgId = D_80121978[i].msgId;
            PickupRemoveUseMsg(objId);
            PickupAddUseMsg(objId, msgId);
        }
    }
}

u8 PickupGetData(Gobj *obj) {
    u8 data = INFO_PICKUP(&gameObjectsInfo[obj->id]);

    if (data == 0) {
        s32 kind = INFO_WORD24(&gameObjectsInfo[obj->id]) & 0x3000;
        if (kind != 0x1000) {
            data = (kind == 0x2000) ? 0x14 : 0;
        } else {
            data = 0x13;
        }
    }
    if (obj->id == gPlayerOffset || (INFO_WORD24(&gameObjectsInfo[obj->id]) & 0x4000)) {
        data |= 0x80;
    }
    return data;
}

void PickupSetDataIDs(u8 data, s16 *armor, s16 *weapon) {
    if (data & 0x80) {
        *armor = 1;
    } else {
        *armor = 0;
    }
    *weapon = data & 0x3F;
}

void PickupObjectAddUseMsgs(Gobj *obj, u8 data) {
    s16 armor;
    s16 weapon;
    s32 amount;

    PickupSetDataIDs(data, &armor, &weapon);
    amount = 0;
    if (armor) {
        PickupAddUseMsg(obj->id, D_8010FCF0[armor]);
    }
    if (weapon) {
        PickupAddUseMsg(obj->id, WEAPON_FIELD(D_80105F64, s16, weapon));
        if (weapon != 0x13 && weapon != 0x14 && weapon != 0x10) {
            amount = WEAPON_FIELD(D_80105F55, u8, weapon) * ((WEAPON_FIELD(D_80105F58, s32, weapon) & 7) + 1);
        } else {
            amount = WEAPON_FIELD(D_80105F56, u8, weapon);
        }
    }
    PickupObjectSet(obj->id, armor, weapon, amount, 1);
}

void UseObject(s16 objId) {
    u8 found = 0;
    s32 i;

    for (i = D_801101BC - 1; i >= 0; i--) {
        if (objId == D_80121978[i].objId) {
            found = 1;
            break;
        }
    }
    if (found) {
        for (i = D_801101BC - 1; i >= 0; i--) {
            s32 to = D_80121978[i].objId;
            u8 msg = 0x27;
            s32 from;
            u8 prio;

            if (objId != (s16)to) {
                prio = 3;
                from = gPlayerOffset;
            } else {
                PickupRemoveUseMsg(objId);
                msg = 0x19;
                prio = 3;
                to = gPlayerOffset;
                from = objId;
            }
            SysPostMessage(msg, prio, from, to, 0, 0, 0, 0);
        }
    }
}

void PickUpInventory(s16 objId) {
    Gobj *obj = gameObjectsInfo[objId].runtime;

    if (!(obj->flags0 & 0x20)) {
        obj->flags0 |= 0x20;
        UseObject(objId);
        if (LevelGameObjectHasModel(obj->id) && (obj->anim->unk00[0xA] & 8) && obj->npcState == 0xB) {
            EGSDisplayRemoveDamage(obj);
            EGSAnimShowDetailDef(obj->anim);
        }
    }
}

void PickupObjectSet(s16 objId, u8 armor, u8 weapon, s16 amount, u8 showMsg) {
    if (gObjPlayer->healthInfo->health > 0 && D_801101C0 < PICKUP_PENDING_MAX) {
        D_80121990[D_801101C0].objId = objId;
        D_80121990[D_801101C0].armor = armor;
        D_80121990[D_801101C0].weapon = weapon;
        D_80121990[D_801101C0].amount = amount;
        D_80121990[D_801101C0].showMsg = showMsg;
        D_801101C0++;
    }
}

void iPickupObjectTakenMsg(char *msg, s32 idx) {
    u8 show;

    if (TextIsStringShowing(D_801219B0[idx])) {
        if (D_801219E8[idx] == TextComputeChecksum(msg)) {
            TextChangeStringDuration(D_801219B0[idx], strlen(msg) * 2 + 0x18);
            show = 0;
        } else {
            TextRemoveString(D_801219B0[idx]);
            show = 1;
        }
    } else {
        show = 1;
    }
    if (show) {
        D_801219B0[idx] = TextStatusMessage(msg, 0);
        D_801219E8[idx] = TextComputeChecksum(msg);
    }
}


















s32 PickupObjectIsNotMaxed(s32 objId) {
    char flakMsg[0x40];
    char ammoMsg[0x40];
    s16 armor;
    s16 weapon;
    s32 ammoType;
    s32 notMaxed = 1;
    Gobj *obj = gameObjectsInfo[objId].runtime;

    if (gObjPlayer->healthInfo->health <= 0) {
        return 0;
    }
    PickupSetDataIDs(obj->flags1, &armor, &weapon);
    ammoType = 1;
    if (weapon != 2) {
        ammoType = weapon;
    }
    if (ammoType != 0 && WeaponHasMaxAmmo(ammoType)) {
        StringMaxedAmmoMsg(ammoMsg, WEAPON_FIELD(D_80105F64, s16, weapon), ammoType != 0x13 && ammoType != 0x14,
                           ammoType != 6 && ammoType != 7);
        notMaxed = 0;
        iPickupObjectTakenMsg(ammoMsg, weapon);
    }
    if (armor == 1 && gObjPlayer->healthInfo->armor >= 600) {
        StringUndamagedFlakJacket(flakMsg);
        iPickupObjectTakenMsg(flakMsg, 0x1A);
        notMaxed = 0;
    }
    return notMaxed;
}

#ifdef NON_MATCHING
/* logic complete; loop induction variables and the manual/auto test layout differ (369 vs 388 insns) */
/* Take the pickups the player is standing on (auto = from walking over them, else on "use"). */
u8 PickupObjectTake(u8 manual) {
    VECTOR32 pos;
    char flakMsg[0x40];
    char takenMsg[0x40];
    u8 took;
    u8 msgShown;
    u8 any;
    char *msg;
    s32 i;
    PickupPending *p;
    s32 type;
    u8 consumed;
    u8 armorTaken;
    s32 weapon;
    u32 *words;
    s32 bit;
    s32 misalign;
    s32 added;
    u8 notGrenade;
    u8 notPistol;
    s16 key;

    any = 0;
    if (gObjPlayer->healthInfo->health <= 0) {
        return 0;
    }
    msg = flakMsg;
    for (i = D_801101C0 - 1; i >= 0; i--) {
        p = &D_80121990[i];
        if (manual ? (p->objId == -1 || !(gameObjectsInfo[p->objId].runtime->flags0 & 0x20))
                   : !p->showMsg) {
            armorTaken = 0;
            took = 0;
            type = p->objId != -1 ? GOBJ_TYPE(p->objId) : -1;
            consumed = 0;
            if (type == -1 || gameObjectsInfo[p->objId].runtime->npcState == 2 || type == 0x63) {
                consumed = 1;
            }
            weapon = 1;
            if (p->weapon != 2) {
                weapon = p->weapon;
            }
            if (weapon != 0) {
                words = (u32 *)gWeaponsAvailable;
                bit = weapon;
                misalign = (u32)words & 3;
                if (misalign) {
                    bit = weapon + misalign * 8;
                    words = (u32 *)((u32)words & ~3);
                }
                if (words[bit >> 5] & (1 << (bit & 0x1F))) {
                    notGrenade = weapon != 0x13 && weapon != 0x14;
                    notPistol = weapon != 6 && weapon != 7;
                    added = WeaponAddAmmo(weapon, p->amount);
                    if (added != 0) {
                        took = 1;
                        StringTakeAmmoMsg(takenMsg, added, WEAPON_FIELD(D_80105F64, s16, p->weapon), notGrenade,
                                          notPistol);
                    } else {
                        StringMaxedAmmoMsg(takenMsg, WEAPON_FIELD(D_80105F64, s16, p->weapon), notGrenade, notPistol);
                    }
                } else {
                    took = 1;
                    WeaponAdd(weapon, p->amount);
                    StringTakeObjectMsg(takenMsg, p->amount, WEAPON_FIELD(D_80105F64, s16, p->weapon));
                    if (weapon == 0x12) {
                        SysPlayXA(0xA3, 0, 1);
                    }
                }
                iPickupObjectTakenMsg(takenMsg, p->weapon);
            }
            switch (p->armor) {
            case 0:
                break;
            case 1:
                if (gObjPlayer->healthInfo->armor < 600) {
                    armorTaken = 1;
                    StringTakeObjectMsg(flakMsg, 1, 0x18);
                    if (consumed) {
                        GameObjectHealth *health = gObjPlayer->healthInfo;
                        s32 armor = health->armor + 250;

                        if (armor > 600) {
                            armor = 600;
                        }
                        health->armor = armor;
                    } else {
                        gObjPlayer->healthInfo->armor = 600;
                    }
                } else {
                    StringUndamagedFlakJacket(flakMsg);
                }
                iPickupObjectTakenMsg(flakMsg, 0x1A);
                break;
            case 2:
#line 825
                ASSERT(p->objId != -1);
                key = gameObjectsInfo[gameObjectsInfo[p->objId].runtime->id].refId;
                armorTaken = 1;
#line 828
                ASSERT(key != -1);
                StringTakeObjectMsg(msg, 1, 0x19);
                WeaponAdd(0x17, 0);
                if (key != -1) {
                    SysPostMessage(0x1B, 5, p->objId, key, 0, 0, 0, 0);
                }
                TextStatusMessage(msg, 0);
                break;
            }
            if (took || armorTaken) {
                if (manual && p->objId != -1) {
                    PickUpInventory(p->objId);
                }
                any = 1;
            }
            if (consumed) {
                func_8008A62C(i);
            }
        }
    }
    if (any) {
        pos.vx = gObjPlayer->anim->frame->pos.vx;
        pos.vy = gObjPlayer->anim->frame->pos.vy;
        pos.vz = gObjPlayer->anim->frame->pos.vz;
        pos.vy = -pos.vy;
        SysPlaySong(1, 0x14, 0, (s32)&pos);
    }
    return any;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/pickup", PickupObjectTake);
#endif

void *PickupSetKeyCallback(void (*cb)()) {
    void *old = D_8010FD00;
    D_8010FD00 = cb;
    return old;
}


s32 UseKey(void) {
    s32 i;

    for (i = 0; i < D_801101BC; i++) {
        if (D_80121978[i].msgId == 4 &&
            ObjectMatchToRefID(D_80121978[i].objId, 0x63)->id != D_80121978[i].objId) {
            PlayerUseCAS((s16)gPlayerOffset, 0x22);
            return 1;
        }
    }
    return 0;
}

void KeyDroppedByNPC(Gobj *npc) {
    VECTOR32 pos;
    Gobj *key;
    GameObjectInfo *info;
    s32 ref = gameObjectsInfo[npc->id].refId;

    if (ref != -1) {
        key = ObjectMatchToRefID(ref, 0x63);
        if (key->id != npc->id && !(*(u32 *)key & 0x22)) {
            key->flags0 |= 2;
            gameObjectsInfo[key->id].matrix = *(GOBJ_MATRIX *)npc->anim->frames[2];
            info = &gameObjectsInfo[key->id];
            info->matrix.m[0][0] = 0;
            info->matrix.m[2][0] = 0;
            info->matrix.m[1][1] = 0;
            info->matrix.m[1][2] = 0;
            MatrixNormal_0(&info->matrix, &info->matrix);
            info->matrix.t[1] = -(npc->move->unk12C + 0x28);
            key->anim->frame = (EGSFrame *)&info->matrix;
            pos.vx = info->matrix.t[0];
            pos.vy = -info->matrix.t[1];
            pos.vz = info->matrix.t[2];
            EGSFrameSetPosition(key->anim->frame, 0, &pos);
            key->anim->unk00[8] &= ~8;
            SysPostMessage(6, 5, key->id, key->id, 0, 0, 0, 0);
        }
    }
}

void iKeyRestore(Gobj *obj) {
    GameObjectInfo *infos;
    Gobj *key;

    key = obj;
    infos = gameObjectsInfo;
    if (infos[infos[key->id].refId].runtime->flags0 & 0x10) {
        key->flags0 &= ~0x20;
        return;
    }
    key->flags0 |= 0x20;
    gLevelKeycardsTaken++;
}

void UseBeaconUpdate(Gobj *obj) {
    Gobj *light = iObjectFindNearbyLight(obj, 0x6F);

    if (light != NULL) {
        if (!(GOBJ_WORD20(obj) & 0x0A000000) && (obj->flags0 & 0x20)) {
            light->flags0 |= 0xA;
            light->healthInfo->health = light->healthInfo->armor;
            light->flags23 &= ~2;
        } else {
            light->flags0 &= ~0xA;
            light->healthInfo->health = -1;
            light->flags23 |= 2;
        }
    }
}

void ObjectBombDamagedCallback(s16 id, s16 by) {
    if (!D_801101C4) {
        if (!(gameObjectsInfo[id].runtime->flags1 & 0x80) && id != GOBJ_ID_666 &&
            gameObjectsModelInfo[gameObjectsInfo[id].modelIndex].type == GOBJ_TYPE_BOMB) {
            D_801101C4 = 1;
            SysPlayXA(0x81, 0, 1);
        }
    }
}

void ObjectBombDeath(s32 reason, s32 xa, s32 secondary, s32 id) {
    s32 track = xa;
    s32 r = reason;
    s32 sec = secondary;

    if (!D_801101C8) {
        D_801101C8 = 1;
        if (SysSoundIsXAPlaying()) {
            SysStopXA();
        }
    } else {
        track = -1;
    }
    gDeviceDetonated = 1;
    if (!gMissionFailed) {
        gMissionFailed = 1;
        LevelMissionFailed((s16)r, track, sec);
    }
    DamageKillAllPeople(gPlayerOffset, 0x12);
}

void ObjectDetonateBomb(Gobj *obj) {
    VECTOR32 pos;

    pos.vx = obj->anim->frame->pos.vx;
    pos.vy = obj->anim->frame->pos.vy;
    pos.vz = obj->anim->frame->pos.vz;
    pos.vy = -pos.vy;
    ParticleSmoke(-1, &pos, &gExploDir, 0x1000, 1);
    InterfaceDeleteBomb();
    PickupRemoveUseMsg(obj->id);
    if (obj->healthInfo->health > 0) {
        obj->healthInfo->health = 0;
    }
    if (obj->anim->unk00[0xA] & 8) {
        EGSAnimShowDetailDef(obj->anim);
    }
}

void *UseSetBombCallback(void (*cb)()) {
    void *old = D_8010FD04;
    D_8010FD04 = cb;
    return old;
}

void *UseSetComputerCallback(void (*cb)()) {
    void *old = D_8010FD08;
    D_8010FD08 = cb;
    return old;
}



s32 IsWideDoor(Gobj *obj) {
    s32 *box = (s32 *)obj->anim->model;
    s32 wide = 0;

    if (box[4] - box[0] > 0xE0 || box[6] - box[2] > 0xE0) {
        wide = 1;
    }
    return wide;
}

/* Door: set flags0 bit 1 when the player (or the opening NPC) is behind it. */
void func_8008C058(Gobj *obj, s32 openerId) {
    VECTOR32 dir;
    VECTOR32 pos;
    VECTOR32 opener;
    EGSFrame *frame = obj->anim->frame;
    Gobj *who;

    dir.vx = frame->m[0][2];
    dir.vy = frame->m[1][2];
    dir.vz = frame->m[2][2];
    dir.vy = -dir.vy;
    pos.vx = frame->pos.vx;
    pos.vy = frame->pos.vy;
    pos.vz = frame->pos.vz;
    pos.vy = -pos.vy;
    who = gameObjectsInfo[openerId].runtime;
    if (who->npcState != 1 && who->npcState != 2) {
        who = gObjPlayer;
    }
    opener = who->move->pos;
    if ((opener.vx - pos.vx) * dir.vx + (opener.vz - pos.vz) * dir.vz > 0) {
        obj->flags0 &= ~2;
    } else {
        obj->flags0 |= 2;
    }
}

void func_8008C194(Gobj *obj) {
    VECTOR32 pos;
    s32 type = GOBJ_TYPE(obj->id);

    pos.vx = obj->anim->frame->pos.vx;
    pos.vy = obj->anim->frame->pos.vy;
    pos.vz = obj->anim->frame->pos.vz;
    pos.vy = -pos.vy;
    if (type != 7 && type != 0x3B) {
        if (!(obj->flags1 & 0x80)) {
            SysPlaySong(0, 0x1E, 0, (s32)&pos);
            if (obj->flags0 & 0x40) {
                SysPlaySong(0, 0x15, (s32)obj, 0);
            }
        } else {
            SysPlaySong(0, 0x13, 0, (s32)&pos);
        }
    } else {
        SysPlaySong(0, 0x1E, 0, (s32)&pos);
        SysPlaySong(0, 0x15, (s32)obj, 0);
    }
}

void func_8008C2FC(Gobj *obj) {
    s32 type = GOBJ_TYPE(obj->id);

    if (type == 7 || type == 0x3B || (*(u32 *)obj & 0x8040) == 0x40) {
        SysStopSong(0, 0x15);
    }
}

#ifdef NON_MATCHING
/* block layout: the original enters the loop at its test and places the LevelDoEOL exit first */
/* Elevator: walk the linked objects for the next floor's object (type 0xE ends the level). */
s32 func_8008C3A4(Gobj *obj) {
    s16 floors = 1;
    s32 ref = (u16)obj->id;
    Gobj *rt;
    s16 type;

    while ((s16)(ref = gameObjectsInfo[(s16)ref].refId) != -1) {
        type = GOBJ_TYPE((s16)ref);
        if (type == 0xE) {
            LevelDoEOL(0);
            return 1;
        }
        if (type != 0x22) {
            if (++floors >= 3) {
                return 0;
            }
            rt = gameObjectsInfo[(s16)ref].runtime;
            if (rt == NULL || rt->npcState != obj->npcState) {
                return 0;
            }
        }
    }
    return 0;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/pickup", func_8008C3A4);
#endif

/* Door opens: the position computed here is never used. */
void func_8008C534(Gobj *obj, s32 type) {
    VECTOR32 pos;

    pos.vx = obj->anim->frame->pos.vx;
    pos.vy = obj->anim->frame->pos.vy;
    pos.vz = obj->anim->frame->pos.vz;
    pos.vy = -pos.vy;
    SysPlaySong(0, 0x15, (s32)obj, 0);
    SysPlaySong(0, 0x11, (s32)obj, 0);
    obj->flags0 |= 8;
    func_8008E418(obj, type);
}

#ifdef NON_MATCHING
/* regalloc: the original loads the pickup byte into v0 and copies it to a2 */
s32 UseLevelSpecificObject(void) {
    s32 data;
    s32 i;
    s32 type;

    if (gObjPlayer->id != gPlayerOffset) {
        GameObjectInfo *info = &gameObjectsInfo[gObjPlayer->id];

        data = INFO_PICKUP(info);
        if (data == 0) {
            s32 kind = INFO_WORD24(info) & 0x3000;
            if (kind != 0x1000) {
                data = (kind == 0x2000) ? 0x14 : 0;
            } else {
                data = 0x13;
            }
        }
    } else {
        data = gCurrentWeapon;
    }
    for (i = 0; i < D_801101BC; i++) {
        if (D_80121978[i].msgId == 3) {
            type = GOBJ_TYPE(D_80121978[i].objId);
            if ((data == 0x19 && type == 0x5C) || (data == 0x18 && type == 0x58)) {
                PlayerUseCAS((s16)gPlayerOffset, 0x22, data);
                return 1;
            }
        }
    }
    return 0;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/pickup", UseLevelSpecificObject);
#endif

void *UseSetLevSpecObjCallback(void (*cb)()) {
    void *old = gObjectInteractionCallback;
    gObjectInteractionCallback = cb;
    return old;
}

void *UseSetSwitchCallback(void (*cb)()) {
    void *old = D_8010FD0C;
    D_8010FD0C = cb;
    return old;
}



void iSwitchReactivate(Gobj *obj) {
    if (obj != NULL && (obj->flags0 & 0x40)) {
        obj->flags0 &= ~0x20;
        PickupAddUseMsg(obj->id, 4);
        SysPostMessage(0x14, 3, obj->id, gPlayerOffset, 0, 0, 0, 0);
    }
}

INCLUDE_RODATA("asm/main/nonmatchings/pickup", D_80012DE0);



void AUTOPICKUPObjDispatch(SysMsg *msg) {
    Gobj *key = gameObjectsInfo[msg->to].runtime;
    s32 ref;
    Gobj *npc;

    switch (msg->id) {
    case 10: {
        VECTOR32 playerPos;
        VECTOR32 keyPos;
        s32 dist;

        if ((*(u32 *)key & 0x62) == 0x42) {
            playerPos = gObjPlayer->move->pos;
            keyPos.vx = key->anim->frame->pos.vx;
            keyPos.vy = key->anim->frame->pos.vy;
            keyPos.vz = key->anim->frame->pos.vz;
            keyPos.vy = -keyPos.vy;
            EGSGeometry_GetFuzzyDistancePoint2Point(&playerPos, &keyPos, &dist);
            if (dist <= 0xC0) {
                key->flags0 = (key->flags0 & ~2) | 0x20;
                key->anim->unk00[8] |= 8;
                PickupObjectSet(key->id, 2, 0, 0, 0);
                PickupObjectTake(0);
                gLevelKeycardsTaken++;
                if (D_8010FD00 != NULL) {
                    D_8010FD00(key->id);
                }
            } else {
                SysPostMessage(0xA, 4, key->id, key->id, 0, 0, 0, 0);
            }
        }
        break;
    }
    case 41: {
        SVECTOR16 saved;
        VECTOR32 pos;
        VECTOR32 up;

        ref = gameObjectsInfo[key->id].refId;
        if (ref != -1) {
            npc = ObjectMatchToRefIDByCatID(ref, 2);
            if (key->id != npc->id && !(*(u32 *)key & 0x22) && npc->healthInfo->health <= 0) {
                KeyDroppedByNPC(npc);
            }
        }
        if ((*(u32 *)key & 0x22) == 2) {
            pos.vx = key->anim->frame->pos.vx;
            pos.vy = key->anim->frame->pos.vy;
            pos.vz = key->anim->frame->pos.vz;
            pos.vy = -pos.vy;
            saved.vx = pos.vx;
            saved.vy = pos.vy;
            saved.vz = pos.vz;
        } else {
            saved.vx = 0x7FFF;
            saved.vy = 0x7FFF;
            saved.vz = 0x7FFF;
        }
        *(s32 *)msg->data[0] = (u16)saved.vx | ((u16)saved.vy << 16);
        *(s16 *)msg->data[1] = saved.vz;
        break;
    case 42:
        saved.vx = ((s16 *)msg->data[0])[0];
        saved.vy = ((s16 *)msg->data[0])[1];
        saved.vz = *(s16 *)msg->data[1];
        if (saved.vx == 0x7FFF && saved.vy == saved.vx && saved.vz == saved.vy) {
            key->flags0 &= ~2;
            if (gameObjectsInfo[key->id].refId != -1) {
                SysExeTickQueueTimer(iKeyRestore, 1, key);
            }
            key->anim->unk00[8] |= 8;
            break;
        }
        up.vx = 0;
        up.vy = 0x1000;
        up.vz = 0;
        EGSFrameSetOrientation(key->anim->frame, 0, &up, 0);
        pos.vx = saved.vx;
        pos.vy = saved.vy;
        pos.vz = saved.vz;
        EGSFrameSetPosition(key->anim->frame, 0, &pos);
        key->flags0 = (key->flags0 | 2) & ~0x20;
        key->anim->unk00[8] &= ~8;
    }
    case 6:
        key->flags0 |= 0x40;
        if ((*(u32 *)key & 0x22) == 2) {
            SysPostMessage(0xA, 4, key->id, key->id, 0, 0, 0, 0);
        }
        break;
    case 5:
        key->flags0 &= ~0x40;
        break;
    case 2:
        func_80148120(key);
        break;
    }
}

void BOMBObjDispatch(SysMsg *msg) {
    VECTOR32 pos;
    Gobj *bomb = gameObjectsInfo[msg->to].runtime;
    u8 byPlayer;
    u8 usable;

    switch (msg->id) {
    case 6:
        CobjAddBulletCollisionBoxGobj(bomb, 1, 0, 0);
        CobjAddCollisionBoxGobj(bomb, 1, 0, 1, 0);
        if (!(bomb->flags1 & 0x80) && bomb->healthInfo->health > 0) {
            InterfaceAddBomb(bomb->id);
        }
        break;
    case 5:
        CobjRemoveBulletCollisionBoxGobj(bomb);
        CobjRemoveCollisionBoxGobj(bomb);
        if (!(bomb->flags1 & 0x80) && bomb->healthInfo->health > 0) {
            InterfaceDeleteBomb();
        }
        break;
    case 13:
        if (!(bomb->flags1 & 0x80) && bomb->healthInfo->health <= 0) {
            ObjectDetonateBomb(bomb);
        }
        break;
    case 18:
        if (!(bomb->flags0 & 0x20) && bomb->healthInfo->health > 0) {
            PickupAddUseMsg(bomb->id, 0);
            SysPostMessage(0x24, 4, bomb->id, gPlayerOffset, 0, 0, 0, 0);
        }
        break;
    case 19:
        if (!(bomb->flags0 & 0x20)) {
            PickupRemoveUseMsg(bomb->id);
            SysPostMessage(0x19, 3, bomb->id, gPlayerOffset, 0, 0, 0, 0);
        }
        break;
    case 20:
        if (!(bomb->flags1 & 0x80)) {
            s16 ref = gameObjectsInfo[bomb->id].refId;

            if (ref != -1) {
                gameObjectsInfo[ref].runtime->flags23 &= ~1;
                NPCEnableGenerator(ref);
            }
        }
        break;
    case 36:
    case 39:
        byPlayer = msg->from == gPlayerOffset;
        if (byPlayer) {
            usable = !(bomb->flags0 & 0x20);
        } else {
            usable = !(bomb->flags1 & 0x80);
        }
        if (usable) {
            if (byPlayer) {
                bomb->flags0 |= 0x20;
                UseBeaconUpdate(bomb);
                UseObject(bomb->id);
            } else {
                bomb->flags1 |= 0x80;
                bomb->flags23 |= 8;
                UseBeaconUpdate(bomb);
                NPCHazmatStop(msg->from);
                InterfaceDeleteBomb();
                if (bomb->anim->unk00[0xA] & 8) {
                    EGSAnimShowDetailDef(bomb->anim);
                }
            }
            pos.vx = bomb->anim->frame->pos.vx;
            pos.vy = bomb->anim->frame->pos.vy;
            pos.vz = bomb->anim->frame->pos.vz;
            pos.vy = -pos.vy;
            SysPlaySong(1, 0x14, 0, (s32)&pos);
            if (bomb->healthInfo->health > 0 && D_8010FD04 != NULL) {
                D_8010FD04(bomb->id, (s16)msg->from);
            }
        }
        break;
    case 41:
        *(s32 *)msg->data[0] = (bomb->flags23 >> 1) & 1;
        *(s16 *)msg->data[1] = bomb->flags0 | (bomb->flags1 << 8);
        break;
    case 42:
        bomb->flags0 = ((u8 *)msg->data[1])[0];
        bomb->flags1 = ((u8 *)msg->data[1])[1];
        if (bomb->flags1 & 0x80) {
            bomb->flags23 |= 2;
            if ((bomb->flags1 & 0x80) && (bomb->anim->unk00[0xA] & 8)) {
                EGSAnimShowDetailDef(bomb->anim);
            }
        }
        break;
    case 2:
        func_8014819C(bomb);
        break;
    }
}

void COMPUTERObjDispatch(SysMsg *msg) {
    VECTOR32 pos;
    Gobj *pc = gameObjectsInfo[msg->to].runtime;
    s32 type = GOBJ_TYPE(pc->id);
    u8 result;
    char *str;

    switch (msg->id) {
    case 6:
        pc->flags0 |= 0x40;
        CobjAddBulletCollisionBoxGobj(pc, 1, 0, 0);
        if (!(pc->flags0 & 1)) {
            CobjAddCollisionBoxGobj(pc, 1, 0, 1, 1);
        }
        break;
    case 5:
        pc->flags0 &= ~0x40;
        CobjRemoveBulletCollisionBoxGobj(pc);
        if (!(pc->flags0 & 1)) {
            CobjRemoveCollisionBoxGobj(pc);
        }
        break;
    case 18:
        if (!(pc->flags0 & 0x20)) {
            PickupAddUseMsg(pc->id, 1);
            if (!(pc->flags0 & 8)) {
                SysPostMessage(0x15, 4, pc->id, gPlayerOffset, 0, 0, 0, 0);
            }
        }
        break;
    case 19:
        if (!(pc->flags0 & 0x20)) {
            PickupRemoveUseMsg(pc->id);
            if (!(pc->flags0 & 8)) {
                SysPostMessage(0x19, 3, pc->id, gPlayerOffset, 0, 0, 0, 0);
            }
        }
        break;
    case 41:
        *(s32 *)msg->data[0] = pc->flags0;
        break;
    case 42:
        pc->flags0 = *(s32 *)msg->data[0];
    case 13:
        if (pc->healthInfo->health <= 0) {
            if (!(pc->flags0 & 0x80)) {
                pc->flags0 |= 0x80;
                ObjectElectronicDestruction(pc, msg->from);
            } else if (pc->anim->unk00[0xA] & 8) {
                EGSAnimShowDetailDef(pc->anim);
            }
        }
        break;
    case 20:
    case 27:
        if (pc->flags0 & 0x10) {
            pc->flags0 &= ~0x10;
        }
        break;
    case 21:
    case 39:
        if (!(pc->flags0 & 0x10)) {
            if (!(pc->flags0 & 0x20)) {
                pc->flags0 |= 0x20;
                result = 1;
                if (D_8010FD08 != NULL) {
                    result = D_8010FD08(pc->id, type);
                }
                pos.vx = pc->anim->frame->pos.vx;
                pos.vy = pc->anim->frame->pos.vy;
                pos.vz = pc->anim->frame->pos.vz;
                pos.vy = -pos.vy;
                SysPlaySong(0, 0xE, 0, (s32)&pos);
                if (result == 1) {
                    ObjectRefMultiSYSPOSTMSG0(pc->id, 0x15, 3);
                }
                UseObject(pc->id);
            }
        } else if (D_8010FCF8 != NULL) {
            str = D_8010FCF8(pc->id, type);
            if (str != NULL) {
                TextStatusMessage(str, 0);
            }
        }
        break;
    case 45:
        if (gObjectMovieDoneCallback != NULL && (pc->flags0 & 0x20)) {
            gObjectMovieDoneCallback(pc->id, type);
        }
        break;
    case 2:
        func_80148234(pc, type);
        break;
    }
}

#ifdef NON_MATCHING
/* control flow written out; tail sharing of the open/close finish differs */
/* Per-tick door movement: sliding/path doors, lifts, and the swinging panel (type 0x3E). */
void func_8008D548(Gobj *door, s32 type) {
    switch (type) {
    case 0x60:
    case 0x71:
    case 0x76:
    case 0x49:
    case 0xA:
    case 0x6:
        if (door->move != NULL) {
            u8 done;

            PathHandleEx(door, 0, 1, 1, -1, -1, &done);
            if (!done) {
                break;
            }
            door->flags0 = (door->flags0 ^ 1) & ~8;
        } else {
            VECTOR32 step = { 0, (door->flags0 & 2) ? 0xAA : -0xAA, 0 };
            VECTOR32 rot;
            u8 wide;

            rot = step;
            wide = IsWideDoor(door);
            if (wide) {
                rot.vy = (door->flags0 & 2) ? 0x44 : -0x44;
            }
            EGSFrameRotateLocalYXZ(door->anim->frame, &rot);
            GOBJ_SET_TIMER(door, GOBJ_TIMER(door) - 1);
            if (GOBJ_TIMER(door)) {
                break;
            }
            door->flags0 = (door->flags0 ^ 3) & ~8;
            GOBJ_SET_TIMER(door, 0);
            door->flags1 |= wide ? 0xF : 6;
        }
        if (!(door->flags1 & 0x80)) {
            door->unk20[0] |= 0x80;
        }
        func_8008C2FC(door);
        func_8008C3A4(door);
        return;
    case 0x7:
    case 0x3B:
        if (door->move != NULL) {
            u8 done;

            if (!GOBJ_TIMER(door)) {
                PathHandleEx(door, 0, 1, 1, -1, -1, &done);
                if (!done) {
                    break;
                }
                door->flags0 ^= 5;
                if (door->flags0 & 1) {
                    done = 0;
                    GOBJ_SET_TIMER(door, 0x48);
                } else {
                    door->flags0 &= ~8;
                    if (!(door->flags1 & 0x80)) {
                        door->unk20[0] |= 0x80;
                    }
                    func_8008C3A4(door);
                }
                func_8008C2FC(door);
            } else {
                GOBJ_SET_TIMER(door, GOBJ_TIMER(door) - 1);
                if (!GOBJ_TIMER(door)) {
                    func_8008C194(door);
                }
                done = 0;
            }
            if (done) {
                return;
            }
            break;
        }
        return;
    case 0x3E: {
        VECTOR32 rot = door->move->rot;
        VECTOR32 vel;
        u8 done = 0;

        if ((door->flags1 & 7) == 2) {
            if (rot.vx > 0) {
                rot.vy = 0;
                rot.vz = 0;
                rot.vx = 0xAAAAA;
            } else {
                rot.vy = 0;
                rot.vz = 0;
                rot.vx = -0x44444;
                door->move->unk13C = 0xA66;
                door->move->unkE0 = D_80107840;
                door->flags1 = (door->flags1 & ~7) | 3;
            }
            door->move->unkF0.vx += rot.vx;
            door->move->unkF0.vy += rot.vy;
            door->move->unkF0.vz += rot.vz;
        } else if ((door->flags1 & 7) == 3) {
            if (rot.vx >= 0xE4) {
                vel = door->move->unkA0;
                vel.vx = -vel.vx;
                door->move->unkE0 = vel;
                door->flags1 = (door->flags1 & ~7) | 4;
            }
        } else {
            if (rot.vx <= 0) {
                vel = door->move->unkA0;
                vel.vx = -vel.vx;
                door->move->unkE0 = vel;
                door->move->unk13C = 0x1000;
            }
            vel = door->move->unkA0;
            if (vel.vx > 0 && rot.vx > 0) {
                done = 1;
                door->flags0 ^= 1;
                MovementRemoveGobj(door);
                func_8008C3A4(door);
            }
        }
        if (done) {
            return;
        }
        break;
    }
    default:
        return;
    }
    SysPostMessage(0xA, 4, door->id, door->id, 0, 0, 0, 0);
}
#else
INCLUDE_ASM("asm/main/nonmatchings/pickup", func_8008D548);
#endif

#ifdef NON_MATCHING
/* type switch in case 24 has more cases (0x61/0x54) in the original; early part 4 insns off */
void DOORObjDispatch(SysMsg *msg) {
    VECTOR32 rot;
    Gobj *door = gameObjectsInfo[msg->to].runtime;
    s32 type = GOBJ_TYPE(door->id);
    s32 ref;
    Gobj *linked;
    s32 state;

    switch (msg->id) {
    case 10:
        if (!(door->flags0 & 0x10)) {
            func_8008D548(door, type);
        }
        break;
    case 6:
        if (!(door->flags1 & 0x80)) {
            CobjAddBulletCollisionBoxGobj(door, 1, 0, (door->unk20[0] >> 4) & 1);
        }
        CobjAddCollisionBoxGobj(door, 1, 0, 1, 1);
        break;
    case 5:
        if (!(door->flags1 & 0x80)) {
            CobjRemoveBulletCollisionBoxGobj(door);
        }
        CobjRemoveCollisionBoxGobj(door);
        break;
    case 26:
        if ((door->flags1 & 7) != 4) {
            SysPostMessage(0x1A, 4, door->id, gPlayerOffset, 0, 0, 0, 0);
        }
        break;
    case 13:
        if (door->healthInfo->health <= 0 && (door->unk20[0] & 0x10)) {
            if (door->anim->unk00[0xA] & 8) {
                EGSAnimShowDetailDef(door->anim);
                EGSDisplayRemoveDamage(door);
            } else {
                door->flags23 |= 2;
                ObjectRemoveAndIgnoreBulletHoles(door);
            }
        }
        break;
    case 18:
        if (!(door->flags0 & 0x10)) {
            if (!(door->flags0 & 0x20) && type != 0x61 && type != 0x54) {
                SysPostMessage(0x18, 3, door->id, gPlayerOffset, 0, 0, 0, 0);
            }
        } else {
            PickupAddUseMsg(door->id, 3);
        }
        break;
    case 19:
        if (!(door->flags0 & 0x20)) {
            if (!(door->flags0 & 0x10)) {
                if (type != 0x61 && type != 0x54) {
                    SysPostMessage(0x19, 3, door->id, gPlayerOffset, 0, 0, 0, 0);
                }
            } else {
                PickupRemoveUseMsg(door->id);
            }
        }
        break;
    case 20:
        if ((door->flags0 & 8) && (type == 7 || type == 0x3B)) {
            break;
        }
        if (type == 0x61 || type == 0x54) {
            door->flags0 &= ~0x10;
            door->flags23 |= 2;
            door->healthInfo->health = -1;
            break;
        }
    case 21:
        door->flags0 |= 8;
        if (!(door->flags1 & 0x80)) {
            ObjectRemoveAndIgnoreBulletHoles(door);
        }
        if (type != 0x3E) {
            func_8008C194(door);
            func_8008D548(door, type);
            break;
        }
    case 24:
        if (door->flags0 & 0x10) {
            break;
        }
        func_8008C194(door);
        switch (type) {
        case 0x49:
        case 0x71:
        case 0x76:
        case 0x60:
        case 0xA:
        case 0x6:
            func_8008C058(door, msg->from);
            for (ref = gameObjectsInfo[door->id].refId; ref != -1; ref = gameObjectsInfo[ref].refId) {
                linked = gameObjectsInfo[ref].runtime;
                if (linked != NULL && linked->npcState == 9) {
                    linked->flags0 |= 0x28;
                    func_8008C058(linked, msg->from);
                    if (!(linked->flags1 & 0x80)) {
                        ObjectRemoveAndIgnoreBulletHoles(linked);
                    }
                    SysPostMessage(0xA, 5, door->id, linked->id, 0, 0, 0, 0);
                }
            }
            break;
        case 0x3E:
            if (door->flags0 & 0x20) {
                return;
            }
            door->flags1 = (door->flags1 & ~7) | 2;
            MovementAddGobj(door);
            rot.vx = 0x44444;
            rot.vy = 0;
            rot.vz = 0;
            door->move->unkF0.vx += rot.vx;
            door->move->unkF0.vy += rot.vy;
            door->move->unkF0.vz += rot.vz;
            break;
        }
        door->flags0 |= 0x28;
        if (!(door->flags1 & 0x80)) {
            ObjectRemoveAndIgnoreBulletHoles(door);
        }
        if (msg->id == 0x18) {
            SysPostMessage(0x19, 3, door->id, gPlayerOffset, 0, 0, 0, 0);
        }
        func_8008D548(door, type);
        break;
    case 27:
        if (door->flags0 & 0x10) {
            door->flags0 &= ~0x10;
        }
        break;
    case 41:
        state = (door->flags23 >> 1) & 1;
        if (door->move != NULL) {
            state |= door->move->unk174 << 16;
        }
        *(s32 *)msg->data[0] = state;
        if ((door->flags0 & 1) && type != 0x3E) {
            door->flags0 ^= 2;
        }
        *(s16 *)msg->data[1] = door->flags0 | (door->flags1 << 8);
        if ((door->flags0 & 1) && type != 0x3E) {
            door->flags0 ^= 2;
        }
        break;
    case 42:
        if (*(s16 *)msg->data[0] != 0) {
            door->flags23 |= 2;
        } else if (door->healthInfo->health <= 0 && (door->anim->unk00[0xA] & 8)) {
            EGSAnimShowDetailDef(door->anim);
        }
        door->flags0 = ((u8 *)msg->data[1])[0];
        door->flags1 = ((u8 *)msg->data[1])[1];
        if (type != 0x3E) {
            if (door->move != NULL) {
                door->move->unk174 = ((s16 *)msg->data[0])[1];
                door->move->unk17C = door->move->unk174 != 0;
                PathSetPositionToPoint(door, door->move->unk174);
                door->flags0 &= ~8;
            }
            if ((door->flags0 & 8) ||
                ((door->flags0 & 1) && (door->move == NULL || type == 7 || type == 0x3B))) {
                SysPostMessage(0xA, 4, door->id, door->id, 0, 0, 0, 0);
            }
        } else if (door->flags0 & 1) {
            door->flags1 = (door->flags1 & ~7) | 4;
            rot = door->move->rot;
            rot.vx += 0x3F4;
            if (rot.vx > 0x800) {
                rot.vx -= 0x1000;
            } else if (rot.vx < -0x800) {
                rot.vx += 0x1000;
            }
            EGSFrameSetRotationYXZ(door->anim->frame, 0, &rot);
        }
        break;
    case 2:
        func_80148440(door, type);
        break;
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/pickup", DOORObjDispatch);
#endif

/* Door update: run the open/close path and post the open/close messages. */
void func_8008E418(Gobj *door, s32 type) {
    u8 done;
    u8 ok;
    u8 recent; /* used again within a tick of the last use */
    s32 timer;

    if (door->flags0 & 8) {
        if (type != 0x40) {
            ok = PathHandleEx(door, (s32)&gObjPathScaleVector, 1, 1, -1, -1, &done);
        } else {
            ok = PathHandleEx(door, (s32)&gObjPathScaleVector, 1, 1, 0,
                              (door->flags0 & 0x80) ? (door->flags1 & 0x7F) - 1 : 1, &done);
        }
#line 2510
        ASSERT(ok == 1);
        if (!done) {
            SysPostMessage(0xA, 4, door->id, door->id, 0, 0, 0, 0);
            recent = (u32)(door->unk04 + 1) >= (u32)gExeTick;
            if (recent) {
                if (!(door->flags0 & 2) && (door->flags0 & 1)) {
                    SysPostMessage(0x25, 5, door->id, gPlayerOffset, 0, 0, 0, 0);
                    door->flags0 |= 2;
                }
            } else if (door->flags0 & 2) {
                SysPostMessage(0x26, 5, door->id, gPlayerOffset, 0, 0, 0, 0);
                door->flags0 &= ~2;
            }
            SysSetSongVolPan(0, 0x15, door, -1, -1);
            return;
        }
        if (door->flags0 & 2) {
            recent = (u32)(door->unk04 + 1) >= (u32)gExeTick;
            if (recent) {
                SysPostMessage(0x26, 5, door->id, gPlayerOffset, 0, 0, 0, 0);
                door->flags0 &= ~2;
            }
        }
        SysStopSong(0, 0x15);
        SysPlaySong(0, 0x12, (s32)door, 0);
        door->flags0 = (door->flags0 & ~8) | 1;
        if ((door->flags0 & 0x80) && type == 0x40) {
            door->flags0 &= ~0x80;
            door->move->unk174 = 0;
            return;
        }
        if (type == 0xD) {
            door->flags0 ^= 4;
            if (door->flags0 & 4) {
                door->flags1 = (door->flags1 & 0x80) | 0x3C;
                SysPostMessage(0xA, 4, door->id, door->id, 0, 0, 0, 0);
            } else {
                recent = (u32)(door->unk04 + 1) >= (u32)gExeTick;
                if (recent && gObjPlayer->healthInfo->health > 0) {
                    LevelDoEOL(0);
                }
            }
            door->unk04 = gExeTick;
        }
    } else if ((door->flags0 & 4) && type == 0xD) {
        timer = GOBJ_TIMER(door) - 1;
        GOBJ_SET_TIMER(door, timer);
        SysPostMessage(GOBJ_TIMER(door) ? 0xA : 0x23, 4, door->id, door->id, 0, 0, 0, 0);
    }
}

void ELEVATORObjDispatch(SysMsg *msg) {
    VECTOR32 pos;
    VECTOR32 riderPos;
    Gobj *lift = gameObjectsInfo[msg->to].runtime;
    s32 type = GOBJ_TYPE(lift->id);
    Gobj *rider;
    char *str;
    s32 ticks;
    u8 recent; /* used again within a tick of the last use */

    switch (msg->id) {
    case 10:
        if (lift->move != NULL && gObjPlayer->healthInfo->health > 0) {
            func_8008E418(lift, type);
        }
        break;
    case 6:
        CobjAddCollisionBoxGobj(lift, 1, 0, 1, 1);
        break;
    case 5:
        CobjRemoveCollisionBoxGobj(lift);
        break;
    case 18:
        if (PathObjectIsAtClosestPoint(lift, gObjPlayer, 0, 1, 1)) {
            PickupAddUseMsg(lift->id, 2);
            SysPostMessage(0x23, 3, lift->id, gPlayerOffset, 0, 0, 0, 0);
        }
        break;
    case 19:
        SysPostMessage(0x19, 3, lift->id, gPlayerOffset, 0, 0, 0, 0);
        PickupRemoveUseMsg(lift->id);
        break;
    case 26:
        lift->unk04 = gExeTick;
        if (type != 0xD && (lift->flags0 & 8)) {
            rider = gameObjectsInfo[msg->from].runtime;
            if (lift->flags0 & 1) {
                pos.vx = lift->anim->frame->pos.vx;
                pos.vy = lift->anim->frame->pos.vy;
                pos.vz = lift->anim->frame->pos.vz;
                pos.vy = -pos.vy;
                riderPos.vx = rider->anim->frame->pos.vx;
                riderPos.vy = rider->anim->frame->pos.vy;
                riderPos.vz = rider->anim->frame->pos.vz;
                riderPos.vy = -riderPos.vy;
                if (riderPos.vy + 0x69 < pos.vy) {
                    MoveInfo *move = lift->move;

                    lift->unk04 = 0;
                    lift->flags0 &= ~1;
                    if (move->unk17C == 0) {
                        move->unk174 = 0;
                        move->unk17C = 1;
                    } else {
                        move->unk174 = 1;
                        move->unk17C = 0;
                    }
                }
            }
        }
        break;
    case 27:
        if (lift->flags0 & 0x10) {
            lift->flags0 &= ~0x10;
        }
        break;
    case 20:
        if (type != 0xD) {
            if (lift->flags0 & 0x10) {
                lift->flags0 &= ~0x10;
            }
            if (!(lift->flags0 & 8) &&
                !PathObjectIsAtClosestPoint(lift, gameObjectsInfo[msg->from].runtime, 0, 1, 0)) {
                func_8008C534(lift, type);
            }
        } else if (!(*(u32 *)lift & 0xC)) {
            func_8008C534(lift, 0xD);
        }
        break;
    case 35:
    case 39:
        if (!(lift->flags0 & 0x10)) {
            if (!(lift->flags0 & 8)) {
                UseObject(lift->id);
                if (type == 0x40 && GOBJ_TYPE(msg->from) == 0x41) {
                    lift->flags0 |= 0x80;
                    lift->move->unk17C = 0;
                    lift->move->unk174 = (lift->flags1 & 0x7F) - 1;
                }
                recent = (u32)(lift->unk04 + 1) >= (u32)gExeTick;
                if (recent) {
                    if (type == 0xD) {
                        SysPlayXA(0xA9, 0, 1);
                    }
                    SysPostMessage(0x25, 5, lift->id, gPlayerOffset, 0, 0, 0, 0);
                    lift->flags0 |= 2;
                    func_8008C534(lift, type);
                } else {
                    func_8008C534(lift, type);
                }
            }
        } else if (D_8010FCF8 != NULL && (u32)gExeTick > (u32)D_801101B8) {
            str = D_8010FCF8(lift->id, type);
            if (str != NULL) {
                ticks = strlen(str) * 2 + 0x18;
                TextStatusMessage(str, ticks);
                ticks += gExeTick;
                if ((u32)D_801101B8 < (u32)ticks) {
                    D_801101B8 = ticks;
                }
            }
        }
        break;
    case 41:
        pos.vx = lift->anim->frame->pos.vx;
        pos.vy = lift->anim->frame->pos.vy;
        pos.vz = lift->anim->frame->pos.vz;
        pos.vy = -pos.vy;
        *(s32 *)msg->data[0] = pos.vy;
        *(s16 *)msg->data[1] = lift->flags0 | (lift->move->unk174 << 8);
        break;
    case 42:
        pos.vx = lift->anim->frame->pos.vx;
        pos.vy = lift->anim->frame->pos.vy;
        pos.vz = lift->anim->frame->pos.vz;
        pos.vy = -pos.vy;
        pos.vy = *(s32 *)msg->data[0];
        EGSFrameSetPosition(lift->anim->frame, 0, &pos);
        lift->flags0 = ((u8 *)msg->data[1])[0];
        lift->move->unk174 = ((u8 *)msg->data[1])[1];
        lift->move->unk17C = lift->move->unk174 != 0;
        if (type == 0x40 && PathObjectIsAtPoint(lift, PathGetNumPoints(lift) - 1)) {
            lift->move->unk17C = 1;
        }
        if (lift->flags0 & 8) {
            SysPostMessage(0xA, 4, lift->id, lift->id, 0, 0, 0, 0);
        }
        break;
    case 2:
        func_801487D4(lift, type);
        break;
    }
}

void INTERACTObjDispatch(SysMsg *msg) {
    VECTOR32 pos;
    VECTOR32 other;
    Gobj *obj = gameObjectsInfo[msg->to].runtime;
    s32 type = GOBJ_TYPE(obj->id);
    char *str;
    u16 flags;
    u8 show;

    switch (msg->id) {
    case 10:
        if (obj->move != NULL) {
            if (type == 0x64) {
                func_80143424(obj);
            } else {
                func_8014236C(obj);
            }
        }
        break;
    case 6:
        if (type == 0x59) {
            InterfaceAddBomb(obj->id);
        } else if (type != 0x58 || (obj->flags0 & 0x20)) {
            CobjAddBulletCollisionBoxGobj(obj, 1, 0, type == 0x58 || type == 0x64);
            CobjAddCollisionBoxGobj(obj, 1, 0, 1, 0);
        }
        break;
    case 5:
        if (type == 0x59) {
            InterfaceDeleteBomb();
        } else if (type != 0x58 || (obj->flags0 & 0x20)) {
            CobjRemoveBulletCollisionBoxGobj(obj);
            CobjRemoveCollisionBoxGobj(obj);
        }
        break;
    case 18:
        obj->flags0 |= 0x40;
        if (!(obj->flags0 & 0x20)) {
            PickupAddUseMsg(obj->id, 3);
            if (!(obj->flags0 & 0x10)) {
                pos.vx = obj->anim->frame->pos.vx;
                pos.vy = obj->anim->frame->pos.vy;
                pos.vz = obj->anim->frame->pos.vz;
                pos.vy = -pos.vy;
                other = gObjPlayer->move->unk10C;
                SysPostMessage((pos.vy < other.vy + 0x69 && type != 0x6C) ? 0x24 : 0x14, 4, obj->id, gPlayerOffset,
                               0, 0, 0, 0);
            } else if (D_8010FCF8 != NULL && type == 0x58) {
                str = D_8010FCF8(obj->id, 0x58);
                if (str != NULL) {
                    TextStatusMessage(str, 0);
                }
            }
        }
        break;
    case 19:
        obj->flags0 &= ~0x40;
        if (!(obj->flags0 & 0x20)) {
            PickupRemoveUseMsg(obj->id);
            if (!(obj->flags0 & 0x10)) {
                SysPostMessage(0x19, 3, obj->id, gPlayerOffset, 0, 0, 0, 0);
            }
        }
        break;
    case 20:
    case 36:
    case 39:
        if (!(obj->flags0 & 0x20)) {
            obj->flags0 |= 0x20;
            gObjectInteractionCallback(obj->id);
            pos.vx = obj->anim->frame->pos.vx;
            pos.vy = obj->anim->frame->pos.vy;
            pos.vz = obj->anim->frame->pos.vz;
            pos.vy = -pos.vy;
            SysPlaySong(1, 0x14, 0, (s32)&pos);
            if (type != 0x58) {
                if (obj->anim->unk00[0xA] & 8) {
                    EGSAnimShowDetailDef(obj->anim);
                }
                EGSDisplayRemoveDamage(obj);
                if (type == 0x59) {
                    UseBeaconUpdate(obj);
                }
            } else {
                obj->anim->unk00[8] &= ~8;
                CobjAddBulletCollisionBoxGobj(obj, 1, 0, 1);
                CobjAddCollisionBoxGobj(obj, 1, 0, 1, 0);
            }
            UseObject(obj->id);
        }
        break;
    case 41:
        if (obj->move != NULL && (type == 0x64 || type == 0x6C)) {
            other.vx = obj->anim->frame->pos.vx;
            other.vy = obj->anim->frame->pos.vy;
            other.vz = obj->anim->frame->pos.vz;
            other.vy = -other.vy;
            *(s32 *)msg->data[0] = other.vy;
        } else {
            *(s32 *)msg->data[0] = 0;
        }
        *(u16 *)msg->data[1] = obj->flags0;
        break;
    case 42:
        flags = *(u16 *)msg->data[1];
        obj->flags0 = flags;
        if (obj->move != NULL) {
            if (type == 0x64) {
                if (flags & 8) {
                    obj->move->unk174 = PathGetNumPoints(obj) - 1;
                    obj->move->unk17C = 0;
                    SysPostMessage(0xA, 4, obj->id, obj->id, 0, 0, 0, 0);
                }
            } else if (type == 0x6C) {
                if (flags & 8) {
                    func_801422A0(obj->id);
                } else if (obj->unk04 <= other.vy) { /* reads other before it is set */
                    obj->flags23 = (obj->flags23 & ~4) | 2;
                }
            } else {
                break;
            }
            other.vx = obj->anim->frame->pos.vx;
            other.vy = obj->anim->frame->pos.vy;
            other.vz = obj->anim->frame->pos.vz;
            other.vy = -other.vy;
            other.vy = *(s32 *)msg->data[0];
            EGSFrameSetPosition(obj->anim->frame, 0, &other);
        }
        if (type != 0x58 && (obj->anim->unk00[0xA] & 8)) {
            show = (*(u32 *)obj & 0x28) == 0x20;
            if (type == 0x64 && show && obj->move != NULL && !PathObjectIsAtPoint(obj, PathGetNumPoints(obj) - 1)) {
                show = 0;
            }
            if (show) {
                EGSAnimShowDetailDef(obj->anim);
            }
        }
        break;
    case 2:
        func_80148CB4(obj, type);
        break;
    }
}

void PICKUPObjDispatch(SysMsg *msg) {
    VECTOR32 pos;
    VECTOR32 playerPos;
    Gobj *obj = gameObjectsInfo[msg->to].runtime;

    switch (msg->id) {
    case 6:
        if (LevelGameObjectHasModel(msg->to)) {
            CobjAddBulletCollisionBoxGobj(obj, 1, 0, 0);
            CobjAddCollisionBoxGobj(obj, 1, 0, 1, 0);
        }
        break;
    case 5:
        if (LevelGameObjectHasModel(msg->to)) {
            CobjRemoveBulletCollisionBoxGobj(obj);
            CobjRemoveCollisionBoxGobj(obj);
        }
        break;
    case 18:
        if (!(obj->flags0 & 0x20)) {
            if (LevelGameObjectHasModel(msg->to)) {
                pos.vx = obj->anim->frame->pos.vx;
                pos.vy = obj->anim->frame->pos.vy;
                pos.vz = obj->anim->frame->pos.vz;
                pos.vy = -pos.vy;
            } else {
                GameObjectInfo *info = (GameObjectInfo *)(obj->id * sizeof(GameObjectInfo) + (u32)gameObjectsInfo);

                pos.vx = info->matrix.t[0];
                pos.vy = info->matrix.t[1];
                pos.vz = info->matrix.t[2];
            }
            playerPos = gObjPlayer->move->unk10C;
            SysPostMessage(pos.vy < playerPos.vy + 0x69 ? 0x16 : 0x17, 4, obj->id, gPlayerOffset, 0, 0, 0, 0);
            PickupObjectAddUseMsgs(obj, obj->flags1);
        }
        break;
    case 19:
        if (!(obj->flags0 & 0x20)) {
            PickupRemoveUseMsg(obj->id);
            SysPostMessage(0x19, 4, obj->id, gPlayerOffset, 0, 0, 0, 0);
        }
        break;
    case 22:
    case 23:
    case 39:
        if (!(obj->flags0 & 0x20) && msg->from == gPlayerOffset) {
            PickupObjectTake(1);
        }
        break;
    case 41:
        *(s32 *)msg->data[0] = obj->flags0;
        break;
    case 42: {
        s32 flags = *(s32 *)msg->data[0];

        obj->flags0 = flags;
        if ((flags & 0x20) && LevelGameObjectHasModel(msg->to) && (obj->anim->unk00[0xA] & 8)) {
            EGSAnimShowDetailDef(obj->anim);
        }
        break;
    }
    case 2:
        func_80149118(obj, msg);
        break;
    }
}

#ifdef NON_MATCHING
/* 8 insns: result = 1 scheduled first instead of after the SysPlaySong args; case 42 loop's other==NULL branch fills its delay slot from the fallthrough */
void SWITCHObjDispatch(SysMsg *msg) {
    VECTOR32 pos;
    Gobj *sw = gameObjectsInfo[msg->to].runtime;
    s16 type = GOBJ_TYPE(sw->id);
    s32 ref;
    Gobj *other;
    u8 more;
    u8 linked;
    s32 result;
    char *str;
    s32 ticks;
    u32 *words;
    s32 bit;
    s32 misalign;
    s32 word;
    s32 mask;
    u16 flags;

    switch (msg->id) {
    case 6:
        CobjAddBulletCollisionBoxGobj(sw, 1, 0, 1);
        CobjAddCollisionBoxGobj(sw, 1, 0, 1, 0);
        break;
    case 5:
        CobjRemoveBulletCollisionBoxGobj(sw);
        CobjRemoveCollisionBoxGobj(sw);
        break;
    case 18:
        sw->flags0 |= 0x40;
        if (!(sw->flags0 & 0x20)) {
            SysPostMessage(0x14, 3, sw->id, gPlayerOffset, 0, 0, 0, 0);
            PickupAddUseMsg(sw->id, 4);
        }
        break;
    case 19:
        sw->flags0 &= ~0x40;
        if (!(sw->flags0 & 0x20)) {
            SysPostMessage(0x19, 3, sw->id, gPlayerOffset, 0, 0, 0, 0);
            PickupRemoveUseMsg(sw->id);
        }
        break;
    case 21:
    case 27:
        if (sw->flags0 & 0x10) {
            sw->flags0 &= ~0x10;
        }
        break;
    case 13:
        if (type == 0x1C || type == 0x7A || type == 0x24 || type == 0x7B || type == 0x79 || type == 0x7C ||
            type == 0x6A) {
            return;
        }
        if (type == 0x3A) {
            if (msg->from != gPlayerOffset) {
                return;
            }
            more = 1;
            ref = sw->id;
            do {
                ref = gameObjectsInfo[ref].refId;
                if (ref != -1) {
                    other = gameObjectsInfo[ref].runtime;
                    if (other != NULL) {
                        if (other->npcState == 4) {
                            more = 0;
                        } else if (other->npcState == 9 && (other->anim->unk00[0xA] & 8)) {
                            EGSAnimShowDetailDef(other->anim);
                        }
                    }
                    SysPostMessage(0x14, 3, sw->id, ref, 0, 0, 0, 0);
                } else {
                    more = 0;
                }
            } while (more);
            if (!(sw->anim->unk00[0xA] & 8)) {
                sw->flags23 |= 2;
            } else {
                EGSAnimShowDetailDef(sw->anim);
            }
            return;
        }
        /* other switches treat "use" as a press */
    case 20:
    case 39:
        if (!(sw->flags0 & 0x10)) {
            if (sw->flags0 & 0x20) {
                break;
            }
            sw->flags0 |= 0x20;
            switch (type) {
            case 0x3A:
                break;
            case 0x6A:
                if (msg->from == gPlayerOffset) {
                    sw->flags0 &= ~0x20;
                    break;
                }
            case 0x1B:
            case 0x1C:
            case 0x1D:
            case 0x24:
            case 0x41:
            case 0x79:
            case 0x7A:
            case 0x7B:
            case 0x7C:
                if (ObjectMatchToRefID(sw->id, 0x63)->id != sw->id) {
                    words = (u32 *)gWeaponsAvailable;
                    bit = 0x17;
                    misalign = (u32)words & 3;
                    if (misalign) {
                        bit = misalign * 8 + 0x17;
                        words = (u32 *)((u32)words & ~3);
                    }
                    word = bit >> 5;
                    mask = 1 << (bit & 0x1F);
                    if (words[word] & mask) {
                        WeaponRemove(0x17);
                    }
                }
                pos.vx = sw->anim->frame->pos.vx;
                pos.vy = sw->anim->frame->pos.vy;
                pos.vz = sw->anim->frame->pos.vz;
                pos.vy = -pos.vy;
                result = 1;
                SysPlaySong(0, 0x14, 0, (s32)&pos);
                if (type != 0x1D && type != 0x6A) {
                    result = 3;
                }
                if (D_8010FD0C != NULL) {
                    result = D_8010FD0C(sw->id);
                }
                if (sw->flags0 & 0x40) {
                    UseObject(sw->id);
                }
                if (result & 1) {
                    ObjectRefMultiSYSPOSTMSG0(sw->id, 0x14, 3);
                }
                if (result & 2) {
                    if (sw->flags0 & 0x40) {
                        SysExeTickQueueTimer(iSwitchReactivate, 0x14, sw);
                    }
                    if (msg->id == 0xD) {
                        sw->healthInfo->health = sw->healthInfo->armor;
                    }
                    sw->flags0 &= ~0x20;
                }
                if (sw->anim->unk00[0xA] & 8) {
                    sw->anim->unk00[0xA] ^= 4;
                }
                break;
            }
        } else if (D_8010FCF8 != NULL && (u32)gExeTick > (u32)D_801101B8) {
            str = D_8010FCF8(sw->id, type);
            if (str != NULL) {
                ticks = strlen(str) * 2 + 0x18;
                TextStatusMessage(str, ticks);
                ticks += gExeTick;
                if ((u32)D_801101B8 < (u32)ticks) {
                    D_801101B8 = ticks;
                }
            }
        }
        break;
    case 41:
        *(s32 *)msg->data[0] = (sw->flags23 >> 1) & 1;
        *(u16 *)msg->data[1] = sw->flags0;
        break;
    case 42:
        flags = *(u16 *)msg->data[1];
        sw->flags0 = flags;
        if ((flags & 0x20) && (sw->anim->unk00[0xA] & 8)) {
            EGSAnimShowDetailDef(sw->anim);
        }
        if (*(s32 *)msg->data[0] != 0) {
            sw->flags23 |= 2;
            if (type == 0x3A) {
                ref = sw->id;
                linked = 1;
                do {
                    ref = gameObjectsInfo[ref].refId;
                    if (ref != -1) {
                        other = gameObjectsInfo[ref].runtime;
                        if (other != NULL) {
                            if (other->npcState == 4) {
                                linked = 0;
                            } else if (other->npcState == 9 && (other->anim->unk00[0xA] & 8)) {
                                EGSAnimShowDetailDef(other->anim);
                            }
                        }
                    } else {
                        linked = 0;
                    }
                } while (linked);
            }
        }
        break;
    case 2:
        func_8014930C(sw, type);
        break;
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/pickup", SWITCHObjDispatch);
#endif
