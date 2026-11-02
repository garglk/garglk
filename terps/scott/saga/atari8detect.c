//
//  atari8detect.c
//  Part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-01-30.
//

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "atari8detect.h"
#include "common_file_utils.h"
#include "companion_search.h"
#include "game_specific.h"
#include "saga.h"
#include "sagagraphics.h"
#include "scott.h"
#include "scott_game_info.h"


static const pairrec a8companionlist[][2] = {
    { { "S.A.G.A. 01 - Adventureland v5.0-416 (1982)(Adventure International)(US)(Side A)[!].atr", 87 }, { "S.A.G.A. 01 - Adventureland v5.0-416 (1982)(Adventure International)(US)(Side B)[!][cr CSS].atr", 95 } },
    { { "S.A.G.A. 02 - Pirate Adventure v5.0-408 (1982)(Adventure International)(US)(Side A)[f][m].atr", 93 }, { "S.A.G.A. 02 - Pirate Adventure v5.0-408 (1982)(Adventure International)(US)(Side B)[cr CSS].atr", 95 } },
    { { "S.A.G.A. 02 - Pirate Adventure v5.0-408 (1982)(Adventure International)(US)(Side A)[f][a].atr", 93 }, { "S.A.G.A. 02 - Pirate Adventure v5.0-408 (1982)(Adventure International)(US)(Side B)[a][cr CSS].atr", 98 } },
    { { "S.A.G.A. 04 - Voodoo Castle v5.1-119 (1983)(Adventure International)(US)(Disk 1 of 2)[!].atr", 92 }, { "S.A.G.A. 04 - Voodoo Castle v5.1-119 (1983)(Adventure International)(US)(Disk 2 of 2)[!][cr CSS].atr", 100 } },
    { { "S.A.G.A. 05 - The Count v5.1-115 (1983)(Adventure International)(US)(Side A)[!].atr", 83 }, { "S.A.G.A. 05 - The Count v5.1-115 (1983)(Adventure International)(US)(Side B)[!][cr CSS].atr", 91 } },
    { { "S.A.G.A. 13 - The Sorcerer of Claymorgue Castle v5.1-125 (1983)(Adventure International)(US)(Disk 1 of 2)[!][cr CSS].atr", 120 }, { "S.A.G.A. 13 - The Sorcerer of Claymorgue Castle v5.1-125 (1983)(Adventure International)(US)(Disk 2 of 2)[f][!].atr", 115 } },
    { { "S.A.G.A. 13 - The Sorcerer of Claymorgue Castle v5.1-125 (1983)(Adventure International)(US)(Disk 1 of 2)[a][cr CSS].atr", 120 }, { "S.A.G.A. 13 - The Sorcerer of Claymorgue Castle v5.1-125 (1983)(Adventure International)(US)(Disk 2 of 2)[a].atr", 112 } },
    { { "S.A.G.A. #13 - The Sorcerer of Claymorgue Castle v5.1-125 (1983)(Adventure International)(US)(Disk 1 of 2)[a][cr CSS].atr", 121 }, { "S.A.G.A. #13 - The Sorcerer of Claymorgue Castle v5.1-125 (1983)(Adventure International)(US)(Disk 2 of 2)[a].atr", 113 } },

    { { NULL }, { NULL } }
};

/* The original interpreter finds a picture through a table of two bytes for
   each of the rooms 0 to 99 followed by each of the objects 0 to 255, where
   object 250 is a colour test and 255 the title picture of the games with
   line drawings. The second byte and the two low bits of the first are the
   number of the disk sector where the picture starts, counted from one, and
   two zero bytes mean that there is no picture.

   In the games with line drawings the table is on the picture disk, and
   the six high bits of the first byte give the position in the sector in
   units of three bytes. In the games with bitmap pictures it is on the boot
   disk, the five high bits give the position in units of seven bytes, and
   bit 2 is set if the picture shows the object when carried rather than in
   the room.

   The table is preceded by the number of the adventure, three times. */
enum {
    A8_HEADER_SIZE = 16,
    A8_SECTOR_SIZE = 128,
    A8_ROOM_SLOTS = 100,
    A8_SLOTS = A8_ROOM_SLOTS + 256,
    A8_COLOR_TEST = 250,
    A8_LINES_TABLE = 0x293,
    A8_BITMAP_TABLE = 0x9593,
};

typedef struct {
    GameIDType game;
    USImageType usage;
    uint8_t slot;
    uint8_t index;
} a8renumber;

/* ScottFree numbers some pictures otherwise than the disk does. See
   a2renumberlist in apple2detect.c. */
static const a8renumber a8renumberlist[] = {
    { COUNT_US, IMG_ROOM, 80, 90 }, // Closeup package
    { COUNT_US, IMG_ROOM, 81, 91 }, // Closeup angry mob
    { COUNT_US, IMG_ROOM_OBJ, 82, 81 }, // Other end of sheet in pit
    { COUNT_US, IMG_ROOM_OBJ, 83, 82 }, // Coat-of-arms at gate
    { COUNT_US, IMG_ROOM_OBJ, 84, 63 }, // Dumb-waiter
    { VOODOO_CASTLE_US, IMG_ROOM, 81, 90 }, // Closeup broken sword
    { VOODOO_CASTLE_US, IMG_ROOM, 82, 96 }, // Closeup chemicals
    { VOODOO_CASTLE_US, IMG_ROOM, 83, 101 }, // Closeup Count Cristo
    { VOODOO_CASTLE_US, IMG_ROOM, 84, 100 }, // Closeup dusty idol
    { VOODOO_CASTLE_US, IMG_ROOM_OBJ, 70, 80 }, // Wooden boards unnailed
};

static int A8Renumber(USImageType usage, int slot)
{
    for (size_t i = 0; i < sizeof a8renumberlist / sizeof a8renumberlist[0]; i++) {
        const a8renumber *entry = &a8renumberlist[i];
        if (entry->game == CurrentGame && entry->usage == usage && entry->slot == slot)
            return entry->index;
    }
    return slot;
}

/* The title picture of Mission Impossible follows the colour test on the
   disk, but is not in the table, as the original does not show it. */
#define A8_MISSION_TITLE 0x8082

static const uint8_t *FindA8PictureTable(const uint8_t *disk, size_t size, size_t offset)
{
    if (disk == NULL || size < offset + A8_SLOTS * 2)
        return NULL;
    const uint8_t *table = disk + offset;
    if (table[-1] == 0 || table[-1] != table[-2] || table[-1] != table[-3])
        return NULL;
    return table;
}

/* Two sectors of the table are filled with something else on the boot disk
   of Voodoo Castle, so we check that there is a picture where an entry says.
   The header is the size of the picture and its left, top, right and bottom
   edges, the left and right ones in bytes. */
static int IsA8Bitmap(const uint8_t *data, size_t datasize, size_t offset)
{
    if (offset < 2 || offset + 6 > datasize)
        return 0;
    size_t size = READ_LE_UINT16(data + offset);
    const uint8_t *edges = data + offset + 2;
    return (size >= 8 && offset + 2 + size <= datasize && edges[0] < edges[2] && edges[2] <= 40 && edges[1] < edges[3] && edges[3] <= 192);
}

static int CompareImgOffsets(const void *a, const void *b)
{
    size_t first = ((const imglist *)a)->offset, second = ((const imglist *)b)->offset;
    return (first > second) - (first < second);
}

/* Returns a list of the pictures on the disk in the order they are stored,
   which ends with an entry with offset 0 and must be freed by the caller.
   The offset is that of the size of a bitmap picture and of the start of a
   line drawing. Returns NULL if there is no table. */
static imglist *ReadA8PictureTable(const uint8_t *data, size_t datasize, const uint8_t *otherdisk, size_t othersize, int is_vector)
{
    const uint8_t *table;

    if (is_vector) {
        table = FindA8PictureTable(data, datasize, A8_LINES_TABLE);
    } else {
        table = FindA8PictureTable(otherdisk, othersize, A8_BITMAP_TABLE);
        if (table == NULL)
            table = FindA8PictureTable(data, datasize, A8_BITMAP_TABLE);
    }
    if (table == NULL)
        return NULL;

    imglist *list = MemAlloc((A8_SLOTS + 2) * sizeof *list);
    int count = 0;

    for (int slot = 0; slot < A8_SLOTS; slot++) {
        uint8_t high = table[slot * 2];
        size_t sector = (high & 3) << 8 | table[slot * 2 + 1];
        if (sector == 0)
            continue;
        size_t offset = A8_HEADER_SIZE + (sector - 1) * A8_SECTOR_SIZE;
        offset += is_vector ? (high >> 2) * 3 : (high >> 3) * 7;

        int index = slot;
        USImageType usage = IMG_ROOM;
        if (slot >= A8_ROOM_SLOTS) {
            index = slot - A8_ROOM_SLOTS;
            if (index == A8_COLOR_TEST)
                ; /* Not an object */
            else if (is_vector)
                usage = IMG_INV_AND_ROOM_OBJ;
            else
                usage = (high & 4) ? IMG_INV_OBJ : IMG_ROOM_OBJ;
        }

        if (is_vector) {
            /* The same two sectors are bad on the Strange Odyssey disk */
            if (offset >= datasize || (slot >= A8_ROOM_SLOTS && index > GameHeader.NumItems && index < A8_COLOR_TEST))
                continue;
        } else if (!IsA8Bitmap(data, datasize, offset)) {
            continue;
        }

        list[count].usage = usage;
        list[count].index = A8Renumber(usage, index);
        list[count].offset = offset;
        count++;
    }

    if (CurrentGame == SECRET_MISSION_US && A8_MISSION_TITLE < datasize) {
        list[count].usage = IMG_INV_AND_ROOM_OBJ;
        list[count].index = 255;
        list[count].offset = A8_MISSION_TITLE;
        count++;
    }

    qsort(list, count, sizeof *list, CompareImgOffsets);
    list[count].usage = 0;
    list[count].index = 0;
    list[count].offset = 0;
    return list;
}

static const CropList a8croplist[] = {
    { VOODOO_CASTLE_US, IMG_ROOM, 10, 8, 8 },
    { VOODOO_CASTLE_US, IMG_ROOM, 11, 8, 8 },
    { VOODOO_CASTLE_US, IMG_ROOM, 14, 8, 8 },
    { VOODOO_CASTLE_US, IMG_ROOM, 19, 8, 8 },
    { VOODOO_CASTLE_US, IMG_ROOM, 20, 8, 8 },
    { HULK_US, IMG_ROOM, 0, 0, 32 },
    { COUNT_US, IMG_ROOM, 0, 8, 0 },

    { 0, 0, 0, 0, 0 }
};

static uint8_t *GetAtari8CompanionFile(size_t *size)
{

    size_t gamefilelen = strlen(game_file);
    char *foundname = LookInDatabase(a8companionlist, game_file, gamefilelen);
    uint8_t *result = NULL;
    if (foundname) {
        result = ReadFileIfExists(foundname, size);
        free((void *)foundname);
        if (result)
            return result;
    }

    const CompanionSearch search = {
        .read = NULL,
        .user = NULL,
        .fallback = CompanionBracketFallback,
        .match_char_before_extension = 0,
    };
    return FindCompanionFile(game_file, &search, size);
}

/* Many images have black bars on one or more sides,
   probably used to center smaller images. The original
   interpreters had a black background which hid this,
   but it looks strange if the background is not black,
   so we crop them off. */
static void SetAtari8ImageCrop(USImage *image)
{
    if (CurrentGame == VOODOO_CASTLE_US && image->usage == IMG_ROOM)
        image->cropleft = 8;

    if ((CurrentGame == CLAYMORGUE_US || CurrentGame == COUNT_US) && image->usage == IMG_ROOM && image->imagedata[7] == 126)
        image->cropright = 8;

    for (int i = 0; a8croplist[i].game != 0; i++) {
        CropList crop = a8croplist[i];
        if (crop.game == CurrentGame && crop.index == image->index && crop.usage == image->usage) {
            image->cropleft = crop.cropleft;
            image->cropright = crop.cropright;
        }
    }
}

static int ExtractImagesFromAtariCompanionFile(uint8_t *data, size_t datasize, uint8_t *otherdisk, size_t othersize)
{
    size_t size;

    int outpic;

    int is_vector = (CurrentGame == ADVENTURELAND_US || CurrentGame == PIRATE_US || CurrentGame == SECRET_MISSION_US || CurrentGame == STRANGE_ODYSSEY_US);

    switch (CurrentGame) {
    case ADVENTURELAND_US:
    case PIRATE_US:
    case SECRET_MISSION_US:
    case STRANGE_ODYSSEY_US:
    case CLAYMORGUE_US:
    case COUNT_US:
    case VOODOO_CASTLE_US:
    case HULK_US:
        break;
    default:
        return 0;
    }

    imglist *list = ReadA8PictureTable(data, datasize, otherdisk, othersize, is_vector);
    if (list == NULL)
        return 0;

    if (is_vector) {
        ImageWidth = 320;
        ImageHeight = 96;
    }

    USImages = NewImage();
    USImage *image = USImages;

    // Now loop round for each image
    for (outpic = 0; list[outpic].offset != 0; outpic++) {

        size_t offset = list[outpic].offset - 2;

        size = READ_LE_UINT16(data + list[outpic].offset) + 2;
        image->systype = SYS_ATARI8;

        if (is_vector) {
            offset += 2;
            size_t nextoff = list[outpic + 1].offset;
            if (nextoff > 0) {
                size = nextoff - offset;
            } else {
                size = datasize - offset;
            }
            image->systype = SYS_ATARI8_LINES;
        }

        image->usage = list[outpic].usage;
        image->index = list[outpic].index;
        image->imagedata = MemAlloc(size);
        image->datasize = size;

        memcpy(image->imagedata, data + offset, size);
        /* Bytes 0xb390 to 0xb410 correspond to the content of
           sector 360 (0x0168) of the original Atari disk which
           contains the volume table of contents (mostly a bitmap
           of used sectors). This is not part of the graphics data,
           so we cut it out. */
        if (list[outpic].offset < 0xb390 && list[outpic].offset + image->datasize > 0xb390) {
            if (is_vector) {
                if (CurrentGame != SECRET_MISSION_US) {
                    memcpy(image->imagedata + 0xb390 - list[outpic].offset, data + 0xb410, size - 0xb390 + offset);
                }
            } else {
                memcpy(image->imagedata + 0xb390 - list[outpic].offset + 2, data + 0xb410, size - 0xb390 + offset);
            }
        }

        if (CurrentGame == STRANGE_ODYSSEY_US && image->index == 24 && image->usage == IMG_INV_AND_ROOM_OBJ && image->datasize > 3 && image->imagedata[3] == 0xA0) {
            image->imagedata[3] = 0x80;
        }

        SetAtari8ImageCrop(image);

        image = AppendImage(image);
    }

    free(list);

    /* Read the inventory image from the boot disk, where it follows the
       table of pictures */
    if (otherdisk && othersize > 0x988e + 0x5fd && CurrentGame != ADVENTURELAND_US) {
        image->usage = IMG_ROOM;
        image->index = INVENTORY_IMAGE;
        image->datasize = 0x5fd;
        image->imagedata = MemAlloc(image->datasize);
        image->systype = SYS_ATARI8;
        memcpy(image->imagedata, otherdisk + 0x988e, image->datasize);
    } else if (image->imagedata == NULL) {
        FreeTrailingEmptyImage(image);
    }

    /* If no images are found, free USImages struct */
    FreeUSImagesIfEmpty();

    return 1;
}

/* The original interpreter has the same table of close-ups as the Apple II
   one (see SetA2Closeups() in apple2detect.c), just before its "<HIT RETURN>"
   prompt in inverse video and after a jump past it. Returns NULL if the
   disk does not have it. */
static const uint8_t *FindA8Closeups(const uint8_t *data, size_t size)
{
    static const uint8_t prompt[] = { 0x0d, 0xbc, 0xc8, 0xc9, 0xd4, 0xa0, 0xd2, 0xc5, 0xd4, 0xd5, 0xd2, 0xce, 0xbe, 0x0d };
    enum { TABLE = 1 + 3 * 16, JMP = 0x4c };

    for (size_t i = TABLE + 3; data != NULL && i + sizeof prompt <= size; i++) {
        if (data[i] == prompt[0] && data[i - TABLE - 3] == JMP && memcmp(data + i, prompt, sizeof prompt) == 0)
            return data + i - TABLE;
    }
    return NULL;
}

/* The picture slots of the table are those of the image lists above, except
   in the two games whose close-ups we number as their Apple II ones. */
/* The pictures must have been read. */
static void SetA8Closeups(const uint8_t *table)
{
    enum { ENTRIES = 16 };
    const uint8_t *items = table + 1, *slots = items + ENTRIES, *nouns = slots + ENTRIES;

    SetCloseupVerb(table[0], 1);
    for (int i = 0; i < ENTRIES; i++) {
        if (nouns[i] == 0)
            continue;
        int index = A8Renumber(IMG_ROOM, slots[i]);
        for (USImage *image = USImages; image != NULL; image = image->next) {
            if (image->usage == IMG_ROOM && image->index == index) {
                AddCloseup(items[i], nouns[i], index);
                break;
            }
        }
    }
    AddExtraCloseups(CurrentGame);
}

// The only available source of Mission Impossible/Secret Mission
// has an error at offset 0xfac1, but the correct data is available
// at another offset on the disk image, so we copy it to the right
// place.
static void PatchMissionImpossible(uint8_t *data, size_t size) {
    if (size > 0xfaf1 && memcmp(data + 0x7d3, "briefing", 8) == 0) {
        uint8_t source[0x30];
        memcpy(source, data + 0xfac1, 0x30);
        if (memcmp(source, "\x0ahigh above the reactor core\x12maintenance room 2\x12", 0x30) == 0) {
            memcpy(data + 0x914, source, 0x30);
        }
    }
}

static const uint8_t atrheader[6] = { 0x96, 0x02, 0x80, 0x16, 0x80, 0x00 };
static const uint8_t atrheader_old[6] = { 0x96, 0x02, 0x40, 0x0a, 0x80, 0x00 };

GameIDType DetectAtari8(uint8_t **sf, size_t *extent)
{
    GameIDType result = UNKNOWN_GAME;
    size_t data_start = 0x04c1;
    // Header actually starts at offset 0x04f9 (0x04c1 + 0x38).
    // We add 50 bytes at the head to match the C64 files.

    if (*extent > MAX_LENGTH || *extent < data_start)
        return UNKNOWN_GAME;

    if (memcmp(*sf, atrheader, sizeof atrheader) != 0) {
        // Check for old version
        if (memcmp(*sf, atrheader_old, sizeof atrheader_old) != 0)
            return UNKNOWN_GAME;
        data_start = 0x040c;
    }

    size_t companionsize;
    uint8_t *companionfile = GetAtari8CompanionFile(&companionsize);

    PatchMissionImpossible(*sf, *extent);

    ImageWidth = 280;
    ImageHeight = 158;
    result = LoadBinaryDatabase(*sf + data_start, *extent - data_start, *Game, 0);
    if (result == UNKNOWN_GAME && companionfile != NULL && companionsize > data_start) {
        debug_print("Could not find database in this file, trying the companion file\n");
        PatchMissionImpossible(companionfile, companionsize);
        result = LoadBinaryDatabase(companionfile + data_start, companionsize - data_start, *Game, 0);
        if (result != UNKNOWN_GAME) {
            debug_print("Found database in companion file. Switching files.\n");
            SwapWithCompanionFile(sf, extent, &companionfile, &companionsize);
        }
    }

    if (result != UNKNOWN_GAME) {
        CurrentSys = SYS_ATARI8;
        if (companionfile) {
            ExtractImagesFromAtariCompanionFile(companionfile, companionsize, *sf, *extent);
            const uint8_t *closeups = FindA8Closeups(*sf, *extent);
            if (closeups == NULL)
                closeups = FindA8Closeups(companionfile, companionsize);
            if (closeups != NULL)
                SetA8Closeups(closeups);
            free(companionfile);
        }
    } else {
        free(companionfile);
        debug_print("Failed loading database\n");
    }

    return result;
}
