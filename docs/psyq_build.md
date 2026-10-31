# Linking with the original Psy-Q linker

The game can be linked with the original **psylink 2.73** (Psy-Q 4.3) and converted with
**cpe2exe**. The output is byte-identical to `orig/scus_942.40` and all 15 overlays. This
proves the link side independently of the compiler. For now every game object still comes
from the modern build (GNU as ELF), converted to SN LNK format.

```
python3 configure.py && ninja                 # the modern build (its objects and maps are inputs)
tools/gen_psylink.py --libs                   # build/psyq/siphon.lnk + converted objects
tools/psyq_link.sh                            # psylink + cpe2exe + compare -> 16x OK
tools/psyq_mapdiff.py                         # optional: our map vs orig/siphon.map
```

With `--obj TU=file.obj`, a ccpsx-compiled object replaces `build/src/main/<TU>.c.o`.
For example, the five pilot TUs from `tools/psyq_check_tu.py --keep DIR egsdebug egstex
egsmisc egsanim stream` still give 16x OK. Without `--libs`, the `lib_*.s` asm objects
are used instead of the real library modules, and the result is also 16x OK.

The tools run under wine through `tools/psyq_wine.sh`. Set `PSYQ_WINEPREFIX` to use your own prefix.

## Tools

| tool | what it does |
|---|---|
| `tools/elf2psyq.py` | Converts an ELF `.o` to an SN `.OBJ`. It handles sections (`.rodata`→`.rdata`, `--rename`), XDEF/XREF/XBSS and relocations, and also offers `--truncate SEC=SIZE` and `--symrename`. |
| `tools/gen_psylink.py` | Writes the psylink command file and converts every object into `build/psyq/obj/`. |
| `tools/psyq_liborder.py` | Writes `config/psyq_libs.txt`: the library modules in the order psylink placed them. |
| `tools/psyq_link.sh` | Runs psylink and `cpe2exe -CJ`, then compares the results. |
| `tools/psyq_mapdiff.py` | Lists the sections and symbols whose addresses differ between two psylink maps. |
| `tools/cpe.py` | Lists the records of a CPE file, or flattens an address range from one. |

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
    defined there into equates; it warns if a *C* object's bss starts unaligned.
    That warning means the TU boundary in the yaml is wrong.
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
  out the cuts from the modern build's map (`blob_cuts`), so newly migrated TUs need no
  changes here.

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
* **ccpsx TUs.** As each TU becomes buildable with ccpsx, pass its OBJ with `--obj`.
  The plan is to make that the default for migrated TUs, then for all of them.
  movie.ovl's libpress/libmcrd/libcard code is still the GNU asm, including DecDCTvlc2,
  which must stay asm.
