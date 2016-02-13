/*-
 * Copyright 2013-2014 Chris Spiegel.
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
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <limits.h>

#include "meta.h"
#include "memory.h"
#include "objects.h"
#include "process.h"
#include "screen.h"
#include "stack.h"
#include "zterp.h"

#define ISDIGIT(c)	(((c) >= '0' && (c) <= '9'))
#define ISXDIGIT(c)	(ISDIGIT(c) || ((c) >= 'a' && (c) <= 'f'))

/* These are strcmp() and strncmp() except that the first string is Unicode. */
static int unicmp(const uint32_t *s1, const char *s2)
{
  while(*s1 != 0 && *s2 == *s1)
  {
    s1++;
    s2++;
  }

  return *s1 - *s2;
}
static int unincmp(const uint32_t *s1, const char *s2, size_t n)
{
  if(n == 0) return 0;
  while(--n > 0 && *s1 != 0 && *s1 == *s2)
  {
    s1++;
    s2++;
  }

  return *s1 - *s2;
}

/* Convert a string to uint32_t. */
static int32_t unito32(const uint32_t *s, int base, int *valid)
{
  const char digits[] = "0123456789abcdef";
  uint32_t n = 0;
  const char *digit;
  int32_t neg = 1;
  uint32_t max = INT32_MAX;

  while(s[0] == ' ') s++;

  if(s[0] == '-')
  {
    neg = -1;
    s++;
    max++; /* Assumes two’s complement, which int32_t is guaranteed to be. */
  }

  if(base == 0)
  {
    base = 10;
    if(unincmp(s, "0x", 2) == 0)
    {
      s += 2;
      base = 16;
    }
  }
  else if(base == 16 && unincmp(s, "0x", 2) == 0)
  {
    s += 2;
  }

  if(base < 2 || base > 16) return 0;

  while((digit = strchr(digits, *s++)) != NULL && (digit - digits) < base)
  {
    int d = (digit - digits);

    /* Detect overflow. */
    if((max / base < n) || (max - (n * base) < d))
    {
      if(valid != NULL) *valid = 1;

      if(neg == -1) return INT32_MIN;
      else          return INT32_MAX;
    }

    n = (n * base) + d;
  }

  if(valid != NULL) *valid = s[-1] == 0;

  return n * neg;
}

uint32_t read_pc;

static void try_user_save(const char *desc)
{
  if(in_interrupt())
  {
    screen_print("[cannot save while in an interrupt]");
  }
  else
  {
    if(push_save(SAVE_USER, read_pc, desc)) screen_print("[save pushed]");
    else                                    screen_print("[save failed]");
  }
}

/* On success, this does not return. */
static void try_user_restore(long i)
{
  if(pop_save(SAVE_USER, i))
  {
    screen_print("[restored]\n\n>");
    interrupt_reset();
  }
  else
  {
    screen_print("[restore failed]");
  }
}

static void meta_status_putc(uint8_t c)
{
  screen_print((char[]){c, 0});
}

/* The Z-machine does not require aligned memory access, so
 * both even and odd addresses must be checked.  However,
 * global variables are word-sized, so if an address inside
 * the global variables has changed, report only if the
 * address is the base of globals plus a multiple of two.
 */
static int in_globals(uint16_t addr)
{
  return addr >= header.globals && addr < header.globals + 480;
}
static int is_global(uint16_t addr)
{
  return in_globals(addr) && (addr - header.globals) % 2 == 0;
}
static unsigned long addr_to_global(uint16_t addr)
{
  return (addr - header.globals) / 2;
}

static uint8_t *debug_change_memory;
static int debug_change_valid[65536];

static void meta_debug_change_start(void)
{
  free(debug_change_memory);
  debug_change_memory = malloc(header.static_start);
  if(debug_change_memory == NULL)
  {
    screen_print("[cannot allocate memory]");
  }
  else
  {
    memcpy(debug_change_memory, memory, header.static_start);
    for(size_t i = 0; i < sizeof debug_change_valid / sizeof *debug_change_valid; i++) debug_change_valid[i] = 1;
    screen_print("[debug change reset]");
  }
}

static void meta_debug_change_inc_dec(const uint32_t *string)
{
  if(debug_change_memory == NULL)
  {
    screen_print("[debug change not started]");
  }
  else
  {
    int saw_change = 0;

    for(uint16_t i = 0; i < header.static_start - 2; i++)
    {
      int16_t new = as_signed(WORD(i));
      int16_t old = as_signed((debug_change_memory[i] << 8) | debug_change_memory[i + 1]);

#define CMP(a, b) (unincmp(string, "inc", 3) == 0 ? a > b : a < b)
      if(CMP(new, old) && debug_change_valid[i])
#undef CMP
      {
        char msg[32];

        if(is_global(i))
        {
          saw_change = 1;
          snprintf(msg, sizeof msg, "G%02lx: %ld -> %ld", addr_to_global(i), (long)old, (long)new);
          screen_puts(msg);
        }
        else if(!in_globals(i))
        {
          saw_change = 1;
          snprintf(msg, sizeof msg, "0x%lx: %ld -> %ld", (unsigned long)i, (long)old, (long)new);
          screen_puts(msg);
        }
      }
      else
      {
        debug_change_valid[i] = 0;
      }
    }

    if(!saw_change) screen_print("[no changes]");

    memcpy(debug_change_memory, memory, header.static_start);
  }
}

static int meta_debug_change(const uint32_t *string)
{
  if(unicmp(string, "start") == 0)
  {
    meta_debug_change_start();
  }
  else if(unincmp(string, "inc", 3) == 0 || unincmp(string, "dec", 3) == 0)
  {
    meta_debug_change_inc_dec(string);
  }
  else
  {
    return -1;
  }

  return 0;
}

static int meta_debug_scan(const uint32_t *string)
{
  static int debug_scan_locations[65536];

  if(unicmp(string, "start") == 0)
  {
    memset(debug_scan_locations, 0, sizeof debug_scan_locations);
    screen_print("[debug scan reset]");
  }
  else if(unicmp(string, "show") == 0)
  {
    for(size_t i = 0; i < header.static_start - 2; i++)
    {
      if(debug_scan_locations[i] == 0)
      {
        char msg[16];

        if(is_global(i))
        {
          snprintf(msg, sizeof msg, "G%02lx", addr_to_global(i));
          screen_puts(msg);
        }
        else if(!in_globals(i))
        {
          snprintf(msg, sizeof msg, "0x%lx", (unsigned long)i);
          screen_puts(msg);
        }
      }
    }
  }
  else if(ISDIGIT(string[0]) || (string[0] == '-' && ISDIGIT(string[1])))
  {
    int32_t value;
    int valid;
    uint16_t count = 0;
    char msg[16];

    value = unito32(string, 0, &valid);
    if(!valid) return -1;

    if(value < -0x8000 || value > 0xffff)
    {
      screen_puts("[value is outside the range of a 16-bit integer and will never be found]");
    }
    else
    {
      if(value < 0) value += 0x10000;

      for(uint16_t i = 0; i < header.static_start - 2; i++)
      {
        if(WORD(i) != value) debug_scan_locations[i] = 1;
        if(debug_scan_locations[i] == 0) count++;
      }

      snprintf(msg, sizeof msg, "%lu location%s", (unsigned long)count, count == 1 ? "" : "s");
      screen_puts(msg);
    }
  }
  else
  {
    return -1;
  }

  return 0;
}

static int meta_debug_print(const uint32_t *string)
{
  int32_t addr;
  char msg[128];
  int valid;

  if(string[0] == 'G' && ISXDIGIT(string[1]) && ISXDIGIT(string[2]) && string[3] == 0)
  {
    addr = unito32(string + 1, 16, &valid);
    if(addr < 0 || addr > 239) return -1;
    addr = header.globals + (2 * addr);
  }
  else
  {
    addr = unito32(string, 16, &valid);
  }

  if(!valid) return -1;

#define MAX(x, y)	((x) > (y) ? (x) : (y))
  if(addr < 0 || addr > MAX(header.static_start, memory_size) - 2)
  {
    snprintf(msg, sizeof msg, "[address out of range (%lx/%lx)]", (unsigned long)header.static_start - 2, (unsigned long)memory_size - 2);
  }
  else
  {
    snprintf(msg, sizeof msg, "%ld (0x%lx)", (long)as_signed(WORD(addr)), (unsigned long)WORD(addr));
  }
#undef MAX

  screen_print(msg);

  return 0;
}

static int meta_debug_help(void)
{
  screen_print(
      "Debug commands are accessed as \"/debug [command]\".  Commands are:\n\n"
      "change start: restart change tracking\n"
      "change dec: display all words which have been decremented since the last check\n"
      "change inc: display all words which have been incremented since the last check\n"
      "scan start: restart scan tracking\n"
      "scan N: update scan list with all words equal to N; if N starts with 0x it is hexadecimal, decimal otherwise\n"
      "scan show: print all locations matching scan criteria\n"
      "print N: print the word at address N; N is hexadecimal\n"
      );

  return 0;
}

static void meta_debug(const uint32_t *string)
{
  int result = -1;

  if(unincmp(string, "change ", 7) == 0)
  {
    result = meta_debug_change(string + 7);
  }
  else if(unincmp(string, "scan ", 5) == 0)
  {
    result = meta_debug_scan(string + 5);
  }
  else if(unincmp(string, "print ", 6) == 0)
  {
    result = meta_debug_print(string + 6);
  }
  else if(unicmp(string, "help") == 0)
  {
    result = meta_debug_help();
  }

  if(result == -1) screen_print("[invalid syntax]");
}

/* Try to parse a meta command.  If input should be re-requested, return
 * “string”.  If a portion of the passed-in string should be processed
 * as user input, return a pointer to the beginning of that portion.
 *
 * There is also the possibility that a meta-command causes a saved game
 * of some kind to be restored (/undo, /restore, and /pop).  In these
 * cases NULL is conceptually returned on success, meaning parsing is
 * done.  However, because interrupt_reset() is called on a successful
 * restore, NULL will never actually be returned because
 * interrupt_reset() will call longjmp().
 */
const uint32_t *handle_meta_command(const uint32_t *string)
{
  if(unicmp(string, "undo") == 0)
  {
    int success = pop_save(SAVE_GAME, 0);

    if(success != 0)
    {
      if(seen_save_undo) store(success);
      else               screen_print("[undone]\n\n>");

      interrupt_reset();
      return NULL;
    }
    else
    {
      screen_print("[no save found, unable to undo]");
    }
  }
  else if(unicmp(string, "scripton") == 0)
  {
    if(output_stream(OSTREAM_SCRIPT, 0)) screen_print("[transcripting on]");
    else                                 screen_print("[transcripting failed]");
  }
  else if(unicmp(string, "scriptoff") == 0)
  {
    output_stream(-OSTREAM_SCRIPT, 0);
    screen_print("[transcripting off]");
  }
  else if(unicmp(string, "recon") == 0)
  {
    if(output_stream(OSTREAM_RECORD, 0)) screen_print("[command recording on]");
    else                                 screen_print("[command recording failed]");
  }
  else if(unicmp(string, "recoff") == 0)
  {
    output_stream(-OSTREAM_RECORD, 0);
    screen_print("[command recording off]");
  }
  else if(unicmp(string, "replay") == 0)
  {
    if(input_stream(ISTREAM_FILE)) screen_print("[replaying commands]");
    else                           screen_print("[replaying commands failed]");
  }
  else if(unicmp(string, "save") == 0)
  {
    if(in_interrupt())
    {
      screen_print("[cannot call /save while in an interrupt]");
    }
    else
    {
      uint32_t tmp = pc;

      /* pc is currently set to the next instruction, but the restore
       * needs to come back to *this* instruction; so temporarily set
       * pc back before saving.
       */
      pc = read_pc;
      if(do_save(1)) screen_print("[saved]");
      else           screen_print("[save failed]");
      pc = tmp;
    }
  }
  else if(unicmp(string, "restore") == 0)
  {
    if(do_restore(1))
    {
      screen_print("[restored]\n\n>");
      interrupt_reset();
      return NULL;
    }
    else
    {
      screen_print("[restore failed]");
    }
  }
  else if(unicmp(string, "ps") == 0 || unicmp(string, "ps ") == 0)
  {
    try_user_save(NULL);
  }
  else if(unincmp(string, "ps ", 3) == 0)
  {
    size_t len = 0;

    while(string[len + 3] != 0) len++;

    char desc[len + 1];

    for(size_t i = 0; i < len + 1; i++)
    {
      uint32_t c = string[i + 3];

      if(c == 0 || (c >= 32 && c <= 126)) desc[i] = c;
      else                                desc[i] = '?';
    }

    try_user_save(desc);
  }
  else if(unicmp(string, "pop") == 0 || unicmp(string, "pop ") == 0)
  {
    try_user_restore(0);
  }
  else if(unincmp(string, "pop ", 4) == 0)
  {
    long saveno;
    int valid;

    saveno = unito32(&string[4], 10, &valid);
    if(!valid || saveno < 1)
    {
      screen_print("[invalid index]");
      return string;
    }

    try_user_restore(saveno - 1);
  }
  else if(unicmp(string, "ls") == 0)
  {
    list_saves(SAVE_USER, screen_print);
  }
  else if(unicmp(string, "status") == 0)
  {
    if(zversion > 3)
    {
      screen_print("/status is only available in V1, V2, and V3");
    }
    else
    {
      int first = variable(0x11), second = variable(0x12);
      char status[64];

      print_object(variable(0x10), meta_status_putc);

      screen_print("\n");

      if(status_is_time())
      {
        snprintf(status, sizeof status, "Time: %d:%02d%s", (first + 11) % 12 + 1, second, first < 12 ? "am" : "pm");
      }
      else
      {
        snprintf(status, sizeof status, "Score: %d\nMoves: %d", first, second);
      }

      screen_print(status);
    }
  }
  else if(unicmp(string, "disable") == 0)
  {
    options.disable_meta_commands = 1;
    screen_print("[meta commands disabled]");
  }
  else if(unincmp(string, "say ", 4) == 0)
  {
    return string + 4;
  }
  else if(unicmp(string, "say") == 0)
  {
    screen_print("[missing command]");
  }
  else if(unincmp(string, "debug ", 6) == 0)
  {
    meta_debug(string + 6);
  }
  else if(unicmp(string, "debug") == 0)
  {
    meta_debug_help();
  }
  else if(unicmp(string, "help") == 0)
  {
    screen_print(
        "The following commands are provided by the interpreter, not the game:\n\n"
        "/undo: undo a turn\n"
        "/scripton: start a transcript\n"
        "/scriptoff: stop a transcript\n"
        "/recon: start a command record\n"
        "/recoff: stop a command record\n"
        "/replay: replay a command record\n"
        "/save: save the game\n"
        "/restore: restore a game saved by /save\n"
        "/ps: add a new in-memory save state\n"
        "/ps [description]: add a new in-memory save state called [description]\n"
        "/pop: restore the last-saved in-memory state\n"
        "/pop [n]: restore the specified in-memory state\n"
        "/ls: list all in-memory save states\n"
        "/status: display the status line (V1, V2, and V3 games only)\n"
        "/disable: disable these commands for the rest of this session\n"
        "/say [command]: pretend like [command] was typed"
        );
  }
  else
  {
    screen_print("[unknown command]");
  }

  return string;
}
