/*
 *	  This program is free software; you can redistribute it and/or
 *	  modify it under the terms of the GNU General Public License
 *	  as published by the Free Software Foundation; either version
 *	  2 of the License, or (at your option) any later version.
 */

/*
 *  c64a8scott.c
 *  Part of ScottFree, an interpreter for adventures in Scott Adams format
 *
 *  The ScottFree-specific code required by
 *  the common code in common_sagadraw/c64a8draw.c
 *  to draw the bitmap graphics of Atari 8-bit and
 *  Commodore 64 games.
 *
 *  Created by Petter Sjölund on 2025-12-15.
 */

#include "scott.h"
#include "c64a8draw.h"
#include "c64a8scott.h"

static USImage *current_image = NULL;

static void C64A8AdjustScott(struct c64a8_bounds *bounds) {
    USImage *image = current_image;
    if (image == NULL || Graphics == NULL)
        return;

    glui32 curheight, curwidth;
    glk_window_get_size(Graphics, &curwidth, &curheight);

    if (image->usage == IMG_ROOM || (bounds->right == 38 && bounds->left < 1)) {
        bounds->right = bounds->right - 1 - image->cropright / 8;
        left_margin = image->cropleft;

        ImageWidth = bounds->right * 8 - 17 + left_margin;
        ImageHeight = bounds->bottom + 2;

        glui32 width_in_pixels = ImageWidth * pixel_size;
        while (width_in_pixels > curwidth && pixel_size > 1) {
            pixel_size--;
            width_in_pixels = ImageWidth * pixel_size;
        }

        x_offset = ((int)curwidth - (int)width_in_pixels) / 2;

        glui32 optimal_height = ImageHeight * pixel_size;

        if (curheight != optimal_height) {
            SetFixedWindowHeight(Graphics, optimal_height);
        }
    }
}

static enum c64a8_variant C64A8Variant(USImage *image)
{
    /* Claymorgue is the only game with these images on the C64 */
    if (image->systype == SYS_C64)
        return C64A8_C64_CLAYMORGUE;
    if (CurrentGame == COUNT_US || CurrentGame == VOODOO_CASTLE_US)
        return C64A8_ATARI8_COUNT_VOODOO;
    return C64A8_ATARI8;
}

int DrawC64A8Image(USImage *image)
{
    current_image = image;
    StartPixelRuns();
    int result = DrawC64A8ImageFromData(image->imagedata, image->datasize, C64A8AdjustScott, C64A8Variant(image));
    FlushPixelRuns();
    return result;
}

