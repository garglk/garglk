//
//  detectgame.c
//  scott
//
//  Created by Administrator on 2022-01-10.
//

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "scott.h"

#include "detectgame.h"
#include "gameinfo.h"

#include "load_TI99_4a.h"
#include "parser.h"

extern const char *sysdict_zx[MAX_SYSMESS];
extern int header[];

int FindCode(const char *x, int base)
{
    unsigned const char *p = entire_file + base;
    int len = strlen(x);
    if (len < 7)
        len = 7;
    while (p < entire_file + file_length - len) {
        if (memcmp(p, x, len) == 0) {
            return p - entire_file;
        }
        p++;
    }
    return -1;
}

void ReadHeader(uint8_t *ptr)
{
    int i, value;
    for (i = 0; i < 24; i++) {
        value = *ptr + 256 * *(ptr + 1);
        header[i] = value;
        ptr += 2;
    }
}

uint8_t *SeekToPos(uint8_t *buf, int offset)
{
    if (offset > file_length)
        return 0;
    return buf + offset;
}

int SeekIfNeeded(int expected_start, int *offset, uint8_t **ptr)
{
    if (expected_start != FOLLOWS) {
        *offset = expected_start + file_baseline_offset;
        uint8_t *ptrbefore = *ptr;
        *ptr = SeekToPos(entire_file, *offset);
        if (*ptr == ptrbefore)
            fprintf(stderr, "Seek unnecessary, could have been set to FOLLOWS.\n");
        if (*ptr == 0)
            return 0;
    }
    return 1;
}

void PrintHeaderInfo(int *h, int ni, int na, int nw, int nr, int mc, int pr, int tr, int wl, int lt, int mn, int trm)
{
    uint16_t value;
    for (int i = 0; i < 13; i++) {
        value = h[i];
        fprintf(stderr, "b $%X %d: ", 0x494d + 0x3FE5 + i * 2, i);
        fprintf(stderr, "\t%d\n", value);
    }

    fprintf(stderr, "Number of items =\t%d\n", ni);
    fprintf(stderr, "Number of actions =\t%d\n", na);
    fprintf(stderr, "Number of words =\t%d\n", nw);
    fprintf(stderr, "Number of rooms =\t%d\n", nr);
    fprintf(stderr, "Max carried items =\t%d\n", mc);
    fprintf(stderr, "Word length =\t%d\n", wl);
    fprintf(stderr, "Number of messages =\t%d\n", mn);
    fprintf(stderr, "Player start location: %d\n", pr);
    fprintf(stderr, "Treasure room: %d\n", tr);
    fprintf(stderr, "Lightsource time left: %d\n", lt);
    fprintf(stderr, "Number of treasures: %d\n", tr);
}

GameIDType DetectGame(const char *file_name)
{

    FILE *f = fopen(file_name, "r");
    if (f == NULL)
        Fatal("Cannot open game");

    for (int i = 0; i < NUMBER_OF_DIRECTIONS; i++)
        Directions[i] = EnglishDirections[i];
    for (int i = 0; i < NUMBER_OF_SKIPPABLE_WORDS; i++)
        SkipList[i] = EnglishSkipList[i];
    for (int i = 0; i < NUMBER_OF_DELIMITERS; i++)
        DelimiterList[i] = EnglishDelimiterList[i];
    for (int i = 0; i < NUMBER_OF_EXTRA_NOUNS; i++)
        ExtraNouns[i] = EnglishExtraNouns[i];

    file_length = GetFileLength(f);

    if (file_length > MAX_GAMEFILE_SIZE) {
        fprintf(stderr, "File too large to be a vaild game file (%zu, max is %d)\n", file_length, MAX_GAMEFILE_SIZE);
        return 0;
    }

    Game = (struct GameInfo *)MemAlloc(sizeof(struct GameInfo));

    // Check if the original ScottFree LoadDatabase() function can read the file.
    CurrentGame = LoadDatabase(f, Options & DEBUGGING);

    if (!CurrentGame) {
        entire_file = MemAlloc(file_length);
        fseek(f, 0, SEEK_SET);
        size_t result = fread(entire_file, 1, file_length, f);
        fclose(f);
        if (result == 0)
            Fatal("File empty or read error!");

        CurrentGame = DetectTI994A(&entire_file, &file_length);
    }

    return CurrentGame;
}
