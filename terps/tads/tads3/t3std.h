/* 
 *   Copyright (c) 1991, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  t3std.h - standard definitions
Function
  Various standard definitions
Notes
  None
Modified
  10/17/98 MJRoberts  - creation (from TADS 2 lib.h)
*/

#ifndef T3_STD_INCLUDED
#define T3_STD_INCLUDED

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <limits.h>

#include "os.h"


/* ------------------------------------------------------------------------ */
/*
 *   err_throw() Return Handling
 *   
 *   Some compilers (such as MSVC 2007) are capable of doing global
 *   optimizations that can detect functions that never return.  err_throw()
 *   is one such function: it uses longjmp() to jump out, so it never returns
 *   to its caller.
 *   
 *   Most compilers can't detect this automatically, and C++ doesn't have a
 *   standard way to declare a function that never returns.  So on most
 *   compilers, the compiler will assume that err_throw() returns, and thus
 *   will generate a warning if err_throw() isn't followed by some proper
 *   control flow statement.  For example, in a function with a return value,
 *   a code branch containing an err_throw() would still need a 'return
 *   <val>' statement - without such a statement, the compiler would generate
 *   an error about a branch without a value return.
 *   
 *   This creates a porting dilemma.  On compilers that can detect that
 *   err_throw() never returns, the presence of any statement in a code
 *   branch after an err_throw() will cause an "unreachable code" error.  For
 *   all other compilers, the *absence* of such code will often cause a
 *   different error ("missing return", etc).
 *   
 *   The only way I can see to deal with this is to use a compile-time
 *   #define to select which type of compiler we're using, and use this to
 *   insert or delete the proper dummy control flow statement after an
 *   err_throw().  So:
 *   
 *   --- INSTRUCTIONS TO BASE CODE DEVELOPERS ---
 *   
 *   - after each err_throw() call, if the code branch needs some kind of
 *   explicit termination (such as a "return val;" statement), code it with
 *   the AFTER_ERR_THROW() macro.  Since err_throw() never *actually*
 *   returns, these will be dummy statements that will never be reached, but
 *   the compiler might require their presence anyway because it doesn't know
 *   better.
 *   
 *   --- INSTRUCTIONS TO PORTERS ---
 *   
 *   - if your compiler CAN detect that err_throw() never returns, define
 *   COMPILER_DETECTS_THROW_NORETURN in your compiler command-line options;
 *   
 *   - otherwise, leave the symbol undefined.  
 */
#ifdef COMPILER_DETECTS_THROW_NORETURN
#define COMPILER_DETECTS_THROW_NORETURN_IN_VMERR
#define AFTER_ERR_THROW(code)
#else
#define AFTER_ERR_THROW(code)   code
#endif

/* 
 *   And the same idea as above, but for the special case where the compiler
 *   detects this within the vmerr.cpp module (for static functions in the
 *   same module), but not across modules.
 */
#ifdef COMPILER_DETECTS_THROW_NORETURN_IN_VMERR
#define VMERR_AFTER_ERR_THROW(code)
#else
#define VMERR_AFTER_ERR_THROW(code)  code
#endif

/*
 *   os_term() return handling.  This is similar to the longjmp() issue
 *   above.  Some compilers perform global optimizations that detect that
 *   exit(), and functions that unconditionally call exit(), such as
 *   os_term(), do not return.  Since these compilers know that anything
 *   following a call to exit() is unreachable, some will complain about if
 *   anything follows an exit().  Compilers that *don't* do such
 *   optimizations will complain if there *isn't* a 'return' after an exit()
 *   if the containing function requires a return value, since as far as
 *   they're concerned the function is falling off the end without returning
 *   a value.  So there's no solution at the C++ level; it's a compiler
 *   variation that we have to address per compiler.  If you get 'unreachable
 *   code' errors in the generic code after calls to os_term(), define this
 *   macro.  Most platforms can leave it undefined.  
 */
#ifdef COMPILER_DETECTS_OS_TERM_NORETURN
#define AFTER_OS_TERM(code)
#else
#define AFTER_OS_TERM(code)   code
#endif

/*
 *   Some compilers (notably gcc >= 4.7.1) require that overloads for the
 *   basic 'new' and 'delete' operators be declared with 'throw' clauses to
 *   match the standard C++ library (that is, 'new' must be declared to throw
 *   std::bad_alloc, and 'delete' must be declared to throw nothing).
 *   Naturally, some other compilers (notably MSVC 2003) don't want the
 *   'throw' clauses.  If your compiler wants the 'throw' declarations,
 *   define NEW_DELETE_NEED_THROW in your makefile, otherwise omit it.  Note
 *   that some compilers (notably gcc < 4.7.1) don't care one way or the
 *   other, so if your compiler doesn't complain, you probably don't need to
 *   worry about this setting.
 */
#ifndef SYSTHROW
#ifdef NEW_DELETE_NEED_THROW
#define SYSTHROW(exc) exc
#else
#define SYSTHROW(exc)
#endif
#endif


/* ------------------------------------------------------------------------ */
/*
 *   T3 OS interface extensions.  These are portable interfaces to
 *   OS-dependent functionality, along the same lines as the functions
 *   defined in tads2/osifc.h but specific to TADS 3.  
 */

/*
 *   Initialize the UI after loading the image file.  The T3 image loader
 *   calls this after it's finished loading a .t3 image file, but before
 *   executing any code in the new game.  This lets the local platform UI
 *   perform any extra initialization that depends on inspecting the contents
 *   of the loaded game.
 *   
 *   This isn't required to do anything; a valid implementation is an empty
 *   stub routine.  The purpose of this routine is to give the UI a chance to
 *   customize the UI according to the details of the loaded game, before the
 *   game starts running.  In particular, the UI configuration might vary
 *   according to which intrinsic classes or function sets are linked by the
 *   game.  For example, one set of windows might be used for a traditional
 *   console game, while another is used for a Web UI game, the latter being
 *   recognizable by linking the tads-net function set.  
 */
void os_init_ui_after_load(class CVmBifTable *bif_table,
                           class CVmMetaTable *meta_table);

/* ------------------------------------------------------------------------ */
/*
 *   Memory debugging 
 */

/* in debug builds, we override operator new, which requires including <new> */
#ifdef T3_DEBUG
#include <new>
#endif

/* for Windows debug builds, add stack trace info to allocation blocks */
#if defined(T3_DEBUG) && defined(T_WIN32)
# define OS_MEM_PREFIX \
    struct { \
        DWORD return_addr; \
    } stk[6];
void os_mem_prefix_set(struct mem_prefix_t *mem);

# define OS_MEM_PREFIX_FMT ", return=(%lx, %lx, %lx, %lx, %lx, %lx)"
# define OS_MEM_PREFIX_FMT_VARS(mem) \
    , (mem)->stk[0].return_addr, \
    (mem)->stk[1].return_addr, \
    (mem)->stk[2].return_addr, \
    (mem)->stk[3].return_addr, \
    (mem)->stk[4].return_addr, \
    (mem)->stk[5].return_addr
#endif

/* provide empty default definitions for system memory header add-ons */
#ifndef OS_MEM_PREFIX
# define OS_MEM_PREFIX
# define os_mem_prefix_set(mem)
# define OS_MEM_PREFIX_FMT
# define OS_MEM_PREFIX_FMT_VARS(mem)
#endif

/* ------------------------------------------------------------------------ */
/*
 *   Types 
 */

/* short-hand for various types */
#ifndef OS_UCHAR_DEFINED
typedef unsigned char  uchar;
#endif

#ifndef OS_USHORT_DEFINED
typedef unsigned short ushort;
#endif

#ifndef OS_UINT_DEFINED
typedef unsigned int   uint;
#endif

#ifndef OS_ULONG_DEFINED
typedef unsigned long  ulong;
#endif

/* sizeof() extension macros */
#ifndef countof
#define countof(array) (sizeof(array)/sizeof((array)[0]))
#endif
#define sizeof_field(struct_name, field) sizeof(((struct_name *)0)->field)

/*
 *   The types int16_t, uint16_t, int32_t, and uint32_t are defined by ANSI
 *   C99 as EXACTLY the specified number of bits (e.g., int16 is a signed
 *   16-bit integer; uint32 is an unsigned 32-bit integer).
 *   
 *   Many modern compilers provide definitions for these types in <stdint.h>.
 *   When <stdint.h> isn't available, ports must provide suitable typedefs in
 *   the osxxx.h header - see tads2/osifc.h.
 *   
 *   Because these types have exact sizes, their limits are fixed, so we can
 *   provide portable definitions here.
 */
#define INT16MAXVAL    32767
#define INT16MINVAL    (-32768)
#define UINT16MAXVAL   65535
#define INT32MAXVAL    2147483647L
#define INT32MINVAL    (-2147483647L-1)
#define UINT32MAXVAL   4294967295U


/*
 *   Text character.  We use ASCII and UTF-8 for most character string
 *   representations, so our basic character type is 'char', which is defined
 *   univerally as one byte.  (UTF-8 uses varying numbers of bytes per
 *   character, but its basic storage unit is bytes.)
 */
typedef char textchar_t;


/* ------------------------------------------------------------------------ */
/*
 *   Logical and Arithmetic right shifts.  C99 deliberately leaves it up to
 *   the implementation to define what happens to the high bits for a right
 *   shift of a negative signed integer: they could be filled with 0s or 1s,
 *   depending on the implementation.  C99 is clear on the result for
 *   unsigned values (the high bits are filled with 0s), however it's still
 *   possible that we'll encounter an older implementation that doesn't
 *   conform.
 *   
 *   There's one case where we care about ASHR vs RSHR being well defined,
 *   and that's the VM's implementation of the OPC_LSHR and OPC_ASHR
 *   instructions.  We want these to be predictable on all platforms - we
 *   insist on breaking the cycle of ambiguity; we don't want to pass along
 *   this headache to our own users writing TADS programs.
 *   
 *   Nearly all modern compilers treat right shifts of signed operands as
 *   arithmetic, and right shifts of unsigned operands as logical.  So our
 *   default implementation will take advantage of this to produce highly
 *   efficient code.  For platforms where the signed/unsigned behavior
 *   doesn't work this way, we provide portable bit-twiddly versions that
 *   will work, but are somewhat more overhead to implement.
 *   
 *   If you're on a platform that DOES provide the "modern" signed==ASHR /
 *   unsigned==LSHR behavior, you don't have to do anything special - that's
 *   the default.  If your platform doesn't use the modern behavior, AND your
 *   compiler's preprocessor does >> calculations the same way as generated
 *   code, you also don't have to do anything, since our #if's below will
 *   detect the situation and generate our portable explicit ASHR/LSHR code.
 *   If your compiler doesn't use the "modern" behavior AND its preprocessor
 *   uses different rules from generated code for >>, then you'll have to
 *   define the preprocessor symbols OS_CUSTOM_ASHR and/or OS_CUSTOM_LSHR in
 *   your makefile.
 *   
 *   You can test that your configuration is correct by compiling and running
 *   the shr.t from the test suite (tads3/test/data).  To further test the
 *   detection conditions and custom macros, use test/test_shr.cpp.  
 */
#if defined(OS_CUSTOM_ASHR) || ((-1 >> 1) != -1)
  /* signed a >> signed b != a ASHR b, so implement with bit masking */
  inline int32_t t3_ashr(int32_t a, int32_t b) {
      int32_t mask = (~0 << (sizeof(int32_t)*CHAR_BIT - b));
      return ((a >> b) | ((a & mask) ? mask : 0));
  };
#else
  /* signed a >> signed b == a ASHR b, so we can use >> */
  inline int32_t t3_ashr(int32_t a, int32_t b) { return a >> b; }
#endif

#if defined(OS_CUSTOM_LSHR) || ((ULONG_MAX >> 1UL) != ULONG_MAX/2)
  /* unsigned a >> unsigned b != a LSHR b, so implement with bit masking */
  inline int32_t t3_lshr(int32_t a, int32_t b) {
      return ((a >> b) & ~(~0 << (sizeof(int32_t)*CHAR_BIT - b)));
  }
#else
  /* 
   *   unsigned a >> unsigned b == a LSHR b, so we can use >>; note that we
   *   explicit mask the left operand to 32 bits in case we're on a 64-bit or
   *   larger platform, as the T3 VM itself is defined as a 32-bit machine 
   */
  inline int32_t t3_lshr(int32_t a, int32_t b) {
      return (int32_t)((uint32_t)a >> (uint32_t)b);
  }
#endif


/* ------------------------------------------------------------------------ */
/*
 *   General portable utility macros
 */

/* clear a struture */
#define CLRSTRUCT(x) memset(&(x), 0, (size_t)sizeof(x))
#define CPSTRUCT(dst,src) memcpy(&(dst), &(src), (size_t)sizeof(dst))


/* TRUE and FALSE */
#ifndef TRUE
# define TRUE 1
#endif /* TRUE */
#ifndef FALSE
# define FALSE 0
#endif /* FALSE */


/* bitwise operations */
#define bit(va, bt) ((va) & (bt))
#define bis(va, bt) ((va) |= (bt))
#define bic(va, bt) ((va) &= ~(bt))


/* conditionally compile code if debugging is enabled */
#ifdef DEBUG
# define IF_DEBUG(x) x
#else /* DEBUG */
# define IF_DEBUG(x)
#endif /* DEBUG */

/* offset within a structure of a member of the structure */
#ifndef offsetof
# define offsetof(s_name, m_name) (size_t)&(((s_name *)0)->m_name)
#endif /* offsetof */


/*
 *   Read an unsigned 32-bit value from the portable external-file
 *   representation.  This is parallel to osrp4(), but explicitly reads an
 *   unsigned value.  The important thing is that we mask the result to 32
 *   bits, to prevent unwarranted sign extension on architectures with word
 *   sizes greater than 32 bits (at the moment, this basically means 64-bit
 *   machines, but it would apply to any >32-bit architecture).
 *   
 *   NB: as of TADS 3.1, osrp4() *should* be doing this automatically.  It
 *   should be reading a 32 bit value and interpreting it as unsigned, doing
 *   any necessary zero extension to larger 'int' sizes.  However, we're
 *   keeping this for now to be sure.  
 */
#define t3rp4u(p) ((ulong)(osrp4(p) & 0xFFFFFFFFU))


/* ------------------------------------------------------------------------ */
/*
 *   Allocate space for a null-terminated string and save a copy of the
 *   string 
 */
char *lib_copy_str(const char *str);
char *lib_copy_str(const char *str, size_t len);

/* 
 *   allocate space for a string of a given length; we'll add in space for
 *   a null terminator 
 */
char *lib_alloc_str(size_t len);

/*
 *   Free a string previously allocated with lib_copy_str() or
 *   lib_alloc_str() 
 */
void lib_free_str(char *buf);

/* ------------------------------------------------------------------------ */
/*
 *   Safe strcpy with an explicit source length.  Truncates the string to the
 *   buffer size, and always null-terminates.  Note that the source string is
 *   NOT null-terminated - we copy the explicit length given, up to the
 *   output size limit.  
 */
inline void lib_strcpy(char *dst, size_t dstsiz,
                       const char *src, size_t srclen)
{
    if (dstsiz > 0)
    {
        size_t copylen = srclen;
        if (copylen > dstsiz - 1)
            copylen = dstsiz - 1;
        memcpy(dst, src, copylen);
        dst[copylen] = '\0';
    }
}

/*
 *   Safe strcpy - checks the output buffer size and truncates the string if
 *   necessary; always null-terminates the result.  
 */
inline void lib_strcpy(char *dst, size_t dstsiz, const char *src)
{
    lib_strcpy(dst, dstsiz, src, strlen(src));
}

/*
 *   Compare two counted-length strings, with or without case sensitivity.
 */
inline int lib_strcmp(const char *str1, size_t len1,
                      const char *str2, size_t len2)
{
    int bylen = len1 - len2, bymem;
    if (bylen == 0)
        return memcmp(str1, str2, len1);
    else if (bylen < 0)
        bymem = memcmp(str1, str2, len1);
    else
        bymem = memcmp(str1, str2, len2);
    return (bymem != 0 ? bymem : bylen);
}
inline int lib_stricmp(const char *str1, size_t len1,
                       const char *str2, size_t len2)
{
    int bylen = len1 - len2, bymem;
    if (bylen == 0)
        return memicmp(str1, str2, len1);
    else if (bylen < 0)
        bymem = memicmp(str1, str2, len1);
    else
        bymem = memicmp(str1, str2, len2);
    return (bymem != 0 ? bymem : bylen);
}

/*
 *   Compare a counted-length string to a regular C string, with or without
 *   case sensitivity.
 */
inline int lib_strcmp(const char *str1, size_t len1, const char *str2)
{
    return lib_strcmp(str1, len1, str2, strlen(str2));
}
inline int lib_stricmp(const char *str1, size_t len1, const char *str2)
{
    return lib_stricmp(str1, len1, str2, strlen(str2));
}


/* ------------------------------------------------------------------------ */
/*
 *   Limited-length strchr.  Searches within the given string for the given
 *   character; stops if we exhaust the length limit 'len' bytes or reach a
 *   null character in the string.  
 */
inline char *lib_strnchr(const char *src, size_t len, int ch)
{
    /* search until we exhaust the length limit or reach a null byte */
    for ( ; len != 0 && *src != '\0' ; --len, ++src)
    {
        /* if this is the character we're looking for, return the pointer */
        if (*src == ch)
            return (char *)src;
    }

    /* didn't find it */
    return 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   Limited-length atoi 
 */
int lib_atoi(const char *str, size_t len);

/*
 *   Limited-length atoi, with auto-advance of the string
 */
int lib_atoi_adv(const char *&str, size_t &len);

/* ------------------------------------------------------------------------ */
/*
 *   Compare two strings, ignoring differences in whitespace between the
 *   strings.  Returns true if the strings are equal (other than
 *   whitespace, false if not.
 *   
 *   Note that we do not ignore the *presence* of whitespace; we only
 *   ignore differences in the amount of whitespace.  For example, "login"
 *   does not equal "log_in" (underscore = whitespace for these examples
 *   only, to emphasize the spacing), because the first lacks whitespace
 *   where the second has it; but "log_in" equals "log___in", because both
 *   strings have whitespace, albeit in different amounts, in the same
 *   place, and are otherwise the same.  
 */
int lib_strequal_collapse_spaces(const char *a, size_t a_len,
                                 const char *b, size_t b_len);


/* ------------------------------------------------------------------------ */
/* 
 *   Utility routine - compare UTF-8 strings with full Unicode case folding.
 *   Returns <0 if a<b, 0 if a==b, >0 if a>b.
 *   
 *   If 'bmatchlen' is non-null, string 'a' can match as a leading substring
 *   of string 'b'.  If 'b' is identical to 'a' or contains 'a' as a leading
 *   substring, we'll return 0 to indicate a match, and fill in '*bmatchlen'
 *   with the number of bytes matched.
 *   
 *   If 'bmatchlen' is null, we'll do a straightforward string comparison, so
 *   a 0 return means the two strings match exactly.
 */
int t3_compare_case_fold(
    const char *a, size_t alen,
    const char *b, size_t blen, size_t *bmatchlen);

/* compare a UTF-8 string against a wchar_t* string */
int t3_compare_case_fold(
    const wchar_t *a, size_t alen,
    const char *b, size_t blen, size_t *bmatchlen);

/*
 *   Compare the minimum portions of two UTF-8 strings with case folding.
 *   This compares the folded version of the first character of each string
 *   to the other.  If they match, we advance the pointers and lengths past
 *   the matched text and return 0; otherwise we return < 0 if the first
 *   string sorts before the second, > 0 if the first sorts after the second.
 *   
 *   In the simplest case, this matches one character from each string.
 *   However, there are situations where the folded version of one character
 *   can correspond to two characters of source text in the other string,
 *   such as the German ess-zed: this will match "ss" in the source text in
 *   the other string, consuming only one character (the ess-zed) in one
 *   string but two ("ss") in the other.
 */
int t3_compare_case_fold_min(class utf8_ptr &a, size_t &alen,
                             class utf8_ptr &b, size_t &blen);

int t3_compare_case_fold_min(class utf8_ptr &a, size_t &alen,
                             const wchar_t* &b, size_t &blen);

int t3_compare_case_fold_min(const wchar_t* &a, size_t &alen,
                             const wchar_t* &b, size_t &blen);


/* ------------------------------------------------------------------------ */
/*
 *   Find a version suffix in an identifier string.  A version suffix
 *   starts with the given character.  If we don't find the character,
 *   we'll return the default version suffix.  In any case, we'll set
 *   name_len to the length of the name portion, excluding the version
 *   suffix and its leading separator.
 *   
 *   For example, with a '/' suffix, a versioned name string would look
 *   like "tads-gen/030000" - the name is "tads_gen" and the version is
 *   "030000".  
 */
const char *lib_find_vsn_suffix(const char *name_string, char suffix_char,
                                const char *default_vsn, size_t *name_len);


/* ------------------------------------------------------------------------ */
/*
 *   Unicode-compatible character classification functions.  These
 *   functions accept any Unicode character, but classify all non-ASCII
 *   characters in the Unicode character set as unknown; hence,
 *   is_digit(ch) will always return false for any non-ASCII character,
 *   even if the character is considered a digit in the Unicode character
 *   set, and to_upper(ch) will return ch for any non-ASCII character,
 *   even if the character has a case conversion defined in the Unicode
 *   set.
 *   
 *   Use the t3_is_xxx() and t3_to_xxx() functions defined vmuni.h for
 *   classifications and conversions that operate over the entire Unicode
 *   character set.  
 */

/* determine if a character is an ASCII character */
inline int is_ascii(wchar_t c) { return (((unsigned int)c) <= 127); }

/* determine if a character is an ASCII space */
inline int is_space(wchar_t c) { return (is_ascii(c) && isspace((char)c)); }

/* determine if a character is an ASCII alphabetic character */
inline int is_alpha(wchar_t c) { return (is_ascii(c) && isalpha((char)c)); }

/* determine if a character is an ASCII numeric character */
inline int is_digit(wchar_t c) { return (is_ascii(c) && isdigit((char)c)); }

/* determine if a character is an ASCII octal numeric character */
inline int is_odigit(wchar_t c)
    { return (is_ascii(c) && isdigit((char)c) && c <= '7'); }

/* determine if a character is an ASCII hex numeric character */
inline int is_xdigit(wchar_t c)
{
    return (is_ascii(c)
            && (isdigit((char)c)
                || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')));
}

/* get the numeric value of a decimal digit character */
inline int value_of_digit(wchar_t c)
{
    return (int)(c - '0');
}

/* get the numeric value of an octal numeric character */
inline int value_of_odigit(wchar_t c)
{
    return (int)(c - '0');
}

/* get the numeric value of a hex numeric character */
inline int value_of_xdigit(wchar_t c)
{
    /* 
     *   since our internal characters are always in unicode, we can take
     *   advantage of the order of unicode characters to reduce the number
     *   of comparisons we must make here 
     */
    return (int)(c >= 'a'
                 ? c - 'a' + 10
                 : c >= 'A' ? c - 'A' + 10
                            : c - '0');
}

/* convert a number 0-15 to a hex digit */
inline char int_to_xdigit(int i)
{
    return ((i >= 0 && i < 10) ? '0' + i :
            (i >= 10 && i < 16 ) ? 'A' + i - 10 :
            '?');
}

/* convert a byte to a pair of hex digits */
inline void byte_to_xdigits(char *buf, unsigned char b)
{
    buf[0] = int_to_xdigit((b >> 4) & 0x0F);
    buf[1] = int_to_xdigit(b & 0x0F);
}

/* determine if a character is a symbol initial character */
inline int is_syminit(wchar_t c)
{
    /* underscores and alphabetic characters can start symbols */
    return (is_ascii(c) && (c == '_' || isalpha((char)c)));
}

/* determine if a character is a symbol non-initial character */
inline int is_sym(wchar_t c)
{
    /* underscores, alphabetics, and digits can be in symbols */
    return (is_ascii(c) && (c == '_' || isalpha((char)c)
                            || isdigit((char)c)));
}

/* determine if a character is ASCII lower-case */
inline int is_lower(wchar_t c) { return (is_ascii(c) && islower((char)c)); }

/* convert ASCII lower-case to upper-case */
inline wchar_t to_upper(wchar_t c)
{
    return (is_ascii(c) ? toupper((char)c) : c);
}

inline wchar_t to_lower(wchar_t c)
{
    return (is_ascii(c) ? tolower((char)c) : c);
}

/* convert a string to lower case */
void t3strlwr(char *p);

/* ------------------------------------------------------------------------ */
/*
 *   sprintf and vsprintf replacements.  These versions provide subsets of
 *   the full 'printf' format capabilities, but check for buffer overflow,
 *   which the standard library's sprintf functions do not.
 *   
 *   NB: the 'args' parameter is effectively const, even though it's not
 *   declared as such.  That is, you can safely call t3vsprintf multiple
 *   times with the same 'args' parameter without worrying that the contents
 *   will be changed on platforms where va_list is a reference type.  (It's
 *   not declared const due to an implementation detail, specifically that
 *   the routine internally needs to make a private copy with va_copy(),
 *   which doesn't accept a const source value.)  
 */
size_t t3sprintf(char *buf, size_t buflen, const char *fmt, ...);
size_t t3vsprintf(char *buf, size_t buflen, const char *fmt, va_list args);

/* 
 *   Automatic memory allocation versions of sprintf and vsprintf: we'll
 *   measure the actual space needed, allocate a buffer, format the message
 *   into the buffer, and return the allocated buffer pointer.  The caller is
 *   responsible for freeing the returned buffer via t3free().  
 */
char *t3sprintf_alloc(const char *fmt, ...);
char *t3vsprintf_alloc(const char *fmt, va_list args);


/* ------------------------------------------------------------------------ */
/*
 *   Basic heap allocation functions.  We don't call malloc and free
 *   directly, but use our own cover functions; when we compile the system
 *   for debugging, we use diagnostic memory allocators so that we can more
 *   easily find memory mismanagement errors (such as leaks, multiple
 *   deletes, and use after deletion).  
 */

#define T3MALLOC_TYPE_MALLOC  1
#define T3MALLOC_TYPE_NEW     2
#define T3MALLOC_TYPE_NEWARR  3

#ifdef T3_DEBUG

/* 
 *   Compiling in debug mode - use our diagnostic heap functions.
 *   Override C++ operators new, new[], delete, and delete[] as well, so
 *   that we can handle those allocations through our diagnostic heap
 *   manager, too.  
 */

void *t3malloc(size_t siz, int alloc_type);
void *t3realloc(void *oldptr, size_t siz);
void  t3free(void *ptr, int alloc_type);

inline void *t3malloc(size_t siz)
    { return t3malloc(siz, T3MALLOC_TYPE_MALLOC); }
inline void *t3mallocnew(size_t siz)
    { return t3malloc(siz, T3MALLOC_TYPE_NEW); }
inline void t3free(void *ptr)
    { t3free(ptr, T3MALLOC_TYPE_MALLOC); }

void *operator new(size_t siz) SYSTHROW(throw (std::bad_alloc));
void *operator new[](size_t siz) SYSTHROW(throw (std::bad_alloc));
void operator delete(void *ptr) SYSTHROW(throw ());
void operator delete[](void *ptr) SYSTHROW(throw ());

/*
 *   List all allocated memory blocks - displays heap information on stdout.
 *   This can be called at program termination to detect un-freed memory
 *   blocks, the existence of which could indicate a memory leak.
 *   
 *   If cb is provided, we'll display output through the given callback
 *   function; otherwise we'll display the output directly on stderr.  
 */
void t3_list_memory_blocks(void (*cb)(const char *msg));

#else /* T3_DEBUG */

/*
 *   Compiling in production mode - use the system memory allocators
 *   directly.  Note that we go through the osmalloc() et. al. functions
 *   rather than calling malloc() directly, so that individual ports can
 *   use customized memory management where necessary or desirable.  
 */
#define t3malloc(siz)          (::osmalloc(siz))
#define t3mallocnew(siz)       (::osmalloc(siz))
#define t3realloc(ptr, siz)    (::osrealloc(ptr, siz))
#define t3free(ptr)            (::osfree(ptr))

#define t3_list_memory_blocks(cb)

#endif /* T3_DEBUG */


/* ------------------------------------------------------------------------ */
/*
 *   A simple array list type.  We keep an underlying array of elements,
 *   automatically expanding the underlying array as needed to accomodate new
 *   elements.  
 */

/* array list element type codes */
#define ARRAY_LIST_ELE_INT   1
#define ARRAY_LIST_ELE_LONG  2
#define ARRAY_LIST_ELE_PTR   3

/* array list element - we can store various types here */
union array_list_ele_t
{
    array_list_ele_t(int i) { intval = i; }
    array_list_ele_t(long l) { longval = l; }
    array_list_ele_t(void *p) { ptrval = p; }

    int intval;
    long longval;
    void *ptrval;

    /* compare to a given value for equality */
    int equals(array_list_ele_t other, int typ)
    {
        return ((typ == ARRAY_LIST_ELE_INT && intval == other.intval)
                || (typ == ARRAY_LIST_ELE_LONG && longval == other.longval)
                || (typ == ARRAY_LIST_ELE_PTR && ptrval == other.ptrval));
    }
};

/*
 *   The array list type 
 */
class CArrayList
{
public:
    CArrayList()
    {
        /* we have nothing allocated yet */
        arr_ = 0;
        cnt_ = 0;

        /* use default initial size and increment */
        alloc_ = 16;
        inc_siz_ = 16;
    }
    CArrayList(size_t init_cnt, size_t inc_siz)
    {
        /* we have nothing allocated yet */
        arr_ = 0;
        cnt_ = 0;

        /* remember the initial size and increment */
        alloc_ = init_cnt;
        inc_siz_ = inc_siz;
    }

    virtual ~CArrayList()
    {
        /* delete our underlying array */
        free_mem(arr_);
    }

    /* get the number of elements in the array */
    size_t get_count() const { return cnt_; }

    /* get the element at the given index (no error checking) */
    int get_ele_int(size_t idx) const { return arr_[idx].intval; }
    long get_ele_long(size_t idx) const { return arr_[idx].longval; }
    void *get_ele_ptr(size_t idx) const { return arr_[idx].ptrval; }

    /* find an element's index; returns -1 if not found */
    int find_ele(int i) const
        { return find_ele(array_list_ele_t(i), ARRAY_LIST_ELE_INT); }
    int find_ele(long l) const
        { return find_ele(array_list_ele_t(l), ARRAY_LIST_ELE_LONG); }
    int find_ele(void *p) const
        { return find_ele(array_list_ele_t(p), ARRAY_LIST_ELE_PTR); }

    /* find an element's index; returns -1 if not found */
    int find_ele(array_list_ele_t ele, int typ) const
    {
        size_t i;
        array_list_ele_t *p;

        /* scan for the element */
        for (i = 0, p = arr_ ; i < cnt_ ; ++i, ++p)
        {
            /* if this is the element, return the index */
            if (p->equals(ele, typ))
                return (int)i;
        }

        /* didn't find it */
        return -1;
    }

    /* add a new element */
    void add_ele(int i) { add_ele(array_list_ele_t(i)); }
    void add_ele(long l) { add_ele(array_list_ele_t(l)); }
    void add_ele(void *p) { add_ele(array_list_ele_t(p)); }

    /* add a new element */
    void add_ele(array_list_ele_t ele)
    {
        /* expand the array if necessary */
        if (arr_ == 0)
        {
            /* we don't have an array yet, so allocate at the initial size */
            init();
        }
        if (cnt_ >= alloc_)
        {
            /* allocate at the new size */
            arr_ = (array_list_ele_t *)
                   realloc_mem(arr_, alloc_ * sizeof(arr_[0]),
                               (alloc_ + inc_siz_) * sizeof(arr_[0]));

            /* remember the new size */
            alloc_ += inc_siz_;
        }

        /* add the new element */
        arr_[cnt_++] = ele;
    }

    /* remove one element by value; returns true if found, false if not */
    void remove_ele(int i)
        { remove_ele(array_list_ele_t(i), ARRAY_LIST_ELE_INT); }
    void remove_ele(long l)
        { remove_ele(array_list_ele_t(l), ARRAY_LIST_ELE_LONG); }
    void remove_ele(void *p)
        { remove_ele(array_list_ele_t(p), ARRAY_LIST_ELE_PTR); }

    /* remove one element by value; returns true if found, false if not */
    int remove_ele(array_list_ele_t ele, int typ)
    {
        size_t i;
        array_list_ele_t *p;

        /* scan for the element */
        for (i = 0, p = arr_ ; i < cnt_ ; ++i, ++p)
        {
            /* if this is the element, remove it */
            if (p->equals(ele, typ))
            {
                /* remove the element at this index */
                remove_ele(i);

                /* indicate that we found the element */
                return TRUE;
            }
        }

        /* we didn't find the element */
        return FALSE;
    }

    /* remove the element at the given index */
    void remove_ele(size_t idx)
    {
        array_list_ele_t *p;

        /* move each following element down one slot */
        for (p = arr_ + idx, ++idx ; idx < cnt_ ; ++idx, ++p)
            *p = *(p + 1);

        /* reduce the in-use count */
        --cnt_;
    }

    /* clear the entire list */
    void clear() { cnt_ = 0; }

protected:
    /* 
     *   Initialize.  This is called to set up the array at the initial size,
     *   stored in alloc_, when we first need memory.  Note that we defer
     *   this until we actually need the memory for two reasons.  First, we
     *   can't call it from the constructor, because the vtable won't be
     *   built at construction, and we need to call the virtual alloc_mem().
     *   Second, by waiting, we ensure that we won't allocate any memory if
     *   our list is never actually needed.  
     */
    void init()
    {
        /* allocate the array */
        arr_ = (array_list_ele_t *)alloc_mem(alloc_ * sizeof(arr_[0]));
    }

    /* memory management */
    virtual void *alloc_mem(size_t siz)
        { return t3malloc(siz); }
    virtual void *realloc_mem(void *p, size_t oldsiz, size_t newsiz)
        { return t3realloc(p, newsiz); }
    virtual void free_mem(void *p)
        { t3free(p); }

    /* our array of elements */
    array_list_ele_t *arr_;

    /* number of elements allocated */
    size_t alloc_;

    /* number of elements currently in use */
    size_t cnt_;

    /* increment size */
    size_t inc_siz_;
};

#endif /* T3_STD_INCLUDED */

