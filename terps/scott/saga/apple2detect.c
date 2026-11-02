//
//  apple2detect.c
//  Part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-08-03.
//

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "apple2draw.h"
#include "common_file_utils.h"
#include "companion_search.h"
#include "game_specific.h"
#include "ciderpress.h"
#include "saga.h"
#include "sagagraphics.h"
#include "scott.h"
#include "woz2nib.h"
#include "apple2_vector_draw.h"

#include "apple2detect.h"

/* Size of the Apple II double hi-res pictures as we draw them. */
#define A2_IMAGE_WIDTH 560
#define A2_IMAGE_HEIGHT 320

/* Offset of the game database in the DOS 3.3 data file. */
#define A2_DATABASE_OFFSET 0x135

static const pairrec a2companionlist[][2] = {
    { { "Scott Adams Graphic Adventure 1 - Adventureland v2.0-416 (4am crack) side A.dsk", 79 }, { "Scott Adams Graphic Adventure 1 - Adventureland v2.0-416 (4am crack) side B - boot.dsk", 86 } },

    { { "SAGA #1 - Adventureland v2.0-416 (1982)(Adventure International)(II+)(US)(Side A)[48K].woz", 90 }, { "SAGA #1 - Adventureland v2.0-416 (1982)(Adventure International)(II+)(US)(Side B)(Boot)[48K].woz", 96 } },

    { { "SAGA #2 - Pirate Adventure v2.1-408 (1982)(Adventure International)(II+)(US)(Side A)[48K].woz", 93 }, { "SAGA #2 - Pirate Adventure v2.1-408 (1982)(Adventure International)(II+)(US)(Side B)(Boot)[48K].woz", 99 } },

    { { "Scott Adams Graphic Adventure 2 - Pirate Adventure v2.1-408 (4am crack) side A.dsk", 82 }, { "Scott Adams Graphic Adventure 2 - Pirate Adventure v2.1-408 (4am crack) side B - boot.dsk", 89 } },

    { { "SAGA #3 - Mission Impossible v2.1-306 (1982)(Adventure International)(II+)(US)(Side A)[48K].woz", 95 }, { "SAGA #3 - Mission Impossible v2.1-306 (1982)(Adventure International)(II+)(US)(Side B)(Boot)[48K].woz", 101 } },

    { { "SAGA #4 - Voodoo Castle v2.1-119 (1982)(Adventure International)(II+)(US)(Side A)[48K].woz", 90 }, { "SAGA #4 - Voodoo Castle v2.1-119 (1982)(Adventure International)(II+)(US)(Side B)(Boot)[48K].woz", 96 } },

    { { "Scott Adams Graphic Adventure 5 - The Count v2.1-115 (4am crack) side A.dsk", 75 }, { "Scott Adams Graphic Adventure 5 - The Count v2.1-115 (4am crack) side B - boot.dsk", 82 } },

    { { "SAGA #5 - The Count v2.1-115 (1982)(Adventure International)(II+)(US)(Side A)[48K].woz", 86 }, { "SAGA #5 - The Count v2.1-115 (1982)(Adventure International)(II+)(US)(Side B)(Boot)[48K].woz", 92 } },

    { { "SAGA #6 - Strange Odyssey v2.1-119 (1982)(Adventure International)(II+)(US)(Side A)[48K].woz", 92 }, { "SAGA #6 - Strange Odyssey v2.1-119 (1982)(Adventure International)(II+)(US)(Side B)(Boot)[48K].woz", 98 } },

    { { "Scott Adams Graphic Adventure 6 - Strange Odyssey v2.1-119 (4am crack) side A.dsk", 81 }, { "Scott Adams Graphic Adventure 6 - Strange Odyssey v2.1-119 (4am crack) side B - boot.dsk", 88 } },

    { { "Sorcerer of Claymorgue Castle (1981)(Adventure International)(Disk 1 of 2)[cr Zapman].do", 87 }, { "Sorcerer of Claymorgue Castle (1981)(Adventure International)(Disk 2 of 2)[cr Zapman][unk filesys].do", 100 } },

    { { "Questprobe featuring The Hulk v2.3-126 (1984)(Adventure International)(Disk 1 of 2)[cr Syndicate - Whip].do", 107 }, { "Questprobe featuring The Hulk v2.3-126 (1984)(Adventure International)(Disk 2 of 2)(Data Disk)[cr Syndicate - Whip].do", 118 } },

    { { NULL }, { NULL } }
};

typedef enum {
    GAME_HAS_NO_PICTURES,
    EVERYTHING_WENT_FINE,
    WRONG_DATABASE
} ExtractResult;

/* The disk index gives a picture the number of its room or object, but
   ScottFree numbers some of them otherwise, the same way on every platform:
   the close-ups from 90, and the pictures an object only has in one room
   (DrawGameSpecificOverlays() in saga.c) from 80. */
typedef struct {
    GameIDType game;
    USImageType usage;
    uint8_t slot;
    uint8_t index;
} a2renumber;

static const a2renumber a2renumberlist[] = {
    { COUNT_US, IMG_ROOM, 80, 90 }, // Closeup package
    { COUNT_US, IMG_ROOM, 81, 91 }, // Closeup angry mob
    { COUNT_US, IMG_ROOM_OBJ, 82, 81 }, // Other end of sheet in pit
    { COUNT_US, IMG_ROOM_OBJ, 83, 82 }, // Coat-of-arms at gate
    { COUNT_US, IMG_ROOM_OBJ, 81, 83 }, // Daisies in flower box
    { VOODOO_CASTLE_US, IMG_ROOM, 80, 91 }, // Closeup voodoo doll
    { VOODOO_CASTLE_US, IMG_ROOM, 81, 92 }, // Closeup voodoo book
    { VOODOO_CASTLE_US, IMG_ROOM, 82, 93 }, // Closeup ring
    { VOODOO_CASTLE_US, IMG_ROOM, 83, 94 }, // Closeup ju-ju statue
    { VOODOO_CASTLE_US, IMG_ROOM, 84, 95 }, // Closeup glowing idol
    { VOODOO_CASTLE_US, IMG_ROOM, 85, 96 }, // Closeup chemicals
    { VOODOO_CASTLE_US, IMG_ROOM, 86, 97 }, // Closeup bloody knife
    { VOODOO_CASTLE_US, IMG_ROOM, 87, 90 }, // Closeup broken sword
    { VOODOO_CASTLE_US, IMG_ROOM, 88, 101 }, // Closeup Count Cristo
};

#define kDiskImageSize 143360
#define kSectorSize 256
#define kNumSectPerTrack 16
#define kNumTracks 35

/* The picture disks of the games with bitmap graphics have no DOS catalog,
   but an index of their own in track 0, sectors 1 to 6: three sectors for the
   room pictures and three for the object pictures, each a table of 256 bytes
   with one entry per room or object number. */
#define A2_INDEX_OFFSET kSectorSize
#define A2_INDEX_TABLES 6
#define A2_INDEX_ENTRIES 256

/* The largest picture is 40 bytes by 160 lines. */
#define A2_MAX_PICTURE_RIGHT 40
#define A2_MAX_PICTURE_BOTTOM 192

static uint8_t *GetApple2CompanionFile(size_t *size, int *isnib);
static ExtractResult ExtractImagesFromApple2CompanionFile(uint8_t *data, size_t datasize, uint8_t *otherdata, size_t othersize, int isnib);
static int ExtractDrawingDataFromBuffer(uint8_t *data, size_t datasize);

static void InitA2DiskImage(uint8_t *data, size_t datasize, int isnib)
{
    if (isnib)
        InitNibImage(data, datasize);
    else
        InitDskImage(data, datasize);
}

// The inventory image is stored as a room image with index 98
static void SetA2InventoryImage(uint8_t *data, size_t datasize)
{
    if (!USImages)
        USImages = NewImage();
    USImages->index = INVENTORY_IMAGE;
    USImages->systype = SYS_APPLE2;
    USImages->datasize = datasize;
    USImages->imagedata = data;
    USImages->usage = IMG_ROOM;
}

/* The database is not at the same place in every DOS file */
static GameIDType LoadApple2Database(uint8_t *database, size_t length)
{
    static const size_t offsets[] = { 0, 0x3d00, 0x3803 };
    GameIDType result = UNKNOWN_GAME;

    for (size_t i = 0; i < sizeof offsets / sizeof offsets[0] && result == UNKNOWN_GAME; i++) {
        if (i == 0 || length > offsets[i])
            result = LoadBinaryDatabase(database + offsets[i], length - offsets[i], *Game, 0);
    }
    return result;
}

// The Apple 2 Saga vector graphics games (i.e. Adventureland, Pirate
// Adventure, Mission Impossible/Secret Mission and Strange Odyssey,
// sometimes contain a database of a different game version on the data
// disk (or A side.) This may cause problems, at least in Pirate Adventure,
// where the alternative version won't display the ship building
// or the navigation animations. Thus, we reload the correct database
// from the other disk.
static void ReloadDatabaseFromCompanion(uint8_t *companionfile, size_t companionsize, int isnib)
{
    uint8_t *invimg = NULL, *m2 = NULL, *m3 = NULL, *closeups = NULL;
    size_t invimgsiz, m3len = 0;

    FreeDiskImage();
    InitA2DiskImage(companionfile, companionsize, isnib);
    uint8_t *temp = ReadApple2DOSFile(companionfile, &companionsize, &invimg, &invimgsiz, &m2, &m3, &m3len, &closeups);
    if (temp) {
        int finalresult = LoadBinaryDatabase(temp + A2_DATABASE_OFFSET, companionsize - A2_DATABASE_OFFSET, *Game, 0);
        if (!finalresult) {
            Fatal("Bad database");
        }
        free(temp);
    }
    free(invimg);
    free(m2);
    free(m3);
    free(closeups);
}

/* The database is loaded: set up the system and find the pictures.
   Takes ownership of companionfile. */
static void SetUpApple2Images(uint8_t *datafile, uint8_t *companionfile,
    size_t companionsize, int isnib, uint8_t *sf, size_t extent)
{
    CurrentSys = SYS_APPLE2;

    ImageWidth = A2_IMAGE_WIDTH;
    ImageHeight = A2_IMAGE_HEIGHT;

    if (CurrentGame == CLAYMORGUE_US_126) {
        size_t invsize = 0x4f8;
        uint8_t *inv = MemAlloc(invsize);
        memcpy(inv, datafile + 0x7e84, invsize);
        SetA2InventoryImage(inv, invsize);
    }
    if (companionfile != NULL) {
        if (ExtractImagesFromApple2CompanionFile(companionfile, companionsize, sf, extent, isnib) == WRONG_DATABASE)
            ReloadDatabaseFromCompanion(companionfile, companionsize, isnib);
        free(companionfile);
    } else if (CurrentGame == SECRET_MISSION_US) {
        // There are one-disk versions of Secret Mission,
        // so look for image files on the main disk as well
        ExtractImagesFromApple2CompanionFile(sf, extent, sf, extent, isnib);
    } else if (USImages != NULL) {
        free(USImages->imagedata);
        free(USImages);
        USImages = NULL;
    }
}

static int A2PictureIndex(USImageType usage, int slot);

/* The table of close-up pictures of the graphics module: the verb, then
   sixteen items, sixteen room picture slots and sixteen nouns, of which
   the unused ones are zero. The pictures must have been read. */
static void SetA2Closeups(const uint8_t *table)
{
    enum { ENTRIES = 16 };
    const uint8_t *items = table + 1, *slots = items + ENTRIES, *nouns = slots + ENTRIES;

    SetCloseupVerb(table[0], 1);
    for (int i = 0; i < ENTRIES; i++) {
        if (nouns[i] == 0)
            continue;
        int index = A2PictureIndex(IMG_ROOM, slots[i]);
        /* A disk can carry the table of another game. */
        for (USImage *image = USImages; image != NULL; image = image->next) {
            if (image->usage == IMG_ROOM && image->index == index) {
                AddCloseup(items[i], nouns[i], index);
                break;
            }
        }
    }
}

/* The disk with the database may not be the one with the graphics module
   and its table of close-ups (Pirate Adventure has a database on both). */
static uint8_t *ReadA2CloseupsFromCompanion(uint8_t *companionfile, size_t companionsize, int isnib)
{
    uint8_t *invimg = NULL, *m2 = NULL, *m3 = NULL, *closeups = NULL;
    size_t invimgsiz, m3len = 0;

    InitA2DiskImage(companionfile, companionsize, isnib);
    free(ReadApple2DOSFile(companionfile, &companionsize, &invimg, &invimgsiz, &m2, &m3, &m3len, &closeups));
    FreeDiskImage();
    free(invimg);
    free(m2);
    free(m3);
    return closeups;
}

GameIDType DetectApple2(uint8_t **sf, size_t *extent)
{
    if (*extent > MAX_LENGTH || *extent < kDiskImageSize)
        return UNKNOWN_GAME;

    if (is_woz_image(*sf, *extent)) {
        uint8_t *result = woz2nib(*sf, extent);
        if (result) {
            free(*sf);
            *sf = result;
            InitNibImage(*sf, *extent);
        }
    } else {
        InitDskImage(*sf, *extent);
    }

    uint8_t *invimg = NULL, *m2 = NULL, *m3 = NULL, *closeups = NULL;
    size_t invimgsiz, m3len = 0;
    int isnib;

    size_t companionsize;
    uint8_t *companionfile = GetApple2CompanionFile(&companionsize, &isnib);

    /* ReadApple2DOSFile() replaces the size of the disk with that of the
       database, and we need both. */
    size_t disksize = *extent;
    size_t datasize = disksize;
    uint8_t *datafile = ReadApple2DOSFile(*sf, &datasize, &invimg, &invimgsiz, &m2, &m3, &m3len, &closeups);

    if (!datafile && companionfile != NULL) {
        FreeDiskImage();
        InitA2DiskImage(companionfile, companionsize, isnib);
        datasize = companionsize;
        datafile = ReadApple2DOSFile(companionfile, &datasize, &invimg, &invimgsiz, &m2, &m3, &m3len, &closeups);
        if (datafile)
            SwapWithCompanionFile(sf, &disksize, &companionfile, &companionsize);
    }
    if (datafile)
        *extent = datasize;

    FreeDiskImage();

    if (invimg)
        SetA2InventoryImage(invimg, invimgsiz);

    if (m2) {
        descrambletable = m2;
    }

    if (m3) {
        ExtractDrawingDataFromBuffer(m3, m3len);
        free(m3);
    }

    if (!datafile) {
        free(companionfile);
        free(closeups);
        debug_print("Failed loading database\n");
        return UNKNOWN_GAME;
    }

    // Header actually starts at offset 0x16d (0x135 + 0x38).
    // We add 50 bytes before the head to match the C64 files.
    size_t data_start = A2_DATABASE_OFFSET;

    if (*extent > MAX_LENGTH || *extent < data_start) {
        free(datafile);
        free(companionfile);
        return UNKNOWN_GAME;
    }

    size_t newlength = *extent - data_start;
    uint8_t *database = MemAlloc(newlength);
    memcpy(database, datafile + data_start, newlength);

    GameIDType result = LoadApple2Database(database, newlength);

    if (result != UNKNOWN_GAME) {
        if (closeups == NULL && companionfile != NULL)
            closeups = ReadA2CloseupsFromCompanion(companionfile, companionsize, isnib);
        SetUpApple2Images(datafile, companionfile, companionsize, isnib, *sf, disksize);
        if (closeups) {
            SetA2Closeups(closeups);
            AddExtraCloseups(result);
        }
    } else {
        free(companionfile);
        debug_print("Failed loading database\n");
    }
    free(closeups);
    free(datafile);
    free(database);
    return result;
}

static uint8_t *ReadA2DiskImageFile(const char *filename, size_t *filesize, int *isnib)
{
    uint8_t *result = ReadFileIfExists(filename, filesize);
    if (result && is_woz_image(result, *filesize)) {
        uint8_t *result2 = woz2nib(result, filesize);
        if (result2) {
            free(result);
            result = result2;
            *isnib = 1;
        }
    }
    return result;
}

static uint8_t *A2CompanionReader(const char *filename, size_t *filesize, void *user)
{
    return ReadA2DiskImageFile(filename, filesize, (int *)user);
}

// If side A is not found, try stripping any parenthesis before the extension,
// mainly to remove "(boot)". If side B is not found, try adding " (boot)".
static uint8_t *A2CompanionFallback(char *buffer, size_t bufsize, size_t namelen, int index, CompanionNameType type, CompanionReader read, void *user, size_t *size)
{
    (void)index;
    int found = 0;
    if (type == TYPE_A)
        found = CompanionStripEnclosed(buffer, namelen, '(', ')', 1);
    else if (type == TYPE_B)
        found = CompanionInsertBeforeExtension(buffer, bufsize, namelen, " (boot)");
    if (!found)
        return NULL;
    debug_print("looking for companion file \"%s\"\n", buffer);
    return read(buffer, size, user);
}

static uint8_t *GetApple2CompanionFile(size_t *size, int *isnib)
{
    *size = 0;
    *isnib = 0;

    size_t gamefilelen = strlen(game_file);
    uint8_t *result = NULL;

    char *foundname = LookInDatabase(a2companionlist, game_file, gamefilelen);
    if (foundname) {
        size_t filesize;
        result = ReadA2DiskImageFile(foundname, &filesize, isnib);
        free(foundname);
        if (result) {
            *size = filesize;
            return result;
        }
    }

    const CompanionSearch search = {
        .read = A2CompanionReader,
        .user = isnib,
        .fallback = A2CompanionFallback,
        .match_char_before_extension = 0,
    };
    return FindCompanionFile(game_file, &search, size);
}

static int ExtractAtAddress(uint8_t *data, size_t datasize, uint16_t addr, size_t len, void (*setter)(const uint8_t *)) {
    if (datasize <= 4)
        return 0;
    uint16_t load_addr = data[0] | (data[1] << 8);
    uint16_t file_len = data[2] | (data[3] << 8);
    if (load_addr > addr || (uint32_t)load_addr + file_len < addr + len)
        return 0;
    size_t offset = 4 + (addr - load_addr);
    if (offset + len > datasize)
        return 0;
    setter(data + offset);
    return 1;
}

static int ExtractDrawingDataFromBuffer(uint8_t *data, size_t datasize) {
    int found = 0;
    found |= ExtractAtAddress(data, datasize, 0x8F7C, 216, SetColorPatternSubindices);
    found |= ExtractAtAddress(data, datasize, 0x9054, 120, SetPatternData);
    found |= ExtractAtAddress(data, datasize, 0x9500, 256, SetBrushBitmaps);
    return found;
}

static int ReadA2VectorImageFiles(uint8_t *data, size_t datasize, USImage *image) {
    size_t number_of_files;
    size_t number_of_images_found = 0;
    A2FileRec *rec = GetAllApple2DOSFiles(data, datasize, &number_of_files);

    for (size_t i = 0; i < number_of_files; i++) {
        A2FileRec *thisrec = &rec[i];
        char *shortname = thisrec->filename;
        if (!IsSagaImage(shortname)) {
            if (!strncmp(shortname, "M3", 2))
                ExtractDrawingDataFromBuffer(thisrec->data, thisrec->datasize);
            free(thisrec->data);
            free(thisrec->filename);
            continue;
        }
        size_t namelen = strlen(shortname);
        if (shortname[0] == 'R') {
            image->usage = IMG_ROOM;
            if (namelen > 4) {
                image->index = (shortname[3] - '0') * 10 + shortname[4] - '0';
            }
        } else if(shortname[0] == 'B') {
            if (namelen > 5) {
                image->index = (shortname[3] - '0') * 100 + (shortname[4] - '0') * 10 + shortname[5] - '0';
            }
            // Mission Impossible uses index 225
            // for its title image, while the other
            // games use 255, so we just fudge it to
            // be consistent.
            if (image->index == 225)
                image->index = 255;
            // The games with vector graphics use
            // the same images for both inventory
            // and room objects
            image->usage = IMG_INV_AND_ROOM_OBJ;
        } else {
            continue;
        }
        if (image->index == -1) {
            continue;
        }
        image->filename = thisrec->filename;
        image->systype = SYS_APPLE2_LINES;
        image->datasize = thisrec->datasize;
        image->imagedata = thisrec->data;
        number_of_images_found++;
        if (i < number_of_files - 1) {
            image = AppendImage(image);
            image->filename = NULL;
        }
    }
    free(rec);
    return number_of_images_found;
}

int LoadDrawingDataFromDisk(uint8_t *data, size_t datasize) {
    size_t number_of_files;
    A2FileRec *rec = GetAllApple2DOSFiles(data, datasize, &number_of_files);
    int found = 0;
    for (size_t i = 0; i < number_of_files; i++) {
        if (!IsSagaImage(rec[i].filename))
            found |= ExtractDrawingDataFromBuffer(rec[i].data, rec[i].datasize);
        free(rec[i].data);
        free(rec[i].filename);
    }
    free(rec);
    return found;
}

/* Read size bytes at offset of a disk in DOS 3.3 sector order. */
static uint8_t *ReadA2Sectors(uint8_t *data, size_t datasize, int isnib, size_t offset, size_t size)
{
    if (isnib) {
        if (offset > kDiskImageSize || size > kDiskImageSize - offset)
            return NULL;
        return ReadImageFromNib(offset, size, data, datasize);
    }
    if (offset > datasize || size > datasize - offset)
        return NULL;
    uint8_t *result = MemAlloc(size);
    memcpy(result, data + offset, size);
    return result;
}

static int A2PictureIndex(USImageType usage, int slot)
{
    for (size_t i = 0; i < sizeof a2renumberlist / sizeof a2renumberlist[0]; i++) {
        const a2renumber *entry = &a2renumberlist[i];
        if (entry->game == CurrentGame && entry->usage == usage && entry->slot == slot)
            return entry->index;
    }
    return slot;
}

/* One entry of the disk index, which is made of the track and the sector of
   the last sector of the picture, and its number of sectors: the original
   reads them backwards, and they are consecutive in DOS 3.3 sector order.
   Bit 7 of the track means there is no picture. Bit 7 of the sector of an
   object picture means that it is the one for the inventory screen, and
   otherwise it is the one for the object lying in a room.
   Returns 0 for an entry that cannot be right. */
static int ReadA2IndexEntry(const uint8_t *tables, int slot, int *inventory, size_t *offset, size_t *size)
{
    uint8_t track = tables[slot];
    uint8_t sector = tables[A2_INDEX_ENTRIES + slot];
    uint8_t count = tables[2 * A2_INDEX_ENTRIES + slot];

    *size = 0;
    if (track & 0x80)
        return 1;

    *inventory = (sector & 0x80) != 0;
    sector &= 0x7f;

    size_t last = (size_t)track * kNumSectPerTrack + sector;
    size_t first_picture_sector = (A2_INDEX_OFFSET / kSectorSize) + A2_INDEX_TABLES;
    if (track >= kNumTracks || sector >= kNumSectPerTrack || count == 0 || last + 1 < first_picture_sector + count)
        return 0;

    *offset = (last + 1 - count) * kSectorSize;
    *size = (size_t)count * kSectorSize;
    return 1;
}

/* A picture begins with its left, top, right and bottom edges. There are
   entries in the index of some disks that lead to sectors of filler. */
static int IsA2Picture(const uint8_t *data)
{
    return data[0] <= data[2] && data[2] > 0 && data[2] <= A2_MAX_PICTURE_RIGHT
        && data[1] <= data[3] && data[3] > 0 && data[3] <= A2_MAX_PICTURE_BOTTOM;
}

/* Read the pictures that the disk index lists. On a disk which is not a
   picture disk, such as the boot disk, the index sectors hold something else
   and most entries are out of range: then there are no pictures at all. */
static void ReadA2BitmapImages(uint8_t *data, size_t datasize, int isnib, USImage *image)
{
    size_t indexsize = A2_INDEX_TABLES * A2_INDEX_ENTRIES;
    uint8_t *index = ReadA2Sectors(data, datasize, isnib, A2_INDEX_OFFSET, indexsize);
    if (index == NULL)
        return;

    int inventory = 0;
    size_t offset, size;

    for (int table = 0; table < 2; table++) {
        for (int slot = 0; slot < A2_INDEX_ENTRIES; slot++) {
            if (!ReadA2IndexEntry(index + table * 3 * A2_INDEX_ENTRIES, slot, &inventory, &offset, &size)) {
                debug_print("No picture index on this disk\n");
                free(index);
                return;
            }
        }
    }

    for (int table = 0; table < 2; table++) {
        for (int slot = 0; slot < A2_INDEX_ENTRIES; slot++) {
            ReadA2IndexEntry(index + table * 3 * A2_INDEX_ENTRIES, slot, &inventory, &offset, &size);
            if (size == 0)
                continue;

            uint8_t *picture = ReadA2Sectors(data, datasize, isnib, offset, size);
            if (picture == NULL || !IsA2Picture(picture)) {
                free(picture);
                continue;
            }

            if (table == 0)
                image->usage = IMG_ROOM;
            else
                image->usage = inventory ? IMG_INV_OBJ : IMG_ROOM_OBJ;
            image->index = A2PictureIndex(image->usage, slot);
            image->datasize = size;
            image->systype = SYS_APPLE2;
            image->imagedata = picture;

            debug_print("Read image with size %zu, usage %d and index %d\n", size, image->usage, image->index);

            image = AppendImage(image);
        }
    }

    free(index);
}

static ExtractResult ExtractImagesFromApple2CompanionFile(uint8_t *data, size_t datasize, uint8_t *otherdata, size_t othersize, int isnib)
{
    int has_file_system;

    switch (CurrentGame) {
    // The picture disks of these have an index of
    // their own instead of a file system.
    case CLAYMORGUE_US:
    case CLAYMORGUE_US_126:
    case HULK_US:
    case HULK_US_PREL:
    case COUNT_US:
    case VOODOO_CASTLE_US:
        has_file_system = 0;
        break;
    // These all have proper file systems
    // with each image in a separate file.
    case ADVENTURELAND_US:
    case PIRATE_US:
    case SECRET_MISSION_US:
    case STRANGE_ODYSSEY_US:
        has_file_system = 1;
        break;
    default:
        return GAME_HAS_NO_PICTURES;
    }

    USImage *image = NewImage();
    if (!USImages) {
        USImages = image;
    } else {
        USImages->next = image;
    }

    if (isnib) {
        InitNibImage(data, datasize);
    }

    ExtractResult result = EVERYTHING_WENT_FINE;

    // The games with line-drawn vector graphics
    // all have a file system on their companion disk
    if (has_file_system) {
        int number_of_images_found = ReadA2VectorImageFiles(data, datasize, image);
        if (number_of_images_found == 0 && data != otherdata) {
            // The vector image versions have a game database
            // file on both disks, so we can't use that
            // to determine which disk is which. We'll just check
            // for image files on both disks instead.
            FreeDiskImage();
            InitA2DiskImage(otherdata, othersize, isnib);
            number_of_images_found = ReadA2VectorImageFiles(otherdata, othersize, image);
            if (number_of_images_found > 0 && data != otherdata) {
                result = WRONG_DATABASE;
            }
        }
        if (number_of_images_found > 0) {
            ImageWidth = A2_IMAGE_WIDTH;
            ImageHeight = A2_IMAGE_HEIGHT;
        }
    } else {
        ReadA2BitmapImages(data, datasize, isnib, image);
    }

    FreeDiskImage();

    FreeUSImagesIfEmpty();

    if (CurrentGame == HULK_US_PREL)
        CurrentGame = HULK_US;
    else if (CurrentGame == CLAYMORGUE_US_126)
        CurrentGame = CLAYMORGUE_US;
    return result;
}
