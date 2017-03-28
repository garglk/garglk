#charset "us-ascii"

/*
 *   Tokenizer - customizable tokenizer class for use with the intrinsic
 *   class 'grammar-production' parser.
 *   
 *   This tokenizer implementation is parameterized with a set of rules
 *   (see below); a basic set of rules is provided, but users can
 *   customize the tokenizer quite extensively simply by subclassing the
 *   Tokenizer class and overriding the 'rules_' property with a new set
 *   of rules declarations.  
 */

#include "tads.h"
#include "t3.h"
#include "dict.h"
#include "tok.h"
#include "vector.h"

/* ------------------------------------------------------------------------ */
/*
 *   Tokenizer exceptions 
 */

/*
 *   base class for all tokenizer errors (to allow blanket 'catch') 
 */
class TokenizerError: Exception
    displayException() { "Tokenizer exception"; }
;

/*
 *   no match for token 
 */
class TokErrorNoMatch: TokenizerError
    construct(str)
    {
        /* remember the full remaining text */
        remainingStr_ = str;

        /* 
         *   for convenience, separately remember the single character
         *   that we don't recognize - this is simply the first character
         *   of the rest of the line 
         */
        curChar_ = str.substr(1, 1);
    }

    displayException()
        { "Tokenizer error: unexpected character '<<curChar_>>'"; }

    /* 
     *   The remainder of the string.  This is the part that couldn't be
     *   matched; we were successful in matching up to this point. 
     */
    remainingStr_ = nil

    /* current character (first character of remainingStr_) */
    curChar_ = nil
;

/* ------------------------------------------------------------------------ */
/*
 *   Basic token types
 */

/* word */
enum token tokWord;

/* quoted string */
enum token tokString;

/* punctuation */
enum token tokPunct;

/* integer number */
enum token tokInt;


/* ------------------------------------------------------------------------ */
/*
 *   Tokenizer base class
 */
class Tokenizer: object
    /*
     *   Tokenizing rules.  The subclass can override this to specify a
     *   list that defines different tokenization rules.  Each entry in the
     *   master rules_ list is one rule.  Each rule is a list consisting of
     *   the name of the rule; the pattern to match for the rule; the token
     *   type (an 'enum token') to use when the rule is matched; the value
     *   computation rule; and the value test rule.
     *   
     *   The name of a rule is just an arbitrary string to identify the
     *   rule.  This can be used to insert new rules in order relative to
     *   known existing rules, or to delete known existing rules.
     *   
     *   If the value computation rule is nil, we'll just use the matching
     *   text as the token value.  If the value rule is a string, we'll use
     *   the string as a replacement pattern (with rexReplace).  If it's a
     *   property ID, we'll invoke the property of self with the following
     *   arguments:
     *   
     *   txt, typ, toks
     *   
     *   'txt' is the matched text; 'typ' is the token type from the rule;
     *   and 'toks' is a vector to which the new token or tokens are to be
     *   added.  The routine is responsible for adding the appropriate
     *   values to the result list.  Note that the routine can add more
     *   than one token to the results if desired.
     *   
     *   If the value test rule is non-nil, it must be either a method or a
     *   function; we'll call the method or function to test to see if the
     *   matched value is valid.  We'll call the method (on self) with the
     *   matching text as the argument; if the method returns true, the
     *   rule matches, otherwise the rule fails, and we'll continue looking
     *   for another rule as though we hadn't matched the rule's regular
     *   expression in the first place.  This can be used for rules that
     *   require more than a simple regular expression match; for example,
     *   the value test can be used to look up the match in a dictionary,
     *   so that the rule only matches tokens that are defined in the
     *   dictionary.  
     */
    rules_ = static
    [
        /* skip whitespace */
        ['whitespace', R'<Space>+', nil, &tokCvtSkip, nil],

        /* certain punctuation marks */
        ['punctuation', R'[.,;:?!]', tokPunct, nil, nil],

        /* 
         *   Words - note that we convert everything to lower-case.  A
         *   word must start with an alphabetic character, but can contain
         *   alphabetics, digits, hyphens, and apostrophes after that. 
         */
        ['word', R'<Alpha>(<AlphaNum>|[-\'])*', tokWord, &tokCvtLower, nil],

        /* strings */
        ['string single-quote', R'\'(.*)\'', tokString, nil, nil],
        ['string double-quote', R'"(.*)"', tokString, nil, nil],

        /* integer numbers */
        ['integer', R'[0-9]+', tokInt, nil, nil]
    ]

    /*
     *   Insert a new rule before or after the existing rule with the name
     *   'curName'.  If 'curName' is nil, or rule is found with the given
     *   name, we'll insert the new rule at the end of the list.  'rule'
     *   must be a list with the standard elements for a tokenizer rule.
     *   'after' is nil to insert the new rule before the given existing
     *   rule, true to insert after it.  
     */
    insertRule(rule, curName, after)
    {
        local idx;

        /* 
         *   if the name of an existing rule was supplied, find the
         *   existing rule with the given name 
         */
        idx = nil;
        if (curName != nil)
            idx = rules_.indexWhich({x: tokRuleName(x) == curName});

        /* if we didn't find curName, insert at the end of the list */
        if (idx == nil)
            idx = rules_.length();

        /* if we're inserting after the given element, adjust the index */
        if (after)
            ++idx;

        /* insert the new rule */
        insertRuleAt(rule, idx);
    }

    /* 
     *   Insert a rule at the given index in our rules list.  'rule' must
     *   be a list with the standard elements for a tokenizer rule.  'idx'
     *   is the index of the new rule; we'll insert before the existing
     *   element at this index; so if 'idx' is 1, we'll insert before the
     *   first existing rule.  
     */
    insertRuleAt(rule, idx)
    {
        /* insert the rule */
        rules_ = rules_.insertAt(idx, rule);
    }

    /*
     *   Delete a rule by name.  This finds the rule with the given name
     *   and removes it from the list. 
     */
    deleteRule(name)
    {
        local idx;
        
        /* find the rule with the given name */
        idx = rules_.indexWhich({x: tokRuleName(x) == name});

        /* if we found the named element, remove it from the list */
        if (idx != nil)
            deleteRuleAt(idx);
    }

    /* delete the rule at the given index */
    deleteRuleAt(idx)
    {
        /* delete the rule */
        rules_ = rules_.removeElementAt(idx);
    }

    /* convert a string to lower-case (for value computation rules) */
    tokCvtLower(txt, typ, toks)
    {
        /* add the lower-cased version of the string to the result list */
        toks.append([txt.toLower(), typ, txt]);
    }

    /* 
     *   processing routine to skip a match - this is used for whitespace
     *   and other text that does not result in any tokens in the result
     *   list 
     */
    tokCvtSkip(txt, typ, toks)
    {
        /* simply skip the text without generating any new tokens */
    }

    /*
     *   Tokenize a string.  If we find text that we can't match to any of
     *   the rules, we'll throw an exception (TokErrorNoMatch).  If we
     *   succeed in tokenizing the entire string, we'll return a list with
     *   one element per token.  Each element of the main list is a
     *   sublist with the following elements describing a token:
     *   
     *   - The first element gives the token's value.
     *   
     *   - The second element the token type (given as a token type enum
     *   value).
     *   
     *   - The third element the original token strings, before any
     *   conversions or evaluations were performed.  For example, this
     *   maintains the original case of strings that are lower-cased for
     *   the corresponding token values.
     */
    tokenize(str)
    {
        local toks = new Vector(32);
        local startIdx = 1;
        local len = str.length();
        
        /* keep going until we run out of string */
    mainLoop:
        while (startIdx <= len)
        {
            /* run through the rules in sequence until we match one */
        ruleLoop:
            for (local i = 1, local cnt = rules_.length() ; i <= cnt ; ++i)
            {
                local cur;
                local match;
                local val;
                        
                /* get the current rule */
                cur = rules_[i];

                /* check for a match to the rule's pattern */
                match = rexMatch(tokRulePat(cur), str, startIdx);
                if (match != nil && match > 0)
                {
                    local test;
                    local txt;
                    local typ;

                    /* get the matching text */
                    txt = str.substr(startIdx, match);

                    /* 
                     *   if there's a value test, invoke it to determine
                     *   if the token really matches 
                     */
                    if ((test = tokRuleTest(cur)) != nil)
                    {
                        local accept;

                        /* check what kind of test function we have */
                        switch (dataType(test))
                        {
                        case TypeFuncPtr:
                        case TypeObject:
                            /* it's a function or anonymous function */
                            accept = (test)(txt);
                            break;

                        case TypeProp:
                            /* it's a method */
                            accept = self.(test)(txt);
                            break;

                        default:
                            /* consider anything else to be accepted */
                            accept = true;
                            break;
                        }

                        /* 
                         *   if the value test failed, it means that the
                         *   token doesn't match this rule after all -
                         *   ignore the regex match and keep searching for
                         *   another rule 
                         */
                        if (!accept)
                            continue ruleLoop;
                    }

                    /* get the type of the token from the rule */
                    typ = tokRuleType(cur);
                    
                    /* get this value processing rule */
                    val = tokRuleVal(cur);

                    /* determine what value to use */
                    switch(dataTypeXlat(val))
                    {
                    case TypeNil:
                        /* use the matching text verbatim */
                        toks.append([txt, typ, txt]);
                        break;
                        
                    case TypeProp:
                        /* 
                         *   invoke the property - it's responsible for
                         *   adding the token or tokens to the results
                         *   lists 
                         */
                        self.(val)(txt, typ, toks);
                        break;
                        
                    case TypeSString:
                        /* it's a regular expression replacement */
                        toks.append(
                            [rexReplace(tokRulePat(cur),
                                        txt, val, ReplaceOnce),
                             typ, txt]);
                        break;

                    case TypeFuncPtr:
                        /* invoke the function */
                        (val)(txt, typ, toks);
                        break;

                    default:
                        /* 
                         *   use any other value exactly as given in
                         *   the rule 
                         */
                        toks.append([val, typ, txt]);
                        break;
                    }

                    /* 
                     *   continue the search at the next character after
                     *   the end of this token 
                     */
                    startIdx += match;

                    /* start over with the rest of the string */
                    continue mainLoop;
                }
            }

            /*
             *   We failed to find a match for this part of the string.
             *   Throw an exception and let the caller figure out what to
             *   do.  The exception parameter gives the rest of the
             *   string, so the caller can display a suitable error
             *   message if desired.  
             */
            throw new TokErrorNoMatch(str.substr(startIdx));
        }

        /* we're done with the string - return out value and type lists */
        return toks.toList();
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Test Section 
 */

#ifdef TOK_TEST

main(args)
{
    "Enter text to tokenize.  Type Q or QUIT when done. ";
    for (;;)
    {
        local str, toks;

        /* read a string */
        "\b>";
        str = inputLine();

        /* catch tokenization errors */
        try
        {
            /* tokenize the string */
            toks = Tokenizer.tokenize(str);

            /* if the first token is 'quit', we're done */
            if (toks.length() > 0
                && getTokType(toks[1]) == tokWord
                && (getTokVal(toks[1])== 'quit' || getTokVal(toks[1]) == 'q'))
            {
                /* they want to stop - exit the command loop */
                break;
            }

            /* display the tokens */
            for (local i = 1, local cnt = toks.length() ; i <= cnt ; ++i)
                "(<<getTokVal(toks[i])>>) ";
        }
        catch (TokErrorNoMatch err)
        {
            "Unrecognized punctuation: <<err.remainingStr_.substr(1, 1)>>";
        }
    }
}

#endif /* TOK_TEST */

