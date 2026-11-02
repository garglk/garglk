//
//  detect_game.h
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-01-10.
//

#ifndef detect_game_h
#define detect_game_h

#include "scott.h"
#include <stdint.h>

GameIDType DetectGame(const char *file_name);
int SeekIfNeeded(int expected_start, size_t *offset, uint8_t **ptr);
GameIDType TryLoading(uint8_t *data, size_t datasize, const GameInfo *info, int dict_start);
DictionaryType GetId(const uint8_t *data, size_t datasize, size_t *offset);
int FindCode(const uint8_t *data, size_t datasize, const char *pattern, int patternLen);
uint8_t *ReadHeader(uint8_t *ptr);
int ParseHeader(int *h, HeaderType type, Header *out);
void PrintHeaderInfo(int *h, const Header *hdr);
uint8_t *SeekToPos(int offset);

void SetGameHeader(const Header *h);
void AllocateGameData(void);
int ReadRoomExits(uint8_t **ptr, int num_rooms, const uint8_t *endptr);
int ReadItemLocations(uint8_t **ptr, int num_items, const uint8_t *endptr);

extern int header[];

#endif /* detect_game_h */
