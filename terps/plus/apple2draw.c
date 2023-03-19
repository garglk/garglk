//
//  apple2draw.c
//  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
//
//  Created by Petter Sjölund on 2022-08-18.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "definitions.h"
#include "glk.h"
#include "graphics.h"

static uint8_t *screenmem = NULL;
static uint8_t lobyte = 0, hibyte = 0;

void ClearApple2ScreenMem(void)
{
    if (screenmem)
        memset(screenmem, 0, 0x2000);
}

static void AdvanceScreenByte(void)
{
    lobyte = ((y & 0xc0) >> 2 | (y & 0xc0)) >> 1 | (y & 8) << 4;
    hibyte = ((y & 7) << 1 | (uint8_t)(y << 2) >> 7) << 1 | (uint8_t)(y << 3) >> 7;
}

static void PutByte(uint8_t work, uint8_t work2)
{
    AdvanceScreenByte();
    screenmem[hibyte * 0x100 + lobyte + x] = work;
    y++;
    AdvanceScreenByte();
    screenmem[hibyte * 0x100 + lobyte + x] = work2;
    y++;
    if (y - 2 >= ylen) {
        if (x == xlen) {
            return;
        }
        x++;
        y = yoff;
    }
    return;
}

void DrawApple2ImageFromVideoMem(void);

int DrawApple2ImageFromData(uint8_t *ptr, size_t datasize)
{
    int work, work2;
    int c;
    int i;
    size_t size;

    uint8_t *origptr = ptr;

    if (screenmem == NULL) {
        screenmem = MemAlloc(0x2000);
        ClearApple2ScreenMem();
    }

    x = 0;
    y = 0;

    work = *ptr++;
    size = work + (*ptr++ * 256);

    // Get the offsets
    xoff = *ptr++;
    yoff = *ptr++;
    x = xoff;
    y = yoff;

    // Get the x length
    xlen = *ptr++;
    ylen = *ptr++;

    glui32 curheight, curwidth;
    glk_window_get_size(Graphics, &curwidth, &curheight);

    if (yoff == 0 && (LastImgType == IMG_ROOM || LastImgType == IMG_SPECIAL)) {
        ImageHeight = ylen + 2;
        ImageWidth = xlen * 8 - 32;
    }

    int optimal_height = ImageHeight * pixel_size;
    if (curheight != optimal_height && ImageWidth * pixel_size <= curwidth) {
        x_offset = (curwidth - ImageWidth * pixel_size) / 2;
        right_margin = (ImageWidth * pixel_size) + x_offset;
        winid_t parent = glk_window_get_parent(Graphics);
        if (parent) {
            glk_window_set_arrangement(parent, winmethod_Above | winmethod_Fixed,
                optimal_height, NULL);
        }
    }

    while (ptr - origptr < size) {
        // First get count
        c = *ptr++;

        if ((c & 0x80) == 0x80) { // is a counter
            c &= 0x7f;
            work = *ptr++;
            work2 = *ptr++;
            for (i = 0; i < c + 1 && ptr - origptr < size; i++) {
                if (hibyte * 0x100 + lobyte + x > 0x1fff)
                    return 0;
                PutByte(work, work2);
                if (x > xlen || y > ylen) {
                    return 1;
                }
            }
        } else {
            // Don't count on the next j characters

            for (i = 0; i < c + 1 && ptr - origptr < size; i++) {
                work = *ptr++;
                work2 = *ptr++;
                if (hibyte * 0x100 + lobyte + x > 0x1fff)
                    return 0;
                PutByte(work, work2);
                if (x > xlen || y > ylen) {
                    return 1;
                }
            }
        }
    }
    return 1;
}

static void PutApplePixel(glsi32 xpos, glsi32 ypos, glui32 color)
{
    xpos = xpos * pixel_size;

    if (upside_down)
        xpos = 280 * pixel_size - xpos;
    xpos += x_offset;

    if (xpos < x_offset || xpos >= right_margin) {
        return;
    }

    ypos = ypos * pixel_size;
    if (upside_down)
        ypos = 157 * pixel_size - ypos;
    ypos += y_offset;

    glk_window_fill_rect(Graphics, color, xpos,
        ypos, pixel_size, pixel_size);
}

// clang-format off
#define BLACK   0
#define PURPLE  0xd53ef9
#define BLUE    0x458ff7
#define ORANGE  0xd7762c
#define GREEN   0x64d440
#define WHITE   0xffffff

static const int32_t hires_artifact_color_table[] =
{
    BLACK,  PURPLE, GREEN,  WHITE,
    BLACK,  BLUE,   ORANGE, WHITE
};
// clang-format on

static int32_t *m_hires_artifact_map = NULL;

// This function and the one below are adapted from MAME

static void generate_artifact_map(void)
{
    // generate hi-res artifact data
    int i, j;
    uint16_t c;

    /* 2^3 dependent pixels * 2 color sets * 2 offsets */
    m_hires_artifact_map = MemAlloc(sizeof(int32_t) * 8 * 2 * 2);

    /* build hires artifact map */
    for (i = 0; i < 8; i++) {
        for (j = 0; j < 2; j++) {
            if (i & 0x02) {
                if ((i & 0x05) != 0)
                    c = 3;
                else
                    c = j ? 2 : 1;
            } else {
                if ((i & 0x05) == 0x05)
                    c = j ? 1 : 2;
                else
                    c = 0;
            }
            m_hires_artifact_map[0 + j * 8 + i] = hires_artifact_color_table[(c + 0) % 8];
            m_hires_artifact_map[16 + j * 8 + i] = hires_artifact_color_table[(c + 4) % 8];
        }
    }
}

void DrawApple2ImageFromVideoMem(void)
{
    if (m_hires_artifact_map == NULL)
        generate_artifact_map();

    int32_t *artifact_map_ptr;

    uint8_t const *const vram = screenmem;
    uint8_t vram_row[42];
    vram_row[0] = 0;
    vram_row[41] = 0;

    for (int row = 0; row < 192; row++) {
        for (int col = 0; col < 40; col++) {
            int const offset = ((((row / 8) & 0x07) << 7) | (((row / 8) & 0x18) * 5 + col)) | ((row & 7) << 10);
            vram_row[1 + col] = vram[offset];
        }

        int pixpos = 0;

        for (int col = 0; col < 40; col++) {
            uint32_t w = (((uint32_t)vram_row[col + 0] & 0x7f) << 0)
                | (((uint32_t)vram_row[col + 1] & 0x7f) << 7)
                | (((uint32_t)vram_row[col + 2] & 0x7f) << 14);

            artifact_map_ptr = &m_hires_artifact_map[((vram_row[col + 1] & 0x80) >> 7) * 16];

            for (int b = 0; b < 7; b++) {
                int32_t const v = artifact_map_ptr[((w >> (b + 7 - 1)) & 0x07) | (((b ^ col) & 0x01) << 3)];
                PutApplePixel(pixpos++, row, v);
            }
        }
    }
}
