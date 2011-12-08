/* $Header$ */

/* Copyright (c) 2010 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  vmstrref.h - reference-counted string class
Function
  
Notes
  
Modified
  08/11/10 MJRoberts  - Creation
*/

#ifndef VMSTRREF_H
#define VMSTRREF_H

#include "osifcnet.h"


class StringRef: public CVmRefCntObj
{
public:
    /* create by copying a literal string */
    StringRef() { init(0, 0, 256); }
    StringRef(long init_alo) { init(0, 0, init_alo); }
    StringRef(const char *s)
    {
        size_t l = strlen(s);
        init(s, l, l);
    }
    StringRef(const char *s, long l)
        { init(s, l, l); }
    StringRef(const char *s, long l, long init_alo)
        { init(s, l, init_alo); }

    /* get the string's contents and length */
    char *get() const { return str; }
    char *getend() const { return str + len; }
    long getlen() const { return len; }

    void setlen(long len) { this->len = len; }
    void addlen(long len) { this->len += len; }

    /* create from a sprintf-style format */
    static StringRef *format(const char *fmt, ...)
    {
        /* create an empty StringRef to hold the new string */
        StringRef *s = new StringRef();

        /* format the string into an allocated buffer */
        va_list argp;
        va_start(argp, fmt);
        s->str = t3sprintf_alloc(fmt, argp);
        va_end(argp);

        /* note the length */
        s->alo = s->len = strlen(s->str);

        /* return the new string */
        return s;
    }

    /* append */
    void append(const char *s) { append(s, strlen(s)); }
    void append(const char *s, long l)
    {
        /* if necessary, reallocate the buffer */
        if (len + l > alo)
        {
            /* 
             *   calculate the expansion size: go to the needed length plus
             *   half again that total for headroom, with minimum headroom of
             *   256 bytes 
             */
            alo = len + l + (len+l > 512 ? (len+l)/2 : 256);

            /* allocate the new buffer */
            str = (char *)t3realloc(str, alo + 1);
        }

        /* copy the new text */
        memcpy(str + len, s, l);

        /* null-terminate it */
        str[len + l] = '\0';

        /* note the new length */
        len += l;
    }

    /* append formatted */
    void appendf(const char *fmt, ...)
    {
        /* do the formatting */
        va_list argp;
        va_start(argp, fmt);
        char *s = t3sprintf_alloc(fmt, argp);
        va_end(argp);

        /* append the result to the string */
        append(s);

        /* we're done with the formatted result */
        t3free(s);
    }

    /* append a utf-8 character */
    void append_utf8(wchar_t ch)
    {
        char buf[3];

        if (ch <= 0x7f)
        {
            /* it's in the range 0..7f - encode as one byte */
            buf[0] = (char)ch;
            append(buf, 1);
        }
        else if (ch <= 0x7ff)
        {
            /* it's in the range 80..7ff - encode as two bytes */
            buf[0] = (char)(0xC0 | ((ch >> 6) & 0x1F));
            buf[1] = (char)(0x80 | (ch & 0x3F));
            append(buf, 2);
        }
        else
        {
            /* it's in the range 800..ffff - encode as three bytes */
            buf[0] = (char)(0xE0 | ((ch >> 12) & 0x0F));
            buf[1] = (char)(0x80 | ((ch >> 6) & 0x3F));
            buf[2] = (char)(0x80 | (ch & 0x3F));
            append(buf, 3);
        }
    }

    /* 
     *   ensure there's space for 'n' added bytes 
     */
    void ensure(long n)
    {
        if (n + len > alo)
        {
            /* 
             *   calculate the expansion size: go to the needed length plus
             *   half again that total for headroom, with minimum headroom of
             *   256 bytes 
             */
            alo = len + n + (len+n > 512 ? (len+n)/2 : 256);

            /* allocate the new buffer */
            str = (char *)t3realloc(str, alo + 1);
        }
    }

    /* truncate to the given size */
    void truncate(long l)
    {
        /* if we're actually shrinking the buffer, make the change */
        if (l < len)
        {
            /* set the new length, and null-terminate at the new length */
            len = l;
            str[l] = '\0';
        }
    }

protected:
    ~StringRef()
    {
        if (str != 0)
            t3free(str);
    }

    void init(const char *s, size_t l, size_t init_alo)
    {
        /* make sure the initial allocation is at least 'l' */
        if (init_alo < l)
            init_alo = l;

        /* allocate the buffer */
        alo = init_alo;
        str = (char *)t3malloc(alo + 1);

        /* copy the string */
        if (str != 0)
            memcpy(str, s, l);

        /* null-terminate it */
        str[l] = '\0';

        /* set the length */
        len = l;
    }

    /* the string */
    char *str;

    /* stored length of the string */
    long len;

    /* size of the allocated buffer */
    long alo;
};


#endif /* VMSTRREF_H */
