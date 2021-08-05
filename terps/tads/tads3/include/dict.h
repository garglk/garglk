#charset "us-ascii"
#pragma once

/* 
 *   Copyright (c) 2000, 2006 Michael J. Roberts
 *   
 *   This file is part of TADS 3.
 *   
 *   This header defines the Dictionary intrinsic class.  
 */


/* include our base class definition */
#include "systype.h"

/*
 *   The Dictionary intrinsic class is a specialized lookup table class
 *   designed for storing the vocabulary table for a parser.  Dictionary
 *   works closely with GrammarProd to supply the vocabulary tokens for the
 *   productions.
 *   
 *   The main difference between Dictionary and a more general hash table is
 *   that Dictionary tags each vocabulary word with a type; for our purposes,
 *   the type is the vocabulary property (&noun, &adjective, etc) that
 *   associates the word with an object.  
 */
intrinsic class Dictionary 'dictionary2/030001': Object
{
    /*
     *   Constructor:
     *   
     *   new Dictionary() - creates a dictionary with a default comparator,
     *   which matches strings exactly (note that upper-case and lower-case
     *   letters are considered distinct)
     *   
     *   new Dictionary(compObj) - create a dictionary with the given
     *   comparator object.  A comparator is an object that implements the
     *   comparator interface - see below for details.  When searching the
     *   dictionary (with findWord, for example), we use the comparator
     *   object to determine if the string beings sought matches a dictionary
     *   string.  
     */

    /* 
     *   Set the comparator object.  This defines how words are compared.
     *   The object must provide the following methods, which comprise the
     *   "comparator" interface.  Note that there's no class that defines
     *   this interface; this is simply a set of methods that we define here,
     *   and which the supplied object must define.
     *   
     *   calcHash(str) - returns an integer giving the hash value of the
     *   given string.  The purpose of the hash value is to arbitrarily
     *   partition the search space, so that we can search only a small
     *   subset of the dictionary when looking for a particular string.  It
     *   is desirable for hash values to distribute uniformly for a given set
     *   of strings.  It's also highly desirable for the hash computation to
     *   be inexpensive (i.e., to run fast), since the whole point of the
     *   hash is to reduce the amount of time it takes to find a string; if
     *   it takes longer to compute the hash value than it would to search
     *   every string in the table, then we don't come out ahead using the
     *   hash.
     *   
     *   matchValues(inputStr, dictStr) - compare the given input string with
     *   the given dictionary string, and return a result indicating whether
     *   or not they match for the purposes of the comparator.  A return
     *   value of zero or nil indicates that the values do not match; any
     *   other return value indicates a match.
     *   
     *   Typically, matchValues() will return a non-zero integer to indicate
     *   a match and to encode additional information about the match using a
     *   bitwise-OR'd combination of flag values.  For example, a comparator
     *   that allows case folding could use bit flag 0x0001 to indicate any
     *   match, and bit flag 0x0002 to indicate a match where the case of one
     *   or more input letters did not match the case of the corresponding
     *   letters in the dictionary string.  So, a return value of 0x0001
     *   would indicate an exact match, and 0x0003 would indicate a match
     *   with case differences.
     *   
     *   Note the asymmetry in the matchValues() arguments: we specifically
     *   designate one string as the input string and one as the dictionary
     *   string.  This allows for asymmetrical comparisons, which are
     *   desirable in some cases: we sometimes want a given input string to
     *   match a given dictionary string even when the two are not identical
     *   character-by-character.  For example, we might want to allow the
     *   user to type only the first six or eight characters of a string in
     *   the dictionary, to save typing; or, we might want to allow a user to
     *   enter unaccented letters and still match dictionary words containing
     *   the corresponding letters with accents.  The asymmetry in the
     *   arguments is there because we often only want these "fuzzy" match
     *   rules to work in one direction; for the truncation example, we'd
     *   want an input word that's a truncated version of a dictionary word
     *   to match, but not the other way around.
     *   
     *   Important: Note that, although the hash value computation is up to
     *   the implementing object to define, we impose one requirement.  It is
     *   REQUIRED that for any two strings s1 and s2, if matchValues(s1, s2)
     *   indicates a match (i.e., returns a value other than 0 or nil), then
     *   calcHash(s1) MUST EQUAL calcHash(s2).  (This does NOT mean that two
     *   strings with equal hash values must be equal, or, equivalently, that
     *   two unequal strings must have different hash values.  Hash
     *   collisions are explicitly allowed, so two strings that don't match
     *   can still have the same hash value.)  
     */
    setComparator(compObj);

    /* 
     *   Find a word; returns a list giving the objects associated with the
     *   string in the dictionary.  If voc_prop is specified, only objects
     *   associated with the word by the given vocabulary property are
     *   returned.  We match the string using the comparator defined for the
     *   dictionary.
     *   
     *   The return value is a list consisting of pairs of entries.  The
     *   first element of each pair is the matching object, and the second is
     *   gives the comparator result for matching the word.  If we use a
     *   StringComparator, this will be a non-zero integer value giving
     *   information on truncation, case folding, and any equivalence
     *   mappings defined in the comparator.  If the comparator is a custom
     *   object, then the second element of the pair will be whatever the
     *   custom comparator's matchValues() method returned for matching the
     *   value for that dictionary entry.
     *   
     *   The reason for giving a matchValues() return value for every
     *   individual match is that the same input string 'str' might match
     *   multiple entries in the dictionary.  For example, the same string
     *   might match one word exactly and one with truncation.  The match
     *   result code lets the caller determine if some matches are "better"
     *   than others, based on how the string matched for each individual
     *   object entry.  
     */
    findWord(str, voc_prop?);

    /*
     *   Add a word to the dictionary, associating the given object with the
     *   given string and property combination. 
     */
    addWord(obj, str, voc_prop);

    /*
     *   Remove the given word association from the dictionary.  This
     *   removes only the association for the given object; other objects
     *   associated with the same word are not affected.  
     */
    removeWord(obj, str, voc_prop);

    /* 
     *   Check to see if the given string 'str' is defined in the dictionary.
     *   Returns true if the word is defined, nil if not.
     *   
     *   If the 'filter' argument is provided, it gives a callback function
     *   that is invoked to determine whether or not to count a particular
     *   word in the dictionary as a match.  The callback is invoked with one
     *   argument: (filter)(match), where 'match' is the result of the
     *   comparator's matchValues(str,dstr) method, where 'dstr' is a
     *   dictionary string matching 'str'.  The filter function returns true
     *   if the string should be counted as a match, nil if not.  The return
     *   value of isWordDefined thus will be true if the filter function
     *   returns true for at least one match, nil if not.  The purpose of the
     *   filter function is to allow the caller to impose a more restrictive
     *   condition than the dictionary's current comparator does; for
     *   example, the caller might use the filter to determine if the
     *   dictionary contains any matches for 'str' that match without any
     *   truncation.  
     */
    isWordDefined(str, filter?);

    /*
     *   Invoke the callback func(obj, str, prop) for each word in the
     *   dictionary.  Note that the callback can be invoked with a single
     *   string multiple times, since the callback is invoked once per
     *   word/object/property association; in other words, the callback is
     *   invoked once for each association created with addWord() or during
     *   compilation.  
     */
    forEachWord(func);

    /*
     *   Get a list of possible spelling corrections for the given word.
     *   This searches the dictionary for words that match the given word
     *   within the given maximum "edit distance".
     *   
     *   The return value is a list giving all of the words in the dictionary
     *   that match the input string within the given maximum edit distance.
     *   Any given dictionary word will appear only once in the returned
     *   list.  The list is in arbitrary order.  Each entry consists of a
     *   sublist, [word, dist, repl], where 'word' is a matching dictionary
     *   word, 'dist' is the edit distance between that dictionary word and
     *   the input string, and 'repl' is the number of character replacements
     *   performed.  (The replacement count is included in the edit distance,
     *   but it's called out separately because some correctors treat
     *   replacements as heavier changes than other edits.  A caller could
     *   use this to break ties for corrections of the same distance.
     *   Consider "book" and "box" as corrections for "bok": both have edit
     *   distance 1, but "book" has no replacements, while "box" has one.)
     *   
     *   The edit distance between two words is defined as the number of
     *   single-character insertions, deletions, replacements, and
     *   transpositions necessary to transform one word into another.  For
     *   example, OPNE can be transformed into OPEN by transposing the N-E
     *   pair, for an edit distance of 1.  XAEMINE can be transformed into
     *   EXAMINE by inserting an E at the beginning, and then deleting the E
     *   at the third letter, for an edit distance of 2.
     *   
     *   Choosing the maximum edit distance is essentially heuristic.  Higher
     *   values make the search take longer, and yield more matches - which
     *   increases the chances that the right match will be found, but also
     *   increases the number of false matches to sift through.  The
     *   literature on spelling correction suggests that 2 is a good value in
     *   practice, across a wide range of applications, based on the most
     *   frequent patterns of human typographical errors.  However, you'll
     *   probably do better to vary the distance based on the word length:
     *   perhaps 1 for words up to 4 letters, 2 for 5-7 letters, and 3 for
     *   words of 8 letters or more.
     *   
     *   If the dictionary has a StringComparator object as its current
     *   comparator, the results will take into account its case folding
     *   setting, truncation length, and character mappings.  These
     *   "approximations" are NOT considered to be edits, so they don't count
     *   against the maximum edit distance.  Custom comparators (not of the
     *   StringComparator class) are ignored: if you use a custom comparator,
     *   this method will only find matches based on the exact text of the
     *   dictionary words.  
     */
    correctSpelling(str, maxEditDistance);
}

/* 
 *   We rely on certain methods defined by the comparator interface, so
 *   export those method names.  
 */
property calcHash, matchValues;
export calcHash 'IfcComparator.calcHash';
export matchValues 'IfcComparator.matchValues';

