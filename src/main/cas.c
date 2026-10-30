#include "common.h"
#include "system.h"
#include "gameobj.h"
#include "gobj.h"
#include "level.h"

/* cas.c's small data, still assembled from asm */
SDATA_LOCAL(D_8010F7B8)
SDATA_LOCAL(D_8010F7C4)
SDATA_LOCAL(D_8010FF88)
SDATA_LOCAL(D_8010FF8C)
SDATA_LOCAL(D_8010FF90)

extern EGSListNode *D_8010F7B8; /* gobjs with pending CAS requests */
extern s16 D_8010FF88;          /* gobj whose requests are being delayed */
extern s32 D_8010FF8C;          /* delay length in ticks (0: none) */
extern s32 D_8010FF90;          /* delay start tick */
extern CASQueue D_8010521C;     /* this frame's de-duplicated action list */
extern u8 D_80115670[];         /* per-action "already queued this frame" flags */
extern s32 gSystemTick;
extern u8 gNullSequence[];

u8 EGSListFind(EGSListNode **list, s32 arg1, void *data);
void *EGSDisplayGetHAWithType(void *anim, s32 type);
void EGSDisplayHAStartUnique(void *anim, s32 arg1);
void EGSDisplayHASetCurrentFrame(void *anim, s32 anim1, s32 frame);
void EGSDisplayAnimateHMD(void *anim);
void EGSFrameUpdateHierarchy(void *frame);
static void func_8002AC1C();

typedef struct CASSequence {
    /* 0x00 */ s16 action; /* <= 0 terminates */
    /* 0x02 */ s16 sub;    /* 0: any */
    /* 0x04 */ u8 needFlags2;
    /* 0x05 */ u8 blockFlags2;
    /* 0x06 */ u8 unk06[0x12];
} CASSequence; /* size 0x18 */

typedef struct CASAnimSet {
    /* 0x00 */ u8 unk00[0x18];
    /* 0x18 */ CASSequence *sequences;
} CASAnimSet;

typedef struct CASRule {
    /* 0x00 */ u32 stateMask; /* 0: any; else bit per request state */
    /* 0x04 */ u8 unk04[4];
    /* 0x08 */ u32 require;   /* CAS flags that must be set */
    /* 0x0C */ s32 forbid;    /* CAS flags that must be clear; -1: flags must equal require */
} CASRule;

typedef struct CASAction {
    /* 0x00 */ u8 unk00[4];
    /* 0x04 */ u32 setFlags;
    /* 0x08 */ u32 clearFlags;
    /* 0x0C */ u32 setFlags2;
    /* 0x10 */ u32 clearFlags2;
    /* 0x14 */ u8 unk14[0xC];
} CASAction; /* size 0x20 */
static void func_8002AD98();

void SYSCasInit(void) {
    D_8010521C.items = NULL;
    D_8010F7B8 = NULL;
    D_8010FF88 = -1;
    D_8010FF8C = 0;
    D_8010FF90 = 0;
}

INCLUDE_ASM("asm/main/nonmatchings/cas", func_80029298);

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

#ifdef NON_MATCHING
/* result lives in v1 instead of v0 */
static s32 func_80029794(GameObjectRuntime *rt, CASAnimSet *set, u8 *req, s32 sub) {
    struct CASSlots *slots;
    CASSequence *seq;
    u32 flags2;
    s32 ret;

    slots = &rt->cas->queue->slots;
    if (slots->current != -1 && slots->state[slots->current] == 2) {
        ret = 0;
    } else {
        seq = CASGetAnimSequence(set, req[1], sub);
        flags2 = rt->cas->flags2;
        ret = 1;
        if (flags2 & seq->needFlags2) {
            ret = (flags2 & seq->blockFlags2) == 0;
        }
    }
    return ret;
}
#else
INCLUDE_ASM("asm/main/nonmatchings/cas", func_80029794);
#endif

#ifdef NON_MATCHING
/* final test: original branches to set 0/1 instead of sltiu (likely a function-wide result variable) */
/* May a request in state *req run under this rule? */
static s32 func_80029838(GameObjectRuntime *rt, CASRule *rule, u8 *req) {
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
    flags = rt->cas->flags;
    have = flags & rule->require;
    if (have != rule->require) {
        return 0;
    }
    if (rule->forbid == -1 && flags != have) {
        return 0;
    }
    return !(rt->cas->flags & rule->forbid);
}
#else
INCLUDE_ASM("asm/main/nonmatchings/cas", func_80029838);
#endif

INCLUDE_ASM("asm/main/nonmatchings/cas", func_80029920);

#ifdef NON_MATCHING
/* same instructions, register numbering differs */
/* Leave action `from` and enter action `to`, updating the CAS flag words. */
static void func_80029B78(GameObjectRuntime *rt, s32 from, s32 to) {
    CASState *cas;
    CASAction *table;
    CASAction *a;

    cas = rt->cas;
    table = cas->actionTable;
    if (from != -1) {
        a = &table[from];
        cas->flags2 &= ~a->clearFlags2;
        rt->cas->flags &= ~a->clearFlags;
    }
    if (to != -1) {
        a = &table[to];
        rt->cas->flags2 |= a->setFlags2;
        rt->cas->flags |= a->setFlags;
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/cas", func_80029B78);
#endif

INCLUDE_ASM("asm/main/nonmatchings/cas", func_80029C04);

INCLUDE_ASM("asm/main/nonmatchings/cas", CASInit);

void CASReset(s32 id) {
    GameObjectRuntime *rt;
    CASState *cas;
    CASQueue *queue;
    int i;

    rt = gameObjectsInfo[id].runtime;
    cas = rt->cas;
    queue = cas->queue;
    queue->slots.current = -1;
    queue->unk6C = 0;
    for (i = 0; i < 3; i++) {
        queue->slots.prio[i] = 0xFF;
        queue->slots.state[i] = 0;
    }
    cas->active = 1;
    cas->mode = 2;
    rt->cas->flags = 0;
    rt->cas->flags2 = 0;
}

typedef struct HAnim {
    /* 0x00 */ u8 unk00[4];
    /* 0x04 */ s16 anim;
    /* 0x06 */ u8 unk06[0xE];
    /* 0x14 */ s32 frame;
} HAnim;

void CASGetCurrentAnim(s32 id, s32 *anim, s32 *frame) {
    HAnim *ha;

    ha = EGSDisplayGetHAWithType(gameObjectsInfo[id].runtime->anim, 0);
    *anim = ha->anim;
    *frame = ha->frame;
}

#ifdef NON_MATCHING
/* scheduling: original saves both params into s-regs in the prologue */
void CASSetAnimFrame(s32 id, s32 anim, s32 frame) {
    GameObjectRuntime *rt;

    rt = gameObjectsInfo[id].runtime;
    EGSDisplayHAStartUnique(rt->anim, 0);
    EGSDisplayHASetCurrentFrame(rt->anim, anim, frame);
    if (rt->anim->unk00[8] & 0x10) {
        EGSDisplayAnimateHMD(rt->anim);
        EGSFrameUpdateHierarchy(rt->anim->frame);
        rt->anim->unk00[8] |= 4;
        rt->anim->unk00[8] &= ~0x10;
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/cas", CASSetAnimFrame);
#endif

INCLUDE_ASM("asm/main/nonmatchings/cas", CASGetHipMotion);

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

INCLUDE_ASM("asm/main/nonmatchings/cas", func_8002A4A0);

INCLUDE_ASM("asm/main/nonmatchings/cas", func_8002AC1C);

INCLUDE_ASM("asm/main/nonmatchings/cas", func_8002AD98);

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

#ifdef NON_MATCHING
/* s0/s1 swapped between list cursor and gobj */
void CASProcess(void) {
    EGSListNode *node;
    EGSListNode *cur;
    GameObjectRuntime *rt;
    CASQueue *queue;

    node = D_8010F7B8;
    while (node != NULL) {
        rt = node->data;
        cur = node;
        queue = rt->cas->queue;
        node = cur->next;
        EGSListRemove(&D_8010F7B8, cur);
        if (rt->id == gPlayerOffset || (rt->npc->flags & 0x200)) {
            func_8002AC1C(rt, queue);
            func_8002B138(queue);
            func_8002AD98(rt, &D_8010521C);
        }
    }
}
#else
INCLUDE_ASM("asm/main/nonmatchings/cas", CASProcess);
#endif

INCLUDE_RODATA("asm/main/nonmatchings/cas", D_80010F50);
