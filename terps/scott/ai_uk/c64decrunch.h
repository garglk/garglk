//
//  c64decrunch.h
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-01-30.
//

#ifndef c64decrunch_h
#define c64decrunch_h

#include <stdio.h>

#include "scott_defines.h"

typedef enum {
    UNKNOWN_FILE_TYPE,
    TYPE_D64,
    TYPE_T64,
    TYPE_US,
    TYPE_T64_US
} file_type;

typedef struct {
    GameIDType id;
    size_t length;
    uint16_t chk;
    file_type type;
    int decompress_iterations;
    const char *switches;
    const char *appendfile;
    int parameter;
    size_t copysource;
    size_t copydest;
    size_t copysize;
    size_t imgoffset;
} c64rec;

GameIDType DetectC64(uint8_t **sf, size_t *extent, const char *filename);

#endif /* c64decrunch_h */
