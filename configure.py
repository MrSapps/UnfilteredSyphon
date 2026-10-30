#!/usr/bin/env python3
"""Generate build.ninja for the Syphon Filter (Nov 11 1998 prototype) decomp.

    python3 configure.py          # writes build.ninja (runs splat first unless --no-split)
    ninja                         # builds and checks SHA1 against orig/SHA1SUMS
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
CFLAGS = "-quiet -O2 -G8 -mgas -mgpOPT -mgpopt -msoft-float -fno-builtin -funsigned-char"
MASPSX = f"{TOOLS}/maspsx/maspsx.py --aspsx-version=2.79 -G8 --expand-div"


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
    args = ap.parse_args()

    if not args.no_split:
        write_main_ext_syms()
        for mod in MODULES:
            split(mod)

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
            "(printf '#line 1 \"$origname\"\\n'; cat $in)"
            f" | cpp {CPPFLAGS} -MMD -MF $out.d -MT $out"
            f" | {CC1} {CFLAGS} -o -"
            " | sed 's/^\\t\\.rdata$$/\\t.section .rodata/'"
            f" | {sys.executable} {MASPSX}"
            " | ${cross}as $asflags -o $out -",  # -G0: maspsx decides what is gp-relative
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
                    incs = re.findall(r'INCLUDE_(?:ASM|RODATA)\("([^"]+)", (\w+)\)', (ROOT / src).read_text())
                    n.build(o, "cc", src, implicit=[f"{d}/{f}.s" for d, f in incs],
                            variables=dict(origname=f"s/{Path(src).name}"))
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


if __name__ == "__main__":
    main()
