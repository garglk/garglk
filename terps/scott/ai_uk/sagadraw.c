/* Sagadraw v2.5
 Originally by David Lodge

 With help from Paul David Doherty (including the colour code)
 */

#include "glk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "scottdefines.h"
#include "scott.h"
#include "sagagraphics.h"
#include "seasofblood.h"

#include "sagadraw.h"

int draw_to_buffer = 0;

uint8_t sprite[256][8];
uint8_t screenchars[768][8];
uint8_t buffer[384][9];

Image *images = NULL;

int white_colour = 15;
int blue_colour = 9;
glui32 dice_colour = 0xff0000;

int32_t errorcount = 0;

palette_type palchosen = NO_PALETTE;

#define STRIDE 765 /* 255 pixels * 3 (r, g, b) */

#define INVALIDCOLOR 16

const char *flipdescription[] = { "none", "90°", "180°", "270°", "ERROR" };

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

        SetColor(0, &black);
        SetColor(1, &blue);
        SetColor(2, &red);
        SetColor(3, &magenta);
        SetColor(4, &green);
        SetColor(5, &cyan);
        SetColor(6, &yellow);
        SetColor(7, &white);
        SetColor(8, &brblack);
        SetColor(9, &brblue);
        SetColor(10, &brred);
        SetColor(11, &brmagenta);
        SetColor(12, &brgreen);
        SetColor(13, &brcyan);
        SetColor(14, &bryellow);
        SetColor(15, &brwhite);
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

        SetColor(0, &black);
        SetColor(1, &blue);
        SetColor(2, &red);
        SetColor(3, &magenta);
        SetColor(4, &green);
        SetColor(5, &cyan);
        SetColor(6, &yellow);
        SetColor(7, &white);
        SetColor(8, &brblack);
        SetColor(9, &brblue);
        SetColor(10, &brred);
        SetColor(11, &brmagenta);
        SetColor(12, &brgreen);
        SetColor(13, &brcyan);
        SetColor(14, &bryellow);
        SetColor(15, &brwhite);

        white_colour = 15;
        blue_colour = 9;
        dice_colour = 0xff0000;
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
        RGB brblue = {
            0,
            0,
            255,
        };
        RGB brred = { 255, 0, 20 };
        RGB brmagenta = { 255, 0, 255 };
        RGB brgreen = { 0, 255, 0 };
        RGB brcyan = { 0, 255, 255 };
        RGB bryellow = { 255, 255, 0 };
        RGB brwhite = { 255, 255, 255 };

        SetColor(0, &black);
        SetColor(1, &blue);
        SetColor(2, &red);
        SetColor(3, &magenta);
        SetColor(4, &green);
        SetColor(5, &cyan);
        SetColor(6, &yellow);
        SetColor(7, &white);
        SetColor(8, &brblack);
        SetColor(9, &brblue);
        SetColor(10, &brred);
        SetColor(11, &brmagenta);
        SetColor(12, &brgreen);
        SetColor(13, &brcyan);
        SetColor(14, &bryellow);
        SetColor(15, &brwhite);

        white_colour = 15;
        blue_colour = 9;
        dice_colour = 0xff0000;

    } else if (palchosen >= C64A) {
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

        SetColor(0, &black);
        SetColor(1, &white);
        SetColor(2, &red);
        SetColor(3, &cyan);
        SetColor(4, &purple);
        SetColor(5, &green);
        SetColor(6, &blue);
        SetColor(7, &yellow);
        SetColor(8, &orange);
        SetColor(9, &brown);
        SetColor(10, &lred);
        SetColor(11, &dgrey);
        SetColor(12, &grey);
        SetColor(13, &lgreen);
        SetColor(14, &lblue);
        SetColor(15, &lgrey);

        white_colour = 1;
        blue_colour = 6;
        dice_colour = 0x5f48e9;
    }
}

const char *colortext(int32_t col)
{
    const char *zxcolorname[] = {
        "black",
        "blue",
        "red",
        "magenta",
        "green",
        "cyan",
        "yellow",
        "white",
        "bright black",
        "bright blue",
        "bright red",
        "bright magenta",
        "bright green",
        "bright cyan",
        "bright yellow",
        "bright white",
        "INVALID",
    };

    const char *c64colorname[] = {
        "black",
        "white",
        "red",
        "cyan",
        "purple",
        "green",
        "blue",
        "yellow",
        "orange",
        "brown",
        "light red",
        "dark grey",
        "grey",
        "light green",
        "light blue",
        "light grey",
        "INVALID",
    };

    if (palchosen >= C64A)
        return (c64colorname[col]);
    else
        return (zxcolorname[col]);
}

int32_t Remap(int32_t color)
{
    int32_t mapcol;

    if ((palchosen == ZX) || (palchosen == ZXOPT)) {
        /* nothing to remap here; shows that the gfx were created on a ZX */
        mapcol = (((color >= 0) && (color <= 15)) ? color : INVALIDCOLOR);
    } else if (palchosen == C64A) {
        /* remap A determined from Golden Baton, applies to S1/S3/S13 too (8col) */
        int32_t c64remap[] = {
            0,
            6,
            2,
            4,
            5,
            3,
            7,
            1,
            8,
            1,
            1,
            1,
            7,
            12,
            8,
            7,
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
            14,
            8,
            12,
            0,
            6,
            2,
            4,
            5,
            3,
            7,
            1,
        };
        mapcol = (((color >= 0) && (color <= 15)) ? c64remap[color] : INVALIDCOLOR);
    } else if (palchosen == C64C) {
        /* remap B determined from Spiderman (16col) */
        int32_t c64remap[] = {
            0,
            6,
            2,
            4,
            5,
            14,
            8,
            12,
            0,
            6,
            2,
            4,
            5,
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

void Flip(uint8_t character[])
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

void rot90(uint8_t character[])
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

void rot270(uint8_t character[])
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

void rot180(uint8_t character[])
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

void transform(int32_t character, int32_t flip_mode, int32_t ptr)
{
    if (character > 255)
        return;
    uint8_t work[8];
    int32_t i;

#ifdef DRAWDEBUG
    debug_print("Plotting char: %d with flip: %02x (%s) at %d: %d,%d\n",
        character, flip_mode, flipdescription[(flip_mode & 48) >> 4], ptr,
        ptr % 0x20, ptr / 0x20);
#endif

    // first copy the character into work
    for (i = 0; i < 8; i++)
        work[i] = sprite[character][i];

    // Now flip it
    if ((flip_mode & 0x30) == 0x10) {
        rot90(work);
        //      debug_print("rot 90 character %d\n",character);
    }
    if ((flip_mode & 0x30) == 0x20) {
        rot180(work);
        //       debug_print("rot 180 character %d\n",character);
    }
    if ((flip_mode & 0x30) == 0x30) {
        rot270(work);
        //       debug_print("rot 270 character %d\n",character);
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



void RectFill(int32_t x, int32_t y, int32_t width, int32_t height,
    int32_t color)
{
    int bufferpos = (y / 8) * 32 + (x / 8);
    if (bufferpos >= 0xD80)
        return;
    buffer[bufferpos][8] = buffer[bufferpos][8] | (color << 3);

    glui32 glk_color = ((pal[color][0] << 16)) | ((pal[color][1] << 8)) | (pal[color][2]);

    glk_window_fill_rect(Graphics, glk_color, x * pixel_size + x_offset,
        y * pixel_size + y_offset, width * pixel_size,
        height * pixel_size);
}

void background(int32_t x, int32_t y, int32_t color)
{
    /* Draw the background */
    RectFill(x * 8, y * 8, 8, 8, color);
}

void plotsprite(int32_t character, int32_t x, int32_t y, int32_t fg,
    int32_t bg)
{
    int32_t i, j;
    background(x, y, bg);
    for (i = 0; i < 8; i++) {
        for (j = 0; j < 8; j++)
            if ((screenchars[character][i] & (1 << j)) != 0)
                PutPixel(x * 8 + j, y * 8 + i, fg);
    }
}

int isNthBitSet(unsigned const char c, int n)
{
    static unsigned const char mask[] = { 128, 64, 32, 16, 8, 4, 2, 1 };
    return ((c & mask[n]) != 0);
}

uint8_t *DrawSagaPictureFromData(uint8_t *dataptr, int xsize, int ysize,
    int xoff, int yoff);

struct image_patch {
    GameIDType id;
    int picture_number;
    int offset;
    int number_of_bytes;
    const char *patch;
};

struct image_patch image_patches[] = {
    { UNKNOWN_GAME, 0, 0, 0, "" },
    { CLAYMORGUE_C64, 12, 378, 9, "\x20\xa4\x02\x80\x20\x82\x01\x20\x84" },
    { CLAYMORGUE_C64, 28, 584, 1, "\x90" },
    { CLAYMORGUE_C64, 16, 0, 12,
        "\x82\xfd\x00\x82\x81\x00\xff\x05\xff\x05\xff\x05" },
    { CLAYMORGUE, 14, 0, 12,
      "\x82\xfd\x00\x82\x81\x00\xff\x05\xff\x05\xff\x05" },
    { GREMLINS_C64, 21, 10, 5, "\x01\xa0\x03\x00\x01" },
    { GREMLINS_C64, 44, 304, 1, "\xb1" },
    { GREMLINS_C64, 81, 176, 1, "\xa0" },
    { GREMLINS_C64, 85, 413, 1, "\x94" },
    { GREMLINS_GERMAN_C64, 20, 191, 9, "\x03\x00\x94\x0c\xb0\x02\x94\x0d\xb0" },
    { SPIDERMAN_C64, 9, 1241, 1, "\x81" },
    { SECRET_MISSION_C64, 11, 88, 2, "\x90\x18" },
    { SECRET_MISSION, 33, 2, 1, "\xa0" },
    { SECRET_MISSION_C64, 33, 2, 1, "\xa0" },
    { SECRET_MISSION, 38, 53, 1, "\x53" },
    { SECRET_MISSION_C64, 38, 53, 1, "\x53" },
    { SEAS_OF_BLOOD_C64, 34, 471, 1, "\xa0" },
    { SEAS_OF_BLOOD_C64, 35, 16, 1, "\xcc" },
    { NUMGAMES, 0, 0, 0, "" },
};

int FindImagePatch(GameIDType game, int image_number, int start)
{
    for (int i = start + 1; image_patches[i].id != NUMGAMES; i++) {
        if (image_patches[i].id == game && image_patches[i].picture_number == image_number) {
            return i;
        }
    }
    return 0;
}

void Patch(uint8_t *offset, int patch_number)
{
    struct image_patch *patch = &image_patches[patch_number];
    for (int i = 0; i < patch->number_of_bytes; i++) {
        uint8_t newval = patch->patch[i];
//        debug_print("Patch: changing offset %d in image %d from %x to %x.\n", i + patch->offset, patch->picture_number, offset[i + patch->offset], newval);
        offset[i + patch->offset] = newval;
    }
}

void PatchOutBrokenClaymorgueImagesC64(void)
{
    Output("[This copy of The Sorcerer of Claymorgue Castle has 16 broken or "
           "missing pictures. These have been patched out.]\n\n");
    for (int i = 12; i < 28; i++) {
        if (i != 16)
            for (int j = 0; j < GameHeader.NumRooms; j++) {
                if (Rooms[j].Image == i) {
                    Rooms[j].Image = 255;
                }
            }
    }
}

void PatchOutBrokenClaymorgueImagesZX(void)
{
   Output("[This copy of The Sorcerer of Claymorgue Castle has 26 broken or "
          "missing pictures. These have been patched out.]\n\n");
   for (int i = 9; i < 36; i++) {
      if (i != 14)
         for (int j = 0; j < GameHeader.NumRooms; j++) {
            if (Rooms[j].Image == i) {
               Rooms[j].Image = 255;
            }
         }
   }
}

size_t hulk_coordinates = 0x26db;
size_t hulk_item_image_offsets = 0x2798;
size_t hulk_look_image_offsets = 0x27bc;
size_t hulk_special_image_offsets = 0x276e;
size_t hulk_image_offset = 0x441b;


void SagaSetup(size_t imgoffset)
{
    int32_t i, y;

    uint16_t image_offsets[Game->number_of_pictures];

    if (palchosen == NO_PALETTE) {
        palchosen = Game->palette;
    }

    if (palchosen == NO_PALETTE) {
        debug_print("unknown palette\n");
        exit(EXIT_FAILURE);
    }

    DefinePalette();

    int version = Game->picture_format_version;

    int32_t CHAR_START = Game->start_of_characters + file_baseline_offset;
    int32_t OFFSET_TABLE_START = Game->start_of_image_data + file_baseline_offset;

    if (Game->start_of_image_data == FOLLOWS) {
        OFFSET_TABLE_START = CHAR_START + 0x800;
    }

    int32_t DATA_OFFSET = Game->image_address_offset + file_baseline_offset;
    if (imgoffset)
        DATA_OFFSET = imgoffset;
    uint8_t *pos;
    int numgraphics = Game->number_of_pictures;
    pos = SeekToPos(entire_file, CHAR_START);

#ifdef DRAWDEBUG
    debug_print("Grabbing Character details\n");
    debug_print("Character Offset: %04x\n",
        CHAR_START - file_baseline_offset);
#endif
    for (i = 0; i < 256; i++) {
        for (y = 0; y < 8; y++) {
            sprite[i][y] = *(pos++);
        }
    }

    /* Now we have hopefully read the character data */
    /* Time for the image offsets */

    images = (Image *)MemAlloc(sizeof(Image) * numgraphics);
    Image *img = images;

    pos = SeekToPos(entire_file, OFFSET_TABLE_START);

    int broken_claymorgue_pictures_c64 = 0;
    int broken_claymorgue_pictures_zx = 0;

    for (i = 0; i < numgraphics; i++) {
        if (Game->picture_format_version == 0) {
            uint16_t address;

            if (i < 11) {
                address = Game->start_of_image_data + (i * 2);
            } else if (i < 28) {
                address = hulk_item_image_offsets + (i - 10) * 2;
            } else if (i < 34) {
                address = hulk_look_image_offsets + (i - 28) * 2;
            } else {
                address = hulk_special_image_offsets + (i - 34) * 2;
            }

            address += file_baseline_offset;
            address = entire_file[address] + entire_file[address + 1] * 0x100;

            image_offsets[i] = address + hulk_image_offset;
        } else {
            image_offsets[i] = *(pos++);
            image_offsets[i] += *(pos++) * 0x100;
        }
    }

    for (int picture_number = 0; picture_number < numgraphics; picture_number++) {
        pos = SeekToPos(entire_file, image_offsets[picture_number] + DATA_OFFSET);
        if (pos == 0)
            return;

        img->width = *(pos++);
        if (img->width > 32)
            img->width = 32;

        img->height = *(pos++);
        if (img->height > 12)
            img->height = 12;

        if (version > 0) {
            img->xoff = *(pos++);
            if (img->xoff > 32)
                img->xoff = 4;
            img->yoff = *(pos++);
            if (img->yoff > 12)
                img->yoff = 0;
        } else {
            if (picture_number > 9 && picture_number < 28) {
                img->xoff = entire_file[hulk_coordinates + picture_number - 10 + file_baseline_offset];
                img->yoff = entire_file[hulk_coordinates + 18 + picture_number - 10 + file_baseline_offset];
            } else {
                img->xoff = img->yoff = 0;
            }
        }

        if (CurrentGame == CLAYMORGUE_C64 && img->height == 0 && img->width == 0 && picture_number == 13) {
            PatchOutBrokenClaymorgueImagesC64();
            broken_claymorgue_pictures_c64 = 1;
        }

        if (broken_claymorgue_pictures_c64 && (picture_number == 16 || picture_number == 28)) {
            img->height = 12;
            img->width = 32;
            img->xoff = 4;
            img->yoff = 0;
            if (picture_number == 28)
                pos = entire_file + 0x2005;
        }

        if (CurrentGame == CLAYMORGUE && img->height == 0 && img->width == 0 && picture_number == 9) {
            PatchOutBrokenClaymorgueImagesZX();
            broken_claymorgue_pictures_zx = 1;
        }

        if (broken_claymorgue_pictures_zx && (picture_number == 14)) {
            img->height = 12;
            img->width = 32;
            img->xoff = 4;
            img->yoff = 0;
        }

        img->imagedata = pos;

        int patch = FindImagePatch(CurrentGame, picture_number, 0);
        while (patch) {
            Patch(img->imagedata, patch);
            patch = FindImagePatch(CurrentGame, picture_number, patch);
        }

        img++;
    }
}

void PrintImageContents(int index, uint8_t *data, size_t size)
{
    debug_print("/* image %d ", index);
    debug_print(
        "width: %d height: %d xoff: %d yoff: %d size: %zu bytes*/\n{ ",
        images[index].width, images[index].height, images[index].xoff,
        images[index].yoff, size);
    for (int i = 0; i < size; i++) {
        debug_print("0x%02x, ", data[i]);
        if (i % 8 == 7)
            debug_print("\n  ");
    }

    debug_print(" },\n");
    return;
}

void debugdrawcharacter(int character)
{
    debug_print("Contents of character %d of 256:\n", character);
    for (int row = 0; row < 8; row++) {
        for (int n = 0; n < 8; n++) {
            if (isNthBitSet(sprite[character][row], n))
                debug_print("■");
            else
                debug_print("0");
        }
        debug_print("\n");
    }
    if (character != 255)
        debugdrawcharacter(255);
}

void debugdraw(int on, int character, int xoff, int yoff, int width)
{
    if (on) {
        int x = character % width;
        int y = character / width;
        plotsprite(character, x + xoff, y + yoff, 0, 15);
        debug_print("Contents of character position %d:\n", character);
        for (int row = 0; row < 8; row++) {
            for (int n = 0; n < 8; n++) {
                if (isNthBitSet(screenchars[character][row], n))
                    debug_print("■");
                else
                    debug_print("0");
            }
            debug_print("\n");
        }
    }
}

uint8_t *DrawSagaPictureFromData(uint8_t *dataptr, int xsize, int ysize,
    int xoff, int yoff)
{
    int32_t offset = 0, cont = 0;
    int32_t i, x, y, mask_mode;
    uint8_t data, data2, old = 0;
    int32_t ink[0x22][14], paper[0x22][14];

    //   uint8_t *origptr = dataptr;
    int version = Game->picture_format_version;

    offset = 0;
    int32_t character = 0;
    int32_t count;
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
            debug_print("******* SOLO CHARACTER: %04x\n", character);
#endif
            transform(character, 0, offset);
            offset++;
            if (offset > 767)
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

            for (i = 0; i < count; i++)
                transform(character, (data & 0x0c) ? (data & 0xf3) : data, offset + i);

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
                        debug_print("Plotting %d directly (overlay) at %d\n", data2,
                            offset);
#endif
                        for (i = 0; i < count; i++)
                            transform(data2, old & 0x0c, offset + i);
                    } else {
                        character = *dataptr++;
                        if ((data2 & 1) == 1)
                            character += 128;
#ifdef DRAWDEBUG
                        debug_print("Plotting %d with flip %02x (%s) at %d %d\n",
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
    } while (offset < xsize * ysize);

    y = 0;
    x = 0;

    //   debug_print("Attribute data begins at offset %ld\n", dataptr -
    //   origptr);

    uint8_t colour = 0;
    // Note version 3 count is inverse it is repeat previous colour
    // Whilst version0-2 count is repeat next character
    while (y < ysize) {
        if (dataptr - entire_file > file_length)
            return dataptr - 1;
        data = *dataptr++;
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
            debug_print("(gfx#:plotting %d,%d:paper=%s,ink=%s)\n", x + xoff2,
                y + yoff, colortext(remap(paper[x][y])),
                colortext(remap(ink[x][y])));
#endif
            offset++;
            if (offset > 766)
                break;
        }
    return dataptr;
}

void DrawSagaPictureNumber(int picture_number)
{
    int numgraphics = Game->number_of_pictures;
    if (picture_number >= numgraphics) {
        debug_print("Invalid image number %d! Last image:%d\n", picture_number,
            numgraphics - 1);
        return;
    }

    Image img = images[picture_number];

    if (img.imagedata == NULL)
        return;

    DrawSagaPictureFromData(img.imagedata, img.width, img.height, img.xoff,
        img.yoff);

    last_image_index = picture_number;
}

void DrawSagaPictureAtPos(int picture_number, int x, int y)
{
    Image img = images[picture_number];

    DrawSagaPictureFromData(img.imagedata, img.width, img.height, x, y);
}

void SwitchPalettes(int pal1, int pal2)
{
    uint8_t temp[3];

    temp[0] = pal[pal1][0];
    temp[1] = pal[pal1][1];
    temp[2] = pal[pal1][2];

    pal[pal1][0] = pal[pal2][0];
    pal[pal1][1] = pal[pal2][1];
    pal[pal1][2] = pal[pal2][2];

    pal[pal2][0] = temp[0];
    pal[pal2][1] = temp[1];
    pal[pal2][2] = temp[2];
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

                    glui32 glk_color = (pal[ink][0] << 16) | (pal[ink][1] << 8) | pal[ink][2];

                    glk_window_fill_rect(
                        Graphics, glk_color, col * 8 * pixel_size + x_offset,
                        (line * 8 + i) * pixel_size, 8 * pixel_size, pixel_size);
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
