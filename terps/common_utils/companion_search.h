/*
 *  companion_search.h
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *
 *  Finds the other disk (or disk side) of a multi-disk game by rewriting
 *  the game file name: "side 1" <-> "side 2", "disk a" <-> "disk b",
 *  "side one" <-> "side two" and so on.
 *
 *  Shared by Plus and ScottFree. Each caller supplies the file reader and
 *  any interpreter-specific fallback renames.
 */

#ifndef companion_search_h
#define companion_search_h

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* A cracker's tag that some Atari 8-bit disk images carry on side A only */
#define COMPANION_CRACK_TAG "[cr CSS]"

typedef enum {
    COMPANION_TYPE_NONE,
    COMPANION_TYPE_A,
    COMPANION_TYPE_B,
    COMPANION_TYPE_ONE,
    COMPANION_TYPE_TWO,
    COMPANION_TYPE_1,
    COMPANION_TYPE_2,
} CompanionNameType;

/* Reads a whole file. Returns NULL if it does not exist. */
typedef uint8_t *(*CompanionReader)(const char *filename, size_t *size, void *user);

/* Called when the plainly renamed file was not found. The buffer holds the
   renamed file name (namelen characters plus a terminating zero) and has room
   for bufsize bytes. May rewrite the buffer and call read. Returns the file
   data, or NULL. */
typedef uint8_t *(*CompanionFallback)(char *buffer, size_t bufsize, size_t namelen, int index, CompanionNameType type, CompanionReader read, void *user, size_t *size);

typedef struct {
    CompanionReader read; /* NULL means ReadFileIfExists */
    void *user; /* Passed on to read */
    CompanionFallback fallback; /* May be NULL */
    /* Also match a single digit or letter just before the extension,
       as in "game1.dsk" <-> "game2.dsk" */
    int match_char_before_extension;
} CompanionSearch;

uint8_t *FindCompanionFile(const char *gamefile, const CompanionSearch *search, size_t *size);

/* Helpers for fallbacks */

/* Removes the last open...close pair before the file extension, e.g.
   "file[tag].dsk" -> "file.dsk". If strip_space is set, a single space
   before the opening character is removed as well. Looks only at the file
   name, not the folders before it. Returns 1 on success. */
int CompanionStripEnclosed(char *buffer, size_t namelen, char open, char close, int strip_space);

/* Inserts marker before the file extension, e.g. "file.dsk" ->
   "file[cr CSS].dsk". Returns 0 if the file name has no extension or
   bufsize is too small. */
int CompanionInsertBeforeExtension(char *buffer, size_t bufsize, size_t namelen, const char *marker);

/* The fallback used by both Plus and the ScottFree Atari 8-bit loader:
   strip a bracketed tag when looking for side B, add COMPANION_CRACK_TAG when
   looking for side A. */
uint8_t *CompanionBracketFallback(char *buffer, size_t bufsize, size_t namelen, int index, CompanionNameType type, CompanionReader read, void *user, size_t *size);

#ifdef __cplusplus
}
#endif

#endif /* companion_search_h */
