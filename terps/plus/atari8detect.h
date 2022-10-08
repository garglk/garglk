//
//  atari8detect.h
//  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
//
//  Created by Petter Sjölund on 2022-01-30.
//

#ifndef detectatari8_h
#define detectatari8_h

#include <stdio.h>
int DetectAtari8(uint8_t **sf, size_t *extent);
int LookForAtari8Images(uint8_t **sf, size_t *extent);

#endif /* atari8detect_h */
