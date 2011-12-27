#charset "us-ascii"

/*
 *   Copyright (c) 2005, 2006 Michael J. Roberts
 *   
 *   This file is part of TADS 3.
 *   
 *   This module defines some classes used by the GrammarProd intrinsic
 *   class.  
 */

#include <tads.h>
#include <gramprod.h>


/* ------------------------------------------------------------------------ */
/*
 *   GrammarProd descriptor classes.  The GrammarProd intrinsic class's
 *   getGrammarInfo() method uses these classes to build its description of
 *   a grammar production.
 */

/* export the classes so the intrinsic class can find them */
export GrammarAltInfo 'GrammarProd.GrammarAltInfo';
export GrammarAltTokInfo 'GrammarProd.GrammarAltTokInfo';


/*
 *   Rule alternative descriptor.  This describes one alternative in a
 *   grammar production.  An alternative is one complete list of matchable
 *   tokens.
 *   
 *   In a 'grammar' statement, alternatives are delimited by '|' symbols at
 *   the top level.  Each group of tokens set off by '|' symbols is one
 *   alternative.
 *   
 *   When '|' symbols are grouped with parentheses in a 'grammar'
 *   statement, the compiler "flattens" the grouping by expanding out the
 *   parenthesized groups until it has entirely top-level alternatives.
 *   So, at the level of a GrammarProd object, there's no such thing as
 *   parentheses or nested '|' symbols.  
 */
class GrammarAltInfo: object
    /*
     *   Constructor.  GrammarProd.getGrammarInfo() calls this once for
     *   each alternative making up the production, passing in the values
     *   that define the alternative.  Note that we have a '...' in our
     *   argument list so that we'll be compatible with any future
     *   GrammarProd versions that add additional arguments - we won't do
     *   anything with the extra arguments, but we'll harmlessly ignore
     *   them, so code compiled with this library version will continue to
     *   work correctly.  
     */
    construct(score, badness, matchObj, toks, ...)
    {
        /* stash away the information */
        gramBadness = badness;
        gramMatchObj = matchObj;
        gramTokens = toks;
    }

    /*
     *   The 'badness' value associated with the alternative.  A value of
     *   zero means that there's no badness. 
     */
    gramBadness = 0

    /* 
     *   the "match object" class - this is the class that
     *   GrammarProd.parseTokens() instantiates to represent a match to
     *   this alternative in the match list that the method returns 
     */
    gramMatchObj = nil

    /*
     *   The token descriptor list.  This is a list of zero or more
     *   GrammarAltTokInfo objects describing the tokens making up this
     *   rule.  
     */
    gramTokens = []
;

/*
 *   Grammar rule token descriptor.  GrammarProd.getGrammarInfo()
 *   instantiates one of these objects to represent each token slot in an
 *   alternative; a GrammarAltInfo object's gramTokens property has a list
 *   of these objects.
 */
class GrammarAltTokInfo: object
    /*
     *   Constructor.  GrammarProd.getGrammarInfo() calls this once for
     *   each token in each alternative in the production, passing in
     *   values to fully describe the token slot: the target property (in a
     *   'grammar' statement, this is the property after a '->' symbol);
     *   the token type; and extra information that depends on the token
     *   type.  Note that we use '...' at the end of the argument list so
     *   that we'll be compatible with any future changes to GrammarProd
     *   that add more arguments to this method.  
     */
    construct(prop, typ, info, ...)
    {
        /* remember the information */
        gramTargetProp = prop;
        gramTokenType = typ;
        gramTokenInfo = info;
    }

    /*
     *   The target property - this is the property of the *match object*
     *   that will store the match information for the token.  In a
     *   'grammar' statement, this is the property after the '->' symbol
     *   for this token. 
     */
    gramTargetProp = nil

    /*
     *   The token type.  This is one of the GramTokTypeXxx values (see
     *   gramprod.h) indicating what kind of token slot this is.  
     */
    gramTokenType = nil

    /*
     *   Detailed information for the token slot, which depends on the
     *   token type:
     *   
     *   GramTokTypeProd - this gives the GrammarProd object defining the
     *   sub-production that this token slot matches
     *   
     *   GramTokTypeSpeech - this is the property ID giving the
     *   part-of-speech property that this token slot matches
     *   
     *   GramTokTypeNSpeech - this is a list of property IDs giving the
     *   part-of-speech properties that this token slot matches
     *   
     *   GramTokTypeLiteral - this is a string giving the literal that this
     *   slot matches
     *   
     *   GramTokTypeTokEnum - this is the enum value giving the token type
     *   that this slot matches
     *   
     *   GramTokTypeStar - no extra information (the value will be nil) 
     */
    gramTokenInfo = nil
;

/*
 *   Dynamic match object interface.  This is a mix-in class that should be
 *   used as a superclass for any class used as the match object when
 *   creating new alternatives dynamically with GrammarProd.addAlt().
 *   
 *   This class provides an implementation of grammarInfo() that works like
 *   the version the compiler generates for static match objects.  In this
 *   case, we use the grammarAltProps information that addAlt() stores in
 *   the match object.  
 */
class DynamicProd: object
    /* 
     *   Generate match information.  This returns the same information
     *   that grammarInfo() returns for match objects that the compiler
     *   generates for static 'grammar' statements.  
     */
    grammarInfo()
    {
        return [grammarTag] + grammarAltProps.mapAll({ p: self.(p) });
    }

    /* 
     *   grammarTag - the name for the collection of alternatives
     *   associated with the match object.  This name is primarily for
     *   debugging purposes; it appears as the first element of the
     *   grammarInfo() result list. 
     */
    grammarTag = 'new-alt'

    /* 
     *   grammarAltProps - the list of "->" properties used in all of the
     *   alternatives associated with this match object.  addAlts() stores
     *   this list automatically - there's no need to create it manually.  
     */
    grammarAltProps = []
;
