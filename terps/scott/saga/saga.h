//
//  saga.h
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-09-29.
//

#ifndef saga_h
#define saga_h

#include <stdio.h>

int DrawUSRoom(int room);
void DrawRoomObjectImages(void);
void DrawUSRoomObject(int item);
void LookUS(void);

typedef struct pairrec {
    uint32_t length;
    uint16_t chk;
    const char *filename;
    uint32_t stringlength;
} pairrec;

typedef enum {
    TYPE_NONE,
    TYPE_A,
    TYPE_B,
    TYPE_ONE,
    TYPE_TWO,
    TYPE_1,
    TYPE_2,
} CompanionNameType;

uint8_t *ReadFileIfExists(const char *name, size_t *size);
int CompareFilenames(const char *str1, size_t length1, const char *str2, size_t length2);
const char *LookForCompanionFilenameInDatabase(const pairrec list[][2], size_t stringlen, size_t *stringlength2);
char *LookInDatabase(const pairrec list[][2], size_t stringlen);

#endif /* saga_h */
