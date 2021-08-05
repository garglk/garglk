/* $Header: d:/cvsroot/tads/tads3/charmap.h,v 1.2 1999/05/17 02:52:29 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  charmap.h - character-set mapper
Function
  Provides mappings between 16-bit Unicode and single-byte, multi-byte,
  and double-byte character sets.
Notes
  
Modified
  10/17/98 MJRoberts  - Creation
*/

#ifndef CHARMAP_H
#define CHARMAP_H

#include <stdlib.h>
#include <memory.h>
#include <string.h>

#include "utf8.h"
#include "os.h"
#include "t3std.h"


/* ------------------------------------------------------------------------ */
/*
 *   Mapping Types.  This enum provides a characterization of a local
 *   character set (as defined in a mapping file).  
 */
enum charmap_type_t
{
    /* 
     *   Single-byte character set - each character is represented with a
     *   single 8-bit byte.
     */
    CHARMAP_TYPE_SB,

    /*
     *   Double-byte character set - each character is represented with
     *   exactly two 8-bit bytes.  In each byte pair, the first byte is
     *   taken as the high-order byte, so a text input stream consisting
     *   of the bytes 0x12, 0x34, 0x56, 0x78 would be interpreted as the
     *   two 16-bit code point values 0x1234, 0x5678.  
     */
    CHARMAP_TYPE_DB,

    /*
     *   Mixed multi-byte - each character is represented by either one or
     *   two 8-bit bytes.  Each two-byte character starts with a byte that
     *   is only used in two-byte characters; each one-byte character
     *   consists of a single byte that is not used as the first byte of
     *   any two-byte character.  In each two-byte character, the first
     *   byte is taken as the high-order byte.
     *   
     *   For example, assuming that 0x00-0x7F are defined as single-byte
     *   characters, and 0x8000-0xFFFF are defined as double-byte
     *   characters, the byte sequence 0x12, 0x81, 0xAB, 0x82, 0xCD, 0x34
     *   would be taken as the character sequence 0x12, 0x81AB, 0x82CD,
     *   0x34.  
     */
    CHARMAP_TYPE_MB
};

/* ------------------------------------------------------------------------ */
/*
 *   Basic character mapper class. 
 */
class CCharmap
{
public:
    /* add a reference */
    void add_ref() { ++ref_cnt_; }

    /* release a reference; delete on removing the last reference */
    void release_ref()
    {
        /* count the unreference */
        --ref_cnt_;

        /* if that leaves no references, delete me */
        if (ref_cnt_ == 0)
            delete this;
    }

protected:
    CCharmap()
    {
        /* start out with one reference, for the initial creator */
        ref_cnt_ = 1;
    }
    
    virtual ~CCharmap() { }
    
    /*
     *   Open and characterize a mapping file.  Returns the osfildef
     *   pointer if the file was successfully opened and parsed, or null
     *   if not.  Sets *map_type to indicate the type of mapping contained
     *   in the file.  
     */
    static osfildef *open_map_file(class CResLoader *res_loader,
                                   const char *table_name,
                                   charmap_type_t *map_type);

    /*
     *   Open and characterize a map file, checking name synonyms 
     */
    static osfildef *open_map_file_syn(class CResLoader *res_loader,
                                       const char *table_name,
                                       charmap_type_t *map_type);

    /* check a name to see if it matches one of the names for ASCII */
    static int name_is_ascii_synonym(const char *table_name)
    {
        /* accept any of the various synonyms for ASCII */
        return (stricmp(table_name, "us-ascii") == 0
                || stricmp(table_name, "asc7dflt") == 0
                || stricmp(table_name, "ascii") == 0
                || stricmp(table_name, "iso646-us") == 0
                || stricmp(table_name, "iso-ir-6") == 0
                || stricmp(table_name, "cp367") == 0
                || stricmp(table_name, "us") == 0);
    }

    /* check for a utf-8 synonym */
    static int name_is_utf8_synonym(const char *table_name)
    {
        return (stricmp(table_name, "utf-8") == 0
                || stricmp(table_name, "utf8") == 0);
    }

    /* check for ucs2-le synonyms */
    static int name_is_ucs2le_synonym(const char *table_name)
    {
        return (stricmp(table_name, "utf-16le") == 0
                || stricmp(table_name, "utf16le") == 0
                || stricmp(table_name, "utf_16le") == 0
                || stricmp(table_name, "unicodel") == 0
                || stricmp(table_name, "unicode-l") == 0
                || stricmp(table_name, "unicode-le") == 0
                || stricmp(table_name, "ucs-2le") == 0
                || stricmp(table_name, "ucs2le") == 0);
    }

    /* check for ucs2-be synonyms */
    static int name_is_ucs2be_synonym(const char *table_name)
    {
        return (stricmp(table_name, "utf-16be") == 0
                || stricmp(table_name, "utf16be") == 0
                || stricmp(table_name, "utf_16be") == 0
                || stricmp(table_name, "unicodeb") == 0
                || stricmp(table_name, "unicode-b") == 0
                || stricmp(table_name, "unicode-be") == 0
                || stricmp(table_name, "ucs-2be") == 0
                || stricmp(table_name, "ucs2be") == 0);
    }

    /* check a name to see if it matches one of the names for ISO 8859-1 */
    static int name_is_8859_1_synonym(const char *table_name)
    {
        /* accept any of the various names for ISO 8859-1 */
        return (stricmp(table_name, "iso-8859-1") == 0
                || stricmp(table_name, "iso_8859-1") == 0
                || stricmp(table_name, "iso_8859_1") == 0
                || stricmp(table_name, "iso8859-1") == 0
                || stricmp(table_name, "iso8859_1") == 0
                || stricmp(table_name, "8859-1") == 0
                || stricmp(table_name, "8859_1") == 0
                || stricmp(table_name, "iso-ir-100") == 0
                || stricmp(table_name, "latin1") == 0
                || stricmp(table_name, "latin-1") == 0
                || stricmp(table_name, "l1") == 0
                || stricmp(table_name, "iso1") == 0
                || stricmp(table_name, "cp819") == 0);
    }

    /* reference count */
    unsigned int ref_cnt_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Base character mapper class for mapping from a local character set to
 *   UTF-8.  This is an abstract interface that must be implemented for
 *   different classes of character sets.  
 */
class CCharmapToUni: public CCharmap
{
public:
    /* initialize */
    CCharmapToUni() { }

    /*
     *   Create a mapping object for a given character table.  We'll read
     *   enough of the character table to determine the appropriate
     *   concrete subclass to instantiate, then create an object, load the
     *   table into the object, and return the object.  The caller is
     *   responsible for deleting the object when finished with it.
     *   
     *   Returns null if the mapping file cannot be loaded.
     */
    static CCharmapToUni *load(class CResLoader *res_loader,
                               const char *table_name);

    /*
     *   Validate a UTF-8 buffer.  If we find any ill-formed byte sequences,
     *   we'll convert the errant bytes to '?' characters.
     */
    static void validate(char *buf, size_t len);

    /*
     *   Determine if the given byte sequence forms a complete character in
     *   the local character set.  Returns true if so, false if not.  'len'
     *   must be at least 1.  
     */
    virtual int is_complete_char(const char *p, size_t len) const = 0;

    /*
     *   Map one character.  Advances the input pointer and length past the
     *   character.  If there's a complete character at 'p', maps the
     *   character and returns true; if additional bytes are needed to form a
     *   complete character, leaves 'p' unchanged and returns false.  
     */
    virtual int mapchar(wchar_t &ch, const char *&p, size_t &len) = 0;

    /*
     *   Convert a string from the local character set to Unicode.
     *   Returns the byte length of the output.  If the output buffer is
     *   too small to store the result, we will return the size of the
     *   full result, but we won't write past the end of the buffer.
     *   
     *   We'll advance *output_ptr by the number of bytes we write.
     *   
     *   If we store anything, we'll decrement *output_buf_len by the
     *   number of bytes we store; if we don't have enough room, we'll set
     *   *output_buf_len to zero.
     *   
     *   input_ptr is a pointer to the input string; input_len is the
     *   length in bytes of the input string.  
     */
    virtual size_t map(char **output_ptr, size_t *output_buf_len,
                       const char *input_ptr, size_t input_len) const = 0;

    /*
     *   Convert a string from the local character set to Unicode.
     *   
     *   This works the same way as map(), but additionally provides
     *   information on the consumption of source bytes by filling in
     *   partial_len with the number of bytes at the end of the source
     *   buffer that are not mappable because they do not form complete
     *   characters in the source character set.  Since we scan all input
     *   regardless of whether there's space to store the resulting output,
     *   this will reflect the same number of bytes no matter what the
     *   output buffer length.  
     */
    virtual size_t map2(char **output_ptr, size_t *output_buf_len,
                        const char *input_ptr, size_t input_len,
                        size_t *partial_len) const = 0;

    /* 
     *   Map a null-terminated string into a buffer; returns the number of
     *   bytes of the buffer actually needed to store the string.  If the
     *   entire string couldn't be mapped, this will return a number
     *   greater than or equal to the output buffer size, but we will not
     *   write beyond the end of the buffer.
     *   
     *   If there's space, the result will be null-terminated; however,
     *   the null terminator byte will not be included in the result
     *   length.  If the return value exactly equals outbuflen, it means
     *   that the string exactly fills the buffer, hence there isn't space
     *   for a null terminator.  
     */
    size_t map_str(char *outbuf, size_t outbuflen, const char *input_str);

    /* 
     *   Map a counted-length string into a buffer; returns the number of
     *   bytes actually needed to store the string.  If the entire string
     *   couldn't be mapped, returns the number of bytes actually needed, but
     *   will only map up to the available size limit.  Does not
     *   null-terminate the result.
     */
    size_t map_str(char *outbuf, size_t outbuflen,
                   const char *input_str, size_t input_len);

    /*
     *   Map a string, allocating space
     *   
     *   The caller is responsible for freeing the returned string with
     *   t3free() when done with it. 
     */
    size_t map_str_alo(char **outbuf, const char *input_str);

    /*
     *   Read characters from a file into a buffer, translating the
     *   characters to UTF-8.  Returns the number of bytes copied into the
     *   buffer; returns zero on end of file.  The buffer must be at least
     *   three bytes long to ensure that at least one character can be read
     *   from the file (the longest UTF-8 character takes up three bytes),
     *   since it would otherwise not be possible to distinguish reaching
     *   the end of the file from simply being unable to fit even one
     *   character into the buffer.
     *   
     *   The file can be opened in text or binary mode; we don't pay any
     *   attention to newline sequences, so the mode is not relevant to us.
     *   
     *   This routine may read fewer than the desired number of bytes.  Upon
     *   return, the file's seek position should be set to the next byte of
     *   the file after the last character copied into the output buffer.
     *   
     *   'read_limit' is the maximum number of bytes we're allowed to read
     *   from the underlying file.  If this is zero, then the read size is
     *   unlimited.  
     */
    virtual size_t read_file(class CVmDataSource *fp,
                             char *buf, size_t bufl) = 0;

protected:
    /* delete the mapping */
    virtual ~CCharmapToUni() { }

    /* load the mapping table from the file */
    void load_table(osfildef *fp);

    /*
     *   Set a mapping.  uni_code_pt is the unicode code point, and
     *   local_code_pt is the code point in the local character set.  
     */
    virtual void set_mapping(wchar_t uni_code_pt, wchar_t local_code_pt) = 0;
};

/* ------------------------------------------------------------------------ */
/*
 *   Base character mapper class for mapping from Unicode UTF-8 to a local
 *   character set.  This is an abstract interface that must be separately
 *   implemented for different classes of character sets.
 *   
 *   Each mapping object maintains a table of mapping tables.  The master
 *   table contains an array of up to 256 sub-tables.  The top 8 bits of
 *   the unicode character value give the index in the master table.  Each
 *   entry in the master table is a pointer to a sub-table, or a null
 *   pointer if there are no mappings for characters in the range for that
 *   sub-table.
 *   
 *   For example, unicode characters 0x0000 through 0x007f are mapped
 *   through the table obtained by getting the pointer at index 0 from the
 *   master table.  Unicode characters 0x0200 through 0x02ff are in the
 *   table at master table index 2.
 *   
 *   If a master table index entry is empty (i.e., the pointer in the
 *   master table at that index is null), it means that all of the
 *   characters in the range for that master index map to the default
 *   character.  Otherwise, we index into the sub-table using the
 *   low-order 8 bits of the Unicode character code to find the character
 *   mapping giving the local character set code for the Unicode value.
 *   
 *   Each entry in the mapping table is the offset of the translation of
 *   the character within the translation array.  The translation array is
 *   an array of bytes.  The first byte of each entry is the length in
 *   bytes of the entry (not including the length byte), followed by the
 *   bytes of the entry.
 *   
 *   The first entry in the translation array is always the default
 *   character, which is the mapping we use for characters with no other
 *   valid mapping.  
 */
class CCharmapToLocal: public CCharmap
{
public:
    /* initialize */
    CCharmapToLocal();

    /* create a mapper and load the mapping from a file */
    static CCharmapToLocal *load(class CResLoader *res_loader,
                                 const char *table_name);
    
    /* 
     *   Convert a character from Unicode to the local character set.
     *   Stores the character's byte or bytes at the given pointer, and
     *   increments the pointer to point to the next byte after the
     *   character.
     *   
     *   Returns the byte length of the output.  If the output buffer is
     *   not long enough to store the result, we simply return the size of
     *   the result without storing anything.
     *   
     *   If we actually store anything, we'll decrement *output_buf_len by
     *   the number of bytes we stored; if we don't have room to store
     *   anything, we'll set *output_buf_len to zero.  
     */
    virtual size_t map(wchar_t unicode_char, char **output_ptr,
                       size_t *output_buf_len) const = 0;

    /*
     *   Simple single-character mapper - returns the byte length of the
     *   local character equivalent of the unicode character, which is
     *   written into the buffer.  If the buffer isn't big enough, we'll
     *   still return the length, but won't write anything to the buffer.  
     */
    size_t map_char(wchar_t unicode_char, char *buf, size_t buflen)
    {
        /* map the character */
        return map(unicode_char, &buf, &buflen);
    }


    /*
     *   Convert a UTF-8 string with a given byte length to the local
     *   character set.
     *   
     *   Returns the byte length of the result.  If the result is too long
     *   to fit in the output buffer, we'll return the number of bytes we
     *   actually were able to store (we'll store as much as we can, and
     *   stop when we run out of space).  We'll indicate in
     *   *src_bytes_used how many bytes of the source we were able to map.
     *   
     *   If the output buffer is null, we will store nothing, but simply
     *   determine how much space it would take to store the entire string.
     *   
     *   This base class provides an implementation of this method that is
     *   suitable for all subclasses, but the method is defined as virtual
     *   so that subclasses can override it with a more tailored (and thus
     *   more efficient) implementation.  The general-purpose base-class
     *   implementation must call the virtual function map() for each
     *   character mapped.  
     */
    virtual size_t map_utf8(char *dest, size_t dest_len,
                            utf8_ptr src, size_t src_byte_len,
                            size_t *src_bytes_used) const;

    /* 
     *   map to local - alternative interface using character buffers (rather
     *   than UTF8 pointers) 
     */
    size_t map_utf8(char *dest, size_t dest_len,
                    const char *src, size_t src_byte_len,
                    size_t *src_bytes_used) const;

    /*
     *   Map to local, allocating a new buffer.  Fills in 'buf' with a
     *   pointer to the new buffer, which we allocate via t3malloc() with
     *   enough space for the mapped string plus a null terminator.  Maps the
     *   string into the buffer and adds a null byte.  Returns the byte
     *   length of the mapped string (not including the null byte).
     *   
     *   The caller is responsible for freeing the returned string with
     *   t3free() when done with it.
     */
    size_t map_utf8_alo(char **buf, const char *src, size_t srclen) const;
    

    /*
     *   Convert a null-terminated UTF-8 string to the local character set.
     *   
     *   Returns the byte length of the result.  If the result is too long
     *   to fit in the output buffer, we'll return the size without storing
     *   the entire string (we'll store as much as we can, and stop when we
     *   run out of space, but continue counting the length needed; call
     *   with a destination buffer length of zero to simply determine how
     *   much space is needed for the result).
     *   
     *   The length returned does NOT include the null terminator.  However,
     *   if there's room, we will null-terminate the result string.  So, if
     *   the caller wants the result to be null terminated, it should make
     *   sure that the buffer contains one byte more than the space reported
     *   as necessary to store the result.  
     */
    virtual size_t map_utf8z(char *dest, size_t dest_len, utf8_ptr src)
        const;

    /*
     *   Convert a null-terminated UTF-8 string to the local character set,
     *   filling in an 'escape' sequence for unknown characters.  For each
     *   unknown character, we'll invoke the given callback to get the
     *   'escaped' representation.  Use &CCharmapToLocal::source_esc_cb, for
     *   example, to map using source-code-style escape sequences.
     *   
     *   The callback takes the unmappable character, a pointer to the output
     *   buffer, and a pointer to the length remaining.  It should fill in
     *   the buffer with the escaped sequence (up to the remaining length
     *   limit), and adjust the buffer pointer and length for the space
     *   consumed.  The return value is the full length required for the
     *   complete escape sequence, even if there's not enough space in the
     *   buffer to hold that many characters.  
     */
    virtual size_t map_utf8z_esc(char *dest, size_t dest_len, utf8_ptr src,
                                 size_t (*esc_fn)(wchar_t, char **, size_t *))
        const;

    /* 
     *   ready-made callback for map_utf8z_esc() - map to unicode 'backslash'
     *   escape sequences ('\u1234'), as we'd use in tads source code 
     */
    static size_t source_esc_cb(wchar_t ch, char **dest, size_t *len);

    /* 
     *   Write data to a file, converting from UTF-8 to the local character
     *   set.  Returns zero on success, non-zero if an error occurs writing
     *   the data.  
     */
    int write_file(class CVmDataSource *fp, const char *buf, size_t bufl);

    /* 
     *   determine if the given Unicode character has a mapping to the local
     *   character set 
     */
    virtual int is_mappable(wchar_t unicode_char) const
    {
        /* 
         *   By default, it's mappable if it has a non-default mapping in
         *   the translation table.  The default mapping is always at offset
         *   zero in the translation table.  
         */
        return (get_mapping(unicode_char) != 0);
    }

    /*
     *   Get the display expansion for a unicode character.  This returns a
     *   pointer to an array of wchar_t characters, and fills in the length
     *   variable.  Returns null if there's no expansion.
     *   
     *   An "expansion" is a list of two or more unicode characters that
     *   should be substituted for the given unicode character when the
     *   character is displayed.  Display expansions are normally used for
     *   visual approximations when the local character set doesn't contain
     *   an exact match for the unicode character; for example, an ASCII
     *   mapping might use the expansion "(c)" to represent the copyright
     *   circled-C symbol, or the two-character sequence "AE" to represent
     *   the AE ligature.  
     */
    const wchar_t *get_expansion(wchar_t unicode_char, size_t *len)
    {
        size_t ofs;
        const wchar_t *map;

        /* get the mapping offset in the expansion array */
        ofs = get_exp_mapping(unicode_char);

        /* if the mapping offset is zero, it means there's no mapping */
        if (ofs == 0)
        {
            /* indicate that there's no mapping by returning null */
            *len = 0;
            return 0;
        }

        /* get the mapping pointer */
        map = get_exp_ptr(ofs);

        /* read the length and skip it */
        *len = (size_t)*map++;

        /* return the pointer to the first character of the expansion */
        return map;
    }

protected:
    /* delete the mapping */
    virtual ~CCharmapToLocal();

    /* given a Unicode character, get the mapping for the character */
    unsigned int get_mapping(wchar_t unicode_char) const
    {
        unsigned int *subtable;

        /* get the mapping table */
        subtable = get_sub_table(unicode_char);

        /* 
         *   If there is no subtable, return the default character, which is
         *   always at offset zero in the translation array; otherwise, use
         *   the low-order 8 bits of the character code as the index into
         *   the subtable and return the value we find there 
         */
        if (subtable == 0)
            return 0;
        else
            return subtable[unicode_char & 0xff];
    }

    /* given a Unicode character, get the expansion for the character */
    unsigned int get_exp_mapping(wchar_t unicode_char) const
    {
        unsigned int *subtable;

        /* get the mapping table */
        subtable = get_exp_sub_table(unicode_char);

        /* 
         *   if there's no subtable, return zero to indicate there's no
         *   expansion; otherwise, return the entry from the subtable 
         */
        return (subtable == 0 ? 0 : subtable[unicode_char & 0xff]);
    }

    /*
     *   Get a pointer to the sequence of bytes in the translation array at
     *   a given offset 
     */
    const unsigned char *get_xlat_ptr(unsigned int ofs) const
    {
        return &xlat_array_[ofs];
    }

    /*
     *   Get a pointer to the translation of a character and the length in
     *   bytes of the translation 
     */
    const unsigned char *get_xlation(wchar_t unicode_char, size_t *map_len)
        const
    {
        const unsigned char *map;

        /* get the translation offset */
        map = get_xlat_ptr(get_mapping(unicode_char));

        /* read the length and skip it in the table */
        *map_len = (size_t)*map++;

        /* return the mapped byte sequence */
        return map;
    }

    /* 
     *   get a pointer to the sequence of wchar_t values in the expansion
     *   array at a given offset 
     */
    const wchar_t *get_exp_ptr(unsigned int ofs) const
    {
        return &exp_array_[ofs];
    }

    /* load the mapping table from a file */
    void load_table(osfildef *fp);

    /*
     *   Given a Unicode character, get the sub-table for the character,
     *   or null if there is no sub-table for this character.  
     */
    unsigned int *get_sub_table(wchar_t unicode_char) const
    {
        /* 
         *   use the high-order 8 bits of the unicode character as the
         *   index into the master table 
         */
        return map_[(unicode_char >> 8) & 0xff];
    }

    /* 
     *   Given a Unicode character, get the expansion sub-table for the
     *   character. or null if there is no sub-table for the character.  
     */
    unsigned int *get_exp_sub_table(wchar_t unicode_char) const
    {
        /* 
         *   use the high-order 8 bits of the unicode character as the index
         *   into the master table 
         */
        return exp_map_[(unicode_char >> 8) & 0xff];
    }

    /*
     *   Set a mapping.  This allocates a new sub-table if necessary, and
     *   stores the local character mapping in the table.  
     */
    void set_mapping(wchar_t unicode_char, unsigned int xlat_offset);

    /* set an expansion mapping */
    void set_exp_mapping(wchar_t unicode_char, unsigned int exp_offset);

    /*
     *   The master mapping table list.  Each entry points to the
     *   sub-array that contains the mapping for the 256 characters whose
     *   high-order 8 bits give the index into this table.  Each entry of
     *   the subarray is the offset within the xlat_array_ byte array of
     *   the first byte of the translation for the unicode character.  
     */
    unsigned int *map_[256];

    /* 
     *   The master expansion mapping list.  This works just like map_, but
     *   points to exp_array_ entries for unicode display expansions.  
     */
    unsigned int *exp_map_[256];

    /*
     *   The translation array.  This is an array of bytes containing the
     *   translations.  map_[high_8_bits][low_8_bits] contains the offset
     *   within this array of the translation of the character with the
     *   given code ((high_8_bits << 8) + low_8_bits).  The first byte at
     *   this offset is the length in bytes of the translation, not
     *   counting the length byte.  The remaining bytes are the bytes of
     *   the translation for the character. 
     */
    unsigned char *xlat_array_;

    /* size of the translation array */
    size_t xlat_array_size_;

    /*
     *   The expansion array.  This is an array of unicode characters
     *   containing the expansions for displaying unicode characters.  This
     *   works just like xlat_array_: each entry in expmap_ is an index into
     *   this array, which gives the starting point in the array of the run
     *   of entries for the expansion of that character.  The first character
     *   of a run is a length prefix giving the number of characters in the
     *   expansion.  
     */
    wchar_t *exp_array_;
};


/* ======================================================================== */
/*
 *   Local character set - to - Unicode UTF-8 mappers 
 */

/* ------------------------------------------------------------------------ */
/*
 *   Trival UTF8-to-UTF8 mapper - performs no conversions.  This can be
 *   used when reading from an external data source that is itself in
 *   UTF-8 format; since this is identical to the format we use
 *   internally, no mapping is required.  
 */
class CCharmapToUniUTF8: public CCharmapToUni
{
public:
    /* read from a file */
    virtual size_t read_file(class CVmDataSource *fp, char *buf, size_t bufl);

    /* determine if a byte sequence forms a complete character */
    virtual int is_complete_char(const char *p, size_t len) const
    {
        /* 
         *   For UTF-8, we can infer the byte length of a character from the
         *   first byte of the sequence.  If the given length is at least the
         *   inferred byte length, we have a complete character.  
         */
        return (len >= utf8_ptr::s_charsize(*p));
    }

    /* map one character */
    virtual int mapchar(wchar_t &ch, const char *&p, size_t &len)
    {
        /* make sure we have a complete character */
        size_t clen = utf8_ptr::s_charsize(*p);
        if (len >= clen)
        {
            /* read the character */
            ch = utf8_ptr::s_getch(p);

            /* advance the input pointers */
            p += clen;
            len -= clen;

            /* success */
            return TRUE;
        }
        else
        {
            /* incomplete character */
            return FALSE;
        }
    }

    /* map a string */
    size_t map(char **output_ptr, size_t *output_buf_len,
               const char *input_ptr, size_t input_len) const
    {
        size_t partial_len;

        /* 
         *   do the full mapping, discarding the partial last character byte
         *   length information 
         */
        return map2(output_ptr, output_buf_len, input_ptr, input_len,
                    &partial_len);
    }

    /* map a string, providing partial character info */
    virtual size_t map2(char **output_ptr, size_t *output_buf_len,
                        const char *input_ptr, size_t input_len,
                        size_t *partial_len) const;

protected:
    /* we don't need a mapping table - ignore any that is set */
    virtual void set_mapping(wchar_t, wchar_t) { }
};

/* ------------------------------------------------------------------------ */
/*
 *   Character mapper base class for UCS-2 to UTF-8.  We will subclass
 *   this mapper for big-endian and little-endian UCS-2 representations,
 *   but both mappers are essentially the same in that only format
 *   translation is required, since UCS-2 and UTF-8 use the same code
 *   point mapping (i.e., Unicode).  
 */
class CCharmapToUniUcs2: public CCharmapToUni
{
public:
    /* read from a file */
    virtual size_t read_file(class CVmDataSource *fp, char *buf, size_t bufl);

    /* determine if a byte sequence forms a complete character */
    virtual int is_complete_char(const char *, size_t len) const
    {
        /* every character in UCS-2 requires two bytes */
        return (len >= 2);
    }

    /* map a string, providing partial character info */
    virtual size_t map2(char **output_ptr, size_t *output_buf_len,
                        const char *input_ptr, size_t input_len,
                        size_t *partial_len) const
    {
        /* 
         *   if the input length is odd, there's one byte of partial
         *   character information at the end of the buffer; otherwise
         *   everything is valid 
         */
        *partial_len = (input_len & 1);

        /* perform the usual mapping */
        return map(output_ptr, output_buf_len, input_ptr, input_len);
    }

protected:
    /* 
     *   there's no mapping table for UCS-2 translations, so we don't need
     *   to do anything with mappings 
     */
    virtual void set_mapping(wchar_t, wchar_t) { }

    /* temporary buffer for reading files */
    char inbuf_[512];
};

/* ------------------------------------------------------------------------ */
/*
 *   Character mapper for UCS-2 little-endian to UTF-8 
 */
class CCharmapToUniUcs2Little: public CCharmapToUniUcs2
{
public:
    /* map a string */
    size_t map(char **output_ptr, size_t *output_buf_len,
               const char *input_ptr, size_t input_len) const;

    /* map one character */
    virtual int mapchar(wchar_t &ch, const char *&p, size_t &len)
    {
        if (len >= 2)
        {
            /* map the character */
            ch = (unsigned char)*p++;
            ch |= ((unsigned char)*p++) << 8;

            /* deduct it from the length */
            len -= 2;

            /* success */
            return TRUE;
        }
        else
            return FALSE;
    }
};

/* ------------------------------------------------------------------------ */
/*
 *   Character mapper for UCS-2 big-endian to UTF-8 
 */
class CCharmapToUniUcs2Big: public CCharmapToUniUcs2
{
public:
    /* map a string */
    size_t map(char **output_ptr, size_t *output_buf_len,
               const char *input_ptr, size_t input_len) const;

    /* map one character */
    virtual int mapchar(wchar_t &ch, const char *&p, size_t &len)
    {
        if (len >= 2)
        {
            /* map the character */
            ch = ((unsigned char)*p++) << 8;
            ch |= (unsigned char)*p++;

            /* deduct it from the length */
            len -= 2;

            /* success */
            return TRUE;
        }
        else
            return FALSE;
    }
};

/* ------------------------------------------------------------------------ */
/*
 *   Basic character mapper for single-byte character sets to UTF-8 
 */
class CCharmapToUniSB_basic: public CCharmapToUni
{
public:
    /* read from a single-byte input file, translating to UTF-8 */
    virtual size_t read_file(class CVmDataSource *fp, char *buf, size_t bufl);

    /* determine if a byte sequence forms a complete character */
    virtual int is_complete_char(const char *, size_t) const
    {
        /* 
         *   every character in a single-byte set requires just one byte;
         *   since 'len' is required to be at least one, there's no way we
         *   can't have a complete character 
         */
        return TRUE;
    }

    /* map a string, providing partial character info */
    virtual size_t map2(char **output_ptr, size_t *output_buf_len,
                        const char *input_ptr, size_t input_len,
                        size_t *partial_len) const
    {
        /* 
         *   for all single-byte character sets, one byte == one character,
         *   so it's impossible to have partial characters 
         */
        *partial_len = 0;

        /* perform the normal mapping */
        return map(output_ptr, output_buf_len, input_ptr, input_len);
    }

protected:
    /* temporary buffer for reading files */
    char inbuf_[512];
};

/* ------------------------------------------------------------------------ */
/*
 *   Character mapper for plain ASCII to UTF-8
 */
class CCharmapToUniASCII: public CCharmapToUniSB_basic
{
public:
    /* map a string */
    size_t map(char **output_ptr, size_t *output_buf_len,
               const char *input_ptr, size_t input_len) const;

    /* map one character */
    virtual int mapchar(wchar_t &ch, const char *&p, size_t &len)
    {
        if (len >= 1)
        {
            /* map the character */
            ch = (unsigned char)*p++;

            /* substitude U+FFFD for any invalid character */
            if (ch > 127)
                ch = 0xFFFD;

            /* deduct it from the length */
            len -= 1;

            /* success */
            return TRUE;
        }
        else
            return FALSE;
    }

protected:
    /* 
     *   there's no map for the ASCII translation, so we can ignore
     *   mapping calls 
     */
    void set_mapping(wchar_t, wchar_t) { }
};

/* ------------------------------------------------------------------------ */
/*
 *   Character mapper for single-byte character sets to UTF-8.
 */
class CCharmapToUniSB: public CCharmapToUniSB_basic
{
public:
    CCharmapToUniSB()
    {
        int i;
        
        /* initialize the mapping table to all U+FFFD */
        for (i = 0 ; i < 256 ; ++i)
            map_[i] = 0xFFFD;
    }
    
    /* map a string */
    size_t map(char **output_ptr, size_t *output_buf_len,
               const char *input_ptr, size_t input_len) const;

    /* map one character */
    virtual int mapchar(wchar_t &ch, const char *&p, size_t &len)
    {
        if (len >= 1)
        {
            /* map the character */
            ch = map_[(unsigned char)*p++];

            /* deduct it from the length */
            len -= 1;

            /* success */
            return TRUE;
        }
        else
            return FALSE;
    }

protected:
    /* set a mapping */
    void set_mapping(wchar_t uni_code_pt, wchar_t local_code_pt)
    {
        /* 
         *   set the mapping, ignoring characters outside of our 8-bit
         *   range 
         */
        if (((unsigned int)local_code_pt) < 256)
            map_[local_code_pt] = uni_code_pt;
    }

private:
    /* 
     *   our mapping table - since the source character set is
     *   single-byte, we need only store a wchar_t for each of the
     *   possible 256 source characters 
     */
    wchar_t map_[256];
};

/* ------------------------------------------------------------------------ */
/*
 *   Character mapper for mixed multi-byte character sets to UTF-8.  This
 *   maps from local character sets that use a mixture of one-byte and
 *   two-byte sequences to represent characters.  
 */

/*
 *   Primary-byte mapping table entry.  This gives us mapping instructions
 *   for each leading byte of a character sequence.
 *   
 *   Each character is represented by a one-byte or two-byte sequence.  This
 *   mapper assumes a context-free mapping, hence for each character
 *   represented by a single byte, that single byte unambiguously indicates
 *   that character, and hence is never the first byte of a two-byte
 *   sequence.  For each character represented by a two-byte sequence, the
 *   first byte of the sequence can only be part of two-byte sequences, hence
 *   whenever we see that first byte we'll know for sure we have a two-byte
 *   character.
 *   
 *   Each mapping here is for a first byte.  If the byte is a single-byte
 *   character, then the 'sub' pointer is null and the 'ch' entry gives the
 *   Unicode code point for the character.  If the byte is the lead byte of
 *   one or more two-byte characters, then the 'sub' pointer is non-null and
 *   'ch' is ignored.  
 */
struct cmap_mb_entry
{
    /* 
     *   The sub-mapping table.  This is a pointer to a table of the Unicode
     *   code points of the two-byte sequences that start with this byte.
     *   Each entry in the array is a Unicode code point, and the array is
     *   indexed by the second byte of the two-byte sequence.  If this
     *   pointer is null, then this lead byte is a single-byte character.
     *   
     *   Note that this pointer, if non-null, always points to a 256-element
     *   array.  This array can thus be indexed directly with any unsigned
     *   8-bit byte value without any range checking.  
     */
    wchar_t *sub;

    /* 
     *   The Unicode code point of this character, if this primary byte is a
     *   one-byte character.  
     */
    wchar_t ch;
};


/*
 *   The multi-byte-to-UTF8 mapper 
 */
class CCharmapToUniMB: public CCharmapToUni
{
public:
    CCharmapToUniMB();

    /* delete the table */
    virtual ~CCharmapToUniMB();

    /* determine if a byte sequence forms a complete character */
    virtual int is_complete_char(const char *p, size_t len) const
    {
        /* 
         *   Check the first byte to see if this is a leading byte or a
         *   stand-alone single byte.  
         */
        if (map_[(unsigned char)*p].sub == 0)
        {
            /* 
             *   it's a stand-alone byte, so the character length is one;
             *   'len' is required to be at least 1, so we definitely have a
             *   complete character 
             */
            return TRUE;
        }
        else
        {
            /* it's a lead byte, so the character length is two */
            return (len >= 2);
        }
    }

    /* map one character */
    virtual int mapchar(wchar_t &ch, const char *&p, size_t &len)
    {
        /* get the first byte's translation entry */
        cmap_mb_entry e = map_[(unsigned char)*p];

        /* check if we have a single-byte or double-byte character */
        if (e.sub != 0)
        {
            /* double-byte character */
            if (len >= 2)
            {
                /* get the second character mapping */
                ch = e.sub[(unsigned char)*++p];

                /* skip the two bytes */
                p += 1;
                len -= 2;

                /* success */
                return TRUE;
            }
        }
        else
        {
            /* single-byte character */
            if (len >= 1)
            {
                /* get the mapping */
                ch = e.ch;

                /* skip the character */
                p += 1;
                len -= 1;

                /* success */
                return TRUE;
            }
        }

        /* if we made it this far, we don't have a complete character */
        return FALSE;
    }

    /* read from a multi-byte input file, translating to UTF-8 */
    virtual size_t read_file(class CVmDataSource *fp, char *buf, size_t bufl);

    /* map a string */
    size_t map(char **output_ptr, size_t *output_buf_len,
               const char *input_ptr, size_t input_len) const
    {
        size_t partial_len;

        /* 
         *   do the full mapping, discarding the partial last character byte
         *   length information 
         */
        return map2(output_ptr, output_buf_len, input_ptr, input_len,
                    &partial_len);
    }

    /* map a string, providing partial character info */
    virtual size_t map2(char **output_ptr, size_t *output_buf_len,
                        const char *input_ptr, size_t input_len,
                        size_t *partial_len) const;

protected:
    /* set a mapping */
    void set_mapping(wchar_t uni_code_pt, wchar_t local_code_pt);

private:
    /* the primary-byte mapping table */
    cmap_mb_entry map_[256];

    /* temporary buffer for reading files */
    char inbuf_[512];
};

/* ------------------------------------------------------------------------ */
/*
 *   Character mapper for double-byte character sets to UTF-8.  This maps
 *   from local character sets that use a two-byte sequence to represent each
 *   local character.
 *   
 *   For now, this is a trivial subclass of the multi-byte mapper; that
 *   mapper handles the more general case where each character can have a
 *   different byte length, so it readily handles the case where every
 *   character is two bytes.  If there's ever any demand for it, a
 *   special-case subclass to handle double-byte character sets specifically
 *   could provide slight efficiency gains, since it wouldn't have to check
 *   each lead byte to determine the character sequence length.  I don't
 *   expect this will be needed, though: there just aren't many fixed
 *   double-byte character sets in use (apart from the UCS2 sets, which we
 *   already have special handling for), and there's little chance of new
 *   ones arising in the future since Unicode is rapidly making the whole
 *   idea of vendor character sets obsolete.  
 */
class CCharmapToUniDB: public CCharmapToUniMB
{
public:
};

/* ------------------------------------------------------------------------ */
/*
 *   Character mapper for plain ISO-8859-1 to UTF-8 
 */
class CCharmapToUni8859_1: public CCharmapToUniSB
{
public:
    /* creation */
    CCharmapToUni8859_1()
    {
        wchar_t i;
        
        /* 
         *   Initialize our mapping table.  Each 8859-1 code point maps to
         *   the same code point in Unicode, so this is a trivial
         *   translation.  
         */
        for (i = 0 ; i < 256 ; ++i)
            set_mapping(i, i);
    }
};

/* ======================================================================== */
/*
 *   Unicode UTF-8 - to - local character set mappers 
 */

/* ------------------------------------------------------------------------ */
/*
 *   Trivial character mapper for UTF8-to-UTF8 conversions.  This can be
 *   used when writing external data in UTF8 format; since this is the
 *   same format we use internally, no conversion is required.  
 */
class CCharmapToLocalUTF8: public CCharmapToLocal
{
public:
    /* map a string */
    virtual size_t map_utf8(char *dest, size_t dest_len,
                            utf8_ptr src, size_t src_byte_len,
                            size_t *src_bytes_used) const;

    /* map a null-terminated string */
    virtual size_t map_utf8z(char *dest, size_t dest_len,
                             utf8_ptr src) const;

    /* map a character */
    size_t map(wchar_t unicode_char, char **output_ptr,
               size_t *output_len) const;

    /* 
     *   determine if the given Unicode character has a mapping to the local
     *   character set 
     */
    virtual int is_mappable(wchar_t unicode_char) const
    {
        /* every character can be mapped UTF8-to-UTF8, obviously */
        return TRUE;
    }
};

/* ------------------------------------------------------------------------ */
/*
 *   Character mapper for single-byte character sets.  Each character in
 *   the local (output) character set is represented by a single byte.
 */
class CCharmapToLocalSB: public CCharmapToLocal
{
public:
    /* map a string */
    virtual size_t map_utf8(char *dest, size_t dest_len,
                            utf8_ptr src, size_t src_byte_len,
                            size_t *src_bytes_used) const;

    /* map a null-terminated string */
    virtual size_t map_utf8z(char *dest, size_t dest_len,
                             utf8_ptr src) const;

    /* map a character */
    size_t map(wchar_t unicode_char, char **output_ptr,
               size_t *output_len) const;
};


/* ------------------------------------------------------------------------ */
/*
 *   Mixed multi-byte mapper.  Each local character is represented by a
 *   sequence of one or more bytes.
 *   
 *   This class is a trivial subclass of CCharmapToLocalSB.  The single-byte
 *   base class already does everything we need to do, because it is designed
 *   to cope with mappings that involve expansions that represent a single
 *   Unicode character with a sequence of local characters (for example,
 *   "(c)" for the copyright symbol).  
 */
class CCharmapToLocalMB: public CCharmapToLocalSB
{
public:
};

/*
 *   Double-byte mapper.  Each local character is represented by exactly two
 *   bytes.  This class is a trivial subclass of CCharmapToLocalMB, because
 *   the multi-byte mapper already handles the more general case of local
 *   character representations that use varying byte lengths; there is no
 *   particular efficiency gain to be had by creating a separate special-case
 *   class for double-byte character sets.  
 */
class CCharmapToLocalDB: public CCharmapToLocalMB
{
public:
};


/* ------------------------------------------------------------------------ */
/*
 *   Character mapper for mapping to local default 7-bit ASCII.  This
 *   mapper is has a built-in character set translation so that we can
 *   always create one without having to find an external mapping file.  
 */
class CCharmapToLocalASCII: public CCharmapToLocalSB
{
public:
    CCharmapToLocalASCII();
};


/*
 *   Character mapper for mapping to local ISO-8859-1.  This mapper has a
 *   built-in character set translation so that we can always create one
 *   even without an external mapping file.  
 */
class CCharmapToLocal8859_1: public CCharmapToLocalSB
{
public:
    CCharmapToLocal8859_1();
};

/* ------------------------------------------------------------------------ */
/*
 *   Character mapper for 16-bit Wide Unicode local character set.  Stores
 *   characters in the correct local wchar_t representation.  Assumes that
 *   the pointer is wchar_t-aligned.
 *   
 *   This is a trival translation.  Because we're mapping from Unicode to
 *   Unicode, the only thing we're changing is the encoding format - the
 *   character code is simply copied without any translation, since
 *   Unicode is the same everywhere.  
 */
class CCharmapToLocalWideUnicode: public CCharmapToLocal
{
public:
    /* map a string */
    virtual size_t map_utf8(char *dest, size_t dest_len,
                            utf8_ptr src, size_t src_byte_len,
                            size_t *src_bytes_used) const;

    /* map a null-terminated string */
    virtual size_t map_utf8z(char *dest, size_t dest_len,
                             utf8_ptr src) const;

    /* map a character */
    size_t map(wchar_t unicode_char, char **output_ptr,
               size_t *output_len) const;

    /* 
     *   determine if the given Unicode character has a mapping to the local
     *   character set 
     */
    virtual int is_mappable(wchar_t unicode_char) const
    {
        /* every character can be mapped UTF8-to-UCS2 */
        return TRUE;
    }
};

/* ------------------------------------------------------------------------ */
/*
 *   Character mapper for 16-bit Wide Unicode, big-endian.  Stores the
 *   characters in big-endian UCS-2 representation. 
 */
class CCharmapToLocalUcs2Big: public CCharmapToLocal
{
public:
    /* map a string */
    virtual size_t map_utf8(char *dest, size_t dest_len,
                            utf8_ptr src, size_t src_byte_len,
                            size_t *src_bytes_used) const;

    /* map a null-terminated string */
    virtual size_t map_utf8z(char *dest, size_t dest_len,
                             utf8_ptr src) const;

    /* map a character */
    size_t map(wchar_t unicode_char, char **output_ptr,
               size_t *output_len) const;
};

/* ------------------------------------------------------------------------ */
/*
 *   Character mapper for 16-bit Wide Unicode, little-endian.  Stores the
 *   characters in little-endian UCS-2 representation.  
 */
class CCharmapToLocalUcs2Little: public CCharmapToLocal
{
public:
    /* map a string */
    virtual size_t map_utf8(char *dest, size_t dest_len,
                            utf8_ptr src, size_t src_byte_len,
                            size_t *src_bytes_used) const;

    /* map a null-terminated string */
    virtual size_t map_utf8z(char *dest, size_t dest_len,
                             utf8_ptr src) const;

    /* map a character */
    size_t map(wchar_t unicode_char, char **output_ptr,
               size_t *output_len) const;
};


#endif /* CHARMAP_H */
