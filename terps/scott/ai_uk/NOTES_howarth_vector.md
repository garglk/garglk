# Howarth (Mysterious Adventures) vector graphics — accuracy notes

Renderer: `ai_uk/line_drawing.c` (`DrawHowarthVectorPicture`). Format is Brian
Howarth / Digital Fantasia "Mysterious Adventures" line-drawing vectors
(`HOWARTH_FORMAT`, picture format 99): a stream of MOVE_TO / draw-line / FILL /
END opcodes over a 256×96 region (the top 12 character rows of a ZX `SCREEN$`).

## Status — line rasterizer and fill are now ROM-exact (resolved 2026-06)

Reverse-engineered the original Digital Fantasia ROM from the ZX Spectrum
*Golden Baton* `.z80` snapshot in Ghidra (loaded as a raw 64K Z80 image; the
interpreter's picture engine lives at `ram:0x6be2`). The two routines that
governed accuracy were re-implemented to match the ROM pixel-for-pixel:

1. **Line rasterizer** `scott_linegraphics_draw_line()` (ROM `0x6c06..0x6ca3`).
   The old code was a generic Level9-derived Bresenham; the ROM uses a *centred*
   Bresenham that differs in three ways that each move pixels by ~1px:
   - error initialised to `major/2` (round-to-nearest), not `2*minor-major`;
   - the loop runs exactly `major` times, stepping the major axis every
     iteration and plotting the **new** position, so the **start point is not
     plotted** (the previous segment's end already covers it) and the end is;
   - a zero-length segment (`dx==dy==0`) plots nothing; ties (`dx==dy`) are
     x-major.
   Verified **byte-identical to a 1:1 asm translation** of the ROM, and
   **1740/1740 (100%)** of the drawn line pixels land on the real Spectrum
   bitmap for Golden Baton's opening room (was ~99% / 17 stray px before).

2. **Fill clamp** `diamond_fill()` (ROM `0x6cee/0x6cf9/0x6d04/0x6d0f`). The ROM
   flood (a 4-connected, double-buffered BFS "plot-if-clear", `0x6d20`) never
   grows into the top screen row or the extreme edge columns — the picture
   border is an implicit wall. The ROM-exact lines correctly plot nothing in
   row 0, so a naïve flood **escaped along the top edge and flooded the whole
   image** (the old Bresenham only stayed contained because its misplaced pixels
   happened to seal row 0). Clamping the flood to the ROM interior
   (`x∈[1,253], y∈[1,CLIPHEIGHT)`) fixes the leak and lifts the fill match from
   95.6% → **99.0%**.

End-to-end colour match vs a real Spectrum `SCREEN$` (ZXOPT palette): **95.26%**
(was 94.11%). See ground-truth method and the remaining ceiling below.

## Remaining ceiling: ZX attribute clash (inherent, ~5%)

The residual mismatch is **not** a rasterizer bug — it is the ZX Spectrum's 8×8
attribute model. A white outline crossing a red-filled cell displays as **red**
on real hardware (one ink per 8×8 cell), but this per-pixel renderer draws it
white. Of Golden Baton room 0's ~1148 mismatched pixels, ~1072 are correctly
*placed* line pixels that simply clash to the cell's fill colour. Reproducing
clash would require an 8×8 attribute pass and would make Spatterlight's output
*worse* (clashy) for users, so it is deliberately not done — this is the same
kind of inherent ceiling as the C64 colour-RAM tests, not a defect to chase.

## Ground-truth method (for re-verification / new rooms)

1. Decode the `.z80` to a raw 64K image (Z80 v2/v3 RLE) and import as
   `z80:LE:16:default` in Ghidra; the engine dispatcher is at `ram:0x6be2`.
2. `build/zxextract <game.z80> <dir> <picnum>` dumps the `LineImages[]` vector
   stream (`pic<NNN>.dat` + `meta.txt`).
3. `mame spectrum -snapshot <game.z80>`, press a key to leave the title, then
   dump `SCREEN$` (`0x4000..0x5aff`) over the Lua bridge; decode with the ZXOPT
   palette to a `C64C2` golden (256×96).
4. `build/zxvectortest grid|cmp <dir> …`. For a *geometry* check (clash-immune),
   compare the renderer's plotted pixels to the real bitmap's set bits — the
   lines should be 100%.

## Not committed
No `.zxvec` golden is committed: an exact-colour golden can't reach 100% because
of attribute clash, and `make zxtest` hard-fails below 100%. The harness
(`zxvectortest`) and this capture recipe stay for future investigation. If a
regression guard is wanted, add a *set-bit geometry* assertion (lines == 100%)
rather than an exact-colour golden.

## Pointers
- Renderer: `ai_uk/line_drawing.c`; opcodes `MOVE_TO 0xc0`, `FILL 0xc1`,
  `END 0xff`, anything else = draw-line-to (the opcode byte is the Y).
- ROM anchors (Golden Baton): dispatcher `0x6be2`, line `0x6c06`, fill driver
  `0x6ca6`, fill BFS/plot-if-clear `0x6cdd`/`0x6d20`, end `0x6d79`.
- Harness: `saga/test/zxvectortest.c`, `saga/test/zxextract.c`.
- Method memory notes: `unquill-illustrator-renderer`,
  `saga-apple2-groundtruth`, `saga-atari8-groundtruth`, `scott-howarth-vector-re`.
