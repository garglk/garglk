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
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Bocfel. If not, see <http://www.gnu.org/licenses/>.

#ifdef ZTERP_UNIX
#define _XOPEN_SOURCE	600
#endif

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <limits.h>

#ifdef ZTERP_GLK
#include "glk.h"
#endif

#include "io.h"
#include "osdep.h"
#include "screen.h"
#include "util.h"
#include "zterp.h"

// OS-specific functions should all be collected in this file for
// convenience. A sort of poor-man’s “reverse” inheritance is used: for
// each function that a particular operating system provides, it should
// #define a macro of the same name. At the end of the file, a generic
// function is provided for each function that has no associated macro
// definition.
//
// The functions required are as follows:
//
// long zterp_os_filesize(FILE *fp)
//
// Return the size of the file referred to by fp. It is safe to assume
// that the file is opened in binary mode. The file position indicator
// need not be maintained. If the size of the file is larger than
// LONG_MAX, -1 should be returned.
//
// bool zterp_os_rcfile(char (*s)[ZTERP_OS_PATH_SIZE], bool create_parent)
//
// Different operating systems have different ideas about where
// configuration data should be stored; this function will copy a
// suitable value for the bocfel configuration file into the buffer
// “*s”. If a configuration file location cannot be determined, return
// false, otherwise true. If “create_parent” is true, attempt to create
// the containing directory, failing if this isn’t possible.
//
// bool zterp_os_autosave_name(char (*name)[ZTERP_OS_PATH_SIZE])
//
// Store a suitable filename for autosaving into “*name”. Return true if
// the file should be accessible, false if there is some problem. For
// example, the Unix implementation returns false if it can’t create all
// directory components in the filename. This should not verify that the
// file exists, because the first time an autosave is created, it
// necessarily won’t exist beforehand.
//
// bool zterp_os_edit_file(const char *filename, char *err, size_t errsize)
//
// Open the specified file in a text editor. Upon success, return true.
// Otherwise, return false, writing a suitable error message into “err”,
// a buffer whose size is “errsize” bytes long.
//
// bool zterp_os_edit_notes(const char *notes, size_t notes_len, char **new_notes, size_t *new_notes_len, char *err, size_t errsize)
//
// Given player notes in “notes” with size “notes_len”, open a text
// editor editing a file which contains those notes. When the editor
// successfully exits, store the contents of the saved file in
// “*new_notes” and its size in “*new_notes_len”, first allocating
// sufficient space with malloc(). Upon success, return true. Otherwise,
// return false, writing a suitable error message into “err”, a buffer
// whose size is “errsize” bytes long.
//
// Upon failure, the contents of “*new_notes” and “*new_notes_len” can
// be left undefined, but any memory allocated for the notes must be
// freed by this function before returning: if this function returns
// false, callers are not permitted to examine either “*new_notes” or
// “*new_notes_len”.
//
// The following functions are useful for non-Glk builds only. They
// provide for some handling of screen functions that is normally taken
// care of by Glk.
//
// void zterp_os_get_screen_size(unsigned *w, unsigned *h)
//
// The size of the terminal, if known, is written into *w (width) and *h
// (height). If terminal size is unavalable, nothing should be written.
//
// void zterp_os_init_term(void)
//
// If something special needs to be done to prepare the terminal for
// output, it should be done here. This function is called once at
// program startup.
//
// bool zterp_os_have_style(int style)
//
// This should return true if the provided style (see style.h for valid
// STYLE_ values) is available. It is safe to assume that styles will
// not be combined; e.g. this will not be called as:
// zterp_os_have_style(STYLE_BOLD | STYLE_ITALIC);
//
// bool zterp_os_have_colors(void)
//
// Returns true if the terminal supports colors.
//
// void zterp_os_set_style(int style, const struct color *fg, const struct color *bg)
//
// Set both a style and foreground/background color. Any previous
// settings should be ignored; for example, if the last call to
// zterp_os_set_style() turned on italics and the current call sets
// bold, the result should be bold, not bold italic.
// Unlike in zterp_os_have_style(), here styles may be combined. See
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

#if defined(ZTERP_UNIX) || defined(ZTERP_WIN32)
static bool read_file(const char *filename, char **new_file, size_t *new_file_len, char *err, size_t errsize)
{
    zterp_io *io = NULL;
    long size;

    io = zterp_io_open(filename, ZTERP_IO_MODE_RDONLY, ZTERP_IO_PURPOSE_DATA);
    if (io == NULL) {
        snprintf(err, errsize, "unable to open temporary file");
        goto err;
    }

    size = zterp_io_filesize(io);
    if (size == -1) {
        snprintf(err, errsize, "unable to determine size of temporary file");
        goto err;
    }

    if (!zterp_io_seek(io, 0, SEEK_SET)) {
        snprintf(err, errsize, "unable to seek in temporary file");
        goto err;
    }

    if (size != 0) {
        *new_file = malloc(size);
        *new_file_len = size;

        if (*new_file == NULL) {
            snprintf(err, errsize, "unable to allocate memory for new file");
            goto err;
        }

        if (!zterp_io_read_exact(io, *new_file, size)) {
            snprintf(err, errsize, "unable to read temporary file");
            goto err;
        }
    } else {
        *new_file = NULL;
        *new_file_len = 0;
    }

    zterp_io_close(io);

    return true;

err:
    if (io != NULL) {
        zterp_io_close(io);
    }

    return false;
}
#endif

#ifdef ZTERP_UNIX
#include <string.h>

#include <sys/stat.h>

static const char *autosave_basename(void)
{
    const char *base_name;

    base_name = strrchr(game_file, '/');
    if (base_name != NULL) {
        return base_name + 1;
    } else {
        return game_file;
    }
}

// Given a filename (or a directory name ending in a slash), create all
// components of all directories, returning false in case of error. This
// function will modify the passed-in string, although it will restore
// the original value before returning.
static bool mkdir_p(char *file)
{
    for (char *p = file; *p != 0; p++) {
        if (*p == '/' && p != file) {
            struct stat st;
            *p = 0;
            mkdir(file, 0755);
            if (stat(file, &st) == -1 || !S_ISDIR(st.st_mode)) {
                return false;
            }
            *p = '/';
        }
    }

    return true;
}
#endif

// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║ Unix functions                                                               ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#ifdef ZTERP_UNIX
#include <errno.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#ifdef __HAIKU__
#include <FindDirectory.h>
#endif

long zterp_os_filesize(FILE *fp)
{
    struct stat st;
    int fd = fileno(fp);

    if (fd == -1 || fstat(fd, &st) == -1 || !S_ISREG(st.st_mode) || st.st_size > LONG_MAX) {
        return -1;
    }

    return st.st_size;
}
#define have_zterp_os_filesize

bool zterp_os_rcfile(char (*s)[ZTERP_OS_PATH_SIZE], bool create_parent)
{
#ifdef __HAIKU__
    char settings_dir[ZTERP_OS_PATH_SIZE];

    if (find_directory(B_USER_SETTINGS_DIRECTORY, -1, false, settings_dir, sizeof settings_dir) != B_OK) {
        return false;
    }

    checked_snprintf(*s, sizeof *s, "%s/bocfel/bocfelrc", settings_dir);
#else
    const char *home;
    const char *config_home;

    home = getenv("HOME");
    if (home != NULL) {
        // This is the legacy location of the config file.
        checked_snprintf(*s, sizeof *s, "%s/.bocfelrc", home);
        if (access(*s, R_OK) == 0) {
            return true;
        }
    }

    config_home = getenv("XDG_CONFIG_HOME");
    if (config_home != NULL) {
        checked_snprintf(*s, sizeof *s, "%s/bocfel/bocfelrc", config_home);
    } else if (home != NULL) {
        checked_snprintf(*s, sizeof *s, "%s/.config/bocfel/bocfelrc", home);
    } else {
        return false;
    }
#endif

    return !create_parent || mkdir_p(*s);
}
#define have_zterp_os_rcfile

bool zterp_os_autosave_name(char (*name)[ZTERP_OS_PATH_SIZE])
{
#ifdef __HAIKU__
    char settings_dir[ZTERP_OS_PATH_SIZE];

    if (find_directory(B_USER_SETTINGS_DIRECTORY, -1, true, settings_dir, sizeof settings_dir) != B_OK) {
        return false;
    }

    checked_snprintf(*name, sizeof *name, "%s/bocfel/autosave/%s-%s", settings_dir, autosave_basename(), get_story_id());
#else
    const char *data_home;

    data_home = getenv("XDG_DATA_HOME");
    if (data_home != NULL) {
        checked_snprintf(*name, sizeof *name, "%s/bocfel/autosave/%s-%s", data_home, autosave_basename(), get_story_id());
    } else {
        char *home = getenv("HOME");
        if (home == NULL) {
            return false;
        }
        checked_snprintf(*name, sizeof *name, "%s/.local/share/bocfel/autosave/%s-%s", home, autosave_basename(), get_story_id());
    }
#endif

    return mkdir_p(*name);
}
#define have_zterp_os_autosave_name

bool zterp_os_edit_file(const char *filename, char *err, size_t errsize)
{
    pid_t pid;

    // A list of possible text editors. The user can specify a text
    // editor using the config file. If he doesn’t, or if it doesn’t
    // exist, keep trying various possible editors in hopes that one
    // exists and works. This really should use XDG to query the user’s
    // preferred text editor, but for now, it doesn’t.
    const char *editors[] = {
        options.editor,
#if defined(__HAIKU__)
        "StyledEdit",
#elif defined(__serenity__)
        "TextEditor",
#elif !defined(__APPLE__)
        "kate",
        "kwrite",
        "nedit-ng",
        "nedit",
        "gedit",
#endif
    };

    pid = fork();
    switch (pid) {
    case -1:
        snprintf(err, errsize, "unable to create new process");
        return false;
    case 0: {
        for (size_t i = 0; i < ASIZE(editors); i++) {
            if (editors[i] != NULL) {
                execlp(editors[i], editors[i], filename, (char *)NULL);
            }
        }
#ifdef __APPLE__
        execlp("open", "open", "-W", "-t", filename, (char *)NULL);
#endif
        _exit(127);
    }
    default: {
        int status;

#ifdef ZTERP_GLK
        while (waitpid(pid, &status, WNOHANG) != pid) {
            usleep(10000);
            glk_tick();
        }
#else
        waitpid(pid, &status, 0);
#endif

        if (!WIFEXITED(status)) {
            snprintf(err, errsize, "editor process terminated abnormally");
            return false;
        }

#ifndef __APPLE__
        if (WEXITSTATUS(status) == 127) {
            snprintf(err, errsize, "unable to find a text editor");
            return false;
        }
#endif

        if (WEXITSTATUS(status) != 0) {
            snprintf(err, errsize, "editor had exit status %d", WEXITSTATUS(status));
            return false;
        }
    }
    }

    return true;
}
#define have_zterp_os_edit_file

bool zterp_os_edit_notes(const char *notes, size_t notes_len, char **new_notes, size_t *new_notes_len, char *err, size_t errsize)
{
    const char *tmpdir;
    char template[ZTERP_OS_PATH_SIZE];
    int bytes_needed;
    int fd;
    size_t bytes;

    tmpdir = getenv("TMPDIR");
    if (tmpdir == NULL) {
        tmpdir = "/tmp";
    }

    bytes_needed = snprintf(template, sizeof template, "%s/bocfel.notes.XXXXXX", tmpdir);
    if (bytes_needed < 0 || bytes_needed >= sizeof template) {
        snprintf(err, errsize, "path to temporary file is too long");
        return false;
    }

    fd = mkstemp(template);
    if (fd == -1) {
        snprintf(err, errsize, "unable to create temporary file");
        return false;
    }

    bytes = 0;
    while (bytes < notes_len) {
        ssize_t n = write(fd, notes + bytes, notes_len - bytes);
        if (n == -1) {
            snprintf(err, errsize, "unable to write to temporary file");
            close(fd);
            return false;
        }
        bytes += n;
    }

    close(fd);

    if (!zterp_os_edit_file(template, err, errsize)) {
        goto err;
    }

    if (!read_file(template, new_notes, new_notes_len, err, errsize)) {
        goto err;
    }

    remove(template);

    return true;

err:
    remove(template);
    return false;
}
#define have_zterp_os_edit_notes

#ifndef ZTERP_GLK

#ifndef ZTERP_NO_CURSES
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
#define have_zterp_os_get_screen_size
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
#define have_zterp_os_init_term

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
#define have_zterp_os_have_style

bool zterp_os_have_colors(void)
{
    return have_colors;
}
#define have_zterp_os_have_colors

static void set_color(const char *string, const struct color *color)
{
    // Cast the first argument to tparm() below to char*, for unfortunate
    // historical purposes: X/Open defines tparm() as taking a char*, and
    // while most modern Unixes have a Curses implementation that takes
    // const char*, at least macOS still takes a char*. No reasonable Curses
    // implementation will modify the argument, so it’s safe enough to just
    // cast here.

    switch (color->mode) {
    case ColorModeANSI:
        if (color->value >= 2 && color->value <= 9) {
            putp(tparm((char *)string, color->value - 2));
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
            putp(tparm((char *)string, screen_convert_color(transformed)));
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
#define have_zterp_os_set_style
#endif

#endif

// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║ Windows functions                                                            ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#elif defined(ZTERP_WIN32)
#include <windows.h>
#include <direct.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

long zterp_os_filesize(FILE *fp)
{
    struct _stat st;
    int fd = _fileno(fp);

    if (fd == -1 || _fstat(_fileno(fp), &st) == -1 || (st.st_mode & _S_IFREG) == 0 || st.st_size > LONG_MAX) {
        return -1;
    }

    return st.st_size;
}
#define have_zterp_os_filesize

static bool mkdir_p(const char *filename)
{
    char drive[MAX_PATH], path[MAX_PATH];

    if (_splitpath_s(filename, drive, sizeof drive, path, sizeof path, NULL, 0, NULL, 0) != 0) {
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

bool zterp_os_rcfile(char (*s)[ZTERP_OS_PATH_SIZE], bool create_parent)
{
    const char *appdata;

    appdata = getenv("APPDATA");
    if (appdata == NULL) {
        return false;
    }

    checked_snprintf(*s, sizeof *s, "%s\\Bocfel\\bocfel.ini", appdata);

    return !create_parent || mkdir_p(*s);
}
#define have_zterp_os_rcfile

bool zterp_os_autosave_name(char (*name)[ZTERP_OS_PATH_SIZE])
{
    const char *appdata;
    char fname[MAX_PATH], ext[MAX_PATH];

    appdata = getenv("APPDATA");
    if (appdata == NULL) {
        return false;
    }

    if (_splitpath_s(game_file, NULL, 0, NULL, 0, fname, sizeof fname, ext, sizeof ext) != 0) {
        return false;
    }

    checked_snprintf(*name, sizeof *name, "%s\\Bocfel\\autosave\\%s%s-%s", appdata, fname, ext, get_story_id());

    return mkdir_p(*name);
}
#define have_zterp_os_autosave_name

bool zterp_os_edit_file(const char *filename, char *err, size_t errsize)
{
    SHELLEXECUTEINFO si = {
        .cbSize = sizeof(SHELLEXECUTEINFO),
        .fMask = SEE_MASK_NOCLOSEPROCESS,
        .hwnd = NULL,
        .lpVerb = "open",
        .lpFile = filename,
        .lpParameters = NULL,
        .lpDirectory = NULL,
        .nShow = SW_SHOW,
        .hInstApp = NULL,
    };

    if (!ShellExecuteEx(&si)) {
        snprintf(err, errsize, "unable to launch text editor");
        return false;
    }

#ifdef ZTERP_GLK
    while (WaitForSingleObject(si.hProcess, 100) != 0) {
        glk_tick();
    }
#else
    WaitForSingleObject(si.hProcess, INFINITE);
#endif

    CloseHandle(si.hProcess);

    return true;
}
#define have_zterp_os_edit_file

bool zterp_os_edit_notes(const char *notes, size_t notes_len, char **new_notes, size_t *new_notes_len, char *err, size_t errsize)
{
    char tempdir[300];
    char notes_name[1024];
    zterp_io *io;

    DWORD ret = GetTempPath(sizeof tempdir, tempdir);
    if (ret == 0 || ret > sizeof tempdir) {
        snprintf(err, errsize, "unable to find temporary directory");
        return false;
    }

    snprintf(notes_name, sizeof notes_name, "%s\\bocfel_notes.txt", tempdir);
    io = zterp_io_open(notes_name, ZTERP_IO_MODE_WRONLY, ZTERP_IO_PURPOSE_DATA);
    if (io == NULL) {
        snprintf(err, errsize, "unable to create temporary file");
        return false;
    }

    if (!zterp_io_write_exact(io, notes, notes_len)) {
        zterp_io_close(io);
        snprintf(err, errsize, "unable to write to temporary file");
        goto err;
    }

    zterp_io_close(io);

    if (!zterp_os_edit_file(notes_name, err, errsize)) {
        goto err;
    }

    if (!read_file(notes_name, new_notes, new_notes_len, err, errsize)) {
        goto err;
    }

    remove(notes_name);

    return true;

err:
    remove(notes_name);

    return false;
}
#define have_zterp_os_edit_notes

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
#define have_zterp_os_get_screen_size

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
#define have_zterp_os_init_term

bool zterp_os_have_style(int style)
{
    return terminal_processing_enabled;
}
#define have_zterp_os_have_style

bool zterp_os_have_colors(void)
{
    return terminal_processing_enabled;
}
#define have_zterp_os_have_colors

void zterp_os_set_style(int style, const struct color *fg, const struct color *bg)
{
    if (!terminal_processing_enabled) {
        return;
    }

    ansi_set_style(style, fg, bg);
}
#define have_zterp_os_set_style
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
#define have_zterp_os_have_style

bool zterp_os_have_colors(void)
{
    return true;
}
#define have_zterp_os_have_colors

void zterp_os_set_style(int style, const struct color *fg, const struct color *bg)
{
    ansi_set_style(style, fg, bg);
}
#define have_zterp_os_set_style
#endif

#endif

// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║ Generic functions                                                            ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#ifndef have_zterp_os_filesize
long zterp_os_filesize(FILE *fp)
{
    // Assume fseek() can seek to the end of binary streams.
    if (fseek(fp, 0, SEEK_END) == -1) {
        return -1;
    }

    return ftell(fp);
}
#endif

#ifndef have_zterp_os_edit_file
bool zterp_os_edit_file(const char *filename, char *err, size_t errsize)
{
    snprintf(err, errsize, "editing unimplemented on this platform");
    return false;
}
#endif

#ifndef have_zterp_os_rcfile
bool zterp_os_rcfile(char (*s)[ZTERP_OS_PATH_SIZE], bool create_parent)
{
    checked_snprintf(*s, sizeof *s, "bocfelrc");
    return true;
}
#endif

#ifndef have_zterp_os_autosave_name
bool zterp_os_autosave_name(char (*name)[ZTERP_OS_PATH_SIZE])
{
    return false;
}
#endif

#ifndef have_zterp_os_edit_notes
bool zterp_os_edit_notes(const char *notes, size_t notes_len, char **new_notes, size_t *new_notes_len, char *err, size_t errsize)
{
    snprintf(err, errsize, "notes unimplemented on this platform");
    return false;
}
#endif

#ifndef ZTERP_GLK
#ifndef have_zterp_os_get_screen_size
void zterp_os_get_screen_size(unsigned *w, unsigned *h)
{
}
#endif

#ifndef have_zterp_os_init_term
void zterp_os_init_term(void)
{
}
#endif

#ifndef have_zterp_os_have_style
bool zterp_os_have_style(int style)
{
    return false;
}
#endif

#ifndef have_zterp_os_have_colors
bool zterp_os_have_colors(void)
{
    return false;
}
#endif

#ifndef have_zterp_os_set_style
void zterp_os_set_style(int style, const struct color *fg, const struct color *bg)
{
}
#endif
#endif
