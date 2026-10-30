#include "common.h"
#include "system.h"
#include "gameobj.h"
#include "gobj.h"

/* object.c's small data, still assembled from asm */
SDATA_LOCAL(D_8010F7CC)
SDATA_LOCAL(D_8010F7D0)
SDATA_LOCAL(D_8010F7D4)
SDATA_LOCAL(D_8010F7D8)
SDATA_LOCAL(gObjectMovieDoneCallback)
SDATA_LOCAL(gNotKilledByFallingObject)
SDATA_LOCAL(D_8010FF98)
SDATA_LOCAL(D_8010FF9C)
SDATA_LOCAL(D_8010FFA0)
SDATA_LOCAL(D_8010FFA4)
SDATA_LOCAL(D_8010FFA8)
SDATA_LOCAL(D_8010FFAC)
SDATA_LOCAL(D_8010FFB0)
SDATA_LOCAL(D_8010FFB4)

typedef void (*ObjectCallback)();

extern ObjectCallback D_8010F7CC; /* quip object callback */
extern ObjectCallback D_8010F7D0; /* special object callback */
extern ObjectCallback D_8010F7D4; /* special ambient callback */
extern ObjectCallback D_8010F7D8; /* ambient sound index callback */
extern ObjectCallback gObjectMovieDoneCallback;
extern u8 gNotKilledByFallingObject;
extern s32 D_8010FF98;
extern s32 D_8010FF9C;
extern s32 D_8010FFA0;
extern s32 D_8010FFA4;
extern s32 D_8010FFA8;
extern s32 D_8010FFAC;
extern s32 D_8010FFB0;
extern s32 D_8010FFB4;
extern MovieInfo gObjectMovieInfo;
extern char D_8010F7E4[]; /* "\\CUT\\%s" (.sdata) */
extern char D_8010F7EC[]; /* "MOVIE" (.sdata) */
extern u8 gUsingSndBigFire;
extern s32 gExploDir;

ObjectCallback ObjectSetMovieDoneCallback(ObjectCallback cb);
ObjectCallback ObjectSetQuipObjectCallback(ObjectCallback cb);
ObjectCallback ObjectSetSpecialObjectCallback(ObjectCallback cb);
ObjectCallback ObjectSetSpecialAmbientCallback(ObjectCallback cb);
ObjectCallback ObjectSetAmbientSndIdxCallback(ObjectCallback cb);
void SysPlayMovie(MovieInfo *info);
void SysStopSong(s32 arg0, s32 song);
s32 SysPlaySong(s32 arg0, s32 song, void *arg2, s32 arg3);
void SysExeTickQueueTimer(void (*cb)(s32), s32 ticks, s32 arg);
void ParticleSmoke(s32 id, VECTOR32 *pos, void *dir, s32 size, s32 arg4);
void EGSAnimShowDetailDef(GameObjectAnim *anim);
void EGSDisplayRemoveDamage();
void ParticleFireHide(); /* (s16 id); unprototyped: EFFECTSObjDispatch passes extra args */
void MatrixNormal_0(void *in, void *out);
void SysSoundQueueXA(s16 level, s32 xa);
void SysSoundPlayQueuedXA(s32 arg0, s32 arg1);
u16 StringContactMeiXingMsg(void);
void CobjAddBulletCollisionBoxGobj(Gobj *gobj, s32 arg1, s32 arg2, s32 arg3);
void CobjAddCollisionBoxGobj(Gobj *gobj, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
void CobjRemoveBulletCollisionBoxGobj(Gobj *gobj);
void CobjRemoveCollisionBoxGobj(Gobj *gobj);
void func_80148A00(Gobj *gobj);
s32 EGSMathFIXMul(s32 a, s32 b);
void ParticleFireShow(s16 id, VECTOR32 *pos, s32 arg2, s32 arg3);
s32 TerrainGetOriginalId(s32 sector);
void TerrainReplaceSector(s32 sector, s32 id);

typedef struct ActivationList {
    /* 0x00 */ s32 count;
    /* 0x04 */ GameObjectInfo **infos;
} ActivationList;

typedef struct LevelSectorInfo {
    /* 0x00 */ u8 unk00[0x14];
    /* 0x14 */ u8 links[1][15]; /* per sector: linked sectors, 0xFF = self, 0xFE = none */
} LevelSectorInfo;

typedef struct LevelData {
    /* 0x00 */ u8 unk00[0x6C];
    /* 0x6C */ LevelSectorInfo sectors;
} LevelData;

extern s32 gPlayerOffset;
extern s32 gExploShakeVector;

void ParticleExplosion(s16 id, VECTOR32 *pos, void *dir, s32 size, s32 arg4);
void GameCameraShakeStart(void *state, s32 ticks, s32 arg2, void *vec, s32 arg4, void *pos);
void EGSDisplaySetFlashTime(s32 ticks);
void MovementAddGobj(Gobj *gobj);
void MovementRemoveGobj(Gobj *gobj);
void func_80149280(Gobj *gobj);
void ObjectSetLightState(Gobj *gobj, s32 state);

static inline u8 ObjectIsInactive(s16 id) {
    Gobj *gobj = gameObjectsInfo[id].runtime;
    s32 ret = 0;

    if (gobj == NULL || gobj->anim == NULL) {
        ret = 1;
    }
    return ret;
}



static inline void ObjectSetAmbient2(s16 id, s32 path, GameObjectInfo *other) {
    GameObjectInfo *info = &gameObjectsInfo[id];

    if (path == other->pathStart) {
        info->flags24 |= 0x20;
    } else {
        info->flags24 &= ~0x20;
    }
}
/* Gobj.unk04 as a word (damage state of multi-part objects) */
#define GOBJ_STATE(g) (*(u32 *)&(g)->unk04)

/* GameObjectHealth with the fields used here */
typedef struct GobjHealth {
    /* 0x00 */ u8 unk00[4];
    /* 0x04 */ s16 lastAttacker;
    /* 0x06 */ s16 armor;
    /* 0x08 */ s16 health;
    /* 0x0A */ u8 unk0A[2];
    /* 0x0C */ s16 unk0C;
} GobjHealth;
#define GOBJ_HEALTH(g) ((GobjHealth *)(g)->healthInfo)

void GameCameraSetScrimScrollRate(u8 rate);
void func_801486DC(Gobj *gobj);
void DamageGOBJ(s32 arg0, s16 from, s16 to, s16 amount, s32 type);

typedef struct TerrSectorDisplay {
    /* 0x00 */ u8 unk00[0x20];
    /* 0x20 */ s32 lightSector;
} TerrSectorDisplay;

typedef struct TerrSectorModel {
    /* 0x00 */ u8 unk00[0x10];
    /* 0x10 */ TerrSectorDisplay *display;
} TerrSectorModel;

typedef struct TerrSector {
    /* 0x00 */ TerrSectorModel *model;
    /* 0x04 */ u8 unk04[0x38];
} TerrSector; /* size 0x3C */

extern TerrSector *gTerrSector;
extern s32 gTerrSectorCount;

s32 EGSDisplayGetSectorLightArray(s32 sector);
void EGSDisplaySectorLight(s32 sector, s16 id, s32 arg2);
void EGSDisplayKillAnimatedLight(s32 sector, s16 id);

/* MoveInfo fields only used here */
typedef struct MoveInfoRot {
    /* 0x00 */ u8 unk00[0xE0];
    /* 0xE0 */ VECTOR32 rotVel;
} MoveInfoRot;
extern s16 gGabeSector;
extern LevelData *gLevData;
extern ActivationList *objActivationList;

void ObjectTextureAnimationsHack(void) {
}

void ObjectInitData(void) {
    ObjectSetMovieDoneCallback(NULL);
    ObjectSetQuipObjectCallback(NULL);
    ObjectSetSpecialObjectCallback(NULL);
    ObjectSetSpecialAmbientCallback(NULL);
    ObjectSetAmbientSndIdxCallback(NULL);
    gNotKilledByFallingObject = 1;
    D_8010FF98 = 0;
    D_8010FF9C = -1;
    D_8010FFA0 = 0;
    D_8010FFA4 = -1;
    D_8010FFA8 = 0;
    D_8010FFAC = -1;
    D_8010FFB0 = 0;
    D_8010FFB4 = 0;
}

void iObjectDoMovieEx(void) {
    InterfaceForceOff();
    EGSDisplayFadeOff();
    SysStatePop();
    SysPlayMovie(&gObjectMovieInfo);
}

void ObjectDoMovieEx(char *name, s32 doneMsgId, s32 doneMsgTo, s32 doneMsgFrom) {
    sprintf(gObjectMovieInfo.name, D_8010F7E4, name);
    gObjectMovieInfo.halfSize = 0x28;
    gObjectMovieInfo.width = 320;
    gObjectMovieInfo.height = 240;
    gObjectMovieInfo.unk84 = 0;
    gObjectMovieInfo.doneMsgId = doneMsgId;
    gObjectMovieInfo.doneMsgFrom = doneMsgFrom;
    gObjectMovieInfo.doneMsgTo = doneMsgTo;
    gObjectMovieInfo.trueColor = 1;
    SysStall(D_8010F7EC, 0xE, iObjectDoMovieEx);
}

ObjectCallback ObjectSetMovieDoneCallback(ObjectCallback cb) {
    ObjectCallback old = gObjectMovieDoneCallback;
    gObjectMovieDoneCallback = cb;
    return old;
}

void ObjectRemoveAndIgnoreBulletHoles(Gobj *gobj) {
    if (gobj->unk20[0] & 0x80) {
        gobj->unk20[0] &= 0x7F;
        EGSDisplayRemoveDamage();
    }
}

void ObjectReplaceSector(Gobj *gobj) {
    s32 sector;
    s32 j;
    s32 i = 0;
    ActivationList *list;
    LevelSectorInfo *sectors = &gLevData->sectors;

    do {
        sector = sectors->links[gGabeSector][i];
        if (sector != 0xFE) {
            if (sector == 0xFF) {
                sector = gGabeSector;
            }
            list = &objActivationList[sector];
            for (j = 0; j < list->count; j++) {
                if (gobj == list->infos[j]->runtime) {
                    TerrainReplaceSector(sector, TerrainGetOriginalId(sector) + 1);
                    return;
                }
            }
        }
        i++;
    } while (sector != gGabeSector);
}

s32 ObjectInvisibleMatchToRefID(s16 id, s16 type) {
    s32 i;

    for (i = 0; i < gLevelGameObjCount; i++) {
        if (ObjectIsInactive(i) && type == GOBJ_TYPE(i) && gameObjectsInfo[i].refId == id) {
            return i;
        }
    }
    return id;
}

GameObjectRuntime *ObjectMatchToRefID(s16 id, s16 type) {
    s32 i;

    for (i = 0; i < gLevelGameObjCount; i++) {
        if (!ObjectIsInactive(i) && type == GOBJ_TYPE(i) && gameObjectsInfo[i].refId == id) {
            return gameObjectsInfo[i].runtime;
        }
    }
    return gameObjectsInfo[id].runtime;
}

#ifdef NON_MATCHING
/* Scheduling: the id sign extension is hoisted before the gameObjectsInfo load. */
GameObjectRuntime *ObjectMatchToRefIDByCatID(s16 id, s16 category) {
    s32 i;
    Gobj *gobj;
    s32 count = gLevelGameObjCount;

    for (i = 0; i < count; i++) {
        if (!ObjectIsInactive(i)) {
            gobj = gameObjectsInfo[i].runtime;
            if (gobj != NULL && category == gobj->npcState && gameObjectsInfo[i].refId == id) {
                return gobj;
            }
        }
    }
    return gameObjectsInfo[id].runtime;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/object", ObjectMatchToRefIDByCatID);
#endif

INCLUDE_ASM("asm/main/nonmatchings/object", ObjectTeleportPlayer);

#ifdef NON_MATCHING
/* Register allocation: id and ref get s0/s2 swapped. */
void ObjectRefMultiSYSPOSTMSG0(s16 id, s32 msg, s32 priority) {
    s16 ref = id;
    u8 more = 1;
    Gobj *gobj;

    do {
        ref = gameObjectsInfo[ref].refId;
        if (ref != -1) {
            gobj = gameObjectsInfo[ref].runtime;
            if (gobj != NULL && gobj->npcState == 4) {
                more = 0;
            }
            SysPostMessage(msg, priority, id, ref, 0, 0, 0, 0);
        } else {
            more = 0;
        }
    } while (more);
}
#else
INCLUDE_ASM("asm/main/nonmatchings/object", ObjectRefMultiSYSPOSTMSG0);
#endif

void ObjectBigExplosionFX(s16 id, VECTOR32 *pos, s32 radius, s32 shakeTicks, s32 flashTicks) {
    if (pos != NULL) {
        pos->vx = gameObjectsInfo[id].runtime->anim->frame->pos.vx;
        pos->vy = gameObjectsInfo[id].runtime->anim->frame->pos.vy;
        pos->vz = gameObjectsInfo[id].runtime->anim->frame->pos.vz;
        pos->vy = -pos->vy;
        ParticleExplosion(-1, pos, &gExploDir, radius, 1);
    }
    GameCameraShakeStart(NULL, shakeTicks, 1, &gExploShakeVector, 0, pos);
    EGSDisplaySetFlashTime(flashTicks);
}

void iElectronicStopShockSnd(s32 handle) {
    SysStopSong(1, handle);
}

void ObjectElectronicDestruction(Gobj *gobj) {
    VECTOR32 pos;

    pos.vx = gobj->anim->frame->pos.vx;
    pos.vy = gobj->anim->frame->pos.vy;
    pos.vz = gobj->anim->frame->pos.vz;
    pos.vy = -pos.vy;
    ParticleSmoke(-1, &pos, &gExploDir, 0x599, 1);
    if (gobj->anim->unk00[0xA] & 8) {
        EGSAnimShowDetailDef(gobj->anim);
    }
    SysExeTickQueueTimer(iElectronicStopShockSnd, 10, (s16)SysPlaySong(1, 0x18, gobj, 0));
}

void func_8002BE44(Gobj *gobj, s32 *count, s32 *handle) {
    VECTOR32 pos;

    if (!gUsingSndBigFire) {
        pos.vx = gobj->anim->frame->pos.vx;
        pos.vy = gobj->anim->frame->pos.vy;
        pos.vz = gobj->anim->frame->pos.vz;
        pos.vy = -pos.vy;
        (*count)++;
    }
}

void func_8002BECC(Gobj *gobj, s32 *count, s32 handle) {
    if (!gUsingSndBigFire) {
        if (*count > 0) {
            (*count)--;
        }
    }
}

void ObjectSetLights(s32 state) {
    s32 i;
    Gobj *gobj;
    s16 type;

    for (i = 0; i < gLevelGameObjCount; i++) {
        gobj = gameObjectsInfo[i].runtime;
        type = GOBJ_TYPE(i);
        if (gobj != NULL && gobj->healthInfo->health > 0 && gobj->npcState == 4 && type != 0x14 &&
            type != 0x28 && type != 0x6D && type != 0x34) {
            ObjectSetLightState(gobj, state);
        }
    }
}

INCLUDE_ASM("asm/main/nonmatchings/object", ObjectSetLightState);

s32 ObjectSetSectorLighting(s32 sector) {
    TerrSector *ts;
    s32 lightSector;
    s32 i;
    Gobj *gobj;
    s32 type;
    s16 ref;

    if (sector < gTerrSectorCount && sector >= 0) {
        ts = &gTerrSector[sector];
    } else {
        ts = NULL;
    }
    lightSector = ts->model->display->lightSector;
    if (EGSDisplayGetSectorLightArray(lightSector)) {
        for (i = 0; i < gLevelGameObjCount; i++) {
            gobj = gameObjectsInfo[i].runtime;
            if (gobj != NULL && gobj->npcState == 4 && !(gobj->flags0 & 2)) {
                type = GOBJ_TYPE(i);
                ref = gameObjectsInfo[gobj->id].refId;
                if (type != 0x14 && type != 0x28 && type != 0x6D && type != 0x34) {
                    EGSDisplaySectorLight(lightSector, ref, 0);
                } else {
                    EGSDisplayKillAnimatedLight(lightSector, ref);
                }
            }
        }
    }
    return 9;
}

INCLUDE_ASM("asm/main/nonmatchings/object", iObjectFindNearbyLight);

u16 ObjectDoQuip(s32 id) {
    u16 msg = 0xFFFF;
    s32 xa = gameObjectsInfo[id].pathStart;

    if (xa > 100) {
        SysSoundQueueXA(gSysInfo.level, xa - 100);
        SysSoundPlayQueuedXA(-1, gPlayerOffset);
    } else if (xa > 0) {
        SysSoundQueueXA(gSysInfo.level, xa);
        msg = StringContactMeiXingMsg();
    }
    return msg;
}

ObjectCallback ObjectSetQuipObjectCallback(ObjectCallback cb) {
    ObjectCallback old = D_8010F7CC;
    D_8010F7CC = cb;
    return old;
}

ObjectCallback ObjectSetSpecialObjectCallback(ObjectCallback cb) {
    ObjectCallback old = D_8010F7D0;
    D_8010F7D0 = cb;
    return old;
}

ObjectCallback ObjectSetSpecialAmbientCallback(ObjectCallback cb) {
    ObjectCallback old = D_8010F7D4;
    D_8010F7D4 = cb;
    return old;
}

#ifdef NON_MATCHING
/* Register allocation differs (type copied to a temp; else-branch address copy missing). */
void ObjectUpdateAmbientObjectSetting(s32 id, s32 type) {
    s32 i;
    s32 path = gameObjectsInfo[id].pathStart;

    for (i = 0; i < gLevelGameObjCount; i++) {
        if (type == GOBJ_TYPE(i)) {
            ObjectSetAmbient2(i, path, &gameObjectsInfo[i]);
        }
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/object", ObjectUpdateAmbientObjectSetting);
#endif

INCLUDE_ASM("asm/main/nonmatchings/object", ObjectUpdateAmbientSndObjectSetting);

ObjectCallback ObjectSetAmbientSndIdxCallback(ObjectCallback cb) {
    ObjectCallback old = D_8010F7D8;
    D_8010F7D8 = cb;
    return old;
}

INCLUDE_ASM("asm/main/nonmatchings/object", func_8002CAB0);

void func_8002CD34(Gobj *gobj) {
    VECTOR32 offset;
    VECTOR32 pos;
    VECTOR32 firePos;
    VECTOR32 dir;
    struct EGSFrame *frame = gobj->anim->frame;
    s16 *m = (s16 *)frame;

    pos.vx = frame->pos.vx;
    pos.vy = frame->pos.vy;
    pos.vz = frame->pos.vz;
    pos.vy = -pos.vy;
    dir.vx = m[2];
    dir.vy = m[5];
    dir.vz = m[8];
    dir.vy = -dir.vy;
    offset.vx = EGSMathFIXMul(dir.vx, 0xC0);
    offset.vy = EGSMathFIXMul(dir.vy, 0xC0);
    offset.vz = EGSMathFIXMul(dir.vz, 0xC0);
    firePos.vx = pos.vx - offset.vx;
    firePos.vy = pos.vy - offset.vy;
    firePos.vz = pos.vz - offset.vz;
    ParticleFireShow(gobj->id, &firePos, 0x400, 1);
}

void func_8002CE20(Gobj *gobj) {
    VECTOR32 offset;
    VECTOR32 pos;
    VECTOR32 firePos;
    VECTOR32 dir;
    struct EGSFrame *frame;
    s16 *m;
    s32 count;

    if (gobj->flags1 & 0x7F) {
        frame = gobj->anim->frame;
        m = (s16 *)frame;
        pos.vx = frame->pos.vx;
        pos.vy = frame->pos.vy;
        pos.vz = frame->pos.vz;
        pos.vy = -pos.vy;
        dir.vx = m[2];
        dir.vy = m[5];
        dir.vz = m[8];
        dir.vy = -dir.vy;
        offset.vx = EGSMathFIXMul(dir.vx, 0xC0);
        offset.vy = EGSMathFIXMul(dir.vy, 0xC0);
        offset.vz = EGSMathFIXMul(dir.vz, 0xC0);
        firePos.vx = pos.vx - offset.vx;
        firePos.vy = pos.vy - offset.vy;
        firePos.vz = pos.vz - offset.vz;
        ParticleFireShow(gobj->id, &firePos, 0x400, 0);
        func_8002BE44(gobj, &D_8010FFA8, &D_8010FFAC);
        count = (gobj->flags1 & 0x7F) - 1;
        gobj->flags1 = (gobj->flags1 & 0x80) | (count & 0x7F);
    }
}

void func_8002CF58(Gobj *gobj) {
    if (!(gobj->flags1 & 0x7F)) {
        ParticleFireHide(gobj->id);
        func_8002BECC(gobj, &D_8010FFA8, D_8010FFAC);
        gobj->flags1 = (gobj->flags1 & 0x80) | 1;
    }
}

void func_8002CFC4(Gobj *gobj, u32 state) {
    GameObjectAnim *anim = gobj->anim;
    s32 ref = gameObjectsInfo[gobj->id].refId;
    Gobj *target;

    GOBJ_STATE(gobj) = state;
    if (anim->unk00[0xA] & 8) {
        EGSAnimShowDetailDef(anim);
    }
    if (ref != -1) {
        target = gameObjectsInfo[ref].runtime;
        if (target->flags0 & 2) {
            SysPostMessage(0x14, 5, gobj->id, ref, 0, 0, 0, 0);
        }
        if (GOBJ_STATE(target) < state) {
            GOBJ_STATE(target) = state;
            if (GOBJ_TYPE(ref) == 0x34 && state == 0) {
                if (target->anim->unk00[0xA] & 8) {
                    EGSAnimShowDetailDef(target->anim);
                }
                target->flags0 |= 0x80;
                GOBJ_HEALTH(target)->health = 0;
            }
            GOBJ_HEALTH(target)->unk0C = 0;
            GOBJ_HEALTH(target)->lastAttacker = gobj->id;
            DamageGOBJ(-1, gobj->id, target->id, GOBJ_HEALTH(target)->health, 0xF);
        }
    }
}

#ifdef NON_MATCHING
/* One register: the else branch reads gobj->id through the part copy (a1) instead of a3. */
void func_8002D190(Gobj *gobj, s16 type) {
    GameObjectHealth *armor = gobj->healthInfo;
    s32 state;
    s32 old;
    Gobj *main;
    Gobj *part;
    struct EGSFrame *frame;
    s16 *m;
    VECTOR32 dir;
    VECTOR32 pos;
    s32 hp = armor->armor;
    s32 third = hp / 3;
    s32 twoThirds = hp * 2 / 3;

    if (armor->health < third) {
        state = 2;
    } else {
        state = armor->health < twoThirds;
    }
    if (type == 0x2C) {
        main = gobj;
        part = ObjectMatchToRefID(main->id, 0x38);
        part->healthInfo->health = main->healthInfo->health;
    } else {
        part = gobj;
        main = gameObjectsInfo[gameObjectsInfo[gobj->id].refId].runtime;
        main->healthInfo->health = part->healthInfo->health;
    }
    old = GOBJ_STATE(main);
    if (state != old) {
        frame = main->anim->frame;
        m = (s16 *)frame;
        GOBJ_STATE(part) = state;
        GOBJ_STATE(main) = state;
        pos.vx = frame->pos.vx;
        pos.vy = frame->pos.vy;
        pos.vz = frame->pos.vz;
        pos.vy = -pos.vy;
        dir.vx = m[2];
        dir.vy = m[5];
        dir.vz = m[8];
        dir.vy = -dir.vy;
        if (old == 0) {
            func_8002CD34(main);
        }
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/object", func_8002D190);
#endif

INCLUDE_RODATA("asm/main/nonmatchings/object", D_80010F60);

INCLUDE_ASM("asm/main/nonmatchings/object", func_8002D328);

void func_8002D790(Gobj *gobj) {
    s16 *m = (s16 *)gobj->anim->frame;

    m[1] = 0;
    m[7] = 0;
    m[3] = 0;
    m[5] = 0;
    MatrixNormal_0(m, m);
}

void ANIMATEDObjDispatch(void) {
}

void EFFECTSObjDispatch(SysMsg *msg) {
    Gobj *gobj = gameObjectsInfo[msg->to].runtime;
    s32 type = GOBJ_TYPE(gobj->id);
    VECTOR32 pos;

    switch (msg->id) {
    case 6:
        gobj->flags0 |= 0x40;
        if (type != 0x75) {
            pos.vx = gobj->anim->frame->pos.vx;
            pos.vy = gobj->anim->frame->pos.vy;
            pos.vz = gobj->anim->frame->pos.vz;
            pos.vy = -pos.vy;
            if (type == 0x36) {
                ParticleFireShow(gobj->id, &pos, 0xC00, 1);
            } else if (type == 0x53) {
                ParticleFireShow(gobj->id, &pos, 0x1000, 2);
            }
        } else {
            if (D_8010FFB4 == 0) {
                GameCameraSetScrimScrollRate(gobj->flags1);
            }
            D_8010FFB4++;
        }
        break;
    case 5:
        gobj->flags0 &= ~0x40;
        if (type != 0x75) {
            ParticleFireHide(gobj->id, type, gobj); /* extra args only match register use */
        } else if (--D_8010FFB4 <= 0) {
            GameCameraSetScrimScrollRate(0);
            D_8010FFB4 = 0;
        }
        break;
    case 2:
        func_801486DC(gobj);
        break;
    }
}

INCLUDE_RODATA("asm/main/nonmatchings/object", D_80010FD0);

INCLUDE_RODATA("asm/main/nonmatchings/object", D_80010FDC);

INCLUDE_ASM("asm/main/nonmatchings/object", EFFECTS2ObjDispatch);

INCLUDE_ASM("asm/main/nonmatchings/object", FIREObjDispatch);

void GENObjDispatch(SysMsg *msg) {
    Gobj *gobj = gameObjectsInfo[msg->to].runtime;

    switch (msg->id) {
    case 6:
        CobjAddBulletCollisionBoxGobj(gobj, 1, 0, 0);
        CobjAddCollisionBoxGobj(gobj, 1, 0, 1, 0);
        break;
    case 5:
        CobjRemoveBulletCollisionBoxGobj(gobj);
        CobjRemoveCollisionBoxGobj(gobj);
        break;
    case 2:
        func_80148A00(gobj);
        break;
    }
}

INCLUDE_ASM("asm/main/nonmatchings/object", GLASSObjDispatch);

INCLUDE_ASM("asm/main/nonmatchings/object", LASERObjDispatch);

INCLUDE_ASM("asm/main/nonmatchings/object", LIGHTObjDispatch);

void ROTATINGObjDispatch(SysMsg *msg) {
    Gobj *gobj = gameObjectsInfo[msg->to].runtime;
    VECTOR32 rotVel;

    switch (msg->id) {
    case 6:
        CobjAddBulletCollisionBoxGobj(gobj, 1, 0, 0);
        gobj->flags0 &= 0x7F;
        MovementAddGobj(gobj);
        rotVel.vy = 0;
        rotVel.vx = 0;
        rotVel.vz = 0x2AAAAA;
        ((MoveInfoRot *)gobj->move)->rotVel = rotVel;
        break;
    case 5:
        CobjRemoveBulletCollisionBoxGobj(gobj);
        MovementRemoveGobj(gobj);
        break;
    case 2:
        func_80149280(gobj);
        break;
    }
}

INCLUDE_ASM("asm/main/nonmatchings/object", SPECIALObjDispatch);

INCLUDE_ASM("asm/main/nonmatchings/object", TGTObjDispatch);
