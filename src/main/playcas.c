#include "common.h"
#include "level.h"
#include "gobj.h"

extern u8 gActionInfo[];
extern u8 gCASAnimInfo[];
void CASInit(s16 id, void *actions, void *anims, s32 count);
void CASReset(s16 id);

void PlayerCASInit(Gobj *gobj, s32 mode) {
    if (gobj->cas == NULL) {
        CASInit(gobj->id, gActionInfo, gCASAnimInfo, 20);
    } else {
        CASReset(gobj->id);
    }
    gobj->cas->active = 1;
    if (mode == 2) {
        gobj->cas->mode = 3;
    } else if (mode == 1) {
        gobj->cas->mode = 2;
    } else if (mode == 0) {
        gobj->cas->mode = 1;
    } else if (mode == 4) {
        gobj->cas->mode = 4;
    }
}

INCLUDE_RODATA("asm/main/nonmatchings/playcas", D_800102D0);

/* CAS callback: an NPC finished an action. */
void NPCActionDoneCAS(s16 id, s32 action) {
    NPCInfo *npc;

    if (id != gPlayerOffset) {
        switch (action) {
        case 0x0A:
        case 0x0B:
        case 0x2E:
        case 0x2F:
        case 0x30:
        case 0x3D:
        case 0x47:
        case 0x4B:
        case 0x54:
            npc = gameObjectsInfo[id].runtime->npc;
            if (npc != NULL && npc->casAction == action) {
                npc->casAction = 0;
            }
            break;
        }
    }
}
