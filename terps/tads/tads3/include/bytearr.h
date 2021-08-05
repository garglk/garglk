#charset "us-ascii"
#pragma once

/*
 *   Copyright (c) 2001, 2006 Michael J. Roberts
 *   
 *   This file is part of TADS 3.
 *   
 *   This header defines the ByteArray intrinsic class.  
 */

/* include our base class definition */
#include "systype.h"

/* we need the CharacterSet class for mapToString */
#include "charset.h"

/*
 *   'ByteArray' intrinsic class.  This class provides a fixed-size array of
 *   unsigned 8-bit byte values; each array element is an integer in the
 *   range 0-255.  
 *   
 *   ByteArray is particularly useful for reading and writing binary files,
 *   since it lets you manipulate the raw bytes in a file directly. 
 */
intrinsic class ByteArray 'bytearray/030002': Object
{
    /*
     *   Constructors:
     *   
     *   new ByteArray(length) - create a byte array with the given number of
     *   bytes.  All elements in the new array are initialized to zero.
     *   
     *   new ByteArray(byteArray, startIndex?, length?) - create a new byte
     *   array as a copy of the given byte range of the given byte array,
     *   which must be an object of intrinsic class ByteArray.  If the
     *   starting index and length are not given, the new object is a
     *   complete copy of the source byte array.
     *   
     *   new ByteArray(string, charset?) - create a new byte array by
     *   mapping the given string to bytes.  If 'charset' is provided,
     *   it's a CharacterSet object, or a string giving the name of a
     *   character set; the characters are mapped to bytes using this
     *   character set mapping.  If 'charset' is missing or nil, the
     *   Unicode character codes of the characters are used as the byte
     *   values for the new array; in this mode, the character codes
     *   must each fit into a byte, meaning that they're in the range 0
     *   to 255, or a numeric overflow error will occur.  
     */

    /* 
     *   Get the number of bytes in the array.  The length is fixed at
     *   creation time.  
     */
    length();

    /* 
     *   create a new ByteArray as a copy of the given range of this array;
     *   if the length is not given, bytes from the starting index to the
     *   end of this array are included in the new array
     */
    subarray(startIndex, length?);

    /* 
     *   Copy bytes from the source array into this array.  Bytes are copied
     *   into this array starting at the given index.  The specified number
     *   of bytes are copied from the source array starting at the given
     *   index.  
     */
    copyFrom(src, srcStartIndex, dstStartIndex, length);

    /*
     *   Fill bytes in this array with the given value.  If no starting
     *   index or length values are given, the entire array is filled with
     *   the given byte value.  The byte value must be an integer in the
     *   range 0 to 255. 
     */
    fillValue(val, startIndex?, length?);

    /*
     *   Convert a range of bytes in the array to a string, interpreting the
     *   bytes in the array as characters in the given character set. 
     *   
     *   If the starting index and length are not given, the entire byte
     *   array is converted to a string.  'charset' must be an object of
     *   intrinsic class CharacterSet.  
     */
    mapToString(charset, startIndex?, length?);

    /*
     *   Read an integer value from the byte array.  Reads bytes from the
     *   starting index; the number of bytes read depends on the format.
     *   Returns an integer giving the value read.
     *   
     *   'format' gives the format of the integer to be read.  This is a
     *   combination (using '|' operators) of three constants, giving the
     *   size, byte order, and signedness of the value.
     *   
     *   First, choose the SIZE of the value: FmtSize8, FmtSize16, FmtSize32,
     *   for 8-bit, 16-bit, and 32-bit integers, respectively.
     *   
     *   Second, choose the BYTE ORDER of the value, as represented in the
     *   byte array: FmtLittleEndian or FmtBigEndian.  The standard T3
     *   portable data interchange format uses little-endian values; this is
     *   a format in which the least-significant byte of a value comes first,
     *   followed by the remaining bytes in ascending order of significance.
     *   The big-endian format uses the opposite order.  The byte order
     *   obviously is irrelevant for 8-bit values.
     *   
     *   Third, choose the SIGNEDNESS of the value: FmtSigned or FmtUnsigned.
     *   Note that FmtUnsigned cannot be used with FmtSize32, because T3
     *   itself doesn't have an unsigned 32-bit integer type.
     *   
     *   For example, to read a 16-bit unsigned integer in the standard T3
     *   portable interchange format, you'd use
     *   
     *.    FmtUnsigned | FmtSize16 | FmtLittleEndian
     *   
     *   For convenience, individual macros are also defined that pre-compose
     *   all of the meaningful combinations; see below.
     *   
     *   The byte array must be large enough to read the required number of
     *   bytes starting at the given index.  An "index out of range"
     *   exception is thrown if there aren't enough bytes in the array to
     *   satisfy the request.  
     */
    readInt(startIndex, format);

    /*
     *   Write an integer value to the byte array.  Writes bytes starting at
     *   the given index; the number of bytes written depends on the format.
     *   The 'format' parameter gives the format, using the same codes as in
     *   readInt().  'val' is the integer value to be written.  If 'val' is
     *   outside of the bounds of the format to be written, the written
     *   value is truncated.
     *   
     *   The byte array must be large enough to hold the number of bytes
     *   required by the format starting at the starting index.  An "index
     *   out of range" exception is thrown if the byte array doesn't have
     *   enough space to store the value.
     *   
     *   The value is not checked for range.  If the value is outside of the
     *   range that the format is capable of storing, the value stored will
     *   be truncated to its least significant bits that fit the format.
     *   For example, attempting to store 0xABCD in an 8-bit format will
     *   store only 0xCD.
     *   
     *   Note that the signedness doesn't matter when writing a value.  The
     *   signedness is important only when reading the value back in.  
     */
    writeInt(startIndex, format, val);

    /*
     *   Pack data values into bytes according to a format definition string,
     *   and store the packed bytes in the byte array starting at the given
     *   index.
     *   
     *   'idx' is the starting index in the array for the packed bytes.
     *   'format' is the format string, which specifies the binary
     *   representations to use for the argument values.  The remaining
     *   arguments after 'format' are the data values to pack.
     *   
     *   Returns the number of bytes written to the array.  (More precisely,
     *   returns the final write pointer as a byte offset from 'idx'.  If a
     *   positioning code like @ or X is used in the string, it's possible
     *   that more bytes were actually written.)
     *   
     *   You can also call packBytes a static method, on the ByteArray class
     *   itself:
     *   
     *.      ByteArray.packBytes(format, ...)
     *   
     *   The static version of the method packs the data into bytes the same
     *   way the regular method does, but returns a new ByteArray object
     *   containing the packed bytes.  Note that there's no index argument
     *   with the static version.
     *   
     *   Refer to Byte Packing in the System Manual for details.  
     */
    packBytes(idx, format, ...);

    /*
     *   Unpack bytes from the byte array starting at the given index, and
     *   translate the bytes into data values according to the given format
     *   string.
     *   
     *   'idx' is the starting index in the array for the unpacking, and
     *   'format' is the format string.  Returns a list of the unpacked
     *   values.
     *   
     *   Refer to Byte Packing in the System Manual for details.  
     */
    unpackBytes(idx, format);

    /*
     *   Get the SHA-256 hash of the bytes in the array.  This calculates the
     *   256-bit Secure Hash Algorithm 2 hash value, returning the hash as a
     *   64-character string of hex digits.  The hash value is computed on
     *   the UTF-8 representation of the string.  If 'idx' and 'len' are
     *   specified, they give the range of bytes to include in the hash; the
     *   default is to hash the whole array.  
     */
    sha256(idx?, len?);

    /*
     *   Get the MD5 digest of the string.  This calculates the 128-bit RSA
     *   MD5 digest value, returning the digest as a 32-character string of
     *   hex digits.  The hash value is computed on the UTF-8 representation
     *   of the string.  If 'idx' and 'len' are specified, the give the range
     *   of bytes to include in the hash; the default is to hash the whole
     *   array.  
     */
    digestMD5();
}

/* ------------------------------------------------------------------------ */
/*
 *   Integer format codes, for readInt() and writeInt()
 */

/* integer sizes */
#define FmtSize8              0x0000
#define FmtSize16             0x0001
#define FmtSize32             0x0002

/* integer byte orders */
#define FmtLittleEndian       0x0000
#define FmtBigEndian          0x0010

/* integer signedness */
#define FmtSigned             0x0000
#define FmtUnsigned           0x0100

/* pre-composed integer formats */
#define FmtInt8               (FmtSize8 | FmtSigned)
#define FmtUInt8              (FmtSize8 | FmtUnsigned)

#define FmtInt16LE            (FmtSize16 | FmtLittleEndian | FmtSigned)
#define FmtUInt16LE           (FmtSize16 | FmtLittleEndian | FmtUnsigned)
#define FmtInt16BE            (FmtSize16 | FmtBigEndian | FmtSigned)
#define FmtUInt16BE           (FmtSize16 | FmtBigEndian | FmtUnsigned)

#define FmtInt32LE            (FmtSize32 | FmtLittleEndian | FmtSigned)
#define FmtInt32BE            (FmtSize32 | FmtBigEndian | FmtSigned)

