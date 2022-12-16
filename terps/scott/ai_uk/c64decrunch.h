//
//  c64decrunch.h
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-01-30.
//

#ifndef c64decrunch_h
#define c64decrunch_h

#include <stdio.h>

#include "scottdefines.h"

GameIDType DetectC64(uint8_t **sf, size_t *extent);

#endif /* c64decrunch_h */
