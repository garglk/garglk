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

GameIDType detect_game(const char *file_name);
int seek_if_needed(int expected_start, int *offset, uint8_t **ptr);
dictionary_type getId(size_t *offset);
int FindCode(char *x, int base);

#endif /* detectgame_h */
