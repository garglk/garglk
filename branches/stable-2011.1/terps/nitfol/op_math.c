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


void op_load(void)
{
  mop_store_result(get_var(operand[0]));
}

void op_store(void)
{
  set_var(operand[0], operand[1]);
}

void op_add(void)
{
  mop_store_result(ARITHMASK(operand[0] + operand[1]));
}

void op_sub(void)
{
  mop_store_result(ARITHMASK(operand[0] + neg(operand[1])));
}


void op_and(void)
{
  mop_store_result(operand[0] & operand[1]);
}

void op_or(void)
{
  mop_store_result(operand[0] | operand[1]);
}

void op_not(void)
{
  mop_store_result(ARITHMASK(~operand[0]));
}


void op_art_shift(void)
{
  if(is_neg(operand[1])) {
    if(is_neg(operand[0])) {
      zword i;
      zword foo = operand[0];
      /* FIXME: is there a better way? */
      for(i = 0; i < neg(operand[1]); i++) {
	foo >>= 1;
	foo |= ZWORD_TOPBITMASK;
      }
      mop_store_result(foo);
    } else {
      mop_store_result(operand[0] >> neg(operand[1]));
    }
  } else {
      mop_store_result(ARITHMASK(operand[0] << operand[1]));
  }
}

void op_log_shift(void)
{
  if(is_neg(operand[1]))
    mop_store_result(operand[0] >> neg(operand[1]) );
  else
    mop_store_result(ARITHMASK(operand[0] << operand[1]));
}


void op_dec(void)
{
  zword val = ARITHMASK(get_var(operand[0]) - 1);
  set_var(operand[0], val);
}

void op_dec_chk(void)
{
  zword val = ARITHMASK(get_var(operand[0]) - 1);
  set_var(operand[0], val);

  if(is_lessthan(val, operand[1]))
    mop_take_branch();
  else
    mop_skip_branch();
}

void op_inc(void)
{
  zword val = ARITHMASK(get_var(operand[0]) + 1);
  set_var(operand[0], val);
}

void op_inc_chk(void)
{
  unsigned val = ARITHMASK(get_var(operand[0]) + 1);
  set_var(operand[0], val);

  if(is_greaterthan(val, operand[1]))
    mop_take_branch();
  else
    mop_skip_branch();
}


zword z_mult(zword a, zword b)
{
  int sign = 0;

  if(is_neg(a)) {
    a = neg(a);
    sign = 1;
  }
  if(is_neg(b)) {
    b = neg(b);
    sign ^= 1;
  }

  if(sign)
    return ARITHMASK(neg(a * b));
  else
    return ARITHMASK(a * b);

}


zword z_div(zword a, zword b)
{
  int sign = 0;
  
  if(b == 0) {
    n_show_error(E_MATH, "division by zero", a);
    return ZWORD_MAX;
  }
  
  if(is_neg(a)) {
    a = neg(a);
    sign = 1;
  }
  if(is_neg(b)) {
    b = neg(b);
    sign ^= 1;
  }

  if(sign)
    return neg(a / b);
  else
    return a / b;
}


zword z_mod(zword a, zword b)
{
  int sign = 0;

  if(b == 0) {
    n_show_error(E_MATH, "modulo by zero", a);
    return 0;
  }

  if(is_neg(a)) {
    a = neg(a);
    sign = 1;
  }
  if(is_neg(b)) {
    b = neg(b);
  }

  if(sign)
    return neg(a % b);
  else
    return a % b;
}


void op_div(void)
{
  mop_store_result(z_div(operand[0], operand[1]));
}


void op_mod(void)
{
  mop_store_result(z_mod(operand[0], operand[1]));
}


void op_mul(void)
{
  mop_store_result(z_mult(operand[0], operand[1]));
}


/* FIXME: use our own rng */
zword z_random(zword num)
{
  static BOOL rising = FALSE;
  static unsigned r = 0;
  zword result = 0;
  
  if(num == 0) {
    if(faked_random_seed)
      srand(faked_random_seed);
    else
      srand(time(NULL));
    rising = FALSE;
  } else if(is_neg(num)) {
    if(neg(num) < 1000) {
      r = 0;
      rising = TRUE;
    } else {
      srand(neg(num));
      rising = FALSE;
    }
  } else {
    if(rising) {
      r++;
      if(r > num)
	r = 1;
      result = r;
    } else {
      result = (1 + (rand() % num));
    }
  }
  return result;
}

void op_random(void)
{
  if(operand[0] == 0)
    n_show_port(E_MATH, "some interpreters don't like @random 0", operand[0]);

  mop_store_result(z_random(operand[0]));
}

