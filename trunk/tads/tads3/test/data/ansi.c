/* 
 *   ansi C++ 16.3.4(5) - tests of macro substitution rules (including
 *   recursive substitutions), stringizing, and token pasting 
 */

/* make sure __FILE__ and __LINE__ are provided */
file = __FILE__;
line = __LINE__;

/* check __DATE__ and __TIME__ */
//date = __DATE__;  // these always change, so omit them 
//time = __TIME__;  // for automated testing

#define x    3
#define f(a) f(x * (a))
#undef  x
#define x    2
#define g    f
#define z    z[0]
#define h    g(~
#define m(a) a(w)
#define w    0,1
#define t(a) a

t(t(g)(0) + t)(1);
f(f(z));
f(y+1) + f(f(z)) % t(t(g)(0) + t)(1);
g(x+(3,4)-w) | h 5) & m
   (f)^m(m);

"Result should be:";
"f(2 * (0)) + t(1);";
"f(2 * (f(2 * (z[0]))));";
"f(2 * (y+1)) + f(2 * (f(2 * (z[0])))) % f(2 * (0)) + t(1);";
"f(2 * (2+(3,4)-0,1)) | f(2 * (~5)) & f(2 * (0,1))^m(0,1);";

/* clean up */
#undef t
#undef w
#undef m
#undef h
#undef z
#undef g
#undef f
#undef x


/*
 *   ansi C++ 16.3.4(7) 
 */

/* VALID */
#define OBJ_LIKE      (1-1)
#define OBJ_LIKE1      /* white space */ (1-1) /* other */
#define FTN_LIKE(a)   ( a )
#define FTN_LIKE( a )(          /* note the white space */ \
                              a /* other stuff on this line
                                 */ )

/* INVALID */
#define OBJ_LIKE    (0)           /* different token sequence */
#define OBJ_LIKE    (1 - 1)       /* different white space */
#define FTN_LIKE(b) ( a )         /* different parameter usage */
#define FTN_LIKE(b) ( b )         /* different parameter spelling */

/* clean up */
#undef OBJ_LIKE
#undef FTN_LIKE
#undef OBJ_LIKE1

/*
 *   stringizing 
 */

#define str(s)      # s
#define xstr(s)     str(s)
#define debug(s, t) printf("x" # s "= %d, x" # t "= %s", \
                                x ## s, x ## t)
#define INCFILE(n)  vers ## n             /* from previous #include example */
#define glue(a, b)  a ## b
#define xglue(a, b) glue(a, b)
#define HIGHLOW     "hello"
#define LOW         LOW ", world"

debug(1, 2);
fputs(str(strncmp("abc\0d", "abc", '\4')/* this goes away */
        == 0) str(: @\n), s);
#include xstr(INCFILE(2).h)
glue(HIGH, LOW);        >>> should be: "hello"
xglue(HIGH, LOW);       >>> should be: "hello" ", world"


"should be:";
'printf("x" "1" "= %d, x" "2" "= %s", x1, x2);'
'fputs("strncmp(\"abc\\0d\", \"abc\", '\\4') == 0" ": @\n", s);'
'#include "vers2.h"    (after macro replacement, before file access)'
'"hello";'
'"hello" ", world";'

/*
 *   include file syntax 
 */

#undef INCFILE
#define INCFILE vers2.h

"should not expand macros on the following two lines";
#include "INCFILE"
#include <INCFILE>

#undef INCFILE
#define INCFILE "vers2.h"
"should expand macros on the following line";
#include INCFILE


