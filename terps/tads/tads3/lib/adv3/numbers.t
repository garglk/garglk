#charset "us-ascii"

/* 
 *   Copyright 2000, 2006 Michael J. Roberts 
 *   
 *   TADS 3 library - number handling.  This module provides utility
 *   functions for converting numbers to strings in various formats.  
 */

#include "adv3.h"


/* ------------------------------------------------------------------------ */
/*
 *   Format a number as a binary string. 
 */
intToBinary(val)
{
    local result;
    local outpos;

    /* if the value is zero, the binary representation is simply '0' */
    if (val == 0)
        return '0';

    /* allocate a vector to store the characters of the result */
    result = new Vector(33).fillValue(nil, 1, 33);

    /* 
     *   Fill up the vector with 1's and 0's, working from the lowest-order
     *   bit to the highest-order bit.  On each iteration, we'll pull out
     *   the low-order bit, add it to the result string, and then shift the
     *   value right one bit so that the next higher-order bit becomes the
     *   low-order bit in the value.  
     */
    for (outpos = 34 ; val != 0 ; val >>= 1)
    {
        /* 
         *   Add the next bit.  Add it at the end of the string so the
         *   final result reads left-to-right, high-to-low.  Note that
         *   Unicode value 0x30 is the digit '0', and Unicode value 0x31 is
         *   the digit '1'.  We build the result as a vector of Unicode
         *   values for efficiency, so that we don't have to repeatedly
         *   allocate partial strings.  
         */
        result[--outpos] = ((val & 1) == 0 ? 0x30 : 0x31);
    }

    /* convert the vector of Unicode characters to a string */
    return makeString(result.toList(outpos, 34 - outpos));
}

/* ------------------------------------------------------------------------ */
/*
 *   Convert an integer number to Roman numerals.  Returns a string with
 *   the Roman numeral format.  This can only accept numbers in the range
 *   1 to 4999; returns nil for anything outside of this range.  
 */
intToRoman(val)
{
    local str;
    local info =
    [
        /* numeral value / corresponding string */
        1000, 'M',
        900,  'CM',
        500,  'D',
        400,  'CD',
        100,  'C',
        90,   'XC',
        50,   'L',
        40,   'XL',
        10,   'X',
        9,    'IX',
        5,    'V',
        4,    'IV',
        1,    'I'
    ];
    local i;

    /* if the value is outside of the legal range, fail immediately */
    if (val < 1 || val > 4999)
        return nil;

    /* 
     *   iterate over the specifiers and apply each one as many times as
     *   possible 
     */
    for (str = '', i = 1 ; val != 0 ; )
    {
        /* 
         *   If we're greater than the current specifier, apply it;
         *   otherwise, move on to the next specifier. 
         */
        if (val >= info[i])
        {
            /* add this specifier's roman numeral into the result */
            str += info[i+1];

            /* subtract the corresponding value */
            val -= info[i];
        }
        else
        {
            /* move to the next specifier */
            i += 2;
        }
    }

    /* return the result */
    return str;
}

