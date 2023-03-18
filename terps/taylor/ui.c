//
//  restorestate.c
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "glk.h"
#ifdef SPATTERLIGHT
#include "glkimp.h"
#endif
#include "glkstart.h"

#include "animations.h"
#include "layouttext.h"
#include "sagadraw.h"
#include "taylor.h"
#include "utility.h"

#define GLK_BUFFER_ROCK 1
#define GLK_STATUS_ROCK 1010
#define GLK_GRAPHICS_ROCK 1020

winid_t Bottom, Top, Graphics;
winid_t CurrentWindow;
static int OutC;
static char OutWord[128];

glui32 TopWidth; /* Terminal width */
glui32 TopHeight = 1; /* Height of top window */

int Options; /* Option flags */
int LineEvent = 0;

winid_t FindGlkWindowWithRock(glui32 rock)
{
    winid_t win;
    glui32 rockptr;
    for (win = glk_window_iterate(NULL, &rockptr); win;
         win = glk_window_iterate(win, &rockptr)) {
        if (rockptr == rock)
            return win;
    }
    return 0;
}

void Display(winid_t w, const char *fmt, ...)
{
    va_list ap;
    char msg[2048];

    int size = sizeof msg;

    va_start(ap, fmt);
    vsnprintf(msg, size, fmt, ap);
    va_end(ap);

    glk_put_string_stream(glk_window_get_stream(w), msg);
    if (Transcript && w == Bottom)
        glk_put_string_stream(Transcript, msg);
}

void HitEnter(void)
{
    glk_request_char_event(Bottom);

    event_t ev;
    int result = 0;
    do {
        glk_select(&ev);
        if (ev.type == evtype_CharInput) {
            if (ev.val1 == keycode_Return) {
                result = 1;
            } else {
                glk_request_char_event(Bottom);
            }
        }
    } while (result == 0);

    return;
}

static void WordFlush(winid_t win);

static int JustWroteNewline = 0;

void PrintCharacter(unsigned char c)
{
    if (OutC == 0 && c == '\0')
        return;

    if (c == '.' || c == '!') {
        if (JustWrotePeriod)
            return;
        JustWrotePeriod = 1;
    } else if (JustWrotePeriod && !isspace(c)) {
        JustWrotePeriod = 0;
    }

    if (CurrentWindow == Bottom) {
        if (isspace(c)) {
            WordFlush(Bottom);
            if ((c == 10 || c == 13) && !JustWroteNewline) {
                Display(Bottom, "\n");
                JustWroteNewline = 1;
            } else if (!JustWroteNewline) {
                Display(Bottom, " ");
            }
            return;
        }
        JustWroteNewline = 0;
        OutWord[OutC] = c;
        OutC++;
        if (OutC > 127)
            WordFlush(Bottom);
        return;
    } else {
        if (isspace(c)) {
            WordFlush(Top);
            WriteToRoomDescriptionStream(" ");
            if (c == '\n') {
                WriteToRoomDescriptionStream("\n");
            }
            return;
        }
        OutWord[OutC] = c;
        OutC++;
        if (OutC == TopWidth)
            WordFlush(Top);
    }
    return;
}

unsigned char WaitCharacter(void)
{
    WordFlush(Bottom);
    glk_request_char_event(Bottom);

    event_t ev;
    do {
        glk_select(&ev);
        Updates(ev);
    } while (ev.type != evtype_CharInput);
    return ev.val1;
}

static void WordFlush(winid_t win)
{
    int i;
    for (i = 0; i < OutC; i++) {
        if (win == Top)
            WriteToRoomDescriptionStream("%c", OutWord[i]);
        else
            Display(win, "%c", OutWord[i]);
    }
    OutC = 0;
}

extern strid_t room_description_stream;
extern char *roomdescbuf;

void TopWindow(void)
{
    WordFlush(Bottom);
    if (roomdescbuf != NULL) {
        fprintf(stderr, "roomdescbuf was not Null, so freeing it\n");
        free(roomdescbuf);
    }
    roomdescbuf = MemAlloc(1000);
    roomdescbuf = memset(roomdescbuf, 0, 1000);
    room_description_stream = glk_stream_open_memory(roomdescbuf, 1000, filemode_Write, 0);

    CurrentWindow = Top;
    glk_window_clear(Top);
}

static void PrintWindowDelimiter(void)
{
    glk_window_get_size(Top, &TopWidth, &TopHeight);
    glk_window_move_cursor(Top, 0, TopHeight - 1);
    glk_stream_set_current(glk_window_get_stream(Top));
    for (int i = 0; i < TopWidth; i++)
        glk_put_char(DelimiterChar);
}

static void FlushRoomDescription(void)
{
    if (!room_description_stream)
        return;

    glk_stream_close(room_description_stream, 0);

    if (Transcript) {
        if (LastChar != '\n')
            glk_put_string_stream(Transcript, "\n");
        glk_put_string_stream(Transcript, roomdescbuf);
    }

    int print_delimiter = 1;

    glk_window_clear(Top);
    glk_window_get_size(Top, &TopWidth, &TopHeight);
    int rows, length;
    char *text_with_breaks = LineBreakText(roomdescbuf, TopWidth, &rows, &length);

    glui32 bottomheight;
    glk_window_get_size(Bottom, NULL, &bottomheight);
    winid_t o2 = glk_window_get_parent(Top);
    if (!(bottomheight < 3 && TopHeight < rows)) {
        glk_window_get_size(Top, &TopWidth, &TopHeight);
        glk_window_set_arrangement(o2, winmethod_Above | winmethod_Fixed, rows,
            Top);
    } else {
        print_delimiter = 0;
    }

    int line = 0;
    int index = 0;
    int i;
    int empty_lines = 0;
    char string[2048];
    if (TopWidth > 2047)
        TopWidth = 2047;
    for (line = 0; line < rows && index < length; line++) {
        for (i = 0; i < TopWidth; i++) {
            string[i] = text_with_breaks[index++];
            if (string[i] == 10 || string[i] == 13 || index >= length)
                break;
        }
        if (i < TopWidth + 1) {
            string[i++] = '\n';
            if (i < 2)
                empty_lines++;
            else
                empty_lines = 0;
        }
        string[i] = 0;
        if (strlen(string) == 0)
            break;
        glk_window_move_cursor(Top, 0, line);
        Display(Top, "%s", string);
    }

    line -= empty_lines;

    if (line < rows - 1) {
        glk_window_get_size(Top, &TopWidth, &TopHeight);
        glk_window_set_arrangement(o2, winmethod_Above | winmethod_Fixed,
            MIN(rows - 1, TopHeight - 1), Top);
    }

    free(text_with_breaks);

    if (print_delimiter) {
        PrintWindowDelimiter();
    }

    if (roomdescbuf != NULL) {
        free(roomdescbuf);
        roomdescbuf = NULL;
    }
}

char *roomdescbuf = NULL;

extern int PendSpace;

void BottomWindow(void)
{
    WordFlush(Top);
    FlushRoomDescription();
    WordFlush(Top);
    PendSpace = 0;
    CurrentWindow = Bottom;
}

void Look(void);

void OpenGraphicsWindow(void);
void CloseGraphicsWindow(void);

void UpdateSettings(void)
{
#ifdef SPATTERLIGHT
    if (gli_sa_delays)
        Options &= ~NO_DELAYS;
    else
        Options |= NO_DELAYS;

    switch (gli_sa_inventory) {
    case 0:
        Options &= ~(FORCE_INVENTORY | FORCE_INVENTORY_OFF);
        break;
    case 1:
        Options = (Options | FORCE_INVENTORY) & ~FORCE_INVENTORY_OFF;
        break;
    case 2:
        Options = (Options | FORCE_INVENTORY_OFF) & ~FORCE_INVENTORY;
        break;
    }

    switch (gli_sa_palette) {
    case 0:
        Options &= ~(FORCE_PALETTE_ZX | FORCE_PALETTE_C64);
        break;
    case 1:
        Options = (Options | FORCE_PALETTE_ZX) & ~FORCE_PALETTE_C64;
        break;
    case 2:
        Options = (Options | FORCE_PALETTE_C64) & ~FORCE_PALETTE_ZX;
        break;
    }
#endif
    palette_type previous_pal = palchosen;
    if (Options & FORCE_PALETTE_ZX)
        palchosen = ZXOPT;
    else if (Options & FORCE_PALETTE_C64) {
        if (BaseGame == QUESTPROBE3 || BaseGame == BLIZZARD_PASS)
            palchosen = C64A;
        else
            palchosen = C64B;
    } else
        palchosen = Game->palette;
    if (palchosen != previous_pal) {
        DefinePalette();
        Resizing = 0;
    }
}

int Resizing = 0;

void Updates(event_t ev)
{
    if (ev.type == evtype_Arrange) {
        Resizing = 1;
        UpdateSettings();
        CloseGraphicsWindow();
        Look();
        Resizing = 0;
    } else if (ev.type == evtype_Timer) {
        switch (BaseGame) {
        case REBEL_PLANET:
            UpdateRebelAnimations();
            break;
        case KAYLETH:
            UpdateKaylethAnimations();
            break;
        default:
            break;
        }
    }
}

const glui32 OptimalPictureSize(glui32 *width, glui32 *height)
{
    *width = 255;
    *height = 96;
    int multiplier = 1;
    glui32 graphwidth, graphheight;
    glk_window_get_size(Graphics, &graphwidth, &graphheight);
    multiplier = graphheight / 96;
    if (255 * multiplier > graphwidth)
        multiplier = graphwidth / 255;

    if (multiplier == 0)
        multiplier = 1;

    *width = 255 * multiplier;
    *height = 96 * multiplier;

    return multiplier;
}

void CloseGraphicsWindow(void)
{
    if (Graphics == NULL)
        Graphics = FindGlkWindowWithRock(GLK_GRAPHICS_ROCK);
    if (Graphics) {
        glk_window_close(Graphics, NULL);
        Graphics = NULL;
        glk_window_get_size(Top, &TopWidth, &TopHeight);
    }
}

void OpenGraphicsWindow(void)
{
#ifdef SPATTERLIGHT
    if (!gli_enable_graphics)
        return;
#endif
    glui32 graphwidth, graphheight, optimal_width, optimal_height;

    if (Top == NULL)
        Top = FindGlkWindowWithRock(GLK_STATUS_ROCK);
    if (Graphics == NULL)
        Graphics = FindGlkWindowWithRock(GLK_GRAPHICS_ROCK);
    if (Graphics == NULL && Top != NULL) {
        glk_window_get_size(Top, &TopWidth, &TopHeight);
        glk_window_close(Top, NULL);
        Graphics = glk_window_open(Bottom, winmethod_Above | winmethod_Proportional,
            60, wintype_Graphics, GLK_GRAPHICS_ROCK);
        glk_window_get_size(Graphics, &graphwidth, &graphheight);
        pixel_size = OptimalPictureSize(&optimal_width, &optimal_height);
        x_offset = ((int)graphwidth - (int)optimal_width) / 2;

        if (graphheight > optimal_height) {
            winid_t parent = glk_window_get_parent(Graphics);
            glk_window_set_arrangement(parent, winmethod_Above | winmethod_Fixed,
                optimal_height, NULL);
        }

        /* Set the graphics window background to match
         * the main window background, best as we can,
         * and clear the window.
         */
        glui32 background_color;
        if (glk_style_measure(Bottom, style_Normal, stylehint_BackColor,
                &background_color)) {
            glk_window_set_background_color(Graphics, background_color);
            glk_window_clear(Graphics);
        }

        Top = glk_window_open(Bottom, winmethod_Above | winmethod_Fixed, TopHeight,
            wintype_TextGrid, GLK_STATUS_ROCK);
        glk_window_get_size(Top, &TopWidth, &TopHeight);
    } else {
        if (!Graphics)
            Graphics = glk_window_open(Bottom, winmethod_Above | winmethod_Proportional, 60,
                wintype_Graphics, GLK_GRAPHICS_ROCK);
        glk_window_get_size(Graphics, &graphwidth, &graphheight);
        pixel_size = OptimalPictureSize(&optimal_width, &optimal_height);
        x_offset = (graphwidth - optimal_width) / 2;
        winid_t parent = glk_window_get_parent(Graphics);
        glk_window_set_arrangement(parent, winmethod_Above | winmethod_Fixed,
            optimal_height, NULL);
    }
}

void OpenTopWindow(void)
{
    Top = FindGlkWindowWithRock(GLK_STATUS_ROCK);
    if (Top == NULL) {
        Top = glk_window_open(Bottom, winmethod_Above | winmethod_Fixed,
            TopHeight, wintype_TextGrid, GLK_STATUS_ROCK);
        if (Top == NULL) {
            Top = Bottom;
        } else {
            glk_window_get_size(Top, &TopWidth, NULL);
        }
    }
}

void DrawBlack(void)
{
    glk_window_fill_rect(Graphics, 0, x_offset, 0, 32 * 8 * pixel_size,
        12 * 8 * pixel_size);
}

void DrawRoomImage(void)
{
    if (MyLoc == 0 || (BaseGame == KAYLETH && MyLoc == 91) || NoGraphics) {
        return;
    }
    ClearGraphMem();
    DrawTaylor(MyLoc);
    StartAnimations();
    DrawSagaPictureFromBuffer();
}

void OpenBottomWindow(void)
{
    Bottom = glk_window_open(0, 0, 0, wintype_TextBuffer, GLK_BUFFER_ROCK);
}

void DisplayInit(void)
{
    if (!Bottom)
        OpenBottomWindow();
    OpenTopWindow();
    UpdateSettings();
}
