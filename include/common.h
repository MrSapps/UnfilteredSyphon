#ifndef COMMON_H
#define COMMON_H

/*
 * cc1 2.7.2 with -G8 emits every function after all file-scope asm, so a plain
 * __asm__ INCLUDE_ASM would land out of order. Wrap each include in a dummy
 * function that maspsx strips again (see tools/maspsx/README.md).
 */
#if !defined(M2CTX) && !defined(PERMUTER)
/*
 * Each nonmatching .s is tagged by tools/split_parts.py: INC_PART 1 holds its
 * strings/consts (emitted in place, like cc1's data), INC_PART 2 its code and jump
 * tables (emitted with the buffered function bodies).
 */
#define INCLUDE_PART1(FOLDER, NAME)                                 \
    __asm__(".pushsection .rodata\n"                               \
            ".set INC_PART, 1\n"                                   \
            ".include \"" FOLDER "/" #NAME ".s\"\n"                \
            ".popsection\n");
#define INCLUDE_PART2(FOLDER, NAME, HACK)                           \
    void HACK() {                                                  \
        __asm__(".pushsection .text # maspsx-keep\n"               \
                "\t.align\t2 # maspsx-keep\n"                     \
                "\t.set noreorder # maspsx-keep\n"                 \
                "\t.set noat # maspsx-keep\n"                      \
                ".set INC_PART, 2 # maspsx-keep\n"                 \
                ".include \"" FOLDER "/" #NAME ".s\" # maspsx-keep\n" \
                "\t.set reorder # maspsx-keep\n"                   \
                "\t.set at # maspsx-keep\n"                        \
                ".popsection # maspsx-keep\n");                    \
    }
#define INCLUDE_ASM(FOLDER, NAME)  \
    INCLUDE_PART1(FOLDER, NAME)    \
    INCLUDE_PART2(FOLDER, NAME, __maspsx_include_asm_hack_##NAME)
#define INCLUDE_RODATA(FOLDER, NAME) \
    INCLUDE_PART1(FOLDER, NAME)      \
    INCLUDE_PART2(FOLDER, NAME, __maspsx_include_asm_hack_rodata_##NAME)
#endif

/*
 * aspsx only uses $gp for small data *defined in the same file*. Until a TU's
 * .sdata/.sbss is migrated into its C file, SDATA_LOCAL(sym) tells maspsx that sym
 * lives in this file's small data (the block is skipped by the assembler).
 */
#if !defined(M2CTX) && !defined(PERMUTER)
#define SDATA_LOCAL(sym) __asm__(".if 0\n.sdata\n" #sym ":\n.word 0\n.text\n.endif\n");
#else
#define SDATA_LOCAL(sym)
#endif

#include "include_asm.h"

typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;

#ifndef NULL
#define NULL ((void *)0)
#endif

#endif /* COMMON_H */
