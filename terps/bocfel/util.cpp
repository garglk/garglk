// Copyright 2010-2021 Chris Spiegel.
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

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "util.h"
#include "process.h"
#include "screen.h"
#include "types.h"
#include "unicode.h"
#include "zterp.h"

#ifdef ZTERP_GLK
#include <glk.h>
#endif

#ifndef ZTERP_NO_SAFETY_CHECKS
void assert_fail(const char *fmt, ...)
{
    va_list ap;
    std::string str;

    va_start(ap, fmt);
    str = vstring(fmt, ap);
    va_end(ap);

    str += fstring(" (pc = 0x%lx)", current_instruction);

    die("%s", str.c_str());
}
#endif

void warning(const char *fmt, ...)
{
    va_list ap;
    std::string str;

    va_start(ap, fmt);
    str = vstring(fmt, ap);
    va_end(ap);

    show_message("Warning: %s", str.c_str());
}

void die(const char *fmt, ...)
{
    va_list ap;
    std::string str;

    va_start(ap, fmt);
    str = vstring(fmt, ap);
    va_end(ap);

    show_message("Fatal error: %s", str.c_str());

#ifdef GARGLK
    std::cerr << "Fatal error: " << str << std::endl;
#endif

    throw Exit(EXIT_FAILURE);
}

enum FakeArg { glkunix_arg_NoValue, glkunix_arg_NumberValue, glkunix_arg_ValueFollows };

void help()
{
    // Simulate a glkunix_argumentlist_t structure so help can be shared.
    struct FakeArgList {
        const char *name;
        FakeArg type;
        const char *description;
    };

    const std::vector<FakeArgList> args = {
#include "help.h"
    };

#ifdef ZTERP_GLK
    glk_set_style(style_Preformatted);
#endif

    screen_puts("Usage: bocfel [args] filename");
    for (const auto &arg : args) {
        std::string line;
        const char *typestr;

        switch (arg.type) {
        case glkunix_arg_NumberValue:  typestr = "number"; break;
        case glkunix_arg_ValueFollows: typestr = "string"; break;
        default:                       typestr = "";       break;
        }

        line = fstring("%s %-12s%s", arg.name, typestr, arg.description);
        screen_puts(line.c_str());
    }
}

// This is not POSIX compliant, but it gets the job done.
// It should not be called more than once.
static int zoptind = 0;
static const char *zoptarg;
static int zgetopt(int argc, char **argv, const char *optstring)
{
    static const char *p = "";
    const char *optp;
    int c;

    if (*p == 0) {
        // No more arguments.
        if (++zoptind >= argc) {
            return -1;
        }

        p = argv[zoptind];

        // No more options.
        if (p[0] != '-' || p[1] == 0) {
            return -1;
        }

        // Handle “--”
        if (*++p == '-') {
            zoptind++;
            return -1;
        }
    }

    c = *p++;

    optp = std::strchr(optstring, c);
    if (optp == nullptr) {
        return '?';
    }

    if (optp[1] == ':') {
        if (*p != 0) {
            zoptarg = p;
        } else {
            zoptarg = argv[++zoptind];
        }

        p = "";
        if (zoptarg == nullptr) {
            return '?';
        }
    }

    return c;
}

ArgStatus arg_status = ArgStatus::Ok;
void process_arguments(int argc, char **argv)
{
    int c;

    while ( (c = zgetopt(argc, argv, "a:A:cCdDeE:fFgGhHikl:mn:N:prR:sS:tT:u:vxXyYz:Z:")) != -1 ) {
        switch (c) {
        case 'a':
            options.eval_stack_size = std::strtoul(zoptarg, nullptr, 10);
            break;
        case 'A':
            options.call_stack_size = std::strtoul(zoptarg, nullptr, 10);
            break;
        case 'c':
            options.disable_color = true;
            break;
        case 'C':
            options.disable_config = true;
            break;
        case 'd':
            options.disable_timed = true;
            break;
        case 'D':
            options.disable_sound = true;
            break;
        case 'e':
            options.enable_escape = true;
            break;
        case 'E':
            options.escape_string = std::make_unique<std::string>(zoptarg);
            break;
        case 'f':
            options.disable_fixed = true;
            break;
        case 'F':
            options.assume_fixed = true;
            break;
        case 'g':
            options.disable_graphics_font = true;
            break;
        case 'G':
            options.enable_alt_graphics = true;
            break;
        case 'h':
            arg_status = ArgStatus::Help;
            return;
        case 'H':
            options.disable_history_playback = true;
            break;
        case 'i':
            options.show_id = true;
            break;
        case 'k':
            options.disable_term_keys = true;
            break;
        case 'l':
            options.username = std::make_unique<std::string>(zoptarg);
            break;
        case 'm':
            options.disable_meta_commands = true;
            break;
        case 'n':
            options.int_number = std::strtoul(zoptarg, nullptr, 10);
            break;
        case 'N':
            options.int_version = zoptarg[0];
            break;
        case 'p':
            options.disable_patches = true;
            break;
        case 'r':
            options.replay_on = true;
            break;
        case 'R':
            options.replay_name = std::make_unique<std::string>(zoptarg);
            break;
        case 's':
            options.record_on = true;
            break;
        case 'S':
            options.record_name = std::make_unique<std::string>(zoptarg);
            break;
        case 't':
            options.transcript_on = true;
            break;
        case 'T':
            options.transcript_name = std::make_unique<std::string>(zoptarg);
            break;
        case 'u':
            options.max_saves = std::strtoul(zoptarg, nullptr, 10);
            break;
        case 'v':
            options.show_version = true;
            break;
        case 'x':
            options.disable_abbreviations = true;
            break;
        case 'X':
            options.enable_censorship = true;
            break;
        case 'y':
            options.overwrite_transcript = true;
            break;
        case 'Y':
            options.override_undo = true;
            break;
        case 'z':
            options.random_seed = std::make_unique<unsigned long>(std::strtoul(zoptarg, nullptr, 10));
            break;
        case 'Z':
            options.random_device = std::make_unique<std::string>(zoptarg);
            break;
        default:
            arg_status = ArgStatus::Fail;
            return;
        }
    }

    // Just ignore excess stories for now.
    if (zoptind < argc) {
        game_file = argv[zoptind];
    }
}

long parseint(const std::string &s, int base, bool &valid)
{
    long ret;
    char *endptr;
    const char *cstr = s.c_str();

    ret = std::strtol(cstr, &endptr, base);
    valid = endptr != cstr && *endptr == 0;

    return ret;
}

std::string vstring(const char *fmt, std::va_list ap)
{
    std::va_list ap_copy;
    std::string s;
    size_t n;

    va_copy(ap_copy, ap);

    n = std::vsnprintf(nullptr, 0, fmt, ap);

    s.resize(n);
    vsnprintf(&s[0], n + 1, fmt, ap_copy);

    va_end(ap_copy);

    return s;
}

std::string fstring(const char *fmt, ...)
{
    std::va_list ap;
    std::string s;

    va_start(ap, fmt);
    s = vstring(fmt, ap);
    va_end(ap);

    return s;
}

std::string ltrim(const std::string &s)
{
    auto pos = s.find_first_not_of(" \t\r");

    if (pos != std::string::npos) {
        return s.substr(pos);
    } else {
        return "";
    }
}

std::string rtrim(const std::string &s)
{
    auto pos = s.find_last_not_of(" \t\r");

    if (pos != std::string::npos) {
        return s.substr(0, pos + 1);
    } else {
        return s;
    }
}

std::string operator"" _s(const char *s, size_t len)
{
    return {s, len};
}
