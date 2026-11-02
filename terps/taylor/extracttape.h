//
//  extracttape.h
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Created by Petter Sjölund on 2022-04-11.
//

#ifndef extracttape_h
#define extracttape_h

#include <stdint.h>

/* GetTZXBlock and the Alkatraz primitives (ZXLdir/ZXLddr/DeAlkatraz/
   DeshuffleAlkatraz) come from the shared decompressz80 library. So does
   ZX_RAM_BASE (0x4000), where the RAM of the ZX Spectrum begins and where
   Taylor's games load. It is used to translate between Z80 addresses
   stored in game data and offsets into our loaded FileImage. */
#include "decompressz80.h"

uint8_t *ProcessFile(uint8_t *image, size_t *length);

#endif /* extracttape_h */
