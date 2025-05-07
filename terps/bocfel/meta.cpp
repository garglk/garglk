// Copyright 2013-2021 Chris Spiegel.
//
// SPDX-License-Identifier: MIT

#include <array>
#include <cstring>
#include <memory>
#include <new>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

#include "meta.h"
#include "io.h"
#include "memory.h"
#include "objects.h"
#include "options.h"
#include "osdep.h"
#include "process.h"
#include "screen.h"
#include "stack.h"
#include "stash.h"
#include "types.h"
#include "unicode.h"
#include "util.h"
#include "zterp.h"

using namespace std::literals;

static bool uni_isdigit(char c)
{
    return c >= '0' && c <= '9';
}

static bool uni_isxdigit(char c)
{
    return "0123456789abcdefg"s.find(c) != std::string::npos;
}

static void try_user_save(const char *desc)
{
    if (in_interrupt()) {
        screen_puts("[Cannot save while in an interrupt]");
    } else {
        if (push_save(SaveStackType::User, SaveType::Meta, SaveOpcode::Read, desc) == SaveResult::Success) {
            screen_puts("[Save pushed]");
        } else {
            screen_puts("[Save failed]");
        }
    }
}

// On success, this does not return.
static void try_user_restore(size_t i)
{
    SaveOpcode saveopcode;

    if (pop_save(SaveStackType::User, i, saveopcode)) {
        screen_message_prompt("[Restored]");
        throw Operation::Restore(saveopcode);
    } else {
        screen_puts("[Restore failed]");
    }
}

static void try_user_drop(size_t i)
{
    if (drop_save(SaveStackType::User, i + 1)) {
        screen_puts("[Dropped]");
    } else {
        screen_puts("[Drop failed]");
    }
}

static std::vector<uint8_t> debug_change_memory;
static std::set<uint16_t> debug_change_invalid;

static void meta_debug_change_start()
{
    debug_change_memory.assign(memory, memory + header.static_start);
    debug_change_invalid.clear();
    screen_puts("[Debug change reset]");
}

static void meta_debug_change_inc_dec(const std::string &string)
{
    if (debug_change_memory.empty()) {
        screen_puts("[Debug change not started]");
    } else {
        bool saw_change = false;

        for (uint16_t addr = 0; addr < header.static_start - 2; addr++) {
            if (debug_change_invalid.find(addr) == debug_change_invalid.end()) {
                int16_t newval = as_signed(word(addr));
                int16_t oldval = as_signed((debug_change_memory[addr] << 8) | debug_change_memory[addr + 1]);

#define CMP(a, b) (string == "inc" ? (a) > (b) : (a) < (b))
                if (CMP(newval, oldval)) {
#undef CMP
                    // The Z-machine does not require aligned memory access, so
                    // both even and odd addresses must be checked. However,
                    // global variables are word-sized, so if an address inside
                    // the global variables has changed, report only if the
                    // address is the base of globals plus a multiple of two.
                    if (is_global(addr) || !in_globals(addr)) {
                        screen_printf("%s: %ld -> %ld\n", addrstring(addr).c_str(), static_cast<long>(oldval), static_cast<long>(newval));
                        saw_change = true;
                    }
                } else {
                    debug_change_invalid.insert(addr);
                }
            }
        }

        if (!saw_change) {
            screen_puts("[No changes]");
        }

        std::memcpy(debug_change_memory.data(), memory, header.static_start);
    }
}

static bool meta_debug_change(const std::string &string)
{
    if (string == "start") {
        meta_debug_change_start();
    } else if (string == "inc" || string == "dec") {
        meta_debug_change_inc_dec(string);
    } else {
        return false;
    }

    return true;
}

static bool meta_debug_scan(const std::string &string)
{
    static std::set<uint16_t> invalid_addr;

    if (string == "start") {
        invalid_addr.clear();
        screen_puts("[Debug scan reset]");
    } else if (string == "show") {
        for (uint16_t addr = 0; addr < header.static_start - 2; addr++) {
            if (invalid_addr.find(addr) == invalid_addr.end()) {
                if (is_global(addr) || !in_globals(addr)) {
                    screen_puts(addrstring(addr));
                }
            }
        }
    } else if (uni_isdigit(string[0]) || (string[0] == '-' && uni_isdigit(string[1]))) {
        long value;
        bool valid;

        value = parseint(string, 0, valid);
        if (!valid) {
            return false;
        }

        if (value < -0x8000 || value > 0xffff) {
            screen_puts("[Value is outside the range of a 16-bit integer and will never be found]");
        } else {
            size_t count = 0;

            if (value < 0) {
                value += 0x10000;
            }

            for (uint16_t addr = 0; addr < header.static_start - 2; addr++) {
                if (word(addr) != value) {
                    invalid_addr.insert(addr);
                }
                if (invalid_addr.find(addr) == invalid_addr.end()) {
                    count++;
                }
            }

            screen_printf("[%zu location%s]\n", count, count == 1 ? "" : "s");
        }
    } else {
        return false;
    }

    return true;
}

static long parse_address(const std::string &string, bool &valid)
{
    long addr;

    if (string[0] == 'G' && uni_isxdigit(string[1]) && uni_isxdigit(string[2]) && string[3] == 0) {
        addr = parseint(&string[1], 16, valid);
        if (addr < 0 || addr > 239) {
            valid = false;
        }
        addr = header.globals + (2 * addr);
    } else {
        addr = parseint(string, 16, valid);
    }

    return addr;
}

static bool validate_address(long addr, bool print)
{
    if (addr < 0 || addr > memory_size - 2) {
        if (print) {
            screen_printf("[Address out of range: must be [0, 0x%lx]]\n", static_cast<unsigned long>(memory_size) - 2);
        }
        return false;
    }

    return true;
}

static bool meta_debug_print(const std::string &string)
{
    long addr;
    bool valid;

    addr = parse_address(string, valid);

    if (!valid) {
        return false;
    }
    if (!validate_address(addr, true)) {
        return true;
    }

    screen_printf("%ld (0x%04lx)\n", static_cast<long>(as_signed(word(addr))), static_cast<unsigned long>(word(addr)));

    return true;
}

#ifndef ZTERP_NO_CHEAT
// This is a lookup table of pairs (frozen, value), where the specified
// address (as the index) is frozen to “value” if “frozen” is true.
static std::array<std::pair<bool, uint16_t>, UINT16_MAX + 1> frozen_addresses;

bool cheat_add(const std::string &how, bool print)
{
    std::istringstream ss(how);
    std::string type, addrstr, valstr;

    if (!std::getline(ss, type, ':') ||
        !std::getline(ss, addrstr, ':') ||
        !std::getline(ss, valstr))
    {
        return false;
    }

    if (type == "freeze" || type == "freezew") {
        long addr;
        long value;
        bool valid;

        addr = parse_address(addrstr, valid);
        if (!valid) {
            return false;
        }
        if (!validate_address(addr, print)) {
            return true;
        }

        value = parseint(valstr, 0, valid);
        if (!valid) {
            return false;
        }

        if (value < 0 || value > UINT16_MAX) {
            if (print) {
                screen_puts("[Values must be in the range [0, 65535]]");
            }
            return true;
        }

        frozen_addresses[addr] = {true, value};
    } else {
        return false;
    }

    if (print) {
        screen_puts("[Frozen]");
    }

    return true;
}

static bool cheat_remove(uint16_t addr)
{
    if (!frozen_addresses[addr].first) {
        return false;
    }

    frozen_addresses[addr] = {false, 0};
    return true;
}

bool cheat_find_freeze(uint32_t addr, uint16_t &val)
{
    if (addr > UINT16_MAX || !frozen_addresses[addr].first) {
        return false;
    }

    val = frozen_addresses[addr].second;

    return true;
}

static bool meta_debug_freeze(const std::string &string)
{
    std::istringstream ss(string);
    std::string addrstr, valstr;

    if (!std::getline(ss, addrstr, ' ') ||
        !std::getline(ss, valstr))
    {
        return false;
    }

    std::string cheat = "freeze:" + addrstr + ":" + valstr;

    return cheat_add(cheat, true);
}

static bool meta_debug_unfreeze(const std::string &string)
{
    long addr;
    bool valid;

    addr = parse_address(string, valid);
    if (!valid) {
        return false;
    }
    if (!validate_address(addr, true)) {
        return true;
    }

    if (cheat_remove(addr)) {
        screen_puts("[Unfrozen]");
    } else {
        screen_puts("[Address not frozen]");
    }

    return true;
}

static bool meta_debug_show_freeze()
{
    bool any_frozen = false;

    for (size_t addr = 0; addr < frozen_addresses.size(); addr++) {
        if (frozen_addresses[addr].first) {
            any_frozen = true;
            screen_printf("%s: %lu\n", addrstring(addr).c_str(), static_cast<unsigned long>(frozen_addresses[addr].second));
        }
    }

    if (!any_frozen) {
        screen_puts("[No frozen values]");
    }

    return true;
}
#endif

#ifndef ZTERP_NO_WATCHPOINTS
static std::array<bool, UINT16_MAX + 1> watch_addresses;

static void watch_add(uint16_t addr)
{
    watch_addresses[addr] = true;
}

static void watch_all()
{
    watch_addresses.fill(true);
}

static bool watch_remove(uint16_t addr)
{
    if (watch_addresses[addr]) {
        watch_addresses[addr] = false;
        return true;
    } else {
        return false;
    }
}

static void watch_none()
{
    watch_addresses.fill(false);
}

void watch_check(uint16_t addr, unsigned long oldval, unsigned long newval)
{
    if (watch_addresses[addr] && oldval != newval) {
        screen_printf("[%s changed: %lu -> %lu (pc = 0x%lx)]\n", addrstring(addr).c_str(), oldval, newval, current_instruction);
    }
}

static bool meta_debug_watch_helper(const std::string &string, bool do_watch)
{
    if (string == "all") {
        if (do_watch) {
            watch_all();
            screen_puts("[Watching all addresses for changes]");
        } else {
            watch_none();
            screen_puts("[Not watching any addresses for changes]");
        }
    } else {
        long addr;
        bool valid;

        addr = parse_address(string, valid);

        if (!valid) {
            return false;
        }
        if (!validate_address(addr, true)) {
            return true;
        }

        if (do_watch) {
            watch_add(addr);
            screen_printf("[Watching %s for changes]\n", addrstring(addr).c_str());
        } else {
            if (watch_remove(addr)) {
                screen_printf("[No longer watching %s for changes]\n", addrstring(addr).c_str());
            } else {
                screen_printf("[%s is not currently being watched]\n", addrstring(addr).c_str());
            }
        }
    }

    return true;
}

static bool meta_debug_watch(const std::string &string)
{
    return meta_debug_watch_helper(string, true);
}

static bool meta_debug_unwatch(const std::string &string)
{
    return meta_debug_watch_helper(string, false);
}

static bool meta_debug_show_watch()
{
    bool any_watched = false;

    for (size_t addr = 0; addr < watch_addresses.size(); addr++) {
        if (watch_addresses[addr]) {
            any_watched = true;
            screen_puts(addrstring(addr));
        }
    }

    if (!any_watched) {
        screen_puts("[No watched values]");
    }

    return true;
}
#endif

static void meta_debug_help()
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
            "unwatch all: stop watching all addresses\n"
            "show_watch: show all watched-for addresses\n"
#endif
            "\nAddresses are either absolute, specified in hexadecimal with an optional leading 0x, or global variables. Global variables have the syntax Gxx, where xx is a hexadecimal value in the range [00, ef], corresponding to global variables 0 to 239.\n"
            );
}

static void meta_debug(const std::string &string)
{
    bool ok = false;
    std::istringstream ss(rtrim(string));
    std::string cmd, args;

    ss >> cmd;
    std::getline(ss >> std::ws, args);

    if (cmd == "change") {
        ok = meta_debug_change(args);
    } else if (cmd == "scan") {
        ok = meta_debug_scan(args);
    } else if (cmd == "print") {
        ok = meta_debug_print(args);
    }
#ifndef ZTERP_NO_CHEAT
    else if (cmd == "freeze") {
        ok = meta_debug_freeze(args);
    } else if (cmd == "unfreeze") {
        ok = meta_debug_unfreeze(args);
    } else if (string == "show_freeze") {
        ok = meta_debug_show_freeze();
    }
#endif
#ifndef ZTERP_NO_WATCHPOINTS
    else if (cmd == "watch") {
        ok = meta_debug_watch(args);
    } else if (cmd == "unwatch") {
        ok = meta_debug_unwatch(args);
    } else if (string == "show_watch") {
        ok = meta_debug_show_watch();
    }
#endif

    if (!ok) {
        meta_debug_help();
    }
}

static void meta_info()
{
    screen_printf("File: %s\n", game_file.c_str());
    screen_printf("Z-machine version: %d\n", memory[0]);
    screen_printf("ID: %s\n", get_story_id().c_str());
}

static std::vector<char> meta_notes;

IFF::TypeID meta_write_bfnt(IO &savefile)
{
    if (meta_notes.empty()) {
        return IFF::TypeID();
    }

    savefile.write32(0); // Version
    savefile.write_exact(meta_notes.data(), meta_notes.size());

    return IFF::TypeID("Bfnt");
}

void meta_read_bfnt(IO &io, uint32_t size)
{
    uint32_t version;

    meta_notes.clear();

    if (size < 4) {
        show_message("Corrupted Bfnt entry (too small)");
        return;
    }

    try {
        version = io.read32();
    } catch (const IO::IOError &) {
        show_message("Unable to read notes size from save file");
        return;
    }

    if (version != 0) {
        show_message("Unsupported Bfnt version %lu", static_cast<unsigned long>(version));
        return;
    }

    size -= 4;

    if (size > 0) {
        try {
            meta_notes.resize(size);
        } catch (const std::bad_alloc &) {
            show_message("Unable to allocate memory for notes (requested %lu bytes)", static_cast<unsigned long>(size));
            return;
        }

        try {
            io.read_exact(meta_notes.data(), size);
        } catch (const IO::IOError &) {
            meta_notes.clear();
            show_message("Unable to read notes from save file");
            return;
        }
    }
}

class NotesStasher : public Stasher {
public:
    void backup() override {
        m_notes = meta_notes;
    }

    bool restore() override {
        meta_notes = m_notes;

        return true;
    }

    void free() override {
        m_notes.clear();
    }

private:
    std::vector<char> m_notes;
};

// Try to parse a meta command. If input should be re-requested, return
// <MetaResult::Rerequest, "">. If a portion of the passed-in string
// should be processed as user input, return <MetaResult::Say, string>.
//
// There is also the possibility that a meta-command causes a saved game
// of some kind to be restored (/undo, /restore, and /pop). In these
// cases Operation::Restore is thrown, so nothing is returned.
std::pair<MetaResult, std::string> handle_meta_command(const uint16_t *string, uint8_t len)
{
    std::string command, rest;
    std::string converted;

    // Convert the input string to UTF-8 using memory-backed I/O.
    try {
        IO io(std::vector<uint8_t>(), IO::Mode::WriteOnly);

        for (size_t i = 0; i < len; i++) {
            try {
                io.putc(string[i]);
            } catch (const IO::IOError &) {
                return {MetaResult::Rerequest, ""};
            }
        }

        const auto &buf = io.get_memory();
        converted = rtrim(std::string(buf.begin(), buf.end()));
    } catch (const IO::OpenError &) {
        return {MetaResult::Rerequest, ""};
    }

    std::istringstream ss(converted);
    ss >> command;
    std::getline(ss >> std::ws, rest);

    // Require a leading slash.
    if (command[0] != '/') {
        return {MetaResult::Rerequest, ""};
    };
    command.erase(0, 1);

#define ZEROARG(name) (command == (name)) if (!rest.empty()) { screen_printf("[/%s takes no arguments]\n", name); } else

    if ZEROARG("undo") {
        SaveOpcode saveopcode;
        if (pop_save(SaveStackType::Game, 0, saveopcode)) {
            if (seen_save_undo) {
                store(2);
            } else {
                screen_message_prompt("[Undone]");
            }

            throw Operation::Restore(saveopcode);
        } else {
            screen_puts("[No save found, unable to undo]");
        }
    } else if ZEROARG("scripton") {
        if (output_stream(OSTREAM_TRANSCRIPT, 0)) {
            screen_puts("[Transcripting on]");
        } else {
            screen_puts("[Transcripting failed]");
        }
    } else if ZEROARG("scriptoff") {
        output_stream(-OSTREAM_TRANSCRIPT, 0);
        screen_puts("[Transcripting off]");
    } else if ZEROARG("recon") {
        if (output_stream(OSTREAM_RECORD, 0)) {
            screen_puts("[Command recording on]");
        } else {
            screen_puts("[Command recording failed]");
        }
    } else if ZEROARG("recoff") {
        output_stream(-OSTREAM_RECORD, 0);
        screen_puts("[Command recording off]");
    } else if ZEROARG("replay") {
        if (input_stream(ISTREAM_FILE)) {
            screen_puts("[Replaying commands]");
        } else {
            screen_puts("[Replaying commands failed]");
        }
    } else if ZEROARG("save") {
        if (in_interrupt()) {
            screen_puts("[Cannot call /save while in an interrupt]");
        } else {
            if (do_save(SaveType::Meta, SaveOpcode::Read)) {
                screen_puts("[Saved]");
            } else {
                screen_puts("[Save failed]");
            }
        }
    } else if ZEROARG("restore") {
        SaveOpcode saveopcode;

        if (do_restore(SaveType::Meta, saveopcode)) {
            screen_message_prompt("[Restored]");
            throw Operation::Restore(saveopcode);
        } else {
            screen_puts("[Restore failed]");
        }
    } else if (command == "ps") {
        if (rest.empty()) {
            try_user_save(nullptr);
        } else {
            try_user_save(rest.c_str());
        }
    } else if (command == "pop" || command == "drop") {
        void (*restore_or_drop)(size_t) = command == "pop" ? try_user_restore : try_user_drop;

        if (restore_or_drop == try_user_drop && rest == "all") {
            while (drop_save(SaveStackType::User, 1)) {
            }

            screen_puts("[All saves dropped]");
        } else if (rest[0] == 0) {
            restore_or_drop(0);
        } else {
            long saveno;
            bool valid;

            saveno = parseint(rest, 10, valid);
            if (!valid || saveno < 1) {
                screen_puts("[Invalid index]");
                return {MetaResult::Rerequest, ""};
            }

            restore_or_drop(saveno - 1);
        }
    } else if ZEROARG("ls") {
        list_saves(SaveStackType::User);
    } else if ZEROARG("savetranscript") {
        screen_save_persistent_transcript();
    } else if ZEROARG("showtranscript") {
        screen_show_persistent_transcript();
    } else if ZEROARG("notes") {
        screen_puts("[Editing notes]");
        screen_flush();
        try {
            meta_notes = zterp_os_edit_notes(meta_notes);
            screen_puts("[Notes saved]");
        } catch (const std::runtime_error &e) {
            screen_printf("[Error updating notes: %s]\n", e.what());
        }
    } else if ZEROARG("shownotes") {
        if (meta_notes.empty()) {
            screen_puts("[No notes taken]");
        } else {
            screen_puts("[Start of notes]");
            screen_puts(std::string(meta_notes.data(), meta_notes.size()));
            screen_puts("[End of notes]");
        }
    } else if ZEROARG("savenotes") {
        if (meta_notes.empty()) {
            screen_puts("[No notes taken]");
        } else {
            try {
                IO io(nullptr, IO::Mode::WriteOnly, IO::Purpose::Data);
                try {
                    io.write_exact(meta_notes.data(), meta_notes.size());
                    screen_puts("[Saved notes to file]");
                } catch (const IO::IOError &) {
                    screen_puts("[Unable to write notes file]");
                }
            } catch (const IO::OpenError &) {
                screen_puts("[Unable to open notes file]");
            }
        }
    } else if ZEROARG("status") {
        if (zversion > 3) {
            screen_puts("[/status is only available in V1, V2, and V3]");
        } else {
            long first = as_signed(variable(0x11)), second = as_signed(variable(0x12));

            print_object(variable(0x10), [](uint8_t c) {
                screen_putc(zscii_to_unicode[c]);
            });

            screen_print("\n");

            if (status_is_time()) {
                auto fmt = screen_format_time(first, second);
                screen_puts(fmt);
            } else {
                if (is_game(Game::Planetfall) || is_game(Game::Stationfall)) {
                    screen_printf("Score: %ld\nTime: %ld\n", first, second);
                } else {
                    screen_printf("Score: %ld\nMoves: %ld\n", first, second);
                }
            }
        }
    } else if ZEROARG("disable") {
        options.disable_meta_commands = true;
        screen_puts("[Meta commands disabled]");
    } else if (command == "say") {
        return {MetaResult::Say, rest};
    } else if ZEROARG("config") {
        auto config_file = zterp_os_rcfile(true);
        if (config_file != nullptr) {
            screen_puts("[Editing configuration file]");
            screen_flush();
            try {
                zterp_os_edit_file(*config_file);
                screen_puts("[Done; changes will be reflected after Bocfel is restarted]");
            } catch (const std::runtime_error &e) {
                screen_printf("[Error editing configuration file: %s]\n", e.what());
            }
        } else {
            screen_puts("[Cannot determine configuration file location]");
        }
    } else if ZEROARG("fixed") {
        if (screen_toggle_force_fixed()) {
            screen_puts("[Force fixed font on]");
        } else {
            screen_puts("[Force fixed font off]");
        }
    } else if (command == "debug") {
        meta_debug(rest);
    } else if ZEROARG("quit") {
        zquit();
    } else if ZEROARG("info") {
        meta_info();
    } else {
        if (command != "help") {
            screen_printf("Unknown command: /%s\n\n", command.c_str());
        }

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
                "/savetranscript: save persistent transcript (if active) to a file\n"
                "/notes: open a text editor to take notes\n"
                "/shownotes: display notes taken, if any\n"
                "/savenotes: save notes taken, if any, to a file\n"
                "/status: display the status line (V1, V2, and V3 games only)\n"
                "/disable: disable these commands for the rest of this session\n"
                "/say [command]: pretend like [command] was typed\n"
                "/config: open the config file in a text editor\n"
                "/fixed: toggle forced fixed-width font in the main window\n"
                "/debug [...]: perform a debugging operation\n"
                "/quit: quit immediately (as if the @quit opcode were executed)\n"
                "/info: display some info about the current game\n"
                );
    }

    return {MetaResult::Rerequest, ""};
}

void init_meta(bool first_run)
{
    if (first_run) {
        stash_register(std::make_unique<NotesStasher>());
    }
}
