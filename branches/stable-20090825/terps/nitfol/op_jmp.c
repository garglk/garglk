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


N_INLINE static void skip_branch(zbyte branch)
{
  if(!(branch & b01000000)) /* Bit 6 clear means 'branch occupies two bytes' */
    PC++;
}

N_INLINE static void take_branch(zbyte branch)
{
  int o = branch & b00111111;

  if(!(branch & b01000000)) {/* Bit 6 clear means 'branch occupies two bytes'*/
    o = (o << 8) + HIBYTE(PC);
    PC++;
    if(branch & b00100000)
      o = -((1 << 14) - o);
  }

  if(o == 0)
    mop_func_return(0);
  else if(o == 1)
    mop_func_return(1);
  else
    PC += o - 2;

#ifndef FAST
  if(PC > game_size) {
    n_show_error(E_INSTR, "attempt to conditionally jump outside of story", o - 2);
    PC -= o - 2;
    return;
  }
#endif
  
  /*  printf("cjmp %x -> %x\n", oldPC, PC); */
}


void mop_skip_branch(void)
{
  zbyte branch = HIBYTE(PC);
  PC++;

  if(branch & b10000000)  /* Bit 7 set means 'branch when true' */
    skip_branch(branch);
  else
    take_branch(branch);

}

void mop_take_branch(void)
{
  zbyte branch = HIBYTE(PC);
  PC++;

  if(branch & b10000000)  /* Bit 7 set means 'branch when true' */
    take_branch(branch);
  else
    skip_branch(branch);
}

void mop_cond_branch(BOOL cond)
{
  zbyte branch = HIBYTE(PC);
  PC++;
  if((branch >> 7) ^ cond)
    skip_branch(branch);
  else
    take_branch(branch);
}


void op_je(void)
{
  int i;
  for(i = 1; i < numoperands; i++)
    if(operand[0] == operand[i]) {
      mop_take_branch();
      return;
    }

  mop_skip_branch();
}

void op_jg(void)
{
  if(is_greaterthan(operand[0], operand[1]))
    mop_take_branch();
  else
    mop_skip_branch();
}

void op_jl(void)
{
  if(is_lessthan(operand[0], operand[1]))
    mop_take_branch();
  else
    mop_skip_branch();
}

void op_jump(void)
{
  operand[0] -= 2;  /* not documented in zspec */
  if(is_neg(operand[0])) {
#ifndef FAST
    if(neg(operand[0]) > PC) {
      n_show_error(E_INSTR, "attempt to jump before beginning of story", -neg(operand[0]));
      return;
    }
#endif
    PC -= neg(operand[0]);
  }
  else {
    PC += operand[0];
    if(PC > game_size) {
      n_show_error(E_INSTR, "attempt to jump past end of story", operand[0]);
      PC -= operand[0];
      return;
    }
  }
  /*  printf("jump %x -> %x\n", oldPC, PC); */
}

void op_jz(void)
{
  mop_cond_branch(operand[0] == 0);
}

void op_test(void)
{
  mop_cond_branch((operand[0] & operand[1]) == operand[1]);
}

void op_verify(void)
{
  mop_cond_branch(LOWORD(HD_CHECKSUM) == z_checksum);
}

void op_piracy(void)
{
  mop_cond_branch(aye_matey);
}
