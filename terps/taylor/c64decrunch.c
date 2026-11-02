//
//  c64decrunch.c
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Created by Petter Sjölund on 2022-01-30.
//

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "c64diskimage.h"
#include "c64unpack.h"
#include "taylor.h"
#include "utility.h"
#include "unp64_interface.h"

#include "c64decrunch.h"

typedef enum {
    UNKNOWN_FILE_TYPE,
    TYPE_D64, /* 1541 disk image */
    TYPE_T64  /* tape/datasette image */
} file_type;

/* How the game is stored in the image. All but ONE_FILE are Temple of
   Terror images with the graphics and the text-only version side by side;
   those have two registry entries, graphics version first. */
typedef enum {
    ONE_FILE,          /* D64: the largest file; T64: the first record */
    TWO_FILES_BY_TS,   /* D64: files not in the directory, found by track and sector */
    TWO_FILES_BY_NAME, /* D64: two named files, each entry naming its own */
    T64_TWO_RECORDS    /* T64: the first two records */
} file_layout;

/* A registry entry identifying a known C64 game image.
   Fields: game ID, expected file length, 16-bit additive checksum,
   container type, number of unp64 decompression passes, optional
   unp64 switch string, the pass (1-based) the switches apply to, how the
   game is stored, and the file name for TWO_FILES_BY_NAME. The last three
   are for the releases that unpack their picture data to somewhere other
   than where the game has it: relocate_size bytes are moved from
   relocate_from to relocate_to in the unpacked file. */
typedef struct {
    GameIDType id;
    size_t length;
    uint16_t chk;
    file_type type;
    int decompress_iterations;
    const char *switches;
    int switch_pass;
    file_layout layout;
    const char *filename;
    size_t relocate_to;
    size_t relocate_from;
    size_t relocate_size;
} c64rec;

/* Known C64 TaylorMade game images indexed by file size + checksum.
   Covers Questprobe 3, Rebel Planet, He-Man/Terraquake, Temple of Terror,
   and Kayleth across various crack/pack releases. */
static const c64rec c64_registry[] = {
    { QUESTPROBE3_64, 0x69e3, 0x3b96, TYPE_T64, 1, NULL, 0, ONE_FILE, NULL }, // Questprobe 3, PUCrunch
    { QUESTPROBE3_64, 0x8298, 0xb93e, TYPE_T64, 1, NULL, 0, ONE_FILE, NULL }, // Questprobe 3, PUCrunch
    { QUESTPROBE3_64, 0x2ab00, 0x863d, TYPE_D64, 1, NULL, 0, ONE_FILE, NULL }, // Questprobe 3, PUCrunch
    { REBEL_PLANET_64, 0xd541, 0x593a, TYPE_T64, 1, NULL, 0, ONE_FILE, NULL }, // Rebel Planet C64 (T64) 1001 CardCruncher Old Packer
    { REBEL_PLANET_64, 0x2ab00, 0xa46f, TYPE_D64, 2, "-e0x0fe9", 2, ONE_FILE, NULL, 0x198a, 0x098a, 0x2e79 }, // Rebel_Planet_1986_Adventure_International.d64
    { REBEL_PLANET_64, 0x2ab00, 0x932e, TYPE_D64, 1, NULL, 0, ONE_FILE, NULL }, // Rebel_Planet_1986_Adventure_International_cr_TFF.d64

    { HEMAN_64, 0xfa17, 0xfbd2, TYPE_T64, 2, NULL, 0, ONE_FILE, NULL }, // Terraquake C64 (T64) Super Compressor / Flexible -> ECA Compacker
    { HEMAN_64, 0x2ab00, 0x4625, TYPE_D64, 2, NULL, 0, ONE_FILE, NULL }, // Masters_of_the_Universe_Terraquake_1987_Gremlin_Graphics_cr_TIA.d64
    { HEMAN_64, 0x2ab00, 0x78ba, TYPE_D64, 4, "-e0xc400", 4, ONE_FILE, NULL, 0x4808, 0xc802, 0x1400 }, // Masters_of_the_Universe_Terraquake_1987_Gremlin_Graphics_cr_Popeye.d64
    // The first pass also unpacks the crack intro at $c000, which tells the
    // next depacker that its data ends at $b1db. Cut the output there.
    { HEMAN_64, 0x2ab00, 0xbd68, TYPE_D64, 4, "-t0xb1da", 1, ONE_FILE, NULL }, // He-Man and the Masters of the Universe in Terraquake (c64.com), WCC crack: ECA Compacker -> Super Compressor / Flexible -> two more

    { TEMPLE_OF_TERROR_64, 0xf716, 0x2b54, TYPE_T64, 4, NULL, 0, T64_TWO_RECORDS, NULL }, // Temple of Terror C64 (T64) 1001 CardCruncher New Packer -> 1001 CardCruncher ACM -> Triad-01 -> Mr.Z Packer
    { TOT_TEXT_ONLY_64, 0xf716, 0x2b54, TYPE_T64, 3, NULL, 0, T64_TWO_RECORDS, NULL }, // Temple of Terror C64 (T64) 1001 CardCruncher New Packer -> 1001 CardCruncher ACM -> Triad-01 -> Mr.Z Packer
    { TEMPLE_OF_TERROR_64, 0x10baa, 0x3b37, TYPE_T64, 4, NULL, 0, T64_TWO_RECORDS, NULL }, // Temple of Terror C64 alt (T64) 1001 CardCruncher New Packer -> 1001 CardCruncher ACM -> Triad-01 -> Mr.Z Packer
    { TOT_TEXT_ONLY_64, 0x10baa, 0x3b37, TYPE_T64, 3, NULL, 0, T64_TWO_RECORDS, NULL }, // Temple of Terror C64 alt (T64) 1001 CardCruncher New Packer -> 1001 CardCruncher ACM -> Triad-01 -> Mr.Z Packer
    { TEMPLE_OF_TERROR_64, 0x2ab00, 0x5720, TYPE_D64, 4, "-e0xc1ef", 2, ONE_FILE, NULL }, // Temple of Terror C64 (D64) ECA Compacker -> Super Compressor / Flexible -> Super Compressor / Equal sequences -> Super Compressor / Equal chars
    { TEMPLE_OF_TERROR_64, 0x2ab00, 0xf3b4, TYPE_D64, 3, NULL, 0, ONE_FILE, NULL, 0x4808, 0xd802, 0x1400 }, // Temple_of_Terror_1987_U.S._Gold_cr_FBR.d64
    { TEMPLE_OF_TERROR_64, 0x2ab00, 0x577e, TYPE_D64, 4, NULL, 0, TWO_FILES_BY_NAME, "TEMPLE O.TERROR2" }, // Temple_of_Terror_1987_U.S._Gold_cr_Triad.d64
    { TOT_TEXT_ONLY_64, 0x2ab00, 0x577e, TYPE_D64, 4, NULL, 0, TWO_FILES_BY_NAME, "TEMPLE O.TERROR1" }, // Temple_of_Terror_1987_U.S._Gold_cr_Triad.d64
    { TEMPLE_OF_TERROR_64, 0x2ab00, 0x7b2d, TYPE_D64, 0, NULL, 0, TWO_FILES_BY_TS, NULL }, // Temple of Terror - Trianon.d64
    { TOT_TEXT_ONLY_64, 0x2ab00, 0x7b2d, TYPE_D64, 0, NULL, 0, TWO_FILES_BY_TS, NULL }, // Temple of Terror - Trianon.d64
    { TEMPLE_OF_TERROR_64, 0x2ab00, 0x4661, TYPE_D64, 0, NULL, 0, TWO_FILES_BY_TS, NULL }, // Temple_of_Terror_1987_U.S._Gold_cr_Trianon.d64
    { TOT_TEXT_ONLY_64, 0x2ab00, 0x4661, TYPE_D64, 0, NULL, 0, TWO_FILES_BY_TS, NULL }, // Temple_of_Terror_1987_U.S._Gold_cr_Trianon.d64
    { TEMPLE_OF_TERROR_64, 0x2ab00, 0x3ee3, TYPE_D64, 6, "-e0xc1ef", 4, ONE_FILE, NULL }, // Temple_of_Terror_Graphic_Version_1987_U.S._Gold_cr_FLT.d64
    { TEMPLE_OF_TERROR_64, 0x2ab00, 0x5b97, TYPE_D64, 4, "-e0xc1ef", 2, ONE_FILE, NULL }, // Temple_of_Terror_1987_U.S._Gold_h_FLT.d64
    { TEMPLE_OF_TERROR_64, 0x2ab00, 0x55a1, TYPE_D64, 4, "-e0xc1ef", 2, ONE_FILE, NULL }, // Temple_of_Terror_1987_US_Gold_cr_FLT.d64

    { TEMPLE_OF_TERROR_64, 0x2ab00, 0x7bf2, TYPE_D64, 4, NULL, 0, TWO_FILES_BY_NAME, "TEMPLE GRA/TRIAD" }, // Temple of Terror (c64.com), Triad crack: 1001 CardCruncher New Packer -> ACM -> Triad-01 -> Mr.Z Packer
    { TOT_TEXT_ONLY_64,    0x2ab00, 0x7bf2, TYPE_D64, 3, NULL, 0, TWO_FILES_BY_NAME, "TEMPLE TEX/TRIAD" }, // Temple of Terror (c64.com), text-only file: 1001 CardCruncher ACM -> Triad-01 -> Mr.Z Packer

    { KAYLETH_64, 0x2ab00, 0xc75f, TYPE_D64, 3, NULL, 0, ONE_FILE, NULL }, // Kayleth D64 Super Compressor / Flexible Hack -> Super Compressor / Equal sequences -> Super Compressor / Equal chars
    { KAYLETH_64, 0xb1f2, 0x7757, TYPE_T64, 3, NULL, 0, ONE_FILE, NULL }, // Kayleth T64 Super Compressor / Flexible Hack -> Super Compressor / Equal sequences -> Super Compressor / Equal chars
    { KAYLETH_64, 0x2ab00, 0x2359, TYPE_D64, 3, NULL, 0, ONE_FILE, NULL }, // Compilation_Karate_Chop_Kayleth_Kettle_Knotie_in_Cave_19xx_-.d64
    { KAYLETH_64, 0x2ab00, 0x3a10, TYPE_D64, 4, NULL, 0, ONE_FILE, NULL }, // Kayleth_1987_U.S._Gold_cr_Newstars.d64
    { KAYLETH_64, 0x2ab00, 0xc184, TYPE_D64, 3, NULL, 0, ONE_FILE, NULL }, // Kayleth_1987_U.S._Gold_cr_Triad.d64
    { KAYLETH_64, 0x2ab00, 0x7a59, TYPE_D64, 4, "-e0x9a0", 2, ONE_FILE, NULL }, // Kayleth_1987_U.S._Gold_cr_OkKoral.d64
    { KAYLETH_64, 0x2ab00, 0xbf75, TYPE_D64, 3, NULL, 0, ONE_FILE, NULL }, // Kayleth_1987_U.S._Gold.d64

    { UNKNOWN_GAME, 0, 0, UNKNOWN_FILE_TYPE, 0, NULL, 0, ONE_FILE, NULL }
};

static GameIDType ProcessC64(uint8_t **sf, size_t *extent, c64rec entry);

/* Extract a file from a T64 tape image by record number (1-based).
   Reads the directory entry at offset 32 + 32*filenum to find the data
   offset and size, then returns a buffer with the 2-byte C64 load address
   prepended. Updates *extent to the extracted file size. The last record
   runs to the end of the image rather than to its end address, which some
   T64 writers get wrong. */
static uint8_t *GetFileFromT64(int filenum, int number_of_records, uint8_t **sf, size_t *extent)
{
    /* The directory entry itself must lie inside the image */
    if (filenum < 0 || (size_t)(64 + 32 * filenum) > *extent)
        return NULL;
    uint8_t *file_records = *sf + 32 + 32 * filenum;
    size_t offset = READ_LE_UINT16(file_records + 8);
    int start_addr = READ_LE_UINT16(file_records + 2);
    int end_addr = READ_LE_UINT16(file_records + 4);
    if (offset >= *extent)
        return NULL;
    size_t size;
    if (number_of_records == filenum)
        size = *extent - offset;
    else if (end_addr > start_addr)
        size = end_addr - start_addr;
    else
        return NULL;
    if (size > *extent - offset)
        return NULL;
    uint8_t *file = MemAlloc(size + 2);
    memcpy(file, file_records + 2, 2);
    memcpy(file + 2, *sf + offset, size);
    *extent = size + 2;
    return file;
}

/* The Trianon disks keep the two versions out of the directory: the
   graphics version starts at track 17, sector 1, behind a loader of
   this many bytes, and the text-only version at track 20, sector 16,
   without its first 16 bytes. */
#define TRIANON_GRAPHICS_TRACK 17
#define TRIANON_GRAPHICS_SECTOR 1
#define TRIANON_TEXT_TRACK 20
#define TRIANON_TEXT_SECTOR 16
#define TRIANON_LOADER_SIZE 176
#define TRIANON_MISSING_HEADER 16

/* Where the hybrid splices the pictures of the graphics version into
   the memory layout of the text-only version: { to, from, length }. */
#define HYBRID_TILES_TO 0x16ea
#define HYBRID_TILES_FROM 0x4708
#define HYBRID_TILES_SIZE 0x1400
#define HYBRID_IMAGES_TO 0x8888
#define HYBRID_IMAGES_FROM 0x8048
#define HYBRID_IMAGES_SIZE 0x4123
#define HYBRID_EXTRA_SIZE 0x4124

/* Temple of Terror ships with two versions on the same disk/tape:
   a graphics version and a text-only version with extra prose. The user
   can also pick a hybrid that combines pictures from the graphics version
   with the expanded text from the text-only version. This function
   extracts both files, prompts for a choice, and decompresses. */
static GameIDType terror_menu(uint8_t **sf, size_t *extent, int recindex)
{
    c64rec rec = c64_registry[recindex];
    c64rec textrec = c64_registry[recindex + 1];
    int isdisk = (rec.type == TYPE_D64);
    OpenBottomWindow();
    Display(Bottom, "This %s image contains one version of Temple of Terror with pictures, and one without pictures but slightly more text.\n\nPlease select one:\n1. Graphics version\n2. Text-only version\n3. Use pictures from file 1 and text from file 2", isdisk ? "disk" : "datasette");

    size_t size1 = *extent;
    size_t size2 = *extent;
    uint8_t *file1 = NULL;
    uint8_t *file2 = NULL;
    switch (rec.layout) {
    case TWO_FILES_BY_TS: {
        uint8_t *tempfile1 = di_get_file_at_ts(*sf, *extent, &size1,
            TRIANON_GRAPHICS_TRACK, TRIANON_GRAPHICS_SECTOR);
        if (tempfile1 != NULL && size1 > TRIANON_LOADER_SIZE) {
            size1 -= TRIANON_LOADER_SIZE;
            file1 = MemAlloc(size1);
            memcpy(file1, tempfile1 + TRIANON_LOADER_SIZE, size1);
        }
        free(tempfile1);
        uint8_t *tempfile2 = di_get_file_at_ts(*sf, *extent, &size2,
            TRIANON_TEXT_TRACK, TRIANON_TEXT_SECTOR);
        if (tempfile2 != NULL) {
            file2 = MemCalloc(size2 + TRIANON_MISSING_HEADER);
            memcpy(file2 + TRIANON_MISSING_HEADER, tempfile2, size2);
            size2 += TRIANON_MISSING_HEADER;
        }
        free(tempfile2);
        break;
    }
    case TWO_FILES_BY_NAME:
        file1 = di_get_file_named(*sf, *extent, &size1, rec.filename);
        file2 = di_get_file_named(*sf, *extent, &size2, textrec.filename);
        break;
    case T64_TWO_RECORDS: {
        int number_of_records = READ_LE_UINT16(*sf + 36);
        file1 = GetFileFromT64(1, number_of_records, sf, &size1);
        file2 = GetFileFromT64(2, number_of_records, sf, &size2);
        break;
    }
    default:
        break;
    }
    free(*sf);
    *sf = NULL;

    if (file1 == NULL || file2 == NULL) {
        Fatal("Could not read the two Temple of Terror files from the image");
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
        /* Swap so file1 is always the one we keep */
        uint8_t *temp = file1;
        file1 = file2;
        file2 = temp;
        size1 = size2;
        rec = textrec;
    }
        // fallthrough
    case 1:
        free(file2);
        *sf = file1;
        *extent = size1;
        return ProcessC64(sf, extent, rec);
    case 3: {
        /* Hybrid: decompress both, then splice the graphics data from
           the graphics version into the text-only version's memory layout */
        ProcessC64(&file1, &size1, rec);
        ProcessC64(&file2, &size2, textrec);

        /* Zeroed: the copies below leave gaps, and the draw instruction
           streams of rooms without a picture run into them. */
        if (size1 < HYBRID_IMAGES_FROM + HYBRID_IMAGES_SIZE
            || size2 + HYBRID_EXTRA_SIZE < HYBRID_IMAGES_TO + HYBRID_IMAGES_SIZE) {
            Fatal("The Temple of Terror files are too short to combine");
        }
        uint8_t *final = MemCalloc(size2 + HYBRID_EXTRA_SIZE);
        memcpy(final, file2, size2);
        memcpy(final + HYBRID_TILES_TO, file1 + HYBRID_TILES_FROM, HYBRID_TILES_SIZE);
        memcpy(final + HYBRID_IMAGES_TO, file1 + HYBRID_IMAGES_FROM, HYBRID_IMAGES_SIZE);
        free(file1);
        free(file2);
        *sf = final;
        *extent = size2 + HYBRID_EXTRA_SIZE;

        Game = FindGameByID(TOT_HYBRID_64);

        return TOT_HYBRID_64;
    }
    default:
        fprintf(stderr, "TAYLOR: invalid switch case!\n");
    }
    return UNKNOWN_GAME;
}

/* Identify and extract a TaylorMade game from a C64 disk or tape image.

   Matches the file against the c64_registry by size + checksum. For D64
   images, extracts the largest file; for T64 tapes, extracts the first
   file record. Temple of Terror images with two game versions trigger
   a selection menu. Replaces *sf and *extent with the extracted file
   data and hands off to ProcessC64 for decompression. */
GameIDType DetectC64(uint8_t **sf, size_t *extent)
{
    if (*extent > MAX_LENGTH || *extent < MIN_LENGTH)
        return UNKNOWN_GAME;

    uint16_t chksum = C64Checksum(*sf, *extent);

    for (int i = 0; c64_registry[i].length != 0; i++) {
        c64rec record = c64_registry[i];
        if (*extent == record.length && chksum == record.chk) {
            /* Temple of Terror images with two game files */
            if (record.layout != ONE_FILE)
                return terror_menu(sf, extent, i);
            if (record.type == TYPE_D64) {
                size_t newlength;
                uint8_t *largest_file = di_get_largest_file(*sf, *extent, &newlength);

                if (largest_file) {
                    free(*sf);
                    *sf = largest_file;
                    *extent = newlength;
                } else {
                    Fatal("Could not read file from disk image");
                }

            /* T64 tape image: extract the first file record */
            } else {
                int number_of_records = READ_LE_UINT16(*sf + 36);
                size_t size = *extent;
                uint8_t *first_file = GetFileFromT64(1, number_of_records, sf, &size);
                if (first_file == NULL) {
                    Fatal("Could not read file from tape image");
                }
                free(*sf);
                *sf = first_file;
                *extent = size;
            }
            return ProcessC64(sf, extent, record);
        }
    }
    return UNKNOWN_GAME;
}

/* Decompress the extracted C64 file and look up the game in the master table.

   Runs unp64 for the number of iterations specified in the registry entry
   (applying switches on a specific iteration if configured), then moves
   the picture data into place for the releases the registry says need it.
   Finally, sets the global Game pointer to the matching entry in games[]. */
static GameIDType ProcessC64(uint8_t **sf, size_t *extent, c64rec record)
{
    C64DecompressIterations(sf, extent, record.decompress_iterations,
        record.switches, record.switch_pass);

    /* The two ranges overlap in Rebel Planet, hence memmove. */
    if (record.relocate_size != 0) {
        if (record.relocate_to + record.relocate_size > *extent
            || record.relocate_from + record.relocate_size > *extent) {
            Fatal("The unpacked game file is too short");
        }
        memmove(*sf + record.relocate_to, *sf + record.relocate_from, record.relocate_size);
    }

    /* Look up the full GameInfo entry in the master games[] table */
    Game = FindGameByID(record.id);

    if (!Game || Game->Title == NULL) {
        Fatal("Game not found!");
    }

    return CurrentGame;
}
