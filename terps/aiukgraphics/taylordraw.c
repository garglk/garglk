//
//  taylordraw.c
//  Part of the AI UK graphics library (shared by ScottFree and TaylorMade)
//
//  Created by Administrator on 2025-12-10.
//
//  High-level image composition for TaylorMade (Adventure Soft) adventure games
//  (Fantastic Four, He-Man, Rebel Planet, Blizzard Pass, Kayleth, Temple of Terror.)
//  Also used by the ScottFree interpreter for Seas of Blood.
//
//  Images are assembled by executing a bytecode instruction
//  stream stored per location. Instructions draw "Irmak" format sub-images
//  into the shared imagebuffer, then apply post-processing transforms
//  (mirror, flip, colour replacement) to build the final frame.
//
//  The instruction set uses opcodes 0xE8–0xFF for special operations
//  and treats all other byte values as sub-image indices to draw at
//  a given (x, y) position via DrawPictureAtPos().
//

#include <stdlib.h>
#include <string.h>

#include "taylordraw.h"

#include "irmak.h"

/* The instructions. Any other byte is the number of a sub-image to
   draw, followed by its position (x, y). */
enum {
    OP_END = 0xff,
    OP_MIRROR_LEFT_HALF = 0xfe,
    OP_REPLACE_COLOUR = 0xfd,   /* (before, after) */
    OP_SET_ATTRIBUTE = 0xfc,    /* Older: (x, y, attribute, length)
                                   Newer: (y, x, height, attribute, width) */
    OP_MAKE_BRIGHT = 0xfb,
    OP_FLIP_HORIZONTALLY = 0xfa,
    OP_DRAW_LOCATION = 0xf9,    /* (location), or in Seas of Blood:
                                   draw the objects, (x, y) */
    OP_IF_OBJECT = 0xf8,        /* Older: (object, image, x, y)
                                   Newer: (object) */
    OP_DRAW_F7 = 0xf7,          /* (image, x, y) */
    OP_DRAW_F6 = 0xf6,          /* (image, x, y) */
    OP_DRAW_F5 = 0xf5,          /* (image, x, y) */
    OP_STOP_IF_OBJECT = 0xf4,   /* (object) */
    OP_MIRROR_TOP_HALF = 0xf3,
    OP_MIRROR_AREA_RIGHT = 0xf2, /* (y, x, end row, width) */
    OP_MIRROR_AREA_DOWN = 0xf1, /* (x, y, last row, width) */
    /* 0xf0 and 0xef are images */
    OP_FLIP_AREA_HORIZONTALLY = 0xee, /* (y, x, end row, width) */
    OP_FLIP_VERTICALLY = 0xed,
    OP_FLIP_AREA_VERTICALLY = 0xec, /* (x, y, last row, width) */
    OP_UNIMPLEMENTED_EB = 0xeb,
    OP_UNIMPLEMENTED_EA = 0xea,
    OP_REPLACE_INK_AND_PAPER = 0xe9, /* (before, after) */
    OP_CLEAR = 0xe8,
};

/* How deep OP_DRAW_LOCATION may nest. The games go two deep (three
   in the garbage streams past the last real picture). */
#define MAX_DRAW_DEPTH 8

#define LAST_ROW (IRMAK_IMGHEIGHT - 1)

/* The area instructions take whatever operands the picture data
   gives them. imagebuffer[] is treated as one long row of cells, as
   in the original: a column past the right edge is a cell of the
   next row. Only the end of the buffer is a limit. */

/* True if 'width' cells of row 'line', starting at column x,
   are all inside imagebuffer[]. */
static int row_fits(int line, int x, int width)
{
    return line * IRMAK_IMGWIDTH + x + width <= IRMAK_IMGSIZE;
}

static void swap_cells(uint8_t *a, uint8_t *b)
{
    uint8_t temp[IRMAK_CELL_BYTES];

    memcpy(temp, a, IRMAK_CELL_BYTES);
    memcpy(a, b, IRMAK_CELL_BYTES);
    memcpy(b, temp, IRMAK_CELL_BYTES);
}

/* Turn the pixels of a cell upside down. */
static void flip_cell_vertically(uint8_t *cell)
{
    for (int pixrow = 0; pixrow < 4; pixrow++) {
        uint8_t temp = cell[pixrow];
        cell[pixrow] = cell[7 - pixrow];
        cell[7 - pixrow] = temp;
    }
}

/* Mirror a rectangular area horizontally: copy the left half of
   the region to the right half, flipping each tile. Used to create
   symmetric scenes (e.g. doorways, corridors) from half an image.
   The area spans columns [x, x+width) and rows [y1, end_row).
   Stops at the first row that reaches past the end of the buffer. */
static void mirror_area(int x, int y1, int width, int end_row)
{
    for (int line = y1; line < end_row; line++) {
        if (!row_fits(line, x, width))
            return;
        int source = line * IRMAK_IMGWIDTH + x;
        int target = source + width - 1;
        for (; source < target; source++, target--) {
            memcpy(imagebuffer[target], imagebuffer[source], IRMAK_CELL_BYTES);
            Flip(imagebuffer[target]);
        }
    }
}

/* Mirror a rectangular area vertically: copy the top half downward
   into the bottom half, reversing pixel rows within each tile.
   The area spans columns [x, x+width) and rows y1 to last_row,
   inclusive.

   The number of rows copied is taken from last_row alone, which is
   only half the area if y1 is 0. With another y1 it is more, and rows
   are copied that have been written to already. This is as it should
   be: He-Man's room 32 has (16, 5, 6, 6), which copies row 5 to row 6,
   row 6 to row 5, row 7 to row 4 and row 8 to row 3, and comes out as
   in the original. */
static void mirror_area_vertically(int x, int y1, int width, int last_row)
{
    for (int line = 0; line <= last_row / 2; line++) {
        for (int col = x; col < x + width; col++) {
            int target = (last_row - line) * IRMAK_IMGWIDTH + col;
            int source = (y1 + line) * IRMAK_IMGWIDTH + col;
            if (target >= IRMAK_IMGSIZE || source >= IRMAK_IMGSIZE)
                return;
            imagebuffer[target][IRMAK_CELL_ATTR] = imagebuffer[source][IRMAK_CELL_ATTR];
            /* Not flip_cell_vertically(): target and source
               may be the same cell */
            for (int pixrow = 0; pixrow < 8; pixrow++)
                imagebuffer[target][7 - pixrow] = imagebuffer[source][pixrow];
        }
    }
}

/* Flip a rectangular sub-region left-to-right within the imagebuffer.
   Columns within [x, x+width) for rows [y1, end_row) are reversed,
   with each tile also flipped internally. The columns are swapped
   in pairs, as in the original, so the middle column of an odd
   number is left as it is.
   Does nothing if the area reaches past the end of the buffer. */
static void flip_area_horizontally(int x, int y1, int width, int end_row)
{
    if (end_row <= y1 || !row_fits(end_row - 1, x, width))
        return;

    for (int line = y1; line < end_row; line++) {
        int left = line * IRMAK_IMGWIDTH + x;
        int right = left + width - 1;
        for (; left < right; left++, right--) {
            swap_cells(imagebuffer[left], imagebuffer[right]);
            Flip(imagebuffer[left]);
            Flip(imagebuffer[right]);
        }
    }
}

/* Flip a rectangular sub-region top-to-bottom within the imagebuffer.
   Rows y1 to last_row (inclusive) in columns [x, x+width) are
   reversed, with pixel rows within each tile also inverted. The rows
   are swapped in pairs, as in the original, so the middle row of an
   odd number is left as it is.
   Does nothing if the area reaches past the end of the buffer. */
static void flip_area_vertically(int x, int y1, int width, int last_row)
{
    if (!row_fits(last_row, x, width))
        return;

    for (int top = y1, bottom = last_row; top < bottom; top++, bottom--) {
        for (int col = x; col < x + width; col++) {
            uint8_t *upper = imagebuffer[top * IRMAK_IMGWIDTH + col];
            uint8_t *lower = imagebuffer[bottom * IRMAK_IMGWIDTH + col];
            swap_cells(upper, lower);
            flip_cell_vertically(upper);
            flip_cell_vertically(lower);
        }
    }
}

/* Paint a horizontal run of tile cells with a colour attribute.
   Used by the older instruction format (Blizzard Pass, Rebel Planet)
   where 0xFC takes (x, y, colour, length). */
static void draw_colour_old(uint8_t x, uint8_t y, uint8_t colour, uint8_t length)
{
    int index = y * IRMAK_IMGWIDTH + x;
    if (index >= IRMAK_IMGSIZE)
        return;
    if (index + length > IRMAK_IMGSIZE)
        length = IRMAK_IMGSIZE - index;
    for (int i = 0; i < length; i++) {
        imagebuffer[index + i][IRMAK_CELL_ATTR] = colour;
    }
}

/* Paint a rectangular area of tile cells with a colour attribute.
   Used by the newer instruction format (He-Man and later) where
   0xFC takes (y, x, height, colour, width). */
static void draw_colour(uint8_t colour, uint8_t x, uint8_t y, uint8_t width, uint8_t height)
{
    for (int h = 0; h < height; h++) {
        for (int w = 0; w < width; w++) {
            int index = (y + h) * IRMAK_IMGWIDTH + x + w;
            if (index >= IRMAK_IMGSIZE)
                return;
            imagebuffer[index][IRMAK_CELL_ATTR] = colour;
        }
    }
}

/* Set the BRIGHT flag on every tile cell in the imagebuffer,
   making all colours use their bright variant. */
static void make_light(void)
{
    for (int i = 0; i < IRMAK_IMGSIZE; i++) {
        imagebuffer[i][IRMAK_CELL_ATTR] |= BRIGHT_FLAG;
    }
}

/* Convert an ink-format colour value to paper-format by shifting
   the ink bits into the paper field position, preserving brightness. */
static uint8_t ink2paper(uint8_t ink)
{
    uint8_t paper = (ink & INK_MASK) << 3;
    return (ink & BRIGHT_FLAG) | paper;
}

/* Replace all occurrences of 'before' with 'after' in the masked
   portion of every cell's attribute byte. The mask selects which
   bits to compare and replace (e.g. INK_MASK or PAPER_MASK).

   Both values must be in the position of the mask already. 'after'
   is not masked: bits of it outside the mask are set in the result. */
static void replace(uint8_t before, uint8_t after, uint8_t mask)
{
    for (int j = 0; j < IRMAK_IMGSIZE; j++) {
        uint8_t *attribute = &imagebuffer[j][IRMAK_CELL_ATTR];
        if ((*attribute & mask) == before)
            *attribute = (*attribute & ~mask) | after;
    }
}

/* Replace one colour with another across the whole image, in both
   the ink and the paper field. Brightness is neither compared nor
   changed. */
static void replace_colour(uint8_t before, uint8_t after)
{
    uint8_t beforeink = before & INK_MASK;
    uint8_t afterink = after & INK_MASK;

    replace(beforeink, afterink, INK_MASK);
    replace(beforeink << 3, afterink << 3, PAPER_MASK);
}

/* Replace a colour in both ink and paper fields, with brightness as
   a part of the colour. 'before' and 'after' are specified in ink
   format; ink2paper() converts them for the paper field replacement.

   'after' goes into the ink pass as it is, so any paper or flash
   bits in it are set in the cells whose ink is replaced. */
static void replace_paper_and_ink(uint8_t before, uint8_t after)
{
    replace(before & (INK_MASK | BRIGHT_FLAG), after, INK_MASK | BRIGHT_FLAG);
    replace(ink2paper(before), ink2paper(after), PAPER_MASK | BRIGHT_FLAG);
}

/* The game data and game-specific configuration, as passed
   to InitTaylor(). */
static taylor_draw_config config;

/* Precomputed index of stream start pointers. stream_offsets[i]
   points to the first byte of stream i's instruction data (the
   byte after the i-th 0xFF terminator, or the start for stream 0).
   Built once during InitTaylor() for O(1) lookup in DrawTaylor(). */
static uint8_t **stream_offsets;
static int stream_count;

/* Initialise the TaylorMade drawing system with pointers into the
   game data and game-specific configuration flags. Builds the
   stream offset index by scanning for 0xFF terminators. */
void InitTaylor(const taylor_draw_config *cfg) {
    config = *cfg;

    uint8_t *data = config.data;
    uint8_t *end = config.end;

    /* Count 0xFF-terminated streams to size the index. */
    int count = 0;
    for (uint8_t *p = data; p < end; p++) {
        if (*p == OP_END)
            count++;
    }
    /* count is the number of terminators; there are count streams
       (stream 0 starts at data, each 0xFF starts the next). */
    stream_count = count;

    free(stream_offsets);
    stream_offsets = malloc(sizeof(uint8_t *) * (count + 1));
    if (!stream_offsets) {
        stream_count = 0;
        return;
    }

    stream_offsets[0] = data;
    int idx = 1;
    for (uint8_t *p = data; p < end && idx <= count; p++) {
        if (*p == OP_END)
            stream_offsets[idx++] = p + 1;
    }
}

/* Whether an object is in the room. The object location array of the
   game has the room of every object: objloc[obj] == room means object
   'obj' is in that room. */
static int object_is_at(uint8_t obj, int location)
{
    return config.objloc != NULL && config.objloc[obj] == location;
}

/* The number of operand bytes that follow an instruction. */
static int operand_count(uint8_t op)
{
    int older = (config.flags & TAYLOR_OLDER) != 0;

    switch (op) {
    case OP_END:
    case OP_MIRROR_LEFT_HALF:
    case OP_MAKE_BRIGHT:
    case OP_FLIP_HORIZONTALLY:
    case OP_MIRROR_TOP_HALF:
    case OP_FLIP_VERTICALLY:
    case OP_UNIMPLEMENTED_EB:
    case OP_UNIMPLEMENTED_EA:
    case OP_CLEAR:
        return 0;
    case OP_STOP_IF_OBJECT:
        return 1;
    case OP_REPLACE_COLOUR:
    case OP_REPLACE_INK_AND_PAPER:
        return 2;
    case OP_DRAW_F7:
    case OP_DRAW_F6:
    case OP_DRAW_F5:
        return 3;
    case OP_MIRROR_AREA_RIGHT:
    case OP_MIRROR_AREA_DOWN:
    case OP_FLIP_AREA_HORIZONTALLY:
    case OP_FLIP_AREA_VERTICALLY:
        return 4;
    case OP_SET_ATTRIBUTE:
        return older ? 4 : 5;
    case OP_DRAW_LOCATION:
        return config.draw_objects != NULL ? 2 : 1;
    case OP_IF_OBJECT:
        return older ? 4 : 1;
    default: /* Draw sub-image */
        return 2;
    }
}

/* Execute the draw instruction stream of location 'loc', see
   DrawTaylor(). 'depth' is the number of streams being executed
   that led to this one through OP_DRAW_LOCATION. */
static int draw_stream(int loc, int current_location, int depth)
{
    if (loc < 0 || loc >= stream_count || !stream_offsets)
        return 0;

    const uint8_t *end = config.end;
    const uint8_t *ptr = stream_offsets[loc];

    /* Execute instructions until 0xFF (end) or data runs out. */
    while (ptr < end) {
        uint8_t op = *ptr;
        const uint8_t *arg = ptr + 1;

        /* Rebel Planet special case: the Zodiac Hotel basement
           (room 43) uses 0xF7 to draw bottles in a locked alcove.
           The original interpreter never renders them. We show
           them only when the alcove is unlocked (detected by the
           phonic fork, object 131, being out of play in room 252).
           Safe because no other game/image uses 0xF7. */
        if (op == OP_DRAW_F7 && (config.flags & TAYLOR_REBEL_PLANET)
            && current_location == 43 && object_is_at(131, 252))
            return 1;

        int operands = operand_count(op);
        if (end - arg < operands)
            return 0;

        switch (op) {
        case OP_END:
            return 1;

        case OP_MIRROR_LEFT_HALF:
            mirror_area(0, 0, IRMAK_IMGWIDTH, IRMAK_IMGHEIGHT);
            break;

        case OP_REPLACE_COLOUR:
            replace_colour(arg[0], arg[1]);
            break;

        case OP_SET_ATTRIBUTE: /* Older format: a linear run.
                                  Newer format: a rectangle. */
            if (config.flags & TAYLOR_OLDER)
                draw_colour_old(arg[0], arg[1], arg[2], arg[3]);
            else
                draw_colour(arg[3], arg[1], arg[0], arg[4], arg[2]);
            break;

        case OP_MAKE_BRIGHT:
            make_light();
            break;

        case OP_FLIP_HORIZONTALLY:
            flip_area_horizontally(0, 0, IRMAK_IMGWIDTH, IRMAK_IMGHEIGHT);
            break;

        case OP_DRAW_LOCATION: /* Draw the picture of another location
                                  as a part of this one, or call the
                                  Seas of Blood object draw callback. */
            if (config.draw_objects != NULL)
                config.draw_objects(arg[0], arg[1]);
            else if (depth < MAX_DRAW_DEPTH)
                draw_stream(arg[0], current_location, depth + 1);
            break;

        case OP_IF_OBJECT: /* Older: draw an image if the object is here.
                              Newer: stop drawing if the object is NOT here. */
            if (config.flags & TAYLOR_OLDER) {
                if (object_is_at(arg[0], current_location))
                    DrawPictureAtPos(arg[1], arg[2], arg[3], 1);
            } else if (!object_is_at(arg[0], current_location)) {
                return 1;
            }
            break;

        case OP_STOP_IF_OBJECT: /* Stop drawing if the object IS here */
            if (object_is_at(arg[0], current_location))
                return 1;
            break;

        case OP_MIRROR_TOP_HALF:
            mirror_area_vertically(0, 0, IRMAK_IMGWIDTH, LAST_ROW);
            break;

        case OP_MIRROR_AREA_RIGHT:
            mirror_area(arg[1], arg[0], arg[3], arg[2]);
            break;

        case OP_MIRROR_AREA_DOWN:
            mirror_area_vertically(arg[0], arg[1], arg[3], arg[2]);
            break;

        case OP_FLIP_AREA_HORIZONTALLY:
            flip_area_horizontally(arg[1], arg[0], arg[3], arg[2]);
            break;

        case OP_FLIP_VERTICALLY:
            flip_area_vertically(0, 0, IRMAK_IMGWIDTH, LAST_ROW);
            break;

        case OP_FLIP_AREA_VERTICALLY:
            flip_area_vertically(arg[0], arg[1], arg[3], arg[2]);
            break;

        case OP_UNIMPLEMENTED_EB:
        case OP_UNIMPLEMENTED_EA:
            return 0;

        case OP_REPLACE_INK_AND_PAPER:
            replace_paper_and_ink(arg[0], arg[1]);
            break;

        case OP_CLEAR: /* Clear the compositing buffer */
            ClearGraphMem();
            break;

        case OP_DRAW_F7: /* Vestigial instructions: the original sets the A */
        case OP_DRAW_F6: /* register to 0x0C, 0x04 or 0x08 and then draws the */
        case OP_DRAW_F5: /* image, but the draw routine does not use A. */
            DrawPictureAtPos(arg[0], arg[1], arg[2], 1);
            break;

        default: /* Draw sub-image: the byte is the image index */
            DrawPictureAtPos(op, arg[0], arg[1], 1);
            break;
        }
        ptr = arg + operands;
    }
    return 0;
}

/* Execute the draw instruction stream for location 'loc'.
   'current_location' is the player's actual room — usually the same
   as 'loc', but differs when an animation draws a frame that is the
   picture of another location. Object visibility checks (0xF8, 0xF4)
   always test against current_location.

   The target stream is found via the precomputed stream_offsets[]
   index built during InitTaylor().

   Returns 1 on success, 0 on error or missing data. */
int DrawTaylor(int loc, int current_location)
{
    return draw_stream(loc, current_location, 0);
}
