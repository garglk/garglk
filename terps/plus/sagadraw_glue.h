//
//  sagadraw_glue.h
//  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
//
//  Created by Petter Sjölund on 2025-12-15.
//

#ifndef sagadrawglue_h
#define sagadrawglue_h

#include <stdint.h>

#include "c64a8draw.h"

void C64A8AdjustPlus(struct c64a8_bounds *bounds);
void Apple2AdjustPlus(uint8_t width, uint8_t height, uint8_t Y_origin);

void TransC64ColorPlus(uint8_t index, uint8_t value);
void TransAtariColorPlus(uint8_t index, uint8_t value);

#endif /* sagadrawglue_h */
