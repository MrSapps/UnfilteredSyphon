#!/usr/bin/env python3
"""Score NON_MATCHING functions: compile TU with -DNON_MATCHING, diff vs asm .s.
usage: score.py [src files...]  (default: all with NON_MATCHING)
prints: difflines total name file
"""
import difflib, re, subprocess, sys, glob, os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "build/nm"
OUT.mkdir(parents=True, exist_ok=True)
os.chdir(ROOT)
ninja = (ROOT / "build.ninja").read_text()


def tu_vars(src):
    i = ninja.index("build build/%s.o: cc " % src)
    blk = ninja[i:ninja.find("\nbuild ", i + 1)]
    v = dict(re.findall(r"^  (\w+) = (.*)$", blk, re.M))
    return v.get("origname", src), v.get("gsize", "8")


def compile(src, defs="-DNON_MATCHING"):
    on, g = tu_vars(src)
    o = OUT / (src.replace("/", "_") + ".o")
    cmd = (f"(printf '#line 1 \"{on}\"\\n'; cat {src}) | cpp -P -undef -D__GNUC__=2 -Dmips -D__psx__ "
           f"-DINCLUDE_ASM_USE_MACRO_INC=1 {defs} -Iinclude -Isrc | tools/compilers/gcc-2.7.2-psx/cc1 -quiet -O2 -G{g} "
           f"-mgas -mgpOPT -mgpopt -msoft-float -fno-builtin -funsigned-char -o - | .venv/bin/python3 tools/maspsx/maspsx.py "
           f"--aspsx-version=2.79 -G{g} --expand-div --use-comm-section | sed 's/^\\s*\\.rdata\\s*$/\\t.section .rodata/' | "
           f"tools/bin/mipsel-linux-gnu-as -EL -march=r3000 -mtune=r3000 -no-pad-sections -O1 -G0 -Iinclude -o {o} -")
    r = subprocess.run(["bash", "-o", "pipefail", "-c", cmd], capture_output=True, text=True)
    if r.returncode:
        print("COMPILE FAIL", src, r.stderr[-2000:], file=sys.stderr)
        return None
    return o


def norm(line):
    line = re.sub(r"0x[0-9a-fA-F]+|\b\d+\b", "N", line)
    line = re.sub(r"%\w+\([^)]*\)", "N", line)
    line = re.sub(r"\.L\w+|\b[A-Za-z_]\w*_[0-9A-F]{8}\b|\b(?:func|D|jtbl)_\w+", "L", line)
    return " ".join(line.split())


def built_insns(o, name):
    d = subprocess.run(["tools/bin/mipsel-linux-gnu-objdump", "-d", "-z", "--disassemble=" + name, "-M", "no-aliases,gpr-names=32", str(o)],
                       capture_output=True, text=True).stdout
    ins = []
    for l in d.splitlines():
        m = re.match(r"\s*[0-9a-f]+:\s+[0-9a-f]{8}\s+(.*)", l)
        if m:
            ins.append(m.group(1).split("#")[0])
    return ins


def orig_insns(sfile):
    ins = []
    for l in open(sfile):
        m = re.match(r"\s*/\* \w+ \w+ ([0-9A-F]{8}) \*/\s+(.*)", l)
        if m:
            ins.append(m.group(1))
    return ins


def disasm_words(words):
    import rabbitizer
    rabbitizer.config.regNames_gprAbiNames = rabbitizer.Abi.NUMERIC
    rabbitizer.config.misc_omit0XOnSmallImm = False
    out = []
    for i, w in enumerate(words):
        x = int.from_bytes(bytes.fromhex(w), "little")
        out.append(rabbitizer.Instruction(x, 0x80000000 + 4 * i).disassemble())
    return out


def built_words(o, name):
    d = subprocess.run(["tools/bin/mipsel-linux-gnu-objdump", "-d", "-z", "--disassemble=" + name, str(o)],
                       capture_output=True, text=True).stdout
    w = []
    for l in d.splitlines():
        m = re.match(r"\s*[0-9a-f]+:\s+([0-9a-f]{8})\s", l)
        if m:
            h = m.group(1)
            w.append("".join(reversed([h[i:i + 2] for i in range(0, 8, 2)])).upper())
    return w


def maskw(w):
    x = int.from_bytes(bytes.fromhex(w), "little")
    op = x >> 26
    if op in (2, 3):
        return x & 0xFC000000
    if op >= 4:
        return x & 0xFFFF0000
    return x


def score(o, name, sfile):
    a = built_words(o, name)
    b = orig_insns(sfile)
    if not a:
        return None
    am = [maskw(w) for w in a]
    bm = [maskw(w) for w in b]
    sm = difflib.SequenceMatcher(None, am, bm, autojunk=False)
    same = sum(bl.size for bl in sm.get_matching_blocks())
    return max(len(a), len(b)) - same, len(b)


def nm_funcs(src):
    t = open(src).read()
    asmdirs = re.findall(r'#else\nINCLUDE_ASM\("([^"]+)", (\w+)\);\n#endif', t)
    return asmdirs


def main():
    srcs = sys.argv[1:] or sorted(set(subprocess.run(["grep", "-rl", "#ifdef NON_MATCHING", "src"], capture_output=True, text=True).stdout.split()))
    res = []
    for src in srcs:
        fs = nm_funcs(src)
        if not fs:
            continue
        o = compile(src)
        if not o:
            continue
        for d, n in fs:
            s = score(o, n, f"{d}/{n}.s")
            if s:
                res.append((s[0], s[1], n, src))
                print(s[0], s[1], n, src, flush=True)
    res.sort()
    with open(OUT / "scores.txt", "w") as f:
        for r in res:
            f.write("%d %d %s %s\n" % r)


if __name__ == "__main__":
    main()
