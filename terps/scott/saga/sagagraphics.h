//
//  sagagraphics.h
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-09-07.
//

#ifndef sagagraphics_h
#define sagagraphics_h

#include "glk.h"
#include "scott_defines.h"

struct USImage {
    USImageType usage;
    int index;
    MachineType systype;
    size_t datasize;
    uint8_t *imagedata;
    int cropleft;
    int cropright;
    char *filename;
    struct USImage *previous;
    struct USImage *next;
};

typedef struct USImage USImage;

extern USImage *USImages;

typedef struct {
    GameIDType game;
    USImageType usage;
    int index;
    int cropleft;
    int cropright;
} CropList;

typedef struct {
    USImageType usage;
    int index;
    size_t offset;
} imglist;

extern int pixel_size;
extern int x_offset, y_offset;
extern int left_margin;
extern glui32 pal[16];

void SetColor(int32_t index, glui32 color);

void PutPixel(glsi32 x, glsi32 y, int32_t color);
void PutDoublePixel(glsi32 x, glsi32 y, int32_t color);
void PutPixelWithWidth(glsi32 xpos, glsi32 ypos, int32_t color, int pixel_width);
/* Bracket a bitmap picture that is drawn in one go, so that its pixels
   reach the window as runs rather than one at a time. */
void StartPixelRuns(void);
void FlushPixelRuns(void);
void RectFill(int32_t x, int32_t y, int32_t width, int32_t height,
              int32_t color);
uint8_t *FindImageFile(const char *shortname, size_t *datasize);

USImage *NewImage(void);

/* Append a new, empty image to the list after tail and return it. */
static inline USImage *AppendImage(USImage *tail)
{
    USImage *image = NewImage();
    image->previous = tail;
    tail->next = image;
    return image;
}

int IsSagaImage(const char *name);
int HasGraphics(void);
void DrawImageOrVector(void);

#endif /* sagagraphics_h */
