/*-
 * Copyright 2010-2015 Chris Spiegel.
 *
 * This file is part of Bocfel.
 *
 * Bocfel is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version
 * 2 or 3, as published by the Free Software Foundation.
 *
 * Bocfel is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Bocfel.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <setjmp.h>

#ifdef ZTERP_GLK
#include <glk.h>
#endif

#include "process.h"
#include "branch.h"
#include "dict.h"
#include "math.h"
#include "memory.h"
#include "meta.h"
#include "objects.h"
#include "random.h"
#include "screen.h"
#include "sound.h"
#include "stack.h"
#include "util.h"
#include "zoom.h"
#include "zterp.h"

unsigned long pc;
unsigned long current_instruction;

uint16_t zargs[8];
int znargs;

static jmp_buf *jumps;
static size_t njumps;

/* Each time an interrupt happens, process_instructions() is called
 * (effectively starting a whole new round of interpreting).  This
 * variable holds the current level of interpreting: 0 for no
 * interrupts, 1 if one interrupt has been called, 2 if an interrupt was
 * called inside of an interrupt, and so on.
 */
static size_t ilevel = -1;

bool in_interrupt(void)
{
  return ilevel > 0;
}

/* Jump back to the previous round of interpreting.  This is used when
 * an interrupt routine returns.
 */
znoreturn
void interrupt_return(void)
{
  longjmp(jumps[ilevel--], 1);
}

/* Jump back to the first round of processing.  This is used when
 * restoring and restarting in case these happen while in an interrupt.
 */
znoreturn
void interrupt_reset(void)
{
  ilevel = 0;
  longjmp(jumps[0], 2);
}

/* Jump back to the first round of processing, but tell it to
 * immediately stop.  This is used to implement @quit.
 */
znoreturn
void interrupt_quit(void)
{
  longjmp(jumps[0], 1);
}

/* Returns true if decoded, false otherwise (omitted) */
static bool decode_base(uint8_t type, uint16_t *loc)
{
  switch(type)
  {
    case 0: /* Large constant. */
      *loc = word(pc);
      pc += 2;
      break;
    case 1: /* Small constant. */
      *loc = byte(pc++);
      break;
    case 2: /* Variable. */
      *loc = variable(byte(pc++));
      break;
    default: /* Omitted. */
      return false;
  }

  return true;
}

static void decode_var(uint8_t types)
{
  uint16_t ret;

  for(int i = 6; i >= 0; i -= 2)
  {
    if(!decode_base((types >> i) & 0x03, &ret)) return;
    zargs[znargs++] = ret;
  }
}

static void (*opcodes[256])(void);
static void (*ext_opcodes[256])(void);
enum opcount { ZERO, ONE, TWO, VAR, EXT };

#define op_call(opcode)		opcodes[opcode]()
#define extended_call(opcode)	ext_opcodes[opcode]()

/* This nifty trick is from Frotz. */
static void zextended(void)
{
  uint8_t opnumber = byte(pc++);

  decode_var(byte(pc++));

  extended_call(opnumber);
}

znoreturn
static void illegal_opcode(void)
{
  die("illegal opcode (pc = 0x%lx)", current_instruction);
}

static void setup_single_opcode(int minver, int maxver, enum opcount opcount, int opcode, void (*fn)(void))
{
  if(zversion < minver || zversion > maxver) return;

  switch(opcount)
  {
    case ZERO:
      opcodes[opcode + 176] = fn;
      break;
    case ONE:
      opcodes[opcode + 128] = fn;
      opcodes[opcode + 144] = fn;
      opcodes[opcode + 160] = fn;
      break;
    case TWO:
      opcodes[opcode +   0] = fn;
      opcodes[opcode +  32] = fn;
      opcodes[opcode +  64] = fn;
      opcodes[opcode +  96] = fn;
      opcodes[opcode + 192] = fn;
      break;
    case VAR:
      opcodes[opcode + 224] = fn;
      break;
    case EXT:
      ext_opcodes[opcode] = fn;
      break;
  }
}

void setup_opcodes(void)
{
  for(int opcode = 0; opcode < 256; opcode++)
  {
    opcodes[opcode] = illegal_opcode;

    /* §14.2.1 */
    ext_opcodes[opcode] = znop;
  }

  setup_single_opcode(1, 6, ZERO, 0x00, zrtrue);
  setup_single_opcode(1, 6, ZERO, 0x01, zrfalse);
  setup_single_opcode(1, 6, ZERO, 0x02, zprint);
  setup_single_opcode(1, 6, ZERO, 0x03, zprint_ret);
  setup_single_opcode(1, 6, ZERO, 0x04, znop);
  setup_single_opcode(1, 4, ZERO, 0x05, zsave);
  setup_single_opcode(1, 4, ZERO, 0x06, zrestore);
  setup_single_opcode(1, 6, ZERO, 0x07, zrestart);
  setup_single_opcode(1, 6, ZERO, 0x08, zret_popped);
  setup_single_opcode(1, 4, ZERO, 0x09, zpop);
  setup_single_opcode(5, 6, ZERO, 0x09, zcatch);
  setup_single_opcode(1, 6, ZERO, 0x0a, zquit);
  setup_single_opcode(1, 6, ZERO, 0x0b, znew_line);
  setup_single_opcode(3, 3, ZERO, 0x0c, zshow_status);
  setup_single_opcode(3, 6, ZERO, 0x0d, zverify);
  setup_single_opcode(5, 6, ZERO, 0x0e, zextended);
  setup_single_opcode(5, 6, ZERO, 0x0f, zpiracy);

  setup_single_opcode(1, 6, ONE, 0x00, zjz);
  setup_single_opcode(1, 6, ONE, 0x01, zget_sibling);
  setup_single_opcode(1, 6, ONE, 0x02, zget_child);
  setup_single_opcode(1, 6, ONE, 0x03, zget_parent);
  setup_single_opcode(1, 6, ONE, 0x04, zget_prop_len);
  setup_single_opcode(1, 6, ONE, 0x05, zinc);
  setup_single_opcode(1, 6, ONE, 0x06, zdec);
  setup_single_opcode(1, 6, ONE, 0x07, zprint_addr);
  setup_single_opcode(4, 6, ONE, 0x08, zcall_1s);
  setup_single_opcode(1, 6, ONE, 0x09, zremove_obj);
  setup_single_opcode(1, 6, ONE, 0x0a, zprint_obj);
  setup_single_opcode(1, 6, ONE, 0x0b, zret);
  setup_single_opcode(1, 6, ONE, 0x0c, zjump);
  setup_single_opcode(1, 6, ONE, 0x0d, zprint_paddr);
  setup_single_opcode(1, 6, ONE, 0x0e, zload);
  setup_single_opcode(1, 4, ONE, 0x0f, znot);
  setup_single_opcode(5, 6, ONE, 0x0f, zcall_1n);

  setup_single_opcode(1, 6, TWO, 0x01, zje);
  setup_single_opcode(1, 6, TWO, 0x02, zjl);
  setup_single_opcode(1, 6, TWO, 0x03, zjg);
  setup_single_opcode(1, 6, TWO, 0x04, zdec_chk);
  setup_single_opcode(1, 6, TWO, 0x05, zinc_chk);
  setup_single_opcode(1, 6, TWO, 0x06, zjin);
  setup_single_opcode(1, 6, TWO, 0x07, ztest);
  setup_single_opcode(1, 6, TWO, 0x08, zor);
  setup_single_opcode(1, 6, TWO, 0x09, zand);
  setup_single_opcode(1, 6, TWO, 0x0a, ztest_attr);
  setup_single_opcode(1, 6, TWO, 0x0b, zset_attr);
  setup_single_opcode(1, 6, TWO, 0x0c, zclear_attr);
  setup_single_opcode(1, 6, TWO, 0x0d, zstore);
  setup_single_opcode(1, 6, TWO, 0x0e, zinsert_obj);
  setup_single_opcode(1, 6, TWO, 0x0f, zloadw);
  setup_single_opcode(1, 6, TWO, 0x10, zloadb);
  setup_single_opcode(1, 6, TWO, 0x11, zget_prop);
  setup_single_opcode(1, 6, TWO, 0x12, zget_prop_addr);
  setup_single_opcode(1, 6, TWO, 0x13, zget_next_prop);
  setup_single_opcode(1, 6, TWO, 0x14, zadd);
  setup_single_opcode(1, 6, TWO, 0x15, zsub);
  setup_single_opcode(1, 6, TWO, 0x16, zmul);
  setup_single_opcode(1, 6, TWO, 0x17, zdiv);
  setup_single_opcode(1, 6, TWO, 0x18, zmod);
  setup_single_opcode(4, 6, TWO, 0x19, zcall_2s);
  setup_single_opcode(5, 6, TWO, 0x1a, zcall_2n);
  setup_single_opcode(5, 6, TWO, 0x1b, zset_colour);
  setup_single_opcode(5, 6, TWO, 0x1c, zthrow);

  setup_single_opcode(1, 6, VAR, 0x00, zcall);
  setup_single_opcode(1, 6, VAR, 0x01, zstorew);
  setup_single_opcode(1, 6, VAR, 0x02, zstoreb);
  setup_single_opcode(1, 6, VAR, 0x03, zput_prop);
  setup_single_opcode(1, 6, VAR, 0x04, zread);
  setup_single_opcode(1, 6, VAR, 0x05, zprint_char);
  setup_single_opcode(1, 6, VAR, 0x06, zprint_num);
  setup_single_opcode(1, 6, VAR, 0x07, zrandom);
  setup_single_opcode(1, 6, VAR, 0x08, zpush);
  setup_single_opcode(1, 6, VAR, 0x09, zpull);
  setup_single_opcode(3, 6, VAR, 0x0a, zsplit_window);
  setup_single_opcode(3, 6, VAR, 0x0b, zset_window);
  setup_single_opcode(4, 6, VAR, 0x0c, zcall_vs2);
  setup_single_opcode(4, 6, VAR, 0x0d, zerase_window);
  setup_single_opcode(4, 6, VAR, 0x0e, zerase_line);
  setup_single_opcode(4, 6, VAR, 0x0f, zset_cursor);
  setup_single_opcode(4, 6, VAR, 0x10, zget_cursor);
  setup_single_opcode(4, 6, VAR, 0x11, zset_text_style);
  setup_single_opcode(4, 6, VAR, 0x12, znop); /* XXX buffer_mode */
  setup_single_opcode(3, 6, VAR, 0x13, zoutput_stream);
  setup_single_opcode(3, 6, VAR, 0x14, zinput_stream);
  setup_single_opcode(3, 6, VAR, 0x15, zsound_effect);
  setup_single_opcode(4, 6, VAR, 0x16, zread_char);
  setup_single_opcode(4, 6, VAR, 0x17, zscan_table);
  setup_single_opcode(5, 6, VAR, 0x18, znot);
  setup_single_opcode(5, 6, VAR, 0x19, zcall_vn);
  setup_single_opcode(5, 6, VAR, 0x1a, zcall_vn2);
  setup_single_opcode(5, 6, VAR, 0x1b, ztokenise);
  setup_single_opcode(5, 6, VAR, 0x1c, zencode_text);
  setup_single_opcode(5, 6, VAR, 0x1d, zcopy_table);
  setup_single_opcode(5, 6, VAR, 0x1e, zprint_table);
  setup_single_opcode(5, 6, VAR, 0x1f, zcheck_arg_count);

  setup_single_opcode(5, 6, EXT, 0x00, zsave5);
  setup_single_opcode(5, 6, EXT, 0x01, zrestore5);
  setup_single_opcode(5, 6, EXT, 0x02, zlog_shift);
  setup_single_opcode(5, 6, EXT, 0x03, zart_shift);
  setup_single_opcode(5, 6, EXT, 0x04, zset_font);
  setup_single_opcode(6, 6, EXT, 0x05, znop); /* XXX draw_picture */
  setup_single_opcode(6, 6, EXT, 0x06, zpicture_data);
  setup_single_opcode(6, 6, EXT, 0x07, znop); /* XXX erase_picture */
  setup_single_opcode(6, 6, EXT, 0x08, znop); /* XXX set_margins */
  setup_single_opcode(5, 6, EXT, 0x09, zsave_undo);
  setup_single_opcode(5, 6, EXT, 0x0a, zrestore_undo);
  setup_single_opcode(5, 6, EXT, 0x0b, zprint_unicode);
  setup_single_opcode(5, 6, EXT, 0x0c, zcheck_unicode);
  setup_single_opcode(5, 6, EXT, 0x0d, zset_true_colour);
  setup_single_opcode(6, 6, EXT, 0x10, znop); /* XXX move_window */
  setup_single_opcode(6, 6, EXT, 0x11, znop); /* XXX window_size */
  setup_single_opcode(6, 6, EXT, 0x12, znop); /* XXX window_style */
  setup_single_opcode(6, 6, EXT, 0x13, zget_wind_prop);
  setup_single_opcode(6, 6, EXT, 0x14, znop); /* XXX scroll_window */
  setup_single_opcode(6, 6, EXT, 0x15, zpop_stack);
  setup_single_opcode(6, 6, EXT, 0x16, znop); /* XXX read_mouse */
  setup_single_opcode(6, 6, EXT, 0x17, znop); /* XXX mouse_window */
  setup_single_opcode(6, 6, EXT, 0x18, zpush_stack);
  setup_single_opcode(6, 6, EXT, 0x19, znop); /* XXX put_wind_prop */
  setup_single_opcode(6, 6, EXT, 0x1a, zprint_form);
  setup_single_opcode(6, 6, EXT, 0x1b, zmake_menu);
  setup_single_opcode(6, 6, EXT, 0x1c, znop); /* XXX picture_table */
  setup_single_opcode(6, 6, EXT, 0x1d, zbuffer_screen);

  /* Zoom extensions. */
  setup_single_opcode(5, 6, EXT, 0x80, zstart_timer);
  setup_single_opcode(5, 6, EXT, 0x81, zstop_timer);
  setup_single_opcode(5, 6, EXT, 0x82, zread_timer);
  setup_single_opcode(5, 6, EXT, 0x83, zprint_timer);
}

void process_instructions(void)
{
  if(njumps <= ++ilevel)
  {
    jumps = realloc(jumps, ++njumps * sizeof *jumps);
    if(jumps == NULL) die("unable to allocate memory for jump buffer");
  }

  switch(setjmp(jumps[ilevel]))
  {
    case 1: /* Normal break from interrupt. */
      return;
    case 2: /* Special break: interrupt_reset() called, so keep interpreting. */
      break;
  }

  while(true)
  {
    uint8_t opcode;

#ifdef ZTERP_GLK_TICK
    glk_tick();
#endif

    current_instruction = pc;
    opcode = byte(pc++);

    /* long 2OP */
    if(opcode < 0x80)
    {
      znargs = 2;

      if(opcode & 0x40) zargs[0] = variable(byte(pc++));
      else              zargs[0] = byte(pc++);

      if(opcode & 0x20) zargs[1] = variable(byte(pc++));
      else              zargs[1] = byte(pc++);
    }

    /* short 1OP */
    else if(opcode < 0xb0)
    {
      znargs = 1;

      if(opcode & 0x20) /* variable */
      {
        zargs[0] = variable(byte(pc++));
      }
      else if(opcode & 0x10) /* small constant */
      {
        zargs[0] = byte(pc++);
      }
      else /* large constant */
      {
        zargs[0] = word(pc);
        pc += 2;
      }
    }

    /* short 0OP (plus EXT) */
    else if(opcode < 0xc0)
    {
      znargs = 0;
    }

    /* Double variable VAR */
    else if(opcode == 0xec || opcode == 0xfa)
    {
      uint8_t types1, types2;

      znargs = 0;

      types1 = byte(pc++);
      types2 = byte(pc++);
      decode_var(types1);
      decode_var(types2);
    }

    /* variable 2OP and VAR */
    else
    {
      znargs = 0;

      decode_var(byte(pc++));
    }

    op_call(opcode);
  }
}
