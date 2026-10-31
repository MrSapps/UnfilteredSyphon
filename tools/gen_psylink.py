#!/usr/bin/env python3
"""Generate a psylink (Psy-Q 4.3 linker) command file for the whole game: the main
exe, every overlay and the embedded hogs, laid out as in orig/siphon.map.

    tools/gen_psylink.py [--out build/psyq] [--libs] [--obj TU=path.obj ...]

Inputs (from the modern build, run `python3 configure.py && ninja` first):
  * build/scus_942.40.ld and build/<ovl>.ld, the splat linker scripts.  They give,
    per output section, the objects in link order.  psylink has a single include
    order for all sections, so the per-section orders are merged (only objects
    that actually contribute bytes constrain the order).
  * every object they name, converted with tools/elf2psyq.py into
    <out>/obj/<same path>.obj.  Overlay objects get their sections renamed
    (.text -> menu.text, ...; the overlay id word -> $menu) because psylink
    assigns sections to groups by name and the original map has per-overlay
    section names.
  * the modern ELFs' symbol tables, for equates: every name some object
    references but no object defines (splat's undefined_syms, linker-script
    symbols) becomes `name equ $value` in the command file.

--libs     use the real Psy-Q library modules listed in config/psyq_libs.txt
           (tools/psyq_liborder.py; extracted into <out>/lib) instead of the lib_*.s asm
           objects, cut the library part out of the splat data blobs, and let psylink
           lay out all commons (see docs/psyq_build.md)
--obj TU=FILE  use FILE (e.g. a ccpsx-compiled OBJ) for TU <TU> instead of
           converting build/src/main/<TU>.c.o (or, for an overlay TU, build/src/<TU>/<TU>.c.o;
           its sections and clashing symbols are renamed like a converted object's)

Writes <out>/siphon.lnk; link with tools/psyq_link.sh.
"""
import argparse
import os
import re
import shutil
import struct
import struct
import sys

TOOLS = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(TOOLS)
sys.path.insert(0, TOOLS)
import elf2psyq  # noqa: E402
from psyq_obj import read_lib  # noqa: E402

OVERLAYS = ["tcache", "menu", "movie", "levspec", "baseext", "cave", "chopper", "subway",
            "subway2", "park", "park2", "museum", "whouse", "title", "init"]
# siphon.map: three overlay areas.  Each `over` group shares the address of the first
# group of its area; the next area starts after the largest of them.
AREAS = [["tcache", "menu", "movie"],
         ["levspec", "baseext", "cave", "chopper", "subway", "subway2", "park", "park2",
          "museum", "whouse", "title"],
         ["init"]]
SECTIONS = [".rodata", ".text", ".data", ".sdata", ".sbss", ".bss"]
SN = {".rodata": ".rdata"}


def ld_sections(path):
    """[(output section, [(object, input section)])] from a splat linker script."""
    out = []
    cur = None
    for line in open(path):
        m = re.match(r"\s*\.(\w+)\s.*:", line)
        if m and not line.strip().startswith("/DISCARD/"):
            cur = (m.group(1), [])
            out.append(cur)
            continue
        m = re.match(r"\s*(build/\S+\.o)\((\.\w+)\);", line)
        if m and cur:
            cur[1].append((m.group(1), m.group(2)))
    return out


def elf_info(path):
    """(defined globals, undefined names, {section name: size}) of an ELF object."""
    secs, syms, rels, _ = elf2psyq.read_elf(open(path, "rb").read())
    used = elf2psyq.referenced(rels, secs)
    syms = [dict(y, shndx=-1) if y["shndx"] == 0 and i not in used else y for i, y in enumerate(syms)]
    sizes = {s.name: s.size for s in secs if s.flags & elf2psyq.SHF_ALLOC}
    defs = {y["name"] for y in syms if y["shndx"] not in (0, elf2psyq.SHN_ABS, elf2psyq.SHN_COMMON)
            and y["bind"] != elf2psyq.STB_LOCAL and y["name"]}
    undef = {y["name"] for y in syms if y["shndx"] == 0 and y["name"]}
    commons = {y["name"] for y in syms if y["shndx"] == elf2psyq.SHN_COMMON}
    return defs, undef | commons, sizes


LIBS_TXT = os.path.join(ROOT, "config/psyq_libs.txt")
SDK_LIB = "/home/snake/dev/psyq_sdk/psyq_4.3/lib"
LIB_PATHS = {"libcd": "/home/snake/dev/psyq_sdk/psyq_4.4/lib/patches/libcd.lib"}
# Where the library part of each data section starts (config/psyq_libs.txt: libsn
# SNMAIN's .data/.sbss; .bss: the game's .bss ends where the 0x29E0 bytes of library
# .bss before the commons begin).  The splat blobs that run past these are cut.
LIB_DATA = {".data": 0x80107BA4, ".sbss": 0x80110228, ".bss": 0x80125278}
# the commons, after all .sbss and all .bss (orig/siphon.map)
COMMONS = {".sbss": (0x80110230, 0x801104A0), ".bss": (0x80127C58, 0x801377DC)}


def lib_modules(outdir):
    """Extract the modules listed in config/psyq_libs.txt; [(lib, module, OBJ path)]."""
    want = []
    for l in open(LIBS_TXT):
        if l.strip() and not l.startswith("#"):
            lib, mod = l.split()[:2]
            want.append((lib, mod))
    out = []
    for lib, mod in want:
        dst = os.path.join(outdir, lib, mod + ".obj")
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        open(dst, "wb").write(elf2psyq.commons_to_xrefs(lib_raw_modules_cache(lib)[mod]))
        out.append((lib, mod, os.path.relpath(dst, ROOT)))
    return out


def lib_raw_modules(b):
    """{module name: OBJ bytes} of an SN LIB archive."""
    out = {}
    p = 4
    while p + 20 <= len(b):
        name = b[p:p + 8].decode("latin-1").rstrip()
        _, hdr, size = struct.unpack_from("<III", b, p + 8)
        out[name] = b[p + hdr:p + size]
        p += size
    return out


def blob_cuts():
    """{ELF object: {section: (keep start, keep end)}} for main objects that need cutting
    (from the modern build's map):
      * a section that runs into the library data is cut where that starts;
      * in .sbss/.bss psylink starts every object's contribution 8-aligned (the
        sections hold commons), so a splat bss blob that starts 4 bytes past an
        8-byte boundary loses those 4 bytes - psylink's padding after the previous
        object recreates them.  Names defined there become equates."""
    cuts = {}
    for l in open(os.path.join(ROOT, "build/scus_942.40.map")):
        m = re.match(r"\s(\.(?:data|sbss|bss))\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+(build/\S+)", l)
        if not m:
            continue
        sec, start, size, obj = m.group(1), int(m.group(2), 16) & 0xFFFFFFFF, int(m.group(3), 16), m.group(4)
        if not size or "/asm/main/lib_" in obj or "/assets/" in obj or not 0x80010000 <= start < 0x801377E0:
            continue
        lo, hi = 0, size
        if sec in LIB_DATA and start + size > LIB_DATA[sec]:
            hi = max(0, LIB_DATA[sec] - start)
        if sec in (".sbss", ".bss") and start & 7:
            if "/asm/" not in obj:
                print("warning: %s %s starts at %08x, not 8-aligned; psylink will move it"
                      % (obj, sec, start), file=sys.stderr)
            else:
                lo = min(hi, -start & 7)
        if (lo, hi) != (0, size):
            cuts.setdefault(obj, {})[sec] = (lo, hi)
    return cuts


_RAW = {}


def lib_raw_modules_cache(lib):
    if lib not in _RAW:
        path = LIB_PATHS.get(lib, os.path.join(SDK_LIB, lib + ".lib"))
        _RAW[lib] = lib_raw_modules(open(path, "rb").read())
    return _RAW[lib]


def read_lib_bytes(b):
    from psyq_obj import parse_obj
    return parse_obj(b)


def common_names():
    """[(section, address, name)] of the commons in siphon.map, by address."""
    names = []
    for l in open(os.path.join(ROOT, "orig/siphon.map")):
        m = re.match(r"\s([0-9A-F]{8})\s\s(\w+)$", l)
        if m:
            names.append((int(m.group(1), 16), m.group(2)))
    out = []
    for sec, (lo, hi) in COMMONS.items():
        out += [(sec, a, n) for a, n in sorted(set(names)) if lo <= a < hi
                and not (n.startswith("_") and n.endswith(("_obj", "_org", "_objend", "_orgend")))]
    return out


def refs_obj(names):
    """An OBJ that only references `names`, to enter them into psylink's symbol table
    in a chosen order."""
    w = elf2psyq.Writer()
    for i, n in enumerate(names):
        w.xref(i + 1, n)
    w.end()
    return bytes(w.out)


def common_size(addr, gap):
    """Size of a common from the gap to the next one.  psylink aligns a common by its
    size (1, 2, 4, 8 and more: 8), so a gap that is padding can be told apart:
    a 3-byte gap after an odd address is a 1-byte common, and so on."""
    al = addr & -addr
    if gap >= 8 and al >= 8:
        return gap
    for n in (4, 2, 1):
        if n <= gap and n <= al:
            return n
    return gap


def all_commons(libsizes):
    """An OBJ declaring every common: each map name in the commons ranges, sized as
    the library declares it or else from the gap up to the next name."""
    names = common_names()
    w = elf2psyq.Writer()
    w.section(1, ".sbss")
    w.section(2, ".bss")
    num = 3
    for sid, (sec, (lo, hi)) in enumerate(COMMONS.items(), 1):
        inr = [(a, n) for s, a, n in names if s == sec]
        for i, (a, n) in enumerate(inr):
            nxt = next((b for b, _ in inr[i + 1:] if b > a), hi)
            w.common(num, sid, libsizes.get(n, common_size(a, nxt - a)), n)
            num += 1
    w.end()
    return bytes(w.out)


def obj_info(path):
    """(defined globals, referenced externals) of an SN OBJ."""
    from psyq_obj import read_lib
    o = read_lib(path)[0].obj
    return {x[3] for x in o.xdefs}, set(o.xrefs.values()) | {c[3] for c in o.commons}


def elf_symtab(path):
    """{name: value} of a linked ELF (globals and locals)."""
    secs, syms, _, _ = elf2psyq.read_elf(open(path, "rb").read())
    return {y["name"]: y["value"] for y in syms if y["name"] and y["shndx"] != 0}


def merge_orders(lists):
    """One order consistent with every list (stable topological merge)."""
    nodes = []
    succ = {}
    pred = {}
    for l in lists:
        for x in l:
            if x not in succ:
                nodes.append(x)
                succ[x], pred[x] = set(), set()
        for a, b in zip(l, l[1:]):
            if b not in succ[a]:
                succ[a].add(b)
                pred[b].add(a)
    order = []
    ready = [x for x in nodes if not pred[x]]
    index = {x: i for i, x in enumerate(nodes)}
    while ready:
        ready.sort(key=index.get)
        x = ready.pop(0)
        order.append(x)
        for y in succ[x]:
            pred[y].discard(x)
            if not pred[y]:
                ready.append(y)
    if len(order) != len(nodes):
        left = [x for x in nodes if x not in order]
        raise SystemExit("cyclic object order involving: %s" % " ".join(left[:8]))
    return order


def module_objects(mod, ld):
    """(object order, {object: renames}) for one module."""
    secs = ld_sections(os.path.join(ROOT, ld))
    lists = []
    info = {}
    for outsec, items in secs:
        per = {}
        for obj, insec in items:
            if obj not in info:
                info[obj] = elf_info(os.path.join(ROOT, obj))
            if info[obj][2].get(insec, 0):
                per.setdefault(insec, []).append(obj)
        lists += per.values()
    order = merge_orders(lists)
    if mod == "main":
        order = [o for o in order if not o.endswith("/header.s.o")]
    return order, info


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="build/psyq")
    ap.add_argument("--libs", action="store_true")
    ap.add_argument("--obj", action="append", default=[], metavar="TU=FILE")
    ap.add_argument("--commons-order", default="rev", choices=["none", "addr", "rev"],
                    help="--libs: enter the common names into psylink's symbol table first, in this "
                    "order (rev = reverse address order, which reproduces the original layout)")
    a = ap.parse_args()
    out = os.path.join(ROOT, a.out)
    os.makedirs(out, exist_ok=True)
    swap = dict(x.split("=", 1) for x in a.obj)
    swapped = set()

    libmods = lib_modules(os.path.join(ROOT, a.out, "lib")) if a.libs else []
    cuts = blob_cuts() if a.libs else {}

    mods = [("main", "build/scus_942.40.ld", "build/scus_942.40.elf")]
    mods += [(o, "build/%s.ld" % o, "build/bin/%s.ovl.elf" % o) for o in OVERLAYS]
    plan = []                   # (module, ELF object, OBJ path, renames)
    alldefs = {}
    refs = []                   # (module, name)
    for mod, ld, _ in mods:
        order, info = module_objects(mod, ld)
        for o in order:
            if o.endswith("/tail_pad.bin.o"):
                continue                       # cpe2exe pads the exe to 2 KB itself
            if mod == "tcache":
                continue
            if a.libs and ("/asm/main/lib_" in o or "/asm/main/data/lib_" in o):
                continue                       # replaced by the real library modules
            ren = {}
            if mod != "main":
                for s in SECTIONS:
                    ren[SN.get(s, s)] = "%s.%s" % (mod, SN.get(s, s).lstrip("."))
                if o.endswith("/header.s.o") or mod == "tcache":
                    ren[".rdata" if mod != "tcache" else ".data"] = "$" + mod
            elif "/assets/main/" in o:
                ren[".data"] = os.path.basename(o)[:-len(".bin.o")]
            dst = os.path.join(a.out, "obj", o[len("build/"):-2] + ".obj")
            tu = os.path.basename(o)[:-len(".c.o")] if o.endswith(".c.o") else None
            if mod != "main" and tu != mod:
                tu = None
            if tu in swap:
                dst = os.path.join(a.out, "obj", "swap", tu + ".obj")
                os.makedirs(os.path.join(ROOT, os.path.dirname(dst)), exist_ok=True)
                data = open(swap[tu], "rb").read()
                if mod != "main":
                    data = elf2psyq.rename_obj(data, sections=ren)
                elif a.libs:
                    data = elf2psyq.commons_to_xrefs(data)
                open(os.path.join(ROOT, dst), "wb").write(data)
                swapped.add(dst)
            trunc = cuts.get(o, {})
            plan.append((mod, o, dst, ren, trunc))
            defs, undef, _ = info[o]
            if tu in swap:
                defs, undef = obj_info(os.path.join(ROOT, dst))
            elif trunc:
                os.makedirs(os.path.dirname(os.path.join(ROOT, dst)), exist_ok=True)
                open(os.path.join(ROOT, dst), "wb").write(elf2psyq.convert(
                    open(os.path.join(ROOT, o), "rb").read(), ren, truncate=trunc,
                    commons_as_xrefs=True))
                defs, undef = obj_info(os.path.join(ROOT, dst))
            for d in defs:
                alldefs.setdefault(d, []).append((mod, o))
            refs += [(mod, n) for n in undef]

    if a.libs:
        # psylink allocates a common right after the section contribution of the
        # first object that declares it.  In the original every common (game and
        # library) sits after all .sbss/.bss in one run, so here all common
        # declarations are turned into plain references and a generated object,
        # included last, declares them all (names/sizes from siphon.map); psylink
        # then lays them out in its own (hash) order, which matches the original.
        libsizes = {}
        for lib, mod, path in libmods:
            raw = lib_raw_modules_cache(lib)[mod]
            for c in read_lib_bytes(raw).commons:
                libsizes[c[3]] = max(libsizes.get(c[3], 0), c[2])
            d, u = obj_info(os.path.join(ROOT, path))
            for n in d:
                alldefs.setdefault(n, []).append(("main", path))
            refs += [("main", n) for n in u]
            plan.append(("main", None, path, {}, {}))
        dst = os.path.join(a.out, "obj", "commons.obj")
        open(os.path.join(ROOT, dst), "wb").write(all_commons(libsizes))
        for n in obj_info(os.path.join(ROOT, dst))[1]:
            alldefs.setdefault(n, []).append(("main", dst))
        commons_plan = ("main", None, dst, {}, {})

    # Overlays share addresses, so splat's D_<addr>/func_<addr> names repeat between
    # them.  psylink has one global namespace: a name defined by more than one module
    # is renamed to <ovl>_<name> inside each overlay that defines it.
    modefs = {}
    for n, l in alldefs.items():
        for m, _ in l:
            modefs.setdefault(m, set()).add(n)
    clashes = {n for n, l in alldefs.items() if len({m for m, _ in l}) > 1}
    for n in sorted(clashes):
        if any(m == "main" for m, _ in alldefs[n]):
            print("warning: %s defined in main and an overlay" % n, file=sys.stderr)

    # equates for references no object resolves, valued from the module's own ELF
    elfs = dict((m, e) for m, _, e in mods)
    tabs = {}
    want = {}                   # (mod, name) -> value
    for mod, n in sorted(set(refs)):
        if n in modefs.get(mod, ()) or n in modefs.get("main", ()):
            continue
        if mod == "main" and n in alldefs:
            continue
        for src in ([mod, "main"] if mod != "main" else ["main"]):
            if src not in tabs:
                tabs[src] = elf_symtab(os.path.join(ROOT, elfs[src]))
            if n in tabs[src]:
                want[(mod, n)] = tabs[src][n] & 0xFFFFFFFF
                break
        else:
            print("warning: no value for %s (%s)" % (n, mod), file=sys.stderr)
    values = {}
    for (mod, n), v in want.items():
        values.setdefault(n, set()).add(v)
    symren = {}                 # mod -> {old: new}
    equ = {}
    for (mod, n), v in want.items():
        name = n
        if mod != "main" and (n in alldefs or len(values[n]) > 1):
            name = "%s_%s" % (mod, n)
            symren.setdefault(mod, {})[n] = name
        equ[name] = v
    for n in clashes:
        for m, _ in alldefs[n]:
            if m != "main":
                symren.setdefault(m, {})[n] = "%s_%s" % (m, n)

    # convert
    for mod, o, dst, ren, trunc in plan:
        if dst in swapped and mod != "main" and symren.get(mod):
            absdst = os.path.join(ROOT, dst)
            data = elf2psyq.rename_obj(open(absdst, "rb").read(), symbols=symren[mod])
            open(absdst, "wb").write(data)
            continue
        if dst in swapped or o is None or trunc:
            continue
        absdst = os.path.join(ROOT, dst)
        src = os.path.join(ROOT, o)
        os.makedirs(os.path.dirname(absdst), exist_ok=True)
        data = elf2psyq.convert(open(src, "rb").read(), ren, symrename=symren.get(mod),
                                commons_as_xrefs=a.libs and mod == "main")
        open(absdst, "wb").write(data)

    # tcache.ovl: the overlay id word, then 0x97F8 bytes of bss at +8 (siphon.map).
    # The 4 bytes in between are uninitialised psylink padding in the original
    # (efefefe4), which a link here can't reproduce, so they are given as data in
    # tcache.rdata (empty in the original map).
    tc = open(os.path.join(ROOT, "assets/tcache/tcache.bin"), "rb").read()
    dst = os.path.join(a.out, "obj", "tcache", "tcache.obj")
    os.makedirs(os.path.dirname(os.path.join(ROOT, dst)), exist_ok=True)
    open(os.path.join(ROOT, dst), "wb").write(elf2psyq.bin_obj(
        [("$tcache", tc[:4]), ("tcache.rdata", tc[4:8]), ("tcache.bss", 0x97F8)]))
    plan.append(("tcache", None, dst, {}, {}))

    if a.libs:
        plan.append(commons_plan)
        if a.commons_order != "none":
            names = [n for _, _, n in common_names()]
            if a.commons_order == "rev":
                names.reverse()
            dst = os.path.join(a.out, "obj", "commons_refs.obj")
            open(os.path.join(ROOT, dst), "wb").write(refs_obj(names))
            plan.insert(0, ("main", None, dst, {}, {}))

    lines = [";", "; generated by tools/gen_psylink.py - do not edit", ";"]
    lines.append("text\tgroup\torg($80010000)")
    lines.append("bss\tgroup\tbss")
    for area in AREAS:
        first = area[0]
        for g in area:
            attrs = []
            if g == "tcache":
                attrs.append("org($801377E0)")
            elif g != first:
                attrs.append("over(%s)" % first)
            attrs.append('file("%s.ovl")' % g)
            lines.append("%s\tgroup\t%s" % (g, ",".join(attrs)))
    lines.append("load\tgroup\torg($80150000)")
    lines.append("")
    # .ctors/.dtors are empty, but libsn SNMAIN refers to their start/end; the
    # original has them at the start of the text group (__ctors_org = $80010000)
    for s in [".ctors", ".dtors", ".rdata", ".text", ".data", ".sdata"]:
        lines.append("\tsection\t%s,text" % s)
    for s in [".sbss", ".bss"]:
        lines.append("\tsection\t%s,bss" % s)
    for g in OVERLAYS:
        if g != "init":
            lines.append("\tsection\t$%s,%s" % (g, g))
        # the original objects had all eight ccpsx sections renamed (siphon.map has
        # _<ovl>_sdata_org etc.); the empty ones only matter for the map
        for s in ["rdata", "text", "data", "sdata", "sbss", "bss", "ctors", "dtors"]:
            lines.append("\tsection\t%s.%s,%s" % (g, s, g))
    lines.append("\tsection\tINTRFACEHog,load")
    lines.append("\tsection\tSPFXHog,load")
    lines.append("")
    for n in sorted(equ):
        lines.append("%s\tequ\t$%08X" % (n, equ[n]))
    lines.append("")
    for mod, o, dst, ren, trunc in plan:
        rel = os.path.relpath(os.path.join(ROOT, dst), out)
        lines.append('\tinclude\t"%s"' % rel.replace("/", "\\"))
    lines.append("")
    lines.append("\tregs\tpc=__SN_ENTRY_POINT")
    lines.append("")
    open(os.path.join(out, "siphon.lnk"), "w").write("\r\n".join(lines) + "\r\n")
    print("wrote %s: %d objects, %d equates, %d renamed overlay symbols" % (
        os.path.join(a.out, "siphon.lnk"), len(plan), len(equ), len(clashes)))


if __name__ == "__main__":
    main()
