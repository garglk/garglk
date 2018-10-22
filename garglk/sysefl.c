/******************************************************************************
 *                                                                            *
 * Copyright (C) 2006-2009 by Tor Andersson.                                  *
 * Copyright (C) 2010 by Ben Cressey, Chris Spiegel.                          *
 * Copyright (C) 2011 by Vaagn Khachatryan.                                   *
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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "glk.h"
#include "garglk.h"

#include <Ecore_Evas.h>
#include <Ecore.h>
#include <Ecore_X.h>
#include <Elementary.h>

static Ecore_Evas *frame;
static Evas_Object *canvas;
static Ecore_X_Window xwin;
static Ecore_X_Cursor hand_cursor;
static Ecore_X_Cursor ibeam_cursor;

static Eina_Bool poll_event_queue = EINA_FALSE;
static Ecore_Idle_Enterer *idle_enterer = NULL;
static Ecore_Idle_Exiter *idle_exiter = NULL;

/*state of control keys*/
static int Control_R = 0;
static int Control_L = 0;
static int Alt_L = 0;
static int Alt_R = 0;

#define MaxBuffer 1024
static int fileselect = 0;
static char filepath[MaxBuffer];

static Ecore_Timer *timerid = NULL;
static volatile int timeouts = 0;

/* buffer for clipboard text */
static char *cliptext = NULL;
static int cliplen = 0;
enum clipsource { PRIMARY , CLIPBOARD };

/* filters for file dialogs */
static char *winfilternames[] =
{
    "Saved game files",
    "Text files",
    "All files",
};

static char *winfilterpatterns[] =
{
    "*.sav",
    "*.txt",
    "*",
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

void gli_notification_waiting(void)
{
    /* stub */
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

static void
my_fileselector_done(void *data, Evas_Object *obj, void *event_info)
{
    const char *selected = event_info;

    char *buf = evas_object_data_get(obj, "buffer");
    if (selected)
        strcpy(buf, selected);
    else
        strcpy(buf, "");

    strcpy( filepath, elm_fileselector_path_get(obj) );
    fileselect = TRUE;
    
    evas_object_del(data); /*close the file selector window*/
    elm_exit();
}

static
void winchoosefile(char *prompt, char *buf, int len, int filter, Eina_Bool is_save)
{
    Evas *evas = NULL;
    if ( frame != NULL ){ //simulate a modal dialog by disabling all events to the main window
        ecore_evas_get( frame );
        disable_idlers();
        evas_event_freeze(evas);
    }
    /*TODO: implement file name filtering */
    Evas_Object *win, *fs, *bg, *vbox, *hbox, *bt;

    win = elm_win_add(NULL, "fileselector", ELM_WIN_BASIC);
    elm_win_title_set(win, prompt);

    bg = elm_bg_add(win);
    elm_win_resize_object_add(win, bg);
    evas_object_size_hint_weight_set(bg, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_show(bg);

    vbox = elm_box_add(win);
    elm_win_resize_object_add(win, vbox);
    evas_object_size_hint_weight_set(vbox, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_show(vbox);

    fs = elm_fileselector_add(win);
    elm_fileselector_is_save_set(fs, is_save);
    elm_fileselector_expandable_set(fs, EINA_FALSE);
    evas_object_size_hint_weight_set(fs, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(fs, EVAS_HINT_FILL, EVAS_HINT_FILL);
    elm_box_pack_end(vbox, fs);
    evas_object_show(fs);
    evas_object_smart_callback_add(fs, "done", my_fileselector_done, win);

    if (strlen(buf))
        elm_fileselector_selected_set( fs, buf );

    if (fileselect)
        elm_fileselector_path_set( fs, filepath );
    else if (getenv("HOME"))
        elm_fileselector_path_set( fs, getenv("HOME") );

    evas_object_resize(win, 400, 350);
    evas_object_show(win);
    
    evas_object_data_set(fs, "buffer", buf);

    elm_run();
    
    if ( evas != NULL ){ //enable events to the main window
        enable_idlers();
        evas_event_thaw(evas);
    }
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

    gli_image_s = gli_image_w * 4; // EFL uses 4 bpp
    
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

static void onkeydown(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
    Evas_Event_Key_Down *eekd = (Evas_Event_Key_Down *) event_info;

    if ( !strcmp( eekd->key, "Control_R" ) ) { Control_R = 1; return; }
    if ( !strcmp( eekd->key, "Control_L" ) ) { Control_L = 1; return; }
    if ( !strcmp( eekd->key, "Alt_R" ) ) { Alt_R = 1; return; }
    if ( !strcmp( eekd->key, "Alt_L" ) ) { Alt_L = 1; return; }

    if ( Control_R || Control_L )
    {
        if      ( !strcmp( eekd->keyname, "a" ) ) gli_input_handle_key(keycode_Home);
        else if ( !strcmp( eekd->keyname, "c" ) ) winclipsend(CLIPBOARD);
        else if ( !strcmp( eekd->keyname, "e" ) ) gli_input_handle_key(keycode_End);
        else if ( !strcmp( eekd->keyname, "u" ) ) gli_input_handle_key(keycode_Escape);
        else if ( !strcmp( eekd->keyname, "v" ) ) winclipreceive(CLIPBOARD);
        else if ( !strcmp( eekd->keyname, "x" ) ) winclipsend(CLIPBOARD);
        else if ( !strcmp( eekd->key, "Left" ) ) gli_input_handle_key(keycode_SkipWordLeft);
        else if ( !strcmp( eekd->key, "Right" ) ) gli_input_handle_key(keycode_SkipWordRight);

        return;
    }
    if ( Alt_R || Alt_R ) return;
    
    if      ( !strcmp( eekd->key, "Return" ) ) gli_input_handle_key(keycode_Return);
    else if ( !strcmp( eekd->key, "BackSpace" ) ) gli_input_handle_key(keycode_Delete);
    else if ( !strcmp( eekd->key, "Delete" ) ) gli_input_handle_key(keycode_Erase);
    else if ( !strcmp( eekd->key, "Tab" ) ) gli_input_handle_key(keycode_Tab);
    else if ( !strcmp( eekd->key, "Prior" ) ) gli_input_handle_key(keycode_PageUp);
    else if ( !strcmp( eekd->key, "Next" ) ) gli_input_handle_key(keycode_PageDown);
    else if ( !strcmp( eekd->key, "Home" ) ) gli_input_handle_key(keycode_Home);
    else if ( !strcmp( eekd->key, "End" ) ) gli_input_handle_key(keycode_End);
    else if ( !strcmp( eekd->key, "Left" ) ) gli_input_handle_key(keycode_Left);
    else if ( !strcmp( eekd->key, "Right" ) ) gli_input_handle_key(keycode_Right);
    else if ( !strcmp( eekd->key, "Up" ) ) gli_input_handle_key(keycode_Up);
    else if ( !strcmp( eekd->key, "Down" ) ) gli_input_handle_key(keycode_Down);
    else if ( !strcmp( eekd->key, "Escape" ) ) gli_input_handle_key(keycode_Escape);
    else if ( !strcmp( eekd->key, "F1" ) ) gli_input_handle_key(keycode_Func1);
    else if ( !strcmp( eekd->key, "F2" ) ) gli_input_handle_key(keycode_Func2);
    else if ( !strcmp( eekd->key, "F3" ) ) gli_input_handle_key(keycode_Func3);
    else if ( !strcmp( eekd->key, "F4" ) ) gli_input_handle_key(keycode_Func4);
    else if ( !strcmp( eekd->key, "F5" ) ) gli_input_handle_key(keycode_Func5);
    else if ( !strcmp( eekd->key, "F6" ) ) gli_input_handle_key(keycode_Func6);
    else if ( !strcmp( eekd->key, "F7" ) ) gli_input_handle_key(keycode_Func7);
    else if ( !strcmp( eekd->key, "F8" ) ) gli_input_handle_key(keycode_Func8);
    else if ( !strcmp( eekd->key, "F9" ) ) gli_input_handle_key(keycode_Func9);
    else if ( !strcmp( eekd->key, "F10" ) ) gli_input_handle_key(keycode_Func10);
    else if ( !strcmp( eekd->key, "F11" ) ) gli_input_handle_key(keycode_Func11);
    else if ( !strcmp( eekd->key, "F12" ) ) gli_input_handle_key(keycode_Func12);
    else {
        unsigned char *str = (unsigned char *) eekd->string;
        
        if ( str != NULL && str[0] >= 32 ){
            glui32 keybuf[1] = {'?'};
            glui32 inlen = strlen( str );
            
            if ( inlen )
                gli_parse_utf8( str, inlen, keybuf, 1 );
            
            gli_input_handle_key( keybuf[0] );
        }
    }
}

static void onkeyup(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
    Evas_Event_Key_Up *eeku = (Evas_Event_Key_Up *) event_info;

    if ( !strcmp( eeku->key, "Control_R" ) ) { Control_R = 0; return; }
    if ( !strcmp( eeku->key, "Control_L" ) ) { Control_L = 0; return; }
    if ( !strcmp( eeku->key, "Alt_R" ) ) { Alt_R = 0; return; }
    if ( !strcmp( eeku->key, "Alt_L" ) ) { Alt_L = 0; return; }
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
    elm_init( *argc, argv );
    
    enable_idlers();
    ecore_event_handler_add( ECORE_EVENT_SIGNAL_EXIT, sig_exit_cb, NULL );
}

void winopen(void)
{
    int defw;
    int defh;

    defw = gli_wmarginx * 2 + gli_cellw * gli_cols;
    defh = gli_wmarginy * 2 + gli_cellh * gli_rows;
    
    frame = ecore_evas_software_x11_new( NULL, 0, 0, 0, defw, defh );
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
    
    evas_object_image_pixels_get_callback_set( canvas, onexpose, NULL );
    evas_object_event_callback_add( canvas, EVAS_CALLBACK_MOUSE_DOWN, onbuttondown, NULL );
    evas_object_event_callback_add( canvas, EVAS_CALLBACK_MOUSE_UP, onbuttonup, NULL );
    evas_object_event_callback_add( canvas, EVAS_CALLBACK_KEY_DOWN, onkeydown, NULL );
    evas_object_event_callback_add( canvas, EVAS_CALLBACK_KEY_UP, onkeyup, NULL );
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
