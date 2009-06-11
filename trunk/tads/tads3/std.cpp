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
    char *buf;

    /* if the source string is null, just return null as the result */
    if (str == 0)
        return 0;

    /* allocate space */
    buf = lib_alloc_str(len);

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
 *   buffer-checked sprintf implementation
 */
void t3sprintf(char *buf, size_t buflen, const char *fmt, ...)
{
    va_list args;

    /* package the arguments as a va_list and invoke our va_list version */
    va_start(args, fmt);
    t3vsprintf(buf, buflen, fmt, args);
    va_end(args);
}

/*
 *   buffer-checked vsprintf implementation 
 */
void t3vsprintf(char *buf, size_t buflen, const char *fmt, va_list args)
{
    size_t rem;
    char *dst;

    /* if there's no room at all in the buffer, give up immediately */
    if (buflen == 0)
        return;
    
    /* scan the buffer */
    for (dst = buf, rem = buflen - 1 ; *fmt != '\0' && rem != 0 ; ++fmt)
    {
        /* check for a format specifier */
        if (*fmt == '%')
        {
            const char *fmt_start = fmt;
            const char *txt;
            size_t txtlen;
            char buf[20];
            int fld_wid = -1;
            int fld_prec = -1;
            char lead_char = ' ';
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

            /* check for left alignment */
            if (*fmt == '-')
            {
                left_align = TRUE;
                ++fmt;
            }

            /* if leading zeroes are desired, note it */
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
                    for ( ; (size_t)fld_wid > txtlen && rem != 0 ;
                          --fld_wid, --rem)
                        *dst++ = ' ';
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
                sprintf(buf, "%d", va_arg(args, int));
                
            num_common:
                /* use the temporary buffer where we formatted the value */
                txt = buf;
                txtlen = strlen(buf);

                /* 
                 *   Pad with leading spaces or zeroes if the requested
                 *   field width exceeds the actual size and we have the
                 *   default left alignment.  
                 */
                if (fld_wid != -1 && !left_align && (size_t)fld_wid > txtlen)
                {
                    /* 
                     *   if we're showing leading zeroes, and we have a
                     *   negative number, show the '-' first 
                     */
                    if (lead_char == '0' && txt[0] == '-' && rem != 0)
                    {
                        /* add the '-' at the start of the output */
                        *dst++ = '-';
                        --rem;

                        /* we've shown the '-', so skip it in the number */
                        ++txt;
                        --txtlen;
                    }

                    /* add the padding */
                    for ( ; (size_t)fld_wid > txtlen && rem != 0 ;
                          --fld_wid, --rem)
                        *dst++ = lead_char;
                }
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
            for (i = txtlen ; i != 0 && rem != 0 ; --i, --rem)
                *dst++ = *txt++;

            /* add an ellipsis if desired */
            if (add_ellipsis)
            {
                /* add three '.' characters */
                for (i = 3 ; i != 0 && rem != 0 ; --i, --rem)
                    *dst++ = '.';

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
                for (i = fld_wid ; i > txtlen && rem != 0 ; --i, --rem)
                    *dst++ = ' ';
            }
        }
        else
        {
            /* it's not a format specifier - just copy it literally */
            *dst++ = *fmt;
            --rem;
        }
    }

    /* add the trailing null */
    *dst = '\0';
}


/* ------------------------------------------------------------------------ */
/*
 *   Debugging routines for memory management 
 */

#ifdef T3_DEBUG

#include <stdio.h>
#include <stdlib.h>

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
};

/* head and tail of memory allocation linked list */
static mem_prefix_t *mem_head = 0;
static mem_prefix_t *mem_tail = 0;

/*
 *   Check the integrity of the heap: traverse the entire list, and make
 *   sure the forward and backward pointers match up.  
 */
static void t3_check_heap()
{
    mem_prefix_t *p;

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
}

/*
 *   Allocate a block, storing it in a doubly-linked list of blocks and
 *   giving the block a unique ID.  
 */
void *t3malloc(size_t siz)
{
    static long id;
    static int check = 0;
    mem_prefix_t *mem;

    /* allocate the memory, including its prefix */
    mem = (mem_prefix_t *)malloc(siz + sizeof(mem_prefix_t));

    /* if that failed, return failure */
    if (mem == 0)
        return 0;

    /* set up the prefix */
    mem->id = id++;
    mem->siz = siz;
    mem->prv = mem_tail;
    mem->nxt = 0;
    if (mem_tail != 0)
        mem_tail->nxt = mem;
    else
        mem_head = mem;
    mem_tail = mem;

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
    newptr = t3malloc(newsiz);

    /* copy the old block into the new block */
    oldsiz = (((mem_prefix_t *)oldptr) - 1)->siz;
    memcpy(newptr, oldptr, (oldsiz <= newsiz ? oldsiz : newsiz));

    /* free the old block */
    t3free(oldptr);

    /* return the new block */
    return newptr;
}


/* free a block, removing it from the allocation block list */
void t3free(void *ptr)
{
    static int check = 0;
    static int double_check = 0;
    static int check_heap = 0;
    mem_prefix_t *mem = ((mem_prefix_t *)ptr) - 1;
    static long ckblk[] = { 0xD9D9D9D9, 0xD9D9D9D9, 0xD9D9D9D9 };
    size_t siz;

    /* check the integrity of the entire heap if desired */
    if (check_heap)
        t3_check_heap();

    /* check for a pre-freed block */
    if (memcmp(mem, ckblk, sizeof(ckblk)) == 0)
    {
        fprintf(stderr, "\n--- memory block freed twice ---\n");
        return;
    }

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
            fprintf(stderr, "\n--- memory block not found in t3free ---\n");
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

    /* display the list of undeleted memory blocks */
    for (mem = mem_head, cnt = 0 ; mem ; mem = mem->nxt, ++cnt)
    {
        sprintf(buf, "  id = %ld, siz = %d\n", mem->id, mem->siz);
        (*cb)(buf);
    }

    /* display totals */
    sprintf(buf, "\nTotal blocks in use: %d\n", cnt);
    (*cb)(buf);
}

#endif /* T3_DEBUG */
