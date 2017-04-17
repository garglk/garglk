#charset "us-ascii"
#pragma once

/*
 *   Copyright 2000, 2006 Michael J. Roberts.
 *   
 *   This file is part of TADS 3.
 *   
 *   This header defines the StringComparator intrinsic class.  
 */


/* include our base class definition */
#include "systype.h"

/*
 *   StringComparator intrinsic class.  This class provides support for
 *   dictionaries based on complex string matches, including truncation
 *   (matching an input word to a dictionary word when the input word is at
 *   least some minimum length, and matches the dictionary word up to the
 *   full length of the input word, but the input word is shorter than the
 *   dictionary word); case folding (matching upper-case letters to
 *   lower-case letters and vice versa); and character equivalences (for
 *   matching accented characters to non-accented equivalents, or matching
 *   special characters to multi-character equivalents, such as matching a
 *   German "ess-zet" ("sharp-s") ligature to a pair of lower-case "s"
 *   characters in input).  
 */
intrinsic class StringComparator 'string-comparator/030000': Object
{
    /*
     *   Constructor:
     *   
     *   new StringComparator(truncLen, caseSensitive, mappings)
     *   
     *   truncLen = the minimum truncation length.  An input string that
     *   matches a dictionary string up to the full length of the input
     *   string, and is shorter than the dictionary string but at least this
     *   truncation length, will match the dictionary string.  If truncLen is
     *   zero or nil, no truncated matches are allowed.
     *   
     *   caseSensitive = true if matches are to be sensitive to case, nil if
     *   not.  If this parameter is nil, then an upper-case letter in an
     *   input string will match a lower-case letter in a dictionary string,
     *   and vice versa.  If this parameter is true, each character must
     *   match exactly.
     *   
     *   mappings is a list of equivalent character mappings.  Each mapping
     *   in the list is a sublist in this format:
     *   
     *.     ['dictChar', 'inputString', ucFlags, lcFlags]
     *   
     *   'dictChar' is a one-character string giving the character to be
     *   mapped in dictionary strings.  'inputString' is a string of one or
     *   more characters that is to be considered equivalent to the
     *   dictionary character when the inputString appears in an input
     *   string.  ucFlags and lcFlags are integer values giving the flag
     *   values to bitwise-OR into the results when this mapping is used to
     *   match an upper-case or lower-case input string, respectively.
     *   
     *   For example, a mapping to allow the German ess-zet character (whose
     *   Unicode value is 0x00DF) to match "ss" sequences in input strings,
     *   with no result flag additions, would look like this:
     *   
     *.    ['\u00DF', 'ss', 0, 0]
     *   
     *   Only one mapping is allowed for each dictionary character.  If more
     *   than one mapping is given for a single dictionary character, only
     *   the latest one in the list is actually used.
     *   
     *   Flag values 0x0001 through 0x0080 are reserved for use by
     *   StringComparator itself.  Callers are free to use any flag values
     *   0x0100 and above.  Note that the system flag values are used as
     *   bitwise OR'd values, so callers should not define any flag values
     *   'f' for which (f & 0xFF) != 0.  
     */

    /*
     *   Calculate a hash value.  This returns an integer giving the hash
     *   value for the given string. 
     */
    calcHash(str);

    /*
     *   Match two values.  The first value is the input string, and the
     *   second is the dictionary string.  Each character in the dictionary
     *   string can match the corresponding input string character exactly
     *   (with or without case sensitivity, as specified in our
     *   constructor), or can match the equivalence mapping sequence for the
     *   dictionary character.
     *   
     *   The return value is zero if the values do not match.  If the values
     *   do match, the return value is a non-zero integer, which will be a
     *   bitwise OR combination of all of the flag values applicable to the
     *   match.  This is a combination of pre-defined flag values (see
     *   below) and any flag values from equivalence mappings.  The flag
     *   values from ALL equivalence mappings that were actually used to
     *   make the match are included.  
     */
    matchValues(inputStr, dictStr);
}

/*
 *   Pre-defined matchValues result flags.  These are set when applicable in
 *   the return value of matchValues().
 *   
 *   This class reserves flag values 0x0001 through 0x0080.  Callers should
 *   not use any flag values with any of these bits set.  Even though we
 *   don't define values for all of these flags currently, the ones we don't
 *   use are reserved for possible use in future versions; to ensure
 *   compatibility with future versions, callers should not use any of the
 *   reserved flags for their own purposes.  
 */

/* 
 *   Match - this flag is set in the return code for all matching strings.
 *   (This flag isn't as useless as it might sound; its purpose is to ensure
 *   that the return value from matchValues() is non-zero for all matches,
 *   even when no other flag values are applicable.)
 */
#define StrCompMatch     0x0001

/* 
 *   Case folding - this flag is set when the two values match, but one or
 *   more characters differ in case (in other words, an upper-case letter in
 *   the input string matched a lower-case letter in the dictionary string,
 *   or vice versa).  
 */
#define StrCompCaseFold  0x0002

/* 
 *   Truncation - this flag is set when the input string is shorter than the
 *   value string (but matches the dictionary completely up to the input
 *   string's full length, and is at least as long as the truncation length
 *   specified in the constructor).  This flag can only be returned when
 *   truncation is allowed (as indicated by a non-zero truncation length in
 *   the constructor), because truncated strings will never match at all
 *   when truncation isn't allowed.  
 */
#define StrCompTrunc     0x0004

