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

#ifdef HEADER

#ifndef DEBUGGING

#define debug_object(o, t)
#define debug_attrib(a, o)

#endif

typedef enum { CONT_GO, CONT_STEP, CONT_NEXT, CONT_FINISH, CONT_UNTIL, CONT_STEPI, CONT_NEXTI } Cont_type;


#endif

BOOL enter_debugger = FALSE;

#ifdef DEBUGGING

BOOL exit_debugger = FALSE;
infix_file *cur_file;
int cur_line;
int cur_break;
int cur_stack_depth;
int infix_selected_frame;

static Cont_type debug_cont = CONT_GO;
static int step_count = 0;

typedef enum { bp_none, bp_break, bp_write_watch, bp_read_watch, bp_access_watch } bptype;

typedef enum { del, del_at_next_stop, disable, donttouch } bpdisp;

typedef struct breakpoint breakpoint;

struct breakpoint {
  breakpoint *next;

  int number;
  BOOL enabled;

  bptype type;
  bpdisp disposition;

  offset PC;
  int break_frame;  /* If not -1, only break at this depth */

  infix_file *file;
  int line;

  unsigned hit_count;
  unsigned ignore_count; /* Don't break until 0 */

  char *condition;

  char *watch_expression;
  int watch_frame;  /* Frame at which to evaluate watch; -1 if no locals */
  z_typed watch_value;
};


static breakpoint *breaklist;
static int breaknumber = 1;


void set_step(Cont_type t, int count)
{
  debug_cont = t;
  step_count = count;
  exit_debugger = TRUE;

  do_check_watches = TRUE;
  if(debug_cont == CONT_GO && breaklist == NULL)
    do_check_watches = FALSE;
}


int infix_set_break(offset location)
{
  breakpoint newbreak;

  infix_location cur_location;
  
  if(location == 0) {
    infix_print_string("No code at that location.\n");
    return 0;
  }
  
  newbreak.number = breaknumber++;
  newbreak.enabled = TRUE;
  newbreak.type = bp_break;
  newbreak.disposition = donttouch;
  newbreak.PC = location;
  newbreak.break_frame = -1;
  newbreak.hit_count = 0;
  newbreak.ignore_count = 0;
  newbreak.condition = NULL;

  LEadd(breaklist, newbreak);

  infix_print_string("Breakpoint ");
  infix_print_number(newbreak.number);
  infix_print_string(" at ");
  infix_print_number(location);

  if(infix_decode_PC(&cur_location, location)) {
    infix_print_string(": file ");
    infix_print_string(cur_location.file->filename);
    infix_print_string(", line ");
    infix_print_number(cur_location.line_num);
  }
  infix_print_string(".\n");

  do_check_watches = TRUE;
  return newbreak.number;
}

static breakpoint *find_breakpoint(int breaknum)
{
  breakpoint *p;
  LEsearch(breaklist, p, p->number == breaknum);

  if(p)
    return p;

  infix_print_string("No breakpoint number ");
  infix_print_number(breaknum);
  infix_print_string(".\n");
  return NULL;
}


void infix_delete_breakpoint(int breaknum)
{
  breakpoint *p, *t;
  LEsearchremove(breaklist, p, t, p->number == breaknum, n_free(p->condition));
}

void infix_set_cond(int breaknum, const char *condition)
{
  breakpoint *p = find_breakpoint(breaknum);

  if(p) {
    if(p->condition) {
      n_free(p->condition);
    }
    p->condition = n_strdup(condition);
  }
}

void infix_set_ignore(int breaknum, int count)
{
  breakpoint *p = find_breakpoint(breaknum);

  if(p)
    p->ignore_count = count;
}

void infix_set_break_enabled(int breaknum, BOOL enabled)
{
  breakpoint *p = find_breakpoint(breaknum);

  if(p)
    p->enabled = enabled;
}

static void infix_show_break(breakpoint *b)
{
  infix_print_number(b->number);
  infix_print_char(' ');
  infix_print_string("breakpoint");
  infix_print_char(' ');
  infix_print_string("keep");
  infix_print_char(' ');
  if(b->enabled)
    infix_print_char('y');
  else
    infix_print_char('n');
  infix_print_offset(b->PC);
  infix_print_char(' ');
  infix_gprint_loc(0, b->PC);
  infix_print_char('\n');
}

void infix_show_all_breakpoints(void)
{
  breakpoint *p;
  if(!breaklist) {
    infix_print_string("No breakpoints or watchpoints.\n");
  } else {
    for(p = breaklist; p; p=p->next)
      infix_show_break(p);
  }
}

void infix_show_breakpoint(int breaknum)
{
  breakpoint *p = find_breakpoint(breaknum);
  if(p) {
    infix_show_break(p);
  } else {
    infix_print_string("No such breakpoint or watchpoint.\n");
  }
}


typedef struct auto_display auto_display;

struct auto_display {
  auto_display *next;

  int number;
  BOOL enabled;

  char *exp;
};

static auto_display *displist;
static int dispnumber = 1;

int infix_auto_display(const char *expression)
{
  auto_display newdisp;
  newdisp.number = dispnumber++;
  newdisp.enabled = TRUE;
  newdisp.exp = n_strdup(expression);

  LEadd(displist, newdisp);

  return newdisp.number;
}

void perform_displays(void)
{
  auto_display *p;
  for(p = displist; p; p=p->next) {
    if(p->enabled) {
      infix_print_number(p->number);
      infix_print_string(": ");
      infix_print_string(p->exp);
      infix_print_string(" = ");
      infix_display(evaluate_expression(p->exp, infix_selected_frame));
    }
  }
}


static auto_display *find_auto_display(int displaynum)
{
  auto_display *p;
  LEsearch(displist, p, p->number == displaynum);

  if(p)
    return p;

  infix_print_string("No auto-display number ");
  infix_print_number(displaynum);
  infix_print_string(".\n");
  return NULL;  
}


void infix_auto_undisplay(int displaynum)
{
  auto_display *p, *t;
  LEsearchremove(displist, p, t, p->number == displaynum, n_free(p->exp));
}


void infix_set_display_enabled(int displaynum, BOOL enabled)
{
  auto_display *p = find_auto_display(displaynum);

  if(p)
    p->enabled = enabled;
}


const char *debug_decode_number(unsigned number)
{
  const char *name;
  z_typed val;
  val.v = number;

  val.t = Z_OBJECT;
  name = infix_get_name(val);
  
  if(!name) {
    val.t = Z_ROUTINE;
    name = infix_get_name(val);
  }
  if(!name) {
    val.t = Z_STRING;
    name = infix_get_name(val);
  }
  return name;
}


unsigned opcode_counters[OFFSET_END];


void check_watches(void)
{
  /* This function is called after every instruction, and infix_decode_PC is
     relatively expensive, so only get it when we need it */
  infix_location cur_location;
  BOOL found_location = FALSE;

  BOOL is_breakpoint = FALSE;
  int depth;
  BOOL go_debug;
  breakpoint *p;


  switch(debug_cont) {
  case CONT_STEP:
    if(!infix_decode_PC(&cur_location, oldPC))
      break;
    found_location = TRUE;
    if(cur_file != cur_location.file || cur_line != cur_location.line_num)
      go_debug = TRUE;
    break;

  case CONT_STEPI:
    go_debug = TRUE;
    break;

  case CONT_NEXT:
    depth = stack_get_depth();
    if(depth < cur_stack_depth) {
      go_debug = TRUE;
    } else if(cur_stack_depth == depth) {
      if(!infix_decode_PC(&cur_location, oldPC))
	break;
      found_location = TRUE;

      if(cur_file != cur_location.file || cur_line != cur_location.line_num)
	go_debug = TRUE;
    }
    break;

  case CONT_NEXTI:
    depth = stack_get_depth();
    if(depth <= cur_stack_depth)
      go_debug = TRUE;
    break;

  case CONT_FINISH:
    depth = stack_get_depth();
    if(depth < cur_stack_depth)
      go_debug = TRUE;
    break;

  case CONT_UNTIL:
    depth = stack_get_depth();
    if(depth < cur_stack_depth) {
      go_debug = TRUE;
    } else if(cur_stack_depth == depth) {
      if(!infix_decode_PC(&cur_location, oldPC))
	break;
      found_location = TRUE;

      if(cur_file != cur_location.file || cur_line > cur_location.line_num)
	go_debug = TRUE;
    }
    break;

  case CONT_GO:
    go_debug = FALSE;
  }

  if(go_debug && step_count && --step_count)
    go_debug = FALSE;

  for(p = breaklist; p; p=p->next) {
    if(p->enabled) {
      BOOL break_hit = FALSE;
      switch(p->type) {
      case bp_none:
      case bp_read_watch:
	break;
	
      case bp_break:
	break_hit = p->PC == oldPC;
	break;

      case bp_write_watch:
      case bp_access_watch:
	
	;
      }

      if(break_hit) {
	if(p->ignore_count) {
	  p->ignore_count--;
	} else {
	  z_typed foo;
	  if(p->condition)
	    foo = evaluate_expression(p->condition, infix_selected_frame);
	  if(!p->condition || foo.v) {
	    is_breakpoint = TRUE;
	    go_debug = TRUE;
	    
	    p->hit_count++;
	    
	    infix_print_string("Breakpoint ");
	    infix_print_number(p->number);
	    infix_print_string(", ");
	    
	    switch(p->disposition) {
	    case del:
	      infix_delete_breakpoint(p->number);
	      break;
	    
	    case disable:
	      p->enabled = FALSE;
	      break;
	      
	    case del_at_next_stop:
	    case donttouch:
	      ;
	    }
	  }
	}
      }
    }
  }


  if(go_debug || enter_debugger) {
    depth = stack_get_depth();

    enter_debugger = FALSE;

    if(!found_location)
      found_location = infix_decode_PC(&cur_location, oldPC);

    if(found_location) {
      cur_file = cur_location.file;
      cur_line = cur_location.line_num;
    } else {
      cur_file = NULL;
      cur_line = 0;
    }

    if(is_breakpoint || cur_stack_depth != depth) {
      infix_gprint_loc(depth, 0);
    } else {
      infix_file_print_line(cur_file, cur_line);
    }

    infix_selected_frame = cur_stack_depth = depth;

    for(p = breaklist; p; p=p->next) {
      if(p->disposition == del_at_next_stop)
	infix_delete_breakpoint(p->number);
    }

    debug_prompt();
  }

  return;
}


void debug_prompt(void)
{
  char buffer[513];
  exit_debugger = FALSE;
  perform_displays();
  while(!exit_debugger) {
    if(db_prompt)
      infix_print_string(db_prompt);
    else
      infix_print_string("(nitfol) ");
    infix_get_string(buffer, 512);
    process_debug_command(buffer);
  }
}

void infix_select_frame(int num)
{
  if(frame_is_valid(num))
    infix_selected_frame = num;
}

void infix_show_frame(int frame)
{
  infix_print_char('#');
  infix_print_number(frame);
  infix_print_string("  ");
  infix_gprint_loc(frame, 0);
}

void infix_backtrace(int start, int length)
{
  int n;
  for(n = start + length - 1; n >= start; n--) {
    infix_show_frame(n);
  }
}



const char *watchnames[] = {
  "reading object",
  "move to object",
  "moving object"
};




void debug_object(zword objectnum, watchinfo type)
{
  /*n_show_debug(E_OBJECT, watchnames[type], objectnum);*/
}

void debug_attrib(zword attribnum, zword objectnum)
{
  /*n_show_debug(E_OBJECT, "using attribute", attribnum);*/
}



#endif

