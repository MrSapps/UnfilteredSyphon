#!/usr/bin/env python3
"""Compare the .text of a Psy-Q LNK object (ccpsx/aspsx output) against a GNU ELF
object (cc1 | maspsx | as output) or a region of the original binary, masking
relocated fields.  Used to validate the original toolchain against the modern one.

usage: psyq_cmpobj.py FILE.OBJ build/src/main/foo.c.o
       psyq_cmpobj.py FILE.OBJ orig/scus_942.40 0xVRAM [0x8000F800 = vram-fileofs]
"""
import os
import struct
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from psyq_obj import read_lib  # noqa: E402
from psyq_libmatch import masks_for  # noqa: E402

TOOLS = os.path.dirname(os.path.abspath(__file__))


def elf_text(path):
    objcopy = os.path.join(TOOLS, "bin", "mipsel-linux-gnu-objcopy")
    out = path + ".text.bin.tmp"
    subprocess.check_call([objcopy, "-O", "binary", "--only-section=.text", path, out])
    data = open(out, "rb").read()
    os.unlink(out)
    readelf = os.path.join(TOOLS, "bin", "mipsel-linux-gnu-readelf")
    rel = subprocess.run([readelf, "-rW", path], capture_output=True, text=True).stdout
    offs = set()
    sect = None
    for line in rel.splitlines():
        if line.startswith("Relocation section"):
            sect = line.split("'")[1]
            continue
        parts = line.split()
        if sect == ".rel.text" and parts and len(parts[0]) == 8:
            try:
                offs.add(int(parts[0], 16) // 4)
            except ValueError:
                pass
    return data, offs


def main():
    a = sys.argv[1:]
    obj = read_lib(a[0])[0].obj
    t = obj.section_named(".text")
    w = struct.unpack("<%dI" % (len(t.data) // 4), t.data)
    m = masks_for(t)
    if len(a) == 2:
        data, eoffs = elf_text(a[1])
    else:
        vram = int(a[2], 0)
        delta = int(a[3], 0) if len(a) > 3 else 0x8000F800
        data = open(a[1], "rb").read()[vram - delta:vram - delta + len(t.data)]
        eoffs = set()
    e = struct.unpack("<%dI" % (len(data) // 4), data[:len(data) // 4 * 4])
    print("psyq .text 0x%x, other 0x%x" % (len(t.data), len(data)))
    bad = 0
    for i in range(min(len(w), len(e))):
        mm = m[i] if i not in eoffs else (m[i] & 0xFFFF0000 if m[i] else 0)
        if (w[i] & mm) != (e[i] & mm):
            bad += 1
            if bad <= 20:
                print("  diff @0x%04x: psyq %08x other %08x" % (i * 4, w[i], e[i]))
    print("%d differing words" % bad)
    sys.exit(1 if bad or len(w) != len(e) else 0)


if __name__ == "__main__":
    main()
