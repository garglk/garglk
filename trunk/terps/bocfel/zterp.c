/*-
 * Copyright 2009-2011 Chris Spiegel.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <limits.h>

#include "zterp.h"
#include "blorb.h"
#include "branch.h"
#include "io.h"
#include "memory.h"
#include "osdep.h"
#include "process.h"
#include "random.h"
#include "screen.h"
#include "stack.h"
#include "unicode.h"
#include "util.h"

#ifdef ZTERP_GLK
#include <glk.h>
#endif

#ifndef LINE_MAX
#define LINE_MAX	2048
#endif
#ifndef PATH_MAX
#define PATH_MAX	4096
#endif

#define ZTERP_VERSION	"0.5.2"

const char *game_file;
struct options options = {
  .eval_stack_size = DEFAULT_STACK_SIZE,
  .call_stack_size = DEFAULT_CALL_DEPTH,
  .disable_color = 0,
  .disable_config = 0,
  .disable_timed = 0,
  .enable_escape = 0,
  .escape_string = NULL,
  .disable_fixed = 0,
  .disable_graphics_font = 0,
  .enable_alt_graphics = 0,
  .show_id = 0,
  .disable_term_keys = 0,
  .disable_utf8 = 0,
  .force_utf8 = 0,
  .int_number = 1, /* DEC */
  .int_version = 'C',
  .replay_on = 0,
  .replay_name = NULL,
  .script_on = 0,
  .script_name = NULL,
  .transcript_on = 0,
  .transcript_name = NULL,
  .max_saves = 10,
  .disable_undo_compression = 0,
  .show_version = 0,
  .disable_abbreviations = 0,
  .enable_censorship = 0,
  .overwrite_transcript = 0,
  .random_seed = -1,
};

static char story_id[64];

static void process_story(void);

uint32_t pc;

/* zversion stores the Z-machine version of the story: 1–6.
 *
 * Z-machine versions 7 and 8 are identical to version 5 but for a
 * couple of tiny details.  They are thus classified as version 5.
 *
 * zwhich stores the actual version (1–8) for the few rare times where
 * this knowledge is necessary.
 */
int zversion;
static int zwhich;

struct header header;

struct
{
  zterp_io *io;
  long offset;
} story;

/* The null character in the alphabet table does not actually signify a
 * null character: character 6 from A2 is special in that it specifies
 * that the next two characters form a 10-bit ZSCII character (§3.4).
 * The code that uses the alphabet table will step around this character
 * when necessary, so it’s safe to use a null character here to mean
 * “nothing”.
 */
uint8_t atable[26 * 3] =
{
  /* A0 */
  'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
  'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',

  /* A1 */
  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
  'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',

  /* A2 */
  0x0, 0xd, '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '.',
  ',', '!', '?', '_', '#', '\'','"', '/', '\\','-', ':', '(', ')',
};

void znop(void)
{
}

void zquit(void)
{
  running = 0;
}

void zverify(void)
{
  uint16_t checksum = 0;
  uint32_t remaining = header.file_length - 0x40;

  if(zterp_io_seek(story.io, story.offset + 0x40, SEEK_SET) == -1)
  {
    branch_if(0);
    return;
  }

  while(remaining != 0)
  {
    uint8_t buf[8192];
    uint32_t to_read = remaining < sizeof buf ? remaining : sizeof buf;

    if(zterp_io_read(story.io, buf, to_read) != to_read)
    {
      branch_if(0);
      return;
    }

    for(uint32_t i = 0; i < to_read; i++) checksum += buf[i];

    remaining -= to_read;
  }

  branch_if(checksum == header.checksum);
}

uint32_t unpack(uint16_t addr, int string)
{
  switch(zwhich)
  {
    case 1: case 2: case 3:
      return addr * 2UL;
    case 4: case 5:
      return addr * 4UL;
    case 6: case 7:
      return (addr * 4UL) + (string ? header.S_O : header.R_O);
    case 8:
      return addr * 8UL;
    default:
      die("unhandled z-machine version: %d", zwhich);
  }
}

void store(uint16_t v)
{
  store_variable(BYTE(pc++), v);
}

void zrestart(void)
{
  /* §6.1.3: Flags2 is preserved on a restart. */
  uint16_t flags2 = WORD(0x10);

  process_story();

  user_store_word(0x10, flags2);
}

void zsave5(void)
{
  zterp_io *savefile;
  size_t n;

  if(znargs == 0)
  {
    zsave();
    return;
  }

  /* This should be able to suggest a filename, but GLK doesn’t support that. */
  savefile = zterp_io_open(NULL, ZTERP_IO_WRONLY | ZTERP_IO_SAVE);
  if(savefile == NULL)
  {
    store(0);
    return;
  }

  n = zterp_io_write(savefile, &memory[zargs[0]], zargs[1]);

  zterp_io_close(savefile);

  store(n == zargs[1]);
}

void zrestore5(void)
{
  zterp_io *savefile;
  size_t n;

  if(znargs == 0)
  {
    zrestore();
    return;
  }

  savefile = zterp_io_open(NULL, ZTERP_IO_RDONLY | ZTERP_IO_SAVE);
  if(savefile == NULL)
  {
    store(0);
    return;
  }

  n = zterp_io_read(savefile, &memory[zargs[0]], zargs[1]);

  zterp_io_close(savefile);

  store(n == zargs[1]);
}

void zpiracy(void)
{
  branch_if(1);
}

/* Find a story ID roughly in the form of an IFID according to §2.2.2.1
 * of draft 7 of the Treaty of Babel.
 *
 * This does not add a ZCODE- prefix, and will not search for a manually
 * created IFID.
 */
static void find_id(void)
{
  char serial[] = "------";

  for(int i = 0; i < 6; i++)
  {
    /* isalnum() cannot be used because it is locale-aware, and this
     * must only check for A–Z, a–z, and 0–9.  Because ASCII (or a
     * compatible charset) is required, testing against 'a', 'z', etc.
     * is OK.
     */
#define ALNUM(c) ( ((c) >= 'a' && (c) <= 'z') || ((c) >= 'A' && (c) <= 'Z') || ((c) >= '0' && (c) <= '9') )
    if(ALNUM(header.serial[i])) serial[i] = header.serial[i];
#undef ALNUM
  }

  if(strchr("012345679", serial[0]) != NULL && strcmp(serial, "000000") != 0)
  {
    snprintf(story_id, sizeof story_id, "%d-%s-%04x", header.release, serial, (unsigned)header.checksum);
  }
  else
  {
    snprintf(story_id, sizeof story_id, "%d-%s", header.release, serial);
  }
}

int is_story(const char *id)
{
  return strcmp(story_id, id) == 0;
}

#ifndef ZTERP_NO_CHEAT
/* The index into these arrays is the address to freeze.
 * The first array tracks whether the address is frozen, while the
 * second holds the frozen value.
 */
static char     freezew_cheat[UINT16_MAX + 1];
static uint16_t freezew_val  [UINT16_MAX + 1];

static void cheat(char *how)
{
  char *p;

  p = strtok(how, ":");
  if(p == NULL) return;

  if(strcmp(p, "freezew") == 0)
  {
    uint16_t addr;

    p = strtok(NULL, ":");
    if(p == NULL) return;
    addr = strtoul(p, NULL, 16);
    p = strtok(NULL, ":");
    if(p == NULL) return;
    freezew_cheat[addr] = 1;
    freezew_val  [addr] = strtoul(p, NULL, 10);
  }
}

int cheat_find_freezew(uint32_t addr, uint16_t *val)
{
  if(addr > UINT16_MAX || !freezew_cheat[addr]) return 0;

  *val = freezew_val[addr];

  return 1;
}
#endif

static void read_config(void)
{
  FILE *fp;
  char file[PATH_MAX + 1];
  char line[LINE_MAX];
  char *key, *val, *p;
  long n;
  int story_matches = 1;

  zterp_os_rcfile(file, sizeof file);

  fp = fopen(file, "r");
  if(fp == NULL) return;

  while(fgets(line, sizeof line, fp) != NULL)
  {
    line[strcspn(line, "#\n")] = 0;
    if(line[0] == 0) continue;

    if(line[0] == '[')
    {
      p = strrchr(line, ']');
      if(p != NULL && p[1] == 0)
      {
        *p = 0;

        story_matches = 0;
        for(p = strtok(line + 1, " ,"); p != NULL; p = strtok(NULL, " ,"))
        {
          if(is_story(p)) story_matches = 1;
        }
      }

      continue;
    }

    if(!story_matches) continue;

    key = strtok(line, " \t=");
    if(key == NULL) continue;
    val = strtok(NULL, "=");
    if(val == NULL) continue;

    /* Trim whitespace. */
    while(isspace((unsigned char)*val)) val++;
    if(*val == 0) continue;
    p = val + strlen(val) - 1;
    while(isspace((unsigned char)*p)) *p-- = 0;

    n = strtol(val, NULL, 10);

#define BOOL(name)	else if(strcmp(key, #name) == 0) options.name = (n != 0)
#define NUMBER(name)	else if(strcmp(key, #name) == 0) options.name = n
#define STRING(name)	else if(strcmp(key, #name) == 0) do { free(options.name); options.name = xstrdup(val); } while(0)
#define CHAR(name)	else if(strcmp(key, #name) == 0) options.name = val[0]
#ifdef GARGLK
#define COLOR(name, num)else if(strcmp(key, "color_" #name) == 0) update_color(num, strtol(val, NULL, 16))
#else
#define COLOR(name, num)else if(0)
#endif

    if(0);

    NUMBER(eval_stack_size);
    NUMBER(call_stack_size);
    BOOL  (disable_color);
    BOOL  (disable_timed);
    BOOL  (enable_escape);
    STRING(escape_string);
    BOOL  (disable_fixed);
    BOOL  (disable_graphics_font);
    BOOL  (enable_alt_graphics);
    BOOL  (disable_term_keys);
    BOOL  (disable_utf8);
    BOOL  (force_utf8);
    NUMBER(max_saves);
    BOOL  (disable_undo_compression);
    NUMBER(int_number);
    CHAR  (int_version);
    BOOL  (replay_on);
    STRING(replay_name);
    BOOL  (script_on);
    STRING(script_name);
    BOOL  (transcript_on);
    STRING(transcript_name);
    BOOL  (disable_abbreviations);
    BOOL  (enable_censorship);
    BOOL  (overwrite_transcript);
    NUMBER(random_seed);

    COLOR(black,   2);
    COLOR(red,     3);
    COLOR(green,   4);
    COLOR(yellow,  5);
    COLOR(blue,    6);
    COLOR(magenta, 7);
    COLOR(cyan,    8);
    COLOR(white,   9);

#ifndef ZTERP_NO_CHEAT
    else if(strcmp(key, "cheat") == 0) cheat(val);
#endif

#undef BOOL
#undef NUMBER
#undef STRING
#undef CHAR
#undef COLOR
  }

  fclose(fp);
}

static int have_statuswin = 0;
static int have_upperwin  = 0;

/* Various parts of the header (those marked “Rst” in §11) should be
 * updated by the interpreter.  This function does that.  This is also
 * used when restoring, because the save file might have come from an
 * interpreter with vastly different settings.
 */
void write_header(void)
{
  uint8_t flags1;

  flags1 = BYTE(0x01);

  if(zversion == 3)
  {
    flags1 |= FLAGS1_NOSTATUS;
    flags1 &= ~(FLAGS1_SCREENSPLIT | FLAGS1_VARIABLE);

#ifdef GARGLK
    /* Assume that if Gargoyle is being used, the default font is not fixed. */
    flags1 |= FLAGS1_VARIABLE;
#endif

    if(have_statuswin)            flags1 &= ~FLAGS1_NOSTATUS;
    if(have_upperwin)             flags1 |= FLAGS1_SCREENSPLIT;
    if(options.enable_censorship) flags1 |= FLAGS1_CENSOR;
  }
  else if(zversion >= 4)
  {
    flags1 |= (FLAGS1_BOLD | FLAGS1_ITALIC | FLAGS1_FIXED);
    flags1 &= ~FLAGS1_TIMED;

    if(zversion >= 5) flags1 &= ~FLAGS1_COLORS;

    if(zversion == 6)
    {
      flags1 &= ~FLAGS1_PICTURES;
      flags1 &= ~FLAGS1_SOUND;
    }

#ifdef ZTERP_GLK
    if(glk_gestalt(gestalt_Timer, 0)) flags1 |= FLAGS1_TIMED;
#ifdef GARGLK
    if(zversion >= 5) flags1 |= FLAGS1_COLORS;
#endif
#else
    if(!zterp_os_have_style(STYLE_BOLD)) flags1 &= ~FLAGS1_BOLD;
    if(!zterp_os_have_style(STYLE_ITALIC)) flags1 &= ~FLAGS1_ITALIC;
    if(zversion >= 5 && zterp_os_have_colors()) flags1 |= FLAGS1_COLORS;
#endif

    if(zversion >= 5 && options.disable_color) flags1 &= ~FLAGS1_COLORS;
    if(options.disable_timed) flags1 &= ~FLAGS1_TIMED;
    if(options.disable_fixed) flags1 &= ~FLAGS1_FIXED;
  }

  STORE_BYTE(0x01, flags1);

  if(zversion >= 5)
  {
    uint16_t flags2 = WORD(0x10);

    flags2 &= ~(FLAGS2_PICTURES | FLAGS2_SOUND | FLAGS2_MOUSE);
    if(zversion >= 6) flags2 &= ~FLAGS2_MENUS;

    if(options.max_saves == 0) flags2 &= ~FLAGS2_UNDO;

    STORE_WORD(0x10, flags2);
  }

  if(zversion >= 4)
  {
    unsigned int width, height;

    /* Interpreter number & version. */
    if(options.int_number < 1 || options.int_number > 11) options.int_number = 1; /* DEC */
    STORE_BYTE(0x1e, options.int_number);
    STORE_BYTE(0x1f, options.int_version);

    get_window_size(&width, &height);

    /* Screen height and width.
     * A height of 255 means infinite, so cap at 254.
     */
    STORE_BYTE(0x20, height > 254 ? 254 : height);
    STORE_BYTE(0x21, width > 255 ? 255 : width);

    if(zversion >= 5)
    {
      /* Screen width and height in units. */
      STORE_WORD(0x22, width > UINT16_MAX ? UINT16_MAX : width);
      STORE_WORD(0x24, height > UINT16_MAX ? UINT16_MAX : height);

      /* Font width and height in units. */
      STORE_BYTE(0x26, 1);
      STORE_BYTE(0x27, 1);

      /* Default background and foreground colors. */
      STORE_BYTE(0x2c, 1);
      STORE_BYTE(0x2d, 1);
    }
  }

  /* Standard revision # */
  STORE_BYTE(0x32, 1);
  STORE_BYTE(0x33, 1);
}

static void process_story(void)
{
  if(zterp_io_seek(story.io, story.offset, SEEK_SET) == -1) die("can't rewind story");

  if(zterp_io_read(story.io, memory, memory_size) != memory_size) die("can't read from story file");

  zversion =		BYTE(0x00);
#ifndef ZTERP_NO_V2
  if(zversion < 1 || zversion > 8) die("only z-code versions 1-8 are supported");
#else
  if(zversion < 3 || zversion > 8) die("only z-code versions 3-8 are supported");
#endif

  zwhich = zversion;
  if(zversion == 7 || zversion == 8) zversion = 5;

  pc =			WORD(0x06);
  if(pc >= memory_size) die("corrupted story: initial pc out of range");

  header.release =	WORD(0x02);
  header.dictionary =	WORD(0x08);
  header.objects =	WORD(0x0a);
  header.globals =	WORD(0x0c);
  header.static_start =	WORD(0x0e);
  header.abbr =		WORD(0x18);

  memcpy(header.serial, &memory[0x12], sizeof header.serial);
  
  /* There is no explicit “end of static” tag; but it must end by 0xffff
   * or the end of the story file, whichever is smaller.
   */
  header.static_end = memory_size < 0xffff ? memory_size : 0xffff;

  /* There must be at least enough room in dynamic memory for the header
   * (64 bytes) and the global variables table (480 bytes).
   */
  if(header.static_start < 544)                   die("corrupted story: dynamic memory too small (%d bytes)", (int)header.static_start);
  if(header.static_start >= memory_size)          die("corrupted story: static memory out of range");

  if(header.dictionary != 0 &&
     header.dictionary < header.static_start)     die("corrupted story: dictionary is not in static memory");

  if(header.objects < 64 ||
     header.objects >= header.static_start)       die("corrupted story: object table is not in dynamic memory");

  if(header.globals < 64 ||
     header.globals >= header.static_start - 479) die("corrupted story: global variables are not in dynamic memory");

  if(header.abbr >= memory_size)                  die("corrupted story: abbreviation table out of range");

  header.file_length =	WORD(0x1a) * (zwhich <= 3 ? 2UL : zwhich <= 5 ? 4UL : 8UL);
  if(header.file_length > memory_size)            die("story's reported size (%lu) greater than file size (%lu)", (unsigned long)header.file_length, (unsigned long)memory_size);

  header.checksum =	WORD(0x1c);

  if(zwhich == 6 || zwhich == 7)
  {
    header.R_O =	WORD(0x28) * 8UL;
    header.S_O =	WORD(0x2a) * 8UL;
  }

#ifdef GLK_MODULE_LINE_TERMINATORS
  if(!options.disable_term_keys)
  {
    if(zversion >= 5 && WORD(0x2e) != 0)
    {
      term_keys_reset();

      for(uint32_t i = WORD(0x2e); i < memory_size && memory[i] != 0; i++)
      {
        term_keys_add(memory[i]);
      }
    }
  }
#endif

  if(zversion == 1)
  {
    memcpy(&atable[26 * 2], " 0123456789.,!?_#'\"/\\<-:()", 26);
  }
  else if(zversion >= 5 && WORD(0x34) != 0)
  {
    if(WORD(0x34) + 26 * 3 >= memory_size) die("corrupted story: alphabet table out of range");

    memcpy(atable, &memory[WORD(0x34)], 26 * 3);

    /* Even with a new alphabet table, characters 6 and 7 from A2 must
     * remain the same (§3.5.5.1).
     */
    atable[52] = 0x00;
    atable[53] = 0x0d;
  }

  /* Check for a header extension table. */
  if(zversion >= 5)
  {
    uint16_t etable = WORD(0x36);

    if(etable != 0)
    {
      uint16_t nentries = user_word(etable);

      /* Unicode table. */
      if(nentries >= 3 && user_word(etable + (2 * 3)) != 0)
      {
        uint16_t utable = user_word(etable + (2 * 3));

        parse_unicode_table(utable);
      }

      /* Flags3. */
      if(nentries >= 4) STORE_WORD(etable + (2 * 4), 0);
      /* True default foreground color. */
      if(nentries >= 5) STORE_WORD(etable + (2 * 5), 0x0000);
      /* True default background color. */
      if(nentries >= 6) STORE_WORD(etable + (2 * 6), 0x7fff);
    }
  }

  if(dynamic_memory == NULL)
  {
    dynamic_memory = malloc(header.static_start);
    if(dynamic_memory == NULL) die("can't allocate memory for dynamic memory");
    memcpy(dynamic_memory, memory, header.static_start);
  }

  /* The configuration file cannot be read until the ID of the current
   * story is known, and the ID of the current story is not known until
   * the file has been processed; so do both of those here.
   */
  find_id();
  if(!options.disable_config) read_config();

  /* Most options directly set their respective variables, but a few
   * require intervention.  Delay that intervention until here so that
   * the configuration file is taken into account.
   */
  if(options.disable_utf8)
  {
#ifndef ZTERP_GLK
    /* If GLK is not being used, the ZSCII to Unicode table needs to be
     * aligned with the IO character set.
     */
    have_unicode = 0;
#endif
    use_utf8_io = 0;
  }
  if(options.force_utf8)
  {
#ifndef ZTERP_GLK
    /* See above. */
    have_unicode = 1;
#endif
    use_utf8_io = 1;
  }
  if(options.escape_string == NULL) options.escape_string = xstrdup("1m");

  /* Now that we have a Unicode table and the user’s Unicode
   * preferences, build the ZSCII to Unicode and Unicode to ZSCII
   * tables.
   */
  setup_tables();

  if(zversion <= 3) have_statuswin = create_statuswin();
  if(zversion >= 3) have_upperwin  = create_upperwin();
  
  write_header();
  /* Put everything in a clean state. */
  seed_random(0);
  init_stack();
  init_screen();

  /* Unfortunately, Beyond Zork behaves badly when the interpreter
   * number is set to DOS: it assumes that it can print out IBM PC
   * character codes and get useful results (e.g. it writes out 0x18
   * expecting an up arrow); however, if the pictures bit is set, it
   * uses the picture font like a good citizen.  Thus turn that bit on
   * when Beyond Zork is being used and the interpreter is set to DOS.
   * It might make sense to do this generally, not just for Beyond Zork;
   * but this is such a minor corner of the Z-machine that it probably
   * doesn’t matter.  For now, peg this to Beyond Zork.
   */
  if(options.int_number == 6 &&
      (is_story("47-870915") || is_story("49-870917") ||
       is_story("51-870923") || is_story("57-871221")))
  {
    STORE_WORD(0x10, WORD(0x10) | FLAGS2_PICTURES);
  }

  if(options.transcript_on)
  {
    STORE_WORD(0x10, WORD(0x10) | FLAGS2_TRANSCRIPT);
    options.transcript_on = 0;
  }

  /* If header transcript/fixed bits have been set, either by the
   * story or by the user, this will activate them.
   */
  user_store_word(0x10, WORD(0x10));

  if(options.script_on)
  {
    output_stream(OSTREAM_SCRIPT, 0);
    options.script_on = 0;
  }

  if(options.replay_on)
  {
    input_stream(ISTREAM_FILE);
    options.replay_on = 0;
  }

  if(zversion == 6)
  {
    zargs[0] = pc;
    call(0);
  }
}

#ifdef ZTERP_GLK
void glk_main(void)
#else
int main(int argc, char **argv)
#endif
{
  zterp_blorb *blorb;

#ifdef ZTERP_GLK
  if(!create_mainwin()) return;
#ifdef GLK_MODULE_UNICODE
  have_unicode = glk_gestalt(gestalt_Unicode, 0);
#endif
#else
  have_unicode = zterp_os_have_unicode();
#endif

  use_utf8_io = zterp_os_have_unicode();

#ifndef ZTERP_GLK
  if(!process_arguments(argc, argv)) exit(1);

  zterp_os_init_term();
#endif

#ifdef ZTERP_GLK
#define PRINT(s)	do { glk_put_string(s); glk_put_char(10); } while(0)
#else
#define PRINT(s)	puts(s)
#endif

  if(options.show_version)
  {
    PRINT("Bocfel " ZTERP_VERSION);
#ifdef ZTERP_NO_SAFETY_CHECKS
    PRINT("Runtime assertions disabled");
#else
    PRINT("Runtime assertions enabled");
#endif
#ifdef ZTERP_NO_V2
    PRINT("Version 1 and 2 support disabled");
#else
    PRINT("Version 1 and 2 support enabled");
#endif
#ifdef ZTERP_NO_CHEAT
    PRINT("Cheat support disabled");
#else
    PRINT("Cheat support enabled");
#endif
#ifdef ZTERP_TANDY
    PRINT("The Tandy bit can be set");
#else
    PRINT("The Tandy bit cannot be set");
#endif

#ifdef ZTERP_GLK
    glk_exit();
#else
    exit(0);
#endif
  }

#undef PRINT

  if(game_file == NULL) die("no story provided");

  story.io = zterp_io_open(game_file, ZTERP_IO_RDONLY);
  if(story.io == NULL) die("cannot open file %s", game_file);

  blorb = zterp_blorb_parse(story.io);
  if(blorb != NULL)
  {
    const zterp_blorb_chunk *chunk;

    chunk = zterp_blorb_find(blorb, BLORB_EXEC, 0);
    if(chunk == NULL) die("no EXEC resource found");
    if(strcmp(chunk->name, "ZCOD") != 0)
    {
      if(strcmp(chunk->name, "GLUL") == 0) die("Glulx stories are not supported (try git or glulxe)");

      die("unknown story type: %s", chunk->name);
    }

    if(chunk->offset > LONG_MAX) die("zcode offset too large");

    memory_size = chunk->size;
    story.offset = chunk->offset;

    zterp_blorb_free(blorb);
  }
  else
  {
    long size = zterp_io_filesize(story.io);

    if(size == -1) die("unable to determine file size");
    if(size > UINT32_MAX) die("file too large");

    memory_size = size;
    story.offset = 0;
  }

  if(memory_size < 64) die("story file too small");
  if(memory_size > SIZE_MAX) die("story file too large");

  memory = malloc(memory_size);
  if(memory == NULL) die("malloc: %s", strerror(errno));

  process_story();

  if(options.show_id)
  {
#ifdef ZTERP_GLK
    glk_put_string(story_id);
    glk_exit();
#else
    puts(story_id);
    exit(0);
#endif
  }

  setup_opcodes();

  process_instructions(0);

#ifndef ZTERP_GLK
  return 0;
#endif
}
