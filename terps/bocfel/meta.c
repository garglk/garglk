/*-
 * Copyright 2013-2015 Chris Spiegel.
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
#include <stdbool.h>
#include <limits.h>

#include "meta.h"
#include "memory.h"
#include "objects.h"
#include "process.h"
#include "screen.h"
#include "stack.h"
#include "unicode.h"
#include "zterp.h"

#define ISDIGIT(c)	((c) >= '0' && (c) <= '9')
#define ISXDIGIT(c)	(ISDIGIT(c) || ((c) >= 'a' && (c) <= 'f'))

static long parseint(const char *s, int base, bool *valid)
{
  long ret;
  char *endptr;

  ret = strtol(s, &endptr, base);
  *valid = endptr != s && *endptr == 0;

  return ret;
}

static void try_user_save(const char *desc)
{
  if(in_interrupt())
  {
    screen_puts("[cannot save while in an interrupt]");
  }
  else
  {
    if(push_save(SAVE_USER, current_instruction, desc)) screen_puts("[save pushed]");
    else                                                screen_puts("[save failed]");
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
    screen_puts("[restore failed]");
  }
}

static void try_user_drop(long i)
{
  if(drop_save(SAVE_USER, i + 1)) screen_puts("[dropped]");
  else                            screen_puts("[drop failed]");
}

static void meta_status_putc(uint8_t c)
{
  screen_print((char[]){c, 0});
}

static uint8_t *debug_change_memory;
static bool debug_change_valid[UINT16_MAX + 1];

static void meta_debug_change_start(void)
{
  free(debug_change_memory);
  debug_change_memory = malloc(header.static_start);
  if(debug_change_memory == NULL)
  {
    screen_puts("[cannot allocate memory]");
  }
  else
  {
    memcpy(debug_change_memory, memory, header.static_start);
    for(size_t addr = 0; addr < sizeof debug_change_valid / sizeof *debug_change_valid; addr++) debug_change_valid[addr] = true;
    screen_puts("[debug change reset]");
  }
}

static void meta_debug_change_inc_dec(const char *string)
{
  if(debug_change_memory == NULL)
  {
    screen_puts("[debug change not started]");
  }
  else
  {
    bool saw_change = false;

    for(uint16_t addr = 0; addr < header.static_start - 2; addr++)
    {
      if(debug_change_valid[addr])
      {
        int16_t new = as_signed(word(addr));
        int16_t old = as_signed((debug_change_memory[addr] << 8) | debug_change_memory[addr + 1]);

#define CMP(a, b) (strcmp(string, "inc") == 0 ? a > b : a < b)
        if(CMP(new, old))
#undef CMP
        {
          if(is_global(addr) || !in_globals(addr))
          {
            screen_printf("%s: %ld -> %ld\n", addrstring(addr), (long)old, (long)new);
            saw_change = true;
          }
        }
        else
        {
          debug_change_valid[addr] = false;
        }
      }
    }

    if(!saw_change) screen_puts("[no changes]");

    memcpy(debug_change_memory, memory, header.static_start);
  }
}

static bool meta_debug_change(const char *string)
{
  while(*string == ' ') string++;

  if(strcmp(string, "start") == 0)
  {
    meta_debug_change_start();
  }
  else if(strcmp(string, "inc") == 0 || strcmp(string, "dec") == 0)
  {
    meta_debug_change_inc_dec(string);
  }
  else
  {
    return false;
  }

  return true;
}

static bool meta_debug_scan(const char *string)
{
  static bool debug_scan_invalid_addr[UINT16_MAX + 1];

  while(*string == ' ') string++;

  if(strcmp(string, "start") == 0)
  {
    for(size_t addr = 0; addr < sizeof debug_scan_invalid_addr / sizeof *debug_scan_invalid_addr; addr++) debug_scan_invalid_addr[addr] = false;
    screen_puts("[debug scan reset]");
  }
  else if(strcmp(string, "show") == 0)
  {
    for(size_t addr = 0; addr < header.static_start - 2; addr++)
    {
      if(!debug_scan_invalid_addr[addr])
      {
        if(is_global(addr) || !in_globals(addr))
        {
          screen_puts(addrstring(addr));
        }
      }
    }
  }
  else if(ISDIGIT(string[0]) || (string[0] == '-' && ISDIGIT(string[1])))
  {
    long value;
    bool valid;

    value = parseint(string, 0, &valid);
    if(!valid) return false;

    if(value < -0x8000 || value > 0xffff)
    {
      screen_puts("[value is outside the range of a 16-bit integer and will never be found]");
    }
    else
    {
      size_t count = 0;

      if(value < 0) value += 0x10000;

      for(size_t addr = 0; addr < header.static_start - 2; addr++)
      {
        if(word(addr) != value) debug_scan_invalid_addr[addr] = true;
        if(!debug_scan_invalid_addr[addr]) count++;
      }

      screen_printf("[%zu location%s]\n", count, count == 1 ? "" : "s");
    }
  }
  else
  {
    return false;
  }

  return true;
}

static long parse_address(const char *string, bool *valid)
{
  long addr;

  if(string[0] == 'G' && ISXDIGIT(string[1]) && ISXDIGIT(string[2]) && string[3] == 0)
  {
    addr = parseint(string + 1, 16, valid);
    if(addr < 0 || addr > 239) *valid = 0;
    addr = header.globals + (2 * addr);
  }
  else
  {
    addr = parseint(string, 16, valid);
  }

  return addr;
}

static bool validate_address(long addr, bool print)
{
  if(addr < 0 || addr > memory_size - 2)
  {
    if(print) screen_printf("[address out of range: must be [0, 0x%lx]]\n", (unsigned long)memory_size - 2);
    return false;
  }

  return true;
}

static bool meta_debug_print(const char *string)
{
  long addr;
  bool valid;

  while(*string == ' ') string++;

  addr = parse_address(string, &valid);

  if(!valid) return false;
  if(!validate_address(addr, true)) return true;

  screen_printf("%ld (0x%lx)\n", (long)as_signed(word(addr)), (unsigned long)word(addr));

  return true;
}

#ifndef ZTERP_NO_CHEAT
/* The index into these arrays is the address to freeze.
 * The first array tracks whether the address is frozen, while the
 * second holds the frozen value.
 */
static bool     freeze_cheat[UINT16_MAX + 1];
static uint16_t freeze_val  [UINT16_MAX + 1];

bool cheat_add(char *how, bool print)
{
  char *type, *addrstr, *valstr;

  type = strtok(how, ":");
  addrstr = strtok(NULL, ":");
  valstr = strtok(NULL, "");

  if(type == NULL || addrstr == NULL || valstr == NULL) return false;

  if(strcmp(type, "freeze") == 0 || strcmp(type, "freezew") == 0)
  {
    long addr;
    long value;
    bool valid;

    addr = parse_address(addrstr, &valid);
    if(!valid) return false;
    if(!validate_address(addr, print)) return true;

    value = parseint(valstr, 0, &valid);
    if(!valid) return false;

    if(value < 0 || value > UINT16_MAX)
    {
      if(print) screen_puts("[values must be in the range [0, 65535]]");
      return true;
    }

    freeze_cheat[addr] = true;
    freeze_val  [addr] = value;
  }
  else
  {
    return false;
  }

  if(print) screen_puts("[frozen]");

  return true;
}

static bool cheat_remove(uint16_t addr)
{
  if(!freeze_cheat[addr]) return false;

  freeze_cheat[addr] = false;
  return true;
}

bool cheat_find_freeze(uint32_t addr, uint16_t *val)
{
  if(addr > UINT16_MAX || !freeze_cheat[addr]) return false;

  *val = freeze_val[addr];

  return true;
}

static bool meta_debug_freeze(char *string)
{
  char *addrstr, *valstr;
  char cheat[64];

  addrstr = strtok(string, " ");
  if(addrstr == NULL) return false;
  valstr = strtok(NULL, "");
  if(valstr == NULL) return false;

  snprintf(cheat, sizeof cheat, "freeze:%s:%s", addrstr, valstr);

  return cheat_add(cheat, true);
}

static bool meta_debug_unfreeze(const char *string)
{
  long addr;
  bool valid;

  while(*string == ' ') string++;

  addr = parse_address(string, &valid);
  if(!valid) return false;
  if(!validate_address(addr, true)) return true;

  if(cheat_remove(addr)) screen_puts("[unfrozen]");
  else                   screen_puts("[address not frozen]");

  return true;
}

static bool meta_debug_show_freeze(void)
{
  bool any_frozen = false;

  for(size_t addr = 0; addr < sizeof freeze_cheat / sizeof *freeze_cheat; addr++)
  {
    if(freeze_cheat[addr])
    {
      any_frozen = true;
      screen_printf("%s: %lu\n", addrstring(addr), (unsigned long)freeze_val[addr]);
    }
  }

  if(!any_frozen) screen_puts("[no frozen values]");

  return true;
}
#endif

#ifndef ZTERP_NO_WATCHPOINTS
static bool watch_addresses[UINT16_MAX + 1];

static void watch_add(uint16_t addr)
{
  watch_addresses[addr] = true;
}

static void watch_all(void)
{
  for(size_t addr = 0; addr < sizeof watch_addresses / sizeof *watch_addresses; addr++) watch_addresses[addr] = true;
}

static bool watch_remove(uint16_t addr)
{
  if(watch_addresses[addr])
  {
    watch_addresses[addr] = false;
    return true;
  }
  else
  {
    return false;
  }
}

static void watch_none(void)
{
  for(size_t addr = 0; addr < sizeof watch_addresses / sizeof *watch_addresses; addr++) watch_addresses[addr] = false;
}

void watch_check(uint16_t addr, unsigned long oldval, unsigned long newval)
{
  if(watch_addresses[addr] && oldval != newval)
  {
    screen_printf("[%s changed: %lu -> %lu (pc = 0x%lx)]\n", addrstring(addr), oldval, newval, current_instruction);
  }
}

static bool meta_debug_watch_helper(const char *string, bool do_watch)
{
  while(*string == ' ') string++;

  if(strcmp(string, "all") == 0)
  {
    if(do_watch)
    {
      watch_all();
      screen_puts("[watching all addresses for changes]");
    }
    else
    {
      watch_none();
      screen_puts("[not watching any addresses for changes]");
    }
  }
  else
  {
    long addr;
    bool valid;

    addr = parse_address(string, &valid);

    if(!valid) return false;
    if(!validate_address(addr, true)) return true;

    if(do_watch)
    {
      watch_add(addr);
      screen_printf("[watching %s for changes]\n", addrstring(addr));
    }
    else
    {
      if(watch_remove(addr)) screen_printf("[no longer watching %lx for changes]\n", addr);
      else                   screen_printf("[%lx is not currently being watched]\n", addr);
    }
  }

  return true;
}

static bool meta_debug_watch(const char *string)
{
  return meta_debug_watch_helper(string, true);
}

static bool meta_debug_unwatch(const char *string)
{
  return meta_debug_watch_helper(string, false);
}

static bool meta_debug_show_watch(void)
{
  bool any_watched = false;

  for(size_t addr = 0; addr < sizeof watch_addresses / sizeof *watch_addresses; addr++)
  {
    if(watch_addresses[addr])
    {
      any_watched = true;
      screen_puts(addrstring(addr));
    }
  }

  if(!any_watched) screen_puts("[no watched values]");

  return true;
}
#endif

static void meta_debug_help(void)
{
  screen_print(
      "Debug commands are accessed as \"/debug [command]\". Commands are:\n\n"
      "change start: restart change tracking\n"
      "change dec: display all words which have been decremented since the last check\n"
      "change inc: display all words which have been incremented since the last check\n"
      "scan start: restart scan tracking\n"
      "scan [n]: update scan list with all words equal to [n]; if [n] starts with 0x it is hexadecimal, 0 it is octal, decimal otherwise\n"
      "scan show: print all locations matching scan criteria\n"
      "print [address]: print the word at address [address]\n"
#ifndef ZTERP_NO_CHEAT
      "freeze [address] [value]: freeze the 16-bit [value] at [address]; [value] can be decimal, hexadecimal, or octal, with a leading 0x signifying hexadecimal and a leading 0 signifying octal\n"
      "unfreeze [address]: unfreeze the value currently frozen at [address]\n"
      "show_freeze: show all frozen values\n"
#endif
#ifndef ZTERP_NO_WATCHPOINTS
      "watch [address]: report any changes to the word at [address]\n"
      "watch all: report any changes to words at all addresses\n"
      "unwatch [address]: stop watching [address]\n"
      "unwatch all: stop watching all adddresses\n"
      "show_watch: show all watched-for addresses\n"
#endif
      "\nAddresses are either absolute, specified in hexadecimal with an optional leading 0x, or global variables. Global variables have the syntax Gxx, where xx is a hexadecimal value in the range [00, ef], corresponding to global variables 0 to 239.\n"
      );
}

static void meta_debug(char *string)
{
  bool ok = false;

  if(strncmp(string, "change ", 7) == 0)
  {
    ok = meta_debug_change(string + 7);
  }
  else if(strncmp(string, "scan ", 5) == 0)
  {
    ok = meta_debug_scan(string + 5);
  }
  else if(strncmp(string, "print ", 6) == 0)
  {
    ok = meta_debug_print(string + 6);
  }
#ifndef ZTERP_NO_CHEAT
  else if(strncmp(string, "freeze ", 7) == 0)
  {
    ok = meta_debug_freeze(string + 7);
  }
  else if(strncmp(string, "unfreeze ", 9) == 0)
  {
    ok = meta_debug_unfreeze(string + 9);
  }
  else if(strcmp(string, "show_freeze") == 0)
  {
    ok = meta_debug_show_freeze();
  }
#endif
#ifndef ZTERP_NO_WATCHPOINTS
  else if(strncmp(string, "watch ", 6) == 0)
  {
    ok = meta_debug_watch(string + 6);
  }
  else if(strncmp(string, "unwatch ", 8) == 0)
  {
    ok = meta_debug_unwatch(string + 8);
  }
  else if(strcmp(string, "show_watch") == 0)
  {
    ok = meta_debug_show_watch();
  }
#endif

  if(!ok) meta_debug_help();
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
const uint32_t *handle_meta_command(const uint32_t *string, const uint8_t len)
{
  const char *command;
  char converted[len + 1], *rest;

  for(size_t i = 0; i < len + 1; i++)
  {
    uint32_t c = string[i];

    if(c > 255) c = LATIN1_QUESTIONMARK;
    converted[i] = c;
  }

  for(int i = (int)len - 1; i >= 0 && converted[i] == ' '; i--) converted[i] = 0;

  command = strtok(converted, " ");
  rest = strtok(NULL, "");
  if(command == NULL) command = "";
  if(rest == NULL) rest = &converted[len];

#define ZEROARG(name) (strcmp(command, name) == 0) if(rest[0] != 0) { screen_printf("[/%s takes no arguments]\n", name); } else

  if ZEROARG("undo")
  {
    if(pop_save(SAVE_GAME, 0))
    {
      if(seen_save_undo) store(2);
      else               screen_print("[undone]\n\n>");

      interrupt_reset();
      return NULL;
    }
    else
    {
      screen_puts("[no save found, unable to undo]");
    }
  }
  else if ZEROARG("scripton")
  {
    if(output_stream(OSTREAM_SCRIPT, 0)) screen_puts("[transcripting on]");
    else                                 screen_puts("[transcripting failed]");
  }
  else if ZEROARG("scriptoff")
  {
    output_stream(-OSTREAM_SCRIPT, 0);
    screen_puts("[transcripting off]");
  }
  else if ZEROARG("recon")
  {
    if(output_stream(OSTREAM_RECORD, 0)) screen_puts("[command recording on]");
    else                                 screen_puts("[command recording failed]");
  }
  else if ZEROARG("recoff")
  {
    output_stream(-OSTREAM_RECORD, 0);
    screen_puts("[command recording off]");
  }
  else if ZEROARG("replay")
  {
    if(input_stream(ISTREAM_FILE)) screen_puts("[replaying commands]");
    else                           screen_puts("[replaying commands failed]");
  }
  else if ZEROARG("save")
  {
    if(in_interrupt())
    {
      screen_puts("[cannot call /save while in an interrupt]");
    }
    else
    {
      uint32_t tmp = pc;

      /* pc is currently set to the next instruction, but the restore
       * needs to come back to *this* instruction; so temporarily set
       * pc back before saving.
       */
      pc = current_instruction;
      if(do_save(true)) screen_puts("[saved]");
      else              screen_puts("[save failed]");
      pc = tmp;
    }
  }
  else if ZEROARG("restore")
  {
    if(do_restore(true))
    {
      screen_print("[restored]\n\n>");
      interrupt_reset();
      return NULL;
    }
    else
    {
      screen_puts("[restore failed]");
    }
  }
  else if(strcmp(command, "ps") == 0)
  {
    if(rest[0] == 0)
    {
      try_user_save(NULL);
    }
    else
    {
      char desc[len + 1];
      size_t i;

      for(i = 0; rest[i] != 0; i++)
      {
        char c = rest[i];

        if(c == 0 || (c >= 32 && c <= 126)) desc[i] = c;
        else                                desc[i] = '?';
      }

      desc[i] = 0;

      try_user_save(desc);
    }
  }
  else if(strcmp(command, "pop") == 0 || strcmp(command, "drop") == 0)
  {
    void (*restore_or_drop)(long) = strcmp(command, "pop") == 0 ? try_user_restore : try_user_drop;

    if(restore_or_drop == try_user_drop && strcmp(rest, "all") == 0)
    {
      while(drop_save(SAVE_USER, 1))
      {
      }

      screen_puts("[all saves dropped]");
    }
    else if(rest[0] == 0)
    {
      restore_or_drop(0);
    }
    else
    {
      long saveno;
      bool valid;

      saveno = parseint(rest, 10, &valid);
      if(!valid || saveno < 1)
      {
        screen_puts("[invalid index]");
        return string;
      }

      restore_or_drop(saveno - 1);
    }
  }
  else if ZEROARG("ls")
  {
    list_saves(SAVE_USER, screen_puts);
  }
  else if ZEROARG("status")
  {
    if(zversion > 3)
    {
      screen_puts("[/status is only available in V1, V2, and V3]");
    }
    else
    {
      int first = variable(0x11), second = variable(0x12);

      print_object(variable(0x10), meta_status_putc);

      screen_print("\n");

      if(status_is_time())
      {
        screen_printf("Time: %d:%02d%s\n", (first + 11) % 12 + 1, second, first < 12 ? "am" : "pm");
      }
      else
      {
        screen_printf("Score: %d\nMoves: %d\n", first, second);
      }
    }
  }
  else if ZEROARG("disable")
  {
    options.disable_meta_commands = true;
    screen_puts("[meta commands disabled]");
  }
  else if(strcmp(command, "say") == 0)
  {
    return &string[rest - converted];
  }
  else if(strcmp(command, "debug") == 0)
  {
    meta_debug(rest);
  }
  else
  {
    if(strcmp(command, "help") != 0) screen_printf("Unknown command: /%s\n\n", command);

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
        "/drop: drop the last-saved in-memory state\n"
        "/drop [n]: drop the specified in-memory state\n"
        "/drop all: drop all in-memory states\n"
        "/ls: list all in-memory save states\n"
        "/status: display the status line (V1, V2, and V3 games only)\n"
        "/disable: disable these commands for the rest of this session\n"
        "/say [command]: pretend like [command] was typed\n"
        "/debug [...]: perform a debugging operation\n"
        );
  }

  return string;
}
