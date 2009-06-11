/*
 *   preprocessor token pasting - examples from ansi C++ 16.3.4(7)
 */

#define glue(a, b)  a ## b
#define xglue(a, b) glue(a, b)
#define HIGHLOW     "hello"
#define LOW         LOW ", world"

xglue(HIGH, LOW)
glue(HIGH, LOW);

