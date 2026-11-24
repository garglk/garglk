/*
 *  companion_search.c
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *
 *  Finds the other disk (or disk side) of a multi-disk game.
 *  Originally part of Plus (companionfile.c), created by Petter Sjölund
 *  on 2022-10-10.
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "common_file_utils.h"
#include "debugprint.h"
#include "memory_allocation.h"

#include "companion_search.h"

/* Room for the longest marker a fallback inserts */
#define FILENAME_BUFFER_EXTRA 16
#define MIN_FILENAME_LENGTH 4
#define MIN_COMPANION_KEYWORD_LENGTH 3

static inline int is_path_separator(char c)
{
    return (c == '/' || c == '\\');
}

static char lower(char c)
{
    return (char)tolower((unsigned char)c);
}

/* Compares text to word, which must be in lower case, ignoring the case of text */
static int matches_word(const char *text, const char *word)
{
    for (; *word != '\0'; text++, word++) {
        if (lower(*text) != *word)
            return 0;
    }
    return 1;
}

/* The start of the file name, after any folders */
static char *file_name_of(char *path)
{
    char *name = path;
    for (char *p = path; *p != '\0'; p++) {
        if (is_path_separator(*p))
            name = p + 1;
    }
    return name;
}

int CompanionStripEnclosed(char *buffer, size_t namelen, char open, char close, int strip_space)
{
    if (!buffer || namelen < MIN_FILENAME_LENGTH) {
        return 0;
    }

    char *name = file_name_of(buffer);

    /* Find file extension (everything after the last period) */
    char *extension = strrchr(name, '.');
    if (!extension) {
        return 0;
    }

    /* Find last closing character before extension */
    char *close_pos = strrchr(name, close);
    if (!close_pos || close_pos >= extension) {
        return 0;
    }

    /* Find matching opening character */
    char *open_pos = strrchr(name, open);
    if (!open_pos || open_pos >= close_pos) {
        return 0;
    }

    if (strip_space && open_pos > name && *(open_pos - 1) == ' ') {
        open_pos--;
    }

    /* Remove the enclosed section, keeping the rest including the terminating zero */
    memmove(open_pos, close_pos + 1, strlen(close_pos + 1) + 1);
    return 1;
}

int CompanionInsertBeforeExtension(char *buffer, size_t bufsize, size_t namelen, const char *marker)
{
    size_t markerlen = strlen(marker);

    /* Find the period before the file extension, within the file name */
    size_t ppos = namelen;
    while (ppos > 0 && buffer[ppos - 1] != '.') {
        ppos--;
        if (is_path_separator(buffer[ppos]))
            return 0;
    }
    if (ppos == 0) {
        return 0;
    }
    ppos--;

    /* A period that starts the file name does not begin an extension */
    if (ppos == 0 || is_path_separator(buffer[ppos - 1])) {
        return 0;
    }

    if (namelen + markerlen >= bufsize) {
        return 0;
    }

    /* Shift extension, including the terminating zero, to make room for marker */
    memmove(buffer + ppos + markerlen, buffer + ppos, namelen - ppos + 1);
    memcpy(buffer + ppos, marker, markerlen);
    return 1;
}

uint8_t *CompanionBracketFallback(char *buffer, size_t bufsize, size_t namelen, int index, CompanionNameType type, CompanionReader read, void *user, size_t *size)
{
    (void)index;
    switch (type) {
        case COMPANION_TYPE_B:
            /* "fileB[tag].dsk" -> "fileB.dsk" */
            if (CompanionStripEnclosed(buffer, namelen, '[', ']', 0)) {
                debug_print("looking for companion file (with removed text in brackets): \"%s\"\n", buffer);
                return read(buffer, size, user);
            }
            break;
        case COMPANION_TYPE_A:
            /* "fileA.dsk" -> "fileA[cr CSS].dsk" */
            if (CompanionInsertBeforeExtension(buffer, bufsize, namelen, COMPANION_CRACK_TAG)) {
                debug_print("looking for companion file (with added tag): \"%s\"\n", buffer);
                return read(buffer, size, user);
            }
            break;
        default:
            break;
    }
    return NULL;
}

/* The replacement letter in the case of the letter it replaces, so that
   "side a" gives "side b" and "Disk One" gives "Disk Two" on a file system
   that tells them apart */
static char match_case(char replacement, char original)
{
    return (char)(isupper((unsigned char)original) ? toupper((unsigned char)replacement) : tolower((unsigned char)replacement));
}

static void replace_word(char *position, const char *word)
{
    for (int i = 0; word[i] != '\0'; i++)
        position[i] = match_case(word[i], position[i]);
}

static uint8_t *default_reader(const char *filename, size_t *size, void *user)
{
    (void)user;
    return ReadFileIfExists(filename, size);
}

/**
 * Tries the companion file name of the given type, then any fallbacks
 */
static uint8_t *LookForCompanionFilename(const char *gamefile, int index,
                                         CompanionNameType type, size_t stringlen,
                                         const CompanionSearch *search,
                                         size_t *filesize)
{
    size_t bufsize = stringlen + FILENAME_BUFFER_EXTRA;
    char *buffer = MemAlloc(bufsize);
    CompanionReader read = search->read ? search->read : default_reader;
    uint8_t *result = NULL;

    memcpy(buffer, gamefile, stringlen + 1);

    switch (type) {
        case COMPANION_TYPE_A:
            replace_word(buffer + index, "a");
            break;
        case COMPANION_TYPE_B:
            replace_word(buffer + index, "b");
            break;
        case COMPANION_TYPE_1:
            buffer[index] = '1';
            break;
        case COMPANION_TYPE_2:
            buffer[index] = '2';
            break;
        case COMPANION_TYPE_ONE:
            replace_word(buffer + index, "one");
            break;
        case COMPANION_TYPE_TWO:
            replace_word(buffer + index, "two");
            break;
        case COMPANION_TYPE_NONE:
            free(buffer);
            return NULL;
    }

    debug_print("looking for companion file \"%s\"\n", buffer);
    result = read(buffer, filesize, search->user);

    if (!result && search->fallback) {
        result = search->fallback(buffer, bufsize, stringlen, index, type,
                                  read, search->user, filesize);
    }

    free(buffer);
    return result;
}

/**
 * Checks if the position in filename, which holds the lower case character c,
 * ends a companion file pattern. Patterns: "side", "disk", or (optionally) a
 * single digit/letter before the extension
 */
static int is_companion_name(const char *filename, int i, char c, int match_char_before_extension)
{
    if (i < MIN_COMPANION_KEYWORD_LENGTH) {
        return 0;
    }

    const char *keyword = filename + i - MIN_COMPANION_KEYWORD_LENGTH;
    if (matches_word(keyword, "side") || matches_word(keyword, "disk")) {
        return 1;
    }

    if (match_char_before_extension && c == '.') {
        char before = lower(filename[i - 1]);
        return before == '1' || before == '2' || before == 'a' || before == 'b';
    }

    return 0;
}

/**
 * Maps the disk identifier at filename[index] to the companion to look for
 */
static CompanionNameType determine_type(const char *filename, size_t length, int index)
{
    switch (lower(filename[index])) {
        case 'a':
            return COMPANION_TYPE_B;

        case 'b':
            return COMPANION_TYPE_A;

        case 't':
            if (length > (size_t)index + 2 && matches_word(filename + index, "two")) {
                return COMPANION_TYPE_ONE;
            }
            break;

        case 'o':
            if (length > (size_t)index + 2 && matches_word(filename + index, "one")) {
                return COMPANION_TYPE_TWO;
            }
            break;

        case '2':
            return COMPANION_TYPE_1;

        case '1':
            return COMPANION_TYPE_2;
    }

    return COMPANION_TYPE_NONE;
}

/**
 * Finds and loads the companion file of a multi-disk game
 *
 * Searches for companion files based on common naming patterns:
 * - "side 1" <-> "side 2"
 * - "disk 1" <-> "disk 2"
 * - "side a" <-> "side b"
 * - "side one" <-> "side two"
 * - "file1.dsk" <-> "file2.dsk", "fileA.dsk" <-> "fileB.dsk"
 *   (only if search->match_char_before_extension is set)
 *
 * @return: Pointer to loaded file data, or NULL if not found
 */
uint8_t *FindCompanionFile(const char *gamefile, const CompanionSearch *search, size_t *size)
{
    if (!size || !gamefile || !search) {
        return NULL;
    }

    size_t gamefilelen = strlen(gamefile);
    if (gamefilelen == 0) {
        return NULL;
    }

    /* Scan backwards through the file name from the end, stopping at a path
       separator: the folders do not count */
    for (int i = (int)gamefilelen - 1; i >= 0 && !is_path_separator(gamefile[i]); i--) {
        char c = lower(gamefile[i]);

        if (!is_companion_name(gamefile, i, c, search->match_char_before_extension) ||
            gamefilelen <= (size_t)i + 2) {
            continue;
        }

        /* A single character before the extension, or "side" or "disk"
           followed by a separator and the identifier */
        int index;
        if (c == '.') {
            index = i - 1;
        } else {
            char separator = gamefile[i + 1];
            if (separator != ' ' && separator != '_' && separator != '.') {
                continue;
            }
            index = i + 2;
        }

        CompanionNameType type = determine_type(gamefile, gamefilelen, index);
        if (type != COMPANION_TYPE_NONE) {
            uint8_t *result = LookForCompanionFilename(gamefile, index, type, gamefilelen, search, size);
            if (result) {
                return result;
            }
        }
    }

    return NULL;
}
