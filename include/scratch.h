#ifndef SCRATCH_H
#define SCRATCH_H

/*
 * Calls made on the scratchpad stack (0x1F8003E0), saving the real $sp at 0x1F800000.
 * The original code does this inline around heavy engine calls; reproduced with inline
 * asm. The clobber list forces live values into callee-saved registers as cc1 did.
 * (Originally worked out for park2.c.)
 */
#define SCRATCH_CLOBBERS                                                               \
    "$2", "$3", "$4", "$5", "$6", "$7", "$8", "$9", "$10", "$11", "$12", "$13", "$14", \
        "$15", "$24", "$25", "$31", "memory"
#define SCRATCH_ENTER                                                                  \
    ".set noat\n\t"                                                                   \
    "lui $1, 0x1F80\n\tsw $29, 0($1)\n\tlui $29, 0x1F80\n\tori $29, $29, 0x3E0\n\t"
#define SCRATCH_LEAVE "lui $29, 0x1F80\n\tlw $29, 0($29)\n\t.set at"

#define SCRATCH_CALL0(fn) \
    __asm__ volatile(SCRATCH_ENTER "jal " #fn "\n\t" SCRATCH_LEAVE : : : SCRATCH_CLOBBERS)
#define SCRATCH_CALL1(fn, arg)                                                         \
    __asm__ volatile(SCRATCH_ENTER "move $4, %0\n\tjal " #fn "\n\t" SCRATCH_LEAVE \
                     :                                                                 \
                     : "r"(arg)                                                        \
                     : SCRATCH_CLOBBERS)
/* As SCRATCH_CALL1, but the argument move sits in the jal delay slot. */
#define SCRATCH_CALL1_DS(fn, arg)                                                      \
    __asm__ volatile(SCRATCH_ENTER ".set noreorder\n\tjal " #fn "\n\tmove $4, %0\n\t"  \
                     ".set reorder\n\t" SCRATCH_LEAVE                                  \
                     :                                                                 \
                     : "r"(arg)                                                        \
                     : SCRATCH_CLOBBERS)

/*
 * Constant-argument form: `ds` is the instruction placed in the jal delay slot.
 * maspsx always inserts its own nop after a `jal`, so the jal + delay slot pair is
 * hidden inside an assembler macro (defined once per file) that maspsx passes through.
 */
__asm__(".macro scratch_jal_ds op, fn, ds:vararg\n\t.set push\n\t.set noreorder\n\t\\op \\fn\n\t\\ds\n\t.set pop\n\t.endm");
#define SCRATCH_CALL_DS(fn, ds)                                                        \
    __asm__ volatile(SCRATCH_ENTER "scratch_jal_ds jal, " #fn ", " ds "\n\t" SCRATCH_LEAVE \
                     :                                                                 \
                     :                                                                 \
                     : SCRATCH_CLOBBERS)

#endif /* SCRATCH_H */
