#charset "us-ascii"

/* 
 *   Copyright (c) 2000, 2006 Michael J. Roberts
 *   
 *   This file is part of TADS 3.
 *   
 *   The header defines the GrammarProd intrinsic class and some associated
 *   properties and constants.  
 */

#ifndef _GRAMPROD_H_
#define _GRAMPROD_H_

/* include our base class definition */
#include "systype.h"

/*
 *   The GrammarProd intrinsic class is a specialized type that's used to
 *   create parsers.  An object of this type is created automatically by the
 *   TADS 3 compiler for each 'grammar' statement.  This class encapsulates
 *   the prototype token list and mapping information defined in a 'grammar'
 *   statement, and provides a method to match its prototype to an actual
 *   input token string.  
 */
intrinsic class GrammarProd 'grammar-production/030001': Object
{
    /*
     *   Parse the token list, starting at this production, using the given
     *   dictionary to look up tokens.  Returns a list of match objects.  If
     *   there are no matches to the grammar, simply returns an empty list.  
     */
    parseTokens(tokenList, dict);

    /*
     *   Retrieve a detailed description of the production.  This returns a
     *   list of GrammarAltInfo objects that describe the rule alternatives
     *   that make up this production.  
     */
    getGrammarInfo();
}

/*
 *   Token slot types.  Each token slot in an alternative has a type, which
 *   determines what it matches in an input token list.  getGrammarInfo()
 *   returns these type codes in the GrammarAltTokInfo objects.  
 */

#define GramTokTypeProd    1              /* token matches a sub-production */
#define GramTokTypeSpeech  2     /* token matches a specific part of speech */
#define GramTokTypeLiteral 3              /* token matches a literal string */
#define GramTokTypeTokEnum 4            /* token matches a token class enum */
#define GramTokTypeStar    5    /* token matches all remaining input tokens */
#define GramTokTypeNSpeech 6      /* matches any of several parts of speech */


/* 
 *   Properties for the first and last token indices, and the complete
 *   original token list.
 *   
 *   Each match tree object will have the firstTokenIndex and lastTokenIndex
 *   properties set to the bounding token indices for its subtree.  Each
 *   match tree object will also have tokenList set to the original token
 *   list passed to the parseTokens() that created the match tree, and
 *   tokenMatchList set to a list of the Dictionary's comparator's
 *   matchValues() result for each token match.  
 */
property firstTokenIndex, lastTokenIndex, tokenList, tokenMatchList;
export firstTokenIndex 'GrammarProd.firstTokenIndex';
export lastTokenIndex  'GrammarProd.lastTokenIndex';
export tokenList       'GrammarProd.tokenList';
export tokenMatchList  'GrammarProd.tokenMatchList';


#endif /* _GRAMPROD_H_ */
