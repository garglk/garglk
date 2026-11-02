//
//  apple2detect.h
//  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
//
//  Created by Petter Sjölund on 2022-08-03.
//

#ifndef apple2detect_h
#define apple2detect_h

int DetectApple2(uint8_t **sf, size_t *extent);
void LookForApple2Images(void);

/* The most special pictures a game can have */
#define A2_SPECIAL_SLOTS 50

void ReadApple2SpecialImages(const uint8_t *data, size_t offset, size_t size);

#endif /* apple2detect_h */
