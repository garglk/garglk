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

BOOL savegame(void)
{
  BOOL result;
  strid_t file;

  if(automap_unexplore())
    return FALSE;

  file = n_file_prompt(fileusage_SavedGame | fileusage_BinaryMode,
		       filemode_Write);
  if(!file)
    return FALSE;

  result = savequetzal(file);

  glk_stream_close(file, NULL);

  return result;
}


void op_save1(void)
{
  if(!savegame()) {
    mop_skip_branch();
  } else {
    mop_take_branch();
  }
}


void op_save4(void)
{
  if(!savegame()) {
    mop_store_result(0);
  } else {
    mop_store_result(1);
  }
}


void op_save5(void)
{
  unsigned i;
  char filename[256];
  unsigned length;
  strid_t file = NULL;
  offset end;
  switch(numoperands) {
  case 0:
    op_save4();
    return;
  case 1: 
    n_show_error(E_INSTR, "call save with bad number of operands", numoperands);
    mop_store_result(0);
    return;
  case 2:
    file = n_file_prompt(fileusage_Data | fileusage_BinaryMode,
			 filemode_Write);
    break;
  default:
    length = LOBYTE(operand[2]);
    if(length > 13)
      n_show_port(E_INSTR, "save with filename > 13 characters", length);
    for(i = 0; i < length; i++)
      filename[i] = glk_char_to_upper(LOBYTE(operand[2] + 1 + i));
    filename[length] = 0;
    file = n_file_name(fileusage_Data | fileusage_BinaryMode,
		       filemode_Write, filename);
    break;
  }
  if(!file) {
    mop_store_result(0);
    return;
  }
  end = ((offset) operand[0]) + operand[1];
  if(end > 65535 || end > total_size) {
    n_show_error(E_MEMORY, "attempt to save data out of range", end);
    mop_store_result(0);
    return;
  }

  w_glk_put_buffer_stream(file, (char *) z_memory + operand[0], operand[1]);
  glk_stream_close(file, NULL);

  mop_store_result(1);
}


BOOL restoregame(void)
{
  BOOL result;
  strid_t file;

  if(automap_unexplore())
    return FALSE;

  file = n_file_prompt(fileusage_SavedGame | fileusage_BinaryMode,
		       filemode_Read);
  if(!file)
    return FALSE;

  result = restorequetzal(file);

  glk_stream_close(file, NULL);

  if(result) {
    glui32 wid, hei;
    z_find_size(&wid, &hei);
    set_header(wid, hei);
  }

  return result;
}


void op_restore1(void)
{
  if(!restoregame())
    mop_skip_branch();
  else
    mop_take_branch();
}

void op_restore4(void)
{
  if(!restoregame())
    mop_store_result(0);
  else
    mop_store_result(2);
}


void op_restore5(void)
{
  int i;
  char filename[256];
  int length;
  strid_t file;
  offset end;
  switch(numoperands) {
  case 0:
    op_restore4();
    return;
  case 1: 
    n_show_error(E_INSTR, "call restore with bad number of operands", numoperands);
    mop_store_result(0);
    return;
  case 2:
    file = n_file_prompt(fileusage_Data | fileusage_BinaryMode,
			 filemode_Read);
    break;
  default:
    length = LOBYTE(operand[2]);
    if(length > 13)
      n_show_port(E_INSTR, "save with filename > 13 characters", length);
    for(i = 0; i < length; i++)
      filename[i] = glk_char_to_upper(LOBYTE(operand[2] + 1 + i));
    filename[length] = 0;
    file = n_file_name(fileusage_Data | fileusage_BinaryMode,
		       filemode_Read, filename);
  }
  if(!file) {
    mop_store_result(0);
    return;
  }
  end = ((offset) operand[0]) + operand[1];
  if(end > 65535 || end > dynamic_size) {
    n_show_error(E_MEMORY, "attempt to restore data out of range", end);
    mop_store_result(0);
    return;
  }

  length = glk_get_buffer_stream(file, (char *) z_memory + operand[0],
				 operand[1]);
  glk_stream_close(file, NULL);
  mop_store_result(length);
}


void op_restart(void)
{
  if(automap_unexplore())
    return;
  z_init(current_zfile);
}


void op_save_undo(void)
{
  if(saveundo(TRUE)) {
    mop_store_result(1);
  } else {
    mop_store_result(0);
  }
}


void op_restore_undo(void)
{
  if(!restoreundo())
    mop_store_result(0);
}


void op_quit(void)
{
  if(automap_unexplore())
    return;
  z_close();
  /* puts("@quit\n"); */
  glk_exit();
}


BOOL check_game_for_save(strid_t gamefile, zword release, const char serial[6],
			 zword checksum)
{
  int i;
  unsigned char header[64];
  glk_stream_set_position(gamefile, 0, seekmode_Start);
  if(glk_get_buffer_stream(gamefile, (char *) header, 64) != 64)
    return FALSE;
  if(header[HD_ZVERSION] == 0 || header[HD_ZVERSION] > 8)
    return FALSE;
  if(MSBdecodeZ(header + HD_RELNUM) != release)
    return FALSE;
  if(MSBdecodeZ(header + HD_CHECKSUM) != checksum)
    return FALSE;
  for(i = 0; i < 6; i++) {
    if(header[HD_SERNUM + i] != serial[i])
      return FALSE;
  }
  return TRUE;
}
