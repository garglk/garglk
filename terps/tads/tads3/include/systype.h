#charset "us-ascii"
#pragma once

/*
 *   Copyright 2000, 2006 Michael J. Roberts.
 *   
 *   This file is part of TADS 3.
 *   
 *   This module defines the fundamental intrinsic classes, including Object,
 *   String, Collection, List, and Iterator.  
 */


/* ------------------------------------------------------------------------ */
/*
 *   TADS datatype codes.  These values are returned by propType(), etc.  
 */
#define TypeNil         1
#define TypeTrue        2
#define TypeObject      5
#define TypeProp        6
#define TypeInt         7
#define TypeSString     8
#define TypeDString     9
#define TypeList        10
#define TypeCode        11
#define TypeFuncPtr     12
#define TypeNativeCode  14
#define TypeEnum        15
#define TypeBifPtr      16


/* ------------------------------------------------------------------------ */
/*
 *   The root object class.  All objects descend from this class. 
 */
intrinsic class Object 'root-object/030004'
{
    /* 
     *   Determine if I'm an instance or subclass of the given class 'cls'.
     *   Note that x.ofKind(x) returns true - an object is of its own kind.  
     */
    ofKind(cls);

    /* get the list of direct superclasses of this object */
    getSuperclassList();

    /* determine if a property is defined or inherited by this object */
    propDefined(prop, flags?);

    /* get the type of a property defined for this object */
    propType(prop);

    /* 
     *   Get a list of my directly-defined properties.  When called on
     *   intrinsic class objects, this returns a list of properties defined
     *   for instances of the class, as well as static properties of the
     *   class.  
     */
    getPropList();

    /* 
     *   get parameter list information for the given method - returns a
     *   list: [minimumArgc, optionalArgc, varargs], where minimumArgc is
     *   the minimum number of arguments, optionalArgc is the number of
     *   additional optional arguments, and varargs is true if the function
     *   takes a varying number of arguments greater than or equal to the
     *   minimum, nil if not.  
     */
    getPropParams(prop);

    /* 
     *   determine if I'm a "class" object - returns true if the object was
     *   defined with the "class" keyword, nil otherwise 
     */
    isClass();

    /* 
     *   Determine if a property is inherited further from the given object.
     *   definingObj is usually the value of the 'definingobj'
     *   pseudo-variable, and origTargetObj is usually the value of the
     *   'targetobj' pseudo-variable.  
     */
    propInherited(prop, origTargetObj, definingObj, flags?);

    /* determine if this instance is transient */
    isTransient();
}

/*
 *   propDefined() flags 
 */
#define PropDefAny           1
#define PropDefDirectly      2
#define PropDefInherits      3
#define PropDefGetClass      4

/* export the objToString method */
property objToString;
export objToString 'objToString';


/* ------------------------------------------------------------------------ */
/*
 *   The IntrinsicClass intrinsic class.  Objects of this type represent the
 *   intrinsic classes themselves.  
 */
intrinsic class IntrinsicClass 'intrinsic-class/030001': Object
{
    /*
     *   Class method: is the given value an IntrinsicClass object?  This
     *   returns true if so, nil if not.  
     *   
     *   It's not possible to determine if an object is an IntrinsicClass
     *   object using x.ofKind(IntrinsicClass) or via x.getSuperclassList().
     *   This is because those methods traverse the nominal class tree:
     *   [1,2,3] is a List, and List is an Object.  However, List and Object
     *   themselves are represented by IntrinsicClass instances, and it's
     *   occasionally useful to know if you're dealing with such an object.
     *   That's where this method comes in.
     *   
     *   This method returns nil for instances of an intrinsic class.  For
     *   example, isIntrinsicClass([1,2,3]) returns nil, because [1,2,3] is a
     *   List instance.  If you get the superclass list for [1,2,3], though,
     *   that will be [List], and isIntrinsicClass(List) returns true.
     */
    isIntrinsicClass(obj);
}

/*
 *   Intrinsic class modifier object (for internal compiler use only) 
 */
intrinsic class IntrinsicClassModifier 'int-class-mod/030000'
{
}

/* ------------------------------------------------------------------------ */
/*
 *   The native collection type - this is the base class for lists, vectors,
 *   and other objects that represent collections of values.  
 */
intrinsic class Collection 'collection/030000': Object
{
    /* 
     *   Create an iterator for the collection.  This returns a new Iterator
     *   object that can be used to iterate over the values in the
     *   collection.  The Iterator will use a snapshot of the collection that
     *   will never change, even if the collection is changed after the
     *   iterator is created.  
     */
    createIterator();

    /*
     *   Create a "live iterator" for the collection.  This returns a new
     *   Iterator object that refers directly to the original collection; if
     *   the original collection changes, the iterator will reflect the
     *   changes in its iteration.  As a result, the iterator is not
     *   guaranteed to visit all of the elements in the collection if the
     *   collection changes during the course of the iteration.  If
     *   consistent results are required, use createIterator() instead.  
     */
    createLiveIterator();
}

/* ------------------------------------------------------------------------ */
/*
 *   The native iterator type - this is the base class for all iterators.
 *   This class is abstract and is thus never directly instantiated.
 *   
 *   Note that iterators can never be created directly with the 'new'
 *   operator.  Instead, iterators must be obtained from a collection via the
 *   collection's createIterator() method.  
 */
intrinsic class Iterator 'iterator/030001': Object
{
    /*
     *   Get the next item in the collection.  This returns the next item's
     *   value, and advances the internal state in the iterator so that a
     *   subsequent call to getNext() returns the next item after this one.
     *   When the iterator is first created, or after calling
     *   resetIterator(), this returns the first item in the collection.  
     */
    getNext();

    /*
     *   Determine if the collection is out of items.  Returns true if
     *   getNext() will return a valid item, nil if no more items are
     *   available.  
     */
    isNextAvailable();

    /*
     *   Reset to the first item.  After calling this routine, the next call
     *   to getNext() will return the first item in the collection.  
     */
    resetIterator();

    /*
     *   Get the current key.  This returns the value of the key for the
     *   current item in the collection.  For an indexed collection, this
     *   returns the index value; for a keyed collection, this returns the
     *   current key value.  
     */
    getCurKey();

    /*
     *   Get the current value.  This returns the value of the current item
     *   in the collection. 
     */
    getCurVal();
}

/*
 *   Indexed object iterator - this type of iterator is used for lists,
 *   vectors, and other indexed collection objects.  
 */
intrinsic class IndexedIterator 'indexed-iterator/030000': Iterator
{
}


/* ------------------------------------------------------------------------ */
/*
 *   AnonFuncPtr depends on Vector 
 */
#include "vector.h"

/*
 *   Anonymous function pointer intrinsic class 
 */
intrinsic class AnonFuncPtr 'anon-func-ptr': Vector
{
}


/* ------------------------------------------------------------------------ */
/*
 *   The "TADS Object" intrinsic class.  All objects that the program
 *   defines with the "class" or "object" statements descend from this
 *   class.  
 */
intrinsic class TadsObject 'tads-object/030005': Object
{
    /* 
     *   Create an instance of this object: in other words, create a new
     *   object whose superclass is this object.  The arguments provided are
     *   passed to the new object's constructor.  This method returns a
     *   reference to the new object.  
     */
    createInstance(...);

    /*
     *   Create a clone of this object.  This creates an exact copy, with
     *   the same property values, as the original.  This does not call any
     *   constructors; it merely instantiates an exact copy of the original.
     *   
     *   Note that the clone is a "shallow" copy, which means that any
     *   objects it references are not themselves cloned.
     */
    createClone();

    /*
     *   Create a transient instance of this object.  This works just like
     *   createInstance(), but creates a transient instance instead of an
     *   ordinary (persistent) instance.  
     */
    createTransientInstance(...);

    /*
     *   Create an instance of an object based on multiple superclasses.
     *   Each argument gives a superclass, and optionally arguments for
     *   invoking the superclass constructor.  If an argument is given as
     *   simply a class, then we don't invoke that superclass's constructor;
     *   if the argument is given as a list, the first element of the list is
     *   the class, and the remaining elements of the list are arguments for
     *   that superclass's constructor.  The arguments are specified in the
     *   same order they would be to define the object, so the first argument
     *   is the dominant superclass.
     *   
     *   For example, suppose we created a class definition like this:
     *   
     *   class D: A, B, C
     *.    construct(x, y)
     *.    {
     *.      inherited A(x);
     *.      inherited C(y);
     *.    }
     *.  ;
     *   
     *   We could obtain the same effect dynamically like so:
     *   
     *   local d = TadsObject.createInstanceOf([A, x], B, [C, y]);
     *   
     *   Note that only *actual* lists are interpreted as constructor
     *   invokers here.  A list-like object (with operator[] and length()
     *   methods) will be treated as a simple superclass, since otherwise it
     *   wouldn't be possible to specify the no-constructor format for such a
     *   superclass.  
     */
    static createInstanceOf(...);

    /*
     *   Create a transient instance based on multiple superclasses.  This
     *   works just like createInstanceOf(), but creates a transient
     *   instance. 
     */
    static createTransientInstanceOf(...);

    /*
     *   Set the superclass list.  scList is a list giving the new
     *   superclasses.  The superclasses must all be TadsObject objects, with
     *   one exception: the list [TadsObject] may be passed to create an
     *   object based directly on TadsObject.  No other intrinsic classes can
     *   be used in the list, and objects of other types cannot be used in
     *   the list.  
     */
    setSuperclassList(scList);

    /* 
     *   Get a method value.  If the property is a method, this returns a
     *   function pointer to the method; this does NOT evaluate the method.
     *   If the property is not a method, this returns nil.
     *   
     *   The returned function pointer can be called like an ordinary
     *   function, but such a call will have no 'self' value, so the
     *   disembodied method won't be able to refer to properties or methods
     *   of 'self'.  The main use of this method is to get a method of one
     *   object to assign as a method of another object using setMethod().  
     */
    getMethod(prop);

    /*
     *   Set a method value.  Assigns the given function (which must be a
     *   function pointer value) to the given property of 'self'.  This
     *   effectively adds a new method to the object.
     *   
     *   The function can be an ordinary named function, or a method pointer
     *   retrieved from this object or from another object with getMethod().
     *   Anonymous functions are NOT allowed here.  
     */
    setMethod(prop, func);
}


/* ------------------------------------------------------------------------ */
/* 
 *   We need CharacterSet and ByteArray (for String.mapToByteArray).  (But
 *   wait to include these until after we've defined Object, since everything
 *   depends on Object.)  
 */
#include "charset.h"
#include "bytearr.h"

/* ------------------------------------------------------------------------ */
/*
 *   The native string type. 
 */
intrinsic class String 'string/030008': Object
{
    /* get the length of the string */
    length();

    /* extract a substring */
    substr(start, len?);

    /* convert to upper case */
    toUpper();

    /* convert to lower case */
    toLower();

    /* 
     *   Find a substring or pattern within the subject string (self),
     *   searching the string from left to right returning the index of the
     *   first match found.  If 'str' is a string, this searches for an exact
     *   match to the substring.  If 'str' is a RexPattern object, this
     *   searches for a match to the pattern.  Returns the character index of
     *   the start of the match if found (the first character is at index 1),
     *   or nil if no match is found.
     *   
     *   'index' is the optional starting index for the search.  the first
     *   character is at index 1; a negative index specifies an offset from
     *   the end of the string, with -1 indicating the last character, -2 the
     *   second to last, and so on.  If 'index' is omitted, the search starts
     *   at the first character.  Note that the search proceeds from left to
     *   right even if 'index' is negative - a negative starting index is
     *   just a convenience to specify an offset from the end of the string,
     *   but the search still proceeds in the same direction.
     *   
     *   (Note: "left to right" in this context simply means from lower to
     *   higher character index in the string.  We're using the term loosely,
     *   in particular ignoring anything related to the reading order or
     *   display direction for different languages or scripts.)
     */
    find(str, index?);

    /* 
     *   convert to a list of Unicode character codes, or get the Unicode
     *   character code for the single character at the given index 
     */
    toUnicode(idx?);

    /* htmlify a string */
    htmlify(flags?);

    /* determine if we start with the given string */
    startsWith(str);

    /* determine if we end with the given string */
    endsWith(str);

    /* 
     *   Map to a byte array, converting to the given character set.  If
     *   'charset' is provided, it must be an object of intrinsic class
     *   CharacterSet, or a string giving the name of a character set.  The
     *   characters in the string are mapped from the internal Unicode
     *   representation to the appropriate byte representation in the given
     *   character set.  Any unmappable characters are replaced with the
     *   usual default/missing character for the set, as defined by the
     *   mapping.
     *   
     *   If 'charset' is omitted or nil, the byte array is created simply by
     *   treating the Unicode character code of each character in the string
     *   as a byte value.  A byte can only hold values from 0 to 255, so a
     *   numeric overflow error is thrown if any character code in the source
     *   string is outside this range.
     *   
     */
    mapToByteArray(charset?);

    /* 
     *   Replace one occurrence or all occurrences of the given substring
     *   with the given new string.
     *   
     *   'self' is the subject string, which we search for instances of the
     *   replacement.
     *   
     *   'origStr' is the string to search for within 'self'.  This is
     *   treated as a literal text substring to find within 'self'.
     *   'origStr' can alternatively be a RexPattern object, in which case
     *   the regular expression is matched.
     *   
     *   'newStr' is the replacement text, as a string.  'newStr' can
     *   alternatively be a function (regular or anonymous) instead of a
     *   string.  In this case, it's invoked as 'newStr(match, index, orig)'
     *   for each match where 'match' is the matching text, 'index' is the
     *   index within the original subject string of the match, and 'orig' is
     *   the full original subject string.  This function must return a
     *   string value, which is used as the replacement text.  Using a
     *   function allows greater flexibility in specifying the replacement,
     *   since it can vary the replacement according to the actual text
     *   matched and its position in the subject string.
     *   
     *   'flags' is a combination of ReplaceXxx flags specifying the search
     *   options.  It's optional; if omitted, the default is ReplaceAll.
     *   
     *   ReplaceOnce and ReplaceAll are mutually exclusive; they mean,
     *   respectively, that only the first occurrence of the match should be
     *   replaced, or that every occurrence should be replaced.  ReplaceOnce
     *   and ReplaceAll are ignored if a 'limit' value is specified (this is
     *   true even if 'limit' is nil, which means that all occurrences are
     *   replaced).
     *   
     *   'index' is the starting index within 'self' for the search.  If this
     *   is given, we'll ignore any matches that start before the starting
     *   index.  If 'index' is omitted, we start the search at the beginning
     *   of the string.  If 'index' is negative, it's an index from the end
     *   of the string: -1 is the last character, -2 the second to last, etc.
     *   
     *   'origStr' can be given as a list of search strings, rather than a
     *   single string.  In this case, we'll search for each of the strings
     *   in the list, and replace each one with 'newStr'.  If 'newStr' is
     *   also a list, each match to an element of the 'origStr' list is
     *   replaced with the corresponding element (at the same index) of the
     *   'newStr' list.  If there are more 'origStr' elements than 'newStr'
     *   elements, each match to an excess 'origStr' element is replaced with
     *   an empty string.  This allows you to perform several replacements
     *   with a single call.
     *   
     *   'limit', if specified, is an integer indicating the maximum number
     *   of matches to replace, or nil to replace all matches.  If the limit
     *   is reached before all matches have been replaced, no further
     *   replacements are performed.  If this parameter is specified, it
     *   overrides any ReplaceOnce or ReplaceAll flag.
     *   
     *   There are two search modes when 'origStr' is a list.  The default is
     *   "parallel" mode.  In this mode, we search for all of the 'origStr'
     *   elements, and replace the leftmost match.  We then search the
     *   remainder of the string, after this first match, again searching for
     *   all of the 'origStr' elements.  Again we replace the leftmost match.
     *   We repeat this until we run out of matches.
     *   
     *   The other option is "serial" mode, which you select by including
     *   ReplaceSerial in the flags argument.  In serial mode, we start by
     *   searching only for the first 'origStr' element.  We replace each
     *   occurrence throughout the string (unless we're in ReplaceOnce mode,
     *   in which case we stop after the first replacement).  If we're in
     *   ReplaceOnce mode and we did a replacement, we're done.  Otherwise,
     *   we start over with the updated string, containing the replacements
     *   so far, and search it for the second 'origStr' element, replacing
     *   each occurrence (or just the first, in ReplaceOnce mode).  We repeat
     *   this for each 'origStr' element.
     *   
     *   The key difference between the serial and parallel modes is that the
     *   serial mode re-scans the updated string after replacing each
     *   'origStr' element, so replacement text could itself be further
     *   modified.  Parallel mode, in contrast, never re-scans replacement
     *   text.  
     */
    findReplace(origStr, newStr, flags?, index?, limit?);

    /*
     *   Splice: delete 'del' characters starting at 'idx', and insert the
     *   string 'ins' in their place.  'ins' is optional; if omitted, this
     *   simply does the deletion without inserting anything.  
     */
    splice(idx, del, ins?);

    /*
     *   Split the string into substrings at the given delimiter, or of a
     *   given fixed length.
     *   
     *   'delim' is the delimiter.  It can be one of the following:
     *   
     *   - A string or RexPattern, giving the delimiter where we split the
     *   string.  We search 'self' for matches to this string or pattern, and
     *   split it at each instance we find, returning a list of the resulting
     *   substrings.  For example, 'one,two,three'.split(',') returns the
     *   list ['one', 'two', 'three'].  The delimiter separates parts, so
     *   it's not part of the returned substrings.
     *   
     *   - An integer, giving a substring length.  We split the string into
     *   substrings of this exact length (except that the last element will
     *   have whatever's left over).  For example, 'abcdefg'.split(2) returns
     *   ['ab', 'cd', 'ef', 'g'].
     *   
     *   If 'delim' is omitted or nil, the default is 1, so we'll split the
     *   string into one-character substrings.
     *   
     *   If 'limit' is included, it's an integer giving the maximum number of
     *   elements to return in the result list.  If we reach the limit, we'll
     *   stop the search and return the entire rest of the string as the last
     *   element of the result list.  If 'limit' is 1, we simply return a
     *   list consisting of the source string, since a limit of one element
     *   means that we can't make any splits at all.  
     */
    split(delim?, limit?);

    /*
     *   Convert special characters and TADS markups to standard HTML
     *   markups.  Returns a new string with the contents of the 'self'
     *   string processed as described below.
     *   
     *   'stateobj' is an object containing the state of the output stream.
     *   This allows an output stream to process its contents a bit at a
     *   time, by maintaining the state of the stream from one call to the
     *   next.  This object gives the prior state of the stream on entry, and
     *   is updated on return to contain the new state after processing this
     *   string.  If this is omitted or nil, a default initial starting state
     *   is used.  The function uses the following properties of the object:
     *   
     *   stateobj.flags_ is an integer with a collection of flag bits giving
     *   the current line state
     *   
     *   stateobj.tag_ is a string containing the text of the tag currently
     *   in progress.  If the string ends in the middle of a tag, this will
     *   be set on return to the text of the tag up to the end of the string,
     *   so that the next call can resume processing the tag where the last
     *   call left off.
     *   
     *   
     *   The function makes the following conversions:
     *   
     *   \n -> <BR>, or nothing at the start of a line
     *   
     *   \b -> <BR> at the start of a line, or <BR><BR> within a line
     *   
     *   \ (quoted space) -> &nbsp; if followed by a space or another quoted
     *   space, or an ordinary space if followed by a non-space character
     *   
     *   \t -> a sequence of &nbsp; characters followed by a space, padding
     *   to the next 8-character tab stop.  This can't take into account the
     *   font metrics, since that's determined by the browser, so it should
     *   only be used with a monospaced font.
     *   
     *   \^ -> sets an internal flag to capitalize the next character
     *   
     *   \v -> sets an internal flag to lower-case the next character
     *   
     *   <Q> ... </Q> -> &ldquo; ... &rdquo; or &lsquo; ... &rsquo;,
     *   depending on the nesting level
     *   
     *   <BR HEIGHT=N> -> N <BR> tags if at the start of a line, N+1 <BR>
     *   tags if within a line
     *   
     *   
     *   Note that this isn't a general-purpose HTML corrector: it doesn't
     *   correct ill-formed markups or standardize deprecated syntax or
     *   browser-specific syntax.  This function is specifically for
     *   standardizing TADS-specific syntax, so that games can use the
     *   traditional TADS syntax with the Web UI.  
     */
    specialsToHtml(stateobj?);

    /*
     *   Convert special characters and HTML markups to plain text, as it
     *   would appear if written out through the regular console output
     *   writer and displayed on a plain text terminal.  Returns a new string
     *   with the contents of the 'self' string processed as described below.
     *   This works very much like specialsToHtml(), but rather than
     *   generating standard HTML output, we generate plain text output.
     *   
     *   'stateobj' has the same meaning asin specialsToHtml().  
     *   
     *   The function makes the following conversions:
     *   
     *   \n -> \n, or nothing at the start of a line
     *   
     *   \b -> \n at the start of a line, or \n\n within a line
     *   
     *   \ (quoted space) -> regular space
     *   
     *   \^ -> sets an internal flag to capitalize the next character
     *   
     *   \v -> sets an internal flag to lower-case the next character
     *   
     *   <Q> ... </Q> -> "..." or '...' depending on the quote nesting level
     *   
     *   <BR HEIGHT=n> -> N \n characters at the start of a line, N+1 \n
     *   characters within a line
     *   
     *   <P> -> \n at the start of a line, \n\n within a line
     *   
     *   <TAG> -> nothing for all other tags
     *   
     *   &amp; -> &
     *   
     *   &lt; -> <
     *   
     *   &gt; -> >
     *   
     *   &quot; -> "
     *   
     *   &ldquo; and &rdquo; -> "
     *   
     *   &lsquo; and &rsquo; -> '
     *   
     *   &#dddd; -> Unicode character dddd 
     */
    specialsToText(stateobj?);

    /*
     *   Encode a URL parameter string.  Spaces are encoded as "+", and all
     *   other non-alphanumeric characters except - and _ are encoded as
     *   "%xx" sequences.  
     */
    urlEncode();
    
    /*
     *   Decode a URL parameter string.  This reverses the effect of
     *   urlEncode(), returning a string with the encodings translated back
     *   into ordinary characters.  Any sequences that do not form valid
     *   UTF-8 characters are converted to '?'.  
     */
    urlDecode();

    /*
     *   Get the SHA-256 hash of the string.  This calculates the 256-bit
     *   Secure Hash Algorithm 2 hash value, returning the hash as a
     *   64-character string of hex digits.  The hash value is computed on
     *   the UTF-8 representation of the string.  
     */
    sha256();

    /*
     *   Get the MD5 digest of the string.  This calculates the 128-bit RSA
     *   MD5 digest value, returning the digest as a 32-character string of
     *   hex digits.  The hash value is computed on the UTF-8 representation
     *   of the string. 
     */
    digestMD5();

    /*
     *   Pack the arguments into bytes, and create a new string from the byte
     *   values.  The characters of the new string correspond to the packed
     *   byte values, so each character will have a Unicode character number
     *   from 0 to 255.
     *   
     *   'format' is a format string describing the packed formats for the
     *   values, and the rest of the arguments are the values to pack into
     *   the string.  Returns a string containing the packed bytes.
     *   
     *   See Byte Packing in the System Manual for more details.  
     */
    static packBytes(format, ...);

    /*
     *   Unpack this string, interpreting the characters in the string as
     *   byte values, and unpacking the bytes according to the format string.
     *   Each character in the string must have a Unicode character number
     *   from 0 to 255; if any characters are outside this range, an error is
     *   thrown.
     *   
     *   This method can be used to unpack a string created with
     *   String.packBytes().  In most cases, using the same format string
     *   that was used to pack the bytes will re-create the original values.
     *   This method can also be convenient for parsing plain text that's
     *   arranged into fixed-width fields.
     *   
     *   'format' is the format string describing the packed byte format.
     *   Returns a list consisting of the unpacked values.
     *   
     *   See Byte Packing in the System Manual for more details.  
     */
    unpackBytes(format);

    /*
     *   Convert each character in the string to title case, according to the
     *   Unicode character database definitions.  Returns a new string with
     *   the title-case version of this string.
     *   
     *   For most characters, title case is the same as upper case.  It
     *   differs from upper case when a single Unicode character represents
     *   more than one printed letter, such as the German sharp S U+00DF, or
     *   the various "ff" and "fi" ligatures.  In these cases, the title-case
     *   conversion consists of the upper-case version of the first letter of
     *   the ligature followed by the lower-case versions of the remaining
     *   characters.  Unicode doesn't define single-character title-case
     *   equivalents of most of the ligatures, so the result is usually a
     *   sequence of the individual characters making up the ligature.  For
     *   example, title-casing the 'ffi' ligature character (U+FB03) produces
     *   the three-character sequence 'F', 'f', 'i'.
     *   
     *   Note that this routine converts every character in the string to
     *   title case, so it's not by itself a full title formatter - it's
     *   simply a character case converter.
     */
    toTitleCase();

    /*
     *   Convert the string to "folded" case.  Returns a new string with the
     *   case-folded version of this string.
     *   
     *   Folded case is used for eliminating case distinctions in sets of
     *   strings, to allow for case-insensitive comparisons, sorting, etc.
     *   This routine produces the case folding defined in the Unicode
     *   character database; in most cases, the result is the same as
     *   converting each original character to upper case and then converting
     *   the result to lower case.  This process not only removes case
     *   differences but also normalizes some variations in the ways certain
     *   character sequences are rendered, such as converting the German
     *   ess-zed U+00DF to "ss", and converting lower-case accented letters
     *   that don't have single character upper-case equivalents to the
     *   corresponding series of composition characters (e.g., U+01F0, a
     *   small 'j' with a caron, turns into U+006A + U+030C, a regular small
     *   'j' followed by a combining caron character).  Without this
     *   normalization, the upper- and lower-case renderings of such
     *   characters wouldn't match.
     */
    toFoldedCase();

    /*
     *   Compare this string to another string, using Unicode character code
     *   points as the collation order.  Returns an integer less than zero if
     *   this string sorts before the other string, zero if they're
     *   identical, or greater than zero if this string sorts after the
     *   other.  This makes the result suitable for use in a sort() callback,
     *   for example.
     *   
     *   Note that if you use the result for sorting text that includes
     *   accented Roman letters, the result order probably won't match the
     *   dictionary order for your language.  Different languages (and even
     *   different countries/cultures sharing a language) have different
     *   rules for dictionary ordering, so collation is inherently language-
     *   specific.  This routine doesn't take language differences into
     *   account; it simply uses the fixed Unicode code point ordering.  The
     *   Unicode ordering for accented characters is somewhat arbitrary,
     *   because the accented Roman characters are arranged into multiple
     *   disjoint blocks that are all separate from the unaccented
     *   characters.  For example, A-caron sorts after Z-caron, which sorts
     *   after A-breve, which sorts after Y-acute, which sorts after A-acute,
     *   which sorts after plain Z. 
     */
    compareTo(str);

    /*
     *   Compare this string to another string, ignoring case.  The two
     *   strings are compared on the basis of the "case folded" versions of
     *   their characters, using the case folding rules from the Unicode
     *   standard (the case folded version of a string is the value returned
     *   by toFoldedCase()).  The return alue is an integer less than zero if
     *   this string sorts before the other string, zero if they're
     *   identical, or greater than zero if this string sorts after the
     *   other. 
     *   
     *   As with compareTo(), this only produces alphabetically correct
     *   sorting order when comparing ASCII strings.
     */
    compareIgnoreCase(str);

    /*
     *   Find the last instance of a substring or pattern within the string,
     *   searching the subject string (self) from right to left (that is,
     *   from the end of the string towards the beginning).  This works like
     *   find(), but searches in the reverse direction.  Returns the index of
     *   the match, or nil if no match is found.
     *   
     *   'str' can be a string or a RexPattern.  If it's a string, we look
     *   for an exact match to the substring.  If it's a RexPattern, we
     *   search for a match to the pattern.
     *   
     *   The optional 'index' specifies the starting index for the search.
     *   This is the index of the character AFTER the last character that's
     *   allowed to be included in the match.  The first character of the
     *   string is at index 1; a negative index indicates an offset from the
     *   end of the string, so -1 is the last character.  0 has the special
     *   meaning of the end of the string, just past the last character in
     *   the string, so you can use 0 (or, equivalently, self.length()+1) to
     *   search the entire string from the end.  For a repeated search, pass
     *   the index of the previous match, since this will find the next
     *   earlier matching substring that doesn't overlap with the previous
     *   match.
     *   
     *   (Note: "right to left" in this context simply means that the search
     *   runs from higher to lower character index in the string.  We're
     *   using the term loosely, in particular ignoring anything related to
     *   the reading order or display direction for different languages or
     *   scripts.)
     */
    findLast(str, index?);

    /*
     *   Find all occurrences of substring or pattern within a string,
     *   returning a list of the results.
     *   
     *   'str' can be a string or RexPattern object.  If it's a string, we
     *   look for exact matches to the substring.  If it's a RexPattern, we
     *   search for matches to the pattern.
     *   
     *   'func' is an optional function used to process the results.  If this
     *   is provided, the function is called once for each match found in the
     *   string.  The function's return value is then placed into the result
     *   list for the overall findAll() result.  The function is called for
     *   each match found as follows: func(match, idx, g1, g2, ...), where
     *   'match' is a string giving the text of the match, 'idx' is an
     *   integer giving the index in the string (self) of the first character
     *   of the match, and the 'gN' arguments are strings giving the text of
     *   the correspondingly numbered capture groups (the parenthesized
     *   groups in a regular expression match).  All of the 'func' arguments
     *   except 'match' are optional: the function can omit them, in which
     *   case the caller doesn't pass them.  If 'func' specifies more 'gN'
     *   capture group parameters than were actually found in the match, nil
     *   is passed for each extra parameter.
     *   
     *   The return value is a list of the results.  If no occurrences of
     *   'str' are found, the result is an empty list.  If 'func' is
     *   specified, each element in the return list is the return value from
     *   calling 'func' for the corresponding match; if 'func' is omitted,
     *   each element in the return list is a string with the text of that
     *   match.
     */
    findAll(str, func?);

    /*
     *   Match the given string or RexPattern object to this string value,
     *   starting at the start of the string or at the given index, if
     *   specified.  If 'str' is a string, we check for a match to the
     *   literal text of the string; if 'str' is a RexPattern, we try to
     *   match the regular expression.  'idx' is the optional starting
     *   position (1 is the first character; negative values are from the end
     *   of the string, with -1 as the last character, -2 as the second to
     *   last, etc).  If 'idx' is omitted, the default is the start of the
     *   string.
     *   
     *   Returns an integer giving the length of the match found if the
     *   string matches 'str', or nil if there's no match.
     *   
     *   The difference between this method and find() is that this method
     *   only checks for a match at the given starting position, without
     *   searching any further in the string, whereas find() searches for a
     *   match at each subsequent character of the string until a match is
     *   found or the string is exhausted.
     */
    match(str, idx?);
}

/*
 *   Flags for String.htmlify 
 */

/* 
 *   translate spaces - each space in a run of multiple spaces is converted
 *   to an &nbsp; sequence 
 */
#define HtmlifyTranslateSpaces    0x0001

/* translate newlines - converts each \n character to a <br> tag */
#define HtmlifyTranslateNewlines  0x0002

/* translate tabs - converts each \t character to a <tab> tag */
#define HtmlifyTranslateTabs      0x0004

/* 
 *   Translate all whitespace characters - translate all spaces, tabs, and
 *   newlines into their HTML equivalents: each space character becomes an
 *   '&nbsp sequence;', each '\n' character becomes a <br> tag; and each
 *   '\t' character becomes a <tab> tag.  
 */
#define HtmlifyTranslateWhitespace \
    (HtmlifyTranslateSpaces | HtmlifyTranslateNewlines | HtmlifyTranslateTabs)


/*
 *   Flags for String.findReplace 
 */
#define ReplaceAll         0x0001
#define ReplaceIgnoreCase  0x0002
#define ReplaceFollowCase  0x0004
#define ReplaceSerial      0x0008
#define ReplaceOnce        0x0010

/* property exports for specialsToHtml's state object */
property flags_, tag_;
export flags_ 'String.specialsToHtml.flags';
export tag_ 'String.specialsToHtml.tag';


/* ------------------------------------------------------------------------ */
/*
 *   The native list type 
 */
intrinsic class List 'list/030008': Collection
{
    /* 
     *   Select a subset of the list: returns a new list consisting only
     *   of the elements for which the callback function 'func' returns
     *   true.  
     */
    subset(func);

    /*
     *   Apply the callback function to each element of this list, and
     *   return a new list consisting of the results.  Effectively maps
     *   the list to a new list using the given function.  Suppose the
     *   original list is
     *   
     *   [x, y, z]
     *   
     *   Then the result list is
     *   
     *   [func(x), func(y), func(z)] 
     */
    mapAll(func);

    /* get the number of elements in the list */
    length();

    /* extract a sublist */
    sublist(start, len?);

    /* intersect with another list */
    intersect(other);

    /* get the index of the first match for the given value */
    indexOf(val);

    /* car/cdr - head/tail of list */
    car();
    cdr();

    /* 
     *   Find the first element for which the given condition is true, and
     *   return the index of the element.  Applies the callback function
     *   (which encodes the condition to evaluate) to each element in
     *   turn, starting with the first.  For each element, if the callback
     *   returns nil, proceeds to the next element; otherwise, stops and
     *   returns the index of the element.  If the callback never returns
     *   true for any element, we'll return nil.  
     */
    indexWhich(cond);

    /* 
     *   Invoke the callback func(val) on each element, in order from first
     *   to last.  No return value.  
     */
    forEach(func);

    /* 
     *   Find the first element for which the given condition is true, and
     *   return the value of the element.  Returns nil if no item
     *   satisfies the condition.  
     */
    valWhich(cond);

    /* find the last element with the given value, and return its index */
    lastIndexOf(val);

    /* 
     *   Find the last element for which the condition is true, and return
     *   the index of the element.  Applies the callback to each element
     *   in turn, starting with the last element and working backwards.
     *   For each element, if the callback returns nil, proceeds to the
     *   previous element; otherwise, stops and returns the index of the
     *   element.  If the callback never returns true for any element,
     *   we'll return nil.  
     */
    lastIndexWhich(cond);

    /* 
     *   Find the last element for which the condition is true, and return
     *   the value of the element 
     */
    lastValWhich(cond);

    /* count the number of elements with the given value */
    countOf(val);

    /* count the number of elements for which the callback returns true */
    countWhich(cond);

    /* get a new list consisting of the unique elements of this list */
    getUnique();

    /* 
     *   append the elements of the list 'lst' to the elements of this
     *   list, then remove repeated elements in the result; returns a new
     *   list with the unique elements of the combination of the two lists 
     */
    appendUnique(lst);

    /* 
     *   append an element - this works almost exactly like the
     *   concatation operator ('+'), but if the argument is a list, this
     *   simply adds the list as a new element, rather than adding each
     *   element of the list as a separate element 
     */
    append(val);

    /* 
     *   Sort the list, returning a new list.  If the 'descending' flag is
     *   provided and is not nil, we'll sort the list in descending order
     *   rather than ascending order.
     *   
     *   If the 'comparisonFunction' argument is provided, it must be a
     *   callback function; the callback takes two arguments, and returns
     *   an integer less than zero if the first argument value is less
     *   than the second, zero if they're equal, and an integer greater
     *   than zero if the first is greater than the second.  If no
     *   'comparisonFunction' argument is provided, or it's provided and
     *   its value is nil, we'll simply compare the list elements as
     *   ordinary values.  The comparison function can be provided for
     *   caller-defined orderings, such as ordering a set of objects.  
     */
    sort(descending?, comparisonFunction?);

    /*
     *   Prepend an element - this inserts the value before the first
     *   existing element. 
     */
    prepend(val);

    /* 
     *   Insert one or more elements at the given index.  If the starting
     *   index is 1, the elements will be inserted before the first existing
     *   element.  If the index is one higher than the number of elements,
     *   the elements will be inserted after all existing elements.  If the
     *   index is negative, it counts backwards from the end of the list: -1
     *   inserts before the last element, -2 inserts before the second to
     *   last, and so on.  If the index is zero, the new elements are
     *   inserted after the last element.
     *   
     *   Note that a list value argument will simply be inserted as a single
     *   element.
     *   
     *   Returns a new list with the value(s) inserted.  
     */
    insertAt(startingIndex, val, ...);

    /*
     *   Delete the element at the given index, reducing the length of the
     *   list by one element.  If the index is negative, it counts from the
     *   end of the list: -1 is the last element, -2 is the second to last,
     *   etc.  Returns a new list with the given element removed.  
     */
    removeElementAt(index);

    /*
     *   Delete the range of elements starting at startingIndex and ending at
     *   endingIndex.  The elements at the ends of the range are included in
     *   the deletion.  If startingIndex == endingIndex, only one element is
     *   removed.  If either index is negative, it counts from the end of the
     *   list: -1 is the last element, -2 is the second to last, etc.
     *   Returns a new list with the given element range removed.  
     */
    removeRange(startingIndex, endingIndex);

    /*
     *   Invoke the callback func(index, val) on each element, in order from
     *   first to last.  No return value.  
     */
    forEachAssoc(func);

    /*
     *   Class method: Generate a new list.  'func' is a callback function,
     *   which can take zero or one argument.  'n' is the number of elements
     *   for the new list.  For each element, 'func' is invoked as func() if
     *   it takes no arguments, or func(idx) if it takes one argument, where
     *   'idx' is the index of the element being generated.  The return value
     *   of the call to 'func' is stored as the list element.  The method
     *   returns the resulting list.  For example, a list of the first ten
     *   even positive integers: List.generate({i: i*2}, 10).  
     */
    static generate(func, n);

    /*
     *   Splice new values into the list.  Deletes the 'del' list items
     *   starting at 'idx', then inserts the extra arguments in their place.
     *   Returns a new list reflecting the spliced values.  To insert items
     *   without deleting anything, pass 0 for 'del'.  To delete items
     *   without inserting anything, omit any additional arguments.  
     */
    splice(idx, del, ...);

    /*
     *   Combine the list elements into a string.  This converts each element
     *   into a string value using the usual default conversions (or throws
     *   an error if string conversion isn't possible), then concatenates the
     *   values together and returns the result.  If 'separator' is provided,
     *   it's a string that's interposed between elements; if this is
     *   omitted, the elements are concatenated together with no extra
     *   characters in between.  
     */
    join(sep?);

    /*
     *   Get the index of the list item with the minimum value.  If 'func' is
     *   missing, this simply returns the index of the list element with the
     *   smallest value, comparing the element values as though using the '>'
     *   and '<' operators.  If 'func' is specified, it must be a function;
     *   it's called as func(x) for each value in the list, and the result of
     *   the overall call is the index of the element for which func(x)
     *   returns the smallest value.  For example, if you have a list of
     *   strings l, l.indexOfMin({x: x.length()}) returns the index of the
     *   shortest string in the list.  
     */
    indexOfMin(func?);

    /*
     *   Get the minimum value in the list.  If 'func' is missing, this
     *   returns the minimum element value.  If 'func' is specified, it must
     *   be a function; it's called as func(x) for each value in the list,
     *   and the result of the overall method call is the element value x
     *   that minimizes func(x).  For example, if l is a list of strings,
     *   l.minVal({x: x.length()}) returns the shortest string.  
     */
    minVal(func?);

    /*
     *   Get the index of the list item with the maximum value.  If 'func' is
     *   missing, this simply returns the index of the list element with the
     *   largest value, comparing the element values as though using the '>'
     *   and '<' operators.  If 'func' is specified, it must be a function;
     *   it's called as func(x) for each value in the list, and the result of
     *   the overall call is the index of the element for which func(x)
     *   returns the greatest value.  For example, if you have a list of
     *   strings l, l.indexOfMax({x: x.length()}) returns the index of the
     *   longest string in the list.  
     */
    indexOfMax(func?);

    /*
     *   Get the maximum value in the list.  If 'func' is missing, this
     *   returns the largest element value.  If 'func' is specified, it must
     *   be a function; it's called as func(x) for each value in the list,
     *   and the result of the overall method call is the element value x
     *   that maximizes func(x).  For example, if l is a list of strings,
     *   l.maxVal({x: x.length()}) returns the longest string.  
     */
    maxVal(func?);
}

/*
 *   Sorting order flags.  These can be passed in as the first argument to
 *   List.sort() (and Vector.sort() as well) to make the meaning of the
 *   argument clearer.  
 */
#define SortAsc    nil
#define SortDesc   true

/*
 *   Export length() as the element counter method for list-like objects in
 *   general. 
 */
export length 'length';


/* ------------------------------------------------------------------------ */
/*
 *   'RexPattern' intrinsic class.  This class encapsulates a compiled
 *   regular expression pattern.
 *   
 *   A RexPattern object can be passed to the regular expression functions
 *   (rexMatch, rexSearch, rexReplace) in place of a string pattern.  Since
 *   compiling a regular expression takes a non-trivial amount of time, it's
 *   more efficient to compile a pattern to a RexPattern object if the same
 *   pattern will be used in multiple searches.  
 */
intrinsic class RexPattern 'regex-pattern/030000': Object
{
    /*
     *   Constructor:
     *   
     *   new RexPattern(patternString) - returns a RexPattern representing
     *   the compiled pattern string.  
     */

    /* retrieve the original pattern string used to construct the object */
    getPatternString();
}

/* ------------------------------------------------------------------------ */
/*
 *   StackFrameDesc intrinsic class.  This class provides access to a stack
 *   frame.  It lets us retrieve the values of local variables and method
 *   context variables (self, definingobj, targetobj, targetprop).  It also
 *   allows us to assign new values to local variables.
 *   
 *   To get the value of a local variable in the frame, simply use
 *   frame[name], where 'frame' is the StackFrameDesc object for the frame,
 *   and 'name' is a string giving the name of the local variable to
 *   retrieve.  If the frame is active, this retrieves the live value of the
 *   variable from the frame; otherwise it retrieves the value from a
 *   snapshot containing the last value before the routine returned to its
 *   caller.
 *   
 *   To assign a new value to a local in the frame, assign a value to
 *   frame[name] for the desired variable name.  If the frame is active, this
 *   updates the live variable in the stack frame, so when execution returns
 *   to the caller the variable will have the new value.  If the frame is
 *   inactive, it updates the snapshot we made when the routine returned to
 *   its caller.
 *   
 *   This object can't be created with 'new'.  Instead, you obtain these
 *   objects via the t3GetStackTrace() function.  That function retrieves
 *   information on an active stack frame in the current call stack,
 *   including the frame object.  
 */
intrinsic class StackFrameDesc 'stack-frame-desc/030000'
{
    /*
     *   Is the stack frame active?  A stack frame is active until the
     *   function or method it represents returns to its caller.  When the
     *   routine returns, the frame becomes inactive.
     *   
     *   When the routine is about to return (so the frame is about to become
     *   inacive), the StackFrameDesc object makes a private snapshot of the
     *   variables in the frame.  Subsequent access to the locals will
     *   automatically use the snapshot copy, so you can continue to access
     *   the locals as normal without worrying about whether or not the
     *   actual stack frame still exists.  This allows you to continue to
     *   access and modify the values of the variables after the routine has
     *   exited.  
     */
    isActive();

    /*
     *   Get a LookupTable consisting of all of the variables (local
     *   variables and parameters) in the frame.  Each element in the table
     *   is keyed by the name of a variable, and contains the current value
     *   of the variable.
     *   
     *   The returned lookup table is a snapshot copy of the current values
     *   of the variables.  If the underlying variable values in the frame
     *   change, the lookup table won't be affected, since it's just a
     *   separate copy made at the moment this routine is called.  Similarly,
     *   changing the value of an entry in the returned lookup table won't
     *   affect the actual variable in the stack frame.
     *   
     *   To retrieve the current live value of a variable in the actual stack
     *   frame, use frame[name], where 'frame' is the StackFrameDesc object
     *   for the frame, and 'name' is a string giving the variable name.  
     */
    getVars();

    /*
     *   Get the value of 'self' in this frame. 
     */
    getSelf();

    /*
     *   Get the value of 'definingobj' in this frame. 
     */
    getDefiningObj();

    /*
     *   Get the value of 'targetobj' in this frame. 
     */
    getTargetObj();

    /*
     *   Get the value of 'targetprop' in this frame. 
     */
    getTargetProp();

    /*
     *   Get the value of 'invokee' in this frame. 
     */
    getInvokee();
}

/*
 *   A StackFrameRef is an internal object used with StackFrameDesc.  This
 *   type is used internally by the VM; user code can't create an instance of
 *   this class directly with 'new', and the class has no public methods.  
 */
intrinsic class StackFrameRef 'stack-frame-ref/030000'
{
}

