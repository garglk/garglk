#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/tads3/TCSRC.CPP,v 1.3 1999/07/11 00:46:55 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tcsrc.cpp - source file reader
Function
  
Notes
  
Modified
  04/13/99 MJRoberts  - Creation
*/

#include <string.h>
#include <stdlib.h>

#include "os.h"
#include "t3std.h"
#include "tcsrc.h"
#include "tcglob.h"
#include "charmap.h"
#include "vmdatasrc.h"


/* ------------------------------------------------------------------------ */
/*
 *   Source files 
 */

/*
 *   Construction
 */
CTcSrcFile::CTcSrcFile(osfildef *fp, class CCharmapToUni *mapper)
{
    /* remember my source file */
    fp_ = new CVmFileSource(fp);

    /* net yet at end of file */
    at_eof_ = FALSE;
    
    /* there's no data in the buffer yet */
    rem_ = 0;
    
    /* remember my character mapper */
    mapper_ = mapper;
}

/*
 *   Deletion 
 */
CTcSrcFile::~CTcSrcFile()
{
    /* close my source file */
    if (fp_ != 0)
        delete fp_;

    /* release my character mapper */
    if (mapper_ != 0)
        mapper_->release_ref();
}


#if 0
// we don't currently need this, but keep the source in case it
// becomes interesting later
//
/* ------------------------------------------------------------------------ */
/*
 *   Open a plain ASCII file, with no #charset marker. 
 */
CTcSrcFile *CTcSrcFile::open_plain(const char *filename)
{
    osfildef *fp;
    char buf[5];
    size_t siz;

    /* 
     *   open the file in binary mode, since we do all of the newline
     *   interpretation explicitly 
     */
    if ((fp = osfoprb(filename, OSFTTEXT)) == 0)
        return 0;

    /* read the first few bytes of the file */
    siz = osfrbc(fp, buf, sizeof(buf));

    /* check for a 3-byte UTF-8 marker */
    if (siz >= 3
        && (uchar)buf[0] == 0xEF
        && (uchar)buf[1] == 0xBB
        && (uchar)buf[2] == 0xBF)
    {
        /* 
         *   seek to the byte after the marker, so that our caller won't see
         *   the marker 
         */
        osfseek(fp, 3, OSFSK_SET);

        /* return a source file reader with a utf-8 mapper */
        return new CTcSrcFile(fp, new CCharmapToUniUTF8());
    }

    /* if we read at least two bytes, try auto-detecting UCS-2 */
    if (siz >= 2)
    {
        /* if the first bytes are 0xFF 0xFE, it's UCS-2 low-byte first */
        if ((unsigned char)buf[0] == 0xFF && (unsigned char)buf[1] == 0xFE)
        {
            /* seek to the byte after the marker */
            osfseek(fp, 2, OSFSK_SET);

            /* return a reader with a little-endian mapper */
            return new CTcSrcFile(fp, new CCharmapToUniUcs2Little());
        }

        /* if the first bytes are 0xFE 0xFF, it's UCS-2 high-byte first */
        if ((unsigned char)buf[0] == 0xFE && (unsigned char)buf[1] == 0xFF)
        {
            /* seek to the byte after the marker */
            osfseek(fp, 2, OSFSK_SET);

            /* return a reader with a little-endian mapper */
            return new CTcSrcFile(fp, new CCharmapToUniUcs2Big());
        }
    }

    /* 
     *   there are no Unicode markers, so our only remaining option is plain
     *   ASCII - return a source file object with a plain ASCII mapper 
     */
    return new CTcSrcFile(fp, new CCharmapToUniASCII());
}
#endif

/* ------------------------------------------------------------------------ */
/*
 *   Open a plain ASCII source file.  
 */
CTcSrcFile *CTcSrcFile::open_ascii(const char *filename)
{
    osfildef *fp;

    /* 
     *   open the file in binary mode, since we do all of the newline
     *   interpretation explicitly 
     */
    if ((fp = osfoprb(filename, OSFTTEXT)) == 0)
        return 0;

    /* return a source reader with a plain ASCII mapper */
    return new CTcSrcFile(fp, new CCharmapToUniASCII());
}


/* ------------------------------------------------------------------------ */
/*
 *   Open a source file 
 */
CTcSrcFile *CTcSrcFile::open_source(const char *filename,
                                    class CResLoader *res_loader,
                                    const char *default_charset,
                                    int *charset_error,
                                    int *default_charset_error)
{
    char buf[275];
    size_t siz;
    osfildef *fp;
    long startofs;
    CCharmapToUni *mapper;

    /* presume we won't find an invalid #charset directive */
    *charset_error = FALSE;

    /* presume we'll have no problem with the default character set */
    *default_charset_error = FALSE;

    /* 
     *   open the file in binary mode, so that we can scan the first few
     *   bytes to see if we can detect the character set from information
     *   at the beginning of the file 
     */
    fp = osfoprb(filename, OSFTTEXT);

    /* if we couldn't open the file, return failure */
    if (fp == 0)
        return 0;

    /* note the starting offset in the file */
    startofs = osfpos(fp);

    /* read the first few bytes of the file */
    siz = osfrbc(fp, buf, sizeof(buf));

    /* check for a 3-byte UTF-8 byte-order marker */
    if (siz >= 3  && (uchar)buf[0] == 0xEF && (uchar)buf[1] == 0xBB
        && (uchar)buf[2] == 0xBF)
    {
        char *p;
        size_t rem;
        uint skip;

        /* skip at least the three-byte marker sequence */
        skip = 3;
        
        /* 
         *   check for a #charset marker for utf-8 - this would be redundant,
         *   but we'll allow it 
         */
        p = buf + 3;
        rem = siz - 3;
        if (rem > 9 && memcmp(p, "#charset ", 9) == 0)
        {
            /* skip spaces */
            for (p += 9, rem -= 9 ; rem != 0 && (*p == ' ' || *p == '\t') ;
                 ++p, --rem);

            /* check for valid character set markers */
            if (rem >= 7 && memicmp(p, "\"utf-8\"", 7) == 0)
            {
                /* skip the whole sequence */
                skip = (p + 7) - buf;
            }
            else if (rem >= 6 && memicmp(p, "\"utf8\"", 6) == 0)
            {
                /* skip the whole sequence */
                skip = (p + 6) - buf;
            }
        }

        /* seek past the character set markers */
        osfseek(fp, startofs + skip, OSFSK_SET);

        /* return a new utf-8 decoder */
        return new CTcSrcFile(fp, new CCharmapToUniUTF8());
    }

    /* if we read at least two bytes, try auto-detecting unicode */
    if (siz >= 2)
    {
        CTcSrcFile *srcf;
        const char *const *cs_names;
        int bige;

        /* presume we won't find a byte-order marker */
        srcf = 0;
        
        /* if the first bytes are 0xFF 0xFE, it's UCS-2 low-byte first */
        if ((unsigned char)buf[0] == 0xFF && (unsigned char)buf[1] == 0xFE)
        {
            static const char *names[] = { "unicodel", "utf-16le", 0 };

            /* create a UCS-2 little-endian reader */
            srcf = new CTcSrcFile(fp, new CCharmapToUniUcs2Little());
            bige = FALSE;
            cs_names = names;
        }

        /* if the first bytes are 0xFE 0xFF, it's UCS-2 high-byte first */
        if ((unsigned char)buf[0] == 0xFE && (unsigned char)buf[1] == 0xFF)
        {
            static const char *names[] = { "unicodeb", "utf-16be", 0 };

            /* create a UCS-2 little-endian reader */
            srcf = new CTcSrcFile(fp, new CCharmapToUniUcs2Big());
            bige = TRUE;
            cs_names = names;
        }

        /* if we found the byte-order marker, we know the character set */
        if (srcf != 0)
        {
            uint skip;

            /* we at least want to skip the byte-order marker */
            skip = 2;
            
            /* check to see if we have a '#charset' directive */
            if (ucs_str_starts_with(buf + 2, siz - 2, "#charset ",
                                    bige, FALSE))
            {
                char *p;
                size_t rem;
                
                /* scan past following spaces */
                for (p = buf + 2 + 18, rem = siz - 2 - 18 ;
                     rem >= 2 && (ucs_char_eq(p, ' ', bige, FALSE)
                                  || ucs_char_eq(p, '\t', bige, FALSE)) ;
                     p += 2, rem -= 2) ;

                /* check for a '"' */
                if (rem >= 2 && ucs_char_eq(p, '"', bige, FALSE))
                {
                    const char *const *n;

                    /* skip the '"' */
                    p += 2;
                    rem -= 2;
                    
                    /* 
                     *   check for a match to any of the valid names for this
                     *   character set 
                     */
                    for (n = cs_names ; *n != 0 ; ++n)
                    {
                        /* if it's a match, stop scanning */
                        if (ucs_str_starts_with(p, rem, *n, bige, TRUE))
                        {
                            size_t l;

                            /* get the length of the name */
                            l = strlen(*n) * 2;

                            /* check for a close quote */
                            if (rem >= l + 2
                                && ucs_char_eq(p + l, '"', bige, FALSE))
                            {
                                /* skip the name and the quote */
                                p += l + 2;
                                rem -= l + 2;

                                /* skip the source text to this point */
                                skip = p - buf;

                                /* stop scanning */
                                break;
                            }
                        }
                    }
                }
            }

            /* seek just past the character set indicators */
            osfseek(fp, startofs + skip, OSFSK_SET);

            /* return the file */
            return srcf;
        }
    }

    /*
     *   It doesn't appear to use UCS-2 encoding (at least, the file
     *   doesn't start with a byte-order sensing sequence).  Check to see
     *   if the file starts with "#charset " in ASCII single-byte
     *   characters.  
     */
    if (siz >= 9 && memcmp(buf, "#charset ", 9) == 0)
    {
        char *p;
        size_t rem;
        
        /* skip the #charset string and any following spaces */
        for (p = buf + 9, rem = siz - 9 ;
             rem > 0 && (*p == ' ' || *p == '\t') ; ++p, --rem) ;

        /* make sure we're looking at a '"' */
        if (rem != 0 && *p == '"')
        {
            char *charset_name;

            /* skip the open quote */
            ++p;
            --rem;
            
            /* remember where the character set name starts */
            charset_name = p;

            /* 
             *   find the closing quote, which must occur before a CR or
             *   LF character 
             */
            for ( ; rem > 0 && *p != '"' && *p != 10 && *p != 13 ;
                 ++p, --rem) ;

            /* make sure we found a matching quote */
            if (rem != 0 && *p == '"')
            {
                /* seek just past the #charset string */
                osfseek(fp, startofs + (p - buf) + 1, OSFSK_SET);

                /* 
                 *   put a null terminator at the end of the character set
                 *   name 
                 */
                *p = '\0';

                /* create a mapper */
                mapper = CCharmapToUni::load(res_loader, charset_name);

                /* 
                 *   if that succeeded, return a reader for the mapper;
                 *   otherwise, simply proceed as though no #charset had
                 *   been present, so that we create a default mapper 
                 */
                if (mapper != 0)
                {
                    /* success - return a reader */
                    return new CTcSrcFile(fp, mapper);
                }
                else
                {
                    /* tell the caller the #charset was invalid */
                    *charset_error = TRUE;
                }
            }
        }
    }

    /* 
     *   we didn't find any sensing codes, so seek back to the start of
     *   the file 
     */
    osfseek(fp, startofs, OSFSK_SET);

    /*
     *   We couldn't identify the file's character set based on anything
     *   in the file, so create a mapper for the given default character
     *   set.  If there's not even a default character set defined, create
     *   a plain ASCII mapper.  
     */
    if (default_charset != 0)
        mapper = CCharmapToUni::load(res_loader, default_charset);
    else
        mapper = new CCharmapToUniASCII();

    /* check to see if we created a mapper */
    if (mapper != 0)
    {
        /* return a source file reader based on the mapper */
        return new CTcSrcFile(fp, mapper);
    }
    else
    {
        /* 
         *   we failed to create a mapper for the default character set -
         *   flag the problem 
         */
        *default_charset_error = TRUE;

        /* close the input file */
        osfcls(fp);

        /* return failure */
        return 0;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Read a line of text from the file.  
 */
size_t CTcSrcFile::read_line(char *buf, size_t bufl)
{
    char *dst;

    /* start out writing to the start of the caller's buffer */
    dst = buf;

    /*
     *   Keep going until we run out of input file, fill up the buffer, or
     *   reach the end of a line 
     */
    for (;;)
    {
        char *src;
        
        /* read some more data if our buffer is empty */
        if (rem_ == 0)
        {
            /* load another buffer-full */
            rem_ = mapper_->read_file(fp_, buf_, sizeof(buf_));

            /* 
             *   If we didn't read anything, we've reached the end of the
             *   file.  If we've already copied anything into the caller's
             *   buffer, null-terminate their buffer and return success;
             *   otherwise, return failure, since the caller has already
             *   read everything available from the file.  
             */
            if (rem_ == 0)
            {
                /* 
                 *   Remember that we've reached the end of the file.
                 *   We're about to return the last of the data, so the
                 *   caller will not need to call us again (although it's
                 *   legal if they do - we'll just return a zero length on
                 *   the next call).  
                 */
                at_eof_ = TRUE;
                
                /* check if we've copied anything to the caller's buffer */
                if (buf == dst)
                {
                    /* the caller's buffer is empty - return end of file */
                    return 0;
                }
                else
                {
                    /* null-terminate the caller's buffer */
                    *dst++ = '\0';

                    /* 
                     *   return the number of bytes copied, including the null
                     *   terminator 
                     */
                    return (dst - buf);
                }
            }

            /* start over at the beginning of the buffer */
            p_ = buf_;
        }

        /*
         *   Scan the input buffer one character (not byte) at a time.
         *   Keep track of how much many bytes we've skipped.  Stop when
         *   we reach a CR or LF character, or when skipping another
         *   character would exceed the remaining capacity of the caller's
         *   buffer, or when we run out of data in our input buffer.  
         */
        for (src = p_ ; rem_ > 0 ; )
        {
            size_t csiz;
            
            /* get the length of the current character */
            csiz = utf8_ptr::s_charsize(*src);

            /* 
             *   if this character plus a null terminator wouldn't fit in
             *   the output buffer, stop scanning 
             */
            if (csiz >= bufl)
            {
                /* 
                 *   There's no more room in the caller's buffer.  Copy
                 *   what we've scanned so far to the output buffer and
                 *   null-terminate the buffer.  
                 */
                memcpy(dst, p_, src - p_);

                /* advance past the copied bytes and write the null byte */
                dst += (src - p_);
                *dst++ = '\0';

                /* advance the buffer read pointer over the copied bytes */
                p_ = src;

                /* return success - indicate the number of bytes copied */
                return (dst - buf);
            }

            /* 
             *   If it's a newline character of some kind, we're done with
             *   this line.  Note that we can just check the byte directly,
             *   since if it's a multi-byte character, we'll never mistake
             *   the first byte for a single-byte newline or carriage return
             *   character, since a UTF-8 lead byte always has the high bit
             *   set.
             *   
             *   Also treat the Unicode character 0x2028 (line separator) as
             *   a newline.  
             */
            if (*src == '\n' || *src == '\r'
                || utf8_ptr::s_getch(src) == 0x2028)
            {
                char nl;
                
                /* copy what we've scanned so far to the caller's buffer */
                memcpy(dst, p_, src - p_);

                /* advance past the copied bytes */
                dst += src - p_;

                /* 
                 *   add a newline to the caller's buffer -- always add a
                 *   '\n' newline, regardless of what kind of newline
                 *   sequence we found in the input; also add a null
                 *   terminator 
                 */
                *dst++ = '\n';
                *dst++ = '\0';

                /* remember which type of newline we found */
                nl = *src;

                /* advance past the newline */
                p_ = src + csiz;
                rem_ -= csiz;

                /* 
                 *   If the input buffer is empty, read more, so that we
                 *   can check the next character after the newline
                 *   character. 
                 */
                if (rem_ == 0)
                {
                    /* read more data */
                    rem_ = mapper_->read_file(fp_, buf_, sizeof(buf_));

                    /* start over at the start of the buffer */
                    p_ = buf_;
                }

                /* 
                 *   Check for a paired newline character.  If we found a
                 *   CR, check for an LF; if we found an LF, check for a
                 *   CR.  This will ensure that we will recognize
                 *   essentially any newline character sequence for any
                 *   platform - this will accept CR, LF, CR-LF, or LF-CR
                 *   sequences. 
                 */
                if (rem_ != 0
                    && ((nl == '\n' && *p_ == '\r')
                        || (nl == '\r' && *p_ == '\n')))
                {
                    /* it's a paired newline - skip the second character */
                    ++p_;
                    --rem_;
                }

                /* we've finished this line - return success */
                return dst - buf;
            }
            
            /* skip this character in the input and proceed */
            src += csiz;
            rem_ -= csiz;

            /* consider this character consumed in the caller's buffer */
            bufl -= csiz;
        }

        /*
         *   We've exhausted the current input buffer, without filling the
         *   caller's buffer.  Copy what we've skipped so far into the
         *   caller's buffer.  
         */
        memcpy(dst, p_, src - p_);

        /* 
         *   Advance the output pointer past the data we just copied, then
         *   continue looping to read more data from the input file. 
         */
        dst += src - p_;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Buffer reader source object 
 */

/*
 *   allocate 
 */
CTcSrcMemory::CTcSrcMemory(const char *buf, size_t len, CCharmapToUni *mapper)
{
    /* 
     *   Allocate a buffer for a UTF8-encoded copy of the buffer -
     *   allocate three bytes per byte of the original, since this is the
     *   worst case for expansion of the encoding.  Allocate one extra
     *   byte to ensure we have space for a null terminator.  
     */
    size_t alo_len = len*3;
    buf_alo_ = (char *)t3malloc(alo_len + 1);

    /* map the buffer */
    char *p = buf_alo_;
    mapper->map(&p, &alo_len, buf, len);

    /* null-terminate the translated buffer */
    *p = '\0';

    /* start reading at the start of the translated buffer */
    buf_ = buf_alo_;
}

/* 
 *   delete 
 */
CTcSrcMemory::~CTcSrcMemory()
{
    /* free our buffer */
    t3free(buf_alo_);
}

/*
 *   read next line 
 */
size_t CTcSrcMemory::read_line(char *buf, size_t bufl)
{
    char *dst;
    const char *src;

    /* if there's nothing left in our buffer, return EOF */
    if (*buf_ == '\0')
        return 0;

    /* start out writing to the start of the caller's buffer */
    dst = buf;

    /*
     *   Scan the input buffer one character (not byte) at a time.  Keep
     *   track of how much many bytes we've skipped.  Stop when we reach a
     *   CR or LF character, or when skipping another character would
     *   exceed the remaining capacity of the caller's buffer, or when we
     *   run out of data in our input buffer.  
     */
    for (src = buf_ ; *src != '\0' ; )
    {
        size_t csiz;

        /* get the length of the current character */
        csiz = utf8_ptr::s_charsize(*src);

        /* 
         *   if this character plus a null terminator wouldn't fit in the
         *   output buffer, stop scanning 
         */
        if (csiz >= bufl)
        {
            /* 
             *   There's no more room in the caller's buffer.  Copy what
             *   we've scanned so far to the output buffer and
             *   null-terminate the buffer.  
             */
            memcpy(dst, buf_, src - buf_);
            
            /* advance past the copied bytes and write the null byte */
            dst += (src - buf_);
            *dst++ = '\0';
            
            /* advance the buffer read pointer over the copied bytes */
            buf_ = src;
            
            /* return success - indicate the number of bytes copied */
            return (dst - buf);
        }

        /* 
         *   If it's a newline character of some kind, we're done with this
         *   line.  Note that we can just check the byte directly, since if
         *   it's a multi-byte character, we'll never mistake the first byte
         *   for a single-byte newline or carriage return character, since a
         *   UTF-8 lead byte always has the high bit set.  Allow Unicode
         *   character 0x2028 (line separator) as a newline as well.  
         */
        if (*src == '\n' || *src == '\r' || utf8_ptr::s_getch(src) == 0x2028)
        {
            char nl;
            
            /* copy what we've scanned so far to the caller's buffer */
            memcpy(dst, buf_, src - buf_);
            
            /* advance past the copied bytes */
            dst += src - buf_;
            
            /* 
             *   add a newline to the caller's buffer -- always add a '\n'
             *   newline, regardless of what kind of newline sequence we
             *   found in the input; also add a null terminator 
             */
            *dst++ = '\n';
            *dst++ = '\0';

            /* remember which type of newline we found */
            nl = *src;

            /* advance past the newline */
            buf_ = src + csiz;

            /* 
             *   Check for a paired newline character.  If we found a CR,
             *   check for an LF; if we found an LF, check for a CR.  This
             *   will ensure that we will recognize essentially any
             *   newline character sequence for any platform - this will
             *   accept CR, LF, CR-LF, or LF-CR sequences.  
             */
            if ((nl == '\n' && *buf_ == '\r')
                || (nl == '\r' && *buf_ == '\n'))
            {
                /* it's a paired newline - skip the second character */
                ++buf_;
            }
            
            /* we've finished this line - return its length */
            return dst - buf;
        }
        
        /* skip this character in the input and proceed */
        src += csiz;

        /* consider this space consumed in the caller's buffer */
        bufl -= csiz;
    }

    /*
     *   We've exhausted the input buffer, without filling the caller's
     *   buffer.  Copy what we've skipped so far into the caller's buffer.
     */
    memcpy(dst, buf_, src - buf_);
    dst += src - buf_;

    /* null-terminate the result buffer */
    *dst++ = '\0';

    /* advance our input pointer to the new (EOF) position */
    buf_ = src;

    /* return the buffer length */
    return dst - buf;
}

