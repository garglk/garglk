#define A(x) #x ## "hello"
#define B(x) "hello" ## #x
#define C(x,y) #x ## #y
#define STR "hello"
#define D(x) #x ## STR
#define E(x) STR ## #x

#define G(x) x ## STR
#define H(x) STR ## x

#define I(x) "hello" ## #x ## "goodbye"
#define J(x, y) "Hello" ## #x ## #y ## "Goodbye"

#define K(x) "Hello ## #x"

#define L(x, y) x ## y

#define PAREN_STR(a) "(" ## a ## ")"
#define CONCAT(a, b) a ## b
#define CONCAT_STR(a, b) #a ## #b
#define DEBUG_PRINT(a) "value of " ## #a ## " = <<a>>"

A: A(asdf);
B: B(jklm);
C: C(abc, def);
D: D(xyz);
E: E(ghi);

G: G(abc);
H: H(def);

I: I(mnop);

J: J(Tuv, Wxy);
K: K(asfd);
L: L("hello", "goodbye");

PAREN_STR("parens");
CONCAT("abc", "def");
CONCAT_STR(uvw, xyz);
DEBUG_PRINT(obj.prop[3]);
