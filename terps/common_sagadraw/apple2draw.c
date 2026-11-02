//
//  apple2draw.c
//
//  Apple 2 bitmap graphics drawing for ScottFree and Plus,
//  two interpreters for adventures in Scott Adams formats
//
//  Created by Petter Sjölund on 2022-08-18.
//

#include <string.h>

#include "glk.h"
#include "debugprint.h"
#include "minmax.h"
#include "memory_allocation.h"

#include "apple2draw.h"
#include "gm_artifact.h"
#include "gm_hires.h"
#include "sagadraw_host.h"

uint8_t *descrambletable = NULL;
uint8_t *screenmem = NULL;

static void EnsureScreenMem(void)
{
    if (screenmem == NULL) {
        screenmem = MemCalloc(A2_SCREEN_MEM_SIZE);
    }
}

void ClearApple2ScreenMem(void)
{
    if (screenmem) {
        memset(screenmem, 0, A2_SCREEN_MEM_SIZE);
    }
}

static int WriteByteAndAdvanceY(uint8_t x, uint8_t *y, uint8_t byte_to_write)
{
    uint16_t address = gm_row_address(*y) + x;
    if (address > GM_MAX_SCREEN_ADDR) {
        debug_print("apple2draw WriteByteAt() ERROR: Attempt to write outside Apple 2 screen memory at offset 0x%04x. Max: 0x%04x\n", address, GM_MAX_SCREEN_ADDR);
        return 0;
    }
    screenmem[address] = byte_to_write;
    (*y)++;
    return 1;
}

// Returns the screenmem offset of the start of row ypos, or -1 if the
// descramble table has no valid entry for it.
static int DescrambleScreenAddress(uint8_t ypos)
{
    if (0xc0 + ypos >= 0x182)
        return -1;
    // The descrambletable values are copied from the disk image, so we must subtract
    // the offset of the Apple 2 screen memory (0x2000), in order to convert it to an index
    // for our screenmem array (starting at 0 rather than 0x2000).
    uint16_t result = ((descrambletable[0xc0 + ypos] << 8) | descrambletable[ypos]) - 0x2000;
    if (result > GM_MAX_SCREEN_ADDR)
        return -1;
    return result;
}

static void WriteScrambledByte(uint8_t xpos, uint8_t ypos, uint8_t byte_to_write)
{
    int address = DescrambleScreenAddress(ypos);
    if (address < 0 || address + xpos > GM_MAX_SCREEN_ADDR) {
        debug_print("apple2draw: scrambled write outside screen memory (x %d, y %d)\n", xpos, ypos);
        return;
    }
    screenmem[address + xpos] = byte_to_write;
}

static size_t DrawScrambledApple2Image(uint8_t *ptr, size_t datasize)
{
    uint8_t *origptr = ptr;
    uint8_t *end_of_data = ptr + datasize;

    uint8_t x_origin = *ptr++;
    uint8_t y_origin = *ptr++;
    uint8_t width = *ptr++;
    uint8_t height = *ptr++;

    uint8_t xpos = x_origin;
    uint8_t ypos = y_origin;

    if (ypos > 0xc0) {
        return 0;
    }

    while (ptr < end_of_data - 1) {
        uint8_t repeats = 1;
        uint8_t work1 = *ptr++;
        if (work1 == 0) {
            if (ptr > end_of_data - 3) {
                return ptr - origptr;
            }
            repeats = *ptr++;
            work1 = *ptr++;
        }

        uint8_t work2 = *ptr++;

        if (repeats == 0) {
            repeats = 1;
        }

        while (repeats--) {
            WriteScrambledByte(xpos, ypos, work1);
            ypos++;
            WriteScrambledByte(xpos, ypos, work2);
            ypos++;
            if (height <= ypos) {
                ypos = y_origin;
                xpos++;
                if (width <= xpos) {
                    return ptr - origptr;
                }
            }
        }
    }
    return ptr - origptr;
}

int DrawApple2ImageFromData(uint8_t *ptr, size_t datasize, enum apple2_variant variant, adjust_plus_fn adjust_plus)
{
    uint8_t pattern_top = 0, pattern_bottom = 0;
    uint8_t repetitions;

    uint8_t xpos = 0;
    uint8_t ypos = 0;

    uint8_t left = 0, top, right, bottom;

    // Both layouts start with at least a four-byte header
    if (ptr == NULL || datasize < 4)
        return 0;

    uint8_t *origptr = ptr;
    uint8_t *end_of_data = ptr + datasize;

    EnsureScreenMem();

    if (descrambletable) {
        size_t result = DrawScrambledApple2Image(ptr, datasize);
        debug_print("Stopped drawing at offset %zx\n", result);
        return (result > 0);
    }

    int counted_rle = variant == APPLE2_COUNT;

    if (!counted_rle) {
        // Get the origin
        left = *ptr++;
        top = *ptr++;
        xpos = left;
        ypos = top;

        // Get the dimensions
        right = *ptr++;
        bottom = *ptr++;
    } else {
        top = 0;
        right = 0;
        bottom = 0;
    }

    // Skip zero padding before the dimensions
    while (right == 0 && bottom == 0) {
        if (end_of_data - ptr < 2)
            return 1;
        right = *ptr++;
        bottom = *ptr++;
    }

    debug_print("width: %d height: %d\n", right, bottom);

    // Graphics window adjustments if we are being called
    // from Plus. (In the ScottFree games using this image
    // format, the images are all the same size.)
    if (adjust_plus != NULL) {
        adjust_plus(right, bottom, top);
    }

    while ((size_t)(ptr - origptr) < datasize - 2) {
        // First get repetitions
        repetitions = *ptr++;

        // In The Count every run is a count followed by the two bytes to
        // repeat. In the other games that is only so when bit 7 of the count
        // is set; otherwise the count is the number of literal byte pairs
        // that follow, minus one.
        int repeat_next_two_bytes = 0;
        if (counted_rle) {
            repeat_next_two_bytes = 1;
            repetitions -= 1;
        } else if (repetitions & 0x80) {
            repeat_next_two_bytes = 1;
            repetitions &= 0x7f;
        }
        if (repeat_next_two_bytes) {
            pattern_top = *ptr++;
            pattern_bottom = *ptr++;
        }
        for (int i = 0; i < repetitions + 1; i++) {
            // If we are not repeating bytes, we simply
            // draw the next (repetitions + 1) * 2 bytes,
            // i.e. we read two new bytes on every iteration
            if (!repeat_next_two_bytes) {
                if ((size_t)(ptr - origptr) >= datasize - 1) {
                    debug_print("Reached end of image data at offset %lx\n", ptr - origptr);
                    return 1;
                }
                pattern_top = *ptr++;
                pattern_bottom = *ptr++;
            }

            if (!WriteByteAndAdvanceY(xpos, &ypos, pattern_top) ||
                !WriteByteAndAdvanceY(xpos, &ypos, pattern_bottom)) {
                return 1;
            }
            if (ypos - 2 >= bottom && xpos < right) {
                xpos++;
                ypos = top;
            }
            if (xpos > right || ypos > bottom) {
                debug_print("Reached end of image dimensions at offset %lx\n", ptr - origptr);
                return 1;
            }
        }
    }
    return 1;
}

// One hi-res scanline is 560 artifact-colour pixels wide, plotted as two
// Glk rows so that the 192 scanlines keep the Apple's 4:3 aspect.
#define A2_LINE_PIXELS (GM_HIRES_COLS * 14)
#define A2_PLOT_HEIGHT 320

// Fills pixels xpos .. xpos + width - 1 of plot row ypos. Upside down, the
// whole picture is mirrored both ways about the 560x320 plot area.
static void PutApplePixels(glsi32 xpos, glsi32 ypos, glui32 color, int width, int upside_down)
{
    if (upside_down) {
        xpos = A2_LINE_PIXELS - xpos - width;
        ypos = A2_PLOT_HEIGHT - 2 - ypos;
    }
    glk_window_fill_rect(Graphics, color, xpos * pixel_size + x_offset,
        ypos * pixel_size + y_offset, pixel_size * width, pixel_size * 2);
}

// The NTSC artifact-colour kernel (artifact LUT, palette, Double7Bits, per-row
// word + colour expansion) lives in common_sagadraw/gm_artifact.c, shared with
// the Comprehend renderer. This file keeps only the Glk plotting on top of it.

// Draws one scanline, one fill_rect per run of same-coloured pixels.
static void RenderLineWithA2ArtifactColors(uint16_t const *in, int row, int upside_down)
{
    uint8_t colors560[A2_LINE_PIXELS];
    gm_render_line_colors(in, colors560);

    int start = 0;
    for (int x = 1; x <= A2_LINE_PIXELS; x++) {
        if (x == A2_LINE_PIXELS || colors560[x] != colors560[start]) {
            PutApplePixels(start, row * 2, gm_apple2_palette[colors560[start]], x - start, upside_down);
            start = x;
        }
    }
}

void DrawApple2ImageFromVideoMemWithFlip(int upside_down)
{
    if (screenmem == NULL)
        return;

    // The picture is the top 160 scanlines. The Apple shows the hi-res page
    // in mixed mode, so rows 160 to 191 are hidden behind the text window
    // even though the vector pictures draw into them.
    int rows = MIN(ImageHeight / 2, 160);

    for (int row = 0; row < rows; row++) {
        uint8_t const *const vram_row = screenmem + gm_row_address(row);
        uint16_t words[40];
        gm_compute_row_words(vram_row, words);
        RenderLineWithA2ArtifactColors(words, row, upside_down);
    }
}

void DrawSingleApple2ImageByte(uint8_t *mem, size_t offset)
{
    // Offsets in the 8-byte "screen holes" belong to no scanline
    if (mem == NULL || offset > GM_MAX_SCREEN_ADDR || (offset & 0x7f) >= 3 * GM_HIRES_COLS)
        return;

    int row = gm_row_from_offset((unsigned)offset);
    int col = (offset & 0x7f) % 40;

    unsigned const row_offset = (unsigned)offset - col;
    uint8_t const *const vram_row = mem + row_offset;

    uint16_t words[40];
    gm_compute_row_words(vram_row, words);
    RenderLineWithA2ArtifactColors(words, row, 0);
}

void DrawApple2ImageFromVideoMem(void)
{
    DrawApple2ImageFromVideoMemWithFlip(0);
}
