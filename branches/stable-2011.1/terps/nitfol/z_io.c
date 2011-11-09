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

#define UPPER_WINDOW 1
#define LOWER_WINDOW 0

static zwinid lower_win, upper_win;
static zwinid current_window;


#define STREAM1 1
#define STREAM2 2
#define STREAM3 4
#define STREAM4 8

static strid_t stream2, stream4;

static int output_stream;
static zword stream3_table_starts[16];
static zword stream3_table_locations[16];
static int stream3_nesting_depth;


static int  font = 1;

static BOOL abort_output = FALSE; /* quickly stop outputting */


BOOL is_transcripting(void)
{
  return (output_stream & STREAM2) != 0;
}

void set_transcript(strid_t stream)
{
  if(stream) {
    if(z_memory) {
      zword flags2 = LOWORD(HD_FLAGS2) | b00000001;
      LOWORDwrite(HD_FLAGS2, flags2);
    }
    stream2 = stream;
    output_stream |= STREAM2;
    if(lower_win)
      z_set_transcript(lower_win, stream2);
  } else {
    if(z_memory) {
      zword flags2 = LOWORD(HD_FLAGS2) & b11111110;
      LOWORDwrite(HD_FLAGS2, flags2);
    }
    output_stream &= ~STREAM2;
    if(lower_win)
      z_set_transcript(lower_win, 0);
  }
}


/* initialize the windowing environment */
void init_windows(BOOL dofixed, glui32 maxwidth, glui32 maxheight)
{
  z_init_windows(dofixed, draw_upper_callback, upper_mouse_callback,
		 maxwidth, maxheight, &upper_win, &lower_win);

  current_window = lower_win;
  output_stream = STREAM1 | (output_stream & STREAM2);
  stream3_nesting_depth = 0;
  font = 1;

  if(output_stream & STREAM2) {
    set_transcript(stream2);
  } else {
    set_transcript(0);
  }

  if(zversion == 6) {
    v6_main_window_is(lower_win);
  }
}


static int upper_roomname_length;

static void counting_glk_put_char(int ch)
{
  upper_roomname_length++;
  glk_put_char(ch);
}

glui32 draw_upper_callback(winid_t win, glui32 width, glui32 height)
{
  glui32 curx = 0, cury = 0;
  
  glui32 numlines = 0;

  if(win == NULL || height == 0) {
    if(zversion <= 3)
      numlines++;
    return numlines;
  }
  
  if(zversion <= 3) {
    zword location = get_var(16);
    offset short_name_off = object_name(location);

    glk_window_move_cursor(win, 0, cury);
    
    if(location && short_name_off) {
      glk_put_char(' '); curx++;
      upper_roomname_length = 0;
      decodezscii(short_name_off, counting_glk_put_char);
      curx += upper_roomname_length;
    }

    glk_window_move_cursor(win, width - 8, cury);
    if((zversion <= 2) || ((LOBYTE(HD_FLAGS1) & 2) == 0)) {
      if(width > curx + 26) {
	glk_window_move_cursor(win, width - 24, cury);
	w_glk_put_string("Score: ");
	g_print_znumber(get_var(17));
	
	glk_window_move_cursor(win, width - 12, cury);
	w_glk_put_string("Moves: ");
	g_print_znumber(get_var(18));
      } else {
	g_print_znumber(get_var(17)); /* score */
	glk_put_char('/');
	g_print_znumber(get_var(18)); /* turns */
      }
    } else {
      const char *ampmstr[8] = { " AM", " PM" };
      int ampm = 0;
      zword hours = get_var(17);
      zword minutes = get_var(18);
      while(hours >= 12) {
	hours-=12;
	ampm ^= 1;
      }
      if(hours == 0)
	hours = 12;
      if(hours < 10)
	glk_put_char(' ');
      g_print_number(hours);
      glk_put_char(':');
      if(minutes < 10)
	glk_put_char('0');
      g_print_number(minutes);
      w_glk_put_string(ampmstr[ampm]);
    }
    numlines++;
    cury++;
    glk_window_move_cursor(win, 0, cury);
  }

  return numlines;
}

void output_string(const char *s)
{
  while(*s)
    output_char(*s++);
}

void output_char(int c)
{
  static int starlength = 0;

  if(output_stream & STREAM3) {  /* Table output */
    zword num_chars = LOWORD(stream3_table_starts[stream3_nesting_depth-1]) +1;
    if((c < 32 && c !=  13) || (c >= 127 && c <= 159) || (c > 255))
      c = '?';     /* Section 7.5.3 */
    LOBYTEwrite(stream3_table_locations[stream3_nesting_depth-1], c);
    stream3_table_locations[stream3_nesting_depth-1] += 1;
    LOWORDwrite(stream3_table_starts[stream3_nesting_depth-1], num_chars);
  } else {
    if(output_stream & STREAM1) {  /* Normal screen output */
      if(c >= 155 && c <= 251) {  /* "extra characters" */
	zword game_unicode_table = header_extension_read(3);
	if(game_unicode_table && LOBYTE(game_unicode_table) >= (zbyte) (c - 155)) {
	  zword address = game_unicode_table + 1 + (c - 155) * 2;
	  c = LOWORD(address);
	} else {
	  const unsigned default_unicode_translation[] = {
	    0xe4, 0xf6, 0xfc, 0xc4, 0xd6, 0xdc, 0xdf, 0xbb,
	    0xab, 0xeb, 0xef, 0xff, 0xcb, 0xcf, 0xe1, 0xe9,
	    0xed, 0xf3, 0xfa, 0xfd, 0xc1, 0xc9, 0xcd, 0xd3,
	    0xda, 0xdd, 0xe0, 0xe8, 0xec, 0xf2, 0xf9, 0xc0,
	    0xc8, 0xcc, 0xd2, 0xd9, 0xe2, 0xea, 0xee, 0xf4,
	    0xfb, 0xc2, 0xca, 0xce, 0xd4, 0xdb, 0xe5, 0xc5,
	    0xf8, 0xd8, 0xe3, 0xf1, 0xf5, 0xc3, 0xd1, 0xd5,
	    0xe6, 0xc6, 0xe7, 0xc7, 0xfe, 0xf0, 0xde, 0xd0,
	    0xa3, 0x153, 0x152, 0xa1, 0xbf
	  };
	  c = default_unicode_translation[c - 155];
	}
      }
      if(c == '*') {
	if(++starlength == 3) /* Three asterisks usually means win or death */
	  if(automap_unexplore())
	    abort_output = TRUE;
      } else {
	starlength = 0;
      }

      if(font == 3) {
	const char font3trans[] =
	  " <>/"    "\\ --"   "||||"  "--\\/"   /* 32-47 */
	  "\\//\\/\\@ "   "   |"  "|-- "        /* 48-63 */
	  "   /"    "\\/\\ "  "    "  "    "    /* 64-79 */
	  "    "    "####"    "  X+"  "udb*"    /* 80-95 */
	  "?abc"    "defg"    "hijk"  "lmno"    /* 96-111 */
	  "pqrs"    "tuvw"    "xyzU"  "DB?";    /* 112-126 */
	if(c >= 32 && c <= 126)
	  c = font3trans[c - 32];
      }

      if(allow_output)
	z_put_char(current_window, c);
    }
  }
}

void n_print_number(unsigned n)
{
  int i;
  char buffer[12];
  int length = n_to_decimal(buffer, n);
  
  for(i = length - 1; i >= 0; i--)
    output_char(buffer[i]);
}


void g_print_number(unsigned n)
{
  int i;
  char buffer[12];
  int length = n_to_decimal(buffer, n);

  for(i = length - 1; i >= 0; i--)
    glk_put_char(buffer[i]);
}

void g_print_snumber(int n)
{
  if(n < 0) {
    glk_put_char('-');
    n = -n;
  }
  g_print_number(n);
}

void g_print_znumber(zword n)
{
  if(is_neg(n)) {
    glk_put_char('-');
    g_print_number(neg(n));
  } else {
    g_print_number(n);
  }
}

void n_print_znumber(zword n)
{
  if(is_neg(n)) {
    output_char('-');
    n_print_number(neg(n));
  } else {
    n_print_number(n);
  }
}


void stream4number(unsigned c)
{
  if(output_stream & STREAM4) {
    glk_stream_set_current(stream4);
    glk_put_char('[');
    g_print_number(c);
    glk_put_char(']');
    glk_put_char(10);
  }
}


void op_buffer_mode(void)
{
  /* FIXME: Glk can't really do this.
   * I could rely on the Plotkin Bug to do it, but that's ugly and would
   * break 20 years from now when somebody fixes it.  I could also print
   * spaces between each letter, which isn't the intended effect
   *
   * For now, do nothing.  Doubt this opcode is used often anyway...
   */
}


void op_check_unicode(void)
{
  unsigned result = 0;
  if(operand[0] <= 255 &&
     (glk_gestalt(gestalt_CharOutput, (unsigned char) operand[0]) !=
      gestalt_CharOutput_CannotPrint))
    result |= 1;
  if(operand[0] <= 255 &&
     (glk_gestalt(gestalt_LineInput, (unsigned char) operand[0]) !=
      FALSE))
    result |= 2;
  mop_store_result(result);
}


void op_erase_line(void)
{
  if(!allow_output)
    return;

  if(operand[0] == 1 && current_window == upper_win) {
    z_erase_line(current_window);
  }
}


void op_erase_window(void)
{
  if(!allow_output)
    return;

#ifdef DEBUG_IO
  n_show_debug(E_OUTPUT, "erase_window", operand[0]);
  return;
#endif

  switch(operand[0]) {
  case neg(1):
    operand[0] = 0; op_split_window();
    current_window = lower_win;
  case neg(2):
  case UPPER_WINDOW:
    z_clear_window(upper_win);
    if(operand[0] == UPPER_WINDOW) break; /* Ok, this is evil, but it works. */
  case LOWER_WINDOW:
    z_clear_window(lower_win);
    break;
  }
}


void op_get_cursor(void)
{
  zword x, y;
  z_getxy(upper_win, &x, &y);
  LOWORDwrite(operand[0], x);
  LOWORDwrite(operand[0] + ZWORD_SIZE, y);
}


void op_new_line(void)
{
  output_char(13);
}


void op_output_stream(void)
{
  if(operand[0] == 0)
    return;
  if(is_neg(operand[0])) {
    switch(neg(operand[0])) {
    case 1:
      if(!allow_output)
	return;
      output_stream &= ~STREAM1;
      break;

    case 2:
      if(!allow_output)
	return;
      set_transcript(0);
      break;

    case 3:
      if(stream3_nesting_depth)
	stream3_nesting_depth--;
      else
	n_show_error(E_OUTPUT, "stream3 unnested too many times", 0);
      if(!stream3_nesting_depth)
	output_stream &= ~STREAM3;
      break;

    case 4:
      if(!allow_output)
	return;
      glk_stream_close(stream4, NULL);
      stream4 = 0;
      output_stream &= ~STREAM4;
      break;

    default:
      n_show_error(E_OUTPUT, "unknown stream deselected", neg(operand[0]));
    }
  } else {
    switch(operand[0]) {
    case 1:
      if(!allow_output)
	return;
      output_stream |= STREAM1;
      break;

    case 2:
      if(!allow_output)
	return;
      if(!stream2) {
	stream2 = n_file_prompt(fileusage_Transcript | fileusage_TextMode,
				filemode_WriteAppend);
      }
      if(stream2)
	set_transcript(stream2);
      break;

    case 3:
      if(stream3_nesting_depth >= 16) {
	n_show_error(E_OUTPUT, "nesting stream 3 too deeply",
		   stream3_nesting_depth);
	return;
      }
      LOWORDwrite(operand[1], 0);
      stream3_table_starts[stream3_nesting_depth] = operand[1];
      stream3_table_locations[stream3_nesting_depth] = operand[1] + 2;
      
      output_stream |= STREAM3;
      stream3_nesting_depth++;
      break;
	
    case 4:
      if(!allow_output)
	return;
      stream4 = n_file_prompt(fileusage_InputRecord | fileusage_TextMode,
			      filemode_WriteAppend);
      if(stream4)
	output_stream |= STREAM4;
      break;
    default:
      n_show_error(E_OUTPUT, "unknown stream selected", operand[0]);
    }
  }
}


void op_print(void)
{
  int length;
  abort_output = FALSE;
  length = decodezscii(PC, output_char);
  if(!abort_output)
    PC += length;
}


void op_print_ret(void)
{
  int length;
  abort_output = FALSE;
  length = decodezscii(PC, output_char);
  if(abort_output)
    return;
  PC += length;
  output_char(13);
  if(abort_output)
    return;
  mop_func_return(1);
}


void op_print_addr(void)
{
  decodezscii(operand[0], output_char);
}


void op_print_paddr(void)
{
  getstring(operand[0]);
}


void op_print_char(void)
{
  if(operand[0] > 1023) {
    n_show_error(E_INSTR, "attempt to print character > 1023", operand[0]);
    return;
  }
  output_char(operand[0]);
}


void op_print_num(void)
{
  n_print_znumber(operand[0]);
}


void op_print_table(void)
{
  unsigned x, y;
  zword text = operand[0];
  zword width = operand[1];
  zword height = operand[2];
  zword skips = operand[3];

  zword startx, starty;
  unsigned win_width, win_height;

  z_getxy(current_window, &startx, &starty);
  z_getsize(current_window, &win_width, &win_height);

  if(numoperands < 4)
    skips = 0;
  if(numoperands < 3)
    height = 1;

  if(current_window == upper_win) {
    if(startx + width - 1 > win_width) {
      int diff;
      n_show_warn(E_OUTPUT, "table too wide; trimming", width);
      diff = startx + width - 1 - win_width;
      width -= diff;
      skips += diff;
    }
    if(starty + height - 1 > win_height) {
      n_show_warn(E_OUTPUT, "table too tall; trimming", height);
      height = win_height - starty + 1;
    }
  }

  for(y = 0; y < height; y++) {
    if(current_window == upper_win && allow_output)
      z_setxy(upper_win, startx, y+starty);

    for(x = 0; x < width; x++) {
      output_char(LOBYTE(text));
      text++;
    }
    text += skips;

    if(current_window != upper_win && y+1 < height)
      output_char(13);
  }
}


void op_set_colour(void)
{
  if(!allow_output)
    return;

  z_set_color(current_window, operand[0], operand[1]);
}


void op_set_cursor(void)
{
  unsigned width, height;
  zword x = operand[1];
  zword y = operand[0];
  
  if(!allow_output)
    return;
  
#ifdef DEBUG_IO
  n_show_debug(E_OUTPUT, "set_cursor y=", operand[0]);
  n_show_debug(E_OUTPUT, "set_cursor x=", operand[1]);
#endif

  if(current_window != upper_win) {
    return;
  }

  z_getsize(current_window, &width, &height);

  if(y == 0 || y > height) {    /* section 8.7.2.3 */
    n_show_error(E_OUTPUT, "illegal line for set_cursor", y);
    if(y == 0 || y > 512)
      return;
    z_set_height(upper_win, y); /* Resize to allow broken games to work */
  }
  if(x == 0 || x > width) {
    n_show_error(E_OUTPUT, "illegal column for set_cursor", x);
    return;
  }

  z_setxy(current_window, x, y);
}


void op_set_text_style(void)
{
  if(!allow_output)
    return;

  z_set_style(current_window, operand[0]);
}


void op_set_window(void)
{
  if(!allow_output)
    return;

#ifdef DEBUG_IO
  n_show_debug(E_OUTPUT, "set_window", operand[0]);
  return;
#endif

  switch(operand[0]) {
  case UPPER_WINDOW:
    current_window = upper_win;
    z_setxy(upper_win, 1, 1);
    break;
  case LOWER_WINDOW:
    current_window = lower_win;
    break;
  default:
    n_show_error(E_OUTPUT, "invalid window selected", operand[0]);
  }
}


void op_split_window(void)
{
  if(!allow_output)
    return;

#ifdef DEBUG_IO
  n_show_debug(E_OUTPUT, "split_window", operand[0]);
#endif

  if(zversion == 6)
    return;


  if(operand[0] > 512) {
    n_show_error(E_OUTPUT, "game is being ridiculous", operand[0]);
    return;
  }

  if(zversion == 3)
    z_set_height(upper_win, 0);  /* clear the whole upper window first */

  z_set_height(upper_win, operand[0]);
}


static BOOL timer_callback(zword routine)
{
  zword dummylocals[16];
  in_timer = TRUE;
  mop_call(routine, 0, dummylocals, -2); /* add a special stack frame */
  decode();                              /* start interpreting the routine */
  in_timer = FALSE;
  exit_decoder = FALSE;
  return time_ret;
}


BOOL upper_mouse_callback(BOOL is_char_event, winid_t win, glui32 x, glui32 y)
{
  int i;
  
  if(!(LOBYTE(HD_FLAGS2) & b00100000))
    return FALSE;

  header_extension_write(1, x + 1);
  header_extension_write(2, y + 1);

  stream4number(254);
  stream4number(x + 1);
  stream4number(y + 1);

  if(is_char_event)
    return TRUE;

  for(i = z_terminators; LOBYTE(i) != 0; i++)
    if(LOBYTE(i) == 255 || LOBYTE(i) == 254)
      return TRUE;

  /* @read will not receive mouse input inputs if they're not
   * terminating characters, but I'm not sure how to reasonably do
   * that and it shouldn't matter to most things */
  
  return FALSE;
}


typedef struct alias_entry alias_entry;

struct alias_entry
{
  alias_entry *next;
  char *from;
  char *to;
  BOOL in_use, is_recursive;
};

static alias_entry *alias_list = NULL;


void parse_new_alias(const char *aliascommand, BOOL is_recursive)
{
  char *from, *to;
  char *stringcopy = n_strdup(aliascommand);
  char *pcommand = stringcopy;
  while(isspace(*pcommand))
    pcommand++;
  from = pcommand;
  while(isgraph(*pcommand))
    pcommand++;
  if(*pcommand) {
    *pcommand = 0;
    pcommand++;
  }
  while(isspace(*pcommand))
    pcommand++;
  to = pcommand;

  while(*to == ' ')
    to++;
  
  if(*to == 0) /* Expand blank aliases to a single space */
    add_alias(from, " ", is_recursive);
  else
    add_alias(from, to, is_recursive);
  free(stringcopy);
}

void add_alias(const char *from, const char *to, BOOL is_recursive)
{
  alias_entry newalias;
  remove_alias(from);
  newalias.next = NULL;
  newalias.from = n_strdup(from);
  newalias.to = n_strdup(to);
  newalias.in_use = FALSE;
  newalias.is_recursive = is_recursive;
  LEadd(alias_list, newalias);
}


BOOL remove_alias(const char *from)
{
  alias_entry *p, *t;
  while(*from == ' ')
    from++;
  LEsearchremove(alias_list, p, t, n_strcmp(p->from, from) == 0, (n_free(p->from), n_free(p->to)));
  return t != NULL;
}


static alias_entry *find_alias(const char *text, int length)
{
  alias_entry *p;
  LEsearch(alias_list, p, n_strmatch(p->from, text, length));
  return p;
}


int search_for_aliases(char *text, int length, int maxlen)
{
  int word_start = 0;
  int i;
  if(!length)
    return length;
  for(i = 0; i <= length; i++) {
    if(i == length || isspace(text[i]) || ispunct(text[i])) {
      int word_length = i - word_start;
      if(word_length) {
	alias_entry *p = find_alias(text + word_start, word_length);
	if(p && !(p->in_use)) {
	  int newlen = strlen(p->to);
	  if(length - word_length + newlen > maxlen)
	    newlen = maxlen - length + word_length;
	  n_memmove(text + word_start + newlen,
		    text + word_start + word_length,
		    maxlen - word_start - MAX(newlen, word_length));
	  n_memcpy(text + word_start, p->to, newlen);

	  if(p->is_recursive) {
	    p->in_use = TRUE;
	    newlen = search_for_aliases(text + word_start, newlen, maxlen - word_start);
	    p->in_use = FALSE;
	  }
	  
	  length += newlen - word_length;
	  i = word_start + newlen;
	}
      }
      word_start = i+1;
    }
  }
  return length;
}


int n_read(zword dest, unsigned maxlen, zword parse, unsigned initlen,
           zword timer, zword routine, unsigned char *terminator)
{
  unsigned length;
  unsigned i;
  char *buffer = (char *) n_malloc(maxlen + 1);
  
#ifdef SMART_TOKENISER
  forget_corrections();
#endif

  if(false_undo)
    initlen = 0;
  false_undo = FALSE;

  if(maxlen < 3)
    n_show_warn(E_OUTPUT, "small text buffer", maxlen);

  if(dest > dynamic_size || dest < 64) {
    n_show_error(E_OUTPUT, "input buffer in invalid location", dest);
    return 0;
  }

  if(dest + maxlen > dynamic_size) {
    n_show_error(E_OUTPUT, "input buffer exceeds dynamic memory", dest + maxlen);
    maxlen = dynamic_size - dest;
  }
  
  if(parse >= dest && dest + maxlen > parse) {
    n_show_warn(E_OUTPUT, "input buffer overlaps parse", dest + maxlen - parse);
    maxlen = parse - dest;
  }

  for(i = 0; i < maxlen; i++)
    buffer[i] = LOBYTE(dest + i);

  length = z_read(current_window, buffer, maxlen, initlen, timer, timer_callback, routine, terminator);
    
  if(read_abort) {
    n_free(buffer);
    return 0;
  }

  length = search_for_aliases(buffer, length, maxlen);
  
  for(i = 0; i < length; i++) {
    buffer[i] = glk_char_to_lower(buffer[i]);
    LOBYTEwrite(dest + i, buffer[i]);
  }

  if(parse)
    z_tokenise(buffer, length, parse, z_dictionary, TRUE);

  n_free(buffer);
  return length;
}


void stream4line(const char *buffer, int length, char terminator)
{
  if(output_stream & STREAM4) {
    w_glk_put_buffer_stream(stream4, buffer, length);
    if(terminator != 10)
      stream4number(terminator);
    glk_put_char_stream(stream4, 10);
  }
}


void op_sread(void)
{
  unsigned maxlen, length;
  unsigned char term;
  zword text = operand[0];

  maxlen = LOBYTE(text) - 1;
  if(numoperands < 3)
    operand[2] = 0;
  if(numoperands < 4)
    operand[3] = 0;

  length = n_read(text + 1, maxlen, operand[1], 0,
		  operand[2], operand[3], &term);
  if(!read_abort) {
    LOBYTEwrite(text + 1 + length, 0);  /* zero terminator */

    if(allow_saveundo) {
      if(!has_done_save_undo && auto_save_undo)
        saveundo(FALSE);
      has_done_save_undo = FALSE;
    }
  }
}

void op_aread(void)
{
  int maxlen, length, initlen;
  unsigned char term;
  zword text = operand[0];

  maxlen = LOBYTE(text);
  initlen = LOBYTE(text + 1);
  if(numoperands < 3)
    operand[2] = 0;
  if(numoperands < 4)
    operand[3] = 0;

  length = n_read(text + 2, maxlen, operand[1], initlen,
		operand[2], operand[3], &term);
  if(!read_abort) {
    LOBYTEwrite(text + 1, length);
    mop_store_result(term);
  
    if(allow_saveundo) {
      if(!has_done_save_undo && auto_save_undo)
        saveundo(FALSE);
      has_done_save_undo = FALSE;
    }
  }
}

void op_read_char(void)
{
  zword validch = 0;

  if(in_timer) {
    n_show_error(E_OUTPUT, "input attempted during time routine", 0);
    mop_store_result(0);
    return;
  }

  if(operand[0] != 1) {
    n_show_warn(E_OUTPUT, "read_char with non-one first operand", operand[0]);
    mop_store_result(0);
    return;
  }

  if(numoperands < 2)
    operand[1] = 0;
  if(numoperands < 3)
    operand[2] = 0;

  validch = z_read_char(current_window, operand[1], timer_callback, operand[2]);

  if(read_abort)
    return;

  mop_store_result(validch);

  /*
  if(!has_done_save_undo && auto_save_undo_char) {
    saveundo(FALSE);
    has_done_save_undo = FALSE;
  }
  */
  
  stream4number(validch);
}


void op_show_status(void)
{
  if(!in_timer)
    z_flush_fixed(upper_win);
}

/* Returns a character, or returns 0 if it found a number, which it stores
   in *num */
unsigned char transcript_getchar(unsigned *num)
{
  glsi32 c;
  *num = 0;
  if(!input_stream1)
    return 0;

  c = glk_get_char_stream(input_stream1);

  if(c == GLK_EOF) {
      glk_stream_close(input_stream1, NULL);
      input_stream1 = 0;
      return 0;
  }
    
  if(c == '[') {
    while((c = glk_get_char_stream(input_stream1)) != GLK_EOF) {
      if(c == ']')
	break;
      if(c >= '0' && c <= '9') {
	*num = (*num * 10) + (c - '0');
      }
    }
    c = glk_get_char_stream(input_stream1);
    if(c != 10)
      n_show_error(E_OUTPUT, "input script not understood", c);

    return 0;
  }
  return c;
}

/* Returns line terminator.  Writes up to *len bytes to dest, writing in the
   actual number of characters read in *len. */
unsigned char transcript_getline(char *dest, glui32 *length)
{
  unsigned char term = 10;
  unsigned char c;
  unsigned num;
  glui32 len;
  if(!input_stream1) {
    *length = 0;
    return 0;
  }
  for(len = 0; len < *length; len++) {
    c = transcript_getchar(&num);
    if(!c) {
      term = num;
      break;
    }
    if(c == 10)
      break;

    dest[len] = c;
  }
  *length = len;
  return term;
}


void op_input_stream(void)
{
  /* *FIXME*: 10.2.4 says we shouldn't wait for [MORE]. Glk won't allow this */
  if(input_stream1)
    glk_stream_close(input_stream1, NULL);
  input_stream1 = 0;

  switch(operand[0]) {
  case 0:
    break;
  case 1:
    input_stream1 = n_file_prompt(fileusage_InputRecord | fileusage_TextMode,
				  filemode_Read);
    break;
  default:
    n_show_error(E_OUTPUT, "unknown input stream selected", operand[0]);
  }
}


void op_set_font(void)
{
  int lastfont;

  if(!allow_output) {
    mop_store_result(0);
    return;
  }

  lastfont = font;
  
#ifdef DEBUG_IO
  n_show_debug(E_OUTPUT, "set_font", operand[0]);
  return;
#endif

  switch(operand[0]) {
  case 1: font = 1; break;
  case 4: font = 4; break;
  case 3: if(enablefont3) font = 3;
  default: mop_store_result(0); return;
  }
  set_fixed(font == 4);

  mop_store_result(lastfont);
}


void op_print_unicode(void)
{
  if(!allow_output)
    return;
  if(operand[0] >= 256 || (operand[0] > 127 && operand[0] < 160)) {
    output_char('?');
    return;
  }
  if(output_stream & STREAM3) {
    if(operand[0] >= 160) {
      const unsigned char default_unicode_zscii_translation[] = {
        0x00, 0xde, 0x00, 0xdb, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0xa3, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0xa2, 0x00, 0x00, 0x00, 0xdf, 
        0xba, 0xaf, 0xc4, 0xd0, 0x9e, 0xca, 0xd4, 0xd6, 
        0xbb, 0xb0, 0xc5, 0xa7, 0xbc, 0xb1, 0xc6, 0xa8, 
        0xda, 0xd1, 0xbd, 0xb2, 0xc7, 0xd2, 0x9f, 0x00, 
        0xcc, 0xbe, 0xb3, 0xc8, 0xa0, 0xb4, 0xd9, 0xa1, 
        0xb5, 0xa9, 0xbf, 0xcd, 0x9b, 0xc9, 0xd3, 0xd5, 
        0xb6, 0xaa, 0xc0, 0xa4, 0xb7, 0xab, 0xc1, 0xa5, 
        0xd8, 0xce, 0xb8, 0xac, 0xc2, 0xcf, 0x9c, 0x00, 
        0xcb, 0xb9, 0xad, 0xc3, 0x9d, 0xae, 0xd7, 0xa6
      };
      unsigned char c = default_unicode_zscii_translation[operand[0] - 160];
      output_char(c == 0 ? '?' : c);
    } else if(operand[0] == 10) {
      output_char(13);
    } else {
      output_char(operand[0]);
    }
  } else {
    if(output_stream & STREAM1) {
      z_put_char(current_window, operand[0]);
    }
  }
}
