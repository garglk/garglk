#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/tads3/STD.CPP,v 1.3 1999/07/11 00:46:52 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  std.cpp - T3 library functions
Function
  
Notes
  
Modified
  04/16/99 MJRoberts  - Creation
*/

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include "os.h"
#include "t3std.h"
#include "utf8.h"
#include "osifcnet.h"


/* ------------------------------------------------------------------------ */
/*
 *   Allocate space for a string of a given length.  We'll add in space
 *   for the null terminator.  
 */
char *lib_alloc_str(size_t len)
{
    char *buf;

    /* allocate the space */
    buf = (char *)t3malloc(len + 1);

    /* make sure it's initially null-terminated */
    buf[0] = '\0';

    /* return the space */
    return buf;
}

/*
 *   Allocate space for a string of known length, and save a copy of the
 *   string.  The length does not include a null terminator, and in fact
 *   the string does not need to be null-terminated.  The copy returned,
 *   however, is null-terminated.  
 */
char *lib_copy_str(const char *str, size_t len)
{
    /* if the source string is null, just return null as the result */
    if (str == 0)
        return 0;

    /* allocate space */
    char *buf = lib_alloc_str(len);

    /* if that succeeded, make a copy */
    if (buf != 0)
    {
        /* copy the string */
        memcpy(buf, str, len);

        /* null-terminate it */
        buf[len] = '\0';
    }

    /* return the buffer */
    return buf;
}

/*
 *   allocate and copy a null-terminated string 
 */
char *lib_copy_str(const char *str)
{
    return (str == 0 ? 0 : lib_copy_str(str, strlen(str)));
}

/*
 *   Free a string previously allocated with lib_copy_str() 
 */
void lib_free_str(char *buf)
{
    if (buf != 0)
        t3free(buf);
}

/* ------------------------------------------------------------------------ */
/*
 *   Utility routine: compare spaces, collapsing whitespace 
 */
int lib_strequal_collapse_spaces(const char *a, size_t a_len,
                                 const char *b, size_t b_len)
{
    const char *a_end;
    const char *b_end;
    utf8_ptr ap, bp;

    /* calculate where the strings end */
    a_end = a + a_len;
    b_end = b + b_len;
    
    /* keep going until we run out of strings */
    for (ap.set((char *)a), bp.set((char *)b) ;
         ap.getptr() < a_end && bp.getptr() < b_end ; )
    {
        /* check to see if we have whitespace in both strings */
        if (is_space(ap.getch()) && is_space(bp.getch()))
        {
            /* skip all whitespace in both strings */
            for (ap.inc() ; ap.getptr() < a_end && is_space(ap.getch()) ;
                 ap.inc()) ;
            for (bp.inc() ; bp.getptr() < b_end && is_space(bp.getch()) ;
                 bp.inc()) ;

            /* keep going */
            continue;
        }

        /* if the characters here don't match, we don't have a match */
        if (ap.getch() != bp.getch())
            return FALSE;

        /* move on to the next character of each string */
        ap.inc();
        bp.inc();
    }

    /* 
     *   if both strings ran out at the same time, we have a match;
     *   otherwise, they're not the same 
     */
    return (ap.getptr() == a_end && bp.getptr() == b_end);
}

/* ------------------------------------------------------------------------ */
/*
 *   Utility routine: do a case-insensitive comparison of two UTF-8 strings.
 *   Returns strcmp-style results: negative if a < b, 0 if a == b, positive
 *   if a > b.
 *   
 *   If 'bmatchlen' is null, it means that the two strings must have the same
 *   number of characters.  Otherwise, we'll return 0 (equal) if 'a' is a
 *   leading substring of 'b', and fill in '*bmatchlen' with the length in
 *   bytes of the 'b' string that we matched.  This might differ from the
 *   length of the 'a' string because of case folding.  To match as a leading
 *   substring, we have to match to a character boundary.  E.g., we won't
 *   match "weis" as a leading substring of "weiß": while "weis" is indeed a
 *   leading substring of "weiss", which is the case-folded version of
 *   "weiß", it doesn't end at a character boundary in the original.
 */
int t3_compare_case_fold(
    const char *a, size_t alen,
    const char *b, size_t blen, size_t *bmatchlen)
{
    /* set up folded-case string readers for the two strings */
    Utf8FoldStr ap(a, alen), bp(b, blen);

    /* scan until we find a mismatch or run out of one string */
    while(ap.more() && bp.more())
    {
        /* get the next character of each string */
        wchar_t ach = ap.getch(), bch = bp.getch();

        /* if they're different, return the sign difference */
        if (ach != bch)
            return ach - bch;
    }

    /* 
     *   if 'a' ran out first, and we have a 'bmatchlen' pointer, then we're
     *   being asked if 'a' is a leading substring of 'b', which it is - fill
     *   in '*bmatchlen' with the length of 'b' that we matched, and return
     *   ture 
     */
    if (bmatchlen != 0 && !ap.more() && bp.at_boundary())
    {
        *bmatchlen = bp.getptr() - b;
        return 0;
    }

    /* which ran out first was shorter, so sorts first */
    return ap.more() ? 1 : bp.more() ? -1 : 0;
}

/*
 *   compare a wchar_t string against a utf-8 string with case folding
 */
int t3_compare_case_fold(
    const wchar_t *a, size_t alen,
    const char *b, size_t blen, size_t *bmatchlen)
{
    /* set up folded-case string readers for the two strings */
    CVmCaseFoldStr ap(a, alen);
    Utf8FoldStr bp(b, blen);

    /* scan until we find a mismatch or run out of one string */
    while (ap.more() && bp.more())
    {
        /* get the next character of each string */
        wchar_t ach = ap.getch(), bch = bp.getch();

        /* if they're different, return the sign difference */
        if (ach != bch)
            return ach - bch;
    }

    /* 
     *   if 'a' ran out first, and we have a 'bmatchlen' pointer, then we're
     *   being asked if 'a' is a leading substring of 'b', which it is - fill
     *   in '*bmatchlen' with the length of 'b' that we matched, and return
     *   ture 
     */
    if (bmatchlen != 0 && !ap.more() && bp.at_boundary())
    {
        *bmatchlen = bp.getptr() - b;
        return 0;
    }

    /* which ran out first was shorter, so sorts first */
    return ap.more() ? 1 : bp.more() ? -1 : 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   Compare the minimum number of characters in each string with case
 *   folding. 
 */
int t3_compare_case_fold_min(
    utf8_ptr &a, size_t &alen, utf8_ptr &b, size_t &blen)
{
    /* if either is empty, there can be no match (unless both are empty) */
    if (alen == 0 || blen == 0)
        return alen - blen;

    /* set up folded-case string readers for the two strings */
    Utf8FoldStr ap(a.getptr(), alen), bp(b.getptr(), blen);

    /* 
     *   Scan until we're at a boundary in both strings.  Note that we start
     *   at a boundary, so always compare at least one character. 
     */
    do
    {
        /* get the next character of each string */
        wchar_t ach = ap.getch(), bch = bp.getch();

        /* if they're different, return the sign difference */
        if (ach != bch)
            return ach - bch;
    }
    while (!ap.at_boundary() || !bp.at_boundary());

    /* 
     *   If we made it to a boundary in each string without finding a
     *   difference, we have a match.  Advance each string past the matched
     *   text.
     */
    size_t ainc = ap.getptr() - a.getptr();
    alen -= ainc;
    a.inc_bytes(ainc);

    size_t binc = bp.getptr() - b.getptr();
    blen -= binc;
    b.inc_bytes(binc);

    /* return "equal" */
    return 0;
}

int t3_compare_case_fold_min(
    utf8_ptr &a, size_t &alen, const wchar_t *&b, size_t &blen)
{
    /* if either is empty, there can be no match (unless both are empty) */
    if (alen == 0 || blen == 0)
        return alen - blen;

    /* set up folded-case string readers for the two strings */
    Utf8FoldStr ap(a.getptr(), alen);
    CVmCaseFoldStr bp(b, blen);

    /* 
     *   Scan until we're at a boundary in both strings.  Note that we start
     *   at a boundary, so always compare at least one character. 
     */
    do
    {
        /* get the next character of each string */
        wchar_t ach = ap.getch(), bch = bp.getch();

        /* if they're different, return the sign difference */
        if (ach != bch)
            return ach - bch;
    }
    while (!ap.at_boundary() || !bp.at_boundary());

    /* 
     *   If we made it to a boundary in each string without finding a
     *   difference, we have a match.  Advance each string past the matched
     *   text.
     */
    size_t ainc = ap.getptr() - a.getptr();
    alen -= ainc;
    a.inc_bytes(ainc);

    blen -= bp.getptr() - b;
    b = bp.getptr();

    /* return "equal" */
    return 0;
}

int t3_compare_case_fold_min(
    const wchar_t* &a, size_t &alen, const wchar_t* &b, size_t &blen)
{
    /* if either is empty, there can be no match (unless both are empty) */
    if (alen == 0 || blen == 0)
        return alen - blen;

    /* set up folded-case string readers for the two strings */
    CVmCaseFoldStr ap(a, alen), bp(b, blen);

    /* 
     *   Scan until we're at a boundary in both strings.  Note that we start
     *   at a boundary, so always compare at least one character. 
     */
    do
    {
        /* get the next character of each string */
        wchar_t ach = ap.getch(), bch = bp.getch();

        /* if they're different, return the sign difference */
        if (ach != bch)
            return ach - bch;
    }
    while (!ap.at_boundary() || !bp.at_boundary());

    /* 
     *   If we got this far, we made it to a boundary in each string without
     *   finding a difference, so we have a match.  Advance each string past
     *   the matched text.
     */
    alen -= ap.getptr() - a;
    a = ap.getptr();

    blen -= bp.getptr() - b;
    b = bp.getptr();

    /* return "equal" */
    return 0;
}


/* ------------------------------------------------------------------------ */
/*
 *   Limited-length atoi 
 */
int lib_atoi(const char *str, size_t len)
{
    /* parse the sign, if present */
    int s = 1;
    if (len >= 1 && *str == '-')
        s = -1, ++str, --len;
    else if (len >= 1 && *str == '+')
        ++str, --len;

    /* scan digits */
    int acc;
    for (acc = 0 ; len > 0 && is_digit(*str) ;
         acc *= 10, acc += value_of_digit(*str), ++str, --len) ;

    /* apply the sign and return the result */
    return s * acc;
}

/*
 *   Limited-length atoi, with auto-advance of the string
 */
int lib_atoi_adv(const char *&str, size_t &len)
{
    /* parse the sign, if present */
    int s = 1;
    if (len >= 1 && *str == '-')
        s = -1, ++str, --len;
    else if (len >= 1 && *str == '+')
        ++str, --len;

    /* scan digits */
    int acc;
    for (acc = 0 ; len > 0 && is_digit(*str) ;
         acc *= 10, acc += value_of_digit(*str), ++str, --len) ;

    /* apply the sign and return the result */
    return s * acc;
}


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
                                const char *default_vsn, size_t *name_len)
{
    const char *vsn;
    
    /* find the suffix character, if any */
    for (vsn = name_string ; *vsn != '\0' && *vsn != suffix_char ; ++vsn);

    /* note the length of the name portion */
    *name_len = vsn - name_string;

    /* 
     *   skip the separator if we found one, to point vsn at the start of
     *   the suffix string itself - it we didn't find the separator
     *   character, use the default version string
     */
    if (*vsn == suffix_char)
        ++vsn;
    else
        vsn = default_vsn;

    /* return the version string */
    return vsn;
}


/* ------------------------------------------------------------------------ */
/*
 *   allocating sprintf implementation 
 */
char *t3sprintf_alloc(const char *fmt, ...)
{
    /* package the arguments as a va_list and invoke our va_list version */
    va_list args;
    va_start(args, fmt);
    char *str = t3vsprintf_alloc(fmt, args);
    va_end(args);

    /* return the allocated string */
    return str;
}

/*
 *   allocating vsprintf implementation 
 */
char *t3vsprintf_alloc(const char *fmt, va_list args)
{
    /* measure the required space - add in a byte for null termination */
    size_t len = t3vsprintf(0, 0, fmt, args) + 1;

    /* allocate space */
    char *buf = (char *)t3malloc(len);
    if (buf == 0)
        return 0;

    /* do the actual formatting */
    t3vsprintf(buf, len, fmt, args);

    /* return the allocated buffer */
    return buf;
}

/*
 *   buffer-checked sprintf implementation
 */
size_t t3sprintf(char *buf, size_t buflen, const char *fmt, ...)
{
    /* package the arguments as a va_list and invoke our va_list version */
    va_list args;
    va_start(args, fmt);
    size_t len = t3vsprintf(buf, buflen, fmt, args);
    va_end(args);

    /* return the length */
    return len;
}

/* check for 'th' suffix in a format code */
static const char *check_nth(const char *&fmt, int ival)
{
    /* presume no suffix */
    const char *nth = "";

    /* check for the 'th' suffix in the format code */
    if (fmt[1] == 't' && fmt[2] == 'h')
    {
        /* skip the extra format characters */
        fmt += 2;

        /* 'th' suffix applies to most numbers */
        nth = "th";

        /* 
         *   check for 1st, 2nd, 3rd, 21st, 22nd, 23rd, etc; but note that
         *   the the teens are all th's 
         */
        if (ival % 100 < 10 || ival % 100 > 20)
        {
            switch (ival % 10)
            {
            case 1:
                nth = "st";
                break;
                
            case 2:
                nth = "nd";
                break;
                
            case 3:
                nth = "rd";
                break;
            }
        }
    }

    /* return the suffix we figured */
    return nth;
}

/*
 *   buffer-checked vsprintf implementation 
 */
size_t t3vsprintf(char *buf, size_t buflen, const char *fmt, va_list args0)
{
    size_t rem;
    size_t need = 0;
    char *dst;

    /* 
     *   make a private copy of the arguments, to ensure that we don't modify
     *   the caller's copy (on some platforms, va_list is a reference type;
     *   the caller might want to reuse their argument pointer for a two-pass
     *   operation, such as a pre-format pass to measure how much space is
     *   needed) 
     */
    va_list args;
    os_va_copy(args, args0);

    /* scan the buffer */
    for (dst = buf, rem = buflen ; *fmt != '\0' ; ++fmt)
    {
        /* check for a format specifier */
        if (*fmt == '%')
        {
            const char *fmt_start = fmt;
            const char *nth = "";
            const char *txt;
            size_t txtlen;
            char buf[20];
            int fld_wid = -1;
            int fld_prec = -1;
            char lead_char = ' ';
            int plus = FALSE;
            int approx = FALSE;
            int add_ellipsis = FALSE;
            int left_align = FALSE;
            size_t i;

            /* skip the '%' */
            ++fmt;

            /* check for the "approximation" flag */
            if (*fmt == '~')
            {
                approx = TRUE;
                ++fmt;
            }

            /* check for an explicit sign */
            if (*fmt == '+')
            {
                plus = TRUE;
                ++fmt;
            }

            /* check for left alignment */
            if (*fmt == '-')
            {
                left_align = TRUE;
                ++fmt;
            }

            /* if leading zeros are desired, note it */
            if (*fmt == '0')
                lead_char = '0';

            /* check for a field width specifier */
            if (is_digit(*fmt))
            {
                /* scan the digits */
                for (fld_wid = 0 ; is_digit(*fmt) ; ++fmt)
                {
                    fld_wid *= 10;
                    fld_wid += value_of_digit(*fmt);
                }
            }
            else if (*fmt == '*')
            {
                /* the value is an integer taken from the arguments */
                fld_wid = va_arg(args, int);

                /* skip the '*' */
                ++fmt;
            }

            /* check for a precision specifier */
            if (*fmt == '.')
            {
                /* skip the '.' */
                ++fmt;

                /* check what we have */
                if (*fmt == '*')
                {
                    /* the value is an integer taken from the arguments */
                    fld_prec = va_arg(args, int);

                    /* skip the '*' */
                    ++fmt;
                }
                else
                {
                    /* scan the digits */
                    for (fld_prec = 0 ; is_digit(*fmt) ; ++fmt)
                    {
                        fld_prec *= 10;
                        fld_prec += value_of_digit(*fmt);
                    }
                }
            }
            
            /* check what follows */
            switch (*fmt)
            {
            case '%':
                /* it's a literal '%' */
                txt = "%";
                txtlen = 1;
                break;

            case 's':
                /* the value is a (char *) */
                txt = va_arg(args, char *);

                /* if it's null, show "(null)" */
                if (txt == 0)
                    txt = "(null)";

                /* get the length, limiting it to the precision */
                {
                    const char *p;

                    /* 
                     *   Scan until we reach a null terminator, or until our
                     *   length reaches the maximum given by the precision
                     *   qualifier.  
                     */
                    for (txtlen = 0, p = txt ;
                         *p != '\0' && (fld_prec == -1
                                        || txtlen < (size_t)fld_prec) ;
                         ++p, ++txtlen) ;
                }

                /* 
                 *   if the 'approximation' flag is specified, limit the
                 *   string to 60 characters or the field width, whichever
                 *   is less 
                 */
                if (approx)
                {
                    size_t lim;
                    
                    /* the default limit is 60 characters */
                    lim = 60;

                    /* 
                     *   if a field width is specified, it overrides the
                     *   default - but take out three characters for the
                     *   '...' suffix 
                     */
                    if (fld_wid != -1 && (size_t)fld_wid > lim)
                        lim = fld_wid - 3;

                    /* if we're over the limit, shorten the string */
                    if (txtlen > lim)
                    {
                        /* shorten the string to the limit */
                        txtlen = lim;

                        /* add an ellipsis to indicate the truncation */
                        add_ellipsis = TRUE;
                    }
                }

                /* 
                 *   if a field width was specified and we have the default
                 *   right alignment, pad to the left with spaces if the
                 *   width is greater than the actual length 
                 */
                if (fld_wid != -1 && !left_align)
                {
                    for ( ; (size_t)fld_wid > txtlen ; --fld_wid)
                    {
                        ++need;
                        if (rem > 1)
                            --rem, *dst++ = ' ';
                    }
                }
                break;

            case 'P':
                /* 
                 *   URL parameter - this is a (char*) value with url
                 *   encoding applied 
                 */
                txt = va_arg(args, char *);

                /* if it's null, use an empty string */
                if (txt == 0)
                    txt = "";

                /* use the field precision if given, or the string length */
                txtlen = (fld_prec >= 0 ? fld_prec : strlen(txt));

                /* encode the parameter and add it to the buffer */
                for ( ; txtlen != 0 ; --txtlen, ++txt)
                {
                    switch (*txt)
                    {
                    case '!':
                    case '*':
                    case '\'':
                    case '(':
                    case ')':
                    case ';':
                    case ':':
                    case '@':
                    case '&':
                    case '=':
                    case '+':
                    case '$':
                    case ',':
                    case '/':
                    case '?':
                    case '#':
                    case '[':
                    case ']':
                    case ' ':
                        /* use % encoding for special characters */
                        sprintf(buf, "%%%02x", (unsigned)(uchar)*txt);
                        need += 3;
                        for (i = 0 ; i < 3 ; ++i)
                        {
                            if (rem > 1)
                                *dst++ = buf[i], --rem;
                        }
                        break;

                    default:
                        /* copy anything else as-is */
                        need += 1;
                        if (rem > 1)
                            *dst++ = *txt, --rem;
                        break;
                    }
                }
                break;

            case 'c':
                /* the value is a (char) */
                buf[0] = (char)va_arg(args, int);
                txt = buf;
                txtlen = 1;
                break;

            case 'd':
                /* the value is an int, formatted in decimal */
                {
                    int ival = va_arg(args, int);
                    sprintf(buf, "%d", ival);

                    /* check for '%dth' notation (1st, 2nd, etc) */
                    nth = check_nth(fmt, ival);
                }
                /* fall through to num_common: */
                
            num_common:
                /* use the temporary buffer where we formatted the value */
                txt = buf;
                txtlen = strlen(buf);

                /* 
                 *   Pad with leading spaces or zeros if the requested
                 *   field width exceeds the actual size and we have the
                 *   default left alignment.  
                 */
                if (fld_wid != -1 && !left_align && (size_t)fld_wid > txtlen)
                {
                    /* 
                     *   if we're showing an explicit sign, and we don't have
                     *   a leading '-' in the results, add it 
                     */
                    if (plus && txt[0] != '-')
                    {
                        /* add the '+' at the start of the output */
                        ++need;
                        if (rem > 1)
                            --rem, *dst++ = '+';
                    }

                    /* 
                     *   if we're showing leading zeros, and we have a
                     *   negative number, show the '-' first 
                     */
                    if (lead_char == '0' && txt[0] == '-')
                    {
                        /* add the '-' at the start of the output */
                        ++need;
                        if (rem > 1)
                            --rem, *dst++ = '-';
                        
                        /* we've shown the '-', so skip it in the number */
                        ++txt;
                        --txtlen;
                    }

                    /* add the padding */
                    for ( ; (size_t)fld_wid > txtlen ; --fld_wid)
                    {
                        ++need;
                        if (rem > 1)
                            --rem, *dst++ = lead_char;
                    }
                }

                /* done */
                break;

            case 'u':
                /* the value is an int, formatted as unsigned decimal */
                sprintf(buf, "%u", va_arg(args, int));
                goto num_common;

            case 'x':
                /* the value is an int, formatted in hex */
                sprintf(buf, "%x", va_arg(args, int));
                goto num_common;

            case 'o':
                /* the value is an int, formatted in hex */
                sprintf(buf, "%o", va_arg(args, int));
                goto num_common;

            case 'l':
                /* it's a 'long' value - get the second specifier */
                switch (*++fmt)
                {
                case 'd':
                    /* it's a long, formatted in decimal */
                    sprintf(buf, "%ld", va_arg(args, long));
                    goto num_common;

                case 'u':
                    /* it's a long, formatted as unsigned decimal */
                    sprintf(buf, "%lu", va_arg(args, long));
                    goto num_common;

                case 'x':
                    /* it's a long, formatted in hex */
                    sprintf(buf, "%lx", va_arg(args, long));
                    goto num_common;

                case 'o':
                    /* it's a long, formatted in octal */
                    sprintf(buf, "%lo", va_arg(args, long));
                    goto num_common;

                default:
                    /* bad specifier - show the literal text */
                    txt = "%";
                    txtlen = 1;
                    fmt = fmt_start;
                    break;
                }
                break;

            default:
                /* bad specifier - show the literal text */
                txt = "%";
                txtlen = 1;
                fmt = fmt_start;
                break;
            }

            /* add the text to the buffer */
            for (i = txtlen ; i != 0 ; --i, ++txt)
            {
                ++need;
                if (rem > 1)
                    --rem, *dst++ = *txt;
            }

            /* add the 'nth' suffix, if applicable */
            for ( ; *nth != 0 ; ++nth)
            {
                ++need;
                if (rem > 1)
                    --rem, *dst++ = *nth;
            }

            /* add an ellipsis if desired */
            if (add_ellipsis)
            {
                /* add three '.' characters */
                for (i = 3 ; i != 0 ; --i)
                {
                    ++need;
                    if (rem > 1)
                        --rem, *dst++ = '.';
                }

                /* for padding purposes, count the ellipsis */
                txtlen += 3;
            }

            /* 
             *   if we have left alignment and the actual length is less
             *   than the field width, pad to the right with spaces 
             */
            if (left_align && fld_wid != -1 && (size_t)fld_wid > txtlen)
            {
                /* add spaces to pad out to the field width */
                for (i = fld_wid ; i > txtlen ; --i)
                {
                    ++need;
                    if (rem > 1)
                        --rem, *dst++ = ' ';
                }
            }
        }
        else
        {
            /* it's not a format specifier - just copy it literally */
            ++need;
            if (rem > 1)
                --rem, *dst++ = *fmt;
        }
    }

    /* add the trailing null */
    if (rem != 0)
        *dst = '\0';

    /* bracket the va_copy at the top of the function */
    os_va_copy_end(args);

    /* return the needed length */
    return need;
}


/* ------------------------------------------------------------------------ */
/*
 *   Convert string to lower case 
 */
void t3strlwr(char *p)
{
    for ( ; *p != '\0' ; ++p)
        *p = (char)to_lower(*p);
}

/* ------------------------------------------------------------------------ */
/*
 *   Debugging routines for memory management 
 */

#ifdef T3_DEBUG

#include <stdio.h>
#include <stdlib.h>

#define T3_DEBUG_MEMGUARD
#ifdef T3_DEBUG_MEMGUARD
#define MEM_GUARD_PREFIX  char guard[sizeof(mem_prefix_t *)];
#define MEM_GUARD_POST_BYTES   sizeof(mem_prefix_t *)
#else /* T3_DEBUG_MEMGUARD */
#define MEM_GUARD_PREFIX
#define MEM_GUARD_BYTES   0
#define mem_check_guard(blk)
#define mem_set_guard(blk)
#endif /* T3_DEBUG_MEMGUARD */


/*
 *   memory block prefix - each block we allocate has this prefix attached
 *   just before the pointer that we return to the program 
 */
struct mem_prefix_t
{
    long id;
    size_t siz;
    mem_prefix_t *nxt;
    mem_prefix_t *prv;
    int alloc_type;
    OS_MEM_PREFIX
    MEM_GUARD_PREFIX
};

#ifdef T3_DEBUG_MEMGUARD
static void mem_make_guard(char *b, const mem_prefix_t *blk)
{
    memcpy(b, &blk, sizeof(blk));
    const static unsigned char x[] =
        { 0xcf, 0xfd, 0xef, 0xfb, 0xbf, 0xfc, 0xdf, 0xfe  };
    for (size_t i = 0 ; i < sizeof(blk) ; ++i)
        b[i] ^= x[i % countof(x)];
}
static void mem_set_guard(mem_prefix_t *blk)
{
    char b[sizeof(mem_prefix_t *)];
    mem_make_guard(b, blk);
    memcpy(blk->guard, b, sizeof(b));
    memcpy((char *)(blk + 1) + blk->siz, b, sizeof(b));
}
static void mem_check_guard(const mem_prefix_t *blk)
{
    char b[sizeof(mem_prefix_t *)];
    mem_make_guard(b, blk);
    if (memcmp(blk->guard, b, sizeof(b)) != 0)
        fprintf(stderr, "pre guard bytes corrupted: addr=%lx, id=%ld, siz=%lu "
                OS_MEM_PREFIX_FMT "\n",
                (long)(blk + 1), blk->id, (unsigned long)blk->siz
                OS_MEM_PREFIX_FMT_VARS(blk));
    if (memcmp((char *)(blk + 1) + blk->siz, b, sizeof(b)) != 0)
        fprintf(stderr, "post guard bytes corrupted: addr=%lx, id=%ld, siz=%lu "
                OS_MEM_PREFIX_FMT "\n",
                (long)(blk + 1), blk->id, (unsigned long)blk->siz
                OS_MEM_PREFIX_FMT_VARS(blk));
}
#endif /* T3_DEBUG_MEMGUARD */

/* head and tail of memory allocation linked list */
static mem_prefix_t *mem_head = 0;
static mem_prefix_t *mem_tail = 0;

/* mutex for protecting the memory tracker list */
static OS_Mutex mem_mutex;

/*
 *   Check the integrity of the heap: traverse the entire list, and make
 *   sure the forward and backward pointers match up.  
 */
static void t3_check_heap()
{
    mem_prefix_t *p;

    /* lock the memory mutex while accessing the memory block list */
    mem_mutex.lock();

    /* scan from the front */
    for (p = mem_head ; p != 0 ; p = p->nxt)
    {
        /* 
         *   If there's a backwards pointer, make sure it matches up.  If
         *   there's no backwards pointer, make sure we're at the head of
         *   the list.  If this is the end of the list, make sure it
         *   matches the tail pointer.  
         */
        if ((p->prv != 0 && p->prv->nxt != p)
            || (p->prv == 0 && p != mem_head)
            || (p->nxt == 0 && p != mem_tail))
            fprintf(stderr, "\n--- heap corrupted ---\n");
    }

    /* done with the list */
    mem_mutex.unlock();
}

/*
 *   Allocate a block, storing it in a doubly-linked list of blocks and
 *   giving the block a unique ID.  
 */
void *t3malloc(size_t siz, int alloc_type)
{
    static long id;
    static int check = 0;
    mem_prefix_t *mem;

    /* make sure the size doesn't overflow when we add the prefix */
    if (siz + sizeof(mem_prefix_t) < siz)
        return 0;

    /* allocate the memory, including its prefix */
    mem = (mem_prefix_t *)malloc(siz + sizeof(mem_prefix_t)
                                 + MEM_GUARD_POST_BYTES);

    /* if that failed, return failure */
    if (mem == 0)
        return 0;

    /* set the guard bytes */
    mem->alloc_type = alloc_type;
    mem->siz = siz;
    mem_set_guard(mem);

    /* lock the memory mutex while accessing the memory block list */
    mem_mutex.lock();

    /* set up the prefix */
    mem->id = id++;
    mem->prv = mem_tail;
    os_mem_prefix_set(mem);
    mem->nxt = 0;
    if (mem_tail != 0)
        mem_tail->nxt = mem;
    else
        mem_head = mem;
    mem_tail = mem;

    /* done with the list */
    mem_mutex.unlock();

    /* check the heap for corruption if desired */
    if (check)
        t3_check_heap();

    /* return the caller's block, which immediately follows the prefix */
    return (void *)(mem + 1);
}

/*
 *   reallocate a block - to simplify, we'll allocate a new block, copy
 *   the old block up to the smaller of the two block sizes, and delete
 *   the old block 
 */
void *t3realloc(void *oldptr, size_t newsiz)
{
    void *newptr;
    size_t oldsiz;

    /* allocate a new block */
    newptr = t3malloc(newsiz, T3MALLOC_TYPE_MALLOC);

    /* copy the old block into the new block */
    oldsiz = (((mem_prefix_t *)oldptr) - 1)->siz;
    memcpy(newptr, oldptr, (oldsiz <= newsiz ? oldsiz : newsiz));

    /* free the old block */
    t3free(oldptr);

    /* return the new block */
    return newptr;
}


/* free a block, removing it from the allocation block list */
void t3free(void *ptr, int alloc_type)
{
    /* statics for debugging */
    static int check = 0;
    static int double_check = 0;
    static int check_heap = 0;
    static long ckblk[] = { 0xD9D9D9D9, 0xD9D9D9D9, 0xD9D9D9D9 };

    /* ignore freeing null */
    if (ptr == 0)
        return;

    /* check the integrity of the entire heap if desired */
    if (check_heap)
        t3_check_heap();

    /* get the prefix */
    mem_prefix_t *mem = ((mem_prefix_t *)ptr) - 1;
    size_t siz;

    /* 
     *   check that the call type matches the allocating call type (malloc,
     *   new, or new[]) 
     */
    if (mem->alloc_type != alloc_type)
        fprintf(stderr, "\n--- memory block freed with wrong call type: "
                "block=%lx, size=%lu, id=%lu, alloc type=%d, free type=%d "
                "---\n",
                (unsigned long)ptr, (unsigned long)mem->siz, mem->id,
                mem->alloc_type, alloc_type);

    /* check for a pre-freed block */
    if (memcmp(mem, ckblk, sizeof(ckblk)) == 0)
    {
        fprintf(stderr, "\n--- memory block freed twice: %lx ---\n",
                (unsigned long)ptr);
        return;
    }

    /* check the guard bytes for overwrites */
    mem_check_guard(mem);

    /* lock the memory mutex while accessing the memory block list */
    mem_mutex.lock();

    /* if desired, check to make sure the block is in our list */
    if (check)
    {
        mem_prefix_t *p;
        for (p = mem_head ; p != 0 ; p = p->nxt)
        {
            if (p == mem)
                break;
        }
        if (p == 0)
            fprintf(stderr,
                    "\n--- memory block not found in t3free: %lx ---\n",
                    (unsigned long)ptr);
    }

    /* unlink the block from the list */
    if (mem->prv != 0)
        mem->prv->nxt = mem->nxt;
    else
        mem_head = mem->nxt;

    if (mem->nxt != 0)
        mem->nxt->prv = mem->prv;
    else
        mem_tail = mem->prv;

    /* 
     *   if we're being really cautious, check to make sure the block is
     *   no longer in the list 
     */
    if (double_check)
    {
        mem_prefix_t *p;
        for (p = mem_head ; p != 0 ; p = p->nxt)
        {
            if (p == mem)
                break;
        }
        if (p != 0)
            fprintf(stderr, "\n--- memory block still in list after "
                    "t3free ---\n");
    }

    /* done with the list */
    mem_mutex.unlock();

    /* make it obvious that the memory is invalid */
    siz = mem->siz;
    memset(mem, 0xD9, siz + sizeof(mem_prefix_t));

    /* free the memory with the system allocator */
    free((void *)mem);
}

/*
 *   Default display lister callback 
 */
static void fprintf_stderr(const char *msg)
{
    fprintf(stderr, "%s", msg);
}
    
/*
 *   Diagnostic routine to display the current state of the heap.  This
 *   can be called just before program exit to display any memory blocks
 *   that haven't been deleted yet; any block that is still in use just
 *   before program exit is a leaked block, so this function can be useful
 *   to help identify and remove memory leaks.  
 */
void t3_list_memory_blocks(void (*cb)(const char *))
{
    mem_prefix_t *mem;
    int cnt;
    char buf[128];

    /* if there's no callback, use our own standard display lister */
    if (cb == 0)
        cb = fprintf_stderr;

    /* display introductory message */
    (*cb)("\n(T3VM) Memory blocks still in use:\n");

    /* lock the memory mutex while accessing the memory block list */
    mem_mutex.lock();

    /* display the list of undeleted memory blocks */
    for (mem = mem_head, cnt = 0 ; mem ; mem = mem->nxt, ++cnt)
    {
        sprintf(buf, "  addr=%lx, id=%ld, siz=%lu" OS_MEM_PREFIX_FMT "\n",
                (long)(mem + 1), mem->id, (unsigned long)mem->siz
                OS_MEM_PREFIX_FMT_VARS(mem));
        (*cb)(buf);
    }

    /* done with the mutex */
    mem_mutex.unlock();

    /* display totals */
    sprintf(buf, "\nTotal blocks in use: %d\n", cnt);
    (*cb)(buf);
}


#ifdef T_WIN32
/*
 *   Windows-specific additions to the memory header.  We'll track the first
 *   couple of return addresses from the stack, to make it easier to track
 *   down where the allocation request came from.  
 */

void os_mem_prefix_set(mem_prefix_t *mem)
{
    /* 
     *   Trace back the call stack.  In the standard Intel stack arrangement,
     *   BP is the base pointer for the frame, and points to the enclosing
     *   frame pointer.  Just above BP is the return address.  We're not
     *   interested in our own return address, so skip the first frame.  
     */
    DWORD bp_;
    __asm mov bp_, ebp;
    bp_ = IsBadReadPtr((DWORD *)bp_, sizeof(bp_)) ? 0 : *(DWORD *)bp_;

    for (size_t i = 0 ;
         i < countof(mem->stk) && !IsBadReadPtr((DWORD *)bp_, sizeof(bp_))
             && bp_ < *(DWORD *)bp_ ;
         ++i, bp_ = *(DWORD *)bp_)
        mem->stk[i].return_addr = ((DWORD *)bp_)[1];
}

#endif /* T_WIN32 */


#endif /* T3_DEBUG */
