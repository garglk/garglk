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

#include <algorithm>
#include <list>

#include "glk.h"
#include "garglk.h"

static std::list<event_t> gli_events;

static void gli_input_guess_focus();
static void gli_input_more_focus();
static void gli_input_next_focus();
static void gli_input_scroll_focus();

void gli_dispatch_event(event_t *event, bool polled)
{
    std::list<event_t>::iterator it;

    if (!polled)
    {
        it = gli_events.begin();
    }
    else
    {
        // §4: glk_select_poll() returns internally-spawned events only,
        // so find the first such event in the list.
        it = std::find_if(gli_events.begin(), gli_events.end(), [](const auto &msg) {
            return msg.type == evtype_Arrange ||
                   msg.type == evtype_Redraw ||
                   msg.type == evtype_SoundNotify ||
                   msg.type == evtype_Timer;
        });
    }

    if (it != gli_events.end())
    {
        *event = *it;
        gli_events.erase(it);
    }
}

/* Various modules can call this to indicate that an event has occurred.*/
void gli_event_store(glui32 type, window_t *win, glui32 val1, glui32 val2)
{
    event_t store{};

    store.type = type;
    store.win = win;
    store.val1 = val1;
    store.val2 = val2;

    gli_events.push_back(store);
}

static void gli_select_or_poll(event_t *event, bool polled)
{
    static bool first_event = false;
    if (!first_event)
    {
        gli_input_guess_focus();
        first_event = true;
    }

    gli_select(event, polled);
}

void glk_select(event_t *event)
{
    gli_select_or_poll(event, false);
}

void glk_select_poll(event_t *event)
{
    gli_select_or_poll(event, true);
}

void glk_tick()
{
#ifdef GARGLK_CONFIG_TICK
    gli_tick();
#endif
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
    {
        if (gli_terminated)
            winexit();

        return;
    }

    bool defer_exit = false;

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
static void gli_input_guess_focus()
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
        gli_force_redraw = true;
        gli_windows_redraw();
    }
}

/* Pick first window with buffered output.
 * This is called after printing 'more' prompts.
 */
static void gli_input_more_focus()
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
static void gli_input_next_focus()
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
        gli_force_redraw = true;
        gli_windows_redraw();
    }
}

/* Pick first window which might want scrolling.
 * This is called after pressing page keys.
 */
static void gli_input_scroll_focus()
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
