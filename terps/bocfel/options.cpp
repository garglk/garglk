// Copyright 2009-2023 Chris Spiegel.
//
// SPDX-License-Identifier: MIT

#include <algorithm>
#include <bitset>
#include <cctype>
#include <cerrno>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <utility>

#ifdef ZTERP_GLK_UNIX
extern "C" {
#include <glk.h>
#include <glkstart.h>
}
#endif

#include "meta.h"
#include "options.h"
#include "osdep.h"
#include "patches.h"
#include "screen.h"
#include "util.h"
#include "zterp.h"

using namespace std::literals;

struct ParseError : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct Range {
    Range() = default;

    Range(unsigned long min_, unsigned long max_) :
        min(min_),
        max(max_)
    {
    }

    // The default max is the largest 32-bit power of 10 (1 billion).
    // This looks a bit nicer to the end-user than a power of two, and
    // is large enough that it will never be a hindrance. In addition,
    // it’s predictable: ULONG_MAX can differ across platforms, so it’s
    // better to avoid it as the max.
    unsigned long min = 0;
    unsigned long max = 1000000000UL;
};

Options options;
std::bitset<3> arg_status;

static Parser bool_helper(bool &target)
{
    return [&target](const std::string &val) {
        if (val == "0") {
            target = false;
        } else if (val == "1") {
            target = true;
        } else {
            throw ParseError("must be 0 or 1");
        }
    };
}

template <typename T>
static Parser number_helper(Range range, T &target, std::function<T(unsigned long)> wrapper)
{
    return [range, &target, wrapper = std::move(wrapper)](const std::string &val) {
        char *endptr;
        errno = 0;
        auto n = std::strtoul(val.c_str(), &endptr, 10);
        if (val[0] == '-' || *endptr != 0) {
            throw ParseError("must be a non-negative integral number");
        } else if ((n == ULONG_MAX && errno == ERANGE) || n < range.min || n > range.max) {
            const std::string msg = "must be an integral number between " + std::to_string(range.min) + " and " + std::to_string(range.max) + " inclusive";
            throw ParseError(msg);
        } else {
            target = wrapper(n);
        }
    };
}

static Parser float_helper(double &target)
{
    return [&target](const std::string &val) {
        char *endptr;
        errno = 0;
        auto n = std::strtod(val.c_str(), &endptr);
        if (val.find_first_of("eE") != std::string::npos || val[0] == '-' || *endptr != 0) {
            throw ParseError("must be a non-negative number");
        } else if (errno == ERANGE) {
            throw ParseError("floating point value out of range");
        } else {
            switch (std::fpclassify(n)) {
            case FP_NORMAL: case FP_ZERO:
                target = n;
                break;
            case FP_NAN:
                throw ParseError("not a number");
            case FP_INFINITE:
                throw ParseError("is infinite");
            case FP_SUBNORMAL:
                throw ParseError("is subnormal");
            default:
                throw ParseError("unknown classification");
            }
        }
    };
}

static Parser char_helper(unsigned char &target)
{
    return [&target](const std::string &val) {
        if (val.size() != 1) {
            throw ParseError("must be a single character");
        } else {
            target = val[0];
        }
    };
}

#ifdef GLK_MODULE_GARGLKTEXT
static Parser color_helper(int num)
{
    return [num](const std::string &val) {
        char *endptr;
        auto color = std::strtoul(val.c_str(), &endptr, 16);
        if (val[0] == '-' || *endptr != 0 || color > 0xffffff) {
            throw ParseError("must be a hexadecimal number ranging from 0x000000 to 0xffffff");
        } else {
            update_color(num, color);
        }
    };
}
#endif

#define BOOL(opt, desc, from_config, name)		add_parser(opt, desc, from_config, #name, bool_helper(name), OptValue::Type::Flag)
#define NUMBER(opt, desc, from_config, name, range)	add_parser(opt, desc, from_config, #name, number_helper<unsigned long>(range, name, [](unsigned long n) { return n; }), OptValue::Type::Number)
#define OPTNUM(opt, desc, from_config, name, range)	add_parser(opt, desc, from_config, #name, number_helper<std::unique_ptr<unsigned long>>(range, name, [](unsigned long n) { return std::make_unique<unsigned long>(n); }), OptValue::Type::Number)
#define FLOAT(opt, desc, from_config, name)		add_parser(opt, desc, from_config, #name, float_helper(name), OptValue::Type::Value)
#define STRING(opt, desc, from_config, name)		add_parser(opt, desc, from_config, #name, [this](const std::string &val) { name = std::make_unique<std::string>(val); }, OptValue::Type::Value)
#define CHAR(opt, desc, from_config, name)		add_parser(opt, desc, from_config, #name, char_helper(name), OptValue::Type::Value)
#ifdef GLK_MODULE_GARGLKTEXT
#define COLOR(name, num)				add_parser(0, "", true, "color_" #name, color_helper(num), OptValue::Type::Value)
#else
#define COLOR(name, num)
#endif

// Some configuration options are available only in certain compile-time
// configurations. For those options, silently ignore them in
// configurations where they are not valid. This allows configuration
// files to be shared between such builds without dispaying diagnostics
// for options which are valid in some builds but not others.

Options::Options() {
    NUMBER('a', "Set the evaluation stack size", true, eval_stack_size, Range());
    NUMBER('A', "Set the call stack size", true, call_stack_size, Range(0, 65535));
    BOOL  ('c', "Disable colors", true, disable_color);
    BOOL  ('C', "Ignore the configuration file", false, disable_config);
    BOOL  ('d', "Disable timed input", true, disable_timed);
    BOOL  ('D', "Disable sound effects", true, disable_sound);
    BOOL  ('e', "Enable ANSI escape sequences in the transcript", true, enable_escape);
    STRING('E', "Set the escape string used to decorate transcripts (via -e)", true, escape_string);
    BOOL  ('f', "Disable fixed-width fonts", true, disable_fixed);
    BOOL  ('F', "Assume the default font is fixed-width", true, assume_fixed);
    BOOL  ('g', "Disable the character graphics font", true, disable_graphics_font);
    BOOL  ('G', "Select alternate box-drawing characters", true, enable_alt_graphics);
    BOOL  ('h', "Display this help message then exit", false, show_help);
    BOOL  ('H', "Disable history playback on restore", true, disable_history_playback);
    BOOL  ('i', "Print the ID of the specified game then exit", false, show_id);
    BOOL  ('k', "Disable terminating keys", true, disable_term_keys);
    STRING('l', "Set the username", true, username);
    BOOL  ('m', "Disable meta commands", true, disable_meta_commands);
    NUMBER('n', "Set the interpreter number (see 11.1.3 in The Z-machine Standards Document 1.1)", true, int_number, Range(1, 11));
    CHAR  ('N', "Set the interpreter version to the single-character version (see 11.1.3.1 in The Z-machine Standards Document 1.1)", true, int_version);
    BOOL  ('p', "Disable the application of patches", true, disable_patches);
    BOOL  ('r', "Play back a command record", true, replay_on);
    STRING('R', "Specify the filename for the command record replay", true, replay_name);
    BOOL  ('s', "Turn on command recording", true, record_on);
    STRING('S', "Specify the filename for the command record", true, record_name);
    BOOL  ('t', "Turn on transcripting", true, transcript_on);
    STRING('T', "Specify the filename for the transcript", true, transcript_name);
    NUMBER('u', "Set the maximum number of UNDO slots", true, undo_slots, Range());
    BOOL  ('v', "Display version and configuration information then exit", false, show_version);
    BOOL  ('x', "Disable abbreviations (x, g, z, o) provided by Bocfel for old Infocom games", true, disable_abbreviations);
    BOOL  ('X', "Turn on the Tandy/censorship flag", true, enable_censorship);
    BOOL  ('y', "Overwrite transcripts rather than appending to them", true, overwrite_transcript);
    BOOL  ('Y', "Use internal UNDO handling code only", true, override_undo);
    OPTNUM('z', "Provide a seed for the PRNG", true, random_seed, Range(0, UINT32_MAX));
    STRING('Z', "Provide a file/device from which 32 bits will be read as a seed", true, random_device);

    BOOL  (0, "", true, autosave);
    BOOL  (0, "", true, persistent_transcript);
    STRING(0, "", true, editor);
    BOOL  (0, "", true, warn_on_v6);
    BOOL  (0, "", true, redirect_v6_windows);
    BOOL  (0, "", true, disable_v6_hacks);
    FLOAT (0, "", true, v6_hack_max_scale);

    COLOR(black,   2);
    COLOR(red,     3);
    COLOR(green,   4);
    COLOR(yellow,  5);
    COLOR(blue,    6);
    COLOR(magenta, 7);
    COLOR(cyan,    8);
    COLOR(white,   9);

    m_from_config.insert({"cheat", [](const std::string &val) {
#ifndef ZTERP_NO_CHEAT
        if (!cheat_add(val, false)) {
            throw ParseError("syntax error");
        }
#endif
    }});

    m_from_config.insert({"patch", [](const std::string &val) {
        try {
            apply_user_patch(val);
        } catch (const PatchStatus::SyntaxError &) {
            throw ParseError("syntax error");
        } catch (const PatchStatus::NotFound &) {
            throw ParseError("does not apply to this story");
        }

        return false;
    }});

#ifdef ZTERP_GLK_UNIX
    int i = 0;

    // This constructor is only ever called once, at startup, but to be
    // proper, delete any allocated strings. If, at some point, this
    // class winds up being constructed multiple times, this will avoid
    // memory leaks.
    for (i = 0; glkunix_arguments[i].argtype != glkunix_arg_End; i++) {
        delete [] glkunix_arguments[i].name;
        delete [] glkunix_arguments[i].desc;
    }

    // glkunix_argumentlist_t is “meant” to be filled with string
    // literals, whose lifetime is that of the program. But here the
    // descriptions are part of the Options object, and the options
    // themselves need to be generated here, so lifetime issues must be
    // taken into account. This is ugly, but just allocate space with
    // “new”.
    for (const auto &opt : sorted_opts()) {
        glkunix_argumentlist_t arg{new char[3], glkunix_arg_NoValue, new char[opt.second.desc.size() + 1]};

        arg.name[0] = '-';
        arg.name[1] = opt.first;
        arg.name[2] = 0;

        if (opt.second.type == OptValue::Type::Number) {
            arg.argtype = glkunix_arg_NumberValue;
        } else if (opt.second.type == OptValue::Type::Value) {
            arg.argtype = glkunix_arg_ValueFollows;
        }

        std::memcpy(arg.desc, opt.second.desc.data(), opt.second.desc.size() + 1);

        glkunix_arguments[i++] = arg;
    }

    glkunix_arguments[i++] = glkunix_argumentlist_t{const_cast<char *>(""), glkunix_arg_ValueFollows, const_cast<char *>("file to load")};
    glkunix_arguments[i++] = glkunix_argumentlist_t{nullptr, glkunix_arg_End, nullptr};
#endif
}

#undef NUMBER
#undef OPTNUM
#undef BOOL
#undef STRING
#undef CHAR
#undef COLOR

void Options::add_parser(char opt, std::string desc, bool use_config, std::string name, const Parser &parser, OptValue::Type type) {
    if (opt != 0) {
        m_opts.insert({opt, {type, std::move(desc), parser}});
    }

    if (use_config) {
        m_from_config.insert({std::move(name), parser});
    }
}

void Options::read_config()
{
    auto file = zterp_os_rcfile(false);
    if (file == nullptr) {
        return;
    }

    std::ifstream f(*file);
    if (!f.is_open()) {
        return;
    }

    parse_grouped_file(f, [&file, this](const std::string &line, int lineno) {
        auto errmsg = [&file, lineno](const std::string &msg) {
            std::cerr << *file << ":" << lineno << ": " << msg << std::endl;
        };

        auto equal = line.find('=');
        if (equal == std::string::npos) {
            errmsg("expected '='");
            return;
        }

        auto key = rtrim(line.substr(0, equal));
        if (key.empty()) {
            errmsg("no key");
            return;
        }

        auto val = ltrim(line.substr(equal + 1));
        if (val.empty()) {
            errmsg("no value");
            return;
        }

        try {
            static const std::unordered_map<std::string, std::string> legacy_names = {
                {"notes_editor", "editor"},
                {"max_saves", "undo_slots"},
            };

            const auto &newkey = legacy_names.at(key);
            if (newkey.empty()) {
                errmsg("warning: configuration option " + key + " no longer exists");
                return;
            } else {
                errmsg("warning: configuration option " + key + " is deprecated; it has been renamed " + newkey);
                key = newkey;
            }
        } catch (const std::out_of_range &) {
        }

        try {
            auto parser = m_from_config.at(key);
            parser(val);
        } catch (const std::out_of_range &) {
            errmsg("unknown configuration option: " + key);
        } catch (const ParseError &e) {
            errmsg("invalid value for " + key + ": " + e.what());
        }
    });
}

int Options::getopt(int argc, char *const argv[])
{
    int optind = 0;
    const char *p = "";

    while (true) {
        if (*p == 0) {
            // No more arguments.
            if (++optind >= argc) {
                return optind;
            }

            p = argv[optind];

            // No more options.
            if (p[0] != '-' || p[1] == 0) {
                return optind;
            }

            // Handle “--”
            if (*++p == '-' && p[1] == 0) {
                return optind + 1;
            }
        }

        char c = *p++;
        try {
            const auto &optvalue = m_opts.at(c);
            const char *optarg = nullptr;

            if (optvalue.type == OptValue::Type::Flag) {
                optvalue.parser("1");
            } else {
                if (*p != 0) {
                    optarg = p;
                    p = "";
                } else {
                    optarg = argv[++optind];
                    if (optarg == nullptr) {
                        arg_status.set(ArgStatus::BadOption);
                        std::string error = "missing argument for -"s + c;
                        m_errors.push_back(error);
                        return optind;
                    }
                }

                optvalue.parser(optarg);
            }
        } catch (const std::out_of_range &) {
            arg_status.set(ArgStatus::BadOption);
        } catch (const ParseError &e) {
            arg_status.set(ArgStatus::BadValue);
            std::string error = "invalid argument for -"s + c + ": " + e.what();
            m_errors.push_back(error);
        }
    }
}

void Options::process_arguments(int argc, char **argv)
{
    int optind = getopt(argc, argv);

    // Just ignore excess stories for now.
    if (optind < argc) {
        game_file = argv[optind];
    }
}

void Options::help()
{
#ifdef ZTERP_GLK
    glk_set_style(style_Preformatted);
#endif

    screen_puts("Usage: bocfel [args] filename");
    for (const auto &opt : sorted_opts()) {
        std::string typestr;

        if (opt.second.type == OptValue::Type::Number) {
            typestr = "number";
        } else if (opt.second.type == OptValue::Type::Value) {
            typestr = "string";
        }

        std::ostringstream ss;
        ss << "-" << opt.first << " " << std::setw(12) << std::left << typestr << opt.second.desc;
        screen_puts(ss.str());
    }
}

std::vector<std::pair<char, OptValue>> Options::sorted_opts()
{
    std::vector<std::pair<char, OptValue>> sorted;

    for (const auto &pair : m_opts) {
        sorted.emplace_back(pair);
    }

    std::sort(sorted.begin(), sorted.end(), [](const std::pair<char, OptValue> &a_, const std::pair<char, OptValue> &b_) {
        unsigned char a = a_.first, b = b_.first;

        if (std::tolower(a) == std::tolower(b)) {
            return std::islower(a) && std::isupper(b);
        }

        return std::tolower(a) < std::tolower(b);
    });

    return sorted;
}
