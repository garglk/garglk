// vim: set ft=cpp:

#ifndef ZTERP_UTIL_H
#define ZTERP_UTIL_H

#include <cstdarg>
#include <functional>
#include <string>
#include <type_traits>

#include "types.h"

#if defined(__GNUC__) && (__GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 7))
#if defined(__MINGW32__) && defined(__GNUC__) && !defined(__clang__)
// Gcc appears to default to ms_printf on MinGW, even when it is
// providing standards-conforming printf() functionality (i.e. if
// __USE_MINGW_ANSI_STDIO is defined), so force gnu_printf there.
#define zprintflike(f, a)	__attribute__((__format__(__gnu_printf__, f, a)))
#else
#define zprintflike(f, a)	__attribute__((__format__(__printf__, f, a)))
#endif
#else
#define zprintflike(f, a)
#endif

// Allow scoped enums to be used as hash keys.
// NOTE: Remove when switching to C++17, as C++17 provides this.
struct EnumClassHash {
    template <typename T>
    std::enable_if_t<std::is_enum<T>::value, std::size_t>
    operator()(T enumValue) const {
        using UnderlyingType = std::underlying_type_t<T>;
        return std::hash<UnderlyingType>{}(static_cast<UnderlyingType>(enumValue));
    }
};

int16_t as_signed(uint16_t n);

#ifndef ZTERP_NO_SAFETY_CHECKS
[[noreturn]]
zprintflike(1, 2)
void assert_fail(const char *fmt, ...);
#define ZASSERT(expr, ...)	do { if (!(expr)) assert_fail(__VA_ARGS__); } while (false)
#else
#define ZASSERT(expr, ...)	((void)0)
#endif

zprintflike(1, 2)
void warning(const char *fmt, ...);

[[noreturn]]
zprintflike(1, 2)
void die(const char *fmt, ...);
void help();

long parseint(const std::string &s, int base, bool &valid);
std::string vstring(const char *fmt, std::va_list ap);
zprintflike(1, 2)
std::string fstring(const char *fmt, ...);
std::string ltrim(const std::string &s);
std::string rtrim(const std::string &s);
void parse_grouped_file(std::ifstream &f, const std::function<void(const std::string &line, int lineno)> &callback);

#endif
