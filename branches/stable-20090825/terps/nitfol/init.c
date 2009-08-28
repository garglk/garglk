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

enum z_caps { HAS_STATUSLINE, HAS_COLOR, HAS_CHARGRAPH, HAS_BOLD, HAS_ITALIC,
	      HAS_FIXED, HAS_SOUND, HAS_GRAPHICS, HAS_TIMER, HAS_MOUSE,
	      HAS_DEFVAR, IS_TRANS, FORCE_FIXED, HAS_UNDO, HAS_MENU,
	      DO_TANDY,
	      HAS_ENDNUM };

struct z_cap_entry {
  enum z_caps c;
  int flagnum;    /* 1 or 2 */
  int bit;
  int min_zversion;
  int max_zversion;
  BOOL reverse;
};

static const struct z_cap_entry z_cap_table[] = {
  { DO_TANDY,        1,  3,  1,  3, FALSE },
  { HAS_STATUSLINE,  1,  4,  1,  3, TRUE },
  { HAS_STATUSLINE,  1,  5,  1,  3, FALSE },
  { HAS_DEFVAR,      1,  6,  1,  3, FALSE },
  { HAS_COLOR,       1,  0,  5, 99, FALSE },
  { HAS_GRAPHICS,    1,  1,  6,  6, FALSE },
  { HAS_BOLD,        1,  2,  4, 99, FALSE },
  { HAS_ITALIC,      1,  3,  4, 99, FALSE },
  { HAS_FIXED,       1,  4,  4, 99, FALSE },
  { HAS_SOUND,       1,  5,  6, 99, FALSE },
  { HAS_TIMER,       1,  7,  4, 99, FALSE },
  { IS_TRANS,        2,  0,  1, 99, FALSE },
  { FORCE_FIXED,     2,  1,  3, 99, FALSE },
  { HAS_CHARGRAPH,   2,  3,  5, 99, FALSE },
  { HAS_UNDO,        2,  4,  5, 99, FALSE },
  { HAS_MOUSE,       2,  5,  5, 99, FALSE },
  { HAS_SOUND,       2,  7,  5, 99, FALSE },
  { HAS_MENU,        2,  8,  6,  6, FALSE }
};

static BOOL cur_cap[HAS_ENDNUM];

static void investigate_suckage(glui32 *wid, glui32 *hei)
{
  winid_t w1, w2;

  *wid = 70; *hei = 24; /* sensible defaults if we can't tell */

  glk_stylehint_set(wintype_AllTypes, style_User2,
		    stylehint_TextColor, 0x00ff0000);
  glk_stylehint_set(wintype_AllTypes, style_Subheader,
		    stylehint_Weight, 1);

  w1 = glk_window_open(0, 0, 0, wintype_TextBuffer, 0);
  if(!w1)
    glk_exit();  /* Run screaming! */
  w2 = glk_window_open(w1, winmethod_Above | winmethod_Fixed, 1,
		       wintype_TextGrid, 0);

  if(w2) {
    winid_t o;
    o = glk_window_get_parent(w2);
    glk_window_set_arrangement(o, winmethod_Above | winmethod_Proportional,
			       100, w2);
    glk_window_get_size(w2, wid, hei);
    glk_window_set_arrangement(o, winmethod_Above | winmethod_Fixed,
			       1, w2);
  }

  cur_cap[HAS_STATUSLINE] = (w2 != 0);
  cur_cap[HAS_COLOR]  = glk_style_distinguish(w1, style_Normal, style_User2);
  cur_cap[HAS_CHARGRAPH] = 0;
  cur_cap[HAS_BOLD] = glk_style_distinguish(w1, style_Normal, style_Subheader);
  cur_cap[HAS_ITALIC] = glk_style_distinguish(w1, style_Normal, style_Emphasized);
  cur_cap[HAS_FIXED] = glk_style_distinguish(w1, style_Normal, style_Preformatted);
#ifdef GLK_MODULE_SOUND
  cur_cap[HAS_SOUND] = glk_gestalt(gestalt_Sound, 0);
#else
  cur_cap[HAS_SOUND] = FALSE;
#endif
#ifdef GLK_MODULE_IMAGE
  cur_cap[HAS_GRAPHICS] = glk_gestalt(gestalt_Graphics, 0);
#else
  cur_cap[HAS_GRAPHICS] = FALSE;
#endif
  cur_cap[HAS_TIMER] = glk_gestalt(gestalt_Timer, 0);
  cur_cap[HAS_MOUSE] = glk_gestalt(gestalt_MouseInput, wintype_TextGrid);

  cur_cap[HAS_DEFVAR] = TRUE;

  cur_cap[HAS_UNDO] = TRUE;
  cur_cap[HAS_MENU] = FALSE;

  if(w1)
    glk_window_close(w1, NULL);
  if(w2)
    glk_window_close(w2, NULL);
}


void set_header(glui32 width, glui32 height)
{
  unsigned i;
  zword flags[3];
  flags[1] = LOBYTE(HD_FLAGS1);
  flags[2] = LOWORD(HD_FLAGS2);

  cur_cap[DO_TANDY] = do_tandy;

  cur_cap[IS_TRANS] = is_transcripting();
  cur_cap[FORCE_FIXED] = is_fixed;

  for(i = 0; i < sizeof(z_cap_table) / sizeof(*z_cap_table); i++) {
    if(zversion >= z_cap_table[i].min_zversion
       && zversion <= z_cap_table[i].max_zversion) {
      if(cur_cap[z_cap_table[i].c])
	flags[z_cap_table[i].flagnum] |= 1 << z_cap_table[i].bit;
      else
	flags[z_cap_table[i].flagnum] &= ~(1 << z_cap_table[i].bit);
    }
  }

  LOBYTEwrite(HD_FLAGS1, flags[1]);
  LOWORDwrite(HD_FLAGS2, flags[2]);

  LOBYTEwrite(HD_TERPNUM, interp_num);
  LOBYTEwrite(HD_TERPVER, interp_ver);

  /* We should be allowed 8 bytes here, but inform 6 (incorrectly) expects
     this space not to be used, so only use zeroed bytes */
  if(username)
    for(i = 0; i < 8 && username[i] && !LOBYTE(HD_USERID + i); i++)
      LOBYTEwrite(HD_USERID + i, username[i]);

  if(zversion == 6) {
    width = 320;
    height = 200;
  }


  if(height > 255)
    height = 255;
  LOBYTEwrite(HD_SCR_HEIGHT, height);    /* screen height (255 = infinite) */
  LOBYTEwrite(HD_SCR_WIDTH, width);      /* screen width */
  if(zversion >= 5) {
    LOWORDwrite(HD_SCR_WUNIT, width);    /* screen width in units */
    LOWORDwrite(HD_SCR_HUNIT, height);   /* screen height in units */
  }

  if(zversion != 6) {
    LOBYTEwrite(HD_FNT_WIDTH, 1);        /* font width/height in units */
    LOBYTEwrite(HD_FNT_HEIGHT, 1);       /* font height/width in units */
  } else {
    LOBYTEwrite(HD_FNT_WIDTH, 8);
    LOBYTEwrite(HD_FNT_HEIGHT, 8);
  }

  LOBYTEwrite(HD_DEF_BACK, 1);           /* default background color */
  LOBYTEwrite(HD_DEF_FORE, 1);           /* default foreground color */

  /* Well, we're close enough given the limitations of Glk, so go ahead
     and lie about the impossible stuff and claim spec 1.0 compliance */
  LOBYTEwrite(HD_STD_REV, 1);
  LOBYTEwrite(HD_STD_REV + 1, 0);
}

static void check_ascii_mode(void)
{
  BOOL hascr = FALSE;
  BOOL haslf = FALSE;
  BOOL hascrnolf = FALSE;
  BOOL haslfnocr = FALSE;
  BOOL has8bit = FALSE;
  offset i;
  for(i = 0; i < total_size; i++) {
    if(z_memory[i] & 0x80)
      has8bit = TRUE;
    else if(z_memory[i] == 0x0a) {
      haslf = TRUE;
      if(i && z_memory[i-1] != 0x0d)
	haslfnocr = TRUE;
    }
    else if(z_memory[i] == 0x0d) {
      hascr = TRUE;
      if(i+1 < total_size && z_memory[i+1] != 0x0a)
	hascrnolf = TRUE;
    }
  }
  if(!has8bit)
    n_show_error(E_CORRUPT, "All bytes are 7 bit; top bits were likely stripped in transfer", 0);
  else if(!hascr)
    n_show_error(E_CORRUPT, "No CR bytes; likely transfered in ASCII mode", 0);
  else if(!haslf)
    n_show_error(E_CORRUPT, "No LF bytes; likely transfered in ASCII mode", 0);
  else if(!hascrnolf)
    n_show_error(E_CORRUPT, "All CR are followed by LFs; likely transfered in ASCII mode", 0);
  else if(!haslfnocr)
    n_show_error(E_CORRUPT, "All LFs are preceded by CRs; likely transfered in ASCII mode", 0);
}


/* Loads header into global values.  Returns true if could be a valid header */
BOOL load_header(strid_t zfile, offset filesize, BOOL report_errors)
{
  zbyte header[64];

  if(glk_get_buffer_stream(zfile, (char *) header, 64) != 64) {
    if(report_errors)
      n_show_error(E_SYSTEM, "file too small", 0);
    return FALSE;
  }

  zversion        = header[HD_ZVERSION];
  switch(zversion) {
  case 1: case 2: case 3:
    granularity = 2; break;
  case 4: case 5: case 6: case 7:
    granularity = 4; break;
  case 8:
    granularity = 8; break;
  default:
    if(report_errors)
      n_show_error(E_VERSION, "unknown version number or not zcode", zversion);
    return FALSE;
  }
  
  high_mem_mark   = MSBdecodeZ(header + HD_HIMEM);
  PC              = MSBdecodeZ(header + HD_INITPC);
  z_dictionary    = MSBdecodeZ(header + HD_DICT);
  z_propdefaults  = MSBdecodeZ(header + HD_OBJTABLE);
  z_globaltable   = MSBdecodeZ(header + HD_GLOBVAR);
  dynamic_size    = MSBdecodeZ(header + HD_STATMEM);
  z_synonymtable  = MSBdecodeZ(header + HD_ABBREV);
  
  switch(zversion) {
  case 1: case 2:
    game_size     = filesize;
    break;
  case 3:
    game_size     = ((offset) MSBdecodeZ(header + HD_LENGTH)) * 2;
    break;
  case 4: case 5:
    game_size     = ((offset) MSBdecodeZ(header + HD_LENGTH)) * 4;
    break;
  case 6: case 7: case 8:
    game_size     = ((offset) MSBdecodeZ(header + HD_LENGTH)) * 8;
    break;
  }
  if(zversion == 6 || zversion == 7) {
    rstart        = ((offset) MSBdecodeZ(header + HD_RTN_OFFSET)) * 8;
    sstart        = ((offset) MSBdecodeZ(header + HD_STR_OFFSET)) * 8;
  } else {
    rstart        = 0;
    sstart        = 0;
  }

  /* Check consistency of stuff */

  if(filesize < game_size) {
    if(report_errors)
      n_show_error(E_CORRUPT, "file on a diet", filesize);
    return FALSE;
  }

  if(dynamic_size > game_size) {
    if(report_errors)
      n_show_error(E_CORRUPT, "dynamic memory length > game size", dynamic_size);
    return FALSE;
  }
  
  if(high_mem_mark < dynamic_size) {
    if(report_errors)
      n_show_error(E_CORRUPT, "dynamic memory overlaps high memory", dynamic_size);
  }

  if(PC > game_size) {
    if(report_errors)
      n_show_error(E_CORRUPT, "initial PC greater than game size", PC);
    return FALSE;
  }

  if(PC < 64) {
    if(report_errors)
      n_show_error(E_CORRUPT, "initial PC in header", PC);
    return FALSE;
  }
  
  return TRUE;
}


void z_init(strid_t zfile)
{
  offset bytes_read;
  offset i;
  glui32 width, height;

  z_random(0); /* Initialize random number generator */

  init_windows(is_fixed, 80, 24);

  glk_stream_set_position(zfile, zfile_offset, seekmode_Start);
  if(!load_header(zfile, total_size, TRUE))
    n_show_fatal(E_CORRUPT, "couldn't load file", 0);

  n_free(z_memory);
  if(game_size <= 65535)
    z_memory = (zbyte *) n_malloc(65535);
  else
    z_memory = (zbyte *) n_malloc(game_size);

  glk_stream_set_position(zfile, zfile_offset, seekmode_Start);
  bytes_read = glk_get_buffer_stream(zfile, (char *) z_memory, game_size);
  if(bytes_read != game_size)
    n_show_fatal(E_SYSTEM, "unexpected number of bytes read", bytes_read);

  z_checksum = 0;
  if (zversion >= 3) {
    for(i = 0x40; i < game_size; i++)
      z_checksum += HIBYTE(i);
    z_checksum = ARITHMASK(z_checksum);

    if(LOWORD(HD_CHECKSUM) != 0 && z_checksum != LOWORD(HD_CHECKSUM)) {
      n_show_error(E_CORRUPT, "Checksum does not match", z_checksum);
      check_ascii_mode();
    }
  }


  
  init_stack(1024, 128);

  if(zversion == 6) {
    zword dummylocals[15];
    mop_call(PC, 0, dummylocals, 0);
  }

  kill_windows();
  investigate_suckage(&width, &height);
  if(height == 0)
    height = 1;
  if(width == 0)
    width = 80;
  set_header(width, height);
  init_windows(is_fixed, width, height);

  if(zversion <= 3) {
    opcodetable[OFFSET_0OP +  5] = op_save1;
    opcodetable[OFFSET_0OP +  6] = op_restore1;
  } else {
    opcodetable[OFFSET_0OP +  5] = op_save4;
    opcodetable[OFFSET_0OP +  6] = op_restore4;
  }
  if(zversion <= 4) {
    opcodetable[OFFSET_0OP +  9] = op_pop;
    opcodetable[OFFSET_1OP + 15] = op_not;
    opcodetable[OFFSET_VAR +  4] = op_sread;
  } else {
    opcodetable[OFFSET_0OP +  9] = op_catch;
    opcodetable[OFFSET_1OP + 15] = op_call_n;
    opcodetable[OFFSET_VAR +  4] = op_aread;
  }
  if(zversion == 6) {
    opcodetable[OFFSET_VAR + 11] = op_set_window6;
  } else {
    opcodetable[OFFSET_VAR + 11] = op_set_window;
  }

  objects_init();
  init_sound();

  in_timer = FALSE;
  exit_decoder = FALSE;
  time_ret = 0;

#ifdef DEBUGGING
  init_infix(0);
#endif

  if(!quiet) {
    output_string("Nitfol 0.5 Copyright 1999 Evin Robertson.\r");
#ifdef DEBUGGING    
    output_string("Nitfol comes with ABSOLUTELY NO WARRANTY; for details type \"/show w\".  This is free software, and you are welcome to change and distribute it under certain conditions; type \"/show copying\" for details.\r");
#else
    output_string("Nitfol is free software which comes with ABSOLUTELY NO WARRANTY.  It is covered by the GNU General Public License, and you are welcome to change and distribute it under certain conditions.  Read the file COPYING which should have been included in this distribution for details.\r");
#endif
  }
}


void z_close(void)
{
#ifdef DEBUGGING
  kill_infix();
#endif
  kill_sound();
  kill_undo();
  free_windows();
  kill_stack();
  n_free(z_memory);
}
