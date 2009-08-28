/*  Nitfol - z-machine interpreter using Glk for output.
    Copyright (C) 1999  Evin Robertson

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.

    The author can be reached at nitfol@deja.com
*/
#include "nitfol.h"

strid_t current_zfile;
glui32 zfile_offset;

strid_t input_stream1;

strid_t blorb_file;
glui32 imagecount = 0;

int using_infix;

int zversion;
int granularity;
offset rstart;
offset sstart;

/* Note that a lot of these are set in nitfol.opt, so changing the defaults
   here only affects ports without initialization code */

const char *username;
BOOL aye_matey = FALSE;
BOOL do_tandy = FALSE;
BOOL do_spell_correct = TRUE;
BOOL do_expand = TRUE;
BOOL do_automap = TRUE;
BOOL fullname = FALSE;
BOOL quiet = TRUE;
BOOL ignore_errors = FALSE;
BOOL auto_save_undo = TRUE;
BOOL auto_save_undo_char = FALSE;

int interp_num = 2;
char interp_ver = 'N';

zbyte *z_memory;

offset PC;
offset oldPC;

/* I would kind of like to make these local to the opcode, but doing so makes
   a bagillion warning messages about unused operands... */
int numoperands;
zword operand[16];

zword maxobjs;                  /* Maximum number of objects that could fit */
zword object_count = 0;         /* Objects before first property */
zword obj_first_prop_addr = 0;  /* Location of start of first property */
zword obj_last_prop_addr = ZWORD_MASK; /* Location of start of last property */
zword prop_table_start = 0;         /* Beginning of property table */
zword prop_table_end   = ZWORD_MASK;/* End of property table */

offset total_size;
offset dynamic_size;
offset high_mem_mark;
offset game_size;

zword z_checksum;      /* calculated checksum, not header */
zword z_globaltable;
zword z_objecttable;
zword z_propdefaults;
zword z_synonymtable;
zword z_dictionary;
zword z_terminators;
zword z_headerext;

int faked_random_seed = 0; /* If nonzero, use this as a seed instead of time */

BOOL in_timer;           /* True if we're inside a timer routine */
BOOL exit_decoder;       /* To let the decoder know we're done */
zword time_ret;          /* Get the return value back to the timer */
BOOL smart_timed = TRUE; /* redraw the prompt */
BOOL lower_block_quotes; /* put block quotes in lower window */
BOOL read_abort;         /* quickly stop reading */
BOOL has_done_save_undo; /* the game has done a save_undo since last move */
BOOL allow_saveundo = TRUE; /* Otherwise, ignore all @save_undo opcodes */
BOOL allow_output = TRUE; /* Otherwise, ignore all output */

BOOL testing_string = FALSE; /* If we're uncertain this is really a string */
BOOL string_bad = FALSE;     /* If it turns out to not be a bad string */

BOOL do_check_watches = FALSE; /* Preventing check_watches from being
				  pointlessly called is a worthwhile speedup */

BOOL false_undo = FALSE; /* We just did a fake undo */

char *db_prompt;            /* Defaults to "(nitfol) " */
char *search_path;          /* Path in which to look for games */

int automap_size = 12;
glui32 automap_split = winmethod_Above;

int stacklimit = 0;

BOOL enablefont3 = FALSE;        /* Enables font3 -> ascii conversion.
				    Nitfol doesn't claim to support it
				    even if you set this flag.  This
				    messes SameGame.z5 up, which won't
				    switch back to font1. */

