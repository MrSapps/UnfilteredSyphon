#include "common.h"
#include "system.h"
#include "gameobj.h"
#include "gobj.h"
#include "level.h"

/* cas.c's data, defined in original order (docs/data_migration.md). */
static const s32 D_80010F50[4] = { 1, 1, 1, 0 }; /* starts every game TU's .rdata */

/* .sdata */
static EGSListNode *D_8010F7B8 = NULL; /* gobjs with pending CAS requests */
static char D_8010F7BC[8] = "s/cas.c"; /* __FILE__; first used by CASInit */
static u8 D_8010F7C4 = 1;              /* the generated hip table still needs building */
static s32 D_8010F7C8 = 0;             /* unreferenced; taken as cas.c's (object.c's .sdata follows) */

/* .sbss */
static s16 D_8010FF88; /* gobj whose requests are being delayed */
static s32 D_8010FF8C; /* delay length in ticks (0: none) */
static s32 D_8010FF90; /* delay start tick */

/* .data */
static CASQueue D_8010521C = { 0 }; /* this frame's de-duplicated action list */
u8 gNullSequence[0x18] = { 0xFF, 0xFF, 0, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0, 0, 0, 0xFF };

extern s32 gSystemTick;

u8 EGSListFind(EGSListNode **list, s32 arg1, void *data);
void *EGSDisplayGetHAWithType(void *anim, s32 type);
void *EGSDisplayGetHA(void *anim, s32 action);
void EGSDisplayHAStartUnique(void *anim, s32 anim1, s32 arg2);
void EGSDisplayHASetCurrentFrame(void *anim, s32 anim1, s32 frame);
void EGSDisplayAnimateHMD(void *anim);
void EGSFrameUpdateHierarchy(void *frame);
void EGSDisplayHAStop(void *anim, s16 anim1);
void EGSDisplayHAStart3(void *anim, s16 anim1, s32 frame, s32 layer, s32 blend);
struct CASHipMotion *EGSDisplayHAGetHipMotionArray(void *ha);
int rcos(int a);
static void func_8002AC1C();

typedef struct CASSequence {
    /* 0x00 */ s16 action; /* <= 0 terminates */
    /* 0x02 */ s16 sub;    /* 0: any */
    /* 0x04 */ u8 needFlags2;
    /* 0x05 */ u8 blockFlags2;
    /* 0x06 */ u8 actions[3]; /* applied when the sequence starts, 0xFF: none */
    /* 0x09 */ s8 unk09[3]; /* -1: ... (by HAnim.unk28 0x8000/0x4000/other) */
    /* 0x0C */ u8 prio; /* lower wins */
    /* 0x0D */ u8 unk0D; /* new CASState.active when nonzero */
    /* 0x0E */ u8 unk0E; /* new CASState.mode */
    /* 0x0F */ u8 nextAction; /* requested when the sequence ends */
    /* 0x10 */ void (*onStart)(s16 id, s32 arg);
    /* 0x14 */ u8 startMsg;
    /* 0x15 */ u8 unk15[3];
} CASSequence; /* size 0x18 */

typedef struct CASModelData {
    s32 w[8];
} CASModelData;

typedef struct CASAnimSet {
    /* 0x00 */ u8 unk00[4];
    /* 0x04 */ u8 unk04;
    /* 0x05 */ u8 unk05[0xB];
    /* 0x10 */ u32 setFlags;   /* applied when no sequence action is */
    /* 0x14 */ u32 clearFlags;
    /* 0x18 */ CASSequence *sequences;
    /* 0x1C */ void (*onDone)(s16 id, s32 set); /* called if the action asks for it */
    /* 0x20 */ s32 doneMsg;                     /* message posted if the action asks for it */
    /* 0x24 */ CASModelData *modelData; /* copied over the gobj's model header */
    /* 0x28 */ u8 unk28[2];
    /* 0x2A */ s16 numSequences;
} CASAnimSet; /* size 0x2C */

#define CAS_NUM_ANIM_SETS 99


typedef struct CASRule {
    /* 0x00 */ u32 stateMask; /* 0: any; else bit per request state */
    /* 0x04 */ u8 unk04[4];
    /* 0x08 */ u32 require;   /* CAS flags that must be set */
    /* 0x0C */ s32 forbid;    /* CAS flags that must be clear; -1: flags must equal require */
} CASRule;

typedef struct HAnim {
    /* 0x00 */ u8 unk00[4];
    /* 0x04 */ s16 anim;
    /* 0x06 */ u8 unk06[6];
    /* 0x0C */ u8 *data;
    /* 0x10 */ u8 unk10[4];
    /* 0x14 */ s32 frame;
    /* 0x18 */ u8 unk18[4];
    /* 0x1C */ void (*handler)(); /* animation event handler */
    /* 0x20 */ u32 eventMask;    /* pending animation events */
    /* 0x24 */ void *handlerArg;
    /* 0x28 */ s32 unk28;
    /* 0x2C */ s32 unk2C;
} HAnim;

/* Animation event callback entry; a list ends with valid == 0. */
typedef struct CASEvent {
    /* 0x00 */ u8 valid;
    /* 0x01 */ u8 pad01[3];
    /* 0x04 */ s32 code;
    /* 0x08 */ void (*fn)(s16 id, s32 set, s32 index, GameObjectRuntime *rt);
} CASEvent; /* size 0xC */

typedef struct CASDone {
    /* 0x00 */ u8 call; /* run the anim set's onDone */
    /* 0x01 */ u8 post; /* post the anim set's doneMsg */
    /* 0x02 */ u8 pad02[2];
    /* 0x04 */ u32 event; /* animation event (high half) that finishes the action */
} CASDone;

typedef struct CASAction {
    /* 0x00 */ u8 hasEvents;
    /* 0x01 */ u8 unk01[3];
    /* 0x04 */ u32 setFlags;
    /* 0x08 */ u32 clearFlags;
    /* 0x0C */ u32 setFlags2;
    /* 0x10 */ u32 clearFlags2;
    /* 0x14 */ CASDone done;
    /* 0x1C */ CASEvent *events;
} CASAction; /* size 0x20 */


static void func_8002AD98();
static void func_80029B78();
static u8 func_80029794();
static u8 func_80029838();
static void func_8002A4A0();
static void func_80029298();
extern void **gCamera;

/* View of a camera handle */
typedef struct CASCamera {
    /* 0x00 */ u8 unk00[0x14];
    /* 0x14 */ s32 index; /* slot in each gobj's per-camera display table */
} CASCamera;



void SYSCasInit(void) {
    D_8010521C.items = NULL;
    D_8010F7B8 = NULL;
    D_8010FF88 = -1;
    D_8010FF8C = 0;
    D_8010FF90 = 0;
}

#ifdef NON_MATCHING
/* register allocation/spills differ (one more stack slot; s-reg assignment) */
/* Animation event handler: run the action's event callbacks, finish its slot, queue the
 * sequence-start callbacks. */
static void func_80029298(s32 unused, u32 event, CASAnimEvent *info) {
    GameObjectRuntime *rt;
    HAnim *ha;
    struct CASSlots *t;
    CASAction *action;
    CASDone *done;
    CASEvent *events;
    CASEvent *ev;
    CASSequence *seq;
    CASAnimEvent *e;
    s32 slot;
    s32 i;
    s32 hit;
    u32 kind;
    s32 code;
    u8 found;

    rt = gameObjectsInfo[info->id].runtime;
    ha = EGSDisplayGetHA(rt->anim, info->action);
    ha->eventMask &= ~event;
    slot = -1;
    t = &rt->cas->queue->slots;
    if (rt->cas->actionTable == NULL) {
        return;
    }
    action = &((CASAction *)rt->cas->actionTable)[info->action];
    done = &action->done;
    events = action->events;
    if (action->hasEvents) {
        if (event == 0x80000000) {
            func_80029B78(rt, info->action, -1, event);
        }
        if (events != NULL) {
            i = 0;
            hit = -1;
            found = 0;
            if (events->valid) {
                kind = event & 0xFFFF0000;
                code = event & 0xFF;
                ev = events;
                do {
                    if (kind == 0x04000000 && code == ev->code) {
                        found = 1;
                        if (ev[1].valid) {
                            hit = i + 1;
                        } else {
                            seq = CASGetAnimSequence(info->set, info->mode, info->sub);
                            if ((ha->unk28 == 0x8000 && seq->unk09[0] == -1)
                                || (ha->unk28 == 0x4000 && seq->unk09[1] == -1) || seq->unk09[2] == -1) {
                                hit = 0;
                            }
                        }
                        if (hit != -1) {
                            ha->eventMask = (ha->eventMask & ~0xFF) | (events[hit].code | 0x04000000);
                        }
                    } else if (kind == 0x80000000 && ev->code == -1) {
                        found = 1;
                    }
                    if (found) {
                        ev->fn(rt->id, info->set->unk04, i, rt);
                        break;
                    }
                    ev++;
                    i++;
                } while (ev->valid);
            }
        }
    }
    for (i = 0; i < 3; i++) {
        if (t->action[i] != 0xFF && t->action[i] == info->action) {
            slot = i;
            break;
        }
    }
    if (slot != -1 && t->state[slot] == 1 && (event & 0xFFFF0000) == done->event) {
        t->state[slot] = 2;
        t->set[slot] = info->set->unk04;
        t->req[slot][0] = info->req0;
        t->req[slot][1] = info->mode;
        t->sub[slot] = info->sub;
        CASRequestAction(rt->id, 0x5A);
    }
    if (event == 0x80000000) {
        seq = CASGetAnimSequence(info->set, info->mode, info->sub);
        if (seq->nextAction != 0) {
            CASRequestAction(rt->id, seq->nextAction);
        }
        if (seq->onStart != NULL || seq->startMsg != 0) {
            e = &t->deferred[t->numDeferred];
            e->id = info->id;
            e->req0 = info->req0;
            e->mode = info->mode;
            e->sub = info->sub;
            e->set = info->set;
            t->numDeferred++;
            CASRequestAction(rt->id, 0x5A);
        }
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/cas", func_80029298);
#endif

CASSequence *CASGetAnimSequence(CASAnimSet *set, s32 action, s32 sub) {
    CASSequence *seqs;
    CASSequence *seq;
    int i;

    seqs = set->sequences;
    if (seqs == NULL) {
        return (CASSequence *)gNullSequence;
    }
    for (i = 0;; i++) {
        seq = &seqs[i];
        if (seq->action < 0 || seq->action == 0) {
            return seq;
        }
        if (seq->action == action && (seq->sub == 0 || seq->sub == sub)) {
            return seq;
        }
    }
}

static u8 func_80029794(GameObjectRuntime *rt, CASAnimSet *set, u8 *req, s32 sub) {
    struct CASSlots *slots;
    CASSequence *seq;
    u32 flags2;
    u8 ret;

    slots = &rt->cas->queue->slots;
    if (slots->current != -1 && slots->state[slots->current] == 2) {
        return 0;
    }
    seq = CASGetAnimSequence(set, req[1], sub);
    if (rt->cas->flags2 & seq->needFlags2) {
        if (rt->cas->flags2 & seq->blockFlags2) {
            return 0;
        }
    }
    return 1;
}

/* May a request in state *req run under this rule? */
static u8 func_80029838(GameObjectRuntime *rt, CASRule *rule, u8 *req) {
    s32 no;
    struct CASSlots *slots;
    u32 flags;
    u32 have;

    slots = &rt->cas->queue->slots;
    if (slots->current != -1 && slots->state[slots->current] == 2) {
        return 1;
    }
    if (rule->stateMask != 0 && !(rule->stateMask & (1 << *req))) {
        return 0;
    }
    if (rule->forbid == -1 && rt->cas->flags != 0) {
        return 0;
    }
    no = 0;
    flags = rt->cas->flags;
    have = flags & rule->require;
    if (have != rule->require) {
        return no;
    }
    if (rule->forbid == -1 && flags != have) {
        return no;
    }
    if (rt->cas->flags & rule->forbid) {
        return no;
    }
    return 1;
}

/* Enter anim set `set`: apply its sequence's actions (or the set's own flags), model data. */
static void func_80029920(GameObjectRuntime *rt, CASAnimSet *set, u8 *req, s32 sub) {
    struct CASSlots *slots;
    CASSequence *seq;
    CASAction *a;
    u8 applied;

    slots = &rt->cas->queue->slots;
    a = rt->cas->actionTable;
    applied = 0;
    if (a != NULL && slots->current != -1 && slots->state[slots->current] == 2) {
        seq = CASGetAnimSequence(set, req[1], sub);
        if (seq->actions[0] != 0xFF) {
            a = &((CASAction *)rt->cas->actionTable)[seq->actions[0]];
            applied = 1;
            rt->cas->flags |= a->setFlags;
            rt->cas->flags &= ~a->clearFlags;
        }
        if (seq->actions[1] != 0xFF) {
            a = &((CASAction *)rt->cas->actionTable)[seq->actions[1]];
            applied = 1;
            rt->cas->flags |= a->setFlags;
            rt->cas->flags &= ~a->clearFlags;
        }
        if (seq->actions[2] != 0xFF) {
            a = &((CASAction *)rt->cas->actionTable)[seq->actions[2]];
            applied = 1;
            rt->cas->flags |= a->setFlags;
            rt->cas->flags &= ~a->clearFlags;
        }
    }
    if (!applied) {
        rt->cas->flags |= set->setFlags;
        rt->cas->flags &= ~set->clearFlags;
    }
    if (set->modelData != NULL) {
        *(CASModelData *)rt->anim->model = *set->modelData;
    }
    if (req[0] == 1 && (rt->cas->flags & 0x100000)) {
        rt->cas->flags &= ~0x100000;
    }
}

/* Leave action `from` and enter action `to`, updating the CAS flag words. */
/* old-style definition: func_8002A4A0 calls it with three arguments */
static void func_80029B78(rt, from, to, event)
    GameObjectRuntime *rt;
    s32 from;
    s32 to;
    s32 event;
{
    CASState *cas;
    CASAction *table;

    cas = rt->cas;
    table = cas->actionTable;
    if (from != -1) {
        cas->flags2 &= ~table[from].clearFlags2;
        rt->cas->flags &= ~table[from].clearFlags;
    }
    if (to != -1) {
        rt->cas->flags2 |= table[to].setFlags2;
        rt->cas->flags |= table[to].setFlags;
    }
}

/* Pick the best-priority non-empty sequence of `set` enabled by the gobj's CAS flags. */
static s32 func_80029C04(GameObjectRuntime *rt, CASAnimSet *set, u8 *req) {
    u8 best;
    s32 found;
    s32 n;
    s32 i;
    CASSequence *seq;

    best = 0xFF;
    found = -1;
    n = set->numSequences;
    for (i = 0; i < n; i++) {
        seq = &set->sequences[i];
        if ((rt->cas->flags & (1 << seq->sub)) && seq->prio <= best
            && (seq->action < 0 || seq->action == 0 || seq->action == req[1])
            && !(seq->actions[0] == 0xFF && seq->actions[1] == 0xFF && seq->actions[2] == 0xFF && seq->unk0D == 0
                 && seq->unk0E == 0 && seq->nextAction == 0 && seq->onStart == NULL && seq->startMsg == 0)) {
            best = seq->prio;
            found = seq->sub;
        }
    }
    return found == -1 ? 0 : found;
}

void CASInit(s32 id, CASAnimSet *sets, CASAction *actions, s32 maxRequests) {
    CASAnimSet *base = sets;
    CASAction *act = actions;
    s32 max = maxRequests;
    GameObjectRuntime *rt;
    CASState *cas;
    CASQueue *queue;
    CASSequence *seq;
    s16 n;
    int i;

    rt = gameObjectsInfo[id].runtime;
    i = 0;
loop:
    n = 0;
    seq = sets->sequences;
count:
    if ((seq++)->action >= 0) {
        n++;
        goto count;
    }
    sets->numSequences = n;
    i++;
    sets++;
    if (i < CAS_NUM_ANIM_SETS) {
        goto loop;
    }
    cas = EGSMemMalloc(sizeof(CASState));
    if (cas == NULL) {
        EGSDebugAssert(1, 0, D_8010F7BC, 671);
    }
    queue = EGSMemMalloc(0xAC);
    if (queue == NULL) {
        EGSDebugAssert(1, 0, D_8010F7BC, 675);
    }
    queue->items = EGSMemMalloc(max * 4);
    if (queue->items == NULL) {
        EGSDebugAssert(1, 0, D_8010F7BC, 678);
    }
    queue->count = 0;
    queue->max = max;
    queue->slots.current = -1;
    queue->slots.numDeferred = 0;
    for (i = 0; i < 3; i++) {
        queue->slots.action[i] = 0xFF;
        queue->slots.state[i] = 0;
    }
    cas->active = 1;
    cas->mode = 1;
    cas->queue = queue;
    cas->animSets = base;
    cas->actionTable = act;
    rt->cas = cas;
    cas->flags = 0;
    rt->cas->flags2 = 0;
    if (D_8010521C.items == NULL) {
        D_8010521C.count = 0;
        D_8010521C.max = 20;
        D_8010521C.items = EGSMemMalloc(0xAC);
        if (D_8010521C.items == NULL) {
            EGSDebugAssert(1, 0, D_8010F7BC, 719);
        }
    }
}

#ifndef _PSYQ
/* INCLUDE_ASM functions still address these through their splat labels */
__asm__(
        ".set D_8010521D, D_8010521C+0x1\n"
        ".set D_80105220, D_8010521C+0x4\n"
        ".set D_80115670, D_80115638+0x38\n");
#endif

void CASReset(s32 id) {
    GameObjectRuntime *rt;
    CASState *cas;
    CASQueue *queue;
    int i;

    rt = gameObjectsInfo[id].runtime;
    cas = rt->cas;
    queue = cas->queue;
    queue->slots.current = -1;
    queue->slots.numDeferred = 0;
    for (i = 0; i < 3; i++) {
        queue->slots.action[i] = 0xFF;
        queue->slots.state[i] = 0;
    }
    cas->active = 1;
    cas->mode = 2;
    rt->cas->flags = 0;
    rt->cas->flags2 = 0;
}


/* Per-frame hip offset of an animation. */
typedef struct CASHipMotion {
    s8 x, y, z, pad;
} CASHipMotion;

/* .bss: one object, since aspsx would 16-align a separate .lcomm for the flags at +0x38.
 * It starts at 0x80115638 (psylink starts each .bss contribution 8-aligned; 0x80115634
 * is the previous object's padding, reached as hip[frame - 1] with frame >= 1). */
typedef struct CASHipTable {
    CASHipMotion hip[14]; /* generated hip motion (default anim), frames 1..14 */
    u8 queued[0x68];      /* per-action "already queued this frame" flags */
} CASHipTable;
static CASHipTable D_80115638;
#define D_80115670 (D_80115638.queued)

typedef struct CASModelExt {
    /* 0x000 */ u8 unk000[0x164];
    /* 0x164 */ u8 *data;
} CASModelExt;

/* GameObjectModel with the field past 0x28 */
typedef struct CASModel {
    /* 0x00 */ u8 unk00[0x28];
    /* 0x28 */ u32 flags;
    /* 0x2C */ void *data; /* u8 * if flags & 0x2000000, else CASModelExt * */
} CASModel;

void CASGetCurrentAnim(s32 id, s32 *anim, s32 *frame) {
    HAnim *ha;

    ha = EGSDisplayGetHAWithType(gameObjectsInfo[id].runtime->anim, 0);
    *anim = ha->anim;
    *frame = ha->frame;
}

void CASSetAnimFrame(s32 id, s32 anim, s32 frame) {
    GameObjectRuntime *rt;

    rt = gameObjectsInfo[id].runtime;
    EGSDisplayHAStartUnique(rt->anim, anim, 0);
    EGSDisplayHASetCurrentFrame(rt->anim, anim, frame);
    if (rt->anim->unk00[8] & 0x10) {
        EGSDisplayAnimateHMD(rt->anim);
        EGSFrameUpdateHierarchy(rt->anim->frame);
        rt->anim->unk00[8] |= 4;
        rt->anim->unk00[8] &= ~0x10;
    }
}

CASHipMotion *CASGetHipMotion(s32 id) {
    GameObjectRuntime *rt;
    HAnim *ha;
    s32 frame;
    CASHipMotion *hips;
    CASModel *model;
    u8 *data;
    s32 i;
    s32 t;

    rt = gameObjectsInfo[id].runtime;
    ha = EGSDisplayGetHAWithType(rt->anim, 0x4000);
    if (ha != NULL || (ha = EGSDisplayGetHAWithType(rt->anim, 0)) != NULL) {
        frame = ha->frame;
        hips = EGSDisplayHAGetHipMotionArray(ha);
        if (hips != NULL) {
            if (GOBJ_TYPE(id) != 0x3C) {
                model = (CASModel *)rt->anim->model;
                if (model->flags & 0x2000000) {
                    data = model->data;
                } else {
                    data = ((CASModelExt *)model->data)->data;
                }
                if (data[0] == 0xEA) {
                    data += data[1] + (data[2] << 8);
                }
                if (ha->data == data) {
                    if (D_8010F7C4 != 0) {
                        for (i = 0; i < 14; i++) {
                            t = (i + 1) * 4096;
                            D_80115638.hip[i].x = rcos(t / 14) / 4096;
                            D_80115638.hip[i].y = 0x62 - rcos(t / 7) / 2048;
                            D_80115638.hip[i].z = 0x19 - rcos(t / 7) / 1024;
                        }
                        D_8010F7C4 = 0;
                    }
                    return &D_80115638.hip[frame - 1];
                }
            }
            if (frame == 1 && ha->unk2C == 0) {
                return &hips[1];
            }
            return &hips[frame - 1];
        }
    }
    return NULL;
}

static u8 func_8002A35C(CASQueue *queue, s32 action) {
    u8 n;

    n = queue->count;
    if (n < queue->max) {
        queue->count = n + 1;
        queue->items[n] = action;
    }
    return 1;
}

void CASRequestAction(s32 id, s32 action) {
    GameObjectRuntime *rt;
    CASQueue *queue;

    rt = gameObjectsInfo[id].runtime;
    queue = rt->cas->queue;
    if (action == 0) {
        return;
    }
    if (D_8010FF8C != 0 && D_8010FF88 == rt->id) {
        if ((u32)D_8010FF8C < (u32)(gSystemTick - D_8010FF90)) {
            D_8010FF8C = 0;
            D_8010FF90 = 0;
        }
    } else if (func_8002A35C(queue, action) && !EGSListFind(&D_8010F7B8, 0, rt)) {
        EGSListAdd(&D_8010F7B8, rt);
    }
}

void CASRequestDelay(s16 id, s32 ticks) {
    D_8010FF88 = id;
    D_8010FF8C = ticks;
    D_8010FF90 = gSystemTick;
}

/* started = 1 goes after the HAStart3 call: sched1 hoists it (started already crosses calls)
 * right in front of the jal, after the arg loads, so reorg puts its spill store in the slot. */
/* Start anim set `set`'s sequence on each body layer (full body 0, upper 0x4000, lower
 * 0x8000), taking the CAS action slots and hooking up the event handler. */
static void func_8002A4A0(GameObjectRuntime *rt, CASAnimSet *set, u8 *req, s32 sub) {
    u8 r[2];
    s16 idx;
    s16 lower;
    CASAction *table;
    u8 keep;
    u8 started;
    CASQueue *queue;
    struct CASSlots *t;
    CASSequence *seq;
    HAnim *ha;
    CASAnimEvent *e;
    s32 layer;
    u32 mask;
    s16 action;
    s32 frame;
    s16 upper;
    s32 i;

    idx = 0;
    lower = 0xFF;
    table = rt->cas->actionTable;
    queue = rt->cas->queue;
    frame = 0;
    r[0] = req[0];
    action = 0xFF;
    r[1] = req[1];
    upper = 0xFF;
    keep = 0;
    started = 0;
    seq = CASGetAnimSequence(set, req[1], sub);
    t = &queue->slots;
    if (seq->unk0D != 0) {
        rt->cas->active = seq->unk0D;
        rt->cas->mode = seq->unk0E;
    }
    if (seq->actions[0] == 0xFF && seq->actions[1] == 0xFF && seq->actions[2] == 0xFF) {
        if (seq->nextAction != 0) {
            CASRequestAction(rt->id, seq->nextAction);
        }
        if (seq->onStart != NULL || seq->startMsg != 0) {
            i = queue->slots.numDeferred;
            t->deferred[i].id = rt->id;
            t->deferred[i].req0 = r[0];
            t->deferred[i].mode = r[1];
            t->deferred[i].sub = sub;
            t->deferred[i].set = set;
            queue->slots.numDeferred++;
        }
    } else if (table != NULL && func_80029794(rt, set, r, sub)
               && (((void **)rt->anim)[((CASCamera *)*gCamera)->index] != NULL || rt->id == gPlayerOffset)) {
        layer = 0;
        do {
            mask = 0;
            if (layer == 0) {
                idx = 0;
                action = seq->actions[2];
                frame = seq->unk09[2];
            } else if (layer == 0x4000) {
                if (upper != 0xFF && upper != -666) {
                    action = upper;
                    keep = 1;
                } else {
                    action = seq->actions[1];
                    frame = seq->unk09[1];
                }
                idx = 1;
            } else if (layer == 0x8000) {
                if (lower != 0xFF && lower != -666) {
                    action = lower;
                    keep = 1;
                } else {
                    action = seq->actions[0];
                    frame = seq->unk09[0];
                }
                idx = 0;
            }
            if (((layer == 0 || layer == 0x8000) && (rt->cas->flags2 & seq->needFlags2))
                || (layer == 0x4000 && (rt->cas->flags2 & seq->blockFlags2))) {
                action = 0xFF;
            }
            if (action != 0xFF) {
                ha = EGSDisplayGetHAWithType(rt->anim, layer);
                if (ha == NULL || ha->anim != action || frame == -2 || lower != 0xFF || upper != 0xFF
                    || frame == -3) {
                    if (frame == -3) {
                        frame = 0;
                    }
                    if (ha != NULL) {
                        if (keep) {
                            frame = ha->unk2C;
                        }
                        EGSDisplayHAStop(rt->anim, ha->anim);
                    }
                    if (frame != -2) {
                        EGSDisplayHAStart3(rt->anim, action, frame, layer, 20);
                        started = 1;
                        if (layer == 0x8000 && lower == 0xFF) {
                            if (seq->actions[1] == 0xFF) {
                                ha = EGSDisplayGetHAWithType(rt->anim, 0x4000);
                                if (ha != NULL) {
                                    upper = ha->anim;
                                }
                            } else {
                                upper = -666;
                            }
                        } else if (layer == 0x4000 && upper == 0xFF) {
                            if (seq->actions[0] == 0xFF) {
                                ha = EGSDisplayGetHAWithType(rt->anim, 0x8000);
                                if (ha != NULL) {
                                    lower = ha->anim;
                                }
                            } else {
                                lower = -666;
                            }
                        }
                        if (table[action].hasEvents) {
                            func_80029B78(rt, -1, action);
                            if (table[action].done.event != 0) {
                                if (t->current == -1) {
                                    s32 j;

                                    for (j = 0; j < 3; j++) {
                                        if (t->state[j] == 0) {
                                            t->current = j;
                                            break;
                                        }
                                    }
                                }
                                t->state[t->current] = 1;
                                t->action[t->current] = action;
                                mask |= table[action].done.event;
                            }
                            if (table[action].events != NULL) {
                                mask |= 0x04000000 | (table[action].events->code & 0xFF);
                            }
                        }
                        mask |= 0x80000000;
                        if (!keep) {
                            t->events[idx].id = rt->id;
                            t->events[idx].action = action;
                            t->events[idx].set = set;
                            t->events[idx].req0 = r[0];
                            t->events[idx].mode = r[1];
                            t->events[idx].sub = sub;
                        }
                        ha = EGSDisplayGetHAWithType(rt->anim, layer);
                        ha->handler = func_80029298;
                        ha->eventMask = mask;
                        ha->handlerArg = &t->events[idx];
                    }
                }
            }
            if (keep || (layer == 0x8000 && lower == -666) || (layer == 0x4000 && upper == -666)) {
                layer = -1;
            } else if (layer == 0) {
                layer = 0x4000;
            } else if (layer == 0x4000 || lower != 0xFF) {
                layer = 0x8000;
            } else if (upper != lower) {
                layer = 0x4000;
            } else {
                layer = -1;
            }
        } while (layer != -1);
        if (started && (rt->anim->unk00[8] & 0x10)) {
            EGSDisplayAnimateHMD(rt->anim);
            EGSFrameUpdateHierarchy(rt->anim->frame);
            rt->anim->unk00[8] |= 4;
            rt->anim->unk00[8] &= ~0x10;
        }
    }
}

/* Finish every slot whose action completed: run its callback/message and free it. */
static void func_8002AC1C(GameObjectRuntime *rt, CASQueue *queue) {
    struct CASSlots *t;
    CASAnimSet *set;
    CASAction *action;
    s32 idx;
    s32 arg;
    s32 i;

    t = &rt->cas->queue->slots;
    for (i = 0; i < 3; i++) {
        if (t->state[i] == 2) {
            idx = t->set[i];
            arg = t->sub[i];
            set = &rt->cas->animSets[idx];
            action = &((CASAction *)rt->cas->actionTable)[t->action[i]];
            t->current = i;
            func_80029920(rt, set, t->req[i], arg);
            if (action->done.call && set->onDone != NULL) {
                set->onDone(rt->id, idx);
            }
            if (action->done.post && set->doneMsg != 0) {
                SysPostMessage(set->doneMsg, 5, rt->id, rt->id, 0, 0, 0, 0);
            }
            t->state[i] = 0;
            t->action[i] = 0xFF;
            t->set[i] = 0;
            t->req[i][0] = 0;
            t->req[i][1] = 0;
            t->sub[i] = 0;
        }
    }
}

/* Run the frame's de-duplicated requests against a gobj until none is accepted. */
static void func_8002AD98(GameObjectRuntime *rt, CASQueue *list) {
    s32 i;
    CASAction *table;
    u8 again;
    struct CASSlots *t;
    CASAnimSet *set;
    CASSequence *seq;
    CASAnimEvent *e;
    u8 *req;
    s32 action;
    s32 sub;
    s32 j;

    t = &rt->cas->queue->slots;
    table = rt->cas->actionTable;
    do {
        again = 0;
        for (i = 0; i < list->count; i++) {
            u8 post;
            u8 call;

            call = 0;
            action = list->items[i];
            post = 0;
            if (action != 0) {
                sub = 0;
                set = &rt->cas->animSets[action];
                req = &rt->cas->active;
                D_80115670[action] = 0;
                if (set->sequences != NULL) {
                    sub = func_80029C04(rt, set, req);
                }
                t->current = -1;
                for (j = 0; j < 3; j++) {
                    if (t->action[j] != 0xFF
                        && (t->action[j] == CASGetAnimSequence(set, req[1], sub)->actions[0]
                            || t->action[j] == CASGetAnimSequence(set, req[1], sub)->actions[1]
                            || t->action[j] == CASGetAnimSequence(set, req[1], sub)->actions[2])) {
                        t->current = j;
                        break;
                    }
                }
                if (func_80029838(rt, set, req)) {
                    func_8002A4A0(rt, set, req, sub);
                    func_80029920(rt, set, req, sub);
                    /* three copies (merged by cross-jumping) give post enough refs to win fp over i */
                    if (table == NULL) {
                        call = 1;
                        post = 1;
                    } else if (t->current == -1) {
                        call = 1;
                        post = 1;
                    } else if (t->state[t->current] != 1) {
                        call = 1;
                        post = 1;
                    }
                    if (call && set->onDone != NULL) {
                        set->onDone(rt->id, action);
                    }
                    if (post && set->doneMsg != 0) {
                        SysPostMessage(set->doneMsg, 2, rt->id, rt->id, 0, 0, 0, 0);
                    }
                    again = 1;
                    list->items[i] = 0;
                }
                if (t->numDeferred != 0) {
                    for (j = 0; j < t->numDeferred; j++) {
                        e = &t->deferred[j];
                        seq = CASGetAnimSequence(e->set, e->mode, e->sub);
                        if (seq->onStart != NULL) {
                            seq->onStart(e->id, e->set->unk04);
                        }
                        if (seq->startMsg != 0) {
                            SysPostMessage(seq->startMsg, 5, rt->id, rt->id, 0, 0, 0, 0);
                        }
                    }
                    t->numDeferred = 0;
                }
            }
        }
    } while (again);
    list->count = 0;
}

/* Copy a gobj's requests into the frame list, dropping actions already queued this frame. */
static void func_8002B138(CASQueue *queue) {
    int i;
    u8 *seen;

    D_8010521C.count = queue->count;
    for (i = 0; i < queue->count; i++) {
        seen = &D_80115670[queue->items[i]];
        if (*seen == 0) {
            *seen = 1;
            D_8010521C.items[i] = queue->items[i];
        } else {
            D_8010521C.items[i] = 0;
        }
        queue->items[i] = 0;
    }
    queue->count = 0;
}

void CASProcess(void) {
    EGSListNode *node;
    EGSListNode *next;
    GameObjectRuntime *rt;
    CASQueue *queue;

    node = D_8010F7B8;
    while (node != NULL) {
        rt = node->data;
        queue = rt->cas->queue;
        next = node->next;
        EGSListRemove(&D_8010F7B8, node);
        node = next;
        if (rt->id == gPlayerOffset || (rt->npc->flags & 0x200)) {
            func_8002AC1C(rt, queue);
            func_8002B138(queue);
            func_8002AD98(rt, &D_8010521C);
        }
    }
}

