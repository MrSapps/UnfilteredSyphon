#include "common.h"
#include "system.h"
#include "gameobj.h"
#include "gobj.h"
#include "egsmath.h"

/* damage.c's small data, still assembled from asm */
SDATA_LOCAL(D_8010F980)
SDATA_LOCAL(D_8010F984)
SDATA_LOCAL(D_8010F988)
SDATA_LOCAL(D_80110170)
SDATA_LOCAL(D_80110174)
SDATA_LOCAL(D_80110178)
SDATA_LOCAL(D_8011017C)
SDATA_LOCAL(D_80110180)
SDATA_LOCAL(D_80110184)

typedef void (*DamageCallback)(s16 id, s16 by);

extern u8 D_8010F988; /* bone angle limits not yet computed */
extern s32 D_80110170; /* cos 40 deg: side limits */
extern s32 D_80110174;
extern s32 D_80110178; /* cos 65 deg: front/back limits */
extern s32 D_8011017C;
extern s32 D_801069F0[4]; /* body bones */
extern s32 D_80106A00[4]; /* side bones */
extern s32 D_80106A10[4]; /* other side bones */
extern s32 D_80106A20[4]; /* front/back bones */
int rand(void);
int rcos(int a);

/* Display list of animated models (see object.c) */
typedef struct DamageDisplayAnim {
    /* 0x00 */ u8 unk00[0xC];
    /* 0x0C */ EGSFrame *frame;
    /* 0x10 */ u8 unk10[4];
    /* 0x14 */ u16 id; /* 0x2000: game object, gameObjectsInfo index in the low 10 bits */
} DamageDisplayAnim;

typedef struct DamageDisplayNode {
    /* 0x00 */ DamageDisplayAnim *anim;
    /* 0x04 */ u8 unk04[4];
    /* 0x08 */ struct DamageDisplayNode *next;
} DamageDisplayNode;

typedef struct DamageDisplay {
    /* 0x00 */ u8 unk00[0x14];
    /* 0x14 */ s32 index; /* camera index into anims' visibility words */
    /* 0x18 */ u8 unk18[0x74];
    /* 0x8C */ DamageDisplayNode *anims;
} DamageDisplay;

extern VECTOR32 gExplosionPosition;
extern VECTOR32 D_8011E8D8; /* position of the hit being applied */
u8 GameCameraCheckMode(s32 mode);
void ParticleTaser(VECTOR32 *pos);
extern s32 particleTaserState;
void NPCPutFireOnNPCMove(s32 id);
void EGSGeometry_GetFuzzyDistancePoint2Point(VECTOR32 *a, VECTOR32 *b, s32 *dist);

extern DamageCallback D_8010F980; /* damaged callback */
extern DamageCallback D_8010F984; /* destroyed callback */
extern u8 D_80110180; /* first pending damage packet, DAMAGE_PACKET_NONE for none */
extern u8 D_80110184;
extern void *gBodyArmorHitMsg;
extern void *D_801077F4; /* flak jacket hit message for level 4 */

/* GameObjectHealth with the fields used here */
typedef struct GobjHealth {
    /* 0x00 */ u8 unk00[2];
    /* 0x02 */ s16 material; /* bullet texture kind */
    /* 0x04 */ s16 lastAttacker;
    /* 0x06 */ s16 armor;
    /* 0x08 */ s16 health;
    /* 0x0A */ s16 lastAttackerBy;
    /* 0x0C */ s16 unk0C; /* pending damage */
    /* 0x0E */ s16 damageType;
} GobjHealth;

/* TargetInfo aim points */
typedef struct DamageTarget {
    /* 0x00 */ s16 targetId;
    /* 0x02 */ u8 unk02[0x5A];
    /* 0x5C */ s16 targetId2;
    /* 0x5E */ u8 unk5E[6];
    /* 0x64 */ VECTOR32 targetPos; /* aim point on the target */
    /* 0x74 */ VECTOR32 aimPos;    /* aim point otherwise */
    /* 0x84 */ VECTOR32 targetDir;
    /* 0x94 */ VECTOR32 aimDir;
} DamageTarget;
#define GOBJ_HEALTH(g) ((GobjHealth *)(g)->healthInfo)

/* A thrown grenade (passed to its fuse timer callback). */
typedef struct Grenade {
    /* 0x00 */ VECTOR32 pos;
    /* 0x10 */ s32 owner; /* gameObjectsInfo index of the thrower */
    /* 0x14 */ s32 fuse;
    /* 0x18 */ u8 active;
} Grenade;

extern u8 D_80106A28[];
extern s32 gPlayerOffset;
extern s32 gSniperMode;
extern u8 npcGrenadeLandedCheck;
extern u32 D_801061E0; /* gWeaponInfo[0x14] (grenade) word 0x14: bits 6..15 = blast damage */

s32 BulletTextureGet(s16 material);
void EGSDisplayRegisterDamage(void *arg0, void *arg1, void *arg2, s32 tex, Gobj *gobj);
void SniperSetMode(s32 mode);
void NPCDropPickupStuff(s16 id);
void EGSAnimShowDetailDef(GameObjectAnim *anim);
s32 DamageRadius(s16 id, VECTOR32 *pos, s32 damage, s32 inner, s32 outer, s32 kind, u8 self);
void SysExeTickQueueTimer(void (*cb)(), s32 ticks, void *arg);
void DamageGOBJ(s16 from, s16 by, s16 to, s16 amount, s16 type);
void DamageGobjCAS(s16 id, s32 action);
void CASRequestAction(s32 id, s32 action);
void func_80069C70(Grenade *g);
/* NPC state (GameObjectRuntime.npc) fields used here */
typedef struct DamageNPC {
    /* 0x00 */ u8 unk00[0x20];
    /* 0x20 */ s32 flags;
    /* 0x24 */ u8 unk24[0x28];
    /* 0x4C */ s8 unk4C; /* -1: dies without a death animation */
} DamageNPC;

extern u8 NPCEndActivationFlag;
extern VECTOR32 D_80012164; /* gas cloud particle direction (rodata) */
extern Grenade D_8011EAE8;  /* the gas cloud's damage ticker */

void DamageDestroyImmediately(s32 by, s32 id);
void NPCPutFireOnNPCShow(s16 id);
void SysPlaySong(s32 arg0, s32 song, s32 arg2, void *arg3);
void ParticleExplosion(s16 id, VECTOR32 *pos, void *dir, s32 size, s32 arg4);
void ParticleFire(s16 id, VECTOR32 *pos, s32 arg2, s32 color, s32 arg4, s32 arg5, s32 arg6);
/* Delayed bullet impact (D_8011E8E8[], 0x20 bytes each) */
typedef struct DamagePacket {
    /* 0x00 */ s32 x;
    /* 0x04 */ s32 z;
    /* 0x08 */ s16 y;
    /* 0x0A */ s16 nx;
    /* 0x0C */ s16 ny;
    /* 0x0E */ s16 nz;
    /* 0x10 */ s16 target; /* -1: terrain */
    /* 0x12 */ s16 unk12;
    /* 0x14 */ s16 from;
    /* 0x16 */ s16 unk16;
    /* 0x18 */ s32 kind;
    /* 0x1C */ u8 delay;
    /* 0x1D */ u8 next;
    /* 0x1E */ u8 unk1E;
    /* 0x1F */ u8 pad1F;
} DamagePacket;

extern s32 gCurrentWeapon;

/* GameObjectInfo.flags24 low byte: NPC weapon */
#define GOBJINFO_WEAPON(info) (*(u8 *)&(info)->flags24)
#define GOBJINFO_WORD24(info) (*(u32 *)&(info)->flags24)
/* Weapon an object holds (no player special case), as one expression (see weapon.c) */
#define GOBJINFO_NPC_WEAPON(info)                                                            \
    (GOBJINFO_WEAPON(info) != 0                ? GOBJINFO_WEAPON(info)                        \
     : (GOBJINFO_WORD24(info) & 0x3000) != 0x1000 ? ((GOBJINFO_WORD24(info) & 0x3000) == 0x2000 ? 0x14 : 0) \
                                               : 0x13)
#define GOBJ_WEAPON_IS(g, w)                                                                         \
    ((g)->id != gPlayerOffset ? (GOBJINFO_WEAPON(&gameObjectsInfo[(g)->id]) != 0 &&                   \
                                 GOBJINFO_WEAPON(&gameObjectsInfo[(g)->id]) == (w))                   \
                              : gCurrentWeapon == (w))

#define DAMAGE_PACKET_NONE 0x10

extern DamagePacket D_8011E8E8[DAMAGE_PACKET_NONE];

void DamageGobjCAS2(s16 target, s16 from, s16 by, s32 amount, s32 kind, s32 aimed, VECTOR32 *pos, VECTOR32 *normal);
void func_800667C4(DamagePacket *packet, s32 prev); /* frees a packet */
void ParticleGrenadeLauncher(s16 id, VECTOR32 *pos);
void ParticleTracer(Gobj *gobj, VECTOR32 *pos, s32 arg2);
DamagePacket *func_80066714(void); /* allocates a damage packet */
s32 ParticleDamageDelay(s32 id, VECTOR32 *pos);
void iDamageShowParticle(s32 id, Gobj *gobj, s32 kind, VECTOR32 *pos, VECTOR32 *normal);
extern VECTOR32 D_80012174; /* explosion camera shake vector (rodata) */
extern u32 D_801061C0;      /* gWeaponInfo[0x13] word 0x14: bits 6..15 = blast damage */

void WeaponDoFiredCallback(s32 id, s32 weapon);
void GameCameraShakeStart(void *state, s32 ticks, s32 arg2, void *vec, s32 arg4, void *pos);
void TextAddMarker(u16 id, void *msg, s32 arg2, s32 arg3);
void NPCSoundGobj(s32 arg0, void *arg1, void *arg2, s32 arg3);
void ParticleHitWall(void *arg0, void *arg1, void *arg2, s32 color);

void DamagePacketUpdate(void) {
    VECTOR32 pos;
    VECTOR32 normal;
    DamagePacket *packet;
    Gobj *gobj;
    s32 i;
    s32 prev = DAMAGE_PACKET_NONE;

    for (i = D_80110180; i != DAMAGE_PACKET_NONE; i = packet->next) {
        packet = &D_8011E8E8[i];
        if (--packet->delay == 0) {
            pos.vx = packet->x;
            pos.vy = packet->y;
            pos.vz = packet->z;
            normal.vx = packet->nx;
            normal.vy = packet->ny;
            normal.vz = packet->nz;
            if (packet->target < 0) {
                iDamageShowParticle(packet->from, 0, packet->kind, &pos, &normal);
            } else {
                gobj = gameObjectsInfo[packet->target].runtime;
                if ((*(s32 *)gobj->anim != 0 || packet->target == gPlayerOffset) &&
                    (gobj->npcState != 2 || (((DamageNPC *)gobj->npc)->flags & 0x200))) {
                    DamageGobjCAS2(packet->target, packet->from, packet->unk16, packet->unk12, packet->kind,
                                   packet->unk1E, &pos, &normal);
                }
            }
            func_800667C4(packet, prev);
        } else {
            prev = i;
        }
    }
}

void func_800669B0(VECTOR32 *from, Gobj *gobj, VECTOR32 *out) {
    VECTOR32 pos;

    pos.vx = gobj->anim->frame->pos.vx;
    pos.vy = gobj->anim->frame->pos.vy;
    pos.vz = gobj->anim->frame->pos.vz;
    pos.vy = -pos.vy;
    out->vx = from->vx - pos.vx;
    out->vy = from->vy - pos.vy;
    out->vz = from->vz - pos.vz;
}

#define DAMAGE_FIX(x) ((x) >= 0 ? (x) >> 12 : -(-(x) >> 12))

/* Pick the bone a bullet coming along dir hits: by the angle to the object's facing. */
s32 WhichBoneIsHit(GameObjectAnim *anim, VECTOR32 *dir, s32 **bones, s32 *count) {
    VECTOR32 d;
    VECTOR32 side;
    VECTOR32 facing;
    s32 dot[2];
    s32 r = rand() & 3;
    s32 hit = 0;

    facing.vx = ((EGSFrame *)anim->frame)->m[0][2];
    facing.vy = ((EGSFrame *)anim->frame)->m[1][2];
    facing.vz = ((EGSFrame *)anim->frame)->m[2][2];
    facing.vy = -facing.vy;
    if (D_8010F988) {
        D_80110170 = rcos(0x1C7);
        D_80110174 = rcos(0x1C7);
        D_80110178 = rcos(0x2E3);
        D_8011017C = rcos(0x2E3);
        D_8010F988 = 0;
    }
    d = *dir;
    d.vy = 0;
    GTEMath_VectorNormal((VECTOR *)&d, (VECTOR *)&d);
    facing.vy = 0;
    GTEMath_VectorNormal((VECTOR *)&facing, (VECTOR *)&facing);
    side.vx = facing.vz;
    side.vy = 0;
    side.vz = -facing.vx;
    *count = 4;
    dot[0] = d.vx * side.vx + d.vy * side.vy + d.vz * side.vz;
    dot[0] = DAMAGE_FIX(dot[0]);
    if (dot[0] >= D_80110174) {
        hit |= 2;
    } else if (-D_80110170 >= dot[0]) {
        hit |= 1;
    }
    dot[0] = d.vx * facing.vx + d.vy * facing.vy + d.vz * facing.vz;
    dot[0] = DAMAGE_FIX(dot[0]);
    if (dot[0] >= D_8011017C) {
        hit |= 8;
    } else if (-D_80110178 >= dot[0]) {
        hit |= 4;
    }
    if (hit & 2) {
        *bones = D_80106A00;
        return (r & 1) ? 5 : 6;
    }
    if (hit & 1) {
        *bones = D_80106A10;
        return (r & 1) ? 0xB : 0xC;
    }
    if (hit & 0xC) {
        *bones = D_80106A20;
        *count = 2;
        return (r & 1) ? 0xE : 0xD;
    }
    *bones = D_801069F0;
    return D_801069F0[r];
}

void iDamageShowParticleGlassSpark(void *arg0, void *arg1, void *arg2) {
    NPCSoundGobj(1, arg0, arg1, 0x1E0);
    ParticleHitWall(arg0, arg1, arg2, 0xC0FFFF);
}

extern VECTOR32 D_80012154; /* zero vector (rodata) */
extern u8 gTargetModeFlag;
extern u8 gPlayerOnFireFlag;
void NPCDamagedByPC(s32 id);
void DamageShowObjectBulletHole(s16 id, void *pos, void *normal);
s32 NPCDebugGetNoDamageFlag(void);
s32 NPCKill(s32 id, VECTOR32 *dir);
void EGSMathFIX_VectorScaleToLength(VECTOR32 *v, s32 length, VECTOR32 *out);
void DamageDeathCAS(s16 id, s32 action);
void AnimateGobjRequestHit(Gobj *gobj, VECTOR32 *dir, u8 mode);
void AnimateGobjElectrocute(Gobj *gobj);
void DamageShowFlakJacketMsg();

/* MoveInfo fields used here */
typedef struct DamageMoveInfo {
    /* 0x00 */ u8 unk00[0x70];
    /* 0x70 */ VECTOR32 impulse;
} DamageMoveInfo;

INCLUDE_RODATA("asm/main/nonmatchings/damage", D_80012144);

INCLUDE_RODATA("asm/main/nonmatchings/damage", D_80012154);

INCLUDE_RODATA("asm/main/nonmatchings/damage", D_80012164);

INCLUDE_RODATA("asm/main/nonmatchings/damage", D_80012174);

INCLUDE_ASM("asm/main/nonmatchings/damage", iDamageShowParticle);

DamageCallback DamageSetDamagedCallback(DamageCallback cb) {
    DamageCallback old = D_8010F980;
    D_8010F980 = cb;
    return old;
}

DamageCallback DamageSetDestroyedCallback(DamageCallback cb) {
    DamageCallback old = D_8010F984;
    D_8010F984 = cb;
    return old;
}

void DamageInit(void) {
    D_8010F980 = NULL;
    D_8010F984 = NULL;
    D_80110184 = 0;
    D_80110180 = 0x10;
}

void DamageShowFlakJacketMsg(id)
    u16 id;
{
    TextAddMarker(id, gSysInfo.level != 4 ? D_801077F4 : gBodyArmorHitMsg, 0x28, 0);
}

/* The object has a flak jacket (the player always does). */
#define GOBJ_HAS_FLAK(g) ((g)->id == gPlayerOffset || (GOBJINFO_WORD24(&gameObjectsInfo[(g)->id]) & 0x4000))

#define DAMAGE_KIND_FLAK 0x17

/* Apply a hit: flak jacket, health, then the death or hit reaction, and tell everyone. */
void DamageGobjCAS2(s16 id, s16 from, s16 by, s32 amount, s32 kind, s32 aimed, VECTOR32 *pos, VECTOR32 *normal) {
    VECTOR32 dir;
    VECTOR32 push;
    VECTOR32 hitDir;
    Gobj *gobj = gameObjectsInfo[id].runtime;
    GobjHealth *health = GOBJ_HEALTH(gobj);
    s32 type = health->damageType;
    u8 hurt = 1;
    s32 weapon;
    s32 armor;
    s32 action;
    s32 killed;
    DamageCallback cb;

    if (from == gPlayerOffset && gobj->npcState == 2) {
        NPCDamagedByPC(id);
    }
    if (aimed) {
        DamageShowObjectBulletHole(id, pos, normal);
    }
    weapon = gameObjectsInfo[from].runtime->id != gPlayerOffset
                 ? GOBJINFO_NPC_WEAPON(&gameObjectsInfo[gameObjectsInfo[from].runtime->id])
                 : gCurrentWeapon;
    if (GOBJ_HEALTH(gobj)->armor != 0 && (GOBJ_HAS_FLAK(gobj) || id == gPlayerOffset) &&
        (from != gPlayerOffset || gSniperMode == 0 || amount < 0) && (from != -1 || type == 0x4C) &&
        (u32)(weapon - 0x10) >= 2 && weapon != 0x13 && amount != 0x7FFF) {
        kind = DAMAGE_KIND_FLAK;
    } else if (gobj->npcState == 2 && GOBJ_HAS_FLAK(gobj) && type == 0x11) {
        GOBJ_HEALTH(gobj)->armor = 0;
    }
    if (type != 0x4C && type != 0x51) {
        iDamageShowParticle(from, gobj, kind, pos, normal);
    }
    if ((u16)(GOBJ_HEALTH(gobj)->health + 1) < 2) {
        return;
    }
    if (GOBJ_HEALTH(gobj)->health != 0x7FFF) {
        if (kind == DAMAGE_KIND_FLAK) {
            armor = GOBJ_HEALTH(gobj)->armor;
            if (amount < 0) {
                amount = -amount;
            }
            if (from == gPlayerOffset) {
                DamageShowFlakJacketMsg(id);
            }
            armor -= amount;
            hurt = 0;
            if (armor < 0) {
                armor = 0;
            }
            GOBJ_HEALTH(gobj)->armor = armor;
        }
        if (hurt) {
            if (amount < GOBJ_HEALTH(gobj)->health) {
                health->health -= amount;
            } else if (NPCDebugGetNoDamageFlag() && id == gPlayerOffset) {
                GOBJ_HEALTH(gobj)->health = 150;
            } else {
                health->health = 0;
            }
        }
        if (GOBJ_HEALTH(gobj)->health == 0) {
            if (gobj->cas != NULL) {
                killed = 0;
                if (id != gPlayerOffset) {
                    action = NPCKill(id, &dir);
                    if (action) {
                        type = action;
                        killed = 1;
                    }
                } else {
                    CASRequestAction(id, 0xD);
                    gTargetModeFlag = 0;
                }
                CASRequestAction(id, type);
                if ((killed || type == 0x11) && (u32)(gobj->npcState - 1) < 2) {
                    if (killed) {
                        if (type == 0x11) {
                            EGSMathFIX_VectorScaleToLength(&dir, 0x18000, &dir);
                            dir.vy += 0x18000;
                        } else {
                            EGSMathFIX_VectorScaleToLength(&dir, 0xB320, &dir);
                            dir.vy += 0xB320;
                        }
                        push = dir;
                    } else if (from != -1) {
                        func_800669B0(pos, gameObjectsInfo[from].runtime, &dir);
                        dir.vx = -dir.vx;
                        dir.vy = -dir.vy;
                        dir.vz = -dir.vz;
                        dir.vy = 0;
                        GTEMath_VectorNormal((VECTOR *)&dir, (VECTOR *)&dir);
                        push.vx = EGSMathFIXMul(dir.vx, -0x18000);
                        push.vy = EGSMathFIXMul(dir.vy, -0x18000);
                        push.vz = EGSMathFIXMul(dir.vz, -0x18000);
                        push.vy += 0x1C000;
                    } else {
                        func_800669B0(&gExplosionPosition, gobj, &dir);
                        dir.vy = 0;
                        GTEMath_VectorNormal((VECTOR *)&dir, (VECTOR *)&dir);
                        push.vx = EGSMathFIXMul(dir.vx, -0x18000);
                        push.vy = EGSMathFIXMul(dir.vy, -0x18000);
                        push.vz = EGSMathFIXMul(dir.vz, -0x18000);
                        push.vy += 0x20000;
                    }
                    ((DamageMoveInfo *)gobj->move)->impulse.vx += push.vx;
                    ((DamageMoveInfo *)gobj->move)->impulse.vy += push.vy;
                    ((DamageMoveInfo *)gobj->move)->impulse.vz += push.vz;
                }
                if ((u32)(gobj->npcState - 1) < 2 && gobj->id != gPlayerOffset && type != 0x58 && rand() % 10 >= 7) {
                    SysPlaySong(1, 0x17, (s32)gobj, NULL);
                }
            } else {
                DamageDeathCAS(id, type);
            }
            cb = D_8010F984;
        } else {
            if ((u32)(gobj->npcState - 1) < 2) {
                hitDir = D_80012154;
                if (from != -1) {
                    func_800669B0(pos, gameObjectsInfo[from].runtime, &hitDir);
                }
                if (gobj->id == gPlayerOffset && !gPlayerOnFireFlag) {
                    SysPlaySong(1, 0x16, (s32)gobj, NULL);
                }
                if (gobj->id == gPlayerOffset) {
                    if (!gPlayerOnFireFlag) {
                        if (hitDir.vx != 0 || hitDir.vy != 0 || hitDir.vz != 0) {
                            AnimateGobjRequestHit(gobj, &hitDir, 0);
                        } else {
                            AnimateGobjElectrocute(gobj);
                        }
                    }
                } else if (hitDir.vx != 0 || hitDir.vy != 0 || hitDir.vz != 0) {
                    AnimateGobjRequestHit(gobj, &hitDir, 1);
                }
            }
            cb = D_8010F980;
        }
        if (cb != NULL) {
            cb(id, by);
        }
    }
    SysPostMessage(0xD, 4, by, id, 0, 0, 0, 0);
}

#ifdef NON_MATCHING
/* Register allocation: the original keeps the raw id in fp and re-extends it per use (all ten callee-saved registers busy) */
/* Hit animation callback: work out where the pending damage hit and from which direction,
 * then apply it now or queue it as a delayed bullet impact. */
void DamageGobjCAS(s16 id, s32 action) {
    VECTOR32 dir;
    u8 aimed = 0;
    DamagePacket *packet = NULL;
    Gobj *gobj = gameObjectsInfo[id].runtime;
    GobjHealth *health = GOBJ_HEALTH(gobj);
    s32 from = health->lastAttacker;
    s32 by = health->lastAttackerBy;
    Gobj *attacker;
    DamageTarget *target;
    s32 delay;

    if (((s32 *)gobj->anim)[((DamageDisplay *)*gCamera)->index] != 0 || gobj->id == gPlayerOffset) {
        if (health->damageType == 0x892) {
            D_8011E8D8.vx = gobj->anim->frame->pos.vx;
            D_8011E8D8.vy = gobj->anim->frame->pos.vy;
            D_8011E8D8.vz = gobj->anim->frame->pos.vz;
            D_8011E8D8.vy = -D_8011E8D8.vy;
            dir = ((DamageTarget *)gobj->target)->targetDir;
        } else if (from != -1) {
            attacker = gameObjectsInfo[from].runtime;
            target = (DamageTarget *)attacker->target;
            aimed = 1;
            if ((from == gPlayerOffset && (GameCameraCheckMode(0xF) || GameCameraCheckMode(0x10))) ||
                id == target->targetId) {
                D_8011E8D8 = target->targetPos;
                dir = target->targetDir;
            } else if (id == target->targetId2) {
                D_8011E8D8 = target->aimPos;
                dir = target->aimDir;
            } else {
                D_8011E8D8.vx = gobj->anim->frame->pos.vx;
                D_8011E8D8.vy = gobj->anim->frame->pos.vy;
                D_8011E8D8.vz = gobj->anim->frame->pos.vz;
                D_8011E8D8.vy = -D_8011E8D8.vy;
                aimed = 0;
                func_800669B0(&D_8011E8D8, attacker, &dir);
            }
        } else {
            if ((u32)(gobj->npcState - 1) < 2) {
                D_8011E8D8.vx = gobj->anim->frames[rand() % 15]->pos.vx;
                D_8011E8D8.vy = gobj->anim->frames[rand() % 15]->pos.vy;
                D_8011E8D8.vz = gobj->anim->frames[rand() % 15]->pos.vz;
                D_8011E8D8.vy = -D_8011E8D8.vy;
            } else {
                D_8011E8D8.vx = gobj->anim->frame->pos.vx;
                D_8011E8D8.vy = gobj->anim->frame->pos.vy;
                D_8011E8D8.vz = gobj->anim->frame->pos.vz;
                D_8011E8D8.vy = -D_8011E8D8.vy;
            }
            if (by != -1) {
                func_800669B0(&D_8011E8D8, gameObjectsInfo[by].runtime, &dir);
            } else {
                dir.vx = (*(EGSFrame **)*gCamera)->pos.vx;
                dir.vy = (*(EGSFrame **)*gCamera)->pos.vy;
                dir.vz = (*(EGSFrame **)*gCamera)->pos.vz;
                dir.vy = -dir.vy;
                dir.vx -= D_8011E8D8.vx;
                dir.vy -= D_8011E8D8.vy;
                dir.vz -= D_8011E8D8.vz;
            }
        }
        if (from == gPlayerOffset && gCurrentWeapon == 0xE && particleTaserState == 0) {
            ParticleTaser(&D_8011E8D8);
        } else if (from < 0) {
            DamageGobjCAS2(id, from, by, health->unk0C, GOBJ_HEALTH(gobj)->material, aimed, &D_8011E8D8, &dir);
        } else {
            delay = ParticleDamageDelay(from, &D_8011E8D8);
            if (delay > 0) {
                packet = func_80066714();
            }
            if (packet != NULL) {
                packet->delay = delay + 1;
                packet->target = id;
                packet->from = from;
                packet->unk16 = by;
                packet->kind = GOBJ_HEALTH(gobj)->material;
                packet->unk1E = aimed;
                packet->unk12 = health->unk0C;
                packet->x = D_8011E8D8.vx;
                packet->y = D_8011E8D8.vy;
                packet->z = D_8011E8D8.vz;
                packet->nx = dir.vx;
                packet->ny = dir.vy;
                packet->nz = dir.vz;
            } else {
                DamageGobjCAS2(id, from, by, health->unk0C, GOBJ_HEALTH(gobj)->material, aimed, &D_8011E8D8, &dir);
            }
            ParticleTracer(gameObjectsInfo[from].runtime, &D_8011E8D8, 0);
        }
        health->lastAttackerBy = -1;
        health->lastAttacker = -1;
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/damage", DamageGobjCAS);
#endif

void DamageDeathCAS(s16 id, s32 action) {
    Gobj *gobj = gameObjectsInfo[id].runtime;

    gobj->healthInfo->health = -1;
    if (id == gPlayerOffset) {
        gobj->healthInfo->armor = 0;
        if (gSniperMode) {
            SniperSetMode(0);
        }
    } else if (gobj->npcState == 2) {
        NPCDropPickupStuff(id);
    }
    if (gobj->anim->unk00[0xA] & 8) {
        EGSAnimShowDetailDef(gobj->anim);
    }
}

void DamageTerrain(s32 id, VECTOR32 *pos, s32 unused, VECTOR32 *normal, s32 kind) {
    Gobj *gobj = gameObjectsInfo[id].runtime;
    DamagePacket *packet;

    if (GOBJ_WEAPON_IS(gobj, 0x10)) {
        ParticleGrenadeLauncher(gobj->id, pos);
        return;
    }
    if (kind == 0x19) {
        ParticleTracer(gobj, pos, 0);
        return;
    }
    packet = func_80066714();
    if (packet != NULL && id >= 0) {
        packet->delay = ParticleDamageDelay(id, pos) + 1;
        packet->kind = kind;
        packet->unk1E = 0;
        packet->target = -1;
        packet->from = id;
        packet->x = pos->vx;
        packet->y = pos->vy;
        packet->z = pos->vz;
        packet->nx = normal->vx;
        packet->ny = normal->vy;
        packet->nz = normal->vz;
        ParticleTracer(gobj, pos, 0);
    } else {
        iDamageShowParticle(id, 0, kind, pos, normal);
    }
}

void DamageShowObjectBulletHole(s16 id, void *arg1, void *arg2) {
    Gobj *gobj = gameObjectsInfo[id].runtime;
    s32 tex;

    if (gobj->unk20[0] & 0x80) {
        tex = BulletTextureGet(GOBJ_HEALTH(gobj)->material);
        if (tex != 0 && gobj->npcState != 2) {
            EGSDisplayRegisterDamage(arg1, arg2, D_80106A28, tex, gobj);
        }
    }
}

/* Damage 'to' (from: attacker gobj id or -1, by: who gets credited). Grenade launcher shots
 * become blasts; the rest go through the hit animation. */
void DamageGOBJ(s16 from, s16 by, s16 to, s16 amount, s16 type) {
    s32 weapon;
    Gobj *target;
    Gobj *attacker;
    s32 before;
    VECTOR32 *pos;

    if (from != -1) {
        if (gameObjectsInfo[from].runtime->id != gPlayerOffset) {
            weapon = GOBJINFO_NPC_WEAPON(&gameObjectsInfo[gameObjectsInfo[from].runtime->id]);
        } else {
            weapon = gCurrentWeapon;
        }
    } else {
        weapon = -1;
    }
    target = gameObjectsInfo[to].runtime;
    if (from != -1) {
        attacker = gameObjectsInfo[from].runtime;
    } else {
        attacker = NULL;
    }
    if ((u32)(weapon - 6) < 2) {
        before = GOBJ_HEALTH(target)->health;
        if (to == ((DamageTarget *)attacker->target)->targetId) {
            pos = &((DamageTarget *)attacker->target)->targetPos;
        } else {
            pos = &((DamageTarget *)attacker->target)->aimPos;
        }
        if ((u32)(target->npcState - 1) >= 2) {
            DamageRadius(from, pos, amount, 0x60, 0x20, 0x11, 0);
        }
        if (before != GOBJ_HEALTH(target)->health) {
            return;
        }
    } else if (weapon == 0x10 && type != 0x892) {
        if (to == ((DamageTarget *)attacker->target)->targetId) {
            pos = &((DamageTarget *)attacker->target)->targetPos;
        } else {
            pos = &((DamageTarget *)attacker->target)->aimPos;
        }
        ParticleGrenadeLauncher(from, pos);
        return;
    }
    if (by == gPlayerOffset || GOBJ_TYPE(to) != GOBJ_TYPE_BOMB) {
        GOBJ_HEALTH(target)->lastAttacker = from;
        GOBJ_HEALTH(target)->lastAttackerBy = by;
        GOBJ_HEALTH(target)->unk0C = (from != -1 && (target->unk20[0] & 0x10)) ? 0 : amount;
        GOBJ_HEALTH(target)->damageType = type;
        if (target->cas != NULL && GOBJ_HEALTH(target)->health > 0) {
            CASRequestAction(to, 0xE);
        } else {
            DamageGobjCAS(to, 0xE);
        }
    }
}

void DamageGOBJHierarchy(s16 id, s16 by) {
    GameObjectInfo *info = gameObjectsInfo;
    Gobj *gobj = info[id].runtime;
    s16 ref = info[id].refId;

    while (ref != -1) {
        if (info[ref].runtime != NULL) {
            DamageGOBJ(-1, by, ref, GOBJ_HEALTH(gobj)->unk0C, 0xF);
        }
        info = gameObjectsInfo;
        ref = info[ref].refId;
    }
}

void DamageDestroyAssociatedObjects(s16 refId, s16 by, s16 type) {
    s32 i;
    Gobj *gobj;
    s16 id;

    for (i = 0; i < gLevelGameObjCount; i++) {
        if (GOBJ_TYPE(i) == type) {
            gobj = gameObjectsInfo[i].runtime;
            if (gobj != NULL) {
                id = gobj->id;
                if (gameObjectsInfo[id].refId == refId) {
                    DamageGOBJ(-1, by, id, gobj->healthInfo->health, 0xF);
                }
            }
        }
    }
}

void DamageDestroyImmediately(s32 by, s32 id) {
    Gobj *gobj = gameObjectsInfo[id].runtime;

    gobj->flags23 |= 2;
    gobj->healthInfo->health = -1;
    if (D_8010F984 != NULL) {
        D_8010F984(id, by);
    }
}

void DamageKillAllPeople(s32 by, s16 kind) {
    s32 i;
    Gobj *gobj;

    for (i = 0; i < gLevelGameObjCount; i++) {
        gobj = gameObjectsInfo[i].runtime;
        if (gobj != NULL && (gobj->npcState == 1 || gobj->npcState == 2)) {
            if (gobj->npcState == 2 && ((DamageNPC *)gobj->npc)->unk4C == -1) {
                DamageDestroyImmediately(by, gobj->id);
            } else {
                DamageGOBJ(-1, by, gobj->id, gobj->healthInfo->health, kind);
                if (kind == 0x11) {
                    NPCPutFireOnNPCShow(gobj->id);
                }
            }
        }
    }
    NPCEndActivationFlag = 1;
}

/* Damage everything displayed (and the player) around pos: full damage within inner, falling off
 * to outer. Big explosions set NPCs on fire. Returns how many objects survived a full hit. */
s32 DamageRadius(s16 id, VECTOR32 *pos, s32 damage, s32 inner, s32 outer, s32 kind, u8 self) {
    VECTOR32 center;
    VECTOR32 at;
    s32 dist;
    Gobj *gobj = gameObjectsInfo[id].runtime;
    DamageDisplayNode *node;
    DamageDisplayAnim *anim;
    Gobj *other;
    s32 idx;
    u16 objId;
    u8 isNpc;
    s32 hits = 0;
    u8 playerSeen = 0;
    s32 amount;

    if (pos == NULL) {
        center.vx = gobj->anim->frame->pos.vx;
        center.vy = gobj->anim->frame->pos.vy;
        center.vz = gobj->anim->frame->pos.vz;
        center.vy = -center.vy;
    } else {
        center = *pos;
    }
    node = ((DamageDisplay *)*gCamera)->anims;
    while (node != NULL || !playerSeen) {
        if (node == NULL) {
            anim = (DamageDisplayAnim *)gObjPlayer->anim;
        } else {
            anim = node->anim;
        }
        objId = anim->id;
        if (objId & 0x2000) {
            idx = objId & 0x3FF;
            other = gameObjectsInfo[idx].runtime;
            isNpc = (u32)(other->npcState - 1) < 2;
            if (idx == gPlayerOffset) {
                playerSeen = 1;
            }
            if ((self || id != idx) && other->healthInfo->health > 0 && other->target != NULL &&
                GOBJ_TYPE(idx) != 2 && (gSysInfo.level != 0x12 || idx == gPlayerOffset) && GOBJ_TYPE(idx) != 2 &&
                (kind != 0x12 || isNpc)) {
                at.vx = anim->frame->pos.vx;
                at.vy = anim->frame->pos.vy;
                at.vz = anim->frame->pos.vz;
                at.vy = -at.vy;
                EGSGeometry_GetFuzzyDistancePoint2Point(&at, &center, &dist);
                if (dist <= 0) {
                    dist = 1;
                }
                if (dist <= inner) {
                    DamageGOBJ(-1, id, idx, damage, kind);
                    if (inner >= 0x100 && dist < inner >> 1 && kind == 0x11) {
                        if (isNpc && !(((DamageNPC *)other->npc)->flags & 4)) {
                            NPCPutFireOnNPCMove(idx);
                        }
                        if (other->healthInfo->health >= 2) {
                            hits++;
                        }
                    }
                } else if (dist < outer) {
                    amount = damage * (outer - dist) / (outer - inner);
                    DamageGOBJ(-1, id, idx, amount > 0 ? amount : 1, kind);
                }
                gExplosionPosition.vx = center.vx;
                gExplosionPosition.vy = center.vy;
                gExplosionPosition.vz = center.vz;
            }
        }
        if (node != NULL) {
            node = node->next;
        }
    }
    return hits;
}

#ifdef NON_MATCHING
/* Register allocation: id/pos callee-saved regs swapped, hit count not sharing radius's reg. */
void DamageExplosion(s32 id, VECTOR32 *pos, u8 grenade) {
    VECTOR32 dir = D_80012164;
    VECTOR32 shake = D_80012174;
    s32 radius = 0x280;
    s32 radius2 = 0x280;
    s32 hits;
    u8 flags;

    if (grenade) {
        WeaponDoFiredCallback(id, 0x13);
        if (id != -1 && id != gPlayerOffset) {
            radius = 0x140;
            radius2 = 0x140;
        }
    }
    hits = DamageRadius(id, pos, (D_801061C0 >> 6) & 0x3FF, radius, radius2, 0x11, 1);
    ParticleExplosion((hits > 0 || !grenade) ? 0x29A : -1, pos, &dir, 0x1000, 1);
    if (hits > 0) {
        ParticleFire(-1, pos, 0x1000, -1, 0, 0x32, 0x4B);
    }
    GameCameraShakeStart(NULL, 5, 1, &shake, 0, pos);
    if (grenade) {
        flags = npcGrenadeLandedCheck;
        npcGrenadeLandedCheck = id == gPlayerOffset ? flags & ~1 : flags & ~2;
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/damage", DamageExplosion);
#endif

#ifdef NON_MATCHING
/* The else branch should read g from a0, not the callee-saved copy. */
void func_80069C70(Grenade *g) {
    s32 radius;
    u8 flags;

    if (--g->fuse != -1) {
        radius = 0x180 - (u32)((200 - g->fuse) * 3) / 2;
        DamageRadius(g->owner, g, (D_801061E0 >> 6) & 0x3FF, radius, radius, 0x12, 1);
        SysExeTickQueueTimer(func_80069C70, 1, g);
    } else if (g->active) {
        flags = npcGrenadeLandedCheck;
        npcGrenadeLandedCheck = g->owner == gPlayerOffset ? flags & ~1 : flags & ~2;
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/damage", func_80069C70);
#endif

void DamageGasCloud(s32 id, VECTOR32 *pos, s8 active) {
    VECTOR32 dir = D_80012164;

    DamageRadius(id, pos, (D_801061E0 >> 6) & 0x3FF, 0x180, 0x180, 0x12, 1);
    SysPlaySong(0, 0x1D, 0, pos);
    NPCSoundGobj(1, (void *)id, pos, 0x3C0);
    ParticleExplosion(-1, pos, &dir, 0x400, 0);
    ParticleFire(-1, pos, 0x1400, 0x203C5A14, 100, 0, 100);
    D_8011EAE8.pos.vx = pos->vx;
    D_8011EAE8.pos.vy = pos->vy;
    D_8011EAE8.pos.vz = pos->vz;
    D_8011EAE8.owner = id;
    D_8011EAE8.fuse = 200;
    D_8011EAE8.active = active;
    SysExeTickQueueTimer(func_80069C70, 1, &D_8011EAE8);
}
