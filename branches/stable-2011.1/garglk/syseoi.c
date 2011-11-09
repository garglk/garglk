/******************************************************************************
 *                                                                            *
 * Copyright (C) 2006-2009 by Tor Andersson.                                  *
 * Copyright (C) 2010 by Ben Cressey, Chris Spiegel.                          *
 * Copyright (C) 2011 by Vaagn Khachatryan.                                   *
 * Virtual keyboard implementation is based on Open Inkpot's libeoi library   *
 *     Copyright (C) 2010 Alexander Kerner <lunohod@openinkpot.org>           *
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

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>

#include <Ecore_Evas.h>
#include <Ecore.h>
#include <Ecore_X.h>
#include <Ecore_File.h>
#include <Edje.h>

#include <libeoi_help.h>
#include <libkeys.h>

#include "glk.h"
#include "garglk.h"

static Ecore_Evas *frame;
static Evas_Object *canvas;
static Ecore_X_Window xwin;
static Ecore_X_Cursor hand_cursor;
static Ecore_X_Cursor ibeam_cursor;

static Eina_Bool poll_event_queue = EINA_FALSE;
static Ecore_Idle_Enterer *idle_enterer = NULL;
static Ecore_Idle_Exiter *idle_exiter = NULL;

#define MaxBuffer 1024
static int fileselect = 0;
static char filepath[MaxBuffer];

static Ecore_Timer *timerid = NULL;
static volatile int timeouts = 0;

/* buffer for clipboard text */
static char *cliptext = NULL;
static int cliplen = 0;
enum clipsource { PRIMARY , CLIPBOARD };

/*describes virtual keyboard layout*/
typedef struct vk_layout_t {
    const char *name;
    const char *sname;
    keys_t *keys;
} vk_layout_t;

/* holds the state of key presses and current keyboard layout */
typedef struct vk_info_t {
    Ecore_Timer *timer;
    keys_t *keys;
    char *last_key;
    bool shift;
    int i;

    Eina_List *layouts;
    Eina_List *curr_layout;
} vk_info_t;

/* callbacks for operations mapped to key presses */
typedef struct {
	char *opname;
	void (*op)(vk_info_t *info);
} _op;

static void cursor_left(vk_info_t *info);
static void cursor_right(vk_info_t *info);
static void history_prev(vk_info_t *info);
static void history_next(vk_info_t *info);
static void page_up(vk_info_t *info);
static void page_down(vk_info_t *info);
static void show_settings(vk_info_t *info);
static void new_line(vk_info_t *info);
static void delete_char(vk_info_t *info);
static void show_help(vk_info_t *info);
static void change_layout(vk_info_t *info);
static void caps_lock(vk_info_t *info);
static void close_settings(vk_info_t *info);
static void quit_app(vk_info_t *info);

static keys_t* keys;
_op operations[] = {
    {"CURSOR_LEFT", cursor_left},
    {"CURSOR_RIGHT", cursor_right},
    {"HISTORY_PREV", history_prev},
    {"HISTORY_NEXT", history_next},
    {"PAGE_UP", page_up},
    {"PAGE_DOWN", page_down},
    {"SETTINGS", show_settings},
    {"RETURN", new_line},
    {"DELETE", delete_char},
    {"HELP", show_help},
    {"CHANGE_LAYOUT", change_layout},
    {"CAPS_LOCK", caps_lock},
    {"CLOSE", close_settings},
    {"QUIT", quit_app},
    {NULL, NULL},
};

static Eina_Bool timeout(void *data)
{
    timeouts = 1;
    return ECORE_CALLBACK_RENEW;
}

void glk_request_timer_events(glui32 millisecs)
{
    if (timerid != NULL)
    {
        ecore_timer_del( timerid );
        timerid = NULL;
    }

    if (millisecs)
    {
        timerid = ecore_timer_add( millisecs / 1000.0, timeout, NULL );
    }
}

void winabort(const char *fmt, ...)
{
    va_list ap;
    char buf[256];
    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    va_end(ap);
    fprintf(stderr, "fatal: %s\n", buf);
    fflush(stderr);
    abort();
}

void winexit(void)
{
    exit(0);
}

/*Main loop will return from idle state if polled. */
static Eina_Bool idle_in_cb( void *data )
{
    if ( poll_event_queue )
        ecore_main_loop_quit();
    return ECORE_CALLBACK_RENEW;
}

static Eina_Bool idle_out_cb( void *data )
{
    ecore_main_loop_quit();
    return ECORE_CALLBACK_RENEW;
}

static void enable_idlers()
{
    if ( idle_enterer == NULL )
        idle_enterer = ecore_idle_enterer_add( idle_in_cb, NULL );
    if ( idle_exiter == NULL )
        idle_exiter = ecore_idle_exiter_add( idle_out_cb, NULL );
}

static void disable_idlers()
{
    if ( idle_enterer != NULL ){
        ecore_idle_enterer_del( idle_enterer );
        idle_enterer = NULL;
    }
    if ( idle_exiter != NULL ){
        ecore_idle_exiter_del( idle_exiter );
        idle_exiter = NULL;
    }
}

/* games are saved/loaded from $(HOME)/.gargoyle/story_name.sav */
static
void winchoosefile(char *prompt, char *buf, int len, int filter, Eina_Bool is_save)
{
    const char *file_name;
    if ( strlen(gli_story_name) )
        file_name = gli_story_name;
    else if ( strlen(gli_story_title) )
        file_name = gli_story_title;
    else
        file_name = gli_program_name;
    
    /*create $(HOME)/.gargoyle folder in case it doesn't exist */
    sprintf(buf, "%s/.gargoyle", getenv("HOME"));
    ecore_file_mkpath(buf);
    
    /*return the full path to the file*/
    sprintf(buf, "%s/.gargoyle/%s.sav", getenv("HOME"), file_name);
}

void winopenfile(char *prompt, char *buf, int len, int filter)
{
    char realprompt[256];
    sprintf(realprompt, "Open: %s", prompt);
    winchoosefile(realprompt, buf, len, filter, EINA_FALSE);
}

void winsavefile(char *prompt, char *buf, int len, int filter)
{
    char realprompt[256];
    sprintf(realprompt, "Save: %s", prompt);
    winchoosefile(realprompt, buf, len, filter, EINA_TRUE);
}

static void cursor_left(vk_info_t *info)   { gli_input_handle_key(keycode_Left); }
static void cursor_right(vk_info_t *info)  { gli_input_handle_key(keycode_Right); }
static void history_prev(vk_info_t *info)  { gli_input_handle_key(keycode_Up); }
static void history_next(vk_info_t *info)  { gli_input_handle_key(keycode_Down); }
static void page_up(vk_info_t *info)       { gli_input_handle_key(keycode_PageUp); }
static void page_down(vk_info_t *info)     { gli_input_handle_key(keycode_PageDown); }
static void new_line(vk_info_t *info)      { gli_input_handle_key(keycode_Return); }
static void delete_char(vk_info_t *info)   { gli_input_handle_key(keycode_Delete); }
static void quit_app(vk_info_t *info)      { exit(0); }

static void show_help(vk_info_t *info)
{
    eoi_help_show( ecore_evas_get( frame ),
                  "gargoyle", "index", "", NULL, NULL);
}

static void show_settings(vk_info_t *info) {}
static void close_settings(vk_info_t *info){}

/*This happens in settings menu upon pressing the Menu key*/
static void change_layout(vk_info_t *info)
{
    info->curr_layout = eina_list_next(info->curr_layout);
    if (!info->curr_layout)
        info->curr_layout = info->layouts;
}

/*This happens in settings menu upon pressing the Zoom key*/
static void caps_lock(vk_info_t *info)
{
    info->shift = !info->shift;
}

static Eina_List *
load_layouts_from_path(Eina_List *layout_list, const char *path1,
                       const char *path2)
{
    char *f, *p;

    char *file = malloc(MaxBuffer);

    Eina_List *l, *ls;

    asprintf(&p, "%s/%s", path1, path2);

    ls = ecore_file_ls(p);
    EINA_LIST_FOREACH(ls, l, f) {
        char *s = strrchr(f, '.');

        if (!s || strcmp(s, ".ini"))
            continue;
		
        *s = '\0';
		
        snprintf(file, MaxBuffer, "%s/%s", path2, f);
		
        free(f);
		
        keys_t *keys = keys_alloc(file);
		
        if (!keys)
			continue;
		
		vk_layout_t *layout = malloc(sizeof(vk_layout_t));

		const char *str;

		str = keys_lookup(keys, "default", "name");
		layout->name = strdup(str);
		str = keys_lookup(keys, "default", "sname");
		layout->sname = strdup(str);

		layout->keys = keys;

		layout_list = eina_list_append(layout_list, layout);
    }
    eina_list_free(ls);

    free(p);
    free(file);

    return layout_list;
}

static const char *
get_action(vk_info_t *info, const char *key)
{
    char *x;

    if (info->shift)
        asprintf(&x, "Shift+%d", info->i + 1);
    else
        asprintf(&x, "%d", info->i + 1);
	vk_layout_t *layout = eina_list_data_get(info->curr_layout);
    const char *action = keys_lookup(layout->keys, key, x);

    free(x);

    return action;
}

static void
send_key(vk_info_t *info)
{
    if (!info->last_key)
        return;
    
    unsigned char *str = (unsigned char *) get_action(info, info->last_key);
    if ( !strcmp(str, "space") ) str = " ";
    
    if ( str != NULL && str[0] >= 32 ){
        glui32 keybuf[1] = {'?'};
        glui32 inlen = strlen( str );
        
        gli_parse_utf8( str, inlen, keybuf, 1 );
        gli_input_handle_key( keybuf[0] );
    }
    
    free(info->last_key);
    info->last_key = NULL;
    info->i = 0;
}

static Eina_Bool
vkbd_timer_cb(void *param)
{
    vk_info_t *info = param;

    info->timer = NULL;

    send_key(info);
    ecore_main_loop_quit();

    return ECORE_CALLBACK_CANCEL;
}

vk_info_t * vkbd_init()
{
    vk_info_t *evk_info = calloc(sizeof(vk_info_t), 1);

    evk_info->keys = keys_alloc("gargoyle");

    evk_info->layouts =
        load_layouts_from_path(evk_info->layouts, "/usr/share/keys",
                               "gargoyle");
    char *user_path;

    asprintf(&user_path, "%s" "/.keys", getenv("HOME")),
        evk_info->layouts =
        load_layouts_from_path(evk_info->layouts, user_path, "gargoyle_custom");
    free(user_path);

    if (!evk_info->layouts)
        return NULL;

    evk_info->curr_layout = evk_info->layouts;
    return evk_info;
}

void winclipstore(glui32 *text, int len)
{
    int i, k;

    i = 0;
    k = 0;

    if (cliptext)
        free(cliptext);

    cliptext = malloc(sizeof(char) * 4 * (len + 1));

    /*convert UTF-32 to UTF-8 */
    while (i < len)
    {
        if (text[i] < 0x80)
        {
            cliptext[k] = text[i];
            k++;
        }
        else if (text[i] < 0x800)
        {
            cliptext[k  ] = (0xC0 | ((text[i] & 0x7C0) >> 6));
            cliptext[k+1] = (0x80 |  (text[i] & 0x03F)      );
            k = k + 2;
        }
        else if (text[i] < 0x10000)
        {
            cliptext[k  ] = (0xE0 | ((text[i] & 0xF000) >> 12));
            cliptext[k+1] = (0x80 | ((text[i] & 0x0FC0) >>  6));
            cliptext[k+2] = (0x80 |  (text[i] & 0x003F)       );
            k = k + 3;
        }
        else if (text[i] < 0x200000)
        {
            cliptext[k  ] = (0xF0 | ((text[i] & 0x1C0000) >> 18));
            cliptext[k+1] = (0x80 | ((text[i] & 0x03F000) >> 12));
            cliptext[k+2] = (0x80 | ((text[i] & 0x000FC0) >>  6));
            cliptext[k+3] = (0x80 |  (text[i] & 0x00003F)       );
            k = k + 4;
        }
        else
        {
            cliptext[k] = '?';
            k++;
        }
        i++;
    }

    /* null-terminated string */
    cliptext[k] = '\0';
    cliplen = k + 1;
}

void winclipsend(int source)
{
    if ( cliplen == 0 )
        return;

    if ( source == PRIMARY )
        ecore_x_selection_primary_set( xwin, cliptext, cliplen );
    else if ( source == CLIPBOARD )
        ecore_x_selection_clipboard_set( xwin, cliptext, cliplen );
}

void winclipreceive(int source)
{
    if ( source == PRIMARY )
        ecore_x_selection_primary_request( xwin, ECORE_X_SELECTION_TARGET_UTF8_STRING );
    else if ( source == CLIPBOARD )
        ecore_x_selection_clipboard_request( xwin, ECORE_X_SELECTION_TARGET_UTF8_STRING );
}

static Eina_Bool selection_notify_handler(void *data, int ev_type, void *ev)
{
    Ecore_X_Event_Selection_Notify *esn = ev;
    
    if ( esn->selection != ECORE_X_SELECTION_CLIPBOARD && esn->selection != ECORE_X_SELECTION_PRIMARY )
        return ECORE_CALLBACK_RENEW;
    if ( strcmp( esn->target, ECORE_X_SELECTION_TARGET_UTF8_STRING ) != 0 )
        return ECORE_CALLBACK_RENEW;
        
    Ecore_X_Selection_Data_Text *data_text = esn->data;
    char *utf8text = data_text->text;
    if ( utf8text == NULL )
        return ECORE_CALLBACK_RENEW;

    int len = strlen( utf8text );
    if ( len == 0 )
        return ECORE_CALLBACK_RENEW;

    glui32 *rptr = malloc( sizeof(glui32) * (len + 1) );
    glui32 rlen = gli_parse_utf8( utf8text, len, rptr, len );

    int i;
    for (i = 0; i < rlen; i++)
    {
        if (rptr[i] == '\0')
            break;
        else if (rptr[i] == '\r' || rptr[i] == '\n')
            continue;
        else if (rptr[i] == '\b' || rptr[i] == '\t')
            continue;
        else if (rptr[i] != 27)
            gli_input_handle_key(rptr[i]);
    }
    free(rptr);
    
    return ECORE_CALLBACK_RENEW;
}

void onresize(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
    int newwid, newhgt;
    evas_object_geometry_get( obj, NULL, NULL, &newwid, &newhgt );
    
    if (newwid == gli_image_w && newhgt == gli_image_h)
        return;
    
    gli_image_w = newwid;
    gli_image_h = newhgt;

    gli_resize_mask(gli_image_w, gli_image_h);

    gli_image_s = gli_image_w; // 1bpp grayscale
    
    //resize the canvas and get its pixels
    evas_object_image_size_set( canvas, gli_image_w, gli_image_h );
    evas_object_image_fill_set( canvas, 0, 0, gli_image_w, gli_image_h );
    gli_image_rgb = evas_object_image_data_get( canvas, EINA_TRUE );
    
    gli_force_redraw = 1;
    
    gli_windows_size_change();
}

static void onexpose(void *data, Evas_Object *o )
{
    if (!gli_drawselect)
        gli_windows_redraw();
    else
        gli_drawselect = FALSE;
}

static void onbuttondown(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
    Evas_Event_Mouse_Down *eemd = (Evas_Event_Mouse_Down *) event_info;
    
    if (eemd->button == 1)
        gli_input_handle_click( eemd->canvas.x, eemd->canvas.y );
    else if (eemd->button == 2)
        winclipreceive(PRIMARY);
}

static void onbuttonup(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
    Evas_Event_Mouse_Up *eemu = (Evas_Event_Mouse_Up *) event_info;
    
    if (eemu->button == 1)
    {
        gli_copyselect = FALSE;
        ecore_x_window_cursor_set( xwin, 0 );
        winclipsend(PRIMARY);
    }
}

static void onscroll(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
    Evas_Event_Mouse_Wheel *eemw = (Evas_Event_Mouse_Wheel *) event_info;
    if ( eemw->direction != 0 ) return; // only up/down scrolling is supported
    
    if ( eemw->z > 0 )
        gli_input_handle_key(keycode_MouseWheelUp);
    else
        gli_input_handle_key(keycode_MouseWheelDown);
}

static void onmotion(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
    Evas_Event_Mouse_Move *eemm = (Evas_Event_Mouse_Move *) event_info;

    /* hyperlinks and selection */
    if (gli_copyselect)
    {
        ecore_x_window_cursor_set( xwin, ibeam_cursor );
        gli_move_selection( eemm->cur.canvas.x, eemm->cur.canvas.y );
    }
    else
    {
        if (gli_get_hyperlink( eemm->cur.canvas.x, eemm->cur.canvas.y ) )
            ecore_x_window_cursor_set( xwin, hand_cursor );
        else
            ecore_x_window_cursor_set( xwin, 0 );
    }
}

static void onkeyup(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
    vk_info_t *info = data;
    Evas_Event_Key_Up *eeku = event_info;

    /* disable the timer for keys 1-9*/
    if (info->timer) {
        ecore_timer_del(info->timer);
        info->timer = NULL;
    }

    /* if there is a pending 1-9 key press that doesn't coincide with the current one, then handle it*/
    if (info->last_key)
        if (strcmp(info->last_key, eeku->keyname))
            send_key(info);
        else /* otherwise cycle to the next letter associated with it*/
            info->i++; /*TODO: make this cycle through options*/
    
    const char* action = keys_lookup_by_event(info->keys, "main", eeku);
    if ( strlen(action) == 0 ){
        /* handle 1-9 keys*/
        action = get_action(info, eeku->keyname);

        if (action)
            info->timer = ecore_timer_add(0.5, &vkbd_timer_cb, info);

        if (!info->last_key || strcmp(info->last_key, eeku->keyname)) {
            free(info->last_key);
            info->last_key = strdup(eeku->keyname);
        }
    }
    else { /* find the operation associated with the key and invoke it*/
        _op *i;
        for(i = operations; i->opname; ++i)
            if(!strcmp(action, i->opname))
                i->op(info);
    }
}

static void onquit( Ecore_Evas *frame )
{
    /* forced exit by wm */
    exit(0);
}

static void onframeresize( Ecore_Evas *frame )
{
    int w, h;
    ecore_evas_geometry_get( frame, NULL, NULL, &w, &h );
    evas_object_resize( canvas, w, h );
}

static Eina_Bool sig_exit_cb(void *data, int ev_type, void *ev)
{
    winexit();
    return ECORE_CALLBACK_RENEW;
}

void wininit(int *argc, char **argv)
{
    if ( ecore_init() == 0 ){
        printf("ERROR: cannot init Ecore.\n");
        exit( EXIT_FAILURE );
    }
    ecore_app_args_set( *argc, (const char **)argv );
    
    if ( ecore_evas_init() == 0 ){
        printf("ERROR: cannot init Evas_Ecore.\n");
        exit( EXIT_FAILURE );
    }
    if ( edje_init() == 0 ){
        printf("ERROR: cannot init Edje.\n");
        exit( EXIT_FAILURE );
    }
    
    enable_idlers();
    ecore_event_handler_add( ECORE_EVENT_SIGNAL_EXIT, sig_exit_cb, NULL );
}

void winopen(void)
{
    int defw;
    int defh;

    defw = gli_wmarginx * 2 + gli_cellw * gli_cols;
    defh = gli_wmarginy * 2 + gli_cellh * gli_rows;
    
    frame = ecore_evas_software_x11_8_new( NULL, 0, 0, 0, defw, defh );
            if ( frame == NULL ){
                fprintf( stderr, "Error creating the main window.\n" );
                exit( EXIT_FAILURE );
            }
    hand_cursor = ecore_x_cursor_shape_get( ECORE_X_CURSOR_HAND2 );
    ibeam_cursor = ecore_x_cursor_shape_get( ECORE_X_CURSOR_XTERM  );

    ecore_evas_borderless_set( frame, 0 );
    ecore_evas_size_base_set( frame, defw, defh );
    ecore_evas_size_min_set( frame,
        gli_wmarginx * 2 + gli_cellw * 0,
        gli_wmarginy * 2 + gli_cellh * 0 );
    ecore_evas_size_max_set( frame,
        gli_wmarginx * 2 + gli_cellw * 255,
        gli_wmarginy * 2 + gli_cellh * 250 );
    ecore_evas_size_step_set( frame, gli_cellw, gli_cellh );
    wintitle();
    ecore_evas_show( frame );
    ecore_evas_callback_delete_request_set( frame, onquit );
    ecore_evas_callback_resize_set( frame, onframeresize );
    
    xwin = ecore_evas_software_x11_window_get( frame );
    ecore_event_handler_add( ECORE_X_EVENT_SELECTION_NOTIFY, selection_notify_handler, NULL );
    
    Evas *evas = ecore_evas_get( frame );
    canvas = evas_object_image_add( evas );
    vk_info_t *info = vkbd_init();
    
    evas_object_image_pixels_get_callback_set( canvas, onexpose, NULL );
    evas_object_event_callback_add( canvas, EVAS_CALLBACK_MOUSE_DOWN, onbuttondown, NULL );
    evas_object_event_callback_add( canvas, EVAS_CALLBACK_MOUSE_UP, onbuttonup, NULL );
    evas_object_event_callback_add( canvas, EVAS_CALLBACK_KEY_UP, onkeyup, info );
    evas_object_event_callback_add( canvas, EVAS_CALLBACK_MOUSE_WHEEL, onscroll, NULL );
    evas_object_event_callback_add( canvas, EVAS_CALLBACK_MOUSE_MOVE, onmotion, NULL );
    evas_object_event_callback_add( canvas, EVAS_CALLBACK_RESIZE, onresize, NULL );
    
    evas_object_resize( canvas, defw, defh );
    evas_object_show( canvas );
    evas_object_focus_set( canvas, EINA_TRUE );
}

void wintitle(void)
{
    char buf[256];

    if (strlen(gli_story_title))
        sprintf(buf, "%s", gli_story_title);
    else if (strlen(gli_story_name))
        sprintf(buf, "%s - %s", gli_story_name, gli_program_name);
    else
        sprintf(buf, "%s", gli_program_name);

    ecore_evas_title_set(frame, buf);
}

void winrepaint(int x0, int y0, int x1, int y1)
{
    evas_object_image_data_update_add(canvas, x0, y0, x1-x0, y1-y0);
    evas_object_image_pixels_dirty_set( canvas, EINA_TRUE );
}

void gli_select(event_t *event, int polled)
{
    gli_curevent = event;
    gli_event_clearevent(event);
    
    poll_event_queue = EINA_TRUE;
    ecore_main_loop_begin(); /*will immediately return from idle state*/
    gli_dispatch_event(gli_curevent, polled);
    
    if (!polled)
    {
        poll_event_queue = EINA_FALSE;
        while (gli_curevent->type == evtype_None && !timeouts)
        {
            ecore_main_loop_begin();
            gli_dispatch_event(gli_curevent, polled);
        }
    }

    if (gli_curevent->type == evtype_None && timeouts)
    {
        gli_event_store(evtype_Timer, NULL, 0, 0);
        gli_dispatch_event(gli_curevent, polled);
        timeouts = 0;
    }

    gli_curevent = NULL;
}

/* monotonic clock time for profiling */
void wincounter(glktimeval_t *time)
{
    time->high_sec = 0;
    time->low_sec  = 0;
    time->microsec = 0;
}
