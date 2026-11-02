//
//  common_file_utils.h
//  Spatterlight
//
//  Created by Administrator on 2025-12-20.
//

#ifndef common_file_utils_h
#define common_file_utils_h

#include <stdio.h>
#include <stdint.h>

uint8_t *ReadFileIfExists(const char *name, size_t *size);
size_t GetFileLength(FILE *in);

#endif /* common_file_utils_h */
