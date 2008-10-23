#charset "us-ascii"

/*
 *   Copyright (c) 2001, 2006 Michael J. Roberts
 *   
 *   This file is part of TADS 3.
 *   
 *   This header defines the ByteArray intrinsic class.  
 */

#ifndef _BYTEARR_H_
#define _BYTEARR_H_

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
intrinsic class ByteArray 'bytearray/030001': Object
{
    /*
     *   Constructors:
     *   
     *   new ByteArray(length) - create a byte array with the given number
     *   of bytes.  All elements in the new array are initialized to zero.
     *   
     *   new ByteArray(byteArray, startIndex?, length?) - create a new byte
     *   array as a copy of the given byte range of the given byte array,
     *   which must be an object of intrinsic class ByteArray.  If the
     *   starting index and length are not given, the new object is a
     *   complete copy of the source byte array.  
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
     *   bytes in the array as characters in the given character set.  The
     *   resulting string is, of course, a standard T3 string encoded in the
     *   Unicode character set, so the value returned is not dependent upon
     *   the mapping character set.
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
}

/* ------------------------------------------------------------------------ */
/*
 *   Integer format codes 
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

#endif /* _BYTEARR_H_ */
