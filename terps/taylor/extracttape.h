//
//  extracttape.h
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Created by Petter Sjölund on 2022-04-11.
//

#ifndef extracttape_h
#define extracttape_h

#include <stdio.h>
#include <stdint.h>

uint8_t *GetTZXBlock(int blockno, uint8_t *srcbuf, size_t *length);
uint8_t *DeAlkatraz(uint8_t *raw_data, uint8_t *target, size_t offset, uint16_t IX, uint16_t DE, uint8_t *loacon, uint8_t add1, uint8_t add2, int selfmodify);
void DeshuffleAlkatraz(uint8_t *mem, uint8_t repeats, uint16_t IX, uint16_t store);
void ldir(uint8_t *mem, uint16_t DE, uint16_t HL, uint16_t BC);
void lddr(uint8_t *mem, uint16_t DE, uint16_t HL, uint16_t BC);
uint8_t *ProcessFile(uint8_t *image, size_t *length);

#endif /* extracttape_h */
