# terps/aiukgraphics

The ZX Spectrum / C64 picture library shared by `terps/scott` (including
`ai_uk/` and `ti994a/`) and `terps/taylor`. Three parts, 1484 lines:

| File | What |
|---|---|
| `irmak.c/.h` | tile decoder and compositing buffer for the Adventure International UK picture format (versions 0-4) |
| `taylordraw.c/.h` | the TaylorMade / Seas of Blood draw-opcode interpreter |
| `aiukpalette.c/.h` | Spectrum, C64 and VGA palettes and the colour remap tables |

It has no build file and no tests of its own: it is compiled into scott and
taylor by the Xcode project and by `scott/Makefile.headless`,
`taylor/Makefile.headless`, `scott/saga/test/Makefile` and the two terps' test
probes.

## The suites

```
cd terps/taylor && make -f Makefile.headless imagecheck mamecheck scriptcheck
cd terps/scott  && make -f Makefile.headless formatcheck check
cd terps/scott/saga/test && make zxtest scenetest titest
xcodebuild -project ../../Spatterlight.xcodeproj -scheme Spatterlight build
```

taylor `imagecheck` hashes every picture of every TaylorMade game; `mamecheck`
compares 538 rooms against MAME captures of the Spectrum originals (the
captures from commit eb035c603). scott `formatcheck` and the saga `zxtest` /
`scenetest` / `titest` targets draw the AI UK pictures (Spectrum, C64, TI-99/4A)
and compare hashes. Run them all before and after anything that touches the
opcode interpreter or the decoder; the pass of October 2026 changed no golden.
See `taylor/test/README.md` and `scott/saga/test/`.

## Not covered

- Corrupt or truncated picture data: there is no malformed-input fixture. The
  bounds checks below were tested by hand and by the old-vs-new differential
  probe, not by a suite row.
- Flips whose x + width runs past column 31 (wrap into the next row): no
  corpus stream does this, and the in-place swap gives a different garbage
  picture than the old scratch-buffer code did.
- Buffer mode with a version 0-2 picture: no game does it. The old code stored
  the unconverted colour byte four columns off from the pixels; the new code
  converts it.

## Behaviour the pass of October 2026 settled

- **Operand orders differ by opcode pair and are correct as written.** 0xF2 and
  0xF1 take (y, x, ...), 0xEE and 0xEC take (x, y, ...); checked against the
  originals, He-Man room 32 (0xF2) is MAME-identical. The order stands in the
  opcode enum in `taylordraw.c`.
- **`mirror_area_vertically` keeps its literal loop**, not the general area
  routine; see the comment at the function (He-Man room 32).
- **The whole-image opcodes are the area routines over the full 32x12 area**:
  0xF3, 0xED, 0xFA, 0xFE. Flips and mirrors are in-place pair swaps, no scratch
  buffers.
- **Recursion (0xF9) stops at depth 8** (`MAX_DRAW_DEPTH`); the corpus nests
  three deep at most. A self-referencing stream used to overflow the stack.
- **Operand counts are checked once per opcode** (`operand_count()`), with the
  Rebel Planet 0xF7 test in front of it because the probe relies on
  DrawTaylor's return value. `TAYLOR_OLDER` changes the 0xFC and 0xF8 counts.
- **Truncated tile data aborts the picture**: `PerformTileTransformations`
  returns failure, stages 2 and 3 are skipped and `DrawPictureAtPos` returns 0.
  The old code painted tile bytes as colours.
- **`last_colour` is file scope and deliberately not reset between pictures**:
  the originals rely on the colour carrying over.
- **Attribute decoding happens once** into a private `attributes[]` array in
  the version 3+ format (version 0-2 colours are converted); stage 3 paints
  directly or copies into `imagebuffer`. Direct mode leaves `imagebuffer`
  alone; Robin's waterfall reads `layout[]` after a direct draw.
- **Remap of a colour above 15 gives 0.** `INVALIDCOLOR` is gone.
- **Palette families**: VGA, ZX and ZXOPT set white 15 / blue 9; every C64
  variant and `NO_PALETTE` fall through to the C64 branch of `DefinePalette`
  with 1 / 6. ZXOPT's bright red is its own 0xff0014. C64C differs from C64B
  only in non-bright red (2 instead of 9).
- **Game colours live with their games**: `dice_colour` in
  `scott/ai_uk/seas_of_blood.c`, `WhiteColour()` / `BlueColour()` in
  `scott/ai_uk/game_specific.c`, the Robin lightning palette swap in
  `robin_of_sherwood.c`. `aiukpalette.h` exports `Remap`, `DefinePalette`,
  `pal[]` and `palchosen` only.
- **`last_image_index` is set by the hosts** (scott_display.c,
  robin_of_sherwood.c), not by the library.
- **Flip and RotateTile** are bit-reverse, row-reverse and transpose; compared
  against the old code on 60000 random tiles, identical. `PixelSet(row, col)`
  replaces the `isNthBitSet(x, 7 - j)` idiom everywhere.
- **No diagnostics.** The stderr and debug_print messages were removed; with
  them on, only the garbage draw instructions past the last real room of the
  TaylorMade games printed anything, and the return values say the same.

## Declined, with the reason

- Prefixing `Image`, `INK_MASK`, `PAPER_MASK`, `BRIGHT_FLAG` with IRMAK_: churn
  in every host for no gain while nothing else defines those names.
- RectFill / PutPixel through a vtable set by `InitIrmak`: only worth it with a
  second back end, and none is planned.
- A `ShutdownTaylor()` and a single-pass `InitTaylor`: the double scan runs
  once per game over a few KB, and a shutdown would have no caller.
- Routing direct mode through the compositing buffer: it would redraw the
  whole frame and clobber what other callers composite into. Solved instead by
  the single attribute decoder above.
