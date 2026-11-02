//
//  Based on Sagadraw v2.5 by David Lodge
//  with help from Paul David Doherty (including the colour code)
//
//  Original code at https://github.com/tautology0/textadventuregraphics
//  See also http://aimemorial.if-legends.org/gfxbdp.html
//
//  This file handles setup and initialisation of the Irmak tile-based
//  graphics system used by Adventure International UK games. It reads
//  the 256 8x8 tile definitions and per-image offset tables from the
//  game data, then hands off to the Irmak decoder (irmak.c) which
//  transforms and composites tiles into final images.
//
//  Some game versions contain corrupted data bytes. The
//  image_patches[] table applies surgical binary fixes at known
//  offsets so these images render correctly.
//


#include <stdio.h>

#include "glk.h"
#include "sagagraphics.h"
#include "scott.h"
#include "detect_game.h"
#include "seas_of_blood.h"
#include "irmak.h"

#include "sagadraw.h"

static uint8_t *EndOfGraphicsData;

/* Binary patch for a single image in a specific game version.
   Each entry replaces number_of_bytes bytes at the given offset
   within the image's data stream. */
struct image_patch {
    GameIDType id;
    int picture_number;
    int offset;
    int number_of_bytes;
    const char *patch;
};

static const struct image_patch image_patches[] = {
    { UNKNOWN_GAME, 0, 0, 0, "" },
    // Claymorgue C64: Many images are hopelessly broken,
    // but some can be salvaged.
    // 12. "Dungeon" room with door
    { CLAYMORGUE_C64, 12, 378, 9, "\x20\xa4\x02\x80\x20\x82\x01\x20\x84" },
    // 28. Dragon
    { CLAYMORGUE_C64, 28, 584, 1, "\x90" },
    // 16. Empty cyan square, "Underwater"
    { CLAYMORGUE_C64, 16, 0, 12,
        "\x82\xfd\x00\x82\x81\x00\xff\x05\xff\x05\xff\x05" },
    // 16. Spectrum version. Empty cyan square, "Underwater"
    { CLAYMORGUE, 14, 0, 12,
        "\x82\xfd\x00\x82\x81\x00\xff\x05\xff\x05\xff\x05" },
    // 21. Gremlins C64. Snow plough
    { GREMLINS_C64, 21, 10, 5, "\x01\xa0\x03\x00\x01" },
    // 44. Behind bar
    { GREMLINS_C64, 44, 304, 1, "\xb1" },
    // 81. Empty pool bottom with drain
    { GREMLINS_C64, 81, 176, 1, "\xa0" },
    // 85. Sports department
    { GREMLINS_C64, 85, 413, 1, "\x94" },
    // 20. Garage
    { GREMLINS_GERMAN_C64, 20, 191, 9, "\x03\x00\x94\x0c\xb0\x02\x94\x0d\xb0" },
    // QP2 Spider-Man: 9. Madame Web. One tile with bad colour in T64 version
    { SPIDERMAN_C64, 9, 1241, 1, "\x81" },
    // 11. Secret Mission. White visitor's room with window. Broken in D64 version
    { SECRET_MISSION_C64, 11, 88, 2, "\x90\x18" },
    // 33. Key on window sill, fix middle segment rotation
    { SECRET_MISSION, 33, 2, 1, "\xa0" },
    // 33. Key on window sill, fix middle segment rotation
    { SECRET_MISSION_C64, 33, 2, 1, "\xa0" },
    // (The broken glass still needs a proper fix to match the background pattern)
    // 38. Broken glass on window sill, fix bg colour
    { SECRET_MISSION, 38, 53, 1, "\x53" },
    // 38. Broken glass on window sill, fix bg colour
    { SECRET_MISSION_C64, 38, 53, 1, "\x53" },
    // 34. Left half of room with bricks and pillars, minor misplaced line
    { SEAS_OF_BLOOD_C64, 34, 471, 1, "\xa0" },
    { NUMGAMES, 0, 0, 0, "" },
};

/* Search the patch table for an entry matching (game, image_number),
   starting after index 'start'. Returns the index, or 0 (the sentinel)
   if no more patches exist. Called in a loop to apply multiple patches
   to the same image. */
static int FindImagePatch(GameIDType game, int image_number, int start)
{
    for (int i = start + 1; image_patches[i].id != NUMGAMES; i++) {
        if (image_patches[i].id == game && image_patches[i].picture_number == image_number) {
            return i;
        }
    }
    return 0;
}

static void Patch(uint8_t *offset, int patch_number)
{
    const struct image_patch *patch = &image_patches[patch_number];
    for (int i = 0; i < patch->number_of_bytes; i++) {
        const char newval = patch->patch[i];
        offset[i + patch->offset] = (uint8_t)newval;
    }
}

/* One specific C64 version of Claymorgue Castle has images 12–27
   (except 16) hopelessly corrupt, and one ZX Spectrum version images
   9–35 (except 14) — not fixable with simple byte patches. Reassign
   the rooms that use images first to last, except unbroken, to image
   255 (no picture) so the game doesn't try to render garbage. */
static void PatchOutBrokenClaymorgueImages(int first, int last, int unbroken)
{
    char message[128];
    snprintf(message, sizeof(message),
        "[This copy of The Sorcerer of Claymorgue Castle has %d broken or "
        "missing pictures. These have been patched out.]\n\n", last - first);
    Output(message);
    for (int i = first; i <= last; i++) {
        if (i != unbroken)
            for (int j = 0; j < GameHeader.NumRooms; j++) {
                if (Rooms[j].Image == i) {
                    Rooms[j].Image = 255;
                }
            }
    }
}

/* Version 0 (Hulk) stores image offsets in four separate tables
   for rooms, items, close-ups, and special images, at hardcoded
   addresses that differ between the C64 and Spectrum releases.
   image_offset adjusts the raw 16-bit pointers read from
   those tables to file-relative positions. */
typedef struct {
    size_t coordinates;
    size_t item_image_offsets;
    size_t closeup_image_offsets;
    size_t special_image_offsets;
    size_t image_offset;
} HulkImageTables;

static const HulkImageTables hulk_tables_c64 = { 0x22cd, 0x2731, 0x2761, 0x2781, -0x7ff };
static const HulkImageTables hulk_tables_zx = { 0x26db, 0x2798, 0x27bc, 0x276e, 0x441b };

/* Build the image offset table. Version 0 (Hulk) scatters its
   offsets across four tables: rooms (0–10), items (11–27),
   close-ups (28–33), and specials (34+). Later versions store
   all offsets in a single contiguous LE16 table at pos. */
static void ReadImageOffsets(uint16_t *image_offsets, int numgraphics,
    uint8_t *pos, const HulkImageTables *hulk)
{
    for (int i = 0; i < numgraphics; i++) {
        if (hulk != NULL) {
            uint16_t address;

            if (i < 11) {
                address = Game->start_of_image_data + (i * 2);
            } else if (i < 28) {
                address = hulk->item_image_offsets + (i - 10) * 2;
            } else if (i < 34) {
                address = hulk->closeup_image_offsets + (i - 28) * 2;
            } else {
                address = hulk->special_image_offsets + (i - 34) * 2;
            }

            address += file_baseline_offset;
            address = READ_LE_UINT16(entire_file + address);

            image_offsets[i] = address + hulk->image_offset;
        } else {
            image_offsets[i] = READ_LE_UINT16_AND_ADVANCE(&pos);
        }
    }
}

/* Read an image header. The data stream starts with width and
   height (in tiles), followed by x/y screen offsets in version 1+.
   Version 0 (Hulk) stores item image coordinates in a separate
   table and has no offsets for room or special images. */
static void ReadImageHeader(Image *img, uint8_t **posptr, int picture_number,
    const HulkImageTables *hulk)
{
    uint8_t *pos = *posptr;

    img->width = *pos++;
    if (img->width > IRMAK_IMGWIDTH)
        img->width = IRMAK_IMGWIDTH;

    img->height = *pos++;
    if (img->height > IRMAK_IMGHEIGHT)
        img->height = IRMAK_IMGHEIGHT;

    if (hulk == NULL) {
        img->xoff = *pos++;
        if (img->xoff > IRMAK_IMGWIDTH)
            img->xoff = 4;
        img->yoff = *pos++;
        if (img->yoff > IRMAK_IMGHEIGHT)
            img->yoff = 0;
    } else {
        /* Hulk item images (11–27) have per-image coordinates;
           all others are drawn at the origin. */
        if (picture_number > 9 && picture_number < 28) {
            img->xoff = entire_file[hulk->coordinates + picture_number - 10 + file_baseline_offset];
            img->yoff = entire_file[hulk->coordinates + 18 + picture_number - 10 + file_baseline_offset];
        } else {
            img->xoff = img->yoff = 0;
        }
    }

    *posptr = pos;
}

static void SetFullScreenHeader(Image *img)
{
    img->height = IRMAK_IMGHEIGHT;
    img->width = IRMAK_IMGWIDTH;
    img->xoff = 4;
    img->yoff = 0;
}

/* Detect the corrupt Claymorgue release by checking whether a
   specific image in the broken range has zero dimensions.
   If so, disable the entire corrupt range and salvage the
   few images that can be reconstructed (16/28 for C64, 14
   for Spectrum) by giving them valid full-screen headers.
   Called for every picture in order; *broken remembers the finding. */
static void SalvageClaymorgueImage(Image *img, uint8_t **pos, int picture_number,
    int *broken)
{
    int empty = (img->height == 0 && img->width == 0);

    if (CurrentGame == CLAYMORGUE_C64) {
        if (empty && picture_number == 13) {
            PatchOutBrokenClaymorgueImages(12, 27, 16);
            *broken = 1;
        }

        if (*broken && (picture_number == 16 || picture_number == 28)) {
            SetFullScreenHeader(img);
            if (picture_number == 28)
                *pos = entire_file + 0x2005;
        }
    } else if (CurrentGame == CLAYMORGUE) {
        if (empty && picture_number == 9) {
            PatchOutBrokenClaymorgueImages(9, 35, 14);
            *broken = 1;
        }

        if (*broken && picture_number == 14)
            SetFullScreenHeader(img);
    }
}

/* Read the 256-entry tile font (each tile is 8 bytes: one byte
   per row of 8 pixels, MSB = leftmost pixel). */
static void ReadTiles(uint8_t *pos)
{
    for (int i = 0; i < 256; i++) {
        for (int y = 0; y < 8 && pos < EndOfGraphicsData; y++) {
            tiles[i][y] = *pos++;
        }
    }
}

static void EnterTextOnlyMode(const char *why)
{
    fprintf(stderr, "SagaSetup: %s. Entering text-only mode.\n", why);
    Game->number_of_pictures = 0;
}

/* Main graphics initialisation entry point. Reads the tile font
   (256 8x8-pixel tile definitions), the per-image offset table,
   and each image's header (width, height, screen position).
   Binary patches are applied to known-broken images before handing
   everything to InitIrmak() for later rendering.

   imgoffset, if non-zero, overrides the game's default data_offset
   (used when graphics data has been relocated). */
void SagaGraphicsSetup(size_t imgoffset)
{
    if (images != NULL)
        return;

    EndOfGraphicsData = entire_file + file_length;

    uint16_t image_offsets[Game->number_of_pictures];

    if (palchosen == NO_PALETTE) {
        palchosen = Game->palette;
    }

    if (palchosen == NO_PALETTE) {
        EnterTextOnlyMode("invalid palette");
        return;
    }

    DefinePalette();

    int version = Game->picture_format_version;

    /* The tile font (256 tiles × 8 bytes each = 0x800 bytes) is stored
       at tiles_start. The image offset table immediately follows in
       most versions, or is at a separately specified address. */
    int32_t tiles_start = Game->start_of_tiles + file_baseline_offset;
    int32_t offset_table_start = Game->start_of_image_data + file_baseline_offset;

    if (Game->start_of_image_data == FOLLOWS) {
        offset_table_start = tiles_start + 0x800;
    }

    /* data_offset is added to each entry in the offset table to
       produce the final file position of each image's data stream. */
    int32_t data_offset = Game->image_address_offset + file_baseline_offset;
    if (imgoffset)
        data_offset = imgoffset;
    uint8_t *pos;
    int numgraphics = Game->number_of_pictures;
    pos = SeekToPos(tiles_start);
    if (pos == NULL) {
        EnterTextOnlyMode("tile data is outside the file");
        return;
    }

    ReadTiles(pos);

    pos = SeekToPos(offset_table_start);
    if (version > 0 && (pos == NULL || EndOfGraphicsData - pos < numgraphics * 2)) {
        EnterTextOnlyMode("image offset table is outside the file");
        return;
    }

    images = (Image *)MemAlloc(sizeof(Image) * numgraphics);
    Image *img = images;

    const HulkImageTables *hulk = NULL;

    if (version == 0) {
        if (CurrentGame == HULK_C64) {
            hulk = &hulk_tables_c64;
        } else if (CurrentGame == HULK) {
            hulk = &hulk_tables_zx;
        } else {
            EnterTextOnlyMode("Unknown game with version 0 graphics");
            return;
        }
    }

    ReadImageOffsets(image_offsets, numgraphics, pos, hulk);

    int broken_claymorgue_pictures = 0;

    for (int picture_number = 0; picture_number < numgraphics; picture_number++) {
        pos = SeekToPos(image_offsets[picture_number] + data_offset);
        if (pos == NULL)
            return;

        ReadImageHeader(img, &pos, picture_number, hulk);
        SalvageClaymorgueImage(img, &pos, picture_number, &broken_claymorgue_pictures);

        /* Point imagedata at the start of this image's tile/attribute
           stream. datasize is a conservative upper bound (to end of
           file) — the actual stream length is determined during
           decoding. Apply any known binary patches for this image. */
        img->imagedata = pos;
        img->datasize = EndOfGraphicsData - pos;

        int patch = FindImagePatch(CurrentGame, picture_number, 0);
        while (patch) {
            Patch(img->imagedata, patch);
            patch = FindImagePatch(CurrentGame, picture_number, patch);
        }

        img++;
    }

    InitIrmak(Game->number_of_pictures, Game->picture_format_version);
}
