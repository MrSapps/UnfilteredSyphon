#!/usr/bin/env python3
"""Compile a C file with several compiler setups and score each function against the original.

    tools/fingerprint.py test.c [--cc gcc-2.7.2-psx ...] [--opt -O2] [--G 8]

Relocated immediates (jal targets, %hi/%lo/%gp_rel) are masked when comparing.
"""
import argparse
import re
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CROSS = ROOT / "tools/bin/mipsel-linux-gnu-"


def syms():
    out = {}
    for p in (ROOT / "config/symbols").glob("*.txt"):
        for line in p.read_text().splitlines():
            m = re.match(r"(\S+) = 0x([0-9A-Fa-f]+);", line)
            if m:
                out[m.group(1)] = int(m.group(2), 16)
    return out


def orig_words(addr, n):
    if 0x80010000 <= addr < 0x801377E0:
        data = (ROOT / "orig/scus_942.40").read_bytes()
        off = addr - 0x80010000 + 0x800
    else:
        raise SystemExit("overlay functions not supported yet")
    return list(struct.unpack_from(f"<{n}I", data, off))


def compile_obj(src, cc, opt, g, aspsx, workdir):
    ccdir = ROOT / "tools/compilers" / cc
    s = workdir / "out.s"
    o = workdir / "out.o"
    cpp = subprocess.run(
        ["cpp", "-P", "-undef", "-D__GNUC__=2", "-Dmips", "-D__psx__", f"-I{ROOT}/include", str(src)],
        check=True, capture_output=True,
    ).stdout
    asm = subprocess.run(
        [str(ccdir / "cc1"), "-quiet", opt, f"-G{g}", "-msoft-float",
         "-fno-builtin", "-funsigned-char", "-o", "-"],
        input=cpp, check=True, capture_output=True,
    ).stdout
    asm = subprocess.run(
        [sys.executable, str(ROOT / "tools/maspsx/maspsx.py"), f"--aspsx-version={aspsx}", f"-G{g}",
         "--expand-div"],
        input=asm, check=True, capture_output=True,
    ).stdout
    s.write_bytes(asm)
    subprocess.run(
        [f"{CROSS}as", "-EL", "-march=r3000", "-mtune=r3000", "-no-pad-sections", "-O1", f"-G{g}",
         "-o", str(o), str(s)],
        check=True, capture_output=True,
    )
    return o


def obj_functions(o):
    """(name, words) for each function in .text of an object."""
    dis = subprocess.run([f"{CROSS}objdump", "-t", str(o)], capture_output=True, text=True).stdout
    funcs = []
    for line in dis.splitlines():
        p = line.split()
        if len(p) >= 6 and p[3] == ".text" and "F" in p[2]:
            funcs.append((p[-1], int(p[0], 16), int(p[4], 16)))
    data = subprocess.run(
        [f"{CROSS}objcopy", "-O", "binary", "-j", ".text", str(o), "/dev/stdout"], capture_output=True
    ).stdout
    return [(n, list(struct.unpack_from(f"<{sz // 4}I", data, a))) for n, a, sz in funcs]


def mask(w):
    op = w >> 26
    if op in (2, 3):
        return w & 0xFC000000
    if op in (0x0F, 0x09, 0x0D) or 0x20 <= op <= 0x2E or op in (0x32, 0x3A):
        return w & 0xFFFF0000
    return w


def score(a, b):
    n = max(len(a), len(b))
    same = sum(1 for x, y in zip(a, b) if mask(x) == mask(y))
    return same, n


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("src")
    ap.add_argument("--cc", nargs="*", default=["gcc-2.7.2-psx", "gcc-2.8.0-psx", "gcc-2.8.1-psx"])
    ap.add_argument("--opt", nargs="*", default=["-O2"])
    ap.add_argument("--G", nargs="*", default=["8"])
    ap.add_argument("--aspsx", nargs="*", default=["2.56", "2.79"])
    ap.add_argument("-v", action="store_true", help="show diff for the best setup")
    args = ap.parse_args()

    table = syms()
    results = []
    with tempfile.TemporaryDirectory() as td:
        for cc in args.cc:
            for opt in args.opt:
                for g in args.G:
                    for asx in args.aspsx:
                        try:
                            o = compile_obj(Path(args.src), cc, opt, g, asx, Path(td))
                        except subprocess.CalledProcessError as e:
                            print(f"{cc} {opt} -G{g} aspsx {asx}: FAILED\n{e.stderr.decode()[-500:]}")
                            continue
                        tot_same = tot_n = 0
                        detail = []
                        for name, words in obj_functions(o):
                            if name not in table:
                                continue
                            ref = orig_words(table[name], len(words))
                            # compare against the original's true length when shorter
                            same, n = score(words, ref)
                            tot_same += same
                            tot_n += n
                            detail.append((name, same, n, words, ref))
                        results.append((tot_same / max(tot_n, 1), cc, opt, g, asx, detail))
    results.sort(key=lambda r: -r[0])
    for pct, cc, opt, g, asx, detail in results:
        fs = " ".join(f"{n}={s}/{t}" for n, s, t, _, _ in detail)
        print(f"{pct*100:6.1f}%  {cc:14s} {opt} -G{g} aspsx {asx}   {fs}")
    if args.v and results:
        for name, s, t, words, ref in results[0][5]:
            if s != t:
                print(f"--- {name}")
                for i, (x, y) in enumerate(zip(words, ref)):
                    flag = "  " if mask(x) == mask(y) else "!!"
                    print(f"{flag} {i*4:4X} {x:08X} {y:08X}")


if __name__ == "__main__":
    main()
