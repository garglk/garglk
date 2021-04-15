// Copyright 2010-2021 Chris Spiegel.
//
// This file is part of Bocfel.
//
// Bocfel is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version
// 2 or 3, as published by the Free Software Foundation.
//
// Bocfel is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Bocfel.  If not, see <http://www.gnu.org/licenses/>.

#ifdef ZTERP_UNIX
#define _XOPEN_SOURCE	600
#endif

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <limits.h>

#include "osdep.h"
#include "screen.h"
#include "zterp.h"

// OS-specific functions should all be collected in this file for
// convenience.  A sort of poor-man’s “reverse” inheritance is used: for
// each function that a particular operating system provides, it should
// #define a macro of the same name.  At the end of the file, a generic
// function is provided for each function that has no associated macro
// definition.
//
// The functions required are as follows:
//
// long zterp_os_filesize(FILE *fp)
//
// Return the size of the file referred to by fp.  It is safe to assume
// that the file is opened in binary mode.  The file position indicator
// need not be maintained.  If the size of the file is larger than
// LONG_MAX, -1 should be returned.
//
// bool zterp_os_rcfile(char *s, size_t n)
//
// Different operating systems have different ideas about where
// configuration data should be stored; this function will copy a
// suitable value for the bocfel configuration file into the buffer “s”
// which is n bytes long. If a configuration file location cannot be
// determined, return false, otherwise true.
//
// bool zterp_os_autosave_name(char *name, size_t n)
//
// Store a suitable filename for autosaving into “name”, which is “n”
// bytes long. Return true if the file should be accessible, false if
// there is some problem. For example, the Unix implementation returns
// false if it can’t create all directory components in the filename.
// This should not verify that the file exists, because the first time
// an autosave is created, it necessarily won’t exist beforehand.
//
// The following functions are useful for non-Glk builds only.  They
// provide for some handling of screen functions that is normally taken
// care of by Glk.
//
// void zterp_os_get_screen_size(unsigned *w, unsigned *h)
//
// The size of the terminal, if known, is written into *w (width) and *h
// (height).  If terminal size is unavalable, nothing should be written.
//
// void zterp_os_init_term(void)
//
// If something special needs to be done to prepare the terminal for
// output, it should be done here.  This function is called once at
// program startup.
//
// bool zterp_os_have_style(int style)
//
// This should return true if the provided style (see style.h for valid
// STYLE_ values) is available.  It is safe to assume that styles will
// not be combined; e.g. this will not be called as:
// zterp_os_have_style(STYLE_BOLD | STYLE_ITALIC);
//
// bool zterp_os_have_colors(void)
//
// Returns true if the terminal supports colors.
//
// void zterp_os_set_style(int style, const struct color *fg, const struct color *bg)
//
// Set both a style and foreground/background color.  Any previous
// settings should be ignored; for example, if the last call to
// zterp_os_set_style() turned on italics and the current call sets
// bold, the result should be bold, not bold italic.
// Unlike in zterp_os_have_style(), here styles may be combined.  See
// the Unix implementation for a reference.
// The colors are pointers to “struct color”: see screen.h.
// This function will be called unconditionally, so implementations must
// ignore requests if they cannot be fulfilled.

// Like snprintf(), but returns false from the calling function if the
// output was truncated.
#define checked_snprintf(s, n, ...) do { if (snprintf((s), (n), __VA_ARGS__) >= (n)) { return false; } } while(0)

// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║ Shared functions                                                             ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#if (defined(ZTERP_WIN32) || defined(ZTERP_DOS)) && !defined(ZTERP_GLK)
static void ansi_set_style(int style, const struct color *fg, const struct color *bg)
{
    printf("\33[0m");

    if (style & STYLE_ITALIC) {
        printf("\33[4m");
    }
    if (style & STYLE_REVERSE) {
        printf("\33[7m");
    }
    if (style & STYLE_BOLD) {
        printf("\33[1m");
    }

    if (fg->mode == ColorModeANSI) {
        printf("\33[%dm", 28 + fg->value);
    }
    if (bg->mode == ColorModeANSI) {
        printf("\33[%dm", 38 + bg->value);
    }
}
#endif

// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║ Unix functions                                                               ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#ifdef ZTERP_UNIX
#include <string.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

long zterp_os_filesize(FILE *fp)
{
    struct stat buf;
    int fd = fileno(fp);

    if (fd == -1 || fstat(fd, &buf) == -1 || !S_ISREG(buf.st_mode) || buf.st_size > LONG_MAX) {
        return -1;
    }

    return buf.st_size;
}
#define zterp_os_filesize

bool zterp_os_rcfile(char *s, size_t n)
{
    const char *home;
    const char *config_home;

    home = getenv("HOME");
    if (home != NULL) {
        // This is the legacy location of the config file.
        checked_snprintf(s, n, "%s/.bocfelrc", home);
        if (access(s, R_OK) == 0) {
            return true;
        }
    }

    config_home = getenv("XDG_CONFIG_HOME");
    if (config_home != NULL) {
        checked_snprintf(s, n, "%s/bocfel/bocfelrc", config_home);
    } else if (home != NULL) {
        checked_snprintf(s, n, "%s/.config/bocfel/bocfelrc", home);
    } else {
        return false;
    }

    return true;
}
#define zterp_os_rcfile

bool zterp_os_autosave_name(char *name, size_t n)
{
    const char *base_name;
    const char *data_home;

    base_name = strrchr(game_file, '/');
    if (base_name != NULL) {
        base_name++;
    } else {
        base_name = game_file;
    }

    data_home = getenv("XDG_DATA_HOME");
    if (data_home != NULL) {
        checked_snprintf(name, n, "%s/bocfel/autosave/%s-%s", data_home, base_name, get_story_id());
    } else {
        char *home = getenv("HOME");
        if (home == NULL) {
            return false;
        }
        checked_snprintf(name, n, "%s/.local/share/bocfel/autosave/%s-%s", home, base_name, get_story_id());
    }

    for (char *p = name; *p != 0; p++) {
        if (*p == '/' && p != name) {
            struct stat st;
            *p = 0;
            mkdir(name, 0755);
            if (stat(name, &st) == -1 || !S_ISDIR(st.st_mode)) {
                return false;
            }
            *p = '/';
        }
    }

    return true;
}
#define zterp_os_autosave_name

#ifndef ZTERP_GLK
#include <sys/ioctl.h>
#include <curses.h>
#include <term.h>
#ifdef TIOCGWINSZ
void zterp_os_get_screen_size(unsigned *w, unsigned *h)
{
    struct winsize winsize;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &winsize) == 0) {
        *w = winsize.ws_col;
        *h = winsize.ws_row;
    }
}
#define zterp_os_get_screen_size
#endif

static const char *ital = NULL, *rev = NULL, *bold = NULL, *none = NULL;
static char *fg_string = NULL, *bg_string = NULL;
static bool have_colors = false;
static bool have_24bit_rgb = false;
void zterp_os_init_term(void)
{
    if (setupterm(NULL, STDOUT_FILENO, NULL) != OK) {
        return;
    }

    // prefer italics over underline for emphasized text
    ital = tgetstr("ZH", NULL);
    if (ital == NULL) {
        ital = tgetstr("us", NULL);
    }
    rev  = tgetstr("mr", NULL);
    bold = tgetstr("md", NULL);
    none = tgetstr("me", NULL);

    fg_string = tgetstr("AF", NULL);
    bg_string = tgetstr("AB", NULL);

    have_colors = none != NULL && fg_string != NULL && bg_string != NULL;

    have_24bit_rgb = tigetflag("RGB") > 0 && tigetnum("colors") == 1U << 24;
}
#define zterp_os_init_term

bool zterp_os_have_style(int style)
{
    if (none == NULL) {
        return false;
    }

    switch (style) {
    case STYLE_ITALIC:
        return ital != NULL;
    case STYLE_REVERSE:
        return rev != NULL;
    case STYLE_BOLD:
        return bold != NULL;
    case STYLE_NONE:
        return none != NULL;
    default:
        return false;
    }
}
#define zterp_os_have_style

bool zterp_os_have_colors(void)
{
    return have_colors;
}
#define zterp_os_have_colors

static void set_color(const char *string, const struct color *color)
{
    switch (color->mode) {
    case ColorModeANSI:
        if (color->value >= 2 && color->value <= 9) {
            putp(tparm(string, color->value - 2));
        }
        break;
    case ColorModeTrue:
        if (have_24bit_rgb) {
            // Presumably for compatibility, even on terminals capable of
            // direct color, the values 0 to 7 are still treated as if on a
            // 16-color terminal (1 is red, 2 is green, and so on). Any
            // other value is treated as a 24-bit color in the expected
            // fashion. If a value between 1 and 7 is chosen, round to 0
            // (which is still black) or 8.
            uint16_t transformed = color->value < 4 ? 0 :
                                   color->value < 8 ? 8 :
                                   color->value;
            putp(tparm(string, screen_convert_color(transformed)));
        }
        break;
    }
}

void zterp_os_set_style(int style, const struct color *fg, const struct color *bg)
{
    // If the terminal cannot be reset, nothing can be used.
    if (none == NULL) {
        return;
    }

    putp(none);

    if ((style & STYLE_ITALIC) && ital != NULL) {
        putp(ital);
    }
    if ((style & STYLE_REVERSE) && rev != NULL) {
        putp(rev);
    }
    if ((style & STYLE_BOLD) && bold != NULL) {
        putp(bold);
    }

    if (have_colors) {
        set_color(fg_string, fg);
        set_color(bg_string, bg);
    }
}
#define zterp_os_set_style
#endif

// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║ Windows functions                                                            ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#elif defined(ZTERP_WIN32)
#include <windows.h>
#include <direct.h>
#include <sys/stat.h>

#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

bool zterp_os_rcfile(char *s, size_t n)
{
    const char *appdata;

    appdata = getenv("APPDATA");
    if (appdata == NULL) {
        return false;
    }

    checked_snprintf(s, n, "%s\\Bocfel\\bocfel.ini", appdata);

    return true;
}
#define zterp_os_rcfile

bool zterp_os_autosave_name(char *name, size_t n)
{
    const char *appdata;
    char drive[MAX_PATH], path[MAX_PATH], fname[MAX_PATH], ext[MAX_PATH];

    appdata = getenv("APPDATA");
    if (appdata == NULL) {
        return false;
    }

    if (_splitpath_s(game_file, NULL, 0, NULL, 0, fname, sizeof fname, ext, sizeof ext) != 0) {
        return false;
    }

    checked_snprintf(name, n, "%s\\Bocfel\\autosave\\%s%s-%s", appdata, fname, ext, get_story_id());

    if (_splitpath_s(name, drive, sizeof drive, path, sizeof path, NULL, 0, NULL, 0) != 0) {
        return false;
    }

    for (char *p = path; *p != 0; p++) {
        if ((*p == '/' || *p == '\\') && p != path) {
            char slash = *p, component[MAX_PATH];
            struct _stat st;
            *p = 0;
            checked_snprintf(component, sizeof component, "%s%s", drive, path);
            _mkdir(component);
            if (_stat(component, &st) == -1 || (st.st_mode & _S_IFDIR) != S_IFDIR) {
                return false;
            }
            *p = slash;
        }
    }

    return true;
}
#define zterp_os_autosave_name

#ifndef ZTERP_GLK
void zterp_os_get_screen_size(unsigned *w, unsigned *h)
{
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (handle != INVALID_HANDLE_VALUE) {
        CONSOLE_SCREEN_BUFFER_INFO screen;
        GetConsoleScreenBufferInfo(handle, &screen);

        *w = screen.srWindow.Right - screen.srWindow.Left + 1;
        *h = screen.srWindow.Bottom - screen.srWindow.Top + 1;
    }
}
#define zterp_os_get_screen_size

static bool terminal_processing_enabled = false;
void zterp_os_init_term(void)
{
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (handle != INVALID_HANDLE_VALUE) {
        DWORD mode;
        if (GetConsoleMode(handle, &mode)) {
            mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            terminal_processing_enabled = SetConsoleMode(handle, mode);
        }
    }
}
#define zterp_os_init_term

bool zterp_os_have_style(int style)
{
    return terminal_processing_enabled;
}
#define zterp_os_have_style

bool zterp_os_have_colors(void)
{
    return terminal_processing_enabled;
}
#define zterp_os_have_colors

void zterp_os_set_style(int style, const struct color *fg, const struct color *bg)
{
    if (!terminal_processing_enabled) {
        return;
    }

    ansi_set_style(style, fg, bg);
}
#define zterp_os_set_style
#endif

// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║ DOS functions                                                                ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#elif defined(ZTERP_DOS)
#ifndef ZTERP_GLK
// This assumes ANSI.SYS (or equivalent) is loaded.
bool zterp_os_have_style(int style)
{
    return true;
}
#define zterp_os_have_style

bool zterp_os_have_colors(void)
{
    return true;
}
#define zterp_os_have_colors

void zterp_os_set_style(int style, const struct color *fg, const struct color *bg)
{
    ansi_set_style(style, fg, bg);
}
#define zterp_os_set_style
#endif

#endif

// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║ Generic functions                                                            ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#ifndef zterp_os_filesize
long zterp_os_filesize(FILE *fp)
{
    // Assume fseek() can seek to the end of binary streams.
    if (fseek(fp, 0, SEEK_END) == -1) {
        return -1;
    }

    return ftell(fp);
}
#endif

#ifndef zterp_os_rcfile
bool zterp_os_rcfile(char *s, size_t n)
{
    checked_snprintf(s, n, "bocfelrc");
    return true;
}
#endif

#ifndef zterp_os_autosave_name
bool zterp_os_autosave_name(char *name, size_t n)
{
    return false;
}
#endif

#ifndef ZTERP_GLK
#ifndef zterp_os_get_screen_size
void zterp_os_get_screen_size(unsigned *w, unsigned *h)
{
}
#endif

#ifndef zterp_os_init_term
void zterp_os_init_term(void)
{
}
#endif

#ifndef zterp_os_have_style
bool zterp_os_have_style(int style)
{
    return false;
}
#endif

#ifndef zterp_os_have_colors
bool zterp_os_have_colors(void)
{
    return false;
}
#endif

#ifndef zterp_os_set_style
void zterp_os_set_style(int style, const struct color *fg, const struct color *bg)
{
}
#endif
#endif
