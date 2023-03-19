/* decompressz80.c: This is a bare-minimum version of the z80
   handling code from libspectrum / Fuse, used to decompress
   .z80 files and read .tzx file in TaylorMade.

   I simply copied things from different places in the original source
   to make it compile and deleted anything I deemed unimportant for my
   purposes.

   Most of the code in here is probably still unnecessary and in
   need of cleanup.
*/

/* z80.c: Routines for handling .z80 snapshots
   Copyright (c) 2001-2009 Philip Kendall, Darren Salt, Fredrick Meunier

   $Id$

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

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Sizes of some of the arrays in the snap structure */
#define SNAPSHOT_RAM_PAGES 16

#define libspectrum_new(type, count) \
    ((type *)libspectrum_malloc_n((count), sizeof(type)))

#define libspectrum_renew(type, mem, count) \
    ((type *)libspectrum_realloc_n((void *)mem, (count), sizeof(type)))

static void *libspectrum_realloc_n(void *ptr, size_t nmemb, size_t size)
{
    if (nmemb > SIZE_MAX / size)
        abort();

    return realloc(ptr, nmemb * size);
}

static void *libspectrum_malloc(size_t size)
{
    void *ptr = malloc(size);

    /* If size == 0, acceptable to return NULL */
    if (size && !ptr)
        abort();

    return ptr;
}

static void *libspectrum_malloc_n(size_t nmemb, size_t size)
{
    if (nmemb > SIZE_MAX / size)
        abort();

    return libspectrum_malloc(nmemb * size);
}

/* Ensure there is room for `requested' characters after the current
 position `ptr' in `buffer'. If not, renew() and update the
 pointers as necessary */
static void libspectrum_make_room(uint8_t **dest, size_t requested, uint8_t **ptr,
    size_t *allocated)
{
    size_t current_length = 0;

    if (*allocated == 0) {

        (*allocated) = requested;
        *dest = libspectrum_new(uint8_t, requested);

    } else {
        current_length = *ptr - *dest;

        /* If there's already enough room here, just return */
        if (current_length + requested <= (*allocated))
            return;

        /* Make the new size the maximum of the new needed size and the
     old allocated size * 2 */
        (*allocated) = current_length + requested > 2 * (*allocated)
            ? current_length + requested
            : 2 * (*allocated);

        *dest = libspectrum_renew(uint8_t, *dest, *allocated);
    }

    /* Update the secondary pointer to the block */
    *ptr = *dest + current_length;
}

struct libspectrum_snap {

    /* Which machine are we using here? */
    int machine;

    /* Registers and the like */
    uint16_t pc;

    uint8_t out_128_memoryport;

    uint8_t *pages[SNAPSHOT_RAM_PAGES];
};

typedef struct libspectrum_snap libspectrum_snap;

/* Error handling */

/* The various errors which can occur */
typedef enum libspectrum_error {

    LIBSPECTRUM_ERROR_NONE = 0,

    LIBSPECTRUM_ERROR_WARNING,
    LIBSPECTRUM_ERROR_MEMORY,
    LIBSPECTRUM_ERROR_UNKNOWN,
    LIBSPECTRUM_ERROR_CORRUPT,
    LIBSPECTRUM_ERROR_SIGNATURE,
    LIBSPECTRUM_ERROR_SLT, /* .slt data found at end of a .z80 file */
    LIBSPECTRUM_ERROR_INVALID, /* Invalid parameter supplied */

    LIBSPECTRUM_ERROR_LOGIC = -1,

} libspectrum_error;

/* The machine types we can handle */
typedef enum libspectrum_machine {

    LIBSPECTRUM_MACHINE_48,
    LIBSPECTRUM_MACHINE_TC2048,
    LIBSPECTRUM_MACHINE_128,
    LIBSPECTRUM_MACHINE_PLUS2,
    LIBSPECTRUM_MACHINE_PENT,
    LIBSPECTRUM_MACHINE_PLUS2A,
    LIBSPECTRUM_MACHINE_PLUS3,

    /* Used by libspectrum_tape_guess_hardware if we can't work out what
   hardware should be used */
    LIBSPECTRUM_MACHINE_UNKNOWN,

    LIBSPECTRUM_MACHINE_16,
    LIBSPECTRUM_MACHINE_TC2068,

    LIBSPECTRUM_MACHINE_SCORP,
    LIBSPECTRUM_MACHINE_PLUS3E,
    LIBSPECTRUM_MACHINE_SE,

    LIBSPECTRUM_MACHINE_TS2068,
    LIBSPECTRUM_MACHINE_PENT512,
    LIBSPECTRUM_MACHINE_PENT1024,
    LIBSPECTRUM_MACHINE_48_NTSC,

    LIBSPECTRUM_MACHINE_128E,

} libspectrum_machine;

typedef enum libspectrum_machine_capability {
    LIBSPECTRUM_MACHINE_CAPABILITY_128_MEMORY = (1u << 0),
    LIBSPECTRUM_MACHINE_CAPABILITY_PLUS3_MEMORY = (1u << 1),
    LIBSPECTRUM_MACHINE_CAPABILITY_TIMEX_MEMORY = (1u << 2),
    LIBSPECTRUM_MACHINE_CAPABILITY_SCORP_MEMORY = (1u << 3),
    LIBSPECTRUM_MACHINE_CAPABILITY_SE_MEMORY = (1u << 4),
    LIBSPECTRUM_MACHINE_CAPABILITY_PENT512_MEMORY = (1u << 5),
    LIBSPECTRUM_MACHINE_CAPABILITY_PENT1024_MEMORY = (1u << 6),
} libspectrum_machine_capability;

/* Given a machine type, what features does it have? */
static int libspectrum_machine_capabilities(libspectrum_machine type)
{
    int capabilities = 0;

    /* 128K Spectrum-style 0x7ffd memory paging */
    switch (type) {
    case LIBSPECTRUM_MACHINE_128:
    case LIBSPECTRUM_MACHINE_PLUS2:
    case LIBSPECTRUM_MACHINE_PLUS2A:
    case LIBSPECTRUM_MACHINE_PLUS3:
    case LIBSPECTRUM_MACHINE_PLUS3E:
    case LIBSPECTRUM_MACHINE_128E:
    case LIBSPECTRUM_MACHINE_PENT:
    case LIBSPECTRUM_MACHINE_PENT512:
    case LIBSPECTRUM_MACHINE_PENT1024:
    case LIBSPECTRUM_MACHINE_SCORP:
        /* FIXME: SE needs to have this capability to be considered a 128k machine
     */
    case LIBSPECTRUM_MACHINE_SE:
        capabilities |= LIBSPECTRUM_MACHINE_CAPABILITY_128_MEMORY;
        break;
    default:
        break;
    }

    /* +3 Spectrum-style 0x1ffd memory paging */
    switch (type) {
    case LIBSPECTRUM_MACHINE_PLUS2A:
    case LIBSPECTRUM_MACHINE_PLUS3:
    case LIBSPECTRUM_MACHINE_PLUS3E:
    case LIBSPECTRUM_MACHINE_128E:
        capabilities |= LIBSPECTRUM_MACHINE_CAPABILITY_PLUS3_MEMORY;
        break;
    default:
        break;
    }

    /* T[CS]20[46]8-style 0x00fd memory paging */
    switch (type) {
    case LIBSPECTRUM_MACHINE_TC2048:
    case LIBSPECTRUM_MACHINE_TC2068:
    case LIBSPECTRUM_MACHINE_TS2068:
        capabilities |= LIBSPECTRUM_MACHINE_CAPABILITY_TIMEX_MEMORY;
        break;
    default:
        break;
    }

    /* Scorpion-style 0x1ffd memory paging */
    switch (type) {
    case LIBSPECTRUM_MACHINE_SCORP:
        capabilities |= LIBSPECTRUM_MACHINE_CAPABILITY_SCORP_MEMORY;
        break;
    default:
        break;
    }

    /* SE-style 0x7ffd and 0x00fd memory paging */
    switch (type) {
    case LIBSPECTRUM_MACHINE_SE:
        capabilities |= LIBSPECTRUM_MACHINE_CAPABILITY_SE_MEMORY;
        break;
    default:
        break;
    }

    /* Pentagon 512-style memory paging */
    switch (type) {
    case LIBSPECTRUM_MACHINE_PENT512:
    case LIBSPECTRUM_MACHINE_PENT1024:
        capabilities |= LIBSPECTRUM_MACHINE_CAPABILITY_PENT512_MEMORY;
        break;
    default:
        break;
    }

    /* Pentagon 1024-style memory paging */
    switch (type) {
    case LIBSPECTRUM_MACHINE_PENT1024:
        capabilities |= LIBSPECTRUM_MACHINE_CAPABILITY_PENT1024_MEMORY;
        break;
    default:
        break;
    }

    return capabilities;
}

/* Length of the basic .z80 headers */
static const int LIBSPECTRUM_Z80_HEADER_LENGTH = 30;

/* Length of the v2 extensions */
#define LIBSPECTRUM_Z80_V2_LENGTH 23

/* Length of the v3 extensions */
#define LIBSPECTRUM_Z80_V3_LENGTH 54

/* Length of xzx's extensions */
#define LIBSPECTRUM_Z80_V3X_LENGTH 55

/* The constants used for each machine type */
enum {

    /* v2 constants */
    Z80_MACHINE_48_V2 = 0,
    Z80_MACHINE_48_IF1_V2 = 1,
    Z80_MACHINE_48_SAMRAM_V2 = 2,
    Z80_MACHINE_128_V2 = 3,
    Z80_MACHINE_128_IF1_V2 = 4,

    /* v3 constants */
    Z80_MACHINE_48 = 0,
    Z80_MACHINE_48_IF1 = 1,
    Z80_MACHINE_48_SAMRAM = 2,
    Z80_MACHINE_48_MGT = 3,
    Z80_MACHINE_128 = 4,
    Z80_MACHINE_128_IF1 = 5,
    Z80_MACHINE_128_MGT = 6,

    /* Extensions */
    Z80_MACHINE_PLUS3 = 7,
    Z80_MACHINE_PLUS3_XZX_ERROR = 8,
    Z80_MACHINE_PENTAGON = 9,
    Z80_MACHINE_SCORPION = 10,
    Z80_MACHINE_PLUS2 = 12,
    Z80_MACHINE_PLUS2A = 13,
    Z80_MACHINE_TC2048 = 14,
    Z80_MACHINE_TC2068 = 15,
    Z80_MACHINE_TS2068 = 128,

    /* The first extension ID; anything here or greater applies to both
     v2 and v3 files */
    Z80_MACHINE_FIRST_EXTENSION = Z80_MACHINE_PLUS3,
};

static libspectrum_error read_header(const uint8_t *buffer,
    libspectrum_snap *snap,
    const uint8_t **data, int *version,
    int *compressed);
static libspectrum_error get_machine_type(libspectrum_snap *snap, uint8_t type, int version);
static libspectrum_error get_machine_type_v2(libspectrum_snap *snap,
    uint8_t type);
static libspectrum_error get_machine_type_v3(libspectrum_snap *snap,
    uint8_t type);
static libspectrum_error get_machine_type_extension(libspectrum_snap *snap,
    uint8_t type);

static libspectrum_error read_blocks(const uint8_t *buffer,
    size_t buffer_length,
    libspectrum_snap *snap, int version,
    int compressed);
static libspectrum_error read_block(const uint8_t *buffer,
    libspectrum_snap *snap,
    const uint8_t **next_block,
    const uint8_t *end, int version,
    int compressed);
static libspectrum_error read_v1_block(const uint8_t *buffer, int is_compressed,
    uint8_t **uncompressed,
    const uint8_t **next_block,
    const uint8_t *end);
static libspectrum_error read_v2_block(const uint8_t *buffer, uint8_t **block,
    size_t *length, int *page,
    const uint8_t **next_block,
    const uint8_t *end);

static void uncompress_block(uint8_t **dest, size_t *dest_length,
    const uint8_t *src, size_t src_length);

void libspectrum_snap_set_machine(libspectrum_snap *snap, int val)
{
    snap->machine = val;
}

uint8_t *libspectrum_snap_pages(libspectrum_snap *snap, int page)
{
    return snap->pages[page];
}

void libspectrum_snap_set_pages(libspectrum_snap *snap, int page,
    uint8_t *buf)
{
    snap->pages[page] = buf;
}

static void libspectrum_print_error(libspectrum_error error, const char *fmt, ...)
{
    va_list ap;
    char msg[2048];

    int size = sizeof msg;

    va_start(ap, fmt);
    vsnprintf(msg, size, fmt, ap);
    va_end(ap);

    fprintf(stderr, "LibSpectrum ");
    switch (error) {
    case LIBSPECTRUM_ERROR_WARNING:
        fprintf(stderr, "warning");
        break;
    case LIBSPECTRUM_ERROR_MEMORY:
        fprintf(stderr, "memory error");
        break;
    case LIBSPECTRUM_ERROR_UNKNOWN:
        fprintf(stderr, "unknown error");
        break;
    case LIBSPECTRUM_ERROR_CORRUPT:
        fprintf(stderr, "corruption error");
        break;
    case LIBSPECTRUM_ERROR_SIGNATURE:
        fprintf(stderr, "signature error");
        break;
    case LIBSPECTRUM_ERROR_SLT:
        fprintf(stderr, "SLT data in Z80 error");
        break;
    case LIBSPECTRUM_ERROR_INVALID:
        fprintf(stderr, "invalid parameter error");
        break;
    case LIBSPECTRUM_ERROR_LOGIC:
        fprintf(stderr, "logic error");
        break;
    default:
        fprintf(stderr, "unhandled error");
        break;
    }

    fprintf(stderr, ": %s\n", msg);
}

int libspectrum_snap_machine(libspectrum_snap *snap) { return snap->machine; }

/* Read an LSB dword from buffer */
uint32_t libspectrum_read_dword(const uint8_t **buffer)
{
    uint32_t value;

    value = (*buffer)[0] + (*buffer)[1] * 0x100 + (*buffer)[2] * 0x10000 + (*buffer)[3] * 0x1000000;

    (*buffer) += 4;

    return value;
}

static libspectrum_error get_machine_type_extension(libspectrum_snap *snap,
    uint8_t type)
{
    switch (type) {

    case Z80_MACHINE_PLUS3:
    case Z80_MACHINE_PLUS3_XZX_ERROR:
        libspectrum_snap_set_machine(snap, LIBSPECTRUM_MACHINE_PLUS3);
        break;
    case Z80_MACHINE_PENTAGON:
        libspectrum_snap_set_machine(snap, LIBSPECTRUM_MACHINE_PENT);
        break;
    case Z80_MACHINE_SCORPION:
        libspectrum_snap_set_machine(snap, LIBSPECTRUM_MACHINE_SCORP);
        break;
    case Z80_MACHINE_PLUS2:
        libspectrum_snap_set_machine(snap, LIBSPECTRUM_MACHINE_PLUS2);
        break;
    case Z80_MACHINE_PLUS2A:
        libspectrum_snap_set_machine(snap, LIBSPECTRUM_MACHINE_PLUS2A);
        break;
    case Z80_MACHINE_TC2048:
        libspectrum_snap_set_machine(snap, LIBSPECTRUM_MACHINE_TC2048);
        break;
    case Z80_MACHINE_TC2068:
        libspectrum_snap_set_machine(snap, LIBSPECTRUM_MACHINE_TC2068);
        break;
    case Z80_MACHINE_TS2068:
        libspectrum_snap_set_machine(snap, LIBSPECTRUM_MACHINE_TS2068);
        break;
    default:
        libspectrum_print_error(
            LIBSPECTRUM_ERROR_UNKNOWN,
            "%s:get_machine_type: unknown extension machine type %d", __FILE__,
            type);
        return LIBSPECTRUM_ERROR_UNKNOWN;
    }

    return LIBSPECTRUM_ERROR_NONE;
}

libspectrum_error internal_z80_read(libspectrum_snap *snap,
    const uint8_t *buffer,
    size_t buffer_length);

uint8_t *DecompressZ80(uint8_t *raw_data, size_t *length)
{
    libspectrum_snap *snap = libspectrum_new(libspectrum_snap, 1);
    for (int i = 0; i < SNAPSHOT_RAM_PAGES; i++)
        libspectrum_snap_set_pages(snap, i, NULL);
    if (internal_z80_read(snap, raw_data, *length) != LIBSPECTRUM_ERROR_NONE) {
        return NULL;
    }

    uint8_t *uncompressed = NULL;

    if (snap->machine == LIBSPECTRUM_MACHINE_48) {
        uncompressed = malloc(0xc000);
        if (uncompressed != NULL) {
            memcpy(uncompressed, snap->pages[5], 0x4000);
            memcpy(uncompressed + 0x4000, snap->pages[2], 0x4000);
            memcpy(uncompressed + 0x8000, snap->pages[0], 0x4000);
            *length = 0xc000;
        }

    } else if (snap->machine == LIBSPECTRUM_MACHINE_128) {
        uncompressed = malloc(0x2001f);
        if (uncompressed != NULL) {
            memcpy(uncompressed, snap->pages[5], 0x4000);
            memcpy(uncompressed + 0x4000, snap->pages[2], 0x4000);
            size_t offset = 0x8000;
            if (snap->out_128_memoryport < SNAPSHOT_RAM_PAGES) {
                memcpy(uncompressed + 0x8000, snap->pages[snap->out_128_memoryport], 0x4000);
                offset += 0x4000;
            }
            for (int i = 0; i < SNAPSHOT_RAM_PAGES && offset < 0x2001f - 0x4000; i++) {
                /* Already written pages 5, 2 and whatever's paged in */
                if (i == 5 || i == 2 || i == snap->out_128_memoryport)
                    continue;
                memcpy(uncompressed + offset, snap->pages[i], 0x4000);
                offset += 0x4000;
            }
            *length = 0x2001f;
        }
    }

    for (int i = 0; i < SNAPSHOT_RAM_PAGES; i++)
        if (snap->pages[i] != NULL)
            free(snap->pages[i]);
    free(snap);

    return uncompressed;
}

libspectrum_error internal_z80_read(libspectrum_snap *snap,
    const uint8_t *buffer,
    size_t buffer_length)
{
    libspectrum_error error;
    const uint8_t *data;
    int version, compressed = 1;

    error = read_header(buffer, snap, &data, &version, &compressed);
    if (error != LIBSPECTRUM_ERROR_NONE)
        return error;

    error = read_blocks(data, buffer_length - (data - buffer), snap, version,
        compressed);
    if (error != LIBSPECTRUM_ERROR_NONE)
        return error;

    return LIBSPECTRUM_ERROR_NONE;
}

static libspectrum_error read_header(const uint8_t *buffer,
    libspectrum_snap *snap,
    const uint8_t **data, int *version,
    int *compressed)
{
    const uint8_t *header = buffer;
    libspectrum_error error;

    snap->pc = header[6] + header[7] * 0x100;

    if (snap->pc == 0) { /* PC == 0x0000 => v2 or greater */

        size_t extra_length;
        const uint8_t *extra_header;

        extra_length = header[LIBSPECTRUM_Z80_HEADER_LENGTH] + header[LIBSPECTRUM_Z80_HEADER_LENGTH + 1] * 0x100;

        switch (extra_length) {
        case LIBSPECTRUM_Z80_V2_LENGTH:
            *version = 2;
            break;
        case LIBSPECTRUM_Z80_V3_LENGTH:
        case LIBSPECTRUM_Z80_V3X_LENGTH:
            *version = 3;
            break;
        default:
            libspectrum_print_error(
                LIBSPECTRUM_ERROR_UNKNOWN,
                "libspectrum_read_z80_header: unknown header length %d",
                (int)extra_length);
            return LIBSPECTRUM_ERROR_UNKNOWN;
        }

        extra_header = buffer + LIBSPECTRUM_Z80_HEADER_LENGTH + 2;

        snap->pc = extra_header[0] + extra_header[1] * 0x100;

        error = get_machine_type(snap, extra_header[2], *version);
        if (error)
            return error;

        if (libspectrum_snap_machine(snap) == LIBSPECTRUM_MACHINE_128)
            snap->out_128_memoryport = extra_header[3];

        if (extra_header[5] & 0x80) {

            switch (libspectrum_snap_machine(snap)) {

            case LIBSPECTRUM_MACHINE_48:
                libspectrum_snap_set_machine(snap, LIBSPECTRUM_MACHINE_16);
                break;
            case LIBSPECTRUM_MACHINE_128:
                libspectrum_snap_set_machine(snap, LIBSPECTRUM_MACHINE_PLUS2);
                break;
            case LIBSPECTRUM_MACHINE_PLUS3:
                libspectrum_snap_set_machine(snap, LIBSPECTRUM_MACHINE_PLUS2A);
                break;
            default:
                break; /* Do nothing */
            }
        }

        (*data) = buffer + LIBSPECTRUM_Z80_HEADER_LENGTH + 2 + extra_length;

    } else { /* v1 .z80 file */

        libspectrum_snap_set_machine(snap, LIBSPECTRUM_MACHINE_48);
        *version = 1;

        /* Need to flag this for later */
        *compressed = (header[12] & 0x20) ? 1 : 0;

        (*data) = buffer + LIBSPECTRUM_Z80_HEADER_LENGTH;
    }

    return LIBSPECTRUM_ERROR_NONE;
}

static libspectrum_error get_machine_type(libspectrum_snap *snap, uint8_t type, int version)
{
    libspectrum_error error;

    if (type < Z80_MACHINE_FIRST_EXTENSION) {

        switch (version) {

        case 2:
            error = get_machine_type_v2(snap, type);
            if (error)
                return error;
            break;

        case 3:
            error = get_machine_type_v3(snap, type);
            if (error)
                return error;
            break;

        default:
            libspectrum_print_error(LIBSPECTRUM_ERROR_LOGIC,
                "%s:get_machine_type: unknown version %d",
                __FILE__, version);
            return LIBSPECTRUM_ERROR_LOGIC;
        }

    } else {
        error = get_machine_type_extension(snap, type);
        if (error)
            return error;
    }

    return LIBSPECTRUM_ERROR_NONE;
}

static libspectrum_error get_machine_type_v2(libspectrum_snap *snap,
    uint8_t type)
{
    switch (type) {

    case Z80_MACHINE_48_V2:
    case Z80_MACHINE_48_SAMRAM_V2:
        libspectrum_snap_set_machine(snap, LIBSPECTRUM_MACHINE_48);
        break;
    case Z80_MACHINE_48_IF1_V2:
        libspectrum_snap_set_machine(snap, LIBSPECTRUM_MACHINE_48);
        break;
    case Z80_MACHINE_128_V2:
        libspectrum_snap_set_machine(snap, LIBSPECTRUM_MACHINE_128);
        break;
    case Z80_MACHINE_128_IF1_V2:
        libspectrum_snap_set_machine(snap, LIBSPECTRUM_MACHINE_128);
        break;
    default:
        libspectrum_print_error(LIBSPECTRUM_ERROR_UNKNOWN,
            "%s:get_machine_type: unknown v2 machine type %d",
            __FILE__, type);
        return LIBSPECTRUM_ERROR_UNKNOWN;
    }

    return LIBSPECTRUM_ERROR_NONE;
}

static libspectrum_error get_machine_type_v3(libspectrum_snap *snap,
    uint8_t type)
{

    switch (type) {

    case Z80_MACHINE_48:
    case Z80_MACHINE_48_SAMRAM:
        libspectrum_snap_set_machine(snap, LIBSPECTRUM_MACHINE_48);
        break;
    case Z80_MACHINE_48_IF1:
        libspectrum_snap_set_machine(snap, LIBSPECTRUM_MACHINE_48);
        break;
    case Z80_MACHINE_48_MGT:
        libspectrum_snap_set_machine(snap, LIBSPECTRUM_MACHINE_48);
        break;
    case Z80_MACHINE_128:
        libspectrum_snap_set_machine(snap, LIBSPECTRUM_MACHINE_128);
        break;
    case Z80_MACHINE_128_MGT:
        libspectrum_snap_set_machine(snap, LIBSPECTRUM_MACHINE_128);
        break;
    default:
        libspectrum_print_error(LIBSPECTRUM_ERROR_UNKNOWN,
            "%s:get_machine_type: unknown v3 machine type %d",
            __FILE__, type);
        return LIBSPECTRUM_ERROR_UNKNOWN;
    }

    return LIBSPECTRUM_ERROR_NONE;
}

/* Given a 48K memory dump `data', place it into the
 appropriate bits of `snap' for a 48K machine */
libspectrum_error libspectrum_split_to_48k_pages(libspectrum_snap *snap,
    const uint8_t *data)
{
    uint8_t *buffer[3];
    size_t i;

    /* If any of the three pages are already occupied, barf */
    if (libspectrum_snap_pages(snap, 5) || libspectrum_snap_pages(snap, 2) || libspectrum_snap_pages(snap, 0)) {
        libspectrum_print_error(
            LIBSPECTRUM_ERROR_LOGIC,
            "libspectrum_split_to_48k_pages: RAM page already in use");
        return LIBSPECTRUM_ERROR_LOGIC;
    }

    for (i = 0; i < 3; i++)
        buffer[i] = libspectrum_new(uint8_t, 0x4000);

    libspectrum_snap_set_pages(snap, 5, buffer[0]);
    libspectrum_snap_set_pages(snap, 2, buffer[1]);
    libspectrum_snap_set_pages(snap, 0, buffer[2]);

    /* Finally, do the copies... */
    memcpy(libspectrum_snap_pages(snap, 5), &data[0x0000], 0x4000);
    memcpy(libspectrum_snap_pages(snap, 2), &data[0x4000], 0x4000);
    memcpy(libspectrum_snap_pages(snap, 0), &data[0x8000], 0x4000);

    return LIBSPECTRUM_ERROR_NONE;
}

static libspectrum_error read_blocks(const uint8_t *buffer,
    size_t buffer_length,
    libspectrum_snap *snap, int version,
    int compressed)
{
    const uint8_t *end, *next_block;

    end = buffer + buffer_length;
    next_block = buffer;

    while (next_block < end) {

        libspectrum_error error;

        error = read_block(next_block, snap, &next_block, end, version, compressed);
        if (error != LIBSPECTRUM_ERROR_NONE)
            return error;
    }

    return LIBSPECTRUM_ERROR_NONE;
}

static libspectrum_error read_block(const uint8_t *buffer,
    libspectrum_snap *snap,
    const uint8_t **next_block,
    const uint8_t *end, int version,
    int compressed)
{
    libspectrum_error error;
    uint8_t *uncompressed;

    int capabilities = libspectrum_machine_capabilities(libspectrum_snap_machine(snap));

    if (version == 1) {

        error = read_v1_block(buffer, compressed, &uncompressed, next_block, end);
        if (error != LIBSPECTRUM_ERROR_NONE)
            return error;

        libspectrum_split_to_48k_pages(snap, uncompressed);

        free(uncompressed);

    } else {

        size_t length;
        int page;

        error = read_v2_block(buffer, &uncompressed, &length, &page, next_block, end);
        if (error != LIBSPECTRUM_ERROR_NONE)
            return error;

        if (page <= 0 || page > 18) {
            libspectrum_print_error(LIBSPECTRUM_ERROR_UNKNOWN,
                "read_block: unknown page %d", page);
            free(uncompressed);
            return LIBSPECTRUM_ERROR_UNKNOWN;
        }

        /* If it's a ROM page, just throw it away */
        if (page < 3) {
            free(uncompressed);
            return LIBSPECTRUM_ERROR_NONE;
        }

        /* Page 11 is the Multiface ROM unless we're emulating something
       Scorpion-like */
        if (page == 11 && !(capabilities & LIBSPECTRUM_MACHINE_CAPABILITY_SCORP_MEMORY)) {
            free(uncompressed);
            return LIBSPECTRUM_ERROR_NONE;
        }

        /* Deal with 48K snaps -- first, throw away page 3, as it's a ROM.
       Then remap the numbers slightly */
        if (!(capabilities & LIBSPECTRUM_MACHINE_CAPABILITY_128_MEMORY)) {

            switch (page) {

            case 3:
                free(uncompressed);
                return LIBSPECTRUM_ERROR_NONE;
            case 4:
                page = 5;
                break;
            case 5:
                page = 3;
                break;
            }
        }

        /* Now map onto RAM page numbers */
        page -= 3;

        if (libspectrum_snap_pages(snap, page) == NULL) {
            libspectrum_snap_set_pages(snap, page, uncompressed);
        } else {
            free(uncompressed);
            libspectrum_print_error(LIBSPECTRUM_ERROR_CORRUPT,
                "read_block: page %d duplicated", page);
            return LIBSPECTRUM_ERROR_CORRUPT;
        }
    }

    return LIBSPECTRUM_ERROR_NONE;
}

static libspectrum_error read_v1_block(const uint8_t *buffer, int is_compressed,
    uint8_t **uncompressed,
    const uint8_t **next_block,
    const uint8_t *end)
{
    if (is_compressed) {

        const uint8_t *ptr;
        int state;
        size_t uncompressed_length = 0;

        state = 0;
        ptr = buffer;

        while (state != 4) {

            if (ptr == end) {
                libspectrum_print_error(LIBSPECTRUM_ERROR_CORRUPT,
                    "read_v1_block: end marker not found");
                return LIBSPECTRUM_ERROR_CORRUPT;
            }

            switch (state) {
            case 0:
                switch (*ptr++) {
                case 0x00:
                    state = 1;
                    break;
                default:
                    state = 0;
                    break;
                }
                break;
            case 1:
                switch (*ptr++) {
                case 0x00:
                    state = 1;
                    break;
                case 0xed:
                    state = 2;
                    break;
                default:
                    state = 0;
                    break;
                }
                break;
            case 2:
                switch (*ptr++) {
                case 0x00:
                    state = 1;
                    break;
                case 0xed:
                    state = 3;
                    break;
                default:
                    state = 0;
                    break;
                }
                break;
            case 3:
                switch (*ptr++) {
                case 0x00:
                    state = 4;
                    break;
                default:
                    state = 0;
                    break;
                }
                break;
            default:
                libspectrum_print_error(LIBSPECTRUM_ERROR_LOGIC,
                    "read_v1_block: unknown state %d", state);
                return LIBSPECTRUM_ERROR_LOGIC;
            }
        }

        /* Length passed here is reduced by 4 to remove the end marker */
        uncompress_block(uncompressed, &uncompressed_length, buffer,
            (ptr - buffer - 4));

        /* Uncompressed data must be exactly 48Kb long */
        if (uncompressed_length != 0xc000) {
            free(*uncompressed);
            libspectrum_print_error(
                LIBSPECTRUM_ERROR_CORRUPT,
                "read_v1_block: data does not uncompress to 48Kb");
            return LIBSPECTRUM_ERROR_CORRUPT;
        }

        *next_block = ptr;

    } else { /* Snap isn't compressed */

        /* Check we've got enough bytes to read */
        if (end - *next_block < 0xc000) {
            libspectrum_print_error(LIBSPECTRUM_ERROR_CORRUPT,
                "read_v1_block: not enough data in buffer");
            return LIBSPECTRUM_ERROR_CORRUPT;
        }

        *uncompressed = libspectrum_new(uint8_t, 0xc000);
        memcpy(*uncompressed, buffer, 0xc000);
        *next_block = buffer + 0xc000;
    }

    return LIBSPECTRUM_ERROR_NONE;
}

/* The signature used to designate the .slt extensions */
static uint8_t slt_signature[] = "\0\0\0SLT";
static size_t slt_signature_length = 6;

static libspectrum_error read_v2_block(const uint8_t *buffer, uint8_t **block,
    size_t *length, int *page,
    const uint8_t **next_block,
    const uint8_t *end)
{
    size_t length2;

    length2 = buffer[0] + buffer[1] * 0x100;
    (*page) = buffer[2];

    if (length2 == 0 && *page == 0) {
        if (buffer + 8 < end && !memcmp(buffer, slt_signature, slt_signature_length)) {
            /* Ah, we have what looks like SLT data... */
            *next_block = buffer + 6;
            return LIBSPECTRUM_ERROR_SLT;
        }
    }

    /* A length of 0xffff => 16384 bytes of uncompressed data */
    if (length2 != 0xffff) {

        /* Check we're not going to run over the end of the buffer */
        if (buffer + 3 + length2 > end) {
            libspectrum_print_error(LIBSPECTRUM_ERROR_CORRUPT,
                "read_v2_block: not enough data in buffer");
            return LIBSPECTRUM_ERROR_CORRUPT;
        }

        *length = 0;
        uncompress_block(block, length, buffer + 3, length2);

        *next_block = buffer + 3 + length2;

    } else { /* Uncompressed block */

        /* Check we're not going to run over the end of the buffer */
        if (buffer + 3 + 0x4000 > end) {
            libspectrum_print_error(LIBSPECTRUM_ERROR_CORRUPT,
                "read_v2_block: not enough data in buffer");
            return LIBSPECTRUM_ERROR_CORRUPT;
        }

        *block = libspectrum_new(uint8_t, 0x4000);
        memcpy(*block, buffer + 3, 0x4000);

        *length = 0x4000;
        *next_block = buffer + 3 + 0x4000;
    }

    return LIBSPECTRUM_ERROR_NONE;
}

static void uncompress_block(uint8_t **dest, size_t *dest_length,
    const uint8_t *src, size_t src_length)
{
    const uint8_t *in_ptr;
    uint8_t *out_ptr;

    /* Allocate memory for dest if requested */
    if (*dest_length == 0) {
        *dest_length = src_length / 2;
        *dest = libspectrum_new(uint8_t, *dest_length);
    }

    in_ptr = src;
    out_ptr = *dest;

    while (in_ptr < src + src_length) {

        /* If we're pointing at the last byte, just copy it across
       and exit */
        if (in_ptr == src + src_length - 1) {
            libspectrum_make_room(dest, 1, &out_ptr, dest_length);
            *out_ptr++ = *in_ptr++;
            continue;
        }

        /* If we're pointing at two successive 0xed bytes, that's
       a run. If not, just copy the byte across */
        if (*in_ptr == 0xed && *(in_ptr + 1) == 0xed) {

            size_t run_length;
            uint8_t repeated;

            in_ptr += 2;
            run_length = *in_ptr++;
            repeated = *in_ptr++;

            libspectrum_make_room(dest, run_length, &out_ptr, dest_length);

            while (run_length--)
                *out_ptr++ = repeated;

        } else {

            libspectrum_make_room(dest, 1, &out_ptr, dest_length);
            *out_ptr++ = *in_ptr++;
        }
    }

    *dest_length = out_ptr - *dest;
}

/* Various types of file we might manage to identify */
typedef enum libspectrum_id_t {

    /* These types present in all versions of libspectrum */

    LIBSPECTRUM_ID_UNKNOWN = 0, /* Unidentified file */
    LIBSPECTRUM_ID_RECORDING_RZX, /* RZX input recording */
    LIBSPECTRUM_ID_SNAPSHOT_SNA, /* .sna snapshot */
    LIBSPECTRUM_ID_SNAPSHOT_Z80, /* .z80 snapshot */
    LIBSPECTRUM_ID_TAPE_TAP, /* Z80-style .tap tape image */
    LIBSPECTRUM_ID_TAPE_TZX, /* TZX tape image */

    /* Below here, present only in 0.1.1 and later */

    /* The next entry is deprecated in favour of the more specific
     LIBSPECTRUM_ID_DISK_CPC and LIBSPECTRUM_ID_DISK_ECPC */
    LIBSPECTRUM_ID_DISK_DSK, /* .dsk +3 disk image */

    LIBSPECTRUM_ID_DISK_SCL, /* .scl TR-DOS disk image */
    LIBSPECTRUM_ID_DISK_TRD, /* .trd TR-DOS disk image */
    LIBSPECTRUM_ID_CARTRIDGE_DCK, /* .dck Timex cartridge image */

    /* Below here, present only in 0.2.0 and later */

    LIBSPECTRUM_ID_TAPE_WARAJEVO, /* Warajevo-style .tap tape image */

    LIBSPECTRUM_ID_SNAPSHOT_PLUSD, /* DISCiPLE/+D snapshot */
    LIBSPECTRUM_ID_SNAPSHOT_SP, /* .sp snapshot */
    LIBSPECTRUM_ID_SNAPSHOT_SNP, /* .snp snapshot */
    LIBSPECTRUM_ID_SNAPSHOT_ZXS, /* .zxs snapshot (zx32) */
    LIBSPECTRUM_ID_SNAPSHOT_SZX, /* .szx snapshot (Spectaculator) */

    /* Below here, present only in 0.2.1 and later */

    LIBSPECTRUM_ID_COMPRESSED_BZ2, /* bzip2 compressed file */
    LIBSPECTRUM_ID_COMPRESSED_GZ, /* gzip compressed file */

    /* Below here, present only in 0.2.2 and later */

    LIBSPECTRUM_ID_HARDDISK_HDF, /* .hdf hard disk image */
    LIBSPECTRUM_ID_CARTRIDGE_IF2, /* .rom Interface 2 cartridge image */

    /* Below here, present only in 0.3.0 and later */

    LIBSPECTRUM_ID_MICRODRIVE_MDR, /* .mdr microdrive cartridge */
    LIBSPECTRUM_ID_TAPE_CSW, /* .csw tape image */
    LIBSPECTRUM_ID_TAPE_Z80EM, /* Z80Em tape image */

    /* Below here, present only in 0.4.0 and later */

    LIBSPECTRUM_ID_TAPE_WAV, /* .wav tape image */
    LIBSPECTRUM_ID_TAPE_SPC, /* SP-style .spc tape image */
    LIBSPECTRUM_ID_TAPE_STA, /* Speculator-style .sta tape image */
    LIBSPECTRUM_ID_TAPE_LTP, /* Nuclear ZX-style .ltp tape image */
    LIBSPECTRUM_ID_COMPRESSED_XFD, /* xfdmaster (Amiga) compressed file */
    LIBSPECTRUM_ID_DISK_IMG, /* .img DISCiPLE/+D disk image */
    LIBSPECTRUM_ID_DISK_MGT, /* .mgt DISCiPLE/+D disk image */

    /* Below here, present only in 0.5.0 and later */

    LIBSPECTRUM_ID_DISK_UDI, /* .udi generic disk image */
    LIBSPECTRUM_ID_DISK_FDI, /* .fdi generic disk image */
    LIBSPECTRUM_ID_DISK_CPC, /* .dsk plain CPC +3 disk image */
    LIBSPECTRUM_ID_DISK_ECPC, /* .dsk extended CPC +3 disk image */
    LIBSPECTRUM_ID_DISK_SAD, /* .sad generic disk image */
    LIBSPECTRUM_ID_DISK_TD0, /* .td0 generic disk image */

    LIBSPECTRUM_ID_DISK_OPD, /* .opu/.opd Opus Discovery disk image */

    LIBSPECTRUM_ID_TAPE_PZX, /* PZX tape image */

    LIBSPECTRUM_ID_AUX_POK, /* POKE file */

} libspectrum_id_t;

/* And 'classes' of file */
typedef enum libspectrum_class_t {

    LIBSPECTRUM_CLASS_UNKNOWN,

    LIBSPECTRUM_CLASS_CARTRIDGE_TIMEX, /* Timex cartridges */
    LIBSPECTRUM_CLASS_DISK_PLUS3, /* +3 disk */
    LIBSPECTRUM_CLASS_DISK_TRDOS, /* TR-DOS disk */
    LIBSPECTRUM_CLASS_DISK_OPUS, /* Opus Discovery disk*/
    LIBSPECTRUM_CLASS_RECORDING, /* Input recording */
    LIBSPECTRUM_CLASS_SNAPSHOT, /* Snapshot */
    LIBSPECTRUM_CLASS_TAPE, /* Tape */

    /* Below here, present only in 0.2.1 and later */

    LIBSPECTRUM_CLASS_COMPRESSED, /* A compressed file */

    /* Below here, present only in 0.2.2 and later */

    LIBSPECTRUM_CLASS_HARDDISK, /* A hard disk image */
    LIBSPECTRUM_CLASS_CARTRIDGE_IF2, /* Interface 2 cartridges */

    /* Below here, present only in 0.3.0 and later */

    LIBSPECTRUM_CLASS_MICRODRIVE, /* Microdrive cartridges */

    /* Below here, present only in 0.4.0 and later */

    LIBSPECTRUM_CLASS_DISK_PLUSD, /* DISCiPLE/+D disk image */

    /* Below here, present only in 0.5.0 and later */

    LIBSPECTRUM_CLASS_DISK_GENERIC, /* generic disk image */

    LIBSPECTRUM_CLASS_AUXILIARY, /* auxiliary supported file */

} libspectrum_class_t;
/* The various types of block available */
typedef enum libspectrum_tape_type {

    /* Values must be the same as used in the .tzx format */

    LIBSPECTRUM_TAPE_BLOCK_ROM = 0x10,
    LIBSPECTRUM_TAPE_BLOCK_TURBO,
    LIBSPECTRUM_TAPE_BLOCK_PURE_TONE,
    LIBSPECTRUM_TAPE_BLOCK_PULSES,
    LIBSPECTRUM_TAPE_BLOCK_PURE_DATA,
    LIBSPECTRUM_TAPE_BLOCK_RAW_DATA,

    LIBSPECTRUM_TAPE_BLOCK_GENERALISED_DATA = 0x19,

    LIBSPECTRUM_TAPE_BLOCK_PAUSE = 0x20,
    LIBSPECTRUM_TAPE_BLOCK_GROUP_START,
    LIBSPECTRUM_TAPE_BLOCK_GROUP_END,
    LIBSPECTRUM_TAPE_BLOCK_JUMP,
    LIBSPECTRUM_TAPE_BLOCK_LOOP_START,
    LIBSPECTRUM_TAPE_BLOCK_LOOP_END,

    LIBSPECTRUM_TAPE_BLOCK_SELECT = 0x28,

    LIBSPECTRUM_TAPE_BLOCK_STOP48 = 0x2a,
    LIBSPECTRUM_TAPE_BLOCK_SET_SIGNAL_LEVEL,

    LIBSPECTRUM_TAPE_BLOCK_COMMENT = 0x30,
    LIBSPECTRUM_TAPE_BLOCK_MESSAGE,
    LIBSPECTRUM_TAPE_BLOCK_ARCHIVE_INFO,
    LIBSPECTRUM_TAPE_BLOCK_HARDWARE,

    LIBSPECTRUM_TAPE_BLOCK_CUSTOM = 0x35,

    LIBSPECTRUM_TAPE_BLOCK_CONCAT = 0x5a,

    /* Past here are block types not in the .tzx format */

    LIBSPECTRUM_TAPE_BLOCK_RLE_PULSE = 0x100,

    /* PZX format blocks */
    LIBSPECTRUM_TAPE_BLOCK_PULSE_SEQUENCE,
    LIBSPECTRUM_TAPE_BLOCK_DATA_BLOCK,

} libspectrum_tape_type;

struct libspectrum_tape_block {
    libspectrum_tape_type type;
};

typedef struct libspectrum_tape_block libspectrum_tape_block;

/* The tape type itself */
struct libspectrum_tape {

    /* All the blocks */
    struct libspectrum_tape_block *blocks;

    /* The last block */
    struct libspectrum_tape_block *last_block;
};

typedef struct libspectrum_tape libspectrum_tape;

static libspectrum_error
tzx_read_data(uint8_t **ptr, const uint8_t *end,
    size_t *length, int bytes)
{
    int i;
    uint32_t multiplier = 0x01;

    if (bytes < 0) {
        bytes = -bytes;
    }

    (*length) = 0;
    for (i = 0; i < bytes; i++, multiplier *= 0x100) {
        *length += **ptr * multiplier;
        (*ptr)++;
    }

    /* Have we got enough bytes left in buffer? */
    if ((end - (*ptr)) < (ptrdiff_t)(*length)) {
        libspectrum_print_error(LIBSPECTRUM_ERROR_CORRUPT,
            "tzx_read_data: not enough data in buffer");
        return LIBSPECTRUM_ERROR_CORRUPT;
    }
    return LIBSPECTRUM_ERROR_NONE;
}

static libspectrum_error
tzx_read_rom_block(uint8_t **ptr,
    const uint8_t *end, size_t *length)
{
    /* Check there's enough left in the buffer for the pause and the
     data length */
    if (end - (*ptr) < 4) {
        libspectrum_print_error(LIBSPECTRUM_ERROR_CORRUPT,
            "tzx_read_rom_block: not enough data in buffer");
        return LIBSPECTRUM_ERROR_CORRUPT;
    }

    (*ptr) += 2;

    /* And the data */
    return tzx_read_data(ptr, end, length, 2);
}

static libspectrum_error
tzx_read_turbo_block(uint8_t **ptr,
    const uint8_t *end, size_t *length)
{
    /* Check there's enough left in the buffer for all the metadata */
    if (end - (*ptr) < 18) {
        libspectrum_print_error(
            LIBSPECTRUM_ERROR_CORRUPT,
            "tzx_read_turbo_block: not enough data in buffer");
        return LIBSPECTRUM_ERROR_CORRUPT;
    }

    (*ptr) += 15;

    /* Read the data in */
    return tzx_read_data(ptr, end, length, 3);
}

static libspectrum_error
tzx_skip_data(uint8_t **ptr, const uint8_t *end,
    int bytes)
{
    int i;
    uint32_t multiplier = 0x01;
    size_t length = 0;

    if (bytes < 0) {
        bytes = -bytes;
    }

    for (i = 0; i < bytes; i++, multiplier *= 0x100) {
        length += **ptr * multiplier;
        (*ptr)++;
    }

    /* Have we got enough bytes left in buffer? */
    if ((end - (*ptr)) < (ptrdiff_t)length) {
        libspectrum_print_error(LIBSPECTRUM_ERROR_CORRUPT,
            "tzx_read_data: not enough data in buffer");
        return LIBSPECTRUM_ERROR_CORRUPT;
    }

    *ptr += length;

    return LIBSPECTRUM_ERROR_NONE;
}

static libspectrum_error tzx_skip_rom_block(uint8_t **ptr, uint8_t *end)
{
    libspectrum_error error = LIBSPECTRUM_ERROR_NONE;

    /* Check there's enough left in the buffer for the pause and the
     data length */
    if (end - (*ptr) < 4) {
        libspectrum_print_error(LIBSPECTRUM_ERROR_CORRUPT,
            "tzx_read_rom_block: not enough data in buffer");
        return LIBSPECTRUM_ERROR_CORRUPT;
    }

    (*ptr) += 2;

    /* And the data */
    error = tzx_skip_data(ptr, end, 2);

    return error;
}

static libspectrum_error tzx_skip_turbo_block(uint8_t **ptr, uint8_t *end)
{
    libspectrum_error error = LIBSPECTRUM_ERROR_NONE;

    /* Check there's enough left in the buffer for all the metadata */
    if (end - (*ptr) < 18) {
        libspectrum_print_error(
            LIBSPECTRUM_ERROR_CORRUPT,
            "tzx_read_turbo_block: not enough data in buffer");
        return LIBSPECTRUM_ERROR_CORRUPT;
    }

    (*ptr) += 15;

    /* Read the data in */
    error = tzx_skip_data(ptr, end, 3);
    return error;
}

static libspectrum_error tzx_skip_comment(uint8_t **ptr, uint8_t *end)
{
    /* Check the length byte exists */
    if ((*ptr) == end) {
        libspectrum_print_error(LIBSPECTRUM_ERROR_CORRUPT,
            "tzx_read_comment: not enough data in buffer");
        return LIBSPECTRUM_ERROR_CORRUPT;
    }

    return tzx_skip_data(ptr, end, -1);
    ;
}

static libspectrum_error tzx_skip_archive_info(uint8_t **ptr, uint8_t *end)
{
    libspectrum_error error = LIBSPECTRUM_ERROR_NONE;

    /* Check there's enough left in the buffer for the length and the count
     byte */
    if (end - (*ptr) < 3) {
        libspectrum_print_error(
            LIBSPECTRUM_ERROR_CORRUPT,
            "tzx_read_archive_info: not enough data in buffer");
        return LIBSPECTRUM_ERROR_CORRUPT;
    }

    size_t length = **ptr + *(*ptr + 1) * 0x100 + 2;
    (*ptr) += length;
    return error;
}

static libspectrum_error tzx_skip_to_block(int blockno, uint8_t **ptr, uint8_t *end)
{
    libspectrum_error error = LIBSPECTRUM_ERROR_NONE;

    int currentblock = 0;

    while (*ptr < end && currentblock < blockno) {

        /* Get the ID of the next block */
        libspectrum_tape_type id = **ptr;
        *ptr = *ptr + 1;

        switch (id) {
        case LIBSPECTRUM_TAPE_BLOCK_ROM:
            error = tzx_skip_rom_block(ptr, end);
            if (error)
                return error;
            break;
        case LIBSPECTRUM_TAPE_BLOCK_TURBO:
            error = tzx_skip_turbo_block(ptr, end);
            if (error)
                return error;
            break;

        case LIBSPECTRUM_TAPE_BLOCK_COMMENT:
            error = tzx_skip_comment(ptr, end);
            if (error)
                return error;
            break;
        case LIBSPECTRUM_TAPE_BLOCK_ARCHIVE_INFO:
            error = tzx_skip_archive_info(ptr, end);
            if (error)
                return error;
            break;
        default: /* For now, don't handle anything else */
            libspectrum_print_error(
                LIBSPECTRUM_ERROR_UNKNOWN,
                "tzx_skip_to_block: unknown block type 0x%02x", id);
            return LIBSPECTRUM_ERROR_UNKNOWN;
        }
        currentblock++;
    }
    return error;
}

/* The .tzx file signature (first 8 bytes) */
const char *const libspectrum_tzx_signature = "ZXTape!\x1a";

static libspectrum_error
find_tzx_block(int blockno, uint8_t *srcbuf, uint8_t **result,
    size_t *length)
{

    libspectrum_error error;

    uint8_t *ptr, *end;
    size_t signature_length = strlen(libspectrum_tzx_signature);

    ptr = srcbuf;
    end = srcbuf + *length;

    /* Must be at least as many bytes as the signature, and the major/minor
     version numbers */
    if (*length < signature_length + 2) {
        libspectrum_print_error(
            LIBSPECTRUM_ERROR_CORRUPT,
            "libspectrum_tzx_create: not enough data in buffer");
        return LIBSPECTRUM_ERROR_CORRUPT;
    }

    /* Now check the signature */
    if (memcmp(ptr, libspectrum_tzx_signature, signature_length)) {
        libspectrum_print_error(LIBSPECTRUM_ERROR_SIGNATURE,
            "libspectrum_tzx_create: wrong signature");
        return LIBSPECTRUM_ERROR_SIGNATURE;
    }
    ptr += signature_length;

    /* Just skip the version numbers */
    ptr += 2;

    error = tzx_skip_to_block(blockno, &ptr, end);
    if (error)
        return error;

    /* Get the ID of the next block */
    libspectrum_tape_type id = *ptr++;

    switch (id) {
    case LIBSPECTRUM_TAPE_BLOCK_ROM:
        error = tzx_read_rom_block(&ptr, end, length);
        if (error)
            return error;
        break;
    case LIBSPECTRUM_TAPE_BLOCK_TURBO:
        error = tzx_read_turbo_block(&ptr, end, length);
        if (error)
            return error;
        break;
    default: /* For now, don't handle anything else */
        libspectrum_print_error(
            LIBSPECTRUM_ERROR_UNKNOWN,
            "libspectrum_tzx_create: unknown block type 0x%02x", id);
        return LIBSPECTRUM_ERROR_UNKNOWN;
    }
    *result = ptr;
    return LIBSPECTRUM_ERROR_NONE;
}

uint8_t *find_tap_block(int wantedindex, const uint8_t *buffer,
    size_t *length)
{
    size_t data_length, buf_length;
    uint8_t *data;

    int blockindex = 0;

    const uint8_t *ptr, *end;

    ptr = buffer;
    end = buffer + *length;

    while (ptr < end) {

        /* If we've got less than two bytes for the length, something's
         gone wrong, so go home */
        if ((end - ptr) < 2) {
            libspectrum_print_error(
                LIBSPECTRUM_ERROR_CORRUPT,
                "libspectrum_tap_read: not enough data in buffer");
            return NULL;
        }

        /* Get the length, and move along the buffer */
        data_length = ptr[0] + ptr[1] * 0x100;
        ptr += 2;

        buf_length = data_length;

        /* Have we got enough bytes left in buffer? */
        if (end - ptr < (ptrdiff_t)buf_length) {
            libspectrum_print_error(
                LIBSPECTRUM_ERROR_CORRUPT,
                "libspectrum_tap_read: not enough data in buffer");
            return NULL;
        }

        if (blockindex == wantedindex) {
            /* Allocate memory for the data */
            data = libspectrum_new(uint8_t, data_length);

            /* Copy the block data across */
            memcpy(data, ptr + 1, buf_length - 1);

            *length = data_length - 2;
            return data;
        }

        blockindex++;

        /* Move along the buffer */
        ptr += buf_length;
    }

    libspectrum_print_error(
        LIBSPECTRUM_ERROR_CORRUPT,
        "find_tap_block: could not find block %d", wantedindex);
    return NULL;
}

uint8_t *GetTZXBlock(int blockno, uint8_t *srcbuf, size_t *length)
{
    uint8_t *ptr;
    libspectrum_error error = find_tzx_block(blockno, srcbuf, &ptr,
        length);
    if (error) {
        libspectrum_print_error(error, "get_tzx_block: could not read block %d\n", blockno);
        return NULL;
    }
    uint8_t *result = libspectrum_malloc(*length);
    if (!result) {
        return NULL;
    }
    memcpy(result, ptr, *length);
    return result;
}
