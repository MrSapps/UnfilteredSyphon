# Migrating a TU's data into its C file

Goal: every main-exe TU defines its own `.rdata/.data/.sdata/.sbss/.bss`, so the
same source builds byte-identical objects with both toolchains:

* the modern build (cc1 2.7.2 + maspsx + GNU as/ld, splat linker script), and
* the original Psy-Q 4.3 toolchain (`ccpsx` = cc1psx 2.7.2.SN32.3.7 + aspsx 2.77,
  later psylink 2.73).

Two facts force this. aspsx addresses small data through `$gp` only when the
symbol is defined (or `.comm`'d) in the same file, so `extern` + `SDATA_LOCAL`
no longer works. And psylink lays out each section object by object, in link order,
so a TU's data has to come from that TU's object.

## How the compiler places data (cc1 2.7.2, `-O2 -G8`, aspsx)

| C definition                                  | section                       | order |
|-----------------------------------------------|-------------------------------|-------|
| initialised, size <= 8 (including `= 0`/`NULL`) | `.sdata`                      | definition order |
| initialised, size > 8                         | `.data` (`.rdata` if `const`) | definition order |
| string literal, size <= 8                     | `.sdata` (!)                  | first use, while the function is compiled |
| string literal, size > 8                      | `.rdata`                      | first use |
| `static`, uninitialised, size <= 8            | `.sbss` (`.lcomm`)            | definition order |
| `static`, uninitialised, size > 8             | `.bss` (`.lcomm`)             | definition order |
| global, uninitialised                         | common (`.comm`)              | **not in the TU's sections**, see below |

* cc1 2.7.2 does not move explicitly zero-initialised variables to bss. `static int x = 0;`
  is `.sdata`, while `static int x;` is `.sbss`.
* File-scope variables are emitted where they are defined. A function's literals are
  emitted while that function is compiled, so variables defined above the first
  function come before every literal in `.sdata`. Unreferenced statics are still
  emitted.
* A symbol listed in `orig/siphon.map` is a global. If it isn't listed, it is
  `static`. Keep the `D_<addr>` name for statics you can't name yet:
  `tools/psyq_check_tu.py` checks those addresses.
* **Commons.** psylink appends every common after the `.sbss`/`.bss` of all
  objects, in its own hash order. That is why the tail of `.sbss`
  (0x80110230–0x801104A0) and of `.bss` (from about 0x80127C58) look shuffled. Commons
  belong to no TU's sections. A TU should still give a *tentative definition*
  (`void *gHogHeader;`) for each small common it reads through `$gp`, otherwise aspsx
  emits `lui/lw`. Use the real map name: the hash order presumably depends on it (not yet verified).
  Commons that the original reached with `lui`/`addiu` stay `extern`.
* Game TUs (not the egs* engine files) start their `.rdata` with a 16-byte
  `{1, 1, 1, 0}` const. It is defined at file scope as
  `static const s32 D_xxx[4] = {1, 1, 1, 0};` before the first function. Its origin
  (probably a header) is still unknown.

### Pitfalls

* **`__LINE__`.** `ASSERT` bakes the line number into `.text`. Keep every line
  above the last ASSERT where it is: when you replace the `SDATA_LOCAL` and `extern`
  blocks, keep the same number of lines.
* **Builtins.** The original compiled without `-fno-builtin`, but the modern build
  passes it. Write inlined library calls explicitly, for example
  `__builtin_strcpy(buf, "CDERR")`. That puts the literal back into `.sdata` and gives
  the same block copy under both toolchains.
* **TU boundaries in the yaml can be wrong.** Check them before you trust them. One
  case found so far: `gEGSMathIdentityMatrix` was in egsanim's `.rodata`, but it starts
  egsmath's. Another: sprite's texture-name strings sit inside joystick.c's
  `INCLUDE_RODATA` blob.
* **maspsx and file-scope asm after `.sdata`.** maspsx stops with `Unable to parse
  .sdata instruction: .pushsection` (or `.if 0`, `.set ...`) when a C `.sdata`
  definition is directly followed by file-scope asm: `INCLUDE_ASM`, `INCLUDE_RODATA`,
  `SDATA_LOCAL` or an `__asm__` of your own. Put remaining `SDATA_LOCAL` lines
  *before* the definitions, and make sure a function (or a `.data`/`.rdata`
  definition) comes between the last `.sdata` definition and the next asm, e.g. by
  defining the variables after the first `INCLUDE_ASM` (egsmem).
* **Splat labels inside a migrated object.** `INCLUDE_ASM` code may address array
  elements through splat's labels (`D_8011EB50` = `D_8011EB40+0x10`). Once the blob
  is gone, define them in the C file under `#ifndef _PSYQ` with
  `__asm__(".set D_8011EB50, D_8011EB40+0x10\n" ...)`, placed after a function
  (directly before the `INCLUDE_ASM` works). If *another* TU's asm uses such a label
  (snddrv's `SoundSeqCallback` indexes from damage's `D_80110170`/`D_80110178`), the
  symbol also needs a `.globl`.
* **Data only an `INCLUDE_ASM` function uses.** A literal whose first use is in an
  asm function (egsdev `"%8lX"`, egshmd `"IR ERR"`, save `"s/save.c"`) cannot come
  from C yet: define it as a named static array at the place the literal was emitted
  (`static char D_8010FEF8[5] = "%8lX";`). A `static const T x;` tentative
  declaration early in the file and the initialised definition later is fine.
* **Small arrays.** An array of <= 8 bytes is `.sdata` too, so anything in `.data`
  is > 8 bytes (damage's 2-entry bone list is one object with what follows it).
* **`.lcomm` layout.** aspsx 2.77 aligns each `.lcomm` to its size rounded up to a
  power of two (at most 16) and packs them; cc1psx emits block-scope statics when
  their function is compiled and file-scope ones at the end, in definition order.
  Some TUs have small `.sbss`/`.bss` items 4 bytes apart that no such layout gives
  (egsdev's `.sbss`/`.bss`, damage's two u8 at 0x80110180/4, cobj's 0xBA4-byte
  object at 0x8011EBAC): their declarations must have been different (unknown);
  leave them in the blob with `SDATA_LOCAL` for now.
* **Blob pieces start where the linker puts them.** An object's section is only
  4-aligned in the final image, so the blob piece after a dotted entry starts at the
  C section's end (damage `.bss` ends at 0x8011EB04, the next piece is
  `bss_8011EB04`), not at the next referenced symbol.
* **`include/scratch.h`** still has no `_PSYQ` variant, so `psyq_check_tu.py` cannot
  assemble TUs that include it (egshmd).
  egsmath's. Another: sprite's texture-name strings sat inside joystick.c's
  `INCLUDE_RODATA` blob (fixed: they are sprite.c's whole `.rdata`).
* **`.lcomm` order is first-declaration order.** cc1 emits the uninitialised statics in
  the order their names were *first declared*, and an `extern` in a header counts.
  (system.c: `mExeTickTimers`, then the queues, follow system.h's extern order.)
  cc1 2.7.2 accepts a `static` definition after a header's `extern` declaration.
* **aspsx aligns every `.lcomm` to its size rounded up to a power of two, at most 16**
  (real 2.77; maspsx's `POWER_OF_TWO` agrees). So an 8-byte static lands 8-aligned and
  a big array 16-aligned. Several game TUs have big `.bss` arrays at 4- or 8-aligned
  offsets, so they were not separate statics. system.c models its queues and sprite
  buffer as one static struct (with `#define`s for the old names); sprite.c's `.bss`
  is left in the blob for now.
* **maspsx cannot parse `.pushsection` right after a `.sdata` definition.** An
  `INCLUDE_RODATA` directly after the data block fails with "Unable to parse .sdata
  instruction". Turn the `{1, 1, 1, 0}` blob into a C const (as stream.c does), or
  move the include above the data.
* **Interior addresses.** When asm (or another TU's C) names an address inside data
  that is now defined in C (`D_800FCB14` = `&gSysMsgTable[0].handle`, `D_80105E10`
  inside `gMultiTextures`, the `.sbss`/`.bss` start symbols used by lib_sn), splat no
  longer defines the label. Add it as an absolute symbol in `config/main.syms.ld`.
* **Pointer tables with string initialisers.** cc1 emits each variable's literals
  right after it, in *reverse* element order, the short ones into `.sdata` and the
  long ones into `.rdata` (level.c's `gLevelNames`, sprite.c's texture tables). Two
  `.sdata` pointers whose strings appear reversed are one 2-element array.
* **Don't retype another TU's extern to reach a migrated struct.** movement.c reads
  gravity y/z as the scalars `D_800FE5E8/EC`. As struct members, cc1 hoists the base
  address into a register and the code changes, so those labels stay as absolute
  symbols in `config/main.syms.ld`.

## Finding the TU's data

```
tools/data_owner.py egstex             # every data address egstex's .text touches
tools/data_owner.py --section .sdata   # a whole section, each address with its users
```

Each lui/lo and `$gp` access in the original `.text` is attributed to the TU whose
code contains it. A TU's data is the contiguous run that only that TU touches,
lying between its neighbours' runs in link order. Unreferenced items at the edges,
such as stream's `cdInt` or the two `""` strings after "T", need judgement. Note
the choice in a comment.

## Placing it: splat yaml

The C object supplies a section through a *dotted* subsegment named after the TU,
exactly like the existing `.rodata` entries. The splat data blob is split around it,
and each leftover piece is named after its start (rom offset for data, vram for bss):

```yaml
      - [0xED0E4, data, data]
      - [0xF72D8, .data, stream]          # stream.c.o(.data)
      - [0xF72EC, data, data_F72EC]
      ...
      - [0x100710, .sdata, egsdebug]      # egsdebug.c.o(.sdata)
      - [0x100720, .sdata, egstex]
      - [0x100728, sdata, sdata_100728]
      - { start: 0x100768, type: sbss, vram: 0x8010FF68, name: sbss }
      - { type: .sbss, vram: 0x80110220, name: egsanim }
      - { type: sbss, vram: 0x80110228, name: sbss_80110228 }
      - { type: bss, vram: 0x801104A0, name: bss }
      - { type: .bss, vram: 0x80122A38, name: egstex }
      - { type: .bss, vram: 0x80122C90, name: egsmisc }
      - { type: bss, vram: 0x80125210, name: bss_80125210 }
```

If a C object has a non-empty section with no dotted entry, splat's automatic sibling
placement drops it at the start of that section, and the sha1 check fails. In that
case, add the missing entry.

`configure.py` runs maspsx with `--use-comm-section`. Without it, maspsx gives each
`.comm` a private copy in the object, and accesses bind to that copy. With it, they
stay real commons and GNU ld resolves them to splat's `.sbss`/`.bss` labels.

## Compiling under ccpsx

`ccpsx` predefines `_PSYQ`. Under `_PSYQ`, `include/common.h` makes `INCLUDE_ASM`,
`INCLUDE_RODATA` and `SDATA_LOCAL` no-ops and skips `include_asm.h`, because aspsx
can't parse `.include`, `.pushsection` or the maspsx markers. `include/scratch.h`
(`.macro`/`.set push`) still needs its own `_PSYQ` variant for the TUs that use it.

## Checklist for one TU

1. `tools/data_owner.py <tu>` gives the address ranges per section. Confirm the
   neighbours and decide how to treat unreferenced edge items.
2. In the C file, replace the `SDATA_LOCAL`/`extern` lines with definitions in address
   order, using initial values from `asm/main/data/*.s`. Keep the line count. Use
   `static` unless the symbol is in `orig/siphon.map`. Write small string literals
   inline instead of `D_` externs.
3. Add tentative definitions for the commons the TU reaches through `$gp`.
4. Add dotted yaml entries and split the blobs. Run `python3 configure.py`, then
   `ninja`, check for `FAILED: [code=`, and run `sha1sum --quiet -c orig/SHA1SUMS.build`.
5. Run `tools/psyq_check_tu.py <tu>`. Every section must be `OK`, with no "unplaced"
   section and no `symbol ... expected` lines.

## Status

| TU       | sections from C                     | modern sha1 | ccpsx (all sections) |
|----------|-------------------------------------|-------------|----------------------|
| egsdebug | .rdata .text .sdata                 | OK          | OK |
| egstex   | .rdata .text .sdata .bss            | OK          | OK |
| egsmisc  | .rdata .text .sdata .bss            | OK          | OK |
| egsanim  | .rdata .text .data .sdata .sbss     | OK          | OK |
| stream   | .rdata .text .data .sdata + commons | OK          | OK |
| egsmem   | .sdata                              | OK          | data OK (EGSMemInit asm) |
| egsfile  | .sdata .bss                         | OK          | data OK (EGSFileOpenFog asm) |
| egshog   | .rdata .text .sdata                 | OK          | OK |
| egsdev   | .rdata .data .sdata (not .sbss/.bss) | OK         | data OK (EmergencyPrintNum asm) |
| egshmd   | .data .sdata + gInfraredColor common | OK         | blocked by scratch.h |
| collision| .rdata .bss                         | OK          | data OK (2 asm functions) |
| shadow   | .rdata + mShadowRequestList common  | OK          | .rdata short until its asm functions are C |
| save     | .rdata .sdata .bss                  | OK          | data OK (SaveRestoreContinue asm) |
| damage   | .rdata .data .sdata .sbss .bss (not 0x80110180/4) | OK | data OK (2 asm functions) |
| main     | .sdata                              | OK          | data OK (asm MainLoop) |
| system   | .data .sdata .sbss .bss + commons   | OK          | data OK (asm functions) |
| game     | .rdata .text .data .sdata .bss      | OK          | OK with a `_PSYQ` scratch.h (see below) |
| level    | .rdata .data .sdata + commons       | OK          | data + .rdata OK (asm functions) |
| camera   | .data .sdata (.bss: boundary with gamecam unclear) | OK | data OK (asm functions) |
| physics  | .data                               | OK          | data OK (asm functions) |
| playcas  | .rdata .text .data                  | OK          | OK |
| sprite   | .rdata .text .data .sdata (.bss still in the blob) | OK | OK |
| movement | .sdata                              | OK          | data OK (asm functions) |

path and los have no data of their own (los only reaches commons).

The TUs that include `include/scratch.h` (game, system, camera, los, movement, ...)
don't compile under ccpsx yet: its `scratch_jal_ds` GAS `.macro` needs a `_PSYQ`
form, e.g. `".set noreorder\n\tjal " #fn "\n\t" ds "\n\t.set reorder"`. With that,
game.c matches in every section.
