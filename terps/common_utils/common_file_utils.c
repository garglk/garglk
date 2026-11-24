/*
 *  common_file_utils.c
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *
 *  Reading whole files.
 */

#include <stdint.h>
#include <stdlib.h>

#include "memory_allocation.h"

#include "common_file_utils.h"

size_t GetFileLength(FILE *in)
{
    long length = -1;
    if (fseek(in, 0, SEEK_END) == 0)
        length = ftell(in);
    fseek(in, 0, SEEK_SET);
    return length > 0 ? (size_t)length : 0;
}

uint8_t *ReadFileIfExists(const char *name, size_t *size)
{
    *size = 0;

    FILE *fptr = fopen(name, "rb");

    if (fptr == NULL)
        return NULL;

    size_t length = GetFileLength(fptr);
    if (length == 0) {
        fclose(fptr);
        return NULL;
    }

    uint8_t *result = MemAlloc(length);
    size_t bytesread = fread(result, 1, length, fptr);

    fclose(fptr);

    if (bytesread == 0) {
        free(result);
        return NULL;
    }

    *size = bytesread;
    return result;
}
