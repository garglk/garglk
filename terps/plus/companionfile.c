//
//  companionfile.c
//  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
//
//  Created by Petter Sjölund on 2022-10-10.
//

#include "common.h"
#include "companion_search.h"
#include "companionfile.h"

#define BUCKAROO_BANZAI_OFFSET 27

/**
 * Standard bracket fallbacks, plus the special case of Buckaroo Banzai 177:
 * Changes 'a' to 'b' at a specific offset
 * Example: "177a - Buckaroo Banzai - Side 1.dsk" ->
 *        "177b - Buckaroo Banzai - Side 2.dsk"
 */
static uint8_t *PlusFallback(char *buffer, size_t bufsize, size_t namelen, int index, CompanionNameType type, CompanionReader read, void *user, size_t *size)
{
    if (type == TYPE_2) {
        if (index > BUCKAROO_BANZAI_OFFSET && buffer[index - BUCKAROO_BANZAI_OFFSET] == 'a') {
            buffer[index - BUCKAROO_BANZAI_OFFSET] = 'b';
            debug_print("looking for companion file (Buckaroo hack): \"%s\"\n", buffer);
            return read(buffer, size, user);
        }
        return NULL;
    }
    return CompanionBracketFallback(buffer, bufsize, namelen, index, type, read, user, size);
}

uint8_t *GetCompanionFile(const char *gamefile, size_t *size)
{
    const CompanionSearch search = {
        .read = NULL,
        .user = NULL,
        .fallback = PlusFallback,
        .match_char_before_extension = 1,
    };
    return FindCompanionFile(gamefile, &search, size);
}
