//
//  line_drawing.h
//  Part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-03-03.
//

#ifndef line_drawing_h
#define line_drawing_h

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t *data;
    int bgcolour;
    size_t size;
} line_image;

void DrawHowarthVectorPicture(int image);
void DrawSomeHowarthVectorPixels(int from_start);
int DrawingHowarthVector(void);

extern line_image *LineImages;

#endif /* line_drawing_h */
