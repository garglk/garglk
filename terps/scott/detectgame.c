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

struct dictionaryKey {
    dictionary_type dict;
    char *signature;
};

struct dictionaryKey dictKeys[] = {
    { FOUR_LETTER_UNCOMPRESSED, "AUTO\0GO\0" },
    { THREE_LETTER_UNCOMPRESSED, "AUT\0GO\0" },
    { FIVE_LETTER_UNCOMPRESSED, "AUTO\0\0GO" },
    { FOUR_LETTER_COMPRESSED, "aUTOgO\0" },
    { GERMAN, "\xc7" "EHENSTEIGE" },
    { FIVE_LETTER_COMPRESSED, "gEHENSTEIGE"}, // Gremlins C64
    { SPANISH, "ANDAENTRAVAN" },
    { FIVE_LETTER_UNCOMPRESSED, "*CROSS*RUN\0\0" } // Claymorgue
};

int FindCode(char *x, int base)
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

dictionary_type getId(size_t *offset)
{
    for (int i = 0; i < 8; i++) {
        *offset = FindCode(dictKeys[i].signature, 0);
        if (*offset != -1) {
            if (i == 4 || i == 5) // GERMAN
                *offset -= 5;
            else if (i == 6) // SPANISH
                *offset -= 8;
            else if (i == 7) // Claymorgue
                *offset -= 11;
            return dictKeys[i].dict;
        }
    }

    return NOT_A_GAME;
}

void read_header(uint8_t *ptr)
{
    int i, value;
    for (i = 0; i < 24; i++) {
        value = *ptr + 256 * *(ptr + 1);
        header[i] = value;
        ptr += 2;
    }
}

uint8_t *seek_to_pos(uint8_t *buf, int offset)
{
    if (offset > file_length)
        return 0;
    return buf + offset;
}

int seek_if_needed(int expected_start, int *offset, uint8_t **ptr)
{
    if (expected_start != FOLLOWS) {
        *offset = expected_start + file_baseline_offset;
        uint8_t *ptrbefore = *ptr;
        *ptr = seek_to_pos(entire_file, *offset);
        if (*ptr == ptrbefore)
            fprintf(stderr, "Seek unnecessary, could have been set to FOLLOWS.\n");
        if (*ptr == 0)
            return 0;
    }
    return 1;
}

void print_header_info(int *h, int ni, int na, int nw, int nr, int mc, int pr, int tr, int wl, int lt, int mn, int trm)
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

GameIDType detect_game(const char *file_name)
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

    // Check if the original ScottFree LoadDatabase() function can read the file.
    if (LoadDatabase(f, 0)) {
        fclose(f);
        GameInfo = MemAlloc(sizeof(*GameInfo));
        GameInfo->gameID = SCOTTFREE;
        return SCOTTFREE;
    }

    file_length = GetFileLength(f);

    if (file_length > MAX_GAMEFILE_SIZE) {
        fprintf(stderr, "File too large to be a vaild game file (%zu, max is %d)\n", file_length, MAX_GAMEFILE_SIZE);
        return 0;
    }

    entire_file = MemAlloc(file_length);
    fseek(f, 0, SEEK_SET);
    size_t result = fread(entire_file, 1, file_length, f);
    fclose(f);
    if (result == 0)
        Fatal("File empty or read error!");

    GameIDType TI994A_id = UNKNOWN_GAME;

    TI994A_id = DetectTI994A(&entire_file, &file_length);
    if (TI994A_id) {
        GameInfo = MemAlloc(sizeof(*GameInfo));
        GameInfo->gameID = SCOTTFREE;
        return SCOTTFREE;
    }

    if (!TI994A_id) {
        size_t offset;

        dictionary_type dict_type = getId(&offset);

        if (dict_type == NOT_A_GAME)
            return UNKNOWN_GAME;

        if (GameInfo == NULL)
            return 0;
    }

    /* Copy ZX Spectrum style system messages as base */
    for (int i = 6; i < MAX_SYSMESS && sysdict_zx[i] != NULL; i++) {
        sys[i] = (char *)sysdict_zx[i];
    }

    return CurrentGame;
}
