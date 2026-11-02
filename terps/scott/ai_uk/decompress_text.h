//
//  decompress_text.h
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Implements the text decompression scheme used by Robin of Sherwood
//  and Seas of Blood.
//
//  Created by Petter Sjölund on 2022-01-10.
//

#ifndef decompress_text_h
#define decompress_text_h

#include <stdint.h>
#include <stdlib.h>

char *DecompressText(uint8_t *source, int stringindex);

#endif /* decompress_text_h */
