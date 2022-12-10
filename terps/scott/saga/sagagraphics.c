//
//  sagagraphics.c
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-09-07.
//

#include <ctype.h>
#include <string.h>

#include "glk.h"
#include "scott.h"

#include "sagagraphics.h"

int pixel_size;
int x_offset = 0;
int y_offset = 0;
int right_margin;
int left_margin = 0;

PALETTE pal;

struct USImage *USImages = NULL;

int has_graphics(void) {
    return (USImages != NULL);
}

USImage *new_image(void) {
    struct USImage *new = MemAlloc(sizeof(USImage));
    new->index = -1;
    new->datasize = 0;
    new->imagedata = NULL;
    new->systype = 0;
    new->usage = 0;
    new->cropleft = 0;
    new->cropright = 0;
    new->previous = NULL;
    new->next = NULL;
    return new;
}

void SetColor(int32_t index, const RGB *color)
{
    pal[index][0] = (*color)[0];
    pal[index][1] = (*color)[1];
    pal[index][2] = (*color)[2];
}

void SetRGB(int32_t index, int red, int green, int blue) {
    red = red * 35.7;
    green = green * 35.7;
    blue = blue * 35.7;

    pal[index][0] = red;
    pal[index][1] = green;
    pal[index][2] = blue;
}

void PutPixel(glsi32 xpos, glsi32 ypos, int32_t color)
{
    if (xpos < 0 || xpos > right_margin || xpos < left_margin) {
        return;
    }
    glui32 glk_color = ((pal[color][0] << 16)) | ((pal[color][1] << 8)) | (pal[color][2]);

    glk_window_fill_rect(Graphics, glk_color, xpos * pixel_size + x_offset,
                         ypos * pixel_size + y_offset, pixel_size, pixel_size);
}

void PutDoublePixel(glsi32 xpos, glsi32 ypos, int32_t color)
{
    if (xpos < 0 || xpos > right_margin || xpos < left_margin) {
        return;
    }
    glui32 glk_color = ((pal[color][0] << 16)) | ((pal[color][1] << 8)) | (pal[color][2]);

    glk_window_fill_rect(Graphics, glk_color, xpos * pixel_size + x_offset,
                         ypos * pixel_size + y_offset, pixel_size * 2, pixel_size);
}

int issagaimg(const char *name) {
    if (name == NULL)
        return 0;
    size_t len = strlen(name);
    if (len < 4)
        return 0;
    char c = name[0];
    if (c == 'R' || c == 'B' || c == 'S') {
        for(int i = 1; i < 4; i++)
            if (!isdigit(name[i]))
                return 0;
        return 1;
    }
    return 0;
}

