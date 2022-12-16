//
//  decompresstext.h
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-01-10.
//

#ifndef decompresstext_h
#define decompresstext_h

#include <stdlib.h>
#include <stdint.h>

char *DecompressText(uint8_t *source, int stringindex);

#endif /* decompresstext_h */
