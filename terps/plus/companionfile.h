//
//  companionfile.h
//  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
//
//  Created by Petter Sjölund on 2022-10-10.
//

#ifndef companionfile_h
#define companionfile_h

#include <stdint.h>
uint8_t *GetCompanionFile(const char *gamefile, size_t *size);

#endif /* companionfile_h */
