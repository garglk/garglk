//
//  titleimage.c
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-10-01.
//

#include <stdlib.h>
#include <string.h>

#include "apple2draw.h"
#include "glk.h"
#include "saga.h"
#include "sagagraphics.h"
#include "scott.h"

#ifdef SPATTERLIGHT
#include "glkimp.h"
#endif

#include "titleimage.h"

glui32 OptimalPictureSize(glui32 *width, glui32 *height);

void ResizeTitleImage(void)
{
    glui32 graphwidth, graphheight, optimal_width, optimal_height;
#ifdef SPATTERLIGHT
    glk_window_set_background_color(Graphics, gbgcol);
    glk_window_clear(Graphics);
#endif
    glk_window_get_size(Graphics, &graphwidth, &graphheight);
    pixel_size = OptimalPictureSize(&optimal_width, &optimal_height);
    x_offset = ((int)graphwidth - (int)optimal_width) / 2;
    right_margin = optimal_width + x_offset;
    y_offset = ((int)graphheight - (int)optimal_height) / 3;
}

void DrawTitleImage(void)
{
    int storedwidth = ImageWidth;
    int storedheight = ImageHeight;
#ifdef SPATTERLIGHT
    if (!gli_enable_graphics)
        return;
#endif
    Top = FindGlkWindowWithRock(GLK_STATUS_ROCK);
    if (Top) {
        glk_window_close(Top, NULL);
        Top = NULL;
    }

    glui32 background_color = -1;

    Bottom = FindGlkWindowWithRock(GLK_BUFFER_ROCK);
    if (Bottom) {
        glk_style_measure(Bottom, style_Normal, stylehint_BackColor,
            &background_color);
        glk_window_close(Bottom, NULL);
    }

    Graphics = glk_window_open(0, 0, 0, wintype_Graphics, GLK_GRAPHICS_ROCK);

    if (glk_gestalt_ext(gestalt_GraphicsCharInput, 0, NULL, 0)) {
        glk_request_char_event(Graphics);
    } else {
        Bottom = glk_window_open(Graphics, winmethod_Below | winmethod_Fixed,
            2, wintype_TextBuffer, GLK_BUFFER_ROCK);
        glk_request_char_event(Bottom);
    }

    if (background_color != -1) {
        glk_window_set_background_color(Graphics, background_color);
        glk_window_clear(Graphics);
    }

    ResizeTitleImage();

    if (DrawUSRoom(99)) {
        ResizeTitleImage();
        glk_window_clear(Graphics);
        DrawUSRoom(99);
        if (CurrentSys == SYS_APPLE2)
            DrawApple2ImageFromVideoMem();
        event_t ev;
        do {
            glk_select(&ev);
            if (ev.type == evtype_Arrange) {
#ifdef SPATTERLIGHT
                if (!gli_enable_graphics)
                    break;
#endif
                ResizeTitleImage();
                glk_window_clear(Graphics);
                DrawUSRoom(99);
                if (CurrentSys == SYS_APPLE2)
                    DrawApple2ImageFromVideoMem();
            }
        } while (ev.type != evtype_CharInput);
    }

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
    OpenGraphicsWindow();
    ResizeTitleImage();
    ImageWidth = storedwidth;
    ImageHeight = storedheight;
    CloseGraphicsWindow();
}

void PrintTitleScreenBuffer(void)
{
    glk_stream_set_current(glk_window_get_stream(Bottom));
    glk_set_style(style_User1);
    glk_window_clear(Graphics);
    Output(title_screen);
    free((void *)title_screen);
    glk_set_style(style_Normal);
    HitEnter();
    glk_window_clear(Graphics);
}

void PrintTitleScreenGrid(void)
{
    int title_length = strlen(title_screen);
    int rows = 0;
    for (int i = 0; i < title_length; i++)
        if (title_screen[i] == '\n')
            rows++;
    winid_t titlewin = glk_window_open(Bottom, winmethod_Above | winmethod_Fixed, rows + 2,
        wintype_TextGrid, 0);
    glui32 width, height;
    glk_window_get_size(titlewin, &width, &height);
    if (width < 40 || height < rows + 2) {
        glk_window_close(titlewin, NULL);
        PrintTitleScreenBuffer();
        return;
    }
    int offset = (width - 40) / 2;
    int pos = 0;
    char row[40];
    row[39] = 0;
    for (int i = 1; i <= rows; i++) {
        glk_window_move_cursor(titlewin, offset, i);
        while (title_screen[pos] != '\n' && pos < title_length)
            Display(titlewin, "%c", title_screen[pos++]);
        pos++;
    }
    free((void *)title_screen);
    HitEnter();
    glk_window_close(titlewin, NULL);
}
