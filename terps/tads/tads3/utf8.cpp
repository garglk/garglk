#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/tads3/utf8.cpp,v 1.2 1999/05/17 02:52:29 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  utf8.cpp - UTF-8 implementation
Function
  
Notes
  
Modified
  10/17/98 MJRoberts  - Creation
*/

#include "utf8.h"

/* ------------------------------------------------------------------------ */
/*
 *   encode a string of wide characters into the buffer 
 */
size_t utf8_ptr::setwchars(const wchar_t *src, size_t src_count,
                           size_t bufsiz)
{
    /* loop through the source and store the characters */
    size_t outbytes;
    for (outbytes = 0 ; src_count > 0 ; --src_count, ++src)
    {
        /* figure out how many bytes we need for this character */
        size_t curbytes = s_wchar_size(*src);

        /* add it to the total output size */
        outbytes += curbytes;

        /* if we have room, add it to the buffer */
        if (bufsiz >= curbytes)
        {
            /* store it */
            setch(*src);
            
            /* deduct this space from the remaining buffer size */
            bufsiz -= curbytes;
        }
        else
        {
            /* 
             *   there's no room for this - make sure we don't store
             *   anything more (since we might have room for a shorter
             *   character later, but that would put a gap in the output
             *   string - better to just truncate here) 
             */
            bufsiz = 0;
        }
    }

    /* return the total output size used (or needed) */
    return outbytes;
}

/* ------------------------------------------------------------------------ */
/*
 *   encode a null-terminated string of wide characters into the buffer 
 */
size_t utf8_ptr::setwcharsz(const wchar_t *src, size_t bufsiz)
{
    /* loop through the source and store the characters */
    size_t outbytes;
    for (outbytes = 0 ; *src != 0 ; ++src)
    {
        /* figure out how many bytes we need for this character */
        size_t curbytes = s_wchar_size(*src);

        /* add it to the total output size */
        outbytes += curbytes;

        /* if we have room, add it to the buffer */
        if (bufsiz >= curbytes)
        {
            /* store it */
            setch(*src);

            /* deduct this space from the remaining buffer size */
            bufsiz -= outbytes;
        }
        else
        {
            /* 
             *   there's no room for this - make sure we don't store
             *   anything more (since we might have room for a shorter
             *   character later, but that would put a gap in the output
             *   string - better to just truncate here) 
             */
            bufsiz = 0;
        }
    }

    /* return the total output size used (or needed) */
    return outbytes;
}

/* ------------------------------------------------------------------------ */
/*
 *   Compare this string to the given string 
 */
int utf8_ptr::s_compare_to(const char *p1, size_t bytelen1,
                           const char *p2, size_t bytelen2)
{
    /* keep going until one or the other string runs out of bytes */
    while (bytelen1 != 0 && bytelen2 != 0)
    {
        wchar_t c1, c2;
        size_t siz1, siz2;
        
        /* get the current character from each string */
        c1 = s_getch(p1);
        c2 = s_getch(p2);

        /* compare them */
        if (c1 > c2)
            return 1;
        else if (c1 < c2)
            return -1;

        /* get the size of each character */
        siz1 = s_charsize(*p1);
        siz2 = s_charsize(*p2);

        /* decrement each counter by the byte size of this character */
        bytelen1 -= siz1;
        bytelen2 -= siz2;

        /* advance to the next character in each string */
        p1 += siz1;
        p2 += siz2;
    }

    /* 
     *   we didn't find any character differences, but one string is
     *   longer than the other -- if they ran out at the same time,
     *   they're identical; otherwise, the one that ran out first is the
     *   lesser one 
     */
    if (bytelen2 != 0)
    {
        /* the first one ran out first, so the first one sorts earlier */
        return -1;
    }
    else if (bytelen1 != 0)
    {
        /* the second one ran out first, so the first one sort later */
        return 1;
    }
    else
    {
        /* they both ran out at the same time */
        return 0;
    }
}

