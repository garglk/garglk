//
//  debug.c
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-01-19.
//

#include "debug.h"

void list_action_addresses(FILE *ptr)
{
    int offset = 0x8110 - 0x3fe5;
    int low_byte, high_byte;
    fseek(ptr, offset, SEEK_SET);
    for (int i = 52; i < 102; i++) {
        low_byte = fgetc(ptr);
        high_byte = fgetc(ptr);
        fprintf(stderr, "Address of action %d: %x\n", i,
            low_byte + 256 * high_byte);
    }
}

void list_condition_addresses(FILE *ptr)
{
    int offset = 0x33A0 - 34;
    int low_byte, high_byte;
    fseek(ptr, offset, SEEK_SET);
    for (int i = 0; i < 20; i++) {
        low_byte = fgetc(ptr);
        high_byte = fgetc(ptr);
        fprintf(stderr, "Address of condition %d: %x\n", i,
            low_byte + 256 * high_byte);
    }
}
