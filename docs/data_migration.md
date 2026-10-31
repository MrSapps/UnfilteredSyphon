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
* **`.lcomm` layout: the original assembler word-aligns every `.lcomm`.** aspsx 2.77
  (psyq_4.3/bin) packs `.lcomm`s by size (alignment = size rounded up to a power of
  two, at most 16), but the exe's static `.sbss` has 1- and 2-byte items 4 bytes apart
  (particle's ten s16 at 0x801100F0..0x80110114, all `lh`/`sh`; damage's two u8 at
  0x80110180/4; egsdev). `/home/snake/dev/psyq_sdk/aspsx/2.56/aspsx.exe` gives every
  `.lcomm` 4-byte alignment and pads its size to 4 (no 16-alignment in `.bss` either),
  which reproduces particle's layout; it also gives the same `.text` and data for every
  TU checked before (egsdebug..damage). So the game was most likely assembled with an
  aspsx that has 2.56's `.lcomm` rule. To check with it, use an SDK dir whose `bin/` is
  psyq_4.3's with `aspsx.exe` swapped for 2.56's, and set `PSYQ_SDK` to it for
  `tools/psyq_check_tu.py` (particle's `.sbss` only passes that way). Packed small
  items in the exe (system 0x8010FF6A, interface 0x8010FFFA, text 0x801101AA, ...) must
  then be arrays or struct members.
  The modern build still runs maspsx with `--aspsx-version=2.79` (power-of-two
  layout). Until maspsx is switched to word `.lcomm` alignment (`LcommAlignment.WORD`;
  maspsx maps every version below 2.8 to `NONE`, which is wrong for 2.56), a TU with such
  slots needs a C form that gives the word layout under 2.77/maspsx too
  (`psyq_check_tu.py` uses 2.77). What works (2026-10-31):
  * **Padding statics** (particle.c `SBSS_PAD1/2/3`, now in both builds; define
    `LCOMM_WORD` to drop them for a word-aligning aspsx). The slot keeps its scalar type.
    A scalar matters: cc1 2.7.2 keeps a non-aggregate static in a register across
    stores through pointers, but reloads an array element or struct member after each
    one (particle's `ParticleTaserGetDestination` re-reads `D_801100E4` otherwise).
  * **Word-sized arrays** behind a `#define x (x[0])` (egsdev `D_80110204`, `D_80110208`,
    damage `D_80110180/4`), where no such reload difference shows up.
  * **One block for `.bss` arrays.** A struct whose first member is at offset 0
    (cas.c `D_80115638`), or, when the other symbols are indexed in loops, a hand-laid
    block: under `_PSYQ`, `__asm__("\t.bss\nsym:\t.space\tN\n...\t.text")` placed
    after a function (aspsx has no equates, `.set x,y` or `x = y`), and in the modern
    build a static array plus `.set` aliases (egsdev `D_80122678`/`gControllers`/...).
  cobj's object at 0x8011EBAC has not been tried yet.
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
* **String literals of a table of structs come out in element order** (weapon.c's
  `gWeaponInfo` names), unlike a plain pointer array's (reverse, player.c's
  `gGabeAnimFiles`). cc1 shares identical literals within a file, so a table that
  points at an earlier literal (weapon's `""`) needs that literal as a named array.
* **An array at an offset inside a migrated object.** Where C indexed a splat label
  inside another object, neither `&s.a[1]` nor `s.a[i + 1]` gives the original's
  per-store `sym($reg)` addressing. Check the boundary first: cas.c's "hip table + 4"
  was really the start of cas's `.bss` (0x80115638), with 0x80115634 only reached as
  `hip[frame - 1]` (see the 8-alignment rule below).
* **`.sbss`/`.bss` contributions start 8-aligned** (docs/psyq_build.md, psylink).
  `gen_psylink.py` warns when a C object's `.sbss`/`.bss` yaml start isn't. Both cases
  found were wrong boundaries: target's `.sbss` starts at 0x8010FFD8 because
  func_80031AEC..func_800323C4 (the target scoring actions, previously in sniper.c)
  are target.c's; cas's `.bss` starts at 0x80115638.
  Plain field symbols are different: joystick.c's `D_801056CC` etc. became
  `#define D_801056CC (D_801056C8.mode)` with identical code.
* **Don't retype another TU's extern to reach a migrated struct.** movement.c reads
  gravity y/z as the scalars `D_800FE5E8/EC`. As struct members, cc1 hoists the base
  address into a register and the code changes, so those labels stay as absolute
  symbols in `config/main.syms.ld`.
* **Word-aligned `.bss` items under the modern build.** Padding can't make maspsx's
  power-of-two layout put a 16-byte item at an 8-aligned address (npc's three vectors
  at 0x8011E8A8, after a 0x18-byte array). npc.c gives the modern build one bigger array
  and names the vectors with `.set` (`#ifdef _PSYQ` keeps the real statics).
* **maspsx adds a stray `nop` before an indexed access to a small static**
  (`lbu $2,D_80110128($2)` right after a load into `$2`; the real aspsx 2.56/2.77 emits
  `lui/addu/lbu` with no nop). npc.c names the storage differently for the modern build
  and aliases the old name with `.set`, so maspsx treats it as extern.
* **psylink's 8-aligned `.sbss`/`.bss` starts can hide a 4-byte tail.** The yaml range
  of npc's `.sbss` is 0x58 bytes but the C object is 0x54; psylink 8-aligns damage's
  start, GNU ld does not, so the modern build gets a 4-byte pad static.
* **The modern link packs input sections with `SUBALIGN(4)`.** So the alignment maspsx
  gives an object's first `.lcomm` doesn't matter, but psylink's 8-aligned starts do: a
  4-byte gap before or after a TU's `.bss` stays a 4-byte splat piece (sound:
  `bss_8011EB04`, `bss_8011EB3C`). A pad static can't do it in `.bss` (4 bytes is `.sbss`).
* **A struct of two u8 in `.sdata` is word-aligned** (DATA_ALIGNMENT), so two packed
  bytes at an odd-ish offset (snddrv 0x8010FD82/83) are two `u8` variables.
* **Zero-filled `.data` arrays need an initialiser** (`= { 0 }`), otherwise they are
  `.bss`/`.sbss` (pickup's vector at 0x80107840, snddrv's per-SEQ tables).
* **A scalar pointer's string literal comes out *before* the pointer.** cc1 outputs the
  literal while it reads the initialiser, then the variable, so `char *p = "health";`
  gives `"health"` in `.sdata` followed by `p` (strings.c; a long literal goes to
  `.rdata` instead, in the same order). Tables still give reverse element order.
  Identical literals are shared.
* **`include/scratch.h`** has a `_PSYQ` variant of `SCRATCH_CALL_DS`: aspsx has no
  `.macro` or `.set push/pop`, and without maspsx in between, a plain
  `.set noreorder`/`.set reorder` bracket gives the same bytes (checked on game.c's
  `func_80016690`). TUs that include it (egshmd, particle, animate, game, ...) now compile
  under ccpsx.

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
has its own `_PSYQ` variant of the one GAS macro it used.

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
| egsdev   | .rdata .data .sdata .sbss .bss      | OK          | OK |
| egshmd   | .data .sdata + gInfraredColor common | OK         | data OK (2 asm functions) |
| collision| .rdata .bss                         | OK          | data OK (2 asm functions) |
| shadow   | .rdata + mShadowRequestList common  | OK          | .rdata short until its asm functions are C |
| save     | .rdata .sdata .bss                  | OK          | data OK (SaveRestoreContinue asm) |
| damage   | .rdata .data .sdata .sbss .bss      | OK          | OK (npc.c's last function func_80066714 uses its static 0x80110180/4: probably damage.c's) |
| main     | .sdata                              | OK          | data OK (asm MainLoop) |
| system   | .data .sdata .sbss .bss + commons   | OK          | data OK (asm functions) |
| game     | .rdata .text .data .sdata .bss      | OK          | OK with a `_PSYQ` scratch.h (see below) |
| level    | .rdata .data .sdata + commons       | OK          | data + .rdata OK (asm functions) |
| camera   | .data .sdata (.bss: boundary with gamecam unclear) | OK | data OK (asm functions) |
| physics  | .data                               | OK          | data OK (asm functions) |
| playcas  | .rdata .text .data                  | OK          | OK |
| sprite   | .rdata .text .data .sdata (.bss still in the blob) | OK | OK |
| movement | .sdata                              | OK          | data OK (asm functions) |
| cas      | .rdata .data .sdata .sbss .bss      | OK          | OK |
| object   | .rdata .data .sdata .sbss           | OK          | OK |
| sniper   | .rdata .data .sdata .sbss .bss (not 0x80115754-60) | OK | OK |
| target   | .rdata .data .sdata .sbss           | OK          | OK |
| joystick | .rdata .data .sdata .sbss (not 0x8010FFE4) | OK    | OK |
| weapon   | .rdata .data .sdata .sbss (.bss still in the blob) | OK | OK |
| player   | .rdata .data .sdata .sbss + commons (.bss still in the blob) | OK | OK |

path and los have no data of their own (los only reaches commons).

The TUs that include `include/scratch.h` (game, system, camera, los, movement, ...)
don't compile under ccpsx yet: its `scratch_jal_ds` GAS `.macro` needs a `_PSYQ`
form, e.g. `".set noreorder\n\tjal " #fn "\n\t" ds "\n\t.set reorder"`. With that,
game.c matches in every section.
| particle | .data .sdata .sbss .bss + 5 commons (.rdata still has the INCLUDE_RODATA blob) | OK | data OK with aspsx 2.56 (`.sbss` DIFF with 2.77); 16 asm functions |
| npc      | .data .sdata .sbss .bss + 14 commons (.rdata still has the INCLUDE_RODATA blob) | OK | data OK with aspsx 2.56 except `.sbss` 0x54 vs 0x58: the last 4 bytes are psylink's 8-alignment before damage (modern-only pad); 16 asm functions |
| sound    | .data .sdata .sbss .bss + mXABlock common (.rdata keeps its INCLUDE_RODATA strings) | OK | data OK with aspsx 2.56; 4 asm functions |
| cobj     | .sdata .bss + 7 commons (0x8010F9C8/CC left in the blob, owner unclear) | OK | data OK; the "0x8011EBAC" object is node 1 of a 150-node pool at 0x8011EB98 |
| terrain  | .sdata + 10 commons (no .data/.sbss/.bss of its own) | OK | .sdata OK; .sdata defined after the first INCLUDE_ASM (maspsx pitfall) |
| pickup   | .data .sdata .sbss .bss + 4 commons | OK | data OK with aspsx 2.56 except `.bss` 0x8C vs 0x90 (psylink's 8-alignment before save) |
| snddrv   | .data .sdata .sbss .bss + 2 commons (0x8010FD20 left in the blob) | OK | data OK; 5 asm functions |
| egsdisp  | .data .sdata .bss + 13 commons (gLRTick at 0x8010FDA0 left in the blob) | OK | data OK; gHADoneCallback (egshmd's .sdata) gets a tentative definition so it stays $gp |
| strings  | .rdata .data .sdata .bss (message table rebuilt from C initialisers) | OK | .rdata .data .sdata .bss OK (aspsx 2.56 and 2.77); StringContactMeiXingMsg asm |
| particle | .rdata .data .sdata .sbss .bss + 5 commons (3 INCLUDE_RODATA blobs left) | OK | OK (aspsx doesn't auto-align `.word`: a blob after the 13-byte `__FILE__` landed unaligned, so D_80012000/10 are C consts) |

## Overlays (-G0)

Every overlay TU (`src/<ovl>/<ovl>.c`) defines its own `.rdata/.data/.bss`; the yaml
(generated by `tools/gen_ovl_yaml.py`, `DATA_FROM_C`/`DATA_PIECES`) has dotted
`.rodata`/`.data`/`.bss` entries for it. Check with `tools/psyq_check_tu.py <ovl>`.

* With `-G0` there is no small data: initialised variables go to `.data` (`const` to
  `.rdata`), every literal to `.rdata`, `static` uninitialised to `.bss` (`.lcomm`),
  uninitialised globals become commons. psylink allocates an overlay object's commons
  in its renamed `<ovl>.bss`, after the TU's own `.bss` (chopperPos, title's/menu's `m*`
  globals, movie's gHuffmanTable, park2's): they stay a splat `bss` piece
  (`DATA_PIECES`) and C gives a tentative definition.
* Each game overlay's `.rdata` starts with `{0, 1, 1, 1, 0}` (one 5-word const). movie
  and menu start with `"bin/<ovl>.c"` instead, so the overlay sources lived in `bin/`.
* Briefing/objective tables: pointer arrays emit their literals in reverse element order,
  separate pointer variables in definition order; nearly all overlay `.rdata` comes
  from these initialisers.
* **Stray padding.** The 1998 psylink left garbage in section padding (subway2 `.rdata`
  ends in "ram", chopper `.data` in "lisio", park has 0x8F). psylink pads with zeros
  now, so these bytes are 1-byte statics after the last item; the checker rejects
  nonzero padding the object doesn't supply. The last `.bss` of an overlay must have
  the map's exact size (subway: a 2-byte pad), because the overlay file ends there.
* **`.lcomm` alignment.** s16/u8 statics 4 apart get explicit pad statics in both builds
  (`BSS_PAD2/3`); arrays over 8 bytes at 4/8-aligned offsets can't be separate statics
  under aspsx 2.77 or maspsx (16-aligned), so they became structs (cave, title, menu
  splits) or, where that changed codegen, storage blocks with the original labels as
  externs: `.set` in the modern build, psylink equates under `_PSYQ` (museum, subway,
  chopper; park2's `.bss` is a file-scope asm block). These are FIXMEs for a build that
  has no modern ELF to take equates from.
* `config/symbols/<ovl>.txt` had names from other overlays (shared addresses);
  the wrong ones were dropped. `config/symbols/overrides.txt` still assigns
  church2Briefings/catacombsBriefings to park, mFileBuffer to baseext and
  mFlamethrowerEndNormal to subway (they belong to levspec, title and park2).
