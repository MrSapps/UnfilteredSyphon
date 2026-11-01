#include "common.h"
#include "weapon.h"
#include "level.h"
#include "system.h"
#include "gobj.h"

/*
 * subway.ovl holds the level code for SUBWAY (func_80142E90) and SUBWAY3 (iSubway3Init),
 * plus the subway train dispatcher (TRAINObjDispatch). A train is a chain of cars: the
 * head car (type 0x1E/0x1F) and the cars linked to it via refId (npcState 7).
 */

/* Object types specific to this overlay */
#define SUBWAY_TYPE_CRATE 0x05      /* comm array object (also an objective) */
#define SUBWAY_TYPE_TRAIN 0x1E      /* train head, SUBWAY */
#define SUBWAY_TYPE_TRAIN3 0x1F     /* train head, SUBWAY3 */
#define SUBWAY_TYPE_GUARD 0x4D      /* killed by the train / blast */
#define SUBWAY_TYPE_FOG 0x73        /* tunnel fog ambient object */
#define SUBWAY_TYPE_VIRUS 0x58      /* the device: blows up the level when destroyed */

#define TRAIN_MAX 2
#define TRAIN_MAX_CARS 5

/* Train system state (D_80145874) */
typedef struct TrainFlags {
    u8 count : 2;   /* trains built */
    u8 current : 2; /* train currently running on the line (SUBWAY3) */
    u8 stopped : 1; /* trains parked */
    u8 reverse : 1; /* SUBWAY3: run the other train next */
    u8 near : 1;    /* the player is next to a moving train */
    u8 shaking : 1; /* camera shake running */
} TrainFlags;

/* SUBWAY flags (D_8014586C) */
typedef struct SubwayFlags {
    u8 npcDead : 1;     /* level-specific NPC killed */
    u8 crateDead : 1;   /* comm array object destroyed */
    u8 switchUsed : 1;  /* power switch thrown */
    u8 inTunnel : 1;    /* tunnel fog on */
    u8 agentMarked : 1; /* CBDC agent marker shown */
} SubwayFlags;

/* SUBWAY3 Aramov chase state (D_80145870) */
typedef struct ChaseFlags {
    s8 side : 2;     /* which side of the platform line Aramov is on */
    u8 waits : 2;    /* checks spent waiting before he stops */
    u8 stopped : 1;
    u8 timing : 1;   /* stop timer running (D_80145830) */
    u8 nearTrain : 1;
    u8 inBox : 1;    /* outside the chase area (or dead) */
} ChaseFlags;

typedef struct SubwayTexAnim {
    u8 unk00[9];
    u8 unk09;
} SubwayTexAnim;

typedef struct SubwayMove {
    /* 0x000 */ VECTOR32 pos;
    /* 0x010 */ u8 unk10[0x180];
    /* 0x190 */ s32 speed;
} SubwayMove;

typedef struct SubwayNPC {
    /* 0x00 */ u8 unk00[0x20];
    /* 0x20 */ s32 flags;
    /* 0x24 */ u8 unk24[4];
    /* 0x28 */ s16 distance; /* to the player */
    /* 0x2A */ u8 unk2A[8];
    /* 0x32 */ s16 unk32;
    /* 0x34 */ u8 unk34[0x13];
    /* 0x47 */ u8 nameMsgId;
} SubwayNPC;

typedef struct SubwayFrame {
    /* 0x00 */ u8 unk00[4];
    /* 0x04 */ s16 rot[8]; /* rotation matrix rows (rot[0], rot[3], rot[6]: facing) */
    /* 0x14 */ VECTOR32 pos; /* y grows downwards */
} SubwayFrame;

typedef struct SubwayLevData {
    /* 0x00 */ u8 unk00;
    /* 0x01 */ u8 fogArg;
    /* 0x02 */ s16 fogDist;
} SubwayLevData;

typedef struct SubwayModel {
    /* 0x00 */ u8 unk00[0x14];
    /* 0x14 */ s32 unk14;
    /* 0x18 */ u8 unk18[0x10];
    /* 0x28 */ u32 flags;
} SubwayModel;

typedef struct SubwayAnim {
    /* 0x00 */ u8 unk00[8];
    /* 0x08 */ u8 unk08;
    /* 0x09 */ u8 unk09;
    /* 0x0A */ u8 flagsA;
    /* 0x0B */ u8 unk0B;
    /* 0x0C */ SubwayFrame *frame;
    /* 0x10 */ SubwayModel *model;
} SubwayAnim;

typedef struct SubwayRuntime {
    /* 0x00 */ u8 flags0;
    /* 0x01 */ u8 flags1; /* train cars: train index */
    /* 0x02 */ s16 id;
    /* 0x04 */ s32 unk04; /* train cars: path speed */
    /* 0x08 */ SubwayAnim *anim;
    /* 0x0C */ SubwayMove *move;
    /* 0x10 */ u8 unk10[8];
    /* 0x18 */ GameObjectHealth *healthInfo;
    /* 0x1C */ SubwayNPC *npc;
    /* 0x20 */ u8 unk20;
    /* 0x21 */ u8 unk21;
    /* 0x22 */ u8 npcState; /* 7: train car */
    /* 0x23 */ u8 flags23;
} SubwayRuntime;

#define CARPOS(t, n) (*(D_80145784[0] + (t) * TRAIN_MAX_CARS + (n)))
#define RT(id) ((SubwayRuntime *)gameObjectsInfo[id].runtime)

typedef struct SubwayDisplayObj {
    /* 0x00 */ u8 unk00[0xC];
    /* 0x0C */ EGSFrame *frame;
    /* 0x10 */ u8 unk10[4];
    /* 0x14 */ u16 idFlags; /* low 10 bits object id, 0x2000 game object */
} SubwayDisplayObj;

typedef struct SubwayDisplay {
    /* 0x00 */ u8 unk00[0x8C];
    /* 0x8C */ EGSListNode *objects;
} SubwayDisplay;

typedef struct SubwayObjectives {
    u8 unk00[0x10];
    void *msg;
} SubwayObjectives;

/*
 * subway.ovl's own data.  The bss items 4 bytes apart are laid out as aspsx 2.56's
 * .lcomm rule does (docs/data_migration.md); aspsx 2.77 and maspsx pack them by size
 * instead, so both builds get explicit padding.  Arrays the original has at 4- or
 * 8-aligned offsets cannot be separate statics (a > 8-byte .lcomm is 16-aligned):
 * they are storage structs, and the code keeps addressing their parts through the
 * original labels (extern + .set below; under _PSYQ psylink's equates give them).
 */
#define BSS_PAD3(n) static u8 n; static s16 n##_2;

/* Mission briefing shown while the level loads (init.ovl reads it through gBriefings). */
typedef struct Briefing {
    /* 0x00 */ char *lines[3];
    /* 0x0C */ s32 count;
    /* 0x10 */ char **objectives;
} Briefing;

static const s32 D_80140FE4[5] = {0, 1, 1, 1, 0}; /* its first word doubles as the comm array use string "" */

static char *D_8014557C[2] = {
    "AGENCY DIRECTIVE:\n\nOur FBI informant has pinpointed the terrorist viral attack in the DC "
    "subway system.  Our contacts within US Defense and Interpol confirm the identities of the "
    "terrorists.  We will drop you in after CBDC (US Army Chemical and Biological Defense Command) "
    "operations begin.\n",
    "Your targets are Rhoemer, Kravitch, Aramov and Girdeux.  SATCOM has intercepted coded "
    "microwave traffic.  Rhoemer must have an uplink station in the area.  Assist CBDC if needed.  "
    "Avoid collateral casualties if possible.  Check your objectives onsite for more information.",
};
Briefing subwayBriefings = {
    {"Washington DC", "Georgia Street Subway", "08:23 22:45\n"}, 2, D_8014557C,
};
static char *D_80145598[2] = {
    "INCOMING FROM LIAN:\n\nGabe, Rhoemer's long gone.  D.C.P.D. reports he's taken out a dozen "
    "officers fifteen blocks east of you and fled the scene.  The entire Garfield Street attack was "
    "just a diversionary tactic.  Washington Park is the key to his plans.\n",
    "You still have a chance to eliminate Aramov.  Don't let her get away.  But remember you can't "
    "use grenades in that tunnel.  Any explosion could damage the tracks and cause a derailment.  "
    "Enough innocent people have already died tonight.  ",
};
Briefing subway3Briefings = {
    {"Washington DC", "Georgia Street Subway", "08:24 00:30\n"}, 2, D_80145598,
};
static char *D_801455B4[5] = {
    "Eliminate Kravitch and destroy comm. array",
    "Protect CBDC bomb squad",
    "Turn off power to terminal security doors",
    "Tag bomb in terminal",
    ".", /* replaced by gEliminateRhoemerMsg (through D_80145608) */
};
static char *D_801455C8[2] = {
    "Do not eliminate any CBDC agent",
    "Avoid damaging viral delivery systems or explosive bombs",
};
static char *D_801455D0[1] = {"Eliminate Aramov"};
static char *D_801455D4[1] = {"Do not use grenades or train lines may be damaged"};
static char *D_801455D8 = "Kravitch"; /* level-specific NPC name */
/* texture animations: source and frames (VRAM x, y) */
static s16 D_801455DC[2] = {0x280, 0x00};
static s16 D_801455E0[8] = {0x290, 0x00, 0x2A0, 0x00, 0x2B0, 0x00, 0x280, 0x20};
static s16 D_801455F0[2] = {0x280, 0x60};
static s16 D_801455F4[8] = {0x2A0, 0x20, 0x290, 0x60, 0x280, 0x40, 0x290, 0x40};
static LevelObjectives D_80145604 = {5, D_801455B4, 2, D_801455C8}; /* SUBWAY objectives/timers save info */
static LevelObjectives D_8014562C = {1, D_801455D0, 1, D_801455D4}; /* SUBWAY3 objectives/timers save info */
static VECTOR32 D_80145654[2] = { /* SUBWAY3 chase area box */
    {-0x4E76, 0x75, -0x461F}, {-0x4D8D, 0x75, -0x455C},
};
static VECTOR32 D_80145674[5] = { /* SUBWAY3 platform line points */
    {-0x2C40, 0x70, -0x1183}, {-0x2D55, 0x70, -0x1385}, {-0x2B63, 0x70, -0x14B4},
    {-0x2D9D, 0x70, -0x12EB}, {-0x2FCA, 0x70, -0x1240},
};
static s32 D_801456C4[4] = {0, 2, 0, 0}; /* train camera shake vector */
static s32 D_801456D4[3] = {0x121, 0x127, 0x12A}; /* SUBWAY3 Aramov taunts (XA) */
extern SubwayObjectives *D_80145608; /* = D_80145604.primaryTexts */

static SubwayTexAnim *D_801456E0;
static SubwayTexAnim *D_801456E4;
static u8 D_801456E8; BSS_PAD3(D_801456E8_pad) /* texture animation 0 running */
static u8 D_801456EC; BSS_PAD3(D_801456EC_pad) /* texture animation 1 running */
typedef struct SubwayLight {
    /* 0x00 */ u8 unk00[0x18];
    /* 0x18 */ s32 unk18; /* D_80145708 */
    /* 0x1C */ u8 unk1C[0x24];
} SubwayLight;
static SubwayLight D_801456F0;     /* train headlight */
extern s32 D_80145708;             /* = D_801456F0.unk18 */
static VECTOR32 D_80145730;        /* SUBWAY3 platform line normal */
static struct {
    u8 shake[0x44];                       /* D_80145740: train camera shake state */
    VECTOR32 carPos[TRAIN_MAX][TRAIN_MAX_CARS]; /* D_80145784 */
} D_80145740_s;
extern u8 D_80145740[];            /* train camera shake state */
extern VECTOR32 D_80145784[TRAIN_MAX][TRAIN_MAX_CARS]; /* car start positions */
static SubwayFrame *D_80145824;    /* headlight frame */
static SubwayRuntime *D_80145828;  /* Aramov */
static s32 D_8014582C;             /* taunts left */
static struct {
    u32 stopTick;                         /* D_80145830 */
    s32 nearDist;                         /* D_80145834 */
    s32 farDist;                          /* D_80145838 */
    u16 carIds[TRAIN_MAX][TRAIN_MAX_CARS]; /* D_8014583C */
    u16 headIds[TRAIN_MAX];               /* D_80145850 */
    u8 carCounts[TRAIN_MAX];              /* D_80145854 */
    VECTOR32 commPos;                     /* D_80145858 */
} D_80145830_s;
extern u32 D_80145830;             /* tick Aramov stops waiting */
extern s32 D_80145834;             /* train near distance */
extern s32 D_80145838;             /* train far distance */
extern u16 D_8014583C[TRAIN_MAX][TRAIN_MAX_CARS]; /* car object ids */
extern u16 D_80145850[TRAIN_MAX];  /* head car object id */
extern u8 D_80145854[TRAIN_MAX];   /* car count */
extern VECTOR32 D_80145858;        /* comm array position */
static SubwayRuntime *D_80145868;  /* comm array object */
static SubwayFlags D_8014586C; BSS_PAD3(D_8014586C_pad)
static ChaseFlags D_80145870; BSS_PAD3(D_80145870_pad)
static TrainFlags D_80145874;
static u8 D_80145875; /* 1: train sound playing */
static s16 D_80145876; /* unused: the map's subway.bss is 0x198 bytes (word padding) */
typedef struct TrainSound {
    u8 playing : 1;
} TrainSound;
#define TRAIN_SOUND ((TrainSound *)&D_80145875)

/* main.exe */
/* gExeTick is used unsigned here (system.h declares it s32) */
#define gExeTickU ((u32)gExeTick)
extern void *gCBCDAgentMsg;
extern void *gCommArrayMsg;
extern void *gLockedMsg;
extern void *gElevatorCallSwitchMsg;
extern void *gPowerSwitchMsg;
extern void *gObjectiveInstructionsMsg;
extern void *gAramovMsg;
extern u8 gEliminateRhoemerMsg[];
extern SubwayLevData *gLevData;
extern u16 gCurrentWeapon;
extern s32 gSniperMode;
extern s32 xaLevelStartIndex[];
extern u8 gObjPathScaleVector[];
void LevelAddingObjectiveInstruction(void);

void EGSDisplayStartTextureAnimation(SubwayTexAnim *anim);
void EGSDisplayStopTextureAnimation(SubwayTexAnim *anim);
SubwayTexAnim *EGSDisplayInitTextureAnimation(void *a, void *b, s32 c, s32 d, s32 e);
void EGSDisplayFlashlightInit(void *light);
void EGSDisplayFlashlightParams(void *light, s32 a, s32 b, s32 c);
void EGSDisplayFlashlightFrame(void *light, SubwayFrame *frame, s32 arg2);
void EGSDisplayFlashlightOn(void *light);
void EGSDisplayFlashlightOff(void *light);
void EGSDisplaySetTerrainFog(s32 dist, s32 arg1);
void EGSFrameSetPosition(SubwayFrame *frame, s32 arg1, VECTOR32 *pos);
s32 GameSysFuzzyMathXZDistance(VECTOR32 *a, VECTOR32 *b);
void EGSGeometry_GetFuzzyDistancePoint2Point(VECTOR32 *a, VECTOR32 *b, s32 *dist);
void EGSGeometry_GetFuzzyXZDistancePoint2Point(VECTOR32 *a, VECTOR32 *b, s32 *dist);
void EGSGeometry_IsIntersectXZPoint2UnrotBox(VECTOR32 *pt, void *box, s32 arg2, u8 *out);
void GameCameraShakeStart(void *state, s32 ticks, s32 arg2, void *vec, s32 arg4, s32 arg5);
void GameCameraShakeStop(void *state);
void GameCameraSetScrimOnFlag(s32 on);
void GameCameraSetScrimScrollRate(s32 rate);
void CameraGetDisplay(void *camera, SubwayDisplay **display);
u16 TextAddMarker(u16 id, void *msg, s32 ticks, s32 arg3);
void DamageGOBJ(s32 arg0, s16 by, s16 id, s16 amount, s32 kind);
void DamageKillAllPeople(s32 by, s32 kind);
void ParticleFireWaveDeath(void);
void SysExeTickQueueTimer(void (*cb)(), s32 ticks, s32 arg);
void GameCamCheckScriptCamDoneCallback(s32 arg);
void LevelSetObjectiveInstructionsCallback(void *(*cb)(void));
void NPCCopDeactivateWashingtonCallback();
void ObjectSetSpecialAmbientCallback(void (*cb)());
void ObjectUpdateAmbientObjectSetting(s32 id, s32 type);
void ObjectDoQuip(s16 id, s32 type);
void NPCSetChaseStopFlag(s16 id, s32 stop);
void NPCSetAwarenessLevel(SubwayRuntime *rt, s32 a, s32 b, s32 c);
void SysSoundSetChaseExciteLevel(s32 level);
void GTEMath_VectorNormal(VECTOR32 *in, VECTOR32 *out);
s32 LevelDoEOL(u8 force);
int rand(void);
void CobjAddBulletCollisionBoxGobj(SubwayRuntime *rt, s32 a, s32 b, s32 c);
void CobjAddCollisionBoxGobj(SubwayRuntime *rt, s32 a, s32 b, s32 c, s32 d);
void CobjRemoveBulletCollisionBoxGobj(SubwayRuntime *rt);
void CobjRemoveCollisionBoxGobj(SubwayRuntime *rt);
void PathHandleEx(SubwayRuntime *rt, void *scale, s32 a, s32 b, s32 c, s32 d, u8 *done);
void PathSetupEX(SubwayRuntime *rt, s32 arg1);
void func_801463A4(s16 id, s32 arg1);   /* LevelCreateGOBJ (init.ovl) */
void func_80149480(s16 id);   /* init.ovl */

static void func_80141648(void);
static void func_801416B8(void);
static void func_80141720(s32 near, s32 far, s32 a, s32 b, s32 c);
static void func_80141A28(s32 train);
static void func_80141B3C(void);
static void func_80141BC8(void);
static SubwayRuntime *func_80141C44(s32 train, SubwayRuntime *obj);
static s32 func_80141DB8(void);
static s16 func_80141F04(s32 id);
static void func_80141FC4(s16 id, s16 by);
static void func_801420D8(SubwayRuntime *rt, s16 by);
#ifdef NON_MATCHING
static void func_801422E4(s16 id, s16 by, s32 arg2, s32 arg3);
#else
void func_801422E4(s16 id, s16 by, s32 arg2, s32 arg3);
#endif
static s32 func_80142648(void);
static void *func_801426FC(void);
static void func_80142724(s16 id);
static s32 func_8014277C(s16 id);
static void func_8014284C(SubwayRuntime *rt);
#ifdef NON_MATCHING
static s32 func_80142934(s16 id);
#else
s32 func_80142934(s16 id);
#endif
static void func_80142A24(s16 id);
static void func_80142B6C(s16 id, s32 type);
static void *func_80142C58(s16 id, s16 type);
static void func_80142C7C(SubwayRuntime *rt, VECTOR32 *pos);
static void *func_80142D74(s16 id, s16 kind);
static void func_80143740(s32 train);
static void func_801438EC(s32 train, u8 fast);
static void func_8014399C(s32 train, u8 notify);
static void func_80143B64(s32 train);
static u8 func_80143CDC(s32 train);
static u8 func_80143D7C(s32 train, VECTOR32 *pt);
static void func_80143E94(void);
static void func_80143EB0(void);
static void func_801446DC(s16 id, s16 by);
static void func_8014476C(s16 by, s16 weapon);
static void func_801447BC(void);
static void func_801447E8(void);

/* Start both texture animations. */
static void func_80141648(void) {
    if (!D_801456E8) {
        EGSDisplayStartTextureAnimation(D_801456E0);
        D_801456E8 = 1;
    }
    if (!D_801456EC) {
        EGSDisplayStartTextureAnimation(D_801456E4);
        D_801456EC = 1;
    }
}

/* Stop both texture animations. */
static void func_801416B8(void) {
    if (D_801456E8) {
        EGSDisplayStopTextureAnimation(D_801456E0);
        D_801456E8 = 0;
    }
    if (D_801456EC) {
        EGSDisplayStopTextureAnimation(D_801456E4);
        D_801456EC = 0;
    }
}

/* Reset the train system and its headlight. */
static void func_80141720(s32 near, s32 far, s32 a, s32 b, s32 c) {
    EGSDisplayFlashlightInit(&D_801456F0);
    EGSDisplayFlashlightParams(&D_801456F0, a, b, c);
    D_80145708 = 0x28;
    D_80145834 = near;
    D_80145838 = far;
    D_80145874.count = 0;
    D_80145874.stopped = 0;
}

/* Build a train from its head car, following the cars linked to it. */
void AnimateMemCardScreen(SubwayRuntime *rt) {
    SubwayRuntime *other;
    GameObjectInfo *info;
    int n;
    int i;
    s16 ref;
    u8 found;
    n = 0;
    D_80145874.current = 0;
    D_80145850[D_80145874.count] = rt->id;
    rt->flags1 = D_80145874.count;
    do {
        D_8014583C[D_80145874.count][n] = rt->id;
        CARPOS(D_80145874.count, n).vx = rt->anim->frame->pos.vx;
        found = 0;
        CARPOS(D_80145874.count, n).vy = rt->anim->frame->pos.vy;
        CARPOS(D_80145874.count, n).vz = rt->anim->frame->pos.vz;
        CARPOS(D_80145874.count, n).vy = -CARPOS(D_80145874.count, n).vy;
        info = gameObjectsInfo;
        for (i = 0; i < gLevelGameObjCount; i++, info++) {
            other = (SubwayRuntime *)info->runtime;
            if (other != NULL && other->npcState == 7) {
                ref = gameObjectsInfo[other->id].refId;
                if (ref != -1 && RT(ref) == rt) {
                    rt = other;
                    found = 1;
                    rt->flags1 = D_80145874.count;
                    break;
                }
            }
        }
        n++;
    } while (found);
    D_80145854[D_80145874.count] = n;
    D_80145874.count++;
}

/* Put a train's cars back at their start positions. */
static void func_80141A28(s32 train) {
    SubwayRuntime *rt;
    int i;
    u16 id;

    for (i = 0; i < D_80145854[train]; i++) {
        id = D_8014583C[train][i];
        rt = RT(id);
        rt->flags0 |= 8;
        EGSFrameSetPosition(rt->anim->frame, 0, &D_80145784[train][i]);
        SysPostMessage(10, 4, id, id, 0, 0, 0, 0);
    }
}

/* Start all trains. */
static void func_80141B3C(void) {
    int i;

    if (!D_80145874.stopped) {
        D_80145874.near = 0;
        D_80145874.shaking = 0;
        D_80145875 &= ~1;
        D_80145874.stopped = 1;
        for (i = 0; i < D_80145874.count; i++) {
            func_80141A28(i);
        }
    }
}

/* Stop all trains. */
static void func_80141BC8(void) {
    if (D_80145874.stopped) {
        if (D_80145874.near) {
            D_80145874.near = 0;
        }
        if (D_80145874.shaking) {
            D_80145874.shaking = 0;
            GameCameraShakeStop(D_80145740);
        }
        D_80145874.stopped = 0;
    }
}

/* Closest active car of a train to an object. */
static SubwayRuntime *func_80141C44(s32 train, SubwayRuntime *obj) {
    VECTOR32 pos;
    VECTOR32 carPos;
    SubwayRuntime *car;
    SubwayRuntime *best;
    s32 bestDist;
    s32 dist;
    int i;
    u8 count;

    bestDist = 0x7FFFFFFF;
    best = NULL;
    count = D_80145854[train];
    pos = obj->move->pos;
    for (i = 0; i < count; i++) {
        car = RT(D_8014583C[train][i]);
        if (car->flags0 & 0x40) {
            carPos.vx = car->anim->frame->pos.vx;
            carPos.vy = car->anim->frame->pos.vy;
            carPos.vz = car->anim->frame->pos.vz;
            carPos.vy = -carPos.vy;
            dist = GameSysFuzzyMathXZDistance(&pos, &carPos);
            if (dist < bestDist) {
                bestDist = dist;
                best = car;
            }
        }
    }
    return best;
}

/* Train whose head car is level with the player. */
static s32 func_80141DB8(void) {
    VECTOR32 pos;
    VECTOR32 heads[TRAIN_MAX];
    SubwayRuntime *head;
    SubwayRuntime *best;
    s32 bestDist;
    s32 dist;
    int i;

    bestDist = 0x7FFFFFFF;
    i = 0;
    best = NULL;
    pos = ((SubwayRuntime *)gObjPlayer)->move->pos;
    for (; i < D_80145874.count; i++) {
        head = RT(D_80145850[i]);
        (heads + i)->vx = head->anim->frame->pos.vx;
        (heads + i)->vy = head->anim->frame->pos.vy;
        (heads + i)->vz = head->anim->frame->pos.vz;
        (heads + i)->vy = -(heads + i)->vy;
        dist = pos.vy - heads[i].vy;
        if (dist < 0) {
            dist = -dist;
        }
        if (dist < bestDist) {
            bestDist = dist;
            best = head;
        }
    }
    return best->flags1;
}

/* Objective of a bomb: 1 on the upper level, 3 below. */
static s16 func_80141F04(s32 id) {
    VECTOR32 pos;
    s32 ret;

    if (id == -1) {
        return 0;
    }
    pos.vx = RT(id)->anim->frame->pos.vx;
    pos.vy = RT(id)->anim->frame->pos.vy;
    pos.vz = RT(id)->anim->frame->pos.vz;
    ret = 3;
    pos.vy = -pos.vy;
    if (pos.vy >= 0x4BA) {
        ret = 1;
    }
    return ret;
}

static void func_80141FC4(s16 id, s16 by) {
    if (by == gPlayerOffset) {
        if (!D_8014586C.agentMarked && gSysInfo.level == 0 && GOBJ_TYPE(id) == GOBJ_TYPE_COP) {
            D_8014586C.agentMarked = 1;
            TextAddMarker(id, gCBCDAgentMsg, 0x28, 0);
        }
        NPCCopDamagedCallback(id, by);
        ObjectBombDamagedCallback(id, by);
    }
}

/* The device blast kills the guards standing next to it. */
static void func_801420D8(SubwayRuntime *rt, s16 by) {
    VECTOR32 pos;
    VECTOR32 other;
    SubwayDisplay *display;
    s32 dist;
    EGSListNode *node;
    SubwayDisplayObj *obj;
    EGSFrame *frame;
    SubwayRuntime *guard;
    s32 id;
    u16 flags;

    if (!(rt->flags0 & 0x80)) {
        pos.vx = rt->anim->frame->pos.vx;
        pos.vy = rt->anim->frame->pos.vy;
        pos.vz = rt->anim->frame->pos.vz;
        pos.vy = -pos.vy;
        CameraGetDisplay(gCamera, &display);
        for (node = display->objects; node != NULL; node = node->next) {
            obj = node->data;
            flags = obj->idFlags;
            frame = obj->frame;
            if ((flags & 0x2000) && rt->id != (id = flags & 0x3FF) && id != GOBJ_ID_666 &&
                gameObjectsModelInfo[gameObjectsInfo[id].modelIndex].type == SUBWAY_TYPE_GUARD) {
                guard = RT(id);
                if (guard->healthInfo->health > 0) {
                    other.vx = frame->pos.vx;
                    other.vy = frame->pos.vy;
                    other.vz = frame->pos.vz;
                    other.vy = -other.vy;
                    EGSGeometry_GetFuzzyDistancePoint2Point(&other, &pos, &dist);
                    if (dist < 0x20 && !(guard->flags0 & 0x80)) {
                        guard->flags0 |= 0x80;
                        DamageGOBJ(-1, by, id, guard->healthInfo->health, 0xF);
                        guard->flags23 |= 2;
                    }
                }
            }
        }
    }
}

/* Weapon an object was destroyed with. */
#define INFO_FLAGS(info) (*(u32 *)&(info)->flags24)

#ifdef NON_MATCHING
/* attacker id lands in $v0 (original $v1) computing its gameObjectsInfo entry; the weapon 6..7 range test is sltiu (original slti) */
static void func_801422E4(s16 id, s16 by, s32 arg2, s32 arg3) {
    VECTOR32 pos;
    SubwayRuntime *rt;
    SubwayRuntime *attacker;
    s16 type;
    s16 ref;
    u16 weapon;
    s32 bits;

    rt = RT(id);
    type = GOBJ_TYPE(id);
    if (type == 1) {
        if (id == gLevelSpecificNpcID && !gDeviceDetonated) {
            D_8014586C.npcDead = 1;
            if (D_8014586C.crateDead) {
                SysPlayXA(0x78, 0, 1);
                LevelObjectiveComplete(0);
            }
        }
    } else if (type == GOBJ_TYPE_COP) {
        if (!gDeviceDetonated) {
            ref = gameObjectsInfo[id].refId;
            NPCCopKilledWashington(id, by, ref, 0, func_80141F04(ref));
        }
    } else if (type == SUBWAY_TYPE_GUARD) {
        if (by != -1) {
            if (by != gPlayerOffset) {
                attacker = RT(by);
                if (rt->npcState == 2) {
                    weapon = *(u8 *)&gameObjectsInfo[attacker->id].flags24;
                    if (*(u8 *)&gameObjectsInfo[attacker->id].flags24 == 0) {
                        bits = INFO_FLAGS(&gameObjectsInfo[attacker->id]) & 0x3000;
                        if (bits != 0x1000) {
                            weapon = bits == 0x2000 ? 0x14 : 0;
                        } else {
                            weapon = 0x13;
                        }
                    }
                } else {
                    weapon = 0;
                }
            } else {
                weapon = gCurrentWeapon;
            }
            if (((gWeaponInfo[(s16)weapon].flags >> 8) & 7) == 1 || ((weapon - 6) & 0xFFFF) < 2) {
                func_801420D8(rt, by);
            }
        }
    } else if (type == GOBJ_TYPE_BOMB) {
        if (!(rt->flags1 & 0x80)) {
            ObjectBombDeath(1, 0x82, 0, arg3);
        }
    } else if (type == SUBWAY_TYPE_VIRUS) {
        gMissionFailed = 1;
        gDeviceDetonated = 1;
        ObjectBigExplosionFX(id, &pos, 0x1000, 5, 10);
        ParticleFireWaveDeath();
        DamageKillAllPeople(gPlayerOffset, 0x11);
    } else if (type == SUBWAY_TYPE_CRATE) {
        D_8014586C.crateDead = 1;
        if (D_8014586C.npcDead) {
            SysPlayXA(0x78, 0, 1);
            LevelObjectiveComplete(0);
        }
    }
}
#else
INCLUDE_ASM("asm/subway/nonmatchings/subway", func_801422E4);
#endif

static s32 func_80142648(void) {
    u32 done;

    if (!gMissionFailed && gObjectives != NULL) {
        done = gObjectives->completed;
        if (done & 2) {
            if (done & 1) {
                if (done & 8) {
                    SysPlayXA(0x172, 0, 1);
                    return 1;
                }
            }
        }
    }
    if (gMissionFailed) {
        StringMissionFailedMsg();
    } else {
        SysPlayXA(0x64, 0, 1);
        LevelObjectivesIncomplete(gObjectivesIncompleteMsg);
    }
    return 0;
}

static void *func_801426FC(void) {
    LevelSetObjectiveInstructionsCallback(NULL);
    return gObjectiveInstructionsMsg;
}

static void func_80142724(s16 id) {
    s16 objective;

    objective = func_80141F04(id);
    SysPlayXA(objective == 1 ? 0x5A : 0x87, 0, 1);
    LevelObjectiveComplete(objective);
}

static s32 func_8014277C(s16 id) {
    s32 ref;

    ref = gameObjectsInfo[id].refId;
    if (GOBJ_TYPE(ref) != 0xB) {
        SysPlayXA(0x7D, 0, 1);
        D_8014586C.switchUsed = 1;
        LevelObjectiveComplete(2);
        return 1;
    }
    return 3;
}

/* Polls whether the player walks past the comm array. */
static void func_8014284C(SubwayRuntime *rt) {
    VECTOR32 pos;
    s32 dist;

    if (!(rt->flags0 & 0x20) && rt->healthInfo->health > 0) {
        if (rt->flags0 & 0x40) {
            pos = ((SubwayRuntime *)gObjPlayer)->move->pos;
            EGSGeometry_GetFuzzyXZDistancePoint2Point(&pos, &D_80145858, &dist);
            if (dist < 0xA0) {
                func_80142C7C(rt, &pos);
            }
        }
        if (!(rt->flags0 & 0x20)) {
            SysExeTickQueueTimer(func_8014284C, 5, (s32)rt);
        }
    }
}

#ifdef NON_MATCHING
/* The original keeps the return value 1 in $s0 across the timer call (3 more insns). */
static s32 func_80142934(s16 id) {
    GameObjectInfo *info;
    s32 ret;
    s32 n;

    info = &gameObjectsInfo[id];
    ret = 1;
    if (info->refId == -1) {
        n = info->pathStart;
        if (n >= 100) {
            n -= 100;
        }
        if (0x78 - xaLevelStartIndex[gSysInfo.level] == n) {
            SysExeTickQueueTimer(func_8014284C, 5, (s32)D_80145868);
            ret = 1;
        } else if (0x8B - xaLevelStartIndex[gSysInfo.level] == n && gObjectives != NULL) {
            gObjectives->unk1C |= 0x10;
        }
    }
    return ret;
}
#else
INCLUDE_ASM("asm/subway/nonmatchings/subway", func_80142934);
#endif

static void func_80142A24(s16 id) {
    s32 ref;
    int type;
    int objective;

    ref = gameObjectsInfo[id].refId;
    if (ref != -1) {
        type = GOBJ_TYPE(ref);
        if (type == GOBJ_TYPE_BOMB) {
            objective = func_80141F04(ref);
            if (gObjectives != NULL && (gObjectives->unk18 & (1 << objective))) {
                return;
            }
        } else if (D_8014586C.switchUsed) {
            if (type == 0x54) {
                return;
            }
            goto quip;
        } else {
            D_8014586C.switchUsed = 1;
            objective = 2;
        }
        LevelObjectiveAdd(objective, id);
        return;
    quip:
        ObjectDoQuip(id, type);
    }
}

/* Tunnel fog ambient object: in or out of the tunnel. */
static void func_80142B6C(s16 id, s32 type) {
    if (gameObjectsInfo[id].pathStart == 1) {
        D_8014586C.inTunnel = 0;
        EGSDisplaySetTerrainFog(gLevData->fogDist, gLevData->fogArg);
        GameCameraSetScrimOnFlag(1);
        GameCameraSetScrimScrollRate(0);
        func_80141648();
        func_80141BC8();
    } else {
        D_8014586C.inTunnel = 1;
        EGSDisplaySetTerrainFog(0x384, gLevData->fogArg);
        GameCameraSetScrimOnFlag(0);
        GameCameraSetScrimScrollRate(1);
        func_801416B8();
        func_80141B3C();
    }
}

static void *func_80142C58(s16 id, s16 type) {
    void *msg;

    msg = NULL;
    if (type == 0x54) {
        msg = gLockedMsg;
    }
    return msg;
}

/* Mark the comm array once the player faces it. */
static void func_80142C7C(SubwayRuntime *rt, VECTOR32 *pos) {
    VECTOR32 dir;
    s32 dot;

    if (!(rt->flags0 & 0x20)) {
        dir.vx = ((SubwayRuntime *)gObjPlayer)->anim->frame->rot[0];
        dir.vy = ((SubwayRuntime *)gObjPlayer)->anim->frame->rot[3];
        dir.vz = ((SubwayRuntime *)gObjPlayer)->anim->frame->rot[6];
        dir.vy = -dir.vy;
        dot = (D_80145858.vx - pos->vx) * dir.vx + (D_80145858.vz - pos->vz) * dir.vz;
        if (dot >= 0) {
            rt->flags0 |= 0x20;
            TextAddMarker(rt->id, gCommArrayMsg, 0x3C, 0);
        }
    }
}

#ifndef _PSYQ
/* labels inside the storage structs above (see the top of the file) */
__asm__(".set D_80145608, D_80145604+0x4\n"
        ".set D_80145708, D_801456F0+0x18\n"
        ".set D_80145740, D_80145740_s\n"
        ".set D_80145784, D_80145740_s+0x44\n"
        ".set D_80145830, D_80145830_s\n"
        ".set D_80145834, D_80145830_s+0x4\n"
        ".set D_80145838, D_80145830_s+0x8\n"
        ".set D_8014583C, D_80145830_s+0xC\n"
        ".set D_80145850, D_80145830_s+0x20\n"
        ".set D_80145854, D_80145830_s+0x24\n"
        ".set D_80145858, D_80145830_s+0x28\n"
        ".set D_80145860, D_80145830_s+0x30\n");
#endif

static void *func_80142D74(s16 id, s16 kind) {
    VECTOR32 pos;
    s32 ref;

    if (kind == 4) {
        ref = gameObjectsInfo[id].refId;
        if (GOBJ_TYPE(ref) == 0xB) {
            return gElevatorCallSwitchMsg;
        }
        return gPowerSwitchMsg;
    }
    if (kind == 1) {
        pos = ((SubwayRuntime *)gObjPlayer)->move->pos;
        func_80142C7C(D_80145868, &pos);
        return (void *)D_80140FE4;
    }
    return NULL;
}

/* SUBWAY level init. */
#ifdef NON_MATCHING
/* Same length; the constant 1 is hoisted out of the object loop ($fp used), and the objective
 * loop counts in $s0 instead of a fresh register. */
void func_80142E90(void) {
    SubwayRuntime *rt;
    GameObjectInfo *info;
    int i;
    int type;
    s32 count;
    int fogOut;
    int fogIn;
    int fog;
    int bumped;
    s32 ref;
    u32 flags;

    bumped = 0;
    D_801456E0 = EGSDisplayInitTextureAnimation(D_801455DC, D_801455E0, 4, 2, 0);
    D_801456E0->unk09 = 0x20;
    D_801456E8 = 1;
    D_801456E4 = EGSDisplayInitTextureAnimation(D_801455F0, D_801455F4, 4, 2, 0);
    D_801456E4->unk09 = 0x20;
    fogIn = -1;
    D_801456EC = 1;
    D_80145868 = NULL;
    D_8014586C.npcDead = 0;
    D_8014586C.crateDead = 0;
    D_8014586C.switchUsed = 0;
    D_8014586C.agentMarked = 0;
    npcSetLevelAlert();
    fogOut = -1;
    InterfaceViralRadarInit();
    func_80141720(0x1680, 0x1680, 0x78, 0xD, 6);
    SaveObjectivesNTimersReinit(&D_80145604, 0);
    i = 0;
    if (gObjectives->unk18 == 0) {
        LevelObjectiveAdd(0, -1);
        LevelObjectiveAdd(4, -1);
        if (gObjectives != NULL) {
            count = gObjectives->count;
            for (; i < count; i++) {
                if (gObjectives != NULL) {
                    gObjectives->unk24 |= 1 << i;
                }
            }
            if (gObjectives != NULL) {
                gObjectives->unk1C &= ~0x10;
            }
        }
        SysExeTickQueueTimer(GameCamCheckScriptCamDoneCallback, 0x14, (s32)LevelAddingObjectiveInstruction);
    }
    D_80145608->msg = gEliminateRhoemerMsg;
    SysStopSong(0, 0x18);
    DamageSetDamagedCallback(func_80141FC4);
    DamageSetDestroyedCallback((void (*)(s16, s16))func_801422E4);
    LevelSetEOLCallback(func_80142648);
    /* (the flags copy keeps cc1 from merging the two bit tests) */
    LevelSetObjectiveInstructionsCallback(
        gObjectives == NULL || (!((flags = gObjectives->unk18) & 2) && !(gObjectives->unk18 & 8)) ? func_801426FC
                                                                                                    : NULL);
    NPCSetCopDeactivateCallback(NPCCopDeactivateWashingtonCallback);
    UseSetBombCallback(func_80142724);
    UseSetSwitchCallback(func_8014277C);
    ObjectSetQuipObjectCallback(func_80142934);
    ObjectSetSpecialObjectCallback(func_80142A24);
    ObjectSetSpecialAmbientCallback(func_80142B6C);
    SetLockedStringCallback(func_80142C58);
    SetUseStringCallback(func_80142D74);

    for (i = 0; i < gLevelGameObjCount; i++) {
        rt = RT(i);
        type = GOBJ_TYPE(i);
        if (type == 1) {
            if (rt->id == gLevelSpecificNpcID && rt->healthInfo->health <= 0) {
                D_8014586C.npcDead = 1;
            }
        } else if (type == 0x38) {
            func_80149480(i);
        } else if (type == 0x2C && !bumped) {
            bumped = 1;
            rt->anim->model->unk14 += 0x20;
        } else if (rt->npcState == 7 && gameObjectsInfo[i].refId == -1) {
            AnimateMemCardScreen(rt);
        } else if (type == 0x1D || type == 0x7C) {
            if (rt->flags0 & 0x20) {
                D_8014586C.switchUsed = 1;
            }
        } else if (type == 0x5D) {
            if (0x78 - xaLevelStartIndex[gSysInfo.level] == i && ((gameObjectsInfo + (s16)i)->flags24 & 0x20) &&
                (gObjectives == NULL || !(gObjectives->completed & 1))) {
                SysExeTickQueueTimer(func_8014284C, 5, (s32)D_80145868);
            }
        } else if (type == 0x5F) {
            ref = gameObjectsInfo[i].refId;
            if (ref != -1 && GOBJ_TYPE(ref) != GOBJ_TYPE_BOMB && ((gameObjectsInfo + (s16)i)->flags24 & 0x20)) {
                D_8014586C.switchUsed = 1;
            }
        } else if (type == SUBWAY_TYPE_VIRUS) {
            rt->anim->unk08 &= ~8;
            rt->flags0 |= 0x20;
        } else if (type == SUBWAY_TYPE_CRATE) {
            D_80145868 = rt;
            rt->flags0 |= 8;
            D_80145858.vx = D_80145868->anim->frame->pos.vx;
            D_80145858.vy = D_80145868->anim->frame->pos.vy;
            D_80145858.vz = D_80145868->anim->frame->pos.vz;
            D_80145858.vy = -D_80145858.vy;
            if (D_80145868->healthInfo->health <= 0) {
                D_8014586C.crateDead = 1;
            }
        } else if (type == GOBJ_TYPE_BOMB) {
            if (func_80141F04(i) == 1 && ObjectMatchToRefID(i, GOBJ_TYPE_COP)->healthInfo->health <= 0) {
                gLevelInnocentsKilled++;
            }
        } else if (type == SUBWAY_TYPE_FOG) {
            if (gameObjectsInfo[i].pathStart == 1) {
                fogIn = i;
            } else {
                fogOut = i;
            }
        }
    }
    rt = RT(D_8014583C[0][0]);
    if (rt != NULL && (rt->flags0 & 8)) {
        func_80141B3C();
    }
    if (fogIn != -1 && fogOut != -1) {
        fog = fogOut;
        if (!(gameObjectsInfo[(s16)fogOut].flags24 & 0x20)) {
            fog = fogIn;
        }
        ObjectUpdateAmbientObjectSetting(fog, SUBWAY_TYPE_FOG);
        func_80142B6C(fog, SUBWAY_TYPE_FOG);
    }
    if (gLevelSpecificNpcID != -1) {
        info = &gameObjectsInfo[gLevelSpecificNpcID];
        if (((SubwayRuntime *)info->runtime)->npcState == 2) {
            gLevelSpecificNpcName = D_801455D8;
            ((SubwayRuntime *)info->runtime)->npc->nameMsgId = 0x5A;
        }
    }
    gDefaultLevelDmgFX = 11;
}
#else
INCLUDE_ASM("asm/subway/nonmatchings/subway", func_80142E90);
#endif

/* Line the cars of a train up behind its head car again. */
static void func_80143740(s32 train) {
    VECTOR32 head;
    VECTOR32 pos;
    SubwayRuntime *car;
    int i;

    for (i = 0; i < D_80145854[train]; i++) {
        car = RT(D_8014583C[train][i]);
        car->move->speed = car->unk04;
        if (i != 0) {
            pos.vx = head.vx - D_80145784[train][0].vx + D_80145784[train][i].vx;
            pos.vy = head.vy;
            pos.vz = head.vz - D_80145784[train][0].vz + D_80145784[train][i].vz;
            EGSFrameSetPosition(car->anim->frame, 0, &pos);
        } else {
            head.vx = car->anim->frame->pos.vx;
            head.vy = car->anim->frame->pos.vy;
            head.vz = car->anim->frame->pos.vz;
            head.vy = -head.vy;
        }
    }
}

/* Train speed: slow (0x1388) or fast (0x1D4C). */
static void func_801438EC(s32 train, u8 fast) {
    SubwayMove *move;
    int i;

    for (i = 0; i < D_80145854[train]; i++) {
        move = RT(D_8014583C[train][i])->move;
        if (!fast) {
            move->speed = 0x1388;
        } else {
            move->speed = 0x1D4C;
        }
    }
}

/* Start a train from its start position (SUBWAY3). */
static void func_8014399C(s32 train, u8 notify) {
    VECTOR32 pos;
    SubwayRuntime *rt;
    int i;
    u16 id;

    D_80145874.current = train;
    for (i = 0; i < D_80145854[train]; i++) {
        id = D_8014583C[train][i];
        rt = RT(id);
        rt->flags0 |= 8;
        EGSFrameSetPosition(rt->anim->frame, 0, &D_80145784[train][i]);
        rt->flags23 &= ~2;
        if (notify) {
            SysPostMessage(10, 4, id, id, 0, 0, 0, 0);
        }
    }
    pos = ((SubwayRuntime *)gObjPlayer)->move->pos;
    if (D_80145834 < GameSysFuzzyMathXZDistance(&pos, &D_80145784[train][0])) {
        func_801438EC(train, 0);
    }
}

/* Stop a train and park it at its start position. */
static void func_80143B64(s32 train) {
    SubwayRuntime *rt;
    int i;

    if (D_80145874.near) {
        D_80145874.near = 0;
        func_801438EC(train, D_80145874.reverse);
    }
    if (D_80145874.shaking) {
        D_80145874.shaking = 0;
        GameCameraShakeStop(D_80145740);
    }
    if (D_80145875 & 1) {
        D_80145875 &= ~1;
        SysStopSong(0, 0x18);
    }
    for (i = 0; i < D_80145854[train]; i++) {
        rt = RT(D_8014583C[train][i]);
        rt->flags0 &= ~8;
        EGSFrameSetPosition(rt->anim->frame, 0, &D_80145784[train][i]);
        rt->flags23 |= 2;
    }
}

/* No car of the train is active. */
static u8 func_80143CDC(s32 train) {
    int i;
    u8 count;

    count = D_80145854[train];
    for (i = 0; i < count; i++) {
        if (RT(D_8014583C[train][i])->flags0 & 0x40) {
            return 0;
        }
    }
    return 1;
}

/* Aramov is not at the last car of the train, or the last car is close to a point. */
static u8 func_80143D7C(s32 train, VECTOR32 *pt) {
    VECTOR32 pos;
    s32 dist;
    SubwayRuntime *last;
    u8 ret;

    last = RT(D_8014583C[train][D_80145854[train] - 1]);
    ret = last != func_80141C44(train, D_80145828);
    if (!ret) {
        pos.vx = last->anim->frame->pos.vx;
        pos.vy = last->anim->frame->pos.vy;
        pos.vz = last->anim->frame->pos.vz;
        pos.vy = -pos.vy;
        EGSGeometry_GetFuzzyXZDistancePoint2Point(&pos, pt, &dist);
        ret = dist < 0x140;
    }
    return ret;
}

static void func_80143E94(void) {
    D_80145870.waits = 0;
    D_80145870.stopped = 0;
}

/* Aramov waits a little, then stops chasing for a while. */
static void func_80143EB0(void) {
    if (!D_80145870.stopped) {
        if (D_80145870.waits < 2) {
            D_80145870.waits++;
            return;
        }
        D_80145870.timing = 1;
        D_80145830 = gExeTickU + 50.0;
        NPCSetChaseStopFlag(D_80145828->id, 1);
    }
}

/* Side of platform line point n that p is on (sign of the dot product with the line normal). */
#define LINE_SIDE(p, n) \
    (((p).vx - D_80145674[n].vx) * D_80145730.vx + ((p).vz - D_80145674[n].vz) * D_80145730.vz)

/* SUBWAY3: Aramov's taunts and his chase along the platforms, switching trains. */
static void func_80143F84(void) {
    VECTOR32 pos;
    VECTOR32 player;
    u8 out;
    int n;
    int i;
    u8 side;
    u8 timing;
    int train;
    int other;
    u8 far;

    if (D_80145828->healthInfo->health > 0 && D_8014582C != 0 && !SysSoundIsXAPlaying() && !(rand() & 0x1FF)) {
        n = rand() % D_8014582C;
        SysPlayXA(D_801456D4[n], (s32)D_80145828, 0);
        i = n;
        D_8014582C--;
        for (; i < D_8014582C; i++) {
            D_801456D4[i] = D_801456D4[i + 1];
        }
    }
    if (D_80145870.inBox) {
        return;
    }
    if (D_80145828->healthInfo->health > 0) {
        if (D_80145828->move != NULL) {
            pos = D_80145828->move->pos;
            EGSGeometry_IsIntersectXZPoint2UnrotBox(&pos, D_80145654, 0, &out);
        } else {
            out = 0;
        }
    } else {
        out = 1;
    }
    D_80145870.inBox = out;
    D_80145874.reverse = out;
    if (D_80145870.inBox) {
        return;
    }
    player = ((SubwayRuntime *)gObjPlayer)->move->pos;
    D_80145828->npc->unk32 = 0x7FFF;
    side = D_80145870.side;
    timing = D_80145870.timing;
    D_80145870.side = LINE_SIDE(pos, 0) < 0;
    if (D_80145870.side) {
        if (LINE_SIDE(pos, 1) > 0) {
            D_80145874.reverse = 1;
            func_80143E94();
        } else if (LINE_SIDE(pos, 2) <= 0) {
            D_80145874.reverse = 1;
            func_80143EB0();
        } else {
            D_80145874.reverse = 0;
            func_80143E94();
        }
    } else {
        if (LINE_SIDE(pos, 3) < 0) {
            D_80145874.reverse = 1;
            func_80143E94();
        } else if (LINE_SIDE(pos, 4) >= 0) {
            D_80145874.reverse = 1;
            func_80143EB0();
        } else {
            D_80145874.reverse = 0;
            func_80143E94();
        }
    }
    if (D_80145870.timing && gExeTickU > D_80145830) {
        D_80145870.timing = 0;
        D_80145870.stopped = 1;
        D_80145874.reverse = 0;
        NPCSetChaseStopFlag(D_80145828->id, 0);
    }
    if (side != D_80145870.side || timing != D_80145870.timing ||
        (D_80145870.nearTrain && !D_80145870.timing) || (D_80145870.waits == 2 && !D_80145870.timing)) {
        if (D_80145870.side) {
            train = 1;
            other = 0;
        } else {
            train = 0;
            other = 1;
        }
        if (func_80143CDC(train)) {
            func_80143B64(train);
            if (!(RT(D_8014583C[other][0])->flags0 & 8)) {
                func_8014399C(other, 1);
            }
            if (D_80145870.nearTrain) {
                D_80145870.nearTrain = 0;
                NPCSetChaseStopFlag(D_80145828->id, 0);
            }
        } else {
            far = func_80143D7C(train, &pos);
            D_80145870.side = side;
            D_80145870.timing = timing;
            D_80145870.nearTrain = far;
            NPCSetChaseStopFlag(D_80145828->id, far);
        }
    } else if (!D_80145870.nearTrain &&
               (D_80145874.reverse || (D_80145828->npc->flags & 0x1000000))) {
        train = D_80145870.side == 0;
        if (func_80143CDC(train) || !func_80143D7C(train, &pos)) {
            if (D_80145870.waits == 0) {
                far = 0;
                if (!gSniperMode) {
                    far = D_80145828->npc->distance >= 0x781;
                }
            } else {
                far = 0;
                if (!gSniperMode) {
                    far = D_80145828->npc->distance >= 0x781;
                }
                if (!far) {
                    D_80145830 = gExeTick;
                }
            }
            if (!far) {
                D_80145874.reverse = 0;
            }
            NPCSetChaseStopFlag(D_80145828->id, far);
        }
    }
}

static void func_801446DC(s16 id, s16 by) {
    if (!gMissionFailed && gLevelSpecificNpcID == id) {
        if (gObjectives != NULL) {
            if (!(gObjectives->unk18 & 1)) {
                LevelObjectiveAdd(0, -1);
            }
            gObjectives->completed |= 1;
        }
        LevelDoEOL(0);
    }
}

static void func_8014476C(s16 by, s16 weapon) {
    if (!gMissionFailed && (weapon == 0x13 || weapon == 0x10)) {
        LevelMissionFailed(0, -2, 0);
    }
}

static void func_801447BC(void) {
    SysPlayXA(0x120, (s32)D_80145828, 0);
}

static void func_801447E8(void) {
    SysExeTickQueueTimer(func_801447BC, 0x14, 0);
}

/* SUBWAY3 level init. */
void iSubway3Init(s32 mode) {
    SubwayRuntime *rt;
    int i;
    s16 type;

    D_80145828 = NULL;
    D_80145830 = gExeTick;
    D_80145870.side = -2;
    D_80145870.waits = 0;
    D_80145870.timing = 1;
    D_80145870.nearTrain = 0;
    D_80145870.inBox = 0;
    func_80141720(0x3200, 0x1900, 0x5A, 0x11, 8);
    D_80145874.stopped = 1;
    D_80145874.reverse = 0;
    SysStopSong(0, 0x18);
    D_80145730.vx = -0xD651;
    D_80145730.vy = 0;
    D_80145730.vz = 0x7C8C;
    GTEMath_VectorNormal(&D_80145730, &D_80145730);
    SaveObjectivesNTimersReinit(&D_8014562C, 0);
    LevelObjectiveAdd(0, -1);
    if (gObjectives != NULL) {
        gObjectives->unk24 |= 1;
    }
    if (mode == 3) {
        func_801447E8();
    } else {
        gSysMovieDoneCallback = func_801447E8;
    }
    SysSoundSetChaseExciteLevel(1);
    DamageSetDestroyedCallback(func_801446DC);
    WeaponSetWeaponFiredCallback(func_8014476C);
    for (i = 0; i < gLevelGameObjCount; i++) {
        rt = RT(i);
        type = GOBJ_TYPE(i);
        if (rt != NULL && rt->npcState == 7 && gameObjectsInfo[rt->id].refId == -1) {
            AnimateMemCardScreen(rt);
        } else if (type == 2) {
            D_80145828 = rt;
        }
    }
    func_8014399C(0, 1);
    if (D_80145828 != NULL) {
        gLevelSpecificNpcID = D_80145828->id;
        gLevelSpecificNpcName = gAramovMsg;
        D_80145828->npc->nameMsgId = 9;
        NPCSetAwarenessLevel(D_80145828, 2, 1, 0);
        D_80145828->flags23 |= 1;
        D_8014582C = 3;
    }
    gDefaultLevelDmgFX = 11;
}

#define GET_POS(v, frame)          \
    ((v).vx = (frame)->pos.vx, \
     (v).vy = (frame)->pos.vy, \
     (v).vz = (frame)->pos.vz, \
     (v).vy = -(v).vy)

#ifdef NON_MATCHING
/* &D_801456F0 is CSE'd into $s0 for the two flashlight calls (the original rematerialises it each time; ours is 1 insn shorter) and the train-sound flag address/value swap v0/v1 */
void TRAINObjDispatch(SysMsg *msg) {
    VECTOR32 player;
    VECTOR32 car;
    VECTOR32 end;
    u8 done;
    SubwayRuntime *rt;
    SubwayRuntime *last;
    SubwayFrame *frame;
    GameObjectHealth *health;
    s32 dy;
    s32 dist;
    s16 hp;
    s16 type;

    rt = RT(msg->to);
    type = GOBJ_TYPE(rt->id);
    switch (msg->id) {
    case 10:
        if (rt->move == NULL || !(rt->flags0 & 8)) {
            break;
        }
        if ((type == SUBWAY_TYPE_TRAIN3 && rt->flags1 == D_80145874.current) || rt->flags1 == func_80141DB8()) {
            if (gameObjectsInfo[rt->id].refId == -1) {
                /* head car: the train reaches the player */
                GET_POS(player, ((SubwayRuntime *)gObjPlayer)->anim->frame);
                if (!D_80145874.near) {
                    frame = rt->anim->frame;
                    last = RT(D_8014583C[rt->flags1][D_80145854[rt->flags1] - 1]);
                    GET_POS(car, frame);
                    GET_POS(end, last->anim->frame);
                    if (type == SUBWAY_TYPE_TRAIN3) {
                        dy = 0;
                    } else {
                        dy = player.vy - car.vy;
                        if (dy < 0) {
                            dy = -dy;
                        }
                    }
                    if (dy < 0x460) {
                        dist = GameSysFuzzyMathXZDistance(&player, &car);
                        if (dist < D_80145834) {
                            if (type == SUBWAY_TYPE_TRAIN3) {
                                func_80143740(rt->flags1);
                            }
                            if (dist < GameSysFuzzyMathXZDistance(&player, &end)) {
                                D_80145874.near = 1;
                                if (!D_80145874.shaking && (D_8014586C.inTunnel || type == SUBWAY_TYPE_TRAIN3)) {
                                    D_80145874.shaking = 1;
                                    GameCameraShakeStart(D_80145740, -1, 1, D_801456C4, 0, 0);
                                }
                                TRAIN_SOUND->playing = 1;
                                SysPlaySong(0, 0x18, 0, 0);
                                D_80145824 = frame;
                                EGSDisplayFlashlightFrame(&D_801456F0, frame, 0);
                                EGSDisplayFlashlightOn(&D_801456F0);
                            }
                        } else if (D_80145874.reverse && rt->move->speed > 0x1388 && dist < 0x4600) {
                            func_801438EC(rt->flags1, D_80145874.reverse);
                        }
                    }
                }
            } else if (D_80145874.near) {
                /* last car: the train has passed the player */
                if (rt == RT(D_8014583C[rt->flags1][D_80145854[rt->flags1] - 1])) {
                    last = RT(D_8014583C[rt->flags1][0]);
                    frame = last->anim->frame;
                    GET_POS(end, frame);
                    GET_POS(car, rt->anim->frame);
                    GET_POS(player, ((SubwayRuntime *)gObjPlayer)->anim->frame);
                    if (type == SUBWAY_TYPE_TRAIN3) {
                        dy = 0;
                    } else {
                        dy = player.vy - car.vy;
                        if (dy < 0) {
                            dy = -dy;
                        }
                    }
                    if (dy < 0x460) {
                        dist = GameSysFuzzyMathXZDistance(&player, &car);
                    } else {
                        dist = D_80145834 + 1;
                    }
                    if (dy >= 0x460 || (dist >= D_80145838 && dist < GameSysFuzzyMathXZDistance(&player, &end))) {
                        D_80145874.near = 0;
                        if (D_80145874.shaking) {
                            D_80145874.shaking = 0;
                            GameCameraShakeStop(D_80145740);
                        }
                        D_80145875 &= ~1;
                        SysStopSong(0, 0x18);
                        EGSDisplayFlashlightOff(&D_801456F0);
                        if (type == SUBWAY_TYPE_TRAIN3) {
                            func_80143B64(rt->flags1);
                            if (D_80145874.reverse) {
                                func_8014399C(rt->flags1 ^ 1, 1);
                            } else {
                                func_8014399C(rt->flags1, 0);
                            }
                        }
                    }
                }
            }
        }
        PathHandleEx(rt, gObjPathScaleVector, 2, 0, -1, -1, &done);
        if (gameObjectsInfo[rt->id].refId == -1) {
            if (type == SUBWAY_TYPE_TRAIN) {
                if (done) {
                    func_80141A28(rt->flags1);
                }
            } else if (rt->flags1 == D_80145874.current) {
                func_80143F84();
                if (done) {
                    func_80143B64(rt->flags1);
                    if (D_80145874.reverse) {
                        func_8014399C(rt->flags1 ^ 1, 1);
                    } else {
                        func_8014399C(rt->flags1, 0);
                    }
                }
            }
        }
        if (rt->flags0 & 8) {
            SysPostMessage(10, 4, rt->id, rt->id, 0, 0, 0, 0);
        }
        break;
    case 6:
        CobjAddBulletCollisionBoxGobj(rt, 1, 0, 0);
        CobjAddCollisionBoxGobj(rt, 1, 0, 1, gameObjectsInfo[rt->id].refId != -1);
        rt->flags0 |= 0x40;
        break;
    case 5:
        CobjRemoveBulletCollisionBoxGobj(rt);
        CobjRemoveCollisionBoxGobj(rt);
        rt->flags0 &= ~0x40;
        break;
    case 26:
        health = ((SubwayRuntime *)gObjPlayer)->healthInfo;
        hp = health->health;
        if (hp > 0) {
            DamageGOBJ(-1, rt->id, gPlayerOffset, health->armor + hp, 0x5F);
        }
        break;
    case 18:
        if (!(rt->flags0 & 8) && type == SUBWAY_TYPE_TRAIN) {
            func_80141B3C();
        }
        break;
    case 41:
        *(u16 *)msg->data[1] = rt->flags0;
        break;
    case 42:
        rt->flags0 = *(u16 *)msg->data[1];
        break;
    case 2:
        rt->npcState = 7;
        func_801463A4(rt->id, 0);
        PathSetupEX(rt, 1);
        rt->anim->flagsA |= 0x20;
        rt->anim->model->flags |= 0x8000;
        rt->flags0 &= ~0x48;
        rt->anim->unk09 = 0x80;
        if (type == SUBWAY_TYPE_TRAIN) {
            rt->unk21 = 0x96;
        } else {
            rt->unk21 = 0xA0;
            rt->unk04 = rt->move->speed;
        }
        break;
    }
}
#else
INCLUDE_ASM("asm/subway/nonmatchings/subway", TRAINObjDispatch);
#endif
