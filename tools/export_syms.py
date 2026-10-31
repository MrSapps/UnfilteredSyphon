#!/usr/bin/env python3
"""Export symbols defined in the main ELF as a linker script for overlays.

Only symbols inside main's own memory (exe image + bss) are exported, so main's
placeholder names for overlay-region addresses don't clash with overlay labels.
"""
import struct
import sys

LO, HI = 0x80010000, 0x801377E0  # code/data/bss of SCUS_942.40
HOG_LO, HOG_HI = 0x80150000, 0x801B7000  # embedded hogs


def symbols(path):
    elf = open(path, "rb").read()
    shoff, = struct.unpack_from("<I", elf, 0x20)
    shentsize, shnum, shstrndx = struct.unpack_from("<HHH", elf, 0x2E)
    shdrs = [struct.unpack_from("<IIIIIIIIII", elf, shoff + i * shentsize) for i in range(shnum)]
    for sh in shdrs:
        if sh[1] != 2:  # SHT_SYMTAB
            continue
        strtab = shdrs[sh[6]]
        for off in range(sh[4], sh[4] + sh[5], 16):
            name, value, _, info, _, shndx = struct.unpack_from("<IIIBBH", elf, off)
            bind = info >> 4
            if shndx == 0 or bind == 0 or not name:  # undefined or local
                continue
            s = elf[strtab[4] + name : elf.index(b"\0", strtab[4] + name)].decode()
            yield s, value


def main():
    elf, out = sys.argv[1:3]
    seen = {}
    for name, value in symbols(elf):
        if (LO <= value < HI or HOG_LO <= value < HOG_HI) and not name.endswith(".NON_MATCHING"):
            seen[name] = value
    with open(out, "w") as f:
        for name, value in sorted(seen.items(), key=lambda x: (x[1], x[0])):
            f.write(f"{name} = 0x{value:08X};\n")


if __name__ == "__main__":
    main()
