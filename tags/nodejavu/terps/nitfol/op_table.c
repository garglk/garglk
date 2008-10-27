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

void op_copy_table(void)
{
  offset i;
  zword first = operand[0];
  zword second = operand[1];
  zword size = operand[2];

  if(second == 0) {                          /* zero 'first' bytes */
    for(i = 0; i < size; i++)
      LOBYTEwrite(first + i, 0);
  } else {
    if(first > second || is_neg(size)) {
      if(is_neg(size))
	size = neg(size);

      for(i = 0; i < size; i++)              /* copy forward */
	LOBYTEcopy(second + i, first + i);
    }
    else {
      for(i = 0; i < size; i++)              /* copy backward */
	LOBYTEcopy(second + size - i - 1, first + size - i - 1);
    }
  }
}


void op_scan_table(void)
{
  zword i;
  int form = operand[3];
  zword address = operand[1];
  if(numoperands < 4)
    form = 0x82; /* default form - scan for words, increment by two bytes */

  if(form & b10000000) {  /* Bit 8 set means scan for words */
    for(i = 0; i < operand[2]; i++) {
      if(LOWORD(address) == operand[0]) {
	mop_store_result(address);
	mop_take_branch();
	return;
      }
      address += form & b01111111; /* Bottom 7 bits give amount to increment */
    }
    mop_store_result(0);
    mop_skip_branch();
  } else {          /* Bit 8 clear means scan for bytes */
    for(i = 0; i < operand[2]; i++) {
      if(LOBYTE(address) == operand[0]) {
	mop_store_result(address);
	mop_take_branch();
	return;
      }
      address += form & b01111111;
    }
    mop_store_result(0);
    mop_skip_branch();
  }
}

void op_loadb(void)
{
  mop_store_result(LOBYTE(operand[0] + operand[1]));
}

void op_loadw(void)
{
  mop_store_result(LOWORD(operand[0] + operand[1] * ZWORD_SIZE));
}

static void z_write_header(zword i, zbyte val)
{
  zbyte diff = LOBYTE(i) ^ val;
  if(diff == 0)
    return;
  if(i >= 0x40) {
    LOBYTEwrite(i, val);
    return;
  }
  if(i != HD_FLAGS2 + 1) {
    n_show_error(E_MEMORY, "attempt to write to non-dynamic byte in header", i);
    return;
  }
  if(diff > b00000111) {
    n_show_error(E_MEMORY, "attempt to change non-dynamic bits in flags2", val);
    return;
  }
  LOBYTEwrite(i, val);
  if(diff & b00000001) {
    if(val & b00000001) {
      operand[0] = 2;
      op_output_stream();
    } else {
      operand[0] = neg(2);
      op_output_stream();
    }
  }
  if(diff & b00000010) {
    if(val & b00000010) {
      set_fixed(TRUE);
    } else {
      set_fixed(FALSE);
    }
  }
}

void op_storeb(void)
{
  zword addr = operand[0] + operand[1];
  if(addr < 0x40)
    z_write_header(addr, (zbyte) operand[2]);
  else
    LOBYTEwrite(addr, operand[2]);
}

void op_storew(void)
{
  zword addr = operand[0] + operand[1] * ZWORD_SIZE;
  if(addr < 0x40) {
    z_write_header(addr,     (zbyte) (operand[2] >> 8));
    z_write_header(addr + 1, (zbyte) (operand[2] & 0xff));
  } else {
    LOWORDwrite(addr, operand[2]);
  }
}


void header_extension_write(zword w, zword val)
{
  w *= ZWORD_SIZE;
  if(z_headerext == 0)
    return;

  if(LOWORD(z_headerext) < w)
    return;

  LOWORDwrite(z_headerext + w, val);
}

zword header_extension_read(unsigned w)
{
  w *= ZWORD_SIZE;
  if(z_headerext == 0)
    return 0;

  if(LOWORD(z_headerext) < w)
    return 0;

  return LOWORD(z_headerext + w);
}
