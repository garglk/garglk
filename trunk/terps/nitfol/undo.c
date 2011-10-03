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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

    The author can be reached at nitfol@deja.com
*/
#include "nitfol.h"


/* Early versions just used quetzal format for the undo slot, which let
   me just call the same function, but made it impossible to add undo
   capability to games which don't natively support it, and was too expensive
   to provide infinite undo/redo.

   Now I use a linked list containing a memory delta and stack stored in
   quetzal format.  We take the delta between consecutive turns. This makes
   things smaller as there should be fewer differences between turns than
   from beginning to end of a story.

   Since there is very little difference between turns, I use quetzal
   format for the delta with a twist.  If there are runs of more than
   127 unchanged bytes, I store the length in a 15 bit number, ala UTF-8.
   This saves over a hundred bytes each turn (depending on game size and
   arrangement).

   Here is some data for advent (note that the usages are mostly for the
   the previous turn, since the save_undo is called shortly after each input):

Command:                    Overhead Dynamic  Stack  Total
                                     Memory
enter                          28  +   20  +  172  =  220
take all                       28  +  435  +  172  =  635
x stream                       28  +  266  +  172  =  466
exit                           28  +  132  +  172  =  332
s                              28  +  120  +  172  =  320
verbose                        28  +  114  +  172  =  314
d                              28  +   64  +  172  =  264
s                              28  +   90  +  172  =  290
unlock grate with keys         28  +  127  +  172  =  327
open grate. down.              28  +  202  +  172  =  402
help                           28  +  199  +  172  =  399

   Overhead doesn't count alignment and malloc overhead. Assume another 20
   bytes there.

   At approx 380 bytes per turn, I can store over 2500 turns in a meg of
   memory, enough for a longish game.

   Note that stack size (and contents mostly) stay the same between calls
   since save_undo is almost always called from the same place.  I should take
   advantage of this somehow.  (another hundred bytes to save, another 1000
   turns per meg...)

*/


static zbyte *prevstate = NULL;

typedef struct move_difference move_difference;

struct move_difference {
  move_difference *next;

  zbyte *delta;       /* Encoded like quetzal mixed with UTF-8 */
  glui32 deltalength;

  offset PC;
  offset oldPC;
  BOOL PC_in_instruction;

  zbyte *stackchunk;  /* Quetzal encoded */
  glui32 stacklength;
};

static move_difference *movelist = NULL;
static int move_index;


void init_undo(void)
{
  kill_undo();

  prevstate = (zbyte *) n_malloc(dynamic_size);
  n_memcpy(prevstate, z_memory, dynamic_size);
}

/* Frees old undo slots if possible in order to reduce memory consumption.
   Will never free the most recent @save_undo */
BOOL free_undo(void)
{
  move_difference *p, *g = NULL;
  for(p = movelist; p; p=p->next)
    if(p->next)
      g = p;
  if(g == NULL)
    return FALSE;

  n_free(g->next->delta);
  n_free(g->next->stackchunk);
  n_free(g->next);
  g->next = NULL;
  return TRUE;
}


BOOL saveundo(BOOL in_instruction)
{
  move_difference newdiff;
  strid_t stack;
  stream_result_t poo;

  if(!allow_saveundo)
    return TRUE;

  /* In games which provide @save_undo, we will have already issued a faked
     saveundo before the first @save_undo hits, since there hadn't been any
     @save_undo before the first read line.  So when this happens, wipe the
     fake saveundo in favor of the real one */
  if(in_instruction && movelist && !movelist->next
     && !movelist->PC_in_instruction)
    init_undo();

    
  if(!quetzal_diff(z_memory, prevstate, dynamic_size, &newdiff.delta,
		   &newdiff.deltalength, TRUE))
    return FALSE;

#ifdef PARANOID
  {
    char *newmem = (char *) n_malloc(dynamic_size);
    n_memcpy(newmem, prevstate, dynamic_size);
    quetzal_undiff(newmem, dynamic_size, newdiff.delta,
		   newdiff.deltalength, TRUE);
    if(n_memcmp(z_memory, newmem, dynamic_size)) {
      n_show_error(E_SAVE, "save doesn't match itself", 0);
    }
    n_free(newmem);
  }
#endif
  
  newdiff.PC = PC;
  newdiff.oldPC = oldPC;
  
  newdiff.PC_in_instruction = in_instruction;
  newdiff.stacklength = get_quetzal_stack_size();
  newdiff.stackchunk = (zbyte *) n_malloc(newdiff.stacklength);
  stack = glk_stream_open_memory((char *) newdiff.stackchunk,
				 newdiff.stacklength, filemode_Write, 0);
  if(!stack) {
    n_free(newdiff.delta);
    n_free(newdiff.stackchunk);
    return FALSE;
  }
  if(!quetzal_stack_save(stack)) {
    glk_stream_close(stack, NULL);
    n_free(newdiff.delta);
    n_free(newdiff.stackchunk);
    return FALSE;
  }
  glk_stream_close(stack, &poo);
  if(poo.writecount != newdiff.stacklength) {
    n_show_error(E_SAVE, "incorrect stack size assessment", poo.writecount);
    n_free(newdiff.delta);
    n_free(newdiff.stackchunk);
    return FALSE;
  }

  while(move_index-- > 0) {
    n_free(movelist->delta);
    n_free(movelist->stackchunk);
    LEremove(movelist);
  }
  LEadd(movelist, newdiff);
  move_index++;
  n_memcpy(prevstate, z_memory, dynamic_size);

  has_done_save_undo = TRUE;
  return TRUE;
}


BOOL restoreundo(void)
{
  strid_t stack;
  int i;
  glui32 wid, hei;
  move_difference *p = movelist;

  if(!p || move_index < 0)
    return FALSE;
  
  for(i = 0; i < move_index; i++) {
    p=p->next;
    if(!p)
      return FALSE;
  }
  move_index++;

  n_memcpy(z_memory, prevstate, dynamic_size);

  quetzal_undiff(prevstate, dynamic_size, p->delta, p->deltalength, TRUE);
  
  stack = glk_stream_open_memory((char *) p->stackchunk, p->stacklength,
				 filemode_Read, 0);

  quetzal_stack_restore(stack, p->stacklength);
  glk_stream_close(stack, NULL);

  if(p->PC_in_instruction) {
    PC = p->PC;
    mop_store_result(2);
    false_undo = FALSE;
  } else {
    PC = p->oldPC;
    false_undo = TRUE;
  }
  has_done_save_undo = TRUE;

  z_find_size(&wid, &hei);
  set_header(wid, hei);
  return TRUE;
}


/* Just like restoreundo, but the opposite ;) 
   The idea is to go to the @save_undo location, but return 0 instead of 2
   so the game thinks it just successfully saved the game. For games which
   don't contain @save_undo, jumps to right after the @read instruction. */
BOOL restoreredo(void)
{
  strid_t stack;
  int i;
  glui32 wid, hei;
  stream_result_t poo;
  move_difference *p = movelist;

  if(!p || move_index <= 0)
    return FALSE;
  
  move_index--;
  for(i = 0; i < move_index; i++) {
    p=p->next;
    if(!p)
      return FALSE;
  }

  quetzal_undiff(prevstate, dynamic_size, p->delta, p->deltalength, TRUE);
  
  n_memcpy(z_memory, prevstate, dynamic_size);

  stack = glk_stream_open_memory((char *) p->stackchunk, p->stacklength,
				 filemode_Read, 0);

  quetzal_stack_restore(stack, p->stacklength);
  glk_stream_close(stack, &poo);

  if(poo.readcount != p->stacklength) {
    n_show_error(E_SAVE, "incorrect stack size assessment", poo.readcount);
    return FALSE;
  }

  if(p->PC_in_instruction) {
    PC = p->PC;
    mop_store_result(0);
    false_undo = FALSE;
  } else {
    PC = p->PC;
    false_undo = FALSE;
  }
  has_done_save_undo = TRUE;

  z_find_size(&wid, &hei);
  set_header(wid, hei);
  return TRUE;
}


#ifdef DEBUGGING

/* For automapping, we want the saveundo/restoreundo to be as fast as possible
   and we also don't want it clobbering the redo capabilities, so give it
   separate fast_saveundo and fast_restoreundo.  Doesn't need multiple undo
   or redo
*/

struct fast_undoslot {
  zbyte *z_mem;
  glui32 z_memsize;
  offset PC;
  zbyte *stackchunk;  /* Quetzal encoded */
  glui32 stackchunksize;
  glui32 stacklength;
};


static struct fast_undoslot automap_undoslot = { NULL, 0, 0, NULL, 0, 0 };

BOOL fast_saveundo(void)
{
  strid_t stack;
  glui32 stack_size;
  /* Avoids realloc() in hopes that'll make it a teensy bit faster */
  if(automap_undoslot.z_memsize < dynamic_size) {
    n_free(automap_undoslot.z_mem);
    automap_undoslot.z_mem = n_malloc(dynamic_size);
    automap_undoslot.z_memsize = dynamic_size;
  }
  n_memcpy(automap_undoslot.z_mem, z_memory, dynamic_size);

  automap_undoslot.PC = oldPC;

  automap_undoslot.stacklength = stack_size = get_quetzal_stack_size();
  if(automap_undoslot.stackchunksize < stack_size) {
    free(automap_undoslot.stackchunk);
    automap_undoslot.stackchunk = (zbyte *) n_malloc(stack_size);
    automap_undoslot.stackchunksize = stack_size;
  }
  
  stack = glk_stream_open_memory((char *) automap_undoslot.stackchunk,
				 automap_undoslot.stacklength,
				 filemode_Write, 0);
  if(!stack)
    return FALSE;
  if(!quetzal_stack_save(stack)) {
    glk_stream_close(stack, NULL);
    return FALSE;
  }
  glk_stream_close(stack, NULL);
  return TRUE;
}


BOOL fast_restoreundo(void)
{
  strid_t stack;
  n_memcpy(z_memory, automap_undoslot.z_mem, dynamic_size);
  PC = automap_undoslot.PC;

  stack = glk_stream_open_memory((char *) automap_undoslot.stackchunk,
				 automap_undoslot.stacklength,
				 filemode_Read, 0);

  quetzal_stack_restore(stack, automap_undoslot.stacklength);
  glk_stream_close(stack, NULL);
  return TRUE;
}

#endif


void kill_undo(void)
{
  n_free(prevstate);
  prevstate = 0;

  while(movelist) {
    n_free(movelist->delta);
    n_free(movelist->stackchunk);
    LEremove(movelist);
  }
  move_index = 0;

#ifdef DEBUGGING
  n_free(automap_undoslot.z_mem);
  n_free(automap_undoslot.stackchunk);
  automap_undoslot.z_mem = NULL;
  automap_undoslot.z_memsize = 0;
  automap_undoslot.PC = 0;
  automap_undoslot.stackchunk = NULL;
  automap_undoslot.stackchunksize = 0;
  automap_undoslot.stacklength = 0;
#endif
}
