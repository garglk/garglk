// Copyright 2010-2021 Chris Spiegel.
//
// SPDX-License-Identifier: MIT

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>

#include "util.h"
#include "process.h"
#include "screen.h"
#include "types.h"
#include "zterp.h"

#ifdef ZTERP_GLK
#include <glk.h>
#endif

// Values are usually stored in a uint16_t because most parts of the
// Z-machine make use of 16-bit unsigned integers. However, in a few
// places the unsigned value must be treated as signed. The “obvious”
// solution is to simply convert to an int16_t, but this is technically
// implementation-defined behavior in C++ and not guaranteed to provide
// the expected result. In order to be maximally portable, this
// function converts a uint16_t to its int16_t counterpart manually.
// Although it ought to be slower, both Gcc and Clang recognize this
// construct and generate the same code as a direct conversion. An
// alternative direct conversion method is included here for reference.
#if 1
int16_t as_signed(uint16_t n)
{
    return ((n & 0x8000) == 0x8000) ? static_cast<long>(n) - 0x10000L : n;
}
#else
int16_t as_signed(uint16_t n)
{
    return n;
}
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
    int n;

    va_copy(ap_copy, ap);

    n = std::vsnprintf(nullptr, 0, fmt, ap);
    if (n < 0) {
        die("error processing format string");
    }

    s.resize(n);
    std::vsnprintf(&s[0], n + 1, fmt, ap_copy);

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

void parse_grouped_file(std::ifstream &f, const std::function<void(const std::string &line, int lineno)> &callback)
{
    std::string line;
    bool story_matches = true;

    for (int lineno = 1; std::getline(f, line); lineno++) {
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
            story_matches = id == get_story_id();

            continue;
        }

        if (!story_matches) {
            continue;
        }

        callback(line, lineno);
    }
}
