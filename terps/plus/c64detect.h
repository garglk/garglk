//
//  c64detect.h
//  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
//
//  Created by Petter Sjölund on 2022-01-30.
//

#ifndef c64detect_h
#define c64detect_h

int DetectC64(uint8_t **sf, size_t *extent);
void PatchSpidermanImages(void);

#endif /* c64detect_h */
