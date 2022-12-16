//
//  detectgame.h
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-01-10.
//

#ifndef detectgame_h
#define detectgame_h

#include "scottdefines.h"
#include <stdint.h>
#include <stdio.h>

GameIDType DetectGame(const char *file_name);
int SeekIfNeeded(int expected_start, size_t *offset, uint8_t **ptr);
int TryLoading(struct GameInfo info, int dict_start, int loud);
DictionaryType GetId(size_t *offset);
int FindCode(const char *x, int base);
uint8_t *ReadHeader(uint8_t *ptr);
int ParseHeader(int *h, HeaderType type, int *ni, int *na, int *nw, int *nr,
                int *mc, int *pr, int *tr, int *wl, int *lt, int *mn,
                int *trm);

void PrintHeaderInfo(int *h, int ni, int na, int nw, int nr, int mc, int pr,
                     int tr, int wl, int lt, int mn, int trm);

extern int header[];

#endif /* detectgame_h */
