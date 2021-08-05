#charset "us-ascii"
#pragma once

/* 
 *   Copyright (c) 2000, 2006 Michael J. Roberts
 *   
 *   This file is part of TADS 3.
 *   
 *   The header defines the GrammarProd intrinsic class and some associated
 *   properties and constants.  
 */


/* include our base class definition */
#include "systype.h"

/*
 *   The GrammarProd intrinsic class is a specialized type that's designed
 *   for creating parsers.  An object of this type is created automatically
 *   by the TADS 3 compiler for each 'grammar' statement.  This class
 *   encapsulates the prototype token list and mapping information defined in
 *   a 'grammar' statement, and provides a method to match its prototype to
 *   an input token string.
 *   
 *   GrammarProd implements a "parser" in a limited computerese sense, which
 *   is essentially a program that produces "sentence diagrams" a la
 *   elementary school grammar lessons.  GrammarProd is thus only a small
 *   part of what we think of as "the parser" in an IF context.  The broader
 *   parser starts with the sentence diagrams that GrammarProd produces, and
 *   must then interpret their meanings and carry out the intentions they
 *   express.
 *   
 *   Parsers built with GrammarProd trees can handle grammars with left or
 *   right recursion, and can handle ambiguous grammars (meaning that a
 *   single input can have multiple ways of matching the grammar).  This is
 *   especially important for natural language parsing, since virtually all
 *   natural languages have ambiguous grammars.  When a match is ambiguous, a
 *   GrammarProd parser builds all of the possible match trees, allowing you
 *   to choose the best match based on context and semantic content.
 *   
 *   You can also create new productions dynamically with new GrammarProd().
 *   Use addAlt() to populate a new production with token matching rules.  
 */
intrinsic class GrammarProd 'grammar-production/030002': Object
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

    /*
     *   Add a new alternative set of alternatives to the rule list for this
     *   grammar production.
     *   
     *   'alt' is a string with the token list specifying the alternative(s)
     *   to add.  This uses the same syntax as the rule list in a static
     *   'grammar' statement.  You can use '|' symbols to add multiple
     *   alternatives.
     *   
     *   'matchObj' is the match object class, which is the class of the
     *   object that parseTokens() reutrns in the match list to represent a
     *   match to this production.  This corresponds to the base class
     *   defined in a static 'grammar' statement.
     *   
     *   'dict' is an optional Dictionary object.  Literal tokens used in the
     *   token list will be automatically added to the dictionary if they're
     *   not already defined.
     *   
     *   'symtab' is an optional LookupTable with the global symbol table.
     *   This must be provided if any symbolic tokens are used in the rule
     *   list (property names, sub-production names, etc).  In most cases
     *   this is simply the symbol table that t3GetGlobalSymbols() returns
     *   during preinit.  
     */
    addAlt(alt, matchObj, dict?, symtab?);

    /*
     *   Delete onen or more alternatives from the rule list for this grammar
     *   production.  'id' identifies the rule(s) to delete:
     *   
     *   - By tag: if 'id' is a string, the method deletes each alternative
     *   whose match object's grammarTag property equals 'id'.  For static
     *   rules defined with 'grammar' statements, the compiler sets
     *   grammarTag to the tag used in the statement defining the rule.  This
     *   makes it easy to delete all of the rules defined by a given
     *   'grammar' statement.  
     *   
     *   - By match object class: if 'id' is an object, the method deletes
     *   every alternative whose match object equals 'id' or is a subclass of
     *   'id'.  This makes it easy to delete a group of dynamically added
     *   rules that share a match object.
     *   
     *   - By index: if 'id' is an integer, it gives the index of the rule to
     *   delete.  This corresponds to an index in the list returned by
     *   getGrammarInfo().
     *   
     *   'dict' is an optional Dictionary object to adjust for the deletion.
     *   If a non-nil 'dict' is given, we'll remove literals from the
     *   dictionary that were defined by the alternative and no longer used
     *   in the production.  
     */
    deleteAlt(id, dict?);

    /* 
     *   Delete all alternatives from the rule list for this grammar
     *   production.  This resets the rule to an empty production with no
     *   alternatives to match, which is convenient if you want to redefine
     *   the entire rule set with a subsequent addAlt() call.
     *   
     *   'dict' is an optional Dictionary object to adjust for the deletion.
     *   If a non-nil 'dict' is given, we'll remove all of the production's
     *   literals from the dictionary.  
     */
    clearAlts(dict?);
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

/*
 *   For alternatives created at run-time with addAlt(), the system maintains
 *   a list of all of the properties used in "->" expressions in all
 *   alternatives associated with a match object.  This list of properties is
 *   stored in the grammarAltProps of the match object.
 */
property grammarAltProps;
export grammarAltProps 'GrammarProd.altProps';

