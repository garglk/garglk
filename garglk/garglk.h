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

#include <stddef.h>

#ifdef GARGLK_USESDL
#include <SDL_timer.h>
#endif

#include "gi_dispa.h"

/* First, we define our own TRUE and FALSE and NULL, because ANSI
 * is a strange world.
 */
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef NULL
#define NULL 0
#endif

/* This macro is called whenever the library code catches an error
 * or illegal operation from the game program.
 */

#define gli_strict_warning(...) do { \
    fputs("Glk library error: ", stderr); \
    fprintf(stderr, __VA_ARGS__); \
    putc('\n', stderr); \
} while(0)

extern int gli_utf8output, gli_utf8input;

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

/* limit number of text rows/columns */
#define MAX_TEXT_COLUMNS 255
#define MAX_TEXT_ROWS 255

#define GLI_SUBPIX 8
#define gli_zoom_int(x) ((x) * gli_zoom + 0.5)
#define gli_unzoom_int(x) ((x) / gli_zoom + 0.5)

extern char gli_program_name[256];
extern char gli_program_info[256];
extern char gli_story_name[256];
extern char gli_story_title[256];
extern int gli_terminated;

extern window_t *gli_rootwin;
extern window_t *gli_focuswin;
extern event_t *gli_curevent;

extern int gli_force_redraw;
extern int gli_more_focus;
extern int gli_cellw;
extern int gli_cellh;

/* Usurp C1 space for ligatures and smart typography glyphs */
#define ENC_LIG_FI 128
#define ENC_LIG_FL 129
#define ENC_LSQUO 130
#define ENC_RSQUO 131
#define ENC_LDQUO 132
#define ENC_RDQUO 133
#define ENC_NDASH 134
#define ENC_MDASH 135
#define ENC_FLOWBREAK 136

/* These are the Unicode versions */
#define UNI_LIG_FI	0xFB01
#define UNI_LIG_FL	0xFB02
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
    int scaled;
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
    int reverse;
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

extern int gli_override_fg_set;
extern int gli_override_bg_set;
extern int gli_override_fg_val;
extern int gli_override_bg_val;
extern int gli_override_reverse;

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

extern int gli_hires;
extern float gli_backingscalefactor;
extern float gli_zoom;

extern int gli_conf_lcd;

extern int gli_conf_graphics;
extern int gli_conf_sound;

extern int gli_conf_fullscreen;

extern int gli_conf_speak;
extern int gli_conf_speak_input;
extern const char *gli_conf_speak_language;

extern int gli_conf_stylehint;
extern int gli_conf_safeclicks;

extern int gli_conf_justify;
extern int gli_conf_quotes;
extern int gli_conf_dashes;
extern int gli_conf_spaces;
extern int gli_conf_caps;

extern int gli_cols;
extern int gli_rows;

extern int gli_conf_lockcols;
extern int gli_conf_lockrows;

extern unsigned char gli_scroll_bg[3];
extern unsigned char gli_scroll_fg[3];
extern int gli_scroll_width;
extern int gli_scroll_width_save;

extern int gli_baseline;
extern int gli_leading;

enum FACES { MONOR, MONOB, MONOI, MONOZ, PROPR, PROPB, PROPI, PROPZ };
enum TYPES { MONOF, PROPF };
enum STYLES { FONTR, FONTB, FONTI, FONTZ };

extern char *gli_conf_propfont;
extern char *gli_conf_propr;
extern char *gli_conf_propb;
extern char *gli_conf_propi;
extern char *gli_conf_propz;

extern char *gli_conf_monofont;
extern char *gli_conf_monor;
extern char *gli_conf_monob;
extern char *gli_conf_monoi;
extern char *gli_conf_monoz;

extern float gli_conf_gamma;
extern float gli_conf_propsize;
extern float gli_conf_monosize;
extern float gli_conf_propaspect;
extern float gli_conf_monoaspect;

extern char *gli_more_prompt;
extern int gli_more_align;
extern int gli_more_font;

extern int gli_forceclick;
extern int gli_copyselect;
extern int gli_drawselect;
extern int gli_claimselect;

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

eventqueue_t *gli_initialize_queue (void);
void gli_queue_event(eventqueue_t *queue, event_t *event);
event_t *gli_retrieve_event(eventqueue_t *queue);
void gli_dispatch_event(event_t *event, int polled);

#define MAGIC_WINDOW_NUM (9876)
#define MAGIC_STREAM_NUM (8769)
#define MAGIC_FILEREF_NUM (7698)

#define strtype_File (1)
#define strtype_Window (2)
#define strtype_Memory (3)

struct glk_stream_struct
{
    glui32 magicnum;
    glui32 rock;

    int type; /* file, window, or memory stream */
    int unicode; /* one-byte or four-byte chars? Not meaningful for windows */

    glui32 readcount, writecount;
    int readable, writable;

    /* for strtype_Window */
    window_t *win;

    /* for strtype_File */
    FILE *file;
    glui32 lastop; /* 0, filemode_Write, or filemode_Read */
    int textfile;

    /* for strtype_Memory */
    void *buf;		/* unsigned char* for latin1, glui32* for unicode */
    void *bufptr;
    void *bufend;
    void *bufeof;
    glui32 buflen;	/* # of bytes for latin1, # of 4-byte words for unicode */
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
    int textmode;

    gidispatch_rock_t disprock;
    fileref_t *next, *prev; /* in the big linked list of filerefs */
};

/*
 * Windows and all that
 */

typedef struct attr_s
{
    unsigned fgset   : 1;
    unsigned bgset   : 1;
    unsigned reverse : 1;
    unsigned         : 1;
    unsigned style   : 4;
    unsigned fgcolor : 24;
    unsigned bgcolor : 24;
    unsigned hyper   : 32;
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

    int line_request;
    int line_request_uni;
    int char_request;
    int char_request_uni;
    int mouse_request;
    int hyper_request;
    int more_request;
    int scroll_request;
    int image_loaded;

    glui32 echo_line_input;
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
    int vertical, backward; /* flags */
    glui32 division; /* winmethod_Fixed or winmethod_Proportional */
    window_t *key; /* NULL or a leaf-descendant (not a Pair) */
    int keydamage; /* used as scratch space in window closing */
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
    int len, newline, dirty, repaint;
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
    int inunicode;
    int inmax;
    long infence;
    long incurs;
    attr_t origattr;
    gidispatch_rock_t inarrayrock;

    glui32 echo_line_input;
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

enum { CHANNEL_IDLE, CHANNEL_SOUND, CHANNEL_MUSIC };

struct glk_schannel_struct
{
    glui32 rock;

    void *sample; /* Mix_Chunk (or FMOD Sound) */
    void *music; /* Mix_Music (or FMOD Music) */
    void *decode; /* Sound_Sample */

    void *sdl_rwops; /* SDL_RWops */
    unsigned char *sdl_memory;
    int sdl_channel;

    int resid; /* for notifies */
    int status;
    int channel;
    int volume;
    glui32 loop;
    int notify;
    int buffered;

#ifdef GARGLK_USESDL
    int paused;

    /* for volume fades */
    int volume_notify;
    int volume_timeout;
    int target_volume;
    double float_volume;
    double volume_delta;
    SDL_TimerID timer;
#endif

    gidispatch_rock_t disprock;
    channel_t *chain_next, *chain_prev;
};

extern void gli_initialize_sound(void);
extern void gli_initialize_tts(void);
extern void gli_tts_speak(const glui32 *buf, size_t len);
extern void gli_tts_flush(void);
extern void gli_tts_purge(void);

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
extern int win_textgrid_unputchar_uni(window_t *win, glui32 ch);
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
extern int win_textbuffer_unputchar_uni(window_t *win, glui32 ch);
extern void win_textbuffer_clear(window_t *win);
extern void win_textbuffer_init_line(window_t *win, char *buf, int maxlen, int initlen);
extern void win_textbuffer_init_line_uni(window_t *win, glui32 *buf, int maxlen, int initlen);
extern void win_textbuffer_cancel_line(window_t *win, event_t *ev);
extern void win_textbuffer_click(window_textbuffer_t *dwin, int x, int y);
extern void gcmd_buffer_accept_readchar(window_t *win, glui32 arg);
extern void gcmd_buffer_accept_readline(window_t *win, glui32 arg);
extern int gcmd_accept_scroll(window_t *win, glui32 arg);

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
extern int gli_window_unput_char_uni(window_t *win, glui32 ch);
extern int gli_window_check_terminator(glui32 ch);
extern void gli_window_refocus(window_t *win);

extern void gli_windows_redraw(void);
extern void gli_windows_size_change(void);
extern void gli_windows_unechostream(stream_t *str);

extern void gli_window_click(window_t *win, int x, int y);

void gli_redraw_rect(int x0, int y0, int x1, int y1);

void gli_input_guess_focus();
void gli_input_more_focus();
void gli_input_next_focus();
void gli_input_scroll_focus();
void gli_input_handle_key(glui32 key);
void gli_input_handle_click(int x, int y);
void gli_input_guess_focus(void);
void gli_input_more_focus(void);
void gli_input_next_focus(void);
void gli_input_scroll_focus(void);
void gli_event_store(glui32 type, window_t *win, glui32 val1, glui32 val2);

extern stream_t *gli_new_stream(glui32 type, int readable, int writable,
    glui32 rock, int unicode);
extern void gli_delete_stream(stream_t *str);
extern stream_t *gli_stream_open_window(window_t *win);
extern strid_t gli_stream_open_pathname(char *pathname, int textmode,
    glui32 rock);
extern void gli_stream_set_current(stream_t *str);
extern void gli_stream_fill_result(stream_t *str,
    stream_result_t *result);
extern void gli_stream_echo_line(stream_t *str, char *buf, glui32 len);
extern void gli_stream_echo_line_uni(stream_t *str, glui32 *buf, glui32 len);
extern void gli_streams_close_all(void);

extern fileref_t *gli_new_fileref(char *filename, glui32 usage,
    glui32 rock);
extern void gli_delete_fileref(fileref_t *fref);

#ifdef BUNDLED_FONTS
void gli_get_builtin_font(int idx, const unsigned char **ptr, unsigned int *len);
#endif

void gli_initialize_fonts(void);
void gli_draw_pixel(int x, int y, unsigned char alpha, unsigned char *rgb);
void gli_draw_pixel_lcd(int x, int y, unsigned char *alpha, unsigned char *rgb);
void gli_draw_clear(unsigned char *rgb);
void gli_draw_rect(int x, int y, int w, int h, unsigned char *rgb);
int gli_draw_string(int x, int y, int f, unsigned char *rgb, unsigned char *text, int len, int spacewidth);
int gli_string_width(int f, unsigned char *text, int len, int spw);
int gli_draw_string_uni(int x, int y, int f, unsigned char *rgb, glui32 *text, int len, int spacewidth);
int gli_string_width_uni(int f, glui32 *text, int len, int spw);
void gli_draw_caret(int x, int y);
void gli_draw_picture(picture_t *pic, int x, int y, int x0, int y0, int x1, int y1);

void gli_startup(int argc, char *argv[]);
void gli_read_config(int argc, char **argv);

rect_t gli_compute_content_box();

extern void gli_select(event_t *event, int polled);

enum FILEFILTERS { FILTER_SAVE, FILTER_TEXT, FILTER_ALL };

void wininit(int *argc, char **argv);
void winopen(void);
void wintitle(void);
void winmore(void);
void winrepaint(int x0, int y0, int x1, int y1);
void winabort(const char *fmt, ...);
void winopenfile(char *prompt, char *buf, int buflen, int filter);
void winsavefile(char *prompt, char *buf, int buflen, int filter);
void winexit(void);
void winclipstore(glui32 *text, int len);
void wincounter(glktimeval_t *time);

void fontreplace(char *font, int type);
void fontload(void);
void fontunload(void);

int giblorb_is_resource_map();
void giblorb_get_resource(glui32 usage, glui32 resnum, FILE **file, long *pos, long *len, glui32 *type);

picture_t *gli_picture_load(unsigned long id);
void gli_picture_store(picture_t *pic);
picture_t *gli_picture_retrieve(unsigned long id, int scaled);
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

glui32 win_graphics_draw_picture(window_graphics_t *cutwin,
  glui32 image, glsi32 xpos, glsi32 ypos,
  int scale, glui32 imagewidth, glui32 imageheight);
void win_graphics_erase_rect(window_graphics_t *cutwin, int whole, glsi32 xpos, glsi32 ypos, glui32 width, glui32 height);
void win_graphics_fill_rect(window_graphics_t *cutwin, glui32 color, glsi32 xpos, glsi32 ypos, glui32 width, glui32 height);
void win_graphics_set_background_color(window_graphics_t *cutwin, glui32 color);

glui32 win_textbuffer_draw_picture(window_textbuffer_t *dwin, glui32 image, glui32 align, glui32 scaled, glui32 width, glui32 height);
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
glui32 gli_parse_utf8(unsigned char *buf, glui32 buflen, glui32 *out, glui32 outlen);

glui32 strlen_uni(glui32 *s);

void gli_put_hyperlink(glui32 linkval, unsigned int x0, unsigned int y0, unsigned int x1, unsigned int y1);
glui32 gli_get_hyperlink(unsigned int x, unsigned int y);
void gli_clear_selection(void);
int gli_check_selection(unsigned int x0, unsigned int y0, unsigned int x1, unsigned int y1);
int gli_get_selection(unsigned int x0, unsigned int y0, unsigned int x1, unsigned int y1, unsigned int *rx0, unsigned int *rx1);
void gli_clipboard_copy(glui32 *buf, int len);
void gli_start_selection(int x, int y);
void gli_resize_mask(unsigned int x, unsigned int y);
void gli_move_selection(int x, int y);
void gli_notification_waiting(void);

void attrset(attr_t *attr, glui32 style);
void attrclear(attr_t *attr);
int attrequal(attr_t *a1, attr_t *a2);
unsigned char *attrfg(style_t *styles, attr_t *attr);
unsigned char *attrbg(style_t *styles, attr_t *attr);
int attrfont(style_t *styles, attr_t *attr);
