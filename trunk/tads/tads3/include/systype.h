#charset "us-ascii"

/*
 *   Copyright 2000, 2006 Michael J. Roberts.
 *   
 *   This file is part of TADS 3.
 *   
 *   This module defines the fundamental intrinsic classes, including Object,
 *   String, Collection, List, and Iterator.  
 */

#ifndef _SYSTYPE_H_
#define _SYSTYPE_H_


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


/* ------------------------------------------------------------------------ */
/*
 *   The IntrinsicClass intrinsic class.  Objects of this type represent the
 *   intrinsic classes themselves.  
 */
intrinsic class IntrinsicClass 'intrinsic-class/030000': Object
{
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
intrinsic class TadsObject 'tads-object/030004': Object
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
intrinsic class String 'string/030005': Object
{
    /* get the length of the string */
    length();

    /* extract a substring */
    substr(start, len?);

    /* convert to upper case */
    toUpper();

    /* convert to lower case */
    toLower();

    /* find a substring */
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
     *   Map to a byte array, converting to the given character set.
     *   'charset' must be an object of intrinsic class CharacterSet; the
     *   characters in the string will be mapped from the internal Unicode
     *   representation to the appropriate byte representation in the given
     *   character set.  
     */
    mapToByteArray(charset);

    /* 
     *   Replace one occurrence or all occurrences of the given substring
     *   with the given new string.  
     */
    findReplace(origStr, newStr, flags, index?);
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

/* replace only one occurrence, or replace all occurrences */
#define ReplaceOnce  0x0000
#define ReplaceAll   0x0001


/* ------------------------------------------------------------------------ */
/*
 *   The native list type 
 */
intrinsic class List 'list/030007': Collection
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
     *   Insert one or more elements at the given index.  If the index is 1,
     *   the elements will be inserted before the first existing element.
     *   If the index is one higher than the number of elements, the
     *   elements will be inserted after all existing elements.
     *   
     *   Note that a list value argument will simply be inserted as a single
     *   element.
     *   
     *   Returns a new list with the value(s) inserted.  
     */
    insertAt(startingIndex, val, ...);

    /*
     *   Delete the element at the given index, reducing the length of the
     *   list by one element.  Returns a new list with the given element
     *   removed.  
     */
    removeElementAt(index);

    /*
     *   Delete the range of elements starting at startingIndex and ending
     *   at endingIndex.  The elements at the ends of the range are included
     *   in the deletion.  If startingIndex == endingIndex, only one element
     *   is removed.  Returns a new list with the given element range
     *   removed.  
     */
    removeRange(startingIndex, endingIndex);

    /*
     *   Invoke the callback func(index, val) on each element, in order from
     *   first to last.  No return value.  
     */
    forEachAssoc(func);
}

/*
 *   Sorting order flags.  These can be passed in as the first argument to
 *   List.sort() (and Vector.sort() as well) to make the meaning of the
 *   argument clearer.  
 */
#define SortAsc    nil
#define SortDesc   true


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

#endif /* _SYSTYPE_H_ */
