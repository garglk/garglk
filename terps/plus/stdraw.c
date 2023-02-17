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

static int IsSTBitSet(int bit, uint8_t byte) {
    return ((byte & (1 << bit)) != 0);
}

static uint32_t NumAnimCols, NumOldAnimCols = 0, ImgAddrOffs, NibblesWide, NxtLineStart, InitialMask, LastMask, CurAddr, LineNibblesLeft;

static uint32_t ColorCycle = 0;

static glui32 StColToGlk(uint8_t hi, uint8_t lo) {
    int blue = lo & 0xf;
    int green = (lo >> 4) & 0xf;
    int red = hi & 0xf;

    red = red * 35.7;
    green = green * 35.7;
    blue = blue * 35.7;

    return (red << 16) | (green << 8) | blue;
}

AnimationColor *AnimColors = NULL;

static void FreeAnimCols(void) {
    if (AnimColors == NULL)
        return;
    for (int i = 0; i < NumOldAnimCols; i++) {
        AnimationColor col = AnimColors[i];
        if (col.Colors != NULL)
            free(col.Colors);
        col.Colors = NULL;
        if (col.Pixels != NULL)
            free(col.Pixels);
        col.Pixels = NULL;
    }
    if (AnimColors != NULL)
        free(AnimColors);
    AnimColors = NULL;
    NumAnimCols = 0;
}

static uint8_t *SetPaletteAnimation(uint8_t *ptr) {
    if (NumAnimCols != 0) {
        if (AnimColors != NULL) {
            FreeAnimCols();
        }
        AnimColors = MemAlloc(sizeof(AnimationColor) * (NumAnimCols + NumOldAnimCols));
        for (int i = NumAnimCols; i < NumAnimCols + NumOldAnimCols; i++) {
            AnimColors[i].Colors = NULL;
            AnimColors[i].Pixels = NULL;
        }
        AnimationColor *col = AnimColors;
        for (int i = 0; i < NumAnimCols; i++, col++) {
            uint8_t val = *ptr++;
            col->ColIdx = 1 + (val >> 4);
            col->CurCol = 0;
            col->NumPix = 0;
            col->Pixels = NULL;
            col->StartOffset = *ptr++;
            uint8_t ratehi = *ptr++;
            col->Rate = *ptr++ + ratehi * 256;
            col->Rate = 1 + col->Rate;
            val = val & 0xf;
            col->NumCol = val;
            col->Colors = MemAlloc(col->NumCol * sizeof(glui32));
            for (int j = 0; j < val; j++) {
                uint8_t hi = *ptr++;
                uint8_t lo = *ptr++;
                col->Colors[j] = StColToGlk(hi, lo);
            };
        }
    }
    return ptr;
}

void PutDoublePixel(glsi32 x, glsi32 y, int32_t color);

uint8_t *PatternLookup = NULL;

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


static void DrawSTNibble(uint8_t byte, uint8_t mask, int xpos, int ypos, Pixel **pixels) {
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
                if (col == AnimColors[c].ColIdx && AnimColors[c].NumPix < 3000) {
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


static void DrawPattern(uint8_t pattern, Pixel **pixels) {
    // mask and LastMask alternate between 0x0f and 0xf0
    uint8_t mask = LastMask ^ 0xff;

    // CurAddr: screen memory where we're currently writing
    int y1 = CurAddr / 160;
    int x1 = 2 * (CurAddr % 160) + 6 * (CurAddr & 1);

    DrawSTNibble(pattern, mask, x1, y1, pixels);

    mask = mask ^ 0xff;
    LastMask = LastMask ^ 0xff;
    if (mask != 0xf) {
        CurAddr += 1 + 6 * (CurAddr & 1);
    }

    // LineBytesLeft = bytes remaining to draw this line

    LineNibblesLeft--;
    if (LineNibblesLeft != 0)
        return;

    // Reached end of line
    // NxtLine: start address of next line
    NxtLineStart += 160; // move down one line
    CurAddr = NxtLineStart;
    // InitialMask: mask at start of line
    mask = InitialMask;
    LastMask = mask ^ 0xff;
    LineNibblesLeft = NibblesWide;
}

void SetRGB(int32_t index, int red, int green, int blue);

int ColorCyclingRunning = 0;

int Intersects(Pixel pix, int xpos, int ypos, int width, int height) {
    return MAX(pix.x, xpos) <= MIN(pix.x + pix.width, xpos + width)
    && MAX(pix.y, ypos) <= MIN(pix.y, ypos + height);
}

void CopyPixel(Pixel *a, Pixel *b) {
    a->width = b->width;
    a->x = b->x;
    a->y = b->y;
}

void CopyAnimCol(AnimationColor *a, AnimationColor *b) {
    a->ColIdx = b->ColIdx;
    a->CurCol = b->CurCol;
    a->NumCol = b->NumCol;
    int datasize = b->NumCol * sizeof(glui32);
    a->Colors = MemAlloc(datasize);
    memcpy(a->Colors, b->Colors, datasize);
    a->Rate = b->Rate;
    a->NumPix = b->NumPix;
    datasize = b->NumPix * sizeof(Pixel);
    a->Pixels = MemAlloc(datasize);
    memcpy(a->Pixels, b->Pixels, datasize);
}

void AddPixels(AnimationColor *animcol, Pixel *pixels, int numpixels) {
    int pixidx = 0;
    Pixel newpix[4000];
    for (int i = 0; i < animcol->NumPix; i++) {
        CopyPixel(&newpix[pixidx++], &(animcol->Pixels[i]));
    }

    for (int i = 0; i < numpixels; i++) {
        CopyPixel(&newpix[pixidx++], &pixels[i]);
    }
    int datasize = sizeof(Pixel) * pixidx;
    free(animcol->Pixels);
    animcol->Pixels = MemAlloc(datasize);
    memcpy(animcol->Pixels, newpix, datasize);
    animcol->NumPix = pixidx;
}


int AddNonHiddenColAnim(AnimationColor *old, AnimationColor *new, int numnew, int xpos, int ypos, int width, int height) {
    int goodpixels = 0;
    Pixel pixels[3000];

    for (int i = 0; i < old->NumPix; i++) {
        if (!Intersects(old->Pixels[i], xpos, ypos, width, height)) {
            CopyPixel(&pixels[goodpixels++], &(old->Pixels[i]));
        }
    }

    if (goodpixels == 0) {
        return 0;
    }

    for (int i = 0; i < numnew; i++) {
        if (new[i].ColIdx == old->ColIdx && new[i].NumPix) {
            AddPixels(&new[i], pixels, goodpixels);
            return 0;
        }
    }

    CopyAnimCol(&new[numnew], old);
    return 1;
}

int DrawSTImageFromData(uint8_t *imgdata, size_t total_size) {

    uint8_t *ptr = &imgdata[4];

    ImgAddrOffs = *ptr++ * 256;
    ImgAddrOffs += *ptr++;
    ImgAddrOffs *= 2;

    yoff = ImgAddrOffs / 160;
    xoff = 2 * (ImgAddrOffs % 160) + 6 * (ImgAddrOffs % 2);

    NibblesWide = *ptr++;

    /* Make sure to clear the last image and any color cycling for all Claymorgue room images */
    /* The small ones have to be special-cased */
    if (NibblesWide > 70 || (CurrentGame == CLAYMORGUE && LastImgType == IMG_ROOM && (LastImgIndex == 0 || LastImgIndex == 5 || LastImgIndex == 11 || LastImgIndex == 16 || LastImgIndex == 18 || LastImgIndex == 31))) {
        glk_window_clear(Graphics);
        FreeAnimCols();
    }

    /* Draw a white background behind some Spider-Man images */
    if (CurrentGame == SPIDERMAN && LastImgType == IMG_SPECIAL && (LastImgIndex == 6 || LastImgIndex == 7 || LastImgIndex == 8 || LastImgIndex == 12 || LastImgIndex == 18)) {
        glui32 winwidth, winheight;
        glk_window_get_size(Graphics, &winwidth, &winheight);
        int width_in_pixels = NibblesWide * 4 * pixel_size;
        glk_window_fill_rect(Graphics, 0xf9f8f9, (winwidth - width_in_pixels) / 2, 0, width_in_pixels, winheight);
    }

    NumOldAnimCols = NumAnimCols;
    NumAnimCols = *ptr++;

    for (int i = 0; i <= 15; i++) {
        int blue = *(ptr + 1) & 0xf;
        int green = (*(ptr + 1) >> 4) & 0xf;
        int red = *ptr & 0xf;

        SetRGB(i, red, green, blue);

        ptr += 2;
    }

    AnimationColor *oldColAnim = NULL;

    if (CurrentGame != CLAYMORGUE && IsSet(DARKBIT)) {
        FreeAnimCols();
        NumOldAnimCols = 0;
    }

    if (AnimColors != NULL) {
        oldColAnim = AnimColors;
        AnimColors = NULL;
    }

    ptr = SetPaletteAnimation(ptr);

    if (CurrentGame != CLAYMORGUE && IsSet(DARKBIT)) {
        FreeAnimCols();
    }

    Pixel **Pixels = MemAlloc(sizeof(Pixel*) * NumAnimCols);
    for (int i = 0; i < NumAnimCols; i++)
        Pixels[i] = MemAlloc(sizeof(Pixel) * 3000);

    uint16_t imgdatasize = imgdata[2] * 256 + imgdata[3];
    uint8_t *endOfData = ptr + imgdatasize;
    if (endOfData > imgdata + total_size)
        endOfData = imgdata + total_size;

    if (PatternLookup == NULL)
        GeneratePatternLookup();

    CurAddr = ImgAddrOffs;

    // CurAddr == screen memory where we're currently writing

    InitialMask = 0xf0;
    LastMask = 0x0f;
    NxtLineStart = CurAddr;
    LineNibblesLeft = NibblesWide;

    uint8_t c;

    do {
        c = *ptr++;
        if ((c & 0x80) == 0) {
            for (int i = c; i >= 0; i--) {
                c = *ptr++;
                DrawPattern(c, Pixels);
            }
        } else {
            // Repeat next byte (c & 0x7f) times
            c = c & 0x7f;
            uint8_t val = *ptr++;
            for (int i = c; i >= 0; i--) {
                DrawPattern(val, Pixels);
            }
        }
    } while (ptr <= endOfData);

    int ImgHeight = NxtLineStart / 160 - yoff;

    for (int i = 0; i < NumAnimCols; i++) {
        if (AnimColors[i].NumPix) {
            AnimColors[i].Pixels = MemAlloc(AnimColors[i].NumPix * sizeof(Pixel));
            memcpy(AnimColors[i].Pixels, Pixels[i], AnimColors[i].NumPix * sizeof(Pixel));
            free(Pixels[i]);
        }
    }
    free(Pixels);

    if (AnimColors == NULL && NumOldAnimCols) {
        AnimColors = MemAlloc(sizeof(AnimationColor) * NumOldAnimCols);
        for (int i = 0; i < NumOldAnimCols; i++) {
            AnimColors[i].Colors = NULL;
            AnimColors[i].Pixels = NULL;
        }

    }

    for (int i = 0; i < NumOldAnimCols; i++) {
        NumAnimCols += AddNonHiddenColAnim(&oldColAnim[i], AnimColors, NumAnimCols, xoff, yoff, NibblesWide * 4, ImgHeight);
    }

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

void ColorCyclingPixel(Pixel p, glui32 glk_color)
{
    glsi32 xpos = p.x * pixel_size;

    if (upside_down)
        xpos = ImageWidth * pixel_size - xpos - (pixel_size * (p.width - 2));
    xpos += x_offset;

    if (xpos < x_offset || xpos >= right_margin) {
        return;
    }

    int ypos = p.y * pixel_size;
    if (upside_down)
        ypos = ImageHeight * pixel_size - ypos - 3 * pixel_size;
    ypos += y_offset;

    glk_window_fill_rect(Graphics, glk_color, xpos,
                         ypos, pixel_size * p.width, pixel_size);
}

extern int STWebAnimation;
extern int STWebAnimationFinished;

void UpdateColorCycling(void) {
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
