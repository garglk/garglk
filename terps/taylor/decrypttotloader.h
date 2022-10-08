//
//  decryptloader.h
//
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  This code is for de-protecting the ZX Spectrum text-only version of Temple of Terror
//
//  Created by Petter Sjölund on 2022-04-18.
//

#ifndef decryptloader_h
#define decryptloader_h

#include <stdint.h>
#include <stdio.h>

uint8_t *DecryptToTSideB(uint8_t *data, size_t *length);

#endif /* decryptloader_h */
