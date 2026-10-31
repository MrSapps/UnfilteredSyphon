# Building with the original Psy-Q toolchain

The whole game builds byte-identical with the original **Psy-Q 4.3** toolchain: every C
TU with ccpsx (cpppsx + cc1psx `2.7.2.SN32.3.7`) and **aspsx 2.77**, the link with
**psylink 2.73** and the real 4.3 libraries (libcd from the 4.3.1 patch), the PS-X EXE
with **cpe2exe**. The exe and all 15 overlays come out identical to `orig/`. The tools
run under wine.

## One command

```
python3 configure.py --psyq        # (= --toolchain=psyq) runs splat, writes build_psyq.ninja
ninja -f build_psyq.ninja          # -> build/psyq/scus_942.40, build/psyq/bin/*.ovl, SHA1 check
```

`build.ninja` (the modern build) is not touched, and `--psyq` does not need it: from an
empty `build/`, the commands above give 16/16 identical in about 25 s (including the
creation of the wine prefix). `--no-split` skips splat as usual. What
`build_psyq.ninja` does:

| step | rule | output |
|---|---|---|
| every C TU (51 main, 14 overlays): `tools/psyq_cc.py -O2 -G8` (main) / `-G0` (overlays), compiled as `s/<tu>.c` | `psyqcc` | `build/psyq/cc/<tu>.obj` |
| hand-written asm (losgte, egsgte, egssort*, egshmdgte, movie's DecDCTvlc2 and libraries, ...), splat data blobs, data-in-text, the hog and tcache binaries: GNU as | `as`, `bin` | `build/psyq/gnu/<path>.o` |
| C TUs listed in `config/psyq_fallback.txt`: the modern cc1 + maspsx + GNU as pipeline | `cc` | `build/psyq/gnu/src/.../<tu>.c.o` |
| `tools/gen_psylink.py --libs --gnu-dir build/psyq/gnu --obj <tu>=build/psyq/cc/<tu>.obj ...`: converts the GNU objects with `elf2psyq.py`, extracts the library modules, writes the command file and `equates.txt` | `psylnk` | `build/psyq/siphon.lnk`, `build/psyq/obj/`, `build/psyq/lib/` |
| `tools/psyq_link.sh --no-compare`: psylink + `cpe2exe -CJ` | `psylink` | `build/psyq/scus_942.40`, `build/psyq/bin/*.ovl`, `siphon.map/.cpe/.sym` |
| `sha1sum -c build/psyq/SHA1SUMS` (written by configure from `orig/SHA1SUMS`) | `sha1` | `build/psyq/.sha1ok` |

`build_psyq.ninja` keeps its own `.ninja_log`/`.ninja_deps` in `build/psyq` (`builddir`),
apart from build.ninja's. ccpsx compiles get depfiles (from cpppsx's line markers) plus
the INCLUDE_ASM'd `.s` files, as in the modern build.

**Fallbacks.** `config/psyq_fallback.txt` lists C TUs to build with the modern compiler
instead (converted by `elf2psyq.py`, like the asm). It is empty: all 65 C TUs match under
ccpsx. Add a TU there if one stops compiling or matching under ccpsx, and remove it once it
does again (`tools/psyq_check_tu.py <tu>`).

**What it still depends on.**

* splat's outputs, from `configure.py` itself: `asm/`, `assets/`, the linker scripts
  `build/*.ld` (the object order) and `config/undefined_*_auto.*.txt`.
* GNU binutils (`tools/bin/mipsel-linux-gnu-as`) for the asm segments, blobs and binaries.
* The SDK at `/home/snake/dev/psyq_sdk` (`psyq_4.3` bin/lib, `psyq_4.4/lib/patches/libcd.lib`;
  `PSYQ_SDK` overrides the 4.3 dir for the tools) and `wine`.
* `orig/siphon.map`, for the names and sizes of the commons (`commons.obj`, see below).
* No linked ELF and no map of the modern build: equate values come from the config text
  files (see [Equates](#equates)), the blob cuts from the blob file names.

**Wine.** `tools/psyq_wine.sh` uses a private prefix, `build/wineprefix` (or
`PSYQ_WINEPREFIX`). It is created on first use under a lock (`build/wineprefix.lock`,
`flock`), so parallel jobs that start together wait for one `wineboot`; `psyq.ini` is
replaced atomically. ccpsx itself is *not* safe in parallel: `ccpsx -S` preprocesses into a
fixed temp file (`C:/users/<user>/Temp/PQ2` in the prefix; wine ignores TMP/TEMP), so two
compiles at once can swap sources (seen: main.obj built from cas.c). `psyq_cc.py`
therefore runs `ccpsx -E -o <file>` and `cc1psx -quiet <flags>` itself, exactly as ccpsx
calls them, with `TMPDIR` set to a private work dir for cc1psx's own temp files. The objects
are identical to those of `ccpsx -S`, and ninja can run the compiles at full `-j`. psylink
and cpe2exe write fixed names in `build/psyq` and run in a depth-1 pool. To stop this
prefix's server: `WINEPREFIX=$PWD/build/wineprefix wineserver -k` (never a bare
`wineserver -k`, which may hit other wine sessions).

## Equates

psylink needs a value for every name some object references but no object defines.
`gen_psylink.py` writes them as `name equ $value` and lists them in
`build/psyq/equates.txt` (name, value, referencing module, source of the value). The values
come from the files the GNU link is given (`config/undefined_{syms,funcs}_auto.<mod>.txt`,
`config/<mod>.syms.ld`, `config/syms.d/<mod>/*.ld`; for an overlay also main's), then
`config/symbols/<mod>[.manual].txt`, and for splat's address labels (`D_80123456`,
`func_...`, `jtbl_...`) the address in the name. Nothing is taken from a linked ELF any more,
and since every C TU is a ccpsx object, no equate stands in for a global a C object
defines (a name an object defines is never an equate). With all 65 TUs from ccpsx there
are 697 (2026-10-31):

| count | what | why it is still an equate |
|---|---|---|
| 178 + 23 + 26 + 5 | main: `D_<addr>` of static data or interior addresses of C objects (`D_8011EB50` = `D_8011EB40+0x10`, `D_8010F7FE` = `&gSniperView[1]`, `D_80105F55` = `&gWeaponInfo[0].clipSize`, ...): splat labels, `config/undefined_syms_auto.main.txt`, `config/main.syms.ld`, `config/syms.d/main/*.ld` | referenced by INCLUDE_ASM'd (NON_MATCHING) functions, or by another TU's C through the splat name. The C object defines the data, but as a static or as part of a larger object, so it has no global of that name. They go away as those functions are decompiled and the references become `&array[i]`/field accesses. |
| 82 | main: `D_<addr>` inside the commons (0x80110230.., 0x80127C58..) | interior addresses of commons (arrays/structs) used by name; `commons.obj` only declares the map names |
| 248 + 2 | init.ovl (`config/undefined_syms_auto.init.txt`): main bss/commons addresses (0x8012xxxx), `D_5EE8` (= `_init_size`), and the hog bases `gINTRFACEHog`/`gSPFXHog` | init reaches main data by address; the hog binaries are sections (`INTRFACEHog`, `SPFXHog`, group `load`) without symbols |
| 52 + 2 | `func_<addr>` calls from main and overlays into overlay code (`MenuExecute`, `TitleExecute`, `*ObjInit`, ...; `config/undefined_funcs_auto.*`), title's calls into movie.ovl's MemCard code (renamed `title_MemCardStart`...), `gMovieOpenFlag`, `gLoadingString` | overlays share addresses, so the callee is referenced by address; the original referenced them by name |
| 4 | `D_801377E0`, `D_80140FE0`, `D_80145BF0`, `D_8014BAD8` | psylink's section boundary symbols (`_bss_orgend`, `_tcache_orgend`, `_init_org`, `_init_bss_org`) used by address |
| 71 | overlay-internal `D_<addr>` of menu/chopper/subway/park/museum | static data or interior labels of the overlay's own C object, referenced by its INCLUDE_ASM'd functions |
| 10 | overlays' `undefined_syms_auto`: main data interiors (`D_80106215`...), movie's `D_A000DF80`/`D_A000DFAC` (uncached KSEG1 addresses) | as above |
| 5 | scratchpad `D_1F80xxxx` | hardware addresses (losgte/egs asm) |

## Running the steps by hand

```
python3 configure.py && ninja                 # or ninja -f build_psyq.ninja (then add --gnu-dir build/psyq/gnu)
tools/psyq_check_tu.py --keep build/psyqtu main system ...    # ccpsx objects, checked per TU
tools/gen_psylink.py --libs [--obj TU=build/psyqtu/TU.obj ...]   # build/psyq/siphon.lnk
tools/psyq_link.sh                            # psylink + cpe2exe + compare -> 16x OK
tools/psyq_mapdiff.py                         # optional: our map vs orig/siphon.map
```

`gen_psylink.py` reads the GNU objects from `build/` by default (the modern build), or
from `--gnu-dir`. With `--obj TU=file.obj`, a ccpsx-compiled object replaces
`build/src/main/<TU>.c.o` (for an overlay TU, `build/src/<TU>/<TU>.c.o`; its sections
are renamed to `<ovl>.text` etc. and its clashing symbols to `<ovl>_<name>`, as for a
converted object); the GNU object of such a TU is not needed. Without `--libs`, the
`lib_*.s` asm objects are used instead of the real library modules, and the result is
also 16x OK. `tools/psyq_objeq.py A.obj B.obj` compares two SN objects ignoring the
source path record (aspsx writes the absolute path of the source it assembled).

The tools run under wine through `tools/psyq_wine.sh`. Set `PSYQ_WINEPREFIX` to use your own prefix.

## Tools

| tool | what it does |
|---|---|
| `tools/psyq_cc.py` | `ccpsx -c` replacement: `ccpsx -E`, cc1psx, then `psyq_asm.py` (expands INCLUDE_ASM for aspsx), then aspsx. Parallel-safe; `--srcname s/x.c`, `--depfile`. |
| `tools/elf2psyq.py` | Converts an ELF `.o` to an SN `.OBJ`. It handles sections (`.rodata`→`.rdata`, `--rename`), XDEF/XREF/XBSS and relocations, and also offers `--truncate SEC=SIZE` and `--symrename`. |
| `tools/gen_psylink.py` | Writes the psylink command file and converts every object into `build/psyq/obj/`. |
| `tools/psyq_liborder.py` | Writes `config/psyq_libs.txt`: the library modules in the order psylink placed them. |
| `tools/psyq_link.sh` | Runs psylink and `cpe2exe -CJ`, writes `scus_942.40` and `bin/*.ovl`, then compares them (`--no-compare` skips that). |
| `tools/psyq_mapdiff.py` | Lists the sections and symbols whose addresses differ between two psylink maps. |
| `tools/psyq_objeq.py` | Compares SN objects, ignoring the source file record. |
| `tools/cpe.py` | Lists the records of a CPE file, or flattens an address range from one. |

## Compiling TUs that still contain asm functions

`INCLUDE_ASM` works under ccpsx too, so a TU does not need to be fully C to be built with
the original compiler and assembler:

```
tools/psyq_cc.py -O2 -G8 -Iinclude -Isrc --srcname s/foo.c -c src/main/foo.c -ofoo.obj   # like ccpsx -c
tools/psyq_check_tu.py --keep build/psyqtu egsmem save ...      # uses psyq_cc.py
```

`psyq_cc.py` runs the same steps as `ccpsx -c` (`ccpsx -v` shows them), with one filter
added:

1. `ccpsx -E` runs cpppsx, then cc1psx compiles (what `ccpsx -S` does, without its
   shared temp file).
2. `tools/psyq_asm.py` expands the INCLUDE_ASM markers.
3. `aspsx -q -G<n>` assembles the result.

If a file has no markers, the filter changes nothing, and the object is the same as
plain `ccpsx -c` gives.

How it works:

* **Markers in source order.** Under `_PSYQ`, `include/common.h` makes
  `INCLUDE_ASM(dir, NAME)` emit `.psyq_include 1 "dir/NAME.s"` at file scope and
  `.psyq_include 2 ...` inside a dummy function `__psyq_include_asm_NAME`. These are the
  same two places as the GAS build's INC_PART 1/2 (`tools/split_parts.py`). cc1psx is
  GCC 2.7.2 and buffers function bodies in the same way, so the parts land in the
  same order.
* **What the filter does.** `psyq_asm.py` replaces each marker with the matching
  INC_PART block, rewritten for aspsx. It removes the dummy function: its `.globl`, the
  `.ent`/`.frame`/`.end` lines, and cc1's `j $31`. After a part-1 block, it restores the
  section cc1 was in.
* **aspsx syntax** (worked out by experiment with aspsx 2.77):
  * Input must have CRLF line endings, and aspsx needs absolute paths under wine.
  * Only numeric registers are accepted (`$v0` is rejected).
  * `%hi(sym+off)` and `%lo(sym+off)` work (relocations 0x52/0x54).
  * There is no `%gp_rel`. Instead, `lw $2, sym` is gp-relative (0x64) when `sym` is
    `.sdata`/`.sbss`/`.comm`/`.lcomm` *in this file*. For an `.extern`, it expands to
    `lui`+`lw` through `$at`, which fails under `.set noat`. So a gp-relative access to
    data the TU does not define cannot assemble. The TU's small data must be migrated
    first (`SDATA_LOCAL` is a no-op under `_PSYQ`).
  * Inside `.set noreorder`, aspsx assembles raw instructions as written.
  * Sections are `.rdata`, `.text`, `.data` and `.sdata`. `.L` labels stay local.
    `.word label` in `.rdata` (jump tables) gets section-relative relocations.
* **What the filter emits.**
  * An instruction without a relocation is written as `.word <encoding>`, taken from
    the splat comment, so aspsx's parser and macros never see it.
  * The rest stay symbolic: `%hi`/`%lo`, `%gp_rel` (as the macro form above), and
    `j`/`jal` to a label.
  * aspsx ignores `.word` when it tracks delay slots. So the instruction after a
    symbolic `j`/`jal` is also written symbolically. Otherwise the next symbolic jump
    is rejected with "not allowed in a delay slot".
  * `.asciz` and `.short` become `.byte`. `glabel`/`dlabel` become `.globl`+label, as
    in the GAS build. `jlabel` becomes a local label.

Status as of 2026-10-31:

* **Main exe.** `psyq_check_tu.py` reports all 51 main TUs OK in every section (aspsx
  2.77). Many of them still contain INCLUDE_ASM functions.
* **Linked.** With all 51 passed to `gen_psylink.py --libs` as `--obj` objects
  (`--obj <tu>=build/psyqtu/<tu>.obj` after `psyq_check_tu.py --keep build/psyqtu ...`),
  `psyq_link.sh` gives 16x OK: the main exe's game code is built entirely by
  ccpsx + aspsx, linked by psylink.
* **aspsx doesn't auto-align `.word`** (GNU as does). An INCLUDE_RODATA blob after
  data of odd size (a `__FILE__` string) lands unaligned; define it in C instead.
* **The other main TUs.** None of them fails because of the asm path:
  * Some fail to assemble ("Assembler does not have AT register available"). Their asm
    makes gp-relative accesses to small data the TU does not define.
  * The rest have a longer `.text`. Their C code goes through `SDATA_LOCAL` externs,
    which ccpsx reaches with `lui`+`lw`.
  * Both are data-migration work. A variable the original reached through $gp from
    several TUs is a common (`.comm`, a tentative definition in C), which aspsx
    does reach through gp.
* **Overlays.** All 14 overlay TUs define their own `.rdata/.data/.bss` and match in
  every section under ccpsx `-O2 -G0` + aspsx 2.77: `tools/psyq_check_tu.py chopper ...`
  (a TU named after an overlay is laid out from `config/ovl/<ovl>.yaml`, compiled with
  `-G0` and compared against `orig/bin/<ovl>.ovl`). Linked with `--obj` for all 14 (plus
  the main TUs that pass), every overlay is byte-identical; see the overlay notes in
  docs/data_migration.md. Left as splat blobs: movie's library rdata/data/bss and the
  commons psylink places after an overlay's own `.bss` (`DATA_PIECES` in
  `tools/gen_ovl_yaml.py`).
* **Main exe link (2026-10-31).** Since data batch 1b, target's `.sbss` (0x8010FFDC) and
  cas's `.bss` (0x80115634) start at addresses ending in 4. psylink moves them to 8, so
  the exe and every overlay that references main symbols after them comes out DIFF. With
  those nine batch-1b files (yaml, main.syms.ld, cas/joystick/object/player/sniper/target/
  weapon.c) taken from eb4c5d2, the link with all 14 overlay objects gives 16x OK. The
  TU boundary there needs fixing.

## SN OBJ facts used by the converter

* Relocation types: `0x10` word32, `0x4A` jal/j 26, `0x52` hi16, `0x54` lo16, `0x64` gprel16.
  The field is left zero and the whole value goes in the expression.
  * Expressions: `0x02 n` is an external symbol and `0x04 n` a section base. `0x2C a b`
    is a+b, `0x2E a b` is b−a, and `0x00 v` is a constant.
  * HI16 and LO16 both carry the full value of their pair; psylink does the carry.
* GNU REL addends move into the expression. A hi16 gets the combined addend of its pair.
* `R_MIPS_PC16` has no SN form. It only occurs as a branch to a global label in the same
  section, so the converter resolves it.
* The SECTION record's alignment byte works as a code, not as a number: 8 (the value ccpsx
  writes) gives 4-byte packing, 2 and 4 give no alignment, and anything else gives 256.
  The converter always writes 8.

## psylink behaviour (worked out by experiment)

* **Groups.** `name group org($addr)`, `name group bss`, and `name group over(other),file("x.ovl")`.
  * An `over` group gets the address of `other`. The next group without an org starts
    after the largest group of that overlay set. That is how levspec..title land at
    0x80140FE0 (after tcache), and init lands after park2.
  * A group with `file()` is written to its own file, bss sections included as zeros.
* **Sections.** psylink assigns sections to groups by name. siphon.map has per-overlay
  names (`menu.text`, ...), so the converter renames an overlay object's sections. The
  original evidently had all eight ccpsx sections renamed: the map has
  `_menu_sdata_org` and `_menu_ctors_org`. The command file declares all of them.
  `.ctors`/`.dtors` must be declared in the `text` group, because libsn SNMAIN uses
  their start and end.
* **Commons (XBSS).** A common is allocated right after the section contribution of the
  *first object that declares it*. Within one object, commons are laid out in
  symbol-table order:
  * Each common is aligned by its size: 1, 2, 4, and 8 for 8 bytes or more.
  * A contribution that holds commons starts 8-aligned. Once a section has any commons
    at all (main `.sbss`/`.bss` always do), *every* object's contribution to it starts
    8-aligned. So in the original, no TU's `.sbss`/`.bss` can start at an address
    ending in 4. A splat bss blob that does start at one is really the previous TU's
    padding. `gen_psylink.py` drops those 4 bytes from the blob and turns any names
    defined there into equates. A *C* object's bss that starts unaligned means the
    TU boundary in the yaml is wrong; `psyq_check_tu.py` and the SHA1 check catch it.
  * Some commons can fall after `__sbss_orgend`, as in the original.

  In the original, every common (game and library alike) sits after all `.sbss`/`.bss` in
  one run. To reproduce this, `--libs` turns every common declaration into a plain XREF
  and declares all commons in one generated object (`obj/commons.obj`) that is
  included last:
  * Sizes come from the library or from the gap in siphon.map (`common_size`).
  * The table order is set by an object included first that only references the names,
    in reverse address order (`--commons-order rev`).

  The result matches the original exactly. How the original build got this layout is
  still unknown.
* **Map symbols.** psylink defines `_<sec>_org/_size/...` for every section. In
  siphon.map, the `_size` values are the pre-padding sums. They give a TU-independent
  check of each section's raw size.
* The 4 bytes at tcache.ovl+4 (`efefefe4`) are uninitialised padding that psylink wrote in
  1998. They are supplied as data in `tcache.rdata`, and `tcache.bss` is given as a
  0x97F8-byte bss.

## Libraries

* The main exe uses 317 modules: the 4.3 libs, with libcd taken from
  `psyq_4.4/lib/patches` (4.3.1). Its data (BIOS) only matches the patch version.
* Modules are placed by their exported map symbols. The duplicates (libc/libc2 MEMMOVE,
  libapi/libcard C112) are resolved in stdlib search order.
* The code-less table modules (SNDEF, GLOBAL, SINCOS, SQRTBL, CSTBL, S_M, ZEROBUF, S_RMP0,
  S_RMP2) are slotted in by their `.data` address.
* `config/psyq_libs.txt` is generated. The modules are included explicitly, in that order,
  after all game objects.
* Library data starts at:

  | section | address | what is there |
  |---|---|---|
  | `.data` | 0x80107BA4 | SNMAIN |
  | `.sbss` | 0x80110228 | SNMAIN, right after egsanim |
  | `.bss`  | 0x80125278 | 0x29E0 bytes of library bss |

  `gen_psylink.py` truncates the splat blobs that run past these addresses. It works
  out the cuts from each blob's address, which splat puts in its file name
  (`data_<file offset>`, `bss_<vram>`; `blob_vram`), and its size, so newly migrated TUs
  need no changes here.

## Known issues / remaining work

* **Hog split (fixed).** The yaml used to split the hogs 0x200 bytes too early. SPFXHog
  now starts at file offset 0x1689C0, as siphon.map says (INTRFACEHog is 0x281C0 bytes).
* **Data blobs.**
  * chopper.data is 0xA4 bytes in the map, but the splat blob is 0xA8. The bytes still
    match, because chopper.bss holds commons and starts 8-aligned.
  * The ranges of the commons and library data are still hard-coded. They come from
    siphon.map, as are the common sizes.
* **Overlay symbols.** splat's overlay symbols repeat between overlays, so
  `gen_psylink.py` renames clashing ones to `<ovl>_<name>`. Equates (`name equ $addr`)
  cover names that no object defines.
* **ccpsx TUs.** All 65 C TUs are built by ccpsx in `build_psyq.ninja`
  (`config/psyq_fallback.txt` is empty). movie.ovl's libpress/libmcrd/libcard code is still the GNU asm, including DecDCTvlc2,
  which must stay asm.
