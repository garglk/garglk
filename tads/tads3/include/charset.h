#charset "us-ascii"

/*
 *   Copyright (c) 2001, 2006 Michael J. Roberts
 *   
 *   This file is part of TADS 3.
 *   
 *   This header defines the CharacterSet intrinsic class.  
 */

#ifndef _CHARSET_H_
#define _CHARSET_H_

/* include our base class definition */
#include "systype.h"

/*
 *   The CharacterSet intrinsic class provides information on character set
 *   translations and can be used to translate between the Unicode character
 *   set that the T3 VM uses internally for string values and the local
 *   character set or sets used for display, keyboard input, and file I/O.  
 */
intrinsic class CharacterSet 'character-set/030001': Object
{
    /*
     *   Constructor:
     *   
     *   new CharacterSet(charsetName) - creates an object to represent the
     *   named local character set.  Certain character set names are
     *   pre-defined:
     *   
     *   us-ascii - the plain 7-bit ASCII character set
     *.  utf-8 - Unicode UTF-8 (a multi-byte unicode encoding)
     *.  utf-16le - little-endian 16-bit Unicode
     *.  utf-16be - big-endian 16-bit Unicode
     *   
     *   In addition, any character set for which the VM has an external
     *   mapping file can be used.  Check your platform-specific T3
     *   installation notes for infomration on how character set mapping
     *   files are implemented on your version of T3.
     *   
     *   A CharacterSet can be created for a non-existent mapping, but the
     *   object cannot be used to perform any mappings; an
     *   UnknownCharacterSetException will be thrown if any mapping is
     *   attempted with a CharacterSet object that has non-existent local
     *   mappings.  You can determine if the local mapping exists with the
     *   isMappingKnown method.  
     */

    /*
     *   Get the name of the character set.  This simply returns the name
     *   that was given to construct the character set. 
     */
    getName();

    /*
     *   Determine if the mapping is known.  This returns true if the
     *   character set has a known local mapping, nil if not.  Note that it
     *   doesn't matter whether or not the character set is actually in use
     *   on the local platform; all that matters is that a T3 mapping file
     *   is available on this machine. 
     */
    isMappingKnown();

    /*
     *   Determine if a character or string of characters is mappable to this
     *   character set.  If the input is an integer, it represents the
     *   Unicode character code for a single character; if the input is a
     *   string, each character in the string is checked.  This returns true
     *   if every character given has a valid mapping in the local character
     *   set, nil if not.  Note that if a string is given, and even one
     *   character is not mappable, this returns nil.  
     */
    isMappable(val);

    /*
     *   Determine if a character or string of characters is "round-trip"
     *   mappable to this character set.  If the input is an integer, it
     *   represents a Unicode character code to be tested; if the input is a
     *   string, each character in the string is tested.  Returns true if
     *   every character given has a valid round-trip mapping, nil if not.
     *   
     *   A character has a round-trip mapping if it can be mapped to this
     *   local character set and then back to Unicode to yield the original
     *   character.  If a character has a round-trip mapping, then in general
     *   the character has an exact representation in the local character set
     *   (as opposed to an approximation: if 'a-umlaut' maps to a simple
     *   unaccented 'a', or to 'ae', then it has only an approximated
     *   representation).  
     */
    isRoundTripMappable(val);
}

#endif /* _CHARSET_H_ */
