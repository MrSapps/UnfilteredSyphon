#!/usr/bin/env python3
"""Generate build.ninja for the Syphon Filter (Nov 11 1998 prototype) decomp.

    python3 configure.py          # writes build.ninja (runs splat first unless --no-split)
    ninja                         # builds and checks SHA1 against orig/SHA1SUMS

    python3 configure.py --psyq   # writes build_psyq.ninja instead (original toolchain)
    ninja -f build_psyq.ninja     # ccpsx/aspsx + psylink under wine -> build/psyq/, SHA1 check

See docs/psyq_build.md for the original-toolchain build.
"""
import argparse
import re
import subprocess
import sys
from pathlib import Path

from ninja import ninja_syntax

ROOT = Path(__file__).parent.resolve()
TOOLS = "tools"
CROSS = f"{TOOLS}/bin/mipsel-linux-gnu-"

# Each module: splat yaml, output path, image base/end, and whether it has a PS-X EXE header.
MODULES = {
    "main": dict(
        yaml="config/scus_942.40.yaml",
        target="scus_942.40",
        base=0x80010000,
        end=0x801B7000,
        exe=True,
    ),
}

# Overlays are raw images loaded at their link address; they resolve main-exe
# symbols by linking against the main ELF with --just-symbols.
OVERLAYS = [
    "tcache", "menu", "movie", "levspec", "baseext", "cave", "chopper", "subway",
    "subway2", "park", "park2", "museum", "whouse", "title", "init",
]


def _ovl_bounds(name):
    text = (ROOT / f"config/ovl/{name}.yaml").read_text()
    vram = re.search(r"vram: (0x[0-9A-F]+)", text)
    base = int(vram.group(1), 16) if vram else 0x801377E0
    size = (ROOT / f"orig/bin/{name}.ovl").stat().st_size
    return base, size


for _name in OVERLAYS:
    _base, _size = _ovl_bounds(_name)
    MODULES[_name] = dict(
        yaml=f"config/ovl/{_name}.yaml",
        target=f"bin/{_name}.ovl",
        base=_base,
        end=None if _base is None else _base + _size,
        exe=False,
        ld_name=_name,
    )

ASFLAGS = "-EL -march=r3000 -mtune=r3000 -no-pad-sections -O1 -G0 -Iinclude"

# Psy-Q 4.x cc1psx (GCC 2.7.2), verified by exact matches in system.c.
CC1 = f"{TOOLS}/compilers/gcc-2.7.2-psx/cc1"
CPPFLAGS = "-P -undef -D__GNUC__=2 -Dmips -D__psx__ -DINCLUDE_ASM_USE_MACRO_INC=1 -Iinclude -Isrc"
# -G is per module: the main exe uses $gp small data (-G8), overlays can't reach it (-G0).
CFLAGS = "-quiet -O2 -G$gsize -mgas -mgpOPT -mgpopt -msoft-float -fno-builtin -funsigned-char"
# --use-comm-section: uninitialised globals stay `.comm` so they resolve to the splat
# .sbss/.bss definitions (psylink places commons after all objects, in hash order),
# instead of maspsx allocating a private copy in the object (docs/data_migration.md).
MASPSX = f"{TOOLS}/maspsx/maspsx.py --aspsx-version=2.77 -G$gsize --expand-div --use-comm-section"


def gnu_cc_command():
    """The modern C pipeline: cpp | cc1 | maspsx | GNU as."""
    return (
        "bash -o pipefail -c \""
        "(printf '#line 1 \\\"$origname\\\"\\\\n'; cat $in)"
        f" | cpp {CPPFLAGS} -MMD -MF $out.d -MT $out"
        f" | {CC1} {CFLAGS} -o -"
        f" | {sys.executable} {MASPSX}"
        " | sed 's/^\\s*\\.rdata\\s*$$/\\t.section .rodata/'"
        " | ${cross}as $asflags -o $out -\""  # as -G0: maspsx decides what is gp-relative
    )


def include_asm_deps(src):
    """INCLUDE_ASM'd/INCLUDE_RODATA'd files of a C file (cpp's depfile doesn't see them)."""
    incs = re.findall(r'INCLUDE_(?:ASM|RODATA)\("([^"]+)", (\w+)\)', (ROOT / src).read_text())
    return [f"{d}/{f}.s" for d, f in incs]


def write_main_ext_syms():
    """Main-exe symbols as absolute references for the overlay splat configs."""
    out = []
    for src in ("config/symbols/main.txt", "config/symbols/main.manual.txt"):
        for line in (ROOT / src).read_text().splitlines():
            if "=" not in line:
                continue
            decl, _, attrs = line.partition("//")
            out.append(f"{decl.strip()} // {attrs.strip()} absolute:True".replace("//  ", "// "))
    (ROOT / "config/symbols/main.ext.txt").write_text("\n".join(out) + "\n")


def split(mod):
    subprocess.check_call(
        [sys.executable, "-m", "splat", "split", MODULES[mod]["yaml"]], cwd=ROOT
    )
    subprocess.check_call([sys.executable, "tools/split_parts.py", f"asm/{mod}"], cwd=ROOT)
    m = MODULES[mod]
    patch_ld(ROOT / f"build/{m.get('ld_name', m['target'])}.ld")


def patch_ld(path):
    """Place NOLOAD sections exactly at the current location.

    GAS gives sections 16-byte alignment, which would push bss off its original
    (8-byte aligned) address; psylink packs it straight after the loaded data.
    """
    text = path.read_text()
    text = re.sub(r"^(\s*\.\w+) \(NOLOAD\)", r"\1 . (NOLOAD)", text, flags=re.M)
    path.write_text(text)


def linker_objects(ld_path):
    """Objects referenced by a splat-generated linker script."""
    objs = []
    for line in Path(ld_path).read_text().splitlines():
        line = line.strip()
        if line.startswith("build/") and "(" in line:
            o = line.split("(")[0]
            if o not in objs:
                objs.append(o)
    return objs


def source_for(obj):
    """Map build/<x>.o back to its source file."""
    src = obj[len("build/") : -len(".o")]
    return src


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--no-split", action="store_true", help="don't rerun splat")
    ap.add_argument("--toolchain", choices=["gnu", "psyq"], default="gnu",
                    help="gnu (default): build.ninja, modern cc1+maspsx+GNU as/ld; "
                    "psyq: build_psyq.ninja, original ccpsx/aspsx/psylink (docs/psyq_build.md)")
    ap.add_argument("--psyq", dest="toolchain", action="store_const", const="psyq",
                    help="same as --toolchain=psyq")
    args = ap.parse_args()

    if not args.no_split:
        write_main_ext_syms()
        for mod in MODULES:
            split(mod)

    if args.toolchain == "psyq":
        write_psyq_ninja()
        return

    with open(ROOT / "build.ninja", "w") as f:
        n = ninja_syntax.Writer(f, width=120)
        n.variable("ninja_required_version", "1.10")
        n.variable("cross", CROSS)
        n.variable("asflags", ASFLAGS)
        n.newline()
        n.rule("as", "${cross}as $asflags $in -o $out", description="AS $in")
        n.rule(
            "cc",
            # __FILE__ must read "s/<name>.c" as on the original dev machine (used by ASSERT)
            # pipefail: a failing stage must fail the build, not yield an empty object
            gnu_cc_command(),
            description="CC $in",
            depfile="$out.d",
            deps="gcc",
        )
        n.rule(
            "bin",
            "printf '.section .data\\n.incbin \"$in\"\\n' | ${cross}as $asflags -o $out -",
            description="BIN $in",
        )
        n.rule(
            "ld",
            "${cross}ld -EL -T $ldscript $ldextra --no-check-sections -nostdlib "
            "-Map $mapfile -o $out",
            description="LD $out",
        )
        n.rule(
            "elf2exe",
            f"{sys.executable} {TOOLS}/elf2exe.py $exeflag --base $base --end $end $in $out",
            description="IMAGE $out",
        )
        n.rule("mainsyms", f"{sys.executable} {TOOLS}/export_syms.py $in $out", description="SYMS $out")
        n.build("build/main_syms.ld", "mainsyms", "build/scus_942.40.elf", implicit=[f"{TOOLS}/export_syms.py"])
        n.rule("sha1", "sha1sum --quiet -c orig/SHA1SUMS.build && touch $out", description="SHA1 check")
        n.newline()

        targets = []
        for mod, m in MODULES.items():
            ld = f"build/{m.get('ld_name', m['target'])}.ld"
            objs = linker_objects(ROOT / ld)
            for o in objs:
                src = source_for(o)
                if src.endswith(".c"):
                    # INCLUDE_ASM'd files aren't seen by cpp's depfile
                    n.build(o, "cc", src, implicit=include_asm_deps(src),
                            variables=dict(origname=f"s/{Path(src).name}", gsize="8" if m["exe"] else "0"))
                elif src.endswith(".s"):
                    n.build(o, "as", src, implicit=["include/macro.inc"])
                else:
                    n.build(o, "bin", src)
            extra = [
                p
                for p in (
                    f"config/undefined_syms_auto.{mod}.txt",
                    f"config/undefined_funcs_auto.{mod}.txt",
                    f"config/{mod}.syms.ld",
                )
                if (ROOT / p).exists()
            ]
            # per-TU absolute symbols (config/syms.d/<module>/<tu>.ld), merge-friendly
            extra += sorted(str(p.relative_to(ROOT)) for p in (ROOT / "config/syms.d" / mod).glob("*.ld"))
            elf = f"build/{m['target']}.elf"
            n.build(
                elf,
                "ld",
                objs,
                implicit=[ld] + extra + ([] if m["exe"] else ["build/main_syms.ld"]),
                variables=dict(
                    ldscript=ld,
                    ldextra=" ".join(f"-T {p}" for p in extra)
                    + ("" if m["exe"] else " -T build/main_syms.ld"),
                    mapfile=f"build/{m['target']}.map",
                ),
            )
            img = f"build/{m['target']}"
            n.build(
                img,
                "elf2exe",
                elf,
                variables=dict(
                    exeflag="--exe" if m["exe"] else "",
                    base=hex(m["base"]),
                    end=hex(m["end"]),
                ),
            )
            targets.append(img)
            n.newline()

        n.build("build/.sha1ok", "sha1", targets, implicit=["orig/SHA1SUMS.build"])
        n.default("build/.sha1ok")

    # checksum list keyed on build outputs
    sums = []
    for line in (ROOT / "orig/SHA1SUMS").read_text().splitlines():
        h, p = line.split()
        name = Path(p).name
        for m in MODULES.values():
            if Path(m["target"]).name == name:
                sums.append(f"{h}  build/{m['target']}")
    (ROOT / "orig/SHA1SUMS.build").write_text("\n".join(sums) + "\n")


PSYQ = "build/psyq"
PSYQ_FALLBACK = "config/psyq_fallback.txt"
# what gen_psylink.py reads besides the objects
PSYQ_LINK_INPUTS = [
    f"{TOOLS}/gen_psylink.py", f"{TOOLS}/elf2psyq.py", f"{TOOLS}/psyq_obj.py",
    "config/psyq_libs.txt", "assets/tcache/tcache.bin", "orig/siphon.map",
]


def psyq_fallbacks():
    """C TUs config/psyq_fallback.txt says to build with the modern compiler."""
    path = ROOT / PSYQ_FALLBACK
    if not path.exists():
        return set()
    return {l.split("#")[0].strip() for l in path.read_text().splitlines()} - {""}


def write_psyq_ninja():
    """build_psyq.ninja: the whole game with the original Psy-Q toolchain.

    * every C TU: tools/psyq_cc.py (ccpsx -E, cc1psx, psyq_asm.py, aspsx) under wine
      -> build/psyq/cc/<tu>.obj, unless listed in config/psyq_fallback.txt;
    * hand-written asm, splat data blobs, binary assets and fallback TUs: GNU as /
      the modern C pipeline -> build/psyq/gnu/<path>.o (no dependency on build.ninja);
    * tools/gen_psylink.py --libs converts those to SN OBJs and writes the psylink
      command file; tools/psyq_link.sh runs psylink + cpe2exe under wine;
    * build/psyq/scus_942.40 and build/psyq/bin/<ovl>.ovl are checked against
      build/psyq/SHA1SUMS (orig/SHA1SUMS with the paths moved).
    The splat outputs (asm/, assets/, build/*.ld, config/undefined_*_auto) come from
    running configure.py itself.
    """
    fallback = psyq_fallbacks()
    gnu = f"{PSYQ}/gnu"
    with open(ROOT / "build_psyq.ninja", "w") as f:
        n = ninja_syntax.Writer(f, width=120)
        n.comment("generated by configure.py --psyq - do not edit")
        n.variable("ninja_required_version", "1.10")
        # its own .ninja_log/.ninja_deps, apart from build.ninja's
        n.variable("builddir", PSYQ)
        n.variable("cross", CROSS)
        n.variable("asflags", ASFLAGS)
        n.newline()
        n.rule("as", "${cross}as $asflags $in -o $out", description="AS $in")
        n.rule("cc", gnu_cc_command(), description="CC (gnu fallback) $in", depfile="$out.d", deps="gcc")
        n.rule(
            "bin",
            "printf '.section .data\\n.incbin \"$in\"\\n' | ${cross}as $asflags -o $out -",
            description="BIN $in",
        )
        # Wine: tools/psyq_wine.sh creates the private prefix (build/wineprefix) once under
        # a lock, and psyq_cc.py keeps every compile in its own work dir, so these run in
        # parallel.  psylink/cpe2exe write fixed file names in build/psyq: one job at a time.
        n.rule(
            "psyqcc",
            f"{sys.executable} {TOOLS}/psyq_cc.py -O2 -G$gsize -Iinclude -Isrc --srcname $srcname "
            "--depfile $out.d -c $in -o$out",
            description="CCPSX $in",
            depfile="$out.d",
            deps="gcc",
        )
        n.rule(
            "psylnk",
            f"{sys.executable} {TOOLS}/gen_psylink.py --out {PSYQ} --gnu-dir {gnu} --libs $objflags > $out.log",
            description="GEN $out",
        )
        n.pool("psylink", 1)
        n.rule("psylink", f"{TOOLS}/psyq_link.sh --no-compare {PSYQ}", description="PSYLINK $out", pool="psylink")
        n.rule("sha1", f"sha1sum --quiet -c {PSYQ}/SHA1SUMS && touch $out", description="SHA1 check (psyq)")
        n.newline()

        tool_deps = [f"{TOOLS}/psyq_cc.py", f"{TOOLS}/psyq_asm.py", f"{TOOLS}/psyq_wine.sh"]
        link_objs, objflags, seen_tus = [], [], set()
        for mod, m in MODULES.items():
            ld = f"build/{m.get('ld_name', m['target'])}.ld"
            for o in linker_objects(ROOT / ld):
                src = source_for(o)
                gobj = f"{gnu}/{o[len('build/'):]}"
                if src.endswith(".c"):
                    tu = Path(src).stem
                    seen_tus.add(tu)
                    if tu in fallback:
                        n.build(gobj, "cc", src, implicit=include_asm_deps(src),
                                variables=dict(origname=f"s/{Path(src).name}", gsize="8" if m["exe"] else "0"))
                        link_objs.append(gobj)
                        continue
                    obj = f"{PSYQ}/cc/{tu}.obj"
                    n.build(obj, "psyqcc", src, implicit=include_asm_deps(src) + tool_deps,
                            variables=dict(srcname=f"s/{Path(src).name}", gsize="8" if m["exe"] else "0"))
                    link_objs.append(obj)
                    objflags.append(f"--obj {tu}={obj}")
                elif src.endswith(".s"):
                    n.build(gobj, "as", src, implicit=["include/macro.inc"])
                    link_objs.append(gobj)
                else:
                    n.build(gobj, "bin", src)
                    link_objs.append(gobj)
            n.newline()
        unknown = fallback - seen_tus
        if unknown:
            sys.exit(f"{PSYQ_FALLBACK}: no such C TU: {' '.join(sorted(unknown))}")

        lds = [f"build/{m.get('ld_name', m['target'])}.ld" for m in MODULES.values()]
        cfg = sorted(str(p.relative_to(ROOT)) for pat in (
            "config/undefined_*_auto.*.txt", "config/*.syms.ld", "config/syms.d/*/*.ld",
            "config/symbols/*.txt") for p in ROOT.glob(pat))
        n.build(f"{PSYQ}/siphon.lnk", "psylnk", link_objs, implicit=lds + cfg + PSYQ_LINK_INPUTS,
                variables=dict(objflags=" ".join(objflags)))
        outs = [f"{PSYQ}/{m['target']}" for m in MODULES.values()]
        n.build(outs, "psylink", f"{PSYQ}/siphon.lnk", implicit=[f"{TOOLS}/psyq_link.sh", f"{TOOLS}/psyq_wine.sh"],
                implicit_outputs=[f"{PSYQ}/siphon.map", f"{PSYQ}/siphon.cpe"])
        n.build(f"{PSYQ}/.sha1ok", "sha1", outs, implicit=[f"{PSYQ}/SHA1SUMS"])
        n.default(f"{PSYQ}/.sha1ok")

    # checksum list keyed on the psyq build outputs
    sums = []
    for line in (ROOT / "orig/SHA1SUMS").read_text().splitlines():
        h, p = line.split()
        name = Path(p).name
        for m in MODULES.values():
            if Path(m["target"]).name == name:
                sums.append(f"{h}  {PSYQ}/{m['target']}")
    (ROOT / PSYQ).mkdir(parents=True, exist_ok=True)
    (ROOT / PSYQ / "SHA1SUMS").write_text("\n".join(sums) + "\n")
    print(f"wrote build_psyq.ninja ({len(objflags)} ccpsx TUs, {len(fallback)} GNU fallbacks); "
          "run: ninja -f build_psyq.ninja")


if __name__ == "__main__":
    main()
