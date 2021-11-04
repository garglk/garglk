/******************************************************************************
 *                                                                            *
 * Copyright (C) 2006-2009 by Tor Andersson, Jesse McGrew.                    *
 * Copyright (C) 2010 by Ben Cressey, Chris Spiegel.                          *
 *                                                                            *
 * This file is part of Gargoyle.                                             *
 *                                                                            *
 * Gargoyle is free software; you can redistribute it and/or modify           *
 * it under the terms of the GNU General Public License as published by       *
 * the Free Software Foundation; either version 2 of the License, or          *
 * (at your option) any later version.                                        *
 *                                                                            *
 * Gargoyle is distributed in the hope that it will be useful,                *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * GNU General Public License for more details.                               *
 *                                                                            *
 * You should have received a copy of the GNU General Public License          *
 * along with Gargoyle; if not, write to the Free Software                    *
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA *
 *                                                                            *
 *****************************************************************************/

/*
 * Private header file for the Gargoyle Implementation of the Glk API.
 * Glk API which this implements: version 0.7.3.
 * Glk designed by Andrew Plotkin <erkyrath@eblong.com>
 * http://www.eblong.com/zarf/glk/index.html
 */

#ifndef GARGLK_GARGLK_H
#define GARGLK_GARGLK_H

// The order here is significant: for the time being, at least, the
// macOS code directly indexes an array using these values.
enum FILEFILTERS { FILTER_SAVE, FILTER_TEXT, FILTER_DATA };

#ifdef __cplusplus
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace garglk {

// This represents a possible configuration file (garglk.ini).
struct ConfigFile {
    ConfigFile(const std::string &path_, bool user_) : path(path_), user(user_) {
    }

    // The path to the file itself.
    std::string path;

    // If true, this config file should be considered as a “user” config
    // file, one that a user would reasonably expect to be a config file
    // for general use. This excludes game-specific config files, for
    // example, while considering various possibilities for config
    // files, such as $HOME/.garglkrc or $HOME/.config/garglk.ini.
    bool user;
};

std::string winopenfile(const char *prompt, enum FILEFILTERS filter);
std::string winsavefile(const char *prompt, enum FILEFILTERS filter);
void winabort(const std::string &msg);
std::string downcase(const std::string &string);
void fontreplace(const std::string &font, int type);
std::vector<ConfigFile> configs(const std::string &exedir, const std::string &gamepath);
void config_entries(const std::string &fname, bool accept_bare, const std::vector<std::string> &matches, std::function<void(const std::string &cmd, const std::string &arg)> callback);
std::string user_config();
void set_lcdfilter(const std::string &filter);
std::string winfontpath(const std::string &filename);

template <typename T, typename Deleter>
std::unique_ptr<T, Deleter> unique(T *p, Deleter deleter)
{
    return std::unique_ptr<T, Deleter>(p, deleter);
}

}

extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "gi_dispa.h"
#include "glk.h"

/* This macro is called whenever the library code catches an error
 * or illegal operation from the game program.
 */

#define gli_strict_warning(...) do { \
    fputs("Glk library error: ", stderr); \
    fprintf(stderr, __VA_ARGS__); \
    putc('\n', stderr); \
} while(0)

extern bool gli_utf8output, gli_utf8input;

/* Callbacks necessary for the dispatch layer.  */

extern gidispatch_rock_t (*gli_register_obj)(void *obj, glui32 objclass);
extern void (*gli_unregister_obj)(void *obj, glui32 objclass,
    gidispatch_rock_t objrock);
extern gidispatch_rock_t (*gli_register_arr)(void *array, glui32 len,
    char *typecode);
extern void (*gli_unregister_arr)(void *array, glui32 len, char *typecode,
    gidispatch_rock_t objrock);

/* Some useful type declarations. */

typedef struct glk_window_struct window_t;
typedef struct glk_stream_struct stream_t;
typedef struct glk_fileref_struct fileref_t;
typedef struct glk_schannel_struct channel_t;

typedef struct window_blank_s window_blank_t;
typedef struct window_pair_s window_pair_t;
typedef struct window_textgrid_s window_textgrid_t;
typedef struct window_textbuffer_s window_textbuffer_t;
typedef struct window_graphics_s window_graphics_t;

/* ---------------------------------------------------------------------- */
/*
 * Drawing operations and fonts and stuff
 */

/* Some globals for gargoyle */

#define TBLINELEN 300
#define SCROLLBACK 512
#define HISTORYLEN 100

#define GLI_SUBPIX 8

extern char gli_program_name[256];
extern char gli_program_info[256];
extern char gli_story_name[256];
extern char gli_story_title[256];
extern bool gli_terminated;

extern window_t *gli_rootwin;
extern window_t *gli_focuswin;

extern bool gli_force_redraw;
extern bool gli_more_focus;
extern int gli_cellw;
extern int gli_cellh;

/* Unicode ligatures and smart typography glyphs */
#define UNI_LIG_FF	0xFB00
#define UNI_LIG_FI	0xFB01
#define UNI_LIG_FL	0xFB02
#define UNI_LIG_FFI	0xFB03
#define UNI_LIG_FFL	0xFB04
#define UNI_LSQUO	0x2018
#define UNI_RSQUO	0x2019
#define UNI_LDQUO	0x201c
#define UNI_RDQUO	0x201d
#define UNI_NDASH	0x2013
#define UNI_MDASH	0x2014

typedef struct rect_s rect_t;
typedef struct picture_s picture_t;
typedef struct piclist_s piclist_t;
typedef struct style_s style_t;
typedef struct mask_s mask_t;

struct rect_s
{
    int x0, y0;
    int x1, y1;
};

struct picture_s
{
    int refcount;
    int w, h;
    unsigned char *rgba;
    unsigned long id;
    bool scaled;
};

struct piclist_s
{
    picture_t *picture;
    picture_t *scaled;
    struct piclist_s *next;
};

struct style_s
{
    int font;
    unsigned char bg[3];
    unsigned char fg[3];
    bool reverse;
};

struct mask_s
{
    int hor;
    int ver;
    glui32 **links;
    rect_t select;
};

extern int gli_image_s;	/* stride */
extern int gli_image_w;
extern int gli_image_h;
extern unsigned char *gli_image_rgb;

/*
 * Config globals
 */

extern char gli_workdir[];
extern char gli_workfile[];

extern style_t gli_tstyles[style_NUMSTYLES];
extern style_t gli_gstyles[style_NUMSTYLES];

extern style_t gli_tstyles_def[style_NUMSTYLES];
extern style_t gli_gstyles_def[style_NUMSTYLES];

extern unsigned char gli_window_color[3];
extern unsigned char gli_border_color[3];
extern unsigned char gli_caret_color[3];
extern unsigned char gli_more_color[3];
extern unsigned char gli_link_color[3];

extern unsigned char gli_window_save[3];
extern unsigned char gli_border_save[3];
extern unsigned char gli_caret_save[3];
extern unsigned char gli_more_save[3];
extern unsigned char gli_link_save[3];

extern bool gli_override_fg_set;
extern int gli_override_fg_val;
extern bool gli_override_bg_set;
extern int gli_override_bg_val;
extern bool gli_override_reverse;

extern int gli_link_style;
extern int gli_caret_shape;
extern int gli_wborderx;
extern int gli_wbordery;

extern int gli_wmarginx;
extern int gli_wmarginy;
extern int gli_wmarginx_save;
extern int gli_wmarginy_save;
extern int gli_wpaddingx;
extern int gli_wpaddingy;
extern int gli_tmarginx;
extern int gli_tmarginy;

extern bool gli_conf_lcd;
extern unsigned char gli_conf_lcd_weights[5];

extern bool gli_conf_graphics;
extern bool gli_conf_sound;

extern bool gli_conf_fullscreen;

extern bool gli_conf_speak;
extern bool gli_conf_speak_input;

#ifdef __cplusplus
extern std::string gli_conf_speak_language;
#endif

extern bool gli_conf_stylehint;
extern bool gli_conf_safeclicks;

extern bool gli_conf_justify;
extern int gli_conf_quotes;
extern int gli_conf_dashes;
extern int gli_conf_spaces;
extern bool gli_conf_caps;

extern int gli_cols;
extern int gli_rows;

extern bool gli_conf_lockcols;
extern bool gli_conf_lockrows;

extern unsigned char gli_scroll_bg[3];
extern unsigned char gli_scroll_fg[3];
extern int gli_scroll_width;

extern int gli_baseline;
extern int gli_leading;

enum FACES { MONOR, MONOB, MONOI, MONOZ, PROPR, PROPB, PROPI, PROPZ };
enum TYPES { MONOF, PROPF };
enum STYLES { FONTR, FONTB, FONTI, FONTZ };

#ifdef __cplusplus
struct gli_font_files {
    std::string r, b, i, z;
};
extern std::string gli_conf_propfont;
extern struct gli_font_files gli_conf_prop, gli_conf_prop_override;
extern std::string gli_conf_monofont;
extern struct gli_font_files gli_conf_mono, gli_conf_mono_override;
#endif

extern float gli_conf_gamma;
extern float gli_conf_propsize;
extern float gli_conf_monosize;
extern float gli_conf_propaspect;
extern float gli_conf_monoaspect;

extern glui32 *gli_more_prompt;
extern glui32 gli_more_prompt_len;
extern int gli_more_align;
extern int gli_more_font;

extern bool gli_forceclick;
extern bool gli_copyselect;
extern bool gli_drawselect;
extern bool gli_claimselect;

/*
 * Standard Glk I/O stuff
 */

/* A macro that I can't think of anywhere else to put it. */

#define gli_event_clearevent(evp)  \
    ((evp)->type = evtype_None,    \
    (evp)->win = NULL,    \
    (evp)->val1 = 0,   \
    (evp)->val2 = 0)

typedef struct eventlog_s eventlog_t;
typedef struct eventqueue_s eventqueue_t;

struct eventlog_s
{
    event_t *event;
    struct eventlog_s *next;
};

struct eventqueue_s
{
    eventlog_t *first;
    eventlog_t *last;
};

void gli_dispatch_event(event_t *event, int polled);

#define MAGIC_WINDOW_NUM (9876)
#define MAGIC_STREAM_NUM (8769)
#define MAGIC_FILEREF_NUM (7698)

#define strtype_File (1)
#define strtype_Window (2)
#define strtype_Memory (3)
#define strtype_Resource (4)

struct glk_stream_struct
{
    glui32 magicnum;
    glui32 rock;

    int type; /* file, window, or memory stream */
    bool unicode; /* one-byte or four-byte chars? Not meaningful for windows */

    glui32 readcount, writecount;
    bool readable, writable;

    /* for strtype_Window */
    window_t *win;

    /* for strtype_File */
    FILE *file;
    glui32 lastop; /* 0, filemode_Write, or filemode_Read */

    /* for strtype_Resource */
    int isbinary;

    /* for strtype_Memory and strtype_Resource. Separate pointers for 
       one-byte and four-byte streams */
    unsigned char *buf;
    unsigned char *bufptr;
    unsigned char *bufend;
    unsigned char *bufeof;
    glui32 *ubuf;
    glui32 *ubufptr;
    glui32 *ubufend;
    glui32 *ubufeof;
    glui32 buflen;
    gidispatch_rock_t arrayrock;

    gidispatch_rock_t disprock;
    stream_t *next, *prev; /* in the big linked list of streams */
};

struct glk_fileref_struct
{
    glui32 magicnum;
    glui32 rock;

    char *filename;
    int filetype;
    bool textmode;

    gidispatch_rock_t disprock;
    fileref_t *next, *prev; /* in the big linked list of filerefs */
};

/*
 * Windows and all that
 */

// For some reason MinGW does "typedef hyper __int64", which conflicts
// with attr_s.hyper below. Unconditionally undefine it here so any
// files which include windows.h will not cause build failures.
#undef hyper

typedef struct attr_s
{
    bool fgset;
    bool bgset;
    bool reverse;
    glui32 style;
    glui32 fgcolor;
    glui32 bgcolor;
    glui32 hyper;
} attr_t;

struct glk_window_struct
{
    glui32 magicnum;
    glui32 rock;
    glui32 type;

    window_t *parent; /* pair window which contains this one */
    rect_t bbox;
    int yadj;
    void *data; /* one of the window_*_t structures */

    stream_t *str; /* the window stream. */
    stream_t *echostr; /* the window's echo stream, if any. */

    bool line_request;
    bool line_request_uni;
    bool char_request;
    bool char_request_uni;
    bool mouse_request;
    bool hyper_request;
    bool more_request;
    bool scroll_request;
    bool image_loaded;

    bool echo_line_input;
    glui32 *line_terminators;
    glui32 termct;

    attr_t attr;
    unsigned char bgcolor[3];
    unsigned char fgcolor[3];

    gidispatch_rock_t disprock;
    window_t *next, *prev; /* in the big linked list of windows */
};

struct window_blank_s
{
    window_t *owner;
};

struct window_pair_s
{
    window_t *owner;
    window_t *child1, *child2;

    /* split info... */
    glui32 dir; /* winmethod_Left, Right, Above, or Below */
    bool vertical, backward; /* flags */
    glui32 division; /* winmethod_Fixed or winmethod_Proportional */
    window_t *key; /* NULL or a leaf-descendant (not a Pair) */
    bool keydamage; /* used as scratch space in window closing */
    glui32 size; /* size value */
    glui32 wborder;  /* winMethod_Border, NoBorder */
};

/* One line of the grid window. */
typedef struct tgline_s
{
    int dirty;
    glui32 chars[256];
    attr_t attrs[256];
} tgline_t;

struct window_textgrid_s
{
    window_t *owner;

    int width, height;
    tgline_t lines[256];

    int curx, cury; /* the window cursor position */

    /* for line input */
    void *inbuf;	/* unsigned char* for latin1, glui32* for unicode */
    int inunicode;
    int inorgx, inorgy;
    int inoriglen, inmax;
    int incurs, inlen;
    attr_t origattr;
    gidispatch_rock_t inarrayrock;
    glui32 *line_terminators;

    /* style hints and settings */
    style_t styles[style_NUMSTYLES];
};

typedef struct tbline_s
{
    int len;
    bool newline, dirty, repaint;
    picture_t *lpic, *rpic;
    glui32 lhyper, rhyper;
    int lm, rm;
    glui32 chars[TBLINELEN];
    attr_t attrs[TBLINELEN];
} tbline_t;

struct window_textbuffer_s
{
    window_t *owner;

    int width, height;
    int spaced;
    int dashed;

    tbline_t *lines;
    int scrollback;

    int numchars;		/* number of chars in last line: lines[0] */
    glui32 *chars;		/* alias to lines[0].chars */
    attr_t *attrs;		/* alias to lines[0].attrs */

    /* adjust margins temporarily for images */
    int ladjw;
    int ladjn;
    int radjw;
    int radjn;

    /* Command history. */
    glui32 *history[HISTORYLEN];
    int historypos;
    int historyfirst, historypresent;

    /* for paging */
    int lastseen;
    int scrollpos;
    int scrollmax;

    /* for line input */
    void *inbuf;	/* unsigned char* for latin1, glui32* for unicode */
    bool inunicode;
    int inmax;
    long infence;
    long incurs;
    attr_t origattr;
    gidispatch_rock_t inarrayrock;

    bool echo_line_input;
    glui32 *line_terminators;

    /* style hints and settings */
    style_t styles[style_NUMSTYLES];

    /* for copy selection */
    glui32 *copybuf;
    int copypos;
};

struct window_graphics_s
{
    window_t *owner;
    unsigned char bgnd[3];
    int dirty;
    int w, h;
    unsigned char *rgb;
};

/* ---------------------------------------------------------------------- */

extern void gli_initialize_sound(void);
extern void gli_initialize_tts(void);
extern void gli_tts_speak(const glui32 *buf, size_t len);
extern void gli_tts_flush(void);
extern void gli_tts_purge(void);
extern gidispatch_rock_t gli_sound_get_channel_disprock(const channel_t *chan);

/* ---------------------------------------------------------------------- */
/*
 * All the annoyingly boring and tedious prototypes...
 */

extern window_blank_t *win_blank_create(window_t *win);
extern void win_blank_destroy(window_blank_t *dwin);
extern void win_blank_rearrange(window_t *win, rect_t *box);
extern void win_blank_redraw(window_t *win);

extern window_pair_t *win_pair_create(window_t *win, glui32 method, window_t *key, glui32 size);
extern void win_pair_destroy(window_pair_t *dwin);
extern void win_pair_rearrange(window_t *win, rect_t *box);
extern void win_pair_redraw(window_t *win);
extern void win_pair_click(window_pair_t *dwin, int x, int y);

extern window_textgrid_t *win_textgrid_create(window_t *win);
extern void win_textgrid_destroy(window_textgrid_t *dwin);
extern void win_textgrid_rearrange(window_t *win, rect_t *box);
extern void win_textgrid_redraw(window_t *win);
extern void win_textgrid_putchar_uni(window_t *win, glui32 ch);
extern bool win_textgrid_unputchar_uni(window_t *win, glui32 ch);
extern void win_textgrid_clear(window_t *win);
extern void win_textgrid_move_cursor(window_t *win, int xpos, int ypos);
extern void win_textgrid_init_line(window_t *win, char *buf, int maxlen, int initlen);
extern void win_textgrid_init_line_uni(window_t *win, glui32 *buf, int maxlen, int initlen);
extern void win_textgrid_cancel_line(window_t *win, event_t *ev);
extern void win_textgrid_click(window_textgrid_t *dwin, int x, int y);
extern void gcmd_grid_accept_readchar(window_t *win, glui32 arg);
extern void gcmd_grid_accept_readline(window_t *win, glui32 arg);

extern window_textbuffer_t *win_textbuffer_create(window_t *win);
extern void win_textbuffer_destroy(window_textbuffer_t *dwin);
extern void win_textbuffer_rearrange(window_t *win, rect_t *box);
extern void win_textbuffer_redraw(window_t *win);
extern void win_textbuffer_putchar_uni(window_t *win, glui32 ch);
extern bool win_textbuffer_unputchar_uni(window_t *win, glui32 ch);
extern void win_textbuffer_clear(window_t *win);
extern void win_textbuffer_init_line(window_t *win, char *buf, int maxlen, int initlen);
extern void win_textbuffer_init_line_uni(window_t *win, glui32 *buf, int maxlen, int initlen);
extern void win_textbuffer_cancel_line(window_t *win, event_t *ev);
extern void win_textbuffer_click(window_textbuffer_t *dwin, int x, int y);
extern void gcmd_buffer_accept_readchar(window_t *win, glui32 arg);
extern void gcmd_buffer_accept_readline(window_t *win, glui32 arg);
extern bool gcmd_accept_scroll(window_t *win, glui32 arg);

/* Declarations of library internal functions. */

extern void gli_initialize_misc(void);
extern void gli_initialize_windows(void);
extern void gli_initialize_babel(void);

extern window_t *gli_new_window(glui32 type, glui32 rock);
extern void gli_delete_window(window_t *win);
extern window_t *gli_window_iterate_treeorder(window_t *win);

extern void gli_window_rearrange(window_t *win, rect_t *box);
extern void gli_window_redraw(window_t *win);
extern void gli_window_put_char_uni(window_t *win, glui32 ch);
extern bool gli_window_unput_char_uni(window_t *win, glui32 ch);
extern bool gli_window_check_terminator(glui32 ch);
extern void gli_window_refocus(window_t *win);

extern void gli_windows_redraw(void);
extern void gli_windows_size_change(void);
extern void gli_windows_unechostream(stream_t *str);

extern void gli_window_click(window_t *win, int x, int y);

void gli_redraw_rect(int x0, int y0, int x1, int y1);

void gli_input_handle_key(glui32 key);
void gli_input_handle_click(int x, int y);
void gli_event_store(glui32 type, window_t *win, glui32 val1, glui32 val2);

extern stream_t *gli_new_stream(int type, int readable, int writable,
    glui32 rock);
extern void gli_delete_stream(stream_t *str);
extern stream_t *gli_stream_open_window(window_t *win);
extern strid_t gli_stream_open_pathname(char *pathname, int writemode,
    int textmode, glui32 rock);
extern void gli_stream_set_current(stream_t *str);
extern void gli_stream_fill_result(stream_t *str,
    stream_result_t *result);
extern void gli_stream_echo_line(stream_t *str, char *buf, glui32 len);
extern void gli_stream_echo_line_uni(stream_t *str, glui32 *buf, glui32 len);
extern void gli_streams_close_all(void);

void gli_initialize_fonts(void);
void gli_draw_pixel(int x, int y, unsigned char alpha, const unsigned char *rgb);
void gli_draw_pixel_lcd(int x, int y, const unsigned char *alpha, const unsigned char *rgb);
void gli_draw_clear(unsigned char *rgb);
void gli_draw_rect(int x, int y, int w, int h, unsigned char *rgb);
int gli_draw_string_uni(int x, int y, int f, unsigned char *rgb, glui32 *text, int len, int spacewidth);
int gli_string_width_uni(int f, glui32 *text, int len, int spw);
void gli_draw_caret(int x, int y);
void gli_draw_picture(picture_t *pic, int x, int y, int x0, int y0, int x1, int y1);

void gli_startup(int argc, char *argv[]);

extern void gli_select(event_t *event, int polled);
#ifdef GARGLK_TICK
extern void gli_tick(void);
#endif

void wininit(int *argc, char **argv);
void winopen(void);
void wintitle(void);
void winmore(void);
void winrepaint(int x0, int y0, int x1, int y1);
void winexit(void);
void winclipstore(glui32 *text, int len);

void fontload(void);
void fontunload(void);

void giblorb_get_resource(glui32 usage, glui32 resnum, FILE **file, long *pos, long *len, glui32 *type);

picture_t *gli_picture_load(unsigned long id);
void gli_picture_store(picture_t *pic);
picture_t *gli_picture_retrieve(unsigned long id, bool scaled);
picture_t *gli_picture_scale(picture_t *src, int destwidth, int destheight);
void gli_picture_increment(picture_t *pic);
void gli_picture_decrement(picture_t *pic);
piclist_t *gli_piclist_search(unsigned long id);
void gli_piclist_clear(void);
void gli_piclist_increment(void);
void gli_piclist_decrement(void);
void gli_picture_store_original(picture_t *pic);
void gli_picture_store_scaled(picture_t *pic);

window_graphics_t *win_graphics_create(window_t *win);
void win_graphics_destroy(window_graphics_t *cutwin);
void win_graphics_rearrange(window_t *win, rect_t *box);
void win_graphics_get_size(window_t *win, glui32 *width, glui32 *height);
void win_graphics_redraw(window_t *win);
void win_graphics_click(window_graphics_t *dwin, int x, int y);

bool win_graphics_draw_picture(window_graphics_t *cutwin,
  glui32 image, glsi32 xpos, glsi32 ypos,
  bool scale, glui32 imagewidth, glui32 imageheight);
void win_graphics_erase_rect(window_graphics_t *cutwin, bool whole, glsi32 xpos, glsi32 ypos, glui32 width, glui32 height);
void win_graphics_fill_rect(window_graphics_t *cutwin, glui32 color, glsi32 xpos, glsi32 ypos, glui32 width, glui32 height);
void win_graphics_set_background_color(window_graphics_t *cutwin, glui32 color);

glui32 win_textbuffer_draw_picture(window_textbuffer_t *dwin, glui32 image, glui32 align, bool scaled, glui32 width, glui32 height);
glui32 win_textbuffer_flow_break(window_textbuffer_t *win);

void gli_calc_padding(window_t *win, int *x, int *y);

/* unicode case mapping */

typedef glui32 gli_case_block_t[2]; /* upper, lower */
/* If both are 0xFFFFFFFF, you have to look at the special-case table */

typedef glui32 gli_case_special_t[3]; /* upper, lower, title */
/* Each of these points to a subarray of the unigen_special_array
(in cgunicode.c). In that subarray, element zero is the length,
and that's followed by length unicode values. */

typedef glui32 gli_decomp_block_t[2]; /* count, position */
/* The position points to a subarray of the unigen_decomp_array.
   If the count is zero, there is no decomposition. */

void gli_putchar_utf8(glui32 val, FILE *fl);
glui32 gli_getchar_utf8(FILE *fl);
glui32 gli_parse_utf8(const unsigned char *buf, glui32 buflen, glui32 *out, glui32 outlen);

glui32 gli_strlen_uni(const glui32 *s);

void gli_put_hyperlink(glui32 linkval, unsigned int x0, unsigned int y0, unsigned int x1, unsigned int y1);
glui32 gli_get_hyperlink(unsigned int x, unsigned int y);
void gli_clear_selection(void);
bool gli_check_selection(unsigned int x0, unsigned int y0, unsigned int x1, unsigned int y1);
bool gli_get_selection(unsigned int x0, unsigned int y0, unsigned int x1, unsigned int y1, unsigned int *rx0, unsigned int *rx1);
void gli_clipboard_copy(glui32 *buf, int len);
void gli_start_selection(int x, int y);
void gli_resize_mask(unsigned int x, unsigned int y);
void gli_move_selection(int x, int y);
void gli_notification_waiting(void);

void attrset(attr_t *attr, glui32 style);
void attrclear(attr_t *attr);
bool attrequal(attr_t *a1, attr_t *a2);
unsigned char *attrfg(style_t *styles, attr_t *attr);
unsigned char *attrbg(style_t *styles, attr_t *attr);
int attrfont(style_t *styles, attr_t *attr);

/* A macro which reads and decodes one character of UTF-8. Needs no
   explanation, I'm sure.

   Oh, okay. The character will be written to *chptr (so pass in "&ch",
   where ch is a glui32 variable). eofcond should be a condition to
   evaluate end-of-stream -- true if no more characters are readable.
   nextch is a function which reads the next character; this is invoked
   exactly as many times as necessary.

   val0, val1, val2, val3 should be glui32 scratch variables. The macro
   needs these. Just define them, you don't need to pay attention to them
   otherwise.

   The macro itself evaluates to true if ch was successfully set, or
   false if something went wrong. (Not enough characters, or an
   invalid byte sequence.)

   This is not the worst macro I've ever written, but I forget what the
   other one was.
*/

#define UTF8_DECODE_INLINE(chptr, eofcond, nextch, val0, val1, val2, val3)  ( \
    (eofcond ? 0 : ( \
        (((val0=nextch) < 0x80) ? (*chptr=val0, 1) : ( \
            (eofcond ? 0 : ( \
                (((val1=nextch) & 0xC0) != 0x80) ? 0 : ( \
                    (((val0 & 0xE0) == 0xC0) ? (*chptr=((val0 & 0x1F) << 6) | (val1 & 0x3F), 1) : ( \
                        (eofcond ? 0 : ( \
                            (((val2=nextch) & 0xC0) != 0x80) ? 0 : ( \
                                (((val0 & 0xF0) == 0xE0) ? (*chptr=(((val0 & 0xF)<<12)  & 0x0000F000) | (((val1 & 0x3F)<<6) & 0x00000FC0) | (((val2 & 0x3F))    & 0x0000003F), 1) : ( \
                                    (((val0 & 0xF0) != 0xF0 || eofcond) ? 0 : (\
                                        (((val3=nextch) & 0xC0) != 0x80) ? 0 : (*chptr=(((val0 & 0x7)<<18)   & 0x1C0000) | (((val1 & 0x3F)<<12) & 0x03F000) | (((val2 & 0x3F)<<6)  & 0x000FC0) | (((val3 & 0x3F))     & 0x00003F), 1) \
                                        )) \
                                    )) \
                                )) \
                            )) \
                        )) \
                )) \
            )) \
        )) \
    )

#ifdef __cplusplus
}
#endif

#endif
