#!/usr/bin/env python3
"""Flatten a linked ELF into a raw image by VMA (like cpe2exe / overlay output).

PS-X EXE mode: the section at VMA 0 is the 0x800-byte header and the rest is laid
out from --base to --end, zero filled. Raw mode (overlays): no header.
"""
import argparse
import struct


def load_sections(path):
    elf = open(path, "rb").read()
    assert elf[:4] == b"\x7fELF" and elf[4] == 1 and elf[5] == 1, "need ELF32 LE"
    shoff, = struct.unpack_from("<I", elf, 0x20)
    shentsize, shnum = struct.unpack_from("<HH", elf, 0x2E)
    out = []
    for i in range(shnum):
        _, typ, flags, addr, off, size = struct.unpack_from("<IIIIII", elf, shoff + i * shentsize)
        SHF_ALLOC, SHT_NOBITS = 2, 8
        if flags & SHF_ALLOC and typ != SHT_NOBITS and size:
            out.append((addr, elf[off : off + size]))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("elf")
    ap.add_argument("out")
    ap.add_argument("--base", type=lambda x: int(x, 0), required=True)
    ap.add_argument("--end", type=lambda x: int(x, 0), required=True)
    ap.add_argument("--exe", action="store_true", help="emit PS-X EXE header from VMA 0 section")
    args = ap.parse_args()

    image = bytearray(args.end - args.base)
    header = None
    for addr, data in load_sections(args.elf):
        if args.exe and addr == 0:
            header = data
            continue
        off = addr - args.base
        if off < 0 or off + len(data) > len(image):
            raise SystemExit(f"section at {addr:08X}+{len(data):X} outside image")
        image[off : off + len(data)] = data

    with open(args.out, "wb") as f:
        if args.exe:
            assert header is not None and len(header) == 0x800, "missing 0x800 header"
            f.write(header)
        f.write(image)


if __name__ == "__main__":
    main()
