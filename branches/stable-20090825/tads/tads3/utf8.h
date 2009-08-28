/* $Header: d:/cvsroot/tads/tads3/utf8.h,v 1.2 1999/05/17 02:52:29 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  utf8.h - UTF-8 character string manipulation
Function
  
Notes
  
Modified
  10/16/98 MJRoberts  - Creation
*/

#ifndef UTF8_H
#define UTF8_H

#include <stdlib.h>

/* ------------------------------------------------------------------------ */
/*
 *   UTF-8 character string pointer.
 *   
 *   Note that this class deviates from the normal naming convention where
 *   each class begins with a capital 'C'.  Since this class is so
 *   low-level, and is used so much like the (char *) type, it seems more
 *   proper to give it a name as though it were a typedef for a native type.
 *   
 *   If ever there was a time when operator overloading is indicated, this
 *   would be it.  We could overload increment and decrement operators, for
 *   example, to step through the string.  However, I just plain don't like
 *   operator overloading, so I do not use it here.  Instead, we use
 *   explicit method names to avoid obfuscating the code as overloaded
 *   operators would.  It's a trade-off: it's less concise this way, but
 *   less obscure.
 *   
 *   Note the important distinction between "byte" and "character": a byte
 *   is the basic multi-bit unit of native storage, and a character
 *   represents the basic lexical unit; a character may be composed of more
 *   than one byte.
 */

class utf8_ptr
{
public:
    /* create a UTF-8 string pointer, with no initial underlying string */
    utf8_ptr() { p_ = 0; }
    
    /* 
     *   Create a UTF-8 string pointer with an underlying string.  The
     *   pointer must point to the first byte of a valid character.  
     */
    utf8_ptr(char *str) { set(str); }

    /* 
     *   Set the pointer to a new underlying buffer.  The pointer must
     *   point to the first byte of a valid character if there are already
     *   characters in the buffer.  
     */
    void set(char *str) { p_ = str; }

    /*
     *   Get the character at the current position 
     */
    wchar_t getch() const { return s_getch(p_); }

    /*
     *   Get the character at a given character offset from the current
     *   position.  The offset must be positive.  
     */
    wchar_t getch_at(size_t ofs) const { return s_getch_at(p_, ofs); }

    /*
     *   Get the character preceding the current character by the given
     *   amount.  The offset must be positive.  getch_before(1) returns
     *   the character preceding the current character, (2) returns the
     *   character two positions before the current character, and so on. 
     */
    wchar_t getch_before(size_t ofs) const { return s_getch_before(p_,ofs); }

    /*
     *   Encode a character into the buffer at the current position, and
     *   increment the pointer past the character.
     */
    void setch(wchar_t ch)
    {
        /* store the character and advance the buffer pointer */
        p_ += s_putch(p_, ch);
    }

    /* call setch() for each character in a null-terminated string */
    void setch_str(const char *str)
    {
        for ( ; *str != '\0' ; ++str)
            p_ += s_putch(p_, *str);
    }

    /*
     *   Encode a string of wide characters into the buffer.  We'll
     *   increment our pointer so that it points to the next available
     *   character when we're done.  Returns the number of bytes used for
     *   the encoding.
     *   
     *   'src_count' is the number of wide characters in the source string.
     *   
     *   'bufsiz' gives the size remaining in the underlying buffer.  If
     *   we run out of space, we won't encode any more characters, but we
     *   will still return the total number of bytes required to encode
     *   the string.  
     */
    size_t setwchars(const wchar_t *src, size_t src_count, size_t bufsiz);

    /*
     *   Encode a null-terminated string of wide-characters into our
     *   buffer.  Works like setwchars(), but stops at the null terminator
     *   in the source rather than taking a character count.
     *   
     *   This routine includes the null terminator in the resulting UTF-8
     *   string, and includes the space it takes in the result length, BUT
     *   we leave our pointer pointing to the null terminator.  
     */
    size_t setwcharsz(const wchar_t *src, size_t bufsiz);
    
    /* increment the pointer by one character */
    void inc() { p_ = s_inc(p_); }

    /* 
     *   increment the pointer by one character, and decrement a remaining
     *   length counter accordingly 
     */
    void inc(size_t *rem)
    {
        char *p;
        
        /* calculate the increment amount */
        p = s_inc(p_);

        /* decrement the length counter by the change */
        *rem -= (p - p_);

        /* save the new pointer value */
        p_ = p;
    }

    /* decrement the pointer by one character */
    void dec() { p_ = s_dec(p_); }

    /* decrement poniter and increment the remaining size to compensate */
    void dec(size_t *rem)
    {
        char *p;

        /* calculate the decrement amount */
        p = s_dec(p_);

        /* decrement the length counter by the change */
        *rem += (p_ - p);
        
        /* save the new pointer value */
        p_ = p;
    }

    /* 
     *   Determine if the current character is a continuation character.
     *   Returns 1 if so, 0 if not. 
     */
    int is_continuation() const { return s_is_continuation(p_); }

    /* 
     *   count the number of characters in the given number of bytes,
     *   starting at the current byte 
     */
    size_t len(size_t bytecnt) const
    {
        char *end;
        char *p;
        size_t cnt;

        /* calculate the ending point */
        p = p_;
        end = p + bytecnt;

        /* increment until we run out of bytes */ 
        for (cnt = 0 ; p < end ; p = s_inc(p), ++cnt) ;

        /* return the result */
        return cnt;
    }

    /* get the byte size of the current character */
    size_t charsize() const { return s_charsize(*p_); }
    
    /*
     *   Get the number of bytes in the given number of characters
     *   starting at the current position.  
     */
    size_t bytelen(size_t charcnt) const
    {
        char *p;

        /* skip the given number of characters */
        for (p = p_ ; charcnt != 0 ; p = s_inc(p), --charcnt) ;

        /* return the number of bytes we skipped */
        return (p - p_);
    }

    /*
     *   count the number of characters to the null terminator
     */
    size_t lenz() const
    {
        char *p;
        size_t cnt;

        /* increment until we find a null byte */
        for (cnt = 0, p = p_ ; *p != 0 ; p = s_inc(p), ++cnt) ;

        /* return the result */
        return cnt;
    }

    /* get the current pointer position */
    char *getptr() const { return p_; }

    /* -------------------------------------------------------------------- */
    /*
     *   Static methods 
     */

    /*
     *   Compare two UTF-8 strings.  Returns a value less than zero if the
     *   first string is lexically less than the second string (i.e., the
     *   first string sorts ahead of the second string), zero if the two
     *   strings are identical, or a value greater than zero if the first
     *   string is lexically greater than the second string.  
     */
    static int s_compare_to(const char *p1, size_t bytelen1,
                            const char *p2, size_t bytelen2);

    /* get the character at the given byte pointer */
    static wchar_t s_getch(const char *p)
    {
        /*
         *   If the high bit is 0, it's a one-byte sequence encoding the
         *   value in the low seven bits.  
         */
        if ((*p & 0x80) == 0)
            return (((unsigned char)*p) & 0x7f);

        /*
         *   If the high two bytes are 110, it's a two-byte sequence, with
         *   the high-order 5 bits in the low 5 bits of the first byte, and
         *   the low-order six bits in the low 6 bits of the second byte.  
         */
        if ((*p & 0xE0) == 0xC0)
            return (((((unsigned char)*p) & 0x1F) << 6)
                    + (((unsigned char)*(p + 1)) & 0x3F));

        /*
         *   Otherwise, we have a three-byte sequence: the high-order 4 bits
         *   are in the low-order 5 bits of the first byte, the next 6 bits
         *   are in the low-order 6 bits of the second byte, and the
         *   low-order 6 bits are in the low-order 6 bits of the third byte.
         */
        return (((((unsigned char)*p) & 0x0F) << 12)
                + ((((unsigned char)*(p + 1)) & 0x3F) << 6)
                + (((unsigned char)*(p + 2)) & 0x3F));
    }

    /* 
     *   get the character at a given positive character offset from a
     *   byte pointer 
     */
    static wchar_t s_getch_at(const char *p, size_t ofs)
    {
        /* skip the given number of characters */
        for ( ; ofs != 0 ; --ofs, p += s_charsize(*p)) ;

        /* return the character at the current position */
        return s_getch(p);
    }

    /*
     *   get the character preceding the current character by the given
     *   number of positions; the offset value must be positive 
     */
    static wchar_t s_getch_before(const char *p, size_t ofs)
    {
        /* skip backwards the given number of characters */
        for ( ; ofs != 0 ; --ofs)
        {
            /* 
             *   back up by one to three bytes, until we find no more
             *   continuation flags 
             */
            --p;
            p -= s_is_continuation(p);
            p -= s_is_continuation(p);
        }
        
        /* return the character at the current position */
        return s_getch(p);
    }

    /* 
     *   Write a given wchar_t value to the given byte pointer.  The
     *   caller must already have checked (via s_wchar_size) that there's
     *   enough room in the buffer for this character's UTF-8
     *   representation.
     *   
     *   Returns the number of bytes stored.  
     */
    static size_t s_putch(char *p, wchar_t ch)
    {
        /* check the range to determine how to encode it */
        if (ch <= 0x7f)
        {
            /* 
             *   it's in the range 0x0000 to 0x007f - encode the low-order
             *   7 bits in one byte 
             */
            *p = (char)(ch & 0x7f);
            return 1;
        }
        else if (ch <= 0x07ff)
        {
            /* 
             *   It's in the range 0x0080 to 0x07ff - encode it in two
             *   bytes.  The high-order 5 bits go in the first byte after
             *   the two-byte prefix of 110, and the low-order 6 bits go in
             *   the second byte after the continuation prefix of 10.  
             */
            *p++ = (char)(0xC0 | ((ch >> 6) & 0x1F));
            *p = (char)(0x80 | (ch & 0x3F));
            return 2;
        }
        else
        {
            /*
             *   It's in the range 0x0800 to 0xffff - encode it in three
             *   bytes.  The high-order 4 bits go in the first byte after
             *   the 1110 prefix, the next 6 bits go in the second byte
             *   after the 10 continuation prefix, and the low-order 6 bits
             *   go in the third byte after another 10 continuation prefix.  
             */
            *p++ = (char)(0xE0 | ((ch >> 12) & 0x0F));
            *p++ = (char)(0x80 | ((ch >> 6) & 0x3F));
            *p = (char)(0x80 | (ch & 0x3F));
            return 3;
        }
    }

    /* increment a pointer to a buffer, returning the result */
    static char *s_inc(char *p)
    {
        /* 
         *   increment the pointer by the size of the current character
         *   and return the result 
         */
        return p + s_charsize(*p);
    }

    /* get the size of the character at the given byte pointer */
    static size_t s_charsize(char c)
    {
        unsigned int ch;

        /*
         *   Check the top three bits.  If the pattern is 111xxxxx, we're
         *   pointing to a three-byte sequence.  If the pattern is
         *   110xxxxx, we're pointing to a two-byte sequence.  If it's
         *   0xxxxxxx, it's a one-byte sequence.
         *   
         *   We're being somewhat clever (tricky, anyway) here at the
         *   expense of clarity.  To avoid conditionals, we're doing some
         *   tricky bit masking and shifting, since these operations are
         *   extremely fast on most machines.  We figure out our increment
         *   using the bit patterns above to generate masks, then shift
         *   these around to produce 1's or 0's, then add up all of the
         *   mask calculations to get our final increment.
         *   
         *   The size is always at least 1 byte, so we start out with an
         *   increment of 1.
         *   
         *   Next, we note that character sizes other than 1 always
         *   require the high bit to be set.  So, the rest is all ANDed
         *   with (byte & 80) shifted right by seven OR'ed to the same
         *   thing shifted right by six, which will give us a bit mask of
         *   0 when the high bit is clear and 3 when it's set.
         *   
         *   Next, we'll pick out that third bit (xx1xxxxx or xx0xxxxx) by
         *   AND'ing with 0x20.  We'll shift this right by 5, to give us 1
         *   if we have a three-byte sequence.
         *   
         *   We'll then add 1 to this, so we'll have a result of 1 for a
         *   two-byte sequence, 2 for a three-byte sequence.  
         */
        ch = (unsigned int)(unsigned char)c;
        return (1 +
                ((((ch & 0x80) >> 7) | ((ch & 0x80) >> 6))
                 & (1 + ((ch & 0x20) >> 5))));
    }

    /* 
     *   get the number of bytes required to encode a given wchar_t in
     *   UTF-8 format 
     */
    static size_t s_wchar_size(wchar_t ch)
    {
        /* 
         *   characters 0-0x7f take up one byte; characters 0x80-0x7ff
         *   take up two bytes; all others take up three bytes 
         */
        return (ch < 0x80 ? 1 : (ch < 0x800 ? 2 : 3));
    }

    /* decrement a pointer by one character, returning the result */
    static char *s_dec(char *p)
    {
        /*
         *   Going backwards, we can't tell that we're on a start byte
         *   until we get there - there's no context to tell us which byte
         *   of a multi-byte sequence we're on, except that we can tell
         *   whether or not we're on the first byte or an extra byte.  So,
         *   decrement the pointer by a byte; if we're not on a start
         *   byte, decrement by another byte; if we're still not on a
         *   start byte, decrement it again.
         *   
         *   Since the longest possible sequence is three bytes, we'll
         *   unroll the loop and simply check twice to see if we're done
         *   yet.  
         */
        --p;
        p -= s_is_continuation(p);
        p -= s_is_continuation(p);

        /* return the result */
        return p;
    }

    /*
     *   Determine if the current byte is a continuation byte.  Returns 1
     *   if this is a continuation byte, 0 if not.  
     */
    static int s_is_continuation(const char *p)
    {
        unsigned int ch;

        /*   
         *   Continuation bytes have the pattern 10xxxxxx.  Initial bytes
         *   never have this pattern.  So, if a byte ANDed with 0xC0 is
         *   0x80 (i.e., the high two bits have the exact patern '10'),
         *   we're on a continuation byte.
         *   
         *   To avoid conditionals, which can be expensive because they
         *   require branching, we'll play more bit mask tricks: we'll
         *   compute a value that's 1 when the high two bits are '10', and
         *   is zero otherwise, and then subtract that from the current
         *   pointer.  To figure this value, we'll mask the byte with 0x80
         *   to pick out the high bit, and shift this right seven bits.
         *   This will give us 1 for 1xxxxxxx.  Then, we'll mask the byte
         *   with 0x40, which will pick out the second bit, invert the
         *   resulting bit pattern, AND it again with 0x40, and shift it
         *   right six bits.  This will give us 1 for x0xxxxxx.  We'll AND
         *   this with the previous calculation, which will give us 1 for
         *   10xxxxxx and 0 for anything else.  
         */
        ch = (unsigned int)(unsigned char)*p;
        return (((ch & 0x80) >> 7)
                & (((~(ch & 0x40)) & 0x40) >> 6));
    }

    /*
     *   Truncate a string to the given byte length, ensuring that only
     *   whole characters are included in the result.  Takes the proposed
     *   truncated length, and returns the actual length to use.  The
     *   returned length will be less than or equal to the proposed
     *   length; if the returned length is less than the proposed length,
     *   it means that the proposed length would have cut off a multi-byte
     *   character, so the actual length had to be shorter to ensure that
     *   no bytes of the final character were included. 
     */
    static size_t s_trunc(const char *p, size_t len)
    {
        const char *last_ch;
        size_t last_ch_len;

        /* 
         *   if the length is zero, no adjustment is needed - you
         *   obviously can't divide zero bytes 
         */
        if (len == 0)
            return 0;

        /* 
         *   Get a pointer to the start of the last byte within the
         *   proposed truncated byte region.  Note that the last byte in
         *   the buffer is at index (len-1), since the byte at index (len)
         *   is the next byte after the truncated region.  
         */
        last_ch = p + len - 1;

        /* 
         *   Decrement this byte pointer until we get to the start of the
         *   character that contains the final byte.  Since a character
         *   can never be more than three bytes long, we need decrement
         *   our pointer a maximum of two times.  
         */
        last_ch -= s_is_continuation(last_ch);
        last_ch -= s_is_continuation(last_ch);

        /* 
         *   figure the number of bytes of the last character that are
         *   actually in the truncated region - this is simply the number
         *   of bytes from where we are now to the end of the region 
         */
        last_ch_len = len - (last_ch - p);

        /*
         *   Now compute the actual size of this last character.  If the
         *   last character's actual size is the same as the truncated
         *   size, then the last character fits exactly and we can return
         *   the proposed length unchanged.  If the last character's
         *   required length is more than the truncated length, it means
         *   that the truncation has cut off the last character so that
         *   not all of its bytes fit, and hence we cannot include ANY of
         *   the last character's bytes in the result. 
         */
        if (last_ch_len >= s_charsize(*last_ch))
        {
            /* the last character fits in the truncation - we're fine */
            return len;
        }
        else
        {
            /* 
             *   the last character doesn't fit - truncate so that none of
             *   the last character's bytes are included 
             */
            return (last_ch - p);
        }
    }

private:
    /* the buffer pointer */
    char *p_;
};

#endif /* UTF8_H */


