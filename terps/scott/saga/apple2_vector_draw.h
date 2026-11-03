//
//  apple2_vector_draw.h
//  scott
//
//  Created by Administrator on 2025-12-29.
//

#ifndef apple2_vector_draw_h
#define apple2_vector_draw_h

#include <stdio.h>
#include "sagagraphics.h"

int DrawApple2VectorImage(USImage *img);
void DrawSomeApple2VectorBytes(int from_start);
int DrawingApple2Vector(void);
int RunApple2VectorTests(const char *supportpath);
/* Test hook: renders the test image <name> and compares it with the
   reference in supportpath. Returns 1 on a match. */
int TestApple2ImageWithName(const char *name, const char *supportpath);
void SetBrushBitmaps(const uint8_t *data);
void SetPatternData(const uint8_t *data);
void SetColorPatternSubindices(const uint8_t *data);

#endif /* apple2_vector_draw_h */
