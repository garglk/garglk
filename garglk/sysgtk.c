/******************************************************************************
 *                                                                            *
 * Copyright (C) 2006-2009 by Tor Andersson.                                  *
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

/* TODO: add mouse down event */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#include "glk.h"
#include "garglk.h"

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms-compat.h>

static GtkWidget *frame;
static GtkWidget *canvas;
static GdkCursor *gdk_hand;
static GdkCursor *gdk_ibeam;
static GtkIMContext *imcontext;

static GdkDisplay *display;

#define MaxBuffer 1024
static int fileselect = 0;
static char filepath[MaxBuffer];

static int timerid = -1;
static int notifyid = -1;

static volatile int timeouts = 0;
static volatile int notify = 0;

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

static int timeout(void *data)
{
    timeouts = 1;
    return TRUE;
}

void glk_request_timer_events(glui32 millisecs)
{
    if (timerid != -1)
    {
        g_source_remove(timerid);
        timerid = -1;
    }

    if (millisecs)
    {
        timerid = g_timeout_add(millisecs, timeout, NULL);
    }
}

static int do_nothing(void *data)
{
    return TRUE;
}

void gli_notification_waiting(void)
{
    if (notifyid != -1)
    {
        g_source_remove(notifyid);
        notifyid = -1;
    }

    notifyid = g_timeout_add(1, do_nothing, NULL);
    notify = 1;
}

void winabort(const char *fmt, ...)
{
    va_list ap;
    char buf[256];
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    // XXX MessageBoxA(NULL, buf, "Fatal error", MB_ICONERROR);
    fprintf(stderr, "fatal: %s\n", buf);
    fflush(stderr);
    abort();
}

void winexit(void)
{
    exit(0);
}

static void winchoosefile(char *prompt, char *buf, int len, int filter, GtkFileChooserAction action, const char *button)
{
    GtkWidget *filedlog = gtk_file_chooser_dialog_new(prompt, NULL, action,
                                                      "_Cancel", GTK_RESPONSE_CANCEL,
                                                      button, GTK_RESPONSE_ACCEPT,
                                                      (gchar *)NULL);
    char *curdir;

    if (filter != FILTER_ALL)
    {
        /* first filter added becomes the default */
        GtkFileFilter *filefilter = gtk_file_filter_new();
        gtk_file_filter_set_name(filefilter, winfilternames[filter]);
        gtk_file_filter_add_pattern(filefilter, winfilterpatterns[filter]);
        gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(filedlog), filefilter);

        /* need a second filter or the UI widget gets weird */
        GtkFileFilter *allfilter = gtk_file_filter_new();
        gtk_file_filter_set_name(allfilter, winfilternames[FILTER_ALL]);
        gtk_file_filter_add_pattern(allfilter, winfilterpatterns[FILTER_ALL]);
        gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(filedlog), allfilter);
    }

    if (action == GTK_FILE_CHOOSER_ACTION_SAVE)
    {
        gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(filedlog), TRUE);
        char savename[32];
        snprintf(savename, sizeof savename, "Untitled%s", winfilterpatterns[filter]+1);
        gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(filedlog), savename);
    }

    if (strlen(buf))
        gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(filedlog), buf);

    if (fileselect)
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(filedlog), filepath);
    else if (getenv("GAMES"))
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(filedlog), getenv("GAMES"));
    else if (getenv("HOME"))
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(filedlog), getenv("HOME"));

    gint result = gtk_dialog_run(GTK_DIALOG(filedlog));

    if (result == GTK_RESPONSE_ACCEPT)
        snprintf(buf, len, "%s", gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(filedlog)));
    else
        buf[0] = '\0';

    curdir = gtk_file_chooser_get_current_folder(GTK_FILE_CHOOSER(filedlog));
    if (curdir != NULL && strlen(curdir) < sizeof(filepath))
    {
        snprintf(filepath, sizeof filepath, "%s", curdir);
        fileselect = TRUE;
    }

    gtk_widget_destroy(filedlog);
    filedlog = NULL;
}

void winopenfile(char *prompt, char *buf, int len, int filter)
{
    char realprompt[256];
    snprintf(realprompt, sizeof realprompt, "Open: %s", prompt);
    winchoosefile(realprompt, buf, len, filter, GTK_FILE_CHOOSER_ACTION_OPEN, "_Open");
}

void winsavefile(char *prompt, char *buf, int len, int filter)
{
    char realprompt[256];
    snprintf(realprompt, sizeof realprompt, "Save: %s", prompt);
    winchoosefile(realprompt, buf, len, filter, GTK_FILE_CHOOSER_ACTION_SAVE, "_Save");
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
    cliplen = k;
}

static void winclipsend(int source)
{
    if (!cliplen)
        return;

    switch (source)
    {
        case PRIMARY:
            gtk_clipboard_set_text(
                    gtk_clipboard_get(GDK_SELECTION_PRIMARY), cliptext, cliplen);
            gtk_clipboard_store(gtk_clipboard_get(GDK_SELECTION_PRIMARY));
            break;
        case CLIPBOARD:
            gtk_clipboard_set_text(
                    gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), cliptext, cliplen);
            gtk_clipboard_store(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD));
            break;
        default:
            return;
    }
}

static void winclipreceive(int source)
{
    gchar *gptr;
    int glen, i;
    glui32 *rptr;
    glui32 rlen;

    switch(source)
    {
        case PRIMARY:
            gptr = gtk_clipboard_wait_for_text(
                    gtk_clipboard_get(GDK_SELECTION_PRIMARY));
            break;
        case CLIPBOARD:
            gptr = gtk_clipboard_wait_for_text(
                    gtk_clipboard_get(GDK_SELECTION_CLIPBOARD));
            break;
        default: return;
    }

    if (!gptr)
        return;

    glen = strlen(gptr);
    if (!glen)
        return;

    rptr = malloc(sizeof(glui32)*(glen+1));
    rlen = gli_parse_utf8((unsigned char *)gptr, glen, rptr, glen);

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
    g_free(gptr);
}

static void onresize(GtkWidget *widget, GtkAllocation *event, void *data)
{
    int newwid = event->width;
    int newhgt = event->height;

    if (newwid == gli_image_w && newhgt == gli_image_h)
        return;

    gli_image_w = newwid;
    gli_image_h = newhgt;

    gli_resize_mask(gli_image_w, gli_image_h);

    gli_image_s = gli_image_w * 4;
    if (gli_image_rgb)
        free(gli_image_rgb);
    gli_image_rgb = malloc(gli_image_s * gli_image_h);

    gli_force_redraw = 1;

    gli_windows_size_change();
}

static void ondraw(GtkWidget *widget, cairo_t *cr, void *data)
{
    if (!gli_drawselect)
        gli_windows_redraw();
    else
        gli_drawselect = FALSE;

    cairo_surface_t *surf = cairo_image_surface_create_for_data(gli_image_rgb,
                    CAIRO_FORMAT_RGB24, gli_image_w, gli_image_h, gli_image_s);

    cairo_set_source_rgb(cr, 1., 1., 1.);
    cairo_set_source_surface(cr, surf, 0., 0.);
    cairo_paint(cr);

    cairo_surface_destroy(surf);
}

static void onbuttondown(GtkWidget *widget, GdkEventButton *event, void *data)
{
    if (event->button == 1)
        gli_input_handle_click(event->x, event->y);
    else if (event->button == 2)
        winclipreceive(PRIMARY);
}

static void onbuttonup(GtkWidget *widget, GdkEventButton *event, void *data)
{
    if (event->button == 1)
    {
        gli_copyselect = FALSE;
        gdk_window_set_cursor(gtk_widget_get_window(widget), NULL);
        winclipsend(PRIMARY);
    }
}

static void onscroll(GtkWidget *widget, GdkEventScroll *event, void *data)
{
    if (event->direction == GDK_SCROLL_UP)
        gli_input_handle_key(keycode_MouseWheelUp);
    else if (event->direction == GDK_SCROLL_DOWN)
        gli_input_handle_key(keycode_MouseWheelDown);
}

static void onmotion(GtkWidget *widget, GdkEventMotion *event, void *data)
{
    int x,y;

    if (event->is_hint)
    {
        GdkSeat *seat = gdk_display_get_default_seat(display);
        GdkDevice *pointer = gdk_seat_get_pointer(seat);
        gdk_window_get_device_position(gtk_widget_get_window(widget), pointer, &x, &y, NULL);
    }
    else
    {
        x = event->x;
        y = event->y;
    }

    /* hyperlinks and selection */
    if (gli_copyselect)
    {
        gdk_window_set_cursor(gtk_widget_get_window(widget), gdk_ibeam);
        gli_move_selection(x, y);
    }
    else
    {
        if (gli_get_hyperlink(x, y))
            gdk_window_set_cursor(gtk_widget_get_window(widget), gdk_hand);
        else
            gdk_window_set_cursor(gtk_widget_get_window(widget), NULL);
    }
}

static void oninput(GtkIMContext *context, gchar *input, void *data)
{
    glui32 inlen;
    glui32 keybuf[1];

    keybuf[0] = '?';

    inlen = strlen(input);
    if(inlen)
        gli_parse_utf8((unsigned char *)input, inlen, keybuf, 1);

    gli_input_handle_key(keybuf[0]);
}

static void onkeydown(GtkWidget *widget, GdkEventKey *event, void *data)
{
    int key = event->keyval;

    if (event->state & GDK_CONTROL_MASK)
    {

        switch(key)
        {
            case GDK_a: case GDK_A: gli_input_handle_key(keycode_Home); break;
            case GDK_b: case GDK_B: gli_input_handle_key(keycode_Left); break;
            case GDK_c: case GDK_C: winclipsend(CLIPBOARD); break;
            case GDK_d: case GDK_D: gli_input_handle_key(keycode_Erase); break;
            case GDK_e: case GDK_E: gli_input_handle_key(keycode_End); break;
            case GDK_f: case GDK_F: gli_input_handle_key(keycode_Right); break;
            case GDK_n: case GDK_N: gli_input_handle_key(keycode_Down); break;
            case GDK_p: case GDK_P: gli_input_handle_key(keycode_Up); break;
            case GDK_u: case GDK_U: gli_input_handle_key(keycode_Escape); break;
            case GDK_v: case GDK_V: winclipreceive(CLIPBOARD); break;
            case GDK_x: case GDK_X: winclipsend(CLIPBOARD); break;
            case GDK_Left: gli_input_handle_key(keycode_SkipWordLeft); break;
            case GDK_Right: gli_input_handle_key(keycode_SkipWordRight); break;
        }

        return;
    }

    if (!gtk_im_context_filter_keypress(imcontext, event))
    {

        switch (key)
        {
            case GDK_Return: gli_input_handle_key(keycode_Return); break;
            case GDK_BackSpace: gli_input_handle_key(keycode_Delete); break;
            case GDK_Delete: gli_input_handle_key(keycode_Erase); break;
            case GDK_Tab: gli_input_handle_key(keycode_Tab); break;
            case GDK_Prior: gli_input_handle_key(keycode_PageUp); break;
            case GDK_Next: gli_input_handle_key(keycode_PageDown); break;
            case GDK_Home: gli_input_handle_key(keycode_Home); break;
            case GDK_End: gli_input_handle_key(keycode_End); break;
            case GDK_Left: gli_input_handle_key(keycode_Left); break;
            case GDK_Right: gli_input_handle_key(keycode_Right); break;
            case GDK_Up: gli_input_handle_key(keycode_Up); break;
            case GDK_Down: gli_input_handle_key(keycode_Down); break;
            case GDK_Escape: gli_input_handle_key(keycode_Escape); break;
            case GDK_F1: gli_input_handle_key(keycode_Func1); break;
            case GDK_F2: gli_input_handle_key(keycode_Func2); break;
            case GDK_F3: gli_input_handle_key(keycode_Func3); break;
            case GDK_F4: gli_input_handle_key(keycode_Func4); break;
            case GDK_F5: gli_input_handle_key(keycode_Func5); break;
            case GDK_F6: gli_input_handle_key(keycode_Func6); break;
            case GDK_F7: gli_input_handle_key(keycode_Func7); break;
            case GDK_F8: gli_input_handle_key(keycode_Func8); break;
            case GDK_F9: gli_input_handle_key(keycode_Func9); break;
            case GDK_F10: gli_input_handle_key(keycode_Func10); break;
            case GDK_F11: gli_input_handle_key(keycode_Func11); break;
            case GDK_F12: gli_input_handle_key(keycode_Func12); break;
            default:
                if (key >= 32 && key <= 255)
                    gli_input_handle_key(key);
        }

    }
}

static void onkeyup(GtkWidget *widget, GdkEventKey *event, void *data)
{
    int key = event->keyval;
    switch(key)
    {
        case GDK_c:
        case GDK_C:
        case GDK_x:
        case GDK_X:
        case GDK_v:
        case GDK_V:
            if (event->state & GDK_CONTROL_MASK)
                return;
        default:
            break;
    }
    gtk_im_context_filter_keypress(imcontext, event);
}

static void onquit(GtkWidget *widget, void *data)
{
    /* forced exit by wm */
    exit(0);
}

void wininit(int *argc, char **argv)
{
    gtk_init(argc, &argv);

    display = gdk_display_get_default();
    if (display == NULL)
    {
        winabort("Can't open display");
    }

    gdk_hand = gdk_cursor_new_for_display(display, GDK_HAND2);
    gdk_ibeam = gdk_cursor_new_for_display(display, GDK_XTERM);
}

void winopen(void)
{
    GdkGeometry geom;
    int defw;
    int defh;

    geom.min_width  = gli_wmarginx * 2 + gli_cellw * 0;
    geom.min_height = gli_wmarginy * 2 + gli_cellh * 0;
    geom.max_width  = gli_wmarginx * 2 + gli_cellw * 255;
    geom.max_height = gli_wmarginy * 2 + gli_cellh * 250;
    geom.width_inc = gli_cellw;
    geom.height_inc = gli_cellh;

    defw = gli_wmarginx * 2 + gli_cellw * gli_cols;
    defh = gli_wmarginy * 2 + gli_cellh * gli_rows;

    frame = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_can_focus(frame, TRUE);
    gtk_widget_set_events(frame, GDK_BUTTON_PRESS_MASK
                               | GDK_BUTTON_RELEASE_MASK
                               | GDK_POINTER_MOTION_MASK
                               | GDK_POINTER_MOTION_HINT_MASK
                               | GDK_SCROLL_MASK);
    g_signal_connect(frame, "button_press_event",
                       G_CALLBACK(onbuttondown), NULL);
    g_signal_connect(frame, "button_release_event",
                       G_CALLBACK(onbuttonup), NULL);
    g_signal_connect(frame, "scroll_event",
                       G_CALLBACK(onscroll), NULL);
    g_signal_connect(frame, "key_press_event",
                       G_CALLBACK(onkeydown), NULL);
    g_signal_connect(frame, "key_release_event",
                       G_CALLBACK(onkeyup), NULL);
    g_signal_connect(frame, "destroy",
                       G_CALLBACK(onquit), "WM destroy");
    g_signal_connect(frame, "motion_notify_event",
        G_CALLBACK(onmotion), NULL);

    canvas = gtk_drawing_area_new();
    g_signal_connect(canvas, "size_allocate",
                       G_CALLBACK(onresize), NULL);
    g_signal_connect(canvas, "size_request",
                       G_CALLBACK(onresize), NULL);
    g_signal_connect(canvas, "draw",
                       G_CALLBACK(ondraw), NULL);
    gtk_container_add(GTK_CONTAINER(frame), canvas);

    gtk_widget_set_size_request(canvas, defw, defh);

    imcontext = gtk_im_multicontext_new();
    g_signal_connect(imcontext, "commit",
        G_CALLBACK(oninput), NULL);

    if (gli_conf_fullscreen)
    {
        GdkMonitor *monitor;
        GdkRectangle geometry;
        int root_monitor = 0;

        monitor = gdk_display_get_monitor(display, root_monitor);
        if (monitor != NULL)
        {
            gdk_monitor_get_geometry(monitor, &geometry);

            defw = geometry.width;
            defh = geometry.height;
            gtk_window_set_decorated(GTK_WINDOW(frame), FALSE);
            gtk_window_set_position(GTK_WINDOW(frame), GTK_WIN_POS_CENTER);
            gtk_window_fullscreen(GTK_WINDOW(frame));
        }
    }

    wintitle();

    gtk_widget_show(canvas);
    gtk_widget_show(frame);

    gtk_widget_grab_focus(frame);
}

void wintitle(void)
{
    char buf[256];

    if (strlen(gli_story_title))
        snprintf(buf, sizeof buf, "%s", gli_story_title);
    else if (strlen(gli_story_name))
        snprintf(buf, sizeof buf, "%s - %s", gli_story_name, gli_program_name);
    else
        snprintf(buf, sizeof buf, "%s", gli_program_name);
    gtk_window_set_title(GTK_WINDOW(frame), buf);
}

void winrepaint(int x0, int y0, int x1, int y1)
{
    /* and pray that gtk+ is smart enough to coalesce... */
    gtk_widget_queue_draw_area(canvas, x0, y0, x1-x0, y1-y0);
}

void gli_select(event_t *event, int polled)
{
    gli_curevent = event;
    gli_event_clearevent(event);

    while (gtk_events_pending())
        gtk_main_iteration();
    gli_dispatch_event(gli_curevent, polled);

    if (!polled)
    {
        while (gli_curevent->type == evtype_None && !timeouts && !notify)
        {
            gtk_main_iteration();
            gli_dispatch_event(gli_curevent, polled);
        }
    }

    if (gli_curevent->type == evtype_None)
    {
        if  (notify)
        {
                notify = 0;
        }

        if (timeouts)
        {
            gli_event_store(evtype_Timer, NULL, 0, 0);
            gli_dispatch_event(gli_curevent, polled);
            timeouts = 0;
        }
    }

    gli_curevent = NULL;
}

/* monotonic clock for profiling */
void wincounter(glktimeval_t *time)
{
    struct timespec tick;
    clock_gettime(CLOCK_MONOTONIC, &tick);

    time->high_sec = 0;
    time->low_sec  = (unsigned int) tick.tv_sec;
    time->microsec = (unsigned int) tick.tv_nsec / 1000;
}
