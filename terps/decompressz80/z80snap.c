/* z80snap.c: reads .z80 snapshots of the ZX Spectrum.

   Part of the decompressz80 library. This began as the .z80 reader of
   libspectrum / Fuse, cut down to what the interpreters need: the RAM of the
   snapshot and, for a 128K machine, which page was banked in. The registers
   and the hardware state are not read.
*/

/* z80.c: Routines for handling .z80 snapshots
   Copyright (c) 2001-2009 Philip Kendall, Darren Salt, Fredrick Meunier

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

   Author contact information:

   E-mail: philip-fuse@shadowmagic.org.uk

*/

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "decompressz80.h"
#include "decompressz80_internal.h"

#define PAGE ZX_PAGE_SIZE

/* RAM pages 0 to 7 are all a 128K Spectrum has. A snapshot of a Pentagon 512
   or a Scorpion 256 has more of them, which are read past and dropped. */
#define RAM_PAGES 8

/* The header that every version has, and the offsets into it that matter */
#define HEADER_LENGTH 30
#define HEADER_PC 6          /* 0 in a version 2 or 3 file */
#define HEADER_FLAGS 12
#define FLAG_COMPRESSED 0x20 /* version 1 only */

/* Versions 2 and 3 follow it with a length and that many bytes more. The
   length tells the versions apart: 23 is version 2, 54 or 55 version 3. */
#define EXTRA_LENGTH_AT 30
#define EXTRA_HEADER_AT 32
#define EXTRA_MACHINE 2 /* offsets from EXTRA_HEADER_AT */
#define EXTRA_PORT_7FFD 3

/* In place of a memory page a version 2 or 3 file may have this, and after it
   the level data of a .slt file, which is of no use here */
static const uint8_t slt_signature[6] = { 0, 0, 0, 'S', 'L', 'T' };

typedef struct {
    uint8_t *ram;       /* RAM_PAGES pages in page order, zero where the file has none */
    uint8_t present;    /* bit n is set when the file had page n */
    int is_128k;        /* the memory is paged; otherwise only pages 5, 2 and 0 mean anything */
    int scorpion;       /* file page 11 is RAM and not the Multiface ROM */
    uint8_t port_7ffd;  /* last value written to the paging port, 0 on a 48K machine */
} Snapshot;

/* Sets the memory layout of the machine that hardware type 'type' of a
   version 2 or 3 header stands for. Returns 0 if the type is not known.

   Types below 7 mean different things in the two versions:

     type   version 2      version 3
     0      48K            48K
     1      48K + IF1      48K + IF1
     2      SamRam         SamRam
     3      128K           48K + MGT
     4      128K + IF1     128K
     5      -              128K + IF1
     6      -              128K + MGT

   From 7 they mean the same in both. Bit 7 of byte 37 of the file turns a 48K
   into a 16K, a 128K into a +2 and a +3 into a +2A, none of which has its
   memory laid out differently, so it is not looked at. */
static int SetMachine(Snapshot *snap, int version, uint8_t type)
{
    snap->is_128k = 0;
    snap->scorpion = 0;

    if (type < 7) {
        int first_128k = (version == 2) ? 3 : 4;
        int last = (version == 2) ? 4 : 6;
        if (type > last)
            return 0;
        snap->is_128k = (type >= first_128k);
        return 1;
    }

    switch (type) {
    case 7:   /* +3 */
    case 8:   /* +3, as xzx wrote it */
    case 9:   /* Pentagon */
    case 12:  /* +2 */
    case 13:  /* +2A */
        snap->is_128k = 1;
        return 1;
    case 10:  /* Scorpion */
        snap->is_128k = 1;
        snap->scorpion = 1;
        return 1;
    case 14:  /* TC2048 */
    case 15:  /* TC2068 */
    case 128: /* TS2068 */
        return 1;
    default:
        return 0;
    }
}

#define PAGE_DROPPED (-1)
#define PAGE_UNKNOWN (-2)

/* The RAM page that page number 'page' of a version 2 or 3 file is stored as,
   PAGE_DROPPED for one that is not kept or PAGE_UNKNOWN for a number the
   format does not have.

     page    128K machine           48K machine
     1, 2    ROMs                   ROMs
     3       RAM page 0             a ROM
     4       RAM page 1             0x8000-0xBFFF, kept as RAM page 2
     5       RAM page 2             0xC000-0xFFFF, kept as RAM page 0
     6, 7    RAM pages 3, 4         -
     8       RAM page 5             0x4000-0x7FFF, kept as RAM page 5
     9, 10   RAM pages 6, 7         -
     11      Multiface ROM, or RAM page 8 of a Scorpion
     12-18   RAM pages 9-15 of a Pentagon 512 or a Scorpion

   A 48K memory image is kept in the pages a 128K machine has at the same
   addresses, so that pages 5, 2 and 0 are 0x4000-0xFFFF for both. */
static int RamPage(const Snapshot *snap, int page)
{
    if (page < 1 || page > 18)
        return PAGE_UNKNOWN;
    if (page < 3)
        return PAGE_DROPPED;
    if (page == 11 && !snap->scorpion)
        return PAGE_DROPPED;

    if (!snap->is_128k) {
        if (page == 3)
            return PAGE_DROPPED;
        if (page == 4)
            return 2;
        if (page == 5)
            return 0;
    }

    return page - 3 < RAM_PAGES ? page - 3 : PAGE_DROPPED;
}

/* Expands compressed .z80 data until it has made 'size' bytes: ED ED nn xx is
   xx repeated nn times, anything else is itself. Returns how much of 'src' it
   took to make them, or 0 if 'src' makes fewer or has a run going past 'size'
   bytes. */
static size_t Unpack(uint8_t *dst, size_t size, const uint8_t *src, size_t src_length)
{
    size_t in = 0, out = 0;

    while (out < size && in < src_length) {
        /* Not a run unless all four bytes of it are there */
        if (src_length - in >= 4 && src[in] == 0xed && src[in + 1] == 0xed) {
            size_t run = src[in + 2];
            if (run > size - out)
                return 0;
            memset(dst + out, src[in + 3], run);
            out += run;
            in += 4;
        } else {
            dst[out++] = src[in++];
        }
    }

    return out == size ? in : 0;
}

/* A version 1 file ends its compressed RAM with this */
static const uint8_t v1_end_marker[4] = { 0x00, 0xed, 0xed, 0x00 };

/* Version 1: nothing but the 48K of RAM from 0x4000, compressed or not.

   A .z80 file has no signature, and a version 1 header is nothing but
   register values, so this is where files that are not snapshots get turned
   away: the RAM has to be all there is after the header. Compressed, it is
   what comes before the first end marker, it has to make exactly 48K, and
   the marker has to be the end of the file. The interpreters rely on this,
   so it is not relaxed for a snapshot with its marker lost or with something
   added to it. */
static int ReadV1Memory(Snapshot *snap, const uint8_t *body, size_t length, int compressed)
{
    static const int page_at[3] = { 5, 2, 0 };
    uint8_t *ram48 = NULL;
    const uint8_t *image = body;

    if (compressed) {
        size_t packed = 0;
        while (packed + sizeof v1_end_marker <= length
            && memcmp(body + packed, v1_end_marker, sizeof v1_end_marker) != 0)
            packed++;
        if (packed + sizeof v1_end_marker != length) {
            ZX_LOG("version 1 snapshot does not end with its end marker");
            return 0;
        }
        ram48 = malloc(3 * PAGE);
        if (ram48 == NULL)
            return 0;
        if (packed == 0 || Unpack(ram48, 3 * PAGE, body, packed) != packed) {
            ZX_LOG("version 1 snapshot does not uncompress to 48K");
            free(ram48);
            return 0;
        }
        image = ram48;
    } else if (length != 3 * PAGE) {
        ZX_LOG("version 1 snapshot is not 48K long");
        return 0;
    }

    for (int i = 0; i < 3; i++) {
        memcpy(snap->ram + page_at[i] * PAGE, image + i * PAGE, PAGE);
        snap->present |= 1 << page_at[i];
    }

    free(ram48);
    return 1;
}

/* Versions 2 and 3: a list of pages, each one a length, a page number and the
   data. A length of 0xffff is for a page that is not compressed. */
static int ReadPages(Snapshot *snap, const uint8_t *body, size_t length)
{
    const uint8_t *p = body, *end = body + length;

    while (p < end) {
        if (end - p < 3) {
            ZX_LOG("page header cut short");
            return 0;
        }

        size_t stored = ReadLE(p, 2);
        int page = p[2];

        if (stored == 0 && page == 0 && end - p > 8
            && memcmp(p, slt_signature, sizeof slt_signature) == 0)
            break;

        p += 3;
        int compressed = (stored != 0xffff);
        if (!compressed)
            stored = PAGE;
        if ((size_t)(end - p) < stored) {
            ZX_LOG("page %d runs past the end of the file", page);
            return 0;
        }

        int ram_page = RamPage(snap, page);
        if (ram_page == PAGE_UNKNOWN) {
            ZX_LOG("unknown page %d", page);
            return 0;
        }
        if (ram_page != PAGE_DROPPED) {
            if (snap->present & (1 << ram_page)) {
                ZX_LOG("page %d is there twice", page);
                return 0;
            }
            uint8_t *dst = snap->ram + ram_page * PAGE;
            if (!compressed) {
                memcpy(dst, p, PAGE);
            } else if (Unpack(dst, PAGE, p, stored) != stored) {
                ZX_LOG("page %d does not uncompress to 16K", page);
                return 0;
            }
            snap->present |= 1 << ram_page;
        }

        p += stored;
    }

    return 1;
}

/* Reads the .z80 file in 'data' into 'snap'. Returns 0 if it is not one, and
   then there is nothing to free. */
static int LoadSnapshot(Snapshot *snap, const uint8_t *data, size_t length)
{
    memset(snap, 0, sizeof *snap);

    if (data == NULL || length < HEADER_LENGTH) {
        ZX_LOG("shorter than a .z80 header");
        return 0;
    }

    int version = 1;
    size_t body = HEADER_LENGTH;

    if (ReadLE(data + HEADER_PC, 2) == 0) {
        if (length < EXTRA_HEADER_AT) {
            ZX_LOG("shorter than a .z80 header");
            return 0;
        }
        size_t extra_length = ReadLE(data + EXTRA_LENGTH_AT, 2);
        if (extra_length == 23) {
            version = 2;
        } else if (extra_length == 54 || extra_length == 55) {
            version = 3;
        } else {
            ZX_LOG("unknown header length %d", (int)extra_length);
            return 0;
        }
        body = EXTRA_HEADER_AT + extra_length;
        if (length < body) {
            ZX_LOG("shorter than a .z80 header");
            return 0;
        }

        const uint8_t *extra = data + EXTRA_HEADER_AT;
        if (!SetMachine(snap, version, extra[EXTRA_MACHINE])) {
            ZX_LOG("unknown version %d machine type %d", version, extra[EXTRA_MACHINE]);
            return 0;
        }
        if (snap->is_128k)
            snap->port_7ffd = extra[EXTRA_PORT_7FFD];
    }

    snap->ram = calloc(RAM_PAGES, PAGE);
    if (snap->ram == NULL)
        return 0;

    int ok;
    if (version == 1)
        ok = ReadV1Memory(snap, data + body, length - body,
            data[HEADER_FLAGS] & FLAG_COMPRESSED);
    else
        ok = ReadPages(snap, data + body, length - body);

    if (ok && snap->present == 0) {
        ZX_LOG("snapshot has no RAM in it");
        ok = 0;
    }

    if (!ok) {
        free(snap->ram);
        snap->ram = NULL;
    }
    return ok;
}

uint8_t *DecompressZ80(const uint8_t *raw_data, size_t *length)
{
    Snapshot snap;
    if (length == NULL || !LoadSnapshot(&snap, raw_data, *length))
        return NULL;

    /* The pages to put first: the three that are in the address space */
    int order[RAM_PAGES] = { 5, 2, 0 };
    int count = 3;
    size_t size = Z80_IMAGE_SIZE_48K;

    if (snap.is_128k) {
        size = Z80_IMAGE_SIZE_128K;
        /* The third is the page banked in at 0xC000, unless that is one of
           the first two, and then come the rest in page order */
        count = 2;
        int banked = snap.port_7ffd & 0x07;
        if (banked != 5 && banked != 2)
            order[count++] = banked;
        for (int i = 0; i < RAM_PAGES; i++)
            if (i != 5 && i != 2 && i != banked)
                order[count++] = i;
    }

    uint8_t *image = calloc(1, size);
    if (image != NULL) {
        for (int i = 0; i < count; i++)
            memcpy(image + i * PAGE, snap.ram + order[i] * PAGE, PAGE);
        *length = size;
    }

    free(snap.ram);
    return image;
}

uint8_t *DecompressZ80Pages(const uint8_t *raw_data, size_t length,
    int *is_128k, int *banked_page)
{
    Snapshot snap;
    if (!LoadSnapshot(&snap, raw_data, length))
        return NULL;

    if (is_128k)
        *is_128k = snap.is_128k;
    if (banked_page)
        *banked_page = snap.port_7ffd & 0x07;

    return snap.ram;
}
