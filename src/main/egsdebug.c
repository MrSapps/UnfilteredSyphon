#include "common.h"

SDATA_LOCAL(D_8010FF10)
SDATA_LOCAL(D_8010FF14)

extern u8 D_8010FF10;             /* debug output level */
extern void (*D_8010FF14)(void);  /* assert callback */
extern char D_8010FF18[];         /* "\t%s\n" (.sdata) */

void printf();

register s32 __sp asm("$29");

/* The stack lives in KSEG0 (negative) on the dev kit. The empty volatile asms
 * keep reorg from filling the bgez $sp delay slots, as in the original. */
#define ON_DEV_KIT() ({ __asm__ volatile(""); __sp < 0; })

s32 EGSDebugInit(u8 level) {
    D_8010FF10 = level;
    return 0;
}

s32 EGSDebugPrinter(char *fmt, ...) {
    if (ON_DEV_KIT() && D_8010FF10 == 1) {
        __asm__ volatile("");
        printf(fmt);
    }
    return 0;
}

void EGSDebugAssertCallback(void (*callback)(void)) {
    D_8010FF14 = callback;
}

void DoAssertPrint(char *msg) {
    printf("Assertion Failed in file %s on line %d:\n");
    if (msg != NULL) {
        printf(D_8010FF18, msg);
    }
}

/* int (not void as in game.h): keeps $v0 live so reorg leaves the li v0,1 alone. */
int EGSDebugAssert(u8 halt, char *msg, char *file, int line) {
    if (halt) {
        if (ON_DEV_KIT()) {
            __asm__ volatile("");
            printf("Assertion Failed in file %s on line %d:\n", file, line);
            if (msg != NULL) {
                printf(D_8010FF18, msg);
            }
            printf("Program Halted!\n");
        }
        if (D_8010FF14 != NULL) {
            D_8010FF14();
        } else {
            __asm__ volatile(".word 0x0000200D"); /* break 0, 128 */
        }
    }
    if (ON_DEV_KIT() && D_8010FF10 == 1) {
        __asm__ volatile("");
        printf("Assertion Failed in file %s on line %d:\n", file, line);
        if (msg != NULL) {
            printf(D_8010FF18, msg);
        }
    }
}
