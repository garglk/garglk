//
//  detectgame.h
//  scott
//
//  Created by Administrator on 2022-01-10.
//

#ifndef detectgame_h
#define detectgame_h

#include <stdio.h>
#include <stdint.h>
#include "definitions.h"

GameIDType DetectGame(const char *file_name);
int SeekIfNeeded(int expected_start, int *offset, uint8_t **ptr);
DictionaryType GetId(size_t *offset);
int FindCode(const char *x, int base);

#endif /* detectgame_h */
