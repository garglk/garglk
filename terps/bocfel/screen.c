/*-
 * Copyright 2010-2017 Chris Spiegel.
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

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdbool.h>
#include <limits.h>
#include <inttypes.h>

#ifdef ZTERP_GLK
#include <glk.h>

#if defined(ZTERP_WIN32) && !defined(GARGLK)
/* rpcndr.h, eventually included via WinGlk.h, defines a type “byte”
 * which conflicts with the “byte” from memory.h. Temporarily redefine
 * it to avoid a compile error.
 */
#define byte rpc_byte
/* Windows Glk puts non-standard declarations (specifically for this
 * file, those guarded by GLK_MODULE_GARGLKTEXT) in WinGlk.h, so include
 * it to get color/style extensions.
 */
#include <WinGlk.h>
#undef byte
#endif
#endif

#include "screen.h"
#include "branch.h"
#include "dict.h"
#include "io.h"
#include "meta.h"
#include "memory.h"
#include "objects.h"
#include "osdep.h"
#include "process.h"
#include "stack.h"
#include "unicode.h"
#include "util.h"
#include "zterp.h"

bool header_fixed_font;

/* This variable stores whether Unicode is supported by the current Glk
 * implementation, which determines whether Unicode or Latin-1 Glk
 * functions are used. In addition, for both Glk and non-Glk versions,
 * this affects the behavior of @check_unicode. In non-Glk versions,
 * this is always true.
 */
static bool have_unicode;

#define ANSI_COLOR(c)	((struct color) { .mode = ColorModeANSI, .value = c })
#define TRUE_COLOR(c)	((struct color) { .mode = ColorModeTrue, .value = c })
#define DEFAULT_COLOR()	((struct color) { .mode = ColorModeDefault })

static struct window
{
  unsigned style;
  struct color fg_color, bg_color;

  enum font { FONT_NONE = -1, FONT_PREVIOUS, FONT_NORMAL, FONT_PICTURE, FONT_CHARACTER, FONT_FIXED } font;
  enum font prev_font;

#ifdef ZTERP_GLK
  winid_t id;
  long x, y; /* Only meaningful for window 1 */
  bool pending_read;
  union line
  {
    char latin1[256];
    glui32 unicode[256];
  } *line;
  bool has_echo;
#endif
} windows[8], *mainwin = &windows[0], *curwin = &windows[0];
#ifdef ZTERP_GLK
static struct window *upperwin = &windows[1];
static struct window statuswin;
static long upper_window_height = 0;
static long upper_window_width = 0;
static winid_t errorwin;
#endif

/* In all versions but 6, styles and colors are global and stored in
 * mainwin. For V6, they’re tracked per window and thus stored in each
 * individual window. For convenience, this macro expands to the “style
 * window” for any version.
 */
#define style_window	(zversion == 6 ? curwin : mainwin)

/* If the window needs to be temporarily switched (@show_status and
 * @print_form print to specific windows, and window_change() might
 * temporarily need to switch to the upper window), the code that
 * requires a specific window can be wrapped in these macros.
 */
#ifdef ZTERP_GLK
#define SWITCH_WINDOW_START(win)	{ struct window *saved_ = curwin; curwin = (win); glk_set_window((win)->id);
#define SWITCH_WINDOW_END()		curwin = saved_; glk_set_window(curwin->id); }
#else
#define SWITCH_WINDOW_START(win)	{ struct window *saved_ = curwin; curwin = (win);
#define SWITCH_WINDOW_END()		curwin = saved_; }
#endif

/* Output stream bits. */
#define STREAM_SCREEN		(1U << 1)
#define STREAM_TRANS		(1U << 2)
#define STREAM_MEMORY		(1U << 3)
#define STREAM_SCRIPT		(1U << 4)

static unsigned int streams = STREAM_SCREEN;
static zterp_io *transio, *scriptio;

#define MAX_STREAM_TABLES	16
static struct
{
  uint16_t table;
  uint16_t i;
} stream_tables[MAX_STREAM_TABLES];
static int stream_tables_index = -1;

static int istream = ISTREAM_KEYBOARD;
static zterp_io *istreamio;

struct input
{
  enum { INPUT_CHAR, INPUT_LINE } type;

  /* ZSCII value of key read for @read_char. */
  uint8_t key;

  /* Unicode line of chars read for @read. */
  uint32_t *line;
  uint8_t maxlen;
  uint8_t len;
  uint8_t preloaded;

  /* Character used to terminate input.  If terminating keys are not
   * supported by the Glk implementation being used (or if Glk is not
   * used at all) this will be ZSCII_NEWLINE; or in the case of
   * cancellation, 0.
   */
  uint8_t term;
};

/* Convert a 15-bit color to a 24-bit color. */
uint32_t screen_convert_color(uint16_t color)
{
  /* Map 5-bit color values to 8-bit. */
  const uint8_t table[] = {
    0x00, 0x08, 0x10, 0x19, 0x21, 0x29, 0x31, 0x3a,
    0x42, 0x4a, 0x52, 0x5a, 0x63, 0x6b, 0x73, 0x7b,
    0x84, 0x8c, 0x94, 0x9c, 0xa5, 0xad, 0xb5, 0xbd,
    0xc5, 0xce, 0xd6, 0xde, 0xe6, 0xef, 0xf7, 0xff
  };

  return table[(color >>  0) & 0x1f] << 16 |
         table[(color >>  5) & 0x1f] <<  8 |
         table[(color >> 10) & 0x1f] <<  0;
}

#ifdef GLK_MODULE_GARGLKTEXT
static glui32 zcolor_map[] = {
  0,
  0,

  0x000000,	/* Black */
  0xef0000,	/* Red */
  0x00d600,	/* Green */
  0xefef00,	/* Yellow */
  0x006bb5,	/* Blue */
  0xff00ff,	/* Magenta */
  0x00efef,	/* Cyan */
  0xffffff,	/* White */
  0xb5b5b5,	/* Light grey */
  0x8c8c8c,	/* Medium grey */
  0x5a5a5a,	/* Dark grey */
};

void update_color(int which, unsigned long color)
{
  if(which < 2 || which > 12) return;

  zcolor_map[which] = color;
}

/* Idea from Nitfol. */
static const int style_map[] =
{
  style_Normal,
  style_Normal,

  style_Subheader,	/* Bold */
  style_Subheader,	/* Bold */
  style_Emphasized,	/* Italic */
  style_Emphasized,	/* Italic */
  style_Alert,		/* Bold Italic */
  style_Alert,		/* Bold Italic */
  style_Preformatted,	/* Fixed */
  style_Preformatted,	/* Fixed */
  style_User1,		/* Bold Fixed */
  style_User1,		/* Bold Fixed */
  style_User2,		/* Italic Fixed */
  style_User2,		/* Italic Fixed */
  style_Note,		/* Bold Italic Fixed */
  style_Note,		/* Bold Italic Fixed */
};

static glui32 gargoyle_color(const struct color *color)
{
  switch(color->mode)
  {
    case ColorModeANSI:
      return zcolor_map[color->value];
    case ColorModeTrue:
      return screen_convert_color(color->value);
    case ColorModeDefault:
      return zcolor_Default;
  }

  return zcolor_Current;
}
#endif

#ifdef ZTERP_GLK
/* This macro makes it so that code elsewhere needn’t check have_unicode before printing. */
#define GLK_PUT_CHAR(c)		do { if(!have_unicode) glk_put_char(unicode_to_latin1[c]); else glk_put_char_uni(c); } while(false)
#endif

void set_current_style(void)
{
#ifdef ZTERP_GLK
  unsigned style = style_window->style;
  if(curwin->id == NULL) return;

#ifdef GLK_MODULE_GARGLKTEXT
  if(curwin->font == FONT_FIXED || header_fixed_font) style |= STYLE_FIXED;

  if(options.disable_fixed) style &= ~STYLE_FIXED;

  ZASSERT(style < 16, "invalid style selected: %x", (unsigned)style);

  glk_set_style(style_map[style]);

  garglk_set_reversevideo(style & STYLE_REVERSE);

  garglk_set_zcolors(gargoyle_color(&style_window->fg_color), gargoyle_color(&style_window->bg_color));
#else
  /* Yes, there are three ways to indicate that a fixed-width font should be used. */
  bool use_fixed_font = (style & STYLE_FIXED) || curwin->font == FONT_FIXED || header_fixed_font;

  /* Glk can’t mix other styles with fixed-width, but the upper window
   * is always fixed, so if it is selected, there is no need to
   * explicitly request it here.  In addition, the user can disable
   * fixed-width fonts or tell Bocfel to assume that the output font is
   * already fixed (e.g. in an xterm); in either case, there is no need
   * to request a fixed font.
   * This means that another style can also be applied if applicable.
   */
  if(use_fixed_font &&
     !options.disable_fixed &&
     !options.assume_fixed &&
     curwin != upperwin)
  {
    glk_set_style(style_Preformatted);
    return;
  }

  /* According to standard 1.1, if mixed styles aren't available, the
   * priority is Fixed, Italic, Bold, Reverse.
   */
  if     (style & STYLE_ITALIC)  glk_set_style(style_Emphasized);
  else if(style & STYLE_BOLD)    glk_set_style(style_Subheader);
  else if(style & STYLE_REVERSE) glk_set_style(style_Alert);
  else                           glk_set_style(style_Normal);
#endif
#else
  zterp_os_set_style(style_window->style, &style_window->fg_color, &style_window->bg_color);
#endif
}

#ifdef ZTERP_GLK
/* Both the upper and lower windows have their own issues to deal with
 * when there is line input.  This function ensures that the cursor
 * position is properly tracked in the upper window, and if possible,
 * aids in the suppression of newline printing on input cancellation in
 * the lower window.
 */
static void cleanup_screen(struct input *input)
{
  if(input->type != INPUT_LINE) return;

  /* If the current window is the upper window, the position of the
   * cursor needs to be tracked, so after a line has successfully been
   * read, advance the cursor to the initial position of the next line,
   * or if a terminating key was used or input was canceled, to the end
   * of the input.
   */
  if(curwin == upperwin)
  {
    if(input->term != ZSCII_NEWLINE) upperwin->x += input->len;

    if(input->term == ZSCII_NEWLINE || upperwin->x >= upper_window_width)
    {
      upperwin->x = 0;
      if(upperwin->y < upper_window_height) upperwin->y++;
    }

    glk_window_move_cursor(upperwin->id, upperwin->x, upperwin->y);
  }

  /* If line input echoing is turned off, newlines will not be printed
   * when input is canceled, but neither will the input line.  Fix that.
   */
  if(curwin->has_echo)
  {
    glk_set_style(style_Input);
    for(int i = 0; i < input->len; i++) GLK_PUT_CHAR(input->line[i]);
    if(input->term == ZSCII_NEWLINE) glk_put_char(UNICODE_LINEFEED);
    set_current_style();
  }
}

/* In an interrupt, if the story tries to read or write, the previous
 * read event (which triggered the interrupt) needs to be canceled.
 * This function does the cancellation.
 */
static void cancel_read_events(struct window *window)
{
  if(window->pending_read)
  {
    event_t ev;

    glk_cancel_char_event(window->id);
    glk_cancel_line_event(window->id, &ev);

    /* If the pending read was a line input, zero terminate the string
     * so when it’s re-requested the length of the already-loaded
     * portion can be discovered.  Also deal with cursor positioning in
     * the upper window, and line echoing in the lower window.
     */
    if(ev.type == evtype_LineInput && window->line != NULL)
    {
      uint32_t line[ev.val1];
      struct input input = { .type = INPUT_LINE, .line = line, .term = 0, .len = ev.val1 };

      if(have_unicode) window->line->unicode[ev.val1] = 0;
      else             window->line->latin1 [ev.val1] = 0;

      for(int i = 0; i < input.len; i++)
      {
        if(have_unicode) line[i] = window->line->unicode[i];
        else             line[i] = (unsigned char)window->line->latin1 [i];
      }

      cleanup_screen(&input);
    }

    window->pending_read = false;
    window->line = NULL;
  }
}
#endif

/* Print out a character.  The character is in “c” and is either Unicode
 * or ZSCII; if the former, “unicode” is true.
 */
static void put_char_base(uint16_t c, bool unicode)
{
  if(c == 0) return;

  if(streams & STREAM_MEMORY)
  {
    ZASSERT(stream_tables_index != -1, "invalid stream table");

    /* When writing to memory, ZSCII should always be used (§7.5.3). */
    if(unicode) c = unicode_to_zscii_q[c];

    user_store_byte(stream_tables[stream_tables_index].table + stream_tables[stream_tables_index].i++, c);
  }
  else
  {
    /* For screen and transcription, always prefer Unicode. */
    if(!unicode) c = zscii_to_unicode[c];

    if(c != 0)
    {
      uint8_t zscii = 0;

      /* §16 makes no mention of what a newline in font 3 should map to.
       * Other interpreters that implement font 3 assume it stays a
       * newline, and this makes the most sense, so don’t do any
       * translation in that case.
       */
      if(curwin->font == FONT_CHARACTER && !options.disable_graphics_font && c != UNICODE_LINEFEED)
      {
        zscii = unicode_to_zscii[c];

        /* These four characters have a “built-in” reverse video (see §16). */
        if(zscii >= 123 && zscii <= 126)
        {
          style_window->style ^= STYLE_REVERSE;
          set_current_style();
        }

        c = zscii_to_font3[zscii];
      }
#ifdef ZTERP_GLK
      if((streams & STREAM_SCREEN) && curwin->id != NULL)
      {
        cancel_read_events(curwin);

        if(curwin == upperwin)
        {
          /* Interpreters seem to have differing ideas about what
           * happens when the cursor reaches the end of a line in the
           * upper window.  Some wrap, some let it run off the edge (or,
           * at least, stop the text at the edge).  The standard, from
           * what I can see, says nothing on this issue.  Follow Windows
           * Frotz and don’t wrap.
           */

          if(c == UNICODE_LINEFEED)
          {
            if(upperwin->y < upper_window_height)
            {
              /* Glk wraps, so printing a newline when the cursor has
               * already reached the edge of the screen will produce two
               * newlines.
               */
              if(upperwin->x < upper_window_width) GLK_PUT_CHAR(c);

              /* Even if a newline isn’t explicitly printed here
               * (because the cursor is at the edge), setting
               * upperwin->x to 0 will cause the next character to be on
               * the next line because the text will have wrapped.
               */
              upperwin->x = 0;
              upperwin->y++;
            }
          }
          else if(upperwin->x < upper_window_width && upperwin->y < upper_window_height)
          {
            upperwin->x++;
            GLK_PUT_CHAR(c);
          }
        }
        else
        {
          GLK_PUT_CHAR(c);
        }
      }
#else
      if((streams & STREAM_SCREEN) && curwin == mainwin) zterp_io_putc(zterp_io_stdout(), c);
#endif

      /* If the reverse video bit was flipped (for the character font), flip it back. */
      if(zscii >= 123 && zscii <= 126)
      {
        style_window->style ^= STYLE_REVERSE;
        set_current_style();
      }

      if((streams & STREAM_TRANS) && curwin == mainwin) zterp_io_putc(transio, c);
    }
  }
}

void put_char_u(uint16_t c)
{
  put_char_base(c, true);
}

void put_char(uint8_t c)
{
  put_char_base(c, false);
}

/* Print a C string to the screen, using put_char_u(). */
void screen_print(const char *s)
{
  unsigned int saved_streams = streams;

  SWITCH_WINDOW_START(mainwin);
  streams = STREAM_SCREEN;
  for(size_t i = 0; s[i] != 0; i++)
  {
    put_char_u(char_to_unicode(s[i]));
  }
  streams = saved_streams;
  SWITCH_WINDOW_END();
}

void screen_printf(const char *fmt, ...)
{
  va_list ap;
  char message[1024];

  va_start(ap, fmt);
  vsnprintf(message, sizeof message, fmt, ap);
  va_end(ap);

  screen_print(message);
}

void screen_puts(const char *s)
{
  screen_print(s);
  screen_print("\n");
}

void show_message(const char *fmt, ...)
{
  va_list ap;
  char message[1024];

  va_start(ap, fmt);
  vsnprintf(message, sizeof message, fmt, ap);
  va_end(ap);

#ifdef ZTERP_GLK
  static glui32 error_lines = 0;

  if(errorwin != NULL)
  {
    glui32 w, h;

    /* Allow multiple messages to stack, but force at least 5 lines to
     * always be visible in the main window.  This is less than perfect
     * because it assumes that each message will be less than the width
     * of the screen, but it’s not a huge deal, really; even if the
     * lines are too long, at least Gargoyle and glktermw are graceful
     * enough.
     */
    glk_window_get_size(mainwin->id, &w, &h);

    if(h > 5) glk_window_set_arrangement(glk_window_get_parent(errorwin), winmethod_Below | winmethod_Fixed, ++error_lines, errorwin);
    glk_put_char_stream(glk_window_get_stream(errorwin), UNICODE_LINEFEED);
  }
  else
  {
    errorwin = glk_window_open(mainwin->id, winmethod_Below | winmethod_Fixed, error_lines = 2, wintype_TextBuffer, 0);
  }

  /* If windows are not supported (e.g. in cheapglk or no Glk), messages
   * will not get displayed.  If this is the case, print to the main
   * window.
   */
  if(errorwin != NULL)
  {
    glk_set_style_stream(glk_window_get_stream(errorwin), style_Alert);
    glk_put_string_stream(glk_window_get_stream(errorwin), message);
  }
  else
#endif
  {
    screen_printf("\n[%s]\n", message);
  }
}

/* See §7.
 * This returns true if the stream was successfully selected.
 * Deselecting a stream is always successful.
 */
bool output_stream(int16_t number, uint16_t table)
{
  ZASSERT(labs(number) <= (zversion >= 3 ? 4 : 2), "invalid output stream selected: %ld", (long)number);

  if(number == 0)
  {
    return true;
  }
  else if(number > 0)
  {
    streams |= 1U << number;
  }
  else if(number < 0)
  {
    if(number != -3 || stream_tables_index == 0) streams &= ~(1U << -number);
  }

  if(number == 2)
  {
    store_word(0x10, word(0x10) | FLAGS2_TRANSCRIPT);
    if(transio == NULL)
    {
      transio = zterp_io_open(options.transcript_name, options.overwrite_transcript ? ZTERP_IO_WRONLY : ZTERP_IO_APPEND, ZTERP_IO_TRANS);
      if(transio == NULL)
      {
        store_word(0x10, word(0x10) & ~FLAGS2_TRANSCRIPT);
        streams &= ~STREAM_TRANS;
        warning("unable to open the transcript");
      }
    }
  }
  else if(number == -2)
  {
    store_word(0x10, word(0x10) & ~FLAGS2_TRANSCRIPT);
  }

  if(number == 3)
  {
    stream_tables_index++;
    ZASSERT(stream_tables_index < MAX_STREAM_TABLES, "too many stream tables");

    stream_tables[stream_tables_index].table = table;
    user_store_word(stream_tables[stream_tables_index].table, 0);
    stream_tables[stream_tables_index].i = 2;
  }
  else if(number == -3 && stream_tables_index >= 0)
  {
    user_store_word(stream_tables[stream_tables_index].table, stream_tables[stream_tables_index].i - 2);
    if(zversion == 6) store_word(0x30, stream_tables[stream_tables_index].i - 2);
    stream_tables_index--;
  }

  if(number == 4)
  {
    if(scriptio == NULL)
    {
      scriptio = zterp_io_open(options.record_name, ZTERP_IO_WRONLY, ZTERP_IO_INPUT);
      if(scriptio == NULL)
      {
        streams &= ~STREAM_SCRIPT;
        warning("unable to open the script");
      }
    }
  }
  /* XXX v6 has even more handling */

  return number < 0 || (streams & (1U << number));
}

void zoutput_stream(void)
{
  output_stream(as_signed(zargs[0]), zargs[1]);
}

/* See §10.
 * This returns true if the stream was successfully selected.
 */
bool input_stream(int which)
{
  istream = which;

  if(istream == ISTREAM_KEYBOARD)
  {
    if(istreamio != NULL)
    {
      zterp_io_close(istreamio);
      istreamio = NULL;
    }
  }
  else if(istream == ISTREAM_FILE)
  {
    if(istreamio == NULL)
    {
      istreamio = zterp_io_open(options.replay_name, ZTERP_IO_RDONLY, ZTERP_IO_INPUT);
      if(istreamio == NULL)
      {
        warning("unable to open the command script");
        istream = ISTREAM_KEYBOARD;
      }
    }
  }
  else
  {
    ZASSERT(0, "invalid input stream: %d", istream);
  }

  return istream == which;
}

void zinput_stream(void)
{
  input_stream(zargs[0]);
}

/* This does not even pretend to understand V6 windows. */
static void set_current_window(struct window *window)
{
  curwin = window;

#ifdef ZTERP_GLK
  if(curwin == upperwin && upperwin->id != NULL)
  {
    upperwin->x = upperwin->y = 0;
    glk_window_move_cursor(upperwin->id, 0, 0);
  }

  glk_set_window(curwin->id);
#endif

  set_current_style();
}

/* Find and validate a window.  If window is -3 and the story is V6,
 * return the current window.
 */
static struct window *find_window(uint16_t window)
{
  int16_t w = as_signed(window);

  ZASSERT(zversion == 6 ? w == -3 || (w >= 0 && w < 8) : w == 0 || w == 1, "invalid window selected: %d", w);

  if(w == -3) return curwin;

  return &windows[w];
}

#ifdef ZTERP_GLK
/* When resizing the upper window, the screen’s contents should not
 * change (§8.6.1); however, the way windows are handled with Glk makes
 * this slightly impossible.  When an Inform game tries to display
 * something with “box”, it expands the upper window, displays the quote
 * box, and immediately shrinks the window down again.  This is a
 * problem under Glk because the window immediately disappears.  Other
 * games, such as Bureaucracy, expect the upper window to shrink as soon
 * as it has been requested.  Thus the following system is used:
 *
 * If a request is made to shrink the upper window, it is granted
 * immediately if there has been user input since the last window resize
 * request.  If there has not been user input, the request is delayed
 * until after the next user input is read.
 */
static long delayed_window_shrink = -1;
static bool saw_input;

static void update_delayed(void)
{
  glui32 height;

  if(delayed_window_shrink == -1 || upperwin->id == NULL) return;

  glk_window_set_arrangement(glk_window_get_parent(upperwin->id), winmethod_Above | winmethod_Fixed, delayed_window_shrink, upperwin->id);
  upper_window_height = delayed_window_shrink;

  /* Glk might resize the window to a smaller height than was requested,
   * so track the actual height, not the requested height.
   */
  glk_window_get_size(upperwin->id, NULL, &height);
  if(height != upper_window_height)
  {
    /* This message probably won’t be seen in a window since the upper
     * window is likely covering everything, but try anyway.
     */
    show_message("Unable to fulfill window size request: wanted %ld, got %lu", delayed_window_shrink, (unsigned long)height);
    upper_window_height = height;
  }

  delayed_window_shrink = -1;
}

static void clear_window(struct window *window)
{
  if(window->id == NULL) return;

  /* glk_window_clear() cannot be used while there are pending read requests. */
  cancel_read_events(window);

  glk_window_clear(window->id);

  window->x = window->y = 0;
}
#endif

/* If restoring from an interrupt (which is a bad idea to begin with),
 * it’s entirely possible that there will be pending read events that
 * need to be canceled, so allow that.
 */
void cancel_all_events(void)
{
#ifdef ZTERP_GLK
  for(int i = 0; i < 8; i++) cancel_read_events(&windows[i]);
#endif
}

static void resize_upper_window(long nlines)
{
#ifdef ZTERP_GLK
  if(upperwin->id == NULL) return;

  long previous_height = upper_window_height;

  /* To avoid code duplication, put all window resizing code in
   * update_delayed() and, if necessary, call it from here.
   */
  delayed_window_shrink = nlines;
  if(upper_window_height <= nlines || saw_input) update_delayed();

  /* If the window is being created, or if it’s shrinking and the cursor
   * is no longer inside the window, move the cursor to the origin.
   */
  if(previous_height == 0 || upperwin->y >= nlines)
  {
    upperwin->x = upperwin->y = 0;
    if(nlines > 0) glk_window_move_cursor(upperwin->id, 0, 0);
  }

  saw_input = false;

  /* §8.6.1.1.2 */
  if(zversion == 3) clear_window(upperwin);

  /* As in a few other areas, changing the upper window causes reverse
   * video to be deactivated, so reapply the current style.
   */
  set_current_style();
#endif
}

void close_upper_window(void)
{
  /* The upper window is never destroyed; rather, when it’s closed, it
   * shrinks to zero height.
   */
  resize_upper_window(0);

#ifdef ZTERP_GLK
  delayed_window_shrink = -1;
  saw_input = false;
#endif

  set_current_window(mainwin);
}

void get_screen_size(unsigned int *width, unsigned int *height)
{
  *width  = 80;
  *height = 24;

#ifdef ZTERP_GLK
  glui32 w, h;

  /* The main window can be proportional, and if so, its width is not
   * generally useful because games tend to care about width with a
   * fixed font.  If a status window is available, or if an upper window
   * is available, use that to calculate the width, because these
   * windows will have a fixed-width font.  The height is the combined
   * height of all windows.
   */
  glk_window_get_size(mainwin->id, &w, &h);
  *height = h;
  if(statuswin.id != NULL)
  {
    glk_window_get_size(statuswin.id, &w, &h);
    *height += h;
  }
  if(upperwin->id != NULL)
  {
    glk_window_get_size(upperwin->id, &w, &h);
    *height += h;
  }
  *width = w;
#else
  zterp_os_get_screen_size(width, height);
#endif

  /* XGlk does not report the size of textbuffer windows, so here’s a safety net. */
  if(*width  == 0) *width  = 80;
  if(*height == 0) *height = 24;

  /* Terrible hack: Because V6 is not properly supported, the window to
   * which Journey writes its story is completely covered up by window
   * 1.  For the same reason, only the bottom 6 lines of window 1 are
   * actually useful, even though the game expands it to cover the whole
   * screen.  By pretending that the screen height is only 6, the main
   * window, where text is actually sent, becomes visible.
   */
  if(is_journey() && *height > 6) *height = 6;
}

#ifdef GLK_MODULE_LINE_TERMINATORS
static uint32_t *term_keys, term_size, term_nkeys;

void term_keys_reset(void)
{
  free(term_keys);
  term_keys = NULL;
  term_size = 0;
  term_nkeys = 0;
}

static void insert_key(uint32_t key)
{
  if(term_nkeys == term_size)
  {
    term_size += 32;

    term_keys = realloc(term_keys, term_size * sizeof *term_keys);
    if(term_keys == NULL) die("unable to allocate memory for terminating keys");
  }

  term_keys[term_nkeys++] = key;
}

void term_keys_add(uint8_t key)
{
  switch(key)
  {
    case ZSCII_UP:    insert_key(keycode_Up); break;
    case ZSCII_DOWN:  insert_key(keycode_Down); break;
    case ZSCII_LEFT:  insert_key(keycode_Left); break;
    case ZSCII_RIGHT: insert_key(keycode_Right); break;
    case ZSCII_F1:    insert_key(keycode_Func1); break;
    case ZSCII_F2:    insert_key(keycode_Func2); break;
    case ZSCII_F3:    insert_key(keycode_Func3); break;
    case ZSCII_F4:    insert_key(keycode_Func4); break;
    case ZSCII_F5:    insert_key(keycode_Func5); break;
    case ZSCII_F6:    insert_key(keycode_Func6); break;
    case ZSCII_F7:    insert_key(keycode_Func7); break;
    case ZSCII_F8:    insert_key(keycode_Func8); break;
    case ZSCII_F9:    insert_key(keycode_Func9); break;
    case ZSCII_F10:   insert_key(keycode_Func10); break;
    case ZSCII_F11:   insert_key(keycode_Func11); break;
    case ZSCII_F12:   insert_key(keycode_Func12); break;

    /* Keypad 0–9 should be here, but Glk doesn’t support that. */
    case ZSCII_KEY0: case ZSCII_KEY1: case ZSCII_KEY2: case ZSCII_KEY3:
    case ZSCII_KEY4: case ZSCII_KEY5: case ZSCII_KEY6: case ZSCII_KEY7:
    case ZSCII_KEY8: case ZSCII_KEY9:
      break;

    /* Mouse clicks would go here if I supported them. */
    case ZSCII_CLICK_MENU: case ZSCII_CLICK_DOUBLE: case ZSCII_CLICK_SINGLE:
      break;

    case 255:
      for(int i = 129; i <= 144; i++) term_keys_add(i);
      break;

    default:
      ZASSERT(0, "invalid terminating key: %u", (unsigned)key);
      break;
  }
}
#endif

/* Decode and print a zcode string at address “addr”.  This can be
 * called recursively thanks to abbreviations; the initial call should
 * have “in_abbr” set to 0.
 * Each time a character is decoded, it is passed to the function
 * “outc”.
 */
static int print_zcode(uint32_t addr, bool in_abbr, void (*outc)(uint8_t))
{
  int abbrev = 0, shift = 0, special = 0;
  int c, lastc;
  uint16_t w;
  uint32_t counter = addr;
  int current_alphabet = 0;

  do
  {
    ZASSERT(counter < memory_size - 1, "string runs beyond the end of memory");

    w = word(counter);

    for(int i = 10; i >= 0; i -= 5)
    {
      c = (w >> i) & 0x1f;

      if(special != 0)
      {
        if(special == 2) lastc = c;
        else             outc((lastc << 5) | c);

        special--;
      }

      else if(abbrev != 0)
      {
        uint32_t new_addr;

        new_addr = user_word(header.abbr + 64 * (abbrev - 1) + 2 * c);

        /* new_addr is a word address, so multiply by 2 */
        print_zcode(new_addr * 2, true, outc);

        abbrev = 0;
      }

      else switch(c)
      {
        case 0:
          outc(ZSCII_SPACE);
          shift = 0;
          break;
        case 1:
          if(zversion == 1)
          {
            outc(ZSCII_NEWLINE);
            shift = 0;
            break;
          }
          /* fallthrough */
        case 2: case 3:
          if(zversion >= 3 || (zversion == 2 && c == 1))
          {
            ZASSERT(!in_abbr, "abbreviation being used recursively");
            abbrev = c;
            shift = 0;
          }
          else
          {
            shift = c - 1;
          }
          break;
        case 4: case 5:
          if(zversion <= 2)
          {
            current_alphabet = (current_alphabet + (c - 3)) % 3;
            shift = 0;
          }
          else
          {
            shift = c - 3;
          }
          break;
        case 6:
          if(zversion <= 2) shift = (current_alphabet + shift) % 3;

          if(shift == 2)
          {
            shift = 0;
            special = 2;
            break;
          }
          /* fallthrough */
        default:
          if(zversion <= 2 && c != 6) shift = (current_alphabet + shift) % 3;

          outc(atable[(26 * shift) + (c - 6)]);
          shift = 0;
          break;
      }
    }

    counter += 2;
  } while((w & 0x8000) == 0);

  return counter - addr;
}

/* Prints the string at addr “addr”.
 *
 * Returns the number of bytes the string took up.  “outc” is passed as
 * the character-print function to print_zcode(); if it is NULL,
 * put_char is used.
 */
int print_handler(uint32_t addr, void (*outc)(uint8_t))
{
  return print_zcode(addr, false, outc != NULL ? outc : put_char);
}

void zprint(void)
{
  pc += print_handler(pc, NULL);
}

void zprint_ret(void)
{
  zprint();
  put_char(ZSCII_NEWLINE);
  zrtrue();
}

void znew_line(void)
{
  put_char(ZSCII_NEWLINE);
}

void zerase_window(void)
{
#ifdef ZTERP_GLK
  switch(as_signed(zargs[0]))
  {
    case -2:
      for(int i = 0; i < 8; i++) clear_window(&windows[i]);
      break;
    case -1:
      close_upper_window();
      /* fallthrough */
    case 0:
      /* 8.7.3.2.1 says V5+ should have the cursor set to 1, 1 of the
       * erased window; V4 the lower window goes bottom left, the upper
       * to 1, 1.  Glk doesn’t give control over the cursor when
       * clearing, and that doesn’t really seem to be an issue; so just
       * call glk_window_clear().
       */
      clear_window(mainwin);
      break;
    case 1:
      clear_window(upperwin);
      break;
    default:
      break;
  }

  /* glk_window_clear() kills reverse video in Gargoyle.  Reapply style. */
  set_current_style();
#endif
}

void zerase_line(void)
{
#ifdef ZTERP_GLK
  /* XXX V6 does pixel handling here. */
  if(zargs[0] != 1 || curwin != upperwin || upperwin->id == NULL) return;

  for(long i = upperwin->x; i < upper_window_width; i++) GLK_PUT_CHAR(UNICODE_SPACE);

  glk_window_move_cursor(upperwin->id, upperwin->x, upperwin->y);
#endif
}

/* XXX This is more complex in V6 and needs to be updated when V6 windowing is implemented. */
static void set_cursor(uint16_t y, uint16_t x)
{
#ifdef ZTERP_GLK
  /* All the windows in V6 can have their cursor positioned; if V6 ever
   * comes about this should be fixed.
   */
  if(curwin != upperwin) return;

  /* -1 and -2 are V6 only, but at least Zracer passes -1 (or it’s
   * trying to position the cursor to line 65535; unlikely!)
   */
  if(as_signed(y) == -1 || as_signed(y) == -2) return;

  /* §8.7.2.3 says 1,1 is the top-left, but at least one program (Paint
   * and Corners) uses @set_cursor 0 0 to go to the top-left; so
   * special-case it.
   */
  if(y == 0) y = 1;
  if(x == 0) x = 1;

  /* This is actually illegal, but some games (e.g. Beyond Zork) expect it to work. */
  if(y > upper_window_height) resize_upper_window(y);

  if(upperwin->id != NULL)
  {
    upperwin->x = x - 1;
    upperwin->y = y - 1;

    glk_window_move_cursor(upperwin->id, x - 1, y - 1);
  }
#endif
}

void zset_cursor(void)
{
  set_cursor(zargs[0], zargs[1]);
}

void zget_cursor(void)
{
#ifdef ZTERP_GLK
  user_store_word(zargs[0] + 0, upperwin->y + 1);
  user_store_word(zargs[0] + 2, upperwin->x + 1);
#else
  user_store_word(zargs[0] + 0, 1);
  user_store_word(zargs[0] + 2, 1);
#endif
}

static bool prepare_color_opcode(int16_t *fg, int16_t *bg, struct window **win)
{
  /* Glk (apart from Gargoyle) has no color support. */
#if !defined(ZTERP_GLK) || defined(GLK_MODULE_GARGLKTEXT)
  if(options.disable_color) return false;

  *fg = as_signed(zargs[0]);
  *bg = as_signed(zargs[1]);
  *win = znargs == 3 ? find_window(zargs[2]) : style_window;

  return true;
#else
  return false;
#endif
}

void zset_colour(void)
{
  int16_t fg, bg;
  struct window *win;

  if(prepare_color_opcode(&fg, &bg, &win))
  {
    /* XXX -1 is a valid color in V6. */
    if(fg >= 1 && fg <= 12) win->fg_color = fg == 1 ? DEFAULT_COLOR() : ANSI_COLOR(fg);
    if(bg >= 1 && bg <= 12) win->bg_color = bg == 1 ? DEFAULT_COLOR() : ANSI_COLOR(bg);

    set_current_style();
  }
}

void zset_true_colour(void)
{
  int16_t fg, bg;
  struct window *win;

  if(prepare_color_opcode(&fg, &bg, &win))
  {
    if     (fg >=  0) win->fg_color = TRUE_COLOR(fg);
    else if(fg == -1) win->fg_color = DEFAULT_COLOR();

    if     (bg >=  0) win->bg_color = TRUE_COLOR(bg);
    else if(bg == -1) win->bg_color = DEFAULT_COLOR();

    set_current_style();
  }
}

/* V6 has per-window styles, but all others have a global style; in this
 * case, track styles via the main window.
 */
void zset_text_style(void)
{
  /* A style of 0 means all others go off. */
  if(zargs[0] == 0) style_window->style = STYLE_NONE;
  else              style_window->style |= zargs[0];

  set_current_style();
}

/* Interpreters seem to disagree on @set_font.  Given the code

   @set_font 4 -> i;
   @set_font 1 -> j;
   @set_font 0 -> k;
   @set_font 1 -> l;

 * the following values are returned:
 * Frotz 2.43:         0, 1, 1, 1
 * Gargoyle r384:      1, 4, 4, 4
 * Fizmo 0.6.5:        1, 4, 1, 0
 * Nitfol 0.5:         1, 4, 0, 1
 * Filfre .987:        1, 4, 0, 1
 * Zoom 1.1.4:         1, 1, 0, 1
 * ZLR 0.07:           0, 1, 0, 1
 * Windows Frotz 1.15: 1, 4, 1, 1
 * XZip 1.8.2:         0, 4, 0, 0
 *
 * The standard says that “ID 0 means ‘the previous font’.” (§8.1.2).
 * The Frotz 2.43 source code says that “zargs[0] = number of font or 0
 * to keep current font”.
 *
 * How to implement @set_font turns on the meaning of “previous”.  Does
 * it mean the previous font _after_ the @set_font call, meaning Frotz
 * is right?  Or is it the previous font _before_ the @set_font call,
 * meaning the identity of two fonts needs to be tracked?
 *
 * Currently I do the latter.  That yields the following:
 * 1, 4, 1, 4
 * Almost comically, no interpreters agree with each other.
 */
void zset_font(void)
{
  struct window *win = curwin;

  if(zversion == 6 && znargs == 2 && as_signed(zargs[1]) != -3)
  {
    ZASSERT(zargs[1] < 8, "invalid window selected: %d", as_signed(zargs[1]));
    win = &windows[zargs[1]];
  }

  /* If no previous font has been stored, consider that an error. */
  if(zargs[0] == FONT_PREVIOUS && win->prev_font != FONT_NONE)
  {
    zargs[0] = win->prev_font;
    zset_font();
  }
  else if(zargs[0] == FONT_NORMAL ||
         (zargs[0] == FONT_CHARACTER && !options.disable_graphics_font) ||
         (zargs[0] == FONT_FIXED     && !options.disable_fixed))
  {
    store(win->font);
    win->prev_font = win->font;
    win->font = zargs[0];
  }
  else
  {
    store(0);
  }

  set_current_style();
}

void zprint_table(void)
{
  uint16_t text = zargs[0], width = zargs[1], height = zargs[2], skip = zargs[3];
  uint16_t n = 0;

#ifdef ZTERP_GLK
  uint16_t start = 0; /* initialize to appease gcc */

  if(curwin == upperwin) start = upperwin->x + 1;
#endif

  if(znargs < 3) height = 1;
  if(znargs < 4) skip = 0;

  for(uint16_t i = 0; i < height; i++)
  {
    for(uint16_t j = 0; j < width; j++)
    {
      put_char(user_byte(text + n++));
    }

    if(i + 1 != height)
    {
      n += skip;
#ifdef ZTERP_GLK
      if(curwin == upperwin)
      {
        set_cursor(upperwin->y + 2, start);
      }
      else
#endif
      {
        put_char(ZSCII_NEWLINE);
      }
    }
  }
}

void zprint_char(void)
{
  /* Check 32 (space) first: a cursory examination of story files
   * indicates that this is the most common value passed to @print_char.
   * This appears to be due to V4+ games blanking the upper window.
   */
#define valid_zscii_output(c)	((c) == 32 || (c) == 0 || (c) == 9 || (c) == 11 || (c) == 13 || ((c) > 32 && (c) <= 126) || ((c) >= 155 && (c) <= 251))
  ZASSERT(valid_zscii_output(zargs[0]), "@print_char called with invalid character: %u", (unsigned)zargs[0]);
#undef valid_zscii_output

  put_char(zargs[0]);
}

void zprint_num(void)
{
  char buf[7];

  snprintf(buf, sizeof buf, "%" PRId16, as_signed(zargs[0]));

  for(size_t i = 0; buf[i] != 0; i++) put_char(buf[i]);
}

void zprint_addr(void)
{
  print_handler(zargs[0], NULL);
}

void zprint_paddr(void)
{
  print_handler(unpack_string(zargs[0]), NULL);
}

/* XXX This is more complex in V6 and needs to be updated when V6 windowing is implemented. */
void zsplit_window(void)
{
  if(zargs[0] == 0) close_upper_window();
  else              resize_upper_window(zargs[0]);
}

void zset_window(void)
{
  set_current_window(find_window(zargs[0]));
}

#ifdef ZTERP_GLK
static void window_change(void)
{
  /* When a textgrid (the upper window) in Gargoyle is rearranged, it
   * forgets about reverse video settings, so reapply any styles to the
   * current window (it doesn’t hurt if the window is a textbuffer).  If
   * the current window is not the upper window that’s OK, because
   * set_current_style() is called when a @set_window is requested.
   */
  set_current_style();

  /* If the new window is smaller, the cursor of the upper window might
   * be out of bounds.  Pull it back in if so.
   */
  if(zversion >= 3 && upperwin->id != NULL)
  {
    glui32 w, h;

    glk_window_get_size(upperwin->id, &w, &h);

    upper_window_width = w;

    /* Force a redraw (do not wait for user input). */
    saw_input = true;
    resize_upper_window(h);
  }

  /* §8.4
   * Only 0x20 and 0x21 are mentioned; what of 0x22 and 0x24?  Zoom and
   * Windows Frotz both update the V5 header entries, so do that here,
   * too.
   *
   * Also, no version restrictions are given, but assume V4+ per §11.1.
   */
  if(zversion >= 4)
  {
    unsigned width, height;

    get_screen_size(&width, &height);

    store_byte(0x20, height > 254 ? 254 : height);
    store_byte(0x21, width > 255 ? 255 : width);

    if(zversion >= 5)
    {
      store_word(0x22, width > UINT16_MAX ? UINT16_MAX : width);
      store_word(0x24, height > UINT16_MAX ? UINT16_MAX : height);
    }
  }
  else
  {
    zshow_status();
  }
}
#endif

#ifdef ZTERP_GLK
static bool timer_running;

static void start_timer(uint16_t n)
{
  if(!timer_available()) return;

  if(timer_running) die("nested timers unsupported");
  glk_request_timer_events(n * 100);
  timer_running = true;
}

static void stop_timer(void)
{
  if(!timer_available()) return;

  glk_request_timer_events(0);
  timer_running = false;
}

static void request_char(void)
{
  if(have_unicode) glk_request_char_event_uni(curwin->id);
  else             glk_request_char_event(curwin->id);

  curwin->pending_read = true;
}

static void request_line(union line *line, glui32 maxlen, glui32 initlen)
{
  if(have_unicode) glk_request_line_event_uni(curwin->id, line->unicode, maxlen, initlen);
  else             glk_request_line_event(curwin->id, line->latin1, maxlen, initlen);

  curwin->pending_read = true;
  curwin->line = line;
}
#endif

#define special_zscii(c) ((c) >= 129 && (c) <= 154)

/* This is called when input stream 1 (read from file) is selected.  If
 * it succefully reads a character/line from the file, it fills the
 * struct at “input” with the appropriate information and returns true.
 * If it fails to read (likely due to EOF) then it sets the input stream
 * back to the keyboard and returns false.
 */
static bool istream_read_from_file(struct input *input)
{
  if(input->type == INPUT_CHAR)
  {
    long c;

    /* If there are carriage returns in the input, this is almost
     * certainly a command script from a Windows system being run on a
     * non-Windows system, so ignore them.
     */
    do
    {
      c = zterp_io_getc(istreamio);
    } while(c == UNICODE_CARRIAGE_RETURN);

    if(c == -1)
    {
      input_stream(ISTREAM_KEYBOARD);
      return false;
    }

    /* Don’t translate special ZSCII characters (cursor keys, function keys, keypad). */
    if(special_zscii(c)) input->key = c;
    else                 input->key = unicode_to_zscii_q[c];
  }
  else
  {
    long n;
    uint16_t line[1024];

    n = zterp_io_readline(istreamio, line, sizeof line / sizeof *line);
    if(n == -1)
    {
      input_stream(ISTREAM_KEYBOARD);
      return false;
    }

    /* As above, ignore carriage returns. */
    if(n > 0 && line[n - 1] == UNICODE_CARRIAGE_RETURN) n--;

    if(n > input->maxlen) n = input->maxlen;

    input->len = n;

#ifdef ZTERP_GLK
    if(curwin->id != NULL)
    {
      glk_set_style(style_Input);
      for(long i = 0; i < n; i++) GLK_PUT_CHAR(line[i]);
      GLK_PUT_CHAR(UNICODE_LINEFEED);
      set_current_style();
    }
#else
    for(long i = 0; i < n; i++) zterp_io_putc(zterp_io_stdout(), line[i]);
    zterp_io_putc(zterp_io_stdout(), UNICODE_LINEFEED);
#endif

    for(long i = 0; i < n; i++) input->line[i] = line[i];
  }

#ifdef ZTERP_GLK
  event_t ev;

  /* It’s possible that output is buffered, meaning that until
   * glk_select() is called, output will not be displayed.  When reading
   * from a command-script, flush on each command so that output is
   * visible while the script is being replayed.
   */
  glk_select_poll(&ev);
  switch(ev.type)
  {
    case evtype_None:
      break;
    case evtype_Arrange:
      window_change();
      break;
    default:
      /* No other events should arrive.  Timers are only started in
       * get_input() and are stopped before that function returns.
       * Input events will not happen with glk_select_poll(), and no
       * other event type is expected to be raised.
       */
      break;
  }

  saw_input = true;
#endif

  return true;
}

#ifdef GLK_MODULE_LINE_TERMINATORS
/* Glk returns terminating characters as keycode_*, but we need them as
 * ZSCII.  This should only ever be called with values that are matched
 * in the switch, because those are the only ones that Glk was told are
 * terminating characters.  In the event that another keycode comes
 * through, though, treat it as Enter.
 */
static uint8_t zscii_from_glk(glui32 key)
{
  switch(key)
  {
    case 13:             return ZSCII_NEWLINE;
    case keycode_Up:     return ZSCII_UP;
    case keycode_Down:   return ZSCII_DOWN;
    case keycode_Left:   return ZSCII_LEFT;
    case keycode_Right:  return ZSCII_RIGHT;
    case keycode_Func1:  return ZSCII_F1;
    case keycode_Func2:  return ZSCII_F2;
    case keycode_Func3:  return ZSCII_F3;
    case keycode_Func4:  return ZSCII_F4;
    case keycode_Func5:  return ZSCII_F5;
    case keycode_Func6:  return ZSCII_F6;
    case keycode_Func7:  return ZSCII_F7;
    case keycode_Func8:  return ZSCII_F8;
    case keycode_Func9:  return ZSCII_F9;
    case keycode_Func10: return ZSCII_F10;
    case keycode_Func11: return ZSCII_F11;
    case keycode_Func12: return ZSCII_F12;
  }

  return ZSCII_NEWLINE;
}
#endif

#ifdef ZTERP_GLK
/* This is like strlen() but in addition to C strings it can find the
 * length of a Unicode string (which is assumed to be zero terminated)
 * if Unicode is being used.
 */
static size_t line_len(const union line *line)
{
  size_t i;

  if(!have_unicode) return strlen(line->latin1);

  for(i = 0; line->unicode[i] != 0; i++)
  {
  }

  return i;
}
#endif

/* Attempt to read input from the user.  The input type can be either a
 * single character or a full line.  If “timer” is not zero, a timer is
 * started that fires off every “timer” tenths of a second (if the value
 * is 1, it will timeout 10 times a second, etc.).  Each time the timer
 * times out the routine at address “routine” is called.  If the routine
 * returns true, the input is canceled.
 *
 * The function returns true if input was stored, false if there was a
 * cancellation as described above.
 */
static bool get_input(uint16_t timer, uint16_t routine, struct input *input)
{
  /* If either of these is zero, no timeout should happen. */
  if(timer   == 0) routine = 0;
  if(routine == 0) timer   = 0;

  /* Flush all streams when input is requested. */
#ifndef ZTERP_GLK
  zterp_io_flush(zterp_io_stdout());
#endif
  zterp_io_flush(scriptio);
  zterp_io_flush(transio);

  /* Generally speaking, newline will be the reason the line input
   * stopped, so set it by default.  It will be overridden where
   * necessary.
   */
  input->term = ZSCII_NEWLINE;

  if(istream == ISTREAM_FILE && istream_read_from_file(input)) return true;
#ifdef ZTERP_GLK
  enum { InputWaiting, InputReceived, InputCanceled } status = InputWaiting;
  union line line;
  struct window *saved = NULL;

  /* In V6, input might be requested on an unsupported window.  If so,
   * switch to the main window temporarily.
   */
  if(curwin->id == NULL)
  {
    saved = curwin;
    curwin = mainwin;
    glk_set_window(curwin->id);
  }

  if(input->type == INPUT_CHAR)
  {
    request_char();
  }
  else
  {
    for(int i = 0; i < input->preloaded; i++)
    {
      if(have_unicode) line.unicode[i] = input->line[i];
      else             line.latin1 [i] = input->line[i];
    }

    request_line(&line, input->maxlen, input->preloaded);
  }

  if(timer != 0) start_timer(timer);

  while(status == InputWaiting)
  {
    event_t ev;

    glk_select(&ev);

    switch(ev.type)
    {
      case evtype_Arrange:
        window_change();
        break;

      case evtype_Timer:
        {
          ZASSERT(timer != 0, "got unexpected evtype_Timer");

          struct window *saved2 = curwin;
          int ret;

          stop_timer();

          ret = direct_call(routine);

          /* It’s possible for an interrupt to switch windows; if it
           * does, simply switch back.  This is the easiest way to deal
           * with an undefined bit of the Z-machine.
           */
          if(curwin != saved2) set_current_window(saved2);

          if(ret != 0)
          {
            status = InputCanceled;
          }
          else
          {
            /* If this got reset to false, that means an interrupt had to
             * cancel the read event in order to either read or write.
             */
            if(!curwin->pending_read)
            {
              if(input->type == INPUT_CHAR) request_char();
              else                          request_line(&line, input->maxlen, line_len(&line));
            }

            start_timer(timer);
          }
        }

        break;

      case evtype_CharInput:
        ZASSERT(input->type == INPUT_CHAR, "got unexpected evtype_CharInput");
        ZASSERT(ev.win == curwin->id, "got evtype_CharInput on unexpected window");

        status = InputReceived;

        switch(ev.val1)
        {
          case keycode_Delete: input->key = ZSCII_DELETE; break;
          case keycode_Return: input->key = ZSCII_NEWLINE; break;
          case keycode_Escape: input->key = ZSCII_ESCAPE; break;
          case keycode_Up:     input->key = ZSCII_UP; break;
          case keycode_Down:   input->key = ZSCII_DOWN; break;
          case keycode_Left:   input->key = ZSCII_LEFT; break;
          case keycode_Right:  input->key = ZSCII_RIGHT; break;
          case keycode_Func1:  input->key = ZSCII_F1; break;
          case keycode_Func2:  input->key = ZSCII_F2; break;
          case keycode_Func3:  input->key = ZSCII_F3; break;
          case keycode_Func4:  input->key = ZSCII_F4; break;
          case keycode_Func5:  input->key = ZSCII_F5; break;
          case keycode_Func6:  input->key = ZSCII_F6; break;
          case keycode_Func7:  input->key = ZSCII_F7; break;
          case keycode_Func8:  input->key = ZSCII_F8; break;
          case keycode_Func9:  input->key = ZSCII_F9; break;
          case keycode_Func10: input->key = ZSCII_F10; break;
          case keycode_Func11: input->key = ZSCII_F11; break;
          case keycode_Func12: input->key = ZSCII_F12; break;

          default:
            input->key = ZSCII_QUESTIONMARK;

            if(ev.val1 <= UINT16_MAX)
            {
              uint8_t c = unicode_to_zscii[ev.val1];

              if(c != 0) input->key = c;
            }

            break;
        }

        break;

      case evtype_LineInput:
        ZASSERT(input->type == INPUT_LINE, "got unexpected evtype_LineInput");
        ZASSERT(ev.win == curwin->id, "got evtype_LineInput on unexpected window");
        input->len = ev.val1;
#ifdef GLK_MODULE_LINE_TERMINATORS
        if(zversion >= 5) input->term = zscii_from_glk(ev.val2);
#endif
        status = InputReceived;
        break;
    }
  }

  stop_timer();

  if(input->type == INPUT_CHAR)
  {
    glk_cancel_char_event(curwin->id);
  }
  else
  {
    /* On cancellation, the buffer still needs to be filled, because
     * it’s possible that line input echoing has been turned off and the
     * contents will need to be written out.
     */
    if(status == InputCanceled)
    {
      event_t ev;

      glk_cancel_line_event(curwin->id, &ev);
      input->len = ev.val1;
      input->term = 0;
    }

    for(glui32 i = 0; i < input->len; i++)
    {
      if(have_unicode) input->line[i] = line.unicode[i] > UINT16_MAX ? UNICODE_REPLACEMENT : line.unicode[i];
      else             input->line[i] = (uint8_t)line.latin1[i];
    }
  }

  curwin->pending_read = false;
  curwin->line = NULL;

  if(status == InputReceived) saw_input = true;

  if(errorwin != NULL)
  {
    glk_window_close(errorwin, NULL);
    errorwin = NULL;
  }

  if(saved != NULL)
  {
    curwin = saved;
    glk_set_window(curwin->id);
  }

  return status != InputCanceled;
#else
  if(input->type == INPUT_CHAR)
  {
    long n;
    uint16_t line[64];

    n = zterp_io_readline(zterp_io_stdin(), line, sizeof line / sizeof *line);

    if(n == -1)
    {
      zquit();
    }
    else if(n == 0)
    {
      input->key = ZSCII_NEWLINE;
    }
    /* Delete and escape are defined for input, so if they're seen,
     * handle them manually.
     */
    else if(line[0] == UNICODE_DELETE)
    {
      input->key = ZSCII_DELETE;
    }
    else if(line[0] == UNICODE_ESCAPE)
    {
      input->key = ZSCII_ESCAPE;
    }
    else
    {
      input->key = unicode_to_zscii[line[0]];
      if(input->key == 0) input->key = ZSCII_NEWLINE;
    }
  }
  else
  {
    input->len = input->preloaded;

    if(input->maxlen > input->preloaded)
    {
      long n;
      uint16_t line[1024];

      n = zterp_io_readline(zterp_io_stdin(), line, sizeof line / sizeof *line);
      if(n == -1)
      {
        zquit();
      }
      else
      {
        if(n > input->maxlen - input->preloaded) n = input->maxlen - input->preloaded;
        for(long i = 0; i < n; i++) input->line[i + input->preloaded] = line[i];
        input->len += n;
      }
    }
  }

  return true;
#endif
}

void zread_char(void)
{
  uint16_t timer = 0;
  uint16_t routine = zargs[2];
  struct input input = { .type = INPUT_CHAR };

#ifdef ZTERP_GLK
  cancel_read_events(curwin);
#endif

  if(zversion >= 4 && znargs > 1) timer = zargs[1];

  if(!get_input(timer, routine, &input))
  {
    store(0);
    return;
  }

#ifdef ZTERP_GLK
  update_delayed();
#endif

  if(streams & STREAM_SCRIPT)
  {
    /* Values 127 to 159 are not valid Unicode, and these just happen to
     * match up to the values needed for special ZSCII keys, so store
     * them as-is.
     */
    if(special_zscii(input.key)) zterp_io_putc(scriptio, input.key);
    else                         zterp_io_putc(scriptio, zscii_to_unicode[input.key]);
  }

  store(input.key);
}
#undef special_zscii

#ifdef ZTERP_GLK
static void status_putc(uint8_t c)
{
  glk_put_char(zscii_to_unicode[c]);
}
#endif

void zshow_status(void)
{
#ifdef ZTERP_GLK
  glui32 width, height;
  char rhs[64];
  int first = variable(0x11), second = variable(0x12);

  if(statuswin.id == NULL) return;

  glk_window_clear(statuswin.id);

  SWITCH_WINDOW_START(&statuswin);

  glk_window_get_size(statuswin.id, &width, &height);

#ifdef GLK_MODULE_GARGLKTEXT
  garglk_set_reversevideo(1);
#else
  glk_set_style(style_Alert);
#endif
  for(glui32 i = 0; i < width; i++) glk_put_char(ZSCII_SPACE);

  glk_window_move_cursor(statuswin.id, 1, 0);

  /* Variable 0x10 is global variable 1. */
  print_object(variable(0x10), status_putc);

  if(status_is_time())
  {
    snprintf(rhs, sizeof rhs, "Time: %d:%02d%s ", (first + 11) % 12 + 1, second, first < 12 ? "am" : "pm");
    if(strlen(rhs) > width) snprintf(rhs, sizeof rhs, "%02d:%02d", first, second);
  }
  else
  {
    snprintf(rhs, sizeof rhs, "Score: %d  Moves: %d ", first, second);
    if(strlen(rhs) > width) snprintf(rhs, sizeof rhs, "%d/%d", first, second);
  }

  if(strlen(rhs) <= width)
  {
    glk_window_move_cursor(statuswin.id, width - strlen(rhs), 0);
    glk_put_string(rhs);
  }

  SWITCH_WINDOW_END();
#endif
}

/* Attempt to read and parse a line of input.  On success, return true.
 * Otherwise, return false to indicate that input should be requested
 * again.
 */
static bool read_handler(void)
{
  uint16_t text = zargs[0], parse = zargs[1];
  uint8_t maxchars = zversion >= 5 ? user_byte(text) : user_byte(text) - 1;
  uint8_t zscii_string[maxchars];
  uint32_t string[maxchars + 1];
  struct input input = { .type = INPUT_LINE, .line = string, .maxlen = maxchars };
  uint16_t timer = 0;
  uint16_t routine = zargs[3];

#ifdef ZTERP_GLK
  cancel_read_events(curwin);
#endif

  if(zversion <= 3) zshow_status();

  if(zversion >= 4 && znargs > 2) timer = zargs[2];

  if(zversion >= 5)
  {
    input.preloaded = user_byte(text + 1);
    ZASSERT(input.preloaded <= maxchars, "too many preloaded characters: %d when max is %d", input.preloaded, maxchars);

#ifdef GARGLK
    for(int i = 0; i < input.preloaded; i++)
    {
      uint16_t c = zscii_to_unicode[user_byte(text + i + 2)];

      /* When using preloaded input (e.g. when the user hits F4), Beyond
       * Zork displays the faked input text in uppercase, but preloads
       * in lowercase. garglk_unput_string_uni() does a case-sensitive
       * comparison, so fails to erase the printed text, resulting in
       * something like “EXAMINEexamine” displayed to the user, with the
       * lowercase “examine” being editable.
       *
       * When the game is Beyond Zork, convert preloaded input to
       * uppercase, “knowing” that any preloaded characters will be
       * valid to pass to toupper(): Bocfel requires ASCII (or a
       * compatible character set), so this assumption is fine.
       */
      if(is_beyond_zork() && c < 256) string[i] = toupper(c);
      else                            string[i] = c;
    }

    string[input.preloaded] = 0;
    if(curwin->id != NULL) garglk_unput_string_uni(string);
#else
    /* Under garglk, preloaded input works as it’s supposed to.
     * Under Glk, it can fail one of two ways:
     * 1. The preloaded text is printed out once, but is not editable.
     * 2. The preloaded text is printed out twice, the second being editable.
     * I have chosen option #2.  For non-Glk, option #1 is done by necessity.
     */
    for(int i = 0; i < input.preloaded; i++) string[i] = zscii_to_unicode[user_byte(text + i + 2)];
#endif
  }

  if(!get_input(timer, routine, &input))
  {
#ifdef ZTERP_GLK
    cleanup_screen(&input);
#endif
    if(zversion >= 5) store(0);
    return true;
  }

#ifdef ZTERP_GLK
  cleanup_screen(&input);
#endif

#ifdef ZTERP_GLK
  update_delayed();
#endif

  if(!options.disable_meta_commands)
  {
    string[input.len] = 0;

    if(string[0] == '/')
    {
      const uint32_t *ret;

      ret = handle_meta_command(string + 1, input.len - 1);
      if(ret == NULL)
      {
        return true;
      }
      else if(ret == string + 1)
      {
        /* The game still wants input, so try again. */
        screen_print("\n>");
        return false;
      }
      else
      {
        ptrdiff_t offset = ret - string;

        input.len -= offset;
        memmove(string, ret, (sizeof string[0]) * (input.len + 1));
      }
    }

    /* V1–4 do not have @save_undo, so simulate one each time @read is
     * called.
     *
     * Although V5 introduced @save_undo, not all games make use of it
     * (e.g. Hitchhiker’s Guide).  Until @save_undo is called, simulate
     * it each @read, just like in V1–4.  If @save_undo is called, all
     * of these interpreter-generated save states are forgotten and the
     * game’s calls to @save_undo take over.
     *
     * Because V1–4 games will never call @save_undo, seen_save_undo
     * will never be true.  Thus there is no need to test zversion.
     *
     * pc is currently set to the next instruction, but the undo needs
     * to come back to *this* instruction; so use current_instruction
     * instead of pc.
     */
    if(!seen_save_undo) push_save(SAVE_GAME, current_instruction, NULL);
  }

  if(options.enable_escape && (streams & STREAM_TRANS))
  {
    zterp_io_putc(transio, 033);
    zterp_io_putc(transio, '[');
    for(int i = 0; options.escape_string[i] != 0; i++) zterp_io_putc(transio, options.escape_string[i]);
  }

  for(int i = 0; i < input.len; i++)
  {
    zscii_string[i] = unicode_to_zscii_q[unicode_tolower(string[i])];
    if(streams & STREAM_TRANS)  zterp_io_putc(transio, string[i]);
    if(streams & STREAM_SCRIPT) zterp_io_putc(scriptio, string[i]);
  }

  if(options.enable_escape && (streams & STREAM_TRANS))
  {
    zterp_io_putc(transio, 033);
    zterp_io_putc(transio, '[');
    zterp_io_putc(transio, '0');
    zterp_io_putc(transio, 'm');
  }

  if(streams & STREAM_TRANS)  zterp_io_putc(transio, UNICODE_LINEFEED);
  if(streams & STREAM_SCRIPT) zterp_io_putc(scriptio, UNICODE_LINEFEED);

  if(zversion >= 5)
  {
    user_store_byte(text + 1, input.len); /* number of characters read */

    for(int i = 0; i < input.len; i++)
    {
      user_store_byte(text + i + 2, zscii_string[i]);
    }

    if(parse != 0) tokenize(text, parse, 0, false);

    store(input.term);
  }
  else
  {
    for(int i = 0; i < input.len; i++)
    {
      user_store_byte(text + i + 1, zscii_string[i]);
    }

    user_store_byte(text + input.len + 1, 0);

    tokenize(text, parse, 0, false);
  }

  return true;
}

void zread(void)
{
  while(!read_handler())
  {
  }
}

void zprint_unicode(void)
{
  if(valid_unicode(zargs[0])) put_char_u(zargs[0]);
  else                        put_char_u(UNICODE_REPLACEMENT);
}

void zcheck_unicode(void)
{
  uint16_t res = 0;

  /* valid_unicode() will tell which Unicode characters can be printed;
   * and if the Unicode character is in the Unicode input table, it can
   * also be read.  If Unicode is not available, then any character >255
   * is invalid for both reading and writing.
   */
  if(have_unicode || zargs[0] < 256)
  {
    /* §3.8.5.4.5: “Unicode characters U+0000 to U+001F and U+007F to
     * U+009F are control codes, and must not be used.”
     *
     * Even though control characters can be read (e.g. delete and
     * linefeed), when they are looked at through a Unicode lens, they
     * should be considered invalid.  I don’t know if this is the right
     * approach, but nobody seems to use @check_unicode, so it’s not
     * especially important.  One implication of this is that it’s
     * impossible for this implementation of @check_unicode to return 2,
     * since a character must be valid for output before it’s even
     * checked for input, and all printable characters are also
     * readable.
     *
     * For what it’s worth, interpreters seem to disagree on this pretty
     * drastically:
     *
     * • Zoom 1.1.5 returns 1 for all control characters.
     * • Fizmo 0.7.8 returns 3 for characters 8, 10, 13, and 27, 1 for
     *   all other control characters.
     * • Frotz 2.44 and Nitfol 0.5 return 0 for all control characters.
     * • Filfre 1.1.1 returns 3 for all control characters.
     * • Windows Frotz 1.19 returns 2 for characters 8, 13, and 27, 0
     *   for other control characters in the range 00 to 1f.  It returns
     *   a mixture of 2 and 3 for control characters in the range 7F to
     *   9F based on whether the specified glyph is available.
     */
    if(valid_unicode(zargs[0]))
    {
      res |= 0x01;
      if(unicode_to_zscii[zargs[0]] != 0)
      {
        res |= 0x02;
      }
    }
  }

#ifdef ZTERP_GLK
  if(glk_gestalt(gestalt_CharOutput, zargs[0]) == gestalt_CharOutput_CannotPrint) res &= ~(uint16_t)0x01;
  if(!glk_gestalt(gestalt_CharInput, zargs[0])) res &= ~(uint16_t)0x02;
#endif

  store(res);
}

/* Should picture_data and get_wind_prop be moved to a V6 source file? */
void zpicture_data(void)
{
  if(zargs[0] == 0)
  {
    user_store_word(zargs[1] + 0, 0);
    user_store_word(zargs[1] + 2, 0);
  }

  /* No pictures means no valid pictures, so never branch. */
  branch_if(false);
}

void zget_wind_prop(void)
{
  uint16_t val;
  struct window *win;

  win = find_window(zargs[0]);

  /* These are mostly bald-faced lies. */
  switch(zargs[1])
  {
    case 0: /* y coordinate */
      val = 0;
      break;
    case 1: /* x coordinate */
      val = 0;
      break;
    case 2:  /* y size */
      val = 100;
      break;
    case 3:  /* x size */
      val = 100;
      break;
    case 4:  /* y cursor */
      val = 0;
      break;
    case 5:  /* x cursor */
      val = 0;
      break;
    case 6: /* left margin size */
      val = 0;
      break;
    case 7: /* right margin size */
      val = 0;
      break;
    case 8: /* newline interrupt routine */
      val = 0;
      break;
    case 9: /* interrupt countdown */
      val = 0;
      break;
    case 10: /* text style */
      val = win->style;
      break;
    case 11: /* colour data */
      val = (9 << 8) | 2;
      break;
    case 12: /* font number */
      val = win->font;
      break;
    case 13: /* font size */
      val = (10 << 8) | 10;
      break;
    case 14: /* attributes */
      val = 0;
      break;
    case 15: /* line count */
      val = 0;
      break;
    case 16: /* true foreground colour */
      val = 0;
      break;
    case 17: /* true background colour */
      val = 0;
      break;
    default:
      die("unknown window property: %u", (unsigned)zargs[1]);
  }

  store(val);
}

/* This is not correct, because @output_stream does not work as it
 * should with a width argument; however, this does print out the
 * contents of a table that was sent to stream 3, so it’s at least
 * somewhat useful.
 *
 * Output should be to the currently-selected window, but since V6 is
 * only marginally supported, other windows are not active.  Send to the
 * main window for the time being.
 */
void zprint_form(void)
{
  SWITCH_WINDOW_START(mainwin);

  for(uint16_t i = 0; i < user_word(zargs[0]); i++)
  {
    put_char(user_byte(zargs[0] + 2 + i));
  }

  put_char(ZSCII_NEWLINE);

  SWITCH_WINDOW_END();
}

void zmake_menu(void)
{
  branch_if(false);
}

void zbuffer_screen(void)
{
  store(0);
}

#ifdef GLK_MODULE_GARGLKTEXT
/* Glk does not guarantee great control over how various styles are
 * going to look, but Gargoyle does.  Abusing the Glk “style hints”
 * functions allows for quite fine-grained control over style
 * appearance.  First, clear the (important) attributes for each style,
 * and then recreate each in whatever mold is necessary.  Re-use some
 * that are expected to be correct (emphasized for italic, subheader for
 * bold, and so on).
 */
static void set_default_styles(void)
{
  int styles[] = { style_Subheader, style_Emphasized, style_Alert, style_Preformatted, style_User1, style_User2, style_Note };

  for(int i = 0; i < 7; i++)
  {
    glk_stylehint_set(wintype_AllTypes, styles[i], stylehint_Size, 0);
    glk_stylehint_set(wintype_AllTypes, styles[i], stylehint_Weight, 0);
    glk_stylehint_set(wintype_AllTypes, styles[i], stylehint_Oblique, 0);

    /* This sets wintype_TextGrid to be proportional, which of course is
     * wrong; but text grids are required to be fixed, so Gargoyle
     * simply ignores this hint for those windows.
     */
    glk_stylehint_set(wintype_AllTypes, styles[i], stylehint_Proportional, 1);
  }
}
#endif

bool create_mainwin(void)
{
#ifdef ZTERP_GLK

#ifdef GLK_MODULE_UNICODE
  have_unicode = glk_gestalt(gestalt_Unicode, 0);
#endif

#ifdef GLK_MODULE_GARGLKTEXT
  set_default_styles();

  /* Bold */
  glk_stylehint_set(wintype_AllTypes, style_Subheader, stylehint_Weight, 1);

  /* Italic */
  glk_stylehint_set(wintype_AllTypes, style_Emphasized, stylehint_Oblique, 1);

  /* Bold Italic */
  glk_stylehint_set(wintype_AllTypes, style_Alert, stylehint_Weight, 1);
  glk_stylehint_set(wintype_AllTypes, style_Alert, stylehint_Oblique, 1);

  /* Fixed */
  glk_stylehint_set(wintype_AllTypes, style_Preformatted, stylehint_Proportional, 0);

  /* Bold Fixed */
  glk_stylehint_set(wintype_AllTypes, style_User1, stylehint_Weight, 1);
  glk_stylehint_set(wintype_AllTypes, style_User1, stylehint_Proportional, 0);

  /* Italic Fixed */
  glk_stylehint_set(wintype_AllTypes, style_User2, stylehint_Oblique, 1);
  glk_stylehint_set(wintype_AllTypes, style_User2, stylehint_Proportional, 0);

  /* Bold Italic Fixed */
  glk_stylehint_set(wintype_AllTypes, style_Note, stylehint_Weight, 1);
  glk_stylehint_set(wintype_AllTypes, style_Note, stylehint_Oblique, 1);
  glk_stylehint_set(wintype_AllTypes, style_Note, stylehint_Proportional, 0);
#endif

  mainwin->id = glk_window_open(0, 0, 0, wintype_TextBuffer, 1);
  if(mainwin->id == NULL) return false;
  glk_set_window(mainwin->id);

#ifdef GLK_MODULE_LINE_ECHO
  mainwin->has_echo = glk_gestalt(gestalt_LineInputEcho, 0);
  if(mainwin->has_echo) glk_set_echo_line_event(mainwin->id, 0);
#endif

  return true;
#else
  return true;
#endif
}

bool create_statuswin(void)
{
#ifdef ZTERP_GLK
  if(statuswin.id == NULL) statuswin.id = glk_window_open(mainwin->id, winmethod_Above | winmethod_Fixed, 1, wintype_TextGrid, 0);
  return statuswin.id != NULL;
#else
  return false;
#endif
}

bool create_upperwin(void)
{
#ifdef ZTERP_GLK
  /* On a restart, this function will get called again.  It would be
   * possible to try to resize the upper window to 0 if it already
   * exists, but it’s easier to just destroy and recreate it.
   */
  if(upperwin->id != NULL) glk_window_close(upperwin->id, NULL);

  /* The upper window appeared in V3. */
  if(zversion >= 3)
  {
    upperwin->id = glk_window_open(mainwin->id, winmethod_Above | winmethod_Fixed, 0, wintype_TextGrid, 0);
    upperwin->x = upperwin->y = 0;
    upper_window_height = 0;

    if(upperwin->id != NULL)
    {
      glui32 w, h;

      glk_window_get_size(upperwin->id, &w, &h);
      upper_window_width = w;

      if(h != 0 || upper_window_width == 0)
      {
        glk_window_close(upperwin->id, NULL);
        upperwin->id = NULL;
      }
    }
  }

  return upperwin->id != NULL;
#else
  return false;
#endif
}

void init_screen(void)
{
  for(int i = 0; i < 8; i++)
  {
    windows[i].style = STYLE_NONE;
    windows[i].fg_color = windows[i].bg_color = DEFAULT_COLOR();
    windows[i].font = FONT_NORMAL;
    windows[i].prev_font = FONT_NONE;

#ifdef ZTERP_GLK
    clear_window(&windows[i]);
#ifdef GLK_MODULE_LINE_TERMINATORS
    if(windows[i].id != NULL && term_nkeys != 0 && glk_gestalt(gestalt_LineTerminators, 0)) glk_set_terminators_line_event(windows[i].id, term_keys, term_nkeys);
#endif
#endif
  }

  close_upper_window();

#ifdef ZTERP_GLK
  if(statuswin.id != NULL) glk_window_clear(statuswin.id);

  if(errorwin != NULL)
  {
    glk_window_close(errorwin, NULL);
    errorwin = NULL;
  }

  stop_timer();

#else
  have_unicode = true;
#endif

  if(scriptio != NULL) zterp_io_close(scriptio);
  scriptio = NULL;

  input_stream(ISTREAM_KEYBOARD);

  streams = STREAM_SCREEN;
  stream_tables_index = -1;
  set_current_window(mainwin);
}
