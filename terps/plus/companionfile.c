//
//  companionfile.c
//  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
//
//  Created by Petter Sjölund on 2022-10-10.
//

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "companionfile.h"

typedef enum {
    TYPE_NONE,
    TYPE_A,
    TYPE_B,
    TYPE_ONE,
    TYPE_TWO,
    TYPE_1,
    TYPE_2,
} CompanionNameType;

static int StripBrackets(char sideB[], size_t length) {
    int left_bracket = 0;
    int right_bracket = 0;
    if (length > 4) {
        for (int i = (int)length - 4; i > 0; i--) {
            char c = sideB[i];
            if (c == ']') {
                if (right_bracket == 0) {
                    right_bracket = i;
                } else {
                    return 0;
                }
            } else if (c == '[') {
                if (right_bracket > 0) {
                    left_bracket = i;
                    break;
                } else {
                    return 0;
                }
            }
        }
        if (right_bracket && left_bracket) {
            right_bracket++;
            while(right_bracket <= length)
                sideB[left_bracket++] = sideB[right_bracket++];
            return 1;
        }
    }
    return 0;
}

size_t GetFileLength(FILE *in)
{
    if (fseek(in, 0, SEEK_END) == -1) {
        return 0;
    }
    size_t length = ftell(in);
    if (length == -1) {
        return 0;
    }
    fseek(in, SEEK_SET, 0);
    return length;
}


uint8_t *ReadFileIfExists(const char *name, size_t *size)
{
    FILE *fptr = fopen(name, "r");

    if (fptr == NULL)
        return NULL;

    *size = GetFileLength(fptr);
    if (*size < 1)
        return NULL;

    uint8_t *result = MemAlloc(*size);
    fseek(fptr, 0, SEEK_SET);
    size_t bytesread = fread(result, 1, *size, fptr);

    fclose(fptr);

    if (bytesread == 0) {
        free(result);
        return NULL;
    }

    return result;
}

static uint8_t *LookForCompanionFilename(int index, CompanionNameType type, size_t stringlen, size_t *filesize) {

    char sideB[stringlen + 10];
    uint8_t *result = NULL;

    memcpy(sideB, game_file, stringlen + 1);
    switch(type) {
        case TYPE_A:
            sideB[index] = 'A';
            break;
        case TYPE_B:
            sideB[index] = 'B';
            break;
        case TYPE_1:
            sideB[index] = '1';
            break;
        case TYPE_2:
            sideB[index] = '2';
            break;
        case TYPE_ONE:
            sideB[index] = 'o';
            sideB[index + 1] = 'n';
            sideB[index + 2] = 'e';
            break;
        case TYPE_TWO:
            sideB[index] = 't';
            sideB[index + 1] = 'w';
            sideB[index + 2] = 'o';
            break;
        case TYPE_NONE:
            break;
    }

    debug_print("looking for companion file \"%s\"\n", sideB);
    result = ReadFileIfExists(sideB, filesize);
    if (!result) {
        if (type == TYPE_B) {
            if (StripBrackets(sideB, stringlen)) {
                debug_print("looking for companion file \"%s\"\n", sideB);
                result = ReadFileIfExists(sideB, filesize);
            }
        } else if (type == TYPE_A) {
            // First we look for the period before the file extension
            size_t ppos = stringlen - 1;
            while(sideB[ppos] != '.' && ppos > 0)
                ppos--;
            if (ppos < 1)
                return NULL;
            // Then we copy the extension to the new end position
            for (size_t i = ppos; i <= stringlen; i++) {
                sideB[i + 8] = sideB[i];
            }
            sideB[ppos++] = '[';
            sideB[ppos++] = 'c';
            sideB[ppos++] = 'r';
            sideB[ppos++] = ' ';
            sideB[ppos++] = 'C';
            sideB[ppos++] = 'S';
            sideB[ppos++] = 'S';
            sideB[ppos] = ']';
            debug_print("looking for companion file \"%s\"\n", sideB);
            result = ReadFileIfExists(sideB, filesize);
        }
    }

    return result;
}


uint8_t *GetCompanionFile(size_t *size) {

    size_t gamefilelen = strlen(game_file);
    uint8_t *result = NULL;

    char c;
    for (int i = (int)gamefilelen - 1; i >= 0 && game_file[i] != '/' && game_file[i] != '\\'; i--) {
        c = tolower(game_file[i]);
        char cminus1 = tolower(game_file[i - 1]);
        /* Pairs like side 1.dsk, side 2.dsk */
        if (i > 3 && ((c == 'e' && cminus1 == 'd' && tolower(game_file[i - 2]) == 'i' && tolower(game_file[i - 3]) == 's') ||
                      /* Pairs like disk 1.dsk, disk 2.dsk */
                      (c == 'k' && cminus1 && tolower(game_file[i - 2] == 'i') && tolower(game_file[i - 3]) == 'd') ||
                      /* Pairs like hulk1.dsk, hulk2.dsk */
                      (c == '.' && (cminus1 == '1' || cminus1 == '2' || cminus1 == 'a' || cminus1 == 'b')) )) {
            if (gamefilelen > i + 2) {
                if (c != '.')
                    c = game_file[i + 1];
                if (c == ' ' || c == '_' || c == '.') {
                    if (c == '.') {
                        c = tolower(game_file[i - 1]);
                        i -= 3;
                    } else
                        c = tolower(game_file[i + 2]);
                    CompanionNameType type = TYPE_NONE;
                    switch (c) {
                        case 'a':
                            type = TYPE_B;
                            break;
                        case 'b':
                            type = TYPE_A;
                            break;
                        case 't':
                            if (gamefilelen > i + 4 && game_file[i + 3] == 'w' && game_file[i + 4] == 'o') {
                                type =  TYPE_ONE;
                            }
                            break;
                        case 'o':
                            if (gamefilelen > i + 4 && game_file[i + 3] == 'n' && game_file[i + 4] == 'e') {
                                type = TYPE_TWO;
                            }
                            break;
                        case '2':
                            type= TYPE_1;
                            break;
                        case '1':
                            type = TYPE_2;
                            break;
                    }
                    if (type != TYPE_NONE)
                        result = LookForCompanionFilename(i + 2, type, gamefilelen, size);
                    if (result)
                        return result;
                }
            }
        }
    }
    return NULL;
}

