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

    Portions of this file are copyright 1998-2004 by Andrew Plotkin.
    You may copy, distribute, and incorporate it into your own programs,
    by any means and under any conditions, as long as you do not modify it.
    You may also modify this file, incorporate it into your own programs,
    and distribute the modified version, as long as you retain a notice
    in your program or documentation which mentions my name and the URL
    shown above.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "glk.h"
#include "garglk.h"

int gli_terminated = 0;

static unsigned char char_tolower_table[256];
static unsigned char char_toupper_table[256];

char gli_program_name[256] = "Unknown";
char gli_program_info[256] = "";
char gli_story_name[256] = "";
char gli_story_title[256] = "";

void garglk_set_program_name(const char *name)
{
    strncpy(gli_program_name, name, sizeof gli_program_name);
    gli_program_name[sizeof gli_program_name-1] = 0;
    wintitle();
}

void garglk_set_program_info(const char *info)
{
    strncpy(gli_program_info, info, sizeof gli_program_info);
    gli_program_info[sizeof gli_program_info-1] = 0;
}

void garglk_set_story_name(const char *name)
{
    strncpy(gli_story_name, name, sizeof gli_story_name);
    gli_story_name[sizeof gli_story_name-1] = 0;
    wintitle();
}

void garglk_set_story_title(const char *title)
{
    strncpy(gli_story_title, title, sizeof gli_story_title);
    gli_story_title[sizeof gli_story_title-1] = 0;
    wintitle();
}

gidispatch_rock_t (*gli_register_obj)(void *obj, glui32 objclass) = NULL;
void (*gli_unregister_obj)(void *obj, glui32 objclass, 
    gidispatch_rock_t objrock) = NULL;
gidispatch_rock_t (*gli_register_arr)(void *array, glui32 len, 
    char *typecode) = NULL;
void (*gli_unregister_arr)(void *array, glui32 len, char *typecode, 
    gidispatch_rock_t objrock) = NULL;

void gli_initialize_misc()
{
    int ix;
    int res;
    
    /* Initialize the to-uppercase and to-lowercase tables. These should
        *not* be localized to a platform-native character set! They are
        intended to work on Latin-1 data, and the code below correctly
        sets up the tables for that character set. */
    
    for (ix=0; ix<256; ix++)
    {
        char_toupper_table[ix] = ix;
        char_tolower_table[ix] = ix;
    }
    for (ix=0; ix<256; ix++)
    {
        if (ix >= 'A' && ix <= 'Z')
            res = ix + ('a' - 'A');
        else if (ix >= 0xC0 && ix <= 0xDE && ix != 0xD7)
            res = ix + 0x20;
        else
            res = 0;

        if (res)
        {
            char_tolower_table[ix] = res;
            char_toupper_table[res] = ix;
        }
    }

}

void glk_exit()
{
    event_t event;

    garglk_set_story_title("[ press any key to exit ]");

    gli_terminated = 1;

    /* wait for gli_handle_input_key to exit() */
    while (1)
        glk_select(&event);
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

void gidispatch_set_object_registry(
    gidispatch_rock_t (*regi)(void *obj, glui32 objclass), 
    void (*unregi)(void *obj, glui32 objclass, gidispatch_rock_t objrock))
{
    window_t *win;
    stream_t *str;
    fileref_t *fref;
    
    gli_register_obj = regi;
    gli_unregister_obj = unregi;
    
    if (gli_register_obj)
    {
        /* It's now necessary to go through all existing objects, and register
            them. */
        for (win = glk_window_iterate(NULL, NULL); 
            win;
            win = glk_window_iterate(win, NULL))
        {
            win->disprock = (*gli_register_obj)(win, gidisp_Class_Window);
        }
        for (str = glk_stream_iterate(NULL, NULL); 
            str;
            str = glk_stream_iterate(str, NULL))
        {
            str->disprock = (*gli_register_obj)(str, gidisp_Class_Stream);
        }
        for (fref = glk_fileref_iterate(NULL, NULL); 
            fref;
            fref = glk_fileref_iterate(fref, NULL))
        {
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
    switch (objclass)
    {
        case gidisp_Class_Window:
            return ((window_t *)obj)->disprock;
        case gidisp_Class_Stream:
            return ((stream_t *)obj)->disprock;
        case gidisp_Class_Fileref:
            return ((fileref_t *)obj)->disprock;
        case gidisp_Class_Schannel:
            return ((channel_t *)obj)->disprock;
        default:
        {
            gidispatch_rock_t dummy;
            dummy.num = 0;
            return dummy;
        }
    }
}
