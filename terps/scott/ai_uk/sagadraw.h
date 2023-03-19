//
//  sagadraw.h
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2021-12-15.
//

#ifndef sagadraw_h
#define sagadraw_h

#include "glk.h"
#include "scottdefines.h"

typedef struct {
    uint8_t *imagedata;
    uint8_t xoff;
    uint8_t yoff;
    uint8_t width;
    uint8_t height;
} Image;

uint8_t *DrawSagaPictureFromData(uint8_t *dataptr, int xsize, int ysize,
    int xoff, int yoff);

void DrawSagaPictureNumber(int picture_number);
void DrawSagaPictureAtPos(int picture_number, int x, int y);
void DrawSagaPictureFromBuffer(void);
void Flip(uint8_t character[]);

void SagaSetup(size_t imgoffset);

void RectFill(int32_t x, int32_t y, int32_t width, int32_t height,
    int32_t color);

void SwitchPalettes(int pal1, int pal2);
void DefinePalette(void);
int32_t Remap(int32_t color);

extern palette_type palchosen;

extern int white_colour;
extern int blue_colour;
extern glui32 dice_colour;

#endif /* sagadraw_h */
