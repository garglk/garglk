//
//  zxscreen.c
//
//  Part of the decompressz80 library: the layout of the ZX Spectrum's screen
//  memory and its colours. See decompressz80.h.
//
//  Created by Petter Sjölund on 2022-01-24.
//

#include <stdint.h>

#include "decompressz80.h"

const uint32_t ZXPalette[16] = {
    0x000000, 0x0000D7, 0xD70000, 0xD700D7, 0x00D700, 0x00D7D7, 0xD7D700, 0xD7D7D7,
    0x000000, 0x0000FF, 0xFF0000, 0xFF00FF, 0x00FF00, 0x00FFFF, 0xFFFF00, 0xFFFFFF
};

/* A bitmap address is 010T TLLL RRRC CCCC: T is the third of the screen, R the
   character row within the third, L the pixel line within the row and C the
   column. The pixel row from the top is TTRRRLLL. */
int ZXBitmapRow(int bitmap_offset)
{
    return ((bitmap_offset & 0x0700) >> 8)
         | ((bitmap_offset & 0x00e0) >> 2)
         | ((bitmap_offset & 0x1800) >> 5);
}

int ZXBitmapOffset(int y, int col)
{
    return ((y & 0xc0) << 5) | ((y & 0x38) << 2) | ((y & 0x07) << 8) | col;
}

/* An attribute is flash, bright, three bits of paper and three of ink */
void ZXDecodeAttr(uint8_t attr, int *ink, int *paper)
{
    int bright = (attr & 0x40) ? 8 : 0;
    if (ink)
        *ink = (attr & 0x07) + bright;
    if (paper)
        *paper = ((attr >> 3) & 0x07) + bright;
}

int ZXScreenIsBlackOnWhite(const uint8_t *screen)
{
    if (!screen)
        return 1;
    for (int i = ZX_BITMAP_SIZE; i < ZX_SCREEN_SIZE; i++)
        if ((screen[i] & 0x3f) != 0x38)
            return 0;
    return 1;
}
