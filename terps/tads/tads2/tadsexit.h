/* 
 *   Copyright (c) 1992, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tadsexit.h  - user exit definitions for TADS version 2
Function
  Defines the interface for user exit functions
Notes
  User exits need a way of getting to certain internal TADS functions.
  These functions are provided as a vector of function pointers in
  a structure passed to the user exit:  the tadsuxdef.
Modified
  04/30/92 MJRoberts     - creation
*/

#ifndef TADSEXIT_INCLUDED
# define TADSEXIT_INCLUDED

/*
 *   On MS-DOS, user exits are compiled in "tiny" model (suitable for
 *   making into a .COM file), but since TADS is a large-model program,
 *   data and functions provided by TADS must be referenced with "far"
 *   pointers.  This keyword is not accepted by compilers on most other
 *   systems, so we'll define the keyword to nothing if we're not on
 *   MS-DOS.  Be sure use the compiler option -DMSDOS when building on
 *   MS-DOS!
 */
#ifndef osfar_t
# ifdef MSDOS
#  define osfar_t far
# else
#  define osfar_t
# endif /* MSDOS */
#endif /* osfar_t */

/*
 *   TADS datatypes usable by user-exit functions.  These types are
 *   returned by tads_tostyp, and used in tads_push for the 'type'
 *   argument.  
 */
#define TADS_NUMBER    1                         /* a number (long integer) */
#define TADS_SSTRING   3                 /* constant (single-quoted) string */
#define TADS_NIL       5                           /* nil (false, no value) */
#define TADS_TRUE      8                                            /* true */
/* #define TADS_RSTRING  12 */                     /* OBSOLETE - DO NOT USE */

/*
 *   "String descriptor" type.  Use this type to hold the return value of
 *   the tads_popstr() function.  Note that a string descriptor is NOT a
 *   pointer to a C-style null-terminated string; instead, you must use
 *   the tads_strlen() and tads_strptr() functions to get the length and
 *   text pointer from a descriptor.  
 */
typedef char osfar_t *tads_strdesc;


/*
 *   tadsufdef: run-time system callback function vector.  
 */
struct tadsufdef
{
    /* type of top of stack */
    int          (osfar_t *tadsuftyp)(void osfar_t *);
    
    /* pop a number */
    long         (osfar_t *tadsufnpo)(void osfar_t *);

    /* pop a string */
    tads_strdesc (osfar_t *tadsufspo)(void osfar_t *);

    /* discard top item of stack */
    void         (osfar_t *tadsufdsc)(void osfar_t *);
    
    /* push a number */
    void         (osfar_t *tadsufnpu)(void osfar_t *, long);
    
    /* push TADS string (allocated with tads_stralo) */
    void         (osfar_t *tadsufspu)(void osfar_t *, unsigned char osfar_t *);
    
    /* push a C-style null-terminated string */
    void         (osfar_t *tadsufcspu)(void osfar_t *, char osfar_t *);

    /* allocate new string */
    char osfar_t *(osfar_t *tadsufsal)(void osfar_t *, int);
    
    /* push a logical value */
    void         (osfar_t *tadsuflpu)(void osfar_t *, int);
};
typedef struct tadsufdef tadsufdef;

/*
 *   tadsuxdef:  context argument passed into user exit function.  Used
 *   to identify callback routines.
 */
struct tadsuxdef
{
    struct    tadscbdef osfar_t *tadsuxcb;              /* callback context */
    tadsufdef osfar_t           *tadsuxuf;           /* vector of functions */
    int                          tadsuxac;                /* argument count */
};
typedef struct tadsuxdef tadsuxdef;


/* internal service macros */
#define tads_vec(ctx) ((ctx)->tadsuxuf)
#define tads_c2u(p,i) ((unsigned int)(((unsigned char osfar_t *)(p))[i]))


/*
 *   These macros provide easy access to the TADS access functions.  All
 *   of the callback macros require the TADS function vector (passed into
 *   the user exit as its argument) as the first argument.
 */

/*
 *   return the datatype (TADS_xxx) of the value on top of the stack
 *   (i.e., the next value that will be returned by tads_pop) 
 */
#define tads_tostyp(ctx) ((*tads_vec(ctx)->tadsuftyp)(ctx))

/*
 *   Pop a number off stack (use when the item on top of the stack is a
 *   number, TADS_NUMBER).  Returns a signed long.
 */
#define tads_popnum(ctx) ((*tads_vec(ctx)->tadsufnpo)(ctx))

/*
 *   Pop a string off the stack.  The pointer returned is to a string
 *   descriptor.  You will need to use the tads_strlen() and tads_strptr()
 *   functions to get the length of the string and a pointer to its
 *   buffer -- do not attempt to use the returned pointer directly as
 *   a string.
 */
#define tads_popstr(ctx) ((*tads_vec(ctx)->tadsufspo)(ctx))

/*
 *   Get the length of a string retrieved with tads_popstr().  
 */
#define tads_strlen(ctx, str) (tads_c2u(str, 0) + (tads_c2u(str, 1) << 8) - 2)

/*
 *   Get a pointer to the string buffer from a value retrieved with
 *   tads_popstr().  The value returned points to the actual text of
 *   the string.  Note that this text will NOT be null-terminated;
 *   you must use tads_strlen() to determine the length of the string.
 */
#define tads_strptr(ctx, str) ((char osfar_t *)((str) + 2))

/*
 *   pop next item off stack and discard it (use to remove TADS_TRUE and
 *   TADS_NIL values from the stack) 
 */
#define tads_pop(ctx) ((*tads_vec(ctx)->tadsufdsc)(ctx))

/*
 *   Push a number onto the stack.  
 */
#define tads_pushnum(ctx, num) ((*tads_vec(ctx)->tadsufnpu)(ctx, (long)(num)))

/*
 *   Push a C-style string onto the stack.  The string must be a normal
 *   C-style null-terminated string.  It should not have been allocated
 *   with tads_stralo(). 
 */
#define tads_pushcstr(ctx, cstr) ((*tads_vec(ctx)->tadsufcspu)(ctx, str))

/*
 *   Allocate space for a string.  The string written to the buffer
 *   returned does NOT need to be null-terminated, but it can be; the null
 *   byte will be ignored if it is present.  The length you allocate will
 *   be the length of the string, regardless of the presence of any null
 *   bytes.  When you allocate a string with this function, push it with
 *   the tads_pushastr() function.  Do NOT push this string with the
 *   tads_pushcstr() function, which is only for C-style strings.  This
 *   function returns a pointer to the buffer you should use for the
 *   string.
 */
#define tads_stralo(ctx, len) ((*tads_vec(ctx)->tadsufsal)(ctx, (int)(len)))

/*
 *   Push a string allocated with tads_stralo().  
 */
#define tads_pushastr(ctx, str) ((*tads_vec(ctx)->tadsufspu)(ctx, str))


/* Push a logical value (true or nil) onto stack */
#define tads_pushtrue(ctx) ((*tads_vec(ctx)->tadsuflpu)(ctx, TADS_TRUE))
#define tads_pushnil(ctx)  ((*tads_vec(ctx)->tadsuflpu)(ctx, TADS_NIL))


/* get number of arguments to this function */
#define tads_argc(ctx) ((ctx)->tadsuxac)


#endif /* TADSEXIT_INCLUDED */
