/******************************************************************************
 *                                                                            *
 * Copyright (C) 2006-2009 by Tor Andersson, Andrew Plotkin.                  *
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

/* cgmisc.c: Miscellaneous functions for Glk API.
    Designed by Andrew Plotkin <erkyrath@eblong.com>
    http://www.eblong.com/zarf/glk/index.html

    Portions of this file are copyright 1998-2016 by Andrew Plotkin.
    It is distributed under the MIT license; see the "LICENSE" file.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "glk.h"
#include "garglk.h"

#ifdef GARGLK
bool gli_terminated = false;
#endif

static unsigned char char_tolower_table[256];
static unsigned char char_toupper_table[256];

gidispatch_rock_t (*gli_register_obj)(void *obj, glui32 objclass) = NULL;
void (*gli_unregister_obj)(void *obj, glui32 objclass, 
    gidispatch_rock_t objrock) = NULL;
gidispatch_rock_t (*gli_register_arr)(void *array, glui32 len, 
    char *typecode) = NULL;
void (*gli_unregister_arr)(void *array, glui32 len, char *typecode, 
    gidispatch_rock_t objrock) = NULL;

#ifndef GARGLK
/* This is needed to redisplay prompts properly after debug output. 
   Not interesting. */
static int debug_output_counter = 0;

static int perform_debug_command(char *cmd);
#endif

void gli_initialize_misc()
{
    int ix;
    int res;
    
    /* Initialize the to-uppercase and to-lowercase tables. These should
        *not* be localized to a platform-native character set! They are
        intended to work on Latin-1 data, and the code below correctly
        sets up the tables for that character set. */
    
    for (ix=0; ix<256; ix++) {
        char_toupper_table[ix] = ix;
        char_tolower_table[ix] = ix;
    }
    for (ix=0; ix<256; ix++) {
        if (ix >= 'A' && ix <= 'Z') {
            res = ix + ('a' - 'A');
        }
        else if (ix >= 0xC0 && ix <= 0xDE && ix != 0xD7) {
            res = ix + 0x20;
        }
        else {
            res = 0;
        }
        if (res) {
            char_tolower_table[ix] = res;
            char_toupper_table[res] = ix;
        }
    }

}

void glk_exit()
{
#ifdef GARGLK
    event_t event;

    if (!gli_wait_on_quit) {
        winexit();
    }

    garglk_set_story_title("[ press any key to exit ]");

    /* After a game exits, Gargoyle waits for a keypress, since
       otherwise text might get lost. It signals this by changing the
       titlebar (as above). However, in full screen mode, the titlebar
       isn't visible, so after quitting a game, Gargoyle looks like it's
       sitting there doing nothing. On exit, in full screen mode, add a
       "press any key to exit" message to all text buffer windows in the
       hopes that one is visible. In weird cases this won't work (e.g.
       if there is no text buffer window), but the vast majority of the
       time this should work fine. */
    if (garglk::winisfullscreen()) {
        for (auto *win = glk_window_iterate(nullptr, 0); win != nullptr; win = glk_window_iterate(win, 0)) {
            if (win->type == wintype_TextBuffer) {
                glk_set_style_stream(glk_window_get_stream(win), style_Subheader);
                glk_put_string_stream(glk_window_get_stream(win), const_cast<char *>("\n[Press any key to exit]"));
            }
        }
    }

    gli_terminated = true;

    /* wait for gli_handle_input_key to exit() */
    while (true) {
        glk_select(&event);
    }
#else
    if (gli_debugger)
        gidebug_announce_cycle(gidebug_cycle_End);
    exit(0);
#endif
}

void glk_set_interrupt_handler(void (*func)(void))
{
    /* This cheap library doesn't understand interrupts. */
}

unsigned char glk_char_to_lower(unsigned char ch)
{
    return char_tolower_table[ch];
}

unsigned char glk_char_to_upper(unsigned char ch)
{
    return char_toupper_table[ch];
}

#ifndef GARGLK
void glk_select(event_t *event)
{
    window_t *win = gli_window_get();
    
    gli_event_clearevent(event);
    
    if (gli_debugger) {
        gidebug_announce_cycle(gidebug_cycle_InputWait);
        if (debug_output_counter) {
            debug_output_counter = 0;
            printf(">");
        }
    }
    fflush(stdout);

    if (!win || !(win->char_request || win->line_request)) {
        /* No input requests. This is legal, but a pity, because the
            correct behavior is to wait forever. Bye bye. */
        while (1) {
            getchar();
        }
    }
    
    if (win->char_request) {
        char *res;
        char buf[256];
        glui32 kval;
        int len;
        
        /* How cheap are we? We don't want to fiddle with line 
            buffering, so we just accept an entire line (terminated by 
            return) and use the first key. Remember that return has to 
            be turned into a special keycode (and so would other keys,
            if we could recognize them.) */
 
        while (1) {
            /* If debug mode is on, it may capture input, in which case
               we need to loop until real input arrives. */

            res = fgets(buf, 255, stdin);
            if (!res) {
                printf("\n<end of input>\n");
                glk_exit();
            }

            if (gli_debugger) {
                if (buf[0] == '/') {
                    perform_debug_command(buf+1);
                    debug_output_counter = 0;
                    printf(">");
                    continue;
                }
            }

            /* not debug input */
            break;
        }

        if (!gli_utf8input) {
            kval = buf[0];
        }
        else {
            int val;
            val = strlen(buf);
            if (val && (buf[val-1] == '\n' || buf[val-1] == '\r'))
                val--;
            len = gli_parse_utf8((unsigned char *)buf, val, &kval, 1);
            if (!len)
                kval = '\n';
        }

        if (kval == '\r' || kval == '\n') {
            kval = keycode_Return;
        }
        else {
            if (!win->char_request_uni && kval >= 0x100)
                kval = '?';
        }
        
        win->char_request = FALSE;
        event->type = evtype_CharInput;
        event->win = win;
        event->val1 = kval;
        
    }
    else {
        /* line_request */
        char *res;
        char buf[256];
        int val;
        glui32 ix;

        while (1) {
            /* If debug mode is on, it may capture input, in which case
               we need to loop until real input arrives. */

            res = fgets(buf, 255, stdin);
            if (!res) {
                printf("\n<end of input>\n");
                glk_exit();
            }

            if (gli_debugger) {
                if (buf[0] == '/') {
                    perform_debug_command(buf+1);
                    debug_output_counter = 0;
                    printf(">");
                    continue;
                }
            }

            /* not debug input */
            break; 
        }

        val = strlen(buf);
        if (val && (buf[val-1] == '\n' || buf[val-1] == '\r'))
            val--;

        if (!gli_utf8input) {
            if (val > win->linebuflen)
                val = win->linebuflen;
            if (!win->line_request_uni) {
                memcpy(win->linebuf, buf, val);
            }
            else {
                glui32 *destbuf = (glui32 *)win->linebuf;
                for (ix=0; ix<val; ix++)
                    destbuf[ix] = (glui32)(((unsigned char *)buf)[ix]);
            }
        }
        else {
            glui32 ubuf[256];
            val = gli_parse_utf8((unsigned char *)buf, val, ubuf, 256);
            if (val > win->linebuflen)
                val = win->linebuflen;
            if (!win->line_request_uni) {
                unsigned char *destbuf = (unsigned char *)win->linebuf;
                for (ix=0; ix<val; ix++) {
                    glui32 kval = ubuf[ix];
                    if (kval >= 0x100)
                        kval = '?';
                    destbuf[ix] = kval;
                }
            }
            else {
                /* We ought to perform Unicode Normalization Form C here. */
                glui32 *destbuf = (glui32 *)win->linebuf;
                for (ix=0; ix<val; ix++)
                    destbuf[ix] = ubuf[ix];
            }
        }

        if (!win->line_request_uni) {
            if (win->echostr) {
                gli_stream_echo_line(win->echostr, win->linebuf, val);
            }
        }
        else {
            if (win->echostr) {
                gli_stream_echo_line_uni(win->echostr, win->linebuf, val);
            }
        }

        if (gli_unregister_arr) {
            if (!win->line_request_uni)
                (*gli_unregister_arr)(win->linebuf, win->linebuflen, 
                    "&+#!Cn", win->inarrayrock);
            else
                (*gli_unregister_arr)(win->linebuf, win->linebuflen, 
                    "&+#!Iu", win->inarrayrock);
        }

        win->line_request = FALSE;
        win->line_request_uni = FALSE;
        win->linebuf = NULL;
        event->type = evtype_LineInput;
        event->win = win;
        event->val1 = val;
    }

    if (gli_debugger)
        gidebug_announce_cycle(gidebug_cycle_InputAccept);
}

void glk_select_poll(event_t *event)
{
    gli_event_clearevent(event);
    
    /* This only checks for timer events at the moment, and we don't
        support any, so I guess this is a pretty simple function. */
}

void glk_tick()
{
    /* Do nothing. */
}

void glk_request_timer_events(glui32 millisecs)
{
    /* Don't make me laugh. */
}
#endif

void gidispatch_set_object_registry(
    gidispatch_rock_t (*regi)(void *obj, glui32 objclass), 
    void (*unregi)(void *obj, glui32 objclass, gidispatch_rock_t objrock))
{
    window_t *win;
    stream_t *str;
    fileref_t *fref;
    
    gli_register_obj = regi;
    gli_unregister_obj = unregi;
    
    if (gli_register_obj) {
        /* It's now necessary to go through all existing objects, and register
            them. */
        for (win = glk_window_iterate(NULL, NULL); 
            win;
            win = glk_window_iterate(win, NULL)) {
            win->disprock = (*gli_register_obj)(win, gidisp_Class_Window);
        }
        for (str = glk_stream_iterate(NULL, NULL); 
            str;
            str = glk_stream_iterate(str, NULL)) {
            str->disprock = (*gli_register_obj)(str, gidisp_Class_Stream);
        }
        for (fref = glk_fileref_iterate(NULL, NULL); 
            fref;
            fref = glk_fileref_iterate(fref, NULL)) {
            fref->disprock = (*gli_register_obj)(fref, gidisp_Class_Fileref);
        }
    }
}

void gidispatch_set_retained_registry(
    gidispatch_rock_t (*regi)(void *array, glui32 len, char *typecode), 
    void (*unregi)(void *array, glui32 len, char *typecode, 
        gidispatch_rock_t objrock))
{
    gli_register_arr = regi;
    gli_unregister_arr = unregi;
}

gidispatch_rock_t gidispatch_get_objrock(void *obj, glui32 objclass)
{
    switch (objclass) {
        case gidisp_Class_Window:
            return ((window_t *)obj)->disprock;
        case gidisp_Class_Stream:
            return ((stream_t *)obj)->disprock;
        case gidisp_Class_Fileref:
            return ((fileref_t *)obj)->disprock;
#ifdef GARGLK
        case gidisp_Class_Schannel:
            return gli_sound_get_channel_disprock(static_cast<const channel_t *>(obj));
#endif
        default: {
            gidispatch_rock_t dummy;
            dummy.num = 0;
            return dummy;
        }
    }
}

#ifndef GARGLK
void gidispatch_set_autorestore_registry(
    long (*locatearr)(void *array, glui32 len, char *typecode,
        gidispatch_rock_t objrock, int *elemsizeref),
    gidispatch_rock_t (*restorearr)(long bufkey, glui32 len,
        char *typecode, void **arrayref))
{
    /* CheapGlk is not able to serialize its UI state. Therefore, it
       does not have the capability of autosaving and autorestoring.
       Therefore, it will never call these hooks. Therefore, we ignore
       them and do nothing here. */
}

static int perform_debug_command(char *cmd)
{
    char *allocbuf = NULL;
    int res = 1;

    if (!gli_utf8input) {
        /* The string is Latin-1. We should convert to UTF-8. We do
           this in a very lazy way: alloc the largest possible buffer. */
        int len = 4*strlen(cmd)+4;
        allocbuf = malloc(len);
        char *ptr = allocbuf;
        int count = 0;
        while (*cmd) {
            count += gli_encode_utf8(*cmd, ptr+count, len-count);
            cmd++;
        }
        *(ptr+count) = '\0';
        cmd = allocbuf;
    }

    int val = strlen(cmd);
    if (val && (cmd[val-1] == '\n' || cmd[val-1] == '\r')) {
        cmd[val-1] = '\0';
    }

    res = gidebug_perform_command(cmd);

    if (allocbuf)
        free(allocbuf);

    return res;
}

#if GIDEBUG_LIBRARY_SUPPORT

void gidebug_output(char *text)
{
    /* Send a line of text to the "debug console", if the user has
       requested debugging mode. */
    /* (The text is UTF-8 whether or not the library output has requested
       that encoding. The user will just have to cope.) */
    if (gli_debugger) {
        printf("Debug: %s\n", text);
        debug_output_counter++;
    }
}

/* Block and wait for debug commands. The library will accept debug commands
   until gidebug_perform_command() returns nonzero.

   This behaves a lot like glk_select(), except that it only handles debug
   input, not any of the standard event types.
*/
void gidebug_pause()
{
    if (!gli_debugger) {
        return;
    }

    gidebug_announce_cycle(gidebug_cycle_DebugPause);
    debug_output_counter = 0;
    printf(">>");
    fflush(stdout);

    while (1) {
        char buf[256];
        char *res;
        int unpause;

        res = fgets(buf, 255, stdin);
        if (!res) {
            printf("\n<end of input>\n");
            break;
        }

        /* The slash is optional at the beginning of a line grabbed this
           way. */
        if (res[0] == '/')
            res++;

        unpause = perform_debug_command(res);
        if (unpause) {
            debug_output_counter = 0;
            break;
        }

        debug_output_counter = 0;
        printf(">>");
    }

    gidebug_announce_cycle(gidebug_cycle_DebugUnpause);
}

#endif /* GIDEBUG_LIBRARY_SUPPORT */
#endif
