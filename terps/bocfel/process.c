/*-
 * Copyright 2010-2012 Chris Spiegel.
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
#include <setjmp.h>

#ifdef ZTERP_GLK
#include <glk.h>
#endif

#include "process.h"
#include "branch.h"
#include "dict.h"
#include "math.h"
#include "memory.h"
#include "objects.h"
#include "random.h"
#include "screen.h"
#include "stack.h"
#include "table.h"
#include "util.h"
#include "zoom.h"
#include "zterp.h"

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
static long ilevel = -1;

long interrupt_level(void)
{
  return ilevel;
}

/* When this is called, the interrupt at level “level” will stop
 * running: if a single interrupt is running, then break_from(1) will
 * stop the interrupt, going back to the main program.  Breaking from
 * interrupt level 0 (which is not actually an interrupt) will end the
 * program.  This is how @quit is implemented.
 */
void break_from(long level)
{
  ilevel = level - 1;
  longjmp(jumps[level], 1);
}

/* If a restore happens inside of an interrupt, the level needs to be
 * set back to 0, but without a longjmp(), so break_from() cannot be
 * used.
 */
void reset_level(void)
{
  ilevel = 0;
}

/* To signal a restart, longjmp() is called with 2; this advises
 * process_instructions() to restart the story file and then continue
 * execution, whereas a value of 1 tells it to return immediately.
 */
static void zrestart(void)
{
  ilevel = 0;
  longjmp(jumps[0], 2);
}

/* Returns 1 if decoded, 0 otherwise (omitted) */
static int decode_base(uint8_t type, uint16_t *loc)
{
  if     (type == 0) *loc = WORD(pc++);		/* Large constant. */
  else if(type == 1) *loc = BYTE(pc);		/* Small constant. */
  else if(type == 2) *loc = variable(BYTE(pc));	/* Variable. */
  else               return 0;			/* Omitted. */

  pc++;

  return 1;
}

static void decode_var(uint8_t types)
{
  uint16_t ret;

  if(!decode_base((types >> 6) & 0x03, &ret)) return;
  zargs[znargs++] = ret;
  if(!decode_base((types >> 4) & 0x03, &ret)) return;
  zargs[znargs++] = ret;
  if(!decode_base((types >> 2) & 0x03, &ret)) return;
  zargs[znargs++] = ret;
  if(!decode_base((types >> 0) & 0x03, &ret)) return;
  zargs[znargs++] = ret;
}

/* op[0] is 0OP, op[1] is 1OP, etc */
static void (*op[5][256])(void);
static const char *opnames[5][256];
enum { ZERO, ONE, TWO, VAR, EXT };

#define op_call(opt, opnumber)	(op[opt][opnumber]())

/* This nifty trick is from Frotz. */
static void zextended(void)
{
  uint8_t opnumber = BYTE(pc++);

  decode_var(BYTE(pc++));

  /* §14.2.1
   * The exception for 0x80–0x83 is for the Zoom extensions.
   * Standard 1.1 implicitly updates §14.2.1 to recommend ignoring
   * opcodes in the range EXT:30 to EXT:255, due to the fact that
   * @buffer_screen is EXT:29.
   */
  if(opnumber > 0x1d && (opnumber < 0x80 || opnumber > 0x83)) return;

  op_call(EXT, opnumber);
}

static void illegal_opcode(void)
{
#ifndef ZTERP_NO_SAFETY_CHECKS
  die("illegal opcode (pc = 0x%lx)", zassert_pc);
#else
  die("illegal opcode");
#endif
}

void setup_opcodes(void)
{
  for(int i = 0; i < 5; i++)
  {
    for(int j = 0; j < 256; j++)
    {
      op[i][j] = illegal_opcode;
    }
  }
#define OP(args, opcode, fn) do { op[args][opcode] = fn; opnames[args][opcode] = #fn; } while(0)
  OP(ZERO, 0x00, zrtrue);
  OP(ZERO, 0x01, zrfalse);
  OP(ZERO, 0x02, zprint);
  OP(ZERO, 0x03, zprint_ret);
  OP(ZERO, 0x04, znop);
  if(zversion <= 4) OP(ZERO, 0x05, zsave);
  if(zversion <= 4) OP(ZERO, 0x06, zrestore);
  OP(ZERO, 0x07, zrestart);
  OP(ZERO, 0x08, zret_popped);
  if(zversion <= 4) OP(ZERO, 0x09, zpop);
  else              OP(ZERO, 0x09, zcatch);
  OP(ZERO, 0x0a, zquit);
  OP(ZERO, 0x0b, znew_line);
  if     (zversion == 3) OP(ZERO, 0x0c, zshow_status);
  else if(zversion >= 4) OP(ZERO, 0x0c, znop); /* §15: Technically illegal in V4+, but a V5 Wishbringer accidentally uses this opcode. */
  if(zversion >= 3) OP(ZERO, 0x0d, zverify);
  if(zversion >= 5) OP(ZERO, 0x0e, zextended);
  if(zversion >= 5) OP(ZERO, 0x0f, zpiracy);

  OP(ONE, 0x00, zjz);
  OP(ONE, 0x01, zget_sibling);
  OP(ONE, 0x02, zget_child);
  OP(ONE, 0x03, zget_parent);
  OP(ONE, 0x04, zget_prop_len);
  OP(ONE, 0x05, zinc);
  OP(ONE, 0x06, zdec);
  OP(ONE, 0x07, zprint_addr);
  if(zversion >= 4) OP(ONE, 0x08, zcall_1s);
  OP(ONE, 0x09, zremove_obj);
  OP(ONE, 0x0a, zprint_obj);
  OP(ONE, 0x0b, zret);
  OP(ONE, 0x0c, zjump);
  OP(ONE, 0x0d, zprint_paddr);
  OP(ONE, 0x0e, zload);
  if(zversion <= 4) OP(ONE, 0x0f, znot);
  else              OP(ONE, 0x0f, zcall_1n);

  OP(TWO, 0x01, zje);
  OP(TWO, 0x02, zjl);
  OP(TWO, 0x03, zjg);
  OP(TWO, 0x04, zdec_chk);
  OP(TWO, 0x05, zinc_chk);
  OP(TWO, 0x06, zjin);
  OP(TWO, 0x07, ztest);
  OP(TWO, 0x08, zor);
  OP(TWO, 0x09, zand);
  OP(TWO, 0x0a, ztest_attr);
  OP(TWO, 0x0b, zset_attr);
  OP(TWO, 0x0c, zclear_attr);
  OP(TWO, 0x0d, zstore);
  OP(TWO, 0x0e, zinsert_obj);
  OP(TWO, 0x0f, zloadw);
  OP(TWO, 0x10, zloadb);
  OP(TWO, 0x11, zget_prop);
  OP(TWO, 0x12, zget_prop_addr);
  OP(TWO, 0x13, zget_next_prop);
  OP(TWO, 0x14, zadd);
  OP(TWO, 0x15, zsub);
  OP(TWO, 0x16, zmul);
  OP(TWO, 0x17, zdiv);
  OP(TWO, 0x18, zmod);
  if(zversion >= 4) OP(TWO, 0x19, zcall_2s);
  if(zversion >= 5) OP(TWO, 0x1a, zcall_2n);
  if(zversion >= 5) OP(TWO, 0x1b, zset_colour);
  if(zversion >= 5) OP(TWO, 0x1c, zthrow);

  OP(VAR, 0x00, zcall);
  OP(VAR, 0x01, zstorew);
  OP(VAR, 0x02, zstoreb);
  OP(VAR, 0x03, zput_prop);
  OP(VAR, 0x04, zread);
  OP(VAR, 0x05, zprint_char);
  OP(VAR, 0x06, zprint_num);
  OP(VAR, 0x07, zrandom);
  OP(VAR, 0x08, zpush);
  OP(VAR, 0x09, zpull);
  if(zversion >= 3) OP(VAR, 0x0a, zsplit_window);
  if(zversion >= 3) OP(VAR, 0x0b, zset_window);
  if(zversion >= 4) OP(VAR, 0x0c, zcall_vs2);
  if(zversion >= 4) OP(VAR, 0x0d, zerase_window);
  if(zversion >= 4) OP(VAR, 0x0e, zerase_line);
  if(zversion >= 4) OP(VAR, 0x0f, zset_cursor);
  if(zversion >= 4) OP(VAR, 0x10, zget_cursor);
  if(zversion >= 4) OP(VAR, 0x11, zset_text_style);
  if(zversion >= 4) OP(VAR, 0x12, znop); /* XXX buffer_mode */
  if(zversion >= 3) OP(VAR, 0x13, zoutput_stream);
  if(zversion >= 3) OP(VAR, 0x14, zinput_stream);
  if(zversion >= 3) OP(VAR, 0x15, zsound_effect);
  if(zversion >= 4) OP(VAR, 0x16, zread_char);
  if(zversion >= 4) OP(VAR, 0x17, zscan_table);
  if(zversion >= 5) OP(VAR, 0x18, znot);
  if(zversion >= 5) OP(VAR, 0x19, zcall_vn);
  if(zversion >= 5) OP(VAR, 0x1a, zcall_vn2);
  if(zversion >= 5) OP(VAR, 0x1b, ztokenise);
  if(zversion >= 5) OP(VAR, 0x1c, zencode_text);
  if(zversion >= 5) OP(VAR, 0x1d, zcopy_table);
  if(zversion >= 5) OP(VAR, 0x1e, zprint_table);
  if(zversion >= 5) OP(VAR, 0x1f, zcheck_arg_count);

  if(zversion >= 5) OP(EXT, 0x00, zsave5);
  if(zversion >= 5) OP(EXT, 0x01, zrestore5);
  if(zversion >= 5) OP(EXT, 0x02, zlog_shift);
  if(zversion >= 5) OP(EXT, 0x03, zart_shift);
  if(zversion >= 5) OP(EXT, 0x04, zset_font);
  if(zversion >= 6) OP(EXT, 0x05, znop); /* XXX draw_picture */
  if(zversion >= 6) OP(EXT, 0x06, zpicture_data);
  if(zversion >= 6) OP(EXT, 0x07, znop); /* XXX erase_picture */
  if(zversion >= 6) OP(EXT, 0x08, znop); /* XXX set_margins */
  if(zversion >= 5) OP(EXT, 0x09, zsave_undo);
  if(zversion >= 5) OP(EXT, 0x0a, zrestore_undo);
  if(zversion >= 5) OP(EXT, 0x0b, zprint_unicode);
  if(zversion >= 5) OP(EXT, 0x0c, zcheck_unicode);
  if(zversion >= 5) OP(EXT, 0x0d, zset_true_colour);
  if(zversion >= 6) OP(EXT, 0x10, znop); /* XXX move_window */
  if(zversion >= 6) OP(EXT, 0x11, znop); /* XXX window_size */
  if(zversion >= 6) OP(EXT, 0x12, znop); /* XXX window_style */
  if(zversion >= 6) OP(EXT, 0x13, zget_wind_prop);
  if(zversion >= 6) OP(EXT, 0x14, znop); /* XXX scroll_window */
  if(zversion >= 6) OP(EXT, 0x15, zpop_stack);
  if(zversion >= 6) OP(EXT, 0x16, znop); /* XXX read_mouse */
  if(zversion >= 6) OP(EXT, 0x17, znop); /* XXX mouse_window */
  if(zversion >= 6) OP(EXT, 0x18, zpush_stack);
  if(zversion >= 6) OP(EXT, 0x19, znop); /* XXX put_wind_prop */
  if(zversion >= 6) OP(EXT, 0x1a, zprint_form);
  if(zversion >= 6) OP(EXT, 0x1b, zmake_menu);
  if(zversion >= 6) OP(EXT, 0x1c, znop); /* XXX picture_table */
  if(zversion >= 6) OP(EXT, 0x1d, zbuffer_screen);

  /* Zoom extensions. */
  OP(EXT, 0x80, zstart_timer);
  OP(EXT, 0x81, zstop_timer);
  OP(EXT, 0x82, zread_timer);
  OP(EXT, 0x83, zprint_timer);
#undef OP
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
    case 2: /* Special break: a restart was requested. */
      {
        /* §6.1.3: Flags2 is preserved on a restart. */
        uint16_t flags2 = WORD(0x10);

        process_story();

        STORE_WORD(0x10, flags2);
      }
      break;
  }

  while(1)
  {
    uint8_t opcode;

#if defined(ZTERP_GLK) && defined(ZTERP_GLK_TICK)
    glk_tick();
#endif

    ZPC(pc);

    opcode = BYTE(pc++);

    /* long 2OP */
    if(opcode < 0x80)
    {
      znargs = 2;

      if(opcode & 0x40) zargs[0] = variable(BYTE(pc++));
      else              zargs[0] = BYTE(pc++);

      if(opcode & 0x20) zargs[1] = variable(BYTE(pc++));
      else              zargs[1] = BYTE(pc++);

      op_call(TWO, opcode & 0x1f);
    }

    /* short 1OP */
    else if(opcode < 0xb0)
    {
      znargs = 1;

      if(opcode < 0x90) /* large constant */
      {
        zargs[0] = WORD(pc);
        pc += 2;
      }
      else if(opcode < 0xa0) /* small constant */
      {
        zargs[0] = BYTE(pc++);
      }
      else /* variable */
      {
        zargs[0] = variable(BYTE(pc++));
      }

      op_call(ONE, opcode & 0x0f);
    }

    /* short 0OP (plus EXT) */
    else if(opcode < 0xc0)
    {
      znargs = 0;

      op_call(ZERO, opcode & 0x0f);
    }

    /* variable 2OP */
    else if(opcode < 0xe0)
    {
      znargs = 0;

      decode_var(BYTE(pc++));

      op_call(TWO, opcode & 0x1f);
    }

    /* Double variable VAR */
    else if(opcode == 0xec || opcode == 0xfa)
    {
      uint8_t types1, types2;

      znargs = 0;

      types1 = BYTE(pc++);
      types2 = BYTE(pc++);
      decode_var(types1);
      decode_var(types2);

      op_call(VAR, opcode & 0x1f);
    }

    /* variable VAR */
    else
    {
      znargs = 0;

      read_pc = pc - 1;

      decode_var(BYTE(pc++));

      op_call(VAR, opcode & 0x1f);
    }
  }
}
