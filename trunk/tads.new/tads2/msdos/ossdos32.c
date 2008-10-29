#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/TADS2/msdos/ossdos32.c,v 1.2 1999/05/17 02:52:19 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1997, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  ossdos32.c - ossdos functions for win32
Function
  
Notes
  
Modified
  11/22/97 MJRoberts  - Creation
*/

#include <windows.h>
#include <wincon.h>
#include <string.h>
#include "os.h"
#include "osgen.h"

/*
 *   Console output buffer handle 
 */
HANDLE G_out_bufhdl;
HANDLE G_in_bufhdl;

/* "plain mode" flag */
extern int os_f_plain;

/* original console mode */
static DWORD S_orig_console_out_mode;
static DWORD S_orig_console_in_mode;

/* screen width */
static int S_scrwid;

/* original size of console screen buffer */
static COORD S_orig_buffer_size;

/* initialize console */
void oss_con_init(void)
{
    DWORD new_console_mode;
    OSVERSIONINFO ver_info;
    
    /* get the console input and output handles */
    G_out_bufhdl = GetStdHandle(STD_OUTPUT_HANDLE);
    G_in_bufhdl = GetStdHandle(STD_INPUT_HANDLE);

    /* get the original console mode so we can restore it when we're done */
    GetConsoleMode(G_out_bufhdl, &S_orig_console_out_mode);
    GetConsoleMode(G_in_bufhdl, &S_orig_console_in_mode);

    /* if we're in 'plain' mode, don't change the console mode */
    if (os_f_plain)
        return;

    /* start with the original console mode bits */
    new_console_mode = S_orig_console_out_mode;

    /*
     *   Turn off wrapping when writing at the end of the line - the
     *   automatic wrapping screws up our scrolling, and we don't need it
     *   anyway since we keep track of all of our cursor positioning
     *   ourselves, hence it's easier if we just turn this feature off 
     */
    new_console_mode &= ~ENABLE_WRAP_AT_EOL_OUTPUT;

    /* 
     *   If we're on a 95 derivative (95, 98, ME), also clear the "processed
     *   output" flag.  This flag is necessary on NT for proper character
     *   set handling, but is not on 95/98/ME; and if that flag isn't
     *   cleared on 95/98/ME, clearing the wrap-at-eol flag has no effect.
     *   So, we must clear this flag on 95/98/ME, but we must leave it
     *   enabled on NT.  (What a pain.)
     */
    ver_info.dwOSVersionInfoSize = sizeof(ver_info);
    GetVersionEx(&ver_info);
    if (ver_info.dwPlatformId != VER_PLATFORM_WIN32_NT)
        new_console_mode &= ~ENABLE_PROCESSED_OUTPUT;

    /* set the new console mode */
    SetConsoleMode(G_out_bufhdl, new_console_mode);
}

/* uninitialize the console */
void oss_con_uninit(void)
{
    CONSOLE_SCREEN_BUFFER_INFO info;
    extern int os_f_plain;

    /* 
     *   If we're not in 'plain' mode, clear the screen below the cursor.
     *   This will ensure that we don't leave a bunch of leftovers from
     *   banner windows below the cursor, which the command prompt would
     *   happily spill into and create a terribly confusing display.  We'll
     *   trust the osgen layer to have left the cursor at the bottom of the
     *   main text window, so this approach will preserve as much output as
     *   possible in the main text window - this is good if there was an
     *   error or farewell message just before the program existed.  
     */
    if (!os_f_plain)
    {
        /* get the current cursor position */
        GetConsoleScreenBufferInfo(G_out_bufhdl, &info);

        /* 
         *   If we're not already at the start of a line, add a newline.
         *   Otherwise, write a blank space and then locate back at the start
         *   of the line.  In either case, writing this extra character will
         *   ensure that we leave the console with the default output color
         *   set to a the main text color.  
         */
        if (info.dwCursorPosition.X != 0)
        {
            /* we're not at the start of a line - add a newline */
            ossdsp(info.dwCursorPosition.Y, info.dwCursorPosition.X,
                   ossgetcolor(OSGEN_COLOR_TEXT, OSGEN_COLOR_TEXTBG, 0, 0),
                   "\n");
        }
        else
        {
            /* 
             *   we're at the start of a line - write out a space just to set
             *   the color to something reasonable 
             */
            ossdsp(info.dwCursorPosition.Y, info.dwCursorPosition.X,
                   ossgetcolor(OSGEN_COLOR_TEXT, OSGEN_COLOR_TEXTBG, 0, 0),
                   " ");

            /* go back to the start of the line */
            ossloc(info.dwCursorPosition.Y, info.dwCursorPosition.X);
        }

        /* clear the screen from the current line to the bottom */
        GetConsoleScreenBufferInfo(G_out_bufhdl, &info);
        ossclr(info.dwCursorPosition.Y, 0,
               G_oss_screen_height - 1, G_oss_screen_width - 1,
               ossgetcolor(OSGEN_COLOR_TEXT, OSGEN_COLOR_TEXTBG, 0, 0));
    }

    /* restore the console to its original mode settings */
    SetConsoleMode(G_out_bufhdl, S_orig_console_out_mode);
    SetConsoleMode(G_in_bufhdl, S_orig_console_in_mode);

    /* get the current screen buffer size */
    if (!os_f_plain && GetConsoleScreenBufferInfo(G_out_bufhdl, &info))
    {
        /* 
         *   restore the original window buffer size, but make sure we don't
         *   set the size smaller than the current buffer size, since that
         *   will have no effect (the user could have expanded the window or
         *   the buffer size while we were running, so we need to check and
         *   adjust our original size accordingly) 
         */
        if (info.dwSize.X > S_orig_buffer_size.X)
            S_orig_buffer_size.X = info.dwSize.X;
        if (info.dwSize.Y > S_orig_buffer_size.Y)
            S_orig_buffer_size.Y = info.dwSize.Y;

        /* set the original buffer size */
        SetConsoleScreenBufferSize(G_out_bufhdl, S_orig_buffer_size);
    }
}

/* process a WINDOW_BUFFER_SIZE_EVENT on input */
void oss_win_resize_event()
{
    CONSOLE_SCREEN_BUFFER_INFO info;

    /*
     *   The window buffer size has changed.  Since we use our own
     *   scrolling, we always want to keep the console window buffer the
     *   same size as the window itself.  So, set the buffer size back to
     *   the window size, but notify the osgen layer of the change in the
     *   underlying window size, if any.  
     */
    if (!os_f_plain && GetConsoleScreenBufferInfo(G_out_bufhdl, &info))
    {
        COORD buf_size;

        /* scroll it to the top left */
        info.srWindow.Bottom -= info.srWindow.Top;
        info.srWindow.Top = 0;
        info.srWindow.Right -= info.srWindow.Left;
        info.srWindow.Left = 0;
        SetConsoleWindowInfo(G_out_bufhdl, TRUE, &info.srWindow);

        /* set the window buffer size to equal the window size */
        buf_size.X = info.srWindow.Right + 1;
        buf_size.Y = info.srWindow.Bottom + 1;
        SetConsoleScreenBufferSize(G_out_bufhdl, buf_size);

        /* calculate the new screen size for the osgen layer */
        G_oss_screen_height = buf_size.Y;
        G_oss_screen_width = buf_size.X;

        /* notify the osgen layer of the change */
        osssb_on_resize_screen();
    }
}

/* scroll up */
void ossscu(int top, int left, int bottom, int right, int blankcolor)
{
    COORD newpos;
    SMALL_RECT pos;
    SMALL_RECT clip;
    CHAR_INFO fill;

    pos.Top = clip.Top = top;
    pos.Left = clip.Left = left;
    pos.Bottom = clip.Bottom = bottom;
    pos.Right = clip.Right = right;

    newpos.X = left;
    newpos.Y = top + 1;

    fill.Char.AsciiChar = ' ';
    fill.Attributes = blankcolor;

    ScrollConsoleScreenBuffer(G_out_bufhdl, &pos, &clip, newpos, &fill);
}

/* scroll down */
void ossscr(int top, int left, int bottom, int right, int blankcolor)
{
    COORD newpos;
    SMALL_RECT pos;
    SMALL_RECT clip;
    CHAR_INFO fill;

    pos.Top = clip.Top = top;
    pos.Left = clip.Left = left;
    pos.Bottom = clip.Bottom = bottom;
    pos.Right = clip.Right = right;

    newpos.X = left;
    newpos.Y = top - 1;

    fill.Char.AsciiChar = ' ';
    fill.Attributes = blankcolor;

    ScrollConsoleScreenBuffer(G_out_bufhdl, &pos, &clip, newpos, &fill);
}

/* clear an area of the screen */
void ossclr(int top, int left, int bottom, int right, int color)
{
    int curline;
    DWORD line_len;

    /* calculate the length of each line that we'll write */
    line_len = right - left + 1;

    /* write each line */
    for (curline = top ; curline <= bottom ; ++curline)
    {
        COORD pos;
        DWORD actual;

        pos.X = left;
        pos.Y = curline;
        FillConsoleOutputAttribute(G_out_bufhdl, (WORD)color, line_len,
                                   pos, &actual);
        FillConsoleOutputCharacter(G_out_bufhdl, ' ', line_len,
                                   pos, &actual);
    }
}

/* locate the cursor */
void ossloc(int line, int column)
{
    COORD pos;
    CONSOLE_SCREEN_BUFFER_INFO info;

    /* if we're not changing the position, leave it alone */
    if (GetConsoleScreenBufferInfo(G_out_bufhdl, &info)
        && info.dwCursorPosition.X == column
        && info.dwCursorPosition.Y == line)
        return;

    /* set the new position */
    pos.X = column;
    pos.Y = line;
    SetConsoleCursorPosition(G_out_bufhdl, pos);
}

/* check for monochrome mode */
int ossmon(void)
{
    return 0;
}

/* display text */
void ossdsp(int line, int column, int color, const char *msg)
{
    COORD pos;
    DWORD nwritten;

    pos.X = column;
    pos.Y = line;
    SetConsoleTextAttribute(G_out_bufhdl, (WORD)color);
    SetConsoleCursorPosition(G_out_bufhdl, pos);
    WriteConsole(G_out_bufhdl, msg, strlen(msg), &nwritten, 0);
}

/* get the display metrics */
void ossgmx(int *max_linep, int *max_columnp)
{
    CONSOLE_SCREEN_BUFFER_INFO info;

    /* get the console handle */
    G_out_bufhdl = GetStdHandle(STD_OUTPUT_HANDLE);

    /* get the size of the screen and return it to the caller's variables */
    if (GetConsoleScreenBufferInfo(G_out_bufhdl, &info))
    {
        /* 
         *   If it's scrolled down, scroll it to the top.  If the screen
         *   buffer size is larger than the window size, reduce the buffer
         *   size to match the window size: we do all of the scrolling
         *   ourselves, so we don't want to do that on top of a separate
         *   scrolling canvas managed by the system. 
         */
        if (!os_f_plain
            && (info.srWindow.Top != 0 || info.srWindow.Left != 0
                || info.dwSize.X > info.srWindow.Right + 1
                || info.dwSize.Y > info.srWindow.Bottom + 1))
        {
            COORD buf_size;
            
            /* scroll it to the top left */
            info.srWindow.Bottom -= info.srWindow.Top;
            info.srWindow.Top = 0;
            info.srWindow.Right -= info.srWindow.Left;
            info.srWindow.Left = 0;
            SetConsoleWindowInfo(G_out_bufhdl, TRUE, &info.srWindow);

            /* remember the original buffer size */
            S_orig_buffer_size = info.dwSize;

            /* set the window buffer size to equal the window size */
            buf_size.X = info.srWindow.Right + 1;
            buf_size.Y = info.srWindow.Bottom + 1;
            SetConsoleScreenBufferSize(G_out_bufhdl, buf_size);
        }
        
        /* got the information - use the current window size */
        *max_columnp = info.srWindow.Right - info.srWindow.Left;
        *max_linep = info.srWindow.Bottom - info.srWindow.Top;
    }
    else
    {
        /* failed to get info - use default sizes */
        *max_columnp = 79;
        *max_linep = 24;
    }

    /* note the screen width for our own use */
    S_scrwid = info.dwMaximumWindowSize.X;
}
