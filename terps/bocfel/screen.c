/*-
 * Copyright 2010-2011 Chris Spiegel.
 *
 * This file is part of Bocfel.
 *
 * Bocfel is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version
 * 2, as published by the Free Software Foundation.
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
#include <string.h>
#include <errno.h>
#include <stddef.h>
#include <stdarg.h>

#ifdef ZTERP_GLK
#include <glk.h>
#endif

#include "screen.h"
#include "branch.h"
#include "dict.h"
#include "io.h"
#include "memory.h"
#include "objects.h"
#include "osdep.h"
#include "process.h"
#include "stack.h"
#include "unicode.h"
#include "util.h"
#include "zterp.h"

/* Windows. */
#define FONT_NONE	-1
#define FONT_PREVIOUS	0
#define FONT_NORMAL	1
#define FONT_PICTURE	2
#define FONT_CHARACTER	3
#define FONT_FIXED	4

static struct window
{
  unsigned style;
  
  int font;
  int prev_font;

  long x, y; /* Only meaningful for window 1 */
#ifdef ZTERP_GLK
  winid_t id;
  int pending_read;
  void *line;
#endif
} windows[8], *mainwin = &windows[0], *curwin = &windows[0];
#ifdef ZTERP_GLK
static struct window *upperwin = &windows[1];
static struct window statuswin;
static long upper_window_height = 0;
static long upper_window_width = 0;
static winid_t errorwin;
#endif

/* In all versions but 6, styles are global and stored in mainwin.  For
 * V6, styles are tracked per window and thus stored in each individual
 * window.  For convenience, this macro expands to the “style window”
 * for any version.
 */
#define style_window	(zversion == 6 ? curwin : mainwin)

/* If the window needs to be temporarily switched (@show_status and
 * @print_form print to specific windows, and window_change() might
 * temporarily need to switch to the upper window), the code that
 * requires a specific window can be wrapped in these macros.
 */
#ifdef ZTERP_GLK
#define SWITCH_WINDOW_START(win)	{ struct window *saved_ = curwin; curwin = (win); glk_set_window((win)->id);
#define SWITCH_WINDOW_END()		curwin = saved_; if(curwin->id == NULL) glk_stream_set_current(NULL); else glk_set_window(curwin->id); }
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

static struct
{
  uint16_t table;
  uint16_t i;
} stables[16];
static int stablei = -1;

static int istream = ISTREAM_KEYBOARD;
static zterp_io *istreamio;

/* This macro makes it so that code elsewhere needn’t check have_unicode before printing. */
#define GLK_PUT_CHAR(c)		do { if(!have_unicode) glk_put_char(unicode_to_latin1[c]); else glk_put_char_uni(c); } while(0)

void output_stream(int16_t number, uint16_t table)
{
  if(number > 0)
  {
    streams |= 1U << number;
  }
  else if(number < 0)
  {
    if(number != -3 || stablei == 0) streams &= ~(1U << -number);
  }

  if(number == 2)
  {
    STORE_WORD(0x10, WORD(0x10) | FLAGS2_TRANSCRIPT);
    if(transio == NULL)
    {
      transio = zterp_io_open(options.transcript_name, ZTERP_IO_TRANS | (options.overwrite_transcript ? ZTERP_IO_WRONLY : ZTERP_IO_APPEND));
      if(transio == NULL)
      {
        STORE_WORD(0x10, WORD(0x10) & ~FLAGS2_TRANSCRIPT);
        streams &= ~STREAM_TRANS;
        warning("unable to open the transcript");
      }
    }
  }
  else if(number == -2)
  {
    STORE_WORD(0x10, WORD(0x10) & ~FLAGS2_TRANSCRIPT);
  }

  if(number == 3)
  {
    stablei++;
    ZASSERT(stablei < 16, "too many stream tables");

    stables[stablei].table = table;
    user_store_word(stables[stablei].table, 0);
    stables[stablei].i = 2;
  }
  else if(number == -3 && stablei >= 0)
  {
    user_store_word(stables[stablei].table, stables[stablei].i - 2);
    stablei--;
  }

  if(number == 4)
  {
    if(scriptio == NULL)
    {
      scriptio = zterp_io_open(options.script_name, ZTERP_IO_WRONLY | ZTERP_IO_INPUT);
      if(scriptio == NULL)
      {
        streams &= ~STREAM_SCRIPT;
        warning("unable to open the script");
      }
    }
  }
  /* XXX v6 has even more handling */
}

void zoutput_stream(void)
{
  output_stream(zargs[0], zargs[1]);
}

void input_stream(int which)
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
      istreamio = zterp_io_open(options.replay_name, ZTERP_IO_INPUT | ZTERP_IO_RDONLY);
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

  /* Sink all output to unavailable windows.  The upper
   * window might be unavailable, and windows 2–8 in V6
   * are definitely unavailable.
   */
  if(curwin->id == NULL) glk_stream_set_current(NULL);
  else                   glk_set_window(curwin->id);
#endif

  set_current_style();
}

/* Find and validate a window.  If window is -3 and the story is V6,
 * return the current window.
 */
static struct window *find_window(uint16_t window)
{
  int16_t w = window;

  ZASSERT(zversion == 6 ? w == -3 || (w >= 0 && w < 8) : w == 0 || w == 1, "invalid window selected: %d", w);

  if(w == -3) return curwin;

  return &windows[w];
}

#ifdef ZTERP_GLK
/* When resizing the upper window, the screen’s contents should not
 * change (§8.6.1); however, the way windows are handled with GLK makes
 * this slightly impossible.  When an Inform game tries to display
 * something with “box”, it expands the upper window, displays the quote
 * box, and immediately shrinks the window down again.  This is a
 * problem under GLK because the window immediately disappears.  Other
 * games, such as Bureaucracy, expect the upper window to shrink as soon
 * as it has been requested.  Thus the following system is used:
 *
 * If a request is made to shrink the upper window, it is granted
 * immediately if there has been user input since the last window resize
 * request.  If there has not been user input, the request is delayed
 * until after the next user input is read.
 */
static long delayed_window_shrink = -1;
static int saw_input;

static void update_delayed(void)
{
  if(delayed_window_shrink == -1 || upperwin->id == NULL) return;

  glk_window_set_arrangement(glk_window_get_parent(upperwin->id), winmethod_Above | winmethod_Fixed, delayed_window_shrink, upperwin->id);
  upper_window_height = delayed_window_shrink;
  delayed_window_shrink = -1;
}
#endif

void show_message(const char *fmt, ...)
{
  va_list ap;
  char message[1024];

  va_start(ap, fmt);
  vsnprintf(message, sizeof message, fmt, ap);
  va_end(ap);

#ifdef ZTERP_GLK
  static int error_lines = 0;

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
    errorwin = glk_window_open(mainwin->id, winmethod_Below | winmethod_Fixed, error_lines = 1, wintype_TextBuffer, 0);
  }

  /* If windows are not supported (e.g. in cheapglk), messages will not
   * get displayed.  If this is the case, attempt to print to standard
   * error.  This is suboptimal, but should not be horrendously bad.
   */
  if(errorwin != NULL)
  {
    glk_set_style_stream(glk_window_get_stream(errorwin), style_Alert);
    glk_put_string_stream(glk_window_get_stream(errorwin), message);
  }
  else
#endif
  {
    /* In GLK messages go to a separate window, but they're interleaved
     * in non-GLK.  Put brackets around the message in an attempt to
     * offset it from the game a bit.
     */
    fprintf(stderr, "\n[%s]\n", message);
  }
}

/* Resize or create the upper window with a height of “nlines”.
 * If this fails, upperwin->id will be set to NULL.
 */
static void open_upper_window(long nlines)
{
#ifdef ZTERP_GLK
  /* The upper window appeared in V3. */
  if(zversion <= 2) return;

  if(upperwin->id == NULL)
  {
    glui32 w, h;
    upperwin->id = glk_window_open(mainwin->id, winmethod_Above | winmethod_Fixed, nlines, wintype_TextGrid, 0);
    upperwin->x = upperwin->y = 0;
    upper_window_height = nlines;

    if(upperwin->id != NULL)
    {
      glk_window_get_size(upperwin->id, &w, &h);
      if(h != nlines) die("upper window created with wrong size");
      upper_window_width = w;

      /* Is this possible? */
      if(upper_window_width == 0)
      {
        glk_window_close(upperwin->id, NULL);
        upperwin->id = NULL;
      }
    }
  }
  else
  {
    glui32 height;

    glk_window_get_size(upperwin->id, NULL, &height);

    if(upper_window_height <= nlines || saw_input)
    {
      glk_window_set_arrangement(glk_window_get_parent(upperwin->id), winmethod_Above | winmethod_Fixed, nlines, upperwin->id);
      upper_window_height = nlines;
      delayed_window_shrink = -1;
    }
    else
    {
      delayed_window_shrink = nlines;
    }

    saw_input = 0;
  }

  /* §8.6.1.1.2 */
  if(upperwin->id != NULL && zversion == 3)
  {
    upperwin->x = upperwin->y = 0;
    glk_window_clear(upperwin->id);
  }

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
  open_upper_window(0);

#ifdef ZTERP_GLK
  saw_input = 0;
#endif

  set_current_window(mainwin);
}

void get_window_size(unsigned int *width, unsigned int *height)
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
  zterp_os_get_winsize(width, height);
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
  if(is_story("83-890706") && *height > 6) *height = 6;
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
    if(term_keys == NULL) die("realloc: %s", strerror(errno));
  }

  term_keys[term_nkeys++] = key;
}

void term_keys_add(uint8_t key)
{
  switch(key)
  {
    case 129: insert_key(keycode_Up); break;
    case 130: insert_key(keycode_Down); break;
    case 131: insert_key(keycode_Left); break;
    case 132: insert_key(keycode_Right); break;
    case 133: insert_key(keycode_Func1); break;
    case 134: insert_key(keycode_Func2); break;
    case 135: insert_key(keycode_Func3); break;
    case 136: insert_key(keycode_Func4); break;
    case 137: insert_key(keycode_Func5); break;
    case 138: insert_key(keycode_Func6); break;
    case 139: insert_key(keycode_Func7); break;
    case 140: insert_key(keycode_Func8); break;
    case 141: insert_key(keycode_Func9); break;
    case 142: insert_key(keycode_Func10); break;
    case 143: insert_key(keycode_Func11); break;
    case 144: insert_key(keycode_Func12); break;

    /* Keypad 0–9 should be here, but GLK doesn’t support that. */
    case 145: case 146: case 147: case 148: case 149:
    case 150: case 151: case 152: case 153: case 154:
      break;

    /* Mouse clicks would go here if I supported them. */
    case 252: case 253: case 254:
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

#ifdef ZTERP_GLK
/* In an interrupt, if the story tries to read or write, the previous
 * read event (which triggered the interrupt) needs to be canceled.
 * This function does the cancellation.
 */
static void cancel_read_events(void)
{
  if(curwin->pending_read)
  {
    event_t ev;

    glk_cancel_char_event(curwin->id);
    glk_cancel_line_event(curwin->id, &ev);

    /* If the pending read was a line input, zero terminate the string
     * so when it’s re-requested the length of the already-loaded
     * portion can be discovered.
     */
    if(ev.type == evtype_LineInput && curwin->line != NULL)
    {
      if(have_unicode) ((glui32 *)curwin->line)[ev.val1] = 0;
      else             ((char   *)curwin->line)[ev.val1] = 0;
    }

    curwin->pending_read = 0;
    curwin->line = NULL;
  }
}
#endif

/* Print out a character.  The character is in “c” and is either Unicode
 * or ZSCII; if the former, “unicode” is true.
 */
static void put_char_base(uint16_t c, int unicode)
{
  if(c == 0) return;

  if(streams & STREAM_MEMORY)
  {
    ZASSERT(stablei != -1, "invalid stream table");

    /* When writing to memory, ZSCII should always be used (§7.5.3). */
    if(unicode) c = unicode_to_zscii_q[c];

    user_store_byte(stables[stablei].table + stables[stablei].i++, c);
  }
  else
  {
    /* For screen and transcription, always prefer Unicode. */
    if(!unicode) c = zscii_to_unicode[c];

    if(c != 0)
    {
      uint8_t zscii = 0;

      if(curwin->font == FONT_CHARACTER && !options.disable_graphics_font)
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
        cancel_read_events();

        if(curwin == upperwin)
        {
          /* If the game tries to write beyond the right or bottom edges
           * of the window, ignore it.  For the life of me I can’t find
           * anything in the standard relating to this, so follow the
           * majority of interpreters.
           */
          if(c == UNICODE_LINEFEED)
          {
            upperwin->x = 0;

            if(upperwin->y < upper_window_height)
            {
              upperwin->y++;
              GLK_PUT_CHAR(c);
            }
          }
          else if(upperwin->x < upper_window_width)
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
  put_char_base(c, 1);
}

void put_char(uint8_t c)
{
  put_char_base(c, 0);
}

/* Decode and print a zcode string at address “addr”.  This can be
 * called recursively thanks to abbreviations; the initial call should
 * have “in_abbr” set to 0.
 * Each time a character is decoded, it is passed to the function
 * “outc”.
 */
static int print_zcode(uint32_t addr, int in_abbr, void (*outc)(uint8_t))
{
  int abbrev = 0, shift = 0, special = 0;
  int c, lastc = 0; /* Initialize lastc to shut gcc up */
  uint16_t w;
  uint32_t counter = addr;

#ifndef ZTERP_NO_V2
  int current_alphabet = 0;
#endif

  do
  {
    w = WORD(counter);

    for(int i = 10; i >= 0; i -= 5)
    {
      c = (w >> i) & 0x1f;

      if(special)
      {
        if(special == 2) lastc = c;
        else             outc((lastc << 5) | c);

        special--;
      }

      else if(abbrev)
      {
        uint32_t new_addr;

        new_addr = WORD(header.abbr + 64 * (abbrev - 1) + 2 * c);

        /* new_addr is a word address, so multiply by 2 */
        print_zcode(new_addr * 2, 1, outc);

        abbrev = 0;
      }

      else switch(c)
      {
        case 0:
          outc(ZSCII_SPACE);
          shift = 0;
          break;
        case 1:
#ifndef ZTERP_NO_V2
          if(zversion == 1)
          {
            outc(ZSCII_NEWLINE);
            shift = 0;
            break;
          }
          /* fallthrough */
#endif
        case 2: case 3:
#ifndef ZTERP_NO_V2
          if(zversion > 2 || (zversion == 2 && c == 1))
          {
#endif
          ZASSERT(!in_abbr, "abbreviation being used recursively");
          abbrev = c;
          shift = 0;
#ifndef ZTERP_NO_V2
          }
          else
          {
            shift = c - 1;
          }
#endif
          break;
        case 4:
#ifndef ZTERP_NO_V2
          if(zversion <= 2)
          {
            current_alphabet = (current_alphabet + 1) % 3;
            shift = 0;
          }
          else
#endif
          shift = 1;
          break;
        case 5:
#ifndef ZTERP_NO_V2
          if(zversion <= 2)
          {
            current_alphabet = (current_alphabet + 2) % 3;
            shift = 0;
          }
          else
#endif
          shift = 2;
          break;
        case 6:
#ifndef ZTERP_NO_V2
          if(zversion <= 2) shift = (current_alphabet + shift) % 3;
#endif
          if(shift == 2)
          {
            shift = 0;
            special = 2;
            break;
          }
          /* fallthrough */
        default:
#ifndef ZTERP_NO_V2
          if(zversion <= 2 && c != 6) shift = (current_alphabet + shift) % 3;
#endif
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
  ZASSERT(addr < memory_size, "print at invalid address: %lx", (unsigned long)addr);

  return print_zcode(addr, 0, outc != NULL ? outc : put_char);
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
  switch((int16_t)zargs[0])
  {
    case -2:
      for(int i = 0; i < 8; i++) if(windows[i].id != NULL) glk_window_clear(windows[i].id);
      break;
    case -1:
      close_upper_window();
      /* fallthrough */
    case 0:
      /* 8.7.3.2.1 says V5+ should have the cursor set to 1, 1 of the
       * erased window; V4 the lower window goes bottom left, the upper
       * to 1, 1.  GLK doesn’t give control over the cursor when
       * clearing, and that doesn’t really seem to be an issue; so just
       * call glk_window_clear().
       */
      glk_window_clear(mainwin->id);
      break;
    case 1:
      if(upperwin->id != NULL) glk_window_clear(upperwin->id);
      break;
    default:
      show_message("@erase_window: unhandled window: %d", (int16_t)zargs[0]);
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
  if(zargs[0] != 1 || curwin != upperwin) return;

  for(long i = upperwin->x; i < upper_window_width; i++) GLK_PUT_CHAR(UNICODE_SPACE);

  glk_window_move_cursor(upperwin->id, upperwin->x, upperwin->y);
#endif
}

/* XXX This is more complex in V6 and needs to be updated when V6 windowing is implemented. */
void zset_cursor(void)
{
#ifdef ZTERP_GLK
  /* All the windows in V6 can have their cursor positioned; if V6 ever
   * comes about this should be fixed.
   */
  if(curwin != upperwin) return;

  /* -1 and -2 are V6 only, but at least Zracer passes -1 (or it’s
   * trying to position the cursor to line 65535; unlikely!)
   */
  if((int16_t)zargs[0] == -1 || (int16_t)zargs[0] == -2) return;

  /* §8.7.2.3 says 1,1 is the top-left, but at least one program (Paint
   * and Corners) uses @set_cursor 0 0 to go to the top-left; so
   * special-case it.
   */
  if(zargs[0] == 0) zargs[0] = 1;
  if(zargs[1] == 0) zargs[1] = 1;

  /* This is actually illegal, but some games (e.g. Beyond Zork) expect it to work. */
  if(zargs[0] > upper_window_height) open_upper_window(zargs[0]);

  if(upperwin->id != NULL)
  {
    upperwin->x = zargs[1] - 1;
    upperwin->y = zargs[0] - 1;

    glk_window_move_cursor(upperwin->id, zargs[1] - 1, zargs[0] - 1);
  }
#endif
}

void zget_cursor(void)
{
#ifdef ZTERP_GLK
  user_store_word(zargs[0] + 0, upperwin->y + 1);
  user_store_word(zargs[0] + 2, upperwin->x + 1);
#else
  user_store_word(zargs[0] + 0, 0);
  user_store_word(zargs[0] + 2, 0);
#endif
}

#ifndef ZTERP_GLK
static int16_t fg_color = 0, bg_color = 0;
#elif defined(GARGLK)
/* Adapted from Gargoyle’s Frotz (Copyright 2010 Ben Cressey). */
static glui32 zcolor_map[] = {
  zcolor_Default,

  0x000000,		/*  2 = black */
  0xef0000,		/*  3 = red */
  0x00d600,		/*  4 = green */
  0xefef00,		/*  5 = yellow */
  0x006bb5,		/*  6 = blue */
  0xff00ff,		/*  7 = magenta */
  0x00efef,		/*  8 = cyan */
  0xffffff,		/*  9 = white */
  0xb5b5b5,		/* 10 = light grey */
  0x8c8c8c,		/* 11 = medium grey */
  0x5a5a5a		/* 12 = dark grey */
};
static glui32 fg_color = zcolor_Default, bg_color = zcolor_Default;

void update_color(int which, unsigned long color)
{
  if(which < 2 || which > 9) return;

  zcolor_map[which - 1] = color;
}
#endif

/* A window argument may be supplied in V6, and this needs to be implemented. */
void zset_colour(void)
{
  /* GLK (apart from Gargoyle) has no color support. */
#if !defined(ZTERP_GLK) || defined(GARGLK)
  int16_t fg = zargs[0], bg = zargs[1];

  /* In V6, each window has its own color settings.  Since multiple
   * windows are not supported, simply ignore all color requests except
   * those in the main window.
   */
  if(zversion == 6 && curwin != mainwin) return;

  if(options.disable_color) return;

  /* XXX -1 is a valid color in V6. */
#ifdef GARGLK
  if(fg >= 1 && fg <= (zversion == 6 ? 12 : 9)) fg_color = zcolor_map[fg - 1];
  if(bg >= 1 && bg <= (zversion == 6 ? 12 : 9)) bg_color = zcolor_map[bg - 1];
#else
  if(fg >= 1 && fg <= 9) fg_color = fg;
  if(bg >= 1 && bg <= 9) bg_color = bg;
#endif

  set_current_style();
#endif
}

/* Adapted from Gargoyle’s Frotz (Copyright 2010 Ben Cressey). */
#ifdef GARGLK
#define zB(i)	(((((i) >> 10) & 0x1f) << 3) | ((((i) >> 10) & 0x1f) >> 2))
#define zG(i)	(((((i) >>  5) & 0x1f) << 3) | ((((i) >>  5) & 0x1f) >> 2))
#define zR(i)	(((((i) >>  0) & 0x1f) << 3) | ((((i) >>  0) & 0x1f) >> 2))
#define zRGB(i)	(zR(i) << 16 | zG(i) << 8 | zB(i))
#endif

void zset_true_colour(void)
{
#ifdef GARGLK
  long fg = (int16_t)zargs[0], bg = (int16_t)zargs[1];

  if     (fg >=  0) fg_color = zRGB(fg);
  else if(fg == -1) fg_color = zcolor_Default;

  if     (bg >=  0) bg_color = zRGB(bg);
  else if(bg == -1) bg_color = zcolor_Default;

  set_current_style();
#endif
}

int header_fixed_font;

#ifdef GARGLK
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
#endif

/* Yes, there are three ways to indicate that a fixed-width font should be used. */
#define use_fixed_font() (header_fixed_font || curwin->font == FONT_FIXED || (style & STYLE_FIXED))

void set_current_style(void)
{
  unsigned style = style_window->style;
#ifdef ZTERP_GLK
#ifdef GARGLK
  if(use_fixed_font()) style |= STYLE_FIXED;

  if(options.disable_fixed) style &= ~STYLE_FIXED;

  ZASSERT(style < 16, "invalid style selected: %x", (unsigned)style);

  glk_set_style(style_map[style]);

  garglk_set_reversevideo(style & STYLE_REVERSE);

  garglk_set_zcolors(fg_color, bg_color);
#else
  /* GLK can’t mix other styles with fixed-width, but the upper window
   * is always fixed, so if it is selected, there is no need to
   * explicitly request it here.  This means that another style can also
   * be applied if applicable.
   */
  if(use_fixed_font() && !options.disable_fixed && curwin != upperwin)
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
  /* zterp_os_set_style() understands Z-machine colors: if fg_color or
   * bg_color is 0 or 1, it does nothing: 0 means keep the current
   * setting, so nothing needs to be done.  1 means default, and since
   * zterp_os_set_style() resets everything, the default color is
   * automatically selected.
   */
  zterp_os_set_style(style, fg_color, bg_color);
#endif
}

#undef use_fixed_font

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

  if(zversion == 6 && znargs == 2 && (int16_t)zargs[1] != -3)
  {
    ZASSERT(zargs[1] < 8, "invalid window selected: %d", (int16_t)zargs[1]);
    win = &windows[zargs[1]];
  }

  /* If no previous font has been stored, consider that an error. */
  if(zargs[0] == FONT_PREVIOUS && win->prev_font != FONT_NONE)
  {
    zargs[0] = win->prev_font;
    zset_font();
  }
  else if(zargs[0] == FONT_NORMAL || (!options.disable_graphics_font && zargs[0] == FONT_CHARACTER) || (!options.disable_fixed && zargs[0] == FONT_FIXED))
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
        zargs[0] = upperwin->y + 2;
        zargs[1] = start;
        zset_cursor();
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
  int i = 0;
  int32_t v = (int16_t)zargs[0];

  if(v < 0) v = -v;

  do
  {
    buf[i++] = '0' + (v % 10);
  } while(v /= 10);

  if((int16_t)zargs[0] < 0) buf[i++] = '-';

  while(i--) put_char(buf[i]);
}

void zprint_addr(void)
{
  print_handler(zargs[0], NULL);
}

void zprint_paddr(void)
{
  print_handler(unpack(zargs[0], 1), NULL);
}

/* XXX This is more complex in V6 and needs to be updated when V6 windowing is implemented. */
void zsplit_window(void)
{
  if(zargs[0] == 0) close_upper_window();
  else              open_upper_window(zargs[0]);
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
    long x = upperwin->x, y = upperwin->y;
    glui32 w, h;

    glk_window_get_size(upperwin->id, &w, &h);

    upper_window_width = w;
    upper_window_height = h;

    if(x > w) x = w;
    if(y > h) y = h;
    zargs[0] = y + 1;
    zargs[1] = x + 1;
      
    SWITCH_WINDOW_START(upperwin);
    zset_cursor();
    SWITCH_WINDOW_END();
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

    get_window_size(&width, &height);

    STORE_BYTE(0x20, height > 254 ? 254 : height);
    STORE_BYTE(0x21, width > 255 ? 255 : width);

    if(zversion >= 5)
    {
      STORE_WORD(0x22, width > UINT16_MAX ? UINT16_MAX : width);
      STORE_WORD(0x24, height > UINT16_MAX ? UINT16_MAX : height);
    }
  }
  else
  {
    zshow_status();
  }
}
#endif

#ifdef ZTERP_GLK
static int timer_running;

static void start_timer(uint16_t n)
{
  if(!TIMER_AVAILABLE()) return;

  if(timer_running) die("nested timers unsupported");
  glk_request_timer_events(n * 100);
  timer_running = 1;
}

static void stop_timer(void)
{
  if(!TIMER_AVAILABLE()) return;

  glk_request_timer_events(0);
  timer_running = 0;
}

static void request_char(void)
{
  if(have_unicode) glk_request_char_event_uni(curwin->id);
  else             glk_request_char_event(curwin->id);

  curwin->pending_read = 1;
}

static void request_line(void *buf, glui32 maxlen, glui32 initlen)
{
  if(have_unicode) glk_request_line_event_uni(curwin->id, buf, maxlen, initlen);
  else             glk_request_line_event(curwin->id, buf, maxlen, initlen);

  curwin->pending_read = 1;
  curwin->line = buf;
}
#endif

#define INPUT_CHAR	0
#define INPUT_LINE	1

struct input
{
  int type;

  /* ZSCII value of key read for @read_char. */
  uint8_t key;

  /* Unicode line of chars read for @read. */
  uint32_t *line;
  uint8_t maxlen;
  uint8_t len;
  uint8_t preloaded;

#ifdef GLK_MODULE_LINE_TERMINATORS
  /* Character used to terminate input. */
  uint8_t term;
#endif
};

#define special_zscii(c) ((c) >= 129 && (c) <= 154)

/* This is called when input stream 1 (read from file) is selected.  If
 * it succefully reads a character/line from the file, it fills the
 * struct at “input” with the appropriate information and returns true.
 * If it fails to read (likely due to EOF) then it sets the input stream
 * back to the keyboard and returns false.
 */
static int istream_read_from_file(struct input *input)
{
  if(input->type == INPUT_CHAR)
  {
    long c;

    c = zterp_io_getc(istreamio);
    if(c == -1)
    {
      input_stream(ISTREAM_KEYBOARD);
      return 0;
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
      return 0;
    }

    if(n > input->maxlen) n = input->maxlen;

    input->len = n;

#ifdef ZTERP_GLK
    glk_set_style(style_Input);
    for(long i = 0; i < n; i++) GLK_PUT_CHAR(line[i]);
    GLK_PUT_CHAR(UNICODE_LINEFEED);
    set_current_style();
#else
    for(long i = 0; i < n; i++) zterp_io_putc(zterp_io_stdout(), line[i]);
    zterp_io_putc(zterp_io_stdout(), UNICODE_LINEFEED);
#endif

    for(long i = 0; i < n; i++) input->line[i] = line[i];

#ifdef GLK_MODULE_LINE_TERMINATORS
    input->term = ZSCII_NEWLINE;
#endif
  }

#ifdef ZTERP_GLK
  saw_input = 1;
#endif

  return 1;
}

#ifdef GLK_MODULE_LINE_TERMINATORS
/* GLK returns terminating characters as keycode_*, but we need them as
 * ZSCII.  This should only ever be called with values that are matched
 * in the switch, because those are the only ones that GLK was told are
 * terminating characters.  In the event that another keycode comes
 * through, though, treat it as Enter.
 */
static uint8_t zscii_from_glk(glui32 key)
{
  switch(key)
  {
    case 13:             return ZSCII_NEWLINE;
    case keycode_Up:     return 129;
    case keycode_Down:   return 130;
    case keycode_Left:   return 131;
    case keycode_Right:  return 131;
    case keycode_Func1:  return 133;
    case keycode_Func2:  return 134;
    case keycode_Func3:  return 135;
    case keycode_Func4:  return 136;
    case keycode_Func5:  return 137;
    case keycode_Func6:  return 138;
    case keycode_Func7:  return 139;
    case keycode_Func8:  return 140;
    case keycode_Func9:  return 141;
    case keycode_Func10: return 142;
    case keycode_Func11: return 143;
    case keycode_Func12: return 144;
  }
  
  return ZSCII_NEWLINE;
}
#endif

#ifdef ZTERP_GLK
/* This is like strlen() but in addition to C strings it can find the
 * length of a Unicode string (which is assumed to be zero terminated)
 * if Unicode is being used.
 */
static size_t line_len(const void *line)
{
  const glui32 *line_uni = line;
  size_t i;

  if(!have_unicode) return strlen(line);

  for(i = 0; line_uni[i] != 0; i++)
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
 * The function returns 1 if input was stored, 0 if there was a
 * cancellation as described above.
 */
static int get_input(int16_t timer, int16_t routine, struct input *input)
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

  if(istream == ISTREAM_FILE && istream_read_from_file(input)) return 1;
#ifdef ZTERP_GLK
  int status = 0;
  char line[256];
  glui32 line_uni[256];
  void *line_buf;
  struct window *saved = NULL;

  /* In V6, input might be requested on an unsupported window.  If so,
   * switch to the main window temporarily.
   */
  if(curwin->id == NULL)
  {
    saved = curwin;
    curwin = mainwin;
  }

  /* This is done so that request_line() can be used either with or without Unicode. */
  if(have_unicode)
  {
    line_buf = line_uni;
    for(int i = 0; i < input->preloaded; i++) line_uni[i] = input->line[i];
  }
  else
  {
    line_buf = line;
    for(int i = 0; i < input->preloaded; i++) line[i] = input->line[i];
  }

  if(input->type == INPUT_CHAR) request_char();
  else                          request_line(line_buf, input->maxlen, input->preloaded);

  if(timer != 0) start_timer(timer);

  while(status == 0)
  {
    event_t ev;

    glk_select(&ev);

    switch(ev.type)
    {
      case evtype_Arrange:
        window_change();
        break;

      case evtype_Timer:
        ZASSERT(timer != 0, "got unexpected evtype_Timer");

        stop_timer();

        if(direct_call(routine))
        {
          status = 2;
        }
        else
        {
          /* If this got reset to 0, that means an interrupt had to
           * cancel the read event in order to either read or write.
           */
          if(!curwin->pending_read)
          {
            if(input->type == INPUT_CHAR) request_char();
            else                          request_line(line_buf, input->maxlen, line_len(line_buf));
          }

          start_timer(timer);
        }

        break;

      case evtype_CharInput:
        ZASSERT(input->type == INPUT_CHAR, "got unexpected evtype_CharInput");
        ZASSERT(ev.win == curwin->id, "got evtype_CharInput on unexpected window");

        status = 1;

        switch(ev.val1)
        {
          case keycode_Delete: input->key =   8; break;
          case keycode_Return: input->key =  13; break;
          case keycode_Escape: input->key =  27; break;
          case keycode_Up:     input->key = 129; break;
          case keycode_Down:   input->key = 130; break;
          case keycode_Left:   input->key = 131; break;
          case keycode_Right:  input->key = 132; break;
          case keycode_Func1:  input->key = 133; break;
          case keycode_Func2:  input->key = 134; break;
          case keycode_Func3:  input->key = 135; break;
          case keycode_Func4:  input->key = 136; break;
          case keycode_Func5:  input->key = 137; break;
          case keycode_Func6:  input->key = 138; break;
          case keycode_Func7:  input->key = 139; break;
          case keycode_Func8:  input->key = 140; break;
          case keycode_Func9:  input->key = 141; break;
          case keycode_Func10: input->key = 142; break;
          case keycode_Func11: input->key = 143; break;
          case keycode_Func12: input->key = 144; break;

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
        for(glui32 i = 0; i < ev.val1; i++)
        {
          if(have_unicode) input->line[i] = line_uni[i] > UINT16_MAX ? UNICODE_QUESTIONMARK : line_uni[i];
          else             input->line[i] = (uint8_t)line[i];
        }
        input->len = ev.val1;
#ifdef GLK_MODULE_LINE_TERMINATORS
        if(zversion >= 5) input->term = zscii_from_glk(ev.val2);
#endif
        status = 1;
        break;
    }
  }

  stop_timer();

  if(input->type == INPUT_CHAR) glk_cancel_char_event(curwin->id);
  else                          glk_cancel_line_event(curwin->id, NULL);

  curwin->pending_read = 0;
  curwin->line = NULL;

  if(status == 1) saw_input = 1;

  if(errorwin != NULL)
  {
    glk_window_close(errorwin, NULL);
    errorwin = NULL;
  }

  if(saved != NULL) curwin = saved;

  return status != 2;
#else
  if(input->type == INPUT_CHAR)
  {
    long n;
    uint16_t line[64];

    n = zterp_io_readline(zterp_io_stdin(), line, sizeof line / sizeof *line);

    /* On error/eof, or if an invalid key was typed, pretend “Enter” was hit. */
    if(n <= 0)
    {
      input->key = ZSCII_NEWLINE;
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
      if(n != -1)
      {
        if(n > input->maxlen - input->preloaded) n = input->maxlen - input->preloaded;
        for(long i = 0; i < n; i++) input->line[i + input->preloaded] = line[i];
        input->len += n;
      }
    }
  }

  return 1;
#endif
}

void zread_char(void)
{
  uint16_t timer = 0;
  uint16_t routine = zargs[2];
  struct input input = { .type = INPUT_CHAR };

#ifdef ZTERP_GLK
  cancel_read_events();
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

#ifdef GARGLK
  garglk_set_reversevideo(1);
#else
  glk_set_style(style_Alert);
#endif
  for(glui32 i = 0; i < width; i++) glk_put_char(ZSCII_SPACE);

  glk_window_move_cursor(statuswin.id, 1, 0);

  /* Variable 0x10 is global variable 1. */
  print_object(variable(0x10), status_putc);

  if(STATUS_IS_TIME())
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

void zread(void)
{
  uint16_t text = zargs[0], parse = zargs[1];
  uint8_t maxchars = zversion >= 5 ? user_byte(text) : user_byte(text) - 1;
  uint8_t zscii_string[maxchars];
  uint8_t zscii_length;
  uint32_t string[maxchars + 1];
  struct input input = { .type = INPUT_LINE, .line = string, .maxlen = maxchars };
  uint16_t timer = 0;
  uint16_t routine = zargs[3];

#ifdef ZTERP_GLK
  cancel_read_events();
#endif

  if(zversion <= 3) zshow_status();

  if(zversion >= 4 && znargs > 2) timer = zargs[2];

  if(zversion >= 5)
  {
    int i;

    input.preloaded = user_byte(text + 1);
    ZASSERT(input.preloaded <= maxchars, "too many preloaded characters: %d when max is %d", input.preloaded, maxchars);

    for(i = 0; i < input.preloaded; i++) string[i] = zscii_to_unicode[memory[text + i + 2]];
    string[i] = 0;

    /* Under garglk, preloaded input works as it’s supposed to.
     * Under GLK, it can fail one of two ways:
     * 1. The preloaded text is printed out once, but is not editable.
     * 2. The preloaded text is printed out twice, the second being editable.
     * I have chosen option #2.  For non-GLK, option #1 is done by necessity.
     */
#ifdef GARGLK
    garglk_unput_string_uni(string);
#endif
  }

  if(!get_input(timer, routine, &input))
  {
    if(zversion >= 5) store(0);
    return;
  }

#ifdef ZTERP_GLK
  update_delayed();
#endif

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

  zscii_length = input.len;

  if(streams & STREAM_TRANS)  zterp_io_putc(transio, UNICODE_LINEFEED);
  if(streams & STREAM_SCRIPT) zterp_io_putc(scriptio, UNICODE_LINEFEED);

  if(zversion >= 5)
  {
    user_store_byte(text + 1, zscii_length); /* number of characters read */

    for(int i = 0; i < zscii_length; i++)
    {
      user_store_byte(text + i + 2, zscii_string[i]);
    }

    if(parse != 0) tokenize(text, parse, 0, 0);

#ifdef GLK_MODULE_LINE_TERMINATORS
    store(input.term);
#else
    store(ZSCII_NEWLINE); /* enter was hit; 1.0 recommends 10, but 1.1 mandates 13 (which is ZSCII newline). */
#endif
  }
  else
  {
    for(int i = 0; i < zscii_length; i++)
    {
      user_store_byte(text + i + 1, zscii_string[i]);
    }

    user_store_byte(text + zscii_length + 1, 0);

    tokenize(text, parse, 0, 0);
  }
}

void zprint_unicode(void)
{
  if(valid_unicode(zargs[0])) put_char_u(zargs[0]);
  else                        put_char_u(UNICODE_QUESTIONMARK);
}

void zcheck_unicode(void)
{
  uint16_t res = 0;

  /* valid_unicode() will tell which Unicode characters can be printed;
   * and if the Unicode character is in the Unicode input table, it can
   * also be read.  If Unicode is not available, then any character >
   * 255 is invalid for both reading and writing.
   */
  if(have_unicode || zargs[0] < 256)
  {
    if(valid_unicode(zargs[0]))         res |= 0x01;
    if(unicode_to_zscii[zargs[0]] != 0) res |= 0x02;
  }

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
  branch_if(0);
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
    put_char_u(zscii_to_unicode[user_byte(zargs[0] + 2 + i)]);
  }

  put_char_u(UNICODE_LINEFEED);

  SWITCH_WINDOW_END();
}

void zbuffer_screen(void)
{
  store(0);
}

#ifdef GARGLK
/* GLK does not guarantee great control over how various styles are
 * going to look, but Gargoyle does.  Abusing the GLK “style hints”
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

int create_mainwin(void)
{
#ifdef ZTERP_GLK

#ifdef GARGLK
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
  if(mainwin->id == NULL) return 0;
  glk_set_window(mainwin->id);
  return 1;
#else
  return 1;
#endif
}

int create_statuswin(void)
{
#ifdef ZTERP_GLK
  if(statuswin.id == NULL) statuswin.id = glk_window_open(mainwin->id, winmethod_Above | winmethod_Fixed, 1, wintype_TextGrid, 0);
  return statuswin.id != NULL;
#else
  return 0;
#endif
}

int create_upperwin(void)
{
  open_upper_window(0);

#ifdef ZTERP_GLK
  return upperwin->id != NULL;
#else
  return 0;
#endif
}

void init_screen(void)
{
  for(int i = 0; i < 8; i++)
  {
    windows[i].style = STYLE_NONE;
    windows[i].font = FONT_NORMAL;
    windows[i].prev_font = FONT_NONE;
    windows[i].x = windows[i].y = 0;

#ifdef ZTERP_GLK
    if(windows[i].id != NULL) glk_window_clear(windows[i].id);
    windows[i].pending_read = 0;
    windows[i].line = NULL;
#ifdef GLK_MODULE_LINE_TERMINATORS
    if(windows[i].id != NULL && term_nkeys != 0 && glk_gestalt(gestalt_LineTerminators, 0)) glk_set_terminators_line_event(windows[i].id, term_keys, term_nkeys);
#endif
#endif
  }

#ifdef ZTERP_GLK
  if(statuswin.id != NULL) glk_window_clear(statuswin.id);
#endif

  close_upper_window();

#ifdef ZTERP_GLK
#ifdef GARGLK
  fg_color = zcolor_Default;
  bg_color = zcolor_Default;
#endif

  timer_running = 0;
  glk_cancel_char_event(mainwin->id);
  glk_cancel_line_event(mainwin->id, NULL);
  glk_request_timer_events(0);
#else
  fg_color = 0;
  bg_color = 0;
#endif

  if(scriptio != NULL) zterp_io_close(scriptio);
  scriptio = NULL;

  input_stream(ISTREAM_KEYBOARD);

  streams = STREAM_SCREEN;
  stablei = -1;
  set_current_window(mainwin);
}
