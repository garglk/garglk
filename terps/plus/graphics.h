//
//  graphics.h
//  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
//
//  Created by Petter Sjölund on 2022-06-04.
//

#ifndef graphics_h
#define graphics_h

#include <stdio.h>
#include "glk.h"

typedef uint8_t RGB[3];
typedef RGB PALETTE[16];

extern winid_t Graphics;

extern int pixel_size;
extern int ImageWidth, ImageHeight;
extern int x_offset, y_offset, right_margin;
extern int upside_down;

extern int x, y;
extern int xlen, ylen;
extern int xoff, yoff;

int DrawCloseup(int item);
void DrawCurrentRoom(void);
int DrawRoomImage(int room);
void DrawItemImage(int item);
int DrawImageWithName(char *filename);
char *ShortNameFromType(char type, int index);
void SetColor(int32_t index, const RGB *color);
void PutPixel(glsi32 xpos, glsi32 ypos, int32_t color);
void PutDoublePixel(glsi32 xpos, glsi32 ypos, int32_t color);

#endif /* graphics_h */
