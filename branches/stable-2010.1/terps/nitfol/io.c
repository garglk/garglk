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
#include "nio.h"

#ifdef HEADER

typedef struct z_window *zwinid;

#endif

BOOL is_fixed;       /* If we are forcing output to be fixed-width */


/* descriptions of z-machine colors, in glk 0x00rrggbb style.
   -1 means to call glk_stylehint_clear instead of glk_stylehint_set.
   The 'current color' will be overwritten on calls to set_colour.

   Go ahead and customize these (making background colors lighter than
   foreground colors might be interesting)
*/

glsi32 bgcolortable[] = {
  -1L,          /* current color */
  -1L,          /* defualt setting */
  0x00000000L,  /* black */
  0x00ff0000L,  /* red */
  0x00008000L,  /* green */
  0x00ffff00L,  /* yellow */
  0x000000ffL,  /* blue */
  0x00ff00ffL,  /* magenta */
  0x0000ffffL,  /* cyan */
  0x00ffffffL,  /* white */
  0x00c0c0c0L,  /* light grey */
  0x00808080L,  /* medium grey */
  0x00404040L   /* dark grey */
};

glsi32 fgcolortable[] = {
  -1L,          /* current color */
  -1L,          /* defualt setting */
  0x00000000L,  /* black */
  0x00ff0000L,  /* red */
  0x00008000L,  /* green */
  0x00ffff00L,  /* yellow */
  0x000000ffL,  /* blue */
  0x00ff00ffL,  /* magenta */
  0x0000ffffL,  /* cyan */
  0x00ffffffL,  /* white */
  0x00c0c0c0L,  /* light grey */
  0x00808080L,  /* medium grey */
  0x00404040L   /* dark grey */
};


static void killglkwithcolor(glui32 styl, int fore, int back)
{
  if(fgcolortable[fore] == -1)
    glk_stylehint_clear(wintype_AllTypes, styl,
			stylehint_TextColor);
  else
    glk_stylehint_set(wintype_AllTypes, styl,
		      stylehint_TextColor, fgcolortable[fore]);

  if(bgcolortable[back] == -1)
    glk_stylehint_clear(wintype_AllTypes, styl,
			stylehint_BackColor);
  else
    glk_stylehint_set(wintype_AllTypes, styl,
		      stylehint_BackColor, bgcolortable[back]);
}


static void set_stylehints(char fore, char back)
{
  glui32 n;
  for(n = 0; n < style_NUMSTYLES; n++)
    killglkwithcolor(n, fore, back);

  /* Subheader will be used for bold */
  glk_stylehint_set(wintype_TextBuffer, style_Subheader,
		    stylehint_Weight, 1);

  /* BlockQuote will be used for reverse proportional text */
  glk_stylehint_set(wintype_TextBuffer, style_BlockQuote,
		    stylehint_Proportional, 0);
  glk_stylehint_set(wintype_TextBuffer, style_BlockQuote,
		    stylehint_Justification, stylehint_just_Centered);
#ifdef stylehint_ReverseColor
  glk_stylehint_set(wintype_TextBuffer, style_BlockQuote,
		    stylehint_ReverseColor, 1);
#endif

  /* User1 will be used for bold italics */
  glk_stylehint_set(wintype_TextBuffer, style_User1,
		    stylehint_Weight, 1);
  glk_stylehint_set(wintype_TextBuffer, style_User1,
		    stylehint_Oblique, 1);

  /* User2 will be used for proportional bold/italic */
  glk_stylehint_set(wintype_TextBuffer, style_User2,
		    stylehint_Proportional, 0);
  glk_stylehint_set(wintype_TextBuffer, style_User2,
		    stylehint_Weight, 1);
}

#define sBOLD 1
#define sITAL 2
#define sFIXE 4
#define sREVE 8

#if 0

static glui32 bitmap_to_style[16] = {
  style_Normal,
  style_Subheader,   /* sBOLD */
  style_Emphasized,  /* sITAL */
  style_User1,       /* sBOLD | sITAL */
  style_Preformatted,/* sFIXE */
  style_User2,       /* sFIXE | sBOLD */
  style_User2,       /* sFIXE | sITAL */
  style_User2,       /* sFIXE | sBOLD | sITAL*/
  style_BlockQuote,  /* sREVE */
  style_BlockQuote,  /* sREVE | sBOLD */
  style_BlockQuote,  /* sREVE | sITAL */
  style_BlockQuote,  /* sREVE | sBOLD | sITAL */
  style_BlockQuote,  /* sFIXE | sREVE */
  style_BlockQuote,  /* sFIXE | sREVE | sBOLD */
  style_BlockQuote,  /* sFIXE | sREVE | sITAL */
  style_BlockQuote   /* sFIXE | sREVE | sBOLD | sITAL */
};

#else

static glui32 bitmap_to_style[16] = {
  style_Normal,
  style_Subheader,   /* sBOLD */
  style_Emphasized,  /* sITAL */
  style_Subheader,   /* sBOLD | sITAL */
  style_Preformatted,/* sFIXE */
  style_Subheader,   /* sFIXE | sBOLD */
  style_Emphasized,  /* sFIXE | sITAL */
  style_Subheader,   /* sFIXE | sBOLD | sITAL*/
  style_Normal,
  style_Subheader,   /* sBOLD */
  style_Emphasized,  /* sITAL */
  style_Subheader,   /* sBOLD | sITAL */
  style_Preformatted,/* sFIXE */
  style_Subheader,   /* sFIXE | sBOLD */
  style_Emphasized,  /* sFIXE | sITAL */
  style_Subheader    /* sFIXE | sBOLD | sITAL*/
};

#endif

typedef struct {
  char fore;
  char back;
  unsigned char style;
} colorstyle;

typedef struct {
  glui32 image_num;
  glui32 x, y;
  glui32 width, height;
} z_image;


struct z_window {
  winid_t win;
  strid_t str;
  glui32 wintype;
  glui32 method;

  strid_t transcript;

  BOOL glk_input_pending;
  glui32 pending_input_type;
  glui32 pending_input_length;

  /* for upper window of v3 - returns # of lines drawn */
  glui32 (*draw_callback)(winid_t win, glui32 width, glui32 height);
  BOOL (*mouse_callback)(BOOL is_char_event, winid_t win, glui32 x, glui32 y);
  
  glui32 width, height;
  glui32 x1, y1, x2, y2;

  glui32 last_height;   /* What the height was last time we got input */
  glui32 biggest_height;/* The biggest it's been since */

  glui32 curr_offset;   /* offset into text_buffer/color_buffer */
  glui32 max_offset;    /* curr_offset must stay < max_offset */
  glui32 buffer_size;   /* max_offset must stay < buffer_size */

  BOOL dirty;           /* Has window been changed since last redraw? */
  BOOL defined;         /* Is our location well defined? */

  unsigned char *text_buffer;  /* whole window for grid, current line for buffer */
  colorstyle *color_buffer;

  z_image images[12];

  colorstyle current;
  colorstyle actual;
};


#define num_z_windows 16

static struct z_window game_windows[num_z_windows];

static glui32 upper_width, upper_height;


static int waitforinput(zwinid window, glui32 *val,
			BOOL (*timer_callback)(zword), zword timer_arg);


void set_glk_stream_current(void)
{
  z_flush_text(&game_windows[0]);
  glk_stream_set_current(game_windows[0].str);
}

void draw_intext_picture(zwinid window, glui32 picture, glui32 alignment)
{
  z_flush_text(window);
  wrap_glk_image_draw(window->win, picture, alignment, 0);
}

void draw_picture(zwinid window, glui32 picture, glui32 x, glui32 y)
{
  int i;
  int useimage = -1;
  glui32 width, height;

  wrap_glk_image_get_info(operand[0], &width, &height);

  for(i = 0; i < 12; i++) {
    if(is_in_bounds(window->images[i].x, window->images[i].y,
		    window->images[i].width, window->images[i].height,
		    x, y, width, height))
      useimage = i;
  }
  if(useimage == -1)
    for(i = 0; i < 12; i++)
      if(window->images[i].image_num == 0)
	useimage = i;
  if(useimage == -1)
    return;

  window->images[useimage].image_num = picture;
  window->images[useimage].x = x;
  window->images[useimage].y = y;
  window->images[useimage].width = width;
  window->images[useimage].height = height;
}


static int showstuffcount = 0;

/* Show an interpreter message */
void showstuff(const char *title, const char *type, const char *message, offset number)
{
  static BOOL loopy = FALSE;
  if(loopy) {
    loopy = FALSE;
    n_show_fatal(E_SYSTEM, "loopy message reporting", 0);
  }
  loopy = TRUE;

  z_pause_timed_input(&game_windows[0]);
  z_flush_text(&game_windows[0]);
  glk_stream_set_current(game_windows[0].str);

  glk_set_style(style_Alert);
  w_glk_put_string("\n[");
  w_glk_put_string(title);
  w_glk_put_string(": ");
  w_glk_put_string(type);
  w_glk_put_string("]: ");
  w_glk_put_string(message);
  w_glk_put_string(" (");
  g_print_snumber(number);
  w_glk_put_string(") ");

#ifdef DEBUGGING
  infix_gprint_loc(stack_get_depth(), 0);
#else
  w_glk_put_string("PC=");
  g_print_number(oldPC);
  glk_put_char('\n');
#endif

  if(++showstuffcount == 100) {
    w_glk_put_string("[pausing every 100 errors]\n");
    z_wait_for_key(&game_windows[0]);
  }

  glk_set_style(style_Normal);

  loopy = FALSE;
}


void init_lower(zwinid *lower)
{
  zwinid lower_win = &game_windows[0];
  glui32 i;

  if(lower)
    *lower = lower_win;

  if(lower_win->win) {
    z_pause_timed_input(lower_win);
    glk_window_close(lower_win->win, NULL);
  }

  set_stylehints(lower_win->current.fore,
		 lower_win->current.back);


  lower_win->dirty = TRUE;
  lower_win->wintype = wintype_TextBuffer;
  lower_win->method = winmethod_Below;
  lower_win->curr_offset = 0;
  lower_win->max_offset = 80 * 24;
  lower_win->buffer_size = lower_win->max_offset;

  if(!lower_win->text_buffer)
    lower_win->text_buffer = (unsigned char *) n_malloc(lower_win->buffer_size);
  if(!lower_win->color_buffer)
    lower_win->color_buffer = (colorstyle *) n_malloc(lower_win->buffer_size * sizeof(colorstyle));

  for(i = 0; i < lower_win->buffer_size; i++) {
    lower_win->text_buffer[i] = ' ';
    lower_win->color_buffer[i] = lower_win->current;
  }

  lower_win->actual = lower_win->current;

  lower_win->win = glk_window_open(game_windows[1].win,
				    winmethod_Below | winmethod_Proportional,
				    50,  /* Percent doesn't matter */
				    wintype_TextBuffer, 0);


  if(lower_win->win == 0) {
    n_show_fatal(E_OUTPUT, "cannot open lower window", 0);
    return;
  }

  lower_win->str = glk_window_get_stream(lower_win->win);

  if(lower_win->transcript)
    glk_window_set_echo_stream(lower_win->win, lower_win->transcript);
}


void init_upper(zwinid *upper)
{
  zwinid upper_win = &game_windows[1];
  glui32 i;

  if(upper)
    *upper = upper_win;

  if(upper_win->win) {
    z_pause_timed_input(upper_win);
    glk_window_close(upper_win->win, NULL);
  }

  upper_win->dirty = TRUE;
  upper_win->wintype = wintype_TextGrid;
  upper_win->method = winmethod_Above | winmethod_Fixed;
  upper_win->height = 0;
  upper_win->width = upper_width;
  upper_win->curr_offset = 0;
  upper_win->max_offset = upper_height * upper_width;
  upper_win->buffer_size = upper_win->max_offset;

  if(!upper_win->text_buffer)
    upper_win->text_buffer = (unsigned char *) n_malloc(upper_win->buffer_size);
  if(!upper_win->color_buffer)
    upper_win->color_buffer = (colorstyle *) n_malloc(upper_win->buffer_size * sizeof(colorstyle));

  for(i = 0; i < upper_win->buffer_size; i++) {
    upper_win->text_buffer[i] = ' ';
    upper_win->color_buffer[i] = upper_win->current;
  }

  upper_win->actual = upper_win->current;

  upper_win->win = glk_window_open(game_windows[0].win,
				    winmethod_Above | winmethod_Fixed,
				    1, /* XXX huh? upper_height, */
				    wintype_TextGrid, 1);

  if(upper_win->win == 0) {
    upper_win->str = 0;
    return;
  }

  upper_win->str = glk_window_get_stream(upper_win->win);

  if(upper_win->str == 0) {
    glk_window_close(upper_win->win, NULL);
    upper_win->win = 0;
    return;
  }
}


void z_init_windows(BOOL dofixed,
		    glui32 (*draw_callback)(winid_t, glui32, glui32),
		    BOOL (*mouse_callback)(BOOL, winid_t, glui32, glui32),
		    glui32 maxwidth, glui32 maxheight,
		    zwinid *upper, zwinid *lower)
{
  colorstyle defaultstyle;
  defaultstyle.fore = 1; defaultstyle.back = 1; defaultstyle.style = 0;

  is_fixed = dofixed;

  kill_windows();

  upper_width = maxwidth; upper_height = maxheight;

  game_windows[0].current = game_windows[1].current = defaultstyle;
  game_windows[1].draw_callback  = draw_callback;
  game_windows[1].mouse_callback = mouse_callback;

  init_lower(lower);
  init_upper(upper);
}


zwinid z_split_screen(glui32 wintype, glui32 method,
		      glui32 (*draw_callback)(winid_t, glui32, glui32),
		      BOOL (*mouse_callback)(BOOL, winid_t, glui32, glui32))
{
  int i;
  for(i = 0; i < num_z_windows; i++) {
    if(!game_windows[i].win) {
      winid_t root = glk_window_get_root();
      game_windows[i].win = glk_window_open(root, method, 0, wintype, 0);
      game_windows[i].str = glk_window_get_stream(game_windows[i].win);
      game_windows[i].wintype = wintype;
      game_windows[i].method = method;
      game_windows[i].transcript = NULL;
      game_windows[i].glk_input_pending = FALSE;
      game_windows[i].draw_callback = draw_callback;
      game_windows[i].mouse_callback = mouse_callback;
      game_windows[i].width = 0;
      game_windows[i].height = 0;
      game_windows[i].curr_offset = 0;
      game_windows[i].max_offset = 1;
      game_windows[i].buffer_size = 2;
      game_windows[i].text_buffer = n_malloc(2);
      game_windows[i].color_buffer = n_malloc(2);
      game_windows[i].dirty = TRUE;
      return &game_windows[i];
    }
  }
  return NULL;
}


void z_kill_window(zwinid win)
{
  if(!win)
    return;
  n_free(win->text_buffer);
  win->text_buffer = NULL;
  n_free(win->color_buffer);
  win->color_buffer = NULL;
  win->transcript = NULL;
  glk_window_close(win->win, NULL);
  win->win = NULL;
  win->str = NULL;
}


/* close any open windows */
void kill_windows(void)
{
  int i;

  for(i = 0; i < num_z_windows; i++)
    z_clear_window(&game_windows[i]);

  free_windows();
  for(i = 0; i < num_z_windows; i++) {
    if(game_windows[i].win) {
      game_windows[i].transcript = NULL;

      glk_window_close(game_windows[i].win, NULL);
      game_windows[i].win = NULL;
      game_windows[i].str = NULL;
    }
  }
  showstuffcount = 0;
}


/* free memory space used by windows, but don't close them */
void free_windows(void)
{
  int i;

  z_flush_all_windows();

  for(i = 0; i < num_z_windows; i++) {
    if(game_windows[i].win) {
      n_free(game_windows[i].text_buffer);
      game_windows[i].text_buffer = NULL;

      n_free(game_windows[i].color_buffer);
      game_windows[i].color_buffer = NULL;
    }
  }
}

zwinid z_find_win(winid_t win)
{
  int i;
  for(i = 0; i < num_z_windows; i++) {
    if(game_windows[i].win == win)
      return &game_windows[i];
  }
  return NULL;
}


static BOOL coloreq(colorstyle a, colorstyle b) /* return true if colors are equivalent */
{
  return (fgcolortable[(int) a.fore] == fgcolortable[(int) b.fore]) &&
         (bgcolortable[(int) a.back] == bgcolortable[(int) b.back]);
}


static void checkforblockquote(zwinid window, zwinid dest_win)
{
  if(window->biggest_height > window->last_height &&
     window->biggest_height > window->height) {
    /* find borders of the blockquote */
    unsigned leftx = window->width, rightx = 0;
    unsigned topy = window->biggest_height;
    unsigned bottomy = window->height;
    unsigned x, y, i;

    i = window->height * window->width;
    for(y = window->height; y < window->biggest_height; y++)
      for(x = 0; x < window->width; x++)
	if(window->text_buffer[i++] != ' ') {
	  if(x < leftx)
	    leftx = x;
	  if(x > rightx)
	    rightx = x;
	  if(y < topy)
	    topy = y;
	  if(y > bottomy)
	    bottomy = y;
	}
    
    z_pause_timed_input(dest_win);
    glk_stream_set_current(game_windows[1].str);
    glk_put_char(10);
    glk_set_style(style_BlockQuote);
    
    /* draw the blockquote */
    for(y = topy; y <= bottomy; y++) {
      i = y * window->width + leftx;
      for(x = leftx; x <= rightx; x++)
	glk_put_char(window->text_buffer[i++]);
      glk_put_char(10);
    }
  }
}


void z_pause_timed_input(zwinid window)
{
  event_t eep;
  if(window->glk_input_pending) {
    window->glk_input_pending = FALSE;

    switch(window->pending_input_type) {
    case evtype_CharInput:
      glk_cancel_char_event(window->win);
      break;
    case evtype_LineInput:
      glk_cancel_line_event(window->win, &eep);
      window->pending_input_length = eep.val1;
    }
  }
}


void z_flush_all_windows(void)
{
  int window;
  for(window = 0; window < num_z_windows; window++) {
    if(game_windows[window].dirty) {
      z_pause_timed_input(&game_windows[window]);
      
      switch(game_windows[window].wintype) {
      case wintype_TextBuffer:
	z_flush_text(&game_windows[window]);
	break;
      case wintype_TextGrid:
	z_flush_fixed(&game_windows[window]);
	break;
      case wintype_Graphics:
	z_flush_graphics(&game_windows[window]);
	break;
      }
    }
  }
}

void z_draw_all_windows(void)
{
  int window;
  for(window = 0; window < num_z_windows; window++) {
    if(game_windows[window].wintype == wintype_TextGrid) {
      game_windows[window].dirty = TRUE;
      z_flush_fixed(&game_windows[window]);
    }
  }
}


static void z_put_styled_string(zwinid window, unsigned char *text,
				colorstyle *color, glui32 length)
{
  glui32 n;
  colorstyle laststyle = color[0];

  if(!length)
    return;

  glk_set_style_stream(window->str, bitmap_to_style[laststyle.style]);

  for(n = 0; n < length; n++) {
    if(color[n].style != laststyle.style)
      glk_set_style_stream(window->str, bitmap_to_style[color[n].style]);
    glk_put_char_stream(window->str, text[n]);
    laststyle = color[n];
  }
}

void z_flush_fixed(zwinid window)
{
  glui32 winx, winy;
  winid_t o;
  glui32 start_line, end_line;

  /* If there's no such window, give up */
  if(!window->win || !window->str ||
     !window->text_buffer || !window->color_buffer)
    return;

  /* glk doesn't allow writing to a window while input is pending */
  z_pause_timed_input(window);

  end_line = window->height;

  /* Has the window grown and shrunk?  If so, probably because someone wants
     to draw a box quote - don't let them shrink the window quite so fast */
  if(window->biggest_height > window->last_height &&
     window->biggest_height > window->height)
    end_line = window->biggest_height;

  /* For v3 games, there's a callback function to draw the room name and
     score; if this is present, we start drawing at a lower position */
  start_line = 0;
  if(window->draw_callback)
    start_line = window->draw_callback(NULL, 0, 0);
  end_line += start_line;

  o = glk_window_get_parent(window->win);
#if 0
  glk_window_get_size(window->win, &winx, &winy);
  if (!(window->method & winmethod_Above || window->method & winmethod_Below)
      || winy != end_line)
    glk_window_set_arrangement(o, window->method,
			       end_line, window->win);
#else
  glk_window_set_arrangement(o, window->method, end_line, window->win);
#endif
  glk_window_get_size(window->win, &winx, &winy);

  if(window->draw_callback) {
    glk_stream_set_current(window->str);
    glk_window_clear(window->win);
    glk_window_move_cursor(window->win, 0, 0);
    window->draw_callback(window->win, winx, winy);
  }

  if(end_line > start_line && window->dirty) {

    unsigned padleft = 0, padmiddle = 0, padright = 0;
    unsigned skipleft = 0, skipmiddle = 0, skipright = 0;
    unsigned width;
    unsigned firstwidth, lastwidth;

    unsigned x, y, i;

    /* Calculate how much space is used for margins */

    unsigned left_margin = window->width, right_margin = window->width;

    i = 0;
    for(y = start_line; y < end_line; y++) {
      
      for(x = 0; x < window->width; x++)
	if(window->text_buffer[i + x] != ' ') {
	  if(x < left_margin)
	    left_margin = x;
	  break;
	}
      
      for(x = 0; x < window->width; x++)
	if(window->text_buffer[i + window->width - x - 1] != ' ') {
	  if(x < right_margin)
	    right_margin = x;
	  break;
	}
      
      i += window->width;
    }
    
    firstwidth = window->width; lastwidth = 0;
    
    if(start_line + 1 == end_line) {
      unsigned longestx = 0;
      unsigned longestlen = 0;
      unsigned thisx = 0;
      unsigned thislen = 0;
      colorstyle lastcolor;
      width = window->width;
      
      for(x = skipleft; x < width + skipleft; x++) {
	if(window->text_buffer[x] == ' '
	   && (!thislen || coloreq(window->color_buffer[x], lastcolor))) {
	  if(!thislen)
	    thisx = x;
	  thislen++;
	  lastcolor = window->color_buffer[x];
	} else {
	  if(thislen > longestlen) {
	    longestx = thisx;
	    longestlen = thislen;
	  }
	  thislen = 0;
	}
      }
      if(longestlen > 4) {
	firstwidth = longestx - skipleft;
	skipmiddle = longestlen - 1;
	lastwidth = width - firstwidth - skipmiddle;
      }
    }
    
    if(skipmiddle && winx < firstwidth + 2 + lastwidth)
      padmiddle = 2;
    
    if(lastwidth && winx >= firstwidth + padmiddle + lastwidth) {
      padmiddle = winx - firstwidth - lastwidth;
    } else {
      if(winx >= window->width)
	width = window->width;
      else
	width = winx;
      
      if(right_margin + left_margin) {
	if(winx > window->width)
	  padleft = (unsigned) ((winx - window->width) * (((float) left_margin) / (right_margin + left_margin)));
	else
	  skipleft = (unsigned) ((window->width - winx) * (((float) left_margin) / (right_margin + left_margin)));
      }
      else {
	padleft = winx - window->width;
      }
      
      if(skipleft > left_margin)
	skipleft = left_margin;
      
      if(winx > window->width)
	padright = winx - window->width - padleft;
      else
	skipright = window->width - winx - skipleft;
      
      if(width < firstwidth + padmiddle) {
	firstwidth = width;
	padmiddle = 0;
	lastwidth = 0;
      } else if(width < firstwidth + padmiddle + lastwidth) {
	lastwidth = width - firstwidth - padmiddle;
      }
    }

    
    glk_stream_set_current(window->str);
    glk_window_move_cursor(window->win, 0, start_line);

    /* draw to the upper window */
    i = 0;
    for(y = start_line; y < end_line; y++) {

      for(x = 0; x < padleft; x++)
	glk_put_char(' ');

      i += skipleft;

      z_put_styled_string(window, window->text_buffer + i,
			  window->color_buffer + i, firstwidth);
      i += firstwidth;

      for(x = 0; x < padmiddle; x++)
	glk_put_char(' ');
      i += skipmiddle;
      
      z_put_styled_string(window, window->text_buffer + i,
			  window->color_buffer + i, lastwidth);
      i += lastwidth;

      for(x = 0; x < padright; x++)
	glk_put_char(' ');

      i += skipright;
    }

    /* Bureaucracy needs the cursor positioned and visible in upper window. */
    glk_window_move_cursor(window->win,
			   window->curr_offset % window->width,
			   window->curr_offset / window->width);
    window->dirty = FALSE;
  }
}


void z_flush_text(zwinid window)
{
  z_pause_timed_input(window);

  if(!window->win || !window->str
     || !window->text_buffer || !window->color_buffer
     || window->curr_offset == 0) {
    window->curr_offset = 0;
    return;
  }
  
  z_put_styled_string(window, window->text_buffer, window->color_buffer,
		      window->curr_offset);

  window->curr_offset = 0;
  window->dirty = FALSE;
}


void z_flush_graphics(zwinid window)
{
  int i;
  glui32 winx, winy;
  float xratio, yratio;
  winid_t parent;

  if(!window->win)
    return;

  glk_window_get_size(window->win, &winx, &winy);
  xratio = ((float) winx) / window->width;
  yratio = ((float) winy) / window->height;

  parent = glk_window_get_parent(window->win);

  /* We want the window to maintain its original height/width ratio */
  switch(window->method & winmethod_DirMask) {
  case winmethod_Left:   /* Left and right splits mean height is fixed - */
  case winmethod_Right:  /* adjust width to the yratio */
    glk_window_set_arrangement(parent, window->method,
			       (glui32) (window->width * yratio), 0);
    break;
  case winmethod_Above:  /* Above and below splits mean width is fixed - */
  case winmethod_Below:  /* adjust height to the xratio */
    glk_window_set_arrangement(parent, window->method,
			       (glui32) (window->height * xratio), 0);
    break;
  }

  /* Check to see what it became, and if it's still off, don't worry */
  glk_window_get_size(window->win, &winx, &winy);
  xratio = ((float) winx) / window->width;
  yratio = ((float) winy) / window->height;

  for(i = 0; i < 12; i++) {
    if(window->images[i].image_num) {
      wrap_glk_image_draw_scaled(window->win, window->images[i].image_num,
				 (glui32) (window->images[i].x * xratio),
				 (glui32) (window->images[i].y * yratio),
				 (glui32) (window->images[i].width * xratio),
				 (glui32) (window->images[i].height * yratio));
    }
  }
}

void z_print_number(zwinid window, int number)
{
  int i;
  char buffer[12];
  int length = n_to_decimal(buffer, number);

  for(i = length - 1; i >= 0; i--)
    z_put_char(window, buffer[i]);
}

void z_put_char(zwinid window, unsigned c)
{
  colorstyle color = window->current;
  if(is_fixed)
    color.style |= sFIXE;

  if(c == 0)     /* Section 3.8.2.1 */
    return;

  window->dirty = TRUE;

  if((c < 32 && c != 13) || (c >= 127 && c <= 159)) {  /*Undefined in latin-1*/
    switch(window->wintype) {
    case wintype_TextBuffer:
      z_put_char(window, '[');
      z_print_number(window, c);
      z_put_char(window, ']');
      return;
    case wintype_TextGrid:
      c = '?';
    }
  }

  switch(c) {
  case 0x152:
    z_put_char(window, 'O');
    c = 'E';
    break;
  case 0x153:
    z_put_char(window, 'o');
    c = 'e';
    break;
  }


  if(c > 255)    /* Section 3.8.5.4.3 */
    c = '?';

  if(c == 13) {  /* Section 7.1.2.2.1 */
    switch(window->wintype) {
    case wintype_TextBuffer:
      window->text_buffer[window->curr_offset] = 10;
      window->curr_offset++;
      z_flush_text(window);
      break;
    case wintype_TextGrid:
      window->curr_offset += window->width;
      window->curr_offset -= window->curr_offset % window->width;
    }
  } else {
    window->text_buffer[window->curr_offset] = c;
    window->color_buffer[window->curr_offset] = color;
    window->curr_offset++;
  }

  if(window->curr_offset >= window->max_offset) {
    switch(window->wintype) {
    case wintype_TextBuffer:
      z_flush_text(window);
      break;
    case wintype_TextGrid:
      if(!window->defined)                  /* Section 8.6.2 */
	n_show_port(E_OUTPUT, "writing past end of window", c);
      
      if(window->max_offset)
	window->curr_offset = window->max_offset - 1;
      else
	window->curr_offset = 0;

      window->defined = FALSE;
    }
  }
}

void z_setxy(zwinid window, zword x, zword y)
{
  window->curr_offset = (y - 1) * window->width + (x - 1);
  window->defined = TRUE;
}

void z_getxy(zwinid window, zword *x, zword *y)
{
  if(window->width) {
    *x = (window->curr_offset % window->width) + 1;
    *y = (window->curr_offset / window->width) + 1;
  } else {
    *x = window->curr_offset + 1;
    *y = 1;
  }
}

void z_getsize(zwinid window, unsigned *width, unsigned *height)
{
  *width = window->width;
  *height = window->height;
}

void z_find_size(glui32 *wid, glui32 *hei)
{
  glui32 oldwid, oldhei;
  zwinid upper = &game_windows[1];
  winid_t o = glk_window_get_parent(upper->win);
  glk_window_get_size(upper->win, &oldwid, &oldhei);
  glk_window_set_arrangement(o, (upper->method & ~winmethod_Fixed) |
			     winmethod_Proportional, 100, upper->win);
  glk_window_get_size(upper->win, wid, hei);
  glk_window_set_arrangement(o, upper->method, oldhei, upper->win);

  upper_width = *wid; upper_height = *hei;
  init_upper(&upper);
}

void z_set_height(zwinid window, unsigned height)
{
  unsigned x;
  if(height * window->width > window->buffer_size) {
    n_show_error(E_OUTPUT, "height too large", height);
    return;
  }

  window->height = height;
  if(height > window->biggest_height)
    window->biggest_height = height;

  x = window->max_offset;
  window->max_offset = height * window->width;

  for(; x < window->max_offset; x++) {
    window->text_buffer[x] = ' ';
    window->color_buffer[x] = window->current;
  }

  window->dirty = TRUE;
}

void z_set_color(zwinid window, unsigned fore, unsigned back)
{
  if(fore >= sizeof(fgcolortable) / sizeof(*fgcolortable)) {
    n_show_error(E_OUTPUT, "illegal foreground color", fore);
    return;
  }
  if(back >= sizeof(bgcolortable) / sizeof(*bgcolortable)) {
    n_show_error(E_OUTPUT, "illegal background color", back);
    return;
  }

  fgcolortable[0] = fgcolortable[fore];
  bgcolortable[0] = bgcolortable[back];
  
  window->current.fore = fore;
  window->current.back = back;
}

void z_set_style(zwinid window, int style)
{
  switch(style) {
  case 0: window->current.style = 0; break;
  case 1: window->current.style |= sREVE; break;
  case 2: window->current.style |= sBOLD; break;
  case 4: window->current.style |= sITAL; break;
  case 8: window->current.style |= sFIXE; break;
  default: n_show_error(E_OUTPUT, "undefined style", style);
  }
}

void set_fixed(BOOL p)
{
  if(!p) {
    is_fixed = FALSE;
  } else {
    is_fixed = TRUE;
  }
}

void z_set_transcript(zwinid window, strid_t stream)
{
  window->transcript = stream;
  glk_window_set_echo_stream(window->win, stream);
}

void z_clear_window(zwinid window)
{
  glui32 i;

  if(window == &game_windows[0] && showstuffcount) {
    z_pause_timed_input(&game_windows[0]);
    z_flush_text(&game_windows[0]);
    glk_stream_set_current(game_windows[0].str);
    w_glk_put_string("[pausing to show unread error message]\n");
    z_wait_for_key(&game_windows[0]);
  }

  window->dirty = TRUE;
  window->curr_offset = 0;

  if(window->win && window->text_buffer && window->color_buffer) {
    switch(window->wintype) {
    case wintype_TextGrid:
      for(i = 0; i < window->max_offset; i++) {
	window->text_buffer[i] = ' ';
	window->color_buffer[i] = window->current;
      }
      window->curr_offset = 0;
      window->dirty = TRUE;
      break;
    case wintype_TextBuffer:
      z_pause_timed_input(window);
      z_flush_text(window);
      if(coloreq(window->actual, window->current)) {
	glk_window_clear(window->win);
      } else {
	init_lower(NULL); /* **FIXME** This is wrong, but deal with it later */
      }
    }
  }
}

void z_erase_line(zwinid window)
{
  if(window->wintype == wintype_TextGrid) {
    int i;
    int x = window->curr_offset % window->width;
    int endoffset = window->curr_offset + (window->width - x);
    
    window->dirty = TRUE;
    for(i = window->curr_offset; i < endoffset; i++) {
      window->text_buffer[i] = ' ';
      window->color_buffer[i] = window->current;
    }
  }
}


/* Waits for input or timeout
 * Returns:
 *   0 - output during wait; may need to redraw or somesuch
 *  -1 - callback routine said to stop
 *  10 - read input
 * 254 - mouse input
 * char and line events will be canceled by the time it exits
 */
static int waitforinput(zwinid window, glui32 *val,
			BOOL (*timer_callback)(zword), zword timer_arg)
{
  int i;
  event_t moo;
  zwinid t;
  
  showstuffcount = 0;

  for(i = 0; i < num_z_windows; i++)
    if(game_windows[i].mouse_callback && game_windows[i].win)
      glk_request_mouse_event(game_windows[i].win);
  
  window->glk_input_pending = TRUE;

  while(window->glk_input_pending) {
    glk_select(&moo);

    check_sound(moo);

    switch(moo.type) {
    case evtype_Timer:
      if(timer_callback && timer_callback(timer_arg)) {
	if(window->pending_input_type == evtype_CharInput) {
	  glk_cancel_char_event(window->win);
	  *val = 0;
	} else {
	  glk_cancel_line_event(window->win, &moo);
	  *val = moo.val1;
	}
	window->glk_input_pending = FALSE;
	return -1;
      }
      break;

    case evtype_CharInput:
      *val = moo.val1;
      window->glk_input_pending = FALSE;
      return 10;

    case evtype_LineInput:
      *val = moo.val1;
      window->glk_input_pending = FALSE;
      return 10;

    case evtype_MouseInput:
      t = z_find_win(moo.win);
      if(t && t->mouse_callback &&
	 t->mouse_callback(window->pending_input_type == evtype_CharInput,
			   moo.win, moo.val1, moo.val2)) {
	if(window->pending_input_type == evtype_CharInput) {
	  glk_cancel_char_event(window->win);
	  *val = 254;
	} else {
	  glk_cancel_line_event(window->win, &moo);
	  *val = moo.val1;
	}
	window->glk_input_pending = FALSE;
	return 254;
      }
      glk_request_mouse_event(moo.win);
      break;
    
    case evtype_Arrange:
      z_draw_all_windows();
    }

    z_flush_all_windows();
  }

  if(window->pending_input_type == evtype_LineInput)
    *val = window->pending_input_length;
  else
    *val = 0;

  return 0;
}


void z_wait_for_key(zwinid window)
{
  glui32 ch;
  do {
    z_draw_all_windows();
    glk_request_char_event(window->win);
    window->pending_input_type = evtype_CharInput;
  } while(waitforinput(window, &ch, NULL, 0) == 0);
  window->pending_input_type = 0;
}


zwinid check_valid_for_input(zwinid window)
{  
  glui32 y, i;
  if(!window->win) {
    zwinid newwin = NULL;
    for(i = 0; i < num_z_windows; i++) {
      if(game_windows[i].win) {
	newwin = &game_windows[i];
	break;
      }
    }
    if(!newwin)
      return NULL;

    if(window->wintype == wintype_TextGrid) {
      i = 0;
      for(y = 0; y < window->height; y++) {
	z_put_char(newwin, 13);
	z_put_styled_string(newwin, window->text_buffer + i,
			    window->color_buffer + i, window->width);
	i += window->width;
      }
      z_put_char(newwin, 13);
    }

    window = newwin;
  }
  return window;
}


/* returns number of characters read */
int z_read(zwinid window, char *dest, unsigned maxlen, unsigned initlen,
	   zword timer, BOOL (*timer_callback)(zword), zword timer_arg,
	   unsigned char *terminator)
{
  /* FIXME: support terminating characters when (if) glk gets support for
     them */
  unsigned i;
  unsigned ux, uy;
  glui32 length;
  BOOL done;

  if(automap_unexplore()) {
    read_abort = TRUE;
    return 0;
  }
  
  read_abort = FALSE;

  if(initlen > maxlen) {
    n_show_error(E_OUTPUT, "initlen > maxlen", initlen);
    return 0;
  }

  if(window == 0)
    window = &game_windows[0];
  
  if(window->pending_input_type != 0) {
    n_show_error(E_OUTPUT, "nested input attempted", 0);
    return 0;
  }

#ifdef DEBUGGING

  if(do_automap) {
    const char *dir = automap_explore();
    if(dir) {
      length = n_strlen(dir);
      if(length > maxlen)
	length = maxlen;
      n_strncpy(dest, dir, length);
      return length;
    }
  }
#endif

  glk_request_timer_events(timer * 100);  /* if time is zero, does nothing */
    
  if(initlen != 0 && window->wintype == wintype_TextBuffer) {
    BOOL good = FALSE;
    if(initlen <= window->curr_offset) {
      good = TRUE;
      for(i = 0; i < initlen; i++)  /* check the end of the linebuffer */
	if(window->text_buffer[window->curr_offset - initlen + i] != dest[i]) {
	  good = FALSE;
	  break;
	}
    }
    if(!good) {
      /* bleah */
      /* argh */
      /* oof */
    } else {
      window->curr_offset -= initlen; /* Remove initial text from linebuffer */
    }
  }
  
  if(window->wintype == wintype_TextGrid) {
    ux = window->curr_offset % window->width;
    uy = window->curr_offset / window->width;
  }

  z_flush_all_windows();
  window = check_valid_for_input(window);

  done = FALSE;
  length = initlen;
  while(!done) {
    int t;

    if(window->wintype == wintype_TextGrid)
      glk_window_move_cursor(window->win, ux, uy);

    if(input_stream1) {
      glui32 len = maxlen;
      *terminator = transcript_getline(dest, &len);
      length = len;
    }
    if(input_stream1) { /* If didn't EOF, input_stream1 will be non-zero */
      glk_stream_set_current(window->str);
      set_glk_stream_current();
      glk_set_style(style_Input);
      glk_put_buffer(dest, length);
      glk_put_char(10);
      done = TRUE;
    } else {
      glk_request_line_event(window->win, dest, maxlen, length);
      window->pending_input_type = evtype_LineInput;
    
      t = waitforinput(window, &length, timer_callback, timer_arg);
      if(t != 0) {
	if(t == -1)
	  *terminator = 0;
	else
	  *terminator = t;
	done = TRUE;
      }
    }

    if(done)
      stream4line(dest, length, *terminator);

#ifdef DEBUGGING
    if(done && length >= 2 && dest[0] == '/') {
      if(dest[1] == '/') {  /* "//" means no command, but start with "/" */
	for(i = 1; i < length; i++)
	  dest[i-1] = dest[i];
	length--;
      } else {
	done = FALSE;
	dest[length] = 0;
	
	process_debug_command(dest+1);

	if(read_abort)
	  done = TRUE;

	length = 0;
      }
    }
#endif
  }
  glk_request_timer_events(0);  /* stop timer */

  window->pending_input_type = 0;

  for(i = 0; i < num_z_windows; i++)
    game_windows[i].biggest_height = game_windows[i].last_height = game_windows[i].height;

  return length;
}

zword z_read_char(zwinid window,
		  zword timer, BOOL (*timer_callback)(zword), zword timer_arg)
{
  unsigned i;
  glui32 ch;
  zword validch = 0;

  if(automap_unexplore()) {
    read_abort = TRUE;
    return 0;
  }

  if(input_stream1) {
    unsigned num;
    validch = transcript_getchar(&num);
    if(!validch)
      validch = num;
  }
  if(input_stream1) {
    return validch;
  }
  
  read_abort = FALSE;

  glk_request_timer_events(timer * 100);

  z_flush_all_windows();
  window = check_valid_for_input(window);

  do {
    do {
      z_draw_all_windows();
      glk_request_char_event(window->win);
      window->pending_input_type = evtype_CharInput;
    } while(waitforinput(window, &ch, timer_callback, timer_arg) == 0);
    
    if(' ' <= ch && ch <= '~')
      validch = ch;
    
    switch(ch) {
    case 8:
    case keycode_Delete: validch = 8; break;
    case 9:
    case keycode_Tab:    validch = 9; break;
    case 13:
    case keycode_Return: validch = 13; break;
/*  case 21:
      if(restoreundo()) {
	read_abort = TRUE;
	return 0;
      }
      break; */
    case 27:
    case keycode_Escape: validch = 27; break;
    case 16:
    case keycode_Up:     validch = 129; break;
    case 14:
    case keycode_Down:   validch = 130; break;
    case 2:
    case keycode_Left:   validch = 131; break;
    case 6:
    case keycode_Right:  validch = 132; break;
    case keycode_Func1:  validch = 133; break;
    case keycode_Func2:  validch = 134; break;
    case keycode_Func3:  validch = 135; break;
    case keycode_Func4:  validch = 136; break;
    case keycode_Func5:  validch = 137; break;
    case keycode_Func6:  validch = 138; break;
    case keycode_Func7:  validch = 139; break;
    case keycode_Func8:  validch = 140; break;
    case keycode_Func9:  validch = 141; break;
    case keycode_Func10: validch = 142; break;
    case keycode_Func11: validch = 143; break;
    case keycode_Func12: validch = 144; break;
    }
  } while(!(validch || ch == 0));

  glk_request_timer_events(0);     /* stop timer */

  window->pending_input_type = 0;

  for(i = 0; i < num_z_windows; i++)
    game_windows[i].biggest_height = game_windows[i].last_height = game_windows[i].height;

  return validch;
}


/*
void zwin_init(int number, glui32 wintype,
	       glui32 x_coord, glui32 y_coord, glui32 x_size, glui32 y_size)
{
  zwinid self = game_windows + number;

  if(x_coord == self->x1) {
    if(y_coord == self->y1) {
      

  if(game_windows[number].win) {
    z_pause_timed_input(game_windows[number].win);
    glk_window_close(game_windows[number].win, NULL);
  }
  set_style_hints();
  game_windows[number].win = glk_window_open(
}
*/
