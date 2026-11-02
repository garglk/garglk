//
//  apple2detect.c
//  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
//
//  Created by Petter Sjölund on 2022-08-03.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "companionfile.h"
#include "definitions.h"
#include "read_le16.h"
#include "graphics.h"

#include "woz2nib.h"
#include "ciderpress.h"

#include "apple2detect.h"

/* Size of a 35 track, 16 sector .dsk image */
#define A2_DISK_SIZE (35 * 16 * 256)
#define A2_SECTOR_SIZE 256

/* The game database begins at this offset of the sector-reversed disk
   image, and the size of its actions is found at A2_ACTION_SIZE_OFFSET. */
#define A2_DATABASE_OFFSET 125438
#define A2_ACTION_SIZE_OFFSET 0x1EA32

/* The range of plausible action sizes: outside it, this is not the disk
   side with the database on it. */
#define A2_MIN_ACTION_SIZE 4000
#define A2_MAX_ACTION_SIZE 7000

/* The image offsets in the database count backwards from here */
#define A2_IMAGE_BASE (0x23000 - 0x100)

/* The game disk with its sectors in reverse order, kept from detection
   until LookForApple2Images() has taken the pictures from it. */
static uint8_t *reversed_disk = NULL;

// If the buffer is an Apple II WOZ disk image, convert it into a standard
// 143360-byte DOS 3.3 ordered .dsk image (via the shared readwoz library) so
// that the rest of this file can treat it exactly like a .dsk file.
// Returns a newly allocated buffer (caller frees) and sets *outsize, or NULL
// if the data is not WOZ or conversion fails.
static uint8_t *ConvertWozToDsk(uint8_t *data, size_t datasize, size_t *outsize)
{
    if (data == NULL || !is_woz_image(data, datasize))
        return NULL;

    size_t nibsize = datasize;
    uint8_t *nib = woz2nib(data, &nibsize);
    if (nib == NULL)
        return NULL;

    uint8_t *dsk = ReadFullImageFromNib(nib, nibsize);
    free(nib);
    if (dsk == NULL)
        return NULL;

    *outsize = A2_DISK_SIZE;
    return dsk;
}

// If *data is a WOZ image, replace it with its conversion to a .dsk image.
static void LoadAsDsk(uint8_t **data, size_t *size)
{
    size_t dsksize;
    uint8_t *dsk = ConvertWozToDsk(*data, *size, &dsksize);
    if (dsk != NULL) {
        free(*data);
        *data = dsk;
        *size = dsksize;
    }
}

// Copy the 256-byte sectors of src into a new buffer of imagesize bytes
// in reverse order, so that the last sector of src comes first.
static uint8_t *ReverseSectors(const uint8_t *src, size_t srcsize, size_t imagesize)
{
    uint8_t *reversed = MemAlloc(imagesize);
    size_t offset = imagesize - A2_SECTOR_SIZE;
    for (size_t i = 0; i < srcsize && i < imagesize; i += A2_SECTOR_SIZE) {
        memcpy(reversed + offset, src + i, A2_SECTOR_SIZE);
        offset -= A2_SECTOR_SIZE;
    }
    return reversed;
}

static int IsPlausibleActionSize(int actionsize)
{
    return (actionsize >= A2_MIN_ACTION_SIZE && actionsize <= A2_MAX_ACTION_SIZE);
}

int DetectApple2(uint8_t **sf, size_t *extent)
{
    // Accept WOZ images by converting them to a .dsk image up front.
    LoadAsDsk(sf, extent);

    if (*sf == NULL || *extent > MAX_LENGTH || *extent < A2_DISK_SIZE)
        return 0;

    reversed_disk = ReverseSectors(*sf, *extent, A2_DISK_SIZE);

    int actionsize = READ_LE_UINT16(reversed_disk + A2_ACTION_SIZE_OFFSET);
    debug_print("Actionsize: %d\n", actionsize);
    if (!IsPlausibleActionSize(actionsize)) {
        size_t companionsize;
        uint8_t *companionfile = GetCompanionFile(game_file, &companionsize);
        // The companion side may itself be a WOZ image; convert it too.
        if (companionfile != NULL)
            LoadAsDsk(&companionfile, &companionsize);
        if (companionfile && companionsize >= A2_DISK_SIZE) {
            free(reversed_disk);
            reversed_disk = ReverseSectors(companionfile, companionsize, companionsize);
            actionsize = READ_LE_UINT16(reversed_disk + A2_ACTION_SIZE_OFFSET);
        }
        free(companionfile);
        if (!IsPlausibleActionSize(actionsize)) {
            free(reversed_disk);
            reversed_disk = NULL;
            return 0;
        }
    }

    size_t datasize = A2_DISK_SIZE - A2_DATABASE_OFFSET;
    uint8_t *gamedata = MemAlloc(datasize);
    memcpy(gamedata, reversed_disk + A2_DATABASE_OFFSET, datasize);

    AdoptDataFile(sf, extent, gamedata, datasize, SYS_APPLE2);
    return 1;
}

/* The special pictures, by number: where the database says each one is,
   or 0 if there is none with that number. */
uint32_t Apple2SpecialImages[A2_SPECIAL_SLOTS];

enum {
    A2_TRACKS = 35,
    A2_SECTORS = 16,
    A2_NAME_SIZE = 5,
    /* A picture number has two digits */
    A2_MAX_NUMBER = 99,
};

/* A table of the special pictures follows the object pictures in the
   database, at offset of data, with a track and a sector for each and
   zeros for the numbers that are not used. It ends with the sector that
   the database ends in. */
void ReadApple2SpecialImages(const uint8_t *data, size_t offset, size_t size)
{
    memset(Apple2SpecialImages, 0, sizeof Apple2SpecialImages);
    size_t left = A2_SECTOR_SIZE - (A2_DATABASE_OFFSET + offset) % A2_SECTOR_SIZE;
    if (offset > size || left > size - offset)
        left = size - offset;
    const uint8_t *ptr = data + offset;
    for (int i = 0; i < A2_SPECIAL_SLOTS && left >= 2; i++, ptr += 2, left -= 2) {
        if (ptr[0] >= A2_TRACKS || ptr[1] >= A2_SECTORS)
            break;
        Apple2SpecialImages[i] = (ptr[0] * A2_SECTORS + ptr[1]) * A2_SECTOR_SIZE;
    }
}

/* The names of the pictures. They are never freed, as those of the other
   systems are string literals. */
static char (*a2names)[A2_NAME_SIZE] = NULL;

/* The picture among those from first to count in Images that has this
   name, or if name is NULL this address in the database. -1 if none. */
static int FindImage(int first, int count, const char *name, uint32_t address)
{
    for (int i = first; i < count; i++) {
        if (name != NULL ? strcmp(Images[i].Filename, name) == 0 : Images[i].DiskOffset == address)
            return i;
    }
    return -1;
}

/* Copy the picture that the database has at address from the disk to
   Images[*created], and name it by its type and number. Its size comes
   first; that of an object picture does not count itself, so size_extra
   is 2 for those. Returns 0 if there is no such picture. */
static int AddImage(int *created, uint32_t address, char type, int number, size_t size_extra)
{
    if (address == 0 || address > A2_IMAGE_BASE || number > A2_MAX_NUMBER)
        return 0;
    size_t offset = A2_IMAGE_BASE - address;
    if (offset + 2 > A2_DISK_SIZE)
        return 0;
    size_t size = READ_LE_UINT16(reversed_disk + offset) + size_extra;
    if (size <= 2)
        return 0;
    if (offset + size > A2_DISK_SIZE)
        size = A2_DISK_SIZE - offset;

    imgrec *rec = &Images[*created];
    snprintf(a2names[*created], A2_NAME_SIZE, "%c0%02d", type, number);
    rec->Filename = a2names[*created];
    rec->DiskOffset = address;
    rec->Data = MemAlloc(size);
    memcpy(rec->Data, reversed_disk + offset, size);
    rec->Size = size;
    (*created)++;
    return 1;
}

/* The actions that draw an object picture ask for it by number. In
   Fantastic Four that is the place of the picture in the table, counted
   from 1, and in Spider-Man the number of its object. Buckaroo Banzai has
   no such actions. */
static int ObjectImageNumber(int index)
{
    return (CurrentGame == FANTASTIC4) ? index + 1 : (int)ObjectImages[index].Object;
}

/* Add the picture of entry index of ObjectImages, of which those from
   first to *created in Images were added before. Returns its place in
   Images, or -1 if it has none. */
static int AddObjectImage(int first, int *created, int index)
{
    uint32_t address = ObjectImages[index].Image;
    char name[A2_NAME_SIZE];
    int number = ObjectImageNumber(index);
    snprintf(name, sizeof name, "B0%02d", number % 100);

    int found = FindImage(first, *created, name, 0);
    if (found >= 0 && Images[found].DiskOffset != address) {
        /* A second picture of the same object. It is only ever drawn
           with the object, so any free number will do. */
        found = FindImage(first, *created, NULL, address);
        for (number = A2_MAX_NUMBER; found < 0 && number > 0; number--) {
            snprintf(name, sizeof name, "B0%02d", number);
            if (FindImage(first, *created, name, 0) < 0)
                break;
        }
    }
    if (found < 0 && AddImage(created, address, 'B', number, 2))
        found = *created - 1;
    return found;
}

void LookForApple2Images(void)
{
    if (reversed_disk == NULL)
        return;

    /* The database has the address of a picture where those of the other
       systems have its number. A room picture is numbered after the first
       room that has it, and the addresses are changed to the numbers. */
    int count = GameHeader.NumRooms + 1 + GameHeader.NumObjImg + 1 + A2_SPECIAL_SLOTS;

    Images = NewImageList(count);
    free(a2names);
    a2names = MemCalloc((size_t)count * A2_NAME_SIZE);

    int created = 0;

    for (int i = 0; i <= GameHeader.NumRooms; i++) {
        uint32_t address = (uint32_t)Rooms[i].Exits[EXIT_IMAGE];
        int found = FindImage(0, created, NULL, address);
        if (found < 0 && AddImage(&created, address, 'R', i, 0))
            found = created - 1;
        if (found >= 0)
            Rooms[i].Exits[EXIT_IMAGE] = ImageNumberFromName(Images[found].Filename);
    }

    int first_object = created;
    for (int i = 0; i <= GameHeader.NumObjImg; i++) {
        int found = AddObjectImage(first_object, &created, i);
        if (found >= 0)
            ObjectImages[i].Image = ImageNumberFromName(Images[found].Filename);
    }

    for (int i = 0; i < A2_SPECIAL_SLOTS; i++)
        AddImage(&created, Apple2SpecialImages[i], 'S', i, 0);

    Images[created].Filename = NULL;

    free(reversed_disk);
    reversed_disk = NULL;
}
