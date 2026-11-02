//
//  graphics.c
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Palette code based on code by David Lodge
//  with help from Paul David Doherty (including the colour code)
//
//  Original code at https://github.com/tautology0/textadventuregraphics
//  See also http://aimemorial.if-legends.org/gfxbdp.html
//

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "glk.h"
#include "aiukpalette.h"
#include "taylor.h"
#include "taylordraw.h"
#include "utility.h"
#include "irmak.h"
#include "extracttape.h"

#include "graphics.h"

glui32 pixel_size;
glui32 x_offset;

static uint8_t *EndOfGraphicsData;

#pragma mark Some common functions

/* What the graphics window shows, when that is known: the cells of
   imagebuffer[] as DrawBufferedPicture() last painted them, and the
   window and geometry it painted them with. Anything else that draws
   in the window, clears it or replaces it makes the picture unknown. */
static struct {
    int known;
    int painting; /* DrawBufferedPicture() is the one drawing */
    winid_t window;
    glui32 pixel_size;
    glui32 x_offset;
    uint8_t cells[IRMAK_IMGSIZE][IRMAK_CELL_BYTES];
} Shown;

/* To be called by anything that changes what the graphics window shows
   without going through DrawBufferedPicture(). */
void GraphicsWindowChanged(void)
{
    Shown.known = 0;
}

/* Whether the window still shows the picture last painted from the
   buffer. Then there is nothing to clear before painting the next. */
int BufferedPictureShown(void)
{
    return Shown.known && Graphics != NULL && Shown.window == Graphics
        && Shown.pixel_size == pixel_size && Shown.x_offset == x_offset;
}

/* Put imagebuffer[] on the screen. Only the cells that differ from the
   picture already there are painted; most turns leave the picture as it
   was, and an animation frame changes a few cells of it. */
void DrawBufferedPicture(void)
{
    if (!Graphics)
        return;
    Shown.painting = 1;
    DrawIrmakBufferChanges(BufferedPictureShown() ? Shown.cells : NULL);
    Shown.painting = 0;

    memcpy(Shown.cells, imagebuffer, sizeof Shown.cells);
    Shown.window = Graphics;
    Shown.pixel_size = pixel_size;
    Shown.x_offset = x_offset;
    Shown.known = 1;
}

void PutPixel(glsi32 x, glsi32 y, int32_t color)
{
    if (!Graphics)
        return;

    if (!Shown.painting)
        GraphicsWindowChanged();
    glk_window_fill_rect(Graphics, pal[color], x * pixel_size + x_offset,
        y * pixel_size, pixel_size, pixel_size);
}

void RectFill(int32_t x, int32_t y, int32_t width, int32_t height,
    int32_t color)
{
    if (!Graphics)
        return;

    if (!Shown.painting)
        GraphicsWindowChanged();
    glk_window_fill_rect(Graphics, pal[color], x * pixel_size + x_offset,
        y * pixel_size, width * pixel_size, height * pixel_size);
}

/* There is an unused image (46) of the cannon
 on the road (room 34) but it has the wrong background
 colour. We fix that here.
 */
void PatchAndDrawQP3Cannon(void)
{
    /* The cells (column, row) whose background is wrongly bright */
    static const struct { int x, y; } bright_cells[] = {
        { 25, 8 }, { 25, 9 }, { 26, 9 }, { 27, 9 }
    };
    DrawPictureNumber(46, 1);
    for (size_t i = 0; i < sizeof bright_cells / sizeof bright_cells[0]; i++)
        imagebuffer[IRMAK_IMGWIDTH * bright_cells[i].y + bright_cells[i].x][IRMAK_CELL_ATTR] &= ~BRIGHT_FLAG;
    DrawBufferedPicture();
}


#pragma mark Patching

typedef struct {
    GameIDType id;
    int picture_number;
    int offset;
    int number_of_bytes;
    const char *patch;
} image_patch;

static const image_patch image_patches[] = {
    { UNKNOWN_GAME, 0, 0, 0, "" },
    { QUESTPROBE3, 55, 604, 3, "\xff\xff\x82" },
    { QUESTPROBE3, 56, 357, 46, "\x79\x81\x78\x79\x7b\x83\x47\x79\x82\x78\x79\x7b\x83\x47\x79\x83"
        "\x78\x79\x7b\x81\x79\x47\x79\x84\x7b\x83\x47\x79\x84\x58\x83\x47"
        "\x7a\x84\x5f\x18\x81\x5f\x47\x50\x84\x5f\x18\x81\x5f\x47" },
    { NUMGAMES, 0, 0, 0, "" },
};

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
    const image_patch *patch = &image_patches[patch_number];
    for (int i = 0; i < patch->number_of_bytes; i++) {
        const char newval = patch->patch[i];
        offset[i + patch->offset] = (uint8_t)newval;
    }
}

#pragma mark Extract images from file

/* Each QP3 image is preceded by a 4-byte header: width, height, xoff, yoff. */
#define IMAGE_HEADER_BYTES 4

/* Patched buffer sizes for two QP3 images that are damaged in many
   distributed copies. We hold a private copy so patching doesn't mutate
   the rest of the loaded file image. */
#define Q3_WIND_TUNNEL_SIZE 608  /* picture 17 (wind tunnel) */
#define Q3_EGG_BIOGEM_SIZE  403  /* picture 56 (egg and bio gem) */

#define Q3_TILE_COUNT 246

/* The irmak picture format version, the one Scott gives Robin of Sherwood
   and Seas of Blood. Irmak treats every version from 3 up alike. */
#define TAYLOR_IRMAK_VERSION 4

static void ExtractSingleQ3Image(Image *img, int picture_number, size_t base, size_t offsets, size_t imgdata)
{
    img->imagedata = NULL;

    if (base + picture_number >= FileImageLen)
        return;
    size_t offset_addr = (FileImage[base + picture_number] & 0x7f) * 2 + offsets;
    if (offset_addr + 1 >= FileImageLen)
        return;
    size_t image_addr = imgdata + READ_LE_UINT16(FileImage + offset_addr);
    if (image_addr + IMAGE_HEADER_BYTES >= FileImageLen)
        return;

    uint8_t *pos = &FileImage[image_addr];
    img->width = *pos++;
    img->height = *pos++;
    img->xoff = *pos++;
    img->yoff = *pos++;
    img->imagedata = pos;
    img->datasize = EndOfGraphicsData - pos;

    /* In some Spectrum versions, the "wind tunnel" image
       is damaged. As the game contains several (wasteful)
       copies of this image and different ones are broken in
       different ways, we just make use the data for number 17
       for all of them (after patching it.)*/
    if (picture_number == 17) {
        img->imagedata = MemCalloc(Q3_WIND_TUNNEL_SIZE);
        img->datasize = Q3_WIND_TUNNEL_SIZE;
        memcpy(img->imagedata, pos, MIN(EndOfGraphicsData - pos, Q3_WIND_TUNNEL_SIZE));
        /* The patch is filed under 55, the last of the copies */
        int patch = FindImagePatch(QUESTPROBE3, 55, 0);
        Patch(img->imagedata, patch);
    } else if (picture_number == 55 || (picture_number >= 18 && picture_number <= 20)) {
        img->imagedata = images[17].imagedata;
        img->datasize = images[17].datasize;
    } else if (picture_number == 56) {
        /* The last image, 56, the egg and the bio gem,
           is broken in many versions, especially the colours.
           We fix it here. */
        img->imagedata = MemCalloc(Q3_EGG_BIOGEM_SIZE);
        img->datasize = Q3_EGG_BIOGEM_SIZE;
        memcpy(img->imagedata, pos, MIN(EndOfGraphicsData - pos, Q3_EGG_BIOGEM_SIZE));
        int patch = FindImagePatch(QUESTPROBE3, 56, 0);
        Patch(img->imagedata, patch);
    }
}

/* Returns 0 if the picture tables are not in the file. */
static int ExtractQ3Images(Image *img)
{
    size_t base = FindCode("\x00\x01\x01\x02\x03\x04\x05\x06\x02\x02", 0, 10);
    size_t offsets = FindCode("\x00\x00\xa7\x02\xa7\x03\xb9\x08\xd7\x0b", 0, 10);
    if (base == NOT_FOUND || offsets == NOT_FOUND) {
        fprintf(stderr, "Cannot find the Questprobe 3 picture tables.\n");
        return 0;
    }
    size_t imgdata = FindCode("\x20\x0c\x00\x00\x8a\x01\x44\xa0\x17\x8a", offsets, 10);
    if (imgdata == NOT_FOUND) {
        fprintf(stderr, "Cannot find the Questprobe 3 picture data.\n");
        return 0;
    }

    for (int picture_number = 0; picture_number < Game->number_of_pictures; picture_number++) {
        ExtractSingleQ3Image(img, picture_number, base, offsets, imgdata);
        img++;
    }
    return 1;
}

/* Bytes of the picture data */
#define IMAGE_END 0xfe /* Ends each picture */
#define REPEAT_COMMAND 0x82 /* irmak COMMAND_BIT | REPEAT_BIT: count, then tile */

/* Tokens of the run-length compression, see DecompressEarlierType() */
#define TOKEN_REPEAT_1 0xef
#define TOKEN_REPEAT_2 0xee
#define TOKEN_REPEAT_3 0xeb
#define TOKEN_REPEAT_N 0xf3 /* count follows */
#define TOKEN_COPY 0xfa /* length + 1 and number of copies follow */

/* Replace the token just written at *index - 1 with the three-byte
   "repeat" instruction it stands for, if there is room for it. */
static void RepeatOpcode(uint8_t *instructions, int capacity, uint8_t repeatcount, int *index)
{
    int i = *index - 1;
    if (i + 3 > capacity)
        return;
    instructions[i++] = REPEAT_COMMAND;
    instructions[i++] = repeatcount;
    instructions[i++] = 0;
    *index = i;
}

static size_t FindTilesStart(void)
{
    /* Look for the tile data */
    size_t pos = FindCode("\x00\x00\x00\x00\x00\x00\x00\x00\x80\x80\x80\x80\x80\x80\x80\x80\x40\x40\x40\x40\x40\x40\x40\x40", 0, 24);
    if (pos == NOT_FOUND) {
        fprintf(stderr, "Cannot find tile data.\n");
        return pos;
    }
    debug_print("Found tiles at pos %zx\n", pos);
    return pos;
}

/* The image format stores width and height in a single byte, so max width is 16. */
/* Blizzard Pass cheats and adds 16 to the width of a hardcoded list of images. */
static void AdjustBlizzardPassImageWidth(Image *img, int picture_number)
{
    switch (picture_number) {
        case 13:
        case 15:
        case 17:
        case 34:
        case 85:
        case 111:
            img->width += 16;
            break;
        default:
            break;
    }
}

#define MAX_INSTRUCTIONS 2048

/* A number of repeating patterns in the image drawing code (may) have been
   replaced with single tokens. Here we replace any tokens with their
   corresponding patterns. */
static int DecompressHemanType(uint8_t *instructions, int capacity, uint8_t **outpos)
{
    int index = 0;
    uint8_t *pos = *outpos;
    size_t patterns_lookup = Game->image_patterns_lookup + FileBaselineOffset;
    do {
        if (index >= capacity || pos >= EndOfGraphicsData) {
            index = MIN(index, capacity - 1);
            break;
        }
        instructions[index++] = *pos;
        for (int i = 0; i < Game->number_of_patterns; i++) {
            /* Look for a pattern marker at pos, and if found,
               insert the corresponding pattern into the instructions array. */
            if (patterns_lookup + i < FileImageLen &&
                *pos == FileImage[patterns_lookup + i]) {
                index--;
                size_t baseoffset = patterns_lookup + Game->number_of_patterns + i * 2;
                if (baseoffset >= FileImageLen - 1)
                    break;
                size_t newoffset = READ_LE_UINT16(FileImage + baseoffset) - ZX_RAM_BASE + FileBaselineOffset;
                while (newoffset < FileImageLen && index < capacity
                    && FileImage[newoffset] != Game->pattern_end_marker) {
                    instructions[index++] = FileImage[newoffset++];
                }
                break;
            }
        }
        pos++;
    } while (pos < EndOfGraphicsData && *pos != IMAGE_END);

    *outpos = pos;
    return index;
}


/* Run-length compression of draw instructions (used in some ZX Spectrum
   versions):
   A number of common "repeat next byte n times" instruction patterns
   beginning with REPEAT_COMMAND have been replaced with single tokens.
   TOKEN_COPY means "copy the next n bytes m times", which is complicated
   by the fact that those n bytes may themselves contain repeat tokens, so
   we call this function again recursively with the bytes to copy.

   See http://aimemorial.if-legends.org/gfxbdp.html and also
   PerformTileTransformations() in irmak.c for more information
   about how repetitions in the tile-based graphics are encoded.
 */

static int DecompressEarlierType(uint8_t *instructions, int capacity, uint8_t **outpos,
    const uint8_t *end, int recursion_guard)
{
    int index = 0;
    uint8_t *pos = *outpos;
    if (capacity < 1)
        return 0;
    do {
        if (index >= capacity || pos >= end) {
            index = MIN(index, capacity - 1);
            break;
        }
        instructions[index++] = *pos;
        switch (*pos) {
            case TOKEN_REPEAT_1:
                RepeatOpcode(instructions, capacity, 1, &index);
                break;
            case TOKEN_REPEAT_2:
                RepeatOpcode(instructions, capacity, 2, &index);
                break;
            case TOKEN_REPEAT_3:
                RepeatOpcode(instructions, capacity, 3, &index);
                break;
            case TOKEN_REPEAT_N:
                if (end - pos < 2)
                    break;
                pos++;
                RepeatOpcode(instructions, capacity, *pos, &index);
                break;
            case TOKEN_COPY:
                if (recursion_guard || end - pos < 3)
                    break;
                /* Call this function recursively with copied_bytes,
                   an IMAGE_END-terminated array of bytes to repeat. */
                index--;
                pos++;
                int numbytes = *pos - 1;
                if (numbytes < 0)
                    numbytes = 0;
                pos++;
                int copies = *pos;
                if (numbytes > end - (pos + 1))
                    numbytes = (int)(end - (pos + 1));
                uint8_t *copied_bytes = MemAlloc(numbytes + 1);
                memcpy(copied_bytes, pos + 1, numbytes);
                uint8_t *stored_pointer = copied_bytes;
                copied_bytes[numbytes] = IMAGE_END;
                for (int i = 0; i < copies; i++) {
                    index += DecompressEarlierType(&instructions[index], capacity - index, &copied_bytes,
                        stored_pointer + numbytes + 1, 1);
                    copied_bytes = stored_pointer;
                    if (index >= capacity) {
                        break;
                    }
                }
                free(stored_pointer);
                break;
            default:
                break;
        }
        pos++;
    } while (pos < end && *pos != IMAGE_END);

    *outpos = pos;
    return index;
}

static uint8_t *ExtractImage(Image *img, uint8_t *pos) {
    uint8_t instructions[MAX_INSTRUCTIONS];
    int instructions_length;
    if (Version == HEMAN_TYPE) {
        instructions_length = DecompressHemanType(instructions, MAX_INSTRUCTIONS, &pos);
    } else {
        instructions_length = DecompressEarlierType(instructions, MAX_INSTRUCTIONS, &pos, EndOfGraphicsData, 0);
    }
    /* Leave room for the terminator */
    if (instructions_length >= MAX_INSTRUCTIONS)
        instructions_length = MAX_INSTRUCTIONS - 1;
    instructions[instructions_length++] = IMAGE_END;

    img->imagedata = MemAlloc(instructions_length);
    img->datasize = instructions_length;
    memcpy(img->imagedata, instructions, instructions_length);
    /* Step over the terminator, unless the data ran out before one */
    return pos < EndOfGraphicsData ? pos + 1 : pos;
}

/* Carry on without pictures, and say so in the game window: stderr is
   not seen in the app. */
static void EnterTextOnlyMode(const char *reason)
{
    fprintf(stderr, "InitGraphics: %s. Entering text-only mode.\n", reason);
    NoGraphics = 1;
    if (Bottom)
        Display(Bottom, "[The pictures in this file could not be read. Playing without graphics.]\n\n");
}

/* The byte at offset of the game file, or NULL if the file is shorter. */
static uint8_t *SeekToPos(size_t offset)
{
    if (offset > FileImageLen || FileImage == NULL)
        return NULL;
    return FileImage + offset;
}

void InitGraphics(void)
{
    if (images != NULL || NoGraphics)
        return;

    if (Game->number_of_pictures == 0) {
        NoGraphics = 1;
        return;
    }

    EndOfGraphicsData = FileImage + FileImageLen;

    if (palchosen == NO_PALETTE) {
        palchosen = Game->palette;
    }

    if (palchosen == NO_PALETTE) {
        EnterTextOnlyMode("invalid palette");
        return;
    }

    DefinePalette();

    size_t tiles_start = FindTilesStart();
    if (tiles_start == NOT_FOUND)
        tiles_start = Game->start_of_tiles + FileBaselineOffset;
    size_t image_blocks_start_address = Game->start_of_image_blocks + FileBaselineOffset;
    uint8_t *pos;
    int numgraphics = Game->number_of_pictures;
    pos = SeekToPos(tiles_start);
    if (pos == NULL || SeekToPos(image_blocks_start_address) == NULL
        || (Version != QUESTPROBE3_TYPE
            && SeekToPos(Game->start_of_image_instructions + FileBaselineOffset) == NULL)) {
        EnterTextOnlyMode("no picture data in this file");
        return;
    }

    /* Questprobe 3 has 246 tiles. Two of its pictures use tile 255 too, and
       the original then draws the bytes that follow the tiles as speckles.
       Here that tile is left empty. */
    for (int i = 0; i < Q3_TILE_COUNT; i++) {
        for (int y = 0; y < 8 && pos < EndOfGraphicsData; y++) {
            tiles[i][y] = *pos++;
        }
    }

    /* Now we have hopefully read the tile data. */
    /* Time for the image offsets. */

    images = (Image *)MemCalloc(sizeof(Image) * numgraphics);
    Image *img = images;
    pos = SeekToPos(image_blocks_start_address);

    if (Version == QUESTPROBE3_TYPE) {
        if (!ExtractQ3Images(images)) {
            free(images);
            images = NULL;
            EnterTextOnlyMode("no Questprobe 3 picture tables in this file");
            return;
        }
    } else {
        for (int picture_number = 0; picture_number < numgraphics; picture_number++) {
            /* The pictures the file ends before are left empty */
            if (pos >= EndOfGraphicsData)
                break;
            uint8_t widthheight = *pos++;
            img->width = ((widthheight & 0xf0) >> 4) + 1;
            img->height = (widthheight & 0x0f) + 1;
            if (CurrentGame == BLIZZARD_PASS) {
                /* The image format stores width and height in a single byte,
                   so max width is 16. Blizzard Pass cheats and adds 16 to
                   the width of a hardcoded list of images. */
                AdjustBlizzardPassImageWidth(img, picture_number);
            }
            pos = ExtractImage(img, pos);
            img++;
        }
        /* Questprobe 3 has no Taylor image data,
           (it uses the older type of graphics)
           so we only set this for the other games. */
        taylor_draw_config config = {
            .data = &FileImage[Game->start_of_image_instructions + FileBaselineOffset],
            .end = EndOfGraphicsData - 5,
            .objloc = ObjectLoc,
            .flags = (Version != HEMAN_TYPE ? TAYLOR_OLDER : 0)
                | (BaseGame == REBEL_PLANET ? TAYLOR_REBEL_PLANET : 0),
        };
        InitTaylor(&config);
    }

    InitIrmak(Game->number_of_pictures, TAYLOR_IRMAK_VERSION);
}
