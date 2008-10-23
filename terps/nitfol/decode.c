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

void decode(void)
{
  unsigned optypes;
  int maxoperands;

  while(!exit_decoder) {
    zbyte opcode = HIBYTE(PC);

    oldPC = PC;

#ifdef DEBUGGING
    if(do_check_watches)
      check_watches();
#endif

    PC++;

#ifndef NO_TICK
    glk_tick();  /* tick tock hickery dock the mouse ran up the clock */
#endif
    
    /* Top bits decide opcode/operand encoding */
    switch(opcode >> 4) {
    case 0: case 1:                         /* long 2OP */
      operand[0] = HIBYTE(PC);              /* small constant */
      operand[1] = HIBYTE(PC+1);            /* small constant */
      numoperands = 2; PC += 2;
      opcode += OFFSET_2OP - 0x00;
      break;


    case 2: case 3:                         /* long 2OP */
      operand[0] = HIBYTE(PC);              /* small constant */
      operand[1] = get_var(HIBYTE(PC+1));   /* variable */
      numoperands = 2; PC += 2;
      opcode += OFFSET_2OP - 0x20;
      break;


    case 4: case 5:                         /* long 2OP */
      operand[0] = get_var(HIBYTE(PC));     /* variable */
      operand[1] = HIBYTE(PC+1);            /* small constant */
      numoperands = 2; PC += 2;
      opcode += OFFSET_2OP - 0x40;
      break;


    case 6: case 7:                         /* long 2OP */
      operand[0] = get_var(HIBYTE(PC));     /* variable */
      operand[1] = get_var(HIBYTE(PC+1));   /* variable */
      numoperands = 2; PC += 2;
      opcode += OFFSET_2OP - 0x60;
      break;


    case 8:                                 /* short 1OP */
      operand[0] = HIWORD(PC);              /* large constant */
      numoperands = 1; PC += ZWORD_SIZE;
      opcode += OFFSET_1OP - 0x80;
      break;


    case 9:                                 /* short 1OP */
      operand[0] = HIBYTE(PC);              /* small constant */
      numoperands = 1; PC += 1;
      opcode += OFFSET_1OP - 0x90;
      break;


    case 10:                                /* short 1OP */
      operand[0] = get_var(HIBYTE(PC));     /* variable */
      numoperands = 1; PC += 1;
      opcode += OFFSET_1OP - 0xa0;
      break;


    case 11:                                /* short 0OP */
      if(opcode != 0xbe) {
	numoperands = 0;
	opcode += OFFSET_0OP - 0xb0;
	break;
      }
                                            /* EXTENDED */
      opcode  = HIBYTE(PC);   /* Get the extended opcode */
      optypes = HIBYTE(PC+1);
      PC += 2;

#ifndef FAST
      if(OFFSET_EXT + opcode > OFFSET_END) {
	n_show_error(E_INSTR, "unknown extended opcode", opcode);
	break;
      }
#endif
      
      for(numoperands = 0; numoperands < 4; numoperands++) {
	switch(optypes & (3 << 6)) {   /* Look at the highest two bits. */
	case 0 << 6:
	  operand[numoperands] = HIWORD(PC); PC+=ZWORD_SIZE; break;
	case 1 << 6:
	  operand[numoperands] = HIBYTE(PC); PC++; break;
	case 2 << 6:
	  operand[numoperands] = get_var(HIBYTE(PC)); PC++; break;
	default:
	  goto END_OF_EXTENDED;   /* inky says, "use the goto." */
	}
	optypes <<= 2;    /* Move the next two bits into position. */
      }
    END_OF_EXTENDED:
      opcode += OFFSET_EXT;
      break;

    case 12: case 13: case 14: case 15:     /* variable operand count */
      maxoperands = 4;
      optypes = ((unsigned) HIBYTE(PC)) << 8; /* Shift left so our loop will */
                                    /* be the same for both 4 and 8 operands */
      PC++;
  
      if(opcode == 0xec || opcode == 0xfa) {  /* call_vs2 and call_vn2 */
	maxoperands = 8;
	optypes |= HIBYTE(PC);  /* Fill the bottom 8 bits */
	PC++;
      }
      
      for(numoperands = 0; numoperands < maxoperands; numoperands++) {
	switch(optypes & (3 << 14)) {   /* Look at the highest two bits. */
	case 0 << 14:
	  operand[numoperands] = HIWORD(PC); PC+=ZWORD_SIZE; break;
	case 1 << 14:
	  operand[numoperands] = HIBYTE(PC); PC++; break;
	case 2 << 14:
	  operand[numoperands] = get_var(HIBYTE(PC)); PC++; break;
	default:
	  goto END_OF_VARIABLE;
	}
	optypes <<= 2;    /* Move the next two bits into position. */
      }
    END_OF_VARIABLE:
      opcode += OFFSET_2OP - 0xc0;
      break;
    }
    opcodetable[opcode]();
  }
}
