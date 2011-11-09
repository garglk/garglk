/* main.c - Frotz V2.40 main function
 *	Copyright (c) 1995-1997 Stefan Jokisch
 *
 * This file is part of Frotz.
 *
 * Frotz is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Frotz is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */

/*
 * This is an interpreter for Infocom V1 to V6 games. It also supports
 * the recently defined V7 and V8 games. Please report bugs to
 *
 *    s.jokisch@avu.de
 *
 */

#include "frotz.h"

#ifndef MSDOS_16BIT
#define cdecl
#endif

extern void interpret (void);
extern void init_memory (void);
extern void init_proc (void);
extern void init_sound (void);
extern void init_text (void);
extern void init_undo (void);
extern void reset_memory (void);

/* Story file name, id number and size */

char *story_name = 0;

enum story story_id = UNKNOWN;
long story_size = 0;

/* Story file header data */

zbyte h_version = 0;
zbyte h_config = 0;
zword h_release = 0;
zword h_resident_size = 0;
zword h_start_pc = 0;
zword h_dictionary = 0;
zword h_objects = 0;
zword h_globals = 0;
zword h_dynamic_size = 0;
zword h_flags = 0;
zbyte h_serial[6] = { 0, 0, 0, 0, 0, 0 };
zword h_abbreviations = 0;
zword h_file_size = 0;
zword h_checksum = 0;
zbyte h_interpreter_number = 0;
zbyte h_interpreter_version = 0;
zbyte h_screen_rows = 0;
zbyte h_screen_cols = 0;
zword h_screen_width = 0;
zword h_screen_height = 0;
zbyte h_font_height = 1;
zbyte h_font_width = 1;
zword h_functions_offset = 0;
zword h_strings_offset = 0;
zbyte h_default_background = 0;
zbyte h_default_foreground = 0;
zword h_terminating_keys = 0;
zword h_line_width = 0;
zbyte h_standard_high = 1;
zbyte h_standard_low = 1;
zword h_alphabet = 0;
zword h_extension_table = 0;
zbyte h_user_name[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

zword hx_table_size = 0;
zword hx_mouse_x = 0;
zword hx_mouse_y = 0;
zword hx_unicode_table = 0;
zword hx_flags = 0;
zword hx_fore_colour = 0;
zword hx_back_colour = 0;

/* Stack data */

zword stack[STACK_SIZE];
zword *sp = 0;
zword *fp = 0;
zword frame_count = 0;

/* IO streams */

bool ostream_screen = TRUE;
bool ostream_script = FALSE;
bool ostream_memory = FALSE;
bool ostream_record = FALSE;
bool istream_replay = FALSE;
bool message = FALSE;

/* Current window and mouse data */

int cwin = 0;
int mwin = 0;

int mouse_y = 0;
int mouse_x = 0;
int menu_selected = 0;

/* Window attributes */

bool enable_wrapping = FALSE;
bool enable_scripting = TRUE;
bool enable_scrolling = FALSE;
bool enable_buffering = FALSE;

/* User options */

int option_attribute_assignment = 0;
int option_attribute_testing = 0;
int option_context_lines = 0;
int option_object_locating = 0;
int option_object_movement = 0;
int option_left_margin = 0;
int option_right_margin = 0;
int option_ignore_errors = 0;
int option_piracy = 0;
int option_undo_slots = MAX_UNDO_SLOTS;
int option_expand_abbreviations = 0;
int option_script_cols = 80;
int option_save_quetzal = 1;
int option_err_report_mode = ERR_DEFAULT_REPORT_MODE;

int option_sound = 1;
char *option_zcode_path;


/* Size of memory to reserve (in bytes) */

long reserve_mem = 0;

/*
 * z_piracy, branch if the story file is a legal copy.
 *
 *	no zargs used
 *
 */

void z_piracy (void)
{

    branch (!option_piracy);

}/* z_piracy */

/*
 * main
 *
 * Prepare and run the game.
 *
 */

#include "glk.h"
#include "glkstart.h"

static int myargc;
static char **myargv;

glkunix_argumentlist_t glkunix_arguments[] =
{
{ "-a", glkunix_arg_NoValue, "-a: watch attribute setting" },
{ "-A", glkunix_arg_NoValue, "-A: watch attribute testing" },
{ "-i", glkunix_arg_NoValue, "-i: ignore fatal errors" },
{ "-o", glkunix_arg_NoValue, "-o: watch object movement" },
{ "-O", glkunix_arg_NoValue, "-O: watch object locating" },
{ "-P", glkunix_arg_NoValue, "-P: alter piracy opcode" },
{ "-Q", glkunix_arg_NoValue, "-Q: use old-style save format" },
{ "-t", glkunix_arg_NoValue, "-t: set Tandy bit" },
{ "-x", glkunix_arg_NoValue, "-x: expand abbreviations g/x/z" },
{ "-s", glkunix_arg_NumberValue, "-s: random number seed value" },
{ "-S", glkunix_arg_NumberValue, "-S: transcript width" },
{ "-u", glkunix_arg_NumberValue, "-u: slots for multiple undo" },
{ "-Z", glkunix_arg_NumberValue, "-Z: error checking mode" },
{ "", glkunix_arg_ValueFollows, "filename: The game file to load." },
{ NULL, glkunix_arg_End, NULL }
};

int glkunix_startup_code(glkunix_startup_t *data)
{
    myargc = data->argc;
    myargv = data->argv;

    os_init_setup ();
    os_process_arguments (myargc, myargv);

    init_buffer ();
    init_err ();
    init_memory ();
    init_proc ();
    init_sound ();
    init_text ();

    os_init_screen ();

    init_undo ();
    z_restart ();
    return TRUE;
}

void glk_main (void)
{
    interpret ();
    reset_memory ();
}

