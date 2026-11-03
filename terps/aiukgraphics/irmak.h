//
//  irmak.h
//  Part of the AI UK graphics library (shared by ScottFree and TaylorMade)
//
//  Created by Administrator on 2025-12-07.
//

#ifndef irmak_h
#define irmak_h

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t *imagedata;
    size_t datasize;
    uint8_t xoff;
    uint8_t yoff;
    uint8_t width;
    uint8_t height;
} Image;

/* Colour attribute byte, version 3+ format: the Spectrum's, less FLASH.
   (The masks of the version 0–2 format are private to irmak.c.) */
#define INK_MASK         0x07
#define PAPER_MASK       0x38
#define BRIGHT_FLAG      0x40

/* The ink and paper of an attribute byte as Spectrum colour numbers
   (0–15: the eight colours, then their bright variants), ready to be
   passed to Remap(). */
static inline uint8_t AttributeInk(uint8_t attribute)
{
    return (attribute & INK_MASK) + ((attribute & BRIGHT_FLAG) ? 8 : 0);
}

static inline uint8_t AttributePaper(uint8_t attribute)
{
    return ((attribute & PAPER_MASK) >> 3) + ((attribute & BRIGHT_FLAG) ? 8 : 0);
}

/* The image area consists of 12 rows
   of 32 tile cells each
   (for a total of 384 tiles) */
#define IRMAK_IMGWIDTH 32
#define IRMAK_IMGHEIGHT 12
#define IRMAK_IMGSIZE (IRMAK_IMGWIDTH * IRMAK_IMGHEIGHT)

/* A cell of imagebuffer[]: eight pixel rows (MSB = left),
   then the colour attribute. */
#define IRMAK_CELL_ATTR 8
#define IRMAK_CELL_BYTES 9

/* Whether the pixel in the given column (0 = leftmost) of a pixel row is
   set. */
static inline int PixelSet(uint8_t row, int column)
{
    return (row & (0x80 >> column)) != 0;
}

/* Passed as x or y to DrawPictureAtPos(): use the position
   stored with the image. Any value outside the image area
   means the same. */
#define IRMAK_DEFAULT_POS 0xff

void InitIrmak(int numimg, int imgver);

void ClearGraphMem(void);
void DrawPictureNumber(int picture_number, int draw_to_buffer);
int DrawPictureAtPos(int picture_number, uint8_t x, uint8_t y, int draw_to_buffer);
void DrawIrmakPictureFromBuffer(void);
void DrawIrmakBufferChanges(const uint8_t shown[][IRMAK_CELL_BYTES]);

void Flip(uint8_t *tile);
void RotateTile(uint8_t *tile, int rotation);

/* Used by SagaGraphicsSetup() and TaylorSetup() */
extern uint8_t tiles[256][8];
/* Used by animate_waterfall() in Robin of Sherwood.
   This contains the tile data used by an image, which
   might be smaller than the image area */
extern uint8_t layout[IRMAK_IMGSIZE][8];
/* Used by Seas of Blood and Taylormade draw routines.
   This always contains the full 32 x 12 tiles of
   the image area, plus attributes (in the ninth byte) */
extern uint8_t imagebuffer[IRMAK_IMGSIZE][IRMAK_CELL_BYTES];

/* The image table, filled in by the host's loader,
   and the two values passed to InitIrmak(). */
extern Image *images;
extern int number_of_images;
extern int image_version;

#endif /* irmak_h */
