//
//  irmak.c
//  Drawing code for Adventure International UK / Adventure Soft graphics
//  Used in ScottFree and TaylorMade
//
//  Created by Administrator on 2025-12-07.
//
//  Based on Sagadraw v2.5 by David Lodge
//  with help from Paul David Doherty (including the colour code)
//
//  Original code at https://github.com/tautology0/textadventuregraphics
//  See also http://aimemorial.if-legends.org/gfxbdp.html
//
//  The Irmak graphics system renders tile-based images used in
//  Adventure International UK / Adventure Soft text adventures.
//  Each image is stored as a stream of tile references with
//  transformation commands (rotation, flip, overlay compositing),
//  followed by colour attribute data (ink/paper pairs per tile cell).
//
//  Rendering proceeds in three stages:
//    1. PerformTileTransformations — decode the tile stream, apply
//       rotations/flips/overlays, and write the result into layout[][].
//    2. DecodeAttributes — decode the RLE-compressed colour stream
//       into attributes[], one Spectrum-style attribute per cell.
//    3. DrawDecodedImage — rasterise the layout+attributes either
//       to the screen directly, or into imagebuffer for later compositing.
//
//  Two rendering paths exist: "direct" mode renders each image
//  independently to the screen; "buffer" mode composites multiple
//  sub-images into the shared imagebuffer[][] before a final
//  DrawIrmakPictureFromBuffer() renders the whole frame.
//

#include <string.h>

#include "glk.h"
#include "aiukpalette.h"

#include "irmak.h"

/* Command byte layout (when COMMAND_BIT is set):
     Bit 7 (0x80): COMMAND_BIT — distinguishes commands from solo tile indices
     Bit 6 (0x40): FLIP_BIT — horizontal mirror
     Bits 5-4 (0x30): ROTATE_BITS — 0/90/180/270 degree rotation
     Bits 3-2 (0x0C): OVERLAY_BITS — compositing mode (OR/AND/XOR) and
                       signals that an overlay chain follows
     Bit 1 (0x02): REPEAT_BIT — next byte is a repeat count
     Bit 0 (0x01): ADD_128_BIT — add 128 to the tile index (for tiles 128–255)
   OVERLAY_MASK (0xF3) masks out the overlay bits, passing through
   rotation/flip for Transform(). */
#define COMMAND_BIT      0x80
#define FLIP_BIT         0x40
#define ROTATE_BITS      0x30
#define ROTATE_SHIFT     4
#define OVERLAY_BITS     0x0C
#define OVERLAY_MASK     0xF3
#define REPEAT_BIT       0x02
#define ADD_128_BIT      0x01

/* Overlay compositing modes (values of OVERLAY_BITS) */
#define MODE_XOR         0x0c
#define MODE_AND         0x08
#define MODE_OR          0x04

/* Colour attribute masks for version 0–2 format.
   Version 3+ masks (INK_MASK, PAPER_MASK, BRIGHT_FLAG) are in irmak.h. */
#define OLD_PAPER_MASK   0x07
#define OLD_INK_MASK     0x70
#define V2_BRIGHT_FLAG   0x08

/* One picture on its way through the three decoding stages. */
typedef struct {
    const uint8_t *dataptr; /* next unread byte of the image data */
    const uint8_t *dataend;
    int xsize;
    int ysize;
    int xoff;
    int yoff;
    int imagesize; /* xsize * ysize, clamped to IRMAK_IMGSIZE */
} IrmakImgContext;

Image *images = NULL;
int number_of_images;
int image_version;

/* The last colour byte read by DecodeAttributes(). Kept from one picture
   to the next, as in the original: colour data that starts with a repeat
   (see the overlong run in PerformTileTransformations()) repeats the last
   colour of the picture drawn before. So this is deliberately not reset
   by InitIrmak() or between pictures. */
static uint8_t last_colour = 0;

/* Called from SagaGraphicsSetup() after the game data has been parsed. */
void InitIrmak(int numimg, int imgver) {
    number_of_images = numimg;
    image_version = imgver;
}

/* The tile font: 256 tiles, each 8 bytes (one byte per row, MSB = left). */
uint8_t tiles[256][8];

/* Scratch space for decoded tile data. Each cell holds 8 bytes of
   1-bpp pixel data built up by Transform() during decoding. */
uint8_t layout[IRMAK_IMGSIZE][8];

/* The colour attributes of the cells of layout[], decoded in stage 2. */
static uint8_t attributes[IRMAK_IMGSIZE];

/* Compositing buffer for multi-part images. Bytes 0–7 hold tile pixel
   data (same format as layout), byte 8 (IRMAK_CELL_ATTR) holds the
   colour attribute. Multiple sub-images are drawn into this buffer,
   then the whole frame is rendered at once by
   DrawIrmakPictureFromBuffer(). */
uint8_t imagebuffer[IRMAK_IMGSIZE][IRMAK_CELL_BYTES];

/* Forward declarations of necessary external functions */
void RectFill(int32_t x, int32_t y, int32_t width, int32_t height, int32_t color);

/* The tile transformations are built from three primitives: mirroring
   left to right (Flip), mirroring top to bottom (ReverseRows) and
   mirroring in the main diagonal (Transpose). */

static uint8_t ReverseBits(uint8_t b)
{
    b = (uint8_t)((b & 0xf0) >> 4 | (b & 0x0f) << 4);
    b = (uint8_t)((b & 0xcc) >> 2 | (b & 0x33) << 2);
    b = (uint8_t)((b & 0xaa) >> 1 | (b & 0x55) << 1);
    return b;
}

/* Flip a tile horizontally, i.e. mirror it */
void Flip(uint8_t *tile)
{
    for (int i = 0; i < 8; i++)
        tile[i] = ReverseBits(tile[i]);
}

static void ReverseRows(uint8_t *tile)
{
    for (int i = 0; i < 4; i++) {
        uint8_t row = tile[i];
        tile[i] = tile[7 - i];
        tile[7 - i] = row;
    }
}

/* Swap rows and columns, with bit n of a row as column n
   (so it is the diagonal from top right to bottom left that stays). */
static void Transpose(uint8_t *tile)
{
    uint8_t work[8] = { 0 };

    for (int row = 0; row < 8; row++)
        for (int col = 0; col < 8; col++)
            work[row] |= (uint8_t)(((tile[col] >> row) & 1) << col);
    memcpy(tile, work, 8);
}

/* Apply one of four rotation modes (0=none, 1=90°, 2=180°, 3=270°,
   clockwise) to an 8-byte tile. */
void RotateTile(uint8_t *tile, int rotation)
{
    switch (rotation) {
    case 1:
        Transpose(tile);
        ReverseRows(tile);
        break;
    case 2:
        Flip(tile);
        ReverseRows(tile);
        break;
    case 3:
        Transpose(tile);
        Flip(tile);
        break;
    default:
        break;
    }
}

/* Apply rotation, flip, and overlay transformations to a tile and
   write the result into layout[offset].

   The 'mode' byte encodes the full transformation:
     - ROTATE_BITS select 0/90/180/270 rotation
     - FLIP_BIT mirrors horizontally (after rotation)
     - OVERLAY_BITS select the compositing mode: direct copy (0),
       OR, AND, or XOR with the existing data in layout[offset].
       This allows multiple tiles to be layered on the same cell. */
static void Transform(uint8_t tile, uint8_t mode, int offset)
{
    uint8_t work[8];
    int32_t i;

    memcpy(work, tiles[tile], 8);

    RotateTile(work, (mode & ROTATE_BITS) >> ROTATE_SHIFT);

    if (mode & FLIP_BIT) {
        Flip(work);
    }

    /* Composite onto layout: direct copy when no overlay mode is set,
       otherwise OR/AND/XOR with existing content. */
    mode &= OVERLAY_BITS;
    for (i = 0; i < 8; i++) {
        if (mode == MODE_XOR)
            layout[offset][i] ^= work[i];
        else if (mode == MODE_AND)
            layout[offset][i] &= work[i];
        else if (mode == MODE_OR)
            layout[offset][i] |= work[i];
        else
            layout[offset][i] = work[i];
    }
}

/* Transform() a tile into 'count' consecutive cells of layout[]. */
static void TransformRun(uint8_t tile, uint8_t mode, int offset, int count)
{
    for (int i = 0; i < count; i++)
        Transform(tile, mode, offset + i);
}

/* Send one run of ink pixels on a scanline, if there is one. */
static void FlushInkRun(int x, int y, int length, int ink)
{
    if (length > 0)
        RectFill(x, y, length, 1, ink);
}

/* Paint the cells of one row that are marked in changed[], or all of
   them when changed is NULL: count cells, the first one at column
   first_col. rows[] points at the eight pixel rows of each cell. A
   rectangle per pixel is slow to draw, so the row first gets its paper,
   one rectangle for every stretch of neighbouring cells of one colour,
   and then every scanline its runs of ink. Everything goes through
   RectFill(), not PutPixel(). */
static void PaintCellRow(int line, int first_col, int count,
                         const uint8_t *const rows[], const int ink[],
                         const int paper[], const uint8_t changed[])
{
    for (int cell = 0; cell < count;) {
        if (changed != NULL && !changed[cell]) {
            cell++;
            continue;
        }
        int first = cell;
        while (cell < count && (changed == NULL || changed[cell])
               && paper[cell] == paper[first])
            cell++;
        RectFill((first_col + first) * 8, line * 8, (cell - first) * 8, 8,
                 paper[first]);
    }

    for (int i = 0; i < 8; i++) {
        int y = line * 8 + i;
        int run_x = 0, run_length = 0, run_ink = 0;
        for (int cell = 0; cell < count; cell++) {
            uint8_t row = rows[cell][i];
            /* Ink in the paper colour is already there. */
            if ((changed != NULL && !changed[cell]) || row == 0
                || ink[cell] == paper[cell]) {
                FlushInkRun(run_x, y, run_length, run_ink);
                run_length = 0;
                continue;
            }
            for (int j = 0; j < 8; j++) {
                if (!(row & (0x80 >> j))) {
                    FlushInkRun(run_x, y, run_length, run_ink);
                    run_length = 0;
                } else if (run_length > 0 && run_ink == ink[cell]) {
                    run_length++;
                } else {
                    FlushInkRun(run_x, y, run_length, run_ink);
                    run_x = (first_col + cell) * 8 + j;
                    run_length = 1;
                    run_ink = ink[cell];
                }
            }
        }
        FlushInkRun(run_x, y, run_length, run_ink);
    }
}

/* Read the next byte of the image data into *out.
   Returns 0 when the data has run out. */
static int NextByte(IrmakImgContext *ctx, uint8_t *out)
{
    if (ctx->dataptr >= ctx->dataend)
        return 0;
    *out = *ctx->dataptr++;
    return 1;
}

/* The overlay chain of a command byte with OVERLAY_BITS set: one or
   more additional tiles which are composited (OR/AND/XOR) onto the
   'count' cells the command just drew at 'offset'. Every link is
   combined in the mode given by the OVERLAY_BITS of the command byte
   *before* it, and the chain continues as long as each successive
   command byte also has OVERLAY_BITS set. A byte below COMMAND_BIT
   ends the chain as a tile index without transformation.

   *tile is the tile carried from one command to the next in
   PerformTileTransformations(); the final plain tile index of a chain
   does not change it. Returns 0 if the data runs out. */
static int OverlayChain(IrmakImgContext *ctx, uint8_t command, uint8_t *tile,
                        int offset, int count)
{
    uint8_t previous = command;

    for (;;) {
        uint8_t data;
        if (!NextByte(ctx, &data))
            return 0;

        if ((data & COMMAND_BIT) == 0) {
            if (image_version >= 3 && (previous & ADD_128_BIT))
                data += 128;
            TransformRun(data, previous & OVERLAY_BITS, offset, count);
            return 1;
        }

        if (!NextByte(ctx, tile))
            return 0;
        /* Unlike in the command proper, the bit is not ignored
           for a tile that is in the upper half already. */
        if (data & ADD_128_BIT)
            *tile += 128;

        TransformRun(*tile, (data & OVERLAY_MASK) | (previous & OVERLAY_BITS),
                     offset, count);

        if ((data & OVERLAY_BITS) == 0)
            return 1;
        previous = data;
    }
}

/* Stage 1: Decode the tile data stream and populate layout[][].
   Each byte in the stream is either a solo tile index (COMMAND_BIT
   clear) or a command byte (COMMAND_BIT set) that specifies
   rotation, flip, overlay mode, an optional repeat count, and an
   optional chain of overlay tiles composited onto the same cells.

   Solo tiles:  [tile_index]
   Commands:    [command_byte] [count?] [tile] [overlay_chain...]

   The result is a fully decoded layout[][] ready for colouring
   in Stage 2 (DecodeAttributes).
   Returns 1 on success, 0 on data exhaustion. */
static int PerformTileTransformations(IrmakImgContext *ctx)
{
    int offset = 0;
    int imagesize = ctx->imagesize;

    /* Carries across iterations; the previous tile value
       determines ADD_128 behavior for solo tiles */
    uint8_t tile = 0;

    while (offset < imagesize) {
        uint8_t data;
        if (!NextByte(ctx, &data))
            return 0;

        if ((data & COMMAND_BIT) == 0) {
            /* Solo tile: the byte is the tile index directly.
               In version 3+, if the previous tile was in the upper
               half (128–255), solo indices are offset by 128 to
               stay in the same bank. */
            if (tile > 127 && image_version > 2) {
                data += 128;
            }
            tile = data;
            Transform(tile, 0, offset);
            offset++;
            continue;
        }

        /* Command byte: may specify a repeated run and/or
           overlay tiles composited on top. */
        int count = 1;
        if (data & REPEAT_BIT) {
            uint8_t repeats;
            if (!NextByte(ctx, &repeats))
                return 0;
            count = repeats + 1;
        }

        /* Get tile and plot it (count) times */
        if (!NextByte(ctx, &tile))
            return 0;

        if ((data & ADD_128_BIT) && tile < 128)
            tile += 128;

        /* A run may be longer than what is left of the image (a piece
           in Rebel Planet's room 34 is). The original draws on and
           reads the run's overlay chain all the same, which decides
           where the colour data starts. So only the plotting is cut
           short. */
        int plotted = count;
        if (offset + plotted > imagesize)
            plotted = imagesize - offset;

        /* The overlay bits only say how the tiles that follow are combined */
        TransformRun(tile, data & OVERLAY_MASK, offset, plotted);

        if ((data & OVERLAY_BITS) != 0 && !OverlayChain(ctx, data, &tile, offset, plotted))
            return 0;

        offset += count;
    }
    return 1;
}

/* Read one run of the RLE-compressed colour attribute stream: the
   colour goes into last_colour and the length of the run is returned,
   or -1 if the data has run out.

   The encoding is version-dependent:
     - Version 0–2: a non-command byte is a literal colour; a command
       byte specifies a repeat count and the colour byte follows.
     - Version 3+: a command byte repeats the *previous* colour (no
       explicit colour byte follows), saving one byte per run. So a
       run here can have a length of 0. */
static int ReadColourRun(IrmakImgContext *ctx)
{
    uint8_t data;
    if (!NextByte(ctx, &data))
        return -1;

    if ((data & COMMAND_BIT) == 0) {
        last_colour = data;
        return 1;
    }

    int count = data - COMMAND_BIT;
    if (image_version < 3) {
        count++;
        if (!NextByte(ctx, &last_colour))
            return -1;
    }
    return count;
}

/* The colour attribute of a version 0–2 picture in the version 3+
   format (see irmak.h). Ink and paper share one brightness in both. */
static uint8_t ConvertOldAttribute(uint8_t colour)
{
    uint8_t attribute = (uint8_t)(((colour & OLD_INK_MASK) >> 4) |
                                  ((colour & OLD_PAPER_MASK) << 3));
    /* Version 0 and 1 always use bright colours */
    if ((colour & V2_BRIGHT_FLAG) || image_version < 2)
        attribute |= BRIGHT_FLAG;
    return attribute;
}

/* Stage 2: decode the colour attributes of the picture into
   attributes[], one for every cell of layout[], in the version 3+
   format whatever the version of the picture.
   Returns 1 on success, 0 on data exhaustion. */
static int DecodeAttributes(IrmakImgContext *ctx)
{
    int x = 0;
    int y = 0;

    while (y < ctx->ysize) {
        int count = ReadColourRun(ctx);
        if (count < 0)
            return 0;

        for (; count > 0; count--) {
            /* Past the clamped size (see DrawIrmakPictureFromContext())
               the rest of the colour data is read and thrown away. */
            int idx = y * ctx->xsize + x;
            if (idx < ctx->imagesize)
                attributes[idx] = (image_version > 2) ? last_colour
                    : ConvertOldAttribute(last_colour);

            x++;
            if (x == ctx->xsize) {
                x = 0;
                y++;
                if (y >= ctx->ysize)
                    break;
            }
        }
    }
    return 1;
}

/* Stage 3: Put the decoded picture, layout[] and attributes[], where
   it goes. In buffer mode its cells are copied into imagebuffer[]; in
   direct mode they are rendered to the screen immediately, a row of
   cells at a time, leaving imagebuffer[] alone. */
static void DrawDecodedImage(IrmakImgContext *ctx, int draw_to_buffer)
{
    int xoff = ctx->xoff;
    /* Version 1–2 images are offset 4 tiles to the right in the data
       but should be drawn starting from column 0. */
    if (image_version > 0 && image_version < 3)
        xoff -= 4;

    int offset = 0;

    for (int y = 0; y < ctx->ysize && offset < ctx->imagesize; y++) {
        /* The last row is cut short if the picture is larger than
           layout[]. */
        int count = ctx->imagesize - offset;
        if (count > ctx->xsize)
            count = ctx->xsize;

        if (draw_to_buffer) {
            for (int x = 0; x < count; x++) {
                /* We only replace the data in imagebuffer
                   which the image covers. Any other data written
                   by previous draw operations is left intact. */
                unsigned bufpos = (y + ctx->yoff) * IRMAK_IMGWIDTH + x + xoff;
                if (bufpos < IRMAK_IMGSIZE) {
                    memcpy(imagebuffer[bufpos], layout[offset + x], 8);
                    imagebuffer[bufpos][IRMAK_CELL_ATTR] = attributes[offset + x];
                }
            }
        } else {
            /* A picture is at most 255 cells wide (Image.width). */
            const uint8_t *rows[UINT8_MAX];
            int ink[UINT8_MAX], paper[UINT8_MAX];

            for (int x = 0; x < count; x++) {
                uint8_t attribute = attributes[offset + x];
                rows[x] = layout[offset + x];
                ink[x] = Remap(AttributeInk(attribute));
                paper[x] = Remap(AttributePaper(attribute));
            }
            PaintCellRow(y + ctx->yoff, xoff, count, rows, ink, paper, NULL);
        }

        offset += count;
    }
}

/* Reset the compositing buffer to blank (all tiles cleared,
   all attributes zeroed). Called before drawing a new frame. */
void ClearGraphMem(void)
{
    memset(imagebuffer, 0, sizeof(imagebuffer));
}

/* Decode and render a single image from its data stream. Runs all
   three stages in sequence: tile transforms → attribute decode →
   final compositing/rendering. If the data runs out in stage 1 or 2,
   nothing is drawn and imagebuffer[] is left as it was.
   Returns 1 if the picture was drawn. */
static int DrawIrmakPictureFromContext(IrmakImgContext *ctx, int draw_to_buffer)
{
    ctx->imagesize = ctx->xsize * ctx->ysize;
    /* Clamp to the capacity of layout[]/attributes[] so that the
       per-cell bounds checks (all against imagesize) also protect the
       real buffers against oversized or corrupt image dimensions. */
    if (ctx->imagesize > IRMAK_IMGSIZE)
        ctx->imagesize = IRMAK_IMGSIZE;

    if (!PerformTileTransformations(ctx) || !DecodeAttributes(ctx))
        return 0;

    DrawDecodedImage(ctx, draw_to_buffer);
    return 1;
}

/* Render the cells of the compositing buffer that differ from shown[],
   a copy of imagebuffer[] as the screen last showed it, or all of them
   when shown is NULL. */
void DrawIrmakBufferChanges(const uint8_t shown[][IRMAK_CELL_BYTES])
{
    for (int line = 0; line < IRMAK_IMGHEIGHT; line++) {
        const uint8_t (*cells)[IRMAK_CELL_BYTES] = imagebuffer + line * IRMAK_IMGWIDTH;
        const uint8_t *rows[IRMAK_IMGWIDTH];
        uint8_t changed[IRMAK_IMGWIDTH];
        int ink[IRMAK_IMGWIDTH], paper[IRMAK_IMGWIDTH];
        int any_changed = 0;

        for (int col = 0; col < IRMAK_IMGWIDTH; col++) {
            uint8_t attribute = cells[col][IRMAK_CELL_ATTR];
            rows[col] = cells[col];
            ink[col] = Remap(AttributeInk(attribute));
            paper[col] = Remap(AttributePaper(attribute));
            changed[col] = shown == NULL
                || memcmp(cells[col], shown[line * IRMAK_IMGWIDTH + col], IRMAK_CELL_BYTES) != 0;
            any_changed |= changed[col];
        }
        if (any_changed)
            PaintCellRow(line, 0, IRMAK_IMGWIDTH, rows, ink, paper, changed);
    }
}

/* Render the entire compositing buffer to the screen. Called after
   all sub-images have been drawn into imagebuffer[][] via buffer mode. */
void DrawIrmakPictureFromBuffer(void)
{
    DrawIrmakBufferChanges(NULL);
}

/* Draw a picture at an explicit tile position (x, y). If x or y
   are out of range (>= the image dimensions), the image's own
   stored offset is used as a fallback — this lets callers pass
   IRMAK_DEFAULT_POS to mean "use the default position".
   Returns 1 on success, 0 if the image is invalid or missing. */
int DrawPictureAtPos(int picture_number, uint8_t x, uint8_t y, int draw_to_buffer)
{
    if (number_of_images == 0)
        return 0;
    /* The draw instructions past the last real room of a TaylorMade
       game are garbage, and ask for pictures that do not exist. */
    if (picture_number < 0 || picture_number >= number_of_images)
        return 0;

    const Image *img = &images[picture_number];
    if (img->imagedata == NULL)
        return 0;

    IrmakImgContext ctx;
    ctx.dataptr = img->imagedata;
    ctx.dataend = img->imagedata + img->datasize;
    ctx.xsize = img->width;
    ctx.ysize = img->height;
    ctx.xoff = (x >= IRMAK_IMGWIDTH) ? img->xoff : x;
    ctx.yoff = (y >= IRMAK_IMGHEIGHT) ? img->yoff : y;

    return DrawIrmakPictureFromContext(&ctx, draw_to_buffer);
}

/* Convenience wrapper: draw a picture at its default position. */
void DrawPictureNumber(int picture_number, int draw_to_buffer)
{
    DrawPictureAtPos(picture_number, IRMAK_DEFAULT_POS, IRMAK_DEFAULT_POS, draw_to_buffer);
}
