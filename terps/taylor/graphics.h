//
//  graphics.h
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Created by Petter Sjölund on 2021-12-15.
//

#ifndef graphics_h
#define graphics_h

#include "glk.h"
#include "taylor.h"

void InitGraphics(void);

void PutPixel(glsi32 x, glsi32 y, int32_t color);
void RectFill(int32_t x, int32_t y, int32_t width, int32_t height,
    int32_t color);

void DrawBufferedPicture(void);
int BufferedPictureShown(void);
void GraphicsWindowChanged(void);

void PatchAndDrawQP3Cannon(void);

extern palette_type palchosen;

extern glui32 pixel_size;
extern glui32 x_offset;

#endif /* graphics_h */
