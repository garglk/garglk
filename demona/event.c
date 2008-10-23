#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "glk.h"
#include "garglk.h"

/* A pointer to the place where the pending glk_select() will store its
   event. When not inside a glk_select() call, this will be NULL. */
event_t *gli_curevent = NULL; 

/* Various modules can call this to indicate that an event has occurred.
   This doesn't try to queue events, but since a single keystroke or
   idle event can only cause one event at most, this is fine. */
void gli_event_store(glui32 type, window_t *win, glui32 val1, glui32 val2)
{
    if (gli_curevent) {
	gli_curevent->type = type;
	gli_curevent->win = win;
	gli_curevent->val1 = val1;
	gli_curevent->val2 = val2;
    }
}

void glk_select(event_t *event)
{
    gli_select(event, 1);
}

void glk_select_poll(event_t *event)
{
    puts("glk_select_poll");
    gli_select(event, 0);
}

void glk_tick()
{
    /* Do nothing. */
}

/* Handle a keystroke.
   This is called from glk_select() whenever a key is hit. */
void gli_input_handle_key(glui32 key)
{
    window_t *win = gli_focuswin;

    if (gli_terminated)
    {
	exit(0);
    }

    if (key == keycode_Tab)
	gli_input_next_focus();

    win = gli_focuswin;

    if (!win)
	return;

    switch (win->type)
    {
    case wintype_TextGrid:
		if (win->char_request || win->char_request_uni)
			gcmd_grid_accept_readchar(win, key);
		if (win->line_request || win->line_request_uni)
			gcmd_grid_accept_readline(win, key);
	break;
    case wintype_TextBuffer:
		if (win->char_request || win->char_request_uni)
		    gcmd_buffer_accept_readchar(win, key);
		if (win->line_request || win->line_request_uni)
		    gcmd_buffer_accept_readline(win, key);
	break;
    }
}

void gli_input_handle_click(int x, int y)
{
    if (gli_rootwin)
	gli_window_click(gli_rootwin, x, y);
}

/* Pick a window which might want input. This is called at the beginning
   of glk_select(). */
void gli_input_guess_focus()
{
    window_t *altwin;

    if (gli_focuswin 
	    && (gli_focuswin->line_request || gli_focuswin->char_request ||
			gli_focuswin->line_request_uni || gli_focuswin->char_request_uni)) {
	return;
    }

    altwin = gli_focuswin;
    do {
	altwin = gli_window_iterate_treeorder(altwin);
	if (altwin 
		&& (altwin->line_request || altwin->char_request ||
			altwin->line_request_uni || altwin->char_request_uni)) {
	    break;
	}
    } while (altwin != gli_focuswin);

    if (gli_focuswin != altwin)
    {
	gli_focuswin = altwin;
	gli_force_redraw = 1;
	gli_windows_redraw();
    }
}

/* Pick next window which might want input.
 * This is called when the users hits 'tab'.
 */
void gli_input_next_focus()
{
    window_t *altwin;

    altwin = gli_focuswin;
    do {
	altwin = gli_window_iterate_treeorder(altwin);
	if (altwin 
		&& (altwin->line_request || altwin->char_request ||
			altwin->line_request_uni || altwin->char_request_uni)) {
	    break;
	}
    } while (altwin != gli_focuswin);

    if (gli_focuswin != altwin)
    {
	gli_focuswin = altwin;
	gli_force_redraw = 1;
	gli_windows_redraw();
    }
}


