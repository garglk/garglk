//
//  atari_8bit_vector_draw.h
//  scott
//
//  Created by Administrator on 2026-01-13.
//

#ifndef atari_8bit_vector_draw_h
#define atari_8bit_vector_draw_h

#include <stdio.h>
#include "sagagraphics.h"

int DrawingAtari8Vector(void);
uint8_t *DrawAtari8BitVectorImage(USImage *img);
void DrawSomeAtari8VectorBytes(int from_start);
int RunAtari8bitVectorTests(const char *supportpath);
/* Test hook: renders the test image <name> and compares it with the
   reference in supportpath. Returns 1 on a match. */
int TestAtari8ImageWithName(const char *name, const char *supportpath);

/* Accessors for the two rendered screen planes, SCREEN1 and SCREEN2 in the
   S.A.G.E. source ($9000 and $A000 in the games; the goldens are .90 and .a0),
   each A8_IMAGE_SIZE (0xf00) bytes. Used by the byte-exact ground-truth
   regression harness. */
const uint8_t *GetAtari8Screen1Plane(void);
const uint8_t *GetAtari8Screen2Plane(void);

#endif /* atari_8bit_vector_draw_h */
