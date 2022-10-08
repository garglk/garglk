//
//  sagadraw.c
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Based on code  by David Lodge
//  with help from Paul David Doherty (including the colour code)
//
//  Original code at https://github.com/tautology0/textadventuregraphics
//

#include "glk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "taylor.h"
#include "utility.h"

#include "sagadraw.h"

int draw_to_buffer = 1;

uint8_t sprite[256][8];
uint8_t screenchars[768][8];
uint8_t buffer[768][9];

static Image *images = NULL;

glui32 pixel_size;
glui32 x_offset;

static uint8_t *taylor_image_data;
static uint8_t *EndOfGraphicsData;
/* palette handler stuff starts here */

typedef uint8_t RGB[3];

typedef RGB PALETTE[16];

PALETTE pal;

int white_colour = 15;
int blue_colour = 9;

int32_t errorcount = 0;

palette_type palchosen = NO_PALETTE;

#define STRIDE 765 /* 255 pixels * 3 (r, g, b) */

#define INVALIDCOLOR 16

//static const char *flipdescription[] = { "none", "90°", "180°", "270°", "ERROR" };

//#define DRAWDEBUG

//static void colrange(int32_t c)
//{
//    if ((c < 0) || (c > 15)) {
//#ifdef DRAWDEBUG
//        fprintf(stderr, "# col out of range: %d\n", c);
//#endif
//        errorcount++;
//    }
//}

//static void checkrange(int32_t x, int32_t y)
//{
//    if ((x < 0) || (x > 254)) {
//#ifdef DRAWDEBUG
//        fprintf(stderr, "# x out of range: %d\n", x);
//#endif
//        errorcount++;
//    }
//    if ((y < 96) || (y > 191)) {
//#ifdef DRAWDEBUG
//        fprintf(stderr, "# y out of range: %d\n", y);
//#endif
//        errorcount++;
//    }
//}

//static void do_palette(const char *palname)
//{
//    if (strcmp("zx", palname) == 0)
//        palchosen = ZX;
//    else if (strcmp("zxopt", palname) == 0)
//        palchosen = ZXOPT;
//    else if (strcmp("c64a", palname) == 0)
//        palchosen = C64A;
//    else if (strcmp("c64b", palname) == 0)
//        palchosen = C64B;
//    else if (strcmp("vga", palname) == 0)
//        palchosen = VGA;
//}

static void set_color(int32_t index, RGB *colour)
{
    pal[index][0] = (*colour)[0];
    pal[index][1] = (*colour)[1];
    pal[index][2] = (*colour)[2];
}

void DefinePalette(void)
{
    /* set up the palette */
    if (palchosen == VGA) {
        RGB black = { 0, 0, 0 };
        RGB blue = { 0, 0, 255 };
        RGB red = { 255, 0, 0 };
        RGB magenta = { 255, 0, 255 };
        RGB green = { 0, 255, 0 };
        RGB cyan = { 0, 255, 255 };
        RGB yellow = { 255, 255, 0 };
        RGB white = { 255, 255, 255 };
        RGB brblack = { 0, 0, 0 };
        RGB brblue = { 0, 0, 255 };
        RGB brred = { 255, 0, 0 };
        RGB brmagenta = { 255, 0, 255 };
        RGB brgreen = { 0, 255, 0 };
        RGB brcyan = { 0, 255, 255 };
        RGB bryellow = { 255, 255, 0 };
        RGB brwhite = { 255, 255, 255 };

        set_color(0, &black);
        set_color(1, &blue);
        set_color(2, &red);
        set_color(3, &magenta);
        set_color(4, &green);
        set_color(5, &cyan);
        set_color(6, &yellow);
        set_color(7, &white);
        set_color(8, &brblack);
        set_color(9, &brblue);
        set_color(10, &brred);
        set_color(11, &brmagenta);
        set_color(12, &brgreen);
        set_color(13, &brcyan);
        set_color(14, &bryellow);
        set_color(15, &brwhite);
    } else if (palchosen == ZX) {
        /* corrected Sinclair ZX palette (pretty dull though) */
        RGB black = { 0, 0, 0 };
        RGB blue = { 0, 0, 154 };
        RGB red = { 154, 0, 0 };
        RGB magenta = { 154, 0, 154 };
        RGB green = { 0, 154, 0 };
        RGB cyan = { 0, 154, 154 };
        RGB yellow = { 154, 154, 0 };
        RGB white = { 154, 154, 154 };
        RGB brblack = { 0, 0, 0 };
        RGB brblue = { 0, 0, 170 };
        RGB brred = { 186, 0, 0 };
        RGB brmagenta = { 206, 0, 206 };
        RGB brgreen = { 0, 206, 0 };
        RGB brcyan = { 0, 223, 223 };
        RGB bryellow = { 239, 239, 0 };
        RGB brwhite = { 255, 255, 255 };

        set_color(0, &black);
        set_color(1, &blue);
        set_color(2, &red);
        set_color(3, &magenta);
        set_color(4, &green);
        set_color(5, &cyan);
        set_color(6, &yellow);
        set_color(7, &white);
        set_color(8, &brblack);
        set_color(9, &brblue);
        set_color(10, &brred);
        set_color(11, &brmagenta);
        set_color(12, &brgreen);
        set_color(13, &brcyan);
        set_color(14, &bryellow);
        set_color(15, &brwhite);

        white_colour = 15;
        blue_colour = 9;

    } else if (palchosen == ZXOPT) {
        /* optimized but not realistic Sinclair ZX palette (SPIN emu) */
        RGB black = { 0, 0, 0 };
        RGB blue = { 0, 0, 202 };
        RGB red = {
            202,
            0,
            0,
        };
        RGB magenta = { 202, 0, 202 };
        RGB green = { 0, 202, 0 };
        RGB cyan = { 0, 202, 202 };
        RGB yellow = { 202, 202, 0 };
        RGB white = { 202, 202, 202 };
        /*
         old David Lodge palette:

         RGB black = { 0, 0, 0 };
         RGB blue = { 0, 0, 214 };
         RGB red = { 214, 0, 0 };
         RGB magenta = { 214, 0, 214 };
         RGB green = { 0, 214, 0 };
         RGB cyan = { 0, 214, 214 };
         RGB yellow = { 214, 214, 0 };
         RGB white = { 214, 214, 214 };
         */
        RGB brblack = { 0, 0, 0 };
        RGB brblue = { 0, 0, 255 };
        RGB brred = { 255, 0, 20 };
        RGB brmagenta = { 255, 0, 255 };
        RGB brgreen = { 0, 255, 0 };
        RGB brcyan = { 0, 255, 255 };
        RGB bryellow = { 255, 255, 0 };
        RGB brwhite = { 255, 255, 255 };

        set_color(0, &black);
        set_color(1, &blue);
        set_color(2, &red);
        set_color(3, &magenta);
        set_color(4, &green);
        set_color(5, &cyan);
        set_color(6, &yellow);
        set_color(7, &white);
        set_color(8, &brblack);
        set_color(9, &brblue);
        set_color(10, &brred);
        set_color(11, &brmagenta);
        set_color(12, &brgreen);
        set_color(13, &brcyan);
        set_color(14, &bryellow);
        set_color(15, &brwhite);

        white_colour = 15;
        blue_colour = 9;

    } else if ((palchosen == C64A) || (palchosen == C64B)) {
        /* and now: C64 palette (pepto/VICE) */
        RGB black = { 0, 0, 0 };
        RGB white = { 255, 255, 255 };
        RGB red = { 191, 97, 72 };
        RGB cyan = { 153, 230, 249 };
        RGB purple = { 177, 89, 185 };
        RGB green = { 121, 213, 112 };
        RGB blue = { 95, 72, 233 };
        RGB yellow = { 247, 255, 108 };
        RGB orange = { 186, 134, 32 };
        RGB brown = { 116, 105, 0 };
        RGB lred = { 231, 154, 132 };
        RGB dgrey = { 69, 69, 69 };
        RGB grey = { 167, 167, 167 };
        RGB lgreen = { 192, 255, 185 };
        RGB lblue = { 162, 143, 255 };
        RGB lgrey = { 200, 200, 200 };

        set_color(0, &black);
        set_color(1, &white);
        set_color(2, &red);
        set_color(3, &cyan);
        set_color(4, &purple);
        set_color(5, &green);
        set_color(6, &blue);
        set_color(7, &yellow);
        set_color(8, &orange);
        set_color(9, &brown);
        set_color(10, &lred);
        set_color(11, &dgrey);
        set_color(12, &grey);
        set_color(13, &lgreen);
        set_color(14, &lblue);
        set_color(15, &lgrey);

        white_colour = 1;
        blue_colour = 6;
    }
}

//static const char *colortext(int32_t col)
//{
//    const char *zxcolorname[] = {
//        "black",
//        "blue",
//        "red",
//        "magenta",
//        "green",
//        "cyan",
//        "yellow",
//        "white",
//        "bright black",
//        "bright blue",
//        "bright red",
//        "bright magenta",
//        "bright green",
//        "bright cyan",
//        "bright yellow",
//        "bright white",
//        "INVALID",
//    };
//
//    const char *c64colorname[] = {
//        "black",
//        "white",
//        "red",
//        "cyan",
//        "purple",
//        "green",
//        "blue",
//        "yellow",
//        "orange",
//        "brown",
//        "light red",
//        "dark grey",
//        "grey",
//        "light green",
//        "light blue",
//        "light grey",
//        "INVALID",
//    };
//
//    if ((palchosen == C64A) || (palchosen == C64B))
//        return (c64colorname[col]);
//    else
//        return (zxcolorname[col]);
//}

int32_t Remap(int32_t color)
{
    int32_t mapcol;

    if ((palchosen == ZX) || (palchosen == ZXOPT)) {
        /* nothing to remap here; shows that the gfx were created on a ZX */
        mapcol = (((color >= 0) && (color <= 15)) ? color : INVALIDCOLOR);
    } else if (palchosen == C64A) {
        /* remap A determined from Golden Baton, applies to S1/S3/S13 too (8col) */
        int32_t c64remap[] = {
            0, // black
            6, // blue
            2, // red
            4, // magenta
            5, // green
            3, // cyan
            7, // yellow
            1, // white
            0, // bright black
            6, // bright blue
            2, // bright red
            4, // bright magenta
            5, // bright green
            3, // bright cyan
            7, // bright yellow
            1, // bright white
        };
        mapcol = (((color >= 0) && (color <= 15)) ? c64remap[color] : INVALIDCOLOR);
    } else if (palchosen == C64B) {
        /* remap B determined from Spiderman (16col) */
        int32_t c64remap[] = {
            0,
            6,
            9,
            4,
            5,
            12,
            8,
            15,
            0,
            14,
            2,
            10,
            13,
            3,
            7,
            1,
        };
        mapcol = (((color >= 0) && (color <= 15)) ? c64remap[color] : INVALIDCOLOR);
    } else
        mapcol = (((color >= 0) && (color <= 15)) ? color : INVALIDCOLOR);

    return (mapcol);
}

/* real code starts here */

static void Flip(uint8_t character[])
{
    int32_t i, j;
    uint8_t work2[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

    for (i = 0; i < 8; i++)
        for (j = 0; j < 8; j++)
            if ((character[i] & (1 << j)) != 0)
                work2[i] += 1 << (7 - j);
    for (i = 0; i < 8; i++)
        character[i] = work2[i];
}

static void rot90(uint8_t character[])
{
    int32_t i, j;
    uint8_t work2[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

    for (i = 0; i < 8; i++)
        for (j = 0; j < 8; j++)
            if ((character[j] & (1 << i)) != 0)
                work2[7 - i] += 1 << j;

    for (i = 0; i < 8; i++)
        character[i] = work2[i];
}

static void rot270(uint8_t character[])
{
    int32_t i, j;
    uint8_t work2[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

    for (i = 0; i < 8; i++)
        for (j = 0; j < 8; j++)
            if ((character[j] & (1 << i)) != 0)
                work2[i] += 1 << (7 - j);

    for (i = 0; i < 8; i++)
        character[i] = work2[i];
}

static void rot180(uint8_t character[])
{
    int32_t i, j;
    uint8_t work2[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

    for (i = 0; i < 8; i++)
        for (j = 0; j < 8; j++)
            if ((character[i] & (1 << j)) != 0)
                work2[7 - i] += 1 << (7 - j);

    for (i = 0; i < 8; i++)
        character[i] = work2[i];
}

static void transform(int32_t character, int32_t flip_mode, int32_t ptr)
{
    uint8_t work[8];
    int32_t i;

#ifdef DRAWDEBUG
    fprintf(stderr, "Plotting char: %d with flip: %02x (%s) at %d: %d,%d\n",
            character, flip_mode, flipdescription[(flip_mode & 48) >> 4], ptr,
            ptr % 0x20, ptr / 0x20);
#endif

    // first copy the character into work
    for (i = 0; i < 8; i++)
        work[i] = sprite[character][i];

    // Now flip it
    if ((flip_mode & 0x30) == 0x10) {
        rot90(work);
        //      fprintf(stderr, "rot 90 character %d\n",character);
    }
    if ((flip_mode & 0x30) == 0x20) {
        rot180(work);
        //       fprintf(stderr, "rot 180 character %d\n",character);
    }
    if ((flip_mode & 0x30) == 0x30) {
        rot270(work);
        //       fprintf(stderr, "rot 270 character %d\n",character);
    }
    if ((flip_mode & 0x40) != 0) {
        Flip(work);
        /* fprintf("flipping character %d\n",character); */
    }
    Flip(work);

    // Now mask it onto the previous character
    for (i = 0; i < 8; i++) {
        if ((flip_mode & 0x0c) == 12)
            screenchars[ptr][i] ^= work[i];
        else if ((flip_mode & 0x0c) == 8)
            screenchars[ptr][i] &= work[i];
        else if ((flip_mode & 0x0c) == 4)
            screenchars[ptr][i] |= work[i];
        else
            screenchars[ptr][i] = work[i];
    }
}

void PutPixel(glsi32 x, glsi32 y, int32_t color)
{
    int y_offset = 0;

    glui32 glk_color = ((pal[color][0] << 16)) | ((pal[color][1] << 8)) | (pal[color][2]);

    glk_window_fill_rect(Graphics, glk_color, x * pixel_size + x_offset,
                         y * pixel_size + y_offset, pixel_size, pixel_size);
}

void RectFill(int32_t x, int32_t y, int32_t width, int32_t height,
              int32_t color, int usebuffer)
{
    glui32 y_offset = 0;

    if (usebuffer) {
        int bufferpos = (y / 8) * 32 + (x / 8);
        if (bufferpos >= 0xD80)
            return;
        buffer[bufferpos][8] = (uint8_t)(buffer[bufferpos][8] | (color << 3));
    }

    glui32 glk_color = ((pal[color][0] << 16)) | ((pal[color][1] << 8)) | (pal[color][2]);

    glk_window_fill_rect(Graphics, glk_color, x * pixel_size + x_offset,
                         y * pixel_size + y_offset, width * pixel_size,
                         height * pixel_size);
}

void background(int32_t x, int32_t y, int32_t color)
{
    /* Draw the background */
    RectFill(x * 8, y * 8, 8, 8, color, 0);
}

void plotsprite(int32_t character, int32_t x, int32_t y, int32_t fg,
                int32_t bg)
{
    if (fg > 15)
        fg = 0;
    if (bg > 15)
        bg = 0;
    int32_t i, j;
    background(x, y, bg);
    for (i = 0; i < 8; i++) {
        for (j = 0; j < 8; j++)
            if ((screenchars[character][i] & (1 << j)) != 0)
                PutPixel(x * 8 + j, y * 8 + i, fg);
    }
}

static int isNthBitSet(unsigned const char c, int n)
{
    static unsigned const char mask[] = { 128, 64, 32, 16, 8, 4, 2, 1 };
    return ((c & mask[n]) != 0);
}

uint8_t *DrawSagaPictureFromData(uint8_t *dataptr, int xsize, int ysize,
                                 int xoff, int yoff, size_t datasize);

struct image_patch {
    GameIDType id;
    int picture_number;
    int offset;
    int number_of_bytes;
    const char *patch;
};

static const struct image_patch image_patches[] = {
    { UNKNOWN_GAME, 0, 0, 0, "" },
    { QUESTPROBE3, 55, 604, 3, "\xff\xff\x82" },
    { QUESTPROBE3, 56, 357, 46, "\x79\x81\x78\x79\x7b\x83\x47\x79\x82\x78\x79\x7b\x83\x47\x79\x83"
        "\x78\x79\x7b\x81\x79\x47\x79\x84\x7b\x83\x47\x79\x84\x58\x83\x47\x7a\x84\x5f\x18\x81\x5f"
        "\x47\x50\x84\x5f\x18\x81\x5f\x47" },
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
    const struct image_patch *patch = &image_patches[patch_number];
    for (int i = 0; i < patch->number_of_bytes; i++) {
        const char newval = patch->patch[i];
        offset[i + patch->offset] = (uint8_t)newval;
    }
}

void DrawTaylor(int loc);

static void Q3Init(size_t *base, size_t *offsets, size_t *imgdata) {
    *base = FindCode("\x00\x01\x01\x02\x03\x04\x05\x06\x02\x02", 0, 10);
    *offsets = FindCode("\x00\x00\xa7\x02\xa7\x03\xb9\x08\xd7\x0b", 0, 10);
    *imgdata =  FindCode("\x20\x0c\x00\x00\x8a\x01\x44\xa0\x17\x8a", *offsets, 10);
}

static uint8_t *Q3Image(int imgnum, size_t base, size_t offsets, size_t imgdata) {
    uint16_t offset_addr = (FileImage[base + imgnum] & 0x7f) * 2 + offsets;
    uint16_t image_addr = imgdata + FileImage[offset_addr] + FileImage[offset_addr + 1] * 256;
    return &FileImage[image_addr];
}

static void RepeatOpcode(int *number, uint8_t *instructions, uint8_t repeatcount)
{
	int i = *number - 1;
    instructions[i++] = 0x82;
    instructions[i++] = repeatcount;
    instructions[i++] = 0;
    *number = i;
}

static size_t FindCharacterStart(void)
{
    /* Look for the character data */
    size_t pos = FindCode("\x00\x00\x00\x00\x00\x00\x00\x00\x80\x80\x80\x80\x80\x80\x80\x80\x40\x40\x40\x40\x40\x40\x40\x40", 0, 24);
    if(pos == -1) {
        fprintf(stderr, "Cannot find character data.\n");
        return 0;
    }
#ifdef DEBUG
    fprintf(stderr, "Found characters at pos %zx\n", pos);
#endif
    return pos;
}

void SagaSetup(void)
{
    if (images != NULL)
        return;

    if (Game->number_of_pictures == 0 ) {
        NoGraphics = 1;
        return;
    }

    EndOfGraphicsData = FileImage + FileImageLen;

    int32_t i, y;

    if (palchosen == NO_PALETTE) {
        palchosen = Game->palette;
    }

    if (palchosen == NO_PALETTE) {
        fprintf(stderr, "unknown palette\n");
        exit(EXIT_FAILURE);
    }

    DefinePalette();

    size_t CHAR_START = FindCharacterStart();
    if (!CHAR_START)
        CHAR_START = Game->start_of_characters + FileBaselineOffset;
#ifdef DRAWDEBUG
    fprintf(stderr, "CHAR_START: %zx (%zu)\n", Game->start_of_characters + FileBaselineOffset, Game->start_of_characters + FileBaselineOffset);
#endif
    uint8_t *pos;
    int numgraphics = Game->number_of_pictures;
    pos = SeekToPos(FileImage, CHAR_START);

#ifdef DRAWDEBUG
    fprintf(stderr, "Grabbing Character details\n");
    fprintf(stderr, "Character Offset: %04lx\n",
            CHAR_START - FileBaselineOffset);
#endif
    for (i = 0; i < 246; i++) {
        for (y = 0; y < 8 && pos < EndOfGraphicsData; y++) {
            sprite[i][y] = *(pos++);
        }
    }

    /* Now we have hopefully read the character data */
    /* Time for the image offsets */

    images = (Image *)MemAlloc(sizeof(Image) * numgraphics);
    Image *img = images;
    size_t image_blocks_start_address = Game->start_of_image_blocks + FileBaselineOffset;

    size_t patterns_lookup = Game->image_patterns_lookup + FileBaselineOffset;

    pos = SeekToPos(FileImage, image_blocks_start_address);

    size_t base = 0, offsets = 0, imgdata = 0;

    if (Version == QUESTPROBE3_TYPE)
        Q3Init(&base, &offsets, &imgdata);

    for (int picture_number = 0; picture_number < numgraphics; picture_number++) {

        if (Version == QUESTPROBE3_TYPE) {
            pos = Q3Image(picture_number, base, offsets, imgdata);
            if (pos > EndOfData - 4 || pos < FileImage) {
                fprintf(stderr, "Image %d out of range!\n", picture_number);
                img->imagedata = NULL;
                img++;
                continue;
            }
            img->width = *pos++;
            img->height = *pos++;
            img->xoff = *pos++;
            img->yoff = *pos++;
            img->imagedata = pos;
            img->datasize = EndOfGraphicsData - pos;
            if (picture_number == 17) {
                img->imagedata = MemAlloc(608);
                img->datasize = 608;
                memcpy(img->imagedata, pos, MIN(EndOfGraphicsData - pos, 608));
                int patch = FindImagePatch(QUESTPROBE3, 55, 0);
                Patch(img->imagedata, patch);
            } else if (picture_number == 55 || (picture_number >= 18 && picture_number <= 20)) {
                img->imagedata = images[17].imagedata;
                img->datasize = images[17].datasize;
            } else if (picture_number == 56) {
                img->imagedata = MemAlloc(403);
                img->datasize = 403;
                memcpy(img->imagedata, pos, MIN(EndOfGraphicsData - pos, 403));
                int patch = FindImagePatch(QUESTPROBE3, 56, 0);
                Patch(img->imagedata, patch);
            }
            img++;
            continue;
        }

        uint8_t widthheight = *pos++;
        img->width = ((widthheight & 0xf0) >> 4) + 1;
        img->height = (widthheight & 0x0f) + 1;
        if (CurrentGame == BLIZZARD_PASS) {
            switch (picture_number) {
                case 13: case 15: case 17: case 34: case 85: case 111:
                    img->width += 16;
                    break;
                default:
                    break;
            }
        }
        uint8_t instructions[2048];
        int number = 0;
        uint8_t *copied_bytes = NULL;
        uint8_t *stored_pointer = NULL;
        do {
            instructions[number++] = *pos;
            uint8_t opcode = *pos;
            if (Version != HEMAN_TYPE) {
                switch (opcode) {
                    case 0xfb:
                        number--;
                        if (!copied_bytes || copied_bytes[0] == 0) {
                            pos = stored_pointer;
                            if (copied_bytes != NULL)
                                free(copied_bytes);
                            copied_bytes = NULL;
                        } else {
                            copied_bytes[0]--;
                            pos = copied_bytes;
                        }
                        break;
                     case 0xef:
                         RepeatOpcode(&number, instructions, 1);
                         break;
                     case 0xee:
                         RepeatOpcode(&number, instructions, 2);
                         break;
                     case 0xeb:
                         RepeatOpcode(&number, instructions, 3);
                         break;
                     case 0xf3:
                         pos++;
                         RepeatOpcode(&number, instructions, *pos);
                         break;
                    case 0xfa:
                        number--;
                        pos++;
                        int numbytes = *pos++;
                        stored_pointer = pos;
                        if (copied_bytes != NULL)
                            free(copied_bytes);
                        copied_bytes = MemAlloc(numbytes + 1);
                        memcpy(copied_bytes, pos, numbytes);
                        copied_bytes[0]--;
                        copied_bytes[numbytes] = 0xfb;
                        pos = copied_bytes;
                        break;
                }
            } else if (Game->number_of_patterns) {
                for (i = 0; i < Game->number_of_patterns; i++) {
                    if (*pos == FileImage[patterns_lookup + i]) {
                        number--;
                        size_t baseoffset = patterns_lookup + Game->number_of_patterns + i * 2;
                        size_t newoffset = FileImage[baseoffset] + FileImage[baseoffset + 1] * 256 - 0x4000 + FileBaselineOffset;
                        while (FileImage[newoffset] != Game->pattern_end_marker) {
                            instructions[number++] = FileImage[newoffset++];
                        }
                        break;
                    }
                }
            }
            pos++;
        } while (*pos != 0xfe);

        instructions[number++] = 0xfe;

        img->imagedata = MemAlloc(number);
        img->datasize = number;
        memcpy(img->imagedata, instructions, number);

        pos++;
        img++;
    }

    taylor_image_data = &FileImage[Game->start_of_image_instructions + FileBaselineOffset];
}

void PrintImageContents(int index, uint8_t *data, size_t size)
{
    fprintf(stderr, "/* image %d ", index);
    fprintf(stderr,
            "width: %d height: %d xoff: %d yoff: %d size: %zu bytes*/\n{ ",
            images[index].width, images[index].height, images[index].xoff,
            images[index].yoff, size);
    for (int i = 0; i < size; i++) {
        fprintf(stderr, "0x%02x, ", data[i]);
        if (i % 8 == 7)
            fprintf(stderr, "\n  ");
    }

    fprintf(stderr, " },\n");
    return;
}

void debugdrawcharacter(int character)
{
    fprintf(stderr, "Contents of character %d of 256:\n", character);
    for (int row = 0; row < 8; row++) {
        for (int n = 0; n < 8; n++) {
            if (isNthBitSet(sprite[character][row], n))
                fprintf(stderr, "■");
            else
                fprintf(stderr, "0");
        }
        fprintf(stderr, "\n");
    }
}

void debugdraw(int on, int character, int xoff, int yoff, int width)
{
    if (on) {
        int x = character % width;
        int y = character / width;
        plotsprite(character, x + xoff, y + yoff, 0, 15);
        fprintf(stderr, "Contents of character position %d:\n", character);
        for (int row = 0; row < 8; row++) {
            for (int n = 0; n < 8; n++) {
                if (isNthBitSet(screenchars[character][row], n))
                    fprintf(stderr, "■");
                else
                    fprintf(stderr, "0");
            }
            fprintf(stderr, "\n");
        }
    }
}

#pragma mark Taylorimage

static void mirror_area(int x1, int y1, int width, int y2)
{
    for (int line = y1; line < y2; line++) {
        int source = line * 32 + x1;
        int target = source + width - 1;
        for (int col = 0; col < width / 2; col++) {
            buffer[target][8] = buffer[source][8];
            for (int pixrow = 0; pixrow < 8; pixrow++)
                buffer[target][pixrow] = buffer[source][pixrow];
            Flip(buffer[target]);
            source++;
            target--;
        }
    }
}

static void mirror_top_half(void)
{
    for (int line = 0; line < 6; line++) {
        for (int col = 0; col < 32; col++) {
            buffer[(11 - line) * 32 + col][8] = buffer[line * 32 + col][8];
            for (int pixrow = 0; pixrow < 8; pixrow++)
                buffer[(11 - line) * 32 + col][7 - pixrow] = buffer[line * 32 + col][pixrow];
        }
    }
}

static void flip_image_horizontally(void)
{
    uint8_t mirror[384][9];

    for (int line = 0; line < 12; line++) {
        for (int col = 32; col > 0; col--) {
            for (int pixrow = 0; pixrow < 9; pixrow++)
                mirror[line * 32 + col - 1][pixrow] = buffer[line * 32 + (32 - col)][pixrow];
            Flip(mirror[line * 32 + col - 1]);
        }
    }

    memcpy(buffer, mirror, 384 * 9);
}

static void flip_image_vertically(void)
{
    uint8_t mirror[384][9];

    for (int line = 0; line < 12; line++) {
        for (int col = 0; col < 32; col++) {
            for (int pixrow = 0; pixrow < 8; pixrow++)
                mirror[(11 - line) * 32 + col][7 - pixrow] = buffer[line * 32 + col][pixrow];
            mirror[(11 - line) * 32 + col][8] = buffer[line * 32 + col][8];
        }
    }
    memcpy(buffer, mirror, 384 * 9);
}

static void flip_area_vertically(uint8_t x1, uint8_t y1, uint8_t width, uint8_t y2) {
//    fprintf(stderr, "flip_area_vertically x1: %d: y1: %d width: %d y2 %d\n", x1, y1, width, y2);
    uint8_t mirror[384][9];

    for (int line = 0; line <= y2; line++) {
        for (int col = x1; col < x1 + width; col++) {
            for (int pixrow = 0; pixrow < 8; pixrow++)
                mirror[(y2 - line) * 32 + col][7 - pixrow] = buffer[(y1 + line) * 32 + col][pixrow];
            mirror[(y2 - line) * 32 + col][8] = buffer[(y1 + line) * 32 + col][8];
        }
    }
    for (int line = y1; line <= y2; line++) {
        for (int col = x1; col < x1 + width; col++) {
            for (int pixrow = 0; pixrow < 8; pixrow++)
                buffer[line * 32 + col][pixrow] = mirror[line * 32 + col][pixrow];
            buffer[line * 32 + col][8] = mirror[line * 32 + col][8];
        }
    }
}

static void mirror_area_vertically(uint8_t x1, uint8_t y1, uint8_t width, uint8_t y2) {
    for (int line = 0; line <= y2 / 2; line++) {
        for (int col = x1; col < x1 + width; col++) {
            buffer[(y2 - line) * 32 + col][8] = buffer[(y1 + line) * 32 + col][8];
            for (int pixrow = 0; pixrow < 8; pixrow++)
                buffer[(y2 - line) * 32 + col][7 - pixrow] = buffer[(y1 + line) * 32 + col][pixrow];
        }
    }
}

static void flip_area_horizontally(uint8_t x1, uint8_t y1, uint8_t width, uint8_t y2) {
//    fprintf(stderr, "flip_area_horizontally x1: %d: y1: %d width: %d y2 %d\n", x1, y1, width, y2);
    uint8_t mirror[384][9];

    for (int line = y1; line < y2; line++) {
        for (int col = 0; col < width; col++) {
            for (int pixrow = 0; pixrow < 9; pixrow++)
                mirror[line * 32 + x1 + col][pixrow] = buffer[line * 32 + (x1 + width - 1) - col][pixrow];
            Flip(mirror[line * 32 + x1 + col]);
        }
    }

    for (int line = y1; line < y2; line++) {
        for (int col = x1; col < x1 + width; col++) {
            for (int pixrow = 0; pixrow < 8; pixrow++)
                buffer[line * 32 + col][pixrow] = mirror[line * 32 + col][pixrow];
            buffer[line * 32 + col][8] = mirror[line * 32 + col][8];
        }
    }
}

static void draw_colour_old(uint8_t x, uint8_t y, uint8_t colour, uint8_t length)
{
    for (int i = 0; i < length; i++) {
        buffer[y * 32 + x + i][8] = colour;
    }
}


static void draw_colour( uint8_t colour, uint8_t x, uint8_t y, uint8_t width, uint8_t height)
{
    for (int h = 0; h < height; h++) {
        for (int w = 0; w < width; w++) {
            buffer[(y + h) * 32 + x + w][8] = colour;
        }
    }
}

static void make_light(void)
{
    for (int i = 0; i < 384; i++) {
        buffer[i][8] = buffer[i][8] | 0x40;
    }
}


static void replace_colour(uint8_t before, uint8_t after)
{
    uint8_t beforeink = before & 7;
    uint8_t afterink = after & 7;
    uint8_t inkmask = 0x07; // 0000 0111

    uint8_t beforepaper = beforeink << 3;
    uint8_t afterpaper = afterink << 3;
    uint8_t papermask = 0x38; // 0011 1000

    for (int j = 0; j < 384; j++) {
        if ((buffer[j][8] & inkmask) == beforeink) {
            buffer[j][8] = (buffer[j][8] & ~inkmask) | afterink;
        }

        if ((buffer[j][8] & papermask) == beforepaper) {
            buffer[j][8] = (buffer[j][8] & ~papermask) | afterpaper;
        }
    }
}

static uint8_t ink2paper(uint8_t ink) {
    uint8_t paper = (ink & 0x07) << 3; // 0000 0111 mask ink
    paper = paper & 0x38; // 0011 1000 mask paper
    return (ink & 0x40) | paper; // 0x40 = 0100 0000 preserve brightness bit from ink
}

static void replace(uint8_t before, uint8_t after, uint8_t mask) {
    for (int j = 0; j < 384; j++) {
        uint8_t col = buffer[j][8] & mask;
        if (col == before) {
            uint8_t newcol = buffer[j][8] | mask;
            newcol = newcol ^ mask;
            buffer[j][8] = newcol | after;
        }
    }
}

static void replace_paper_and_ink(uint8_t before, uint8_t after) {
    uint8_t beforeink = before & 0x47; // 0100 0111 ink and brightness
    replace(beforeink, after, 0x47);
    uint8_t beforepaper = ink2paper(before);
    uint8_t afterpaper = ink2paper(after);
    replace(beforepaper, afterpaper, 0x78); // 0111 1000 mask paper and brightness
}

void ClearGraphMem(void)
{
    memset(buffer, 0, 384 * 9);
}

void DrawTaylor(int loc)
{
    uint8_t *ptr = taylor_image_data;
    for (int i = 0; i < loc; i++) {
        while (*(ptr) != 0xff)
            ptr++;
        ptr++;
    }

    while (ptr < EndOfGraphicsData) {
        //        fprintf(stderr, "DrawTaylorRoomImage: Instruction %d: 0x%02x\n", instruction++, *ptr);
        switch (*ptr) {
            case 0xff:
                //                fprintf(stderr, "End of picture\n");
                return;
            case 0xfe:
                //                fprintf(stderr, "0xfe mirror_left_half\n");
                mirror_area(0, 0, 32, 12);
                break;
            case 0xfd:
                //                fprintf(stderr, "0xfd Replace colour %x with %x\n", *(ptr + 1), *(ptr + 2));
                    replace_colour(*(ptr + 1), *(ptr + 2));
                ptr += 2;
                break;
            case 0xfc: // Draw colour: x, y, attribute, length 7808
                if (Game->type != HEMAN_TYPE) {
                    // fprintf(stderr, "0xfc (7808) Draw attribute %x at %d,%d length %d\n", *(ptr + 3), *(ptr + 1), *(ptr + 2), *(ptr + 4));
                    draw_colour_old(*(ptr + 1), *(ptr + 2), *(ptr + 3), *(ptr + 4));
                    ptr = ptr + 4;
                } else {
                    // fprintf(stderr, "0xfc (7808) Draw attribute %x at %d,%d height %d width %d\n", *(ptr + 4), *(ptr + 2), *(ptr + 1), *(ptr + 3), *(ptr + 5));
                    draw_colour(*(ptr + 4), *(ptr + 2), *(ptr + 1), *(ptr + 5), *(ptr + 3));
                    ptr = ptr + 5;
                }
                break;
            case 0xfb: // Make all screen colours bright 713e
                // fprintf(stderr, "Make colours in picture area bright\n");
                make_light();
                break;
            case 0xfa: // Flip entire image horizontally 7646
                // fprintf(stderr, "0xfa Flip entire image horizontally\n");
                flip_image_horizontally();
                break;
            case 0xf9: //0xf9 Draw picture n recursively;
                // fprintf(stderr, "Draw Room Image %d recursively\n", *(ptr + 1));
                DrawTaylor(*(ptr + 1));
                ptr++;
                break;
            case 0xf8:
                // fprintf(stderr, "0xf8: Skip rest of picture if object %d is not present\n", *(ptr + 1));
                ptr++;
                if (CurrentGame == BLIZZARD_PASS || CurrentGame == REBEL_PLANET || CurrentGame == REBEL_PLANET_64 ) {
                    if (ObjectLoc[*ptr] == MyLoc) {
                        DrawSagaPictureAtPos(*(ptr + 1), *(ptr + 2), *(ptr + 3));
                    }
                    ptr += 3;
                } else {
                    if (ObjectLoc[*ptr] != MyLoc)
                        return;
                }
                break;
            case 0xf4: // End if object arg1 is present
                // fprintf(stderr, "0xf4: Skip rest of picture if object %d IS present\n", *(ptr + 1));
                if (ObjectLoc[*(ptr + 1)] == MyLoc)
                    return;
                ptr++;
                break;
            case 0xf3:
                // fprintf(stderr, "0xf3: goto 753d Mirror top half vertically\n");
                mirror_top_half();
                break;
            case 0xf2: // arg1 arg2 arg3 arg4 Mirror horizontally
                // fprintf(stderr, "0xf2: Mirror area x: %d y: %d width:%d y2:%d horizontally\n", *(ptr + 2), *(ptr + 1), *(ptr + 4),  *(ptr + 3));
                mirror_area(*(ptr + 2), *(ptr + 1), *(ptr + 4),  *(ptr + 3));
                ptr = ptr + 4;
                break;
            case 0xf1: // arg1 arg2 arg3 arg4 Mirror vertically
                // fprintf(stderr, "0xf1: Mirror area x: %d y: %d width:%d y2:%d vertically\n", *(ptr + 2), *(ptr + 1), *(ptr + 4),  *(ptr + 3));
                mirror_area_vertically(*(ptr + 1), *(ptr + 2), *(ptr + 4),  *(ptr + 3));
                ptr = ptr + 4;
                break;
            case 0xee: // arg1 arg2 arg3 arg4  Flip area horizontally
                // fprintf(stderr, "0xf1: Flip area x: %d y: %d width:%d y2:%d horizontally\n", *(ptr + 2), *(ptr + 1), *(ptr + 4),  *(ptr + 3));
                flip_area_horizontally(*(ptr + 2), *(ptr + 1), *(ptr + 4),  *(ptr + 3));
                ptr = ptr + 4;
                break;
            case 0xed:
                // fprintf(stderr, "0xed: Flip entire image vertically\n");
                flip_image_vertically();
                break;
            case 0xec: // Flip area vertically
                // fprintf(stderr, "0xf1: Flip area x: %d y: %d width:%d y2:%d vertically\n", *(ptr + 2), *(ptr + 1), *(ptr + 4),  *(ptr + 3));
                flip_area_vertically(*(ptr + 1), *(ptr + 2), *(ptr + 4), *(ptr + 3));
                ptr = ptr + 4;
                break;
            case 0xe9:
                // fprintf(stderr, "0xe9: (77ac) replace paper and ink %d for colour %d?\n",  *(ptr + 1), *(ptr + 2));
                replace_paper_and_ink(*(ptr + 1), *(ptr + 2));
                ptr = ptr + 2;
                break;
            case 0xe8:
                // fprintf(stderr, "Clear graphics memory\n");
                ClearGraphMem();
                break;
            case 0xf7: // set A to 0c and call 70b7, but A seems to not be used. Vestigial code?
                if ((CurrentGame == REBEL_PLANET || CurrentGame == REBEL_PLANET_64) && MyLoc == 43 && ObjectLoc[131] == 252)
                    return;
            case 0xf6: // set A to 04 and call 70b7. See 0xf7 above.
            case 0xf5: // set A to 08 and call 70b7. See 0xf7 above.
                // fprintf(stderr, "0x%02x: set A to unused value and draw image block %d at %d, %d\n",  *ptr, *(ptr + 1), *(ptr + 2), *(ptr + 3));
                ptr++; // Deliberate fallthrough
            default: // else draw image *ptr at x, y
                // fprintf(stderr, "Default: Draw image block %d at %d,%d\n", *ptr, *(ptr + 1), *(ptr + 2));
                DrawSagaPictureAtPos(*ptr, *(ptr + 1), *(ptr + 2));
                ptr = ptr + 2;
                break;
        }
        ptr++;
    }
}

uint8_t *DrawSagaPictureFromData(uint8_t *dataptr, int xsize, int ysize,
                                 int xoff, int yoff, size_t datasize)
{
    if (dataptr == NULL)
        return NULL;

    int32_t offset = 0, cont = 0;
    int32_t i, x, y, mask_mode;
    uint8_t data, data2, old = 0;
    int32_t ink[0x22][14], paper[0x22][14];

    int version = 4;

    uint8_t *origptr = dataptr;

    offset = 0;
    int32_t character = 0;
    int32_t count;
    int offsetlimit = xsize * ysize;
    do {
        count = 1;

        /* first handle mode */
        data = *dataptr++;
        if (data < 0x80) {
            if (character > 127 && version > 2) {
                data += 128;
            }
            character = data;
#ifdef DRAWDEBUG
            fprintf(stderr, "******* SOLO CHARACTER: %04x\n", character);
#endif
            transform(character, 0, offset);
            offset++;
            if (offset > offsetlimit)
                break;
        } else {
            // first check for a count
            if ((data & 2) == 2) {
                count = (*dataptr++) + 1;
            }

            // Next get character and plot (count) times
            character = *dataptr++;

            // Plot the initial character
            if ((data & 1) == 1 && character < 128)
                character += 128;

            for (i = 0; i < count; i++) {
                if (offset + i > offsetlimit)
                    goto draw_attributes;
                transform(character, (data & 0x0c) ? (data & 0xf3) : data, offset + i);
            }

            // Now check for overlays
            if ((data & 0xc) != 0) {
                // We have overlays so grab each member of the stream and work out what
                // to do with it

                mask_mode = (data & 0xc);
                data2 = *dataptr++;
                old = data;
                do {
                    cont = 0;
                    if (data2 < 0x80) {
                        if (version == 4 && (old & 1) == 1)
                            data2 += 128;
#ifdef DRAWDEBUG
                        fprintf(stderr, "Plotting %d directly (overlay) at %d\n", data2,
                                offset);
#endif
                        for (i = 0; i < count; i++)
                            transform(data2, old & 0x0c, offset + i);
                    } else {
                        character = *dataptr++;
                        if ((data2 & 1) == 1)
                            character += 128;
#ifdef DRAWDEBUG
                        fprintf(stderr, "Plotting %d with flip %02x (%s) at %d %d\n",
                                character, (data2 | mask_mode),
                                flipdescription[((data2 | mask_mode) & 48) >> 4], offset,
                                count);
#endif
                        for (i = 0; i < count; i++)
                            transform(character, (data2 & 0xf3) | mask_mode, offset + i);
                        if ((data2 & 0x0c) != 0) {
                            mask_mode = (data2 & 0x0c);
                            old = data2;
                            cont = 1;
                            data2 = *dataptr++;
                        }
                    }
                } while (cont != 0);
            }
            offset += count;
        }
    } while (offset < offsetlimit);

draw_attributes:

    y = 0;
    x = 0;

    //       fprintf(stderr, "Attribute data begins at offset %ld\n", dataptr -
    //       origptr);

    uint8_t colour = 0;
    // Note version 3 count is inverse it is repeat previous colour
    // Whilst version0-2 count is repeat next character
    while (y < ysize) {
        if (dataptr - origptr > datasize) {
            fprintf(stderr, "DrawSagaPictureFromData: data offset %zu out of range! Image size %zu. Bailing!\n", dataptr - origptr, datasize);
                return dataptr;
        }
        data = *dataptr++;
//        fprintf(stderr, "%03ld: read attribute data byte %02x\n", dataptr -
//                origptr - 1, data);
        if ((data & 0x80)) {
            count = (data & 0x7f) + 1;
            if (version >= 3) {
                count--;
            } else {
                colour = *dataptr++;
            }
        } else {
            count = 1;
            colour = data;
        }
        while (count > 0) {
            // Now split up depending on which version we're using

            // For version 3+

            if (draw_to_buffer)
                buffer[(yoff + y) * 32 + (xoff + x)][8] = colour;
            else {
                if (version > 2) {
                    if (x > 33)
                        return NULL;
                    // ink is 0-2, screen is 3-5, 6 in bright flag
                    ink[x][y] = colour & 0x07;
                    paper[x][y] = (colour & 0x38) >> 3;

                    if ((colour & 0x40) == 0x40) {
                        paper[x][y] += 8;
                        ink[x][y] += 8;
                    }
                } else {
                    if (x > 33)
                        return NULL;
                    paper[x][y] = colour & 0x07;
                    ink[x][y] = ((colour & 0x70) >> 4);

                    if ((colour & 0x08) == 0x08 || version < 2) {
                        paper[x][y] += 8;
                        ink[x][y] += 8;
                    }
                }
            }

            x++;
            if (x == xsize) {
                x = 0;
                y++;
                if (y > ysize)
                    break;
            }
            count--;
        }
    }
    offset = 0;
    int32_t xoff2;
    for (y = 0; y < ysize; y++)
        for (x = 0; x < xsize; x++) {
            xoff2 = xoff;
            if (version > 0 && version < 3)
                xoff2 = xoff - 4;

            if (draw_to_buffer) {
                for (i = 0; i < 8; i++)
                    buffer[(y + yoff) * 32 + x + xoff2][i] = screenchars[offset][i];
            } else {
                plotsprite(offset, x + xoff2, y + yoff, Remap(ink[x][y]),
                           Remap(paper[x][y]));
            }

#ifdef DRAWDEBUG
            uint8_t colour = buffer[(yoff + y) * 32 + (xoff + x)][8];

            int paper = (colour >> 3) & 0x7;
            paper += 8 * ((colour & 0x40) == 0x40);
            paper = Remap(paper);
            int ink = (colour & 0x7);
            ink += 8 * ((colour & 0x40) == 0x40);
            ink = Remap(ink);

            fprintf(stderr, "(gfx#:plotting %d,%d:paper=%s,ink=%s)\n", x + xoff2,
                    y + yoff, colortext(paper),
                    colortext(ink));
#endif
            offset++;
            if (offset > offsetlimit)
                break;
        }
    return dataptr;
}

void DrawSagaPictureNumber(int picture_number)
{
    if (Game->number_of_pictures == 0)
        return;
//    int numgraphics = Game->number_of_pictures;
//    if (picture_number >= numgraphics) {
//        fprintf(stderr, "Invalid image number %d! Last image:%d\n", picture_number,
//                numgraphics - 1);
//        return;
//    }

    Image img = images[picture_number];

    if (img.imagedata == NULL)
        return;

    DrawSagaPictureFromData(img.imagedata, img.width, img.height, img.xoff,
                            img.yoff, img.datasize);
}

void DrawSagaPictureAtPos(int picture_number, int x, int y)
{
    Image img = images[picture_number];

    DrawSagaPictureFromData(img.imagedata, img.width, img.height, x, y, img.datasize);
}

void DrawSagaPictureFromBuffer(void)
{
    for (int line = 0; line < 12; line++) {
        for (int col = 0; col < 32; col++) {

            uint8_t colour = buffer[col + line * 32][8];

            int paper = (colour >> 3) & 0x7;
            paper += 8 * ((colour & 0x40) == 0x40);
            paper = Remap(paper);
            int ink = (colour & 0x7);
            ink += 8 * ((colour & 0x40) == 0x40);
            ink = Remap(ink);

            background(col, line, paper);

            for (int i = 0; i < 8; i++) {
                if (buffer[col + line * 32][i] == 0)
                    continue;
                if (buffer[col + line * 32][i] == 255) {
                    glui32 glk_color = (glui32)((pal[ink][0] << 16) | (pal[ink][1] << 8) | pal[ink][2]);

                    glk_window_fill_rect(
                                         Graphics, glk_color, (glsi32)(col * 8 * pixel_size + x_offset),
                                         (glsi32)(line * 8 + i) * pixel_size, 8 * pixel_size, pixel_size);
                    continue;
                }
                for (int j = 0; j < 8; j++)
                    if ((buffer[col + line * 32][i] & (1 << j)) != 0) {
                        int ypos = line * 8 + i;
                        PutPixel(col * 8 + j, ypos, ink);
                    }
            }
        }
    }
}
