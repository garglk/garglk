# terps/common_sagadraw

The picture renderers shared by `terps/scott` (`saga/`, `ai_uk/`), `terps/plus`
and `terps/comprehend`. 1447 lines:

| File | What | Used by |
|---|---|---|
| `apple2draw.c/.h` | Apple II SAGA images: RLE decode into a hi-res page, scrambled images, the artifact-colour line renderer with flip | scott, plus |
| `c64a8draw.c/.h` | C64 and Atari 8-bit SAGA images: RLE decode, colour mapping, `enum c64a8_variant` | scott, plus |
| `atari8_palette.h` | the 256-entry Atari 8-bit palette, unmodified | c64a8draw |
| `pcdraw.c/.h` | DOS `.PAK` CGA images | scott, plus |
| `gm_vector.c/.h` | Graphics Magician cursor, line and brush code, 6502 write for write | scott `apple2_vector_draw.c`, comprehend |
| `gm_artifact.c/.h` | NTSC artifact colours for hi-res and double hi-res lines, `gm_apple2_palette` | apple2draw, comprehend |
| `gm_hires.h` | page size, rows, columns, `gm_row_address`, `gm_row_from_offset` | all of the above, scott, comprehend |
| `sagadraw_host.h` | the host globals and callbacks (`SetColor`, `PutPixelWithWidth`, `Graphics`, `pixel_size`, ...) | the three library files and the files that define them: `scott/saga/sagagraphics.c`, `scott/scott.c`, `plus/graphics.c`, `plus/plusmain.c` |

No build file and no tests of its own. Xcode finds these headers through the
`common_sagadraw` group in the project file, not through a search path, so a
new header must be added to that group.

## The suites

```
cd terps/scott/saga/test && make groundtruth dostest apple2test c64test c64a8test titest zxtest scenetest
cd ../..               && make -f Makefile.headless check formatcheck
cd ../comprehend       && make test
cd ../plus             && make -f Makefile.headless imagecheck scriptcheck synthcheck
xcodebuild -project ../../Spatterlight.xcodeproj -scheme Spatterlight build
```

All of them compare against hardware-derived goldens byte for byte
(groundtruth 319/319 at the time of the pass). Each suite links its own
subset of the library and records pixels into its own grid through stub
host callbacks; `c64test` keeps a copy of `DrawPatternAndAdvancePos` because
it does not link `c64a8draw.c`, and the copies of the row-address formula in
`comprehend/test/` are independent oracles on purpose.

Every `.c` here should build with `-Wall -Wextra -Wshadow -Wpedantic`.

## Not covered

- Malformed data and count mode (`APPLE2_COUNT`, `C64A8_ATARI8_COUNT_VOODOO`):
  no suite row reaches them, because every Count disk in the corpus is
  scrambled. The bounds fixes of October 2026 were checked with a throwaway
  differential harness (old build against new, every real blob plus four
  fuzzed copies of each, 13270 runs, also under ASan/UBSan), not by a suite.
- Rows 160-191 of an Apple II page: the vector games draw there, the Apple
  hides it behind the text window, and this code never draws it.

## Behaviour the pass of October 2026 settled

- **Apple II pictures stop at scanline 159** in both orientations
  (`row < MIN(ImageHeight / 2, 160)`). Row 160 used to show as a stray line
  on title screens. The 15 Plus Apple II image goldens lost two plot rows
  each; no Scott golden draws this path.
- **The artifact line renderer draws each colour run where it belongs**: a
  flat scan over `colors560[]`, one fill_rect per run. The old loop drew
  every run but the first one pixel to the right and lost single-pixel runs.
  Checked pixel for pixel against MAME `apple2e` page dumps of Spider-Man's
  title and menu; the DHGR kernel against the OO-Topos title with MAME's
  composite "Pixel-run (color bias)". Upside-down drawing is an exact 180
  degree mirror at every `pixel_size`.
- **Brush indices and brush coordinates wrap like the 6502's bytes.** Brush 8
  and up wrap onto 0..7; `brush_quadrant` scanline and column wrap at the
  edges. The hardware captures show it, so there is no clamp. Comments at
  both places say so.
- **C64 and Atari colours are the original interpreters' algorithm**: two
  16-entry hue tables picked by luminance, with Claymorgue's variant
  (`C64A8_C64_CLAYMORGUE`), and the Atari palette looked up with
  `value & 0xfe` because the GTIA ignores luminance bit 0. The hand-picked
  readability fixes are a short per-interpreter override list in
  `c64a8draw.c`, matching exact values only. Nothing is left "unchanged" any
  more; values the old map left stale ($E4, $F1, $F3, $F9, $FD) get their
  original colours.
- **`enum c64a8_variant`** (`C64A8_ATARI8`, `C64A8_ATARI8_COUNT_VOODOO`,
  `C64A8_C64`, `C64A8_C64_CLAYMORGUE`) replaces the `is_c64` and
  `voodoo_or_count` flags; the renderer derives counted RLE and the palette
  from it. Bounds callbacks take `struct c64a8_bounds *`. The Apple callback
  stays separate: read-only, carries `top` not `left`, and only tells Plus
  the image size.
- **`DrawPatternAndAdvancePos` is void**: four double-width pixels on one
  row, `*y` moves down a row. `scott/saga/c64_small.c` calls it too.
- **`DrawDOSImageFromData(ptr, datasize)`** takes the buffer length, returns 0
  for NULL or data shorter than the header, and clamps the declared size to
  the buffer. Only the empty B019.PAK changed among the 286 real PAKs. The
  four CGA palette slots are set on every draw on purpose: Scott and Plus
  share slots 0..3 with the other renderers.
- **Scrambled Apple II writes are skipped, not redirected to row 0**:
  `DescrambleScreenAddress` returns -1 on error and `WriteScrambledByte` drops
  writes that would leave the page. `DrawSingleApple2ImageByte` rejects the
  8-byte screen holes. `repetitions == 255` gives 256 iterations like a 6502
  counter.
- **`gm_apple2_palette` entries 5 and 10 are both 0x808080**: two greys,
  authentic. Bocfel's Apple II table is a different renderer's and stays.
- **`gm_set_color` leaves the colour unchanged for an index of 8 or more.**
- **Style is WebKit** (function braces on their own line, 4-space
  continuation, `//` comments except the `/* 0 */` index markers). The
  formatting pass compiled to byte-identical objects; the one `uint8_t` loop
  counter in `gm_spread7` was left because `int` compiles differently.

## Declined, with the reason

- One RLE pair iterator for the Apple and C64 decoders: about 20 shared
  lines, and the two loops leave at different points; reproducing both
  exactly risks more than the duplication costs.
- A shared `test/sagadraw_stubs.c`: each harness links a different subset and
  records into its own grid; it would need weak symbols or per-test ifdefs.
- An `Apple2ScreenState` struct or a host-context struct instead of
  `screenmem` and the host globals: there is only ever one Apple screen, and
  `sagadraw_host.h` now states the contract once with compiler checking.
- A `common_sagadraw/test/` of its own: the consumer suites already compare
  every entry point against hardware-derived goldens.
- A `sagadraw_*` prefix with wrapper names: doubles the API for a while and
  gains little.
- `gm_render_line_colors_phase()`: both public renderers are one-line
  wrappers over the same kernel and every caller knows its mode.
- Naming the Applesoft HMASK literals `0xc0`, `0x81`, `0x7f`: names would
  hide the bit patterns.
