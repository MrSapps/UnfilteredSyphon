#!/usr/bin/env python3
"""Evidence for translation-unit boundaries in SCUS_942.40 .text.

For each function (from splat's glabels) lists the .rdata it references and which
`s/<file>.c` __FILE__ string it uses. Functions are printed in address order with
a marker wherever the referenced .rdata jumps backwards (a strong file-boundary tell:
each object's .rdata is contiguous and laid out in the same order as its .text).
"""
import re
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from map2syms import code_refs, Section  # noqa: E402

EXE = Path("orig/scus_942.40")
BASE = 0x80010000
RDATA = (0x80010000, 0x80014408)


def functions(elf="build/scus_942.40.elf"):
    """(name, addr) for every function symbol in the linked main ELF, address order."""
    import struct as st

    data = Path(elf).read_bytes()
    shoff, = st.unpack_from("<I", data, 0x20)
    shentsize, shnum = st.unpack_from("<HH", data, 0x2E)
    shdrs = [st.unpack_from("<IIIIIIIIII", data, shoff + i * shentsize) for i in range(shnum)]
    funcs = {}
    SHF_EXECINSTR = 4
    for sh in shdrs:
        if sh[1] != 2:
            continue
        strtab = shdrs[sh[6]][4]
        for off in range(sh[4], sh[4] + sh[5], 16):
            nm, value, _, info, _, shndx = st.unpack_from("<IIIBBH", data, off)
            styp = info & 0xF
            in_text = 0 < shndx < len(shdrs) and shdrs[shndx][2] & SHF_EXECINSTR
            # cc1 2.7.2 doesn't mark C functions with .type, so accept untyped text symbols too
            if nm and (styp == 2 or (styp == 0 and in_text)):
                name = data[strtab + nm : data.index(b"\0", strtab + nm)].decode()
                if not name.startswith(("__maspsx", ".", "$", "jtbl_")) and "NON_MATCHING" not in name:
                    funcs.setdefault(value, name)
    return [[n, a] for a, n in sorted(funcs.items())]


def main():
    data = EXE.read_bytes()[0x800:]
    img = (BASE, data)
    rd = data[: RDATA[1] - BASE]
    files = {
        BASE + m.start(): m.group(1).decode()
        for m in re.finditer(rb"s/([a-z0-9_]+)\.c\0", rd)
    }
    labels = {
        int(m.group(1), 16)
        for m in re.finditer(r"label \w*?_?([0-9A-F]{8})\b", Path("asm/main/data/rdata.rodata.s").read_text())
    }
    labels |= set(files)
    funcs = functions()
    ends = [f[1] for f in funcs[1:]] + [0x800DD000]
    rows = []
    for (name, start), end in zip(funcs, ends):
        if start >= 0x800DD000:  # Psy-Q libraries
            break
        refs = sorted(
            r for r in code_refs(img, Section(start, end, "text", ".text")) if r in labels
        )
        tu = sorted({files[r] for r in refs if r in files})
        rows.append((start, name, refs, tu))

    last = 0
    for start, name, refs, tu in rows:
        lo = refs[0] if refs else None
        mark = ""
        if lo is not None:
            if lo < last:
                mark = "  <<< rdata goes back"
            last = max(last, refs[-1])
        r = f"{lo:08X}..{refs[-1]:08X}" if refs else "-"
        print(f"{start:08X} {name:45s} {r:20s} {','.join(tu)}{mark}")


if __name__ == "__main__":
    main()
