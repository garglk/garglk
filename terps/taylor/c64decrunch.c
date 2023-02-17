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
    { REBEL_PLANET_64, 0x2ab00, 0xa46f, TYPE_D64, 2, "-e0x0fe9", 2 }, // Rebel_Planet_1986_Adventure_International.d64
    { REBEL_PLANET_64, 0x2ab00, 0x932e, TYPE_D64, 1, NULL, 0 }, // Rebel_Planet_1986_Adventure_International_cr_TFF.d64

    { HEMAN_64, 0xfa17, 0xfbd2, TYPE_T64, 2, NULL, 0 }, // Terraquake C64 (T64) Super Compressor / Flexible -> ECA Compacker
    { HEMAN_64, 0x2ab00, 0x4625, TYPE_D64, 2, NULL, 0 }, // Masters_of_the_Universe_Terraquake_1987_Gremlin_Graphics_cr_TIA.d64
    { HEMAN_64, 0x2ab00, 0x78ba, TYPE_D64, 4, "-e0xc400", 4 }, // Masters_of_the_Universe_Terraquake_1987_Gremlin_Graphics_cr_Popeye.d64

    { TEMPLE_OF_TERROR_64, 0xf716, 0x2b54, TYPE_T64, 4, NULL, 0 }, // Temple of Terror C64 (T64) 1001 CardCruncher New Packer -> 1001 CardCruncher ACM -> Triad-01 -> Mr.Z Packer
    { TOT_TEXT_ONLY_64, 0xf716, 0x2b54, TYPE_T64, 3, NULL, 0 }, // Temple of Terror C64 (T64) 1001 CardCruncher New Packer -> 1001 CardCruncher ACM -> Triad-01 -> Mr.Z Packer
    { TEMPLE_OF_TERROR_64, 0x10baa, 0x3b37, TYPE_T64, 4, NULL, 0 }, // Temple of Terror C64 alt (T64) 1001 CardCruncher New Packer -> 1001 CardCruncher ACM -> Triad-01 -> Mr.Z Packer
    { TOT_TEXT_ONLY_64, 0x10baa, 0x3b37, TYPE_T64, 3, NULL, 0 }, // Temple of Terror C64 alt (T64) 1001 CardCruncher New Packer -> 1001 CardCruncher ACM -> Triad-01 -> Mr.Z Packer
    { TEMPLE_OF_TERROR_64, 0x2ab00, 0x5720, TYPE_D64, 4, "-e0xc1ef", 2 }, // Temple of Terror C64 (D64) ECA Compacker -> Super Compressor / Flexible -> Super Compressor / Equal sequences -> Super Compressor / Equal chars
    { TEMPLE_OF_TERROR_64, 0x2ab00, 0xf3b4, TYPE_D64, 3, NULL, 0}, // Temple_of_Terror_1987_U.S._Gold_cr_FBR.d64
    { TEMPLE_OF_TERROR_64, 0x2ab00, 0x577e, TYPE_D64, 4, NULL, 0 }, // Temple_of_Terror_1987_U.S._Gold_cr_Triad.d64
    { TOT_TEXT_ONLY_64, 0x2ab00, 0x577e, TYPE_D64, 4, NULL, 0 }, // Temple_of_Terror_1987_U.S._Gold_cr_Triad.d64
    { TEMPLE_OF_TERROR_64, 0x2ab00, 0x7b2d, TYPE_D64, 0, NULL, 0 }, // Temple of Terror - Trianon.d64
    { TOT_TEXT_ONLY_64, 0x2ab00, 0x7b2d, TYPE_D64, 0, NULL, 0 }, // Temple of Terror - Trianon.d64
    { TEMPLE_OF_TERROR_64, 0x2ab00, 0x4661, TYPE_D64, 0, NULL, 0 }, // Temple_of_Terror_1987_U.S._Gold_cr_Trianon.d64
    { TOT_TEXT_ONLY_64, 0x2ab00, 0x4661, TYPE_D64, 0, NULL, 0 }, // Temple_of_Terror_1987_U.S._Gold_cr_Trianon.d64
    { TEMPLE_OF_TERROR_64, 0x2ab00, 0x3ee3, TYPE_D64, 6, "-e0xc1ef", 4 }, // Temple_of_Terror_Graphic_Version_1987_U.S._Gold_cr_FLT.d64
    { TEMPLE_OF_TERROR_64, 0x2ab00, 0x5b97, TYPE_D64, 4, "-e0xc1ef", 2 }, // Temple_of_Terror_1987_U.S._Gold_h_FLT.d64
    { TEMPLE_OF_TERROR_64, 0x2ab00, 0x55a1, TYPE_D64, 4, "-e0xc1ef", 2 }, // Temple_of_Terror_1987_US_Gold_cr_FLT.d64

    { KAYLETH_64, 0x2ab00, 0xc75f, TYPE_D64, 3, NULL, 0 }, // Kayleth D64 Super Compressor / Flexible Hack -> Super Compressor / Equal sequences -> Super Compressor / Equal chars
    { KAYLETH_64, 0xb1f2, 0x7757, TYPE_T64, 3, NULL, 0 }, // Kayleth T64 Super Compressor / Flexible Hack -> Super Compressor / Equal sequences -> Super Compressor / Equal chars
    { KAYLETH_64, 0x2ab00, 0x2359, TYPE_D64, 3, NULL, 0 }, // Compilation_Karate_Chop_Kayleth_Kettle_Knotie_in_Cave_19xx_-.d64
    { KAYLETH_64, 0x2ab00, 0x3a10, TYPE_D64, 4, NULL, 0 }, // Kayleth_1987_U.S._Gold_cr_Newstars.d64
    { KAYLETH_64, 0x2ab00, 0xc184, TYPE_D64, 3, NULL, 0 }, // Kayleth_1987_U.S._Gold_cr_Triad.d64
    { KAYLETH_64, 0x2ab00, 0x7a59, TYPE_D64, 4, "-e0x9a0", 2 }, // Kayleth_1987_U.S._Gold_cr_OkKoral.d64
    { KAYLETH_64, 0x2ab00, 0xbf75, TYPE_D64, 3, NULL, 0 }, // Kayleth_1987_U.S._Gold.d64

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

static GameIDType DecrunchC64(uint8_t **sf, size_t *extent, struct c64rec entry);

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

static uint8_t *get_file_named(uint8_t *data, size_t length, size_t *newlength,
                               const char *name)
{
    uint8_t *file = NULL;
    *newlength = 0;
    DiskImage *d64 = di_create_from_data(data, (int)length);
    unsigned char rawname[100];
    di_rawname_from_name(rawname, name);
    if (d64) {
        ImageFile *c64file = di_open(d64, rawname, 0xc2, "rb");
        if (c64file) {
            uint8_t buf[0xffff];
            *newlength = di_read(c64file, buf, 0xffff);
            file = MemAlloc(*newlength);
            memcpy(file, buf, *newlength);
            free(c64file);
        }
    }
    return file;
}

static uint8_t *get_file_at_ts(uint8_t *data, size_t length, size_t *newlength, int track, int sector)
{
    uint8_t *file = NULL;
    *newlength = 0;
    DiskImage *d64 = di_create_from_data(data, (int)length);
    if (d64) {
        ImageFile *c64file = di_create_file_from_ts(d64, track, sector);
        if (c64file) {
            uint8_t buf[0xffff];
            *newlength = di_read(c64file, buf, 0xffff);
            file = MemAlloc(*newlength);
            memcpy(file, buf, *newlength);
            free(c64file);
        }
    }
    return file;
}

static GameIDType terror_menu(uint8_t **sf, size_t *extent, int recindex)
{
    struct c64rec rec = c64_registry[recindex];
    int isdisk = (rec.type == TYPE_D64);
    OpenBottomWindow();
    Display(Bottom, "This %s image contains one version of Temple of Terror with pictures, and one with without pictures but slightly more text.\n\nPlease select one:\n1. Graphics version\n2. Text-only version\n3. Use pictures from file 1 and text from file 2", isdisk ? "disk" : "datasette");

    int number_of_records = 0;
    if (!isdisk)
        number_of_records = (*sf)[36] + (*sf)[37] * 0x100;
    size_t size1 = *extent;
    size_t size2 = *extent;
    uint8_t *file1 = NULL;
    uint8_t *file2 = NULL;
    if (isdisk) {
        if (rec.chk == 0x4661 || rec.chk == 0x7b2d) {
            uint8_t *tempfile1 = get_file_at_ts(*sf, *extent, &size1, 17, 1);
            size1 -= 176;
            file1 = MemAlloc(size1);
            memcpy(file1, tempfile1 + 176, size1);
            uint8_t *tempfile2 = get_file_at_ts(*sf, *extent, &size2, 20, 16);
            file2 = MemAlloc(size2 + 16);
            memcpy(file2 + 16, tempfile2, size2);
            size2 += 16;
        } else {
            file1 = get_file_named(*sf, *extent,  &size1, "TEMPLE O.TERROR2");
            file2 = get_file_named(*sf, *extent,  &size2, "TEMPLE O.TERROR1");
        }
    } else {
        file1 = GetFileFromT64(1, number_of_records, sf, &size1);
        file2 = GetFileFromT64(2, number_of_records, sf, &size2);
    }
    free(*sf);

    if (file1 == NULL || file2 == NULL) {
        fprintf(stderr, "TAYLOR: terror_menu() Failed loading file!\n");
        return UNKNOWN_GAME;
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
    }
    return UNKNOWN_GAME;
}

GameIDType DetectC64(uint8_t **sf, size_t *extent)
{
    if (*extent > MAX_LENGTH || *extent < MIN_LENGTH)
        return UNKNOWN_GAME;

    uint16_t chksum = checksum(*sf, *extent);

    for (int i = 0; c64_registry[i].length != 0; i++) {
        struct c64rec record = c64_registry[i];
        if (*extent == record.length && chksum == record.chk) {
            if (record.type == TYPE_D64) {
                if (chksum == 0x577e || chksum == 0x4661 || chksum == 0x7b2d) {
                    return terror_menu(sf, extent, i);
                } else {
                    size_t newlength;
                    uint8_t *largest_file = get_largest_file(*sf, *extent, &newlength);

                    if (largest_file) {
                        free(*sf);
                        *sf = largest_file;
                        *extent = newlength;
                    } else {
                        Fatal("Could not read file from disk image");
                    }
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
    return UNKNOWN_GAME;
}

static GameIDType DecrunchC64(uint8_t **sf, size_t *extent, struct c64rec record)
{
    size_t decompressed_length = *extent;

    uint8_t *uncompressed = MemAlloc(0x10000);

    int result = 0;

    for (int i = 1; i <= record.decompress_iterations; i++) {
        /* We only send switches on the iteration specified by parameter */
        if (i == record.parameter && record.switches != NULL) {
            result = unp64(*sf, *extent, uncompressed,
                &decompressed_length, record.switches);
        } else
            result = unp64(*sf, *extent, uncompressed,
                &decompressed_length, NULL);
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

    uint8_t temp[0x2e79];

    switch (record.chk) {
        case 0xa46f:
            memcpy(temp, *sf + 0x098a, 0x2e79);
            memcpy(*sf + 0x198a, temp, 0x2e79);
            break;
        case 0x78ba:
            memcpy(temp, *sf + 0xc802, 0x1400);
            memcpy(*sf + 0x4808, temp, 0x1400);
            break;
        case 0xf3b4:
            memcpy(temp, *sf + 0xd802, 0x1400);
            memcpy(*sf + 0x4808, temp, 0x1400);
            break;
        default:
            break;
    }

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
