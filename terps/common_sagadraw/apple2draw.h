//
//  apple2draw.h
//
//  Apple 2 bitmap graphics drawing for ScottFree and Plus,
//  two interpreters for adventures in Scott Adams formats
//
//  Created by Petter Sjölund on 2022-09-17.
//

#ifndef apple2draw_h
#define apple2draw_h

#include <stdint.h>
#include <stddef.h>

#include "gm_hires.h"

#define A2_SCREEN_MEM_SIZE GM_HIRES_PAGE_SIZE

void ClearApple2ScreenMem(void);
void DrawApple2ImageFromVideoMem(void);
void DrawApple2ImageFromVideoMemWithFlip(int upside_down);
// Which original interpreter an image was built for
enum apple2_variant {
    APPLE2_STANDARD, // header: left, top, right, bottom; a run repeats its
                     // two bytes only when bit 7 of the count is set
    APPLE2_COUNT,    // The Count: the header has no origin, and every run
                     // is a count followed by the two bytes to repeat
};

// Called with the right edge, bottom edge and top row from the image
// header, so Plus can size its graphics window.
typedef void (*adjust_plus_fn)(uint8_t right, uint8_t bottom, uint8_t top);
int DrawApple2ImageFromData(uint8_t *ptr, size_t datasize, enum apple2_variant variant, adjust_plus_fn adjust_plus);
void DrawSingleApple2ImageByte(uint8_t *mem, size_t offset);
// The hi-res page, A2_SCREEN_MEM_SIZE bytes; NULL until first drawn to.
extern uint8_t *screenmem;
extern uint8_t *descrambletable;

#endif /* apple2draw_h */
