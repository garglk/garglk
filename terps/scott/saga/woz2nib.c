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

static void FatalError( char *format, ... )
{
    va_list argp;

    fprintf( stderr, "\nFatal: " );

    va_start( argp, format );
    vprintf( format, argp );
    va_end( argp );

    fprintf( stderr, "\n" );

    exit( 1 );
}

static char *create_bitstring(uint8_t *bitstream, int bit_count) {
    char *result = malloc(bit_count + 1);
    for (int i = 0; i <= bit_count / 8; i++) {
        uint8_t byte = bitstream[i];
        int byteindex = i * 8;
        for (int j = 0; j < 8; j++) {
            if (byteindex + j >= bit_count)
                continue;
            if (byte & (1 << (7 - j))) {
                result[byteindex + j] = '1';
            } else {
                result[byteindex + j] = '0';
            }
        }
    }
    result[bit_count] = '\0';
    return result;
}

// This is a translation of $trk =~ s{^0*(1.{7})}{}o;
// Extract first '1' + 7 characters ("bits").
// Truncate start of bitstring to first char ("bit") following this.
// Bitstring must be null terminated.
static char *extract_nibble(char *bitstring, int *length, char nibble[8]) {
    int i = 0;

    // skip any initial '0'.
    while (bitstring[i] == '0')
        i++;

    if (bitstring[i] != '1')
        // bitstring contained no '1'.
        return NULL;

    for (int j = 1; j < 8; j++)
        if (bitstring[i + j] == '\0')
            // End of bitstring reached,
            // does not contain 8 "bits".
            return NULL;

    memcpy(nibble, bitstring + i, 8);
    *length = *length - i - 8;
    uint8_t temp[*length];
    memcpy(temp, bitstring + i + 8, *length);
    memcpy(bitstring, temp, *length);

    bitstring[*length] = '\0';
    return bitstring;
}

static uint8_t bitstring2byte(char nibble[8]) {
    uint8_t result = 0;
    for (int i = 7; i >=0; i--)
        if (nibble[i] == '1') {
            result += 1 << (7 - i);
        }
    return result;
}


uint8_t *woz2nib(uint8_t *ptr, size_t *len) {
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

    if (magic[0] != 'W' || magic[1] != 'O' || magic[2] != 'Z' || (magic[3] != '1' && magic[3] != '2' ))
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
        uint8_t chunk_data[chunk_size];
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
                if (version >= 2)  {
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
                    return NULL;
                }
                uint8_t tempmap[160];
                memcpy(tempmap, chunk_data, 160);
                debug_print("TMAP: ");
                for (int i = 0; i < nr_tracks ; i++) {
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
                        trks[i].bit_count = chunk_data[cpos + 4] + (chunk_data[cpos + 5] << 8) + (chunk_data[cpos + 6] << 16) +
                        (chunk_data[cpos + 7] << 24);
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
                        trks[idx].bitstream = malloc(6646);
                        memcpy(trks[idx].bitstream, chunk_data, 6646);
                        int newlen = chunkdatalen - 6656;
                        if (newlen > 0) {
                            uint8_t tempdata[newlen];
                            memcpy(tempdata, chunk_data + 6656, newlen);
                            memcpy(chunk_data, tempdata, newlen);
                        }
                        chunkdatalen -= 6656;
                        idx++;
                    }
                }
                break;
            case WRIT:
                debug_print ("WRIT: %d bytes\n", chunk_size);
                // ignore WRIT chunks
                break;
            case META:
                debug_print ("META: %d bytes\n", chunk_size);
                for (int i = 0; i < chunk_size; i++) {
                    debug_print("%c", chunk_data[i]);
                }
                break;
            default:
                debug_print("ignoring unknown chunk: %s; size=%dbytes\n", chunkstring, chunk_size);
                break;
        }
    }

    if (info_found == 0)
        FatalError("INFO chunk not found\n");
    if (tmap_found == 0)
        FatalError("TMAP chunk not found\n");
    if (trks_found == 0)
        FatalError("TRKS chunk not found\n");

    size_t out_file_size = nr_tracks * ((format == NIBBLE) ? NIBBLES_PER_TRACK : (SECTORS_PER_TRACK * BYTES_PER_SECTOR));
    *len = out_file_size;

    uint8_t *outfile = malloc(out_file_size);

    int nr_empty_nonstandard_tracks = 0;

    size_t write_position = 0;
    int trks_index = 0;
    char t_hex[3];
    uint8_t *bitstream = NULL;
    char *bitstring = NULL;

    for (int track = 0; track < nr_tracks; track++) {
        snprintf(t_hex, sizeof t_hex, "%02x", track);
        debug_print("Track $%s\n", t_hex);
        trks_index = tmap[track];
        if (trks_index == 0xff) { // empty track
            debug_print("T%s: empty track\n", t_hex);
            if (format == NIBBLE) {
                write_position += NIBBLES_PER_TRACK;
            }
            if (track > STANDARD_TRACKS_PER_DISK)
                nr_empty_nonstandard_tracks++;

        }

        trksdata *trk = &trks[trks_index];
        if (trk == NULL) {
            debug_print("T[%s]: bad TMAP value\n", t_hex);
            return 0;
        }

        if (magic[3] == '2') {
            int start = trk->starting_block * WOZ_BLOCK_SIZE;
            if (start >= woz_size) {
                debug_print("start block offset (%d) of track %s is beyond file size (%zu)\n", start, t_hex, woz_size);
                return NULL;
            }

            int length = trk->block_count * WOZ_BLOCK_SIZE;
            if (start + length > woz_size) {
                debug_print("length (%d) of track %s extends beyond end of file\n", length, t_hex);
                return NULL;
            }

            bitstream = woz_buffer + start;

        } else {
            if (magic[3] != '1')
                FatalError("bug");
            bitstream = trk->bitstream;
        }

        int bit_count = trk->bit_count;
        int nr_sectors = 0;
        if (bitstring != NULL) {
            free(bitstring);
        }
        bitstring = create_bitstring(bitstream, bit_count);

        const char *sync_bytes[17];
        sync_bytes[16] = "111111110011111111001111111100" "11111111001111111100";
        sync_bytes[13] = "111111110111111110111111110111111110" "111111110111111110111111110111111110";
        for (int ns = 16; ns >= 13; ns -= 3) {
            int sync_len = 50;
            if (ns == 13)
                sync_len = 72;
            char *pos = strstr(bitstring, sync_bytes[ns]);
            if (pos != NULL) { // Found sync bytes
                int offset = (int)(pos - bitstring);
                nr_sectors = ns;
                int tail_len = bit_count - offset - sync_len;
                int head_len = offset + sync_len;

                char head[head_len];
                memcpy(head, bitstring, head_len);

                char tail[tail_len];
                memcpy(tail, bitstring + head_len, tail_len);

                memcpy(bitstring, tail, tail_len);
                memcpy(bitstring + tail_len, head, head_len);
                bitstring[bit_count] = '\0';
                break;
            } else { // has been split when linearizing the circular track?
                debug_print ("rotate and retry by %d bits\n", sync_len);

                int tail_len = bit_count - sync_len;

                char head[sync_len];
                memcpy(head, bitstring + tail_len, sync_len);

                char tail[tail_len];
                memcpy(tail, bitstring, tail_len);

                memcpy(bitstring, head, sync_len);
                memcpy(bitstring + sync_len, tail, tail_len);

                pos = strstr(bitstring, sync_bytes[ns]); //try once more...
                if (pos != NULL) {
                    int offset = (int)(pos - bitstring);
                    nr_sectors = ns;
                    int tail_len = bit_count - offset - sync_len;
                    int head_len = offset + sync_len;
                    char head[head_len];
                    memcpy(head, bitstring, head_len);

                    char tail[tail_len];
                    memcpy(tail, bitstring + head_len, tail_len);

                    memcpy(bitstring, tail, tail_len);
                    memcpy(bitstring + tail_len, head, head_len);
                    bitstring[bit_count] = '\0';
                    break;
                }
            }
        }
        if (nr_sectors == 0) {
            debug_print("track %s has no sync bytes\n", t_hex);
        }

        uint8_t nibbles[NIBBLES_PER_TRACK];
        int number_of_nibbles = 0;
        char nibble[8];
        do {
            bitstring = extract_nibble(bitstring, &bit_count, nibble);
            if (bitstring)
                nibbles[number_of_nibbles++] = bitstring2byte(nibble);
        } while (bitstring != NULL && number_of_nibbles < NIBBLES_PER_TRACK);

        debug_print("T[%s]: %d nibbles ", t_hex, number_of_nibbles);

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

    if (bitstring != NULL) {
        free(bitstring);
    }

    for (int i = 0; i < 160; i++)
        if (trks[i].bitstream != NULL)
            free(trks[i].bitstream);

    memcpy(outfile, tempout, MIN(out_file_size, writepos));
    return outfile;
}
