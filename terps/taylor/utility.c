//
//  utility.c
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Created by Petter Sjölund on 2022-03-10.
//

#include <stdlib.h>

#include "glk.h"
#include "taylor.h"

#include "utility.h"

strid_t Transcript = NULL;

extern winid_t Bottom, Top;

void CleanupAndExit(void)
{
    if (Transcript)
        glk_stream_close(Transcript, NULL);
    glk_exit();
}

void Fatal(const char *x)
{
    if (Bottom)
        Display(Bottom, "%s!\n", x);
    fprintf(stderr, "%s!\n", x);
    CleanupAndExit();
}

void *MemAlloc(size_t size)
{
    void *t = (void *)malloc(size);
    if (t == NULL)
        Fatal("Out of memory");
    return (t);
}

uint8_t *SeekToPos(uint8_t *buf, size_t offset)
{
    if (offset > FileImageLen)
        return 0;
    return buf + offset;
}

void print_memory(int address, int length)
{
    unsigned char *p = FileImage + address;
    for (int i = address; i <= address + length; i += 16) {
        fprintf(stderr, "\n%04X:  ", i);
        for (int j = 0; j < 16; j++) {
            fprintf(stderr, "%02X ", *p++);
        }
    }
}

uint8_t *readFile(const char *name, size_t *size)
{
    FILE *f = fopen(name, "r");
    if (f == NULL)
        return NULL;

    fseek(f, 0, SEEK_END);
    *size = ftell(f);
    if (*size == -1)
        return NULL;

    uint8_t *data = MemAlloc(*size);

    size_t origsize = *size;

    fseek(f, 0, SEEK_SET);
    *size = fread(data, 1, origsize, f);
    if (*size != origsize) {
        fprintf(stderr, "File read error on file %s! Wanted %zu bytes, got %zu\n", name, origsize, *size);
    }

    fclose(f);
    return data;
}

int rotate_left_with_carry(uint8_t *byte, int last_carry)
{
    int carry = ((*byte & 0x80) > 0);
    *byte = *byte << 1;
    if (last_carry)
        *byte = *byte | 0x01;
    return carry;
}

int rotate_right_with_carry(uint8_t *byte, int last_carry)
{
    int carry = ((*byte & 0x01) > 0);
    *byte = *byte >> 1;
    if (last_carry)
        *byte = *byte | 0x80;
    return carry;
}
