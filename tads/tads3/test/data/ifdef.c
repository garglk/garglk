/* Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved. */
/*
Name
  main.t - test source file for test_tok.exe.
Function
  Tests the tokenizer and preprocessor functions
Notes
  
Modified
  04/17/99 MJRoberts  - Creation
*/

#pragma all_once-

#define AAA  1
#define YES  1
#define NO   0

#if AAA ? YES : NO
"AAA -> yes - ok";
#else
"AAA -> no???";
#endif

#undef AAA
#define AAA 0
#if AAA ? YES : NO
"AAA -> yes???";
#else
"AAA -> no - ok";
#endif


#if 1 + 2 > 8 - 5 ? 1 : 0
"bad ?:";
#else
"good ?:";
#endif

#if 1+2*3 > 8 ? 0 : 1
"good ?:";
#else
"bad ?:";
#endif

#define PRINTVAL(val) printf(#@val + '= %d\n', val)
PRINTVAL(abc);
PRINTVAL(def_);

#if defined(PRINTVAL)
"printval is defined!";
#endif

#if defined(UNDEFINED_SYMBOL) && defined(PRINTVAL)
"something's wrong!!!";
#endif

#if defined(DEFINITELY_NOT_DEFINED) || defined(PRINTVAL)
"okay!!!";
#endif


#define LINENUM  100
#define FILENAME "test"

"try string translations";
#if 'u' > '7'
#pragma message("u > 7 - ok")
#else
#pragma message("u not > 7 - WRONG")
#endif

#if '\u00C7' > 'aaaa'
#pragma message("\\u00C7 > aaaa - ok")
#else
#pragma message("\\u00c7 not > aaaa - WRONG")
#endif

#if '\uC7' > '\xC6'
#pragma message("\\uC7 > \\xC6 - ok")
#else
#pragma message("\\C7 not > \\xC6 - WRONG")
#endif

"pragma message with a translation";
#define TEST "this is a test message"
#pragma message(TEST)
#error "macro: " TEST "!!!"


"try some macros with arguments";
#define PRODUCT(a, b) ((a) * (b))
#define SUM(a, b) ((a) + (b))
#define SUM3(a, b, c) ((a) + (b) + (c))

"testing product(10, 20): "; PRODUCT(10, 20);
"testing product(sum(10, sum3(1, 2, 3)), product(5, 10)): ";
   PRODUCT(SUM(10, SUM3(1, 2, 3)), PRODUCT(5, 10));

"testing sum(sum(sum(1, 2), sum(3, 4)), sum(sum(5, 6), sum(7, 8))): ";
   SUM(SUM(SUM(1, 2), SUM(3, 4)), SUM(SUM(5, 6), SUM(7, 8)));

#define CIRCULAR_A(foo) CIRCULAR_B(foo*A)
#define CIRCULAR_B(foo) CIRCULAR_A(foo*B)
"circular_a(100): "; CIRCULAR_A(100);
"circular_a(circular_b(50)): "; CIRCULAR_A(CIRCULAR_B(50));

#if SUM(1+2, 3+4) == PRODUCT(2, 5)
   "good argument expansion";
#else
   "bad argument expansion";
#endif

#if PRODUCT(10,11) != PRODUCT(11,10)
    "bad expansion";
#elif SUM(5 + 6, 7) == SUM3(5, 6, 7)
    "good expansion";
#else
    "bad else expansion";
#endif
    

"try some #if's with macro expansions";

#define TEST_ONE  1
#define TEST_NINE 9
#define TEST_TEN  10

#if TEST_ONE == TEST_TEN
   "bad if";
#elif TEST_ONE + TEST_NINE == TEST_TEN
   "good elif with expansion!!!";
#else
   "bad else";
#endif

#define TEST_STR_1  'abc'
#define TEST_STR_2  'def'
#define TEST_STR_3  'abc'

#if TEST_STR_1 < TEST_STR_2
    "good string #if";
#endif

#if TEST_STR_1 > TEST_STR_2
    "bad string #if";
#else
    "good string #else";
#endif

#if TEST_STR_1 == TEST_STR_2
    "bad string #if";
#elif TEST_STR_1 == TEST_STR_3
    "good string #elif";
#endif

"try some #if's with constant expressions"
#if 0
    "bad if"
#elif 1
   "good if!!!"
#else
   "bad else"
#endif

#if 1+2*3 == (4+11)*2 - ((6+7) << 1) + (0xF03 & 03)
   "good if!!! (and a tough one, too)";
#elif 1 == 1
   "bad if"
#elif 1 == 2
   "another bad if"
#else
   "bad else"
#endif

#if 1 == 2
   "bad if"
#elif 2 == 3
   "another bad if"
#elif 3+1 == 6-4/2
   "good elif!!!!!!"
#elif 3 == 3
   "bad elif"
#elif 4 == 0
   "bad elif"
#elif 4 == 4
   "bad elif"
#else
   "bad else"
#endif
"done with if/elif";

"define some macros";
#define FOO
#define BAR

"try out a positive #if";
#ifdef FOO
   "this is a good #if";
#else /* FOO */
   "this is a bad #else";
#endif /* FOO */
"end of positive #if";

"try a negative #if";
#ifdef BLECH
   "this is a bad #if";
#else /* FOO */
   "this is a good #else";
#endif /* FOO */
"end of negative #if";

"try some nested #if's";
#ifdef FOO
      "good #if - level 1";   A
# ifdef BAR
      "good #if - level 2";   B
#  ifdef BLECH
      "bad #if - level 3";    C
#  else
      "good #else - level 3";  D
#  endif /* BLECH */
      "good #if - level 2";    E
# else /* BAR */
      "bad #else - level 2";   F
#  ifdef FOO
      "good #if within a bad #if -> bad - level 2";   G
#  else
      "bad #else within bad #if -> bad - level 2";  H
#  endif /* nested FOO */
# endif /* BAR */
      "good #if - level 1";    I
#else /* FOO */
      "bad #else - level 1";   J
# ifdef BAR
      "good #if in bad else -> bad - level 2";   B
#  ifdef BLECH
      "bad #if in bad else -> bad - level 3";    C
#  else
      "good #else in bad else -> bad - level 3";  D
#  endif /* BLECH */
      "good #if in bad else -> bad - level 2";    E
# else /* BAR */
      "bad #else in bad else -> bad - level 2";   F
#  ifdef FOO
      "good #if within a bad #if within bad #else -> bad - level 2";   G
#  else
      "bad #else within bad #if within bad #else -> bad - level 2";  H
#  endif /* nested FOO */
# endif /* BAR */
#endif /* FOO */
#ifdef FOO
    "good again";
#else
    "bad again";
#endif
"done with nested #if's";

"ifndef test";
#ifndef FOO
   "bad ifndef";
#else
   "good ifndef else";
#endif
#ifndef BLECH
   "good ifndef";
#else
   "bad ifndef else";
#endif



// this is a single-line comment, which should be eliminated

/* this is a multi-line comment on just one line */

/*
 *   this is a full multi-line comment 
 */

/*
 *   This is another multi-line comment.  The #include that follows should
 *   be ignored, since it's part of this comment.

#include "badfile.t"

 */

// the following #include should be ignored
// #include "badfile.t"

/*
 *   the following #include should be obeyed 
 */
#include "header.t"

"This is a string.  The #include below should be ignored,
because it's part of the string.

#include "badfile.t"

That's that!";

"This is a string with escaped \"quote marks\".  Those quotes
shouldn't end the string.
\"the next #include should be ignored, since it's still part of the string:

#include <badfile.t>

That's all for the string!";

/*
 *   here's a comment with a "string" embedded 
 */

"Here's a string with a /*comment*/ embedded.";

"Here's some mixing:" /* comment */  "String" /* comment */ "Done!!!" // EOL


/*
 *   Include the header with a comment dangling after the end of the
 *   include line. 
 */
#include "header.t"    /* Start a comment here,
                          but don't finish it
                          for a few lines, just to
                          make it harder. */  "<--- after the comment";

"That's it!";

#include "header.t"
Line directly after the header!


"Try some random #line directives!";

#line 1234 "foo_test.c"

hello!

#line 6789 "test2.c"

goodbye!

#line 1+2+3 "foo_test.c"

"__LINE__ and __FILE__ near the end of file: "; __LINE__, __FILE__;

"Make sure we come back here after we're done with an include file.";
#include "header.t"
>>> back from include!!! <<<


"Try expanding macros and calculating values in #line";
#line 5 + 2*LINENUM FILENAME + ".h"

"try __FILE__ and __LINE__";
"__FILE__: <<__FILE__>>, __LINE__: << __LINE__ >>\n";

"__FILE__: <<
  __FILE__
  >>, __LINE__:
  <<
  __LINE__ >> -- that was __FILE and __LINE!\n";

