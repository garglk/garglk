//
//  sagadraw.h
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Created by Petter Sjölund on 2021-12-15.
//

#ifndef sagadraw_h
#define sagadraw_h

#include "glk.h"
#include "taylor.h"

typedef struct {
    uint8_t *imagedata;
    size_t datasize;
    uint8_t xoff;
    uint8_t yoff;
    uint8_t width;
    uint8_t height;
} Image;

uint8_t *DrawSagaPictureFromData(uint8_t *dataptr, int xsize, int ysize,
                                     int xoff, int yoff, size_t datasize);

void DrawSagaPictureNumber(int picture_number);
void DrawSagaPictureFromBuffer(void);
void DrawSagaPictureAtPos(int picture_number, int x, int y);

void SagaSetup(void);

void PutPixel(glsi32 x, glsi32 y, int32_t color);
void RectFill(int32_t x, int32_t y, int32_t width, int32_t height,
    int32_t color, int usebuffer);

void DefinePalette(void);

void DrawTaylor(int loc);
void ClearGraphMem(void);
int32_t Remap(int32_t color);

extern palette_type palchosen;

extern int white_colour;
extern int blue_colour;

extern glui32 pixel_size;
extern glui32 x_offset;

#endif /* sagadraw_h */
