#charset "us-ascii"
#pragma once

/*
 *   Copyright 2009 Michael J. Roberts.
 *   
 *   This file is part of TADS 3.
 *   
 *   This module defines the StringBuffer intrinsic class.  StringBuffer is
 *   essentialy a mutable version of String: you can append, insert, replace,
 *   and delete characters in place, without creating new objects as a
 *   result.  This is useful for complex string constructions that involve
 *   many incremental steps, since it avoids the repeated memory allocation
 *   and string copying operations that would be involved if the intermediate
 *   steps were performed with ordinary String objects.  
 */

/*
 *   A StringBuffer is a mutable character string object.  You can insert,
 *   append, delete, and replace characters in the buffer in place.  These
 *   operations don't create new objects as they do with ordinary strings,
 *   but simply modify the existing StringBuffer object's contents.
 *   
 *   The object manages memory automatically.  When you first create the
 *   object, it allocates an initial buffer to hold its character contents.
 *   You can specify the initial buffer size with a constructor argument, or
 *   simply let the object pick a default.  As you add text to the buffer,
 *   the object automatically allocates more memory as needed to accommodate
 *   the added text.  The maximum size for the string contained in the buffer
 *   is about 32000 characters.
 *   
 *   Construction: new StringBuffer() creates a buffer with default initial
 *   size values.  new StringBuffer(length, increment) specifies the initial
 *   buffer size in characters ('length'), and the minimum number of
 *   characters to add to the buffer each time it's automatically expanded
 *   ('increment').
 *   
 *   Passing a StringBuffer to an internal function or method that takes a
 *   String argument, such as tadsSay(), will automatically convert the
 *   object to a string.  To explicitly convert a StringBuffer to an ordinary
 *   String, use the toString() function.  You can also create an ordinary
 *   string from a section of the buffer using the substr() method.  
 */
intrinsic class StringBuffer 'stringbuffer/030000'
{
    /*
     *   Get the length in characters of the current text in the buffer. 
     */
    length();

    /*
     *   Retrieve the Unicode character value of the character at the given
     *   index.  Returns an integer with the Unicode value.  If idx is
     *   negative, it's an index from the end of the string: -1 is the last
     *   character, -2 is the second to last, etc.  
     */
    charAt(idx);
    
    /*
     *   Append text to the current contents of the buffer.  This adds the
     *   new text at the end of the current text.  The value is automatically
     *   converted to a string if possible; this includes numbers and
     *   true and nil values.  
     */
    append(str);

    /*
     *   Insert text into the buffer just before the character at the given
     *   index.  The first character is at index 1, so to insert the new text
     *   before the first current character, insert at index 1.  If the index
     *   is past the end of the current text, this has the same effect as
     *   append().  A negative value indexes from the end of the string.  The
     *   text is automatically converted to a string if possible.  
     */
    insert(txt, idx);

    /*
     *   Copy text into the buffer, starting at the given index (the first
     *   character in the buffer is at index 1).  Overwrites any text
     *   currently in the buffer at this point.  
     */
    copyChars(txt, idx);

    /*
     *   Delete the given text.  This deletes 'len' characters starting at
     *   the given index (the first character is at index 1).  If the length
     *   is omitted, the portion from idx to the end of the string is
     *   deleted.  A negative idx value indexes from the end of the string.
     */
    deleteChars(idx, len?);

    /*
     *   Splice text.  This deletes 'len' characters starting at the given
     *   index (the first character is at index 1), and replaces them with
     *   the given new text.  If the new text is nil, this simply deletes the
     *   old characters without inserting anything new.  If 'len' is zero,
     *   simply inserts the new text without deleting any old text.  A
     *   negative idx value indexes from the end of the string.  The 'str'
     *   value is automatically converted to a string if possible.  
     */
    splice(idx, len, str);

    /*
     *   Retrieve the substring of the buffer starting at the given index and
     *   running for the given character length.  If the length is omitted,
     *   everything from the starting index to the end of the buffer is
     *   included in the result string.  A negative value for 'idx' indexes
     *   from the end of the string.  
     */
    substr(idx, len?);
}
