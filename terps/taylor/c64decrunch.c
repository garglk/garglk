//
//  c64decrunch.c
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Created by Petter Sjölund on 2022-01-30.
//

#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "utility.h"
#include "taylor.h"

#include "c64decrunch.h"
#include "c64diskimage.h"

#include "unp64_interface.h"

#define MAX_LENGTH 300000
#define MIN_LENGTH 24

typedef enum {
    UNKNOWN_FILE_TYPE,
    TYPE_D64,
    TYPE_T64 } file_type;

struct c64rec {
    GameIDType id;
    size_t length;
    uint16_t chk;
    file_type type;
    int decompress_iterations;
    const char *switches;
    int parameter;
};

static const struct c64rec c64_registry[] = {
    { QUESTPROBE3_64,  0x69e3, 0x3b96, TYPE_T64, 1, NULL, 0 }, // Questprobe 3, PUCrunch
    { QUESTPROBE3_64,  0x8298, 0xb93e, TYPE_T64, 1, NULL, 0 }, // Questprobe 3, PUCrunch
    { QUESTPROBE3_64,  0x2ab00, 0x863d, TYPE_D64, 1, NULL, 0 }, // Questprobe 3, PUCrunch
    { REBEL_PLANET_64, 0xd541, 0x593a, TYPE_T64, 1, NULL, 0 }, // Rebel Planet C64 (T64) 1001 CardCruncher Old Packer
    { HEMAN_64, 0xfa17, 0xfbd2, TYPE_T64, 2, NULL, 0 }, // Terraquake C64 (T64) Super Compressor / Flexible -> ECA Compacker
    { TEMPLE_OF_TERROR_64, 0xf716, 0x2b54, TYPE_T64, 4, NULL, 0 }, // Temple of Terror C64 (T64) 1001 CardCruncher New Packer -> 1001 CardCruncher ACM -> Triad-01 -> Mr.Z Packer
    { TOT_TEXT_ONLY_64, 0xf716, 0x2b54, TYPE_T64, 3, NULL, 0 }, // Temple of Terror C64 (T64) 1001 CardCruncher New Packer -> 1001 CardCruncher ACM -> Triad-01 -> Mr.Z Packer
    { TEMPLE_OF_TERROR_64, 0x10baa, 0x3b37, TYPE_T64, 4, NULL, 0 }, // Temple of Terror C64 alt (T64) 1001 CardCruncher New Packer -> 1001 CardCruncher ACM -> Triad-01 -> Mr.Z Packer
    { TOT_TEXT_ONLY_64, 0x10baa, 0x3b37, TYPE_T64, 3, NULL, 0 }, // Temple of Terror C64 alt (T64) 1001 CardCruncher New Packer -> 1001 CardCruncher ACM -> Triad-01 -> Mr.Z Packer
    { TEMPLE_OF_TERROR_64, 0x2ab00, 0x5720, TYPE_D64, 4, "-e0xc1ef", 2 }, // Temple of Terror C64 (D64) ECA Compacker -> Super Compressor / Flexible -> Super Compressor / Equal sequences -> Super Compressor / Equal chars
    { KAYLETH_64, 0x2ab00, 0xc75f, TYPE_D64, 3, NULL, 0 }, // Kayleth D64 Super Compressor / Flexible Hack -> Super Compressor / Equal sequences -> Super Compressor / Equal chars
    { KAYLETH_64, 0xb1f2, 0x7757, TYPE_T64, 3, NULL, 0 }, // Kayleth T64 Super Compressor / Flexible Hack -> Super Compressor / Equal sequences -> Super Compressor / Equal chars

    { UNKNOWN_GAME, 0, 0, UNKNOWN_FILE_TYPE, 0, NULL, 0 }
};

extern struct GameInfo games[NUMGAMES];

static uint16_t checksum(uint8_t *sf, size_t extent)
{
    uint16_t c = 0;
    for (int i = 0; i < extent; i++)
        c += sf[i];
    return c;
}

static int DecrunchC64(uint8_t **sf, size_t *extent, struct c64rec entry);

static uint8_t *get_largest_file(uint8_t *data, size_t length, size_t *newlength)
{
    uint8_t *file = NULL;
    *newlength = 0;
    DiskImage *d64 = di_create_from_data(data, (int)length);
    if (d64) {
        RawDirEntry *largest = find_largest_file_entry(d64);
        if (largest) {
            ImageFile *c64file = di_open(d64, largest->rawname, largest->type, "rb");
            if (c64file) {
                uint8_t buf[0xffff];
                *newlength = di_read(c64file, buf, 0xffff);
                file = MemAlloc(*newlength);
                memcpy(file, buf, *newlength);
                free(c64file);
            }
        }
    }
    return file;
}

static uint8_t *GetFileFromT64(int filenum, int number_of_records, uint8_t **sf, size_t *extent) {
    uint8_t *file_records = *sf + 32 + 32 * filenum;
    int offset = file_records[8] + file_records[9] * 0x100;
    int start_addr = file_records[2] + file_records[3] * 0x100;
    int end_addr = file_records[4] + file_records[5] * 0x100;
    size_t size;
    if (number_of_records == filenum)
        size = *extent - offset;
    else
        size = end_addr - start_addr;
    if (size < 1 || offset + size > *extent)
        return NULL;
    uint8_t *file = MemAlloc(size + 2);
    memcpy(file, file_records + 2, 2);
    memcpy(file + 2, *sf + offset, size);
    *extent = size + 2;
    return file;
}

static int terror_menu(uint8_t **sf, size_t *extent, int recindex)
{
    OpenBottomWindow();
    Display(Bottom, "This datasette image contains one version of Temple of Terror with pictures, and one with without pictures but slightly more text.\n\nPlease select one:\n1. Graphics version\n2. Text-only version\n3. Use pictures from file 1 and text from file 2");

    int number_of_records = (*sf)[36] + (*sf)[37] * 0x100;
    size_t size1 = *extent;
    uint8_t *file1 = GetFileFromT64(1, number_of_records, sf, &size1);
    size_t size2 = *extent;
    uint8_t *file2 = GetFileFromT64(2, number_of_records, sf, &size2);
    free(*sf);
    struct c64rec rec = c64_registry[recindex];

    if (file1 == NULL || file2 == NULL) {
        fprintf(stderr, "TAYLOR: terror_menu() Failed loading file!\n");
        return 0;
    }
    glk_request_char_event(Bottom);

    event_t ev;
    int result = 0;
    do {
        glk_select(&ev);
        if (ev.type == evtype_CharInput) {
            if (ev.val1 == '1' || ev.val1 == '2' || ev.val1 == '3') {
                result = ev.val1 - '0';
            } else {
                glk_request_char_event(Bottom);
            }
        }
    } while (result == 0);

    glk_window_clear(Bottom);

    switch (result) {
        case 2: {
            uint8_t *temp = file1;
            file1 = file2;
            file2 = temp;
            size1 = size2;
            rec = c64_registry[recindex + 1];
        }
            // fallthrough
        case 1:
            free(file2);
            *sf = file1;
            *extent = size1;
            return DecrunchC64(sf, extent, rec);
        case 3:
        {
            DecrunchC64(&file1, &size1, rec);
            DecrunchC64(&file2, &size2, c64_registry[recindex + 1]);

            uint8_t *final = MemAlloc(size2 + 0x4124);
            memcpy(final, file2, size2);
            memcpy(final + 0x16ea, file1 + 0x4708, 0x1400);
            memcpy(final + 0x8888, file1 + 0x8048, 0x4123);
            free(file1);
            free(file2);
            *sf = final;
            *extent = size2 + 0x4124;

            for (int i = 0; games[i].Title != NULL; i++) {
                if (games[i].gameID == TOT_HYBRID_64) {
                    Game = &games[i];
                    break;
                }
            }

            return TOT_HYBRID_64;
        }
        default:
            fprintf(stderr, "TAYLOR: invalid switch case!\n");
            return 0;

    }
    return 0;
}

int DetectC64(uint8_t **sf, size_t *extent)
{
    if (*extent > MAX_LENGTH || *extent < MIN_LENGTH)
        return 0;

    uint16_t chksum = checksum(*sf, *extent);

    for (int i = 0; c64_registry[i].length != 0; i++) {
        if (*extent == c64_registry[i].length && chksum == c64_registry[i].chk) {
            if (c64_registry[i].type == TYPE_D64) {
                size_t newlength;
                uint8_t *largest_file = get_largest_file(*sf, *extent, &newlength);

                if (largest_file) {
                    free(*sf);
                    *sf = largest_file;
                    *extent = newlength;
                } else {
                    Fatal("Could not read file from disk image");
                }

            } else if (c64_registry[i].type == TYPE_T64) {
                if (c64_registry[i].chk == 0x2b54 || c64_registry[i].chk == 0x3b37)
                    return terror_menu(sf, extent, i);
                int number_of_records = (*sf)[36] + (*sf)[37] * 0x100;
                size_t size = *extent;
                uint8_t *first_file = GetFileFromT64(1, number_of_records, sf, &size);
                free(*sf);
                *sf = first_file;
                *extent = size;
            } else {
                Fatal("Unknown type");
            }
            return DecrunchC64(sf, extent, c64_registry[i]);
        }
    }
    return 0;
}

static int DecrunchC64(uint8_t **sf, size_t *extent, struct c64rec record)
{
    size_t decompressed_length = *extent;

    uint8_t *uncompressed = MemAlloc(0xffff);

    char *switches[4];
    char string[100];
    int numswitches = 0;

    if (record.switches != NULL) {
        strcpy(string, record.switches);
        switches[numswitches] = strtok(string, " ");

        while (switches[numswitches] != NULL)
            switches[++numswitches] = strtok(NULL, " ");
    }

    size_t result = 0;

    for (int i = 1; i <= record.decompress_iterations; i++) {
        /* We only send switches on the iteration specified by parameter */
        if (i == record.parameter && record.switches != NULL) {
            result = unp64(*sf, *extent, uncompressed,
                &decompressed_length, switches, numswitches);
        } else
            result = unp64(*sf, *extent, uncompressed,
                &decompressed_length, NULL, 0);
        if (result) {
            if (*sf != NULL)
                free(*sf);
            *sf = MemAlloc(decompressed_length);
            memcpy(*sf, uncompressed, decompressed_length);
            *extent = decompressed_length;
        } else {
            free(uncompressed);
            uncompressed = NULL;
            break;
        }
    }

    if (uncompressed != NULL)
        free(uncompressed);

    for (int i = 0; games[i].Title != NULL; i++) {
        if (games[i].gameID == record.id) {
            Game = &games[i];
            break;
        }
    }

    if (!Game || Game->Title == NULL) {
        Fatal("Game not found!");
    }

    return CurrentGame;
}
