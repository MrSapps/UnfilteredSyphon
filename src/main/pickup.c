#include "common.h"
#include "system.h"
#include "level.h"
#include "gameobj.h"
#include "gobj.h"

/* pickup.c's small data, still assembled from asm */
SDATA_LOCAL(D_8010FCF8)
SDATA_LOCAL(D_8010FCFC)
SDATA_LOCAL(D_8010FD00)
SDATA_LOCAL(D_8010FD04)
SDATA_LOCAL(D_8010FD08)
SDATA_LOCAL(D_8010FD0C)
SDATA_LOCAL(gObjectInteractionCallback)
SDATA_LOCAL(D_801101B8)
SDATA_LOCAL(D_801101BC)
SDATA_LOCAL(D_801101C0)
SDATA_LOCAL(D_801101C4)
SDATA_LOCAL(D_801101C8)
SDATA_LOCAL(gDeviceDetonated)
SDATA_LOCAL(gLevelKeycards)
SDATA_LOCAL(gLevelKeycardsTaken)
SDATA_LOCAL(gNextTickAllowMsgLongText)

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

extern char *(*D_8010FCF8)();         /* locked-door string callback */
extern char *(*D_8010FCFC)();         /* use string callback */
extern void (*D_8010FD00)();          /* key callback */
extern void (*D_8010FD04)();          /* bomb callback */
extern void (*D_8010FD08)();          /* computer callback */
extern void (*D_8010FD0C)();          /* switch callback */
extern void (*gObjectInteractionCallback)(); /* level specific object callback */
extern s32 D_801101B8;
extern s32 D_801101BC;                /* UseMsg count */
extern s32 D_801101C0;                /* PickupPending count */
extern u8 D_801101C4;
extern u8 D_801101C8;
extern u8 gDeviceDetonated;
extern s32 gLevelKeycards;
extern s32 gLevelKeycardsTaken;
extern u32 gNextTickAllowMsgLongText;

extern UseMsg D_80121978[USE_MSG_MAX];
extern PickupPending D_80121990[PICKUP_PENDING_MAX];
extern u16 D_801219B0[PICKUP_TAKEN_MSG_COUNT]; /* taken-message text ids */
extern u8 D_801219E8[PICKUP_TAKEN_MSG_COUNT];  /* checksums of those messages */

void TextRemoveString();
s32 TextIsStringShowing(u16 str);
u8 TextComputeChecksum(char *str);
void TextChangeStringDuration(u16 str, s32 ticks);
s32 strlen(char *str);
u8 LevelGameObjectHasModel(s16 id);
void EGSDisplayRemoveDamage(Gobj *obj);
void EGSAnimShowDetailDef(GameObjectAnim *anim);
char *StringSetUseMsg(s16 msgId);
u16 TextAddMarker(u16 objId, char *str, s32 arg2, s32 arg3);
void DamageKillAllPeople(s32 by, s32 type);
void ParticleSmoke(s32 arg0, VECTOR32 *pos, void *dir, s32 arg3, s32 arg4);
void InterfaceDeleteBomb(void);

extern char D_8010FCEC[]; /* "" */
extern s32 gExploDir;

void PickupAddUseMsg(s16 objId, s16 msgId);
void PickupRemoveUseMsg(s16 objId);
void UseObject(s16 objId);
void *UseSetComputerCallback(void (*cb)());

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

#ifdef NON_MATCHING
/* the original strength-reduces both list offsets; GCC here recomputes idx*6 */
static void func_8008A550(s32 idx) {
    s32 count = --D_801101BC;
    s32 i;

    for (i = idx; i < count; i++) {
        UseMsg *dst = &D_80121978[i];
        UseMsg *src = &D_80121978[i + 1];
        dst->textId = src->textId;
        dst->msgId = src->msgId;
        dst->objId = src->objId;
    }
    D_80121978[D_801101BC].textId = 0xFFFF;
    D_80121978[D_801101BC].msgId = -1;
    D_80121978[D_801101BC].objId = -1;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/pickup", func_8008A550);
#endif

#ifdef NON_MATCHING
/* same loop shape as func_8008A550 */
/* global until PickupObjectTake, DOORObjDispatch and ELEVATORObjDispatch are C */
void func_8008A62C(s32 idx) {
    for (D_801101C0--; idx < D_801101C0; idx++) {
        D_80121990[idx].objId = D_80121990[idx + 1].objId;
        D_80121990[idx].armor = D_80121990[idx + 1].armor;
        D_80121990[idx].weapon = D_80121990[idx + 1].weapon;
        D_80121990[idx].amount = D_80121990[idx + 1].amount;
        D_80121990[idx].showMsg = D_80121990[idx + 1].showMsg;
    }
    D_80121990[D_801101C0].armor = 0;
    D_80121990[D_801101C0].objId = -1;
    D_80121990[D_801101C0].weapon = 0;
    D_80121990[D_801101C0].amount = 0;
    D_80121990[D_801101C0].showMsg = 0;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/pickup", func_8008A62C);
#endif

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

INCLUDE_ASM("asm/main/nonmatchings/pickup", PickupObjectAddUseMsgs);

#ifdef NON_MATCHING
/* PickupRemoveUseMsg gets objId (s2) in the original, the equal 'to' (a3) here */
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

            if (objId != to) {
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
#else
INCLUDE_ASM("asm/main/nonmatchings/pickup", UseObject);
#endif

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

#ifdef NON_MATCHING
/* original loads &D_801219B0 before idx*2 */
void iPickupObjectTakenMsg(char *msg, s32 idx) {
    u16 *text = idx + D_801219B0;
    u8 show;

    if (TextIsStringShowing(*text)) {
        if (D_801219E8[idx] == TextComputeChecksum(msg)) {
            TextChangeStringDuration(*text, strlen(msg) * 2 + 0x18);
            show = 0;
        } else {
            TextRemoveString(*text);
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
#else
INCLUDE_ASM("asm/main/nonmatchings/pickup", iPickupObjectTakenMsg);
#endif


















INCLUDE_ASM("asm/main/nonmatchings/pickup", PickupObjectIsNotMaxed);

INCLUDE_ASM("asm/main/nonmatchings/pickup", PickupObjectTake);

void *PickupSetKeyCallback(void (*cb)()) {
    void *old = D_8010FD00;
    D_8010FD00 = cb;
    return old;
}


INCLUDE_ASM("asm/main/nonmatchings/pickup", UseKey);

INCLUDE_ASM("asm/main/nonmatchings/pickup", KeyDroppedByNPC);

#ifdef NON_MATCHING
/* regalloc: original keeps a second copy of obj in v1 for the else path */
void iKeyRestore(Gobj *obj) {
    if (gameObjectsInfo[gameObjectsInfo[obj->id].refId].runtime->flags0 & 0x10) {
        obj->flags0 &= ~0x20;
        return;
    }
    obj->flags0 |= 0x20;
    gLevelKeycardsTaken++;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/pickup", iKeyRestore);
#endif

INCLUDE_ASM("asm/main/nonmatchings/pickup", UseBeaconUpdate);

#ifdef NON_MATCHING
/* addu operand order of the model table lookup */
void ObjectBombDamagedCallback(s16 id, s16 by) {
    GameObjectInfo *info;

    if (!D_801101C4) {
        info = &gameObjectsInfo[id];
        if (!(info->runtime->flags1 & 0x80) && id != GOBJ_ID_666 &&
            gameObjectsModelInfo[info->modelIndex].type == GOBJ_TYPE_BOMB) {
            D_801101C4 = 1;
            SysPlayXA(0x81, 0, 1);
        }
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/pickup", ObjectBombDamagedCallback);
#endif

#ifdef NON_MATCHING
/* prologue: original copies xa (a1) to s0 before reason (a0) to s1 */
void ObjectBombDeath(s32 reason, s32 xa, s32 secondary, s32 id) {
    s32 track = xa;

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
        LevelMissionFailed((s16)reason, track, secondary);
    }
    DamageKillAllPeople(gPlayerOffset, 0x12);
}
#else
INCLUDE_ASM("asm/main/nonmatchings/pickup", ObjectBombDeath);
#endif

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

INCLUDE_ASM("asm/main/nonmatchings/pickup", func_8008C058);

INCLUDE_ASM("asm/main/nonmatchings/pickup", func_8008C194);

INCLUDE_ASM("asm/main/nonmatchings/pickup", func_8008C2FC);

INCLUDE_ASM("asm/main/nonmatchings/pickup", func_8008C3A4);

INCLUDE_ASM("asm/main/nonmatchings/pickup", func_8008C534);

INCLUDE_ASM("asm/main/nonmatchings/pickup", UseLevelSpecificObject);

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



INCLUDE_ASM("asm/main/nonmatchings/pickup", iSwitchReactivate);

INCLUDE_RODATA("asm/main/nonmatchings/pickup", D_80012DE0);

INCLUDE_RODATA("asm/main/nonmatchings/pickup", D_80012DF0);

INCLUDE_ASM("asm/main/nonmatchings/pickup", AUTOPICKUPObjDispatch);

INCLUDE_ASM("asm/main/nonmatchings/pickup", BOMBObjDispatch);

INCLUDE_ASM("asm/main/nonmatchings/pickup", COMPUTERObjDispatch);

INCLUDE_ASM("asm/main/nonmatchings/pickup", func_8008D548);

INCLUDE_ASM("asm/main/nonmatchings/pickup", DOORObjDispatch);

INCLUDE_ASM("asm/main/nonmatchings/pickup", func_8008E418);

INCLUDE_ASM("asm/main/nonmatchings/pickup", ELEVATORObjDispatch);

INCLUDE_ASM("asm/main/nonmatchings/pickup", INTERACTObjDispatch);

INCLUDE_ASM("asm/main/nonmatchings/pickup", PICKUPObjDispatch);

INCLUDE_ASM("asm/main/nonmatchings/pickup", SWITCHObjDispatch);
