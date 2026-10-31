#!/usr/bin/env python3
"""Minimal parser for SN Systems / Psy-Q LNK object files (.OBJ) and LIB archives.

Library use:
    from psyq_obj import read_lib, parse_obj
    for mod in read_lib(path):      # Module(name, exports, obj)
        o = mod.obj                 # Obj: sections {id: Section}, relocs, xdefs, xrefs
        text = o.section_named(".text")

CLI:
    psyq_obj.py FILE.LIB|FILE.OBJ [-v]   list modules, section sizes, exports
"""
import struct
import sys


class Section:
    def __init__(self, sid, group, align, name):
        self.id, self.group, self.align, self.name = sid, group, align, name
        self.data = bytearray()
        self.bss = 0                   # uninitialised size (ZEROES in bss-type sections)
        self.relocs = []               # (section offset, type, expr)
        self.chunk = 0                 # start of the last code record


class Obj:
    def __init__(self):
        self.sections = {}
        self.xdefs = []                # (symnum, section, offset, name)
        self.xrefs = {}                # symnum -> name
        self.locals = []               # (section, offset, name)
        self.commons = []              # (symnum, section, size, name)
        self.files = {}
        self.cpu = None

    def section_named(self, name):
        for s in self.sections.values():
            if s.name == name:
                return s
        return None


class Module:
    def __init__(self, name, date, exports, obj):
        self.name, self.date, self.exports, self.obj = name, date, exports, obj


def _pstr(b, p):
    n = b[p]
    return b[p + 1:p + 1 + n].decode("latin-1"), p + 1 + n


def _expr(b, p):
    """Parse a relocation expression; return (expr tuple, new pos)."""
    op = b[p]
    p += 1
    if op == 0x00:
        return ("const", struct.unpack_from("<I", b, p)[0]), p + 4
    if op in (0x02, 0x04, 0x06, 0x0C, 0x16, 0x18, 0x1A):
        # 2 = xref symbol, 4 = section base, 6 = section bank?, 0x0c = section start,
        # 0x16 = section end ... (only 2/4 are common in libs)
        return (op, struct.unpack_from("<H", b, p)[0]), p + 2
    if 0x20 <= op <= 0x3E:
        a, p = _expr(b, p)
        c, p = _expr(b, p)
        return (op, a, c), p
    raise ValueError("unknown expr op 0x%02x at 0x%x" % (op, p - 1))


def parse_obj(b, start=0, end=None):
    if end is None:
        end = len(b)
    if b[start:start + 3] != b"LNK":
        raise ValueError("not an LNK object")
    o = Obj()
    p = start + 4
    cur = None
    while p < end:
        op = b[p]
        p += 1
        if op == 0:
            break
        elif op == 2:                        # code bytes
            n = struct.unpack_from("<H", b, p)[0]
            p += 2
            cur.chunk = len(cur.data)    # patch offsets are relative to this
            cur.data += b[p:p + n]
            p += n
        elif op == 6:                        # switch section
            cur = o.sections[struct.unpack_from("<H", b, p)[0]]
            p += 2
        elif op == 8:                        # zeroes
            n = struct.unpack_from("<I", b, p)[0]
            p += 4
            if cur.name.endswith("bss"):
                cur.bss += n
            else:
                cur.data += bytes(n)
        elif op == 10:                       # relocation
            t = b[p]
            off = struct.unpack_from("<H", b, p + 1)[0]
            e, p = _expr(b, p + 3)
            cur.relocs.append((cur.chunk + off, t, e))
        elif op == 12:                       # xdef
            sym, sec, off = struct.unpack_from("<HHI", b, p)
            name, p = _pstr(b, p + 8)
            o.xdefs.append((sym, sec, off, name))
        elif op == 14:                       # xref
            sym = struct.unpack_from("<H", b, p)[0]
            name, p = _pstr(b, p + 2)
            o.xrefs[sym] = name
        elif op == 16:                       # section symbol
            sid, grp, al = struct.unpack_from("<HHB", b, p)
            name, p = _pstr(b, p + 5)
            o.sections[sid] = Section(sid, grp, al, name)
        elif op == 18:                       # local symbol
            sec, off = struct.unpack_from("<HI", b, p)
            name, p = _pstr(b, p + 6)
            o.locals.append((sec, off, name))
        elif op == 20:                       # group symbol
            p += 3
            _, p = _pstr(b, p)
        elif op == 28:                       # file name
            fn = struct.unpack_from("<H", b, p)[0]
            name, p = _pstr(b, p + 2)
            o.files[fn] = name
        elif op == 46:                       # cpu type
            o.cpu = b[p]
            p += 1
        elif op == 48:                       # common / xbss
            sym, sec, size = struct.unpack_from("<HHI", b, p)
            name, p = _pstr(b, p + 8)
            o.commons.append((sym, sec, size, name))
        elif op == 50:
            p += 2
        elif op == 52:
            p += 3
        elif op == 54:
            p += 4
        elif op == 56:
            p += 6
        elif op == 58:
            p += 8
        elif op == 60:
            p += 2
        elif op == 74:                       # function start
            p += 2 + 4 + 2 + 4 + 2 + 4 + 2 + 4 + 4
            _, p = _pstr(b, p)
        elif op in (76, 78, 80):
            p += 10
        elif op == 82:                       # def
            p += 2 + 4 + 2 + 2 + 4
            _, p = _pstr(b, p)
        elif op == 84:                       # def2
            p += 2 + 4 + 2 + 2 + 4
            nd = struct.unpack_from("<H", b, p)[0]
            p += 2 + 4 * nd
            _, p = _pstr(b, p)
            _, p = _pstr(b, p)
        else:
            raise ValueError("unknown LNK opcode %d at 0x%x" % (op, p - 1))
    return o


def read_lib(path):
    b = open(path, "rb").read()
    if b[:4] == b"LNK\x02":
        import os
        return [Module(os.path.splitext(os.path.basename(path))[0].upper(), 0, [], parse_obj(b))]
    if b[:3] != b"LIB":
        raise ValueError("%s: not a LIB" % path)
    mods = []
    p = 4
    while p + 20 <= len(b):
        name = b[p:p + 8].decode("latin-1").rstrip()
        date, hdr, size = struct.unpack_from("<III", b, p + 8)
        q = p + 20
        exports = []
        while b[q]:
            s, q = _pstr(b, q)
            exports.append(s)
        mods.append(Module(name, date, exports, parse_obj(b, p + hdr, p + size)))
        p += size
    return mods


def main():
    verbose = "-v" in sys.argv
    for path in [a for a in sys.argv[1:] if a != "-v"]:
        for m in read_lib(path):
            secs = " ".join("%s:%x" % (s.name, len(s.data) or s.bss)
                            for s in m.obj.sections.values() if s.data or s.bss)
            print("%-8s %s" % (m.name, secs))
            if verbose:
                for x in m.obj.xdefs:
                    print("    %-6s %06x %s" % (m.obj.sections[x[1]].name, x[2], x[3]))


if __name__ == "__main__":
    main()
