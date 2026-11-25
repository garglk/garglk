/*
 *  common_file_utils.h
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *
 *  Reading whole files.
 */

#ifndef common_file_utils_h
#define common_file_utils_h

#include <stdio.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Returns the contents of a file in a new allocation and its length in *size.
   Returns NULL with *size set to 0 if the file is missing, empty or unreadable.
   If reading stops early, *size is the number of bytes actually read. */
uint8_t *ReadFileIfExists(const char *name, size_t *size);

/* Returns the length of a seekable file, and leaves it rewound. An empty file
   and a failure both return 0. */
size_t GetFileLength(FILE *in);

#ifdef __cplusplus
}
#endif

#endif /* common_file_utils_h */
