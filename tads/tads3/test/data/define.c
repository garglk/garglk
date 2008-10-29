#define recurs(x) (recurs(x) + 1)
recurs(recurs(5));

#define foo1 a,b
#define bar1(x) lose1(x)
#define lose1(x) (1 + (x))
bar1(foo1);

#define foo2 (a,b)
#define bar2(x) lose((x))
bar2(foo2);

#define xstr3(s) str3(s)
#define str3(s) #s
#define foo3  987
xstr3(foo3)

#define str(s) #s
#define foo 4
str (foo)

#define x (4 + y)
#define y (2 * x)

x;
y;

#define PASTE(a, b) a ## b

#if PASTE(1, 2) > 10
#pragma message("yes")
#else
#pragma message("no")
#endif


#if PASTE(1,
2) > 10
#pragma message("yes")
#else
#pragma message("no")
#endif

This is a test.  /* This is a comment. */  End of the test.
This       is         another        test.

   PASTE(({a, b, c}), ([d, e, f]));


Another multiline macro test: PASTE(100
                                     ,
                                    200
                                      +
                                    300
                                       ); that is all!!!;

Test of a multiline macro: PASTE(100,
                                 200); end of multi-line macro!


PASTE(aaa xxx,
#ifdef FOO
      bbb
#else
      ccc
#endif

     );

100 ## 200


#define FOOBAR 100
   PASTE(FOO, BAR)


#define FOO 1
#define BAR 2
   PASTE(FOO, BAR);


#define A(x) x*2
#define B(y, z) y(z)

B(A, 5)

PASTE(FOO, 1);
PASTE
  (X,
   Y);
PASTE


  X, y;

#define RECURSIVE(x) RECURSIVE(x)
    RECURSIVE(100);
        
"Try a macro that ends without closing its argument list...":
  PASTE(100,
        