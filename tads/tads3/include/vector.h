#charset "us-ascii"
#pragma once

/*
 *   Copyright (c) 2000, 2006 Michael J. Roberts
 *   
 *   This file is part of TADS 3.
 *   
 *   This header defines the Vector intrinsic class.  
 */


/* include our base class definition */
#include "systype.h"


/*
 *   The Vector intrinsic class provides a varying-length array type.
 *   Vectors can be expanded dynamically, and values within a vector can be
 *   changed.  (In contrast, List is a constant type: a value within a list
 *   cannot be changed, and new values can't be added to a list.  Any
 *   manipulation of a List results in a new, separate List object, leaving
 *   the original unchanged.  Vector allows new values to be added and
 *   existing values to be changed in place, without creating a new object.)
 */
intrinsic class Vector 'vector/030005': Collection
{
    /* 
     *   Create a list with the same elements as the vector.  If 'start' is
     *   specified, it's the index of the first element we store; we'll
     *   store elements starting at index 'start'.  If 'cnt' is specified,
     *   it gives the maximum number of elements for the new list; by
     *   default, we'll store all of the elements from 'start' to the last
     *   element.  
     */
    toList(start?, cnt?);

    /* get the number of elements in the vector */
    length();

    /* 
     *   Copy from another vector or list.  Elements are copied from the
     *   source vector or list starting at the element given by 'src_start',
     *   and are copied into 'self' starting at the index given by
     *   'dst_start'.  At most 'cnt' values are copied, but we stop when we
     *   reach the last element of either the source or destination values.
     *   If either index is negative, it counts from the end of the vector:
     *   -1 is the last element, -2 is the second to last, and so on.  
     */
    copyFrom(src, src_start, dst_start, cnt);

    /* 
     *   Fill with a given value, starting at the given element (the first
     *   element if not specified), and running for the given number of
     *   elements (the remaining existing elements of the vector, if not
     *   specified).  The vector is expanded if necessary.  A negative
     *   starting index counts backwards from the last element.  
     */
    fillValue(val, start?, cnt?);

    /*
     *   Select a subset of the vector.  Returns a new vector consisting
     *   only of the elements of this vector for which the callback function
     *   returns true.  
     */
    subset(func);

    /*
     *   Apply a callback function to each element of the vector.  For each
     *   element of the vector, invokes the callback, and replaces the
     *   element with the return value of the callback.  Modifies the vector
     *   in-place, and returns 'self'.  
     */
    applyAll(func);

    /* 
     *   Find the first element for which the given condition is true.
     *   Apply the callback function (which encodes the condition to
     *   evaluate) to each element in turn, starting with the first.  For
     *   each element, if the callback returns nil, proceed to the next
     *   element; otherwise, stop and return the index of the element.  If
     *   the callback never returns true for any element, we'll return nil.  
     */
    indexWhich(cond);

    /* 
     *   Invoke the callback func(val) on each element, in order from first
     *   to last.  No return value.  
     */
    forEach(func);

    /* 
     *   Invoke the callback func(index, val) on each element, in order from
     *   first to last.  No return value.  
     */
    forEachAssoc(func);

    /*
     *   Apply the callback function to each element of this vector, and
     *   return a new vector consisting of the results.  Effectively maps
     *   the vector to a new vector using the given function, leaving the
     *   original vector unchanged.  
     */
    mapAll(func);

    /* get the index of the first match for the given value */
    indexOf(val);

    /* 
     *   Find the first element for which the given condition is true, and
     *   return the value of the element.  
     */
    valWhich(cond);

    /* find the last element with the given value, and return its index */
    lastIndexOf(val);

    /* 
     *   Find the last element for which the condition is true, and return
     *   the index of the element.  Applies the callback to each element in
     *   turn, starting with the last element and working backwards.  For
     *   each element, if the callback returns nil, proceeds to the previous
     *   element; otherwise, stops and returns the index of the element.  If
     *   the callback never returns true for any element, we'll return nil.  
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

    /* create a new vector consisting of the unique elements of this vector */
    getUnique();

    /*
     *   append the elements of the list or vector 'val' to the elements of
     *   this vector, then remove repeated elements in the result; returns a
     *   new vector with the unique elements of the combination 
     */
    appendUnique(val);

    /* 
     *   Sort the vector in place; returns 'self'.  If the 'descending'
     *   flag is provided and is not nil, we'll sort the vector in
     *   descending order rather than ascending order.
     *   
     *   If the 'comparisonFunction' argument is provided, it must be a
     *   callback function; the callback takes two arguments, and returns
     *   an integer less than zero if the first argument value is less
     *   than the second, zero if they're equal, and an integer greater
     *   than zero if the first is greater than the second.  If no
     *   'comparisonFunction' argument is provided, or it's provided and
     *   its value is nil, we'll simply compare the vector elements as
     *   ordinary values.  The comparison function can be provided for
     *   caller-defined orderings, such as ordering a set of objects.  
     */
    sort(descending?, comparisonFunction?);

    /* 
     *   Set the length - if this is shorter than the current length,
     *   existing items will be discarded; if it's longer, the newly added
     *   slots will be set to nil.  Returns 'self'.
     */
    setLength(newElementCount);

    /* 
     *   Insert one or more elements at the given index.  If the starting
     *   index is 1, the elements will be inserted before the first existing
     *   element.  If the index is one higher than the number of elements,
     *   the elements will be inserted after all existing elements.  A
     *   negative starting index counts from the end of the vector: -1 is the
     *   last element, -2 is the second to last, and so on.  A zero starting
     *   index inserts after the last existing element.
     *   
     *   Note that a list value argument will simply be inserted as a single
     *   element.
     *   
     *   Returns 'self'.  
     */
    insertAt(startingIndex, val, ...);

    /*
     *   Delete the element at the given index, reducing the length of the
     *   vector by one element.  If 'index' is negative, it counts from the
     *   end of the vector: -1 is the last element, -2 is the second to last,
     *   and so on.  Returns 'self'.  
     */
    removeElementAt(index);

    /*
     *   Delete the range of elements starting at startingIndex and ending at
     *   endingIndex.  The elements at the ends of the range are included in
     *   the deletion.  If startingIndex == endingIndex, only one element is
     *   removed.  If either index is negative, it counts backwards from the
     *   last element: -1 is the last element, -2 is the second to last, and
     *   so on.  The length of the vector is reduced by the number of
     *   elements removed.  Returns 'self'.  
     */
    removeRange(startingIndex, endingIndex);

    /* 
     *   Append an element to the vector.  This works just like insertAt()
     *   with a starting index one higher than the length of the vector.
     *   This has almost the same effect as the '+' operator, but treats a
     *   list value like any other value by simply inserting the list as a
     *   single new element (rather than appending each item in the list
     *   individually, as the '+' operator would).  
     */
    append(val);

    /*
     *   Prepend an element.  This works like insertAt() with a starting
     *   index of 1. 
     */
    prepend(val);

    /*
     *   Append all elements from a list or vector.  This works like
     *   append(val), except that if 'val' is a list or vector, the elements
     *   of 'val' will be appended individually, like the '+' operator does.
     *   The difference between this method and the '+' operator is that
     *   this method modifies this Vector by adding the new elements
     *   directly to the existing Vector object, whereas the '+' operator
     *   creates a new Vector to store the result.  
     */
    appendAll(val);

    /*
     *   Remove an element by value.  Each element of the vector matching
     *   the given value is removed.  The vector is modified in-place.  The
     *   return value is 'self'.  
     */
    removeElement(val);

    /*
     *   Splice new values into the vector.  Deletes the 'del' elements
     *   starting at 'idx', then inserts the extra arguments in their place.
     *   Updates the vector in place.  To insert items without deleting
     *   anything, pass 0 for 'del'.  To delete items without inserting
     *   anything, omit any additional arguments.  Returns 'self'.
     */
    splice(idx, del, ...);

    /*
     *   Combine the vector elements into a string.  This converts each
     *   element into a string value using the usual default conversions (or
     *   throws an error if string conversion isn't possible), then
     *   concatenates the values together and returns the result.  If
     *   'separator' is provided, it's a string that's interposed between
     *   elements; if this is omitted, the elements are concatenated together
     *   with no extra characters in between.  
     */
    join(sep?);

    /*
     *   Class method: Generate a new Vector.  'func' is a callback function,
     *   which can take zero or one argument.  'n' is the number of elements
     *   for the new list.  For each element, 'func' is invoked as func() if
     *   it takes no arguments, or func(idx) if it takes one argument, where
     *   'idx' is the index of the element being generated.  The return value
     *   of the call to 'func' is stored as the list element.  The method
     *   returns the resulting list.  For example, a vector of the first ten
     *   even positive integers: Vector.generate({i: i*2}, 10).  
     */
    static generate(func, n);

    /*
     *   Get the index of the element with the minimum value.  If 'func' is
     *   missing, this simply returns the index of the element with the
     *   smallest value, comparing the element values as though using the '>'
     *   and '<' operators.  If 'func' is specified, it must be a function;
     *   it's called as func(x) for each element's value, and the result of
     *   the overall call is the index of the element for which func(x)
     *   returns the smallest value.  For example, if you have a vector v
     *   containing string elements, v.indexOfMin({x: x.length()}) returns
     *   the index of the shortest string.  
     */
    indexOfMin(func?);

    /*
     *   Get the minimum element value.  If 'func' is missing, this simply
     *   returns the smallest element value.  If 'func' is specified, it must
     *   be a function; it's called as func(x) for each element value x, and
     *   the result of the overall method call is the element value x that
     *   minimizes func(x).  For example, if v is a vector containing string
     *   elements, v.minVal({x: x.length()}) returns the shortest string.  
     */
    minVal(func?);

    /*
     *   Get the index of the element the maximum value.  If 'func' is
     *   missing, this simply returns the index of the element with the
     *   largest value, comparing the element values as though using the '>'
     *   and '<' operators.  If 'func' is specified, it must be a function;
     *   it's called as func(x) for each element value x, and the result of
     *   the overall call is the index of the element for which func(x)
     *   returns the greatest value.  For example, if you have a vector v
     *   containing string elements, v.indexOfMax({x: x.length()}) returns
     *   the index of the longest string.  
     */
    indexOfMax(func?);

    /*
     *   Get the maximum element value.  If 'func' is missing, this returns
     *   the largest element value.  If 'func' is specified, it must be a
     *   function; it's called as func(x) for each element value x, and the
     *   result of the overall method call is the element value x that
     *   maximizes func(x).  For example, if v is a vector containing string
     *   elements, v.minVal({x: x.length()}) returns the longest string.  
     */
    maxVal(func?);
}

