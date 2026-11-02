//
//  common_file_utils.c
//  Spatterlight
//
//  Created by Administrator on 2025-12-20.
//

#include <stdint.h>
#include <stdlib.h>

#include "memory_allocation.h"

#include "common_file_utils.h"

size_t GetFileLength(FILE *in)
{
    if (fseek(in, 0, SEEK_END) == -1) {
        return 0;
    }
    long length = ftell(in);
    if (length == -1) {
        return 0;
    }
    fseek(in, 0, SEEK_SET);
    return (size_t)length;
}

uint8_t *ReadFileIfExists(const char *name, size_t *size)
{
    FILE *fptr = fopen(name, "r");

    if (fptr == NULL)
        return NULL;

    *size = GetFileLength(fptr);
    if (*size == 0) {
        fclose(fptr);
        return NULL;
    }

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
