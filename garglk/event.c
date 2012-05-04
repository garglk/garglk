/******************************************************************************
 *                                                                            *
 * Copyright (C) 2006-2009 by Tor Andersson.                                  *
 * Copyright (C) 2010 by Ben Cressey.                                         *
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
#include <string.h>
#include "glk.h"
#include "garglk.h"

/* A pointer to the place where the pending glk_select() will store its
   event. When not inside a glk_select() call, this will be NULL. */
event_t *gli_curevent = NULL;
static eventqueue_t *gli_events_logged = NULL;
static eventqueue_t *gli_events_polled = NULL;
static int gli_first_event = FALSE;

eventqueue_t *gli_initialize_queue (void)
{
    eventqueue_t *queue = malloc(sizeof(eventqueue_t));

    if (!queue)
        return 0;

    queue->first = NULL;
    queue->last = NULL;
    return queue;
}

void gli_queue_event(eventqueue_t *queue, event_t *msg)
{
    if (queue)
    {
        eventlog_t *log = malloc(sizeof(eventlog_t));

        if (!log)
            return;

        log->event = msg;
        log->next = NULL;

        if (queue->last)
            queue->last->next = log;
        queue->last = log;

        if (!queue->first)
            queue->first = log;
    }
}

event_t *gli_retrieve_event(eventqueue_t *queue)
{
    if (!queue)
        return 0;

    eventlog_t *first = queue->first;
    if (!first)
        return 0;

    event_t *msg = first->event;
    eventlog_t *next = first->next;

    queue->first = next;
    if (!queue->first)
        queue->last = NULL;

    free(first);

    return msg;
}

void gli_dispatch_event(event_t *event, int polled)
{
    event_t *dispatch;

    if (!polled)
    {
        dispatch = gli_retrieve_event(gli_events_logged);
        if (!dispatch)
            dispatch = gli_retrieve_event(gli_events_polled);
    }
    else
    {
        dispatch = gli_retrieve_event(gli_events_polled);
    }

    if (dispatch)
    {
        memcpy(event,dispatch,sizeof(event_t));
        free(dispatch);
    }
}

/* Various modules can call this to indicate that an event has occurred.*/
void gli_event_store(glui32 type, window_t *win, glui32 val1, glui32 val2)
{
    event_t *store = malloc(sizeof(event_t));
    if (!store)
        return;
    store->type = type;
    store->win = win;
    store->val1 = val1;
    store->val2 = val2;

    switch (type)
    {
        case evtype_Arrange:
        case evtype_Redraw:
        case evtype_SoundNotify:
        case evtype_Timer:
            if (!gli_events_polled)
                gli_events_polled = gli_initialize_queue();
            gli_queue_event(gli_events_polled, store);
            break;

        default:
            if (!gli_events_logged)
                gli_events_logged = gli_initialize_queue();
            gli_queue_event(gli_events_logged, store);
            break;
    }
}

void glk_select(event_t *event)
{
    if (!gli_first_event)
    {
        gli_input_guess_focus();
        gli_first_event = TRUE;
    }
    gli_select(event, 0);
}

void glk_select_poll(event_t *event)
{
    if (!gli_first_event)
    {
        gli_input_guess_focus();
        gli_first_event = TRUE;
    }
    gli_select(event, 1);
}

void glk_tick()
{
    /* Do nothing. */
}

/* Handle a keystroke. */
void gli_input_handle_key(glui32 key)
{
    if (gli_more_focus)
    {
        gli_input_more_focus();
    }

    else
    {
        switch (key)
        {
            case keycode_Tab:
                gli_input_next_focus();
                return;
            case keycode_PageUp:
            case keycode_PageDown:
            case keycode_MouseWheelUp:
            case keycode_MouseWheelDown:
                gli_input_scroll_focus();
                break;
            default:
                gli_input_guess_focus();
                break;
        }
    }

    window_t *win = gli_focuswin;

    if (!win)
        return;

    int defer_exit = 0;

    switch (win->type)
    {
        case wintype_TextGrid:
            if (win->char_request || win->char_request_uni)
                gcmd_grid_accept_readchar(win, key);
            else if (win->line_request || win->line_request_uni)
                gcmd_grid_accept_readline(win, key);
            break;
        case wintype_TextBuffer:
            if (win->char_request || win->char_request_uni)
                gcmd_buffer_accept_readchar(win, key);
            else if (win->line_request || win->line_request_uni)
                gcmd_buffer_accept_readline(win, key);
            else if (win->more_request || win->scroll_request)
                defer_exit = gcmd_accept_scroll(win, key);
            break;
    }

    if (gli_terminated && !defer_exit)
    {
        winexit();
    }
}

void gli_input_handle_click(int x, int y)
{
    if (gli_rootwin)
        gli_window_click(gli_rootwin, x, y);
}

/* Pick first window which might want input.
 * This is called after every keystroke.
 */
void gli_input_guess_focus()
{
    window_t *altwin = gli_focuswin;

    do
    {
        if (altwin 
            && (altwin->line_request || altwin->char_request ||
                altwin->line_request_uni || altwin->char_request_uni))
            break;
        altwin = gli_window_iterate_treeorder(altwin);
    }
    while (altwin != gli_focuswin);

    if (gli_focuswin != altwin)
    {
        gli_focuswin = altwin;
        gli_force_redraw = 1;
        gli_windows_redraw();
    }
}

/* Pick first window with buffered output.
 * This is called after printing 'more' prompts.
 */
void gli_input_more_focus()
{
    window_t *altwin = gli_focuswin;

    do
    {
        if (altwin 
            && (altwin->more_request))
            break;
        altwin = gli_window_iterate_treeorder(altwin);
    }
    while (altwin != gli_focuswin);

    gli_focuswin = altwin;
}

/* Pick next window which might want input.
 * This is called after pressing 'tab'.
 */
void gli_input_next_focus()
{
    window_t *altwin = gli_focuswin;

    do
    {
        altwin = gli_window_iterate_treeorder(altwin);
        if (altwin 
            && (altwin->line_request || altwin->char_request ||
                altwin->line_request_uni || altwin->char_request_uni))
            break;
    }
    while (altwin != gli_focuswin);

    if (gli_focuswin != altwin)
    {
        gli_focuswin = altwin;
        gli_force_redraw = 1;
        gli_windows_redraw();
    }
}

/* Pick first window which might want scrolling.
 * This is called after pressing page keys.
 */
void gli_input_scroll_focus()
{
    window_t *altwin = gli_focuswin;

    do
    {
        if (altwin 
            && (altwin->scroll_request))
            break;
        altwin = gli_window_iterate_treeorder(altwin);
    }
    while (altwin != gli_focuswin);

    gli_focuswin = altwin;
}
