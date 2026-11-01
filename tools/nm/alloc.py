#!/usr/bin/env python3
"""alloc.py src func : global-alloc view of a function (gcc 2.7.2 global.c allocno_compare).
Prints each multi-block pseudo: refs, live length, priority, hard reg, sorted by
allocation order (priority desc, then pseudo number)."""
import math, os, re, subprocess, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
D = ROOT / "build/nm/dump"
D.mkdir(parents=True, exist_ok=True)
sys.path.insert(0, os.path.dirname(__file__))
from score import tu_vars

REGS = "zero at v0 v1 a0 a1 a2 a3 t0 t1 t2 t3 t4 t5 t6 t7 s0 s1 s2 s3 s4 s5 s6 s7 t8 t9 k0 k1 gp sp fp ra".split()


def main():
    src, func = sys.argv[1], sys.argv[2]
    defs = sys.argv[3] if len(sys.argv) > 3 else "-DNON_MATCHING"
    on, g = tu_vars(src)
    i = D / "x.i"
    cmd = (f"(printf '#line 1 \"{on}\"\\n'; cat {src}) | cpp -P -undef -D__GNUC__=2 -Dmips -D__psx__ "
           f"-DINCLUDE_ASM_USE_MACRO_INC=1 {defs} -Iinclude -Isrc > {i} && cd {D} && "
           f"{ROOT}/tools/compilers/gcc-2.7.2-psx/cc1 -quiet -O2 -G{g} -mgas -mgpOPT -mgpopt -msoft-float "
           f"-fno-builtin -funsigned-char -dl -dg x.i -o x.s")
    subprocess.run(["bash", "-c", cmd], cwd=ROOT, capture_output=True)

    def section(path):
        t = Path(path).read_text()
        m = re.search(r"^;; Function %s\n(.*?)(?=^;; Function |\Z)" % re.escape(func), t, re.M | re.S)
        return m.group(1) if m else ""

    lreg = section(D / "x.i.lreg")
    greg = section(D / "x.i.greg")
    disp = dict((int(a), int(b)) for a, b in re.findall(r"(\d+) in (\d+)", greg.split("Register dispositions:")[1].split("\n\n")[0]))
    rows = []
    for m in re.finditer(r"^Register (\d+) used (\d+) times across (\d+) insns(.*)$", lreg, re.M):
        r, refs, live, rest = int(m.group(1)), int(m.group(2)), int(m.group(3)), m.group(4)
        if " in block " in rest:
            continue                      # local-alloc pseudo
        pri = int((math.floor(math.log2(refs)) * refs / max(live, 1)) * 10000) if refs else 0
        calls = re.search(r"crosses (\d+) call", rest)
        rows.append((-pri, r, refs, live, calls.group(1) if calls else "0", disp.get(r)))
    rows.sort()
    for npri, r, refs, live, calls, h in rows:
        print(f"p{r:<5} refs={refs:<4} live={live:<5} calls={calls:<3} pri={-npri:<7} -> {REGS[h] if h is not None else 'stack'}")


if __name__ == "__main__":
    main()
