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

void mop_call(zword dest, unsigned num_args, zword *args, int result_var)
{
  unsigned i;
  offset destPC;
  unsigned num_local;

  if(dest == 0) {
    check_set_var(result_var, 0);
    return;
  }

  destPC = UNPACKR(dest);

  /*  printf("call %x -> %x\n", oldPC, destPC); */
  
#ifndef FAST
  if(destPC > game_size) {
    n_show_error(E_INSTR, "call past end of story", dest);
    check_set_var(result_var, 0);
    return;
  }
#endif
  
  num_local = HIBYTE(destPC); /* first byte is # of local variables */

#ifndef FAST
  if(num_local > 15) {
    n_show_error(E_INSTR, "call to non-function (initial byte > 15)", dest);
    check_set_var(result_var, 0);
    return;
  }
#endif

  destPC++;     /* Go on past the variable count */

  if(zversion < 5) {
    for(i = num_args; i < num_local; i++)
      args[i] = HIWORD(destPC + i * ZWORD_SIZE);  /* Load default locals */
    destPC += num_local * ZWORD_SIZE;
  } else {
    for(i = num_args; i < num_local; i++)
      args[i] = 0;                                /* locals default to zero */
  }

  add_stack_frame(PC, num_local, args, num_args, result_var);

  PC = destPC;

  /*n_show_debug(E_DEBUG, "function starting", dest);*/
}

void op_call_n(void)
{
  mop_call(operand[0], numoperands - 1, operand + 1, -1);
}

void op_call_s(void)
{
  zbyte ret_var = HIBYTE(PC);
  PC++;
  mop_call(operand[0], numoperands - 1, operand + 1, ret_var);
}

void op_ret(void)
{
  mop_func_return(operand[0]);
}

void op_rfalse(void)
{
  mop_func_return(0);
}

void op_rtrue(void)
{
  mop_func_return(1);
}
