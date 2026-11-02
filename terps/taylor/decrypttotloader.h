//
//  decryptloader.h
//
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  This code is for de-protecting the ZX Spectrum text-only version of Temple of Terror
//
//  Created by Petter Sjölund on 2022-04-18.
//

#ifndef decrypttotloader_h
#define decrypttotloader_h

#include <stdint.h>

/* Decrypts the Temple of Terror Side B tape image at data into a new
   ZX_MEMORY_SIZE memory image for the caller to free, and sets *length to
   ZX_MEMORY_SIZE; NULL on failure. Never frees data. */
uint8_t *DecryptToTSideB(const uint8_t *data, size_t *length);

#endif /* decrypttotloader_h */
