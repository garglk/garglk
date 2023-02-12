/* This is a cut-down version of UNP64 with only the bare minimum
 * needed to decompress a number of Commodore 64 adventure games.
 * It is distributed under the zlib License by kind permission of
 * the original authors Magnus Lind and iAN CooG.
 */

/*
 UNP64 - generic Commodore 64 prg unpacker
 (C) 2008-2022 iAN CooG/HVSC Crew^C64Intros
 original source and idea: testrun.c, taken from exo20b7

 Follows original disclaimer
 */

/*
 * Copyright (c) 2002 - 2023 Magnus Lind.
 *
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 * claim that you wrote the original software. If you use this software
 * in a product, an acknowledgment in the product documentation would be
 * appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 * misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 */

/*
 C++ version based on code adapted to ScummVM by Avijeet Maurya
 */

#ifndef UNP64_TYPES_H
#define UNP64_TYPES_H

#include <cstdint>

namespace Unp64 {

#if !defined(SIZE_MAX)
#define SIZE_MAX 0xFFFFFFFF
#endif

#if !defined ARRAYSIZE
#define ARRAYSIZE(x) ((int)(sizeof(x) / sizeof(x[0])))
#endif

#if !defined MIN
#define MIN(a,b) (a < b ? a : b)
#endif

typedef unsigned long long size_t;

inline uint16_t READ_LE_UINT16(const void *ptr) {
    const uint8_t *b = (const uint8_t *)ptr;
    return (b[1] << 8) | b[0];
}

} // End of namespace Unp64

#endif
