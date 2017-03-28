/* $Header: d:/cvsroot/tads/tads3/TCUNAS.H,v 1.3 1999/07/11 00:46:58 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tcunas.h - TADS 3 Compiler Unassembler
Function
  
Notes
  
Modified
  05/10/99 MJRoberts  - Creation
*/

#ifndef TCUNAS_H
#define TCUNAS_H

#include <stdio.h>
#include <stdarg.h>

#include "t3std.h"
#include "tcgen.h"


/* ------------------------------------------------------------------------ */
/*
 *   byte-code source for unassembler 
 */
class CTcUnasSrc
{
public:
    virtual ~CTcUnasSrc() { }
    
    /* 
     *   read the next byte; returns zero on success, non-zero at the end
     *   of the byte stream 
     */
    virtual int next_byte(char *ch) = 0;

    /* get the current offset */
    virtual ulong get_ofs() const = 0;
};

/*
 *   code stream implementation of byte code source
 */
class CTcUnasSrcCodeStr: public CTcUnasSrc
{
public:
    CTcUnasSrcCodeStr(CTcCodeStream *str)
    {
        /* remember our underlying code stream */
        str_ = str;

        /* start at the first byte of the code stream */
        ofs_ = 0;
    }

    /* read from the code stream */
    int next_byte(char *ch)
    {
        /* if there's anything left, return the byte and bump the pointer */
        if (ofs_ < str_->get_ofs())
        {
            *ch = str_->get_byte_at(ofs_);
            ++ofs_;
            return 0;
        }

        /* indicate end of file */
        return 1;
    }

    /* get the current offset */
    ulong get_ofs() const
    {
        return ofs_;
    }

protected:
    /* underlying code stream object */
    CTcCodeStream *str_;

    /* current read offset in code stream */
    ulong ofs_;
};

/* ------------------------------------------------------------------------ */
/*
 *   output stream for unassembler 
 */
class CTcUnasOut
{
public:
    virtual ~CTcUnasOut() { }

    /* write a line of text to the output, printf-style */
    virtual void print(const char *fmt, ...) = 0;
};

/*
 *   stdio implementation of output stream - writes data to standard
 *   output 
 */
class CTcUnasOutStdio: public CTcUnasOut
{
public:
    void print(const char *fmt, ...)
    {
        va_list va;

        /* display the data on the standard output */
        va_start(va, fmt);
        vprintf(fmt, va);
        va_end(va);
    }
};

/*
 *   Text file (osfildef) implementation of output stream.  The file handle
 *   is managed by the caller.  
 */
class CTcUnasOutFile: public CTcUnasOut
{
public:
    CTcUnasOutFile(osfildef *fp) { fp_ = fp; }

    void print(const char *fmt, ...)
    {
        char buf[1024];
        va_list va;

        /* format the text */
        va_start(va, fmt);
        t3vsprintf(buf, sizeof(buf), fmt, va);
        va_end(va);

        /* write the formatted text to the file */
        os_fprintz(fp_, buf);
    }

protected:
    /* our file handle */
    osfildef *fp_;
};

#endif /* TCUNAS_H */

