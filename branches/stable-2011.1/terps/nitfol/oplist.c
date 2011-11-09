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

static void op_nop(void) { ; }

static void op_XXX(void)
{
  zbyte ins = HIBYTE(oldPC);
  n_show_error(E_INSTR, "illegal instruction", ins);
}

#ifdef HEADER

/* offsets in opcodetable for various types */
#define OFFSET_0OP 0x00
#define OFFSET_1OP 0x10
#define OFFSET_2OP 0x20
#define OFFSET_VAR 0x40
#define OFFSET_EXT 0x60
#define OFFSET_END 0x80

typedef void (*op_func)(void);

#endif

op_func opcodetable[] = {
  /* 0OP */
  op_rtrue,         op_rfalse,        op_print,         op_print_ret,
  op_nop,           op_save4,         op_restore4,      op_restart,
  op_ret_popped,    op_catch,         op_quit,          op_new_line,
  op_show_status,   op_verify,        op_XXX,           op_piracy,
  /* 1OP */
  op_jz,            op_get_sibling,   op_get_child,     op_get_parent,
  op_get_prop_len,  op_inc,           op_dec,           op_print_addr,
  op_call_s,        op_remove_obj,    op_print_obj,     op_ret,
  op_jump,          op_print_paddr,   op_load,          op_call_n,
  /* 2OP */
  op_XXX,           op_je,            op_jl,            op_jg,
  op_dec_chk,       op_inc_chk,       op_jin,           op_test,
  op_or,            op_and,           op_test_attr,     op_set_attr,
  op_clear_attr,    op_store,         op_insert_obj,    op_loadw,
  op_loadb,         op_get_prop,      op_get_prop_addr, op_get_next_prop,
  op_add,           op_sub,           op_mul,           op_div,
  op_mod,           op_call_s,        op_call_n,        op_set_colour,
  op_throw,         op_XXX,           op_XXX,           op_XXX,    
  /* VAR */
  op_call_s,        op_storew,        op_storeb,        op_put_prop,
  op_aread,         op_print_char,    op_print_num,     op_random,
  op_push,          op_pull,          op_split_window,  op_set_window,
  op_call_s,        op_erase_window,  op_erase_line,    op_set_cursor,
  op_get_cursor,    op_set_text_style,op_buffer_mode,   op_output_stream,
  op_input_stream,  op_sound_effect,  op_read_char,     op_scan_table,   
  op_not,           op_call_n,        op_call_n,        op_tokenise,
  op_encode_text,   op_copy_table,    op_print_table,   op_check_arg_count,
  /* EXT */
  op_save5,         op_restore5,      op_log_shift,     op_art_shift,
  op_set_font,      op_draw_picture,  op_picture_data,  op_erase_picture,
  op_set_margins,   op_save_undo,     op_restore_undo,  op_print_unicode,
  op_check_unicode, op_XXX,           op_XXX,           op_XXX,    
  op_move_window,   op_window_size,   op_window_style,  op_get_wind_prop,
  op_scroll_window, op_pop_stack,     op_read_mouse,    op_mouse_window,
  op_push_stack,    op_put_wind_prop, op_print_form,    op_make_menu,
  op_picture_table, op_XXX,           op_XXX,           op_XXX
};

#ifdef DEBUGGING

opcodeinfo opcodelist[] = {
  /* 0OP */
  { "rtrue",           v1, vM,   0, 0, opNONE, 0 },
  { "rfalse",          v1, vM,   0, 0, opNONE, 0 },
  { "print",           v1, vM,   0, 0, opTEXTINLINE, 0 },
  { "print_ret",       v1, vM,   0, 0, opTEXTINLINE, 0 },
  { "nop",             v1, vM,   0, 0, opNONE, 0 },
  { "save",            v4, v4,   0, 0, opSTORES, 0 },
  { "restore",         v4, v4,   0, 0, opSTORES, 0 },
  { "restart",         v1, vM,   0, 0, opNONE, 0 },
  { "ret_popped",      v1, vM,   0, 0, opNONE, 0 },
  { "catch",           v5, vM,   0, 0, opSTORES, 0 },
  { "quit",            v1, vM,   0, 0, opNONE, 0 },
  { "new_line",        v1, vM,   0, 0, opNONE, 0 },
  { "show_status",     v3, v3,   0, 0, opNONE, 0 },
  { "verify",          v3, vM,   0, 0, opBRANCHES, 0 },
  { "extended",        v5, vM,   0, 0, opNONE, 0 },
  { "piracy",          v5, vM,   0, 0, opBRANCHES, 0 },
  /* 1OP */
  { "jz",              v1, vM,   1, 1, opBRANCHES, 0 },
  { "get_sibling",     v1, vM,   1, 1, opSTORES | opBRANCHES, 0 },
  { "get_child",       v1, vM,   1, 1, opSTORES | opBRANCHES, 0 },
  { "get_parent",      v1, vM,   1, 1, opSTORES, 0 },
  { "get_prop_len",    v1, vM,   1, 1, opSTORES, 0 },
  { "inc",             v1, vM,   1, 1, opNONE, 0 },
  { "dec",             v1, vM,   1, 1, opNONE, 0 },
  { "print_addr",      v1, vM,   1, 1, opNONE, 0 },
  { "call_1s",         v4, vM,   1, 1, opSTORES, 0 },
  { "remove_obj",      v1, vM,   1, 1, opNONE, 0 },
  { "print_obj",       v1, vM,   1, 1, opNONE, 0 },
  { "ret",             v1, vM,   1, 1, opNONE, 0 },
  { "jump",            v1, vM,   1, 1, opJUMPS, 0 },
  { "print_paddr",     v1, vM,   1, 1, opNONE, 0 },
  { "load",            v1, vM,   1, 1, opSTORES, 0 },
  { "call_1n",         v5, vM,   1, 1, opNONE, 0 },
  /* 2OP, 0 */
  { "XXX",             v1, vM,   0, 8, opNONE, 0 },
  { "je",              v1, vM,   1, 4, opBRANCHES, 0 },
  { "jl",              v1, vM,   2, 2, opBRANCHES, 0 },
  { "jg",              v1, vM,   2, 2, opBRANCHES, 0 },
  { "dec_chk",         v1, vM,   2, 2, opBRANCHES, 0 },
  { "inc_chk",         v1, vM,   2, 2, opBRANCHES, 0 },
  { "jin",             v1, vM,   2, 2, opBRANCHES, 0 },
  { "test",            v1, vM,   2, 2, opBRANCHES, 0 },
  { "or",              v1, vM,   2, 2, opNONE, 0 },
  { "and",             v1, vM,   2, 2, opNONE, 0 },
  { "test_attr",       v1, vM,   2, 2, opBRANCHES, 0 },
  { "set_attr",        v1, vM,   2, 2, opNONE, 0 },
  { "clear_attr",      v1, vM,   2, 2, opNONE, 0 },
  { "store",           v1, vM,   2, 2, opNONE, 0 },
  { "insert_obj",      v1, vM,   2, 2, opNONE, 0 },
  { "loadw",           v1, vM,   2, 2, opSTORES, 0 },
  { "loadb",           v1, vM,   2, 2, opSTORES, 0 },
  { "get_prop",        v1, vM,   2, 2, opSTORES, 0 },
  { "get_prop_addr",   v1, vM,   2, 2, opSTORES, 0 },
  { "get_next_prop",   v1, vM,   2, 2, opSTORES, 0 },
  { "add",             v1, vM,   2, 2, opSTORES, 0 },
  { "sub",             v1, vM,   2, 2, opSTORES, 0 },
  { "mul",             v1, vM,   2, 2, opSTORES, 0 },
  { "div",             v1, vM,   2, 2, opSTORES, 0 },
  { "mod",             v1, vM,   2, 2, opSTORES, 0 },
  { "call_2s",         v4, vM,   2, 2, opSTORES, 0 },
  { "call_2n",         v5, vM,   2, 2, opNONE, 0 },
  { "set_colour",      v5, vM,   2, 2, opNONE, 0 },
  { "throw",           v5, vM,   2, 2, opNONE, 0 },
  { "XXX",             v1, vM,   0, 8, opNONE, 0 },
  { "XXX",             v1, vM,   0, 8, opNONE, 0 },
  { "XXX",             v1, vM,   0, 8, opNONE, 0 },
  /* VAR */
  { "call_vs",         v1, vM,   1, 4, opSTORES, 0 },
  { "storew",          v1, vM,   3, 3, opNONE, 0 },
  { "storeb",          v1, vM,   3, 3, opNONE, 0 },
  { "put_prop",        v1, vM,   3, 3, opNONE, 0 },
  { "aread",           v5, vM,   0, 4, opSTORES, 0 },
  { "print_char",      v1, vM,   1, 1, opNONE, 0 },
  { "print_num",       v1, vM,   1, 1, opNONE, 0 },
  { "random",          v1, vM,   1, 1, opSTORES, 0 },
  { "push",            v1, vM,   1, 1, opNONE, 0 },
  { "pull",            v1, vM,   1, 1, opNONE, 0 },
  { "split_window",    v3, vM,   1, 1, opNONE, 0 },
  { "set_window",      v3, vM,   1, 1, opNONE, 0 },
  { "call_vs2",        v4, vM,   1, 8, opSTORES, 0 },
  { "erase_window",    v4, vM,   1, 1, opNONE, 0 },
  { "erase_line",      v4, vM,   1, 1, opNONE, 0 },
  { "set_cursor",      v4, vM,   2, 2, opNONE, 0 },
  { "get_cursor",      v4, vM,   1, 1, opNONE, 0 },
  { "set_text_style",  v4, vM,   1, 1, opNONE, 0 },
  { "buffer_mode",     v4, vM,   1, 1, opNONE, 0 },
  { "output_stream",   v5, vM,   1, 2, opNONE, 0 },
  { "input_stream",    v3, vM,   1, 1, opNONE, 0 },
  { "sound_effect",    v3, vM,   4, 4, opNONE, 0 },
  { "read_char",       v4, vM,   1, 3, opSTORES, 0 },
  { "scan_table",      v4, vM,   4, 4, opSTORES | opBRANCHES, 0 },
  { "not",             v5, vM,   1, 1, opSTORES, 0 },
  { "call_vn",         v5, vM,   1, 4, opNONE, 0 },
  { "call_vn2",        v5, vM,   1, 8, opNONE, 0 },
  { "tokenise",        v5, vM,   4, 4, opNONE, 0 },
  { "encode_text",     v5, vM,   4, 4, opNONE, 0 },
  { "copy_table",      v5, vM,   3, 3, opNONE, 0 },
  { "print_table",     v5, vM,   4, 4, opNONE, 0 },
  { "check_arg_count", v5, vM,   1, 1, opBRANCHES, 0 },
  /* EXT */
  { "save",            v5, vM,   0, 3, opSTORES, 0 },
  { "restore",         v5, vM,   0, 3, opSTORES, 0 },
  { "log_shift",       v5, vM,   2, 2, opSTORES, 0 },
  { "art_shift",       v5, vM,   2, 2, opSTORES, 0 },
  { "set_font",        v5, vM,   1, 1, opSTORES, 0 },
  { "draw_picture",    v6, vM,   3, 3, opNONE, 0 },
  { "picture_data",    v6, vM,   2, 2, opBRANCHES, 0 },
  { "erase_picture",   v6, vM,   3, 3, opNONE, 0 },
  { "set_margins",     v6, vM,   3, 3, opNONE, 0 },
  { "save_undo",       v5, vM,   0, 0, opSTORES, 0 },
  { "restore_undo",    v5, vM,   0, 0, opSTORES, 0 },
  { "print_unicode",   v5, vM,   1, 1, opNONE, 0 },
  { "check_unicode",   v5, vM,   1, 1, opSTORES, 0 },
  { "XXX",             v1, vM,   0, 8, opNONE, 0 },
  { "XXX",             v1, vM,   0, 8, opNONE, 0 },
  { "XXX",             v1, vM,   0, 8, opNONE, 0 },
  { "move_window",     v6, vM,   3, 3, opNONE, 0 },
  { "window_size",     v6, vM,   3, 3, opNONE, 0 },
  { "window_style",    v6, vM,   2, 3, opNONE, 0 },
  { "get_wind_prop",   v6, vM,   2, 2, opSTORES, 0 },
  { "scroll_window",   v6, vM,   2, 2, opNONE, 0 },
  { "pop_stack",       v6, vM,   1, 2, opNONE, 0 },
  { "read_mouse",      v6, vM,   1, 1, opNONE, 0 },
  { "mouse_window",    v6, vM,   1, 1, opNONE, 0 },
  { "push_stack",      v6, vM,   1, 2, opBRANCHES, 0 },
  { "put_wind_prop",   v6, vM,   3, 3, opNONE, 0 },
  { "print_form",      v6, vM,   1, 1, opNONE, 0 },
  { "make_menu",       v6, vM,   2, 2, opBRANCHES, 0 },
  { "picture_table",   v6, vM,   1, 1, opNONE, 0 },
  { "XXX",             v1, vM,   0, 8, opNONE, 0 },
  { "XXX",             v1, vM,   0, 8, opNONE, 0 },
  { "XXX",             v1, vM,   0, 8, opNONE, 0 }, 
};

#endif
