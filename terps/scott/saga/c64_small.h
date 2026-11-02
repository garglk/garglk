//
//  c64_small.h
//  scott
//
//  Created by Administrator on 2026-01-23.
//

#ifndef c64_small_h
#define c64_small_h

#include "c64decrunch.h"

GameIDType handle_all_in_one(uint8_t **database, size_t *datasize, c64rec c64_registry);
int DrawMiniC64(USImage *img);

#endif /* c64_small_graphics_h */
