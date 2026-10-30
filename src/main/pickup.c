#include "common.h"
#include "system.h"
#include "level.h"
#include "gameobj.h"
#include "gobj.h"
#include "game.h"

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

Gobj *ObjectMatchToRefID(s16 id, s32 type);
void PlayerUseCAS();
Gobj *iObjectFindNearbyLight(Gobj *obj, s32 type);

void iPickupObjectTakenMsg(char *msg, s32 idx);
void MatrixNormal_0(GOBJ_MATRIX *in, GOBJ_MATRIX *out);
void EGSFrameSetPosition(EGSFrame *frame, s32 arg1, VECTOR32 *pos);
s32 LevelDoEOL(u8 force);
void func_8008E418(Gobj *door, s32 msg);
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
extern s16 D_8010FCF0[]; /* armor pickup message ids */

extern char D_8010FCEC[]; /* "" */
extern s32 gCurrentWeapon;
extern s32 gExploDir;

void PickupAddUseMsg(s16 objId, s16 msgId);
void PickupRemoveUseMsg(s16 objId);
void UseObject(s16 objId);
void *UseSetComputerCallback(void (*cb)());

/* used again within a tick of the last use */
#define DOOR_RECENT(door) ((u32)((door)->unk04 + 1) < (u32)gExeTick ? 0 : 1)
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


















#ifdef NON_MATCHING
/* scheduling: notMaxed = 0 lands after the first argument setup in the original */
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
        notMaxed = 0;
        StringMaxedAmmoMsg(ammoMsg, WEAPON_FIELD(D_80105F64, s16, weapon), ammoType != 0x13 && ammoType != 0x14,
                           ammoType != 6 && ammoType != 7);
        iPickupObjectTakenMsg(ammoMsg, weapon);
    }
    if (armor == 1 && gObjPlayer->healthInfo->armor >= 600) {
        StringUndamagedFlakJacket(flakMsg);
        iPickupObjectTakenMsg(flakMsg, 0x1A);
        notMaxed = 0;
    }
    return notMaxed;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/pickup", PickupObjectIsNotMaxed);
#endif

INCLUDE_ASM("asm/main/nonmatchings/pickup", PickupObjectTake);

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
void func_8008C534(Gobj *obj, s32 openerId) {
    VECTOR32 pos;

    pos.vx = obj->anim->frame->pos.vx;
    pos.vy = obj->anim->frame->pos.vy;
    pos.vz = obj->anim->frame->pos.vz;
    pos.vy = -pos.vy;
    SysPlaySong(0, 0x15, (s32)obj, 0);
    SysPlaySong(0, 0x11, (s32)obj, 0);
    obj->flags0 |= 8;
    func_8008E418(obj, openerId);
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

INCLUDE_RODATA("asm/main/nonmatchings/pickup", D_80012DF0);


INCLUDE_ASM("asm/main/nonmatchings/pickup", AUTOPICKUPObjDispatch);

INCLUDE_ASM("asm/main/nonmatchings/pickup", BOMBObjDispatch);

INCLUDE_ASM("asm/main/nonmatchings/pickup", COMPUTERObjDispatch);

INCLUDE_ASM("asm/main/nonmatchings/pickup", func_8008D548);

INCLUDE_ASM("asm/main/nonmatchings/pickup", DOORObjDispatch);

#ifdef NON_MATCHING
/* the original tests the last-use tick as a value (sltu; xori; beqz); its ASSERT owns the "s/pickup.c" string */
/* Door update: run the open/close path and post the open/close messages. */
void func_8008E418(Gobj *door, s32 msg) {
    u8 done;
    u8 ok;

    if (door->flags0 & 8) {
        if (msg != 0x40) {
            ok = PathHandleEx(door, (s32)&gObjPathScaleVector, 1, 1, -1, -1, &done);
        } else {
            ok = PathHandleEx(door, (s32)&gObjPathScaleVector, 1, 1, 0,
                              (door->flags0 & 0x80) ? (door->flags1 & 0x7F) - 1 : 1, &done);
        }
#line 2510
        ASSERT(ok == 1);
        if (!done) {
            SysPostMessage(0xA, 4, door->id, door->id, 0, 0, 0, 0);
            if (DOOR_RECENT(door)) {
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
        if ((door->flags0 & 2) && DOOR_RECENT(door)) {
            SysPostMessage(0x26, 5, door->id, gPlayerOffset, 0, 0, 0, 0);
            door->flags0 &= ~2;
        }
        SysStopSong(0, 0x15);
        SysPlaySong(0, 0x12, (s32)door, 0);
        door->flags0 = (door->flags0 & ~8) | 1;
        if ((door->flags0 & 0x80) && msg == 0x40) {
            door->flags0 &= ~0x80;
            door->move->unk174 = 0;
            return;
        }
        if (msg == 0xD) {
            door->flags0 ^= 4;
            if (door->flags0 & 4) {
                door->flags1 = (door->flags1 & 0x80) | 0x3C;
                SysPostMessage(0xA, 4, door->id, door->id, 0, 0, 0, 0);
            } else if (DOOR_RECENT(door) && gObjPlayer->healthInfo->health > 0) {
                LevelDoEOL(0);
            }
            door->unk04 = gExeTick;
        }
    } else if ((door->flags0 & 4) && msg == 0xD) {
        door->flags1 = (door->flags1 & 0x80) | (((door->flags1 & 0x7F) - 1) & 0x7F);
        SysPostMessage((door->flags1 & 0x7F) ? 0xA : 0x23, 4, door->id, door->id, 0, 0, 0, 0);
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/pickup", func_8008E418);
#endif

INCLUDE_ASM("asm/main/nonmatchings/pickup", ELEVATORObjDispatch);

INCLUDE_ASM("asm/main/nonmatchings/pickup", INTERACTObjDispatch);

INCLUDE_ASM("asm/main/nonmatchings/pickup", PICKUPObjDispatch);

INCLUDE_ASM("asm/main/nonmatchings/pickup", SWITCHObjDispatch);
