// Copyright 2009-2021 Chris Spiegel.
//
// This file is part of Bocfel.
//
// Bocfel is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version
// 2 or 3, as published by the Free Software Foundation.
//
// Bocfel is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Bocfel. If not, see <http://www.gnu.org/licenses/>.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>
#include <limits.h>
#include <locale.h>

#include "zterp.h"
#include "blorb.h"
#include "branch.h"
#include "io.h"
#include "memory.h"
#include "meta.h"
#include "osdep.h"
#include "patches.h"
#include "process.h"
#include "random.h"
#include "screen.h"
#include "sound.h"
#include "stack.h"
#include "unicode.h"
#include "util.h"

#ifdef ZTERP_GLK
#include <glk.h>
#endif

#define MAX_LINE	2048

#define DEFAULT_INT_NUMBER	1 // DEC

const char *game_file;
struct options options = {
    .eval_stack_size = DEFAULT_STACK_SIZE,
    .call_stack_size = DEFAULT_CALL_DEPTH,
    .disable_color = false,
    .disable_config = false,
    .disable_timed = false,
    .disable_sound = false,
    .enable_escape = false,
    .escape_string = NULL,
    .disable_fixed = false,
    .assume_fixed = false,
    .disable_graphics_font = false,
    .enable_alt_graphics = false,
    .disable_history_playback = false,
    .show_id = false,
    .disable_term_keys = false,
    .username = NULL,
    .disable_meta_commands = false,
    .int_number = DEFAULT_INT_NUMBER,
    .int_version = 'C',
    .disable_patches = false,
    .replay_on = false,
    .replay_name = NULL,
    .record_on = false,
    .record_name = NULL,
    .transcript_on = false,
    .transcript_name = NULL,
    .max_saves = 100,
    .show_version = false,
    .disable_abbreviations = false,
    .enable_censorship = false,
    .overwrite_transcript = false,
    .override_undo = false,
    .random_seed = -1,
    .random_device = NULL,

    .autosave = false,
    .persistent_transcript = false,
    .editor = NULL,
};

static char story_id[64];

const char *get_story_id(void)
{
    return story_id;
}

// zversion stores the Z-machine version of the story: 1–6.
//
// Z-machine versions 7 and 8 are identical to version 5 but for a
// couple of tiny details. They are thus classified as version 5.
//
// zwhich stores the actual version (1–8) for the few rare times where
// this knowledge is necessary.
int zversion;
static int zwhich;

struct header header;

static bool checksum_verified;

// The null character in the alphabet table does not actually signify a
// null character: character 6 from A2 is special in that it specifies
// that the next two characters form a 10-bit ZSCII character (§3.4).
// The code that uses the alphabet table will step around this character
// when necessary, so it’s safe to use a null character here to mean
// “nothing”.
uint8_t atable[26 * 3] = {
    // A0
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
    'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',

    // A1
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
    'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',

    // A2
    0x0, 0xd, '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '.',
    ',', '!', '?', '_', '#', '\'','"', '/', '\\','-', ':', '(', ')',
};

static unsigned long unpack_multiplier;

uint32_t unpack_routine(uint16_t addr)
{
    return (addr * unpack_multiplier) + header.R_O;
}

uint32_t unpack_string(uint16_t addr)
{
    return (addr * unpack_multiplier) + header.S_O;
}

void store(uint16_t v)
{
    store_variable(byte(pc++), v);
}

static bool is_story(const char **ids, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        if (strcmp(story_id, ids[i]) == 0) {
            return true;
        }
    }

    return false;
}

static bool games[GameCount];

static void initialize_games(void)
{
    // All V1234 games from Andrew Plotkin’s Obsessively Complete
    // Infocom Catalog: https://eblong.com/infocom/
    const char *infocom1234[] = {
#include "v1234.h"
    };

    const char *journey[] = { "83-890706" };
    const char *lurking_horror[] = { "203-870506", "219-870912", "221-870918" };
    const char *planetfall[] = { "1-830517", "20-830708", "26-831014", "29-840118", "37-851003", "39-880501" };
    const char *stationfall[] = { "1-861017", "63-870218", "87-870326", "107-870430" };

    games[GameInfocom1234] = is_story(infocom1234, ASIZE(infocom1234));
    games[GameJourney] = is_story(journey, ASIZE(journey));
    games[GameLurkingHorror] = is_story(lurking_horror, ASIZE(lurking_horror));
    games[GamePlanetfall] = is_story(planetfall, ASIZE(planetfall));
    games[GameStationfall] = is_story(stationfall, ASIZE(stationfall));
}

bool is_game(enum Game game)
{
    switch(game) {
    case GameInfocom1234: case GameJourney: case GameLurkingHorror: case GamePlanetfall: case GameStationfall:
        return games[game];
    default:
        return false;
    }
}

// Find a story ID roughly in the form of an IFID according to §2.2.2.1
// of draft 7 of the Treaty of Babel.
//
// This does not add a ZCODE- prefix, and will not search for a manually
// created IFID.
static void find_id(void)
{
    char serial[] = "------";

    for (int i = 0; i < 6; i++) {
        // isalnum() cannot be used because it is locale-aware, and this
        // must only check for A–Z, a–z, and 0–9. Because ASCII (or a
        // compatible charset) is required, testing against 'a', 'z', etc.
        // is OK.
#define ALNUM(c) ( ((c) >= 'a' && (c) <= 'z') || ((c) >= 'A' && (c) <= 'Z') || ((c) >= '0' && (c) <= '9') )
        if (ALNUM(header.serial[i])) {
            serial[i] = header.serial[i];
        }
#undef ALNUM
    }

    if (strchr("012345679", serial[0]) != NULL && strcmp(serial, "000000") != 0) {
        snprintf(story_id, sizeof story_id, "%d-%s-%04x", header.release, serial, (unsigned)header.checksum);
    } else {
        snprintf(story_id, sizeof story_id, "%d-%s", header.release, serial);
    }

    initialize_games();
}

// isspace() is locale-aware, so explicitly spell out what is
// considered to be whitespace.
static bool is_whitespace(int c)
{
    return c == ' ' || c == '\t';
}

static char *ltrim(char *string)
{
    while (is_whitespace(*string)) {
        string++;
    }

    return string;
}

static void read_config(void)
{
    FILE *fp;
    char file[ZTERP_OS_PATH_SIZE];
    char buf[MAX_LINE];
    char *key, *val, *p;
    bool story_matches = true;

    if (!zterp_os_rcfile(&file, false)) {
        return;
    }

    fp = fopen(file, "r");
    if (fp == NULL) {
        return;
    }

    int lineno = 0;
    while (fgets(buf, sizeof buf, fp) != NULL) {
        char *line = buf;

        if (strchr(line, '\n') != NULL || feof(fp)) {
            lineno++;
        } else {
            fprintf(stderr, "%s:%d: line too long: aborting read of configuration file\n", file, lineno + 1);
            return;
        }

        line[strcspn(line, "#\r\n")] = 0;
        line = ltrim(line);
        if (line[0] == 0) {
            continue;
        }

        if (line[0] == '[') {
            p = strrchr(line, ']');
            if (p == NULL) {
                fprintf(stderr, "%s:%d: expected ']'\n", file, lineno);
            } else if (*ltrim(&p[1]) != 0) {
                fprintf(stderr, "%s:%d: unexpected characters following ']'\n", file, lineno);
            } else {
                *p = 0;

                story_matches = false;
                for (p = strtok(line + 1, " ,"); p != NULL; p = strtok(NULL, " ,")) {
                    const char *ids[] = { p };
                    if (is_story(ids, ASIZE(ids))) {
                        story_matches = true;
                        break;
                    }
                }
            }

            continue;
        }

        if (!story_matches) {
            continue;
        }

        key = line;

        // Key ends at the first whitespace/equals sign.
        for (val = key; *val != 0 && !is_whitespace(*val) && *val != '='; val++) {
        }

        // Find the equals sign, null terminating along the way.
        while (is_whitespace(*val)) {
            *val++ = 0;
        }

        if (*val != '=') {
            fprintf(stderr, "%s:%d: expected '='\n", file, lineno);
            continue;
        }

        // If there was no whitespace before the equals sign it won’t
        // have been null terminated, so do that here.
        *val++ = 0;

        if (*key == 0) {
            fprintf(stderr, "%s:%d: no key\n", file, lineno);
            continue;
        }

        val = ltrim(val);
        if (*val == 0) {
            fprintf(stderr, "%s:%d: missing value\n", file, lineno);
            continue;
        }

        // Trim trailing whitespace.
        p = val + strlen(val) - 1;
        while (is_whitespace(*p)) {
            *p-- = 0;
        }

#define START()		if (false) do { } while (false)

#define BOOL(name)	else if (strcmp(key, #name) == 0) do { \
    if (strcmp(val, "0") == 0) { \
        options.name = false; \
    } else if (strcmp(val, "1") == 0) { \
        options.name = true; \
    } else { \
        fprintf(stderr, "%s:%d: invalid value for %s: must be 0 or 1\n", file, lineno, #name); \
    } \
} while (false)

#define NUMBER(name)	else if (strcmp(key, #name) == 0) do { \
    char *endptr; \
    long n; \
    n = strtol(val, &endptr, 10); \
    if (*endptr != 0) { \
        fprintf(stderr, "%s:%d: invalid value for %s: must be a number\n", file, lineno, #name); \
    } else { \
        options.name = n; \
    } \
} while (false)

#define STRING(name)	else if (strcmp(key, #name) == 0) do { \
    free(options.name); \
    options.name = xstrdup(val); \
} while (false)

#define CHAR(name)	else if (strcmp(key, #name) == 0) do { \
    if (strlen(val) != 1) { \
        fprintf(stderr, "%s:%d: invalid value for %s: must be a single character\n", file, lineno, #name); \
    } else { \
        options.name = val[0]; \
    } \
} while (false)

#ifdef GLK_MODULE_GARGLKTEXT
#define COLOR(name, num)else if (strcmp(key, "color_" #name) == 0) do { \
    char *endptr; \
    unsigned long color; \
    color = strtoul(val, &endptr, 16); \
    if (*val == '-' || *endptr != 0 || color > 0xffffff) { \
        fprintf(stderr, "%s:%d: invalid value for color_%s: must be a hexadecimal number ranging from 0x000000 to 0xffffff\n", file, lineno, #name); \
    } else { \
        update_color(num, color); \
    } \
} while (false)
#else
#define COLOR(name, num)else if (strcmp(key, "color_" #name) == 0) do { } while (false)
#endif

        // Some configuration options are available only in certain
        // compile-come configurations. For those options, silently
        // ignore them in configurations where they are not valid. This
        // allows configuration files to be shared between such builds
        // without dispaying diagnostics for options which are valid in
        // some builds but not others.

        START();

        NUMBER(eval_stack_size);
        NUMBER(call_stack_size);
        BOOL  (disable_color);
        BOOL  (disable_timed);
        BOOL  (disable_sound);
        BOOL  (enable_escape);
        STRING(escape_string);
        BOOL  (disable_fixed);
        BOOL  (assume_fixed);
        BOOL  (disable_graphics_font);
        BOOL  (enable_alt_graphics);
        BOOL  (disable_history_playback);
        BOOL  (disable_term_keys);
        STRING(username);
        BOOL  (disable_meta_commands);
        NUMBER(max_saves);
        NUMBER(int_number);
        CHAR  (int_version);
        BOOL  (disable_patches);
        BOOL  (replay_on);
        STRING(replay_name);
        BOOL  (record_on);
        STRING(record_name);
        BOOL  (transcript_on);
        STRING(transcript_name);
        BOOL  (disable_abbreviations);
        BOOL  (enable_censorship);
        BOOL  (overwrite_transcript);
        BOOL  (override_undo);
        NUMBER(random_seed);
        STRING(random_device);

        BOOL(autosave);
        BOOL(persistent_transcript);
        STRING(editor);

        COLOR(black,   2);
        COLOR(red,     3);
        COLOR(green,   4);
        COLOR(yellow,  5);
        COLOR(blue,    6);
        COLOR(magenta, 7);
        COLOR(cyan,    8);
        COLOR(white,   9);

        else if (strcmp(key, "cheat") == 0) {
#ifndef ZTERP_NO_CHEAT
            if (!cheat_add(val, false)) {
                fprintf(stderr, "%s:%d: syntax error in cheat\n", file, lineno);
            }
#endif
        }

        else if (strcmp(key, "patch") == 0) {
            switch (apply_user_patch(val)) {
            case PatchStatusSyntaxError:
                fprintf(stderr, "%s:%d: syntax error in patch\n", file, lineno);
                break;
            case PatchStatusNotFound:
                fprintf(stderr, "%s:%d: patch does not apply to this story\n", file, lineno);
                break;
            case PatchStatusOk:
                break;
            }
        }

        // Legacy name.
        else if (strcmp(key, "notes_editor") == 0) {
            free(options.editor);
            options.editor = xstrdup(val);
        }

        else {
            fprintf(stderr, "%s:%d: unknown configuration option: %s\n", file, lineno, key);
        }

#undef START
#undef BOOL
#undef NUMBER
#undef STRING
#undef CHAR
#undef COLOR
    }

    fclose(fp);
}

static bool have_statuswin = false;
static bool have_upperwin  = false;

static void write_flags1(void)
{
    uint8_t flags1;

    flags1 = byte(0x01);

    if (zversion == 3) {
        flags1 |= FLAGS1_NOSTATUS;
        flags1 &= ~(FLAGS1_SCREENSPLIT | FLAGS1_VARIABLE);

#ifdef GARGLK
        // Assume that if Gargoyle is being used, the default font is not fixed.
        flags1 |= FLAGS1_VARIABLE;
#endif

        if (have_statuswin) {
            flags1 &= ~FLAGS1_NOSTATUS;
        }
        if (have_upperwin) {
            flags1 |= FLAGS1_SCREENSPLIT;
        }
        if (options.enable_censorship) {
            flags1 |= FLAGS1_CENSOR;
        }
    } else if (zversion >= 4) {
        flags1 |= (FLAGS1_BOLD | FLAGS1_ITALIC | FLAGS1_FIXED);
        flags1 &= ~FLAGS1_TIMED;

        if (zversion >= 5) {
            flags1 &= ~FLAGS1_COLORS;
        }

        if (zversion == 6) {
            flags1 &= ~(FLAGS1_PICTURES | FLAGS1_SOUND);
            if (sound_loaded()) {
                flags1 |= FLAGS1_SOUND;
            }
        }

#ifdef ZTERP_GLK
        if (glk_gestalt(gestalt_Timer, 0)) {
            flags1 |= FLAGS1_TIMED;
        }
#ifdef GLK_MODULE_GARGLKTEXT
        if (zversion >= 5) {
            flags1 |= FLAGS1_COLORS;
        }
#endif
#else
        if (!zterp_os_have_style(STYLE_BOLD)) {
            flags1 &= ~FLAGS1_BOLD;
        }
        if (!zterp_os_have_style(STYLE_ITALIC)) {
            flags1 &= ~FLAGS1_ITALIC;
        }
        if (zversion >= 5 && zterp_os_have_colors()) {
            flags1 |= FLAGS1_COLORS;
        }
#endif

        if (zversion >= 5 && options.disable_color) {
            flags1 &= ~FLAGS1_COLORS;
        }
        if (options.disable_timed) {
            flags1 &= ~FLAGS1_TIMED;
        }
        if (options.disable_fixed) {
            flags1 &= ~FLAGS1_FIXED;
        }
    }

    store_byte(0x01, flags1);
}

static void write_flags2(void)
{
    if (zversion >= 5) {
        uint16_t flags2 = word(0x10);

#ifdef ZTERP_GLK
        if (!glk_gestalt(gestalt_MouseInput, wintype_TextGrid)) {
            flags2 &= ~FLAGS2_MOUSE;
        }
#else
        flags2 &= ~FLAGS2_MOUSE;
#endif
        if (!sound_loaded()) {
            flags2 &= ~FLAGS2_SOUND;
        }
        if (zversion >= 6) {
            flags2 &= ~FLAGS2_MENUS;
        }

        if (options.disable_graphics_font) {
            flags2 &= ~FLAGS2_PICTURES;
        }

        if (options.max_saves == 0) {
            flags2 &= ~FLAGS2_UNDO;
        }

        store_word(0x10, flags2);
    }
}

static void write_header_extension_table(void)
{
    if (header.extension_table == 0) {
        return;
    }

    // Flags3.
    if (header.extension_entries >= 4) {
        store_word(header.extension_table + (2 * 4), 0);
    }

    // True default foreground color.
    if (header.extension_entries >= 5) {
        store_word(header.extension_table + (2 * 5), 0x0000);
    }

    // True default background color.
    if (header.extension_entries >= 6) {
        store_word(header.extension_table + (2 * 6), 0x7fff);
    }
}

// Various parts of the header (those marked “Rst” in §11) should be
// updated by the interpreter. This function does that. This is also
// used when restoring, because the save file might have come from an
// interpreter with vastly different settings.
void write_header(void)
{
    write_flags1();
    write_flags2();
    write_header_extension_table();

    if (zversion >= 4) {
        unsigned int width, height;

        // Interpreter number & version.
        if (options.int_number < 1 || options.int_number > 11) {
            options.int_number = DEFAULT_INT_NUMBER;
        }
        store_byte(0x1e, options.int_number);
        store_byte(0x1f, options.int_version);

        get_screen_size(&width, &height);

        // Screen height and width.
        // A height of 255 means infinite, so cap at 254.
        store_byte(0x20, height > 254 ? 254 : height);
        store_byte(0x21, width > 255 ? 255 : width);

        if (zversion >= 5) {
            // Screen width and height in units.
            store_word(0x22, width > UINT16_MAX ? UINT16_MAX : width);
            store_word(0x24, height > UINT16_MAX ? UINT16_MAX : height);

            // Font width and height in units.
            store_byte(0x26, 1);
            store_byte(0x27, 1);

            // Default background and foreground colors.
            store_byte(0x2c, 1);
            store_byte(0x2d, 1);
        }
    }

    // Standard revision #
    store_byte(0x32, 1);
    store_byte(0x33, 1);

    if (options.username != NULL) {
        strncpy((char *)&memory[0x38], options.username, 8);
    }
}

static void process_alphabet_table(void)
{
    if (zversion == 1) {
        memcpy(&atable[26 * 2], " 0123456789.,!?_#'\"/\\<-:()", 26);
    } else if (zversion >= 5 && word(0x34) != 0) {
        if (word(0x34) + 26 * 3 > memory_size) {
            die("corrupted story: alphabet table out of range");
        }

        memcpy(atable, &memory[word(0x34)], 26 * 3);

        // Even with a new alphabet table, characters 6 and 7 from A2 must
        // remain the same (§3.5.5.1).
        atable[52] = 0x00;
        atable[53] = 0x0d;
    }
}

static uint16_t mouse_click_addr;

static void read_header_extension_table(void)
{
    if (header.extension_table == 0) {
        return;
    }

    if (header.extension_entries >= 2) {
        mouse_click_addr = header.extension_table + 2;
    }

    // Unicode table.
    if (header.extension_entries >= 3 && word(header.extension_table + (2 * 3)) != 0) {
        uint16_t utable = word(header.extension_table + (2 * 3));

        parse_unicode_table(utable);
    }
}

void zterp_mouse_click(uint16_t x, uint16_t y) {
    if (mouse_click_addr != 0) {
        store_word(mouse_click_addr, x);
        store_word(mouse_click_addr + 2, y);
    }
}

static void calculate_checksum(zterp_io *io, long offset)
{
    uint32_t remaining = header.file_length - 0x40;
    uint16_t checksum = 0;

    if (!zterp_io_seek(io, offset + 0x40, SEEK_SET)) {
        return;
    }

    while (remaining != 0) {
        uint8_t buf[8192];
        uint32_t to_read = remaining < sizeof buf ? remaining : sizeof buf;

        if (!zterp_io_read_exact(io, buf, to_read)) {
            return;
        }

        for (uint32_t i = 0; i < to_read; i++) {
            checksum += buf[i];
        }

        remaining -= to_read;
    }

    checksum_verified = checksum == header.checksum;
}

static void process_story(zterp_io *io, long offset)
{
    if (!zterp_io_seek(io, offset, SEEK_SET)) {
        die("unable to rewind story");
    }

    if (!zterp_io_read_exact(io, memory, memory_size)) {
        die("unable to read from story file");
    }

    zversion = byte(0x00);
    if (zversion < 1 || zversion > 8) {
        die("only z-code versions 1-8 are supported");
    }

    zwhich = zversion;
    if (zversion == 7 || zversion == 8) {
        zversion = 5;
    }

    switch (zwhich) {
    case 1: case 2: case 3:
        unpack_multiplier = 2;
        break;
    case 4: case 5: case 6: case 7:
        unpack_multiplier = 4;
        break;
    case 8:
        unpack_multiplier = 8;
        break;
    default:
        die("unhandled z-machine version: %d", zwhich);
    }

    header.pc = word(0x06);
    if (header.pc >= memory_size) {
        die("corrupted story: initial pc out of range");
    }

    if (zversion == 6 && header.pc == 0) {
        die("corrupted story: packed address 0 is invalid for initial pc");
    }

    header.release = word(0x02);
    header.dictionary = word(0x08);
    header.objects = word(0x0a);
    header.globals = word(0x0c);
    header.abbr = word(0x18);

    // There is no explicit “end of static” tag; but it must end by 0xffff
    // or the end of the story file, whichever is smaller.
    header.static_start = word(0x0e);
    header.static_end = memory_size < 0xffff ? memory_size : 0xffff;

    if (zversion >= 5) {
        header.extension_table = word(0x36);
        header.extension_entries = user_word(header.extension_table);

        if (header.extension_table + (2 * header.extension_entries) > memory_size) {
            die("corrupted story: header extension table out of range");
        }
    }

    memcpy(header.serial, &memory[0x12], sizeof header.serial);

#define PROPSIZE	(zversion <= 3 ? 62UL : 126UL)

    // There must be at least enough room in dynamic memory for the header
    // (64 bytes), the global variables table (480 bytes), and the
    // property defaults table (62 or 126 bytes).
    if (header.static_start < 64UL + 480UL + PROPSIZE) {
        die("corrupted story: dynamic memory too small (%d bytes)", (int)header.static_start);
    }

    if (header.static_start >= memory_size) {
        die("corrupted story: static memory out of range");
    }

    if (header.dictionary != 0 && header.dictionary < header.static_start) {
        die("corrupted story: dictionary is not in static memory");
    }

    if (header.objects < 64 || header.objects + PROPSIZE > header.static_start) {
        die("corrupted story: object table is not in dynamic memory");
    }

#undef PROPSIZE

    if (header.globals < 64 || header.globals + 480UL > header.static_start) {
        die("corrupted story: global variables are not in dynamic memory");
    }

    if (header.abbr >= memory_size) {
        die("corrupted story: abbreviation table out of range");
    }

    header.file_length = word(0x1a) * (zwhich <= 3 ? 2UL : zwhich <= 5 ? 4UL : 8UL);
    if (header.file_length > memory_size) {
        die("story's reported size (%lu) greater than file size (%lu)", (unsigned long)header.file_length, (unsigned long)memory_size);
    }

    header.checksum = word(0x1c);

    calculate_checksum(io, offset);

    if (zwhich == 6 || zwhich == 7) {
        header.R_O = word(0x28) * 8UL;
        header.S_O = word(0x2a) * 8UL;
    }

    if (zversion >= 5 && !options.disable_term_keys) {
        header.terminating_characters_table = word(0x2e);
    }

    if (dynamic_memory == NULL) {
        dynamic_memory = malloc(header.static_start);
        if (dynamic_memory == NULL) {
            die("unable to allocate memory for dynamic memory");
        }
        memcpy(dynamic_memory, memory, header.static_start);
    }

#ifdef ZTERP_GLK
#endif

    process_alphabet_table();
    read_header_extension_table();

    // The configuration file cannot be read until the ID of the current
    // story is known, and the ID of the current story is not known until
    // the file has been processed; so do both of those here.
    find_id();
    if (!options.disable_config) {
        read_config();
    }

    // Most options directly set their respective variables, but a few
    // require intervention. Delay that intervention until here so that
    // the configuration file is taken into account.
    if (options.escape_string == NULL) {
        options.escape_string = xstrdup("1m");
    }

    if (!options.disable_sound) {
        init_sound();
    }

    // Now that we have a Unicode table and the user’s Unicode
    // preferences, build the ZSCII to Unicode and Unicode to ZSCII
    // tables.
    setup_tables();

    if (!options.disable_patches) {
        apply_patches();
    }

    if (zversion <= 3) {
        have_statuswin = create_statuswin();
    }

    if (zversion >= 3) {
        have_upperwin  = create_upperwin();
    }

    if (options.transcript_on) {
        store_word(0x10, word(0x10) | FLAGS2_TRANSCRIPT);
    }

    if (options.record_on) {
        output_stream(OSTREAM_RECORD, 0);
    }

    if (options.replay_on) {
        input_stream(ISTREAM_FILE);
    }
}

static void start_story(void)
{
    uint16_t flags2;
    static bool first_run = true;

    pc = header.pc;

    // Reset dynamic memory.
    // §6.1.3: Flags2 is preserved on a restart. This function is also
    // used at the initial program start, but Flags2 should be preserved
    // there, as well: the pictures bit and the transcript bit might
    // have been set during story processing, and those should persist.
    flags2 = word(0x10);
    memcpy(memory, dynamic_memory, header.static_start);
    store_word(0x10, flags2);

    write_header();

    // Put everything in a clean state.
    init_stack(first_run);
    init_screen(first_run);
    init_random(first_run);
    init_meta(first_run);

    if (zversion == 6) {
        znargs = 1;
        zargs[0] = pc;
        start_v6();
    }

    first_run = false;
}

void znop(void)
{
}

void zrestart(void)
{
    start_story();
    interrupt_restart();
}

void zquit(void)
{
    char autosave_name[ZTERP_OS_PATH_SIZE];

    // On @quit, remove the autosave (if any exists). First try to
    // rename it to the original name plus .bak; this allows users to
    // restore accidentally-deleted files. If the rename fails, though,
    // just try to delete it.
    if (options.autosave && zterp_os_autosave_name(&autosave_name)) {
        char backup_name[(sizeof autosave_name) + 4];
        snprintf(backup_name, sizeof backup_name, "%s.bak", autosave_name);
        if (rename(autosave_name, backup_name) == -1) {
            remove(autosave_name);
        }
    }

    interrupt_quit();
}

void zverify(void)
{
    branch_if(checksum_verified);
}

static zterp_io *open_savefile(enum zterp_io_mode mode)
{
    // When this is null, the user is prompted for a filename.
    char *suggested = NULL;
    char aux[ZTERP_OS_PATH_SIZE];

    // If no prompt argument is given, assume 0.
    if (znargs == 3 || (znargs == 4 && zargs[3] == 0)) {
        uint16_t addr = zargs[2];
        // Max string size: 255
        // Possible .AUX: 4
        // Null terminator: 1
        // 255 + 4 + 1 = 260
        char filename[260] = { 0 };

        for (size_t i = 0; i < user_byte(addr); i++) {
            uint8_t c = user_byte(addr + i + 1);

            // Convert to uppercase per §7.6.1.1; and according to the
            // description for @save in §15, the characters are ASCII.
            if (c >= 97 && c <= 122) {
                c -= 32;
            }

            filename[i] = c;
        }

        if (filename[0] != 0) {
            if (strchr(filename, '.') == NULL) {
                strcat(filename, ".AUX");
            }

            if (zterp_os_aux_file(&aux, filename)) {
                suggested = aux;
            }
        }
    }

    // If there is a suggested filename and “prompt” is 1, this should
    // prompt the user with the suggested filename, but Glk doesn’t
    // support that.
    return zterp_io_open(suggested, mode, ZTERP_IO_PURPOSE_DATA);
}

void zsave5(void)
{
    zterp_io *savefile;

    if (znargs == 0) {
        zsave();
        return;
    }

    ZASSERT(zargs[0] + zargs[1] < memory_size, "attempt to save beyond the end of memory");

    savefile = open_savefile(ZTERP_IO_MODE_WRONLY);
    if (savefile == NULL) {
        store(0);
        return;
    }

    store(zterp_io_write_exact(savefile, &memory[zargs[0]], zargs[1]));

    zterp_io_close(savefile);
}

void zrestore5(void)
{
    zterp_io *savefile;
    uint8_t *buf;
    size_t n;

    if (znargs == 0) {
        zrestore();
        return;
    }

    savefile = open_savefile(ZTERP_IO_MODE_RDONLY);
    if (savefile == NULL) {
        store(0);
        return;
    }

    buf = malloc(zargs[1]);
    if (buf == NULL) {
        zterp_io_close(savefile);
        store(0);
        return;
    }

    n = zterp_io_read(savefile, buf, zargs[1]);
    for (size_t i = 0; i < n; i++) {
        user_store_byte(zargs[0] + i, buf[i]);
    }

    free(buf);

    zterp_io_close(savefile);

    store(n);
}

void zpiracy(void)
{
    branch_if(true);
}

#ifdef ZTERP_GLK
void glk_main(void)
#else
int main(int argc, char **argv)
#endif
{
    zterp_io *story_io;
    long story_offset;
    zterp_blorb *blorb;

    // strftime() is given the %c conversion specification, which is
    // locale-aware, so honor the user’s time locale.
    setlocale(LC_TIME, "");

    // It’s too early to properly set up all tables (neither the alphabet
    // nor Unicode table has been read from the story file), but it’s
    // possible for messages to be displayed to the user before a story is
    // even loaded, so at least the basic tables need to be created so
    // that non-Unicode platforms have proper translations available.
    setup_tables();

#ifdef ZTERP_GLK
    if (!create_mainwin()) {
        return;
    }
#endif

#ifndef ZTERP_GLK
    process_arguments(argc, argv);
#endif

    if (arg_status != ARG_OK) {
        help();
#ifdef ZTERP_GLK
        glk_exit();
#else
        if (arg_status == ARG_HELP) {
            exit(0);
        } else {
            exit(EXIT_FAILURE);
        }
#endif
    }

#ifndef ZTERP_GLK
    zterp_os_init_term();
#endif

    if (options.show_version) {
        char config[ZTERP_OS_PATH_SIZE];

        screen_puts("Bocfel " ZTERP_VERSION);
#ifdef ZTERP_NO_SAFETY_CHECKS
        screen_puts("Runtime assertions disabled");
#else
        screen_puts("Runtime assertions enabled");
#endif
#ifdef ZTERP_NO_CHEAT
        screen_puts("Cheat support disabled");
#else
        screen_puts("Cheat support enabled");
#endif
#ifdef ZTERP_TANDY
        screen_puts("The Tandy bit can be set");
#else
        screen_puts("The Tandy bit cannot be set");
#endif

        if (zterp_os_rcfile(&config, false)) {
            screen_printf("Configuration file: %s\n", config);
        } else {
            screen_printf("Cannot determine configuration file location\n");
        }

#ifdef ZTERP_GLK
        glk_exit();
#else
        exit(0);
#endif
    }

    if (game_file == NULL) {
        help();
        die("no story provided");
    }

    story_io = zterp_io_open(game_file, ZTERP_IO_MODE_RDONLY, ZTERP_IO_PURPOSE_DATA);
    if (story_io == NULL) {
        die("cannot open file %s", game_file);
    }

    blorb = zterp_blorb_parse(story_io);
    if (blorb != NULL) {
        const zterp_blorb_chunk *chunk;

        chunk = zterp_blorb_find(blorb, BLORB_EXEC, 0);
        if (chunk == NULL) {
            die("no EXEC resource found");
        }
        if (chunk->type != STRID(&"ZCOD")) {
            if (chunk->type == STRID(&"GLUL")) {
                die("Glulx stories are not supported (try git or glulxe)");
            }

            die("unknown story type: %s (0x%08lx)", chunk->name, (unsigned long)chunk->type);
        }

#if UINT32_MAX > LONG_MAX
        if (chunk->offset > LONG_MAX) {
            die("zcode offset too large");
        }
#endif

        memory_size = chunk->size;
        story_offset = chunk->offset;

        zterp_blorb_free(blorb);
    } else {
        long size = zterp_io_filesize(story_io);

        if (size == -1) {
            die("unable to determine file size");
        }
#if LONG_MAX > UINT32_MAX
        if (size > UINT32_MAX) {
            die("file too large");
        }
#endif

        memory_size = size;
        story_offset = 0;
    }

    if (memory_size < 64) {
        die("story file too small");
    }
#if UINT32_MAX > SIZE_MAX - 22
    if (memory_size > SIZE_MAX - 22) {
        die("story file too large");
    }
#endif

    // It’s possible for a story to be cut short in the middle of an
    // instruction. If so, the processing loop will run past the end of
    // memory. Either pc needs to be checked each and every time it is
    // incremented, or a small guard needs to be placed at the end of
    // memory that will trigger an illegal instruction error. The latter
    // is done by filling the end of memory with zeroes, which do not
    // represent a valid instruction.
    //
    // There need to be at least 22 bytes for the worst case: 0xec
    // (call_vs2) as the last byte in memory. The next two bytes, which
    // will be zeroes, indicate that 8 large constants, or 16 bytes, will
    // be next. This is a store instruction, so one more byte will be
    // read to determine where to store. Another byte is read to
    // determine the next opcode; this will be zero, which is nominally a
    // 2OP, requiring two more bytes to be read. At this point the opcode
    // will be looked up, resulting in an illegal instruction error.
    memory = malloc(memory_size + 22);
    if (memory == NULL) {
        die("unable to allocate memory for story file");
    }
    memset(memory + memory_size, 0, 22);

    process_story(story_io, story_offset);

    zterp_io_close(story_io);

    if (options.show_id) {
        screen_puts(story_id);
    } else {
        start_story();

        // If header transcript/fixed bits have been set, either by the
        // story or by the user, this will activate them.
        user_store_word(0x10, word(0x10));

        setup_opcodes();
        process_instructions();
    }
}
