//
//  saga.c
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-09-29.
//

#include <stdlib.h>
#include <string.h>

#include "apple2draw.h"
#include "apple2_vector_draw.h"
#include "atari_8bit_vector_draw.h"
#include "c64a8draw.h"
#include "c64a8scott.h"
#include "c64_small.h"
#include "detect_game.h"
#include "pcdraw.h"
#include "randomness.h"
#include "rtpi_graphics.h"
#include "sagagraphics.h"
#include "scott.h"
#include "scott_meta.h"
#include "scott_actions.h"

#include "saga.h"

/* DOS image filenames for Questprobe featuring The Incredible Hulk.
   (The only SAGA graphical game released for MS-DOS)
   Naming convention:
     "R01xx"  — Room image, where xx is the room number.
     "B01xxR" — Object image associated with a room, where xx is the object index.
     "B01xxI" — Object image for the inventory screen.
*/
// clang-format off

static const char *DOSFilenames[] =
  { "B01024R", "B01044I", "R0109",   "R0190",
    "B01001I", "B01025R", "B01045R", "R0112",  "R0191",
    "B01002I", "B01026R", "B01046I", "R0115",  "R0192",
    "B01003I", "B01029I", "B01048I", "R0116",  "R0193",
    "B01004I", "B01030I", "B01050I", "R0119",  "R0194",
    "B01012I", "B01031I", "B01051I", "R0120",  "R0195",
    "B01013R", "B01032I", "B01053R", "R0181",  "R0196",
    "B01016I", "B01033R", "B01070R", "R0182",  "R0197",
    "B01017R", "B01034R", "B01072R", "R0183",  "R0198",
    "B01018I", "B01035I", "R0184",   "R0199",  "B01019I",
    "B01036R", "R0100",   "R0185",   "B01020R","B01037I",
    "R0101",   "R0186",   "B01021I", "B01039R","R0102",
    "R0187",   "B01022R", "B01040R", "R0103",  "R0188",
    "B01023I", "B01042I", "R0104",   "R0189",  NULL };

// clang-format on

/* Load all DOS-format image files into a linked list of USImage nodes.
   Parses each filename to determine image type (room, room object,
   inventory object) and index number from the naming convention. */
int LoadDOSImages(void)
{
    USImages = NewImage();

    USImage *image = USImages;
    size_t datasize;
    int found = 0;

    for (int i = 0; DOSFilenames[i] != NULL; i++) {
        const char *shortname = DOSFilenames[i];
        uint8_t *data = FindImageFile(shortname, &datasize);
        if (data) {
            found++;
            image->datasize = datasize;
            image->imagedata = MemAlloc(datasize);
            memcpy(image->imagedata, data, datasize);
            free(data);
            image->systype = SYS_MSDOS;
            /* "R01xx" filenames are room images; extract 2-digit room index from positions 3-4 */
            if (shortname[0] == 'R') {
                image->usage = IMG_ROOM;
                image->index = (shortname[3] - '0') * 10 + shortname[4] - '0';
            } else {
                /* "B01xxR" or "B01xxI" — extract 2-digit object index from positions 4-5 */
                image->index = (shortname[4] - '0') * 10 + shortname[5] - '0';
                if (shortname[6] == 'R') {
                    image->usage = IMG_ROOM_OBJ;
                } else {
                    image->usage = IMG_INV_OBJ;
                }
            }
            image = AppendImage(image);
        }
    }
    if (!found) {
        free(USImages);
        USImages = NULL;
        return 0;
    }
    return 1;
}

/* Parse the US-format dictionary from binary game data.

   Words are stored as fixed-length strings (GameHeader.WordLength chars each).
   The dictionary contains nouns first, then verbs, with (NumWords + 1) entries
   of each. A '*' character in the stream does not count toward word length
   (used as a synonym marker). A zero byte at the start of a word is skipped
   (padding). A character with the high bit set (> 127) signals end of
   dictionary. */
static uint8_t *ReadUSDictionary(uint8_t *ptr)
{
    char dictword[32];
    char c = 0;
    int charindex = 0;

    int nn = GameHeader.NumWords + 1;

    for (int wordnum = 0; wordnum <= GameHeader.NumWords * 2 + 1; wordnum++) {
        for (int i = 0; i < GameHeader.WordLength; i++) {
            c = *(ptr++);
            /* Skip leading zero-padding at the start of a word */
            if (c == 0 && charindex == 0)
                c = *(ptr++);
            dictword[charindex++] = c;
            /* '*' marks a synonym; don't count it toward the word length */
            if (c == '*')
                i--;
        }
        dictword[charindex] = 0;

        /* First (NumWords + 1) entries are nouns, the rest are verbs */
        char *word = MemAlloc(charindex + 1);
        memcpy(word, dictword, charindex + 1);
        if (wordnum < nn) {
            Nouns[wordnum] = word;
            debug_print("Nouns %d: \"%s\"\n", wordnum, word);
        } else {
            Verbs[wordnum - nn] = word;
            debug_print("Verbs %d: \"%s\"\n", wordnum - nn, word);
        }

        /* High bit set on last character signals end of dictionary */
        if (c > 127)
            return ptr;

        charindex = 0;
    }

    return ptr;
}

/* Draw an Apple II format image. Clears the screen buffer first for room
   images to avoid compositing artifacts from the previous scene. */
static int DrawApple2Image(USImage *image)
{
    if (image->usage == IMG_ROOM)
        ClearApple2ScreenMem();
    DrawApple2ImageFromData(image->imagedata, image->datasize, CurrentGame == COUNT_US ? APPLE2_COUNT : APPLE2_STANDARD, NULL);
    debug_print("Drawing image with index %d, usage %d\n", image->index, image->usage);
    return 1;
}

/* Draw a DOS/PC (CGA) format image. */
static int DrawDOSImage(USImage *image) {
    if (image->imagedata == NULL) {
        fprintf(stderr, "DrawDOSImage: ptr == NULL\n");
        return 0;
    }

    debug_print("DrawDOSImage: usage:%d index:%d\n", image->usage, image->index);

    StartPixelRuns();
    int result = DrawDOSImageFromData(image->imagedata, image->datasize);
    FlushPixelRuns();
    return result;
}

/* Dispatch image drawing to the appropriate platform-specific renderer
   based on the image's system type (DOS, C64, Apple II, Atari 8-bit, TI-99/4A). */
static int DrawUSImage(USImage *image)
{
    last_image_index = image->index;
    switch (image->systype) {
        case SYS_MSDOS:
            return DrawDOSImage(image);
        case SYS_C64_TINY:
            return DrawMiniC64(image);
        case SYS_APPLE2:
            return DrawApple2Image(image);
        case SYS_APPLE2_LINES:
            return DrawApple2VectorImage(image);
        case SYS_C64:
        case SYS_ATARI8:
            return DrawC64A8Image(image);
        case SYS_ATARI8_LINES:
            DrawAtari8BitVectorImage(image);
            return 1;
        case SYS_TI994A:
            DrawRTPIImage(image);
            return 1;
        default:
            return 0;
    }
}

/* Draw images for all items the player is currently carrying.
   Iterates the image list in forward order (low index to high). */
static void DrawInventoryImages(void)
{
    USImage *image = USImages;
    if (image != NULL) {
        do {
            if ((image->usage == IMG_INV_OBJ || image->usage == IMG_INV_AND_ROOM_OBJ) &&
                image->index <= GameHeader.NumItems &&
                Items[image->index].Location == CARRIED) {
                DrawUSImage(image);
            }
            image = image->next;
        } while (image != NULL);
    }
}

static void DrawRoomObjectImages(void)
{
    /* We must draw the object images like this, from highest index
       to lowest, to match the original and not get too many
       weird results where objects are partially covered by
       stuff which ought to be behind them.
       (While the images on the inventory screen are draw in the
       opposite order.) */
    for (int i = GameHeader.NumItems; i >= 0; i--) {
        if (Items[i].Location == MyLoc) {
            DrawUSRoomObject(i);
        }
    }
}

/* Find and draw the room image matching the given room number.
   Returns 1 on success, 0 if no matching image was found. */
int DrawUSRoom(int room)
{
    USImage *image = USImages;
    if (image != NULL) {
        do {
            if (image->usage == IMG_ROOM && image->index == room) {
                return DrawUSImage(image);
            }
            image = image->next;
        } while (image != NULL);
    }
    return 0;
}

/* Rooms and items of the US games that get overlays */
enum {
    HULK_ITEM_LARGE_HOLE = 14, /* Two items, 14 and 15 */
    HULK_ITEM_BIO_GEM = 18,
    HULK_ITEM_WAX = 21,

    COUNT_ROOM_FLOWER_BOX = 5,
    COUNT_ROOM_BATHROOM = 8,
    COUNT_ROOM_OUTSIDE_CASTLE = 9,
    COUNT_ROOM_PIT = 18,
    COUNT_ITEM_COAT_OF_ARMS = 5,
    COUNT_ITEM_MIRROR = 17,
    COUNT_ITEM_SHEET_END = 35, /* The other end of the sheet */
    COUNT_ITEM_DAISIES = 61,

    VOODOO_ROOM_CHIMNEY = 14,
    VOODOO_ITEM_BOARDS = 45,

    RTPI_ROOM_SEA = 11,
    RTPI_ROOM_UNDERSEA = 12,
    RTPI_ROOM_UNDER_DOCK = 22, /* On beam under dock */
    RTPI_ITEM_DOCK_EAST = 26,
    RTPI_ITEM_DOCK_UNDERSIDE = 27,
    RTPI_ITEM_BOAT_WEST = 28,
    RTPI_ITEM_DOCK = 32, /* DESTROYED at night */
};

/* Draw a "fuzzy" (blurred/distorted) version of the room image for
   Return to Pirate's Isle. Used when the player's vision is impaired
   (not wearing corrective lenses). Suppressed when it's truly dark
   (at sea at night with no floodlights). */
int DrawFuzzyRoom(int room)
{
    /* If at sea at night, and floodlights are off or battery is out, don't draw fuzzy image
       (because it really is dark) */
    if (Items[RTPI_ITEM_DOCK].Location == DESTROYED && (!TestBitFlag(29) || CurrentCounter <= 0))
        return 0;
    /* Bitflag 2 is set while we cannot see clearly: the glasses are off or
       lost, the face mask is off or full of water, or the silt is stirred
       up. The game then says everything is "too FUZZY to see". If the flag
       is clear, it really is dark. */
    if (!TestBitFlag(2))
        return 0;

    USImage *image = USImages;
    if (image != NULL) {
        do {
            if (image->usage == IMG_ROOM && image->index == room) {
                return DrawFuzzyRTPIImage(image);
            }
            image = image->next;
        } while (image != NULL);
    }
    return 0;
}

/* Find and draw the object image for a given item index as it appears
   in the current room. Searches for images tagged as room objects. */
void DrawUSRoomObject(int item)
{
    USImage *image = USImages;
    if (image != NULL) {
        do {
            if ((image->usage == IMG_ROOM_OBJ ||
                image->usage == IMG_INV_AND_ROOM_OBJ) &&
                image->index == item) {
                DrawUSImage(image);
                return;
            }
            image = image->next;
        } while (image != NULL);
    }
}

/* The room image to show for the current location. The US disk releases
   (except Apple II) of The Incredible Hulk reuse images for several
   locations. */
static int USRoomImage(void)
{
    static const struct {
        uint8_t loc;
        uint8_t image;
    } hulk_aliases[] = {
        { 5, 3 }, { 6, 3 },     // Tunnel going outside
        { 7, 4 }, { 8, 4 },     // Field
        { 10, 9 }, { 11, 9 },   // Hole
        { 13, 2 }, { 14, 2 },   // Dome
        { 17, 16 }, { 18, 16 }, // warp
    };

    if (CurrentGame == HULK_US && USImages && USImages->systype != SYS_APPLE2) {
        for (size_t i = 0; i < sizeof hulk_aliases / sizeof hulk_aliases[0]; i++)
            if (hulk_aliases[i].loc == MyLoc)
                return hulk_aliases[i].image;
    }
    return MyLoc;
}

/* Game-specific composite image overlays.
   Some objects are drawn as extra layers on top of the room image,
   but only when they are present in specific rooms. These couldn't
   be handled by the generic DrawRoomObjectImages() because they
   use pseudo-item indices or conditional room checks. */
static void DrawGameSpecificOverlays(void)
{
    switch (CurrentGame) {
        case HULK_US:
            if (Items[HULK_ITEM_BIO_GEM].Location == MyLoc && MyLoc == Items[HULK_ITEM_BIO_GEM].InitialLoc)
                DrawUSRoomObject(70);
            if (Items[HULK_ITEM_WAX].Location == MyLoc && MyLoc == Items[HULK_ITEM_WAX].InitialLoc)
                DrawUSRoomObject(72);
            if (Items[HULK_ITEM_LARGE_HOLE].Location == MyLoc || Items[HULK_ITEM_LARGE_HOLE + 1].Location == MyLoc)
                DrawUSRoomObject(13);
            break;
        case COUNT_US:
            if (Items[COUNT_ITEM_MIRROR].Location == MyLoc && MyLoc == COUNT_ROOM_BATHROOM)
                DrawUSRoomObject(80);
            if (Items[COUNT_ITEM_SHEET_END].Location == MyLoc && MyLoc == COUNT_ROOM_PIT)
                DrawUSRoomObject(81);
            if (Items[COUNT_ITEM_COAT_OF_ARMS].Location == MyLoc && MyLoc == COUNT_ROOM_OUTSIDE_CASTLE)
                DrawUSRoomObject(82);
            if (Items[COUNT_ITEM_DAISIES].Location == MyLoc && MyLoc == COUNT_ROOM_FLOWER_BOX) // Apple II only
                DrawUSRoomObject(83);
            break;
        case VOODOO_CASTLE_US:
            if (Items[VOODOO_ITEM_BOARDS].Location == MyLoc && MyLoc == VOODOO_ROOM_CHIMNEY)
                DrawUSRoomObject(80);
            break;
        case RETURN_TO_PIRATES_ISLE:
            /* Dock scene uses layered compositing: underside of dock,
               then beam overlays, and nighttime sea/boat variants. */
            if (MyLoc == RTPI_ROOM_UNDER_DOCK) {
                DrawUSRoomObject(27); // Draw underside of dock when under the dock
                DrawUSRoom(31);       // Draw low beam (on top of underside of dock)
            } else if (MyLoc == RTPI_ROOM_SEA) {
                if (erkyrath_random() % 2 == 1) {
                    /* If a random chance of 1 in 2 succeeds, randomly draw one of two fish images
                       (but not if it would end up on top of the boat or dock) */
                    int fish = 33 + erkyrath_random() % 2;
                    if (!(fish == 34 && Items[RTPI_ITEM_BOAT_WEST].Location == MyLoc) &&
                        !(fish == 33 && Items[RTPI_ITEM_DOCK_EAST].Location == MyLoc)) {
                        DrawUSRoom(fish);
                    }
                }
                if (Items[RTPI_ITEM_DOCK].Location == DESTROYED) {
                    DrawUSRoom(29);       // Draw sea at night
                    if (Items[RTPI_ITEM_BOAT_WEST].Location == MyLoc) {
                        DrawUSRoom(32);   // Boat to the west at night
                    }
                } else if (Items[RTPI_ITEM_DOCK_UNDERSIDE].Location == MyLoc) {
                    /* Draw high beam */
                    DrawUSRoom(30);
                }
            } else if (MyLoc == RTPI_ROOM_UNDERSEA) {
                /* If a random chance of 1 in 2 succeeds, randomly draw one of three fish images */
                if (erkyrath_random() % 2 == 1) {
                    DrawUSRoom(35 + erkyrath_random() % 3);
                }
            }

            /* If it is night and we are out of battery, just make the image black by clearing the VDP */
            if (Items[RTPI_ITEM_DOCK].Location == DESTROYED && (!TestBitFlag(29) || CurrentCounter <= 0)) {
                ClearVDP();
            }

            break;
        default:
            break;
    }
}

/* Main drawing routine for US SAGA games. Redraws the graphics window
   with the current room image and any visible object overlays.

   Called whenever the display needs refreshing: after movement, taking/
   dropping items, restore/undo, window resize, preference changes, or
   lighting changes.

   Contains per-game special cases for composite images (e.g. The Hulk
   reuses room images for multiple locations; The Count has context-dependent
   object overlays; Return to Pirate's Isle composites dock/beam/boat
   layers). */
void LookUS(void)
{
    if (!Graphics || !(should_look_in_transcript || should_draw_image))
        return;

    if (Top != Bottom)
        glk_window_clear(Top);

    if (should_draw_image) {
        glk_window_clear(Graphics);
        if (!DrawUSRoom(USRoomImage())) {
            return;
        }

        DrawRoomObjectImages();
        DrawGameSpecificOverlays();
        DrawImageOrVector();
    }
    should_draw_image = 0;
}

/* Display the inventory screen. Room image 98 is used as the inventory
   background. If no such image exists, falls back to redrawing
   the normal room view. Carried item images are composited on top. */
void InventoryUS(void)
{
    if (!Graphics || !HasGraphics())
        return;
    glk_window_clear(Graphics);
    if (!DrawUSRoom(INVENTORY_IMAGE)) {
        should_draw_image = 1;
        LookUS();
        return;
    }
    DrawInventoryImages();
    DrawImageOrVector();
    if (!showing_inventory) {
        showing_inventory = 1;
        Output(sys[HIT_ENTER]);
        HitEnter();
    }
}

/* Quick plausibility check on parsed header values to reject files that
   aren't actually Scott Adams games. Verifies that item, action, word,
   room, and message counts fall within reasonable bounds. */
static int SanityCheckScottFreeHeader(void)
{
    int16_t v = header[1]; // items
    if (v < 10 || v > 500)
        return 0;
    v = header[2]; // actions
    if (v < 100 || v > 500)
        return 0;
    v = header[3]; // word pairs
    if (v < 48 || v > 200)
        return 0;
    v = header[4]; // rooms
    if (v < 10 || v > 100)
        return 0;
    v = header[5]; // messages
    if (v < 10 || v > 255)
        return 0;

    return 1;
}

/* Skip over 'count' bytes of data that we don't need to parse
   (e.g. lookup tables). In debug builds, dumps the skipped values. */
uint8_t *Skip(uint8_t *ptr, int count, uint8_t *eof)
{
#if (DEBUG_PRINT)
    for (int i = 0; i < count && ptr + i + 1 < eof; i += 2) {
        uint16_t val = READ_LE_UINT16(ptr + i);
        debug_print("Unknown value %d: %d (%x)\n", i / 2, val, val);
    }
#else
    (void)eof;
#endif
    return ptr + count;
}

/* Parse the game file header and allocate all game data arrays.

   When dict_start > 0, we're probing for the UK release of Hulk: the
   dictionary position is known, so we calculate a baseline offset to
   compensate for any extra data prepended to the file.

   When dict_start == 0, this is a standard US-format binary database.
   The first 0x38 bytes contain version and adventure number fields, which are
   used to identify the specific game title. */
static uint8_t *LoadHeader(uint8_t *ptr, size_t length, GameInfo info, int dict_start) {
    Header hdr;

    size_t offset;

    uint8_t *data = ptr;

    if (dict_start) {
        /* UK Hulk detection: compute file offset adjustment from known dictionary position */
        file_baseline_offset = dict_start - info.start_of_dictionary - 645;
        debug_print("LoadBinaryDatabase: file baseline offset:%d\n",
                    file_baseline_offset);
        offset = info.start_of_header + file_baseline_offset;
        if (offset < length)
            ptr = data + offset;
        else
            return NULL;
    } else {
        /* Standard US binary: scan the preamble for version and adventure number.
           These are the first two values under 500 in the 0x38-byte preamble. */
        if (length < 0x38)
            return NULL;
        int version = 0;
        int adventure_number = 0;
        for (int i = 0; i < 0x38; i += 2) {
            int value = READ_LE_UINT16(ptr + i);
            if (value < 500) {
                if (version == 0)
                    version = value;
                else {
                    adventure_number = value;
                    break;
                }
            }
        }
        debug_print("Version: %d\n", version);
        debug_print("Adventure number: %d\n", adventure_number);
        if (adventure_number == 0)
            return NULL;
        /* Map known version/adventure pairs to specific game IDs */
        if (version == 127 && adventure_number == 1)
            CurrentGame = HULK_US;
        else if (version == 126) {
            if (adventure_number == 2)
                CurrentGame = HULK_US_PREL;
            else if (adventure_number == 13)
                CurrentGame = CLAYMORGUE_US_126;
        } else
            CurrentGame = adventure_number;
        Game->type = US_VARIANT;
        ptr = Skip(ptr, 0x38, data + length);
    }

    if (ptr == NULL || ptr + 30 > data + length)
        return NULL;

    ptr = ReadHeader(ptr);

    ParseHeader(header, US_HEADER, &hdr);
    PrintHeaderInfo(header, &hdr);

    if (!SanityCheckScottFreeHeader())
        return NULL;

    SetGameHeader(&hdr);
    AllocateGameData();

    /* When probing for UK Hulk, verify the parsed header matches the
       expected values from the game database. A mismatch means this
       isn't the game we're looking for. */
    if (dict_start) {
        if (header[0] != info.word_length
            || header[1] != info.number_of_words
            || header[2] != info.number_of_actions
            || header[3] != info.number_of_items
            || header[4] != info.number_of_messages
            || header[5] != info.number_of_rooms
            || header[6] != info.max_carried) {
            return NULL;
        }
    }
    return ptr;
}

/* Locate and parse the dictionary. The dictionary always starts with
   the word "ANY" (the universal noun), so we scan forward until we
   find that marker, then hand off to ReadUSDictionary. */
static uint8_t *ParseDictionary(uint8_t *ptr, uint8_t *endptr) {
    while (!(*ptr == 'A' && *(ptr + 1) == 'N' && *(ptr + 2) == 'Y') && ptr < endptr)
        ptr++;

    if (ptr >= endptr)
        return NULL;

    return ReadUSDictionary(ptr);
}

/* Read and return a lenght-prefixed string and update the pointer.
   If string length is 0, return a period string literal (".") */
static char *ReadPascalString(uint8_t **inptr, uint8_t *endptr) {
    char *string = ".";
    uint8_t *ptr = *inptr;
    uint8_t string_length = *ptr++;
    if (string_length != 0 && ptr + string_length < endptr) {
        string = MemAlloc(string_length + 1);
        memcpy(string, ptr, string_length);
        string[string_length] = 0;
        ptr += string_length;
    }
    *inptr = ptr;
    return string;
}

/* Parse room descriptions from the binary data. Each room is stored as
   a length-prefixed string (1 byte length + that many characters). */
uint8_t *ParseRooms(uint8_t *ptr, uint8_t *endptr, int number_of_rooms) {
    Room *rp = Rooms;
    for (int counter = 0; counter <= number_of_rooms; counter++) {
        rp->Text = ReadPascalString(&ptr, endptr);
        debug_print("Room %d: \"%s\"\n", counter, rp->Text);
        rp++;
    }
    return ptr;
}

/* Parse action messages from the binary data. Same length-prefixed
   string format as rooms. */
uint8_t *ParseMessages(uint8_t *ptr, uint8_t *endptr, int number_of_messages) {
    int counter = 0;
    for (counter = 0; counter <= number_of_messages; counter++) {
        Messages[counter] = ReadPascalString(&ptr,endptr);
        debug_print("Message %d: \"%s\"\n", counter, Messages[counter]);
    }
    return ptr;
}

/* Parse item descriptions and initial locations from binary data.

   Item descriptions are length-prefixed strings. An item name containing
   a '/' delimiter has an auto-get/drop word after it (e.g. "Lamp/LAM").
   "//" or "[slash]*" means no auto-get word.

   After all item strings, a separate table of [stride]-byte entries gives each
   item's starting location. */
uint8_t *ParseItems(uint8_t *ptr, uint8_t *endptr, int number_of_items, int stride) {
    for (int counter = 0; counter <= number_of_items && ptr < endptr; counter++) {
        Items[counter].Text = ReadPascalString(&ptr, endptr);

        ParseItemSlashAutoGet(counter);
        if (Items[counter].AutoGet != NULL && ptr < endptr)
            ptr++;

        debug_print("Item %d: %s\n", counter, Items[counter].Text);
        if (Items[counter].AutoGet)
            debug_print("Autoget:%s\n", Items[counter].AutoGet);
    }

    ptr++;
    for (int ct = 0; ct <= number_of_items && ptr < endptr; ct++) {
        Items[ct].Location = *ptr;
        Items[ct].InitialLoc = Items[ct].Location;
        debug_print("Item %d (%s) start location: %d\n", ct, Items[ct].Text, Items[ct].Location);
        ptr += stride;
    }

    return ptr;
}

/* Parse the action table from binary data.

   The US binary format stores action data in a columnar layout rather
   than row-by-row: all verb bytes come first (one per action), then all
   noun bytes, then opcode bytes (2 pairs of 2 columns), then
   condition words (5 columns of 16-bit values). Each column has
   (number_of_actions + 1) entries.

   The verb and noun are packed into Vocab as (verb * 150 + noun).
   Action opcodes are similarly packed as (value * 150 + value2).
   Conditions are 16-bit words encoding condition type and argument. */
uint8_t *ParseActions(uint8_t *ptr, uint8_t *data, size_t datalength, int number_of_actions, int big_endian) {
    size_t base = ptr - data;
    size_t stride = number_of_actions + 1;

    /* The byte columns region spans 6 columns of 'stride' bytes each.
       The condition region follows: 5 columns of 'stride' 16-bit words. */
    size_t cond_base = base + 6 * stride;
    size_t cond_stride = stride * 2;

    /* Verify the entire action table fits within the data */
    size_t end_of_table = cond_base + 5 * cond_stride;
    if (end_of_table > datalength)
        return NULL;

    for (int ct = 0; ct <= number_of_actions; ct++) {
        /* Column 0: verbs, Column 1: nouns */
        int verb = data[base + ct];
        int noun = data[base + ct + stride];
        debug_print("Action %d: verb:%d noun:%d\n", ct, verb, noun);

        Actions[ct].Vocab = verb * 150 + noun;

        /* Columns 2-5: two opcode pairs (each pair is two byte columns) */
        for (int j = 0; j < 2; j++) {
            int value  = data[base + ct + (2 + j * 2) * stride];
            int value2 = data[base + ct + (3 + j * 2) * stride];
            Actions[ct].Opcode[j] = 150 * value + value2;
            debug_print("Action %d: Opcode[%d]: %d %d\n", ct, j, value, value2);
        }

        /* 5 columns of 16-bit condition words */
        for (int j = 0; j < 5; j++) {
            size_t off = cond_base + ct * 2 + j * cond_stride;
            Actions[ct].Condition[j] = big_endian
                ? READ_BE_UINT16(data + off)
                : READ_LE_UINT16(data + off);
            debug_print("Action %d: Condition %d: %d\n", ct, j, Actions[ct].Condition[j]);
        }
    }

    return data + end_of_table;
}

/* Parse room exit connections from binary data.
   Like the action table, exits are stored in columnar layout: all North
   exits for every room, then all South exits, etc. (6 directions total).
   Each exit is a [stride]-byte value giving the destination room number. */
uint8_t *ParseRoomConnections(uint8_t *ptr, uint8_t *endptr, int number_of_rooms, int stride) {
    for (int j = 0; j < 6; j++) {
        int counter = 0;
        Room *rp = Rooms;

        while (counter <= number_of_rooms && ptr < endptr) {
            rp->Image = 255;
            rp->Exits[j] = *ptr;
            debug_print("Room %d (%s) exit %d (%s): %d\n", counter, Rooms[counter].Text, j, Nouns[j + 1], rp->Exits[j]);
            ptr += stride;
            counter++;
            rp++;
        }
    }
    return ptr;
}

/* Additional parsing for the UK release of Questprobe featuring The Hulk.
   The UK version stores room-to-image and item-to-image mapping tables
   at known offsets that the standard US parser doesn't handle. */
static GameIDType HulkUKSpecificParsing(uint8_t *ptr, uint8_t *endptr, GameInfo info) {
// MARK: room images
    size_t offset;

    if (SeekIfNeeded(info.start_of_room_image_list, &offset, &ptr) == 0) {
        FreeDatabase();
        return UNKNOWN_GAME;
    }

    Room *rp = Rooms;

    for (int i = 0; i <= GameHeader.NumRooms && ptr < endptr; i++) {
        rp->Image = *ptr++;
        rp++;
    }

// MARK: item images

    if (SeekIfNeeded(info.start_of_item_image_list, &offset, &ptr) == 0) {
        FreeDatabase();
        return UNKNOWN_GAME;
    }

    Item *ip = Items;

    for (int i = 0; i <= GameHeader.NumItems; i++) {
        ip->Image = 0xff;
        ip++;
    }

    /* Item image indices are stored as a list of item numbers, terminated
       by 0xFF. Image numbers start at 10 and increment sequentially. */
    int index, image = 10;

    do {
        index = *ptr++;
        if (index != 0xff)
            Items[index].Image = image++;
    } while (index != 0xff && ptr < endptr);

    /* The C64 release has 15 bytes of stray text (a disk command, "N0:B"
       + eight characters + "SU") lying over the third condition word of
       action lines 184 to 191, which costs line 185 its second parameter.
       Put back the values that the Spectrum release has there. */
    if (info.gameID == HULK_C64 && GameHeader.NumActions >= 191) {
        for (int i = 184; i <= 191; i++)
            Actions[i].Condition[2] = 0;
        Actions[185].Condition[2] = 21 * 20; /* parameter 21 */
    }

    return info.gameID;
}

GameIDType FreeGameResources(void) {
    FreeDatabase();
    return UNKNOWN_GAME;
}

/* Top-level parser for US-format SAGA binary game files.

   Parses the file sequentially: header, dictionary, rooms, messages,
   items, actions, and room connections. Returns the detected game ID
   on success, or UNKNOWN_GAME on failure (freeing any partial parse).

   When dict_start > 0, this is being called as a probe for the UK
   Hulk variant — additional image mapping tables are parsed at the end. */
GameIDType LoadBinaryDatabase(uint8_t *data, size_t length, GameInfo info, int dict_start) {
    FreeDatabase();
    uint8_t *ptr = data;

    uint8_t *endptr =  data + length - 2;

    // Parse the header
    ptr = LoadHeader(ptr, length, info, dict_start);
    if (ptr == NULL || ptr >= endptr) {
        return FreeGameResources();
    }

    // Parse the dictionary
    ptr = ParseDictionary(ptr, endptr);
    if (ptr == NULL || ptr >= endptr) {
        return FreeGameResources();
    }

    // Parse rooms
    ptr = ParseRooms(ptr, endptr, GameHeader.NumRooms);
    if (ptr == NULL || ptr >= endptr) {
        return FreeGameResources();
    }

    // Parse messages
    ptr = ParseMessages(ptr, endptr, GameHeader.NumMessages);
    if (ptr == NULL || ptr >= endptr) {
        return FreeGameResources();
    }

    // Parse items
    ptr = ParseItems(ptr, endptr, GameHeader.NumItems, 2);
    if (ptr == NULL || ptr >= endptr) {
        return FreeGameResources();
    }

    // Skip item strings lookup table
    ptr = Skip(ptr, (GameHeader.NumItems + 1) * 4, data + length);

    // Parse actions
    ptr = ParseActions(ptr, data, length, GameHeader.NumActions, 0);
    if (ptr == NULL || ptr >= endptr) {
        return FreeGameResources();
    }

    // Skip room descriptions lookup table
    ptr = Skip(ptr, (GameHeader.NumRooms + 1) * 2, data + length);

    endptr += 2;

    ptr = ParseRoomConnections(ptr, endptr, GameHeader.NumRooms, 2);
    if (ptr == NULL || ptr > endptr) {
        return FreeGameResources();
    }

    // The dict_start parameter is only set if we are
    // trying to detect the Adventure International UK
    // release of Questprobe featuring The Incredible Hulk.
    if (dict_start) {
        return HulkUKSpecificParsing(ptr, endptr, info);
    }

    /* Two C64 images of Voodoo Castle (the "Cracked by Toko" disk and the
       T64) have one damaged byte, 0x55 for 0x01, in the third condition
       word of action line 27. It turns parameter 21 into a test for room
       1096, so the line never fires. Put back the value that the other
       releases have there. */
    if (CurrentGame == VOODOO_CASTLE_US && GameHeader.NumActions >= 27
        && Actions[27].Condition[2] == 0x55a4)
        Actions[27].Condition[2] = 21 * 20; /* parameter 21 */

    return CurrentGame;
}


/* Compare two filenames backward from the end, ignoring file extensions.
   This handles the case where game files may live in different directories
   (different path prefixes) but share the same base name. Compares only
   as many characters as the shorter base name has. Returns 1 if they match. */
static int CompareFilenames(const char *str1, size_t length1, const char *str2, size_t length2)
{
    /* Strip file extensions by scanning backward for '.' */
    while (length1 > 0 && str1[length1] != '.') {
        length1--;
    }
    while (length2 > 0 && str2[length2] != '.') {
        length2--;
    }
    size_t length = MIN(length1, length2);
    if (length <= 0)
        return 0;
    /* Compare characters backward from the end of each base name */
    for (int i = length; i > 0; i--) {
        if (str1[length1--] != str2[length2--]) {
            return 0;
        }
    }
    return 1;
}

// Give the companion file name the file extension of the game file.
// This makes the companion file logic work even if the player has renamed the files to, say, end with ".saga"
// (well, as long as both files have been renamed in the same way.)
static char *AddGameFileExtension(const char *companion_name, size_t *namelength,
                           const char *game_path, size_t gamepathlen) {
    if (!companion_name || !namelength || !game_path) {
        fprintf(stderr, "AddGameFileExtension: NULL parameter\n");
        return NULL;
    }

    // Find extension in game_path
    const char *game_ext = strrchr(game_path, '.');
    // Find extension in filename
    const char *companion_ext = strrchr(companion_name, '.');

    //(It should not really be possible for any of them
    // to not have an extension)
    size_t extension_length = game_ext ? (gamepathlen - (game_ext - game_path)) : 0;
    size_t base_length = companion_ext ? companion_ext - companion_name : *namelength;

    *namelength = base_length + extension_length;

    // Allocate result
    char *result = MemAlloc(*namelength + 1);

    // Build result string
    memcpy(result, companion_name, base_length + 1);
    if (extension_length > 0) {
        // Copy includes terminating 0
        memcpy(result + base_length, game_ext, extension_length + 1);
    }
    return result;
}

// Helper to get filename length
static size_t get_filename_length(const char *filename, size_t length_field)
{
    return (length_field != 0) ? length_field : strlen(filename);
}

/* Search a database of filename pairs for a match against the current game file.
   Each entry in 'list' is a pair of filenames that belong together (e.g. game
   data file and its companion graphics file). If we match either filename in a
   pair, we return a copy of the other one (the "companion"), with the game
   file's extension applied to it. Returns NULL if no match is found. */
static char *LookForCompanionFilenameInDatabase(const pairrec list[][2], size_t game_filename_len, size_t *out_companion_len)
{
    for (int i = 0; list[i][0].filename != NULL; i++) {
        for (int j = 0; j < 2; j++) {
            const char *candidate = list[i][j].filename;
            size_t candidate_len = get_filename_length(candidate, list[i][j].stringlength);
            if (CompareFilenames(game_file, game_filename_len, candidate, candidate_len)) {
                int companion_idx = 1 - j;
                const char *companion = list[i][companion_idx].filename;
                *out_companion_len = get_filename_length(companion, list[i][companion_idx].stringlength);
                return AddGameFileExtension(companion, out_companion_len, game_file, game_filename_len);
            }
        }
    }
    return NULL;
}

/* Look up the companion filename for the current game file in a database
   of known filename pairs, and return its full path (using the same
   directory as the game file). Does not verify whether the file exists.

   For example, if the game file is "/Games/HULK.DAT" and the companion
   database says HULK.DAT pairs with HULK.PIC, this returns "/Games/HULK.PIC". */
void SwapWithCompanionFile(uint8_t **sf, size_t *extent, uint8_t **companion, size_t *companionsize)
{
    uint8_t *temp = *companion;
    size_t tempsize = *companionsize;
    *companion = *sf;
    *companionsize = *extent;
    *sf = temp;
    *extent = tempsize;
}

/* Unlink and free the empty node that the image loaders leave at the end
   of the USImages list. */
void FreeTrailingEmptyImage(USImage *image)
{
    if (image != USImages) {
        image->previous->next = NULL;
        free(image);
    } else {
        free(USImages);
        USImages = NULL;
    }
}

void FreeUSImagesIfEmpty(void)
{
    if (USImages != NULL && USImages->next == NULL && USImages->imagedata == NULL) {
        free(USImages);
        USImages = NULL;
    }
}

char *LookInDatabase(const pairrec list[][2], const char *game_path, size_t pathlen)
{
    size_t resultlen;
    char *foundname = LookForCompanionFilenameInDatabase(list, pathlen, &resultlen);
    if (foundname != NULL) {
        /* Extract the directory portion of the game file path */
        const char *file_delimiter_position = strrchr(game_file, '/');
        if (file_delimiter_position == NULL)
            file_delimiter_position = strrchr(game_path, '\\');
        size_t stringlen;
        if (file_delimiter_position == NULL) {
            /* No directory separator found — file is in the working directory */
            return foundname;
        } else {
            stringlen = file_delimiter_position - game_path + 1;
        }
        /* Prepend the game file's directory to the companion filename */
        size_t newlen = stringlen + resultlen;
        char *path = MemAlloc(newlen + 1);
        memcpy(path, game_path, stringlen);
        memcpy(path + stringlen, foundname, resultlen + 1);
        free(foundname);
        return path;
    }

    return NULL;
}
