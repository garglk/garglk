//
//  pcdraw.h
//  Part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-09-07.
//

#ifndef pcdraw_h
#define pcdraw_h

#include "sagagraphics.h"

uint8_t *FindImageFile(const char *shortname, size_t *datasize);
int DrawDOSImage(USImage *image);

#endif /* pcdraw_h */
