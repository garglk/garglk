//
//  stdraw.c
//  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
//  Routines to draw Atari ST graphics
//
//  Created by Petter Sjölund on 2022-01-30.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "animations.h"
#include "common.h"
#include "definitions.h"
#include "glk.h"
#include "graphics.h"
#include "read_le16.h"
#include "stdraw.h"

/* The most pixel runs we record for one cycling colour in one image */
#define MAX_ANIM_PIXELS 3000

/* Bytes before the picture data proper: 2 unknown, 2 data size, 2 screen
   offset, 1 width, 1 number of cycling colours, and a 16 colour palette */
#define ST_IMAGE_HEADER_SIZE 40

/* The ST screen is 160 bytes (320 pixels) wide in low resolution */
#define ST_BYTES_PER_LINE 160

/* Claymorgue room images which are small, but still replace the whole
   picture rather than being drawn on top of it */
static const int claymorgue_small_rooms[] = { 0, 5, 11, 16, 18, 31 };

/* Spider-Man special images that want a white background behind them */
static const int spiderman_white_specials[] = { 6, 7, 8, 12, 18 };

static int xoff = 0, yoff = 0;

/* A horizontal run of pixels in a colour that is being cycled */
typedef struct {
    uint16_t x;
    uint16_t y;
    uint16_t width;
} Pixel;

typedef struct {
    uint8_t ColIdx;
    uint8_t CurCol;
    uint8_t NumCol;
    glui32 *Colors;
    int StartOffset;
    int Rate;
    int NumPix;
    Pixel *Pixels;
} AnimationColor;

static int IsSTBitSet(int bit, uint8_t byte)
{
    return ((byte & (1 << bit)) != 0);
}

static uint32_t ImgAddrOffs, NibblesWide, NxtLineStart, InitialMask, LastMask, CurAddr, LineNibblesLeft;

static uint32_t ColorCycle = 0;

/* The cycling colours of the picture on screen. The first NumAnimCols
   entries are valid; each owns its Colors and Pixels arrays. */
static AnimationColor *AnimColors = NULL;
static int NumAnimCols = 0;

static int IsInList(int value, const int *list, size_t count)
{
    for (size_t i = 0; i < count; i++)
        if (list[i] == value)
            return 1;
    return 0;
}

/* Convert a 4-bit-per-channel ST palette entry to a Glk colour. The
   scale factor is 35.7, done in integers. */
static glui32 StColToGlk(uint8_t hi, uint8_t lo)
{
    int blue = (lo & 0xf) * 357 / 10;
    int green = ((lo >> 4) & 0xf) * 357 / 10;
    int red = (hi & 0xf) * 357 / 10;

    return (red << 16) | (green << 8) | blue;
}

static void FreeAnimColArray(AnimationColor *cols, int count)
{
    if (cols == NULL)
        return;
    for (int i = 0; i < count; i++) {
        free(cols[i].Colors);
        free(cols[i].Pixels);
    }
    free(cols);
}

static void FreeAnimCols(void)
{
    FreeAnimColArray(AnimColors, NumAnimCols);
    AnimColors = NULL;
    NumAnimCols = 0;
}

/* Read the NumAnimCols colour cycling records at ptr into a new
   AnimColors array. Returns a pointer to the first byte after the
   records. */
static uint8_t *SetPaletteAnimation(uint8_t *ptr, const uint8_t *end)
{
    if (NumAnimCols == 0)
        return ptr;

    AnimColors = MemAlloc(sizeof(AnimationColor) * NumAnimCols);
    for (int i = 0; i < NumAnimCols; i++) {
        AnimColors[i].Colors = NULL;
        AnimColors[i].Pixels = NULL;
        AnimColors[i].NumPix = 0;
    }
    AnimationColor *col = AnimColors;
    for (int i = 0; i < NumAnimCols; i++, col++) {
        if (end - ptr < 4 || end - ptr < 4 + 2 * (ptr[0] & 0xf)) {
            /* Truncated data */
            NumAnimCols = i;
            return (uint8_t *)end;
        }
        uint8_t val = *ptr++;
        col->ColIdx = 1 + (val >> 4);
        col->CurCol = 0;
        col->StartOffset = *ptr++;
        col->Rate = 1 + READ_BE_UINT16(ptr);
        ptr += 2;
        col->NumCol = val & 0xf;
        col->Colors = MemAlloc(col->NumCol * sizeof(glui32));
        for (int j = 0; j < col->NumCol; j++) {
            uint8_t hi = *ptr++;
            uint8_t lo = *ptr++;
            col->Colors[j] = StColToGlk(hi, lo);
        }
    }
    return ptr;
}

static uint8_t *PatternLookup = NULL;

static void GeneratePatternLookup(void)
{
    uint32_t pattern;
    uint16_t previous;

    pattern = 0xffff0000;
    PatternLookup = MemAlloc(1024);
    uint8_t *ptr = PatternLookup;
    do {
        for (int i = 0; i < 4; i++) {
            ptr[i] = 0;
            if ((pattern & 1 << i) != 0) {
                ptr[i] = ptr[i] | 0x33;
            }
            if ((pattern & 1 << (i + 4)) != 0) {
                ptr[i] = ptr[i] | 0xcc;
            }
        }
        ptr = ptr + 4;
        previous = pattern + 1;
        pattern = (pattern & 0xffff0000) | previous;
    } while (previous != 256);
}

static void DrawSTNibble(uint8_t byte, uint8_t mask, int xpos, int ypos, Pixel **pixels)
{
    uint16_t offs = byte << 2;
    int startbit = 7;
    int endbit = 4;
    if (mask == 0xf) {
        startbit = 3;
        endbit = 0;
    }

    for (int i = startbit; i >= endbit; i -= 2) {
        uint8_t col = 0;
        for (int j = 0; j < 4; j++) {
            if (IsSTBitSet(i, PatternLookup[offs + j]))
                col |= 1 << (3 - j);
        }

        if (col) {
            for (int c = 0; c < NumAnimCols; c++) {
                if (col == AnimColors[c].ColIdx && AnimColors[c].NumPix < MAX_ANIM_PIXELS) {
                    if (AnimColors[c].NumPix && pixels[c][AnimColors[c].NumPix - 1].y == ypos && pixels[c][AnimColors[c].NumPix - 1].x == xpos + 5 - i) {
                        pixels[c][AnimColors[c].NumPix - 1].width += 2;
                        break;
                    }

                    pixels[c][AnimColors[c].NumPix].x = xpos + 7 - i;
                    pixels[c][AnimColors[c].NumPix].y = ypos;
                    pixels[c][AnimColors[c].NumPix].width = 2;

                    AnimColors[c].NumPix++;

                    break;
                }
            }
            PutDoublePixel(xpos + 7 - i, ypos, col);
        }
    }
}

static void DrawPattern(uint8_t pattern, Pixel **pixels)
{
    // mask and LastMask alternate between 0x0f and 0xf0
    uint8_t mask = LastMask ^ 0xff;

    // CurAddr: screen memory where we're currently writing
    int y1 = CurAddr / ST_BYTES_PER_LINE;
    int x1 = 2 * (CurAddr % ST_BYTES_PER_LINE) + 6 * (CurAddr & 1);

    DrawSTNibble(pattern, mask, x1, y1, pixels);

    mask = mask ^ 0xff;
    LastMask = LastMask ^ 0xff;
    if (mask != 0xf) {
        CurAddr += 1 + 6 * (CurAddr & 1);
    }

    // LineNibblesLeft = nibbles remaining to draw this line

    LineNibblesLeft--;
    if (LineNibblesLeft != 0)
        return;

    // Reached end of line
    // NxtLine: start address of next line
    NxtLineStart += ST_BYTES_PER_LINE; // move down one line
    CurAddr = NxtLineStart;
    // InitialMask: mask at start of line
    mask = InitialMask;
    LastMask = mask ^ 0xff;
    LineNibblesLeft = NibblesWide;
}

int ColorCyclingRunning = 0;

/* Does the pixel run touch the rectangle? */
static int Intersects(Pixel pix, int xpos, int ypos, int width, int height)
{
    return MAX(pix.x, xpos) <= MIN(pix.x + pix.width, xpos + width)
        && ypos <= pix.y && pix.y <= ypos + height;
}

/* Append numpixels pixel runs to those of animcol */
static void AddPixels(AnimationColor *animcol, const Pixel *pixels, int numpixels)
{
    int total = animcol->NumPix + numpixels;
    Pixel *newpix = MemAlloc(sizeof(Pixel) * total);
    if (animcol->NumPix)
        memcpy(newpix, animcol->Pixels, sizeof(Pixel) * animcol->NumPix);
    memcpy(newpix + animcol->NumPix, pixels, sizeof(Pixel) * numpixels);
    free(animcol->Pixels);
    animcol->Pixels = newpix;
    animcol->NumPix = total;
}

/* The original cycles colour registers, and each new picture sets all of
   them and replaces the cycling with its own. An item picture repeats
   the cycling records of the rooms it is drawn in, so that the room goes
   on cycling around it. This interpreter repaints pixel runs instead, so
   the runs of an old cycling colour that the new picture leaves
   uncovered are moved to the new picture's colour with the same palette
   index. If the new picture has no such colour, they stop cycling. */
static void MoveNonHiddenColAnim(const AnimationColor *old, AnimationColor *new, int numnew, int xpos, int ypos, int width, int height)
{
    AnimationColor *target = NULL;
    for (int i = 0; i < numnew; i++) {
        if (new[i].ColIdx == old->ColIdx) {
            target = &new[i];
            break;
        }
    }

    if (target == NULL || old->NumPix == 0)
        return;

    int goodpixels = 0;
    Pixel *pixels = MemAlloc(sizeof(Pixel) * old->NumPix);

    for (int i = 0; i < old->NumPix; i++) {
        if (!Intersects(old->Pixels[i], xpos, ypos, width, height))
            pixels[goodpixels++] = old->Pixels[i];
    }

    if (goodpixels)
        AddPixels(target, pixels, goodpixels);
    free(pixels);
}

/* Pictures wider than this many nibbles (four pixels each) cover the
   whole graphics window, and clear it before they are drawn. */
#define ST_FULL_WIDTH_NIBBLES 70

int STImageClearsWindow(const uint8_t *imgdata, size_t total_size)
{
    return imgdata != NULL && total_size >= ST_IMAGE_HEADER_SIZE
        && imgdata[6] > ST_FULL_WIDTH_NIBBLES;
}

int DrawSTImageFromData(uint8_t *imgdata, size_t total_size)
{
    if (imgdata == NULL || total_size < ST_IMAGE_HEADER_SIZE)
        return 0;

    const uint8_t *endOfBuffer = imgdata + total_size;
    uint8_t *ptr = &imgdata[4];

    ImgAddrOffs = READ_BE_UINT16(ptr) * 2;
    ptr += 2;

    yoff = ImgAddrOffs / ST_BYTES_PER_LINE;
    xoff = 2 * (ImgAddrOffs % ST_BYTES_PER_LINE) + 6 * (ImgAddrOffs % 2);

    NibblesWide = *ptr++;

    /* Make sure to clear the last image and any color cycling for all Claymorgue room images */
    /* The small ones have to be special-cased */
    if (NibblesWide > ST_FULL_WIDTH_NIBBLES || (CurrentGame == CLAYMORGUE && LastImgType == IMG_ROOM && IsInList(LastImgIndex, claymorgue_small_rooms, ARRAY_SIZE(claymorgue_small_rooms)))) {
        ClearGraphicsWindow();
        FreeAnimCols();
    }

    /* Draw a white background behind some Spider-Man images */
    if (CurrentGame == SPIDERMAN && LastImgType == IMG_SPECIAL && IsInList(LastImgIndex, spiderman_white_specials, ARRAY_SIZE(spiderman_white_specials))) {
        glui32 winwidth, winheight;
        glk_window_get_size(Graphics, &winwidth, &winheight);
        int width_in_pixels = NibblesWide * 4 * pixel_size;
        GraphicsWindowChanged();
        glk_window_fill_rect(Graphics, 0xf9f8f9, ((int)winwidth - width_in_pixels) / 2, 0, width_in_pixels, winheight);
    }

    /* Set the cycling colours of the picture on screen aside, to merge
       what is left of them with those of the new picture below */
    AnimationColor *oldColAnim = AnimColors;
    int NumOldAnimCols = NumAnimCols;
    AnimColors = NULL;

    NumAnimCols = *ptr++;

    for (int i = 0; i <= 15; i++) {
        SetColor(i, StColToGlk(ptr[0], ptr[1]));
        ptr += 2;
    }

    /* No colour cycling in the dark, neither old nor new */
    if (CurrentGame != CLAYMORGUE && IsSet(DARKBIT)) {
        FreeAnimColArray(oldColAnim, NumOldAnimCols);
        oldColAnim = NULL;
        NumOldAnimCols = 0;
        NumAnimCols = 0;
    }

    ptr = SetPaletteAnimation(ptr, endOfBuffer);

    Pixel **Pixels = MemAlloc(sizeof(Pixel *) * NumAnimCols);
    for (int i = 0; i < NumAnimCols; i++)
        Pixels[i] = MemAlloc(sizeof(Pixel) * MAX_ANIM_PIXELS);

    uint16_t imgdatasize = READ_BE_UINT16(imgdata + 2);
    const uint8_t *endOfData = ptr + imgdatasize;
    if (endOfData > endOfBuffer)
        endOfData = endOfBuffer;

    if (PatternLookup == NULL)
        GeneratePatternLookup();

    CurAddr = ImgAddrOffs;

    // CurAddr == screen memory where we're currently writing

    InitialMask = 0xf0;
    LastMask = 0x0f;
    NxtLineStart = CurAddr;
    LineNibblesLeft = NibblesWide;

    /* The picture data is run-length encoded: a count byte with the high
       bit clear is followed by count + 1 literal bytes, one with the
       high bit set by a single byte to repeat (count & 0x7f) + 1 times.
       The loop condition is that of the original, which may start one
       last run right at the end of the data; no read goes past the end
       of the buffer. */
    while (ptr < endOfBuffer) {
        uint8_t c = *ptr++;
        if ((c & 0x80) == 0) {
            for (int i = c; i >= 0 && ptr < endOfBuffer; i--)
                DrawPattern(*ptr++, Pixels);
        } else {
            if (ptr >= endOfBuffer)
                break;
            c = c & 0x7f;
            uint8_t val = *ptr++;
            for (int i = c; i >= 0; i--)
                DrawPattern(val, Pixels);
        }
        if (ptr > endOfData)
            break;
    }

    int ImgHeight = NxtLineStart / ST_BYTES_PER_LINE - yoff;

    for (int i = 0; i < NumAnimCols; i++) {
        if (AnimColors[i].NumPix) {
            AnimColors[i].Pixels = MemAlloc(AnimColors[i].NumPix * sizeof(Pixel));
            memcpy(AnimColors[i].Pixels, Pixels[i], AnimColors[i].NumPix * sizeof(Pixel));
        }
        free(Pixels[i]);
    }
    free(Pixels);

    for (int i = 0; i < NumOldAnimCols; i++) {
        MoveNonHiddenColAnim(&oldColAnim[i], AnimColors, NumAnimCols, xoff, yoff, NibblesWide * 4, ImgHeight);
    }

    /* What survived was copied */
    FreeAnimColArray(oldColAnim, NumOldAnimCols);

    if (NumAnimCols) {
        ColorCyclingRunning = 1;
        ColorCycle = 0;
        SetTimer(25);
    } else {
        ColorCyclingRunning = 0;
        if (!AnimationRunning)
            SetTimer(0);
    }

    return 1;
}

static void ColorCyclingPixel(Pixel p, glui32 glk_color)
{
    glsi32 xpos = p.x * pixel_size;

    if (upside_down)
        xpos = ImageWidth * pixel_size - xpos - (pixel_size * (p.width - 2));
    xpos += x_offset;

    if (xpos < x_offset || xpos >= x_offset + ImageWidth * pixel_size) {
        return;
    }

    int ypos = p.y * pixel_size;
    if (upside_down)
        ypos = ImageHeight * pixel_size - ypos - 3 * pixel_size;
    ypos += y_offset;

    GraphicsWindowRecoloured();
    glk_window_fill_rect(Graphics, glk_color, xpos,
        ypos, pixel_size * p.width, pixel_size);
}

void UpdateColorCycling(void)
{
    for (int i = 0; i < NumAnimCols; i++) {
        if ((ColorCycle + AnimColors[i].StartOffset) % AnimColors[i].Rate == 0) {
            glui32 color = AnimColors[i].Colors[AnimColors[i].CurCol];

            AnimColors[i].CurCol++;
            if (AnimColors[i].CurCol >= AnimColors[i].NumCol) {
                AnimColors[i].CurCol = 0;
                if (CurrentGame == SPIDERMAN && STWebAnimation) {
                    STWebAnimationFinished = 1;
                }
            }

            for (int j = 0; j < AnimColors[i].NumPix; j++) {
                ColorCyclingPixel(AnimColors[i].Pixels[j], color);
            }
        }
    }
    ColorCycle++;
}
