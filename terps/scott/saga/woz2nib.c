//
//  woz2nib.c
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-09-09.
//  Based on woz2dsk.pl by LEE Sau Dan <leesaudan@gmail.com>
//
//  This is a quick translation of Perl code that I don't fully understand,
//  so it likely contains bugs and redundant code, but seems to be working.
//

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debugprint.h"

#include "woz2nib.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define STANDARD_TRACKS_PER_DISK 35
#define SECTORS_PER_TRACK 16
#define BYTES_PER_SECTOR 256
#define NIBBLES_PER_TRACK 6656

typedef enum {
    DOS,
    ProDOS,
    NIBBLE
} SectorOrderType;

typedef enum {
    INFO,
    TMAP,
    TRKS,
    WRIT,
    META,
    NUM_CHUNK_TYPES
} ChunkType;

static const char *chunktypes[6] = { "INFO", "TMAP", "TRKS", "WRIT", "META", NULL };

typedef struct {
    uint16_t starting_block;
    uint16_t block_count;
    uint32_t bit_count;
    uint16_t bytes_used;
    uint8_t *bitstream;
} trksdata;

static const uint32_t crc32_tab[] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
    0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
    0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
    0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
    0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
    0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
    0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
    0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
    0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
    0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
    0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
    0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
    0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
    0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
    0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
    0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
    0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
    0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
    0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
    0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
    0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
    0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
    0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
    0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
    0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
    0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
    0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
    0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
    0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
    0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
    0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
    0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
    0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

static uint32_t crc32(uint32_t crc, const void *buf, size_t size)
{
    const uint8_t *p;
    p = buf;
    crc = ~crc;
    while (size--)
        crc = crc32_tab[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

#define WOZ_BLOCK_SIZE 512

static void FatalError(char *format, ...)
{
    va_list argp;

    fprintf(stderr, "\nFatal: ");

    va_start(argp, format);
    vprintf(format, argp);
    va_end(argp);

    fprintf(stderr, "\n");

    exit(1);
}

static void *MemAlloc(size_t size)
{
    void *t = (void *)malloc(size);
    if (t == NULL)
        FatalError("Out of memory");
    return (t);
}

static int rotate_left_with_carry(uint8_t *byte, int last_carry)
{
    int carry = ((*byte & 0x80) > 0);
    *byte = *byte << 1;
    if (last_carry)
        *byte |= 0x1;
    return carry;
}

// This is a translation of $trk =~ s{^0*(1.{7})}{}o;
// Extract first '1' + 7 characters ("bits").
static uint8_t extract_nibble(uint8_t *bitstream, int bits, int *pos)
{
    int bytes = bits / 8 + (bits % 8 != 0);
    // skip any initial '0'.
    while (*pos < bytes && bitstream[*pos] == 0) {
        (*pos)++;
    }

    if (*pos >= bits / 8) {
        // bitstream contained only zeros until no full byte left.
        return 0;
    }

    int carry = 0;
    int shiftcount = 0;
    while ((bitstream[*pos] & 0x80) == 0) {
        carry = rotate_left_with_carry(&bitstream[*pos + 1], carry);
        carry = rotate_left_with_carry(&bitstream[*pos], carry);
        shiftcount++;
    }
    uint8_t nibble = bitstream[*pos];
    bitstream[*pos] = 0;
    bitstream[*pos + 1] = bitstream[*pos + 1] >> shiftcount;
    return nibble;
}

static uint8_t *swap_head_and_tail(uint8_t *bitstream, int headbits, int bits)
{

    int bitspill = bits % 8;
    int anybitspill = (bitspill != 0);
    int totalbytes = bits / 8 + anybitspill;
    int headspill = headbits % 8;
    int anyheadspill = (headspill != 0);
    int tailbits = bits - headbits;
    int tailspill = tailbits % 8;
    int headbytes = headbits / 8 + anyheadspill;

    uint8_t lastbyte = bitstream[totalbytes - 1];

    if (anybitspill)
        lastbyte = (bitstream[totalbytes - 2] << bitspill) | (lastbyte >> (8 - bitspill));

    int tailbytes = tailbits / 8 + 2;

    // if there are spillover head bits, the tail copy must
    // include the last head byte,
    // then be left shifted (headspill) times

    uint8_t *tail = MemAlloc(tailbytes);

    if (!anyheadspill) {
        // If the number of head bits are cleanly divisible by 8 we don't need to do any shifting
        memcpy(tail, bitstream + headbytes, tailbytes - 1);
    } else {
        for (int i = 0; i < tailbytes && headbytes + i < totalbytes; i++) {
            tail[i] = (bitstream[headbytes - 1 + i] << headspill) | (bitstream[headbytes + i] >> (8 - headspill));
        }
    }

    // if there are spillover tail bits, the head copy must
    // then be right shifted (tailspill) times,
    // while shifting in the original last tail bits
    // we calculated as lastbyte above

    uint8_t *head = MemAlloc(headbytes + 1);

    head[0] = (bitstream[0] >> tailspill) | (lastbyte << (8 - tailspill));
    for (int i = 1; i <= headbytes; i++) {
        head[i] = (bitstream[i] >> tailspill) | (bitstream[i - 1] << (8 - tailspill));
    }

    memcpy(bitstream, tail, tailbytes);
    memcpy(bitstream + tailbits / 8, head, headbytes + 1);

    free(head);
    free(tail);

    return bitstream;
}

typedef enum {
    FOUND_NONE,
    FOUND16,
    FOUND13,
} SearchResultType;

// clang-format off

static const uint8_t syncbytes16[7] = {
    0xff, //    11111111
    0x3f, //    00111111
    0xcf, //    11001111
    0xf3, //    11110011
    0xfc, //    11111100
    0xff, //    11111111
    0x00  //    00
};

// clang-format on

static const uint8_t syncbytes13[9] = {
    0xff, //    11111111
    0x7f, //    01111111
    0xbf, //    10111111
    0xdf, //    11011111
    0xef, //    11101111
    0xf7, //    11110111
    0xfb, //    11111011
    0xfd, //    11111101
    0xfe, //    11111110
};

// These two functions look for the two sync bytes sequences
// in the bitstream array.
// This is obviously not the quickest way to do it. It should be
// possible to create a way to compare 64 bits at once on a
// standard 64-bit little endian system.
// This also seems relevant:
// https://stackoverflow.com/questions/1572290/fastest-way-to-scan-for-bit-pattern-in-a-stream-of-bits

// Look for any of the two sync byte sequences in the bitstream array
// This function assumes that the bitstream array is at least 9 bytes long.
static SearchResultType find_in_bytes(uint8_t *bitstream)
{
    int may_be_16 = 1;
    int may_be_13 = 1;
    for (int i = 1; i < 9; i++) {
        // We know that the first byte is a matching 0xff
        bitstream++;
        if (may_be_13) {
            if (syncbytes13[i] != *bitstream) {
                may_be_13 = 0;
            } else if (i == 8) {
                return FOUND13;
            }
        }
        if (may_be_16) {
            if (i == 6 && (*bitstream & 0xc0) == 0) {
                return FOUND16;
            }
            if (i > 6 || syncbytes16[i] != *bitstream) {
                may_be_16 = 0;
            }
        }
        if (may_be_16 + may_be_13 == 0)
            return FOUND_NONE;
    }
    return FOUND_NONE;
}

static SearchResultType find_syncbytes(uint8_t *bitstream, int bitcount, int *pos)
{
    int bytes = bitcount / 8;
    int repeats = bytes - 9;
    SearchResultType result = FOUND_NONE;
    // Both sync byte sequences begin with 0xff, so first we do
    // a simple loop looking for that, without shifting any bits.
    // This is enough to find what we're looking for
    // in the majority of cases.
    for (int i = 0; i < repeats; i++) {
        if (bitstream[i] == 0xff) {
            result = find_in_bytes(&bitstream[i]);
            if (result != FOUND_NONE) {
                *pos = i * 8;
                return result;
            }
        }
    }

    uint8_t temp[9];
    // Otherwise we will have to look harder, for byte pairs that can be left shifted to 0xff
    for (int i = 0; i < repeats; i++) {
        uint8_t b = bitstream[i];
        // Skip any bytes where the rightmost bit is unset
        // or the next byte has the leftmost bit unset
        if ((b & 0x01) != 0 && (bitstream[i + 1] & 0x80) != 0) {
            for (int j = 1; j < 8; j++) {
                // Check if this bit and the next left shifted j places become 0xff
                if (((bitstream[i] << j) | (bitstream[i + 1] >> (8 - j))) == 0xff) {
                    // If so, copy the following 8 bytes to a buffer left shifted j positions,
                    // and compare them to the sync byte sequences.
                    for (int k = 8; k > 0; k--) {
                        temp[k] = ((bitstream[i + k] << j) | (bitstream[i + k + 1] >> (8 - j)));
                    }
                    result = find_in_bytes(temp);
                    if (result != FOUND_NONE) {
                        *pos = i * 8 + j;
                        return result;
                    }
                }
            }
        }
    }
    return result;
}

uint8_t *woz2nib(uint8_t *ptr, size_t *len)
{
    uint8_t tempout[300000];
    int writepos = 0;
    SectorOrderType format = NIBBLE;
    int nr_tracks = STANDARD_TRACKS_PER_DISK;
    uint8_t *woz_buffer = ptr;
    size_t woz_size = *len;
    int woz_unpack_pos = 12;
    char magic[4];
    uint8_t fixed[4];
    uint64_t crc = 0;
    for (int i = 0; i < 4; i++) {
        magic[i] = woz_buffer[i];
        fixed[i] = woz_buffer[4 + i];
        crc += (uint64_t)woz_buffer[8 + i] << (8 * i);
    }

    if (magic[0] != 'W' || magic[1] != 'O' || magic[2] != 'Z' || (magic[3] != '1' && magic[3] != '2'))
        FatalError("WOZ header not found\n");

    if (fixed[0] != 0xff || fixed[1] != 0x0a || fixed[2] != 0x0d || fixed[3] != 0x0a)
        FatalError("bad header\n");

    uint32_t computed_crc32 = crc32(0, ptr + woz_unpack_pos, woz_size - woz_unpack_pos);

    if (computed_crc32 != crc) {
        debug_print("CRC mismatch (stored=%08llx; computed=%08x)\n", crc, computed_crc32);
        return NULL;
    }

    uint8_t tmap[40];
    int info_found = 0;
    int tmap_found = 0;
    int trks_found = 0;
    trksdata trks[160];
    for (int i = 0; i < 160; i++)
        trks[i].bitstream = NULL;

    while (woz_unpack_pos < woz_size) {
        char chunkstring[5];
        uint32_t chunk_size = 0;
        for (int i = 0; i < 4; i++) {
            chunkstring[i] = woz_buffer[woz_unpack_pos + i];
            chunk_size += woz_buffer[woz_unpack_pos + 4 + i] << (8 * i);
        }
        chunkstring[4] = '\0';
        uint8_t *chunk_data = MemAlloc(chunk_size);
        woz_unpack_pos += 8;
        memcpy(chunk_data, woz_buffer + woz_unpack_pos, chunk_size);
        woz_unpack_pos += chunk_size;

        ChunkType chunk_id = NUM_CHUNK_TYPES;
        for (int i = 0; i < NUM_CHUNK_TYPES; i++) {
            if (strncmp(chunkstring, chunktypes[i], 4) == 0) {
                chunk_id = i;
                break;
            }
        }

        switch (chunk_id) {
        case INFO:
            if (chunk_size != 60) {
                debug_print("INFO chunk size is not 60 but %d bytes\n", chunk_size);
                free(chunk_data);
                return NULL;
            }
            info_found = 1;
            uint8_t version = chunk_data[0];
            uint8_t disk_type = chunk_data[1];
            if (disk_type != 1)
                FatalError("Only 5.25\" disks are supported\n");
            //                uint8_t protected = chunk_data[2];
            //                uint8_t synchronized = chunk_data[3];
            //                uint8_t cleaned = chunk_data[4];
            char creator[32];
            memcpy(creator, chunk_data + 5, 32);
            creator[31] = '\0';
            if (version >= 2) {
                uint8_t disk_sides = chunk_data[37];
                if (disk_sides != 1)
                    FatalError("Only 1-sided images are supported\n");
                //                    uint8_t boot_sector_format = chunk_data[38];
                //                    uint8_t optimal_bit_timing = chunk_data[39];
                //                    uint16_t compatible_hardware = chunk_data[40] + chunk_data[41] * 256;
                //                    uint16_t required_ram = chunk_data[42] + chunk_data[43] * 256;
                //                    uint16_t largest_track = chunk_data[44] + chunk_data[45] * 256;
                //                    uint16_t flux_block = chunk_data[46] + chunk_data[47] * 256;
                //                    uint16_t largest_flux_track = chunk_data[48] + chunk_data[49] * 256;
            }
            debug_print("INFO: ver %d, type ", version);
            switch (disk_type) {
            case 1:
                debug_print("5.25\"");
                break;
            case 2:
                debug_print("3.5\"");
                break;
            default:
                debug_print("unknown");
                break;
            }
            debug_print(", creator: %s\n", creator);
            debug_print("WOZ file version %d\n", version);
            break;
        case TMAP:
            tmap_found = 1;
            if (chunk_size != 160) {
                debug_print("TMAP chunk size is not 160 but %d bytes\n", chunk_size);
                free(chunk_data);
                return NULL;
            }
            uint8_t tempmap[160];
            memcpy(tempmap, chunk_data, 160);
            debug_print("TMAP: ");
            for (int i = 0; i < nr_tracks; i++) {
                tmap[i] = tempmap[i * 4];
                if (i > 0)
                    debug_print(",");
                debug_print("%d", tmap[i]);
            }
            debug_print("\n");
            break;
        case TRKS:
            trks_found = 1;
            debug_print("TRKS: %d bytes\n", chunk_size);
            if (magic[3] == '2') {
                int cpos = 0;
                debug_print("  trks: ");
                for (int i = 0; i < 160; i++, cpos += 8) {
                    trks[i].bitstream = NULL;
                    trks[i].starting_block = chunk_data[cpos] + chunk_data[cpos + 1] * 256;
                    trks[i].block_count = chunk_data[cpos + 2] + chunk_data[cpos + 3] * 256;
                    trks[i].bit_count = chunk_data[cpos + 4] + (chunk_data[cpos + 5] << 8) + (chunk_data[cpos + 6] << 16) + (chunk_data[cpos + 7] << 24);
                    //                        debug_print("%d,%d,%d/", trks[i].starting_block, trks[i].block_count, trks[i].bit_count);
                }
                //                    debug_print("\n");
            } else {
                if (magic[3] != '1')
                    FatalError("bug");
                int chunkdatalen = chunk_size;
                int idx = 0;
                while (chunkdatalen >= 6656 && idx < 160) {
                    trks[idx].bytes_used = chunk_data[6646] + chunk_data[6647] * 256;
                    trks[idx].bit_count = chunk_data[6648] + chunk_data[6649] * 256;
                    trks[idx].bitstream = MemAlloc(6646);
                    memcpy(trks[idx].bitstream, chunk_data, 6646);
                    int newlen = chunkdatalen - 6656;
                    if (newlen > 0) {
                        uint8_t *tempdata = MemAlloc(newlen);
                        memcpy(tempdata, chunk_data + 6656, newlen);
                        memcpy(chunk_data, tempdata, newlen);
                        free(tempdata);
                    }
                    chunkdatalen -= 6656;
                    idx++;
                }
            }
            break;
        case WRIT:
            debug_print("WRIT: %d bytes\n", chunk_size);
            // ignore WRIT chunks
            break;
        case META:
            debug_print("META: %d bytes\n", chunk_size);
            for (int i = 0; i < chunk_size; i++) {
                debug_print("%c", chunk_data[i]);
            }
            break;
        default:
            debug_print("ignoring unknown chunk: %s; size=%dbytes\n", chunkstring, chunk_size);
            break;
        }
        free(chunk_data);
    }

    if (info_found == 0)
        FatalError("INFO chunk not found\n");
    if (tmap_found == 0)
        FatalError("TMAP chunk not found\n");
    if (trks_found == 0)
        FatalError("TRKS chunk not found\n");

    size_t out_file_size = nr_tracks * ((format == NIBBLE) ? NIBBLES_PER_TRACK : (SECTORS_PER_TRACK * BYTES_PER_SECTOR));
    *len = out_file_size;

    uint8_t *outfile = MemAlloc(out_file_size);

    int nr_empty_nonstandard_tracks = 0;

    size_t write_position = 0;
    int trks_index = 0;
    uint8_t *bitstream = NULL;

    for (int track = 0; track < nr_tracks; track++) {
        trks_index = tmap[track];
        if (trks_index == 0xff) { // empty track
            debug_print("T[%02x]: empty track\n", track);
            if (format == NIBBLE) {
                write_position += NIBBLES_PER_TRACK;
            }
            if (track > STANDARD_TRACKS_PER_DISK)
                nr_empty_nonstandard_tracks++;
        }

        trksdata *trk = &trks[trks_index];
        if (trk == NULL) {
            debug_print("T[%02x]: bad TMAP value\n", trks_index);
            return NULL;
        }

        if (magic[3] == '2') {
            int start = trk->starting_block * WOZ_BLOCK_SIZE;
            if (start >= woz_size) {
                debug_print("start block offset (%d) of track %02x is beyond file size (%zu)\n", start, track, woz_size);
                return NULL;
            }

            int length = trk->block_count * WOZ_BLOCK_SIZE;
            if (start + length > woz_size) {
                debug_print("length (%d) of track %02x extends beyond end of file\n", length, track);
                return NULL;
            }

            bitstream = woz_buffer + start;
        } else {
            if (magic[3] != '1')
                FatalError("Unsupported Woz version!");
            bitstream = trk->bitstream;
        }

        int bit_count = trk->bit_count;
        int bytes = bit_count / 8;

        int foundbitpos = -1;
        SearchResultType result = find_syncbytes(bitstream, trk->bit_count, &foundbitpos);
        if (result == FOUND_NONE) {
            bitstream = swap_head_and_tail(bitstream, bit_count - 72, bit_count);
            result = find_syncbytes(bitstream, trk->bit_count, &foundbitpos);
        }

        switch (result) {
        case FOUND16:
            bitstream = swap_head_and_tail(bitstream, foundbitpos + 50, bit_count);
            break;
        case FOUND13:
            bitstream = swap_head_and_tail(bitstream, foundbitpos + 72, bit_count);
            break;
        default:
            debug_print("Found no sync bytes in track %d bitstream\n", track);
            break;
        }

        int pos2 = 0;
        uint8_t nibbles[NIBBLES_PER_TRACK];
        int number_of_nibbles = 0;
        do {
            nibbles[number_of_nibbles++] = extract_nibble(bitstream, bit_count, &pos2);
        } while (pos2 < bytes && number_of_nibbles < NIBBLES_PER_TRACK);

        if (number_of_nibbles < NIBBLES_PER_TRACK) {
            for (int i = number_of_nibbles; i < NIBBLES_PER_TRACK; i++)
                nibbles[i] = 0;
            debug_print("padded to %d nibbles\n", NIBBLES_PER_TRACK);
        } else {
            debug_print("(just made!)\n");
        }

        memcpy(tempout + writepos, nibbles, NIBBLES_PER_TRACK);
        writepos += NIBBLES_PER_TRACK;
    }

    for (int i = 0; i < 160; i++)
        if (trks[i].bitstream != NULL)
            free(trks[i].bitstream);

    memcpy(outfile, tempout, MIN(out_file_size, writepos));
    return outfile;
}
