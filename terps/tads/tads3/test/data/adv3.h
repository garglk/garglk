#charset "us-ascii"

/* 
 *   Copyright (c) 2000, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
 *   adv3.h - TADS 3 library main header
 *   
 *   Each source code file in a game should #include this header near the
 *   top of the source file.  
 */

#ifndef ADV3_H
#define ADV3_H

/* ------------------------------------------------------------------------ */
/*
 *   Include the system headers that we depend upon.  We include these
 *   here so that each game source file will pick up the same set of
 *   system headers in the same order, which is important for intrinsic
 *   function set definitions.  
 */
#include "tads.h"
#include "t3.h"
#include "vector.h"


/* ------------------------------------------------------------------------ */
/* 
 *   set the location property 
 */
+ property location;


/* ------------------------------------------------------------------------ */
/*
 *   Transparency levels 
 */

/* the sense is passed without loss of detail */
enum transparent;

/* the sense is passed, but with a loss of detail associated with distance */
enum distant;

/* 
 *   the sense is passed, but with a loss of detail due to an obscuring
 *   layer of material 
 */
enum obscured;

/* the sense is not passed at all */
enum opaque;


/* ------------------------------------------------------------------------ */
/*
 *   Size classes.  An object is large, medium, or small with respect to
 *   each sense; the size is used to determine how well the object can be
 *   sensed at a distance or when obscured.
 *   
 *   What "size" means depends on the sense.  For sight, the size
 *   indicates the visual size of the object.  For hearing, the size
 *   indicates the loudness of the object.  
 */

/* 
 *   Large - the object is large enough that its details can be sensed
 *   from a distance or through an obscuring medium.
 */
enum large;

/* 
 *   Medium - the object can be sensed at a distance or when obscured, but
 *   not in any detail.  Most objects fall into this category.  Note that
 *   things that are parts of large objects should normally be medium.  
 */
enum medium;

/*
 *   Small - the object cannot be sensed at a distance at all.  This is
 *   appropriate for detailed parts of medium-class objects.  
 */
enum small;


/* ------------------------------------------------------------------------ */
/*
 *   Listing Options 
 */

/* 
 *   use "tall" notation, which lists objects in a single column, one item
 *   per line (the default is "wide" notation, which creates a sentence
 *   with the object listing) 
 */
#define LIST_TALL     0x0001

/* 
 *   in "tall" mode, recursively list the contents of each object; this
 *   flag isn't meaningful unless it is combined with LIST_TALL 
 */
#define LIST_RECURSE  0x0002

/* 
 *   use "long list" notation - separates items that contain sublists with
 *   special punctuation, to set off the individual items in the longer
 *   listing from the items in the sublists (for example, separates items
 *   with semicolons rather than commas) 
 */
#define LIST_LONG     0x0004

/* use the group description of items displayed */
#define LIST_GROUP    0x0008

/* this is an inventory listing */
#define LIST_INVENT   0x0010

/* 
 *   this is a separate list of items being worn (so "being worn" status
 *   messages should not be shown) 
 */
#define LIST_WORN     0x0020

/* this is a listing of the contents of another item previously listed */
#define LIST_CONTENTS 0x0040

/* this listing is in the dark */
#define LIST_DARK     0x0080


/* ------------------------------------------------------------------------ */
/*
 *   Spelled-out number options 
 */

/* 
 *   Use tens of hundreds rather than thousands if possible - 1950 is
 *   'nineteen hundred fifty' rather than 'one thousand nine hundred
 *   fifty'.  This only works if the number (not including the millions
 *   and billions) is in the range 1,100 to 9,999, because we don't want
 *   to say something like 'one hundred twenty hundred' for 12,000.  
 */
#define SPELLINT_TEEN_HUNDREDS  0x0001

/*
 *   use 'and' before the tens - 125 is 'one hundred and twenty-five'
 *   rather than 'one hundred twenty-five' 
 */
#define SPELLINT_AND_TENS       0x0002

/*
 *   put a comma after each power group - 123456 is 'one hundred
 *   twenty-three thousand, four hundred fifty-six' 
 */
#define SPELLINT_COMMAS         0x0004


/* ------------------------------------------------------------------------ */
/*
 *   Decimal number format options
 */

/*
 *   Use a group separator character between digit groups, using the
 *   default setting in languageGlobals.
 */
#define INTDEC_GROUP_SEP        0x0001

/* 
 *   Explicitly use a comma/period to separate digit groups, overriding
 *   the current languageGlobals setting.
 */
#define INTDEC_GROUP_COMMA      0x0002
#define INTDEC_GROUP_PERIOD     0x0004


#endif /* ADV3_H */

