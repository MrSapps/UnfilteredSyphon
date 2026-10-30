#!/usr/bin/env python3
"""Set up a decomp-permuter directory for a NON_MATCHING function.

    tools/permute_setup.py src/main/pickup.c UseObject      # -> permute/UseObject/
    tools/decomp-permuter/permuter.py permute/UseObject -j8

The C under `#ifdef NON_MATCHING` is the starting point. The TU's SDATA_LOCAL
symbols go into a prelude that compile.sh prepends, so gp-relative accesses
compile the same way as in the real build.
"""
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PERMUTER = ROOT / "tools/decomp-permuter"
CPP = ["cpp", "-P", "-undef", "-D__GNUC__=2", "-Dmips", "-D__psx__", "-DPERMUTER", "-DNON_MATCHING",
       "-I", str(ROOT / "include"), "-I", str(ROOT / "src")]


def main():
    src, func = Path(sys.argv[1]), sys.argv[2]
    text = (ROOT / src).read_text()
    mod = src.parts[1]
    gsize = "8" if mod == "main" else "0"
    asm = ROOT / f"asm/{mod}/nonmatchings/{src.stem}/{func}.s"
    if not asm.exists():
        sys.exit(f"{asm} not found")
    out = ROOT / "permute" / func
    out.mkdir(parents=True, exist_ok=True)

    pre = subprocess.run(CPP + ["-"], input=text, capture_output=True, text=True, check=True).stdout
    (out / "base.c").write_text(pre)
    subprocess.run([sys.executable, str(PERMUTER / "strip_other_fns.py"), str(out / "base.c"), func], check=True)

    sdata = re.findall(r"^SDATA_LOCAL\((\w+)\)", text, re.M)
    prelude = "".join(f'__asm__(".if 0\\n.sdata\\n{s}:\\n.word 0\\n.text\\n.endif\\n");\n' for s in sdata)
    (out / "prelude.c").write_text(prelude)

    cc1 = ROOT / "tools/compilers/gcc-2.7.2-psx/cc1"
    maspsx = ROOT / "tools/maspsx/maspsx.py"
    asbin = ROOT / "tools/bin/mipsel-linux-gnu-as"
    py = ROOT / ".venv/bin/python3"
    (out / "compile.sh").write_text(
        "#!/bin/bash\n"
        "# ./compile.sh input.c -o output.o\n"
        "set -o pipefail\n"
        f'DIR="$(dirname "$0")"\n'
        f'cat "$DIR/prelude.c" "$1" | {cc1} -quiet -O2 -G{gsize} -mgas -mgpOPT -mgpopt -msoft-float '
        "-fno-builtin -funsigned-char -o - "
        f"| {py} {maspsx} --aspsx-version=2.79 -G{gsize} --expand-div "
        "| sed 's/^\\s*\\.rdata\\s*$/\\t.section .rodata/' "
        f'| {asbin} -EL -march=r3000 -mtune=r3000 -no-pad-sections -O1 -G0 -I{ROOT}/include -o "$3" -\n'
    )
    (out / "compile.sh").chmod(0o755)

    (out / "target.s").write_text(
        f'.include "macro.inc"\n.set noat\n.set noreorder\n.set INC_PART, 2\n.section .text\n.include "{asm}"\n'
    )
    subprocess.run([str(asbin), "-EL", "-march=r3000", "-mtune=r3000", "-no-pad-sections", "-O1", "-G0",
                    f"-I{ROOT}/include", "-I", str(ROOT), str(out / "target.s"), "-o", str(out / "target.o")],
                   check=True)
    (out / "settings.toml").write_text('func_name = "%s"\ncompiler_type = "gcc"\n' % func)
    print(f"ready: {out.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
