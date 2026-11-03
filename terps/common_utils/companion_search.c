//
//  companion_search.c
//  Spatterlight
//
//  Finds the other disk (or disk side) of a multi-disk game.
//  Originally part of Plus (companionfile.c), created by Petter Sjölund
//  on 2022-10-10.
//

#include <ctype.h>
#include <string.h>

#include "common_file_utils.h"
#include "debugprint.h"

#include "companion_search.h"

/* Room for the longest marker a fallback inserts */
#define FILENAME_BUFFER_EXTRA 16
#define MIN_FILENAME_LENGTH 4
#define MIN_COMPANION_KEYWORD_LENGTH 3

static inline int is_path_separator(char c)
{
    return (c == '/' || c == '\\');
}

int CompanionStripEnclosed(char *buffer, size_t namelen, char open, char close, int strip_space)
{
    if (!buffer || namelen < MIN_FILENAME_LENGTH) {
        return 0;
    }

    // Find file extension (everything after the last period)
    char *extension = strrchr(buffer, '.');
    if (!extension) {
        return 0;
    }

    // Find last closing character before extension
    char *close_pos = strrchr(buffer, close);
    if (!close_pos || close_pos >= extension) {
        return 0;
    }

    // Find matching opening character
    char *open_pos = strrchr(buffer, open);
    if (!open_pos || open_pos >= close_pos) {
        return 0;
    }

    if (strip_space && open_pos > buffer && *(open_pos - 1) == ' ') {
        open_pos--;
    }

    // Remove the enclosed section, keeping the rest including the terminating zero
    memmove(open_pos, close_pos + 1, strlen(close_pos + 1) + 1);
    return 1;
}

int CompanionInsertBeforeExtension(char *buffer, size_t bufsize, size_t namelen, const char *marker)
{
    size_t markerlen = strlen(marker);

    // Find the period before file extension
    size_t ppos = namelen - 1;
    while (buffer[ppos] != '.' && ppos > 0) {
        ppos--;
    }

    if (ppos < 1) {
        return 0;
    }

    if (namelen + markerlen >= bufsize) {
        return 0;
    }

    // Shift extension, including the terminating zero, to make room for marker
    for (size_t i = namelen; i >= ppos; i--) {
        buffer[i + markerlen] = buffer[i];
    }

    memcpy(buffer + ppos, marker, markerlen);
    return 1;
}

uint8_t *CompanionBracketFallback(char *buffer, size_t bufsize, size_t namelen, int index, CompanionNameType type, CompanionReader read, void *user, size_t *size)
{
    (void)index;
    switch (type) {
        case TYPE_B:
            // "fileB[tag].dsk" -> "fileB.dsk"
            if (CompanionStripEnclosed(buffer, namelen, '[', ']', 0)) {
                debug_print("looking for companion file (with removed text in brackets): \"%s\"\n", buffer);
                return read(buffer, size, user);
            }
            break;
        case TYPE_A:
            // "fileA.dsk" -> "fileA[cr CSS].dsk"
            if (CompanionInsertBeforeExtension(buffer, bufsize, namelen, "[cr CSS]")) {
                debug_print("looking for companion file (with added [cr CSS]): \"%s\"\n", buffer);
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
    return isupper((unsigned char)original) ? toupper((unsigned char)replacement) : tolower((unsigned char)replacement);
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
    char buffer[stringlen + FILENAME_BUFFER_EXTRA];
    CompanionReader read = search->read ? search->read : default_reader;
    uint8_t *result = NULL;

    memcpy(buffer, gamefile, stringlen + 1);

    switch (type) {
        case TYPE_A:
            replace_word(buffer + index, "a");
            break;
        case TYPE_B:
            replace_word(buffer + index, "b");
            break;
        case TYPE_1:
            buffer[index] = '1';
            break;
        case TYPE_2:
            buffer[index] = '2';
            break;
        case TYPE_ONE:
            replace_word(buffer + index, "one");
            break;
        case TYPE_TWO:
            replace_word(buffer + index, "two");
            break;
        case TYPE_NONE:
            return NULL;
    }

    debug_print("looking for companion file \"%s\"\n", buffer);
    result = read(buffer, filesize, search->user);

    if (!result && search->fallback) {
        result = search->fallback(buffer, sizeof(buffer), stringlen, index, type,
                                  read, search->user, filesize);
    }

    return result;
}

/**
 * Checks if the position in filename matches a companion file pattern
 * Patterns: "side", "disk", or (optionally) a single digit/letter before
 * the extension
 */
static int is_companion_name(const char *filename, int i, char c, int match_char_before_extension)
{
    if (i <= MIN_COMPANION_KEYWORD_LENGTH) {
        return 0;
    }

    char c_minus1 = tolower(filename[i - 1]);
    char c_minus2 = tolower(filename[i - 2]);
    char c_minus3 = tolower(filename[i - 3]);

    // Check for "side" pattern
    if (c == 'e' && c_minus1 == 'd' && c_minus2 == 'i' && c_minus3 == 's') {
        return 1;
    }

    // Check for "disk" pattern
    if (c == 'k' && c_minus1 == 's' && c_minus2 == 'i' && c_minus3 == 'd') {
        return 1;
    }

    // Check for single digit/letter before extension
    if (match_char_before_extension && c == '.' &&
        (c_minus1 == '1' || c_minus1 == '2' ||
         c_minus1 == 'a' || c_minus1 == 'b')) {
        return 1;
    }

    return 0;
}

/**
 * Determines the companion file type based on the current character
 * This maps the current disk identifier to what we should look for
 */
static CompanionNameType determine_type(const char *filename, size_t length,
                                        int i, char character)
{
    switch (character) {
        case 'a':
            return TYPE_B;  // If we have 'a', look for 'B'

        case 'b':
            return TYPE_A;  // If we have 'b', look for 'A'

        case 't':
            // Check for "two" -> look for "one"
            if (length > (size_t)(i + 4) &&
                tolower(filename[i + 3]) == 'w' &&
                tolower(filename[i + 4]) == 'o') {
                return TYPE_ONE;
            }
            break;

        case 'o':
            // Check for "one" -> look for "two"
            if (length > (size_t)(i + 4) &&
                tolower(filename[i + 3]) == 'n' &&
                tolower(filename[i + 4]) == 'e') {
                return TYPE_TWO;
            }
            break;

        case '2':
            return TYPE_1;  // If we have '2', look for '1'

        case '1':
            return TYPE_2;  // If we have '1', look for '2'
    }

    return TYPE_NONE;
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

    uint8_t *result = NULL;

    // Scan backwards through filename from the end
    // Stop at path separator (we only care about the filename, not the path)
    for (int i = (int)gamefilelen - 1; i >= 0 && !is_path_separator(gamefile[i]); i--) {
        char c = tolower(gamefile[i]);

        // Check if this position matches a companion file pattern
        if (is_companion_name(gamefile, i, c, search->match_char_before_extension) && gamefilelen > (size_t)(i + 2)) {
            // A single character before the extension, or "side" or "disk"
            // followed by a separator (space, underscore, or period)
            int before_extension = (c == '.');
            if (!before_extension) {
                c = gamefile[i + 1];
            }

            // Only proceed if separator is valid
            if (c == ' ' || c == '_' || c == '.') {
                // Extract the disk identifier character
                char disk_char;
                int adjusted_index;

                if (before_extension) {
                    // Pattern: "file1.dsk" or "filea.dsk"
                    disk_char = tolower(gamefile[i - 1]);
                    adjusted_index = i - 3;
                } else {
                    // Pattern: "side 1.dsk" or "disk_2.dsk"
                    disk_char = tolower(gamefile[i + 2]);
                    adjusted_index = i;
                }

                // Determine what type of companion to look for
                CompanionNameType type = determine_type(gamefile, gamefilelen,
                                                        adjusted_index, disk_char);

                if (type != TYPE_NONE) {
                    result = LookForCompanionFilename(gamefile, adjusted_index + 2,
                                                      type, gamefilelen, search, size);
                    if (result) {
                        return result;
                    }
                }
            }
        }
    }

    return NULL;
}
