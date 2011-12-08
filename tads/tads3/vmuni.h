/* $Header$ */

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmuni.h - T3 VM Unicode-specific functions
Function
  
Notes
  
Modified
  08/27/99 MJRoberts  - Creation
*/

#ifndef VMUNI_H
#define VMUNI_H

#include "stdlib.h"

/* ------------------------------------------------------------------------ */
/*
 *   Upper/lower case classification and conversion functions for Unicode
 *   character values.  These routines are implemented in a
 *   machine-generated source file (the source file is mechanically
 *   derived from the Unicode character database).
 */
int t3_is_lower(wchar_t ch);
int t3_is_upper(wchar_t ch);
wchar_t t3_to_lower(wchar_t ch);
wchar_t t3_to_upper(wchar_t ch);

/*
 *   Character types.  Types are mutually exclusive, so a character has
 *   exactly one type.  
 */
#define T3_CTYPE_UNDEF  0                        /* character isn't defined */
#define T3_CTYPE_ALPHA  1           /* alphabetic, with no case information */
#define T3_CTYPE_UPPER  2                          /* upper-case alphabetic */
#define T3_CTYPE_LOWER  3                          /* lower-case alphabetic */
#define T3_CTYPE_DIGIT  4                                          /* digit */
#define T3_CTYPE_SPACE  5                          /* horizontal whitespace */
#define T3_CTYPE_PUNCT  6                                    /* punctuation */
#define T3_CTYPE_VSPACE 7                            /* vertical whitespace */
#define T3_CTYPE_NONE   8       /* character doesn't fit any other category */

/* get the character type */
unsigned char t3_get_chartype(wchar_t ch);

/* 
 *   character classification functions 
 */

/* alphabetic? */
inline int t3_is_alpha(wchar_t ch)
{
    unsigned char ctype;

    ctype = t3_get_chartype(ch);
    return (ctype == T3_CTYPE_UPPER
            || ctype == T3_CTYPE_LOWER
            || ctype == T3_CTYPE_ALPHA);
            
}

/* uppercase? */
inline int t3_is_upper(wchar_t ch)
{
    return (t3_get_chartype(ch) == T3_CTYPE_UPPER);
}

/* lowercase? */
inline int t3_is_lower(wchar_t ch)
{
    return (t3_get_chartype(ch) == T3_CTYPE_LOWER);
}

/* digit? */
inline int t3_is_digit(wchar_t ch)
{
    return (t3_get_chartype(ch) == T3_CTYPE_DIGIT);
}

/* horizontal whitespace? */
inline int t3_is_space(wchar_t ch)
{
    return (t3_get_chartype(ch) == T3_CTYPE_SPACE);
}

/* vertical whitespace? */
inline int t3_is_vspace(wchar_t ch)
{
    return (t3_get_chartype(ch) == T3_CTYPE_VSPACE);
}

/* any whitespace, horizontal or vertical? */
inline int t3_is_whitespace(wchar_t ch)
{
    int t = t3_get_chartype(ch);
    return (t == T3_CTYPE_SPACE || t == T3_CTYPE_VSPACE);
}

/* punctuation? */
inline int t3_is_punct(wchar_t ch)
{
    return (t3_get_chartype(ch) == T3_CTYPE_PUNCT);
}

/* is it a defined unicode character? */
inline int t3_is_unichar(wchar_t ch)
{
    return (t3_get_chartype(ch) != T3_CTYPE_UNDEF);
}

#endif /* VMUNI_H */

