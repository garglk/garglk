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

#include <stdlib.h>
#include "wchar.h"
#include "t3std.h"


/* ------------------------------------------------------------------------ */
/*
 *   Upper/lower case classification and conversion functions for Unicode
 *   character values.  These routines are implemented in a
 *   machine-generated source file (the source file is mechanically
 *   derived from the Unicode character database).
 */
int t3_is_lower(wchar_t ch);
int t3_is_upper(wchar_t ch);
int t3_is_title(wchar_t ch);
const wchar_t *t3_to_lower(wchar_t ch);
const wchar_t *t3_to_upper(wchar_t ch);
const wchar_t *t3_to_title(wchar_t ch);
const wchar_t *t3_to_fold(wchar_t ch);
wchar_t t3_simple_case_fold(wchar_t ch);

/*
 *   Character types.  Types are mutually exclusive, so a character has
 *   exactly one type.  
 */
#define T3_CTYPE_UNDEF  0                        /* character isn't defined */
#define T3_CTYPE_ALPHA  1           /* alphabetic, with no case information */
#define T3_CTYPE_UPPER  2                          /* upper-case alphabetic */
#define T3_CTYPE_TITLE  3                          /* title-case alphabetic */
#define T3_CTYPE_LOWER  4                          /* lower-case alphabetic */
#define T3_CTYPE_DIGIT  5                                          /* digit */
#define T3_CTYPE_SPACE  6                          /* horizontal whitespace */
#define T3_CTYPE_VSPAC  7                            /* vertical whitespace */
#define T3_CTYPE_PUNCT  8                                    /* punctuation */
#define T3_CTYPE_OTHER  9       /* character doesn't fit any other category */

/* macro name strings, in order of appearance */
#define T3_CTYPE_NAMES \
    "T3_CTYPE_UNDEF", "T3_CTYPE_ALPHA", "T3_CTYPE_UPPER", "T3_CTYPE_TITLE", \
    "T3_CTYPE_LOWER", "T3_CTYPE_DIGIT", "T3_CTYPE_SPACE", "T3_CTYPE_VSPAC", \
    "T3_CTYPE_PUNCT", "T3_CTYPE_OTHER"

/* get the character type */
int t3_get_chartype(wchar_t ch);

/* 
 *   character classification functions 
 */

/* alphabetic? */
inline int t3_is_alpha(wchar_t ch)
{
    int ctype = t3_get_chartype(ch);
    return (ctype >= T3_CTYPE_ALPHA && ctype <= T3_CTYPE_LOWER);
}

/* uppercase? */
inline int t3_is_upper(wchar_t ch)
{
    int ctype = t3_get_chartype(ch);
    return (ctype == T3_CTYPE_UPPER || ctype == T3_CTYPE_TITLE);
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
    return (t3_get_chartype(ch) == T3_CTYPE_VSPAC);
}

/* any whitespace, horizontal or vertical? */
inline int t3_is_whitespace(wchar_t ch)
{
    int t = t3_get_chartype(ch);
    return (t == T3_CTYPE_SPACE || t == T3_CTYPE_VSPAC);
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

/* ------------------------------------------------------------------------ */
/*
 *   Case folding wchar_t string reader
 */
class CVmCaseFoldStr
{
public:
    CVmCaseFoldStr(const wchar_t *s) { init(s, wcslen(s)); }
    CVmCaseFoldStr(const wchar_t *s, size_t len) { init(s, len); }

    /* get the current pointer */
    const wchar_t *getptr() const { return p; }

    /* is another character available? */
    int more() const { return rem != 0 || *fp != 0 || fp == ie; }

    /* are we at a character boundary? */
    int at_boundary() const { return fp != fpbase && *fp == 0; }

    /* get the next character */
    wchar_t getch()
    {
        /* if the expansion is exhausted, expand the next source character */
        if (fp != ie && *fp == 0)
        {
            /* if there's nothing left in the source string, we're done */
            if (rem == 0)
                return 0;

            /* get the next source character */
            wchar_t ch = *p++;
            --rem;

            /* get its case-folded expansion */
            if ((fp = t3_to_fold(ch)) == 0)
            {
                ie[0] = ch;
                fpbase = fp = ie;
            }
        }

        /* return the next expansion character */
        return *fp++;
    }

    /*
     *   Case-insensitive wide-string comparison routine.  Returns true if
     *   the strings are equal, false if not.
     */
    static int wstreq(const wchar_t *a, const wchar_t *b)
    {
        /* loop over characters until we find a mismatch */
        for (CVmCaseFoldStr fa(a), fb(b) ; ; )
        {
            /* get the next character of each folded string */
            wchar_t ca = fa.getch();
            wchar_t cb = fb.getch();
            
            /* if they differ, return not equal */
            if (ca != cb)
                return FALSE;
            
            /* if they both ended here, we have a match */
            if (ca == 0)
                return TRUE;
        }
    }

private:
    void init(const wchar_t *s, size_t len)
    {
        /* set up at the start of the string */
        this->p = s;
        this->rem = len;

        /* null-terminate our special one-byte identity conversion buffer */
        ie[1] = 0;

        /* start without anything loaded */
        fpbase = ie;
        fp = &ie[1];
    }

    /* current position in case folding expansion of current source char */
    const wchar_t *fp;

    /* start of current folding expansion */
    const wchar_t *fpbase;

    /* buffer for identity expansions */
    wchar_t ie[2];

    /* original source string and character length */
    const wchar_t *p;
    size_t rem;
};

#endif /* VMUNI_H */

