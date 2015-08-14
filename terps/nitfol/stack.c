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

/* Contains everything which references the stack directly. */

/* Uses two stacks because I don't want to worry about alignment issues */

typedef struct Stack_frame Stack_frame;

struct Stack_frame {
  offset stack_stack_start;  /* points into stack_stack to local variables
				for this frame, which are followed by
				stack for pushing/popping */
  offset return_PC;
  int num_locals;
  zword arguments;           /* Number of arguments used for check_count */
  int result_variable;       /* Where to place the result upon returning */
};

static zword *stack_stack = NULL; /* Holds local variables and pushed values */
static offset stack_pointer;      /* Current offset into stack_stack */
static offset stack_min;  /* Minimum for stack_pointer (how much we can pop) */
static offset stack_max;  /* Maximum for stack_pointer (size of stack) */
static zword *local_vars; /* Pointer to local variables for current frame */

static Stack_frame *stack_frames = NULL;
static zword frame_count;    /* number of frames on the stack */
static zword frame_max;


void init_stack(offset initialstack_stack_size, zword initialframe_size)
{
  n_free(stack_stack);
  stack_stack = (zword *) n_malloc(sizeof(*stack_stack) * 
				   initialstack_stack_size);
  stack_pointer = 0;
  stack_min = 0;
  stack_max = initialstack_stack_size;

  n_free(stack_frames);
  stack_frames = (Stack_frame *) n_malloc(sizeof(*stack_frames) *
					  initialframe_size);
  frame_count = 0;
  if(stacklimit && initialframe_size > stacklimit)
    frame_max = stacklimit;
  else
    frame_max = initialframe_size;

  stack_frames[frame_count].stack_stack_start = 0;
  stack_frames[frame_count].return_PC = 0;
  stack_frames[frame_count].num_locals = 0;
  stack_frames[frame_count].arguments = 0;
  stack_frames[frame_count].result_variable = -2;
  local_vars = stack_stack + stack_frames[frame_count].stack_stack_start;
}

void kill_stack(void)
{
  n_free(stack_stack);
  n_free(stack_frames);
  stack_stack = 0;
  stack_frames = 0;
}

/* Perform various sanity checks on the stack to make sure all is well in
   Denmark. */
BOOL verify_stack(void)
{
  zword f;
  if(frame_count > frame_max) {
    n_show_error(E_STACK, "more frames than max", frame_count);
    return FALSE;
  }
  if(!stack_frames) {
    n_show_error(E_STACK, "no frames", 0);
    return FALSE;
  }
  for(f = 0; f < frame_count; f++) {
    if(stack_frames[f].stack_stack_start > stack_pointer) {
      n_show_error(E_STACK, "stack start after end", f);
      return FALSE;
    }
    if(stack_frames[f].return_PC > total_size) {
      n_show_error(E_STACK, "PC after end of game", f);
      return FALSE;
    }
    if(stack_frames[f].num_locals > 15) {
      n_show_error(E_STACK, "too many locals", f);
      return FALSE;
    }
    if(stack_frames[f].arguments > 7) {
      n_show_error(E_STACK, "too many arguments", f);
      return FALSE;
    }
    if(stack_frames[f].result_variable > 255) {
      n_show_error(E_STACK, "result variable too big", f);
      return FALSE;
    }
    if(stack_frames[f].result_variable < -2) {
      n_show_error(E_STACK, "unknown magic result variable", f);
      return FALSE;
    }
  }
  return TRUE;
}


/* Insure we have at least addsize more zwords available on the stack, and
 * if not, allocate more space
 */
static void check_stack_stack(offset addsize)
{
  if(stack_pointer + addsize >= stack_max) {
    stack_max *= 2;
    stack_stack = (zword *) n_realloc(stack_stack, 
				      sizeof(*stack_stack) * stack_max);

    n_show_port(E_STACK, "stack larger than available on some interps", stack_max);

    local_vars = stack_stack + stack_frames[frame_count].stack_stack_start;
  }
}


void add_stack_frame(offset return_PC, unsigned num_locals, zword *locals,
		     unsigned num_args, int result_var)
{
  unsigned n;
  /* Don't increment the frame yet because we have error messages yet to
     show which need to receive a valid frame to output local variables */
  if(frame_count+1 >= frame_max) {
    frame_max *= 2;
    if(stacklimit && frame_max > stacklimit) {
      frame_max = stacklimit;
      if(frame_count+1 >= frame_max) {
	n_show_fatal(E_STACK, "recursed deeper than allowed", frame_count+1);
      }
    }
    stack_frames = (Stack_frame *)
                   n_realloc(stack_frames, sizeof(*stack_frames) * frame_max);
    n_show_port(E_STACK, "deep recursion not available on some 'terps", frame_max);
  }
  frame_count++;
  stack_frames[frame_count].stack_stack_start = stack_pointer;
  stack_frames[frame_count].return_PC = return_PC;
  stack_frames[frame_count].num_locals = num_locals;
  stack_frames[frame_count].arguments = num_args;
  stack_frames[frame_count].result_variable = result_var;


  check_stack_stack(num_locals);
  for(n = 0; n < num_locals; n++)
    stack_stack[stack_pointer++] = locals[n];

  stack_min = stack_pointer;

  local_vars = stack_stack + stack_frames[frame_count].stack_stack_start;
}


void remove_stack_frame(void)
{
#ifndef FAST
  if(frame_count == 0) {
    n_show_error(E_STACK, "attempt to remove initial stack frame", 0);
    return;
  }
#endif
  stack_pointer = stack_frames[frame_count].stack_stack_start;
  frame_count--;
  stack_min = stack_frames[frame_count].stack_stack_start +
              stack_frames[frame_count].num_locals;
  local_vars = stack_stack + stack_frames[frame_count].stack_stack_start;
}


void check_set_var(int var, zword val)
{
  switch(var) {
  default: set_var(var, val); break;
  case -2: exit_decoder = TRUE; time_ret = val;     /* timer junk */
  case -1: ;
  }
}


void mop_func_return(zword ret_val)
{
  int var;
  PC = stack_frames[frame_count].return_PC;
  var = stack_frames[frame_count].result_variable;
  remove_stack_frame();
  check_set_var(var, ret_val);
  /*  printf("retn %x\n", PC); */
}


void op_catch(void)
{
  mop_store_result(frame_count);
}


unsigned stack_get_numlocals(int frame)
{
  if(stack_frames)
    return stack_frames[frame].num_locals;
  return 0;
}


void op_throw(void)
{
#ifndef FAST
  if(operand[1] > frame_count) {
    n_show_error(E_STACK, "attempting to throw away non-existent frames", operand[1]);
    return;
  }
#endif
  if(operand[1] != 0) {
    frame_count = operand[1];
    mop_func_return(operand[0]);
  } else {
    n_show_error(E_STACK, "attempting to throw away initial frame", operand[0]);
  }
}

void op_check_arg_count(void)
{
  if(stack_frames[frame_count].arguments >= operand[0])
    mop_take_branch();
  else
    mop_skip_branch();
}


static zword stack_pop(void)
{
#ifndef FAST
  if(stack_pointer <= stack_min) {
    n_show_error(E_STACK, "underflow - excessive popping", stack_pointer);
    return 0;
  }
#endif
  return stack_stack[--stack_pointer];
}


static void stack_push(zword n)
{
  check_stack_stack(1);
  stack_stack[stack_pointer++] = n;
}


void op_push(void)
{
  stack_push(operand[0]);
}


void op_pop(void)
{
  stack_pop();
}


void op_pull(void)
{
  if(zversion == 6) {  /* v6 uses user stacks */
    if(numoperands == 0 || operand[0] == 0)
      mop_store_result(stack_pop());
    else {
      zword space = LOWORD(operand[0]) + 1; /* One more slot is free */
      LOWORDwrite(operand[0], space);
      mop_store_result(LOWORD(operand[0] + space * ZWORD_SIZE));
    }
  } else {
    zword val = stack_pop();
    set_var(operand[0], val);
  }
}


void op_pop_stack(void)
{
  zword i;
  if(numoperands < 2 || operand[1] == 0) {
    for(i = 0; i < operand[0]; i++)
      stack_pop();
  } else {
    zword space = LOWORD(operand[1]) + operand[0];
    LOWORDwrite(operand[1], space);
  }
}


void op_push_stack(void)
{
  zword space = LOWORD(operand[1]);
  if(space) {
    LOWORDwrite(operand[1] + space * ZWORD_SIZE, operand[0]);
    LOWORDwrite(operand[1], space - 1);
    mop_take_branch();
  } else {
    mop_skip_branch();
  }
}


void mop_store_result(zword val)
{
  set_var(HIBYTE(PC), val);
  PC++;
}


void op_ret_popped(void)
{
  mop_func_return(stack_pop());
}

unsigned stack_get_depth(void)
{
  return frame_count;
}

BOOL frame_is_valid(unsigned frame)
{
  return frame <= frame_count;
}

offset frame_get_PC(unsigned frame)
{
  if(frame == frame_count) {
    return PC;
  }
  return stack_frames[frame+1].return_PC;
}

zword frame_get_var(unsigned frame, int var)
{
  if(var == 0 || var > 0x10) {
    n_show_error(E_STACK, "variable not readable from arbitrary frame", var);
    return 0;
  }

  if(var > stack_frames[frame].num_locals)
    n_show_error(E_STACK, "reading nonexistant local", var);

  return stack_stack[stack_frames[frame].stack_stack_start + (var - 1)];
}


void frame_set_var(unsigned frame, int var, zword val)
{
  if(var == 0 || var > 0x10) {
    n_show_error(E_STACK, "variable not writable from arbitrary frame", var);
    return;
  }

  if(var > stack_frames[frame].num_locals)
    n_show_error(E_STACK, "writing nonexistant local", var);

  stack_stack[stack_frames[frame].stack_stack_start + (var - 1)] = val;;
}

zword get_var(int var)
{
  if(var < 0x10) {
    if(var != 0) {
#ifndef FAST
      if(var > stack_frames[frame_count].num_locals)
	n_show_error(E_INSTR, "reading nonexistant local", var);
#endif
      return local_vars[var - 1];
    }
    return stack_pop();
  }
  return LOWORD(z_globaltable + (var - 0x10) * ZWORD_SIZE);
}

void set_var(int var, zword val)
{
  if(var < 0x10) {
    if(var != 0) {
#ifndef FAST
      if(var > stack_frames[frame_count].num_locals)
	n_show_error(E_INSTR, "setting nonexistant local", var);
#endif
      local_vars[var - 1] = val;
    } else {
      stack_push(val);
    }
  } else {
    LOWORDwrite(z_globaltable + (var - 0x10) * ZWORD_SIZE, val);
  }
}


const unsigned qstackframe[] = { 3, 1, 1, 1, 2, 0 };
enum qstacknames { qreturnPC, qflags, qvar, qargs, qeval };

BOOL quetzal_stack_restore(strid_t stream, glui32 qsize)
{
  glui32 i = 0;
  int num_frames = 0;

  kill_stack();
  init_stack(1024, 128);
  
  while(i < qsize) {
    unsigned n;
    unsigned num_locals;
    zword locals[16];
    int num_args;
    int var;

    glui32 qframe[5];
    i += fillstruct(stream, qstackframe, qframe, NULL);

    if(qframe[qreturnPC] > total_size) {
      n_show_error(E_SAVE, "function return PC past end of memory",
		 qframe[qreturnPC]);
      return FALSE;
    }

    if((qframe[qflags] & b11100000) != 0) {
      n_show_error(E_SAVE, "expected top bits of flag to be zero", qframe[qflags]);
      return FALSE;
    }
    
    var = qframe[qvar];
    if(qframe[qflags] & b00010000)  /* from a call_n */
      var = -1;
    
    num_locals = qframe[qflags] & b00001111;

    if(num_locals > 15) {
      n_show_error(E_SAVE, "too many locals", num_locals);
      return FALSE;
    }
    
    num_args = 0;
    switch(qframe[qargs]) {
    default:
      n_show_error(E_SAVE, "invalid argument count", qframe[qargs]);
      return FALSE;
    case b01111111: num_args++;
    case b00111111: num_args++;
    case b00011111: num_args++;
    case b00001111: num_args++;
    case b00000111: num_args++;
    case b00000011: num_args++;
    case b00000001: num_args++;
    case b00000000: ;
    }
    
    for(n = 0; n < num_locals; n++) {
      unsigned char v[ZWORD_SIZE];
      glk_get_buffer_stream(stream, (char *) v, ZWORD_SIZE);
      locals[n] = MSBdecodeZ(v);
      i+=ZWORD_SIZE;
    }
    
    if(zversion != 6 && num_frames == 0)
      ;               /* dummy stack frame; don't really use it */
    else
      add_stack_frame(qframe[qreturnPC],
		      num_locals, locals,
		      num_args, var);
    
    for(n = 0; n < qframe[qeval]; n++) {
      unsigned char v[ZWORD_SIZE];
      glk_get_buffer_stream(stream, (char *) v, ZWORD_SIZE);
      stack_push(MSBdecodeZ(v));
      i += ZWORD_SIZE;
    }
    
    num_frames++;
  }
  if(!verify_stack()) {
    n_show_error(E_SAVE, "restored stack fails verification", 0);
    return FALSE;
  }
  return TRUE;
}


glui32 get_quetzal_stack_size(void)
{
  glui32 framespace;
  glui32 stackspace;
  framespace = frame_count * 8;
  stackspace = stack_pointer * ZWORD_SIZE;
  if(zversion != 6)
    framespace += 8;  /* for the dummy frame */
  return framespace + stackspace;
}


BOOL quetzal_stack_save(strid_t stream)
{
  unsigned frame_num = 0;

  if(zversion == 6)
    frame_num++;

  if(!verify_stack()) {
    n_show_error(E_SAVE, "stack did not pass verification before saving", 0);
    return FALSE;
  }

  /* We have to look one ahead to see how much stack space a frame uses; when
     we get to the last frame, there will be no next frame, so this won't work
     if there wasn't a frame there earlier with the correct info.  Add and
     remove a frame to make things happy */
  add_stack_frame(0, 0, NULL, 0, 0);
  remove_stack_frame();

  for(; frame_num <= frame_count; frame_num++) {
    unsigned n;
    int num_locals;
    unsigned stack_size;

    glui32 qframe[5];

    const unsigned char argarray[8] = {
      b00000000, b00000001, b00000011, b00000111,
      b00001111, b00011111, b00111111, b01111111
    };

    qframe[qreturnPC] = stack_frames[frame_num].return_PC;

    qframe[qvar]      = stack_frames[frame_num].result_variable;

    num_locals        = stack_frames[frame_num].num_locals;

    if(num_locals > 15) {
      n_show_error(E_SAVE, "num_locals too big", num_locals);
      return FALSE;
    }

    qframe[qflags] = num_locals;

    if(stack_frames[frame_num].result_variable == -1) {
      qframe[qflags] |= b00010000;
      qframe[qvar] = 0;
    }

    if(stack_frames[frame_num].arguments > 7) {
      n_show_error(E_SAVE, "too many function arguments", stack_frames[frame_num].arguments);
      return FALSE;
    }
    
    qframe[qargs] = argarray[stack_frames[frame_num].arguments];

    stack_size = (stack_frames[frame_num+1].stack_stack_start -
		  stack_frames[frame_num].stack_stack_start -
		  num_locals);

    qframe[qeval] = stack_size;
	             
    if(frame_num == 0) {
      qframe[qreturnPC] = 0;
      qframe[qflags] = 0;
      qframe[qvar] = 0;
      qframe[qargs] = 0;
    }

    emptystruct(stream, qstackframe, qframe);
    
    for(n = 0; n < num_locals + stack_size; n++) {
      unsigned char v[ZWORD_SIZE];
      zword z = stack_stack[stack_frames[frame_num].stack_stack_start + n];
      MSBencodeZ(v, z);
      w_glk_put_buffer_stream(stream, (char *) v, ZWORD_SIZE);
    }
  }
  return TRUE;
}
