#!/usr/bin/env python3
"""Read a psylink .CPE file: list its load records, or flatten an address range.

    cpe.py FILE.cpe                       list records
    cpe.py FILE.cpe START END OUT.bin     write [START, END) (hex vram) with zero fill
"""
import struct
import sys


def read_cpe(path):
    """{'loads': [(addr, bytes)], 'regs': {reg: value}, 'unit': n}"""
    b = open(path, "rb").read()
    if b[:4] != b"CPE\x01":
        raise ValueError("%s: not a CPE file" % path)
    p = 4
    loads, regs, unit = [], {}, None
    while p < len(b):
        op = b[p]
        p += 1
        if op == 0:
            break
        if op == 1:
            addr, n = struct.unpack_from("<II", b, p)
            loads.append((addr, b[p + 8:p + 8 + n]))
            p += 8 + n
        elif op == 2:
            regs["run"] = struct.unpack_from("<I", b, p)[0]
            p += 4
        elif op == 3:
            r, v = struct.unpack_from("<HI", b, p)
            regs[r] = v
            p += 6
        elif op == 4:
            r, v = struct.unpack_from("<HH", b, p)
            regs[r] = v
            p += 4
        elif op == 5:
            r, v = struct.unpack_from("<HB", b, p)
            regs[r] = v
            p += 3
        elif op == 8:
            unit = b[p]
            p += 1
        else:
            raise ValueError("%s: unknown CPE record %d at %#x" % (path, op, p - 1))
    return dict(loads=loads, regs=regs, unit=unit)


def flatten(cpe, start, end):
    out = bytearray(end - start)
    for addr, data in cpe["loads"]:
        lo, hi = max(addr, start), min(addr + len(data), end)
        if lo < hi:
            out[lo - start:hi - start] = data[lo - addr:hi - addr]
    return bytes(out)


def main():
    c = read_cpe(sys.argv[1])
    if len(sys.argv) == 2:
        for addr, data in c["loads"]:
            print("load %08x..%08x (%#x)" % (addr, addr + len(data), len(data)))
        print("regs", {k: hex(v) for k, v in c["regs"].items()}, "unit", c["unit"])
        return
    s, e = int(sys.argv[2], 16), int(sys.argv[3], 16)
    open(sys.argv[4], "wb").write(flatten(c, s, e))


if __name__ == "__main__":
    main()
