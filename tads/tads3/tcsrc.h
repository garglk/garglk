/* $Header: d:/cvsroot/tads/tads3/TCSRC.H,v 1.3 1999/07/11 00:46:55 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tcsrc.h - TADS3 compiler source file reader
Function
  Provides I/O on source files, translating character sets between
  the file's character set and the internal UTF8 representation.

  A source file's character set is determined as follows:

  - If the first two bytes of the file are 0xFF, 0xFE, the file is taken
  as Unicode UCS-2 encoding, 16 bits per character, with low-order bytes
  stored first in each byte pair making up a character.  (The special
  Unicode marker character is 0xFEFF, and the character 0xFFFE is
  specifically illegal.  So, if we see FF FE in that order, we know that
  it must be taken to mean 0xFEFF, hence high byte second, because the
  other possibility - high byte first - would yield an invalid character.)

  - If the first two bytes of the file are 0xFE, 0xFF, the file is taken
  as Unicode UCS-2 encoding, 16 bits per character, with high-order bytes
  stored first.

  - If the first nine bytes of the contain the ASCII string "#charset"
  followed by an ASCII 0x20 byte, we obtain the name of the character
  set stored in the file by skipping past any following 0x20 or 0x09
  bytes.  The next character must be a double quote (0x22) character.
  We will scan characters up to the next double quote (0x22) character,
  which must occur within 256 bytes or before any CR or LF (0x0D, 0x0A)
  characters.  The characters between the quotes will be taken as the
  character set name.

  - If none of the above information is found at the beginning of the
  file, we take the file to be in the current global default character
  set.
Notes
  
Modified
  04/12/99 MJRoberts  - Creation
*/

#ifndef TCSRC_H
#define TCSRC_H

#include <stdlib.h>


/* ------------------------------------------------------------------------ */
/*
 *   Generic source object 
 */
class CTcSrcObject
{
public:
    CTcSrcObject() { }
    virtual ~CTcSrcObject() { }

    /* 
     *   Read the next line.  Fills in the buffer with a null-terminated
     *   string, ending in a newline if the line fits in the buffer, or
     *   ending without a newline if not.  Returns zero at end of file.
     */
    virtual size_t read_line(char *buf, size_t buflen) = 0;

    /* have we reached the end of the file? */
    virtual int at_eof() const = 0;
};

/* ------------------------------------------------------------------------ */
/*
 *   Source File base class
 */
class CTcSrcFile: public CTcSrcObject
{
public:
    /*
     *   Create the source file reader.  We take ownership of the mapper
     *   object, so we'll delete it when we're deleted. 
     */
    CTcSrcFile(osfildef *fp, class CCharmapToUni *mapper);

    /* delete */
    virtual ~CTcSrcFile();

    /* 
     *   Read a line of text from the file.  On success, returns the
     *   length of the line read, including the null terminator character
     *   and any newline character.  At end of file or other error,
     *   returns zero.  Because the result is always null-terminated, a
     *   return value of zero will never occur except on error.
     *   
     *   The result will be a UTF-8 string, and will always be
     *   null-terminated.  If the source line fits into the buffer, a
     *   newline character ('\n') will be the last character of the
     *   string.  If the line is too long for the buffer, the result will
     *   end with something other than a newline character, and the next
     *   read_line() call will retrieve the remainder of the line (or as
     *   much of the remainder as will fit into the buffer for that call).
     *   The one exception is that, if the file does not end in a newline,
     *   the last line will be returned without a newline; this condition
     *   can be distinguished by the non-zero return value of the
     *   subsequent call to read_line().
     *   
     *   This routine translates from the source character set to UTF-8,
     *   and automatically translates newline conventions.  We handle CR,
     *   LF, CR-LF, or LF-CR newlines (CR is ASCII 13, LF is ASCII 10).  
     */
    size_t read_line(char *buf, size_t bufl);

    /* determine if we've reached end of file */
    int at_eof() const { return at_eof_; }

    /*
     *   Open a source file.  We'll scan the beginning of the file to
     *   determine what type of source file reader to use, then create an
     *   appropriate source file reader subclass to read the file.  We
     *   expect the filename to be limited to ASCII characters.
     *   
     *   If we can't identify the character set that the file uses, we'll
     *   use the given default character set.  If no default character set
     *   is given, we'll create a plain ASCII reader.
     *   
     *   If we encounter a #charset directive, and we can't load the
     *   desired character set map, we'll set *charset_error to true;
     *   otherwise, we'll set *charset_error to false.  Note that
     *   *charset_error will be set to false if there's simply no #charset
     *   directive.
     *   
     *   If we fail to open the default character set, we'll return null
     *   and set *default_charset_error to true.  
     */
    static CTcSrcFile *open_source(const char *filename,
                                   class CResLoader *res_loader,
                                   const char *default_charset,
                                   int *charset_error,
                                   int *default_charset_error);

    /*
     *   Open a plain ASCII source file or a Unicode file.  This doesn't look
     *   for a #charset marker, but it does check for Unicode byte-order
     *   markers.  If we find a Unicode byte-order marker, we'll read the
     *   file using the suitable Unicode mapper; otherwise we'll read it
     *   using a plain ASCII mapper.  
     */
    static CTcSrcFile *open_plain(const char *filename);

    /*
     *   Open a plain ASCII source file. 
     */
    static CTcSrcFile *open_ascii(const char *filename);

protected:
    /* 
     *   match the leading substring of a unicode utf-16 string to a given
     *   ascii string 
     */
    static int ucs_str_starts_with(const char *ustr, size_t ulen,
                                   const char *astr,
                                   int big_endian, int case_fold)
    {
        /* compare each character of the unicode string to the ascii string */
        for ( ; ulen >= 2 && *astr != '\0' ; ustr += 2, ulen -= 2, ++astr)
        {
            /* if the characters don't match, we don't have a match */
            if (!ucs_char_eq(ustr, *astr, big_endian, case_fold))
                return FALSE;
        }

        /* 
         *   if we reached the end of the ASCII string, we have a match;
         *   otherwise, we ran out of the Unicode string before we ran out of
         *   the ASCII string, so we don't have a match 
         */
        return (*astr == 0);
    }

    /* does a Unicode character match an ASCII character? */
    static int ucs_char_eq(const char *ustr, char ac, int big_endian,
                           int case_fold)
    {
        uchar lo, hi;
        uint uc;

        /* get this unicode character, translating its endianness */
        if (big_endian)
            hi = (uchar)*ustr, lo = (uchar)*(ustr + 1);
        else
            lo = (uchar)*ustr, hi = (uchar)*(ustr + 1);
        uc = (hi << 8) + lo;

        /* if it's outside of ASCII range, we obviously can't match */
        if (uc > 127)
            return FALSE;

        /* if we're folding case, convert both to lower case */
        if (case_fold)
            ac = (char)tolower(ac), uc = tolower((char)uc);

        /* compare the characters */
        return (ac == (char)uc);
    }

    /* end-of-file flag */
    unsigned int at_eof_ : 1;
    
    /* my source file */
    class CVmDataSource *fp_;

    /* read buffer */
    char buf_[1024];

    /* amount of data in the buffer */
    size_t rem_;

    /* current position in buffer */
    char *p_;

    /* my character mapper */
    class CCharmapToUni *mapper_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Memory buffer-based source reader 
 */
class CTcSrcMemory: public CTcSrcObject
{
public:
    CTcSrcMemory(const char *buf, size_t len, class CCharmapToUni *mapper);
    ~CTcSrcMemory();

    /* read the next line */
    size_t read_line(char *buf, size_t bufl);

    /* determine if we've reached end of file */
    int at_eof() const { return (*buf_ == '\0'); }

private:
    /* allocated buffer */
    char *buf_alo_;
    
    /* current buffer pointer */
    const char *buf_;
};

#endif /* TCSRC_H */

