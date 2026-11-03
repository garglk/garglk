//
//  c64_small_graphics.c
//  scott
//
//  Created by Administrator on 2026-01-23.
//

// Handles the compact C64 graphics format used by early Scott Adams
// SAGA adventures (Pirate Adventure, Voodoo Castle) where game database
// and image data are bundled in a single file. Room backgrounds use
// RLE-compressed multicolor bitmaps; small item images are stored
// as C64 hardware sprites (24x21 monochrome).

#include <stdlib.h>
#include <string.h>

#include "scott.h"
#include "saga.h"
#include "memory_allocation.h"
#include "sagagraphics.h"
#include "c64a8draw.h"

#include "c64_small.h"

/* Screen position for drawing a C64 sprite overlay */
typedef struct {
    int index; // object index this sprite represents
    int x;     // pixel x position on screen
    int y;     // pixel y position on screen
} spritelist;

#define MAX_SPRITES 32
#define MAX_AIO_ITEMS 256

/* The objects drawn as sprites, read from the game by ReadAllInOneImages().
   Ends with an entry whose index is 0. */
static spritelist sprites[MAX_SPRITES + 1];

/* For an object picture that is only shown in one room, that room. */
static uint8_t room_gate[MAX_AIO_ITEMS];

static void AddSprite(int index, int x, int y)
{
    int i = 0;
    while (sprites[i].index != 0)
        i++;
    if (i >= MAX_SPRITES)
        return;
    sprites[i].index = index;
    sprites[i].x = x;
    sprites[i].y = y;
}

/* The pictures are found through the interpreter's own tables. The file
   is a memory image that starts with its load address, and the tables
   are given here as C64 memory addresses.

   AIO_ROOM_TABLE holds the address of every room picture, from room 1,
   and AIO_OBJECT_TABLE the address of every object picture. Which object
   a slot of the latter belongs to is told by a list that differs between
   the games: Pirate Adventure has the object of each slot, in slot order,
   Voodoo Castle the slot of each object.

   An object picture is either a bitmap like the room pictures or, in
   Pirate Adventure, a five-byte sprite record: sprite number, x (low
   byte, high bit) and y in sprite coordinates. The objects of
   PIRATE_SPRITE_ITEMS are sprites. The bitmaps of PIRATE_GATE_ITEMS are
   only drawn in the matching room of PIRATE_GATE_ROOMS. All three lists
   end with a byte of 0x80 or more. */
enum {
    AIO_ROOM_TABLE = 0x2f00,
    AIO_OBJECT_TABLE = 0x2f80,
    AIO_SPRITE_DATA = 0x3000,
    AIO_SPRITE_SIZE = 63,
    AIO_SPRITE_STEP = 64,
    AIO_SPRITE_X0 = 24, /* sprite coordinates of the top left pixel */
    AIO_SPRITE_Y0 = 50,
    AIO_HEADER_SIZE = 8,

    PIRATE_SLOT_ITEMS = 0x254c,
    PIRATE_SPRITE_ITEMS = 0x14d4,
    PIRATE_GATE_ITEMS = 0x14cd,
    PIRATE_GATE_ROOMS = 0x14c7,

    VOODOO_ITEM_SLOTS = 0x24f4
};

/* Objects of Pirate Adventure that have no picture in the original,
   shown with the picture of another object. */
static const struct {
    int item;
    int like;
} pirate_extras[] = {
    {  7, 25 }, // empty bottle: bottle of rum
    { 42, 25 }, // bottle of salt water
    { 63,  8 }, // burnt-out torch: unlit torch
    {  9,  8 }, // lit torch
};

/* Room gates of our own. The original draws the open treasure chest
   wherever it is, at its place in the shack picture. */
static const struct {
    int item;
    int room;
} pirate_extra_gates[] = {
    { 28, 9 },
};

typedef struct {
    const uint8_t *data;
    size_t end;   /* the pictures end here, where the database begins */
    int load;
} aiofile;

/* The file offset of a memory address, or 0 if size bytes from there are
   not all inside the picture part of the file. */
static size_t AioOffset(const aiofile *f, int address, size_t size)
{
    if (address < f->load)
        return 0;
    size_t offset = (size_t)(address - f->load) + 2;
    if (offset + size > f->end)
        return 0;
    return offset;
}

static int AioByte(const aiofile *f, int address)
{
    size_t offset = AioOffset(f, address, 1);
    return offset ? f->data[offset] : 0xff;
}

static int AioWord(const aiofile *f, int address)
{
    size_t offset = AioOffset(f, address, 2);
    return offset ? READ_LE_UINT16(f->data + offset) : 0xffff;
}

/* The offset of the bitmap at address, or 0 if there is no plausible
   picture header there. */
static size_t AioBitmap(const aiofile *f, int address)
{
    size_t offset = AioOffset(f, address, AIO_HEADER_SIZE);
    if (offset == 0)
        return 0;
    const uint8_t *p = f->data + offset;
    if (p[0] >= p[2] || p[2] > 40 || p[1] >= p[3] || p[3] > 200)
        return 0;
    for (int i = 4; i < AIO_HEADER_SIZE; i++)
        if (p[i] > 15)
            return 0;
    return offset;
}

/* Fill slots[] with the object-table slot of every object, or -1. */
static void ReadAllInOneSlots(const aiofile *f, int *slots)
{
    for (int i = 0; i < MAX_AIO_ITEMS; i++)
        slots[i] = -1;

    if (CurrentGame == PIRATE_US) {
        for (int slot = 0; slot < MAX_AIO_ITEMS; slot++) {
            int item = AioByte(f, PIRATE_SLOT_ITEMS + slot);
            if (item >= 0x80)
                break;
            slots[item] = slot;
        }
    } else {
        /* The list is one entry short: the last object, the smiling
           Count Cristo of the ending, has none. */
        for (int item = 0; item < GameHeader.NumItems && item < MAX_AIO_ITEMS; item++) {
            int slot = AioByte(f, VOODOO_ITEM_SLOTS + item);
            if (slot < 0x80)
                slots[item] = slot;
        }
    }
}

/* Find out which objects of Pirate Adventure are sprites and which are
   only drawn in one room. */
static void ReadPirateGates(const aiofile *f, uint8_t *is_sprite)
{
    for (int i = 0; i < MAX_AIO_ITEMS; i++) {
        int item = AioByte(f, PIRATE_SPRITE_ITEMS + i);
        if (item >= 0x80)
            break;
        is_sprite[item] = 1;
    }
    for (int i = 0; i < MAX_AIO_ITEMS; i++) {
        int item = AioByte(f, PIRATE_GATE_ITEMS + i);
        if (item >= 0x80)
            break;
        room_gate[item] = (uint8_t)AioByte(f, PIRATE_GATE_ROOMS + i);
    }
    for (size_t i = 0; i < sizeof pirate_extra_gates / sizeof pirate_extra_gates[0]; i++)
        room_gate[pirate_extra_gates[i].item] = (uint8_t)pirate_extra_gates[i].room;
}

/* Give the objects of pirate_extras the picture of their likeness. */
static void AddPirateExtras(int *slots, uint8_t *is_sprite)
{
    for (size_t i = 0; i < sizeof pirate_extras / sizeof pirate_extras[0]; i++) {
        int item = pirate_extras[i].item, like = pirate_extras[i].like;
        slots[item] = slots[like];
        is_sprite[item] = is_sprite[like];
        room_gate[item] = room_gate[like];
    }
}

static int CompareOffsets(const void *a, const void *b)
{
    size_t x = *(const size_t *)a, y = *(const size_t *)b;
    return (x > y) - (x < y);
}

/* A bitmap has no stored length. It runs to the start of whatever
   picture comes next in the file; the decoder stops where it ends. */
static size_t AioBitmapSize(const size_t *starts, int numstarts, size_t offset, size_t end)
{
    for (int i = 0; i < numstarts; i++)
        if (starts[i] > offset)
            return starts[i] - offset;
    return end - offset;
}

static USImage *AddAllInOneImage(USImage *image, USImageType usage, int index, const uint8_t *data, size_t size)
{
    image->systype = SYS_C64_TINY;
    image->usage = usage;
    image->index = index;
    image->imagedata = MemAlloc(size);
    image->datasize = size;
    memcpy(image->imagedata, data, size);
    return AppendImage(image);
}

/* Read the picture tables of an all-in-one game and extract every picture
   into the USImages linked list. Returns 0 if there are none. */
static int ReadAllInOneImages(const uint8_t *data, size_t extent, size_t cutoff)
{
    aiofile f = { data, cutoff < extent ? cutoff : extent, READ_LE_UINT16(data) };

    int slots[MAX_AIO_ITEMS];
    uint8_t is_sprite[MAX_AIO_ITEMS] = { 0 };
    size_t object_offsets[MAX_AIO_ITEMS] = { 0 };
    size_t room_offsets[MAX_AIO_ITEMS] = { 0 };
    size_t starts[2 * MAX_AIO_ITEMS + 1];
    int numstarts = 0;

    memset(sprites, 0, sizeof sprites);
    memset(room_gate, 0, sizeof room_gate);

    ReadAllInOneSlots(&f, slots);
    if (CurrentGame == PIRATE_US) {
        ReadPirateGates(&f, is_sprite);
        AddPirateExtras(slots, is_sprite);
    }

    int numrooms = 0;
    while (numrooms < GameHeader.NumRooms && numrooms < MAX_AIO_ITEMS - 1) {
        size_t offset = AioBitmap(&f, AioWord(&f, AIO_ROOM_TABLE + numrooms * 2));
        if (offset == 0)
            break;
        room_offsets[++numrooms] = offset;
        starts[numstarts++] = offset;
    }

    int have_sprites = 0;
    for (int item = 0; item < MAX_AIO_ITEMS; item++) {
        if (slots[item] < 0)
            continue;
        int address = AioWord(&f, AIO_OBJECT_TABLE + slots[item] * 2);
        if (is_sprite[item]) {
            size_t record = AioOffset(&f, address, 4);
            if (record == 0)
                continue;
            const uint8_t *r = data + record;
            object_offsets[item] = AioOffset(&f, AIO_SPRITE_DATA + r[0] * AIO_SPRITE_STEP, AIO_SPRITE_SIZE);
            if (object_offsets[item] == 0)
                continue;
            AddSprite(item, r[1] + (r[2] ? 256 : 0) - AIO_SPRITE_X0, r[3] - AIO_SPRITE_Y0);
            have_sprites = 1;
        } else {
            object_offsets[item] = AioBitmap(&f, address);
            if (object_offsets[item])
                starts[numstarts++] = object_offsets[item];
        }
    }
    if (have_sprites) {
        size_t offset = AioOffset(&f, AIO_SPRITE_DATA, 1);
        if (offset)
            starts[numstarts++] = offset;
    }
    qsort(starts, numstarts, sizeof starts[0], CompareOffsets);

    USImages = NewImage();
    USImage *image = USImages;

    for (int item = 0; item < MAX_AIO_ITEMS; item++) {
        size_t offset = object_offsets[item];
        if (offset == 0)
            continue;
        size_t size = is_sprite[item] ? AIO_SPRITE_SIZE : AioBitmapSize(starts, numstarts, offset, f.end);
        image = AddAllInOneImage(image, IMG_ROOM_OBJ, item, data + offset, size);
    }
    for (int room = 1; room <= numrooms; room++) {
        size_t offset = room_offsets[room];
        image = AddAllInOneImage(image, IMG_ROOM, room, data + offset,
                                 AioBitmapSize(starts, numstarts, offset, f.end));
    }

    FreeTrailingEmptyImage(image);

    /* Safety check: discard the list if it ended up empty */
    FreeUSImagesIfEmpty();

    return USImages != NULL;
}

/* Prepend a hard-coded all-black "darkness" ROOM image at index 0.
   The RLE body fills the rect with palette-index-0 pixels
   (which is black after SetColor(0, 0)). */
static void PrependDarknessImage(void)
{
    USImage *background = NewImage();
    background->systype = SYS_C64_TINY;
    background->usage = IMG_ROOM;
    background->index = 0;
    background->datasize = 8 + 7 * 3;
    background->imagedata = MemCalloc(background->datasize);
    background->imagedata[0] = 11;                    /* left  = 88  / 8 */
    background->imagedata[1] = 0;                     /* top */
    background->imagedata[2] = 29;                    /* right = 232 / 8 */
    background->imagedata[3] = (uint8_t)ImageHeight;  /* bottom */

    /* 7 repeat-128 commands, each emitting 128 pairs of color-0 pixels.
       Comfortably covers 19 columns x up to 41 pairs = 779 pairs. */
    for (int i = 0; i < 7; i++) {
        // This leaves two zero bytes after every 0xff.
        background->imagedata[8 + i * 3] = 0xff;
    }
    background->next = USImages;
    if (USImages != NULL)
        USImages->previous = background;
    USImages = background;
}

/* Load an all-in-one C64 file that bundles the game database and image
   data together. Splits the file at imgoffset: the portion after the
   cutoff becomes the game database (returned via *sf / *extent), while
   the image data before the cutoff is extracted into USImages through
   the interpreter's picture tables. */
GameIDType handle_all_in_one(uint8_t **sf, size_t *extent, c64rec c64_registry)
{
    size_t cutoff = c64_registry.imgoffset;
    if (cutoff == 0 || cutoff >= *extent) {
        return UNKNOWN_GAME;
    }

    /* Extract the game database portion (everything after the image data) */
    size_t smallsize = *extent - cutoff;
    uint8_t *shorter = MemAlloc(smallsize);
    memcpy(shorter, *sf + cutoff, smallsize);

    GameIDType result = LoadBinaryDatabase(shorter, smallsize, *Game, 0);
    if (result == UNKNOWN_GAME) {
        free(shorter);
        return UNKNOWN_GAME;
    }

    /* The Count and the c64.com release of Voodoo Castle have no
       pictures and are played as text. */
    int has_pictures = 0;
    if (CurrentGame == PIRATE_US || CurrentGame == VOODOO_CASTLE_US)
        has_pictures = ReadAllInOneImages(*sf, *extent, cutoff);

    if (!has_pictures) {
        /* Text-only */
        CurrentSys = SYS_C64;
    } else {
        /* Color 0 is always black */
        SetColor(0, 0);

        ImageWidth = 320;
        if (CurrentGame == VOODOO_CASTLE_US)
            ImageHeight = 80;
        else
            ImageHeight = 78;

        PrependDarknessImage();
    }

    /* Replace the original all-in-one buffer with just the database portion */
    free(*sf);
    *sf = shorter;
    *extent = smallsize;

    return CurrentGame;
}

/* Standard Commodore 64 color palette (16 colors, RGB) */
glui32 colorC64[16] = {
    0x000000, // black
    0xffffff, // white
    0xbf6148, // red
    0x99e6f9, // cyan
    0xb159b9, // purple
    0x62D375, // green
    0x5f48e9, // blue
    0xf7ff6c, // yellow
    0xba8620, // orange
    0x877219, // brown
    0xe79a84, // light red
    0x7B7B7B, // light grey
    0xa7a7a7, // grey
    0xc0ffb9, // light green
    0xa28fff, // light blue
    0x454545, // dark grey
};

/* Decode and draw an RLE-compressed multicolor bitmap.
   Returns a pointer past the consumed data, or NULL on error.

   Data format:
     Byte 0:    left column (in character cells, 8px each)
     Byte 1:    top scanline
     Byte 2:    right column
     Byte 3:    bottom scanline
     Bytes 4-7: four palette entries (C64 color indices)
     Remaining: RLE-compressed pixel data (pairs of bytes)

   RLE encoding: a signed count byte followed by pixel data.
     count >= 0: (count+1) literal pairs follow (2 bytes each)
     count <  0: one pair follows, repeated (count & 0x7f)+1 times

   Pixels are drawn in 8-pixel-wide columns, top to bottom. When the
   bottom edge is reached, drawing advances to the next column. */
static uint8_t *DrawMiniC64ImageFromData(uint8_t *ptr, size_t datasize)
{
    if (datasize < 8)
        return NULL;

    uint8_t *endptr = ptr + datasize - 1;

    /* Parse the bounding rectangle (in character-cell coordinates).
       Images with left < 10 are shifted right so all images align to
       column 11, with the right edge extended to match. */
    int shifted = 0;
    int left = *ptr++;
    if (left < 10) {
        shifted = 11 - left;
        left = 11;
    }
    left *= 8;
    int xpos = left;

    int top = *ptr++;
    int ypos = top;

    int right = (*ptr++ + shifted) * 8;

    int bottom = *ptr++;

    /* Read and apply the 4-entry palette (multicolor mode: 2 bits per pixel.) */
    /* Overlay images have all colors set to 0. For these, don't apply the palette –
       these use the palette of the background image */
    if (ptr + 4 > endptr)
        return NULL;
    for (int i = 0; i < 4; i++) {
        int color = *ptr++;
        if (color > 15) {
            fprintf(stderr, "Error Illegal color!\n");
            return NULL;
        } else if (color != 0) {
            SetColor(i, colorC64[color]);
        }
    }

    /* Decode RLE-compressed pixel data */
    uint8_t work1 = 0, work2 = 0;

    while (xpos <= right && ptr < endptr) {
        int8_t count = *ptr++;
        int repeat_pattern = (count < 0);
        if (repeat_pattern) {
            count &= 0x7f;
            work1 = *ptr++;
            work2 = *ptr++;
        }
        for (int i = 0; i <= count; i++) {
            if (!repeat_pattern) {
                if (ptr + 1 > endptr)
                    return ptr;
                work1 = *ptr++;
                work2 = *ptr++;
            }
            /* Draw two bytes of pixel data and advance the cursor */
            DrawPatternAndAdvancePos(xpos, &ypos, work1);
            DrawPatternAndAdvancePos(xpos, &ypos, work2);

            if (ypos > bottom) {
                if (xpos == right)
                    return ptr;
                xpos += 8;
                ypos = top;
            }
        }
    }
    return ptr;
}

/* Draw a C64 hardware sprite: 24x21 pixels, monochrome (white).
   Sprite data is 63 bytes (3 bytes per row x 21 rows). Each row is
   a 24-bit bitmask where set bits are drawn as white pixels. */
static int DrawSprite(USImage *img, int index) {
    spritelist sprite = sprites[index];
    SetColor(5, 0xffffff);
    for (int i = 0; i < 63; i += 3) {
        glsi32 y = sprite.y + i / 3;

        uint32_t line = img->imagedata[i] << 16 | img->imagedata[i + 1] << 8 | img->imagedata[i + 2];
        for (int j = 23; j >= 0; j--) {
            if ((line >> j) & 1) {
                PutPixel(sprite.x + (23 - j), y, 5);
            }
        }
    }
    return 1;
}

/* Main entry point for drawing a C64 "tiny" image.
   Some object bitmaps are location-specific overlays that only make
   sense in one room, so skip them elsewhere. Objects that
   are C64 sprites are dispatched to DrawSprite; everything else goes
   through the RLE bitmap decoder. After decoding, the image buffer is
   shrunk to its actual consumed size. */
int DrawMiniC64(USImage *img) {
    if (img->usage == IMG_ROOM_OBJ) {
        /* Location-specific bitmap overlays: only draw in the correct room */
        if (img->index >= 0 && img->index < MAX_AIO_ITEMS && room_gate[img->index] != 0 && MyLoc != room_gate[img->index])
            return 0;
        /* Check if this object has a sprite; if so, draw it directly */
        for (int i = 0; sprites[i].index != 0; i++) {
            if (img->index == sprites[i].index) {
                return DrawSprite(img, i);
            }
        }
    }
    uint8_t *result  = DrawMiniC64ImageFromData(img->imagedata, img->datasize);
    if (result == NULL)
        return 0;

    /* Shrink the buffer to the actual decoded size, discarding trailing
       padding that was allocated based on the gap to the next image. */
    size_t actual_size = result - img->imagedata;
    if (actual_size < img->datasize) {
        img->imagedata = MemRealloc(img->imagedata, actual_size);;
        img->datasize = actual_size;
    }
   return 1;
}
