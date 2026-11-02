//
//  sagagraphics.c
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-09-07.
//

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "apple2draw.h"
#include "sagadraw_host.h"
#include "apple2_vector_draw.h"
#include "atari_8bit_vector_draw.h"
#include "vector_common.h"
#include "common_file_utils.h"
#include "rtpi_graphics.h"
#include "glk.h"
#include "scott.h"

#include "sagagraphics.h"

int pixel_size;
int x_offset = 0;
/* The y_offset global is only used by the title image.
 We always position the in-game graphics
 relative to the graphics window top.
 Images have an internal "y origin" value
 that determines their vertical position. */
int y_offset = 0;
/* The first picture column to draw */
int left_margin = 0;

USImage *USImages = NULL;

int HasGraphics(void)
{
    return (USImages != NULL);
}

void DrawImageOrVector(void) {
    if (!Graphics)
        return;
    if (USImages && (USImages->systype == SYS_APPLE2_LINES || USImages->systype == SYS_ATARI8_LINES)) {
        // DrawSomeVectorPixels() picks the Apple II or Atari 8-bit
        // renderer from CurrentSys, which matches the image system type
        if (gli_slowdraw)
            glk_request_timer_events(TimerDelay());
        else
            DrawSomeVectorPixels(1);
    } else if (CurrentSys == SYS_APPLE2 && USImages) {
        DrawApple2ImageFromVideoMem();
    } else if (CurrentGame == RETURN_TO_PIRATES_ISLE) {
        DrawRTPIFromMem();
    }
}

USImage *NewImage(void)
{
    USImage *new = MemAlloc(sizeof(USImage));
    new->index = -1;
    new->datasize = 0;
    new->imagedata = NULL;
    new->systype = 0;
    new->usage = 0;
    new->cropleft = 0;
    new->cropright = 0;
    new->previous = NULL;
    new->next = NULL;
    return new;
}

void SetColor(int32_t index, glui32 color)
{
    pal[index] = color;
}

/* A bitmap picture is decoded a pixel at a time in runs of one colour:
   left to right along the rows on MS-DOS, down 8-pixel columns on the
   Commodore 64 and Atari. Each fill_rect is a round trip to the window
   server, so between StartPixelRuns() and FlushPixelRuns() the pixels are
   collected here: those of one colour that follow each other on a row into
   a run, and runs that lie exactly under each other into a block, which is
   sent as a single rectangle. */
typedef struct {
    glui32 color;
    glsi32 x, y;
    glui32 width, height; /* width 0: empty */
} PixelRect;

static PixelRect pending_run;   /* The row being collected */
static PixelRect pending_block; /* The finished runs before it */

static int collecting_runs = 0;

static void SendPendingBlock(void)
{
    if (pending_block.width == 0)
        return;
    if (Graphics)
        glk_window_fill_rect(Graphics, pending_block.color, pending_block.x,
            pending_block.y, pending_block.width, pending_block.height);
    pending_block.width = 0;
}

/* The run is finished: add it to the block if it continues it downwards,
   otherwise the block is finished too and the run starts a new one. */
static void CloseRun(void)
{
    if (pending_run.width == 0)
        return;
    if (pending_block.width != 0 && pending_run.color == pending_block.color
        && pending_run.x == pending_block.x && pending_run.width == pending_block.width
        && pending_run.y == pending_block.y + (glsi32)pending_block.height) {
        pending_block.height += pending_run.height;
    } else {
        SendPendingBlock();
        pending_block = pending_run;
    }
    pending_run.width = 0;
}

static void FillRun(glui32 color, glsi32 x, glsi32 y, glui32 width, glui32 height)
{
    if (width == 0 || height == 0)
        return;

    glsi32 right = x + (glsi32)width;
    glsi32 pending_right = pending_run.x + (glsi32)pending_run.width;

    /* A rectangle of the same colour on the same row that touches or
       overlaps the collected run covers the same pixels when joined to
       it. Anything else is drawn after the run, keeping the order. */
    if (pending_run.width != 0 && color == pending_run.color
        && y == pending_run.y && height == pending_run.height
        && x <= pending_right && right >= pending_run.x) {
        if (x < pending_run.x)
            pending_run.x = x;
        if (right > pending_right)
            pending_right = right;
        pending_run.width = (glui32)(pending_right - pending_run.x);
        return;
    }

    CloseRun();
    pending_run.color = color;
    pending_run.x = x;
    pending_run.y = y;
    pending_run.width = width;
    pending_run.height = height;
}

/* Start collecting the pixels of a picture into runs. Only for a picture
   that is drawn in one go: nothing else may draw to the graphics window,
   and no event may be waited for, before FlushPixelRuns(). */
void StartPixelRuns(void)
{
    collecting_runs = 1;
}

/* Send what is left of the picture and go back to drawing every pixel
   as it comes. */
void FlushPixelRuns(void)
{
    CloseRun();
    SendPendingBlock();
    collecting_runs = 0;
}

void PutPixelWithWidth(glsi32 xpos, glsi32 ypos, int32_t color, int pixel_width)
{
    if (!Graphics || xpos < 0 || xpos < left_margin) {
        return;
    }

    glsi32 x = xpos * pixel_size + x_offset;
    glsi32 y = ypos * pixel_size + y_offset;

    if (collecting_runs)
        FillRun(pal[color], x, y, pixel_size * pixel_width, pixel_size);
    else
        glk_window_fill_rect(Graphics, pal[color], x, y, pixel_size * pixel_width, pixel_size);
}

void PutPixel(glsi32 xpos, glsi32 ypos, int32_t color)
{
    PutPixelWithWidth(xpos, ypos, color, 1);
}

void PutDoublePixel(glsi32 xpos, glsi32 ypos, int32_t color)
{
    PutPixelWithWidth(xpos, ypos, color, 2);
}

void RectFill(int32_t x, int32_t y, int32_t width, int32_t height,
              int32_t color)
{
    if (!Graphics)
        return;

    glk_window_fill_rect(Graphics, pal[color], x * pixel_size + x_offset,
                         y * pixel_size + y_offset, width * pixel_size,
                         height * pixel_size);
}

int IsSagaImage(const char *name)
{
    if (name == NULL)
        return 0;
    size_t len = strlen(name);
    if (len < 4)
        return 0;
    char c = name[0];
    if (c == 'R' || c == 'B' || c == 'S') {
        for (int i = 1; i < 4; i++)
            if (!isdigit(name[i]))
                return 0;
        return 1;
    }
    return 0;
}

uint8_t *FindImageFile(const char *shortname, size_t *datasize)
{
    *datasize = 0;
    uint8_t *data = NULL;
    size_t pathlen = strlen(DirPath) + strlen(shortname) + 5;
    char *filename = MemAlloc(pathlen);
    int n = snprintf(filename, pathlen, "%s%s.PAK", DirPath, shortname);
    if (n > 0) {
        data = ReadFileIfExists(filename, datasize);
        if (data == NULL) {
            fprintf(stderr, "Could not find or read image file %s\n", filename);
        }
    }
    free(filename);
    return data;
}
