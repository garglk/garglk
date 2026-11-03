//
//  title_image.c
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-10-01.
//

#include <stdlib.h>
#include <string.h>

#include "apple2draw.h"
#include "decompressz80.h"
#include "glk.h"
#include "saga.h"
#include "sagagraphics.h"
#include "vector_common.h"
#include "zx_title.h"
#include "scott.h"
#include "scott_meta.h"
#include "scott_display.h"

#ifdef SPATTERLIGHT
#include "glkimp.h"
#endif

#include "title_image.h"

static void ResizeTitleImage(void)
{
    glui32 graphheight, optimal_height;
#ifdef SPATTERLIGHT
    glk_window_set_background_color(Graphics, gbgcol);
    glk_window_clear(Graphics);
#endif
    FitPictureToGraphicsWindow(&graphheight, &optimal_height);
    y_offset = ((int)graphheight - (int)optimal_height) / 3;
}

/* Replace the whole window tree with a single full-screen graphics window,
   plus a thin text buffer below it if the Glk library can't take key input
   in a graphics window. If background_color is non-NULL, it receives the
   background colour of the closed main window (left untouched if there was
   none). */
static void OpenTitleWindows(glui32 *background_color)
{
    Top = FindGlkWindowWithRock(GLK_STATUS_ROCK);
    if (Top) {
        glk_window_close(Top, NULL);
        Top = NULL;
    }

    Bottom = FindGlkWindowWithRock(GLK_BUFFER_ROCK);
    if (Bottom) {
        if (background_color)
            glk_style_measure(Bottom, style_Normal, stylehint_BackColor,
                background_color);
        glk_window_close(Bottom, NULL);
        Bottom = NULL;
    }

    Graphics = glk_window_open(0, 0, 0, wintype_Graphics, GLK_GRAPHICS_ROCK);

    if (glk_gestalt_ext(gestalt_GraphicsCharInput, 0, NULL, 0) == 0)
        Bottom = glk_window_open(Graphics, winmethod_Below | winmethod_Fixed,
            2, wintype_TextBuffer, GLK_BUFFER_ROCK);
}

/* Tear down the title windows and rebuild the normal main and status
   windows. */
static void CloseTitleWindows(void)
{
    glk_window_close(Graphics, NULL);
    Graphics = NULL;
    Bottom = FindGlkWindowWithRock(GLK_BUFFER_ROCK);
    if (Bottom != NULL)
        glk_window_close(Bottom, NULL);
    Bottom = glk_window_open(0, 0, 0, wintype_TextBuffer, GLK_BUFFER_ROCK);
    if (Bottom == NULL)
        glk_exit();
    glk_set_window(Bottom);
    OpenTopWindow();
}

/* Draw the US title picture (room image 99) fitted to the window */
static void DrawUSTitle(void)
{
    ResizeTitleImage();
    glk_window_clear(Graphics);
    DrawUSRoom(99);
    if (USImages->systype == SYS_APPLE2_LINES || USImages->systype == SYS_ATARI8_LINES) {
        DrawUSRoomObject(255);
    }
    DrawImageOrVector();
}

/* The window that takes the key press on a title screen. OpenTitleWindows()
   only opens Bottom when the graphics window can't take key input, so
   Bottom comes first. */
static winid_t TitleKeyWindow(void)
{
    return Bottom != NULL ? Bottom : Graphics;
}

static void WaitForKeyOnTitleScreen(void) {

    winid_t keywin = TitleKeyWindow();
    if (keywin == NULL)
        return;
    glk_request_char_event(keywin);

    event_t ev;
    do {
        glk_select(&ev);
        if (ev.type == evtype_Arrange) {
            if (!GraphicsEnabled())
                break;
            int stored_slowdraw = gli_slowdraw;
            gli_slowdraw = 0;
            DrawUSTitle();
            gli_slowdraw = stored_slowdraw;
        } else if (ev.type == evtype_Timer) {
            if (DrawingVector()) {
                DrawSomeVectorPixels((VectorState == NO_VECTOR_IMAGE));
            }
        }
    } while (ev.type != evtype_CharInput);
}

void DrawZXTitleImage(void)
{
    if (!GraphicsEnabled())
        return;
    if (!ZXLoadingScreen)
        return;

    /* Force the ZX palette, whatever the game's own platform palette is. */
    palette_type storedpal = palchosen;
    palchosen = ZXOPT;
    DefinePalette();

    OpenTitleWindows(NULL);

    if (Graphics) {
#ifdef SPATTERLIGHT
        glk_window_set_background_color(Graphics, gbgcol);
#endif
        glk_window_clear(Graphics);

        /* Reveal the picture slowly when the slow-draw setting is on,
           otherwise paint it at once. The reveal is linear, which is the
           order a real Spectrum loaded it from tape: Scott Adams Spectrum
           games use ordinary loaders. The picture sits a third of the way
           down the window. */
        ZXTitle title = { Graphics, pal, 3, 0, 0, 0 };
        ZXTitleShow(&title, ZXLoadingScreen, TitleKeyWindow(), gli_slowdraw, NULL, 0);
    }

    CloseTitleWindows();

    /* Restore the game's own palette. */
    palchosen = storedpal;
    DefinePalette();

    free(ZXLoadingScreen);
    ZXLoadingScreen = NULL;
}

#define RTPI_TITLE_LINES 24
#define RTPI_TITLE_COLS 40
#define BLANK_LINE_MARK 99

/* Split the title text into RTPI_TITLE_LINES newline-terminated lines,
   leading spaces stripped. Returns 0 (and leaves nothing allocated) if
   the text has fewer lines than that. */
static int GetRTPILines(const char *text, char *lines[RTPI_TITLE_LINES]) {
    int lineidx = 0;
    int pos = 0;
    while (lineidx < RTPI_TITLE_LINES && text[pos] != '\0') {
        // Skip leading spaces
        while (text[pos] == ' ') {
            pos++;
        }
        int start = pos;
        while (text[pos] != '\n' && text[pos] != '\r' && text[pos] != '\0') {
            pos++;
        }
        int linelen = pos - start;
        if (text[pos] != '\0')
            pos++;
        lines[lineidx] = MemAlloc(linelen + 2);
        memcpy(lines[lineidx], text + start, linelen);
        lines[lineidx][linelen] = '\n';
        lines[lineidx][linelen + 1] = '\0';
        lineidx++;
    }

    if (lineidx < RTPI_TITLE_LINES) {
        for (int j = 0; j < lineidx; j++) {
            free(lines[j]);
        }
        return 0;
    }
    return 1;
}

/*

0. (HIT ANY KEY)
1. Texas Instruments and
2. Adventure International
3.
4. Proudly present
5. Scott Adams Graphic Adventure
6.
7.
8. “RETURN TO PIRATE’S ISLE”
9.
10.
11. Copyright 1983
12. by Scott Adams
13. Written by Scott Adams.
14.
15. With special assistance from: Eric White, Scott Smith, and Linda Paladino.
16.
17. With love to my Mom who made it all possible!
18. The next picture you see is correct!
19.
20. Do not try to adjust your TV. Just use your Adventuring skills!
21.
22.
23. GOOD LUCK !

*/

static void InitRTPITitle(void) {
    if (!title_screen)
        return;
    Graphics = FindGlkWindowWithRock(GLK_GRAPHICS_ROCK);
    if (!Graphics)
        return;
    Bottom = FindGlkWindowWithRock(GLK_BUFFER_ROCK);
    if (Bottom)
        glk_window_close(Bottom, NULL);
    glk_stylehint_set(wintype_TextBuffer, style_User2, stylehint_Justification, stylehint_just_Centered);
    Bottom = glk_window_open(Graphics, winmethod_Below | winmethod_Proportional, 50, wintype_TextBuffer, GLK_BUFFER_ROCK);
}

static void RTPITitle(void) {
    /* The order the title lines are shown in; BLANK_LINE_MARK is a
       "wait for a key, then clear the window" page break. */
    static const uint8_t order[RTPI_TITLE_LINES] =
    { 1, 2, 4, 3, 5, 8, 9, 11, 12, 14, 0, BLANK_LINE_MARK, 13, 15, 17, 19, 0,
      BLANK_LINE_MARK, 18, 20, 21, 23, 22, 0 };

    glk_stream_set_current(glk_window_get_stream(Bottom));
    char *lines[RTPI_TITLE_LINES];
    int ok = GetRTPILines(title_screen, lines);
    free(title_screen);
    title_screen = NULL;
    if (!ok)
        return;
    glk_set_style(style_User2);
    for (int i = 0; i < RTPI_TITLE_LINES; i++) {
        if (order[i] == BLANK_LINE_MARK) {
            WaitForKeyOnTitleScreen();
            glk_window_clear(Bottom);
        } else {
            Output(lines[order[i]]);
        }
    }
    glk_set_style(style_Normal);
    for (int i = 0; i < RTPI_TITLE_LINES; i++)
        free(lines[i]);
}

void DrawTitleImageScott(void)
{
    int storedwidth = ImageWidth;
    int storedheight = ImageHeight;
    if (!GraphicsEnabled())
        return;
    const glui32 no_color = (glui32)-1;
    glui32 background_color = no_color;
    OpenTitleWindows(&background_color);

    InitRTPITitle();

    if (background_color != no_color) {
        glk_window_set_background_color(Graphics, background_color);
        glk_window_clear(Graphics);
    }

    ResizeTitleImage();

    if (DrawUSRoom(99)) {
        DrawUSTitle();

        if (CurrentGame == RETURN_TO_PIRATES_ISLE) {
            RTPITitle();
        }

        WaitForKeyOnTitleScreen();
    }

    CloseTitleWindows();
    OpenGraphicsWindow();
    ResizeTitleImage();
    ImageWidth = storedwidth;
    ImageHeight = storedheight;
    y_offset = 0;
    CloseGraphicsWindow();
}

void PrintTitleScreenBuffer(void)
{
    glk_stream_set_current(glk_window_get_stream(Bottom));
    glk_set_style(style_User1);
    if (Graphics)
        glk_window_clear(Graphics);
    Output(title_screen);
    free(title_screen);
    title_screen = NULL;
    glk_set_style(style_Normal);
    HitEnter();
    if (Graphics)
        glk_window_clear(Graphics);
}

void PrintTitleScreenGrid(void)
{
    int title_length = strlen(title_screen);
    glui32 rows = 0;

    // Count rows
    for (int i = 0; i < title_length; i++)
        if (title_screen[i] == '\n')
            rows++;

    winid_t titlewin = glk_window_open(Bottom, winmethod_Above | winmethod_Fixed, rows + 2,
        wintype_TextGrid, 0);
    glui32 width, height;
    glk_window_get_size(titlewin, &width, &height);

    // If the screen size is too small to fit the entire text,
    // just flush the text to the buffer window and be done with it.
    if (width < RTPI_TITLE_COLS || height < rows + 2) {
        glk_window_close(titlewin, NULL);
        PrintTitleScreenBuffer();
        return;
    }
    int offset = (width - RTPI_TITLE_COLS) / 2;
    int pos = 0;
    for (glui32 i = 1; i <= rows; i++) {
        glk_window_move_cursor(titlewin, offset, i);
        while (title_screen[pos] != '\n' && pos < title_length)
            Display(titlewin, "%c", title_screen[pos++]);
        pos++;
    }
    free(title_screen);
    title_screen = NULL;
    HitEnter();
    glk_window_close(titlewin, NULL);
}
