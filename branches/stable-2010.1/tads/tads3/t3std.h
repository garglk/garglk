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

#include "os.h"


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

/* maximum/minimum portable values for various types */
#define ULONGMAXVAL   0xffffffffUL
#define USHORTMAXVAL  0xffffU
#define UCHARMAXVAL   0xffU
#define SLONGMAXVAL   0x7fffffffL
#define SSHORTMAXVAL  0x7fff
#define SCHARMAXVAL   0x7f
#define SLONGMINVAL   (-(0x7fffffff)-1)
#define SSHORTMINVAL  (-(0x7fff)-1)
#define SCHARMINVAL   (-(0x7f)-1)


/*
 *   Text character 
 */
typedef char textchar_t;

/*
 *   16-bit signed/unsigned integer types 
 */
#ifndef OS_INT16_DEFINED
typedef short int16;
#endif
#ifndef OS_UINT16_DEFINED
typedef unsigned short uint16;
#endif

/*
 *   32-bit signed/unsigned integer types 
 */
#ifndef OS_INT32_DEFINED
typedef long int32;
#endif
#ifndef OS_UINT32_DEFINED
typedef unsigned long uint32;
#endif


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
 *   Safe strcpy - checks the output buffer size and truncates the string if
 *   necessary; always null-terminates the result.  
 */
inline void lib_strcpy(char *dst, size_t dstsiz, const char *src)
{
    if (dstsiz > 0)
    {
        size_t copylen = strlen(src);
        if (copylen > dstsiz - 1)
            copylen = dstsiz - 1;
        memcpy(dst, src, copylen);
        dst[copylen] = '\0';
    }
}

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

/* ------------------------------------------------------------------------ */
/*
 *   sprintf and vsprintf replacements.  These versions provide subsets of
 *   the full 'printf' format capabilities, but check for buffer overflow,
 *   which the standard library's sprintf functions do not.  
 */
void t3sprintf(char *buf, size_t buflen, const char *fmt, ...);
void t3vsprintf(char *buf, size_t buflen, const char *fmt, va_list args);


/* ------------------------------------------------------------------------ */
/*
 *   Basic heap allocation functions.  We don't call malloc and free
 *   directly, but use our own cover functions; when we compile the system
 *   for debugging, we use diagnostic memory allocators so that we can more
 *   easily find memory mismanagement errors (such as leaks, multiple
 *   deletes, and use after deletion).  
 */

#ifdef T3_DEBUG

/* 
 *   Compiling in debug mode - use our diagnostic heap functions.
 *   Override C++ operators new, new[], delete, and delete[] as well, so
 *   that we can handle those allocations through our diagnostic heap
 *   manager, too.  
 */

void *t3malloc(size_t siz);
void *t3realloc(void *oldptr, size_t siz);
void  t3free(void *ptr);

void *operator new(size_t siz);
void *operator new[](size_t siz);
void operator delete(void *ptr);
void operator delete[](void *ptr);

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

