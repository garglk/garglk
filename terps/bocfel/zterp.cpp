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

#include <algorithm>
#include <array>
#include <climits>
#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <ios>
#include <iostream>
#include <istream>
#include <map>
#include <new>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "zterp.h"
#include "blorb.h"
#include "branch.h"
#include "iff.h"
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
#include "types.h"
#include "unicode.h"
#include "util.h"

#ifdef ZTERP_GLK
#include <glk.h>
#endif

std::string game_file;
Options options;

static std::string story_id;

const std::string &get_story_id()
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

Header header;

static bool checksum_verified;

// The null character in the alphabet table does not actually signify a
// null character: character 6 from A2 is special in that it specifies
// that the next two characters form a 10-bit ZSCII character (§3.4).
// The code that uses the alphabet table will step around this character
// when necessary, so it’s safe to use a null character here to mean
// “nothing”.
std::array<uint8_t, 26 * 3> atable = {
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

static std::set<Game> games;

static void initialize_games()
{
    // All V1234 games from Andrew Plotkin’s Obsessively Complete
    // Infocom Catalog: https://eblong.com/infocom/
    std::set<std::string> infocom1234 = {
#include "v1234.h"
    };

    std::map<Game, std::set<std::string>> gamemap = {
        { Game::Infocom1234, infocom1234 },
        { Game::Journey, { "83-890706" } },
        { Game::LurkingHorror, { "203-870506", "219-870912", "221-870918" } },
        { Game::Planetfall, { "1-830517", "20-830708", "26-831014", "29-840118", "37-851003", "39-880501" } },
        { Game::Stationfall, { "1-861017", "63-870218", "87-870326", "107-870430" } },
    };

    for (const auto &pair : gamemap) {
        if (pair.second.find(story_id) != pair.second.end()) {
            games.insert(pair.first);
        }
    }
}

bool is_game(Game game)
{
    return games.find(game) != games.end();
}

// Find a story ID roughly in the form of an IFID according to §2.2.2.1
// of revision 10 of the Treaty of Babel.
//
// This does not add a ZCODE- prefix, and will not search for a manually
// created IFID.
static void find_id()
{
    std::string serial = "------";

    // isalnum() cannot be used because it is locale-aware, and this
    // must only check for A–Z, a–z, and 0–9. Because ASCII (or a
    // compatible charset) is required, testing against 'a', 'z', etc.
    // is OK.
    std::copy_if(header.serial, header.serial + 6, serial.begin(), [](auto c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
    });

    std::ostringstream ss;
    ss << header.release << "-" << serial;
    if (std::strchr("012345679", serial[0]) != nullptr &&
        serial != "000000" &&
        serial != "999999" &&
        serial != "------") {

        ss << "-" << std::hex << std::setw(4) << std::setfill('0') << header.checksum;
    }
    story_id = ss.str();

    initialize_games();
}

static void read_config()
{
    std::string line;
    bool story_matches = true;

    auto file = zterp_os_rcfile(false);
    if (file == nullptr) {
        return;
    }

    std::ifstream f(*file);
    if (!f.is_open()) {
        return;
    }

    int lineno = 0;
    while (std::getline(f, line)) {
        lineno++;

        line = ltrim(line);

        auto comment = line.find('#');
        if (comment != std::string::npos) {
            line.erase(comment);
        }
        line = rtrim(line);

        if (line.empty()) {
            continue;
        }

        if (line[0] == '[' && line.back() == ']') {
            std::string id = line.substr(1, line.size() - 2);
            story_matches = id == story_id;

            continue;
        }

        if (!story_matches) {
            continue;
        }

        auto equal = line.find('=');
        if (equal == std::string::npos) {
            std::cerr << *file << ":" << lineno << ": expected '='" << std::endl;
            continue;
        }

        auto key = rtrim(line.substr(0, equal));
        if (key.empty()) {
            std::cerr << *file << ":" << lineno << ": no key" << std::endl;
            continue;
        }

        auto val = ltrim(line.substr(equal + 1));
        if (val.empty()) {
            std::cerr << *file << ":" << lineno << ": no value" << std::endl;
            continue;
        }

        try {
            std::map<std::string, std::string> legacy_names = {
                {"notes_editor", "editor"},
                {"max_saves", "undo_slots"},
            };

            auto newkey = legacy_names.at(key);
            std::cerr << *file << ":" << lineno << ": warning: configuration option " << key;
            if (newkey.empty()) {
                std::cerr << " no longer exists" << std::endl;
                continue;
            } else {
                std::cerr << " is deprecated; it has been renamed " << newkey << std::endl;
            }
            key = newkey;
        } catch (const std::out_of_range &) {
        }

#define START()		if (false) do { } while (false)

#define BOOL(name)	else if (key == #name) do { \
    if (val == "0") { \
        options.name = false; \
    } else if (val == "1") { \
        options.name = true; \
    } else { \
        std::cerr << *file << ":" << lineno << ": invalid value for " << #name << ": must be 0 or 1" << std::endl; \
    } \
} while (false)

#define NUMHELP(name, wrapper)	else if (key == #name) do { \
    char *endptr; \
    unsigned long n; \
    n = std::strtoul(val.c_str(), &endptr, 10); \
    if (val[0] == '-' || *endptr != 0) { \
        std::cerr << *file << ":" << lineno << ": invalid value for " << #name << ": must be a non-negative number" << std::endl; \
    } else { \
        options.name = wrapper(n); \
    } \
} while (false)

#define NUMBER(name)	NUMHELP(name, [](unsigned long num) { return num; })

#define OPTNUM(name)	NUMHELP(name, std::make_unique<unsigned long>)

#define STRING(name)	else if (key == #name) do { \
    options.name = std::make_unique<std::string>(val); \
} while (false)

#define CHAR(name)	else if (key == #name) do { \
    if (val.size() != 1) { \
        std::cerr << *file << ":" << lineno << ": invalid value for " << #name << ": must be a single character" << std::endl; \
    } else { \
        options.name = val[0]; \
    } \
} while (false)

#ifdef GLK_MODULE_GARGLKTEXT
#define COLOR(name, num)else if (key == "color_" #name) do { \
    char *endptr; \
    unsigned long color; \
    color = std::strtoul(val.c_str(), &endptr, 16); \
    if (val[0] == '-' || *endptr != 0 || color > 0xffffff) { \
        std::cerr << *file << ":" << lineno << ": invalid value for color_" << #name << ": must be a hexadecimal number ranging from 0x000000 to 0xffffff" << std::endl; \
    } else { \
        update_color(num, color); \
    } \
} while (false)
#else
#define COLOR(name, num)else if (key == "color_" #name) do { } while (false)
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
        NUMBER(undo_slots);
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
        OPTNUM(random_seed);
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

        else if (key == "cheat") {
#ifndef ZTERP_NO_CHEAT
            if (!cheat_add(val, false)) {
                std::cerr << *file << ":" << lineno << ": syntax error in cheat" << std::endl;
            }
#endif
        }

        else if (key == "patch") {
            try {
                apply_user_patch(val);
            } catch (const PatchStatus::SyntaxError &) {
                std::cerr << *file << ":" << lineno << ": syntax error in patch" << std::endl;
            } catch (const PatchStatus::NotFound &) {
                std::cerr << *file << ":" << lineno << ": patch does not apply to this story" << std::endl;
            }
        }

        else {
            std::cerr << *file << ":" << lineno << ": unknown configuration option: " << key << std::endl;
        }

#undef START
#undef BOOL
#undef NUMHELP
#undef NUMBER
#undef OPTNUM
#undef STRING
#undef CHAR
#undef COLOR
    }
}

static bool have_statuswin = false;
static bool have_upperwin  = false;

static void write_flags1()
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

static void write_flags2()
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

        if (options.undo_slots == 0) {
            flags2 &= ~FLAGS2_UNDO;
        }

        store_word(0x10, flags2);
    }
}

static void write_header_extension_table()
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
void write_header()
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

        get_screen_size(width, height);

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

    if (options.username != nullptr) {
        options.username->resize(8, '\0');
        std::copy(options.username->begin(), options.username->end(), &memory[0x38]);
    }
}

static void process_alphabet_table()
{
    if (zversion == 1) {
        std::memcpy(&atable[26 * 2], " 0123456789.,!?_#'\"/\\<-:()", 26);
    } else if (zversion >= 5 && word(0x34) != 0) {
        if (word(0x34) + 26 * 3 > memory_size) {
            die("corrupted story: alphabet table out of range");
        }

        std::memcpy(atable.data(), &memory[word(0x34)], 26 * 3);

        // Even with a new alphabet table, characters 6 and 7 from A2 must
        // remain the same (§3.5.5.1).
        atable[52] = 0x00;
        atable[53] = 0x0d;
    }
}

static uint16_t mouse_click_addr;

static void read_header_extension_table()
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

void zterp_mouse_click(uint16_t x, uint16_t y)
{
    if (mouse_click_addr != 0) {
        store_word(mouse_click_addr, x);
        store_word(mouse_click_addr + 2, y);
    }
}

static void calculate_checksum(IO &io, long offset)
{
    uint32_t remaining = header.file_length - 0x40;
    uint16_t checksum = 0;

    try {
        io.seek(offset + 0x40, IO::SeekFrom::Start);
    } catch (const IO::IOError &) {
        return;
    }

    while (remaining != 0) {
        std::array<uint8_t, 8192> buf;
        uint32_t to_read = remaining < buf.size() ? remaining : buf.size();

        try {
            io.read_exact(buf.data(), to_read);
        } catch (const IO::IOError &) {
            return;
        }

        for (uint32_t i = 0; i < to_read; i++) {
            checksum += buf[i];
        }

        remaining -= to_read;
    }

    checksum_verified = checksum == header.checksum;
}

static void process_story(IO &io, long offset)
{
    try {
        io.seek(offset, IO::SeekFrom::Start);
    } catch (const IO::IOError &) {
        die("unable to rewind story");
    }

    try {
        io.read_exact(memory, memory_size);
    } catch (const IO::IOError &) {
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

    std::memcpy(header.serial, &memory[0x12], sizeof header.serial);

    unsigned long propsize = zversion <= 3 ? 62 : 126;

    // There must be at least enough room in dynamic memory for the header
    // (64 bytes), the global variables table (480 bytes), and the
    // property defaults table (62 or 126 bytes).
    if (header.static_start < 64UL + 480UL + propsize) {
        die("corrupted story: dynamic memory too small (%d bytes)", static_cast<int>(header.static_start));
    }

    if (header.static_start >= memory_size) {
        die("corrupted story: static memory out of range");
    }

    if (header.dictionary != 0 && header.dictionary < header.static_start) {
        die("corrupted story: dictionary is not in static memory");
    }

    if (header.objects < 64 || header.objects + propsize > header.static_start) {
        die("corrupted story: object table is not in dynamic memory");
    }

    if (header.globals < 64 || header.globals + 480UL > header.static_start) {
        die("corrupted story: global variables are not in dynamic memory");
    }

    if (header.abbr >= memory_size) {
        die("corrupted story: abbreviation table out of range");
    }

    header.file_length = word(0x1a) * (zwhich <= 3 ? 2UL : zwhich <= 5 ? 4UL : 8UL);
    if (header.file_length > memory_size) {
        die("story's reported size (%lu) greater than file size (%lu)", static_cast<unsigned long>(header.file_length), static_cast<unsigned long>(memory_size));
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

    if (dynamic_memory == nullptr) {
        try {
            dynamic_memory = new uint8_t[header.static_start];
        } catch (const std::bad_alloc &) {
            die("unable to allocate memory for dynamic memory");
        }
        std::memcpy(dynamic_memory, memory, header.static_start);
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
        have_upperwin = create_upperwin();
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

void start_story()
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
    std::memcpy(memory, dynamic_memory, header.static_start);
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

void znop()
{
}

void zrestart()
{
    throw Operation::Restart();
}

void zquit()
{
    // On @quit, remove the autosave (if any exists). First try to
    // rename it to the original name plus .bak; this allows users to
    // restore accidentally-deleted files. If the rename fails, though,
    // just try to delete it.
    if (options.autosave) {
        auto autosave_name = zterp_os_autosave_name();
        if (autosave_name != nullptr) {
            std::string backup_name = *autosave_name;
            backup_name += ".bak";
            if (std::rename(autosave_name->c_str(), backup_name.c_str()) == -1) {
                std::remove(autosave_name->c_str());
            }
        }
    }

    throw Operation::Quit();
}

void zverify()
{
    branch_if(checksum_verified);
}

static std::shared_ptr<IO> open_savefile(IO::Mode mode)
{
    // When this is null, the user is prompted for a filename.
    std::unique_ptr<std::string> suggested;

    // If no prompt argument is given, assume 0.
    if (znargs == 3 || (znargs == 4 && zargs[3] == 0)) {
        uint16_t addr = zargs[2];
        std::string filename;

        for (size_t i = 0; i < user_byte(addr); i++) {
            uint8_t c = user_byte(addr + i + 1);

            // Convert to uppercase per §7.6.1.1; and according to the
            // description for @save in §15, the characters are ASCII.
            if (c >= 97 && c <= 122) {
                c -= 32;
            }

            filename.push_back(c);
        }

        if (!filename.empty()) {
            if (filename.find('.') == std::string::npos) {
                filename += ".AUX";
            }

            auto aux = zterp_os_aux_file(filename);
            if (aux != nullptr) {
                suggested = std::make_unique<std::string>(*aux);
            }
        }
    }

    // If there is a suggested filename and “prompt” is 1, this should
    // prompt the user with the suggested filename, but Glk doesn’t
    // support that.
    return std::make_shared<IO>(suggested.get(), mode, IO::Purpose::Data);
}

void zsave5()
{
    std::shared_ptr<IO> savefile;

    if (znargs == 0) {
        zsave();
        return;
    }

    ZASSERT(zargs[0] + zargs[1] < memory_size, "attempt to save beyond the end of memory");

    try {
        savefile = open_savefile(IO::Mode::WriteOnly);
    } catch (const IO::OpenError &) {
        store(0);
        return;
    }

    try {
        savefile->write_exact(&memory[zargs[0]], zargs[1]);
        store(1);
    } catch (const IO::IOError &) {
        store(0);
    }
}

void zrestore5()
{
    std::shared_ptr<IO> savefile;
    std::vector<uint8_t> buf;
    size_t n;

    if (znargs == 0) {
        zrestore();
        return;
    }

    if (zargs[1] == 0) {
        store(0);
        return;
    }

    try {
        savefile = open_savefile(IO::Mode::ReadOnly);
    } catch (const IO::OpenError &) {
        store(0);
        return;
    }

    try {
        buf.resize(zargs[1]);
    } catch (const std::bad_alloc &) {
        store(0);
        return;
    }

    n = savefile->read(buf.data(), zargs[1]);
    for (size_t i = 0; i < n; i++) {
        user_store_byte(zargs[0] + i, buf[i]);
    }

    store(n);
}

void zpiracy()
{
    branch_if(true);
}

#ifdef ZTERP_GLK
static void real_main()
#else
static void real_main(int argc, char **argv)
#endif
{
    struct {
        std::shared_ptr<IO> io;
        long offset = 0;
    } story;

    // strftime() is given the %c conversion specification, which is
    // locale-aware, so honor the user’s time locale.
    std::setlocale(LC_TIME, "");

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

    if (arg_status != ArgStatus::Ok) {
        help();
        if (arg_status == ArgStatus::Help) {
            throw Exit(0);
        } else {
            throw Exit(EXIT_FAILURE);
        }
    }

#ifndef ZTERP_GLK
    zterp_os_init_term();
#endif

    if (options.show_version) {
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

        auto config = zterp_os_rcfile(false);
        if (config != nullptr) {
            screen_printf("Configuration file: %s\n", config->c_str());
        } else {
            screen_printf("Cannot determine configuration file location\n");
        }

        throw Exit(0);
    }

    if (game_file.empty()) {
        help();
        die("no story provided");
    }

    try {
        story.io = std::make_shared<IO>(&game_file, IO::Mode::ReadOnly, IO::Purpose::Data);
    } catch (const IO::OpenError &) {
        die("cannot open file %s", game_file.c_str());
    }

    try {
        Blorb blorb = Blorb(story.io);

        const auto *chunk = blorb.find(Blorb::Usage::Exec, 0);
        if (chunk == nullptr) {
            die("no EXEC resource found");
        }
        if (chunk->type != IFF::TypeID(&"ZCOD")) {
            if (chunk->type == IFF::TypeID(&"GLUL")) {
                die("Glulx stories are not supported (try git or glulxe)");
            }

            die("unknown story type: %s (0x%08lx)", chunk->type.name().c_str(), static_cast<unsigned long>(chunk->type.val()));
        }

#if UINT32_MAX > LONG_MAX
        if (chunk->offset > LONG_MAX) {
            die("zcode offset too large");
        }
#endif

        memory_size = chunk->size;
        story.offset = chunk->offset;
    } catch (const Blorb::InvalidFile &) {
        long size = story.io->filesize();

        if (size == -1) {
            die("unable to determine file size");
        }
#if LONG_MAX > UINT32_MAX
        if (size > UINT32_MAX) {
            die("file too large");
        }
#endif

        memory_size = size;
        story.offset = 0;
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
    try {
        memory = new uint8_t[memory_size + 22];
    } catch (const std::bad_alloc &) {
        die("unable to allocate memory for story file");
    }
    memset(memory + memory_size, 0, 22);

    process_story(*story.io, story.offset);

    if (options.show_id) {
#ifdef ZTERP_GLK
        glk_set_style(style_Preformatted);
#endif
        screen_puts(story_id);
    } else {
        start_story();

        // If header transcript/fixed bits have been set, either by the
        // story or by the user, this will activate them.
        user_store_word(0x10, word(0x10));

        setup_opcodes();
        process_loop();
    }
}

#ifdef ZTERP_GLK
void glk_main()
#else
int main(int argc, char **argv)
#endif
{
#ifdef ZTERP_GLK
    try {
        real_main();
    } catch (const Exit &) {
#else
    try {
        real_main(argc, argv);
        return 0;
    } catch (const Exit &exit) {
        return exit.code();
#endif
    } catch (const std::exception &e) {
        std::cerr << "Unhandled exception: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Unhandled exception" << std::endl;
    }

#ifndef ZTERP_GLK
    return EXIT_FAILURE;
#endif
}
