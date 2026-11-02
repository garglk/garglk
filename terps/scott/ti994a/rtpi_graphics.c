//
//  rtpi_graphics.c
//  scott
//
//  Created by Administrator on 2026-02-12.
//
//  Loader and graphics decoder for "Return to Pirate's Isle" (RTPI),
//  a Scott Adams adventure originally released for the TI-99/4A.
//
//  The game data is distributed as a zip archive containing ROM files
//  ("c.bin" for CPU ROM, "g.bin" for GROM graphics data, or the five
//  split MAME GROM files "phm3189g3.bin" through "phm3189g7.bin").
//
//  Graphics use the TI-99/4A's VDP (Video Display Processor) tile
//  system: 256 screen positions arranged as 32 columns × 12 rows of
//  8×8-pixel tiles. Image data is stored in GROM as a bytecode stream
//  of 8 opcodes (0–7) that manipulate a 52-tile font with rotations
//  and bitwise compositing (OR/AND/XOR), followed by RLE-compressed
//  colour data. Colours use the TMS9918A's attribute format: one byte
//  per pixel row, upper nibble = foreground, lower nibble = background.
//
//  If the game data contains a standard Scott Adams header, I have not been
//  able to find it. Instead, all offsets and counts are hardcoded in DetectRTPI().
//

#include <string.h>
#include <stdlib.h>

#include "scott.h"
#include "irmak.h"
#include "detect_game.h"
#include "saga.h"
#include "sagagraphics.h"
#include "unzip_in_mem.h"

#include "rtpi_graphics.h"
#include "ti99_disk.h"

/* VDP screen buffer: 32 columns × 12 rows × 8 bytes per tile = 0xC00 bytes.
   vdp_pixels holds 1-bpp tile pattern data; vdp_colors holds per-row
   colour attributes (upper nibble = fg, lower nibble = bg). */
#define RTPI_IMAGE_SIZE (32 * 12 * 8)

static uint8_t vdp_pixels[RTPI_IMAGE_SIZE];
static uint8_t vdp_colors[RTPI_IMAGE_SIZE];

#define NUM_GROM_BANKS        5
#define GROM_BANK_SIZE              0x2000

/* Total GROM data size (5 × 0x2000 byte banks). */
#define GROM_SIZE (NUM_GROM_BANKS * GROM_BANK_SIZE)

/* Size of the per-bank header that prefixes each GROM bank, and the
   usable data region that follows it within each bank. */
#define GROM_HEADER_SIZE 0x802
#define GROM_BANK_DATA_SIZE (GROM_BANK_SIZE - GROM_HEADER_SIZE)

/* Usable GROM data size with headers cut out (5 × 0x17fe = 0x77f6). */
#define GROM_FLAT_SIZE (NUM_GROM_BANKS * GROM_BANK_DATA_SIZE)

#define NUM_RTPI_ROOM_IMAGES (24 + 9)
#define NUM_RTPI_ITEM_IMAGES 7
#define NUM_RTPI_TILES 52
#define RTPI_TITLE_TEXT_LENGTH 0x1e2

/* MAME zip layout splits the GROM across five 0x2000-byte bank files
 named phm3189g3.bin through phm3189g7.bin. */
#define FIRST_GROM_BANK_NUMBER      3
#define RTPI_GROM_FILENAME_BUF_SIZE 14

/* The 52-entry tile font, loaded from the CPU ROM (c.bin). */
static uint8_t rtpi_tile_font[NUM_RTPI_TILES][8];

/* Debug names for the 8 draw opcodes. */
static const char *draw_opcode_names[8] = {
    "op0_copy_tiles",
    "op1_repeat_tile",
    "op2_move_cursor",
    "op3_copy_one_tile",
    "op4_decode_and_blit_tiles",
    "op5_copy_tile_run",
    "op6_sub_opcodes",
    "op7_combine_tile_pairs"
};

/* Decode the RLE-compressed colour attribute stream that follows the
   tile pattern opcodes. Each byte in vdp_colors holds the colour for
   one pixel row: upper nibble = foreground, lower nibble = background.

   The encoding uses three modes:
     - Literal byte (top nibble != 0): write directly to vdp_colors.
     - Top nibble == 0x7: address jump — next byte forms a 12-bit
       write offset (in units of 8 bytes).
     - Top nibble == 0, bottom nibble != 0: repeat count in lower
       nibble; next byte is the colour to repeat (count × 8 times).
     - Byte == 0x00: next byte is a full 8-bit repeat count (for
       runs longer than 15). */
static int DecodeRTPIColors(uint8_t *ptr, USImage *image) {
    debug_print("DecodeRTPIColors\n");
    uint8_t *origptr = image->imagedata;
    uint8_t *origcolptr = ptr;
    (void)origcolptr; /* read by debug_print only */
    uint8_t *end = origptr + image->datasize;

    uint16_t write_offset = 0;

    while (ptr < end && write_offset < RTPI_IMAGE_SIZE) {
        uint8_t draw_op = *ptr++;

        uint8_t top    = draw_op & 0xf0;
        uint8_t bottom = draw_op & 0x0f;

        if (top == 0x70) {
            if (ptr >= end)
                break;
            uint8_t newaddr = *ptr++;
            write_offset = ((bottom << 8) | newaddr) * 8;
            debug_print("DecodeRTPIColors: move opcode (0x70). New write offset: 0x%04x\n", write_offset);
            if (write_offset >= RTPI_IMAGE_SIZE) {
                debug_print("DecodeRTPIColors: write offset 0x%04x is offscreen. Max is 0x%04x. (read offset: 0x%lx) Bailing\n", write_offset, RTPI_IMAGE_SIZE, ptr - origcolptr);
                return 0;
            }
            continue;
        }
        if (top != 0 && draw_op != 0) {
            debug_print("DecodeRTPIColors: Drew 0x%02x to 0x%04x.\n", draw_op, write_offset);
            vdp_colors[write_offset++] = draw_op;
            continue;
        }

        int repeat_count;
        if (draw_op == 0) {
            if (ptr >= end)
                break;
            repeat_count = *ptr++;
            if (repeat_count == 0)
                break;
        } else {
            repeat_count = bottom;
        }

        if (ptr >= end)
            break;
        int repeats = repeat_count * 8;
        uint8_t color = *ptr++;
        debug_print("DecodeRTPIColors: Writing color 0x%02x %d times.\n", color, repeats);
        for (int i = 0; i < repeats; i++) {
            if (write_offset >= RTPI_IMAGE_SIZE) {
                debug_print("DecodeRTPIColors: write offset is offscreen: 0x%04x, max 0x%04x.\n", write_offset, RTPI_IMAGE_SIZE);
                return 0;
            }
            vdp_colors[write_offset++] = color;
        }
    }

    size_t used = ptr - origptr;
    if (used < image->datasize) {
        image->imagedata = MemRealloc(image->imagedata, used);
        image->datasize = used;
        LoadTrace("DecodeRTPIColors: Pruned size of %s %d to 0x%zx\n", image->usage == IMG_ROOM ? "IMG_ROOM" : "IMG_ROOM_OBJ", image->index, used);
    }
    return 1;
}

/* Copy a tile from the font and apply rotation. Special case: tile 0
   with rotation 1 produces a solid black (all-bits-set) tile rather
   than a rotated blank. */
static int get_tile_and_rotate(uint8_t *tile, int tile_index, int rotation) {
    if (tile_index >= NUM_RTPI_TILES) {
        debug_print("get_tile_and_rotate: illegal tile %d\n", tile_index);
        return 0;
    }
    memcpy(tile, rtpi_tile_font[tile_index], 8);
    // Index 0 with rotation 1 means a black tile
    if (tile_index == 0 && rotation == 1) {
        memset(tile, 0xff, 8);
    } else {
        RotateTile(tile, rotation);
    }
    return 1;
}

/* Read one byte from the GROM stream and unpack the tile index
   (bits 0–5) and rotation (bits 6–7). */
static int get_next_byte(uint8_t **ptr, uint8_t *gend, uint16_t *tile_index, int *rotation) {
    uint8_t *gptr = *ptr;
    if (gptr >= gend)
        return 0;
    uint8_t grom_byte = *gptr++;
    *tile_index = grom_byte & 0x3f;
    *rotation = grom_byte >> 6;
    *ptr = gptr;
    return 1;
}

/* Copy 8 bytes from the VDP pixel buffer to the tile pointer.
   Bounds-checked against the buffer end. */
static int copy_tile_from_vdp(uint8_t *tile, int screen_index) {
    if (screen_index * 8 + 8 > RTPI_IMAGE_SIZE)
        return 0;
    memcpy(tile, vdp_pixels + screen_index * 8, 8);
    return 1;
}

/* Write 8 bytes of tile data to the VDP pixel buffer and advance
   the write pointer. Bounds-checked against the buffer end. */
static uint8_t *blit_tile(uint8_t *dst, uint8_t *tile) {
    if (dst + 8 > vdp_pixels + RTPI_IMAGE_SIZE)
        return dst;
    memcpy(dst, tile, 8);
    return dst + 8;
}


/* Decode a GROM image bytecode stream into vdp_pixels[] and vdp_colors[].

   When 'fuzzy' is non-zero, the stream pointer starts 2 bytes late,
   causing the bytecode to be misinterpreted — the resulting garbled
   image simulates the protagonist's myopia (used when glasses are
   not worn).

   The bytecode operates on a 16-bit instruction word per iteration:
     bits 13–15: opcode (0–7)
     bits 9–12:  repeat count / sub-opcode selector
     bits 6–7:   tile rotation (0/90/180/270)
     bits 0–5:   tile index (0–51) for tile-based ops
     bits 0–8:   screen position index for VDP-read ops

   The stream ends when opcode 7 with repeat==0 is encountered, which
   signals the transition to the colour attribute section.

   Each opcode has a handler below, working on the decoder state. */
typedef struct {
    uint8_t *gptr; /* read position in the bytecode */
    uint8_t *gend;
    uint8_t *dst; /* write position in vdp_pixels */
    uint16_t tile_index;
    uint16_t screen_index;
    int rotation;
    int repeats;
    uint8_t tile[8];
    uint8_t worktile[8];
} RTPIDecoder;

/* Opcode 0. Copy tiles: read from VDP, compose, blit, repeat.
 * For each repeat, reads a tile from VDP into the buffer,
 * blits the buffer to VDP at R4, clears the buffer, and reads
 * the next tile index from GROM for the next iteration.
 */
static void RTPICopyTiles(RTPIDecoder *d) {
    for (int i = 0; i < d->repeats; i++) {
        if (!copy_tile_from_vdp(d->tile, d->screen_index))
            break;
        d->dst = blit_tile(d->dst, d->tile);
        if (i < d->repeats - 1) {
            if (d->gptr >= d->gend)
                break;
            d->screen_index = *d->gptr++;
        }
    }
}

/* Opcode 1. Repeat tile: draw tile with rotation, blit repeatedly.
 * Processes a single tile with full rotation/composition,
 * then blits the result to VDP (repeats) times. Each blit writes the
 * same composed tile to consecutive VDP positions.
 */
static void RTPIRepeatTile(RTPIDecoder *d) {
    if (!get_tile_and_rotate(d->tile, d->tile_index, d->rotation))
        return;

    for (int i = 0; i < d->repeats; i++) {
        d->dst = blit_tile(d->dst, d->tile);
    }
    memset(d->tile, 0, 8);
}

/* Opcode 3. Copy one tile: read tile from VDP and blit with repeats.
 * Reads one tile from VDP into the buffer, then blits
 * it to VDP (repeat) times.
 */
static void RTPICopyOneTile(RTPIDecoder *d) {
    if (!copy_tile_from_vdp(d->tile, d->screen_index))
        return;
    for (int i = 0; i < d->repeats; i++) {
        d->dst = blit_tile(d->dst, d->tile);
    }
    memset(d->tile, 0, 8);
}

/* Opcode 4. Process tile with rotation, blit, clear, repeat.
 * Decode and blit tiles. For each iteration, reads a tile
 * index from GROM, processes it with rotation, blits to VDP,
 * clears the buffer, and repeats for (repeats) iterations.
 */
static void RTPIDecodeTiles(RTPIDecoder *d) {
    for (int i = 0; i < d->repeats; i++) {
        if (!get_tile_and_rotate(d->tile, d->tile_index, d->rotation))
            continue;
        d->dst = blit_tile(d->dst, d->tile);
        if (i < d->repeats - 1) {
            if (!get_next_byte(&d->gptr, d->gend, &d->tile_index, &d->rotation))
                break;
        }
    }
    memset(d->tile, 0, 8);
}

/* Opcode 5. Copy tile run from VDP.
 * Reads a tile from VDP, blits it, increments the tile
 * index, clears the buffer, and repeats. This draws a
 * sequential run of tiles (tile N, N+1, N+2, ...).
 */
static void RTPICopyTileRun(RTPIDecoder *d) {
    for (int i = 0; i < d->repeats; i++) {
        if (!copy_tile_from_vdp(d->tile, d->screen_index))
            break;
        d->dst = blit_tile(d->dst, d->tile);
        d->screen_index++;
    }
    memset(d->tile, 0, 8);
}

/* Opcode 6. Sub-opcodes.
 * One of 16 sub-opcodes depending on
 * the value of `repeats`. Only the ones actually
 * used by the game are implemented here.
 */
static void RTPISubOpcode(RTPIDecoder *d) {
    if (d->repeats > 0 && d->repeats < 13) {
        if (!get_tile_and_rotate(d->worktile, d->tile_index, d->rotation))
            return;
    }

    switch(d->repeats) {
        case 0: //  0. Write 8 raw bytes to screen memory directly from GROM
            if (d->gptr + 8 > d->gend || d->dst + 8 > vdp_pixels + RTPI_IMAGE_SIZE)
                break;
            for (int i = 0; i < 8; i++) {
                *d->dst++ = *d->gptr++;
            }
            break;
        case 1: //  1-4. OR tile_index with next (repeats) tiles, then blit and clear
        case 2:
        case 3:
        case 4:
            for (int i = 0; i < d->repeats; i++) {
                for (int j = 0; j < 8; j++)
                    d->tile[j] |= d->worktile[j];
                if (i < d->repeats - 1) {
                    if (!get_next_byte(&d->gptr, d->gend, &d->tile_index, &d->rotation))
                        break;
                    if (!get_tile_and_rotate(d->worktile, d->tile_index, d->rotation))
                        break;
                }
            }
            d->dst = blit_tile(d->dst, d->tile);
            memset(d->tile, 0, 8);
            break;
        case 6: //  6. Clear, AND tile_index with next tile, blit and clear
            if (!get_next_byte(&d->gptr, d->gend, &d->tile_index, &d->rotation))
                break;
            if (!get_tile_and_rotate(d->tile, d->tile_index, d->rotation))
                break;
            for (int i = 0; i < 8; i++)
                d->tile[i] &= d->worktile[i];
            d->dst = blit_tile(d->dst, d->tile);
            memset(d->tile, 0, 8);
            break;
        case 9: //  9. AND tile with tile_index
            for (int i = 0; i < 8; i++) {
                d->tile[i] &= d->worktile[i];
            }
            break;
        case 10: // 10. OR tile with tile_index
            for (int i = 0; i < 8; i++) {
                d->tile[i] |= d->worktile[i];
            }
            break;
        case 11: // 11. XOR tile with tile_index
            for (int i = 0; i < 8; i++) {
                d->tile[i] ^= d->worktile[i];
            }
            break;
        case 13: // 13. Copy screen tile at screen_index to tile
            copy_tile_from_vdp(d->tile, d->screen_index);
            break;
        case 15: // 15. Blit tile and copy tile_index to tile
            d->dst = blit_tile(d->dst, d->tile);
            if (!get_tile_and_rotate(d->tile, d->tile_index, 0))
                memset(d->tile, 0, 8);
            break;
        default:
            fprintf(stderr, "DrawRTPIImageWithOptionalFuzz: Unimplemented sub-opcode (%d)\n", d->repeats);
            break;
    }
}

/* Opcode 7. Combine tile pairs.
 * OR two tiles and blit them.
 * Repeat (repeats / 2) times.
 */
static void RTPICombineTilePairs(RTPIDecoder *d) {
    for (int i = 0; i < d->repeats; i++) {
        if (!get_tile_and_rotate(d->worktile, d->tile_index, d->rotation))
            break;
        for (int j = 0; j < 8; j++)
            d->tile[j] |= d->worktile[j];
        if (i % 2 == 1) {
            d->dst = blit_tile(d->dst, d->tile);
            memset(d->tile, 0, 8);
        }
        if (i < d->repeats - 1) {
            if (!get_next_byte(&d->gptr, d->gend, &d->tile_index, &d->rotation))
                break;
        }
    }
}

static int DrawRTPIImageWithOptionalFuzz(USImage *image, int fuzzy) {

    if (image == NULL || image->imagedata == NULL || image->datasize < 2) {
        return 0;
    }

    debug_print("DrawRTPIImageWithOptionalFuzz: drawing image %d\n", image->index);

    /* Clear colour memory for full room images to avoid residual
       colours from the previous image. Don't clear for overlay
       images (room objects) drawn on top of a base image. */
    if (image->usage == IMG_ROOM && image->index <= GameHeader.NumRooms) {
        ClearVDP();
    }

    RTPIDecoder decoder = { 0 };
    RTPIDecoder *d = &decoder;
    d->dst = vdp_pixels;

    /* Fuzzy mode: skip 2 bytes to misalign the bytecode stream,
       producing a garbled image (simulates myopia). */
    d->gptr = image->imagedata + (fuzzy ? 2 : 0);
    d->gend = image->imagedata + image->datasize;

    while (d->gptr + 2 <= d->gend) {
        /*   - Bits 0-5: tile index. There are only 52 tile patterns.
         *   - Bits 6-7: rotation mode (0=none, 1=rot-90, 2=rot-180, 3=rot-270)
         *   - Some opcodes (such as 0) instead use bits 0-8 as the index to an
         *     already drawn tile in video memory (screen_index).
         *   - Bits 9-12: repeat count / subop index
         *   - Bits 13-15: opcode
         */
        uint16_t draw_ops_word = READ_BE_UINT16(d->gptr);
        d->gptr += 2;
        d->tile_index = draw_ops_word & 0x3f;
        d->screen_index = draw_ops_word & 0x1ff;
        d->rotation = (draw_ops_word & 0xc0) >> 6;
        d->repeats = (draw_ops_word & 0x1e00) >> 9;
        int op = (draw_ops_word & 0xe000) >> 13;

        if (d->repeats == 0) {
            if (op == 7) {
                debug_print("DrawRTPIImageWithOptionalFuzz: repeats is 0 and opcode is 7: Jump to DecodeRTPIColors.\n");
                return DecodeRTPIColors(d->gptr - 1, image);
            }

            if (op != 2) {
                d->gptr--;
                memset(d->tile, 0, 8);
                if (op != 6)
                    continue;
            }
        }

        debug_print("DrawRTPIImage: tile index: %d rot:%d rep:%d op:%d\n", d->tile_index, d->rotation, d->repeats, op);

        (void)draw_opcode_names; /* read by debug_print only */
        debug_print("DrawRTPIImage: draw op: %s\n", draw_opcode_names[op]);

        switch (op) {
            case 0:
                RTPICopyTiles(d);
                break;
            case 1:
                RTPIRepeatTile(d);
                break;
            case 2:
                /* Move the write address to a tile position, without
                 * writing any tile data.
                 */
                if (d->screen_index * 8 < RTPI_IMAGE_SIZE)
                    d->dst = vdp_pixels + d->screen_index * 8;
                break;
            case 3:
                RTPICopyOneTile(d);
                break;
            case 4:
                RTPIDecodeTiles(d);
                break;
            case 5:
                RTPICopyTileRun(d);
                break;
            case 6:
                RTPISubOpcode(d);
                break;
            case 7:
                RTPICombineTilePairs(d);
                break;
        }
    }
    return 0;
}

/* Normal image rendering. */
int DrawRTPIImage(USImage *image) {
    return DrawRTPIImageWithOptionalFuzz(image, 0);
}

/* Render with deliberate misalignment (myopia effect). */
int DrawFuzzyRTPIImage(USImage *image) {
    return DrawRTPIImageWithOptionalFuzz(image, 1);
}

/* TI-99/4a colour information from MAME:

 Color            Y      R-Y     B-Y     R       G       B       R   G   B
 0 Transparent
 1 Black         0.00    0.47    0.47    0.00    0.00    0.00      0   0   0
 2 Medium green  0.53    0.07    0.20    0.13    0.79    0.26     33 200  66
 3 Light green   0.67    0.17    0.27    0.37    0.86    0.47     94 220 120
 4 Dark blue     0.40    0.40    1.00    0.33    0.33    0.93     84  85 237
 5 Light blue    0.53    0.43    0.93    0.49    0.46    0.99    125 118 252
 6 Dark red      0.47    0.83    0.30    0.83    0.32    0.30    212  82  77
 7 Cyan          0.73    0.00    0.70    0.26    0.92    0.96     66 235 245
 8 Medium red    0.53    0.93    0.27    0.99    0.33    0.33    252  85  84
 9 Light red     0.67    0.93    0.27    1.13(!) 0.47    0.47    255 121 120
 A Dark yellow   0.73    0.57    0.07    0.83    0.76    0.33    212 193  84
 B Light yellow  0.80    0.57    0.17    0.90    0.81    0.50    230 206 128
 C Dark green    0.47    0.13    0.23    0.13    0.69    0.23     33 176  59
 D Magenta       0.53    0.73    0.67    0.79    0.36    0.73    201  91 186
 E Gray          0.80    0.47    0.47    0.80    0.80    0.80    204 204 204
 F White         1.00    0.47    0.47    1.00    1.00    1.00    255 255 255
 */

/* Set up the 16-entry palette to match the TMS9918A VDP's fixed
   colour table. RGB values are based on the MAME emulator's
   analysis of the TMS9918A's composite video output, and then
   hand adjusted. */
static void SetupRTPIColors(void) {

    glui32 black =     0x000000;
    glui32 blue =      0x5051E0;
    glui32 medgreen =  0x21c842;
    glui32 lghtgreen = 0x5edc78;
    glui32 magenta =   0xc95bba;
    glui32 darkred =   0xE15D53;
    glui32 cyan =      0x42ebf5;
    glui32 mediumred = 0xff655C;
    glui32 lightred =  0xff847e;
    glui32 yellow =    0xD4C154;
    glui32 white =     0xCDCDCD;
    glui32 brblue =    0x7F77F8;
    glui32 drkgreen =  0x00ae3f;
    glui32 bryellow =  0xEAD087;
    glui32 brwhite =   0xffffff;

    SetColor( 0, black);
    SetColor( 1, black);
    SetColor( 2, medgreen);
    SetColor( 3, lghtgreen);
    SetColor( 4, blue);
    SetColor( 5, brblue);
    SetColor( 6, darkred);
    SetColor( 7, cyan);
    SetColor( 8, mediumred);
    SetColor( 9, lightred);
    SetColor(10, yellow);
    SetColor(11, bryellow);
    SetColor(12, drkgreen);
    SetColor(13, magenta);
    SetColor(14, white);
    SetColor(15, brwhite);
}

/* Make any image black by clearing the vdp color data. */
void ClearVDP(void) {
    memset(vdp_colors, 0, RTPI_IMAGE_SIZE);
}

/* Render the decoded VDP buffers (vdp_pixels + vdp_colors) to the
   screen. Iterates over each tile position, unpacking the per-row
   colour attribute (fg in upper nibble, bg in lower) and drawing
   foreground/background pixels accordingly. */
void DrawRTPIFromMem(void) {
    glk_window_clear(Graphics);

    uint8_t *ptr = vdp_pixels;
    uint8_t *colorptr = vdp_colors;
    uint8_t fg, bg;

    uint8_t *endptr = vdp_pixels + (RTPI_IMAGE_SIZE - 1);
    int x = 0; int y = 0;
    while (y < ImageHeight && ptr <= endptr) {
        for (int line = 0; line < 8; line++) {
            uint8_t byte = *ptr++;
            fg = *colorptr++;
            bg = fg & 0xf;
            fg >>= 4;
            for (int p = 0; p < 8; p++) {
                if (PixelSet(byte, p)) {
                    PutPixel(x + p, y + line, fg);
                } else {
                    PutPixel(x + p, y + line, bg);
                }
            }
        }
        x += 8;
        if (x > 255) {
            x = 0;
            y += 8;
        }
    }
}

/* Copy image data from GROM into a newly allocated USImage, handling
   the GROM bank header discontinuity. Each 0x2000-byte GROM bank has
   an 0x802-byte header that must be skipped when an image spans a bank
   boundary. After copying, allocates and links a new (empty) USImage
   node and returns it for the next image to use. */
static USImage *finalize_img_make_new(USImage *image, uint16_t offset, uint16_t size, uint8_t *grom) {

    if (offset >= GROM_SIZE)
        Fatal("Bad image data\n");

    image->imagedata = MemAlloc(size);
    memcpy(image->imagedata, grom + offset, size);

    uint16_t maxrange = offset + size;

    /* If the image spans a GROM bank boundary, the middle portion
       contains a bank header (GROM_HEADER_SIZE bytes) that is not
       image data. Re-copy the tail from past the header to stitch
       the image back together. */
    if (maxrange > GROM_BANK_DATA_SIZE && (maxrange - GROM_BANK_DATA_SIZE) % GROM_BANK_SIZE < size) {
        uint16_t cutoff = size - ((maxrange - GROM_BANK_DATA_SIZE) % GROM_BANK_SIZE);
        if (offset + cutoff + GROM_HEADER_SIZE + size - cutoff >= GROM_SIZE)
            Fatal("Bad image data\n");
        memcpy(image->imagedata + cutoff, grom + offset + cutoff + GROM_HEADER_SIZE, size - cutoff);
    }

    image->datasize = size;

    return AppendImage(image);
}

#define LAST_IMAGE_SIZE 0x72

/* Reference: image-to-room mapping in the original GROM data.

   Image for room 0 (.) has offset 0x8000 (darkness)
   Image for room 1 (bottom bunk) has offset 0x8047
   Image for room 2 (ship's cabin) has offset 0x831f
   Image for room 3 (*I'm on a dock) has offset 0x854c
   Image for room 4 (*I'm on deck) has offset 0x87d7
   Image for room 5 (beach by a small hill) has offset 0x8953
   Image for room 6 (*I'm on ledge 8 feet below hill summit) has offset 0x8ab2
   Image for room 7 (*I'm on top of a small hill) has offset 0x8d10
   Image for room 8 (cavern) has offset 0x0000 (no image)
   Image for room 9 (tool shed) has offset 0x0000 (no image)
   Image for room 10 (*Sorry, to explore Pirate's Isle you'll need Adventure #2) has offset 0x8e84
   Image for room 11 (sea) has offset 0x8fcf
   Image for room 12 (*I'm undersea) has offset 0x90c8
   Image for room 13 (smugglers hold inside ship) has offset 0x92fe
   Image for room 14 (hall) has offset 0x0000 (no image)
   Image for room 15 (*I'm on a rocky beach by sea) has offset 0x9426
   Image for room 16 (top bunk) has offset 0x9619
   Image for room 17 (*I'm on the engine) has offset 0x9773
   Image for room 18 (*I'm in an engine room) has offset 0xa0fa
   Image for room 19 (*I'm on catwalk on outside of ship by porthole) has offset 0xa378
   Image for room 20 (narrow crawlway) has offset 0xa468
   Image for room 21 (ship's hold) has offset 0xa54e
   Image for room 22 (*I'm on beam under dock) has offset 0x8fcf
   Image for room 23 (sunken ship) has offset 0xa5ca
   Image for room 24 (Never Never Land) has offset 0xa86a
   Image for room 25 () has offset 0xab0f // Title screen / Texas Instruments logo
   Image for room 26 () has offset 0xadc2 // Congrats, victory screen
   Image for room 27 () has offset 0xc39a // Smuggler's hold from below
   Image for room 28 () has offset 0xc47c // Boat's deck at night with island in the distance
   Image for room 29 () has offset 0xc588 // Sea at night
   Image for room 30 () has offset 0xc610 // High beam (to be drawn over Underside of Dock)
   Image for room 31 () has offset 0xc676 // Low beam (to be drawn over I'm on beam under dock)
   Image for room 32 () has offset 0xc6dc // Boat to the west at night (to be drawn over 29)
   Image for room object 63 (Sunken Ship) has offset 0xc775
   Image for room object 26 (Dock to the East) has offset 0xc8f7
   Image for room object 28 (Boat to the WEST) has offset 0xc9f1
   Image for room object 36 (Rocky beach) has offset 0xcaa2
   Image for room object 27 (Underside of Dock) has offset 0xcb04
   Image for room object 7  (Sleeping Pirate) has offset 0xcca6
   Image for room object 52 (Pirate at helm) has offset 0xcd08
*/

/* Build a linked list of USImage structs from the GROM data. Room
   images come first (24 rooms + 9 extra scenes), followed by 7 item
   overlay images. Image sizes are generally computed from the gap
   between consecutive offsets, with hardcoded overrides for images
   whose data is not contiguous (17, 22, 26). */
static void LoadRTPIGraphics(uint16_t *room_image_offsets, uint16_t item_image_offsets[NUM_RTPI_ITEM_IMAGES][2], uint8_t *grom) {
    USImages = NewImage();
    USImage *image = USImages;

    for (int i = 0; i < NUM_RTPI_ROOM_IMAGES; i++) {

        if (room_image_offsets[i] == 0)
            continue;

        size_t offset = room_image_offsets[i];

        image->systype = SYS_TI994A;
        image->usage = IMG_ROOM;
        image->index = i;
        if (i == 25) {
            /* Image 25 is the Texas Instruments logo / title screen;
               map it to the standard title image index (99). */
            image->index = 99;
        } else if (i == 27) {
            /* Smuggler's hold from below — displayed as an object
               overlay for room 29 (inside of boat). */
            image->index = 29;
            image->usage = IMG_ROOM_OBJ;
        } else if (i == 28) {
            /* Deck at night — displayed as an object overlay for
               room 4 (isle off in distance). */
            image->index = 4;
            image->usage = IMG_ROOM_OBJ;
        }

        size_t size;

        /* Some images span GROM bank headers, so their sizes can't be computed
         from the offset table and must be hardcoded. */
        if (i == 17) {
            size = 0x183;
        } else if (i == 22) {
            size = 0xf7;
        } else if (i == 26) {
            size = 0xdd4;
        } else {
            /* Normal case: walk forward to the next entry whose offset
               is >= ours and use the gap as the size. If we run out of
               room entries, fall back to the first item image offset. */
            int j = i + 1;
            while (j < NUM_RTPI_ROOM_IMAGES && room_image_offsets[j] < offset)
                j++;
            size_t next = (j < NUM_RTPI_ROOM_IMAGES) ? room_image_offsets[j]
                                                           : item_image_offsets[0][1];
            size = next - offset;
        }

        image = finalize_img_make_new(image, offset, size, grom);
    }

    /* Item overlay images. Each entry in item_image_offsets[][] is a pair:
       [0] = index of room image the item overlays, [1] = GROM offset of image data.
       Size is computed from the gap to the next item image, except for the
       last one which has a hardcoded size (LAST_IMAGE_SIZE). */
    for (int i = 0; i < NUM_RTPI_ITEM_IMAGES; i++) {
        size_t offset = item_image_offsets[i][1];

        size_t size;
        if (i < NUM_RTPI_ITEM_IMAGES - 1) {
            size = item_image_offsets[i + 1][1] - offset;
        } else {
            size = LAST_IMAGE_SIZE;
        }

        image->systype = SYS_TI994A;
        image->usage = IMG_ROOM_OBJ;
        image->index = item_image_offsets[i][0];

        image = finalize_img_make_new(image, offset, size, grom);
    }

    /* The loop always leaves one empty trailing USImage node.
       Unlink and free it so the list ends cleanly. */
    if (image->imagedata == NULL)
        FreeTrailingEmptyImage(image);
}

typedef struct {
    const char *filename;
    USImageType type;
    int index;
    size_t size;
} rtpi_image_record;

static const rtpi_image_record rtpi_disk_image_table[] = {
    { "PICT//O", IMG_ROOM,     26, 0x167 },
    { "PICT1/O", IMG_ROOM,      1, 0x2d5 },
    { "PICT2/O", IMG_ROOM,      2, 0x21c },
    { "PICT3/O", IMG_ROOM,      3, 0x288 },
    { "PICT4/O", IMG_ROOM,      4, 0x1c0 },
    { "PICT5/O", IMG_ROOM,      5, 0x15c },
    { "PICT6/O", IMG_ROOM,      6, 0x25b },
    { "PICT7/O", IMG_ROOM,      7, 0x171 },
    { "PICT:/O", IMG_ROOM,     10, 0x148 },
    { "PICT;/O", IMG_ROOM,     11, 0x0f6 },
    { "PICT</O", IMG_ROOM,     12, 0x233 },
    { "PICT=/O", IMG_ROOM,     13, 0x125 },
    { "PICT?/O", IMG_ROOM,     15, 0x1f0 },
    { "PICT@/O", IMG_ROOM,     16, 0x157 },
    { "PICTA/O", IMG_ROOM,     17, 0x180 },
    { "PICTB/O", IMG_ROOM,     18, 0x27b },
    { "PICTC/O", IMG_ROOM,     19, 0x0ed },
    { "PICTD/O", IMG_ROOM,     20, 0x0e5 },
    { "PICTE/O", IMG_ROOM,     21, 0x079 },
    { "PICT;/O", IMG_ROOM,     22, 0x0f6 },
    { "PICTG/O", IMG_ROOM,     23, 0x29d },
    { "PICTH/O", IMG_ROOM,     24, 0x2a2 },
    { "PICB5/O", IMG_ROOM_OBJ,  4, 0x0d5 },
    { "PICTI/O", IMG_ROOM,     99, 0x2b0 },
    { "PICTK/O", IMG_ROOM_OBJ, 29, 0x0df },
    { "PICB9/O", IMG_ROOM,     30, 0x065 },
    { "PICBA/O", IMG_ROOM,     31, 0x065 },
    { "PICB1/O", IMG_ROOM_OBJ, 63, 0x181 },
    { "PICB2/O", IMG_ROOM_OBJ, 26, 0x0f9 },
    { "PICB3/O", IMG_ROOM_OBJ, 28, 0x0b0 },
    { "PICB4/O", IMG_ROOM_OBJ, 36, 0x061 },
    { "PICB6/O", IMG_ROOM_OBJ, 27, 0x198 },
    { "PICB7/O", IMG_ROOM_OBJ,  7, 0x061 },
    { "PICB8/O", IMG_ROOM_OBJ, 52, 0x072 },
    { "PICBB/O", IMG_ROOM_OBJ, 31, 0x036 },
    { "PICBE/O", IMG_ROOM,     33, 0x01e },
    { "PICBF/O", IMG_ROOM,     34, 0x024 },
    { "PICBG/O", IMG_ROOM,     35, 0x026 },
    { "PICBH/O", IMG_ROOM,     36, 0x01c },
    { "PICBI/O", IMG_ROOM,     37, 0x01c },
    {NULL, 0, 0, 0}
};

/* A picture file loads at an address of its own choosing (0x2700 on
   the known disks) and begins with a word giving the length of what
   follows. */
#define DSK_PICTURE_LEADER 2

/* The "ADV*RPI*CM" disk image has a damaged record in the middle of
   room 5's picture: a full record of 24 words, of which the first ten
   can be read. These are the other fourteen. */
#define ROOM_5_PATCH_OFFSET 0xfc
static const uint8_t rtpi_room_5_tile_patch[28] = {
    0xC4, 0x46, 0x4C, 0x00, 0xE0,
    0x05, 0x0F, 0x0C, 0xF5, 0x06, 0x5F, 0x03, 0x1F, 0x02, 0x2F, 0x04, 0x02,
    0x05, 0x0F, 0x06, 0xF5, 0x06, 0x15, 0x04, 0xF5, 0x05, 0x2F, 0x06
};

static int LoadRTPIGraphicsFromDSK(const uint8_t *dsk, size_t size, const ti99_disk_file *files) {
    USImages = NewImage();
    USImage *image = USImages;

    /* Walk the known image table and look up each entry on the disk */
    for (int i = 0; rtpi_disk_image_table[i].filename != NULL; i++) {
        const rtpi_image_record *rec = &rtpi_disk_image_table[i];

        ti99_object obj;
        if (!TI99LoadObject(dsk, size, files, rec->filename, &obj))
            continue;

        uint8_t *imagedata = TI99CopyObjectBytes(&obj, obj.low + DSK_PICTURE_LEADER, rec->size);
        int damaged = obj.damaged;
        TI99FreeObject(&obj);
        if (imagedata == NULL)
            continue;

        /* The data lost with the damaged record is a hole of zeros in
           the middle of the picture; the records after it say where
           they load, so the rest is in place. */
        if (damaged && rec->type == IMG_ROOM && rec->index == 5 &&
            rec->size >= ROOM_5_PATCH_OFFSET + sizeof(rtpi_room_5_tile_patch)) {
            memcpy(imagedata + ROOM_5_PATCH_OFFSET, rtpi_room_5_tile_patch, sizeof(rtpi_room_5_tile_patch));
        }

        image->systype = SYS_TI994A;
        image->usage = rec->type;
        image->index = rec->index;
        image->datasize = rec->size;
        image->imagedata = imagedata;

        /* Append a new node for the next image */
        image = AppendImage(image);
    }

    /* The loop always leaves one empty trailing USImage node.
       Unlink and free it so the list ends cleanly. */
    if (image->imagedata == NULL)
        FreeTrailingEmptyImage(image);

    return 1;
}


/* Sanity bound for word_length to prevent runaway parsing if the
   header value is bogus. */
#define RTPI_DICT_MAX_WORD_LENGTH 1000

/* Storage for one parsed dictionary word (one byte reserved for NUL). */
#define RTPI_DICT_WORD_BUF_SIZE 32

/* Trailing placeholder slots in Verbs[]/Nouns[] beyond number_of_words. */
#define RTPI_DICT_PADDING 2

/* End-of-dictionary marker: high bit set on the final byte. */
#define RTPI_DICT_END_THRESHOLD 0x7F

/* Parse the dictionary from the GROM data stream into the global
   Nouns[] and Verbs[] arrays.

   The dictionary is a flat byte stream of fixed-width words
   (info.word_length chars each). Nouns come first, then verbs. A '*'
   character marks a synonym: it restarts the current word entry so
   the synonym shares the same word number (the '*' prefix is preserved
   in the stored string, as the game engine uses it to identify synonyms).
   Trailing spaces within a word are collapsed. A byte with bit 7 set
   signals the end of the dictionary. */
static uint8_t *ReadRTPIDictionary(const GameInfo *info, uint8_t *ptr, uint8_t *endptr) {
    int wl = info->word_length;
    if (wl < 1 || wl > RTPI_DICT_MAX_WORD_LENGTH)
        Fatal("Bad word length");

    char dictword[RTPI_DICT_WORD_BUF_SIZE];

    int nw = info->number_of_words;
    int nv = info->number_of_verbs;
    int nn = info->number_of_nouns;

    /* Initialize all slots to "." (the Scott Adams placeholder for
       empty/unused dictionary entries). */
    for (int i = 0; i < nw + RTPI_DICT_PADDING; i++) {
        Verbs[i] = ".";
        Nouns[i] = ".";
    }

    for (int wordnum = 0; wordnum <= nv + nn; wordnum++) {
        int charindex = 0;
        int restarts = 0;
        char c = 0;
        for (int i = 0; i < wl; i++) {
            if (ptr >= endptr)
                return ptr;
            c = *ptr++;

            /* Skip leading NUL bytes (padding between entries). */
            if (c == 0 && charindex == 0) {
                if (ptr >= endptr)
                    return ptr;
                c = *ptr++;
            }
            /* Collapse trailing spaces: if the previous char was a space
               and the current one isn't, back up to overwrite the space. */
            if (c != ' ' && charindex > 0 && dictword[charindex - 1] == ' ') {
                i--;
                charindex--;
            }
            /* '*' marks a synonym — reset the word buffer and restart
               reading so this entry replaces the previous one at the
               same word number. The '*' prefix is preserved. */
            if (c == '*') {
                charindex = 0;
                i = -1;
                if (++restarts > wl)
                    break;
            }
            if (charindex < RTPI_DICT_WORD_BUF_SIZE - 1)
                dictword[charindex++] = c;
        }
        dictword[charindex] = 0;
        char *word = MemAlloc(charindex + 1);
        memcpy(word, dictword, charindex + 1);
        if (wordnum < nn) {
            Nouns[wordnum] = word;
            debug_print("Noun %d: \"%s\"\n", wordnum, word);
        } else if (wordnum - nn < nw + RTPI_DICT_PADDING) {
            Verbs[wordnum - nn] = word;
            debug_print("Verb %d: \"%s\"\n", wordnum - nn, word);
        }

        /* A byte with the high bit set terminates the dictionary. */
        if (c > RTPI_DICT_END_THRESHOLD)
            return ptr;
    }

    return ptr;
}

/* Load all game data (dictionary, rooms, messages, items, actions,
   room connections) from the GROM data for Return to Pirate's Isle.

   The data is at absolute offsets within the GROM image (specified in
   the GameInfo struct). Each section is parsed sequentially, with
   bounds checks between sections. Returns the game ID on success, or
   UNKNOWN_GAME if any section extends past the data boundary. */
static GameIDType LoadRTPI(uint8_t *data, size_t length, GameInfo info)
{
    uint8_t *ptr;
    uint8_t *endptr = data + length;

// MARK: dictionary

    ptr = data + info.start_of_dictionary;
    if (ptr >= endptr)
        return FreeGameResources();

    ReadRTPIDictionary(&info, ptr, endptr);

// MARK: rooms

    ptr = data + info.start_of_room_descriptions;
    if (ptr >= endptr)
        return FreeGameResources();

    ptr = ParseRooms(ptr, endptr, GameHeader.NumRooms);

// MARK: messages

    if (ptr >= endptr)
        return FreeGameResources();

    ptr = ParseMessages(ptr, endptr, GameHeader.NumMessages);

// MARK: items

    if (ptr >= endptr)
        return FreeGameResources();


    ptr = ParseItems(ptr, endptr, GameHeader.NumItems, 1);

// MARK: actions

    ptr += 0x90;

    if (ptr >= endptr)
        return FreeGameResources();

    ptr = ParseActions(ptr, data, length, GameHeader.NumActions, 1);
    if (ptr == NULL)
        return FreeGameResources();

    ptr += 0x32;

    if (ptr >= endptr)
        return FreeGameResources();

// MARK: room connections

    ParseRoomConnections(ptr, endptr, GameHeader.NumRooms, 1);

    return info.gameID;
}

/* Translate a TI-99/4A GROM address into a file offset.

   In the original hardware, GROM addresses start at 0x6000. The file
   image starts at offset 0, so we subtract 0x6000 from any address
   greater than or equal to 0x8000, which are in the range the game uses.
   Addresses below 0x8000 are left unchanged. */
static inline uint16_t adjust_grom_offset(uint16_t offset) {
    return (offset >= 0x8000) ? offset - 0x6000 : offset;
}

/* To make the game data contiguous, we cut out all the grom headers
   and arrange the banks in order 1, 2, 3, 4, 0. The output buffer
   is always GROM_FLAT_SIZE (0x77f6) bytes. */
static uint8_t *CutOutGromHeaders(uint8_t *grom) {
    uint8_t *result = MemAlloc(GROM_FLAT_SIZE);

    for (size_t i = 0; i < NUM_GROM_BANKS; i++) {
        /* Bank order 1, 2, 3, 4, 0. */
        size_t src_bank = (i + 1) % NUM_GROM_BANKS;
        memcpy(result + i * GROM_BANK_DATA_SIZE, grom + src_bank * GROM_BANK_SIZE, GROM_BANK_DATA_SIZE);
    }

    free(grom);
    return result;
}

/* Byte offsets within the c.bin (CPU ROM) file where the image offset
   tables begin. ROOM_IMAGES_OFFSET points to 33 big-endian 16-bit
   GROM addresses (one per room image). ITEM_IMAGES_OFFSET points to
   7 pairs of (room_index, GROM_address), each 4 bytes. */
#define ROOM_IMAGES_OFFSET 0x12
#define ITEM_IMAGES_OFFSET 0x54

/* Minimum c.bin size for all of the embedded tables, title text and
   tile font to fit. */
#define CPU_ROM_MIN_SIZE          0x1e94

/* Byte offsets within the c.bin (CPU ROM) of additional payloads. */
#define CPU_ROM_TITLE_TEXT_OFFSET 0x13db
#define CPU_ROM_TILE_FONT_OFFSET  0x1cf4

/* Each ITEM_IMAGES_OFFSET entry is 4 bytes: a 2-byte room_index
   followed by a 2-byte GROM address. */
#define ITEM_IMAGE_ENTRY_SIZE 4

/* Number of int fields in the synthetic Scott Adams header. */
#define RTPI_RAW_HEADER_SIZE 25

/* Disk-image (.dsk) distribution layout: game data is stored as
   DSK_RECORD_SIZE-byte records on DSK_SECTOR_SIZE-byte sectors,
   starting at DSK_GROM_SRC_START in the disk image and placed
   into the rebuilt GROM buffer starting at DSK_GROM_DST_START.
   The title screen and tile font are data in the object files of two
   of the program's modules, at these addresses within them. */
#define DSK_GROM_DST_START    0x3d79
#define DSK_GROM_DST_END      0x76f6
#define DSK_GROM_SRC_START    0x2200
#define DSK_RECORD_SIZE       0xfe
#define DSK_SECTOR_SIZE       0x100
#define DSK_TITLE_TEXT_FILE    "ADV5/O"
#define DSK_TITLE_TEXT_ADDRESS 0x01e4
#define DSK_TILE_FONT_FILE     "ADV6/O"
#define DSK_TILE_FONT_ADDRESS  0x0370

/* GameInfo describing the GROM layout. The offsets are raw byte
   positions within the GROM file. Graphics data is handled
   separately via LoadRTPIGraphics, so the image-related fields
   are all zero. */
static const GameInfo rtpi_info = {
    "Return to Pirate's Isle",
    RETURN_TO_PIRATES_ISLE,
    US_VARIANT,               // type
    ENGLISH,                  // subtype
    FOUR_LETTER_UNCOMPRESSED, // dictionary type

    71,  // Number of items
    277, // Number of actions
    104, // Number of words
    26,  // Number of rooms
    10,  // Max carried items
    4,   // Word length
    89,  // Number of messages

    103, // number_of_verbs
    105, // number_of_nouns

    0,   // no header
    NO_HEADER,  // no header style

    0,   // no room images
    0,   // no item flags
    0,   // no item images

    0x4F6C, // actions
    UNCOMPRESSED,
    0x3d98,  // dictionary
    0x41B1,  // start_of_room_descriptions
    0x60fE,  // start_of_room_connections
    0x438D,  // start_of_messages
    FOLLOWS, // start_of_item_descriptions
    FOLLOWS, // start_of_item_locations

    0, // start_of_system_messages
    0, // start of directions

    0, // start_of_tiles
    0, // start_of_image_data
    0, // image_address_offset
    0, // number_of_pictures
    0, // palette
    0, // picture_format_version
};

/* Replace embedded NULs with newlines so a multi-line blob can be
   displayed as a single string, then NUL-terminate at the end. */
static void replace_nuls_with_newlines(char *str, size_t length) {
    for (size_t i = 0; i < length; i++) {
        if (str[i] == 0)
            str[i] = '\n';
    }
    str[length - 1] = '\0';
}

/* Load the zip-archive distribution: extract c.bin and g.bin (or
   the split MAME phm3189*.bin files), parse the image offset
   tables, extract title text and tile font, load image data, and
   produce the contiguous GROM buffer. Returns NULL on failure. */
static uint8_t *load_rtpi_zip_payload(uint8_t *data, size_t datalength) {
    size_t rom_length;

    /* Try to extract the CPU ROM, accepting either filename variant. */
    uint8_t *cpu_rom = extract_file_from_zip_data(data, datalength, "c.bin", &rom_length);
    if (!cpu_rom) {
        cpu_rom = extract_file_from_zip_data(data, datalength, "phm3189c.bin", &rom_length);
        if (!cpu_rom)
            return NULL;
    }

    if (rom_length < CPU_ROM_MIN_SIZE) {
        free(cpu_rom);
        return NULL;
    }

    /* Extract room and item image offset tables from the CPU ROM.
       Room offsets are 33 big-endian 16-bit GROM addresses at 0x12.
       Item offsets are 7 (room_index, GROM_address) pairs at 0x54. */
    uint16_t room_image_offsets[NUM_RTPI_ROOM_IMAGES];
    uint16_t item_image_offsets[NUM_RTPI_ITEM_IMAGES][2];

    for (int i = 0; i < NUM_RTPI_ROOM_IMAGES; i++) {
        room_image_offsets[i] = adjust_grom_offset(READ_BE_UINT16(cpu_rom + ROOM_IMAGES_OFFSET + i * 2));
    }
    for (int i = 0; i < NUM_RTPI_ITEM_IMAGES; i++) {
        item_image_offsets[i][0] = READ_BE_UINT16(cpu_rom + ITEM_IMAGES_OFFSET + i * ITEM_IMAGE_ENTRY_SIZE);
        item_image_offsets[i][1] = adjust_grom_offset(READ_BE_UINT16(cpu_rom + ITEM_IMAGES_OFFSET + i * ITEM_IMAGE_ENTRY_SIZE + 2));
    }

    /* Extract the title screen text. */
    title_screen = MemAlloc(RTPI_TITLE_TEXT_LENGTH);
    memcpy((char *)title_screen, cpu_rom + CPU_ROM_TITLE_TEXT_OFFSET, RTPI_TITLE_TEXT_LENGTH);
    replace_nuls_with_newlines((char *)title_screen, RTPI_TITLE_TEXT_LENGTH);

    /* Extract the 52-tile font (8 bytes per tile). */
    memcpy(rtpi_tile_font, cpu_rom + CPU_ROM_TILE_FONT_OFFSET, NUM_RTPI_TILES * 8);

    free(cpu_rom);

    /* Load the GROM data. Try the single g.bin file first. */
    size_t grom_length;
    uint8_t *grom_data = extract_file_from_zip_data(data, datalength, "g.bin", &grom_length);

    /* If g.bin is not present, this may be the MAME zip format which
       splits the GROM across NUM_GROM_BANKS bank files (g3–g7). */
    if (!grom_data) {
        grom_data = MemCalloc(GROM_SIZE);
        char filename[RTPI_GROM_FILENAME_BUF_SIZE];
        size_t bank_length;
        for (int i = 0; i < NUM_GROM_BANKS; i++) {
            snprintf(filename, RTPI_GROM_FILENAME_BUF_SIZE, "phm3189g%d.bin", FIRST_GROM_BANK_NUMBER + i);
            uint8_t *bank = extract_file_from_zip_data(data, datalength, filename, &bank_length);
            if (!bank) {
                free(grom_data);
                return NULL;
            }
            memcpy(grom_data + GROM_BANK_SIZE * i, bank, bank_length);
            free(bank);
        }
    }

    /* Image positions are relative to the raw banks, so copy them
       out before CutOutGromHeaders re-arranges the buffer. */
    LoadRTPIGraphics(room_image_offsets, item_image_offsets, grom_data);
    return CutOutGromHeaders(grom_data);
}

/* Load the disk-image distribution: rebuild the GROM buffer from
   on-disk records and extract title text and tile font. */
static uint8_t *load_rtpi_disk_payload(uint8_t *data, size_t datalength) {
    uint8_t *grom_data = MemAlloc(GROM_FLAT_SIZE);
    size_t dst_off = DSK_GROM_DST_START;
    size_t src_off = DSK_GROM_SRC_START;
    while (dst_off < DSK_GROM_DST_END) {
        memcpy(grom_data + dst_off, data + src_off, DSK_RECORD_SIZE);
        src_off += DSK_SECTOR_SIZE;
        dst_off += DSK_RECORD_SIZE;
    }

    /* Without a directory there is still the game, with no pictures. */
    ti99_disk_file *files = TI99ReadDirectory(data, datalength);
    if (files == NULL)
        return grom_data;

    /* The title text and the tile font are data in two of the
       program's modules. */
    ti99_object obj;
    if (TI99LoadObject(data, datalength, files, DSK_TITLE_TEXT_FILE, &obj)) {
        title_screen = (char *)TI99CopyObjectBytes(&obj, DSK_TITLE_TEXT_ADDRESS, RTPI_TITLE_TEXT_LENGTH);
        TI99FreeObject(&obj);
        if (title_screen != NULL)
            replace_nuls_with_newlines(title_screen, RTPI_TITLE_TEXT_LENGTH);
    }

    /* Extract the 52-tile font (8 bytes per tile). */
    if (TI99LoadObject(data, datalength, files, DSK_TILE_FONT_FILE, &obj)) {
        uint8_t *flat_tiles = TI99CopyObjectBytes(&obj, DSK_TILE_FONT_ADDRESS, NUM_RTPI_TILES * 8);
        TI99FreeObject(&obj);
        if (flat_tiles != NULL) {
            memcpy(rtpi_tile_font, flat_tiles, NUM_RTPI_TILES * 8);
            free(flat_tiles);
        }
    }

    LoadRTPIGraphicsFromDSK(data, datalength, files);
    TI99FreeDirectory(files);
    return grom_data;
}

/* Detect and load the TI-99/4A "Return to Pirate's Isle" game.
   Supports the zip-archive distribution (c.bin + g.bin or the
   MAME split phm3189*.bin layout) and the .dsk disk image.

   On success, all game data structures are populated and the
   function returns RETURN_TO_PIRATES_ISLE. On failure (missing
   files, bad data) it returns UNKNOWN_GAME. */
GameIDType DetectRTPI(uint8_t *data, size_t datalength) {
    int is_disk = !memcmp(entire_file, "PIRATE", 6) || !memcmp(entire_file, "ADV*RPI*CM", 10) || !memcmp(entire_file, "RTNPIRAT", 8);

    uint8_t *grom_data = is_disk ? load_rtpi_disk_payload(data, datalength)
                                 : load_rtpi_zip_payload(data, datalength);
    if (!grom_data)
        return UNKNOWN_GAME;

    /* I have been unable to find a standard Scott Adams header in
       the data, so all game parameters are hardcoded here to match
       those in the adv14a.dat file created by Paul David Doherty
       (likely created by analyzing the same game data we are
       dealing with here.)
       See https://unbox.ifarchive.org/?url=/if-archive/scott-adams/games/scottfree/AdamsGames.zip
     */
    int raw_header[RTPI_RAW_HEADER_SIZE] = {
        4,       // Word length
        104,     // Number of words
        277,     // Number of actions
        71,      // Number of items
        89,      // Number of messages
        24,      // Number of rooms
        10,      // Max carried
        1,       // Starting location
        13,      // Number of treasures
        1,       // Light time
        13 << 8, // Treasure room
    };

    Header hdr;
    ParseHeader(raw_header, US_HEADER, &hdr);
    SetGameHeader(&hdr);
    /* Zero-filled, and never short of Items[LIGHT_SOURCE]: see
       AllocateGameData(). The dictionary padding matches RTPI_DICT_PADDING. */
    AllocateGameData();
    PrintHeaderInfo(raw_header, &hdr);

    /* Parse all game data from the GROM, then set up the TI-99/4A
       colour table. The original zip data is freed here since
       it's no longer needed once the GROM has been loaded. */
    if (LoadRTPI(grom_data, GROM_FLAT_SIZE, rtpi_info) == RETURN_TO_PIRATES_ISLE) {
        free(data);
        free(grom_data);
        SetupRTPIColors();
        return RETURN_TO_PIRATES_ISLE;
    }

    free(grom_data);
    return UNKNOWN_GAME;
}

/* Override the default Scott Adams system messages with the text used
   by the TI-99/4A version of Return to Pirate's Isle. These differ
   from the standard messages in wording and punctuation. */
void UpdateRTPISystemMessages(void) {
    sys[TOO_DARK_TO_SEE] = "I can't SEE!\n";
    sys[YOU_SEE] = ". Some visible items: ";
    sys[EXITS] = "Obvious exits: ";
    sys[YOU_FELL_AND_BROKE_YOUR_NECK] = "Fell & Thanks a lot; I'm Dead! ";
    sys[DANGEROUS_TO_MOVE_IN_DARK] = "Dangerous move! ";
    sys[WHAT_NOW] = "-> Command me: ";
    sys[YOUVE_SOLVED_IT] = "Congrats! You did it!\n";
}
