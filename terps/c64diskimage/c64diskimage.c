/*
 https://paradroid.automac.se/diskimage/

 Copyright (c) 2003-2006, Per Olofsson
 All rights reserved.

 Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

 Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 Cut down to a reader of whole files for Spatterlight: the write path, the
 file streams and the error info of the original are gone, and every track
 and sector link is checked before it is followed.
 */

#include "c64diskimage.h"

#include <stdlib.h>
#include <string.h>

#define D64SIZE 174848
#define D64ERRSIZE 175531
#define D64_40SIZE 196608
#define D64_40ERRSIZE 197376
#define D71SIZE 349696
#define D71ERRSIZE 351062
#define D81SIZE 819200
#define D81ERRSIZE 822400

#define SECTOR_SIZE 256
#define SECTOR_DATA 254 /* what is left of a sector after the link */
#define MAXTRACKS 80
#define MAXSECTORS 40

/* A directory sector holds eight entries. The first two bytes of the first
   are the link to the next directory sector. */
#define ENTRY_SIZE 32
#define ENTRY_TYPE 2 /* 0 in a scratched or never used entry */
#define ENTRY_START 3 /* track and sector of the first sector of the file */
#define ENTRY_NAME 5
#define ENTRY_BLOCKS 30 /* little endian */
#define NAME_LENGTH 16
#define NAME_PAD 0xa0

typedef enum { D64, D71, D81 } ImageType;

typedef struct {
    int track;
    int sector;
} TrackSector;

struct diskimage {
    const uint8_t *image;
    ImageType type;
    int tracks;
};

/* The sectors that a walk along a chain has been to, one bit each. */
typedef struct {
    uint8_t bits[(MAXTRACKS + 1) * MAXSECTORS / 8];
} Visited;

/* The 1541 has four zones of tracks. Tracks 36 to 40, on the disks that have
   them, are like those of the last zone. */
static int d64_sectors_per_track(int track)
{
    if (track < 18)
        return 21;
    if (track < 25)
        return 19;
    if (track < 31)
        return 18;
    return 17;
}

static int d64_first_block(int track)
{
    int block = 0;
    for (int t = 1; t < track; t++)
        block += d64_sectors_per_track(t);
    return block;
}

/* The second side of a D71 is laid out like the first */
static int sectors_per_track(const DiskImage *di, int track)
{
    switch (di->type) {
    case D64:
        return d64_sectors_per_track(track);
    case D71:
        return d64_sectors_per_track(track > 35 ? track - 35 : track);
    case D81:
        return 40;
    }
    return 0;
}

static int ts_is_valid(const DiskImage *di, TrackSector ts)
{
    return ts.track >= 1 && ts.track <= di->tracks
        && ts.sector >= 0 && ts.sector < sectors_per_track(di, ts.track);
}

/* Returns the 256 bytes of a sector, or NULL if it is not on the disk */
static const uint8_t *sector_data(const DiskImage *di, TrackSector ts)
{
    if (!ts_is_valid(di, ts))
        return NULL;

    int block = 0;
    switch (di->type) {
    case D64:
        block = d64_first_block(ts.track);
        break;
    case D71:
        if (ts.track > 35)
            block = d64_first_block(36) + d64_first_block(ts.track - 35);
        else
            block = d64_first_block(ts.track);
        break;
    case D81:
        block = (ts.track - 1) * 40;
        break;
    }
    return di->image + (size_t)(block + ts.sector) * SECTOR_SIZE;
}

static TrackSector link_of(const uint8_t *sector)
{
    TrackSector ts = { sector[0], sector[1] };
    return ts;
}

/* Marks a sector, which must be a valid one, as visited. Returns nonzero if
   it already was. */
static int visit(Visited *visited, TrackSector ts)
{
    int bit = ts.track * MAXSECTORS + ts.sector;
    uint8_t mask = (uint8_t)(1 << (bit & 7));
    int seen = visited->bits[bit >> 3] & mask;
    visited->bits[bit >> 3] |= mask;
    return seen;
}

DiskImage *di_create_from_data(const uint8_t *data, size_t length)
{
    ImageType type;
    int tracks;

    if (data == NULL)
        return NULL;

    switch (length) {
    case D64SIZE:
    case D64ERRSIZE:
        type = D64;
        tracks = 35;
        break;
    case D64_40SIZE:
    case D64_40ERRSIZE:
        type = D64;
        tracks = 40;
        break;
    case D71SIZE:
    case D71ERRSIZE:
        type = D71;
        tracks = 70;
        break;
    case D81SIZE:
    case D81ERRSIZE:
        type = D81;
        tracks = 80;
        break;
    default:
        return NULL;
    }

    DiskImage *di = malloc(sizeof *di);
    if (di == NULL)
        return NULL;
    di->image = data;
    di->type = type;
    di->tracks = tracks;
    return di;
}

void di_free(DiskImage *di)
{
    free(di);
}

/* A walk through the live entries of the directory */
typedef struct {
    const DiskImage *di;
    const uint8_t *sector; /* NULL at the end of the directory */
    int offset; /* of the next entry in sector */
    Visited visited;
} DirWalk;

static void dir_enter(DirWalk *walk, TrackSector ts)
{
    walk->offset = 0;
    walk->sector = sector_data(walk->di, ts);
    /* A directory that links back into itself ends there */
    if (walk->sector != NULL && visit(&walk->visited, ts))
        walk->sector = NULL;
}

static void dir_start(DirWalk *walk, const DiskImage *di)
{
    TrackSector first = { 18, 1 }; /* the 1541 and 1571 ignore the link of the BAM */

    memset(walk, 0, sizeof *walk);
    walk->di = di;
    if (di->type == D81) {
        TrackSector header = { 40, 0 };
        first = link_of(sector_data(di, header));
    }
    dir_enter(walk, first);
}

/* Returns the next entry that is in use, or NULL when there are no more */
static const uint8_t *dir_next(DirWalk *walk)
{
    while (walk->sector != NULL) {
        if (walk->offset == SECTOR_SIZE) {
            dir_enter(walk, link_of(walk->sector));
            continue;
        }
        const uint8_t *entry = walk->sector + walk->offset;
        walk->offset += ENTRY_SIZE;
        if (entry[ENTRY_TYPE] != 0)
            return entry;
    }
    return NULL;
}

/* Follows the chain of sectors that starts at ts and returns the number of
   bytes in the file, copying them to out unless it is NULL. Returns -1 if
   the first sector cannot be read. */
static long walk_chain(const DiskImage *di, TrackSector ts, uint8_t *out)
{
    Visited visited = { { 0 } };
    long total = 0;
    const uint8_t *sector = sector_data(di, ts);

    if (sector == NULL)
        return -1;

    while (sector != NULL && !visit(&visited, ts)) {
        TrackSector next = link_of(sector);
        int used = SECTOR_DATA;

        if (next.track == 0) {
            /* The last sector: the link gives the offset of its last byte */
            used = next.sector > 0 ? next.sector - 1 : 0;
        } else if (!ts_is_valid(di, next)) {
            /* A sector that links off the disk is not part of the file */
            return total > 0 ? total : -1;
        }

        if (out != NULL)
            memcpy(out + total, sector + 2, (size_t)used);
        total += used;

        if (next.track == 0)
            break;
        ts = next;
        sector = sector_data(di, ts);
    }
    return total;
}

static uint8_t *read_chain(const DiskImage *di, TrackSector ts, size_t *length)
{
    *length = 0;

    long size = walk_chain(di, ts, NULL);
    if (size < 0)
        return NULL;

    /* Allocate at least one byte, so that an empty file is not mistaken for
       a failure */
    uint8_t *file = malloc(size > 0 ? (size_t)size : 1);
    if (file == NULL)
        return NULL;
    walk_chain(di, ts, file);
    *length = (size_t)size;
    return file;
}

static uint8_t *read_entry(const DiskImage *di, const uint8_t *entry,
    size_t *length)
{
    TrackSector start = { entry[ENTRY_START], entry[ENTRY_START + 1] };
    return read_chain(di, start, length);
}

/* Compares a name of a directory entry, padded to 16 bytes, with a pattern
   that ends at its NUL. */
static int match_pattern(const char *pattern, const uint8_t *rawname)
{
    for (int i = 0; i < NAME_LENGTH; i++) {
        uint8_t c = (uint8_t)pattern[i];
        if (c == '*')
            return 1;
        if (c == 0 || c == NAME_PAD || rawname[i] == NAME_PAD)
            return (c == 0 || c == NAME_PAD) && rawname[i] == NAME_PAD;
        if (c != '?' && c != rawname[i])
            return 0;
    }
    return pattern[NAME_LENGTH] == 0;
}

uint8_t *di_read_file_named(const DiskImage *di, const char *name,
    size_t *length)
{
    DirWalk walk;
    const uint8_t *entry;

    *length = 0;
    dir_start(&walk, di);
    while ((entry = dir_next(&walk)) != NULL) {
        if (match_pattern(name, entry + ENTRY_NAME))
            return read_entry(di, entry, length);
    }
    return NULL;
}

char **di_get_all_file_names(const DiskImage *di, int *numfiles)
{
    DirWalk walk;
    const uint8_t *entry;
    char **names = NULL;
    int count = 0, capacity = 0;

    *numfiles = 0;
    dir_start(&walk, di);
    while ((entry = dir_next(&walk)) != NULL) {
        const uint8_t *rawname = entry + ENTRY_NAME;
        size_t len = 0;
        while (len < NAME_LENGTH && rawname[len] != NAME_PAD && rawname[len] != 0)
            len++;
        if (len == 0)
            continue;

        /* Keep room for the terminating NULL */
        if (count + 1 >= capacity) {
            capacity = capacity ? capacity * 2 : 16;
            char **grown = realloc(names, (size_t)capacity * sizeof *names);
            if (grown == NULL)
                break;
            names = grown;
        }
        char *name = malloc(len + 1);
        if (name == NULL)
            break;
        memcpy(name, rawname, len);
        name[len] = 0;
        names[count++] = name;
    }

    if (count == 0) {
        free(names);
        return NULL;
    }
    names[count] = NULL;
    *numfiles = count;
    return names;
}

uint8_t *di_get_file_named(const uint8_t *data, size_t length,
    size_t *newlength, const char *name)
{
    uint8_t *file = NULL;
    DiskImage *di = di_create_from_data(data, length);

    *newlength = 0;
    if (di != NULL) {
        file = di_read_file_named(di, name, newlength);
        di_free(di);
    }
    return file;
}

uint8_t *di_get_largest_file(const uint8_t *data, size_t length,
    size_t *newlength)
{
    uint8_t *file = NULL;
    DiskImage *di = di_create_from_data(data, length);

    *newlength = 0;
    if (di != NULL) {
        DirWalk walk;
        const uint8_t *entry, *largest = NULL;
        int largestblocks = 0;

        dir_start(&walk, di);
        while ((entry = dir_next(&walk)) != NULL) {
            int blocks = entry[ENTRY_BLOCKS] + entry[ENTRY_BLOCKS + 1] * 0x100;
            if (blocks > largestblocks) {
                largest = entry;
                largestblocks = blocks;
            }
        }
        if (largest != NULL)
            file = read_entry(di, largest, newlength);
        di_free(di);
    }
    return file;
}

uint8_t *di_get_file_at_ts(const uint8_t *data, size_t length,
    size_t *newlength, int track, int sector)
{
    uint8_t *file = NULL;
    DiskImage *di = di_create_from_data(data, length);

    *newlength = 0;
    if (di != NULL) {
        TrackSector ts = { track, sector };
        file = read_chain(di, ts, newlength);
        di_free(di);
    }
    return file;
}
