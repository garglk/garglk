//
//  pcdraw.c
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Routines to draw MS-DOS bitmap graphics
//  Based on code by David Lodge 29/04/2005
//
//  Original code at https://github.com/tautology0/textadventuregraphics
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scott.h"
#include "sagagraphics.h"

int x = 0, y = 0, at_last_line = 0;

int xlen = 280, ylen = 158;
int xoff = 0, yoff = 0;
int size;
int ycount = 0;
int skipy = 1;

extern char *DirPath;

uint8_t *FindImageFile(const char *shortname, size_t *datasize) {
    *datasize = 0;
    uint8_t *data = NULL;
    size_t pathlen = strlen(DirPath) + strlen(shortname) + 5;
    char *filename = MemAlloc(pathlen);
    int n = snprintf(filename, pathlen, "%s%s.PAK", DirPath, shortname);
    if (n > 0) {
        FILE *infile=fopen(filename,"rb");
        if (infile) {
            fseek(infile, 0, SEEK_END);
            size_t length = ftell(infile);
            if (length > 0) {
                debug_print("Found and read image file %s\n", filename);
                fseek(infile, 0, SEEK_SET);
                data = MemAlloc(length);
                *datasize = fread(data, 1, length, infile);
            }
            fclose(infile);
        } else {
            fprintf(stderr, "Could not find or read image file %s\n", filename);
        }
    }
    free(filename);
    return data;
}

static void DrawDOSPixels(int pattern)
{
    int pix1,pix2,pix3,pix4;
    // Now get colors
    pix1=(pattern & 0xc0)>>6;
    pix2=(pattern & 0x30)>>4;
    pix3=(pattern & 0x0c)>>2;
    pix4=(pattern & 0x03);

    if (!skipy) {
        PutDoublePixel(x,y, pix1); x += 2;
        PutDoublePixel(x,y, pix2); x += 2;
        PutDoublePixel(x,y, pix3); x += 2;
        PutDoublePixel(x,y, pix4); x += 2;
    } else {
        PutPixel(x,y, pix1); x++;
        PutPixel(x,y, pix2); x++;
        PutPixel(x,y, pix3); x++;
        PutPixel(x,y, pix4); x++;
    }

    if (x>=xlen+xoff)
    {
        y+=2;
        x=xoff;
        ycount++;
    }
    if (ycount>ylen) {
        y=yoff + 1;
        at_last_line++;
        ycount=0;
    }
}

int DrawDOSImage(USImage *image)
{
    if (image->imagedata == NULL) {
        fprintf(stderr, "DrawDOSImage: ptr == NULL\n");
        return 0;
    }

    debug_print("DrawDOSImage: usage:%d index:%d\n", image->usage, image->index);


    x=0;
    y=0;
    at_last_line = 0;

    xlen=0;
    ylen=0;
    xoff=0; yoff=0;
    ycount=0;
    skipy=1;

    int work;
    int c;
    int i;
    int rawoffset;
    RGB black =   { 0,0,0 };
    RGB magenta = { 255,0,255 };
    RGB cyan =    { 0,255,255 };
    RGB white =   { 255,255,255 };

    /* set up the palette */
    SetColor(0,&black);
    SetColor(1,&cyan);
    SetColor(2,&magenta);
    SetColor(3,&white);

    uint8_t *ptr = image->imagedata;
    uint8_t *origptr = ptr;

    // Get the size of the graphics chunck
    ptr = origptr + 0x05;
    work = *ptr++;
    size = work + (*ptr * 256);

    // Get whether it is lined
    ptr = origptr + 0x0d;
    work = *ptr++;
    if (work == 0xff) skipy=0;

    // Get the offset
    ptr = origptr + 0x0f;
    work = *ptr++;
    rawoffset = work + (*ptr * 256);
    xoff=((rawoffset % 80)*4)-24;
    yoff=rawoffset / 40;
    yoff -= (yoff & 1);
    x=xoff;
    y=yoff;

    // Get the y length
    ptr = origptr + 0x11;
    work = *ptr++;
    ylen = work + (*ptr * 256);
    ylen -= rawoffset;
    ylen /= 80;

    // Get the x length
    ptr = origptr + 0x13;
    xlen = *ptr * 4;

    ptr = origptr + 0x17;
    while (ptr - origptr < size)
    {
        // First get count
        c = *ptr++;

        if ((c & 0x80) == 0x80)
        { // is a counter
            work = *ptr++;
            c &= 0x7f;
            for (i=0;i<c+1;i++)
            {
                DrawDOSPixels(work);
            }
        }
        else
        {
            // Don't count on the next j characters
            for (i=0;i<c+1;i++)
            {
                work = *ptr++;
                DrawDOSPixels(work);
            }
        }
        if (at_last_line > 1) break;
    }
    return 1;
}
