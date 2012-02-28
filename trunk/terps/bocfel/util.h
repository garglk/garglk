#ifndef ZTERP_UTIL_H
#define ZTERP_UTIL_H

#ifdef ZTERP_GLK
#include <glk.h>
#endif

#if defined(__GNUC__) && (__GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 7))
#define znoreturn		__attribute__((__noreturn__))
#define zprintflike(f, a)	__attribute__((__format__(__printf__, f, a)))
#else
#define znoreturn
#define zprintflike(f, a)
#endif

#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 1))
#define zexternally_visible	__attribute__((__externally_visible__))
#else
#define zexternally_visible
#endif

#ifndef ZTERP_NO_SAFETY_CHECKS
extern unsigned long zassert_pc;
#define ZPC(pc)		do { zassert_pc = pc; } while(0)

zprintflike(1, 2)
znoreturn
void assert_fail(const char *, ...);
#define ZASSERT(expr, ...) do { if(!(expr)) assert_fail(__VA_ARGS__); } while(0)
#else
#define ZPC(pc)			((void)0)
#define ZASSERT(expr, ...)	((void)0)
#endif

zprintflike(1, 2)
void warning(const char *, ...);

zprintflike(1, 2)
znoreturn
void die(const char *, ...);

char *xstrdup(const char *);
int process_arguments(int, char **);

/* Somewhat ugly hack to get around the fact that some Glk functions may
 * not exist.  These function calls should all be guarded (e.g.
 * if(have_unicode), with have_unicode being set iff GLK_MODULE_UNICODE
 * is defined) so they will never be called if the Glk implementation
 * being used does not support them, but they must at least exist to
 * prevent link errors.
 */
#ifdef ZTERP_GLK
#ifndef GLK_MODULE_UNICODE
#define glk_put_char_uni(...)		die("bug %s:%d: glk_put_char_uni() called with no unicode", __FILE__, __LINE__)
#define glk_put_string_uni(...)		die("bug %s:%d: glk_put_string_uni() called with no unicode", __FILE__, __LINE__)
#define glk_request_char_event_uni(...)	die("bug %s:%d: glk_request_char_event_uni() called with no unicode", __FILE__, __LINE__)
#define glk_request_line_event_uni(...)	die("bug %s:%d: glk_request_line_event_uni() called with no unicode", __FILE__, __LINE__)
#endif
#ifndef GLK_MODULE_LINE_ECHO
#define glk_set_echo_line_event(...)	die("bug: %s %d: glk_set_echo_line_event() called with no line echo", __FILE__, __LINE__)
#endif
#endif

#endif
