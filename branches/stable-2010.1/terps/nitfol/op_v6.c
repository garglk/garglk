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


typedef enum { z6_text, z6_picture, z6_rectangle } z6_type;

struct graph_piece {
  struct graph_piece *next;
  struct graph_piece *prev;

  int window;
  zword x, y;
  zword width, height;
  z6_type type;

  char *text;
  zword picnum;
  int color;   /* -1 means erase to background color */
};

static struct graph_piece *older_pieces;


static zwinid lower_win;

static zword window_props[8][16];
static int current_window;

typedef enum { w_y_coord,     w_x_coord,   w_y_size,     w_x_size,
	       w_y_cursor,    w_x_cursor,  w_l_margin,   w_r_margin,
	       w_nl_routine,  w_int_count, w_text_style, w_colour_data,
	       w_font_number, w_font_size, w_attributes, w_line_count
} win_prop_names;

typedef enum { wa_wrapping, wa_scrolling, wa_transcript, wa_buffered } win_attributes;


static int get_window_num(int arg)
{
  if(numoperands <= arg || operand[arg] == neg(3))
    return current_window;
  if(operand[arg] > 7) {
    n_show_error(E_OUTPUT, "illegal window number", operand[arg]);
    return current_window;
  }
  return operand[arg];
}

static BOOL check_window_prop(int prop_num)
{
  if(prop_num > 15) {
    n_show_error(E_OUTPUT, "illegal wind_prop number", prop_num);
    return FALSE;
  }
  return TRUE;
}


int is_in_bounds(glui32 x1, glui32 y1, glui32 width1, glui32 height1,
		 glui32 x2, glui32 y2, glui32 width2, glui32 height2)
{
  return (x1 >= x2 && y1 >= y2 &&
	  x1 + width1 <= x2 + width2 && y1 + height1 <= y2 + height2);
}


static void add_piece(struct graph_piece new_piece)
{
  struct graph_piece *p, *q;

  return;

  if(new_piece.x + new_piece.width >
     window_props[current_window][w_x_coord] +
     window_props[current_window][w_x_size]) {
    new_piece.width = window_props[current_window][w_x_coord] +
                      window_props[current_window][w_x_size] -
                      new_piece.x;
  }

  if(new_piece.y + new_piece.height >
     window_props[current_window][w_y_coord] +
     window_props[current_window][w_y_size]) {
    new_piece.height = window_props[current_window][w_y_coord] +
                       window_props[current_window][w_y_size] -
                       new_piece.y;
  }

  q = NULL;
  p = older_pieces;
  while(p) {
    if(is_in_bounds(p->x, p->y, p->width, p->height,
		    new_piece.x, new_piece.y,
		    new_piece.width, new_piece.height)) {
      if(q) {
	q->next = p->next;
	n_free(p);
	p = q->next;
      } else {
	LEremove(older_pieces);
	p = older_pieces;
      }
    } else {
      p = p-> next;
    }
    q = p;
  }


  LEadd(older_pieces, new_piece);



}


void v6_main_window_is(zwinid win)
{
  lower_win = win;
}


void op_set_window6(void)
{
  int win = get_window_num(0);
  current_window = win;
}


void op_set_margins(void)
{
  int win = get_window_num(2);

#ifdef DEBUG_IO
  n_show_debug(E_OUTPUT, "set_margins w=", operand[2]);
  n_show_debug(E_OUTPUT, "set_margins l=", operand[0]);
  n_show_debug(E_OUTPUT, "set_margins r=", operand[1]);
#endif

  window_props[win][w_l_margin] = operand[0];
  window_props[win][w_r_margin] = operand[1];

  /* FIXME: move cursor */
}


void op_move_window(void)
{
  int win = get_window_num(0);

#ifdef DEBUG_IO
  n_show_debug(E_OUTPUT, "move_window w=", operand[0]);
  n_show_debug(E_OUTPUT, "move_window y=", operand[1]);
  n_show_debug(E_OUTPUT, "move_window x=", operand[2]);
#endif

  window_props[win][w_y_coord] = operand[1];
  window_props[win][w_x_coord] = operand[2];
}


void op_window_size(void)
{
  int win = get_window_num(0);

#ifdef DEBUG_IO
  n_show_debug(E_OUTPUT, "window_size w=", operand[0]);
  n_show_debug(E_OUTPUT, "window_size y=", operand[1]);
  n_show_debug(E_OUTPUT, "window_size x=", operand[2]);
#endif

  window_props[win][w_y_size] = operand[1];
  window_props[win][w_x_size] = operand[2];
}


void op_window_style(void)
{
  zword attr;
  int win = get_window_num(0);

  if(numoperands < 3)
    operand[2] = 0;

  attr = window_props[win][w_attributes];
  switch(operand[2]) {
  case 0: attr  = operand[1]; break;
  case 1: attr |= operand[1]; break;
  case 2: attr &= ~(operand[1]); break;
  case 3: attr ^= operand[1]; break;
  default: n_show_error(E_OUTPUT, "invalid flag operation", operand[2]);
  }

  window_props[win][w_attributes] = attr;
}


void op_get_wind_prop(void)
{
  int win = get_window_num(0);

  if(!check_window_prop(operand[1])) {
    mop_store_result(0);
    return;
  }
  mop_store_result(window_props[win][operand[1]]);
}


void op_put_wind_prop(void)
{
  int win = get_window_num(0);

  if(!check_window_prop(operand[1])) {
    mop_store_result(0);
    return;
  }
  window_props[win][operand[1]] = operand[2];
}


void op_scroll_window(void)
{
  ;
}


void op_read_mouse(void)
{
  ;
}


void op_mouse_window(void)
{
  ;
}


void op_print_form(void)
{
  ;
}


void op_make_menu(void)
{
  mop_skip_branch();
}


void op_picture_table(void)
{
  ; /* Glk contains no image prefetching facilities, so nothing to do here */
}


void op_draw_picture(void)
{
  struct graph_piece new_piece;
  glui32 width, height;

  z_put_char(lower_win, 13); /* Work around a bug in xglk */
  draw_intext_picture(lower_win, operand[0], imagealign_MarginLeft);


  /*

  new_piece.window = current_window;
  new_piece.x = operand[2] + window_props[current_window][w_x_coord];
  new_piece.y = operand[1] + window_props[current_window][w_y_coord];

  wrap_glk_image_get_info(operand[0], &width, &height);

  new_piece.width = width;
  new_piece.height = height;

  new_piece.type = z6_picture;
  new_piece.picnum = operand[0];

  add_piece(new_piece);

  */
}


void op_picture_data(void)
{
  if(glk_gestalt(gestalt_Graphics, 0)) {
    glui32 width, height;
    
    if(operand[0] == 0) {
      LOWORDwrite(operand[1], imagecount);
      LOWORDwrite(operand[1], 42); /* FIXME: where do I get picture release? */
    }
    else if(wrap_glk_image_get_info(operand[0], &width, &height)) {
      LOWORDwrite(operand[1], height);
      LOWORDwrite(operand[1] + ZWORD_SIZE, width);
      mop_take_branch();
      return;
    }
  }
  mop_skip_branch();
}


void op_erase_picture(void)
{
  struct graph_piece new_piece;
  glui32 width, height;

  new_piece.window = current_window;
  new_piece.x = operand[2] + window_props[current_window][w_x_coord];
  new_piece.y = operand[1] + window_props[current_window][w_y_coord];

  wrap_glk_image_get_info(operand[0], &width, &height);

  new_piece.width = width;
  new_piece.height = height;

  new_piece.type = z6_rectangle;
  new_piece.color = -1;

  add_piece(new_piece);

}

