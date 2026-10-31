#!/usr/bin/env python3
"""Compile a C file with the original Psy-Q toolchain, expanding INCLUDE_ASM.

    psyq_cc.py [ccpsx flags...] -c FILE.c -oFILE.obj

runs what `ccpsx -c` would, with tools/psyq_asm.py spliced in between the compiler
and the assembler:

    ccpsx -S (cpppsx + cc1psx)  ->  psyq_asm.py  ->  aspsx -q -G<n>

ccpsx itself runs `aspsx -q -G<n> in -o out` (seen with -v), so for a file without
INCLUDE_ASM the result is identical to plain `ccpsx -c`.  The tools run through
tools/psyq_wine.sh (PSYQ_WINEPREFIX, PSYQ_SDK apply).  --keep-s DIR keeps the
intermediate assembly.
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile

TOOLS = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(TOOLS)
sys.path.insert(0, TOOLS)
import psyq_asm  # noqa: E402


def wine(args, cwd):
    r = subprocess.run([os.path.join(TOOLS, "psyq_wine.sh")] + args, cwd=cwd,
                       capture_output=True, text=True, errors="replace")
    return r.returncode, r.stdout + r.stderr


def main():
    argv = sys.argv[1:]
    keep = None
    if "--keep-s" in argv:
        i = argv.index("--keep-s")
        keep = argv[i + 1]
        del argv[i:i + 2]
    out, src, flags = None, None, []
    i = 0
    while i < len(argv):
        a = argv[i]
        if a == "-o":
            out = argv[i + 1]
            i += 2
            continue
        if a.startswith("-o"):
            out = a[2:]
        elif a == "-c":
            pass
        elif a.endswith(".c") and not a.startswith("-"):
            src = a
        else:
            flags.append(a)
        i += 1
    if not out or not src:
        sys.exit(__doc__)
    cwd = os.getcwd()
    out = os.path.abspath(out)
    g = [f for f in flags if re.match(r"-G\d+$", f)]
    tmp = keep or tempfile.mkdtemp(prefix="psyqcc", dir=os.path.dirname(out))
    os.makedirs(tmp, exist_ok=True)
    base = os.path.splitext(os.path.basename(src))[0]
    cc1_s = os.path.join(tmp, base + ".cc1.s")
    as_s = os.path.join(tmp, base + ".s")
    for f in (cc1_s, out):
        if os.path.exists(f):
            os.unlink(f)
    try:
        rc, log = wine(["ccpsx"] + flags + ["-S", src, "-o" + cc1_s], cwd)
        if not os.path.exists(cc1_s):
            sys.stderr.write(log)
            sys.exit(1)
        text = open(cc1_s, newline="").read().replace("\r\n", "\n")
        lines = psyq_asm.filter_cc1(text, ROOT)
        with open(as_s, "w", newline="") as f:
            f.write("\r\n".join(lines) + "\r\n")
        rc, log2 = wine(["aspsx", "-q"] + g + [as_s, "-o", out], cwd)
        if not os.path.exists(out) or rc:
            sys.stderr.write(log + log2)
            if os.path.exists(out):
                os.unlink(out)
            sys.exit(1)
    finally:
        if not keep:
            shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    main()
