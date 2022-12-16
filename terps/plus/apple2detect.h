//
//  apple2detect.h
//  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
//
//  Created by Petter Sjölund on 2022-08-03.
//

#ifndef apple2detect_h
#define apple2detect_h

#include <stdio.h>

int DetectApple2(uint8_t **sf, size_t *extent);
void LookForApple2Images(void);

#endif /* apple2detect_h */
