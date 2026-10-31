#!/usr/bin/env python3
"""Convert a GNU-as MIPS ELF relocatable (.o) into an SN Systems LNK object (.OBJ)
that the original Psy-Q linker (psylink 2.73) accepts.

usage: elf2psyq.py IN.o OUT.obj [--rename OLD=NEW ...] [--locals] [--stats]

What is carried over:
  * every SHF_ALLOC section (.rodata becomes .rdata, the Psy-Q name); --rename
    changes names after that, e.g. `--rename .text=menu.text` for an overlay object
    (psylink groups sections by name, see docs/psyq_build.md)
  * global/weak symbols -> XDEF, undefined -> XREF, commons -> XBSS (placed in
    .sbss if size <= 8 else .bss, which is what aspsx does with -G8), named local
    symbols -> LOCAL records with --locals (only for map/debugging)
  * relocations, as psylink expressions `symbol|section base + constant`:
        R_MIPS_32      -> 0x10      R_MIPS_26      -> 0x4A
        R_MIPS_HI16    -> 0x52      R_MIPS_LO16    -> 0x54
        R_MIPS_GPREL16 -> 0x64
    The ELF objects are REL (addend in place); the addend is moved into the
    expression and the field is cleared, like ccpsx/aspsx emit it.  HI16/LO16 take
    the combined addend of their pair.  R_MIPS_PC16 has no SN equivalent; it is
    resolved here when the target is in the same section, and is an error otherwise.

Relocations against a symbol defined in the same object are expressed as
section base + offset (aspsx does the same), so only real externals are XREFs.
"""
import argparse
import struct
import sys

R_MIPS_32, R_MIPS_26, R_MIPS_HI16, R_MIPS_LO16, R_MIPS_GPREL16, R_MIPS_PC16 = 2, 4, 5, 6, 7, 10
SN_TYPE = {R_MIPS_32: 0x10, R_MIPS_26: 0x4A, R_MIPS_HI16: 0x52, R_MIPS_LO16: 0x54,
           R_MIPS_GPREL16: 0x64}
SHT_PROGBITS, SHT_SYMTAB, SHT_NOBITS, SHT_REL, SHT_MIPS_REGINFO = 1, 2, 8, 9, 0x70000006
SHF_ALLOC = 2
SHN_UNDEF, SHN_ABS, SHN_COMMON = 0, 0xFFF1, 0xFFF2
STB_LOCAL, STB_GLOBAL, STB_WEAK = 0, 1, 2
STT_SECTION, STT_FILE = 3, 4

SN_NAMES = {".rodata": ".rdata"}
CHUNK = 0x8000


class ElfSection:
    pass


def read_elf(b):
    if b[:4] != b"\x7fELF" or b[4] != 1 or b[5] != 1:
        raise ValueError("not a 32-bit little-endian ELF")
    shoff, = struct.unpack_from("<I", b, 0x20)
    shentsize, shnum, shstrndx = struct.unpack_from("<HHH", b, 0x2E)
    secs = []
    for i in range(shnum):
        s = ElfSection()
        (s.name_off, s.type, s.flags, s.addr, s.offset, s.size, s.link, s.info,
         s.align, s.entsize) = struct.unpack_from("<10I", b, shoff + i * shentsize)
        s.index = i
        s.data = b[s.offset:s.offset + s.size] if s.type != SHT_NOBITS else b""
        secs.append(s)
    strtab = secs[shstrndx].data
    for s in secs:
        s.name = strtab[s.name_off:strtab.index(b"\0", s.name_off)].decode()
    syms = []
    symsec = next((s for s in secs if s.type == SHT_SYMTAB), None)
    if symsec:
        names = secs[symsec.link].data
        for i in range(symsec.size // 16):
            nm, value, size, info, other, shndx = struct.unpack_from("<IIIBBH", symsec.data, i * 16)
            name = names[nm:names.index(b"\0", nm)].decode()
            syms.append(dict(name=name, value=value, size=size, bind=info >> 4,
                             type=info & 0xF, shndx=shndx))
    rels = {}
    for s in secs:
        if s.type == SHT_REL:
            rels[s.info] = [struct.unpack_from("<II", s.data, i * 8) for i in range(s.size // 8)]
    gp0 = 0
    for s in secs:
        if s.type == SHT_MIPS_REGINFO and s.size >= 24:
            gp0 = struct.unpack_from("<I", s.data, 20)[0]
    return secs, syms, rels, gp0


def referenced(rels, secs):
    """Indices of the symbols used by relocations of allocated sections."""
    used = set()
    for idx, rl in rels.items():
        if secs[idx].flags & SHF_ALLOC:
            used.update(info >> 8 for _, info in rl)
    return used


def pstr(s):
    e = s.encode("latin-1")
    if len(e) > 255:
        raise ValueError("name too long: " + s)
    return bytes([len(e)]) + e


class Writer:
    def __init__(self):
        self.out = bytearray(b"LNK\x02")
        self.out += b"\x2e\x07"            # processor type 7 (R3000)

    def section(self, sid, name, align=8):
        self.out += b"\x10" + struct.pack("<HHB", sid, 0, align) + pstr(name)

    def switch(self, sid):
        self.out += b"\x06" + struct.pack("<H", sid)

    def code(self, data):
        self.out += b"\x02" + struct.pack("<H", len(data)) + data

    def zeroes(self, n):
        self.out += b"\x08" + struct.pack("<I", n)

    def reloc(self, typ, off, expr):
        self.out += b"\x0a" + struct.pack("<BH", typ, off) + expr

    def xdef(self, num, sid, off, name):
        self.out += b"\x0c" + struct.pack("<HHI", num, sid, off) + pstr(name)

    def xref(self, num, name):
        self.out += b"\x0e" + struct.pack("<H", num) + pstr(name)

    def local(self, sid, off, name):
        self.out += b"\x12" + struct.pack("<HI", sid, off) + pstr(name)

    def common(self, num, sid, size, name):
        self.out += b"\x30" + struct.pack("<HHI", num, sid, size) + pstr(name)

    def end(self):
        self.out += b"\x00"


def e_const(v):
    return b"\x00" + struct.pack("<I", v & 0xFFFFFFFF)


def e_term(kind, num, addend):
    """kind 2 = xref symbol, 4 = section base; returns base + addend."""
    t = bytes([kind]) + struct.pack("<H", num)
    if addend == 0:
        return t
    if addend > 0:
        return b"\x2c" + t + e_const(addend)
    return b"\x2e" + e_const(-addend) + t      # 0x2e: second - first


def sext16(v):
    v &= 0xFFFF
    return v - 0x10000 if v & 0x8000 else v


def convert(b, renames=None, keep_locals=False, stats=None, symrename=None, truncate=None,
            commons_as_xrefs=False):
    """ELF bytes -> OBJ bytes.  `symrename` {old: new} renames global symbols
    (definitions and references).  `truncate` {ELF section: size or (start, end)}
    keeps only that part of a section (a start > 0 only for bss): relocations and
    symbols outside it are dropped, and globals defined there become external
    references.  `commons_as_xrefs` turns
    common declarations into plain references (see docs/psyq_build.md)."""
    secs, syms, rels, gp0 = read_elf(b)
    renames = renames or {}
    symrename = symrename or {}
    for s in secs:
        keep = (truncate or {}).get(s.name)
        if keep is None:
            continue
        lo, hi = (0, keep) if isinstance(keep, int) else keep
        hi = min(hi, s.size)
        lo = min(lo, hi)
        if (lo, hi) == (0, s.size):
            continue
        if lo and (s.type != SHT_NOBITS or s.index in rels):
            raise ValueError("%s: can only drop the start of a bss section" % s.name)
        s.size = hi - lo
        s.data = s.data[lo:hi] if s.type != SHT_NOBITS else b""
        if s.index in rels:
            rels[s.index] = [r for r in rels[s.index] if r[0] < hi]
        for y in syms:
            if y["shndx"] == s.index and y["type"] != STT_SECTION:
                if lo <= y["value"] < hi or y["value"] == hi == lo:
                    y["value"] -= lo
                else:
                    y["shndx"] = SHN_UNDEF if y["bind"] != STB_LOCAL else SHN_ABS
                    y["value"] = 0
    for y in syms:
        if y["bind"] != STB_LOCAL and y["name"] in symrename:
            y["name"] = symrename[y["name"]]
    w = Writer()
    alloc = [s for s in secs if s.flags & SHF_ALLOC and s.type in (SHT_PROGBITS, SHT_NOBITS)
             and s.name not in (".reginfo", ".MIPS.abiflags")]
    sid_of = {}
    snname = {}
    num = 1
    for s in alloc:
        nm = SN_NAMES.get(s.name, s.name)
        nm = renames.get(nm, nm)
        sid_of[s.index] = num
        snname[s.index] = nm
        num += 1

    # commons need an .sbss/.bss to live in
    commons = [y for y in syms if y["shndx"] == SHN_COMMON]
    extra = {}
    for y in commons:
        want = ".sbss" if 0 < y["size"] <= 8 else ".bss"
        want = renames.get(want, want)
        if want not in snname.values() and want not in extra:
            extra[want] = num
            num += 1
    for s in alloc:
        if s.align > 8:
            pass                               # psylink packs to 4 anyway (see docs)
        w.section(sid_of[s.index], snname[s.index])
    for nm, sid in extra.items():
        w.section(sid, nm)
    sid_by_name = {snname[i]: sid_of[i] for i in sid_of}
    sid_by_name.update(extra)

    # symbol numbering: externs get their own numbers after the sections
    used = referenced(rels, secs)
    xref_num = {}
    xdefs = []
    for i, y in enumerate(syms):
        if i == 0 or y["type"] in (STT_SECTION, STT_FILE):
            continue
        if y["shndx"] == SHN_UNDEF:
            if y["name"] and i in used:
                xref_num[i] = num
                num += 1
        elif y["shndx"] == SHN_COMMON:
            pass
        elif y["shndx"] == SHN_ABS:
            if y["bind"] != STB_LOCAL:
                raise ValueError("absolute global symbol %s not supported" % y["name"])
        elif y["shndx"] in sid_of:
            if y["bind"] in (STB_GLOBAL, STB_WEAK):
                xdefs.append((num, sid_of[y["shndx"]], y["value"], y["name"]))
                num += 1

    def target(symidx):
        """(kind, number, base offset) for a relocation symbol."""
        y = syms[symidx]
        if y["shndx"] == SHN_UNDEF:
            return 2, xref_num[symidx], 0
        if y["shndx"] == SHN_COMMON:
            return 2, common_num[symidx], 0
        if y["shndx"] in sid_of:
            return 4, sid_of[y["shndx"]], y["value"]
        raise ValueError("relocation against unsupported symbol %r (shndx %#x)" % (y["name"], y["shndx"]))

    common_num = {}
    for i, y in enumerate(syms):
        if y["shndx"] == SHN_COMMON:
            common_num[i] = num
            num += 1

    cut = {sid_of[s.index]: s.size for s in alloc if s.name in (truncate or {})}
    for s in alloc:
        sid = sid_of[s.index]
        w.switch(sid)
        if s.type == SHT_NOBITS:
            if s.size:
                w.zeroes(s.size)
            continue
        data = bytearray(s.data)
        rl = rels.get(s.index, [])
        out = []                               # (offset, sn type, expr)
        last_hi = {}                           # symidx -> hi field of the latest HI16
        for k, (off, info) in enumerate(rl):
            typ, si = info & 0xFF, info >> 8
            if stats is not None:
                stats[typ] = stats.get(typ, 0) + 1
            ins, = struct.unpack_from("<I", data, off)
            if typ == R_MIPS_32:
                kind, n, base = target(si)
                add = ins
                struct.pack_into("<I", data, off, 0)
            elif typ == R_MIPS_26:
                kind, n, base = target(si)
                add = (ins & 0x3FFFFFF) << 2
                struct.pack_into("<I", data, off, ins & 0xFC000000)
            elif typ == R_MIPS_HI16:
                kind, n, base = target(si)
                lo = None
                for off2, info2 in rl[k + 1:]:
                    if info2 & 0xFF == R_MIPS_LO16 and info2 >> 8 == si:
                        lo, = struct.unpack_from("<I", data, off2)
                        break
                if lo is None:
                    raise ValueError("%s: HI16 at %#x without LO16" % (s.name, off))
                last_hi[si] = ins & 0xFFFF
                add = ((ins & 0xFFFF) << 16) + sext16(lo)
                struct.pack_into("<I", data, off, ins & 0xFFFF0000)
            elif typ == R_MIPS_LO16:
                kind, n, base = target(si)
                add = (last_hi.get(si, 0) << 16) + sext16(ins)
                if si not in last_hi:
                    add = sext16(ins)
                struct.pack_into("<I", data, off, ins & 0xFFFF0000)
            elif typ == R_MIPS_GPREL16:
                kind, n, base = target(si)
                add = sext16(ins)
                if syms[si]["bind"] == STB_LOCAL:
                    add += gp0
                struct.pack_into("<I", data, off, ins & 0xFFFF0000)
            elif typ == R_MIPS_PC16:
                y = syms[si]
                if y["shndx"] != s.index:
                    raise ValueError("%s: PC16 at %#x to %r outside the section" % (s.name, off, y["name"]))
                # REL addend is in place (GNU as stores -1: PC-relative to the delay slot)
                disp = (y["value"] + (sext16(ins) << 2) - off) >> 2
                struct.pack_into("<I", data, off, (ins & 0xFFFF0000) | (disp & 0xFFFF))
                continue
            else:
                raise ValueError("%s: unsupported relocation type %d at %#x" % (s.name, typ, off))
            if kind == 4 and n in cut and base + add >= cut[n]:
                raise ValueError("%s: relocation at %#x points into the truncated part" % (s.name, off))
            out.append((off, SN_TYPE[typ], e_term(kind, n, base + add)))
        out.sort(key=lambda r: r[0])
        p = 0
        ri = 0
        while p < len(data):
            q = min(p + CHUNK, len(data))
            w.code(bytes(data[p:q]))
            while ri < len(out) and out[ri][0] < q:
                off, t, e = out[ri]
                w.reloc(t, off - p, e)
                ri += 1
            p = q
    for n, sid, off, name in xdefs:
        w.xdef(n, sid, off, name)
    for i, n in xref_num.items():
        w.xref(n, syms[i]["name"])
    for i, n in common_num.items():
        y = syms[i]
        want = ".sbss" if 0 < y["size"] <= 8 else ".bss"
        if commons_as_xrefs:
            w.xref(n, y["name"])
        else:
            w.common(n, sid_by_name[renames.get(want, want)], y["size"], y["name"])
    if keep_locals:
        for y in syms:
            if (y["bind"] == STB_LOCAL and y["shndx"] in sid_of and y["name"]
                    and y["type"] not in (STT_SECTION, STT_FILE) and not y["name"].startswith(".L")):
                w.local(sid_of[y["shndx"]], y["value"], y["name"])
    w.end()
    return bytes(w.out)


_FIXED = {2: None, 6: 2, 8: 4, 46: 1, 50: 2, 52: 3, 54: 4, 56: 6, 58: 8, 60: 2,
          76: 10, 78: 10, 80: 10}


def _skip_expr(b, p):
    op = b[p]
    if op == 0:
        return p + 5
    if op in (0x02, 0x04, 0x06, 0x0C, 0x16, 0x18, 0x1A):
        return p + 3
    if 0x20 <= op <= 0x3E:
        return _skip_expr(b, _skip_expr(b, p + 1))
    raise ValueError("unknown expression op %#x" % op)


def commons_to_xrefs(b):
    """Rewrite an SN OBJ so that its common (XBSS) declarations become XREFs."""
    out = bytearray(b[:4])
    p = 4
    while p < len(b):
        op = b[p]
        q = p + 1
        if op == 0:
            out += b[p:]
            break
        if op == 2:
            q += 2 + struct.unpack_from("<H", b, q)[0]
        elif op == 10:
            q = _skip_expr(b, q + 3)
        elif op == 12:
            q += 8 + 1 + b[q + 8]
        elif op == 14:
            q += 2 + 1 + b[q + 2]
        elif op == 16:
            q += 5 + 1 + b[q + 5]
        elif op == 18:
            q += 6 + 1 + b[q + 6]
        elif op == 20:
            q += 3 + 1 + b[q + 3]
        elif op == 28:
            q += 2 + 1 + b[q + 2]
        elif op == 48:
            sym = b[q:q + 2]
            q += 8 + 1 + b[q + 8]
            out += b"\x0e" + sym + b[p + 9:q]
            p = q
            continue
        elif op == 74:
            q += 28
            q += 1 + b[q]
        elif op == 82:
            q += 14
            q += 1 + b[q]
        elif op == 84:
            q += 14
            q += 2 + 4 * struct.unpack_from("<H", b, q)[0]
            q += 1 + b[q]
            q += 1 + b[q]
        elif op in _FIXED:
            q += _FIXED[op]
        else:
            raise ValueError("unknown LNK record %d at %#x" % (op, p))
        out += b[p:q]
        p = q
    return bytes(out)


def bin_obj(sections, commons=()):
    """An OBJ holding raw bytes: [(section name, bytes or bss size)], plus
    commons [(name, section name, size)]."""
    w = Writer()
    for i, (name, _) in enumerate(sections):
        w.section(i + 1, name)
    for i, (name, data) in enumerate(sections):
        w.switch(i + 1)
        if isinstance(data, int):
            w.zeroes(data)
            continue
        for p in range(0, len(data), CHUNK):
            w.code(bytes(data[p:p + CHUNK]))
    names = [n for n, _ in sections]
    for i, (name, sec, size) in enumerate(commons):
        w.common(len(sections) + 1 + i, names.index(sec) + 1, size, name)
    w.end()
    return bytes(w.out)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("input")
    ap.add_argument("output")
    ap.add_argument("--rename", action="append", default=[], metavar="OLD=NEW",
                    help="rename a section (after .rodata -> .rdata)")
    ap.add_argument("--locals", action="store_true", help="emit named local symbols")
    ap.add_argument("--stats", action="store_true", help="print relocation type counts")
    ap.add_argument("--symrename", action="append", default=[], metavar="OLD=NEW",
                    help="rename a global symbol (definition and references)")
    ap.add_argument("--truncate", action="append", default=[], metavar="SECTION=SIZE",
                    help="keep only the first SIZE bytes of an ELF section")
    a = ap.parse_args()
    ren = dict(r.split("=", 1) for r in a.rename)
    st = {} if a.stats else None
    try:
        out = convert(open(a.input, "rb").read(), ren, a.locals, st, dict(r.split("=", 1) for r in a.symrename),
                      {k: int(v, 0) for k, v in (t.split("=", 1) for t in a.truncate)})
    except ValueError as e:
        sys.exit("%s: %s" % (a.input, e))
    open(a.output, "wb").write(out)
    if st:
        print(a.input, st)


if __name__ == "__main__":
    main()
