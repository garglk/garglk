// vim: set ft=cpp:

#ifndef ZTERP_UTIL_H
#define ZTERP_UTIL_H

#include <cstdarg>
#include <string>

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

enum class ArgStatus {
    Ok,
    Help,
    Fail,
};

extern ArgStatus arg_status;
void process_arguments(int argc, char **argv);
long parseint(const std::string &s, int base, bool &valid);
std::string vstring(const char *fmt, std::va_list ap);
zprintflike(1, 2)
std::string fstring(const char *fmt, ...);
std::string ltrim(const std::string &s);
std::string rtrim(const std::string &s);

#endif
