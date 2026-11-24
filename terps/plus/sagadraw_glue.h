/*
 *  sagadraw_glue.h
 *  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version. See the file COPYING.
 *
 *  Created by Petter Sjölund on 2025-12-15.
 */

#ifndef sagadraw_glue_h
#define sagadraw_glue_h

#include <stdint.h>

#include "c64a8draw.h"

/* Called by the shared picture code with the size of the image being drawn */
void C64A8AdjustPlus(struct c64a8_bounds *bounds);
void Apple2AdjustPlus(uint8_t width, uint8_t height, uint8_t y_origin);

#endif /* sagadraw_glue_h */
