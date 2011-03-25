#ifndef ZTERP_UTIL_H
#define ZTERP_UTIL_H

#if defined(__GNUC__) && (__GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR >= 7))
#define znoreturn		__attribute__((__noreturn__))
#define zprintflike(f, a)	__attribute__((__format__(__printf__, f, a)))
#else
#define znoreturn
#define zprintflike(f, a)
#endif

#ifndef ZTERP_NO_SAFETY_CHECKS
extern unsigned long zassert_pc;
#define ZPC(pc)		do { zassert_pc = pc; } while(0)

zprintflike(4, 5)
znoreturn
void assert_fail(const char *, const char *, int, const char *, ...);
#define ZASSERT(expr, ...) do { if(!(expr)) assert_fail(__func__, __FILE__, __LINE__, __VA_ARGS__); } while(0)
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

#endif
