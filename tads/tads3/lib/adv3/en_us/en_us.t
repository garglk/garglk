#charset "us-ascii"

/*
 *   Copyright 2000, 2006 Michael J. Roberts.  All Rights Reserved.
 *.  Past-tense extensions written by Michel Nizette, and incorporated by
 *   permission.
 *   
 *   TADS 3 Library - English (United States variant) implementation
 *   
 *   This defines the parts of the TADS 3 library that are specific to the
 *   English language as spoken (and written) in the United States.
 *   
 *   We have attempted to isolate here the parts of the library that are
 *   language-specific, so that translations to other languages or dialects
 *   can be created by replacing this module, without changing the rest of
 *   the library.
 *   
 *   In addition to this module, a separate set of US English messages are
 *   defined in the various msg_xxx.t modules.  Those modules define
 *   messages in English for different stylistic variations.  For a given
 *   game, the author must select one of the message modules - but only
 *   one, since they all define variations of the same messages.  To
 *   translate the library, a translator must create at least one module
 *   defining those messages as well; only one message module is required
 *   per language.
 *   
 *   The past-tense system was contributed by Michel Nizette.
 *   
 *.                                  -----
 *   
 *   "Watch an immigrant struggling with a second language or a stroke
 *   patient with a first one, or deconstruct a snatch of baby talk, or try
 *   to program a computer to understand English, and ordinary speech
 *   begins to look different."
 *   
 *.         Stephen Pinker, "The Language Instinct"
 */

#include "tads.h"
#include "tok.h"
#include "adv3.h"
#include "en_us.h"
#include <vector.h>
#include <dict.h>
#include <gramprod.h>
#include <strcomp.h>


/* ------------------------------------------------------------------------ */
/*
 *   Fill in the default language for the GameInfo metadata class.
 */
modify GameInfoModuleID
    languageCode = 'en-US'
;

/* ------------------------------------------------------------------------ */
/*
 *   Simple yes/no confirmation.  The caller must display a prompt; we'll
 *   read a command line response, then return true if it's an affirmative
 *   response, nil if not.
 */
yesOrNo()
{
    /* switch to no-command mode for the interactive input */
    "<.commandnone>";

    /*
     *   Read a line of input.  Do not allow real-time event processing;
     *   this type of prompt is used in the middle of a command, so we
     *   don't want any interruptions.  Note that the caller must display
     *   any desired prompt, and since we don't allow interruptions, we
     *   won't need to redisplay the prompt, so we pass nil for the prompt
     *   callback.
     */
    local str = inputManager.getInputLine(nil, nil);

    /* switch back to mid-command mode */
    "<.commandmid>";

    /*
     *   If they answered with something starting with 'Y', it's
     *   affirmative, otherwise it's negative.  In reading the response,
     *   ignore any leading whitespace.
     */
    return rexMatch('<space>*[yY]', str) != nil;
}

/* ------------------------------------------------------------------------ */
/*
 *   During start-up, install a case-insensitive truncating comparator in
 *   the main dictionary.
 */
PreinitObject
    execute()
    {
        /* set up the main dictionary's comparator */
        languageGlobals.setStringComparator(
            new StringComparator(gameMain.parserTruncLength, nil, []));
    }

    /*
     *   Make sure we run BEFORE the main library preinitializer, so that
     *   we install the comparator in the dictionary before we add the
     *   vocabulary words to the dictionary.  This doesn't make any
     *   difference in terms of the correctness of the dictionary, since
     *   the dictionary will automatically rebuild itself whenever we
     *   install a new comparator, but it makes the preinitialization run
     *   a tiny bit faster by avoiding that rebuild step.
     */
    execAfterMe = [adv3LibPreinit]
;

/* ------------------------------------------------------------------------ */
/*
 *   Language-specific globals
 */
languageGlobals: object
    /*
     *   Set the StringComparator object for the parser.  This sets the
     *   comparator that's used in the main command parser dictionary. 
     */
    setStringComparator(sc)
    {
        /* remember it globally, and set it in the main dictionary */
        dictComparator = sc;
        cmdDict.setComparator(sc);
    }

    /*
     *   The character to use to separate groups of digits in large
     *   numbers.  US English uses commas; most Europeans use periods.
     *
     *   Note that this setting does not affect system-level BigNumber
     *   formatting, but this information can be passed when calling
     *   BigNumber formatting routines.
     */
    digitGroupSeparator = ','

    /*
     *   The decimal point to display in floating-point numbers.  US
     *   English uses a period; most Europeans use a comma.
     *
     *   Note that this setting doesn't affect system-level BigNumber
     *   formatting, but this information can be passed when calling
     *   BigNumber formatting routines.
     */
    decimalPointCharacter = '.'

    /* the main dictionary's string comparator */
    dictComparator = nil
;


/* ------------------------------------------------------------------------ */
/*
 *   Language-specific extension of the default gameMain object
 *   implementation.
 */
modify GameMainDef
    /*
     *   Option setting: the parser's truncation length for player input.
     *   As a convenience to the player, we can allow the player to
     *   truncate long words, entering only the first, say, 6 characters.
     *   For example, rather than typing "x flashlight", we could allow the
     *   player to simply type "x flashl" - truncating "flashlight" to six
     *   letters.
     *   
     *   We use a default truncation length of 6, but games can change this
     *   by overriding this property in gameMain.  We use a default of 6
     *   mostly because that's what the old Infocom games did - many
     *   long-time IF players are accustomed to six-letter truncation from
     *   those games.  Shorter lengths are superficially more convenient
     *   for the player, obviously, but there's a trade-off, which is that
     *   shorter truncation lengths create more potential for ambiguity.
     *   For some games, a longer length might actually be better for the
     *   player, because it would reduce spurious ambiguity due to the
     *   parser matching short input against long vocabulary words.
     *   
     *   If you don't want to allow the player to truncate long words at
     *   all, set this to nil.  This will require the player to type every
     *   word in its entirety.
     *   
     *   Note that changing this property dynamicaly will have no effect.
     *   The library only looks at it once, during library initialization
     *   at the very start of the game.  If you want to change the
     *   truncation length dynamically, you must instead create a new
     *   StringComparator object with the new truncation setting, and call
     *   languageGlobals.setStringComparator() to select the new object.  
     */
    parserTruncLength = 6

    /*
     *   Option: are we currently using a past tense narrative?  By
     *   default, we aren't.
     *
     *   This property can be reset at any time during the game in order to
     *   switch between the past and present tenses.  The macro
     *   setPastTense can be used for this purpose: it just provides a
     *   shorthand for setting gameMain.usePastTense directly.
     *
     *   Authors who want their game to start in the past tense can achieve
     *   this by overriding this property on their gameMain object and
     *   giving it a value of true.
     */
    usePastTense = nil
;


/* ------------------------------------------------------------------------ */
/*
 *   Language-specific modifications for ThingState.
 */
modify ThingState
    /*
     *   Our state-specific tokens.  This is a list of vocabulary words
     *   that are state-specific: that is, if a word is in this list, the
     *   word can ONLY refer to this object if the object is in a state
     *   with that word in its list.
     *   
     *   The idea is that you set up the object's "static" vocabulary with
     *   the *complete* list of words for all of its possible states.  For
     *   example:
     *   
     *.     + Matchstick 'lit unlit match';
     *   
     *   Then, you define the states: in the "lit" state, the word 'lit' is
     *   in the stateTokens list; in the "unlit" state, the word 'unlit' is
     *   in the list.  By putting the words in the state lists, you
     *   "reserve" the words to their respective states.  When the player
     *   enters a command, the parser will limit object matches so that the
     *   reserved state-specific words can only refer to objects in the
     *   corresponding states.  Hence, if the player refers to a "lit
     *   match", the word 'lit' will only match an object in the "lit"
     *   state, because 'lit' is a reserved state-specific word associated
     *   with the "lit" state.
     *   
     *   You can re-use a word in multiple states.  For example, you could
     *   have a "red painted" state and a "blue painted" state, along with
     *   an "unpainted" state.
     */
    stateTokens = []

    /*
     *   Match the name of an object in this state.  We'll check the token
     *   list for any words that apply only to *other* states the object
     *   can assume; if we find any, we'll reject the match, since the
     *   phrase must be referring to an object in a different state.
     */
    matchName(obj, origTokens, adjustedTokens, states)
    {
        /* scan each word in our adjusted token list */
        for (local i = 1, local len = adjustedTokens.length() ;
             i <= len ; i += 2)
        {
            /* get the current token */
            local cur = adjustedTokens[i];

            /*
             *   If this token is in our own state-specific token list,
             *   it's acceptable as a match to this object.  (It doesn't
             *   matter whether or not it's in any other state's token list
             *   if it's in our own, because its presence in our own makes
             *   it an acceptable matching word when we're in this state.) 
             */
            if (stateTokens.indexWhich({t: t == cur}) != nil)
                continue;

            /*
             *   It's not in our own state-specific token list.  Check to
             *   see if the word appears in ANOTHER state's token list: if
             *   it does, then this word CAN'T match an object in this
             *   state, because the token is special to that other state
             *   and thus can't refer to an object in a state without the
             *   token. 
             */
            if (states.indexWhich(
                {s: s.stateTokens.indexOf(cur) != nil}) != nil)
                return nil;
        }

        /* we didn't find any objection, so we can match this phrase */
        return obj;
    }

    /*
     *   Check a token list for any tokens matching any of our
     *   state-specific words.  Returns true if we find any such words,
     *   nil if not.
     *
     *   'toks' is the *adjusted* token list used in matchName().
     */
    findStateToken(toks)
    {
        /*
         *   Scan the token list for a match to any of our state-specific
         *   words.  Since we're using the adjusted token list, every
         *   other entry is a part of speech, so work through the list in
         *   pairs.
         */
        for (local i = 1, local len = toks.length() ; i <= len ; i += 2)
        {
            /*
             *   if this token matches any of our state tokens, indicate
             *   that we found a match
             */
            if (stateTokens.indexWhich({x: x == toks[i]}) != nil)
                return true;
        }

        /* we didn't find a match */
        return nil;
    }

    /* get our name */
    listName(lst) { return listName_; }

    /*
     *   our list name setting - we define this so that we can be easily
     *   initialized with a template (we can't initialize listName()
     *   directly in this manner because it's a method, but we define the
     *   listName() method to simply return this property value, which we
     *   can initialize with a template)
     */
    listName_ = nil
;

/* ------------------------------------------------------------------------ */
/*
 *   Language-specific modifications for VocabObject.
 */
modify VocabObject
    /*
     *   The vocabulary initializer string for the object - this string
     *   can be initialized (most conveniently via a template) to a string
     *   of this format:
     *
     *   'adj adj adj noun/noun/noun*plural plural plural'
     *
     *   The noun part of the string can be a hyphen, '-', in which case
     *   it means that the string doesn't specify a noun or plural at all.
     *   This can be useful when nouns and plurals are all inherited from
     *   base classes, and only adjectives are to be specified.  (In fact,
     *   any word that consists of a single hyphen will be ignored, but
     *   this is generally only useful for the adjective-only case.)
     *
     *   During preinitialization, we'll parse this string and generate
     *   dictionary entries and individual vocabulary properties for the
     *   parts of speech we find.
     *
     *   Note that the format described above is specific to the English
     *   version of the library.  Non-English versions will probably want
     *   to use different formats to conveniently encode appropriate
     *   language-specific information in the initializer string.  See the
     *   comments for initializeVocabWith() for more details.
     *
     *   You can use the special wildcard # to match any numeric
     *   adjective.  This only works as a wildcard when it stands alone,
     *   so a string like "7#" is matched as that literal string, not as a
     *   wildcard.  If you want to use a pound sign as a literal
     *   adjective, just put it in double quotes.
     *
     *   You can use the special wildcard "\u0001" (include the double
     *   quotes within the string) to match any literal adjective.  This
     *   is the literal adjective equivalent of the pound sign.  We use
     *   this funny character value because it is unlikely ever to be
     *   interesting in user input.
     *
     *   If you want to match any string for a noun and/or adjective, you
     *   can't do it with this property.  Instead, just add the property
     *   value noun='*' to the object.
     */
    vocabWords = ''

    /*
     *   On dynamic construction, initialize our vocabulary words and add
     *   them to the dictionary.
     */
    construct()
    {
        /* initialize our vocabulary words from vocabWords */
        initializeVocab();

        /* add our vocabulary words to the dictionary */
        addToDictionary(&noun);
        addToDictionary(&adjective);
        addToDictionary(&plural);
        addToDictionary(&adjApostS);
        addToDictionary(&literalAdjective);
    }

    /* add the words from a dictionary property to the global dictionary */
    addToDictionary(prop)
    {
        /* if we have any words defined, add them to the dictionary */
        if (self.(prop) != nil)
            cmdDict.addWord(self, self.(prop), prop);
    }

    /* initialize the vocabulary from vocabWords */
    initializeVocab()
    {
        /* inherit vocabulary from this class and its superclasses */
        inheritVocab(self, new Vector(10));
    }

    /*
     *   Inherit vocabulary from this class and its superclasses, adding
     *   the words to the given target object.  'target' is the object to
     *   which we add our vocabulary words, and 'done' is a vector of
     *   classes that have been visited so far.
     *
     *   Since a class can be inherited more than once in an inheritance
     *   tree (for example, a class can have multiple superclasses, each
     *   of which have a common base class), we keep a vector of all of
     *   the classes we've visited.  If we're already in the vector, we'll
     *   skip adding vocabulary for this class or its superclasses, since
     *   we must have already traversed this branch of the tree from
     *   another subclass.
     */
    inheritVocab(target, done)
    {
        /*
         *   if we're in the list of classes handled already, don't bother
         *   visiting me again
         */
        if (done.indexOf(self) != nil)
            return;

        /* add myself to the list of classes handled already */
        done.append(self);

        /* 
         *   add words from our own vocabWords to the target object (but
         *   only if it's our own - not if it's only inherited, as we'll
         *   pick up the inherited ones explicitly in a bit) 
         */
        if (propDefined(&vocabWords, PropDefDirectly))
            target.initializeVocabWith(vocabWords);

        /* add vocabulary from each of our superclasses */
        foreach (local sc in getSuperclassList())
            sc.inheritVocab(target, done);
    }

    /*
     *   Initialize our vocabulary from the given string.  This parses the
     *   given vocabulary initializer string and adds the words defined in
     *   the string to the dictionary.
     *
     *   Note that this parsing is intentionally located in the
     *   English-specific part of the library, because it is expected that
     *   other languages will want to define their own vocabulary
     *   initialization string formats.  For example, a language with
     *   gendered nouns might want to use gendered articles in the
     *   initializer string as an author-friendly way of defining noun
     *   gender; languages with inflected (declined) nouns and/or
     *   adjectives might want to encode inflected forms in the
     *   initializer.  Non-English language implementations are free to
     *   completely redefine the format - there's no need to follow the
     *   conventions of the English format in other languages where
     *   different formats would be more convenient.
     */
    initializeVocabWith(str)
    {
        local sectPart;
        local modList = [];

        /* start off in the adjective section */
        sectPart = &adjective;

        /* scan the string until we run out of text */
        while (str != '')
        {
            local len;
            local cur;

            /*
             *   if it starts with a quote, find the close quote;
             *   otherwise, find the end of the current token by seeking
             *   the next delimiter
             */
            if (str.startsWith('"'))
            {
                /* find the close quote */
                len = str.find('"', 2);
            }
            else
            {
                /* no quotes - find the next delimiter */
                len = rexMatch('<^space|star|/>*', str);
            }

            /* if there's no match, use the whole rest of the string */
            if (len == nil)
                len = str.length();

            /* if there's anything before the delimiter, extract it */
            if (len != 0)
            {
                /* extract the part up to but not including the delimiter */
                cur = str.substr(1, len);

                /*
                 *   if we're in the adjectives, and either this is the
                 *   last token or the next delimiter is not a space, this
                 *   is implicitly a noun
                 */
                if (sectPart == &adjective
                    && (len == str.length()
                        || str.substr(len + 1, 1) != ' '))
                {
                    /* move to the noun section */
                    sectPart = &noun;
                }

                /*
                 *   if the word isn't a single hyphen (in which case it's
                 *   a null word placeholder, not an actual vocabulary
                 *   word), add it to our own appropriate part-of-speech
                 *   property and to the dictionary
                 */
                if (cur != '-')
                {
                    /*
                     *   by default, use the part of speech of the current
                     *   string section as the part of speech for this
                     *   word
                     */
                    local wordPart = sectPart;

                    /*
                     *   Check for parentheses, which indicate that the
                     *   token is "weak."  This doesn't affect anything
                     *   about the token or its part of speech except that
                     *   we must include the token in our list of weak
                     *   tokens.
                     */
                    if (cur.startsWith('(') && cur.endsWith(')'))
                    {
                        /* it's a weak token - remove the parens */
                        cur = cur.substr(2, cur.length() - 2);

                        /*
                         *   if we don't have a weak token list yet,
                         *   create the list
                         */
                        if (weakTokens == nil)
                            weakTokens = [];

                        /* add the token to the weak list */
                        weakTokens += cur;
                    }

                    /*
                     *   Check for special formats: quoted strings,
                     *   apostrophe-S words.  These formats are mutually
                     *   exclusive.
                     */
                    if (cur.startsWith('"'))
                    {
                        /*
                         *   It's a quoted string, so it's a literal
                         *   adjective.
                         */

                        /* remove the quote(s) */
                        if (cur.endsWith('"'))
                            cur = cur.substr(2, cur.length() - 2);
                        else
                            cur = cur.substr(2);

                        /* change the part of speech to 'literal adjective' */
                        wordPart = &literalAdjective;
                    }
                    else if (cur.endsWith('\'s'))
                    {
                        /*
                         *   It's an apostrophe-s word.  Remove the "'s"
                         *   suffix and add the root word using adjApostS
                         *   as the part of speech.  The grammar rules are
                         *   defined to allow this part of speech to be
                         *   used exclusively with "'s" suffixes in input.
                         *   Since the tokenizer always pulls the "'s"
                         *   suffix off of a word in the input, we have to
                         *   store any vocabulary words with "'s" suffixes
                         *   the same way, with the "'s" suffixes removed.
                         */

                        /* change the part of speech to adjApostS */
                        wordPart = &adjApostS;

                        /* remove the "'s" suffix from the string */
                        cur = cur.substr(1, cur.length() - 2);
                    }

                    /* add the word to our own list for this part of speech */
                    if (self.(wordPart) == nil)
                        self.(wordPart) = [cur];
                    else
                        self.(wordPart) += cur;

                    /* add it to the dictionary */
                    cmdDict.addWord(self, cur, wordPart);

                    if (cur.endsWith('.'))
                    {
                        local abbr;

                        /*
                         *   It ends with a period, so this is an
                         *   abbreviated word.  Enter the abbreviation
                         *   both with and without the period.  The normal
                         *   handling will enter it with the period, so we
                         *   only need to enter it specifically without.
                         */
                        abbr = cur.substr(1, cur.length() - 1);
                        self.(wordPart) += abbr;
                        cmdDict.addWord(self, abbr, wordPart);
                    }

                    /* note that we added to this list */
                    if (modList.indexOf(wordPart) == nil)
                        modList += wordPart;
                }
            }

            /* if we have a delimiter, see what we have */
            if (len + 1 < str.length())
            {
                /* check the delimiter */
                switch(str.substr(len + 1, 1))
                {
                case ' ':
                    /* stick with the current part */
                    break;

                case '*':
                    /* start plurals */
                    sectPart = &plural;
                    break;

                case '/':
                    /* start alternative nouns */
                    sectPart = &noun;
                    break;
                }

                /* remove the part up to and including the delimiter */
                str = str.substr(len + 2);

                /* skip any additional spaces following the delimiter */
                if ((len = rexMatch('<space>+', str)) != nil)
                    str = str.substr(len + 1);
            }
            else
            {
                /* we've exhausted the string - we're done */
                break;
            }
        }

        /* uniquify each word list we updated */
        foreach (local p in modList)
            self.(p) = self.(p).getUnique();
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Language-specific modifications for Thing.  This class contains the
 *   methods and properties of Thing that need to be replaced when the
 *   library is translated to another language.
 *   
 *   The properties and methods defined here should generally never be used
 *   by language-independent library code, because everything defined here
 *   is specific to English.  Translators are thus free to change the
 *   entire scheme defined here.  For example, the notions of number and
 *   gender are confined to the English part of the library; other language
 *   implementations can completely replace these attributes, so they're
 *   not constrained to emulate their own number and gender systems with
 *   the English system.  
 */
modify Thing
    /*
     *   Flag that this object's name is rendered as a plural (this
     *   applies to both a singular noun with plural usage, such as
     *   "pants" or "scissors," and an object used in the world model to
     *   represent a collection of real-world objects, such as "shrubs").
     */
    isPlural = nil

    /*
     *   Flag that this is object's name is a "mass noun" - that is, a
     *   noun denoting a continuous (effectively infinitely divisible)
     *   substance or material, such as water, wood, or popcorn; and
     *   certain abstract concepts, such as knowledge or beauty.  Mass
     *   nouns are never rendered in the plural, and use different
     *   determiners than ordinary ("count") nouns: "some popcorn" vs "a
     *   kernel", for example.
     */
    isMassNoun = nil

    /*
     *   Flags indicating that the object should be referred to with
     *   gendered pronouns (such as 'he' or 'she' rather than 'it').
     *
     *   Note that these flags aren't mutually exclusive, so it's legal
     *   for the object to have both masculine and feminine usage.  This
     *   can be useful when creating collective objects that represent
     *   more than one individual, for example.
     */
    isHim = nil
    isHer = nil

    /*
     *   Flag indicating that the object can be referred to with a neuter
     *   pronoun ('it').  By default, this is true if the object has
     *   neither masculine nor feminine gender, but it can be overridden
     *   so that an object has both gendered and ungendered usage.  This
     *   can be useful for collective objects, as well as for cases where
     *   gendered usage varies by speaker or situation, such as animals.
     */
    isIt
    {
        /* by default, we're an 'it' if we're not a 'him' or a 'her' */
        return !(isHim || isHer);
    }

    /*
     *   Test to see if we can match the pronouns 'him', 'her', 'it', and
     *   'them'.  By default, these simply test the corresponding isXxx
     *   flags (except 'canMatchThem', which tests 'isPlural' to see if the
     *   name has plural usage).
     */
    canMatchHim = (isHim)
    canMatchHer = (isHer)
    canMatchIt = (isIt)
    canMatchThem = (isPlural)

    /* can we match the given PronounXxx pronoun type specifier? */
    canMatchPronounType(typ)
    {
        /* check the type, and return the appropriate indicator property */
        switch (typ)
        {
        case PronounHim:
            return canMatchHim;

        case PronounHer:
            return canMatchHer;

        case PronounIt:
            return canMatchIt;

        case PronounThem:
            return canMatchThem;

        default:
            return nil;
        }
    }

    /*
     *   The grammatical cardinality of this item when it appears in a
     *   list.  This is used to ensure verb agreement when mentioning the
     *   item in a list of items.  ("Cardinality" is a fancy word for "how
     *   many items does this look like").
     *
     *   English only distinguishes two degrees of cardinality in its
     *   grammar: one, or many.  That is, when constructing a sentence, the
     *   only thing the grammar cares about is whether an object is
     *   singular or plural: IT IS on the table, THEY ARE on the table.
     *   Since English only distinguishes these two degrees, two is the
     *   same as a hundred is the same as a million for grammatical
     *   purposes, so we'll consider our cardinality to be 2 if we're
     *   plural, 1 otherwise.
     *
     *   Some languages don't express cardinality at all in their grammar,
     *   and others distinguish cardinality in greater detail than just
     *   singular-vs-plural, which is why this method has to be in the
     *   language-specific part of the library.
     */
    listCardinality(lister) { return isPlural ? 2 : 1; }

    /*
     *   Proper name flag.  This indicates that the 'name' property is the
     *   name of a person or place.  We consider proper names to be fully
     *   qualified, so we don't add articles for variations on the name
     *   such as 'theName'.
     */
    isProperName = nil

    /*
     *   Qualified name flag.  This indicates that the object name, as
     *   given by the 'name' property, is already fully qualified, so
     *   doesn't need qualification by an article like "the" or "a" when
     *   it appears in a sentence.  By default, a name is considered
     *   qualified if it's a proper name, but this can be overridden to
     *   mark a non-proper name as qualified when needed.
     */
    isQualifiedName = (isProperName)

    /*
     *   The name of the object - this is a string giving the object's
     *   short description, for constructing sentences that refer to the
     *   object by name.  Each instance should override this to define the
     *   name of the object.  This string should not contain any articles;
     *   we use this string as the root to generate various forms of the
     *   object's name for use in different places in sentences.
     */
    name = ''

    /*
     *   The name of the object, for the purposes of disambiguation
     *   prompts.  This should almost always be the object's ordinary
     *   name, so we return self.name by default.
     *
     *   In rare cases, it might be desirable to override this.  In
     *   particular, if a game has two objects that are NOT defined as
     *   basic equivalents of one another (which means that the parser
     *   will always ask for disambiguation when the two are ambiguous
     *   with one another), but the two nonetheless have identical 'name'
     *   properties, this property should be overridden for one or both
     *   objects to give them different names.  This will ensure that we
     *   avoid asking questions of the form "which do you mean, the coin,
     *   or the coin?".  In most cases, non-equivalent objects will have
     *   distinct 'name' properties to begin with, so this is not usually
     *   an issue.
     *
     *   When overriding this method, take care to override
     *   theDisambigName, aDisambigName, countDisambigName, and/or
     *   pluralDisambigName as needed.  Those routines must be overridden
     *   only when the default algorithms for determining articles and
     *   plurals fail to work properly for the disambigName (for example,
     *   the indefinite article algorithm fails with silent-h words like
     *   "hour", so if disambigName is "hour", aDisambigName must be
     *   overridden).  In most cases, the automatic algorithms will
     *   produce acceptable results, so the default implementations of
     *   these other routines can be used without customization.
     */
    disambigName = (name)

    /*
     *   The "equivalence key" is the value we use to group equivalent
     *   objects.  Note that we can only treat objects as equivalent when
     *   they're explicitly marked with isEquivalent=true, so the
     *   equivalence key is irrelevant for objects not so marked.
     *   
     *   Since the main point of equivalence is to allow creation of groups
     *   of like-named objects that are interchangeable in listings and in
     *   command input, we use the basic disambiguation name as the
     *   equivalence key.  
     */
    equivalenceKey = (disambigName)

    /*
     *   The definite-article name for disambiguation prompts.
     *
     *   By default, if the disambiguation name is identical to the
     *   regular name (i.e, the string returned by self.disambigName is
     *   the same as the string returned by self.name), then we simply
     *   return self.theName.  Since the base name is the same in either
     *   case, presumably the definite article names should be the same as
     *   well.  This way, if the object overrides theName to do something
     *   special, then we'll use the same definite-article name for
     *   disambiguation prompts.
     *
     *   If the disambigName isn't the same as the regular name, then
     *   we'll apply the same algorithm to the base disambigName that we
     *   normally do to the regular name to produce the theName.  This
     *   way, if the disambigName is overridden, we'll use the overridden
     *   disambigName to produce the definite-article version, using the
     *   standard definite-article algorithm.
     *
     *   Note that there's an aspect of this conditional approach that
     *   might not be obvious.  It might look as though the test is
     *   redundant: if name == disambigName, after all, and the default
     *   theName returns theNameFrom(name), then this ought to be
     *   identical to returning theNameFrom(disambigName).  The subtlety
     *   is that theName could be overridden to produce a custom result,
     *   in which case returning theNameFrom(disambigName) would return
     *   something different, which probably wouldn't be correct: the
     *   whole reason theName would be overridden is that the algorithmic
     *   determination (theNameFrom) gets it wrong.  So, by calling
     *   theName directly when disambigName is the same as name, we are
     *   assured that we pick up any override in theName.
     *
     *   Note that in rare cases, neither of these default approaches will
     *   produce the right result; this will happen if the object uses a
     *   custom disambigName, but that name doesn't fit the normal
     *   algorithmic pattern for applying a definite article.  In these
     *   cases, the object should simply override this method to specify
     *   the custom name.
     */
    theDisambigName = (name == disambigName
                       ? theName : theNameFrom(disambigName))

    /*
     *   The indefinite-article name for disambiguation prompts.  We use
     *   the same logic here as in theDisambigName.
     */
    aDisambigName = (name == disambigName ? aName : aNameFrom(disambigName))

    /*
     *   The counted name for disambiguation prompts.  We use the same
     *   logic here as in theDisambigName.
     */
    countDisambigName(cnt)
    {
        return (name == disambigName && pluralName == pluralDisambigName
                ? countName(cnt)
                : countNameFrom(cnt, disambigName, pluralDisambigName));
    }

    /*
     *   The plural name for disambiguation prompts.  We use the same
     *   logic here as in theDisambigName.
     */
    pluralDisambigName = (name == disambigName
                          ? pluralName : pluralNameFrom(disambigName))

    /*
     *   The name of the object, for the purposes of disambiguation prompts
     *   to disambiguation among this object and basic equivalents of this
     *   object (i.e., objects of the same class marked with
     *   isEquivalent=true).
     *
     *   This is used in disambiguation prompts in place of the actual text
     *   typed by the user.  For example, suppose the user types ">take
     *   coin", then we ask for help disambiguating, and the player types
     *   ">gold".  This narrows things down to, say, three gold coins, but
     *   they're in different locations so we need to ask for further
     *   disambiguation.  Normally, we ask "which gold do you mean",
     *   because the player typed "gold" in the input.  Once we're down to
     *   equivalents, we don't have to rely on the input text any more,
     *   which is good because the input text could be fragmentary (as in
     *   our present example).  Since we have only equivalents, we can use
     *   the actual name of the objects (they're all the same, after all).
     *   This property gives the name we use.
     *
     *   For English, this is simply the object's ordinary disambiguation
     *   name.  This property is separate from 'name' and 'disambigName'
     *   for the sake of languages that need to use an inflected form in
     *   this context.
     */
    disambigEquivName = (disambigName)

    /*
     *   Single-item listing description.  This is used to display the
     *   item when it appears as a single (non-grouped) item in a list.
     *   By default, we just show the indefinite article description.
     */
    listName = (aName)

    /*
     *   Return a string giving the "counted name" of the object - that is,
     *   a phrase describing the given number of the object.  For example,
     *   for a red book, and a count of 5, we might return "five red
     *   books".  By default, we use countNameFrom() to construct a phrase
     *   from the count and either our regular (singular) 'name' property
     *   or our 'pluralName' property, according to whether count is 1 or
     *   more than 1.  
     */
    countName(count) { return countNameFrom(count, name, pluralName); }

    /*
     *   Returns a string giving a count applied to the name string.  The
     *   name must be given in both singular and plural forms.
     */
    countNameFrom(count, singularStr, pluralStr)
    {
        /* if the count is one, use 'one' plus the singular name */
        if (count == 1)
            return 'one ' + singularStr;

        /*
         *   Get the number followed by a space - spell out numbers below
         *   100, but use numerals to denote larger numbers.  Append the
         *   plural name to the number and return the result.
         */
        return spellIntBelowExt(count, 100, 0, DigitFormatGroupSep)
            + ' ' + pluralStr;
    }

    /*
     *   Get the 'pronoun selector' for the various pronoun methods.  This
     *   returns:
     *   
     *.  - singular neuter = 1
     *.  - singular masculine = 2
     *.  - singular feminine = 3
     *.  - plural = 4
     */
    pronounSelector = (isPlural ? 4 : isHer ? 3 : isHim ? 2 : 1)

    /*
     *   get a string with the appropriate pronoun for the object for the
     *   nominative case, objective case, possessive adjective, possessive
     *   noun
     */
    itNom { return ['it', 'he', 'she', 'they'][pronounSelector]; }
    itObj { return ['it', 'him', 'her', 'them'][pronounSelector]; }
    itPossAdj { return ['its', 'his', 'her', 'their'][pronounSelector]; }
    itPossNoun { return ['its', 'his', 'hers', 'theirs'][pronounSelector]; }

    /* get the object reflexive pronoun (itself, etc) */
    itReflexive
    {
        return ['itself', 'himself', 'herself', 'themselves']
               [pronounSelector];
    }

    /* demonstrative pronouns ('that' or 'those') */
    thatNom { return ['that', 'he', 'she', 'those'][pronounSelector]; }
    thatIsContraction
    {
        return thatNom + tSel(isPlural ? ' are' : '&rsquo;s', ' ' + verbToBe);
    }
    thatObj { return ['that', 'him', 'her', 'those'][pronounSelector]; }

    /*
     *   get a string with the appropriate pronoun for the object plus the
     *   correct conjugation of 'to be'
     */
    itIs { return itNom + ' ' + verbToBe; }

    /* get a pronoun plus a 'to be' contraction */
    itIsContraction
    {
        return itNom
            + tSel(isPlural ? '&rsquo;re' : '&rsquo;s', ' ' + verbToBe);
    }

    /*
     *   get a string with the appropriate pronoun for the object plus the
     *   correct conjugation of the given regular verb for the appropriate
     *   person
     */
    itVerb(verb)
    {
        return itNom + ' ' + conjugateRegularVerb(verb);
    }

    /*
     *   Conjugate a regular verb in the present or past tense for our
     *   person and number.
     *
     *   In the present tense, this is pretty easy: we add an 's' for the
     *   third person singular, and leave the verb unchanged for plural (it
     *   asks, they ask).  The only complication is that we must check some
     *   special cases to add the -s suffix: -y -> -ies (it carries), -o ->
     *   -oes (it goes).
     *
     *   In the past tense, we can equally easily figure out when to use
     *   -d, -ed, or -ied.  However, we have a more serious problem: for
     *   some verbs, the last consonant of the verb stem should be repeated
     *   (as in deter -> deterred), and for others it shouldn't (as in
     *   gather -> gathered).  To figure out which rule applies, we would
     *   sometimes need to know whether the last syllable is stressed, and
     *   unfortunately there is no easy way to determine that
     *   programmatically.
     *
     *   Therefore, we do *not* handle the case where the last consonant is
     *   repeated in the past tense.  You shouldn't use this method for
     *   this case; instead, treat it as you would handle an irregular
     *   verb, by explicitly specifying the correct past tense form via the
     *   tSel macro.  For example, to generate the properly conjugated form
     *   of the verb "deter" for an object named "thing", you could use an
     *   expression such as:
     *
     *   'deter' + tSel(thing.verbEndingS, 'red')
     *
     *   This would correctly generate "deter", "deters", or "deterred"
     *   depending on the number of the object named "thing" and on the
     *   current narrative tense.
     */
    conjugateRegularVerb(verb)
    {
        /*
         *   Which tense are we currently using?
         */
        if (gameMain.usePastTense)
        {
            /*
             *   We want the past tense form.
             *
             *   If the last letter is 'e', simply add 'd'.
             */
            if (verb.endsWith('e')) return verb + 'd';

            /*
             *   Otherwise, if the verb ending would become 'ies' in the
             *   third-person singular present, then it becomes 'ied' in
             *   the past.
             */
            else if (rexMatch(iesEndingPat, verb))
                    return verb.substr(1, verb.length() - 1) + 'ied';

            /*
             *   Otherwise, use 'ed' as the ending.  Don't try to determine
             *   if the last consonant should be repeated: that's too
             *   complicated.  We'll just ignore the possibility.
             */
            else return verb + 'ed';
        }
        else
        {
            /*
             *   We want the present tense form.
             *
             *   Check our number and person.
             */
            if (isPlural)
            {
                /*
                 *   We're plural, so simply use the base verb form ("they
                 *   ask").
                 */
                return verb;
            }
            else
            {
                /*
                 *   Third-person singular, so we must add the -s suffix.
                 *   Check for special spelling cases:
                 *
                 *   '-y' changes to '-ies', unless the 'y' is preceded by
                 *   a vowel
                 *
                 *   '-sh', '-ch', and '-o' endings add suffix '-es'
                 */
                if (rexMatch(iesEndingPat, verb))
                    return verb.substr(1, verb.length() - 1) + 'ies';
                else if (rexMatch(esEndingPat, verb))
                    return verb + 'es';
                else
                    return verb + 's';
            }
        }
    }

    /* verb-ending patterns for figuring out which '-s' ending to add */
    iesEndingPat = static new RexPattern('.*[^aeiou]y$')
    esEndingPat = static new RexPattern('.*(o|ch|sh)$')

    /*
     *   Get the name with a definite article ("the box").  By default, we
     *   use our standard definite article algorithm to apply an article
     *   to self.name.
     *
     *   The name returned must be in the nominative case (which makes no
     *   difference unless the name is a pronoun, since in English
     *   ordinary nouns don't vary according to how they're used in a
     *   sentence).
     */
    theName = (theNameFrom(name))

    /*
     *   theName in objective case.  In most cases, this is identical to
     *   the normal theName, so we use that by default.  This must be
     *   overridden if theName is a pronoun (which is usually only the
     *   case for player character actors; see our language-specific Actor
     *   modifications for information on that case).
     */
    theNameObj { return theName; }

    /*
     *   Generate the definite-article name from the given name string.
     *   If my name is already qualified, don't add an article; otherwise,
     *   add a 'the' as the prefixed definite article.
     */
    theNameFrom(str) { return (isQualifiedName ? '' : 'the ') + str; }

    /*
     *   theName as a possessive adjective (Bob's book, your book).  If the
     *   name's usage is singular (i.e., isPlural is nil), we'll simply add
     *   an apostrophe-S.  If the name is plural, and it ends in an "s",
     *   we'll just add an apostrophe (no S).  If it's plural and doesn't
     *   end in "s", we'll add an apostrophe-S.
     *
     *   Note that some people disagree about the proper usage for
     *   singular-usage words (especially proper names) that end in 's'.
     *   Some people like to use a bare apostrophe for any name that ends
     *   in 's' (so Chris -> Chris'); other people use apostrophe-s for
     *   singular words that end in an "s" sound and a bare apostrophe for
     *   words that end in an "s" that sounds like a "z" (so Charles
     *   Dickens -> Charles Dickens').  However, most usage experts agree
     *   that proper names take an apostrophe-S in almost all cases, even
     *   when ending with an "s": "Chris's", "Charles Dickens's".  That's
     *   what we do here.
     *
     *   Note that this algorithm doesn't catch all of the special
     *   exceptions in conventional English usage.  For example, Greek
     *   names ending with "-es" are usually written with the bare
     *   apostrophe, but we don't have a property that tells us whether the
     *   name is Greek or not, so we can't catch this case.  Likewise, some
     *   authors like to possessive-ize words that end with an "s" sound
     *   with a bare apostrophe, as in "for appearance' sake", and we don't
     *   attempt to catch these either.  For any of these exceptions, you
     *   must override this method for the individual object.
     */
    theNamePossAdj
    {
        /* add apostrophe-S, unless it's a plural ending with 's' */
        return theName
            + (isPlural && theName.endsWith('s') ? '&rsquo;' : '&rsquo;s');
    }

    /*
     *   TheName as a possessive noun (that is Bob's, that is yours).  We
     *   simply return the possessive adjective name, since the two forms
     *   are usually identical in English (except for pronouns, where they
     *   sometimes differ: "her" for the adjective vs "hers" for the noun).
     */
    theNamePossNoun = (theNamePossAdj)

    /*
     *   theName with my nominal owner explicitly stated, if we have a
     *   nominal owner: "your backpack," "Bob's flashlight."  If we have
     *   no nominal owner, this is simply my theName.
     */
    theNameWithOwner()
    {
        local owner;

        /*
         *   if we have a nominal owner, show with our owner name;
         *   otherwise, just show our regular theName
         */
        if ((owner = getNominalOwner()) != nil)
            return owner.theNamePossAdj + ' ' + name;
        else
            return theName;
    }

    /*
     *   Default preposition to use when an object is in/on this object.
     *   By default, we use 'in' as the preposition; subclasses can
     *   override to use others (such as 'on' for a surface).
     */
    objInPrep = 'in'

    /*
     *   Default preposition to use when an actor is in/on this object (as
     *   a nested location), and full prepositional phrase, with no article
     *   and with an indefinite article.  By default, we use the objInPrep
     *   for actors as well.
     */
    actorInPrep = (objInPrep)

    /* preposition to use when an actor is being removed from this location */
    actorOutOfPrep = 'out of'

    /* preposition to use when an actor is being moved into this location */
    actorIntoPrep
    {
        if (actorInPrep is in ('in', 'on'))
            return actorInPrep + 'to';
        else
            return actorInPrep;
    }

    /*
     *   describe an actor as being in/being removed from/being moved into
     *   this location
     */
    actorInName = (actorInPrep + ' ' + theNameObj)
    actorInAName = (actorInPrep + ' ' + aNameObj)
    actorOutOfName = (actorOutOfPrep + ' ' + theNameObj)
    actorIntoName = (actorIntoPrep + ' ' + theNameObj)

    /*
     *   A prepositional phrase that can be used to describe things that
     *   are in this room as seen from a remote point of view.  This
     *   should be something along the lines of "in the airlock", "at the
     *   end of the alley", or "on the lawn".
     *
     *   'pov' is the point of view from which we're seeing this room;
     *   this might be
     *
     *   We use this phrase in cases where we need to describe things in
     *   this room when viewed from a point of view outside of the room
     *   (i.e., in a different top-level room).  By default, we'll use our
     *   actorInName.
     */
    inRoomName(pov) { return actorInName; }

    /*
     *   Provide the prepositional phrase for an object being put into me.
     *   For a container, for example, this would say "into the box"; for
     *   a surface, it would say "onto the table."  By default, we return
     *   our library message given by our putDestMessage property; this
     *   default is suitable for most cases, but individual objects can
     *   customize as needed.  When customizing this, be sure to make the
     *   phrase suitable for use in sentences like "You put the book
     *   <<putInName>>" and "The book falls <<putInName>>" - the phrase
     *   should be suitable for a verb indicating active motion by the
     *   object being received.
     */
    putInName() { return gLibMessages.(putDestMessage)(self); }

    /*
     *   Get a description of an object within this object, describing the
     *   object's location as this object.  By default, we'll append "in
     *   <theName>" to the given object name.
     */
    childInName(childName)
        { return childInNameGen(childName, theName); }

    /*
     *   Get a description of an object within this object, showing the
     *   owner of this object.  This is similar to childInName, but
     *   explicitly shows the owner of the containing object, if any: "the
     *   flashlight in bob's backpack".
     */
    childInNameWithOwner(childName)
        { return childInNameGen(childName, theNameWithOwner); }

    /*
     *   get a description of an object within this object, as seen from a
     *   remote location
     */
    childInRemoteName(childName, pov)
        { return childInNameGen(childName, inRoomName(pov)); }

    /*
     *   Base routine for generating childInName and related names.  Takes
     *   the name to use for the child and the name to use for me, and
     *   combines them appropriately.
     *
     *   In most cases, this is the only one of the various childInName
     *   methods that needs to be overridden per subclass, since the others
     *   are defined in terms of this one.  Note also that if the only
     *   thing you need to do is change the preposition from 'in' to
     *   something else, you can just override objInPrep instead.
     */
    childInNameGen(childName, myName)
        { return childName + ' ' + objInPrep + ' ' + myName; }

    /*
     *   Get my name (in various forms) distinguished by my owner or
     *   location.
     *
     *   If the object has an owner, and either we're giving priority to
     *   the owner or our immediate location is the same as the owner,
     *   we'll show using a possessive form with the owner ("bob's
     *   flashlight").  Otherwise, we'll show the name distinguished by
     *   our immediate container ("the flashlight in the backpack").
     *
     *   These are used by the ownership and location distinguishers to
     *   list objects according to owners in disambiguation lists.  The
     *   ownership distinguisher gives priority to naming by ownership,
     *   regardless of the containment relationship between owner and
     *   self; the location distinguisher gives priority to naming by
     *   location, showing the owner only if the owner is the same as the
     *   location.
     *
     *   We will presume that objects with proper names are never
     *   indistinguishable from other objects with proper names, so we
     *   won't worry about cases like "Bob's Bill".  This leaves us free
     *   to use appropriate articles in all cases.
     */
    aNameOwnerLoc(ownerPriority)
    {
        local owner;

        /* show in owner or location format, as appropriate */
        if ((owner = getNominalOwner()) != nil
            && (ownerPriority || isDirectlyIn(owner)))
        {
            local ret;

            /*
             *   we have an owner - show as "one of Bob's items" (or just
             *   "Bob's items" if this is a mass noun or a proper name)
             */
            ret = owner.theNamePossAdj + ' ' + pluralName;
            if (!isMassNoun && !isPlural)
                ret = 'one of ' + ret;

            /* return the result */
            return ret;
        }
        else
        {
            /* we have no owner - show as "an item in the location" */
            return location.childInNameWithOwner(aName);
        }
    }
    theNameOwnerLoc(ownerPriority)
    {
        local owner;

        /* show in owner or location format, as appropriate */
        if ((owner = getNominalOwner()) != nil
            && (ownerPriority || isDirectlyIn(owner)))
        {
            /* we have an owner - show as "Bob's item" */
            return owner.theNamePossAdj + ' ' + name;
        }
        else
        {
            /* we have no owner - show as "the item in the location" */
            return location.childInNameWithOwner(theName);
        }
    }
    countNameOwnerLoc(cnt, ownerPriority)
    {
        local owner;

        /* show in owner or location format, as appropriate */
        if ((owner = getNominalOwner()) != nil
            && (ownerPriority || isDirectlyIn(owner)))
        {
            /* we have an owner - show as "Bob's five items" */
            return owner.theNamePossAdj + ' ' + countName(cnt);
        }
        else
        {
            /* we have no owner - show as "the five items in the location" */
            return location.childInNameWithOwner('the ' + countName(cnt));
        }
    }

    /*
     *   Note that I'm being used in a disambiguation prompt by
     *   owner/location.  If we're showing the owner, we'll set the
     *   antecedent for the owner's pronoun, if the owner is a 'him' or
     *   'her'; this allows the player to refer back to our prompt text
     *   with appropriate pronouns.
     */
    notePromptByOwnerLoc(ownerPriority)
    {
        local owner;

        /* show in owner or location format, as appropriate */
        if ((owner = getNominalOwner()) != nil
            && (ownerPriority || isDirectlyIn(owner)))
        {
            /* we are showing by owner - let the owner know about it */
            owner.notePromptByPossAdj();
        }
    }

    /*
     *   Note that we're being used in a prompt question with our
     *   possessive adjective.  If we're a 'him' or a 'her', set our
     *   pronoun antecedent so that the player's response to the prompt
     *   question can refer back to the prompt text by pronoun.
     */
    notePromptByPossAdj()
    {
        if (isHim)
            gPlayerChar.setHim(self);
        if (isHer)
            gPlayerChar.setHer(self);
    }

    /*
     *   My name with an indefinite article.  By default, we figure out
     *   which article to use (a, an, some) automatically.
     *
     *   In rare cases, the automatic determination might get it wrong,
     *   since some English spellings defy all of the standard
     *   orthographic rules and must simply be handled as special cases;
     *   for example, the algorithmic determination doesn't know about
     *   silent-h words like "hour".  When the automatic determination
     *   gets it wrong, simply override this routine to specify the
     *   correct article explicitly.
     */
    aName = (aNameFrom(name))

    /* the indefinite-article name in the objective case */
    aNameObj { return aName; }

    /*
     *   Apply an indefinite article ("a box", "an orange", "some lint")
     *   to the given name.  We'll try to figure out which indefinite
     *   article to use based on what kind of noun phrase we use for our
     *   name (singular, plural, or a "mass noun" like "lint"), and our
     *   spelling.
     *
     *   By default, we'll use the article "a" if the name starts with a
     *   consonant, or "an" if it starts with a vowel.
     *
     *   If the name starts with a "y", we'll look at the second letter;
     *   if it's a consonant, we'll use "an", otherwise "a" (hence "an
     *   yttrium block" but "a yellow brick").
     *
     *   If the object is marked as having plural usage, we will use
     *   "some" as the article ("some pants" or "some shrubs").
     *
     *   Some objects will want to override the default behavior, because
     *   the lexical rules about when to use "a" and "an" are not without
     *   exception.  For example, silent-"h" words ("honor") are written
     *   with "an", and "h" words with a pronounced but weakly stressed
     *   initial "h" are sometimes used with "an" ("an historian").  Also,
     *   some 'y' words might not follow the generic 'y' rule.
     *
     *   'U' words are especially likely not to follow any lexical rule -
     *   any 'u' word that sounds like it starts with 'y' should use 'a'
     *   rather than 'an', but there's no good way to figure that out just
     *   looking at the spelling (consider "a universal symbol" and "an
     *   unimportant word", or "a unanimous decision" and "an unassuming
     *   man").  We simply always use 'an' for a word starting with 'u',
     *   but this will have to be overridden when the 'u' sounds like 'y'.
     */
    aNameFrom(str)
    {
        /* remember the original source string */
        local inStr = str;

        /*
         *   The complete list of unaccented, accented, and ligaturized
         *   Latin vowels from the Unicode character set.  (The Unicode
         *   database doesn't classify characters as vowels or the like,
         *   so it seems the only way we can come up with this list is
         *   simply to enumerate the vowels.)
         *
         *   These are all lower-case letters; all of these are either
         *   exclusively lower-case or have upper-case equivalents that
         *   map to these lower-case letters.
         *
         *   (Note an implementation detail: the compiler will append all
         *   of these strings together at compile time, so we don't have
         *   to perform all of this concatenation work each time we
         *   execute this method.)
         *
         *   Note that we consider any word starting with an '8' to start
         *   with a vowel, since 'eight' and 'eighty' both take 'an'.
         */
        local vowels = '8aeiou\u00E0\u00E1\u00E2\u00E3\u00E4\u00E5\u00E6'
                       + '\u00E8\u00E9\u00EA\u00EB\u00EC\u00ED\u00EE\u00EF'
                       + '\u00F2\u00F3\u00F4\u00F5\u00F6\u00F8\u00F9\u00FA'
                       + '\u00FB\u00FC\u0101\u0103\u0105\u0113\u0115\u0117'
                       + '\u0119\u011B\u0129\u012B\u012D\u012F\u014D\u014F'
                       + '\u0151\u0169\u016B\u016D\u016F\u0171\u0173\u01A1'
                       + '\u01A3\u01B0\u01CE\u01D0\u01D2\u01D4\u01D6\u01D8'
                       + '\u01DA\u01DC\u01DF\u01E1\u01E3\u01EB\u01ED\u01FB'
                       + '\u01FD\u01FF\u0201\u0203\u0205\u0207\u0209\u020B'
                       + '\u020D\u020F\u0215\u0217\u0254\u025B\u0268\u0289'
                       + '\u1E01\u1E15\u1E17\u1E19\u1E1B\u1E1D\u1E2D\u1E2F'
                       + '\u1E4D\u1E4F\u1E51\u1E53\u1E73\u1E75\u1E77\u1E79'
                       + '\u1E7B\u1E9A\u1EA1\u1EA3\u1EA5\u1EA7\u1EA9\u1EAB'
                       + '\u1EAD\u1EAF\u1EB1\u1EB3\u1EB5\u1EB7\u1EB9\u1EBB'
                       + '\u1EBD\u1EBF\u1EC1\u1EC3\u1EC5\u1EC7\u1EC9\u1ECB'
                       + '\u1ECD\u1ECF\u1ED1\u1ED3\u1ED5\u1ED7\u1ED9\u1EDB'
                       + '\u1EDD\u1EDF\u1EE1\u1EE3\u1EE5\u1EE7\u1EE9\u1EEB'
                       + '\u1EED\u1EEF\u1EF1\uFF41\uFF4F\uFF55';

        /*
         *   A few upper-case vowels in unicode don't have lower-case
         *   mappings - consider them separately.
         */
        local vowelsUpperOnly = '\u0130\u019f';

        /*
         *   the various accented forms of the letter 'y' - these are all
         *   lower-case versions; the upper-case versions all map to these
         */
        local ys = 'y\u00FD\u00FF\u0177\u01B4\u1E8F\u1E99\u1EF3'
                   + '\u1EF5\u1EF7\u1EF9\u24B4\uFF59';

        /* if the name is already qualified, don't add an article at all */
        if (isQualifiedName)
            return str;

        /* if it's plural or a mass noun, use "some" as the article */
        if (isPlural || isMassNoun)
        {
            /* use "some" as the article */
            return 'some ' + str;
        }
        else
        {
            local firstChar;
            local firstCharLower;

            /* if it's empty, just use "a" */
            if (inStr == '')
                return 'a';

            /* get the first character of the name */
            firstChar = inStr.substr(1, 1);

            /* skip any leading HTML tags */
            if (rexMatch(patTagOrQuoteChar, firstChar) != nil)
            {
                /*
                 *   Scan for tags.  Note that this pattern isn't quite
                 *   perfect, as it won't properly ignore close-brackets
                 *   that are inside quoted material, but it should be good
                 *   enough for nearly all cases in practice.  In cases too
                 *   complex for this pattern, the object will simply have
                 *   to override aDesc.
                 */
                local len = rexMatch(patLeadingTagOrQuote, inStr);

                /* if we got a match, strip out the leading tags */
                if (len != nil)
                {
                    /* strip off the leading tags */
                    inStr = inStr.substr(len + 1);

                    /* re-fetch the first character */
                    firstChar = inStr.substr(1, 1);
                }
            }

            /* get the lower-case version of the first character */
            firstCharLower = firstChar.toLower();

            /*
             *   if the first word of the name is only one letter long,
             *   treat it specially
             */
            if (rexMatch(patOneLetterWord, inStr) != nil)
            {
                /*
                 *   We have a one-letter first word, such as "I-beam" or
                 *   "M-ray sensor", or just "A".  Choose the article based
                 *   on the pronunciation of the letter as a letter.
                 */
                return (rexMatch(patOneLetterAnWord, inStr) != nil
                        ? 'an ' : 'a ') + str;
            }

            /*
             *   look for the first character in the lower-case and
             *   upper-case-only vowel lists - if we find it, it takes
             *   'an'
             */
            if (vowels.find(firstCharLower) != nil
                || vowelsUpperOnly.find(firstChar) != nil)
            {
                /* it starts with a vowel */
                return 'an ' + str;
            }
            else if (ys.find(firstCharLower) != nil)
            {
                local secondChar;

                /* get the second character, if there is one */
                secondChar = inStr.substr(2, 1);

                /*
                 *   It starts with 'y' - if the second letter is a
                 *   consonant, assume the 'y' is a vowel sound, hence we
                 *   should use 'an'; otherwise assume the 'y' is a
                 *   diphthong 'ei' sound, which means we should use 'a'.
                 *   If there's no second character at all, or the second
                 *   character isn't alphabetic, use 'a' - "a Y" or "a
                 *   Y-connector".
                 */
                if (secondChar == ''
                    || rexMatch(patIsAlpha, secondChar) == nil
                    || vowels.find(secondChar.toLower()) != nil
                    || vowelsUpperOnly.find(secondChar) != nil)
                {
                    /*
                     *   it's just one character, or the second character
                     *   is non-alphabetic, or the second character is a
                     *   vowel - in any of these cases, use 'a'
                     */
                    return 'a ' + str;
                }
                else
                {
                    /* the second character is a consonant - use 'an' */
                    return 'an ' + str;
                }
            }
            else if (rexMatch(patElevenEighteen, inStr) != nil)
            {
                /*
                 *   it starts with '11' or '18', so it takes 'an' ('an
                 *   11th-hour appeal', 'an 18-hole golf course')
                 */
                return 'an ' + str;
            }
            else
            {
                /* it starts with a consonant */
                return 'a ' + str;
            }
        }
    }

    /* pre-compile some regular expressions for aName */
    patTagOrQuoteChar = static new RexPattern('[<"\']')
    patLeadingTagOrQuote = static new RexPattern(
        '(<langle><^rangle>+<rangle>|"|\')+')
    patOneLetterWord = static new RexPattern('<alpha>(<^alpha>|$)')
    patOneLetterAnWord = static new RexPattern('<nocase>[aefhilmnorsx]')
    patIsAlpha = static new RexPattern('<alpha>')
    patElevenEighteen = static new RexPattern('1[18](<^digit>|$)')

    /*
     *   Get the default plural name.  By default, we'll use the
     *   algorithmic plural determination, which is based on the spelling
     *   of the name.
     *
     *   The algorithm won't always get it right, since some English
     *   plurals are irregular ("men", "children", "Attorneys General").
     *   When the name doesn't fit the regular spelling patterns for
     *   plurals, the object should simply override this routine to return
     *   the correct plural name string.
     */
    pluralName = (pluralNameFrom(name))

    /*
     *   Get the plural form of the given name string.  If the name ends in
     *   anything other than 'y', we'll add an 's'; otherwise we'll replace
     *   the 'y' with 'ies'.  We also handle abbreviations and individual
     *   letters specially.
     *
     *   This can only deal with simple adjective-noun forms.  For more
     *   complicated forms, particularly for compound words, it must be
     *   overridden (e.g., "Attorney General" -> "Attorneys General",
     *   "man-of-war" -> "men-of-war").  Likewise, names with irregular
     *   plurals ('child' -> 'children', 'man' -> 'men') must be handled
     *   with overrides.
     */
    pluralNameFrom(str)
    {
        local len;
        local lastChar;
        local lastPair;

        /*
         *   if it's marked as having plural usage, just use the ordinary
         *   name, since it's already plural
         */
        if (isPlural)
            return str;

        /* check for a 'phrase of phrase' format */
        if (rexMatch(patOfPhrase, str) != nil)
        {
            local ofSuffix;

            /*
             *   Pull out the two parts - the part up to the 'of' is the
             *   part we'll actually pluralize, and the rest is a suffix
             *   we'll stick on the end of the pluralized part.
             */
            str = rexGroup(1)[3];
            ofSuffix = rexGroup(2)[3];

            /*
             *   now pluralize the part up to the 'of' using the normal
             *   rules, then add the rest back in at the end
             */
            return pluralNameFrom(str) + ofSuffix;
        }

        /* if there's no short description, return an empty string */
        len = str.length();
        if (len == 0)
            return '';

        /*
         *   If it's only one character long, handle it specially.  If it's
         *   a lower-case letter, add an apostrophe-S.  If it's a capital
         *   A, E, I, M, U, or V, we'll add apostrophe-S (because these
         *   could be confused with words or common abbreviations if we
         *   just added "s": As, Es, Is, Ms, Us, Vs).  If it's anything
         *   else (any other capital letter, or any non-letter character),
         *   we'll just add an "s".
         */
        if (len == 1)
        {
            if (rexMatch(patSingleApostropheS, str) != nil)
                return str + '&rsquo;s';
            else
                return str + 's';
        }

        /* get the last character of the name, and the last pair of chars */
        lastChar = str.substr(len, 1);
        lastPair = (len == 1 ? lastChar : str.substr(len - 1, 2));

        /*
         *   If the last letter is a capital letter, assume it's an
         *   abbreviation without embedded periods (CPA, PC), in which case
         *   we just add an "s" (CPAs, PCs).  Likewise, if it's a number,
         *   just add "s": "the 1940s", "the low 20s".
         */
        if (rexMatch(patUpperOrDigit, lastChar) != nil)
            return str + 's';

        /*
         *   If the last character is a period, it must be an abbreviation
         *   with embedded periods (B.A., B.S., Ph.D.).  In these cases,
         *   add an apostrophe-S.
         */
        if (lastChar == '.')
            return str + '&rsquo;s';

        /*
         *   If it ends in a non-vowel followed by 'y', change -y to -ies.
         *   (This doesn't apply if a vowel precedes a terminal 'y'; in
         *   such cases, we'll use the normal '-s' ending instead: "survey"
         *   -> "surveys", "essay" -> "essays", "day" -> "days".)
         */
        if (rexMatch(patVowelY, lastPair) != nil)
            return str.substr(1, len - 1) + 'ies';

        /* if it ends in s, x, z, or h, add -es */
        if ('sxzh'.find(lastChar) != nil)
            return str + 'es';

        /* for anything else, just add -s */
        return str + 's';
    }

    /* some pre-compiled patterns for pluralName */
    patSingleApostropheS = static new RexPattern('<case><lower|A|E|I|M|U|V>')
    patUpperOrDigit = static new RexPattern('<upper|digit>')
    patVowelY = static new RexPattern('[^aeoiu]y')
    patOfPhrase = static new RexPattern(
        '<nocase>(.+?)(<space>+of<space>+.+)')

    /* get my name plus a being verb ("the box is") */
    nameIs { return theName + ' ' + verbToBe; }

    /* get my name plus a negative being verb ("the box isn't") */
    nameIsnt { return nameIs + 'n&rsquo;t'; }

    /*
     *   My name with the given regular verb in agreement: in the present
     *   tense, if my name has singular usage, we'll add 's' to the verb,
     *   otherwise we won't.  In the past tense, we'll add 'd' (or 'ed').
     *   This can't be used with irregular verbs, or with regular verbs
     *   that have the last consonant repeated before the past -ed ending,
     *   such as "deter".
     */
    nameVerb(verb) { return theName + ' ' + conjugateRegularVerb(verb); }

    /* being verb agreeing with this object as subject */
    verbToBe
    {
        return tSel(isPlural ? 'are' : 'is', isPlural ? 'were' : 'was');
    }

    /* past tense being verb agreeing with object as subject */
    verbWas { return tSel(isPlural ? 'were' : 'was', 'had been'); }

    /* 'have' verb agreeing with this object as subject */
    verbToHave { return tSel(isPlural ? 'have' : 'has', 'had'); }

    /*
     *   A few common irregular verbs and name-plus-verb constructs,
     *   defined for convenience.
     */
    verbToDo = (tSel('do' + verbEndingEs, 'did'))
    nameDoes = (theName + ' ' + verbToDo)
    verbToGo = (tSel('go' + verbEndingEs, 'went'))
    verbToCome = (tSel('come' + verbEndingS, 'came'))
    verbToLeave = (tSel('leave' + verbEndingS, 'left'))
    verbToSee = (tSel('see' + verbEndingS, 'saw'))
    nameSees = (theName + ' ' + verbToSee)
    verbToSay = (tSel('say' + verbEndingS, 'said'))
    nameSays = (theName + ' ' + verbToSay)
    verbMust = (tSel('must', 'had to'))
    verbCan = (tSel('can', 'could'))
    verbCannot = (tSel('cannot', 'could not'))
    verbCant = (tSel('can&rsquo;t', 'couldn&rsquo;t'))
    verbWill = (tSel('will', 'would'))
    verbWont = (tSel('won&rsquo;t', 'wouldn&rsquo;t'))

    /*
     *   Verb endings for regular '-s' verbs, agreeing with this object as
     *   the subject.  We define several methods each of which handles the
     *   past tense differently.
     *
     *   verbEndingS doesn't try to handle the past tense at all - use it
     *   only in places where you know for certain that you'll never need
     *   the past tense form, or in expressions constructed with the tSel
     *   macro: use verbEndingS as the macro's first argument, and specify
     *   the past tense ending explicitly as the second argument.  For
     *   example, you could generate the correctly conjugated form of the
     *   verb "to fit" for an object named "key" with an expression such
     *   as:
     *
     *   'fit' + tSel(key.verbEndingS, 'ted')
     *
     *   This would generate 'fit', 'fits', or 'fitted' according to number
     *   and tense.
     *
     *   verbEndingSD and verbEndingSEd return 'd' and 'ed' respectively in
     *   the past tense.
     *
     *   verbEndingSMessageBuilder_ is for internal use only: it assumes
     *   that the correct ending to be displayed in the past tense is
     *   stored in langMessageBuilder.pastEnding_.  It is used as part of
     *   the string parameter substitution mechanism.
     */
    verbEndingS { return isPlural ? '' : 's'; }
    verbEndingSD = (tSel(verbEndingS, 'd'))
    verbEndingSEd = (tSel(verbEndingS, 'ed'))
    verbEndingSMessageBuilder_ =
        (tSel(verbEndingS, langMessageBuilder.pastEnding_))

    /*
     *   Verb endings (present or past) for regular '-es/-ed' and
     *   '-y/-ies/-ied' verbs, agreeing with this object as the subject.
     */
    verbEndingEs { return tSel(isPlural ? '' : 'es', 'ed'); }
    verbEndingIes { return tSel(isPlural ? 'y' : 'ies', 'ied'); }

    /*
     *   Dummy name - this simply displays nothing; it's used for cases
     *   where messageBuilder substitutions want to refer to an object (for
     *   internal bookkeeping) without actually showing the name of the
     *   object in the output text.  This should always simply return an
     *   empty string.
     */
    dummyName = ''

    /*
     *   Invoke a property (with an optional argument list) on this object
     *   while temporarily switching to the present tense, and return the
     *   result.
     */
    propWithPresent(prop, [args])
    {
        return withPresent({: self.(prop)(args...)});
    }

    /*
     *   Method for internal use only: invoke on this object the property
     *   stored in langMessageBuilder.fixedTenseProp_ while temporarily
     *   switching to the present tense, and return the result.  This is
     *   used as part of the string parameter substitution mechanism.
     */
    propWithPresentMessageBuilder_
    {
        return propWithPresent(langMessageBuilder.fixedTenseProp_);
    }

    /*
     *   For the most part, "strike" has the same meaning as "hit", so
     *   define this as a synonym for "attack" most objects.  There are a
     *   few English idioms where "strike" means something different, as
     *   in "strike match" or "strike tent."
     */
    dobjFor(Strike) asDobjFor(Attack)
;

/* ------------------------------------------------------------------------ */
/*
 *   An object that uses the same name as another object.  This maps all of
 *   the properties involved in supplying the object's name, number, and
 *   other usage information from this object to a given target object, so
 *   that all messages involving this object use the same name as the
 *   target object.  This is a mix-in class that can be used with any other
 *   class.
 *   
 *   Note that we map only the *reported* name for the object.  We do NOT
 *   give this object any vocabulary from the other object; in other words,
 *   we don't enter this object into the dictionary with the other object's
 *   vocabulary words.  
 */
class NameAsOther: object
    /* the target object - we'll use the same name as this object */
    targetObj = nil

    /* map our naming and usage properties to the target object */
    isPlural = (targetObj.isPlural)
    isMassNoun = (targetObj.isMassNoun)
    isHim = (targetObj.isHim)
    isHer = (targetObj.isHer)
    isIt = (targetObj.isIt)
    isProperName = (targetObj.isProperName)
    isQualifiedName = (targetObj.isQualifiedName)
    name = (targetObj.name)

    /* map the derived name properties as well, in case any are overridden */
    disambigName = (targetObj.disambigName)
    theDisambigName = (targetObj.theDisambigName)
    aDisambigName = (targetObj.aDisambigName)
    countDisambigName(cnt) { return targetObj.countDisambigName(cnt); }
    disambigEquivName = (targetObj.disambigEquivName)
    listName = (targetObj.listName)
    countName(cnt) { return targetObj.countName(cnt); }

    /* map the pronoun properites, in case any are overridden */
    itNom = (targetObj.itNom)
    itObj = (targetObj.itObj)
    itPossAdj = (targetObj.itPossAdj)
    itPossNoun = (targetObj.itPossNoun)
    itReflexive = (targetObj.itReflexive)
    thatNom = (targetObj.thatNom)
    thatObj = (targetObj.thatObj)
    thatIsContraction = (targetObj.thatIsContraction)
    itIs = (targetObj.itIs)
    itIsContraction = (targetObj.itIsContraction)
    itVerb(verb) { return targetObj.itVerb(verb); }
    conjugateRegularVerb(verb)
        { return targetObj.conjugateRegularVerb(verb); }
    theName = (targetObj.theName)
    theNameObj = (targetObj.theNameObj)
    theNamePossAdj = (targetObj.theNamePossAdj)
    theNamePossNoun = (targetObj.theNamePossNoun)
    theNameWithOwner = (targetObj.theNameWithOwner)
    aNameOwnerLoc(ownerPri)
        { return targetObj.aNameOwnerLoc(ownerPri); }
    theNameOwnerLoc(ownerPri)
        { return targetObj.theNameOwnerLoc(ownerPri); }
    countNameOwnerLoc(cnt, ownerPri)
        { return targetObj.countNameOwnerLoc(cnt, ownerPri); }
    notePromptByOwnerLoc(ownerPri)
        { targetObj.notePromptByOwnerLoc(ownerPri); }
    notePromptByPossAdj()
        { targetObj.notePromptByPossAdj(); }
    aName = (targetObj.aName)
    aNameObj = (targetObj.aNameObj)
    pluralName = (targetObj.pluralName)
    nameIs = (targetObj.nameIs)
    nameIsnt = (targetObj.nameIsnt)
    nameVerb(verb) { return targetObj.nameVerb(verb); }
    verbToBe = (targetObj.verbToBe)
    verbWas = (targetObj.verbWas)
    verbToHave = (targetObj.verbToHave)
    verbToDo = (targetObj.verbToDo)
    nameDoes = (targetObj.nameDoes)
    verbToGo = (targetObj.verbToGo)
    verbToCome = (targetObj.verbToCome)
    verbToLeave = (targetObj.verbToLeave)
    verbToSee = (targetObj.verbToSee)
    nameSees = (targetObj.nameSees)
    verbToSay = (targetObj.verbToSay)
    nameSays = (targetObj.nameSays)
    verbMust = (targetObj.verbMust)
    verbCan = (targetObj.verbCan)
    verbCannot = (targetObj.verbCannot)
    verbCant = (targetObj.verbCant)
    verbWill = (targetObj.verbWill)
    verbWont = (targetObj.verbWont)

    verbEndingS = (targetObj.verbEndingS)
    verbEndingSD = (targetObj.verbEndingSD)
    verbEndingSEd = (targetObj.verbEndingSEd)
    verbEndingEs = (targetObj.verbEndingEs)
    verbEndingIes = (targetObj.verbEndingIes)
;

/*
 *   Name as Parent - this is a special case of NameAsOther that uses the
 *   lexical parent of a nested object as the target object.  (The lexical
 *   parent is the enclosing object in a nested object definition; in other
 *   words, it's the object in which the nested object is embedded.)  
 */
class NameAsParent: NameAsOther
    targetObj = (lexicalParent)
;

/*
 *   ChildNameAsOther is a mix-in class that can be used with NameAsOther
 *   to add the various childInXxx naming to the mapped properties.  The
 *   childInXxx names are the names generated when another object is
 *   described as located within this object; by mapping these properties
 *   to our target object, we ensure that we use exactly the same phrasing
 *   as we would if the contained object were actually contained by our
 *   target rather than by us.
 *   
 *   Note that this should always be used in combination with NameAsOther:
 *   
 *   myObj: NameAsOther, ChildNameAsOther, Thing ...
 *   
 *   You can also use it the same way in combination with a subclass of
 *   NameAsOther, such as NameAsParent.  
 */
class ChildNameAsOther: object
    objInPrep = (targetObj.objInPrep)
    actorInPrep = (targetObj.actorInPrep)
    actorOutOfPrep = (targetObj.actorOutOfPrep)
    actorIntoPrep = (targetObj.actorIntoPrep)
    childInName(childName) { return targetObj.childInName(childName); }
    childInNameWithOwner(childName)
        { return targetObj.childInNameWithOwner(childName); }
    childInNameGen(childName, myName)
        { return targetObj.childInNameGen(childName, myName); }
    actorInName = (targetObj.actorInName)
    actorOutOfName = (targetObj.actorOutOfName)
    actorIntoName = (targetObj.actorIntoName)
    actorInAName = (targetObj.actorInAName)
;


/* ------------------------------------------------------------------------ */
/*
 *   Language modifications for the specialized container types
 */
modify Surface
    /*
     *   objects contained in a Surface are described as being on the
     *   Surface
     */
    objInPrep = 'on'
    actorInPrep = 'on'
    actorOutOfPrep = 'off of'
;

modify Underside
    objInPrep = 'under'
    actorInPrep = 'under'
    actorOutOfPrep = 'from under'
;

modify RearContainer
    objInPrep = 'behind'
    actorInPrep = 'behind'
    actorOutOfPrep = 'from behind'
;

/* ------------------------------------------------------------------------ */
/*
 *   Language modifications for Actor.
 *   
 *   An Actor has a "referral person" setting, which determines how we
 *   refer to the actor; this is almost exclusively for the use of the
 *   player character.  The typical convention is that we refer to the
 *   player character in the second person, but a game can override this on
 *   an actor-by-actor basis.  
 */
modify Actor
    /* by default, use my pronoun for my name */
    name = (itNom)

    /*
     *   Pronoun selector.  This returns an index for selecting pronouns
     *   or other words based on number and gender, taking into account
     *   person, number, and gender.  The value returned is the sum of the
     *   following components:
     *
     *   number/gender:
     *.  - singular neuter = 1
     *.  - singular masculine = 2
     *.  - singular feminine = 3
     *.  - plural = 4
     *
     *   person:
     *.  - first person = 0
     *.  - second person = 4
     *.  - third person = 8
     *
     *   The result can be used as a list selector as follows (1=first
     *   person, etc; s=singular, p=plural; n=neuter, m=masculine,
     *   f=feminine):
     *
     *   [1/s/n, 1/s/m, 1/s/f, 1/p, 2/s/n, 2/s/m, 2/s/f, 2/p,
     *.  3/s/n, 3/s/m, 3/s/f, 3/p]
     */
    pronounSelector
    {
        return ((referralPerson - FirstPerson)*4
                + (isPlural ? 4 : isHim ? 2 : isHer ? 3 : 1));
    }

    /*
     *   get the verb form selector index for the person and number:
     *
     *   [1/s, 2/s, 3/s, 1/p, 2/p, 3/p]
     */
    conjugationSelector
    {
        return (referralPerson + (isPlural ? 3 : 0));
    }

    /*
     *   get an appropriate pronoun for the object in the appropriate
     *   person for the nominative case, objective case, possessive
     *   adjective, possessive noun, and objective reflexive
     */
    itNom
    {
        return ['I', 'I', 'I', 'we',
               'you', 'you', 'you', 'you',
               'it', 'he', 'she', 'they'][pronounSelector];
    }
    itObj
    {
        return ['me', 'me', 'me', 'us',
               'you', 'you', 'you', 'you',
               'it', 'him', 'her', 'them'][pronounSelector];
    }
    itPossAdj
    {
        return ['my', 'my', 'my', 'our',
               'your', 'your', 'your', 'your',
               'its', 'his', 'her', 'their'][pronounSelector];
    }
    itPossNoun
    {
        return ['mine', 'mine', 'mine', 'ours',
               'yours', 'yours', 'yours', 'yours',
               'its', 'his', 'hers', 'theirs'][pronounSelector];
    }
    itReflexive
    {
        return ['myself', 'myself', 'myself', 'ourselves',
               'yourself', 'yourself', 'yourself', 'yourselves',
               'itself', 'himself', 'herself', 'themselves'][pronounSelector];
    }

    /*
     *   Demonstrative pronoun, nominative case.  We'll use personal a
     *   personal pronoun if we have a gender or we're in the first or
     *   second person, otherwise we'll use 'that' or 'those' as we would
     *   for an inanimate object.
     */
    thatNom
    {
        return ['I', 'I', 'I', 'we',
               'you', 'you', 'you', 'you',
               'that', 'he', 'she', 'those'][pronounSelector];
    }

    /* demonstrative pronoun, objective case */
    thatObj
    {
        return ['me', 'me', 'me', 'us',
               'you', 'you', 'you', 'you',
               'that', 'him', 'her', 'those'][pronounSelector];
    }

    /* demonstrative pronoun, nominative case with 'is' contraction */
    thatIsContraction
    {
        return thatNom
            + tSel(['&rsquo;m', '&rsquo;re', '&rsquo;s',
                    '&rsquo;re', '&rsquo;re', ' are'][conjugationSelector],
                   ' ' + verbToBe);
    }

    /*
     *   We don't need to override itIs: the base class handling works for
     *   actors too.
     */

    /* get my pronoun with a being verb contraction ("the box's") */
    itIsContraction
    {
        return itNom + tSel(
            '&rsquo;'
            + ['m', 're', 's', 're', 're', 're'][conjugationSelector],
            ' ' + verbToBe);
    }

    /*
     *   Conjugate a regular verb in the present or past tense for our
     *   person and number.
     *
     *   In the present tense, this is pretty easy: we add an 's' for the
     *   third person singular, and leave the verb unchanged for every
     *   other case.  The only complication is that we must check some
     *   special cases to add the -s suffix: -y -> -ies, -o -> -oes.
     *
     *   In the past tense, we use the inherited handling since the past
     *   tense ending doesn't vary with person.
     */
    conjugateRegularVerb(verb)
    {
        /*
         *   If we're in the third person or if we use the past tense,
         *   inherit the default handling; otherwise, use the base verb
         *   form regardless of number (regular verbs use the same
         *   conjugated forms for every case but third person singular: I
         *   ask, you ask, we ask, they ask).
         */
        if (referralPerson != ThirdPerson && !gameMain.usePastTense)
        {
            /*
             *   we're not using the third-person or the past tense, so the
             *   conjugation is the same as the base verb form
             */
            return verb;
        }
        else
        {
            /*
             *   we're using the third person or the past tense, so inherit
             *   the base class handling, which conjugates these forms
             */
            return inherited(verb);
        }
    }

    /*
     *   Get the name with a definite article ("the box").  If the
     *   narrator refers to us in the first or second person, use a
     *   pronoun rather than the short description.
     */
    theName
        { return (referralPerson == ThirdPerson ? inherited : itNom); }

    /* theName in objective case */
    theNameObj
        { return (referralPerson == ThirdPerson ? inherited : itObj); }

    /* theName as a possessive adjective */
    theNamePossAdj
        { return (referralPerson == ThirdPerson ? inherited : itPossAdj); }

    /* theName as a possessive noun */
    theNamePossNoun
    { return (referralPerson == ThirdPerson ? inherited : itPossNoun); }

    /*
     *   Get the name with an indefinite article.  Use the same rules of
     *   referral person as for definite articles.
     */
    aName { return (referralPerson == ThirdPerson ? inherited : itNom); }

    /* aName in objective case */
    aNameObj { return (referralPerson == ThirdPerson ? inherited : itObj); }

    /* being verb agreeing with this object as subject */
    verbToBe
    {
        return tSel(['am', 'are', 'is', 'are', 'are', 'are'],
                    ['was', 'were', 'was', 'were', 'were', 'were'])
               [conjugationSelector];
    }

    /* past tense being verb agreeing with this object as subject */
    verbWas
    {
        return tSel(['was', 'were', 'was', 'were', 'were', 'were']
                    [conjugationSelector], 'had been');
    }

    /* 'have' verb agreeing with this object as subject */
    verbToHave
    {
        return tSel(['have', 'have', 'has', 'have', 'have', 'have']
                    [conjugationSelector], 'had');
    }

    /*
     *   verb endings for regular '-s' and '-es' verbs, agreeing with this
     *   object as the subject
     */
    verbEndingS
    {
        return ['', '', 's', '', '', ''][conjugationSelector];
    }
    verbEndingEs
    {
        return tSel(['', '', 'es', '', '', ''][conjugationSelector], 'ed');
    }
    verbEndingIes
    {
        return tSel(['y', 'y', 'ies', 'y', 'y', 'y'][conjugationSelector],
                    'ied');
    }

    /* "I'm not" doesn't fit the regular "+n't" rule */
    nameIsnt
    {
        return conjugationSelector == 1 && !gameMain.usePastTense
            ? 'I&rsquo;m not' : inherited;
    }

    /*
     *   Show my name for an arrival/departure message.  If we've been seen
     *   before by the player character, we'll show our definite name,
     *   otherwise our indefinite name.
     */
    travelerName(arriving)
        { say(gPlayerChar.hasSeen(self) ? theName : aName); }

    /*
     *   Test to see if we can match the third-person pronouns.  We'll
     *   match these if our inherited test says we match them AND we can
     *   be referred to in the third person.
     */
    canMatchHim = (inherited && canMatch3rdPerson)
    canMatchHer = (inherited && canMatch3rdPerson)
    canMatchIt = (inherited && canMatch3rdPerson)
    canMatchThem = (inherited && canMatch3rdPerson)

    /*
     *   Test to see if we can match a third-person pronoun ('it', 'him',
     *   'her', 'them').  We can unless we're the player character and the
     *   player character is referred to in the first or second person.
     */
    canMatch3rdPerson = (!isPlayerChar || referralPerson == ThirdPerson)

    /*
     *   Set a pronoun antecedent to the given list of ResolveInfo objects.
     *   Pronoun handling is language-specific, so this implementation is
     *   part of the English library, not the generic library.
     *
     *   If only one object is present, we'll set the object to be the
     *   antecedent of 'it', 'him', or 'her', according to the object's
     *   gender.  We'll also set the object as the single antecedent for
     *   'them'.
     *
     *   If we have multiple objects present, we'll set the list to be the
     *   antecedent of 'them', and we'll forget about any antecedent for
     *   'it'.
     *
     *   Note that the input is a list of ResolveInfo objects, so we must
     *   pull out the underlying game objects when setting the antecedents.
     */
    setPronoun(lst)
    {
        /* if the list is empty, ignore it */
        if (lst == [])
            return;

        /*
         *   if we have multiple objects, the entire list is the antecedent
         *   for 'them'; otherwise, it's a singular antecedent which
         *   depends on its gender
         */
        if (lst.length() > 1)
        {
            local objs = lst.mapAll({x: x.obj_});

            /* it's 'them' */
            setThem(objs);

            /* forget any 'it' */
            setIt(nil);
        }
        else if (lst.length() == 1)
        {
            /*
             *   We have only one object, so set it as an antecedent
             *   according to its gender.
             */
            setPronounObj(lst[1].obj_);
        }
    }

    /*
     *   Set a pronoun to refer to multiple potential antecedents.  This is
     *   used when the verb has multiple noun slots - UNLOCK DOOR WITH KEY.
     *   For verbs like this, we have no way of knowing in advance whether
     *   a future pronoun will refer back to the direct object or the
     *   indirect object (etc) - we could just assume that 'it' will refer
     *   to the direct object, but this won't always be what the player
     *   intended.  In natural English, pronoun antecedents must often be
     *   inferred from context at the time of use - so we use the same
     *   approach.
     *
     *   Pass an argument list consisting of ResolveInfo lists - that is,
     *   pass one argument per noun slot in the verb, and make each
     *   argument a list of ResolveInfo objects.  In other words, you call
     *   this just as you would setPronoun(), except you can pass more than
     *   one list argument.
     *
     *   We'll store the multiple objects as antecedents.  When we need to
     *   resolve a future singular pronoun, we'll figure out which of the
     *   multiple antecedents is the most logical choice in the context of
     *   the pronoun's usage.
     */
    setPronounMulti([args])
    {
        local lst, subLst;
        local gotThem;

        /*
         *   If there's a plural list, it's 'them'.  Arbitrarily use only
         *   the first plural list if there's more than one.
         */
        if ((lst = args.valWhich({x: x.length() > 1})) != nil)
        {
            /* set 'them' to the plural list */
            setPronoun(lst);

            /* note that we had a clear 'them' */
            gotThem = true;
        }

        /* from now on, consider only the sublists with exactly one item */
        args = args.subset({x: x.length() == 1});

        /* get a list of the singular items from the lists */
        lst = args.mapAll({x: x[1].obj_});

        /*
         *   Set 'it' to all of the items that can match 'it'; do likewise
         *   with 'him' and 'her'.  If there are no objects that can match
         *   a given pronoun, leave that pronoun unchanged.  
         */
        if ((subLst = lst.subset({x: x.canMatchIt})).length() > 0)
            setIt(subLst);
        if ((subLst = lst.subset({x: x.canMatchHim})).length() > 0)
            setHim(subLst);
        if ((subLst = lst.subset({x: x.canMatchHer})).length() > 0)
            setHer(subLst);

        /*
         *   set 'them' to the potential 'them' matches, if we didn't
         *   already find a clear plural list
         */
        if (!gotThem
            && (subLst = lst.subset({x: x.canMatchThem})).length() > 0)
            setThem(subLst);
    }

    /*
     *   Set a pronoun antecedent to the given ResolveInfo list, for the
     *   specified type of pronoun.  We don't have to worry about setting
     *   other types of pronouns to this antecedent - we specifically want
     *   to set the given pronoun type.  This is language-dependent
     *   because we still have to figure out the number (i.e. singular or
     *   plural) of the pronoun type.
     */
    setPronounByType(typ, lst)
    {
        /* check for singular or plural pronouns */
        if (typ == PronounThem)
        {
            /* it's plural - set a list antecedent */
            setPronounAntecedent(typ, lst.mapAll({x: x.obj_}));
        }
        else
        {
            /* it's singular - set an individual antecedent */
            setPronounAntecedent(typ, lst[1].obj_);
        }
    }

    /*
     *   Set a pronoun antecedent to the given simulation object (usually
     *   an object descended from Thing).
     */
    setPronounObj(obj)
    {
        /*
         *   Actually use the object's "identity object" as the antecedent
         *   rather than the object itself.  In some cases, we use multiple
         *   program objects to represent what appears to be a single
         *   object in the game; in these cases, the internal program
         *   objects all point to the "real" object as their identity
         *   object.  Whenever we're manipulating one of these internal
         *   program objects, we want to make sure that its the
         *   player-visible object - the identity object - that appears as
         *   the antecedent for subsequent references.
         */
        obj = obj.getIdentityObject();

        /*
         *   Set the appropriate pronoun antecedent, depending on the
         *   object's gender.
         *
         *   Note that we'll set an object to be the antecedent for both
         *   'him' and 'her' if the object has both masculine and feminine
         *   usage.
         */

        /* check for masculine usage */
        if (obj.canMatchHim)
            setHim(obj);

        /* check for feminine usage */
        if (obj.canMatchHer)
            setHer(obj);

        /* check for neuter usage */
        if (obj.canMatchIt)
            setIt(obj);

        /* check for third-person plural usage */
        if (obj.canMatchThem)
            setThem([obj]);
    }

    /* set a possessive anaphor */
    setPossAnaphorObj(obj)
    {
        /* check for each type of usage */
        if (obj.canMatchHim)
            possAnaphorTable[PronounHim] = obj;
        if (obj.canMatchHer)
            possAnaphorTable[PronounHer] = obj;
        if (obj.canMatchIt)
            possAnaphorTable[PronounIt] = obj;
        if (obj.canMatchThem)
            possAnaphorTable[PronounThem] = [obj];
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Give the postures some additional attributes
 */

modify Posture

    /*
     *   Intransitive and transitive forms of the verb, for use in library
     *   messages.  Each of these methods simply calls one of the two
     *   corresponding fixed-tense properties, depending on the current
     *   tense.
     */
    msgVerbI = (tSel(msgVerbIPresent, msgVerbIPast))
    msgVerbT = (tSel(msgVerbTPresent, msgVerbTPast))

    /*
     *   Fixed-tense versions of the above properties, to be defined
     *   individually by each instance of the Posture class.
     */

    /* our present-tense intransitive form ("he stands up") */
    // msgVerbIPresent = 'stand{s} up'

    /* our past-tense intransitive form ("he stood up") */
    // msgVerbIPast = 'stood up'

    /* our present-tense transitive form ("he stands on the chair") */
    // msgVerbTPresent = 'stand{s}'

    /* our past-tense transitive form ("he stood on the chair") */
    // msgVerbTPast = 'stood'

    /* our participle form */
    // participle = 'standing'
;

modify standing
    msgVerbIPresent = 'stand{s} up'
    msgVerbIPast = 'stood up'
    msgVerbTPresent = 'stand{s}'
    msgVerbTPast = 'stood'
    participle = 'standing'
;

modify sitting
    msgVerbIPresent = 'sit{s} down'
    msgVerbIPast = 'sat down'
    msgVerbTPresent = 'sit{s}'
    msgVerbTPast = 'sat'
    participle = 'sitting'
;

modify lying
    msgVerbIPresent = 'lie{s} down'
    msgVerbIPast = 'lay down'
    msgVerbTPresent = 'lie{s}'
    msgVerbTPast = 'lay'
    participle = 'lying'
;

/* ------------------------------------------------------------------------ */
/*
 *   For our various topic suggestion types, we can infer the full name
 *   from the short name fairly easily.
 */
modify SuggestedAskTopic
    fullName = ('ask {it targetActor/him} about ' + name)
;

modify SuggestedTellTopic
    fullName = ('tell {it targetActor/him} about ' + name)
;

modify SuggestedAskForTopic
    fullName = ('ask {it targetActor/him} for ' + name)
;

modify SuggestedGiveTopic
    fullName = ('give {it targetActor/him} ' + name)
;

modify SuggestedShowTopic
    fullName = ('show {it targetActor/him} ' + name)
;

modify SuggestedYesTopic
    name = 'yes'
    fullName = 'say yes'
;

modify SuggestedNoTopic
    name = 'no'
    fullName = 'say no'
;

/* ------------------------------------------------------------------------ */
/*
 *   Provide custom processing of the player input for matching
 *   SpecialTopic patterns.  When we're trying to match a player's command
 *   to a set of active special topics, we'll run the input through this
 *   processing to produce the string that we actually match against the
 *   special topics.
 *
 *   First, we'll remove any punctuation marks.  This ensures that we'll
 *   still match a special topic, for example, if the player puts a period
 *   or a question mark at the end of the command.
 *
 *   Second, if the user's input starts with "A" or "T" (the super-short
 *   forms of the ASK ABOUT and TELL ABOUT commands), remove the "A" or "T"
 *   and keep the rest of the input.  Some users might think that special
 *   topic suggestions are meant as ask/tell topics, so they might
 *   instinctively try these as A/T commands.
 *
 *   Users *probably* won't be tempted to do the same thing with the full
 *   forms of the commands (e.g., ASK BOB ABOUT APOLOGIZE, TELL BOB ABOUT
 *   EXPLAIN).  It's more a matter of habit of using A or T for interaction
 *   that would tempt a user to phrase a special topic this way; once
 *   you're typing out the full form of the command, it generally won't be
 *   grammatical, as special topics generally contain the sense of a verb
 *   in their phrasing.
 */
modify specialTopicPreParser
    processInputStr(str)
    {
        /*
         *   remove most punctuation from the string - we generally want to
         *   ignore these, as we mostly just want to match keywords
         */
        str = rexReplace(punctPat, str, '', ReplaceAll);

        /* if it starts with "A" or "T", strip off the leading verb */
        if (rexMatch(aOrTPat, str) != nil)
            str = rexGroup(1)[3];

        /* return the processed result */
        return str;
    }

    /* pattern for string starting with "A" or "T" verbs */
    aOrTPat = static new RexPattern(
        '<nocase><space>*[at]<space>+(<^space>.*)$')

    /* pattern to eliminate punctuation marks from the string */
    punctPat = static new RexPattern('[.?!,;:]');
;

/*
 *   For SpecialTopic matches, treat some strings as "weak": if the user's
 *   input consists of just one of these weak strings and nothing else,
 *   don't match the topic.
 */
modify SpecialTopic
    matchPreParse(str, procStr)
    {
        /* if it's one of our 'weak' strings, don't match */
        if (rexMatch(weakPat, str) != nil)
            return nil;

        /* it's not a weak string, so match as usual */
        return inherited(str, procStr);
    }

    /*
     *   Our "weak" strings - 'i', 'l', 'look': these are weak because a
     *   user typing one of these strings by itself is probably actually
     *   trying to enter the command of the same name, rather than entering
     *   a special topic.  These come up in cases where the special topic
     *   is something like "say I don't know" or "tell him you'll look into
     *   it".
     */
    weakPat = static new RexPattern('<nocase><space>*(i|l|look)<space>*$')
;

/* ------------------------------------------------------------------------ */
/*
 *   English-specific Traveler changes
 */
modify Traveler
    /*
     *   Get my location's name, from the PC's perspective, for describing
     *   my arrival to or departure from my current location.  We'll
     *   simply return our location's destName, or "the area" if it
     *   doesn't have one.
     */
    travelerLocName()
    {
        /* get our location's name from the PC's perspective */
        local nm = location.getDestName(gPlayerChar, gPlayerChar.location);

        /* if there's a name, return it; otherwise, use "the area" */
        return (nm != nil ? nm : 'the area');
    }

    /*
     *   Get my "remote" location name, from the PC's perspective.  This
     *   returns my location name, but only if my location is remote from
     *   the PC's perspective - that is, my location has to be outside of
     *   the PC's top-level room.  If we're within the PC's top-level
     *   room, we'll simply return an empty string.
     */
    travelerRemoteLocName()
    {
        /*
         *   if my location is outside of the PC's outermost room, we're
         *   remote, so return my location name; otherwise, we're local,
         *   so we don't need a remote name at all
         */
        if (isIn(gPlayerChar.getOutermostRoom()))
            return '';
        else
            return travelerLocName;
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   English-specific Vehicle changes
 */
modify Vehicle
    /*
     *   Display the name of the traveler, for use in an arrival or
     *   departure message.
     */
    travelerName(arriving)
    {
        /*
         *   By default, start with the indefinite name if we're arriving,
         *   or the definite name if we're leaving.
         *
         *   If we're leaving, presumably they've seen us before, since we
         *   were already in the room to start with.  Since we've been
         *   seen before, the definite is appropriate.
         *
         *   If we're arriving, even if we're not being seen for the first
         *   time, we haven't been seen yet in this place around this
         *   time, so the indefinite is appropriate.
         */
        say(arriving ? aName : theName);

        /* show the list of actors aboard */
        aboardVehicleListerObj.showList(
            libGlobal.playerChar, nil, allContents(), 0, 0,
            libGlobal.playerChar.visibleInfoTable(), nil);
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   English-specific PushTraveler changes
 */
modify PushTraveler
    /*
     *   When an actor is pushing an object from one room to another, show
     *   its name with an additional clause indicating the object being
     *   moved along with us.
     */
    travelerName(arriving)
    {
        "<<gPlayerChar.hasSeen(self) ? theName : aName>>,
        pushing <<obj_.theNameObj>>,";
    }
;


/* ------------------------------------------------------------------------ */
/*
 *   English-specific travel connector changes
 */

modify PathPassage
    /* treat "take path" the same as "enter path" or "go through path" */
    dobjFor(Take) maybeRemapTo(
        gAction.getEnteredVerbPhrase() == 'take (dobj)', TravelVia, self)

    dobjFor(Enter)
    {
        verify() { logicalRank(50, 'enter path'); }
    }
    dobjFor(GoThrough)
    {
        verify() { logicalRank(50, 'enter path'); }
    }
;

modify AskConnector
    /*
     *   This is the noun phrase we'll use when asking disambiguation
     *   questions for this travel connector: "Which *one* do you want to
     *   enter..."
     */
    travelObjsPhrase = 'one'
;

/* ------------------------------------------------------------------------ */
/*
 *   English-specific changes for various nested room types.
 */
modify BasicChair
    /* by default, one sits *on* a chair */
    objInPrep = 'on'
    actorInPrep = 'on'
    actorOutOfPrep = 'off of'
;

modify BasicPlatform
    /* by default, one stands *on* a platform */
    objInPrep = 'on'
    actorInPrep = 'on'
    actorOutOfPrep = 'off of'
;

modify Booth
    /* by default, one is *in* a booth */
    objInPrep = 'in'
    actorInPrep = 'in'
    actorOutOfPrep = 'out of'
;

/* ------------------------------------------------------------------------ */
/*
 *   Language modifications for Matchstick
 */
modify Matchstick
    /* "strike match" means "light match" */
    dobjFor(Strike) asDobjFor(Burn)

    /* "light match" means "burn match" */
    dobjFor(Light) asDobjFor(Burn)
;

/*
 *   Match state objects.  We show "lit" as the state for a lit match,
 *   nothing for an unlit match.
 */
matchStateLit: ThingState 'lit'
    stateTokens = ['lit']
;
matchStateUnlit: ThingState
    stateTokens = ['unlit']
;


/* ------------------------------------------------------------------------ */
/*
 *   English-specific modifications for Room.
 */
modify Room
    /*
     *   The ordinary 'name' property is used the same way it's used for
     *   any other object, to refer to the room when it shows up in
     *   library messages and the like: "You can't take the hallway."
     *
     *   By default, we derive the name from the roomName by converting
     *   the roomName to lower case.  Virtually every room will need a
     *   custom room name setting, since the room name is used mostly as a
     *   title for the room, and good titles are hard to generate
     *   mechanically.  Many times, converting the roomName to lower case
     *   will produce a decent name to use in messages: "Ice Cave" gives
     *   us "You can't eat the ice cave."  However, games will want to
     *   customize the ordinary name separately in many cases, because the
     *   elliptical, title-like format of the room name doesn't always
     *   translate well to an object name: "West of Statue" gives us the
     *   unworkable "You can't eat the west of statue"; better to make the
     *   ordinary name something like "plaza".  Note also that some rooms
     *   have proper names that want to preserve their capitalization in
     *   the ordinary name: "You can't eat the Hall of the Ancient Kings."
     *   These cases need to be customized as well.
     */
    name = (roomName.toLower())

    /*
     *   The "destination name" of the room.  This is primarily intended
     *   for use in showing exit listings, to describe the destination of
     *   a travel connector leading away from our current location, if the
     *   destination is known to the player character.  We also use this
     *   as the default source of the name in similar contexts, such as
     *   when we can see this room from another room connected by a sense
     *   connector.
     *
     *   The destination name usually mirrors the room name, but we use
     *   the name in prepositional phrases involving the room ("east, to
     *   the alley"), so this name should include a leading article
     *   (usually definite - "the") unless the name is proper ("east, to
     *   Dinsley Plaza").  So, by default, we simply use the "theName" of
     *   the room.  In many cases, it's better to specify a custom
     *   destName, because this name is used when the PC is outside of the
     *   room, and thus can benefit from a more detailed description than
     *   we'd normally use for the basic name.  For example, the ordinary
     *   name might simply be something like "hallway", but since we want
     *   to be clear about exactly which hallway we're talking about when
     *   we're elsewhere, we might want to use a destName like "the
     *   basement hallway" or "the hallway outside the operating room".
     */
    destName = (theName)

    /*
     *   For top-level rooms, describe an object as being in the room by
     *   describing it as being in the room's nominal drop destination,
     *   since that's the nominal location for the objects directly in the
     *   room.  (In most cases, the nominal drop destination for a room is
     *   its floor.)
     *
     *   If the player character isn't in the same outermost room as this
     *   container, use our remote name instead of the nominal drop
     *   destination.  The nominal drop destination is usually something
     *   like the floor or the ground, so it's only suitable when we're in
     *   the same location as what we're describing.
     */
    childInName(childName)
    {
        /* if the PC isn't inside us, we're viewing this remotely */
        if (!gPlayerChar.isIn(self))
            return childInRemoteName(childName, gPlayerChar);
        else
            return getNominalDropDestination().childInName(childName);
    }
    childInNameWithOwner(chiName)
    {
        /* if the PC isn't inside us, we're viewing this remotely */
        if (!gPlayerChar.isIn(self))
            return inherited(chiName);
        else
            return getNominalDropDestination().childInNameWithOwner(chiName);
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   English-specific modifications for the default room parts.
 */

modify Floor
    childInNameGen(childName, myName) { return childName + ' on ' + myName; }
    objInPrep = 'on'
    actorInPrep = 'on'
    actorOutOfPrep = 'off of'
;

modify defaultFloor
    noun = 'floor' 'ground'
    name = 'floor'
;

modify defaultGround
    noun = 'ground' 'floor'
    name = 'ground'
;

modify DefaultWall noun='wall' plural='walls' name='wall';
modify defaultCeiling noun='ceiling' 'roof' name='ceiling';
modify defaultNorthWall adjective='n' 'north' name='north wall';
modify defaultSouthWall adjective='s' 'south' name='south wall';
modify defaultEastWall adjective='e' 'east' name='east wall';
modify defaultWestWall adjective='w' 'west' name='west wall';
modify defaultSky noun='sky' name='sky';


/* ------------------------------------------------------------------------ */
/*
 *   The English-specific modifications for directions.
 */
modify Direction
    /* describe a traveler arriving from this direction */
    sayArriving(traveler)
    {
        /* show the generic arrival message */
        gLibMessages.sayArriving(traveler);
    }

    /* describe a traveler departing in this direction */
    sayDeparting(traveler)
    {
        /* show the generic departure message */
        gLibMessages.sayDeparting(traveler);
    }
;

/*
 *   The English-specific modifications for compass directions.
 */
modify CompassDirection
    /* describe a traveler arriving from this direction */
    sayArriving(traveler)
    {
        /* show the generic compass direction description */
        gLibMessages.sayArrivingDir(traveler, name);
    }

    /* describe a traveler departing in this direction */
    sayDeparting(traveler)
    {
        /* show the generic compass direction description */
        gLibMessages.sayDepartingDir(traveler, name);
    }
;

/*
 *   The English-specific definitions for the compass direction objects.
 *   In addition to modifying the direction objects to define the name of
 *   the direction, we add a 'directionName' grammar rule.
 */
#define DefineLangDir(root, dirNames, backPre) \
grammar directionName(root): dirNames: DirectionProd \
   dir = root##Direction \
; \
\
modify root##Direction \
   name = #@root \
   backToPrefix = backPre

DefineLangDir(north, 'north' | 'n', 'back to the');
DefineLangDir(south, 'south' | 's', 'back to the');
DefineLangDir(east, 'east' | 'e', 'back to the');
DefineLangDir(west, 'west' | 'w', 'back to the');
DefineLangDir(northeast, 'northeast' | 'ne', 'back to the');
DefineLangDir(northwest, 'northwest' | 'nw', 'back to the');
DefineLangDir(southeast, 'southeast' | 'se', 'back to the');
DefineLangDir(southwest, 'southwest' | 'sw', 'back to the');
DefineLangDir(up, 'up' | 'u', 'back');
DefineLangDir(down, 'down' | 'd', 'back');
DefineLangDir(in, 'in', 'back');
DefineLangDir(out, 'out', 'back');

/*
 *   The English-specific shipboard direction modifications.  Certain of
 *   the ship directions have no natural descriptions for arrival and/or
 *   departure; for example, there's no good way to say "arriving from
 *   fore."  Others don't fit any regular pattern: "he goes aft" rather
 *   than "he departs to aft."  As a result, these are a bit irregular
 *   compared to the compass directions and so are individually defined
 *   below.
 */

DefineLangDir(port, 'port' | 'p', 'back to')
    sayArriving(trav)
        { gLibMessages.sayArrivingShipDir(trav, 'the port direction'); }
    sayDeparting(trav)
        { gLibMessages.sayDepartingShipDir(trav, 'port'); }
;

DefineLangDir(starboard, 'starboard' | 'sb', 'back to')
    sayArriving(trav)
        { gLibMessages.sayArrivingShipDir(trav, 'starboard'); }
    sayDeparting(trav)
        { gLibMessages.sayDepartingShipDir(trav, 'starboard'); }
;

DefineLangDir(aft, 'aft' | 'a', 'back to')
    sayArriving(trav) { gLibMessages.sayArrivingShipDir(trav, 'aft'); }
    sayDeparting(trav) { gLibMessages.sayDepartingAft(trav); }
;

DefineLangDir(fore, 'fore' | 'forward' | 'f', 'back to')
    sayArriving(trav) { gLibMessages.sayArrivingShipDir(trav, 'forward'); }
    sayDeparting(trav) { gLibMessages.sayDepartingFore(trav); }
;

/* ------------------------------------------------------------------------ */
/*
 *   Some helper routines for the library messages.
 */
class MessageHelper: object
    /*
     *   Show a list of objects for a disambiguation query.  If
     *   'showIndefCounts' is true, we'll show the number of equivalent
     *   items for each equivalent item; otherwise, we'll just show an
     *   indefinite noun phrase for each equivalent item.
     */
    askDisambigList(matchList, fullMatchList, showIndefCounts, dist)
    {
        /* show each item */
        for (local i = 1, local len = matchList.length() ; i <= len ; ++i)
        {
            local equivCnt;
            local obj;

            /* get the current object */
            obj = matchList[i].obj_;

            /*
             *   if this isn't the first, add a comma; if this is the
             *   last, add an "or" as well
              */
            if (i == len)
                ", or ";
            else if (i != 1)
                ", ";

            /*
             *   Check to see if more than one equivalent of this item
             *   appears in the full list.
             */
            for (equivCnt = 0, local j = 1,
                 local fullLen = fullMatchList.length() ; j <= fullLen ; ++j)
            {
                /*
                 *   if this item is equivalent for the purposes of the
                 *   current distinguisher, count it
                 */
                if (!dist.canDistinguish(obj, fullMatchList[j].obj_))
                {
                    /* it's equivalent - count it */
                    ++equivCnt;
                }
            }

            /* show this item with the appropriate article */
            if (equivCnt > 1)
            {
                /*
                 *   we have multiple equivalents - show either with an
                 *   indefinite article or with a count, depending on the
                 *   flags the caller provided
                 */
                if (showIndefCounts)
                {
                    /* a count is desired for each equivalent group */
                    say(dist.countName(obj, equivCnt));
                }
                else
                {
                    /* no counts desired - show with an indefinite article */
                    say(dist.aName(obj));
                }
            }
            else
            {
                /* there's only one - show with a definite article */
                say(dist.theName(obj));
            }
        }
    }

    /*
     *   For a TAction result, select the short-form or long-form message,
     *   according to the disambiguation status of the action.  This is for
     *   the ultra-terse default messages, such as "Taken" or "Dropped",
     *   that sometimes need more descriptive variations.
     *   
     *   If there was no disambiguation involved, we'll use the short
     *   version of the message.
     *   
     *   If there was unclear disambiguation involved (meaning that there
     *   was more than one logical object matching a noun phrase, but the
     *   parser was able to decide based on likelihood rankings), we'll
     *   still use the short version, because we assume that the parser
     *   will have generated a parenthetical announcement to point out its
     *   choice.
     *   
     *   If there was clear disambiguation involved (meaning that more than
     *   one in-scope object matched a noun phrase, but there was only one
     *   choice that passed the logicalness tests), AND the announcement
     *   mode (in gameMain.ambigAnnounceMode) is DescribeClear, we'll
     *   choose the long-form message.  
     */
    shortTMsg(short, long)
    {
        /* check the disambiguation flags and the announcement mode */
        if ((gAction.getDobjFlags() & (ClearDisambig | AlwaysAnnounce))
            == ClearDisambig
            && gAction.getDobjCount() == 1
            && gameMain.ambigAnnounceMode == DescribeClear)
        {
            /* clear disambig and DescribeClear mode - use the long message */
            return long;
        }
        else
        {
            /* in other cases, use the short message */
            return short;
        }
    }

    /*
     *   For a TIAction result, select the short-form or long-form message.
     *   This works just like shortTIMsg(), but takes into account both the
     *   direct and indirect objects. 
     */
    shortTIMsg(short, long)
    {
        /* check the disambiguation flags and the announcement mode */
        if (((gAction.getDobjFlags() & (ClearDisambig | AlwaysAnnounce))
             == ClearDisambig
             || (gAction.getIobjFlags() & (ClearDisambig | AlwaysAnnounce))
             == ClearDisambig)
            && gAction.getDobjCount() == 1
            && gAction.getIobjCount() == 1
            && gameMain.ambigAnnounceMode == DescribeClear)
        {
            /* clear disambig and DescribeClear mode - use the long message */
            return long;
        }
        else
        {
            /* in other cases, use the short message */
            return short;
        }
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Custom base resolver
 */
modify Resolver
    /*
     *   Get the default in-scope object list for a given pronoun.  We'll
     *   look for a unique object in scope that matches the desired
     *   pronoun, and return a ResolveInfo list if we find one.  If there
     *   aren't any objects in scope that match the pronoun, or multiple
     *   objects are in scope, there's no default.
     */
    getPronounDefault(typ, np)
    {
        local map = [PronounHim, &canMatchHim,
                     PronounHer, &canMatchHer,
                     PronounIt, &canMatchIt];
        local idx = map.indexOf(typ);
        local filterProp = (idx != nil ? map[idx + 1] : nil);
        local lst;

        /* if we couldn't find a filter for the pronoun, ignore it */
        if (filterProp == nil)
            return [];

        /*
         *   filter the list of all possible defaults to those that match
         *   the given pronoun
         */
        lst = getAllDefaults.subset({x: x.obj_.(filterProp)});

        /*
         *   if the list contains exactly one element, then there's a
         *   unique default; otherwise, there's either nothing here that
         *   matches the pronoun or the pronoun is ambiguous, so there's
         *   no default
         */
        if (lst.length() == 1)
        {
            /*
             *   we have a unique object, so they must be referring to it;
             *   because this is just a guess, though, mark it as vague
             */
            lst[1].flags_ |= UnclearDisambig;

            /* return the list */
            return lst;
        }
        else
        {
            /*
             *   the pronoun doesn't have a unique in-scope referent, so
             *   we can't guess what they mean
             */
            return [];
        }
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Custom interactive resolver.  This is used for responses to
 *   disambiguation questions and prompts for missing noun phrases.
 */
modify InteractiveResolver
    /*
     *   Resolve a pronoun antecedent.  We'll resolve a third-person
     *   singular pronoun to the target actor if the target actor matches
     *   in gender, and the target actor isn't the PC.  This allows
     *   exchanges like this:
     *
     *.  >bob, examine
     *.  What do you want Bob to look at?
     *.
     *.  >his book
     *
     *   In the above exchange, we'll treat "his" as referring to Bob, the
     *   target actor of the action, because we have referred to Bob in
     *   the partial command (the "BOB, EXAMINE") that triggered the
     *   interactive question.
     */
    resolvePronounAntecedent(typ, np, results, poss)
    {
        local lst;

        /* try resolving with the target actor as the antecedent */
        if ((lst = resolvePronounAsTargetActor(typ)) != nil)
            return lst;

        /* use the inherited result */
        return inherited(typ, np, results, poss);
    }

    /*
     *   Get the reflexive third-person pronoun binding (himself, herself,
     *   itself, themselves).  If the target actor isn't the PC, and the
     *   gender of the pronoun matches, we'll consider this as referring
     *   to the target actor.  This allows exchanges of this form:
     *
     *.  >bob, examine
     *.  What do you want Bob to examine?
     *.
     *.  >himself
     */
    getReflexiveBinding(typ)
    {
        local lst;

        /* try resolving with the target actor as the antecedent */
        if ((lst = resolvePronounAsTargetActor(typ)) != nil)
            return lst;

        /* use the inherited result */
        return inherited(typ);
    }

    /*
     *   Try matching the given pronoun type to the target actor.  If it
     *   matches in gender, and the target actor isn't the PC, we'll
     *   return a resolve list consisting of the target actor.  If we
     *   don't have a match, we'll return nil.
     */
    resolvePronounAsTargetActor(typ)
    {
        /*
         *   if the target actor isn't the player character, and the
         *   target actor can match the given pronoun type, resolve the
         *   pronoun as the target actor
         */
        if (actor_.canMatchPronounType(typ) && !actor_.isPlayerChar())
        {
            /* the match is the target actor */
            return [new ResolveInfo(actor_, 0, nil)];
        }

        /* we didn't match it */
        return nil;
    }
;

/*
 *   Custom disambiguation resolver.
 */
modify DisambigResolver
    /*
     *   Perform special resolution on pronouns used in interactive
     *   responses.  If the pronoun is HIM or HER, then look through the
     *   list of possible matches for a matching gendered object, and use
     *   it as the result if we find one.  If we find more than one, then
     *   use the default handling instead, treating the pronoun as
     *   referring back to the simple antecedent previously set.
     */
    resolvePronounAntecedent(typ, np, results, poss)
    {
        /* if it's a non-possessive HIM or HER, use our special handling */
        if (!poss && typ is in (PronounHim, PronounHer))
        {
            local prop;
            local sub;

            /* get the gender indicator property for the pronoun */
            prop = (typ == PronounHim ? &canMatchHim : &canMatchHer);

            /*
             *   Scan through the match list to find the objects that
             *   match the gender of the pronoun.  Note that if the player
             *   character isn't referred to in the third person, we'll
             *   ignore the player character for the purposes of matching
             *   this pronoun - if we're calling the PC 'you', then we
             *   wouldn't expect the player to refer to the PC as 'him' or
             *   'her'.
             */
            sub = matchList.subset({x: x.obj_.(prop)});

            /* if the list has a single entry, then use it as the match */
            if (sub.length() == 1)
                return sub;

            /*
             *   if it has more than one entry, it's still ambiguous, but
             *   we might have narrowed it down, so throw a
             *   still-ambiguous exception and let the interactive
             *   disambiguation ask for further clarification
             */
            results.ambiguousNounPhrase(nil, ResolveAsker, 'one',
                                        sub, matchList, matchList,
                                        1, self);
            return [];
        }

        /*
         *   if we get this far, it means we didn't use our special
         *   handling, so use the inherited behavior
         */
        return inherited(typ, np, results, poss);
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Distinguisher customizations for English.
 *   
 *   Each distinguisher must provide a method that gets the name of an item
 *   for a disamgiguation query.  Since these are inherently
 *   language-specific, they're defined here.  
 */

/*
 *   The null distinguisher tells objects apart based strictly on the name
 *   string.  When we list objects, we simply show the basic name - since
 *   we can tell apart our objects based on the base name, there's no need
 *   to resort to other names.  
 */
modify nullDistinguisher
    /* we can tell objects apart if they have different base names */
    canDistinguish(a, b) { return a.name != b.name; }

    name(obj) { return obj.name; }
    aName(obj) { return obj.aName; }
    theName(obj) { return obj.theName; }
    countName(obj, cnt) { return obj.countName(cnt); }
;


/*
 *   The basic distinguisher can tell apart objects that are not "basic
 *   equivalents" of one another.  Thus, we need make no distinctions when
 *   listing objects apart from showing their names.  
 */
modify basicDistinguisher
    name(obj) { return obj.disambigName; }
    aName(obj) { return obj.aDisambigName; }
    theName(obj) { return obj.theDisambigName; }
    countName(obj, cnt) { return obj.countDisambigName(cnt); }
;

/*
 *   The ownership distinguisher tells objects apart based on who "owns"
 *   them, so it shows the owner or location name when listing the object. 
 */
modify ownershipDistinguisher
    name(obj) { return obj.theNameOwnerLoc(true); }
    aName(obj) { return obj.aNameOwnerLoc(true); }
    theName(obj) { return obj.theNameOwnerLoc(true); }
    countName(obj, cnt) { return obj.countNameOwnerLoc(cnt, true); }

    /* note that we're prompting based on this distinguisher */
    notePrompt(lst)
    {
        /*
         *   notify each object that we're referring to it by
         *   owner/location in a disambiguation prompt
         */
        foreach (local cur in lst)
            cur.obj_.notePromptByOwnerLoc(true);
    }
;

/*
 *   The location distinguisher tells objects apart based on their
 *   containers, so it shows the location name when listing the object. 
 */
modify locationDistinguisher
    name(obj) { return obj.theNameOwnerLoc(nil); }
    aName(obj) { return obj.aNameOwnerLoc(nil); }
    theName(obj) { return obj.theNameOwnerLoc(nil); }
    countName(obj, cnt) { return obj.countNameOwnerLoc(cnt, nil); }

    /* note that we're prompting based on this distinguisher */
    notePrompt(lst)
    {
        /* notify the objects of their use in a disambiguation prompt */
        foreach (local cur in lst)
            cur.obj_.notePromptByOwnerLoc(nil);
    }
;

/*
 *   The lit/unlit distinguisher tells apart objects based on whether
 *   they're lit or unlit, so we list objects as lit or unlit explicitly.  
 */
modify litUnlitDistinguisher
    name(obj) { return obj.nameLit; }
    aName(obj) { return obj.aNameLit; }
    theName(obj) { return obj.theNameLit; }
    countName(obj, cnt) { return obj.pluralNameLit; }
;

/* ------------------------------------------------------------------------ */
/*
 *   Enligh-specific light source modifications
 */
modify LightSource
    /* provide lit/unlit names for litUnlitDistinguisher */
    nameLit = ((isLit ? 'lit ' : 'unlit ') + name)
    aNameLit()
    {
        /*
         *   if this is a mass noun or a plural name, just use the name
         *   with lit/unlit; otherwise, add "a"
         */
        if (isPlural || isMassNoun)
            return (isLit ? 'lit ' : 'unlit ') + name;
        else
            return (isLit ? 'a lit ' : 'an unlit ') + name;
    }
    theNameLit = ((isLit ? 'the lit ' : 'the unlit ') + name)
    pluralNameLit = ((isLit ? 'lit ' : 'unlit ') + pluralName)

    /*
     *   Allow 'lit' and 'unlit' as adjectives - but even though we define
     *   these as our adjectives in the dictionary, we'll only accept the
     *   one appropriate for our current state, thanks to our state
     *   objects.
     */
    adjective = 'lit' 'unlit'
;

/*
 *   Light source list states.  An illuminated light source shows its
 *   status as "providing light"; an unlit light source shows no extra
 *   status.
 */
lightSourceStateOn: ThingState 'providing light'
    stateTokens = ['lit']
;
lightSourceStateOff: ThingState
    stateTokens = ['unlit']
;

/* ------------------------------------------------------------------------ */
/*
 *   Wearable states - a wearable item can be either worn or not worn.
 */

/* "worn" */
wornState: ThingState 'being worn'
    /*
     *   In listings of worn items, don't bother mentioning our 'worn'
     *   status, as the entire list consists of items being worn - it
     *   would be redundant to point out that the items in a list of items
     *   being worn are being worn.
     */
    wornName(lst) { return nil; }
;

/*
 *   "Unworn" state.  Don't bother mentioning the status of an unworn item,
 *   since this is the default for everything.  
 */
unwornState: ThingState;


/* ------------------------------------------------------------------------ */
/*
 *   Typographical effects output filter.  This filter looks for certain
 *   sequences in the text and converts them to typographical equivalents.
 *   Authors could simply write the HTML for the typographical markups in
 *   the first place, but it's easier to write the typewriter-like
 *   sequences and let this filter convert to HTML.
 *
 *   We perform the following conversions:
 *
 *   '---' -> &zwnbsp;&mdash;
 *.  '--' -> &zwnbsp;&ndash;
 *.  sentence-ending punctuation -> same + &ensp;
 *
 *   Since this routine is called so frequently, we hard-code the
 *   replacement strings, rather than using properties, for slightly faster
 *   performance.  Since this routine is so simple, games that want to
 *   customize the replacement style should simply replace this entire
 *   routine with a new routine that applies the customizations.
 *
 *   Note that we define this filter in the English-specific part of the
 *   library, because it seems almost certain that each language will want
 *   to customize it for local conventions.
 */
typographicalOutputFilter: OutputFilter
    filterText(ostr, val)
    {
        /*
         *   Look for sentence-ending punctuation, and put an 'en' space
         *   after each occurrence.  Recognize ends of sentences even if we
         *   have closing quotes, parentheses, or other grouping characters
         *   following the punctuation.  Do this before the hyphen
         *   substitutions so that we can look for ordinary hyphens rather
         *   than all of the expanded versions.
         */
        val = rexReplace(eosPattern, val, '%1\u2002', ReplaceAll);

        /* undo any abbreviations we mistook for sentence endings */
        val = rexReplace(abbrevPat, val, '%1. ', ReplaceAll);

        /*
         *   Replace dashes with typographical hyphens.  Three hyphens in a
         *   row become an em-dash, and two in a row become an en-dash.
         *   Note that we look for the three-hyphen sequence first, because
         *   if we did it the other way around, we'd incorrectly find the
         *   first two hyphens of each '---' sequence and replace them with
         *   an en-dash, causing us to miss the '---' sequences entirely.
         *   
         *   We put a no-break marker (\uFEFF) just before each hyphen, and
         *   an okay-to-break marker (\u200B) just after, to ensure that we
         *   won't have a line break between the preceding text and the
         *   hyphen, and to indicate that a line break is specifically
         *   allowed if needed to the right of the hyphen.  
         */
        val = val.findReplace(['---', '--'],
                              ['\uFEFF&mdash;\u200B', '\uFEFF&ndash;\u200B']);

        /* return the result */
        return val;
    }

    /*
     *   The end-of-sentence pattern.  This looks a bit complicated, but
     *   all we're looking for is a period, exclamation point, or question
     *   mark, optionally followed by any number of closing group marks
     *   (right parentheses or square brackets, closing HTML tags, or
     *   double or single quotes in either straight or curly styles), all
     *   followed by an ordinary space.
     *
     *   If a lower-case letter follows the space, though, we won't
     *   consider it a sentence ending.  This applies most commonly after
     *   quoted passages ending with what would normally be sentence-ending
     *   punctuation: "'Who are you?' he asked."  In these cases, the
     *   enclosing sentence isn't ending, so we don't want the extra space.
     *   We can tell the enclosing sentence isn't ending because a
     *   non-capital letter follows.
     *
     *   Note that we specifically look only for ordinary spaces.  Any
     *   sentence-ending punctuation that's followed by a quoted space or
     *   any typographical space overrides this substitution.
     */
    eosPattern = static new RexPattern(
        '<case>'
        + '('
        +   '[.!?]'
        +   '('
        +     '<rparen|rsquare|dquote|squote|\u2019|\u201D>'
        +     '|<langle><^rangle>*<rangle>'
        +   ')*'
        + ')'
        + ' +(?![-a-z])'
        )

    /* pattern for abbreviations that were mistaken for sentence endings */
    abbrevPat = static new RexPattern(
        '<nocase>%<(' + abbreviations + ')<dot>\u2002')

    /* 
     *   Common abbreviations.  These are excluded from being treated as
     *   sentence endings when they appear with a trailing period.
     *   
     *   Note that abbrevPat must be rebuilt manually if you change this on
     *   the fly - abbrevPat is static, so it picks up the initial value of
     *   this property at start-up, and doesn't re-evaluate it while the
     *   game is running.  
     */
    abbreviations = 'mr|mrs|ms|dr|prof'
;

/* ------------------------------------------------------------------------ */
/*
 *   The English-specific message builder.
 */
langMessageBuilder: MessageBuilder

    /*
     *   The English message substitution parameter table.
     *
     *   Note that we specify two additional elements for each table entry
     *   beyond the standard language-independent complement:
     *
     *   info[4] = reflexive property - this is the property to invoke
     *   when the parameter is used reflexively (in other words, its
     *   target object is the same as the most recent target object used
     *   in the nominative case).  If this is nil, the parameter has no
     *   reflexive form.
     *
     *   info[5] = true if this is a nominative usage, nil if not.  We use
     *   this to determine which target objects are used in the nominative
     *   case, so that we can remember those objects for subsequent
     *   reflexive usages.
     */
    paramList_ =
    [
        /* parameters that imply the actor as the target object */
        ['you/he', &theName, 'actor', nil, true],
        ['you/she', &theName, 'actor', nil, true],
        ['you\'re/he\'s', &itIsContraction, 'actor', nil, true],
        ['you\'re/she\'s', &itIsContraction, 'actor', nil, true],
        ['you\'re', &itIsContraction, 'actor', nil, true],
        ['you/him', &theNameObj, 'actor', &itReflexive, nil],
        ['you/her', &theNameObj, 'actor', &itReflexive, nil],
        ['your/her', &theNamePossAdj, 'actor', nil, nil],
        ['your/his', &theNamePossAdj, 'actor', nil, nil],
        ['your', &theNamePossAdj, 'actor', nil, nil],
        ['yours/hers', &theNamePossNoun, 'actor', nil, nil],
        ['yours/his', &theNamePossNoun, 'actor', nil, nil],
        ['yours', &theNamePossNoun, 'actor', nil, nil],
        ['yourself/himself', &itReflexive, 'actor', nil, nil],
        ['yourself/herself', &itReflexive, 'actor', nil, nil],
        ['yourself', &itReflexive, 'actor', nil, nil],

        /* parameters that don't imply any target object */
        ['the/he', &theName, nil, nil, true],
        ['the/she', &theName, nil, nil, true],
        ['the/him', &theNameObj, nil, &itReflexive, nil],
        ['the/her', &theNameObj, nil, &itReflexive, nil],
        ['the\'s/her', &theNamePossAdj, nil, &itPossAdj, nil],
        ['the\'s/hers', &theNamePossNoun, nil, &itPossNoun, nil],

        /*
         *  Verb 's' endings.  In most cases, you should use 's/d', 's/ed',
         *  or 's/?ed' rather than 's', except in places where you know you
         *  will never need the past tense form, because 's' doesn't handle
         *  the past tense.  Don't use 's/?ed' with a literal question
         *  mark; put a consonant in place of the question mark instead.
         */
        ['s', &verbEndingS, nil, nil, true],
        ['s/d', &verbEndingSD, nil, nil, true],
        ['s/ed', &verbEndingSEd, nil, nil, true],
        ['s/?ed', &verbEndingSMessageBuilder_, nil, nil, true],

        ['es', &verbEndingEs, nil, nil, true],
        ['es/ed', &verbEndingEs, nil, nil, true],
        ['ies', &verbEndingIes, nil, nil, true],
        ['ies/ied', &verbEndingIes, nil, nil, true],
        ['is', &verbToBe, nil, nil, true],
        ['are', &verbToBe, nil, nil, true],
        ['was', &verbWas, nil, nil, true],
        ['were', &verbWas, nil, nil, true],
        ['has', &verbToHave, nil, nil, true],
        ['have', &verbToHave, nil, nil, true],
        ['does', &verbToDo, nil, nil, true],
        ['do', &verbToDo, nil, nil, true],
        ['goes', &verbToGo, nil, nil, true],
        ['go', &verbToGo, nil, nil, true],
        ['comes', &verbToCome, nil, nil, true],
        ['come', &verbToCome, nil, nil, true],
        ['leaves', &verbToLeave, nil, nil, true],
        ['leave', &verbToLeave, nil, nil, true],
        ['sees', &verbToSee, nil, nil, true],
        ['see', &verbToSee, nil, nil, true],
        ['says', &verbToSay, nil, nil, true],
        ['say', &verbToSay, nil, nil, true],
        ['must', &verbMust, nil, nil, true],
        ['can', &verbCan, nil, nil, true],
        ['cannot', &verbCannot, nil, nil, true],
        ['can\'t', &verbCant, nil, nil, true],
        ['will', &verbWill, nil, nil, true],
        ['won\'t', &verbWont, nil, nil, true],
        ['a/he', &aName, nil, nil, true],
        ['an/he', &aName, nil, nil, true],
        ['a/she', &aName, nil, nil, true],
        ['an/she', &aName, nil, nil, true],
        ['a/him', &aNameObj, nil, &itReflexive, nil],
        ['an/him', &aNameObj, nil, &itReflexive, nil],
        ['a/her', &aNameObj, nil, &itReflexive, nil],
        ['an/her', &aNameObj, nil, &itReflexive, nil],
        ['it/he', &itNom, nil, nil, true],
        ['it/she', &itNom, nil, nil, true],
        ['it/him', &itObj, nil, &itReflexive, nil],
        ['it/her', &itObj, nil, &itReflexive, nil],

        /*
         *   note that we don't have its/his, because that leaves
         *   ambiguous whether we want an adjective or noun form - so we
         *   only use the feminine pronouns with these, to make the
         *   meaning unambiguous
         */
        ['its/her', &itPossAdj, nil, nil, nil],
        ['its/hers', &itPossNoun, nil, nil, nil],

        ['it\'s/he\'s', &itIsContraction, nil, nil, true],
        ['it\'s/she\'s', &itIsContraction, nil, nil, true],
        ['it\'s', &itIsContraction, nil, nil, true],
        ['that/he', &thatNom, nil, nil, true],
        ['that/she', &thatNom, nil, nil, true],
        ['that/him', &thatObj, nil, &itReflexive, nil],
        ['that/her', &thatObj, nil, &itReflexive, nil],
        ['that\'s', &thatIsContraction, nil, nil, true],
        ['itself', &itReflexive, nil, nil, nil],
        ['itself/himself', &itReflexive, nil, nil, nil],
        ['itself/herself', &itReflexive, nil, nil, nil],

        /* default preposition for standing in/on something */
        ['on', &actorInName, nil, nil, nil],
        ['in', &actorInName, nil, nil, nil],
        ['outof', &actorOutOfName, nil, nil, nil],
        ['offof', &actorOutOfName, nil, nil, nil],
        ['onto', &actorIntoName, nil, nil, nil],
        ['into', &actorIntoName, nil, nil, nil],

        /*
         *   The special invisible subject marker - this can be used to
         *   mark the subject in sentences that vary from the
         *   subject-verb-object structure that most English sentences
         *   take.  The usual SVO structure allows the message builder to
         *   see the subject first in most sentences naturally, but in
         *   unusual sentence forms it is sometimes useful to be able to
         *   mark the subject explicitly.  This doesn't actually result in
         *   any output; it's purely for marking the subject for our
         *   internal book-keeping.
         *
         *   (The main reason the message builder wants to know the subject
         *   in the first place is so that it can use a reflexive pronoun
         *   if the same object ends up being used as a direct or indirect
         *   object: "you can't open yourself" rather than "you can't open
         *   you.")
         */
        ['subj', &dummyName, nil, nil, true]
    ]

    /*
     *   Add a hook to the generateMessage method, which we use to
     *   pre-process the source string before expanding the substitution
     *   parameters.
     */
    generateMessage(orig) { return inherited(processOrig(orig)); }

    /*
     *   Pre-process a source string containing substitution parameters,
     *   before generating the expanded message from it.
     *
     *   We use this hook to implement the special tense-switching syntax
     *   {<present>|<past>}.  Although it superficially looks like an
     *   ordinary substitution parameter, we actually can't use the normal
     *   parameter substitution framework for that, because we want to
     *   allow the <present> and <past> substrings themselves to contain
     *   substitution parameters, and the normal framework doesn't allow
     *   for recursive substitution.
     *
     *   We simply replace every sequence of the form {<present>|<past>}
     *   with either <present> or <past>, depending on the current
     *   narrative tense.  We then substitute braces for square brackets in
     *   the resulting string.  This allows treating every bracketed tag
     *   inside the tense-switching sequence as a regular substitution
     *   parameter.
     *
     *   For example, the sequence "{take[s]|took}" appearing in the
     *   message string would be replaced with "take{s}" if the current
     *   narrative tense is present, and would be replaced with "took" if
     *   the current narrative tense is past.  The string "take{s}", if
     *   selected, would in turn be expanded to either "take" or "takes",
     *   depending on the grammatical person of the subject, as per the
     *   regular substitution mechanism.
     */
    processOrig(str)
    {
        local idx = 1;
        local len;
        local match;
        local replStr;

        /*
         *   Keep searching the string until we run out of character
         *   sequences with a special meaning (specifically, we look for
         *   substrings enclosed in braces, and stuttered opening braces).
         */
        for (;;)
        {
            /*
             *   Find the next special sequence.
             */
            match = rexSearch(patSpecial, str, idx);

            /*
             *   If there are no more special sequence, we're done
             *   pre-processing the string.
             */
            if (match == nil) break;

            /*
             *   Remember the starting index and length of the special
             *   sequence.
             */
            idx = match[1];
            len = match[2];

            /*
             *   Check if this special sequence matches our tense-switching
             *   syntax.
             */
            if (rexMatch(patTenseSwitching, str, idx) == nil)
            {
                /*
                 *   It doesn't, so forget about it and continue searching
                 *   from the end of this special sequence.
                 */
                idx += len;
                continue;
            }

            /*
             *   Extract either the first or the second embedded string,
             *   depending on the current narrative tense.
             */
            match = rexGroup(tSel(1, 2));
            replStr = match[3];

            /*
             *   Convert all square brackets to braces in the extracted
             *   string.
             */
            replStr = replStr.findReplace('[', '{', ReplaceAll);
            replStr = replStr.findReplace(']', '}', ReplaceAll);

            /*
             *   In the original string, replace the tense-switching
             *   sequence with the extracted string.
             */
            str = str.substr(1, idx - 1) + replStr + str.substr(idx + len);

            /*
             *   Move the index at the end of the substituted string.
             */
            idx += match[2];
        }

        /*
         *   We're done - return the result.
         */
        return str;
    }

    /*
     *   Pre-compiled regular expression pattern matching any sequence with
     *   a special meaning in a message string.
     *
     *   We match either a stuttered opening brace, or a single opening
     *   brace followed by any sequence of characters that doesn't contain
     *   a closing brace followed by a closing brace.
     */
    patSpecial = static new RexPattern
        ('<lbrace><lbrace>|<lbrace>(?!<lbrace>)((?:<^rbrace>)*)<rbrace>')

    /*
     *   Pre-compiled regular expression pattern matching our special
     *   tense-switching syntax.
     *
     *   We match a single opening brace, followed by any sequence of
     *   characters that doesn't contain a closing brace or a vertical bar,
     *   followed by a vertical bar, followed by any sequence of characters
     *   that doesn't contain a closing brace or a vertical bar, followed
     *   by a closing brace.
     */
    patTenseSwitching = static new RexPattern
    (
        '<lbrace>(?!<lbrace>)((?:<^rbrace|vbar>)*)<vbar>'
                          + '((?:<^rbrace|vbar>)*)<rbrace>'
    )

    /*
     *   The most recent target object used in the nominative case.  We
     *   note this so that we can supply reflexive mappings when the same
     *   object is re-used in the objective case.  This allows us to map
     *   things like "you can't take you" to the better-sounding "you
     *   can't take yourself".
     */
    lastSubject_ = nil

    /* the parameter name of the last subject ('dobj', 'actor', etc) */
    lastSubjectName_ = nil

    /*
     *   Get the target object property mapping.  If the target object is
     *   the same as the most recent subject object (i.e., the last object
     *   used in the nominative case), and this parameter has a reflexive
     *   form property, we'll return the reflexive form property.
     *   Otherwise, we'll return the standard property mapping.
     *
     *   Also, if there was an exclamation mark at the end of any word in
     *   the tag, we'll return a property returning a fixed-tense form of
     *   the property for the tag.
     */
    getTargetProp(targetObj, paramObj, info)
    {
        local ret;

        /*
         *   If this target object matches the last subject, and we have a
         *   reflexive rendering, return the property for the reflexive
         *   rendering.
         *
         *   Only use the reflexive rendering if the parameter name is
         *   different - if the parameter name is the same, then presumably
         *   the message will have been written with a reflexive pronoun or
         *   not, exactly as the author wants it.  When the author knows
         *   going in that these two objects are structurally the same,
         *   they want the exact usage they wrote.
         */
        if (targetObj == lastSubject_
            && paramObj != lastSubjectName_
            && info[4] != nil)
        {
            /* use the reflexive rendering */
            ret = info[4];
        }
        else
        {
            /* no special handling; inherit the default handling */
            ret = inherited(targetObj, paramObj, info);
        }

        /* if this is a nominative usage, note it as the last subject */
        if (info[5])
        {
            lastSubject_ = targetObj;
            lastSubjectName_ = paramObj;
        }

        /*
         *   If there was an exclamation mark at the end of any word in the
         *   parameter string (which we remember via the fixedTenseProp_
         *   property), store the original target property in
         *   fixedTenseProp_ and use &propWithPresentMessageBuilder_ as the
         *   target property instead.  propWithPresentMessageBuilder_ acts
         *   as a wrapper for the original target property, which it
         *   invokes after temporarily switching to the present tense.
         */
        if (fixedTenseProp_)
        {
            fixedTenseProp_ = ret;
            ret = &propWithPresentMessageBuilder_;
        }

        /* return the result */
        return ret;
    }

    /* end-of-sentence match pattern */
    patEndOfSentence = static new RexPattern('[.;:!?]<^alphanum>')

    /*
     *   Process result text.
     */
    processResult(txt)
    {
        /*
         *   If the text contains any sentence-ending punctuation, reset
         *   our internal memory of the subject of the sentence.  We
         *   consider the sentence to end with a period, semicolon, colon,
         *   question mark, or exclamation point followed by anything
         *   other than an alpha-numeric.  (We require the secondary
         *   character so that we don't confuse things like "3:00" or
         *   "7.5" to contain sentence-ending punctuation.)
         */
        if (rexSearch(patEndOfSentence, txt) != nil)
        {
            /*
             *   we have a sentence ending in this run of text, so any
             *   saved subject object will no longer apply after this text
             *   - forget our subject object
             */
            lastSubject_ = nil;
            lastSubjectName_ = nil;
        }

        /* return the inherited processing */
        return inherited(txt);
    }

    /* some pre-compiled search patterns we use a lot */
    patIdObjSlashIdApostS = static new RexPattern(
        '(<^space>+)(<space>+<^space>+)\'s(/<^space>+)$')
    patIdObjApostS = static new RexPattern(
        '(?!<^space>+\'s<space>)(<^space>+)(<space>+<^space>+)\'s$')
    patParamWithExclam = static new RexPattern('.*(!)(?:<space>.*|/.*|$)')
    patSSlashLetterEd = static new RexPattern(
        's/(<alpha>ed)$|(<alpha>ed)/s$')

    /*
     *   Rewrite a parameter string for a language-specific syntax
     *   extension.
     *
     *   For English, we'll handle the possessive apostrophe-s suffix
     *   specially, by allowing the apostrophe-s to be appended to the
     *   target object name.  If we find an apostrophe-s on the target
     *   object name, we'll move it to the preceding identifier name:
     *
     *   the dobj's -> the's dobj
     *.  the dobj's/he -> the's dobj/he
     *.  he/the dobj's -> he/the's dobj
     *
     *   We also use this method to check for the presence of an
     *   exclamation mark at the end of any word in the parameter string
     *   (triggering the fixed-tense handling), and to detect a parameter
     *   string matching the {s/?ed} syntax, where ? is any letter, and
     *   rewrite it literally as 's/?ed' literally.
     */
    langRewriteParam(paramStr)
    {
        /*
         *   Check for an exclamation mark at the end of any word in the
         *   parameter string, and remember the result of the test.
         */
        local exclam = rexMatch(patParamWithExclam, paramStr);
        fixedTenseProp_ = exclam;

        /*
         *   Remove the exclamation mark, if any.
         */
        if (exclam)
        {
            local exclamInd = rexGroup(1)[1];
            paramStr = paramStr.substr(1, exclamInd - 1)
                       + paramStr.substr(exclamInd + 1);
        }

        /* look for "id obj's" and "id1 obj's/id2" */
        if (rexMatch(patIdObjSlashIdApostS, paramStr) != nil)
        {
            /* rewrite with the "'s" moved to the preceding parameter name */
            paramStr = rexGroup(1)[3] + '\'s'
                       + rexGroup(2)[3] + rexGroup(3)[3];
        }
        else if (rexMatch(patIdObjApostS, paramStr) != nil)
        {
            /* rewrite with the "'s" moved to the preceding parameter name */
            paramStr = rexGroup(1)[3] + '\'s' + rexGroup(2)[3];
        }

        /*
         *   Check if this parameter matches the {s/?ed} or {?ed/s} syntax.
         */
        if (rexMatch(patSSlashLetterEd, paramStr))
        {
            /*
             *   It does - remember the past verb ending, and rewrite the
             *   parameter literally as 's/?ed'.
             */
            pastEnding_ = rexGroup(1)[3];
            paramStr = 's/?ed';
        }

        /* return our (possibly modified) result */
        return paramStr;
    }

    /*
     *   This property is used to temporarily store the past-tense ending
     *   of a verb to be displayed by Thing.verbEndingSMessageBuilder_.
     *   It's for internal use only; game authors shouldn't have any reason
     *   to access it directly.
     */
    pastEnding_ = nil

    /*
     *   This property is used to temporarily store either a boolean value
     *   indicating whether the last encountered parameter string had an
     *   exclamation mark at the end of any word, or a property to be
     *   invoked by Thing.propWithPresentMessageBuilder_.  This field is
     *   for internal use only; authors shouldn't have any reason to access
     *   it directly.
     */
    fixedTenseProp_ = nil
;


/* ------------------------------------------------------------------------ */
/*
 *   Temporarily override the current narrative tense and invoke a callback
 *   function.
 */
withTense(usePastTense, callback)
{
    /*
     *   Remember the old value of the usePastTense flag.
     */
    local oldUsePastTense = gameMain.usePastTense;
    /*
     *   Set the new value.
     */
    gameMain.usePastTense = usePastTense;
    /*
     *   Invoke the callback (remembering the return value) and restore the
     *   usePastTense flag on our way out.
     */
    local ret;
    try { ret = callback(); }
    finally { gameMain.usePastTense = oldUsePastTense; }
    /*
     *   Return the result.
     */
    return ret;
}


/* ------------------------------------------------------------------------ */
/*
 *   Functions for spelling out numbers.  These functions take a numeric
 *   value as input, and return a string with the number spelled out as
 *   words in English.  For example, given the number 52, we'd return a
 *   string like 'fifty-two'.
 *
 *   These functions obviously have language-specific implementations.
 *   Note also that even their interfaces might vary by language.  Some
 *   languages might need additional information in the interface; for
 *   example, some languages might need to know the grammatical context
 *   (such as part of speech, case, or gender) of the result.
 *
 *   Note that some of the spellIntXxx flags might not be meaningful in all
 *   languages, because most of the flags are by their very nature
 *   associated with language-specific idioms.  Translations are free to
 *   ignore flags that indicate variations with no local equivalent, and to
 *   add their own language-specific flags as needed.
 */

/*
 *   Spell out an integer number in words.  Returns a string with the
 *   spelled-out number.
 *
 *   Note that this simple version of the function uses the default
 *   options.  If you want to specify non-default options with the
 *   SpellIntXxx flags, you can call spellIntExt().
 */
spellInt(val)
{
    return spellIntExt(val, 0);
}

/*
 *   Spell out an integer number in words, but only if it's below the given
 *   threshold.  It's often awkward in prose to spell out large numbers,
 *   but exactly what constitutes a large number depends on context, so
 *   this routine lets the caller specify the threshold.
 *   
 *   If the absolute value of val is less than (not equal to) the threshold
 *   value, we'll return a string with the number spelled out.  If the
 *   absolute value is greater than or equal to the threshold value, we'll
 *   return a string representing the number in decimal digits.  
 */
spellIntBelow(val, threshold)
{
    return spellIntBelowExt(val, threshold, 0, 0);
}

/*
 *   Spell out an integer number in words if it's below a threshold, using
 *   the spellIntXxx flags given in spellFlags to control the spelled-out
 *   format, and using the DigitFormatXxx flags in digitFlags to control
 *   the digit format.  
 */
spellIntBelowExt(val, threshold, spellFlags, digitFlags)
{
    local absval;

    /* compute the absolute value */
    absval = (val < 0 ? -val : val);

    /* check the value to see whether to spell it or write it as digits */
    if (absval < threshold)
    {
        /* it's below the threshold - spell it out in words */
        return spellIntExt(val, spellFlags);
    }
    else
    {
        /* it's not below the threshold - write it as digits */
        return intToDecimal(val, digitFlags);
    }
}

/*
 *   Format a number as a string of decimal digits.  The DigitFormatXxx
 *   flags specify how the number is to be formatted.`
 */
intToDecimal(val, flags)
{
    local str;
    local sep;

    /* perform the basic conversion */
    str = toString(val);

    /* add group separators as needed */
    if ((flags & DigitFormatGroupComma) != 0)
    {
        /* explicitly use a comma as a separator */
        sep = ',';
    }
    else if ((flags & DigitFormatGroupPeriod) != 0)
    {
        /* explicitly use a period as a separator */
        sep = '.';
    }
    else if ((flags & DigitFormatGroupSep) != 0)
    {
        /* use the current languageGlobals separator */
        sep = languageGlobals.digitGroupSeparator;
    }
    else
    {
        /* no separator */
        sep = nil;
    }

    /* if there's a separator, add it in */
    if (sep != nil)
    {
        local i;
        local len;

        /*
         *   Insert the separator before each group of three digits.
         *   Start at the right end of the string and work left: peel off
         *   the last three digits and insert a comma.  Then, move back
         *   four characters through the string - another three-digit
         *   group, plus the comma we inserted - and repeat.  Keep going
         *   until the amount we'd want to peel off the end is as long or
         *   longer than the entire remaining string.
         */
        for (i = 3, len = str.length() ; len > i ; i += 4)
        {
            /* insert this comma */
            str = str.substr(1, len - i) + sep + str.substr(len - i + 1);

            /* note the new length */
            len = str.length();
        }
    }

    /* return the result */
    return str;
}

/*
 *   Spell out an integer number - "extended" interface with flags.  The
 *   "flags" argument is a (bitwise-OR'd) combination of SpellIntXxx
 *   values, specifying the desired format of the result.
 */
spellIntExt(val, flags)
{
    local str;
    local trailingSpace;
    local needAnd;
    local powers = [1000000000, ' billion ',
                    1000000,    ' million ',
                    1000,       ' thousand ',
                    100,        ' hundred '];

    /* start with an empty string */
    str = '';
    trailingSpace = nil;
    needAnd = nil;

    /* if it's zero, it's a special case */
    if (val == 0)
        return 'zero';

    /*
     *   if the number is negative, note it in the string, and use the
     *   absolute value
     */
    if (val < 0)
    {
        str = 'negative ';
        val = -val;
    }

    /* do each named power of ten */
    for (local i = 1 ; val >= 100 && i <= powers.length() ; i += 2)
    {
        /*
         *   if we're in teen-hundreds mode, do the teen-hundreds - this
         *   only works for values from 1,100 to 9,999, since a number like
         *   12,000 doesn't work this way - 'one hundred twenty hundred' is
         *   no good 
         */
        if ((flags & SpellIntTeenHundreds) != 0
            && val >= 1100 && val < 10000)
        {
            /* if desired, add a comma if there was a prior power group */
            if (needAnd && (flags & SpellIntCommas) != 0)
                str = str.substr(1, str.length() - 1) + ', ';

            /* spell it out as a number of hundreds */
            str += spellIntExt(val / 100, flags) + ' hundred ';

            /* take off the hundreds */
            val %= 100;

            /* note the trailing space */
            trailingSpace = true;

            /* we have something to put an 'and' after, if desired */
            needAnd = true;

            /*
             *   whatever's left is below 100 now, so there's no need to
             *   keep scanning the big powers of ten
             */
            break;
        }

        /* if we have something in this power range, apply it */
        if (val >= powers[i])
        {
            /* if desired, add a comma if there was a prior power group */
            if (needAnd && (flags & SpellIntCommas) != 0)
                str = str.substr(1, str.length() - 1) + ', ';

            /* add the number of multiples of this power and the power name */
            str += spellIntExt(val / powers[i], flags) + powers[i+1];

            /* take it out of the remaining value */
            val %= powers[i];

            /*
             *   note that we have a trailing space in the string (all of
             *   the power-of-ten names have a trailing space, to make it
             *   easy to tack on the remainder of the value)
             */
            trailingSpace = true;

            /* we have something to put an 'and' after, if one is desired */
            needAnd = true;
        }
    }

    /*
     *   if we have anything left, and we have written something so far,
     *   and the caller wanted an 'and' before the tens part, add the
     *   'and'
     */
    if ((flags & SpellIntAndTens) != 0
        && needAnd
        && val != 0)
    {
        /* add the 'and' */
        str += 'and ';
        trailingSpace = true;
    }

    /* do the tens */
    if (val >= 20)
    {
        /* anything above the teens is nice and regular */
        str += ['twenty', 'thirty', 'forty', 'fifty', 'sixty',
                'seventy', 'eighty', 'ninety'][val/10 - 1];
        val %= 10;

        /* if it's non-zero, we'll add the units, so add a hyphen */
        if (val != 0)
            str += '-';

        /* we no longer have a trailing space in the string */
        trailingSpace = nil;
    }
    else if (val >= 10)
    {
        /* we have a teen */
        str += ['ten', 'eleven', 'twelve', 'thirteen', 'fourteen',
                'fifteen', 'sixteen', 'seventeen', 'eighteen',
                'nineteen'][val - 9];

        /* we've finished with the number */
        val = 0;

        /* there's no trailing space */
        trailingSpace = nil;
    }

    /* if we have a units value, add it */
    if (val != 0)
    {
        /* add the units name */
        str += ['one', 'two', 'three', 'four', 'five',
                'six', 'seven', 'eight', 'nine'][val];

        /* we have no trailing space now */
        trailingSpace = nil;
    }

    /* if there's a trailing space, remove it */
    if (trailingSpace)
        str = str.substr(1, str.length() - 1);

    /* return the string */
    return str;
}

/*
 *   Return a string giving the numeric ordinal representation of a number:
 *   1st, 2nd, 3rd, 4th, etc.  
 */
intOrdinal(n)
{
    local s;

    /* start by getting the string form of the number */
    s = toString(n);

    /* now add the appropriate suffix */
    if (n >= 10 && n <= 19)
    {
        /* the teens all end in 'th' */
        return s + 'th';
    }
    else
    {
        /*
         *   for anything but a teen, a number whose last digit is 1
         *   always has the suffix 'st' (for 'xxx-first', as in '141st'),
         *   a number whose last digit is 2 always ends in 'nd' (for
         *   'xxx-second', as in '532nd'), a number whose last digit is 3
         *   ends in 'rd' (for 'xxx-third', as in '53rd'), and anything
         *   else ends in 'th'
         */
        switch(n % 10)
        {
        case 1:
            return s + 'st';

        case 2:
            return s + 'nd';

        case 3:
            return s + 'rd';

        default:
            return s + 'th';
        }
    }
}


/*
 *   Return a string giving a fully spelled-out ordinal form of a number:
 *   first, second, third, etc.
 */
spellIntOrdinal(n)
{
    return spellIntOrdinalExt(n, 0);
}

/*
 *   Return a string giving a fully spelled-out ordinal form of a number:
 *   first, second, third, etc.  This form takes the same flag values as
 *   spellIntExt().
 */
spellIntOrdinalExt(n, flags)
{
    local s;

    /* get the spelled-out form of the number itself */
    s = spellIntExt(n, flags);

    /*
     *   If the number ends in 'one', change the ending to 'first'; 'two'
     *   becomes 'second'; 'three' becomes 'third'; 'five' becomes
     *   'fifth'; 'eight' becomes 'eighth'; 'nine' becomes 'ninth'.  If
     *   the number ends in 'y', change the 'y' to 'ieth'.  'Zero' becomes
     *   'zeroeth'.  For everything else, just add 'th' to the spelled-out
     *   name
     */
    if (s == 'zero')
        return 'zeroeth';
    if (s.endsWith('one'))
        return s.substr(1, s.length() - 3) + 'first';
    else if (s.endsWith('two'))
        return s.substr(1, s.length() - 3) + 'second';
    else if (s.endsWith('three'))
        return s.substr(1, s.length() - 5) + 'third';
    else if (s.endsWith('five'))
        return s.substr(1, s.length() - 4) + 'fifth';
    else if (s.endsWith('eight'))
        return s.substr(1, s.length() - 5) + 'eighth';
    else if (s.endsWith('nine'))
        return s.substr(1, s.length() - 4) + 'ninth';
    else if (s.endsWith('y'))
        return s.substr(1, s.length() - 1) + 'ieth';
    else
        return s + 'th';
}

/* ------------------------------------------------------------------------ */
/*
 *   Parse a spelled-out number.  This is essentially the reverse of
 *   spellInt() and related functions: we take a string that contains a
 *   spelled-out number and return the integer value.  This uses the
 *   command parser's spelled-out number rules, so we can parse anything
 *   that would be recognized as a number in a command.
 *
 *   If the string contains numerals, we'll treat it as a number in digit
 *   format: for example, if it contains '789', we'll return 789.
 *
 *   If the string doesn't parse as a number, we return nil.
 */
parseInt(str)
{
    try
    {
        /* tokenize the string */
        local toks = cmdTokenizer.tokenize(str);

        /* parse it */
        return parseIntTokens(toks);
    }
    catch (Exception exc)
    {
        /*
         *   on any exception, just return nil to indicate that we couldn't
         *   parse the string as a number
         */
        return nil;
    }
}

/*
 *   Parse a spelled-out number that's given as a token list (as returned
 *   from Tokenizer.tokenize).  If we can successfully parse the token list
 *   as a number, we'll return the integer value.  If not, we'll return
 *   nil.
 */
parseIntTokens(toks)
{
    try
    {
        /*
         *   if the first token contains digits, treat it as a numeric
         *   string value rather than a spelled-out number
         */
        if (toks.length() != 0
            && rexMatch('<digit>+', getTokOrig(toks[1])) != nil)
            return toInteger(getTokOrig(toks[1]));

        /* parse it using the spelledNumber production */
        local lst = spelledNumber.parseTokens(toks, cmdDict);

        /*
         *   if we got a match, return the integer value; if not, it's not
         *   parseable as a number, so return nil
         */
        return (lst.length() != 0 ? lst[1].getval() : nil);
    }
    catch (Exception exc)
    {
        /*
         *   on any exception, just return nil to indicate that it's not
         *   parseable as a number
         */
        return nil;
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Additional token types for US English.
 */

/* special "apostrophe-s" token */
enum token tokApostropheS;

/* special apostrophe token for plural possessives ("the smiths' house") */
enum token tokPluralApostrophe;

/* special abbreviation-period token */
enum token tokAbbrPeriod;

/* special "#nnn" numeric token */
enum token tokPoundInt;

/*
 *   Command tokenizer for US English.  Other language modules should
 *   provide their own tokenizers to allow for differences in punctuation
 *   and other lexical elements.
 */
cmdTokenizer: Tokenizer
    rules_ = static
    [
        /* skip whitespace */
        ['whitespace', new RexPattern('<Space>+'), nil, &tokCvtSkip, nil],

        /* certain punctuation marks */
        ['punctuation', new RexPattern('[.,;:?!]'), tokPunct, nil, nil],

        /*
         *   We have a special rule for spelled-out numbers from 21 to 99:
         *   when we see a 'tens' word followed by a hyphen followed by a
         *   digits word, we'll pull out the tens word, the hyphen, and
         *   the digits word as separate tokens.
         */
        ['spelled number',
         new RexPattern('<NoCase>(twenty|thirty|forty|fifty|sixty|'
                        + 'seventy|eighty|ninety)-'
                        + '(one|two|three|four|five|six|seven|eight|nine)'
                        + '(?!<AlphaNum>)'),
         tokWord, &tokCvtSpelledNumber, nil],


        /*
         *   Initials.  We'll look for strings of three or two initials,
         *   set off by periods but without spaces.  We'll look for
         *   three-letter initials first ("G.H.W. Billfold"), then
         *   two-letter initials ("X.Y. Zed"), so that we find the longest
         *   sequence that's actually in the dictionary.  Note that we
         *   don't have a separate rule for individual initials, since
         *   we'll pick that up with the regular abbreviated word rule
         *   below.
         *
         *   Some games could conceivably extend this to allow strings of
         *   initials of four letters or longer, but in practice people
         *   tend to elide the periods in longer sets of initials, so that
         *   the initials become an acronym, and thus would fit the
         *   ordinary word token rule.
         */
        ['three initials',
         new RexPattern('<alpha><period><alpha><period><alpha><period>'),
         tokWord, &tokCvtAbbr, &acceptAbbrTok],

        ['two initials',
         new RexPattern('<alpha><period><alpha><period>'),
         tokWord, &tokCvtAbbr, &acceptAbbrTok],

        /*
         *   Abbbreviated word - this is a word that ends in a period,
         *   such as "Mr.".  This rule comes before the ordinary word rule
         *   because we will only consider the period to be part of the
         *   word (and not a separate token) if the entire string
         *   including the period is in the main vocabulary dictionary.
         */
        ['abbreviation',
         new RexPattern('<Alpha|-><AlphaNum|-|squote>*<period>'),
         tokWord, &tokCvtAbbr, &acceptAbbrTok],

        /*
         *   A word ending in an apostrophe-s.  We parse this as two
         *   separate tokens: one for the word and one for the
         *   apostrophe-s.
         */
        ['apostrophe-s word',
         new RexPattern('<Alpha|-|&><AlphaNum|-|&|squote>*<squote>[sS]'),
         tokWord, &tokCvtApostropheS, nil],

        /*
         *   A plural word ending in an apostrophe.  We parse this as two
         *   separate tokens: one for the word and one for the apostrophe. 
         */
        ['plural possessive word',
         new RexPattern('<Alpha|-|&><AlphaNum|-|&|squote>*<squote>'
                        + '(?!<AlphaNum>)'),
         tokWord, &tokCvtPluralApostrophe, nil],

        /*
         *   Words - note that we convert everything to lower-case.  A word
         *   must start with an alphabetic character, a hyphen, or an
         *   ampersand; after the initial character, a word can contain
         *   alphabetics, digits, hyphens, ampersands, and apostrophes.
         */
        ['word',
         new RexPattern('<Alpha|-|&><AlphaNum|-|&|squote>*'),
         tokWord, nil, nil],

        /* an abbreviation word starting with a number */
        ['abbreviation with initial digit',
         new RexPattern('<Digit>(?=<AlphaNum|-|&|squote>*<Alpha|-|&|squote>)'
                        + '<AlphaNum|-|&|squote>*<period>'),
         tokWord, &tokCvtAbbr, &acceptAbbrTok],

        /*
         *   A word can start with a number, as long as there's something
         *   other than numbers in the string - if it's all numbers, we
         *   want to treat it as a numeric token.
         */
        ['word with initial digit',
         new RexPattern('<Digit>(?=<AlphaNum|-|&|squote>*<Alpha|-|&|squote>)'
                        + '<AlphaNum|-|&|squote>*'), tokWord, nil, nil],

        /* strings with ASCII "straight" quotes */
        ['string ascii-quote',
         new RexPattern('<min>([`\'"])(.*)%1(?!<AlphaNum>)'),
         tokString, nil, nil],

        /* some people like to use single quotes like `this' */
        ['string back-quote',
         new RexPattern('<min>`(.*)\'(?!<AlphaNum>)'), tokString, nil, nil],

        /* strings with Latin-1 curly quotes (single and double) */
        ['string curly single-quote',
         new RexPattern('<min>\u2018(.*)\u2019'), tokString, nil, nil],
        ['string curly double-quote',
         new RexPattern('<min>\u201C(.*)\u201D'), tokString, nil, nil],

        /*
         *   unterminated string - if we didn't just match a terminated
         *   string, but we have what looks like the start of a string,
         *   match to the end of the line
         */
        ['string unterminated',
         new RexPattern('([`\'"\u2018\u201C](.*)'), tokString, nil, nil],

        /* integer numbers */
        ['integer', new RexPattern('[0-9]+'), tokInt, nil, nil],

        /* numbers with a '#' preceding */
        ['integer with #',
         new RexPattern('#[0-9]+'), tokPoundInt, nil, nil]
    ]

    /*
     *   Handle an apostrophe-s word.  We'll return this as two separate
     *   tokens: one for the word preceding the apostrophe-s, and one for
     *   the apostrophe-s itself.
     */
    tokCvtApostropheS(txt, typ, toks)
    {
        local w;
        local s;

        /*
         *   pull out the part up to but not including the apostrophe, and
         *   pull out the apostrophe-s part
         */
        w = txt.substr(1, txt.length() - 2);
        s = txt.substr(-2);

        /* add the part before the apostrophe as the main token type */
        toks.append([w, typ, w]);

        /* add the apostrophe-s as a separate special token */
        toks.append([s, tokApostropheS, s]);
    }

    /*
     *   Handle a plural apostrophe word ("the smiths' house").  We'll
     *   return this as two tokens: one for the plural word, and one for
     *   the apostrophe. 
     */
    tokCvtPluralApostrophe(txt, typ, toks)
    {
        local w;
        local s;

        /*
         *   pull out the part up to but not including the apostrophe, and
         *   separately pull out the apostrophe 
         */
        w = txt.substr(1, txt.length() - 1);
        s = txt.substr(-1);

        /* add the part before the apostrophe as the main token type */
        toks.append([w, typ, w]);

        /* add the apostrophe-s as a separate special token */
        toks.append([s, tokPluralApostrophe, s]);
    }

    /*
     *   Handle a spelled-out hyphenated number from 21 to 99.  We'll
     *   return this as three separate tokens: a word for the tens name, a
     *   word for the hyphen, and a word for the units name.
     */
    tokCvtSpelledNumber(txt, typ, toks)
    {
        /* parse the number into its three parts with a regular expression */
        rexMatch(patAlphaDashAlpha, txt);

        /* add the part before the hyphen */
        toks.append([rexGroup(1)[3], typ, rexGroup(1)[3]]);

        /* add the hyphen */
        toks.append(['-', typ, '-']);

        /* add the part after the hyphen */
        toks.append([rexGroup(2)[3], typ, rexGroup(2)[3]]);
    }
    patAlphaDashAlpha = static new RexPattern('(<alpha>+)-(<alpha>+)')

    /*
     *   Check to see if we want to accept an abbreviated token - this is
     *   a token that ends in a period, which we use for abbreviated words
     *   like "Mr." or "Ave."  We'll accept the token only if it appears
     *   as given - including the period - in the dictionary.  Note that
     *   we ignore truncated matches, since the only way we'll accept a
     *   period in a word token is as the last character; there is thus no
     *   way that a token ending in a period could be a truncation of any
     *   longer valid token.
     */
    acceptAbbrTok(txt)
    {
        /* look up the word, filtering out truncated results */
        return cmdDict.isWordDefined(
            txt, {result: (result & StrCompTrunc) == 0});
    }

    /*
     *   Process an abbreviated token.
     *
     *   When we find an abbreviation, we'll enter it with the abbreviated
     *   word minus the trailing period, plus the period as a separate
     *   token.  We'll mark the period as an "abbreviation period" so that
     *   grammar rules will be able to consider treating it as an
     *   abbreviation -- but since it's also a regular period, grammar
     *   rules that treat periods as regular punctuation will also be able
     *   to try to match the result.  This will ensure that we try it both
     *   ways - as abbreviation and as a word with punctuation - and pick
     *   the one that gives us the best result.
     */
    tokCvtAbbr(txt, typ, toks)
    {
        local w;

        /* add the part before the period as the ordinary token */
        w = txt.substr(1, txt.length() - 1);
        toks.append([w, typ, w]);

        /* add the token for the "abbreviation period" */
        toks.append(['.', tokAbbrPeriod, '.']);
    }

    /*
     *   Given a list of token strings, rebuild the original input string.
     *   We can't recover the exact input string, because the tokenization
     *   process throws away whitespace information, but we can at least
     *   come up with something that will display cleanly and produce the
     *   same results when run through the tokenizer.
     */
    buildOrigText(toks)
    {
        local str;

        /* start with an empty string */
        str = '';

        /* concatenate each token in the list */
        for (local i = 1, local len = toks.length() ; i <= len ; ++i)
        {
            /* add the current token to the string */
            str += getTokOrig(toks[i]);

            /*
             *   if this looks like a hyphenated number that we picked
             *   apart into two tokens, put it back together without
             *   spaces
             */
            if (i + 2 <= len
                && rexMatch(patSpelledTens, getTokVal(toks[i])) != nil
                && getTokVal(toks[i+1]) == '-'
                && rexMatch(patSpelledUnits, getTokVal(toks[i+2])) != nil)
            {
                /*
                 *   it's a hyphenated number, all right - put the three
                 *   tokens back together without any intervening spaces,
                 *   so ['twenty', '-', 'one'] turns into 'twenty-one'
                 */
                str += getTokOrig(toks[i+1]) + getTokOrig(toks[i+2]);

                /* skip ahead by the two extra tokens we're adding */
                i += 2;
            }
            else if (i + 1 <= len
                     && getTokType(toks[i]) == tokWord
                     && getTokType(toks[i+1]) is in
                        (tokApostropheS, tokPluralApostrophe))
            {
                /*
                 *   it's a word followed by an apostrophe-s token - these
                 *   are appended together without any intervening spaces
                 */
                str += getTokOrig(toks[i+1]);

                /* skip the extra token we added */
                ++i;
            }

            /*
             *   If another token follows, and the next token isn't a
             *   punctuation mark, and the previous token wasn't an open
             *   paren, add a space before the next token.
             */
            if (i != len
                && rexMatch(patPunct, getTokVal(toks[i+1])) == nil
                && getTokVal(toks[i]) != '(')
                str += ' ';
        }

        /* return the result string */
        return str;
    }

    /* some pre-compiled regular expressions */
    patSpelledTens = static new RexPattern(
        '<nocase>twenty|thirty|forty|fifty|sixty|seventy|eighty|ninety')
    patSpelledUnits = static new RexPattern(
        '<nocase>one|two|three|four|five|six|seven|eight|nine')
    patPunct = static new RexPattern('[.,;:?!)]')
;


/* ------------------------------------------------------------------------ */
/*
 *   Grammar Rules
 */

/*
 *   Command with explicit target actor.  When a command starts with an
 *   actor's name followed by a comma followed by a verb, we take it as
 *   being directed to the actor.
 */
grammar firstCommandPhrase(withActor):
    singleNounOnly->actor_ ',' commandPhrase->cmd_
    : FirstCommandProdWithActor

    /* "execute" the target actor phrase */
    execActorPhrase(issuingActor)
    {
        /* flag that the actor's being addressed in the second person */
        resolvedActor_.commandReferralPerson = SecondPerson;
    }
;

grammar firstCommandPhrase(askTellActorTo):
    ('ask' | 'tell' | 'a' | 't') singleNounOnly->actor_
    'to' commandPhrase->cmd_
    : FirstCommandProdWithActor

    /* "execute" the target actor phrase */
    execActorPhrase(issuingActor)
    {
        /*
         *   Since our phrasing is TELL <ACTOR> TO <DO SOMETHING>, the
         *   actor clearly becomes the antecedent for a subsequent
         *   pronoun.  For example, in TELL BOB TO READ HIS BOOK, the word
         *   HIS pretty clearly refers back to BOB.
         */
        if (resolvedActor_ != nil)
        {
            /* set the possessive anaphor object to the actor */
            resolvedActor_.setPossAnaphorObj(resolvedActor_);

            /* flag that the actor's being addressed in the third person */
            resolvedActor_.commandReferralPerson = ThirdPerson;

            /*
             *   in subsequent commands carried out by the issuer, the
             *   target actor is now the pronoun antecedent (for example:
             *   after TELL BOB TO GO NORTH, the command FOLLOW HIM means
             *   to follow Bob)
             */
            issuingActor.setPronounObj(resolvedActor_);
        }
    }
;

/*
 *   An actor-targeted command with a bad command phrase.  This is used as
 *   a fallback if we fail to match anything on the first attempt at
 *   parsing the first command on a line.  The point is to at least detect
 *   the target actor phrase, if that much is valid, so that we better
 *   customize error messages for the rest of the command.  
 */
grammar actorBadCommandPhrase(main):
    singleNounOnly->actor_ ',' miscWordList
    | ('ask' | 'tell' | 'a' | 't') singleNounOnly->actor_ 'to' miscWordList
    : FirstCommandProdWithActor

    /* to resolve nouns, we merely resolve the actor */
    resolveNouns(issuingActor, targetActor, results)
    {
        /* resolve the underlying actor phrase */
        return actor_.resolveNouns(getResolver(issuingActor), results);
    }
;


/*
 *   Command-only conjunctions.  These words and groups of words can
 *   separate commands from one another, and can't be used to separate noun
 *   phrases in a noun list.  
 */
grammar commandOnlyConjunction(sentenceEnding):
    '.'
    | '!'
    : BasicProd

    /* these conjunctions end the sentence */
    isEndOfSentence() { return true; }
;

grammar commandOnlyConjunction(nonSentenceEnding):
    'then'
    | 'and' 'then'
    | ',' 'then'
    | ',' 'and' 'then'
    | ';'
    : BasicProd

    /* these conjunctions do not end a sentence */
    isEndOfSentence() { return nil; }
;


/*
 *   Command-or-noun conjunctions.  These words and groups of words can be
 *   used to separate commands from one another, and can also be used to
 *   separate noun phrases in a noun list.
 */
grammar commandOrNounConjunction(main):
    ','
    | 'and'
    | ',' 'and'
    : BasicProd

    /* these do not end a sentence */
    isEndOfSentence() { return nil; }
;

/*
 *   Noun conjunctions.  These words and groups of words can be used to
 *   separate noun phrases from one another.  Note that these do not need
 *   to be exclusive to noun phrases - these can occur as command
 *   conjunctions as well; this list is separated from
 *   commandOrNounConjunction in case there are conjunctions that can never
 *   be used as command conjunctions, since such conjunctions, which can
 *   appear here, would not appear in commandOrNounConjunctions.  
 */
grammar nounConjunction(main):
    ','
    | 'and'
    | ',' 'and'
    : BasicProd

    /* these conjunctions do not end a sentence */
    isEndOfSentence() { return nil; }
;

/* ------------------------------------------------------------------------ */
/*
 *   Noun list: one or more noun phrases connected with conjunctions.  This
 *   kind of noun list can end in a terminal noun phrase.
 *   
 *   Note that a single noun phrase is a valid noun list, since a list can
 *   simply be a list of one.  The separate production nounMultiList can be
 *   used when multiple noun phrases are required.  
 */

/*
 *   a noun list can consist of a single terminal noun phrase
 */
grammar nounList(terminal): terminalNounPhrase->np_ : NounListProd
    resolveNouns(resolver, results)
    {
        /* resolve the underlying noun phrase */
        return np_.resolveNouns(resolver, results);
    }
;

/*
 *   a noun list can consist of a list of a single complete (non-terminal)
 *   noun phrase
 */
grammar nounList(nonTerminal): completeNounPhrase->np_ : NounListProd
    resolveNouns(resolver, results)
    {
        /* resolve the underlying noun phrase */
        return np_.resolveNouns(resolver, results);
    }
;

/*
 *   a noun list can consist of a list with two or more entries
 */
grammar nounList(list): nounMultiList->lst_ : NounListProd
    resolveNouns(resolver, results)
    {
        /* resolve the underlying list */
        return lst_.resolveNouns(resolver, results);
    }
;

/*
 *   An empty noun list is one with no words at all.  This is matched when
 *   a command requires a noun list but the player doesn't include one;
 *   this construct has "badness" because we only want to match it when we
 *   have no choice.
 */
grammar nounList(empty): [badness 500] : EmptyNounPhraseProd
    responseProd = nounList
;

/* ------------------------------------------------------------------------ */
/*
 *   Noun Multi List: two or more noun phrases connected by conjunctions.
 *   This is almost the same as the basic nounList production, but this
 *   type of production requires at least two noun phrases, whereas the
 *   basic nounList production more generally defines its list as any
 *   number - including one - of noun phrases.
 */

/*
 *   a multi list can consist of a noun multi- list plus a terminal noun
 *   phrase, separated by a conjunction
 */
grammar nounMultiList(multi):
    nounMultiList->lst_ nounConjunction terminalNounPhrase->np_
    : NounListProd
    resolveNouns(resolver, results)
    {
        /* return a list of all of the objects from both underlying lists */
        return np_.resolveNouns(resolver, results)
            + lst_.resolveNouns(resolver, results);
    }
;

/*
 *   a multi list can consist of a non-terminal multi list
 */
grammar nounMultiList(nonterminal): nonTerminalNounMultiList->lst_
    : NounListProd
    resolveNouns(resolver, results)
    {
        /* resolve the underlying list */
        return lst_.resolveNouns(resolver, results);
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   A non-terminal noun multi list is a noun list made up of at least two
 *   non-terminal noun phrases, connected by conjunctions.
 *
 *   This is almost the same as the regular non-terminal noun list
 *   production, but this production requires two or more underlying noun
 *   phrases, whereas the basic non-terminal noun list matches any number
 *   of underlying phrases, including one.
 */

/*
 *   a non-terminal multi-list can consist of a pair of complete noun
 *   phrases separated by a conjunction
 */
grammar nonTerminalNounMultiList(pair):
    completeNounPhrase->np1_ nounConjunction completeNounPhrase->np2_
    : NounListProd
    resolveNouns(resolver, results)
    {
        /* return the combination of the two underlying noun phrases */
        return np1_.resolveNouns(resolver, results)
            + np2_.resolveNouns(resolver, results);
    }
;

/*
 *   a non-terminal multi-list can consist of another non-terminal
 *   multi-list plus a complete noun phrase, connected by a conjunction
 */
grammar nonTerminalNounMultiList(multi):
    nonTerminalNounMultiList->lst_ nounConjunction completeNounPhrase->np_
    : NounListProd
    resolveNouns(resolver, results)
    {
        /* return the combination of the sublist and the noun phrase */
        return lst_.resolveNouns(resolver, results)
            + np_.resolveNouns(resolver, results);
    }
;


/* ------------------------------------------------------------------------ */
/*
 *   "Except" list.  This is a noun list that can contain anything that's
 *   in a regular noun list plus some things that only make sense as
 *   exceptions, such as possessive nouns (e.g., "mine").
 */

grammar exceptList(single): exceptNounPhrase->np_ : ExceptListProd
    resolveNouns(resolver, results)
    {
        return np_.resolveNouns(resolver, results);
    }
;

grammar exceptList(list):
    exceptNounPhrase->np_ nounConjunction exceptList->lst_
    : ExceptListProd
    resolveNouns(resolver, results)
    {
        /* return a list consisting of all of our objects */
        return np_.resolveNouns(resolver, results)
            + lst_.resolveNouns(resolver, results);
    }
;

/*
 *   An "except" noun phrase is a normal "complete" noun phrase or a
 *   possessive noun phrase that doesn't explicitly qualify another phrase
 *   (for example, "all the coins but bob's" - the "bob's" is just a
 *   possessive noun phrase without another noun phrase attached, since it
 *   implicitly qualifies "the coins").
 */
grammar exceptNounPhrase(singleComplete): completeNounPhraseWithoutAll->np_
    : ExceptListProd
    resolveNouns(resolver, results)
    {
        return np_.resolveNouns(resolver, results);
    }
;

grammar exceptNounPhrase(singlePossessive): possessiveNounPhrase->poss_
    : ButPossessiveProd
;


/* ------------------------------------------------------------------------ */
/*
 *   A single noun is sometimes required where, structurally, a list is not
 *   allowed.  Single nouns should not be used to prohibit lists where
 *   there is no structural reason for the prohibition - these should be
 *   used only where it doesn't make sense to use a list structurally.  
 */
grammar singleNoun(normal): singleNounOnly->np_ : LayeredNounPhraseProd
;

/*
 *   An empty single noun is one with no words at all.  This is matched
 *   when a command requires a noun list but the player doesn't include
 *   one; this construct has "badness" because we only want to match it
 *   when we have no choice.
 */
grammar singleNoun(empty): [badness 500] : EmptyNounPhraseProd
    /* use a nil responseProd, so that we get the phrasing from the action */
    responseProd = nil

    /* the fallback responseProd, if we can't get one from the action */
    fallbackResponseProd = singleNoun
;

/*
 *   A user could attempt to use a noun list with more than one entry (a
 *   "multi list") where a single noun is required.  This is not a
 *   grammatical error, so we accept it grammatically; however, for
 *   disambiguation purposes we score it lower than a singleNoun production
 *   with only one noun phrase, and if we try to resolve it, we'll fail
 *   with an error.  
 */
grammar singleNoun(multiple): nounMultiList->np_ : SingleNounWithListProd
;


/*
 *   A *structural* single noun phrase.  This production is for use where a
 *   single noun phrase (not a list of nouns) is required grammatically.
 */
grammar singleNounOnly(main):
    terminalNounPhrase->np_
    | completeNounPhrase->np_
    : SingleNounProd
;

/* ------------------------------------------------------------------------ */
/*
 *   Prepositionally modified single noun phrases.  These can be used in
 *   indirect object responses, so allow for interactions like this:
 *   
 *   >unlock door
 *.  What do you want to unlock it with?
 *   
 *   >with the key
 *   
 *   The entire notion of prepositionally qualified noun phrases in
 *   interactive indirect object responses is specific to English, so this
 *   is implemented in the English module only.  However, the general
 *   notion of specialized responses to interactive indirect object queries
 *   is handled in the language-independent library in some cases, in such
 *   a way that the language-specific library can customize the behavior -
 *   see TIAction.askIobjResponseProd.  
 */
class PrepSingleNounProd: SingleNounProd
    resolveNouns(resolver, results)
    {
        return np_.resolveNouns(resolver, results);
    }

    /* 
     *   If the response starts with a preposition, it's pretty clearly a
     *   response to the special query rather than a new command. 
     */
    isSpecialResponseMatch()
    {
        return (np_ != nil && np_.firstTokenIndex > 1);
    }
;

/*
 *   Same thing for a Topic phrase
 */
class PrepSingleTopicProd: TopicProd
    resolveNouns(resolver, results)
    {
        return np_.resolveNouns(resolver, results);
    }
;

grammar inSingleNoun(main):
     singleNoun->np_ | ('in' | 'into' | 'in' 'to') singleNoun->np_
    : PrepSingleNounProd
;

grammar forSingleNoun(main):
   singleNoun->np_ | 'for' singleNoun->np_ : PrepSingleNounProd
;

grammar toSingleNoun(main):
   singleNoun->np_ | 'to' singleNoun->np_ : PrepSingleNounProd
;

grammar throughSingleNoun(main):
   singleNoun->np_ | ('through' | 'thru') singleNoun->np_
   : PrepSingleNounProd
;

grammar fromSingleNoun(main):
   singleNoun->np_ | 'from' singleNoun->np_ : PrepSingleNounProd
;

grammar onSingleNoun(main):
   singleNoun->np_ | ('on' | 'onto' | 'on' 'to') singleNoun->np_
    : PrepSingleNounProd
;

grammar withSingleNoun(main):
   singleNoun->np_ | 'with' singleNoun->np_ : PrepSingleNounProd
;

grammar atSingleNoun(main):
   singleNoun->np_ | 'at' singleNoun->np_ : PrepSingleNounProd
;

grammar outOfSingleNoun(main):
   singleNoun->np_ | 'out' 'of' singleNoun->np_ : PrepSingleNounProd
;

grammar aboutTopicPhrase(main):
   topicPhrase->np_ | 'about' topicPhrase->np_
   : PrepSingleTopicProd
;

/* ------------------------------------------------------------------------ */
/*
 *   Complete noun phrase - this is a fully-qualified noun phrase that
 *   cannot be modified with articles, quantifiers, or anything else.  This
 *   is the highest-level individual noun phrase.  
 */

grammar completeNounPhrase(main):
    completeNounPhraseWithAll->np_ | completeNounPhraseWithoutAll->np_
    : LayeredNounPhraseProd
;

/*
 *   Slightly better than a purely miscellaneous word list is a pair of
 *   otherwise valid noun phrases connected by a preposition that's
 *   commonly used in command phrases.  This will match commands where the
 *   user has assumed a command with a prepositional structure that doesn't
 *   exist among the defined commands.  Since we have badness, we'll be
 *   ignored any time there's a valid command syntax with the same
 *   prepositional structure.
 */
grammar completeNounPhrase(miscPrep):
    [badness 100] completeNounPhrase->np1_
        ('with' | 'into' | 'in' 'to' | 'through' | 'thru' | 'for' | 'to'
         | 'onto' | 'on' 'to' | 'at' | 'under' | 'behind')
        completeNounPhrase->np2_
    : NounPhraseProd
    resolveNouns(resolver, results)
    {
        /* note that we have an invalid prepositional phrase structure */
        results.noteBadPrep();

        /* resolve the underlying noun phrases, for scoring purposes */
        np1_.resolveNouns(resolver, results);
        np2_.resolveNouns(resolver, results);

        /* return nothing */
        return [];
    }
;


/*
 *   A qualified noun phrase can, all by itself, be a full noun phrase
 */
grammar completeNounPhraseWithoutAll(qualified): qualifiedNounPhrase->np_
    : LayeredNounPhraseProd
;

/*
 *   Pronoun rules.  A pronoun is a complete noun phrase; it does not allow
 *   further qualification.  
 */
grammar completeNounPhraseWithoutAll(it):   'it' : ItProd;
grammar completeNounPhraseWithoutAll(them): 'them' : ThemProd;
grammar completeNounPhraseWithoutAll(him):  'him' : HimProd;
grammar completeNounPhraseWithoutAll(her):  'her' : HerProd;

/*
 *   Reflexive second-person pronoun, for things like "bob, look at
 *   yourself"
 */
grammar completeNounPhraseWithoutAll(yourself):
    'yourself' | 'yourselves' | 'you' : YouProd
;

/*
 *   Reflexive third-person pronouns.  We accept these in places such as
 *   the indirect object of a two-object verb.
 */
grammar completeNounPhraseWithoutAll(itself): 'itself' : ItselfProd
    /* check agreement of our binding */
    checkAgreement(lst)
    {
        /* the result is required to be singular and ungendered */
        return (lst.length() == 1 && lst[1].obj_.canMatchIt);
    }
;

grammar completeNounPhraseWithoutAll(themselves):
    'themself' | 'themselves' : ThemselvesProd

    /* check agreement of our binding */
    checkAgreement(lst)
    {
        /*
         *   For 'themselves', allow anything; we could balk at this
         *   matching a single object that isn't a mass noun, but that
         *   would be overly picky, and it would probably reject at least
         *   a few things that really ought to be acceptable.  Besides,
         *   'them' is the closest thing English has to a singular
         *   gender-neutral pronoun, and some people intentionally use it
         *   as such.
         */
        return true;
    }
;

grammar completeNounPhraseWithoutAll(himself): 'himself' : HimselfProd
    /* check agreement of our binding */
    checkAgreement(lst)
    {
        /* the result is required to be singular and masculine */
        return (lst.length() == 1 && lst[1].obj_.canMatchHim);
    }
;

grammar completeNounPhraseWithoutAll(herself): 'herself' : HerselfProd
    /* check agreement of our binding */
    checkAgreement(lst)
    {
        /* the result is required to be singular and feminine */
        return (lst.length() == 1 && lst[1].obj_.canMatchHer);
    }
;

/*
 *   First-person pronoun, for referring to the speaker: "bob, look at me"
 */
grammar completeNounPhraseWithoutAll(me): 'me' | 'myself' : MeProd;

/*
 *   "All" and "all but".
 *   
 *   "All" is a "complete" noun phrase, because there's nothing else needed
 *   to make it a noun phrase.  We make this a special kind of complete
 *   noun phrase because 'all' is not acceptable as a complete noun phrase
 *   in some contexts where any of the other complete noun phrases are
 *   acceptable.
 *   
 *   "All but" is a "terminal" noun phrase - this is a special kind of
 *   complete noun phrase that cannot be followed by another noun phrase
 *   with "and".  "All but" is terminal because we want any and's that
 *   follow it to be part of the exception list, so that we interpret "take
 *   all but a and b" as excluding a and b, not as excluding a but then
 *   including b as a separate list.  
 */
grammar completeNounPhraseWithAll(main):
    'all' | 'everything'
    : EverythingProd
;

grammar terminalNounPhrase(allBut):
    ('all' | 'everything') ('but' | 'except' | 'except' 'for')
        exceptList->except_
    : EverythingButProd
;

/*
 *   Plural phrase with an exclusion list.  This is a terminal noun phrase
 *   because it ends in an exclusion list.
 */
grammar terminalNounPhrase(pluralExcept):
    (qualifiedPluralNounPhrase->np_ | detPluralNounPhrase->np_)
    ('except' | 'except' 'for' | 'but' | 'but' 'not') exceptList->except_
    : ListButProd
;

/*
 *   Qualified singular with an exception
 */
grammar terminalNounPhrase(anyBut):
    'any' nounPhrase->np_
    ('but' | 'except' | 'except' 'for' | 'but' 'not') exceptList->except_
    : IndefiniteNounButProd
;

/* ------------------------------------------------------------------------ */
/*
 *   A qualified noun phrase is a noun phrase with an optional set of
 *   qualifiers: a definite or indefinite article, a quantifier, words such
 *   as 'any' and 'all', possessives, and locational specifiers ("the box
 *   on the table").
 *
 *   Without qualification, a definite article is implicit, so we read
 *   "take box" as equivalent to "take the box."
 *
 *   Grammar rule instantiations in language-specific modules should set
 *   property np_ to the underlying noun phrase match tree.
 */

/*
 *   A qualified noun phrase can be either singular or plural.  The number
 *   is a feature of the overall phrase; the phrase might consist of
 *   subphrases of different numbers (for example, "bob's coins" is plural
 *   even though it contains a singular subphrase, "bob"; and "one of the
 *   coins" is singular, even though its subphrase "coins" is plural).
 */
grammar qualifiedNounPhrase(main):
    qualifiedSingularNounPhrase->np_
    | qualifiedPluralNounPhrase->np_
    : LayeredNounPhraseProd
;

/* ------------------------------------------------------------------------ */
/*
 *   Singular qualified noun phrase.
 */

/*
 *   A singular qualified noun phrase with an implicit or explicit definite
 *   article.  If there is no article, a definite article is implied (we
 *   interpret "take box" as though it were "take the box").
 */
grammar qualifiedSingularNounPhrase(definite):
    ('the' | 'the' 'one' | 'the' '1' | ) indetSingularNounPhrase->np_
    : DefiniteNounProd
;

/*
 *   A singular qualified noun phrase with an explicit indefinite article.
 */
grammar qualifiedSingularNounPhrase(indefinite):
    ('a' | 'an') indetSingularNounPhrase->np_
    : IndefiniteNounProd
;

/*
 *   A singular qualified noun phrase with an explicit arbitrary
 *   determiner.
 */
grammar qualifiedSingularNounPhrase(arbitrary):
    ('any' | 'one' | '1' | 'any' ('one' | '1')) indetSingularNounPhrase->np_
    : ArbitraryNounProd
;

/*
 *   A singular qualified noun phrase with a possessive adjective.
 */
grammar qualifiedSingularNounPhrase(possessive):
    possessiveAdjPhrase->poss_ indetSingularNounPhrase->np_
    : PossessiveNounProd
;

/*
 *   A singular qualified noun phrase that arbitrarily selects from a
 *   plural set.  This is singular, even though the underlying noun phrase
 *   is plural, because we're explicitly selecting one item.
 */
grammar qualifiedSingularNounPhrase(anyPlural):
    'any' 'of' explicitDetPluralNounPhrase->np_
    : ArbitraryNounProd
;

/*
 *   A singular object specified only by its containment, with a definite
 *   article.
 */
grammar qualifiedSingularNounPhrase(theOneIn):
    'the' 'one' ('that' ('is' | 'was') | 'that' tokApostropheS | )
    ('in' | 'inside' | 'inside' 'of' | 'on' | 'from')
    completeNounPhraseWithoutAll->cont_
    : VagueContainerDefiniteNounPhraseProd

    /*
     *   our main phrase is simply 'one' (so disambiguation prompts will
     *   read "which one do you mean...")
     */
    mainPhraseText = 'one'
;

/*
 *   A singular object specified only by its containment, with an
 *   indefinite article.
 */
grammar qualifiedSingularNounPhrase(anyOneIn):
    ('anything' | 'one') ('that' ('is' | 'was') | 'that' tokApostropheS | )
    ('in' | 'inside' | 'inside' 'of' | 'on' | 'from')
    completeNounPhraseWithoutAll->cont_
    : VagueContainerIndefiniteNounPhraseProd
;

/* ------------------------------------------------------------------------ */
/*
 *   An "indeterminate" singular noun phrase is a noun phrase without any
 *   determiner.  A determiner is a phrase that specifies the phrase's
 *   number and indicates whether or not it refers to a specific object,
 *   and if so fixes which object it refers to; determiners include
 *   articles ("the", "a") and possessives.
 *
 *   Note that an indeterminate phrase is NOT necessarily an indefinite
 *   phrase.  In fact, in most cases, we assume a definite usage when the
 *   determiner is omitted: we take TAKE BOX as meaning TAKE THE BOX.  This
 *   is more or less the natural way an English speaker would interpret
 *   this ill-formed phrasing, but even more than that, it's the
 *   Adventurese convention, taking into account that most players enter
 *   commands telegraphically and are accustomed to noun phrases being
 *   definite by default.
 */

/* an indetermine noun phrase can be a simple noun phrase */
grammar indetSingularNounPhrase(basic):
    nounPhrase->np_
    : LayeredNounPhraseProd
;

/*
 *   An indetermine noun phrase can specify a location for the object(s).
 *   The location must be a singular noun phrase, but can itself be a fully
 *   qualified noun phrase (so it can have possessives, articles, and
 *   locational qualifiers of its own).
 *   
 *   Note that we take 'that are' even though the noun phrase is singular,
 *   because what we consider a singular noun phrase can have plural usage
 *   ("scissors", for example).  
 */
grammar indetSingularNounPhrase(locational):
    nounPhrase->np_
    ('that' ('is' | 'was')
     | 'that' tokApostropheS
     | 'that' ('are' | 'were')
     | )
    ('in' | 'inside' | 'inside' 'of' | 'on' | 'from')
    completeNounPhraseWithoutAll->cont_
    : ContainerNounPhraseProd
;

/* ------------------------------------------------------------------------ */
/*
 *   Plural qualified noun phrase.
 */

/*
 *   A simple unqualified plural phrase with determiner.  Since this form
 *   of plural phrase doesn't have any additional syntax that makes it an
 *   unambiguous plural, we can only accept an actual plural for the
 *   underlying phrase here - we can't accept an adjective phrase.
 */
grammar qualifiedPluralNounPhrase(determiner):
    ('any' | ) detPluralOnlyNounPhrase->np_
    : LayeredNounPhraseProd
;

/* plural phrase qualified with a number and optional "any" */
grammar qualifiedPluralNounPhrase(anyNum):
    ('any' | ) numberPhrase->quant_ indetPluralNounPhrase->np_
    | ('any' | ) numberPhrase->quant_ 'of' explicitDetPluralNounPhrase->np_
    : QuantifiedPluralProd
;

/* plural phrase qualified with a number and "all" */
grammar qualifiedPluralNounPhrase(allNum):
    'all' numberPhrase->quant_ indetPluralNounPhrase->np_
    | 'all' numberPhrase->quant_ 'of' explicitDetPluralNounPhrase->np_
    : ExactQuantifiedPluralProd
;

/* plural phrase qualified with "both" */
grammar qualifiedPluralNounPhrase(both):
    'both' detPluralNounPhrase->np_
    | 'both' 'of' explicitDetPluralNounPhrase->np_
    : BothPluralProd
;

/* plural phrase qualified with "all" */
grammar qualifiedPluralNounPhrase(all):
    'all' detPluralNounPhrase->np_
    | 'all' 'of' explicitDetPluralNounPhrase->np_
    : AllPluralProd
;

/* vague plural phrase with location specified */
grammar qualifiedPluralNounPhrase(theOnesIn):
    ('the' 'ones' ('that' ('are' | 'were') | )
     | ('everything' | 'all')
       ('that' ('is' | 'was') | 'that' tokApostropheS | ))
    ('in' | 'inside' | 'inside' 'of' | 'on' | 'from')
    completeNounPhraseWithoutAll->cont_
    : AllInContainerNounPhraseProd
;

/* ------------------------------------------------------------------------ */
/*
 *   A plural noun phrase with a determiner.  The determiner can be
 *   explicit (such as an article or possessive) or it can implied (the
 *   implied determiner is the definite article, so "take boxes" is
 *   understood as "take the boxes").
 */
grammar detPluralNounPhrase(main):
    indetPluralNounPhrase->np_ | explicitDetPluralNounPhrase->np_
    : LayeredNounPhraseProd
;

/* ------------------------------------------------------------------------ */
/*
 *   A determiner plural phrase with an explicit underlying plural (i.e.,
 *   excluding adjective phrases with no explicitly plural words).
 */
grammar detPluralOnlyNounPhrase(main):
    implicitDetPluralOnlyNounPhrase->np_
    | explicitDetPluralOnlyNounPhrase->np_
    : LayeredNounPhraseProd
;

/*
 *   An implicit determiner plural phrase is an indeterminate plural phrase
 *   without any extra determiner - i.e., the determiner is implicit.
 *   We'll treat this the same way we do a plural explicitly determined
 *   with a definite article, since this is the most natural interpretation
 *   in English.
 *
 *   (This might seem like a pointless extra layer in the grammar, but it's
 *   necessary for the resolution process to have a layer that explicitly
 *   declares the phrase to be determined, even though the determiner is
 *   implied in the structure.  This extra layer is important because it
 *   explicitly calls results.noteMatches(), which is needed for rankings
 *   and the like.)
 */
grammar implicitDetPluralOnlyNounPhrase(main):
    indetPluralOnlyNounPhrase->np_
    : DefinitePluralProd
;

/* ------------------------------------------------------------------------ */
/*
 *   A plural noun phrase with an explicit determiner.
 */

/* a plural noun phrase with a definite article */
grammar explicitDetPluralNounPhrase(definite):
    'the' indetPluralNounPhrase->np_
    : DefinitePluralProd
;

/* a plural noun phrase with a definite article and a number */
grammar explicitDetPluralNounPhrase(definiteNumber):
    'the' numberPhrase->quant_ indetPluralNounPhrase->np_
    : ExactQuantifiedPluralProd
;

/* a plural noun phrase with a possessive */
grammar explicitDetPluralNounPhrase(possessive):
    possessiveAdjPhrase->poss_ indetPluralNounPhrase->np_
    : PossessivePluralProd
;

/* a plural noun phrase with a possessive and a number */
grammar explicitDetPluralNounPhrase(possessiveNumber):
    possessiveAdjPhrase->poss_ numberPhrase->quant_
    indetPluralNounPhrase->np_
    : ExactQuantifiedPossessivePluralProd
;

/* ------------------------------------------------------------------------ */
/*
 *   A plural noun phrase with an explicit determiner and only an
 *   explicitly plural underlying phrase.
 */
grammar explicitDetPluralOnlyNounPhrase(definite):
    'the' indetPluralOnlyNounPhrase->np_
    : AllPluralProd
;

grammar explicitDetPluralOnlyNounPhrase(definiteNumber):
    'the' numberPhrase->quant_ indetPluralNounPhrase->np_
    : ExactQuantifiedPluralProd
;

grammar explicitDetPluralOnlyNounPhrase(possessive):
    possessiveAdjPhrase->poss_ indetPluralOnlyNounPhrase->np_
    : PossessivePluralProd
;

grammar explicitDetPluralOnlyNounPhrase(possessiveNumber):
    possessiveAdjPhrase->poss_ numberPhrase->quant_
    indetPluralNounPhrase->np_
    : ExactQuantifiedPossessivePluralProd
;


/* ------------------------------------------------------------------------ */
/*
 *   An indeterminate plural noun phrase.
 *
 *   For the basic indeterminate plural phrase, allow an adjective phrase
 *   anywhere a plural phrase is allowed; this makes possible the
 *   short-hand of omitting a plural word when the plural number is
 *   unambiguous from context.
 */

/* a simple plural noun phrase */
grammar indetPluralNounPhrase(basic):
    pluralPhrase->np_ | adjPhrase->np_
    : LayeredNounPhraseProd
;

/*
 *   A plural noun phrase with a locational qualifier.  Note that even
 *   though the overall phrase is plural (and so the main underlying noun
 *   phrase is plural), the location phrase itself must always be singular.
 */
grammar indetPluralNounPhrase(locational):
    (pluralPhrase->np_ | adjPhrase->np_) ('that' ('are' | 'were') | )
    ('in' | 'inside' | 'inside' 'of' | 'on' | 'from')
    completeNounPhraseWithoutAll->cont_
    : ContainerNounPhraseProd
;

/*
 *   An indetermine plural noun phrase with only explicit plural phrases.
 */
grammar indetPluralOnlyNounPhrase(basic):
    pluralPhrase->np_
    : LayeredNounPhraseProd
;

grammar indetPluralOnlyNounPhrase(locational):
    pluralPhrase->np_ ('that' ('are' | 'were') | )
    ('in' | 'inside' | 'inside' 'of' | 'on' | 'from')
    completeNounPhraseWithoutAll->cont_
    : ContainerNounPhraseProd
;

/* ------------------------------------------------------------------------ */
/*
 *   Noun Phrase.  This is the basic noun phrase, which serves as a
 *   building block for complete noun phrases.  This type of noun phrase
 *   can be qualified with articles, quantifiers, and possessives, and can
 *   be used to construct possessives via the addition of "'s" at the end
 *   of the phrase.
 *   
 *   In most cases, custom noun phrase rules should be added to this
 *   production, as long as qualification (with numbers, articles, and
 *   possessives) is allowed.  For a custom noun phrase rule that cannot be
 *   qualified, a completeNounPhrase rule should be added instead.  
 */
grammar nounPhrase(main): compoundNounPhrase->np_
    : LayeredNounPhraseProd
;

/*
 *   Plural phrase.  This is the basic plural phrase, and corresponds to
 *   the basic nounPhrase for plural forms.
 */
grammar pluralPhrase(main): compoundPluralPhrase->np_
    : LayeredNounPhraseProd
;

/* ------------------------------------------------------------------------ */
/*
 *   Compound noun phrase.  This is one or more noun phrases connected with
 *   'of', as in "piece of paper".  The part after the 'of' is another
 *   compound noun phrase.
 *   
 *   Note that this general rule does not allow the noun phrase after the
 *   'of' to be qualified with an article or number, except that we make an
 *   exception to allow a definite article.  Other cases ("a piece of four
 *   papers") do not generally make sense, so we won't attempt to support
 *   them; instead, games can add as special cases new nounPhrase rules for
 *   specific literal sequences where more complex grammar is necessary.  
 */
grammar compoundNounPhrase(simple): simpleNounPhrase->np_
    : NounPhraseWithVocab
    getVocabMatchList(resolver, results, extraFlags)
    {
        return np_.getVocabMatchList(resolver, results, extraFlags);
    }
    getAdjustedTokens()
    {
        return np_.getAdjustedTokens();
    }
;

grammar compoundNounPhrase(of):
    simpleNounPhrase->np1_ 'of'->of_ compoundNounPhrase->np2_
    | simpleNounPhrase->np1_ 'of'->of_ 'the'->the_ compoundNounPhrase->np2_
    : NounPhraseWithVocab
    getVocabMatchList(resolver, results, extraFlags)
    {
        local lst1;
        local lst2;

        /* resolve the two underlying lists */
        lst1 = np1_.getVocabMatchList(resolver, results, extraFlags);
        lst2 = np2_.getVocabMatchList(resolver, results, extraFlags);

        /*
         *   the result is the intersection of the two lists, since we
         *   want the list of objects with all of the underlying
         *   vocabulary words
         */
        return intersectNounLists(lst1, lst2);
    }
    getAdjustedTokens()
    {
        local ofLst;

        /* generate the 'of the' list from the original words */
        if (the_ == nil)
            ofLst = [of_, &miscWord];
        else
            ofLst = [of_, &miscWord, the_, &miscWord];

        /* return the full list */
        return np1_.getAdjustedTokens() + ofLst + np2_.getAdjustedTokens();
    }
;


/* ------------------------------------------------------------------------ */
/*
 *   Compound plural phrase - same as a compound noun phrase, but involving
 *   a plural part before the 'of'.  
 */

/*
 *   just a single plural phrase
 */
grammar compoundPluralPhrase(simple): simplePluralPhrase->np_
    : NounPhraseWithVocab
    getVocabMatchList(resolver, results, extraFlags)
    {
        return np_.getVocabMatchList(resolver, results, extraFlags);
    }
    getAdjustedTokens()
    {
        return np_.getAdjustedTokens();
    }
;

/*
 *   <plural-phrase> of <noun-phrase>
 */
grammar compoundPluralPhrase(of):
    simplePluralPhrase->np1_ 'of'->of_ compoundNounPhrase->np2_
    | simplePluralPhrase->np1_ 'of'->of_ 'the'->the_ compoundNounPhrase->np2_
    : NounPhraseWithVocab
    getVocabMatchList(resolver, results, extraFlags)
    {
        local lst1;
        local lst2;

        /* resolve the two underlying lists */
        lst1 = np1_.getVocabMatchList(resolver, results, extraFlags);
        lst2 = np2_.getVocabMatchList(resolver, results, extraFlags);

        /*
         *   the result is the intersection of the two lists, since we
         *   want the list of objects with all of the underlying
         *   vocabulary words
         */
        return intersectNounLists(lst1, lst2);
    }
    getAdjustedTokens()
    {
        local ofLst;

        /* generate the 'of the' list from the original words */
        if (the_ == nil)
            ofLst = [of_, &miscWord];
        else
            ofLst = [of_, &miscWord, the_, &miscWord];

        /* return the full list */
        return np1_.getAdjustedTokens() + ofLst + np2_.getAdjustedTokens();
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Simple noun phrase.  This is the most basic noun phrase, which is
 *   simply a noun, optionally preceded by one or more adjectives.
 */

/*
 *   just a noun
 */
grammar simpleNounPhrase(noun): nounWord->noun_ : NounPhraseWithVocab
    /* generate a list of my resolved objects */
    getVocabMatchList(resolver, results, extraFlags)
    {
        return noun_.getVocabMatchList(resolver, results, extraFlags);
    }
    getAdjustedTokens()
    {
        return noun_.getAdjustedTokens();
    }
;

/*
 *   <adjective> <simple-noun-phrase> (this allows any number of adjectives
 *   to be applied) 
 */
grammar simpleNounPhrase(adjNP): adjWord->adj_ simpleNounPhrase->np_
    : NounPhraseWithVocab

    /* generate a list of my resolved objects */
    getVocabMatchList(resolver, results, extraFlags)
    {
        /*
         *   return the list of objects in scope matching our adjective
         *   plus the list from the underlying noun phrase
         */
        return intersectNounLists(
            adj_.getVocabMatchList(resolver, results, extraFlags),
            np_.getVocabMatchList(resolver, results, extraFlags));
    }
    getAdjustedTokens()
    {
        return adj_.getAdjustedTokens() + np_.getAdjustedTokens();
    }
;

/*
 *   A simple noun phrase can also include a number or a quoted string
 *   before or after a noun.  A number can be spelled out or written with
 *   numerals; we consider both forms equivalent in meaning.
 *   
 *   A number in this type of usage is grammatically equivalent to an
 *   adjective - it's not meant to quantify the rest of the noun phrase,
 *   but rather is simply an adjective-like modifier.  For example, an
 *   elevator's control panel might have a set of numbered buttons which we
 *   want to refer to as "button 1," "button 2," and so on.  It is
 *   frequently the case that numeric adjectives are equally at home before
 *   or after their noun: "push 3 button" or "push button 3".  In addition,
 *   we accept a number by itself as a lone adjective, as in "push 3".  
 */

/*
 *   just a numeric/string adjective (for things like "push 3", "push #3",
 *   'push "G"')
 */
grammar simpleNounPhrase(number): literalAdjPhrase->adj_
    : NounPhraseWithVocab
    getVocabMatchList(resolver, results, extraFlags)
    {
        /*
         *   note that this counts as an adjective-ending phrase, since we
         *   don't have a noun involved
         */
        results.noteAdjEnding();

        /* pass through to the underlying literal adjective phrase */
        local lst = adj_.getVocabMatchList(resolver, results,
                                           extraFlags | EndsWithAdj);

        /* if in global scope, also try a noun interpretation */
        if (resolver.isGlobalScope)
            lst = adj_.addNounMatchList(lst, resolver, results, extraFlags);

        /* return the result */
        return lst;
    }
    getAdjustedTokens()
    {
        /* pass through to the underlying literal adjective phrase */
        return adj_.getAdjustedTokens();
    }
;

/*
 *   <literal-adjective> <noun> (for things like "board 44 bus" or 'push
 *   "G" button')
 */
grammar simpleNounPhrase(numberAndNoun):
    literalAdjPhrase->adj_ nounWord->noun_
    : NounPhraseWithVocab
    getVocabMatchList(resolver, results, extraFlags)
    {
        local nounList;
        local adjList;

        /* get the list of objects matching the rest of the noun phrase */
        nounList = noun_.getVocabMatchList(resolver, results, extraFlags);

        /* get the list of objects matching the literal adjective */
        adjList = adj_.getVocabMatchList(resolver, results, extraFlags);

        /* intersect the two lists and return the results */
        return intersectNounLists(nounList, adjList);
    }
    getAdjustedTokens()
    {
        return adj_.getAdjustedTokens() + noun_.getAdjustedTokens();
    }
;

/*
 *   <noun> <literal-adjective> (for things like "press button 3" or 'put
 *   tab "A" in slot "B"')
 */
grammar simpleNounPhrase(nounAndNumber):
    nounWord->noun_ literalAdjPhrase->adj_
    : NounPhraseWithVocab
    getVocabMatchList(resolver, results, extraFlags)
    {
        local nounList;
        local adjList;

        /* get the list of objects matching the rest of the noun phrase */
        nounList = noun_.getVocabMatchList(resolver, results, extraFlags);

        /* get the literal adjective matches */
        adjList = adj_.getVocabMatchList(resolver, results, extraFlags);

        /* intersect the two lists and return the results */
        return intersectNounLists(nounList, adjList);
    }
    getAdjustedTokens()
    {
        return noun_.getAdjustedTokens() + adj_.getAdjustedTokens();
    }
;

/*
 *   A simple noun phrase can also end in an adjective, which allows
 *   players to refer to objects using only their unique adjectives rather
 *   than their full names, which is sometimes more convenient: just "take
 *   gold" rather than "take gold key."
 *   
 *   When a particular phrase can be interpreted as either ending in an
 *   adjective or ending in a noun, we will always take the noun-ending
 *   interpretation - in such cases, the adjective-ending interpretation is
 *   probably a weak binding.  For example, "take pizza" almost certainly
 *   refers to the pizza itself when "pizza" and "pizza box" are both
 *   present, but it can also refer just to the box when no pizza is
 *   present.
 *   
 *   Equivalent to a noun phrase ending in an adjective is a noun phrase
 *   ending with an adjective followed by "one," as in "the red one."  
 */
grammar simpleNounPhrase(adj): adjWord->adj_ : NounPhraseWithVocab
    /* generate a list of my resolved objects */
    getVocabMatchList(resolver, results, extraFlags)
    {
        /* note in the results that we end in an adjective */
        results.noteAdjEnding();

        /* generate a list of objects matching the adjective */
        local lst = adj_.getVocabMatchList(
            resolver, results, extraFlags | EndsWithAdj);

        /* if in global scope, also try a noun interpretation */
        if (resolver.isGlobalScope)
            lst = adj_.addNounMatchList(lst, resolver, results, extraFlags);

        /* return the result */
        return lst;
    }
    getAdjustedTokens()
    {
        /* return the adjusted token list for the adjective */
        return adj_.getAdjustedTokens();
    }
;

grammar simpleNounPhrase(adjAndOne): adjective->adj_ 'one'
    : NounPhraseWithVocab
    /* generate a list of my resolved objects */
    getVocabMatchList(resolver, results, extraFlags)
    {
        /*
         *   This isn't exactly an adjective ending, but consider it as
         *   such anyway, since we're not matching 'one' to a vocabulary
         *   word - we're just using it as a grammatical marker that we're
         *   not providing a real noun.  If there's another match for
         *   which 'one' is a noun, that one is definitely preferred to
         *   this one; the adj-ending marking will ensure that we choose
         *   the other one.
         */
        results.noteAdjEnding();

        /* generate a list of objects matching the adjective */
        return getWordMatches(adj_, &adjective, resolver,
                              extraFlags | EndsWithAdj, VocabTruncated);
    }
    getAdjustedTokens()
    {
        return [adj_, &adjective];
    }
;

/*
 *   In the worst case, a simple noun phrase can be constructed from
 *   arbitrary words that don't appear in our dictionary.
 */
grammar simpleNounPhrase(misc):
    [badness 200] miscWordList->lst_ : NounPhraseWithVocab
    getVocabMatchList(resolver, results, extraFlags)
    {
        /* get the match list from the underlying list */
        local lst = lst_.getVocabMatchList(resolver, results, extraFlags);

        /*
         *   If there are no matches, note in the results that we have an
         *   arbitrary word list.  Note that we do this only if there are
         *   no matches, because we might match non-dictionary words to an
         *   object with a wildcard in its vocabulary words, in which case
         *   this is a valid, matching phrase after all.
         */
        if (lst == nil || lst.length() == 0)
            results.noteMiscWordList(lst_.getOrigText());

        /* return the match list */
        return lst;
    }
    getAdjustedTokens()
    {
        return lst_.getAdjustedTokens();
    }
;

/*
 *   If the command has qualifiers but omits everything else, we can have
 *   an empty simple noun phrase.
 */
grammar simpleNounPhrase(empty): [badness 600] : NounPhraseWithVocab
    getVocabMatchList(resolver, results, extraFlags)
    {
        /* we have an empty noun phrase */
        return results.emptyNounPhrase(resolver);
    }
    getAdjustedTokens()
    {
        return [];
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   An AdjPhraseWithVocab is an English-specific subclass of
 *   NounPhraseWithVocab, specifically for noun phrases that contain
 *   entirely adjectives.  
 */
class AdjPhraseWithVocab: NounPhraseWithVocab
    /* the property for the adjective literal - this is usually adj_ */
    adjVocabProp = &adj_

    /* 
     *   Add the vocabulary matches that we'd get if we were treating our
     *   adjective as a noun.  This combines the noun interpretation with a
     *   list of matches we got for the adjective version.  
     */
    addNounMatchList(lst, resolver, results, extraFlags)
    {
        /* get the word matches with a noun interpretation of our adjective */
        local nLst = getWordMatches(
            self.(adjVocabProp), &noun, resolver, extraFlags, VocabTruncated);

        /* combine the lists and return the result */
        return combineWordMatches(lst, nLst);
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   A "literal adjective" phrase is a number or string used as an
 *   adjective.
 */
grammar literalAdjPhrase(number):
    numberPhrase->num_ | poundNumberPhrase->num_
    : AdjPhraseWithVocab

    adj_ = (num_.getStrVal())
    getVocabMatchList(resolver, results, extraFlags)
    {
        local numList;

        /*
         *   get the list of objects matching the numeral form of the
         *   number as an adjective
         */
        numList = getWordMatches(num_.getStrVal(), &adjective,
                                 resolver, extraFlags, VocabTruncated);

        /* add the list of objects matching the special '#' wildcard */
        numList += getWordMatches('#', &adjective, resolver,
                                  extraFlags, VocabTruncated);

        /* return the combined lists */
        return numList;
    }
    getAdjustedTokens()
    {
        return [num_.getStrVal(), &adjective];
    }
;

grammar literalAdjPhrase(string): quotedStringPhrase->str_
    : AdjPhraseWithVocab

    adj_ = (str_.getStringText().toLower())
    getVocabMatchList(resolver, results, extraFlags)
    {
        local strList;
        local wLst;

        /*
         *   get the list of objects matching the string with the quotes
         *   removed
         */
        strList = getWordMatches(str_.getStringText().toLower(),
                                 &literalAdjective,
                                 resolver, extraFlags, VocabTruncated);

        /* add the list of objects matching the literal-adjective wildcard */
        wLst = getWordMatches('\u0001', &literalAdjective, resolver,
                              extraFlags, VocabTruncated);
        strList = combineWordMatches(strList, wLst);

        /* return the combined lists */
        return strList;
    }
    getAdjustedTokens()
    {
        return [str_.getStringText().toLower(), &adjective];
    }
;

/*
 *   In many cases, we might want to write what is semantically a literal
 *   string qualifier without the quotes.  For example, we might want to
 *   refer to an elevator button that's labeled "G" as simply "button G",
 *   without any quotes around the "G".  To accommodate these cases, we
 *   provide the literalAdjective part-of-speech.  We'll match these parts
 *   of speech the same way we'd match them if they were quoted.
 */
grammar literalAdjPhrase(literalAdj): literalAdjective->adj_
    : AdjPhraseWithVocab
    getVocabMatchList(resolver, results, extraFlags)
    {
        local lst;

        /* get a list of objects in scope matching our literal adjective */
        lst = getWordMatches(adj_, &literalAdjective, resolver,
                             extraFlags, VocabTruncated);

        /* if the scope is global, also include ordinary adjective matches */
        if (resolver.isGlobalScope)
        {
            /* get the ordinary adjective bindings */
            local aLst = getWordMatches(adj_, &adjective, resolver,
                                        extraFlags, VocabTruncated);

            /* global scope - combine the lists */
            lst = combineWordMatches(lst, aLst);
        }

        /* return the result */
        return lst;
    }
    getAdjustedTokens()
    {
        return [adj_, &literalAdjective];
    }
;


/* ------------------------------------------------------------------------ */
/*
 *   A noun word.  This can be either a simple 'noun' vocabulary word, or
 *   it can be an abbreviated noun with a trailing abbreviation period.
 */
class NounWordProd: NounPhraseWithVocab
    getVocabMatchList(resolver, results, extraFlags)
    {
        local w;
        local nLst;

        /* get our word text */
        w = getNounText();

        /* get the list of matches as nouns */
        nLst = getWordMatches(w, &noun, resolver, extraFlags, VocabTruncated);

        /*
         *   If the resolver indicates that we're in a "global" scope,
         *   *also* include any additional matches as adjectives.
         *
         *   Normally, when we're operating in limited, local scopes, we
         *   use the structure of the phrasing to determine whether to
         *   match a noun or adjective; if we have a match for a given word
         *   as a noun, we'll treat it only as a noun.  This allows us to
         *   take PIZZA to refer to the pizza (for which 'pizza' is defined
         *   as a noun) rather than to the PIZZA BOX (for which 'pizza' is
         *   a mere adjective) when both are in scope.  It's obvious which
         *   the player means in such cases, so we can be smart about
         *   choosing the stronger match.
         *
         *   In cases of global scope, though, it's much harder to guess
         *   about the player's intentions.  When the player types PIZZA,
         *   they might be thinking of the box even though there's a pizza
         *   somewhere else in the game.  Since the two objects might be in
         *   entirely different locations, both out of view, we can't
         *   assume that one or the other is more likely on the basis of
         *   which is closer to the player's senses.  So, it's better to
         *   allow both to match for now, and decide later, based on the
         *   context of the command, which was actually meant.
         */
        if (resolver.isGlobalScope)
        {
            /* get the list of matching adjectives */
            local aLst = getWordMatches(w, &adjective, resolver,
                                        extraFlags, VocabTruncated);

            /* combine it with the noun list */
            nLst = combineWordMatches(nLst, aLst);
        }

        /* return the match list */
        return nLst;
    }
    getAdjustedTokens()
    {
        /* the noun includes the period as part of the literal text */
        return [getNounText(), &noun];
    }

    /* the actual text of the noun to match to the dictionary */
    getNounText() { return noun_; }
;

grammar nounWord(noun): noun->noun_ : NounWordProd
;

grammar nounWord(nounAbbr): noun->noun_ tokAbbrPeriod->period_
    : NounWordProd

    /*
     *   for dictionary matching purposes, include the text of our noun
     *   with the period attached - the period is part of the dictionary
     *   entry for an abbreviated word
     */
    getNounText() { return noun_ + period_; }
;

/* ------------------------------------------------------------------------ */
/*
 *   An adjective word.  This can be either a simple 'adjective' vocabulary
 *   word, or it can be an 'adjApostS' vocabulary word plus a 's token.  
 */
grammar adjWord(adj): adjective->adj_ : AdjPhraseWithVocab
    /* generate a list of resolved objects */
    getVocabMatchList(resolver, results, extraFlags)
    {
        /* return a list of objects in scope matching our adjective */
        return getWordMatches(adj_, &adjective, resolver,
                              extraFlags, VocabTruncated);
    }
    getAdjustedTokens()
    {
        return [adj_, &adjective];
    }
;

grammar adjWord(adjApostS): adjApostS->adj_ tokApostropheS->apost_
    : AdjPhraseWithVocab
    /* generate a list of resolved objects */
    getVocabMatchList(resolver, results, extraFlags)
    {
        /* return a list of objects in scope matching our adjective */
        return getWordMatches(adj_, &adjApostS, resolver,
                              extraFlags, VocabTruncated);
    }
    getAdjustedTokens()
    {
        return [adj_, &adjApostS];
    }
;

grammar adjWord(adjAbbr): adjective->adj_ tokAbbrPeriod->period_
    : AdjPhraseWithVocab
    getVocabMatchList(resolver, results, extraFlags)
    {
        /*
         *   return the list matching our adjective *with* the period
         *   attached; the period is part of the dictionary entry for an
         *   abbreviated word
         */
        return getWordMatches(adj_ + period_, &adjective, resolver,
                              extraFlags, VocabTruncated);
    }
    getAdjustedTokens()
    {
        /* the adjective includes the period as part of the literal text */
        return [adj_ + period_, &adjective];
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Possessive phrase.  This is a noun phrase expressing ownership of
 *   another object.
 *   
 *   Note that all possessive phrases that can possibly be ambiguous must
 *   define getOrigMainText() to return the "main noun phrase" text.  In
 *   English, this means that we must omit any "'s" suffix.  This is needed
 *   only when the phrase can be ambiguous, so pronouns don't need it since
 *   they are inherently unambiguous.  
 */
grammar possessiveAdjPhrase(its): 'its' : ItsAdjProd
    /* we only agree with a singular ungendered noun */
    checkAnaphorAgreement(lst)
        { return lst.length() == 1 && lst[1].obj_.canMatchIt; }
;
grammar possessiveAdjPhrase(his): 'his' : HisAdjProd
    /* we only agree with a singular masculine noun */
    checkAnaphorAgreement(lst)
        { return lst.length() == 1 && lst[1].obj_.canMatchHim; }
;
grammar possessiveAdjPhrase(her): 'her' : HerAdjProd
    /* we only agree with a singular feminine noun */
    checkAnaphorAgreement(lst)
        { return lst.length() == 1 && lst[1].obj_.canMatchHer; }
;
grammar possessiveAdjPhrase(their): 'their' : TheirAdjProd
    /* we only agree with a single noun that has plural usage */
    checkAnaphorAgreement(lst)
        { return lst.length() == 1 && lst[1].obj_.isPlural; }
;
grammar possessiveAdjPhrase(your): 'your' : YourAdjProd
    /* we are non-anaphoric */
    checkAnaphorAgreement(lst) { return nil; }
;
grammar possessiveAdjPhrase(my): 'my' : MyAdjProd
    /* we are non-anaphoric */
    checkAnaphorAgreement(lst) { return nil; }
;

grammar possessiveAdjPhrase(npApostropheS):
    ('the' | ) nounPhrase->np_ tokApostropheS->apost_ : LayeredNounPhraseProd

    /* get the original text without the "'s" suffix */
    getOrigMainText()
    {
        /* return just the basic noun phrase part */
        return np_.getOrigText();
    }
;

grammar possessiveAdjPhrase(ppApostropheS):
    ('the' | ) pluralPhrase->np_
       (tokApostropheS->apost_ | tokPluralApostrophe->apost_)
    : LayeredNounPhraseProd

    /* get the original text without the "'s" suffix */
    getOrigMainText()
    {
        /* return just the basic noun phrase part */
        return np_.getOrigText();
    }

    resolveNouns(resolver, results)
    {
        /* note that we have a plural phrase, structurally speaking */
        results.notePlural();

        /* inherit the default handling */
        return inherited(resolver, results);
    }

    /* the possessive phrase is plural */
    isPluralPossessive = true
;

/*
 *   Possessive noun phrases.  These are similar to possessive phrases, but
 *   are stand-alone phrases that can act as nouns rather than as
 *   qualifiers for other noun phrases.  For example, for a first-person
 *   player character, "mine" would be a possessive noun phrase referring
 *   to an object owned by the player character.
 *   
 *   Note that many of the words used for possessive nouns are the same as
 *   for possessive adjectives - for example "his" is the same in either
 *   case, as are "'s" words.  However, we make the distinction internally
 *   because the two have different grammatical uses, and some of the words
 *   do differ ("her" vs "hers", for example).  
 */
grammar possessiveNounPhrase(its): 'its': ItsNounProd;
grammar possessiveNounPhrase(his): 'his': HisNounProd;
grammar possessiveNounPhrase(hers): 'hers': HersNounProd;
grammar possessiveNounPhrase(theirs): 'theirs': TheirsNounProd;
grammar possessiveNounPhrase(yours): 'yours' : YoursNounProd;
grammar possessiveNounPhrase(mine): 'mine' : MineNounProd;

grammar possessiveNounPhrase(npApostropheS):
    ('the' | )
    (nounPhrase->np_ tokApostropheS->apost_
     | pluralPhrase->np (tokApostropheS->apost_ | tokPluralApostrophe->apost_))
    : LayeredNounPhraseProd

    /* get the original text without the "'s" suffix */
    getOrigMainText()
    {
        /* return just the basic noun phrase part */
        return np_.getOrigText();
    }
;


/* ------------------------------------------------------------------------ */
/*
 *   Simple plural phrase.  This is the most basic plural phrase, which is
 *   simply a plural noun, optionally preceded by one or more adjectives.
 *   
 *   (English doesn't have any sort of adjective declension in number, so
 *   there's no need to distinguish between plural and singular adjectives;
 *   this equivalent rule in languages with adjective-noun agreement in
 *   number would use plural adjectives here as well as plural nouns.)  
 */
grammar simplePluralPhrase(plural): plural->plural_ : NounPhraseWithVocab
    /* generate a list of my resolved objects */
    getVocabMatchList(resolver, results, extraFlags)
    {
        local lst;

        /* get the list of matching plurals */
        lst = getWordMatches(plural_, &plural, resolver,
                             extraFlags, PluralTruncated);

        /* get the list of matching 'noun' definitions */
        local nLst = getWordMatches(plural_, &noun, resolver,
                                    extraFlags, VocabTruncated);

        /* get the combined list */
        local comboLst = combineWordMatches(lst, nLst);

        /*
         *   If we're in global scope, add in the matches for just plain
         *   'noun' properties as well.  This is important because we'll
         *   sometimes want to define a word that's actually a plural
         *   usage (in terms of the real-world English) under the 'noun'
         *   property.  This occurs particularly when a single game-world
         *   object represents a multiplicity of real-world objects.  When
         *   the scope is global, it's hard to anticipate all of the
         *   possible interactions with vocabulary along these lines, so
         *   it's easiest just to include the 'noun' matches.
         */
        if (resolver.isGlobalScope)
        {
            /* keep the combined list */
            lst = comboLst;
        }
        else if (comboLst.length() > lst.length())
        {
            /*
             *   ordinary scope, so don't include the noun matches; but
             *   since we found extra items to add, at least mark the
             *   plural matches as potentially ambiguous
             */
            lst.forEach({x: x.flags_ |= UnclearDisambig});
        }

        /* return the result list */
        return lst;
    }
    getAdjustedTokens()
    {
        return [plural_, &plural];
    }
;

grammar simplePluralPhrase(adj): adjWord->adj_ simplePluralPhrase->np_ :
    NounPhraseWithVocab

    /* resolve my object list */
    getVocabMatchList(resolver, results, extraFlags)
    {
        /*
         *   return the list of objects in scope matching our adjective
         *   plus the list from the underlying noun phrase
         */
        return intersectNounLists(
            adj_.getVocabMatchList(resolver, results, extraFlags),
            np_.getVocabMatchList(resolver, results, extraFlags));
    }
    getAdjustedTokens()
    {
        return adj_.getAdjustedTokens() + np_.getAdjustedTokens();
    }
;

grammar simplePluralPhrase(poundNum):
    poundNumberPhrase->num_ simplePluralPhrase->np_
    : NounPhraseWithVocab

    /* resolve my object list */
    getVocabMatchList(resolver, results, extraFlags)
    {
        local baseList;
        local numList;

        /* get the base list for the rest of the phrase */
        baseList = np_.getVocabMatchList(resolver, results, extraFlags);

        /* get the numeric matches, including numeric wildcards */
        numList = getWordMatches(num_.getStrVal(), &adjective,
                                 resolver, extraFlags, VocabTruncated)
                  + getWordMatches('#', &adjective,
                                   resolver, extraFlags, VocabTruncated);

        /* return the intersection of the lists */
        return intersectNounLists(numList, baseList);
    }
    getAdjustedTokens()
    {
        return [num_.getStrVal(), &adjective] + np_.getAdjustedTokens();
    }
;

/*
 *   A simple plural phrase can end with an adjective and "ones," as in
 *   "the red ones."
 */
grammar simplePluralPhrase(adjAndOnes): adjective->adj_ 'ones'
    : NounPhraseWithVocab
    getVocabMatchList(resolver, results, extraFlags)
    {
        /* generate a list of objects matching the adjective */
        return getWordMatches(adj_, &adjective, resolver,
                              extraFlags | EndsWithAdj, VocabTruncated);
    }
    getAdjustedTokens()
    {
        return [adj_, &adjective];
    }
;

/*
 *   If the command has qualifiers that require a plural, but omits
 *   everything else, we can have an empty simple noun phrase.
 */
grammar simplePluralPhrase(empty): [badness 600] : NounPhraseWithVocab
    getVocabMatchList(resolver, results, extraFlags)
    {
        /* we have an empty noun phrase */
        return results.emptyNounPhrase(resolver);
    }
    getAdjustedTokens()
    {
        return [];
    }
;

/*
 *   A simple plural phrase can match unknown words as a last resort.
 */
grammar simplePluralPhrase(misc):
    [badness 300] miscWordList->lst_ : NounPhraseWithVocab
    getVocabMatchList(resolver, results, extraFlags)
    {
        /* get the match list from the underlying list */
        local lst = lst_.getVocabMatchList(resolver, results, extraFlags);

        /*
         *   if there are no matches, note in the results that we have an
         *   arbitrary word list that doesn't correspond to any object
         */
        if (lst == nil || lst.length() == 0)
            results.noteMiscWordList(lst_.getOrigText());

        /* return the vocabulary match list */
        return lst;
    }
    getAdjustedTokens()
    {
        return lst_.getAdjustedTokens();
    }
;


/* ------------------------------------------------------------------------ */
/*
 *   An "adjective phrase" is a phrase made entirely of adjectives.
 */
grammar adjPhrase(adj): adjective->adj_ : AdjPhraseWithVocab
    getVocabMatchList(resolver, results, extraFlags)
    {
        /* note the adjective ending */
        results.noteAdjEnding();

        /* return the match list */
        local lst = getWordMatches(adj_, &adjective, resolver,
                                   extraFlags | EndsWithAdj, VocabTruncated);

        /* if in global scope, also try a noun interpretation */
        if (resolver.isGlobalScope)
            lst = addNounMatchList(lst, resolver, results, extraFlags);

        /* return the result */
        return lst;
    }

    getAdjustedTokens()
    {
        return [adj_, &adjective];
    }
;

grammar adjPhrase(adjAdj): adjective->adj_ adjPhrase->ap_
    : NounPhraseWithVocab
    /* generate a list of my resolved objects */
    getVocabMatchList(resolver, results, extraFlags)
    {
        /*
         *   return the list of objects in scope matching our adjective
         *   plus the list from the underlying adjective phrase
         */
        return intersectWordMatches(
            adj_, &adjective, resolver, extraFlags, VocabTruncated,
            ap_.getVocabMatchList(resolver, results, extraFlags));
    }
    getAdjustedTokens()
    {
        return [adj_, &adjective] + ap_.getAdjustedTokens();
    }
;


/* ------------------------------------------------------------------------ */
/*
 *   A "topic" is a special type of noun phrase used in commands like "ask
 *   <actor> about <topic>."  We define a topic as simply an ordinary
 *   single-noun phrase.  We distinguish this in the grammar to allow games
 *   to add special syntax for these.  
 */
grammar topicPhrase(main): singleNoun->np_ : TopicProd
;

/*
 *   Explicitly match a miscellaneous word list as a topic.
 *
 *   This might seem redundant with the ordinary topicPhrase that accepts a
 *   singleNoun, because singleNoun can match a miscellaneous word list.
 *   The difference is that singleNoun only matches a miscWordList with a
 *   "badness" value, whereas we match a miscWordList here without any
 *   badness.  We want to be more tolerant of unrecognized input in topic
 *   phrases than in ordinary noun phrases, because it's in the nature of
 *   topic phrases to go outside of what's implemented directly in the
 *   simulation model.  At a grammatical level, we don't want to treat
 *   topic phrases that we can resolve to the simulation model any
 *   differently than we treat those we can't resolve, so we must add this
 *   rule to eliminate the badness that singleNoun associated with a
 *   miscWordList match.
 *
 *   Note that we do prefer resolvable noun phrase matches to miscWordList
 *   matches, but we handle this preference with the resolver's scoring
 *   mechanism rather than with badness.
 */
grammar topicPhrase(misc): miscWordList->np_ : TopicProd
   resolveNouns(resolver, results)
   {
       /* note in the results that we have an arbitrary word list */
       results.noteMiscWordList(np_.getOrigText());

       /* inherit the default TopicProd behavior */
       return inherited(resolver, results);
   }
;

/* ------------------------------------------------------------------------ */
/*
 *   A "quoted string" phrase is a literal enclosed in single or double
 *   quotes.
 *
 *   Note that this is a separate production from literalPhrase.  This
 *   production can be used when *only* a quoted string is allowed.  The
 *   literalPhrase production allows both quoted and unquoted text.
 */
grammar quotedStringPhrase(main): tokString->str_ : LiteralProd
    /*
     *   get my string, with the quotes trimmed off (so we return simply
     *   the contents of the string)
     */
    getStringText() { return stripQuotesFrom(str_); }
;

/*
 *   Service routine: strip quotes from a *possibly* quoted string.  If the
 *   string starts with a quote, we'll remove the open quote.  If it starts
 *   with a quote and it ends with a corresponding close quote, we'll
 *   remove that as well.  
 */
stripQuotesFrom(str)
{
    local hasOpen;
    local hasClose;

    /* presume we won't find open or close quotes */
    hasOpen = hasClose = nil;

    /*
     *   Check for quotes.  We'll accept regular ASCII "straight" single
     *   or double quotes, as well as Latin-1 curly single or double
     *   quotes.  The curly quotes must be used in their normal
     */
    if (str.startsWith('\'') || str.startsWith('"'))
    {
        /* single or double quote - check for a matching close quote */
        hasOpen = true;
        hasClose = (str.length() > 2 && str.endsWith(str.substr(1, 1)));
    }
    else if (str.startsWith('`'))
    {
        /* single in-slanted quote - check for either type of close */
        hasOpen = true;
        hasClose = (str.length() > 2
                    && (str.endsWith('`') || str.endsWith('\'')));
    }
    else if (str.startsWith('\u201C'))
    {
        /* it's a curly double quote */
        hasOpen = true;
        hasClose = str.endsWith('\u201D');
    }
    else if (str.startsWith('\u2018'))
    {
        /* it's a curly single quote */
        hasOpen = true;
        hasClose = str.endsWith('\u2019');
    }

    /* trim off the quotes */
    if (hasOpen)
    {
        if (hasClose)
            str = str.substr(2, str.length() - 2);
        else
            str = str.substr(2);
    }

    /* return the modified text */
    return str;
}

/* ------------------------------------------------------------------------ */
/*
 *   A "literal" is essentially any phrase.  This can include a quoted
 *   string, a number, or any set of word tokens.
 */
grammar literalPhrase(string): quotedStringPhrase->str_ : LiteralProd
    getLiteralText(results, action, which)
    {
        /* get the text from our underlying quoted string */
        return str_.getStringText();
    }

    getTentativeLiteralText()
    {
        /*
         *   our result will never change, so our tentative text is the
         *   same as our regular literal text
         */
        return str_.getStringText();
    }

    resolveLiteral(results)
    {
        /* flag the literal text */
        results.noteLiteral(str_.getOrigText());
    }
;

grammar literalPhrase(miscList): miscWordList->misc_ : LiteralProd
    getLiteralText(results, action, which)
    {
        /* get my original text */
        local txt = misc_.getOrigText();

        /*
         *   if our underlying miscWordList has only one token, strip
         *   quotes, in case that token is a quoted string token
         */
        if (misc_.getOrigTokenList().length() == 1)
            txt = stripQuotesFrom(txt);

        /* return the text */
        return txt;
    }

    getTentativeLiteralText()
    {
        /* our regular text is permanent, so simply use it now */
        return misc_.getOrigText();
    }

    resolveLiteral(results)
    {
        /*
         *   note the length of our literal phrase - when we have a choice
         *   of interpretations, we prefer to choose shorter literal
         *   phrases, since this means that we'll have more of our tokens
         *   being fully interpreted rather than bunched into an
         *   uninterpreted literal
         */
        results.noteLiteral(misc_.getOrigText());
    }
;

/*
 *   In case we have a verb grammar rule that calls for a literal phrase,
 *   but the player enters a command with nothing in that slot, match an
 *   empty token list as a last resort.  Since this phrasing has a badness,
 *   we won't match it unless we don't have any better structural match.
 */
grammar literalPhrase(empty): [badness 400]: EmptyLiteralPhraseProd
    resolveLiteral(results) { }
;

/* ------------------------------------------------------------------------ */
/*
 *   An miscellaneous word list is a list of one or more words of any kind:
 *   any word, any integer, or any apostrophe-S token will do.  Note that
 *   known and unknown words can be mixed in an unknown word list; we care
 *   only that the list is made up of tokWord, tokInt, tokApostropheS,
 *   and/or abbreviation-period tokens.
 *
 *   Note that this kind of phrase is often used with a 'badness' value.
 *   However, we don't assign any badness here, because a miscellaneous
 *   word list might be perfectly valid in some contexts; instead, any
 *   productions that include a misc word list should specify badness as
 *   desired.
 */
grammar miscWordList(wordOrNumber):
    tokWord->txt_ | tokInt->txt_ | tokApostropheS->txt_
    | tokPluralApostrophe->txt_
    | tokPoundInt->txt_ | tokString->txt_ | tokAbbrPeriod->txt_
    : NounPhraseWithVocab
    getVocabMatchList(resolver, results, extraFlags)
    {
        /* we don't match anything directly with our vocabulary */
        return [];
    }
    getAdjustedTokens()
    {
        /* our token type is the special miscellaneous word type */
        return [txt_, &miscWord];
    }
;

grammar miscWordList(list):
    (tokWord->txt_ | tokInt->txt_ | tokApostropheS->txt_
     | tokPluralApostrophe->txt_ | tokAbbrPeriod->txt_
     | tokPoundInt->txt_ | tokString->txt_) miscWordList->lst_
    : NounPhraseWithVocab
    getVocabMatchList(resolver, results, extraFlags)
    {
        /* we don't match anything directly with our vocabulary */
        return [];
    }
    getAdjustedTokens()
    {
        /* our token type is the special miscellaneous word type */
        return [txt_, &miscWord] + lst_.getAdjustedTokens();
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   A main disambiguation phrase consists of a disambiguation phrase,
 *   optionally terminated with a period.
 */
grammar mainDisambigPhrase(main):
    disambigPhrase->dp_
    | disambigPhrase->dp_ '.'
    : BasicProd
    resolveNouns(resolver, results)
    {
        return dp_.resolveNouns(resolver, results);
    }
    getResponseList() { return dp_.getResponseList(); }
;

/*
 *   A "disambiguation phrase" is a phrase that answers a disambiguation
 *   question ("which book do you mean...").
 *   
 *   A disambiguation question can be answered with several types of
 *   syntax:
 *   
 *.  all/everything/all of them
 *.  both/both of them
 *.  any/any of them
 *.  <disambig list>
 *.  the <ordinal list> ones
 *.  the former/the latter
 *   
 *   Note that we assign non-zero badness to all of the ordinal
 *   interpretations, so that we will take an actual vocabulary
 *   interpretation instead of an ordinal interpretation whenever possible.
 *   For example, if an object's name is actually "the third button," this
 *   will give us greater affinity for using "third" as an adjective than
 *   as an ordinal in our own list.  
 */
grammar disambigPhrase(all):
    'all' | 'everything' | 'all' 'of' 'them' : DisambigProd
    resolveNouns(resolver, results)
    {
        /* they want everything we proposed - return the whole list */
        return removeAmbigFlags(resolver.getAll(self));
    }

    /* there's only me in the response list */
    getResponseList() { return [self]; }
;

grammar disambigPhrase(both): 'both' | 'both' 'of' 'them' : DisambigProd
    resolveNouns(resolver, results)
    {
        /*
         *   they want two items - return the whole list (if it has more
         *   than two items, we'll simply act as though they wanted all of
         *   them)
         */
        return removeAmbigFlags(resolver.getAll(self));
    }

    /* there's only me in the response list */
    getResponseList() { return [self]; }
;

grammar disambigPhrase(any): 'any' | 'any' 'of' 'them' : DisambigProd
    resolveNouns(resolver, results)
    {
        local lst;

        /* they want any item - arbitrarily pick the first one */
        lst = resolver.matchList.sublist(1, 1);

        /*
         *   add the "unclear disambiguation" flag to the item we picked,
         *   to indicate that the selection was arbitrary
         */
        if (lst.length() > 0)
            lst[1].flags_ |= UnclearDisambig;

        /* return the result */
        return lst;
    }

    /* there's only me in the response list */
    getResponseList() { return [self]; }
;

grammar disambigPhrase(list): disambigList->lst_ : DisambigProd
    resolveNouns(resolver, results)
    {
        return removeAmbigFlags(lst_.resolveNouns(resolver, results));
    }

    /* there's only me in the response list */
    getResponseList() { return lst_.getResponseList(); }
;

grammar disambigPhrase(ordinalList):
    disambigOrdinalList->lst_ 'ones'
    | 'the' disambigOrdinalList->lst_ 'ones'
    : DisambigProd

    resolveNouns(resolver, results)
    {
        /* return the list with the ambiguity flags removed */
        return removeAmbigFlags(lst_.resolveNouns(resolver, results));
    }

    /* the response list consists of my single ordinal list item */
    getResponseList() { return [lst_]; }
;

/*
 *   A disambig list consists of one or more disambig list items, connected
 *   by noun phrase conjunctions.  
 */
grammar disambigList(single): disambigListItem->item_ : DisambigProd
    resolveNouns(resolver, results)
    {
        return item_.resolveNouns(resolver, results);
    }

    /* the response list consists of my single item */
    getResponseList() { return [item_]; }
;

grammar disambigList(list):
    disambigListItem->item_ commandOrNounConjunction disambigList->lst_
    : DisambigProd

    resolveNouns(resolver, results)
    {
        return item_.resolveNouns(resolver, results)
            + lst_.resolveNouns(resolver, results);
    }

    /* my response list consists of each of our list items */
    getResponseList() { return [item_] + lst_.getResponseList(); }
;

/*
 *   Base class for ordinal disambiguation items
 */
class DisambigOrdProd: DisambigProd
    resolveNouns(resolver, results)
    {
        /* note the ordinal match */
        results.noteDisambigOrdinal();

        /* select the result by the ordinal */
        return selectByOrdinal(ord_, resolver, results);
    }

    selectByOrdinal(ordTok, resolver, results)
    {
        local idx;
        local matchList = resolver.ordinalMatchList;

        /*
         *   look up the meaning of the ordinal word (note that we assume
         *   that each ordinalWord is unique, since we only create one of
         *   each)
         */
        idx = cmdDict.findWord(ordTok, &ordinalWord)[1].numval;

        /*
         *   if it's the special value -1, it indicates that we should
         *   select the *last* item in the list
         */
        if (idx == -1)
            idx = matchList.length();

        /* if it's outside the limits of the match list, it's an error */
        if (idx > matchList.length())
        {
            /* note the problem */
            results.noteOrdinalOutOfRange(ordTok);

            /* no results */
            return [];
        }

        /* return the selected item as a one-item list */
        return matchList.sublist(idx, 1);
    }
;

/*
 *   A disambig vocab production is the base class for disambiguation
 *   phrases that involve vocabulary words.
 */
class DisambigVocabProd: DisambigProd
;

/*
 *   A disambig list item consists of:
 *
 *.  first/second/etc
 *.  the first/second/etc
 *.  first one/second one/etc
 *.  the first one/the second one/etc
 *.  <compound noun phrase>
 *.  possessive
 */

grammar disambigListItem(ordinal):
    ordinalWord->ord_
    | ordinalWord->ord_ 'one'
    | 'the' ordinalWord->ord_
    | 'the' ordinalWord->ord_ 'one'
    : DisambigOrdProd
;

grammar disambigListItem(noun):
    completeNounPhraseWithoutAll->np_
    | terminalNounPhrase->np_
    : DisambigVocabProd
    resolveNouns(resolver, results)
    {
        /* get the matches for the underlying noun phrase */
        local lst = np_.resolveNouns(resolver, results);

        /* note the matches */
        results.noteMatches(lst);

        /* return the match list */
        return lst;
    }
;

grammar disambigListItem(plural):
    pluralPhrase->np_
    : DisambigVocabProd
    resolveNouns(resolver, results)
    {
        local lst;

        /*
         *   get the underlying match list; since we explicitly have a
         *   plural, the result doesn't need to be unique, so simply
         *   return everything we find
         */
        lst = np_.resolveNouns(resolver, results);

        /*
         *   if we didn't get anything, it's an error; otherwise, take
         *   everything, since we explicitly wanted a plural usage
         */
        if (lst.length() == 0)
            results.noMatch(resolver.getAction(), np_.getOrigText());
        else
            results.noteMatches(lst);

        /* return the list */
        return lst;
    }
;

grammar disambigListItem(possessive): possessiveNounPhrase->poss_
    : DisambigPossessiveProd
;

/*
 *   A disambig ordinal list consists of two or more ordinal words
 *   separated by noun phrase conjunctions.  Note that there is a minimum
 *   of two entries in the list.
 */
grammar disambigOrdinalList(tail):
    ordinalWord->ord1_ ('and' | ',') ordinalWord->ord2_ : DisambigOrdProd
    resolveNouns(resolver, results)
    {
        /* note the pair of ordinal matches */
        results.noteDisambigOrdinal();
        results.noteDisambigOrdinal();

        /* combine the selections of our two ordinals */
        return selectByOrdinal(ord1_, resolver, results)
            + selectByOrdinal(ord2_, resolver, results);
    }
;

grammar disambigOrdinalList(head):
    ordinalWord->ord_ ('and' | ',') disambigOrdinalList->lst_
    : DisambigOrdProd
    resolveNouns(resolver, results)
    {
        /* note the ordinal match */
        results.noteDisambigOrdinal();

        /* combine the selections of our ordinal and the sublist */
        return selectByOrdinal(ord_, resolver, results)
            + lst_.resolveNouns(resolver, results);
    }
;


/* ------------------------------------------------------------------------ */
/*
 *   Ordinal words.  We define a limited set of these, since we only use
 *   them in a few special contexts where it would be unreasonable to need
 *   even as many as define here.
 */
#define defOrdinal(str, val) object ordinalWord=#@str numval=val

defOrdinal(former, 1);
defOrdinal(first, 1);
defOrdinal(second, 2);
defOrdinal(third, 3);
defOrdinal(fourth, 4);
defOrdinal(fifth, 5);
defOrdinal(sixth, 6);
defOrdinal(seventh, 7);
defOrdinal(eighth, 8);
defOrdinal(ninth, 9);
defOrdinal(tenth, 10);
defOrdinal(eleventh, 11);
defOrdinal(twelfth, 12);
defOrdinal(thirteenth, 13);
defOrdinal(fourteenth, 14);
defOrdinal(fifteenth, 15);
defOrdinal(sixteenth, 16);
defOrdinal(seventeenth, 17);
defOrdinal(eighteenth, 18);
defOrdinal(nineteenth, 19);
defOrdinal(twentieth, 20);
defOrdinal(1st, 1);
defOrdinal(2nd, 2);
defOrdinal(3rd, 3);
defOrdinal(4th, 4);
defOrdinal(5th, 5);
defOrdinal(6th, 6);
defOrdinal(7th, 7);
defOrdinal(8th, 8);
defOrdinal(9th, 9);
defOrdinal(10th, 10);
defOrdinal(11th, 11);
defOrdinal(12th, 12);
defOrdinal(13th, 13);
defOrdinal(14th, 14);
defOrdinal(15th, 15);
defOrdinal(16th, 16);
defOrdinal(17th, 17);
defOrdinal(18th, 18);
defOrdinal(19th, 19);
defOrdinal(20th, 20);

/*
 *   the special 'last' ordinal - the value -1 is special to indicate the
 *   last item in a list
 */
defOrdinal(last, -1);
defOrdinal(latter, -1);


/* ------------------------------------------------------------------------ */
/*
 *   A numeric production.  These can be either spelled-out numbers (such
 *   as "fifty-seven") or numbers entered in digit form (as in "57").
 */
class NumberProd: BasicProd
    /* get the numeric (integer) value */
    getval() { return 0; }

    /*
     *   Get the string version of the numeric value.  This should return
     *   a string, but the string should be in digit form.  If the
     *   original entry was in digit form, then the original entry should
     *   be returned; otherwise, a string should be constructed from the
     *   integer value.  By default, we'll do the latter.
     */
    getStrVal() { return toString(getval()); }
;

/*
 *   A quantifier is simply a number, entered with numerals or spelled out.
 */
grammar numberPhrase(digits): tokInt->num_ : NumberProd
    /* get the numeric value */
    getval() { return toInteger(num_); }

    /*
     *   get the string version of the numeric value - since the token was
     *   an integer to start with, return the actual integer value
     */
    getStrVal() { return num_; }
;

grammar numberPhrase(spelled): spelledNumber->num_ : NumberProd
    /* get the numeric value */
    getval() { return num_.getval(); }
;

/*
 *   A number phrase preceded by a pound sign.  We distinguish this kind of
 *   number phrase from plain numbers, since this kind has a somewhat more
 *   limited set of valid contexts.  
 */
grammar poundNumberPhrase(main): tokPoundInt->num_ : NumberProd
    /*
     *   get the numeric value - a tokPoundInt token has a pound sign
     *   followed by digits, so the numeric value is the value of the
     *   substring following the '#' sign
     */
    getval() { return toInteger(num_.substr(2)); }

    /*
     *   get the string value - we have a number token following the '#',
     *   so simply return the part after the '#'
     */
    getStrVal() { return num_.substr(2); }
;


/*
 *   Number literals.  We'll define a set of special objects for numbers:
 *   each object defines a number and a value for the number.
 */
#define defDigit(num, val) object digitWord=#@num numval=val
#define defTeen(num, val)  object teenWord=#@num numval=val
#define defTens(num, val)  object tensWord=#@num numval=val

defDigit(one, 1);
defDigit(two, 2);
defDigit(three, 3);
defDigit(four, 4);
defDigit(five, 5);
defDigit(six, 6);
defDigit(seven, 7);
defDigit(eight, 8);
defDigit(nine, 9);
defTeen(ten, 10);
defTeen(eleven, 11);
defTeen(twelve, 12);
defTeen(thirteen, 13);
defTeen(fourteen, 14);
defTeen(fifteen, 15);
defTeen(sixteen, 16);
defTeen(seventeen, 17);
defTeen(eighteen, 18);
defTeen(nineteen, 19);
defTens(twenty, 20);
defTens(thirty, 30);
defTens(forty, 40);
defTens(fifty, 50);
defTens(sixty, 60);
defTens(seventy, 70);
defTens(eighty, 80);
defTens(ninety, 90);

grammar spelledSmallNumber(digit): digitWord->num_ : NumberProd
    getval()
    {
        /*
         *   Look up the units word - there should be only one in the
         *   dictionary, since these are our special words.  Return the
         *   object's numeric value property 'numval', which gives the
         *   number for the name.
         */
        return cmdDict.findWord(num_, &digitWord)[1].numval;
    }
;

grammar spelledSmallNumber(teen): teenWord->num_ : NumberProd
    getval()
    {
        /* look up the dictionary word for the number */
        return cmdDict.findWord(num_, &teenWord)[1].numval;
    }
;

grammar spelledSmallNumber(tens): tensWord->num_ : NumberProd
    getval()
    {
        /* look up the dictionary word for the number */
        return cmdDict.findWord(num_, &tensWord)[1].numval;
    }
;

grammar spelledSmallNumber(tensAndUnits):
    tensWord->tens_ '-'->sep_ digitWord->units_
    | tensWord->tens_ digitWord->units_
    : NumberProd
    getval()
    {
        /* look up the words, and add up the values */
        return cmdDict.findWord(tens_, &tensWord)[1].numval
            + cmdDict.findWord(units_, &digitWord)[1].numval;
    }
;

grammar spelledSmallNumber(zero): 'zero' : NumberProd
    getval() { return 0; }
;

grammar spelledHundred(small): spelledSmallNumber->num_ : NumberProd
    getval() { return num_.getval(); }
;

grammar spelledHundred(hundreds): spelledSmallNumber->hun_ 'hundred'
    : NumberProd
    getval() { return hun_.getval() * 100; }
;

grammar spelledHundred(hundredsPlus):
    spelledSmallNumber->hun_ 'hundred' spelledSmallNumber->num_
    | spelledSmallNumber->hun_ 'hundred' 'and'->and_ spelledSmallNumber->num_
    : NumberProd
    getval() { return hun_.getval() * 100 + num_.getval(); }
;

grammar spelledHundred(aHundred): 'a' 'hundred' : NumberProd
    getval() { return 100; }
;

grammar spelledHundred(aHundredPlus):
    'a' 'hundred' 'and' spelledSmallNumber->num_
    : NumberProd
    getval() { return 100 + num_.getval(); }
;

grammar spelledThousand(thousands): spelledHundred->thou_ 'thousand'
    : NumberProd
    getval() { return thou_.getval() * 1000; }
;

grammar spelledThousand(thousandsPlus):
    spelledHundred->thou_ 'thousand' spelledHundred->num_
    : NumberProd
    getval() { return thou_.getval() * 1000 + num_.getval(); }
;

grammar spelledThousand(thousandsAndSmall):
    spelledHundred->thou_ 'thousand' 'and' spelledSmallNumber->num_
    : NumberProd
    getval() { return thou_.getval() * 1000 + num_.getval(); }
;

grammar spelledThousand(aThousand): 'a' 'thousand' : NumberProd
    getval() { return 1000; }
;

grammar spelledThousand(aThousandAndSmall):
    'a' 'thousand' 'and' spelledSmallNumber->num_
    : NumberProd
    getval() { return 1000 + num_.getval(); }
;

grammar spelledMillion(millions): spelledHundred->mil_ 'million': NumberProd
    getval() { return mil_.getval() * 1000000; }
;

grammar spelledMillion(millionsPlus):
    spelledHundred->mil_ 'million'
    (spelledThousand->nxt_ | spelledHundred->nxt_)
    : NumberProd
    getval() { return mil_.getval() * 1000000 + nxt_.getval(); }
;

grammar spelledMillion(aMillion): 'a' 'million' : NumberProd
    getval() { return 1000000; }
;

grammar spelledMillion(aMillionAndSmall):
    'a' 'million' 'and' spelledSmallNumber->num_
    : NumberProd
    getval() { return 1000000 + num_.getval(); }
;

grammar spelledMillion(millionsAndSmall):
    spelledHundred->mil_ 'million' 'and' spelledSmallNumber->num_
    : NumberProd
    getval() { return mil_.getval() * 1000000 + num_.getval(); }
;

grammar spelledNumber(main):
    spelledHundred->num_
    | spelledThousand->num_
    | spelledMillion->num_
    : NumberProd
    getval() { return num_.getval(); }
;


/* ------------------------------------------------------------------------ */
/*
 *   "OOPS" command syntax
 */
grammar oopsCommand(main):
    oopsPhrase->oops_ | oopsPhrase->oops_ '.' : BasicProd
    getNewTokens() { return oops_.getNewTokens(); }
;

grammar oopsPhrase(main):
    'oops' miscWordList->lst_
    | 'oops' ',' miscWordList->lst_
    | 'o' miscWordList->lst_
    | 'o' ',' miscWordList->lst_
    : BasicProd
    getNewTokens() { return lst_.getOrigTokenList(); }
;

grammar oopsPhrase(missing):
    'oops' | 'o'
    : BasicProd
    getNewTokens() { return nil; }
;

/* ------------------------------------------------------------------------ */
/*
 *   finishGame options.  We provide descriptions and keywords for the
 *   option objects here, because these are inherently language-specific.
 *   
 *   Note that we provide hyperlinks for our descriptions when possible.
 *   When we're in plain text mode, we can't show links, so we'll instead
 *   show an alternate form with the single-letter response highlighted in
 *   the text.  We don't highlight the single-letter response in the
 *   hyperlinked version because (a) if the user wants a shortcut, they can
 *   simply click the hyperlink, and (b) most UI's that show hyperlinks
 *   show a distinctive appearance for the hyperlink itself, so adding even
 *   more highlighting within the hyperlink starts to look awfully busy.  
 */
modify finishOptionQuit
    desc = "<<aHrefAlt('quit', 'QUIT', '<b>Q</b>UIT', 'Leave the story')>>"
    responseKeyword = 'quit'
    responseChar = 'q'
;

modify finishOptionRestore
    desc = "<<aHrefAlt('restore', 'RESTORE', '<b>R</b>ESTORE',
            'Restore a saved position')>> a saved position"
    responseKeyword = 'restore'
    responseChar = 'r'
;

modify finishOptionRestart
    desc = "<<aHrefAlt('restart', 'RESTART', 'RE<b>S</b>TART',
            'Start the story over from the beginning')>> the story"
    responseKeyword = 'restart'
    responseChar = 's'
;

modify finishOptionUndo
    desc = "<<aHrefAlt('undo', 'UNDO', '<b>U</b>NDO',
            'Undo the last move')>> the last move"
    responseKeyword = 'undo'
    responseChar = 'u'
;

modify finishOptionCredits
    desc = "see the <<aHrefAlt('credits', 'CREDITS', '<b>C</b>REDITS',
            'Show credits')>>"
    responseKeyword = 'credits'
    responseChar = 'c'
;

modify finishOptionFullScore
    desc = "see your <<aHrefAlt('full score', 'FULL SCORE',
            '<b>F</b>ULL SCORE', 'Show full score')>>"
    responseKeyword = 'full score'
    responseChar = 'f'
;

modify finishOptionAmusing
    desc = "see some <<aHrefAlt('amusing', 'AMUSING', '<b>A</b>MUSING',
            'Show some amusing things to try')>> things to try"
    responseKeyword = 'amusing'
    responseChar = 'a'
;

modify restoreOptionStartOver
    desc = "<<aHrefAlt('start', 'START', '<b>S</b>TART',
            'Start from the beginning')>> the game from the beginning"
    responseKeyword = 'start'
    responseChar = 's'
;

modify restoreOptionRestoreAnother
    desc = "<<aHrefAlt('restore', 'RESTORE', '<b>R</b>ESTORE',
            'Restore a saved position')>> a different saved position"
;

/* ------------------------------------------------------------------------ */
/*
 *   Context for Action.getVerbPhrase().  This keeps track of pronoun
 *   antecedents in cases where we're stringing together a series of verb
 *   phrases.
 */
class GetVerbPhraseContext: object
    /* get the objective form of an object, using a pronoun as appropriate */
    objNameObj(obj)
    {
        /*
         *   if it's the pronoun antecedent, use the pronoun form;
         *   otherwise, use the full name
         */
        if (obj == pronounObj)
            return obj.itObj;
        else
            return obj.theNameObj;
    }

    /* are we showing the given object pronomially? */
    isObjPronoun(obj) { return (obj == pronounObj); }

    /* set the pronoun antecedent */
    setPronounObj(obj) { pronounObj = obj; }

    /* the pronoun antecedent */
    pronounObj = nil
;

/*
 *   Default getVerbPhrase context.  This can be used when no other context
 *   is needed.  This context instance has no state - it doesn't track any
 *   antecedents.  
 */
defaultGetVerbPhraseContext: GetVerbPhraseContext
    /* we don't remember any antecedents */
    setPronounObj(obj) { }
;

/* ------------------------------------------------------------------------ */
/*
 *   Implicit action context.  This is passed to the message methods that
 *   generate implicit action announcements, to indicate the context in
 *   which the message is to be used.
 */
class ImplicitAnnouncementContext: object
    /*
     *   Should we use the infinitive form of the verb, or the participle
     *   form for generating the announcement?  By default, use use the
     *   participle form: "(first OPENING THE BOX)".
     */
    useInfPhrase = nil

    /* is this message going in a list? */
    isInList = nil

    /*
     *   Are we in a sublist of 'just trying' or 'just asking' messages?
     *   (We can only have sublist groupings one level deep, so we don't
     *   need to worry about what kind of sublist we're in.)
     */
    isInSublist = nil

    /* our getVerbPhrase context - by default, don't use one */
    getVerbCtx = nil

    /* generate the announcement message given the action description */
    buildImplicitAnnouncement(txt)
    {
        /* if we're not in a list, make it a full, stand-alone message */
        if (!isInList)
            txt = '<./p0>\n<.assume>first ' + txt + '<./assume>\n';

        /* return the result */
        return txt;
    }
;

/* the standard implicit action announcement context */
standardImpCtx: ImplicitAnnouncementContext;

/* the "just trying" implicit action announcement context */
tryingImpCtx: ImplicitAnnouncementContext
    /*
     *   The action was merely attempted, so use the infinitive phrase in
     *   the announcement: "(first trying to OPEN THE BOX)".
     */
    useInfPhrase = true

    /* build the announcement */
    buildImplicitAnnouncement(txt)
    {
        /*
         *   If we're not in a list of 'trying' messages, add the 'trying'
         *   prefix message to the action description.  This isn't
         *   necessary if we're in a 'trying' list, since the list itself
         *   will have the 'trying' part.
         */
        if (!isInSublist)
            txt = 'trying to ' + txt;

        /* now build the message into the full text as usual */
        return inherited(txt);
    }
;

/*
 *   The "asking question" implicit action announcement context.  By
 *   default, we generate the message exactly the same way we do for the
 *   'trying' case.
 */
askingImpCtx: tryingImpCtx;

/*
 *   A class for messages appearing in a list.  Within a list, we want to
 *   keep track of the last direct object, so that we can refer to it with
 *   a pronoun later in the list.
 */
class ListImpCtx: ImplicitAnnouncementContext, GetVerbPhraseContext
    /*
     *   Set the appropriate base context for the given implicit action
     *   announcement report (an ImplicitActionAnnouncement object).
     */
    setBaseCtx(ctx)
    {
        /*
         *   if this is a failed attempt, use a 'trying' context;
         *   otherwise, use a standard context
         */
        if (ctx.justTrying)
            baseCtx = tryingImpCtx;
        else if (ctx.justAsking)
            baseCtx = askingImpCtx;
        else
            baseCtx = standardImpCtx;
    }

    /* we're in a list */
    isInList = true

    /* we are our own getVerbPhrase context */
    getVerbCtx = (self)

    /* delegate the phrase format to our underlying announcement context */
    useInfPhrase = (delegated baseCtx)

    /* build the announcement using our underlying context */
    buildImplicitAnnouncement(txt) { return delegated baseCtx(txt); }

    /* our base context - we delegate some unoverridden behavior to this */
    baseCtx = nil
;

/* ------------------------------------------------------------------------ */
/*
 *   Language-specific Action modifications.
 */
modify Action
    /*
     *   In the English grammar, all 'predicate' grammar definitions
     *   (which are usually made via the VerbRule macro) are associated
     *   with Action match tree objects; in fact, each 'predicate' grammar
     *   match tree is the specific Action subclass associated with the
     *   grammar for the predicate.  This means that the Action associated
     *   with a grammar match is simply the grammar match object itself.
     *   Hence, we can resolve the action for a 'predicate' match simply
     *   by returning the match itself: it is the Action as well as the
     *   grammar match.
     *
     *   This approach ('grammar predicate' matches are based on Action
     *   subclasses) works well for languages like English that encode the
     *   role of each phrase in the word order of the sentence.
     *
     *   Languages that encode phrase roles using case markers or other
     *   devices tend to be freer with word order.  As a result,
     *   'predicate' grammars for such languages should generally not
     *   attempt to capture all possible word orderings for a given
     *   action, but should instead take the complementary approach of
     *   capturing the possible overall sentence structures independently
     *   of verb phrases, and plug in a verb phrase as a component, just
     *   like noun phrases plug into the English grammar.  In these cases,
     *   the match objects will NOT be Action subclasses; the Action
     *   objects will instead be buried down deeper in the match tree.
     *   Hence, resolveAction() must be defined on whatever class is used
     *   to construct 'predicate' grammar matches, instead of on Action,
     *   since Action will not be a 'predicate' match.
     */
    resolveAction(issuingActor, targetActor) { return self; }

    /*
     *   Return the interrogative pronoun for a missing object in one of
     *   our object roles.  In most cases, this is simply "what", but for
     *   some actions, "whom" is more appropriate (for example, the direct
     *   object of "ask" is implicitly a person, so "whom" is most
     *   appropriate for this role).
     */
    whatObj(which)
    {
        /* intransitive verbs have no objects, so there's nothing to show */
    }

    /*
     *   Translate an interrogative word for whatObj.  If the word is
     *   'whom', translate to the library message for 'whom'; this allows
     *   authors to use 'who' rather than 'whom' as the objective form of
     *   'who', which sounds less stuffy to many people.
     */
    whatTranslate(txt)
    {
        /*
         *   if it's 'whom', translate to the library message for 'whom';
         *   otherwise, just show the word as is
         */
        return (txt == 'whom' ? gLibMessages.whomPronoun : txt);
    }

    /*
     *   Return a string with the appropriate pronoun (objective form) for
     *   a list of object matches, with the given resolved cardinality.
     *   This list is a list of ResolveInfo objects.
     */
    objListPronoun(objList)
    {
        local himCnt, herCnt, themCnt;
        local FirstPersonCnt, SecondPersonCnt;
        local resolvedNumber;

        /* if there's no object list at all, just use 'it' */
        if (objList == nil || objList == [])
            return 'it';

        /* note the number of objects in the resolved list */
        resolvedNumber = objList.length();

        /*
         *   In the tentatively resolved object list, we might have hidden
         *   away ambiguous matches.  Expand those back into the list so
         *   we have the full list of in-scope matches.
         */
        foreach (local cur in objList)
        {
            /*
             *   if this one has hidden ambiguous objects, add the hidden
             *   objects back into our list
             */
            if (cur.extraObjects != nil)
                objList += cur.extraObjects;
        }

        /*
         *   if the desired cardinality is plural and the object list has
         *   more than one object, simply say 'them'
         */
        if (objList.length() > 1 && resolvedNumber > 1)
            return 'them';

        /*
         *   singular cardinality - count masculine and feminine objects,
         *   and count the referral persons
         */
        himCnt = herCnt = themCnt = 0;
        FirstPersonCnt = SecondPersonCnt = 0;
        foreach (local cur in objList)
        {
            /* if it's masculine, count it */
            if (cur.obj_.isHim)
                ++himCnt;

            /* if it's feminine, count it */
            if (cur.obj_.isHer)
                ++herCnt;

            /* if it has plural usage, count it */
            if (cur.obj_.isPlural)
                ++themCnt;

            /* if it's first person usage, count it */
            if (cur.obj_.referralPerson == FirstPerson)
                ++FirstPersonCnt;

            /* if it's second person usage, count it */
            if (cur.obj_.referralPerson == SecondPerson)
                ++SecondPersonCnt;
        }

        /*
         *   if they all have plural usage, show "them"; if they're all of
         *   one gender, show "him" or "her" as appropriate; if they're
         *   all neuter, show "it"; otherwise, show "them"
         */
        if (themCnt == objList.length())
            return 'them';
        else if (FirstPersonCnt == objList.length())
            return 'myself';
        else if (SecondPersonCnt == objList.length())
            return 'yourself';
        else if (himCnt == objList.length() && herCnt == 0)
            return 'him';
        else if (herCnt == objList.length() && himCnt == 0)
            return 'her';
        else if (herCnt == 0 && himCnt == 0)
            return 'it';
        else
            return 'them';
    }

    /*
     *   Announce a default object used with this action.
     *
     *   'resolvedAllObjects' indicates where we are in the command
     *   processing: this is true if we've already resolved all of the
     *   other objects in the command, nil if not.  We use this
     *   information to get the phrasing right according to the situation.
     */
    announceDefaultObject(obj, whichObj, resolvedAllObjects)
    {
        /*
         *   the basic action class takes no objects, so there can be no
         *   default announcement
         */
        return '';
    }

    /*
     *   Announce all defaulted objects in the action.  By default, we
     *   show nothing.
     */
    announceAllDefaultObjects(allResolved) { }

    /*
     *   Return a phrase describing the action performed implicitly, as a
     *   participle phrase.  'ctx' is an ImplicitAnnouncementContext object
     *   describing the context in which we're generating the phrase.
     *
     *   This comes in two forms: if the context indicates we're only
     *   attempting the action, we'll return an infinitive phrase ("open
     *   the box") for use in a larger participle phrase describing the
     *   attempt ("trying to...").  Otherwise, we'll be describing the
     *   action as actually having been performed, so we'll return a
     *   present participle phrase ("opening the box").
     */
    getImplicitPhrase(ctx)
    {
        /*
         *   Get the phrase.  Use the infinitive or participle form, as
         *   indicated in the context.
         */
        return getVerbPhrase(ctx.useInfPhrase, ctx.getVerbCtx);
    }

    /*
     *   Get the infinitive form of the action.  We are NOT to include the
     *   infinitive complementizer (i.e., "to") as part of the result,
     *   since the complementizer isn't used in all contexts in which we
     *   might want to use the infinitive; for example, we don't want a
     *   "to" in phrases involving an auxiliary verb, such as "he can open
     *   the box."
     */
    getInfPhrase()
    {
        /* return the verb phrase in infinitive form */
        return getVerbPhrase(true, nil);
    }

    /*
     *   Get the root infinitive form of our verb phrase as part of a
     *   question in which one of the verb's objects is the "unknown" of
     *   the interrogative.  'which' is one of the role markers
     *   (DirectObject, IndirectObject, etc), indicating which object is
     *   the subject of the interrogative.
     *
     *   For example, for the verb UNLOCK <dobj> WITH <iobj>, if the
     *   unknown is the direct object, the phrase we'd return would be
     *   "unlock": this would plug into contexts such as "what do you want
     *   to unlock."  If the indirect object is the unknown for the same
     *   verb, the phrase would be "unlock it with", which would plug in as
     *   "what do you want to unlock it with".
     *
     *   Note that we are NOT to include the infinitive complementizer
     *   (i.e., "to") as part of the phrase we generate, since the
     *   complementizer isn't used in some contexts where the infinitive
     *   conjugation is needed (for example, "what should I <infinitive>").
     */
    getQuestionInf(which)
    {
        /*
         *   for a verb without objects, this is the same as the basic
         *   infinitive
         */
        return getInfPhrase();
    }

    /*
     *   Get a string describing the full action in present participle
     *   form, using the current command objects: "taking the watch",
     *   "putting the book on the shelf"
     */
    getParticiplePhrase()
    {
        /* return the verb phrase in participle form */
        return getVerbPhrase(nil, nil);
    }

    /*
     *   Get the full verb phrase in either infinitive or participle
     *   format.  This is a common handler for getInfinitivePhrase() and
     *   getParticiplePhrase().
     *
     *   'ctx' is a GetVerbPhraseContext object, which lets us keep track
     *   of antecedents when we're stringing together multiple verb
     *   phrases.  'ctx' can be nil if the verb phrase is being used in
     *   isolation.
     */
    getVerbPhrase(inf, ctx)
    {
        /*
         *   parse the verbPhrase into the parts before and after the
         *   slash, and any additional text following the slash part
         */
        rexMatch('(.*)/(<alphanum|-|squote>+)(.*)', verbPhrase);

        /* return the appropriate parts */
        if (inf)
        {
            /*
             *   infinitive - we want the part before the slash, plus the
             *   extra prepositions (or whatever) after the switched part
             */
            return rexGroup(1)[3] + rexGroup(3)[3];
        }
        else
        {
            /* participle - it's the part after the slash */
            return rexGroup(2)[3] + rexGroup(3)[3];
        }
    }

    /*
     *   Show the "noMatch" library message.  For most verbs, we use the
     *   basic "you can't see that here".  Verbs that are mostly used with
     *   intangible objects, such as LISTEN TO and SMELL, might want to
     *   override this to use a less visually-oriented message.
     */
    noMatch(msgObj, actor, txt) { msgObj.noMatchCannotSee(actor, txt); }

    /*
     *   Verb flags - these are used to control certain aspects of verb
     *   formatting.  By default, we have no special flags.
     */
    verbFlags = 0

    /* add a space prefix/suffix to a string if the string is non-empty */
    spPrefix(str) { return (str == '' ? str : ' ' + str); }
    spSuffix(str) { return (str == '' ? str : str + ' '); }
;

/*
 *   English-specific additions for single-object verbs.
 */
modify TAction
    /* return an interrogative word for an object of the action */
    whatObj(which)
    {
        /*
         *   Show the interrogative for our direct object - this is the
         *   last word enclosed in parentheses in our verbPhrase string.
         */
        rexSearch('<lparen>.*?(<alpha>+)<rparen>', verbPhrase);
        return whatTranslate(rexGroup(1)[3]);
    }

    /* announce a default object used with this action */
    announceDefaultObject(obj, whichObj, resolvedAllObjects)
    {
        /*
         *   get any direct object preposition - this is the part inside
         *   the "(what)" specifier parens, excluding the last word
         */
        rexSearch('<lparen>(.*<space>+)?<alpha>+<rparen>', verbPhrase);
        local prep = (rexGroup(1) == nil ? '' : rexGroup(1)[3]);

        /* do any verb-specific adjustment of the preposition */
        if (prep != nil)
            prep = adjustDefaultObjectPrep(prep, obj);

        /* 
         *   get the object name - we need to distinguish from everything
         *   else in scope, since we considered everything in scope when
         *   making our pick 
         */
        local nm = obj.getAnnouncementDistinguisher(
            gActor.scopeList()).theName(obj);

        /* show the preposition (if any) and the object */
        return (prep == '' ? nm : prep + nm);
    }

    /*
     *   Adjust the preposition.  In some cases, the verb will want to vary
     *   the preposition according to the object.  This method can return a
     *   custom preposition in place of the one in the verbPhrase.  By
     *   default, we just use the fixed preposition from the verbPhrase,
     *   which is passed in to us in 'prep'.  
     */
    adjustDefaultObjectPrep(prep, obj) { return prep; }

    /* announce all defaulted objects */
    announceAllDefaultObjects(allResolved)
    {
        /* announce a defaulted direct object if appropriate */
        maybeAnnounceDefaultObject(dobjList_, DirectObject, allResolved);
    }

    /* show the verb's basic infinitive form for an interrogative */
    getQuestionInf(which)
    {
        /*
         *   Show the present-tense verb form (removing the participle
         *   part - the "/xxxing" part).  Include any prepositions
         *   attached to the verb itself or to the direct object (inside
         *   the "(what)" parens).
         */
        rexSearch('(.*)/<alphanum|-|squote>+(.*?)<space>+'
                  + '<lparen>(.*?)<space>*?<alpha>+<rparen>',
                  verbPhrase);
        return rexGroup(1)[3] + spPrefix(rexGroup(2)[3])
            + spPrefix(rexGroup(3)[3]);
    }

    /* get the verb phrase in infinitive or participle form */
    getVerbPhrase(inf, ctx)
    {
        local dobj;
        local dobjText;
        local dobjIsPronoun;
        local ret;

        /* use the default pronoun context if one wasn't supplied */
        if (ctx == nil)
            ctx = defaultGetVerbPhraseContext;

        /* get the direct object */
        dobj = getDobj();

        /* note if it's a pronoun */
        dobjIsPronoun = ctx.isObjPronoun(dobj);

        /* get the direct object name */
        dobjText = ctx.objNameObj(dobj);

        /* get the phrasing */
        ret = getVerbPhrase1(inf, verbPhrase, dobjText, dobjIsPronoun);

        /* set the pronoun antecedent to my direct object */
        ctx.setPronounObj(dobj);

        /* return the result */
        return ret;
    }

    /*
     *   Given the text of the direct object phrase, build the verb phrase
     *   for a one-object verb.  This is a class method that can be used by
     *   other kinds of verbs (i.e., non-TActions) that use phrasing like a
     *   single object.
     *
     *   'inf' is a flag indicating whether to use the infinitive form
     *   (true) or the present participle form (nil); 'vp' is the
     *   verbPhrase string; 'dobjText' is the direct object phrase's text;
     *   and 'dobjIsPronoun' is true if the dobj text is rendered as a
     *   pronoun.
     */
    getVerbPhrase1(inf, vp, dobjText, dobjIsPronoun)
    {
        local ret;
        local dprep;
        local vcomp;

        /*
         *   parse the verbPhrase: pick out the 'infinitive/participle'
         *   part, the complementizer part up to the '(what)' direct
         *   object placeholder, and any preposition within the '(what)'
         *   specifier
         */
        rexMatch('(.*)/(<alphanum|-|squote>+)(.*) '
                 + '<lparen>(.*?)<space>*?<alpha>+<rparen>(.*)',
                 vp);

        /* start off with the infinitive or participle, as desired */
        if (inf)
            ret = rexGroup(1)[3];
        else
            ret = rexGroup(2)[3];

        /* get the prepositional complementizer */
        vcomp = rexGroup(3)[3];

        /* get the direct object preposition */
        dprep = rexGroup(4)[3];

        /* do any verb-specific adjustment of the preposition */
        if (dprep != nil)
            dprep = adjustDefaultObjectPrep(dprep, getDobj());

        /*
         *   if the direct object is not a pronoun, put the complementizer
         *   BEFORE the direct object (the 'up' in "PICKING UP THE BOX")
         */
        if (!dobjIsPronoun)
            ret += spPrefix(vcomp);

        /* add the direct object preposition */
        ret += spPrefix(dprep);

        /* add the direct object, using the pronoun form if applicable */
        ret += ' ' + dobjText;

        /*
         *   if the direct object is a pronoun, put the complementizer
         *   AFTER the direct object (the 'up' in "PICKING IT UP")
         */
        if (dobjIsPronoun)
            ret += spPrefix(vcomp);

        /*
         *   if there's any suffix following the direct object
         *   placeholder, add it at the end of the phrase
         */
        ret += rexGroup(5)[3];

        /* return the complete phrase string */
        return ret;
    }
;

/*
 *   English-specific additions for two-object verbs.
 */
modify TIAction
    /*
     *   Flag: omit the indirect object in a query for a missing direct
     *   object.  For many verbs, if we already know the indirect object
     *   and we need to ask for the direct object, the query sounds best
     *   when it includes the indirect object: "what do you want to put in
     *   it?"  or "what do you want to take from it?".  This is the
     *   default phrasing.
     *
     *   However, the corresponding query for some verbs sounds weird:
     *   "what do you want to dig in with it?" or "whom do you want to ask
     *   about it?".  For such actions, this property should be set to
     *   true to indicate that the indirect object should be omitted from
     *   the queries, which will change the phrasing to "what do you want
     *   to dig in", "whom do you want to ask", and so on.
     */
    omitIobjInDobjQuery = nil

    /*
     *   For VerbRules: does this verb rule have a prepositional or
     *   structural phrasing of the direct and indirect object slots?  That
     *   is, are the object slots determined by a prepositional marker, or
     *   purely by word order?  For most English verbs with two objects,
     *   the indirect object is marked by a preposition: GIVE BOOK TO BOB,
     *   PUT BOOK IN BOX.  There are a few English verbs that don't include
     *   any prespositional markers for the objects, though, and assign the
     *   noun phrase roles purely by the word order: GIVE BOB BOOK, SHOW
     *   BOB BOOK, THROW BOB BOOK.  We define these phrasings with separate
     *   verb rules, which we mark with this property.
     *
     *   We use this in ranking verb matches.  Non-prepositional verb
     *   structures are especially prone to matching where they shouldn't,
     *   because we can often find a way to pick out words to fill the
     *   slots in the absence of any marker words.  For example, GIVE GREEN
     *   BOOK could be interpreted as GIVE BOOK TO GREEN, where GREEN is
     *   assumed to be an adjective-ending noun phrase; but the player
     *   probably means to give the green book to someone who they assumed
     *   would be filled in as a default.  So, whenever we find an
     *   interpretation that involves a non-prespositional phrasing, we'll
     *   use this flag to know we should be suspicious of it and try
     *   alternative phrasing first.
     *
     *   Most two-object verbs in English use prepositional markers, so
     *   we'll set this as the default.  Individual VerbRules that use
     *   purely structural phrasing should override this.
     */
    isPrepositionalPhrasing = true

    /* resolve noun phrases */
    resolveNouns(issuingActor, targetActor, results)
    {
        /*
         *   If we're a non-prepositional phrasing, it means that we have
         *   the VERB IOBJ DOBJ word ordering (as in GIVE BOB BOX or THROW
         *   BOB COIN).  For grammar match ranking purposes, give these
         *   phrasings a lower match probability when the dobj phrase
         *   doesn't have a clear qualifier.  If the dobj phrase starts
         *   with 'the' or a qualifier word like that (GIVE BOB THE BOX),
         *   then it's pretty clear that the structural phrasing is right
         *   after all; but if there's no qualifier, we could reading too
         *   much into the word order.  We could have something like GIVE
         *   GREEN BOX, where we *could* treat this as two objects, but we
         *   could just as well have a missing indirect object phrase.
         */
        if (!isPrepositionalPhrasing)
        {
            /*
             *   If the direct object phrase starts with 'a', 'an', 'the',
             *   'some', or 'any', the grammar is pretty clearly a good
             *   match for the non-prepositional phrasing.  Otherwise, it's
             *   suspect, so rank it accordingly.
             */
            if (rexMatch('(a|an|the|some|any)<space>',
                         dobjMatch.getOrigText()) == nil)
            {
                /* note this as weak phrasing level 100 */
                results.noteWeakPhrasing(100);
            }
        }

        /* inherit the base handling */
        inherited(issuingActor, targetActor, results);
    }

    /* get the interrogative for one of our objects */
    whatObj(which)
    {
        switch (which)
        {
        case DirectObject:
            /*
             *   the direct object interrogative is the first word in
             *   parentheses in our verbPhrase string
             */
            rexSearch('<lparen>.*?(<alpha>+)<rparen>', verbPhrase);
            break;

        case IndirectObject:
            /*
             *   the indirect object interrogative is the second
             *   parenthesized word in our verbPhrase string
             */
            rexSearch('<rparen>.*<lparen>.*?(<alpha>+)<rparen>', verbPhrase);
            break;
        }

        /* show the group match */
        return whatTranslate(rexGroup(1)[3]);
    }

    /* announce a default object used with this action */
    announceDefaultObject(obj, whichObj, resolvedAllObjects)
    {
        /* presume we won't have a verb or preposition */
        local verb = '';
        local prep = '';

        /*
         *   Check the full phrasing - if we're showing the direct object,
         *   but an indirect object was supplied, use the verb's
         *   participle form ("asking bob") in the default string, since
         *   we must clarify that we're not tagging the default string on
         *   to the command line.  Don't include the participle form if we
         *   don't know all the objects yet, since in this case we are in
         *   fact tagging the default string onto the command so far, as
         *   there's nothing else in the command to get in the way.
         */
        if (whichObj == DirectObject && resolvedAllObjects)
        {
            /*
             *   extract the verb's participle form (including any
             *   complementizer phrase)
             */
            rexSearch('/(<^lparen>+) <lparen>', verbPhrase);
            verb = rexGroup(1)[3] + ' ';
        }

        /* get the preposition to use, if any */
        switch(whichObj)
        {
        case DirectObject:
            /* use the preposition in the first "(what)" phrase */
            rexSearch('<lparen>(.*?)<space>*<alpha>+<rparen>', verbPhrase);
            prep = rexGroup(1)[3];
            break;

        case IndirectObject:
            /* use the preposition in the second "(what)" phrase */
            rexSearch('<rparen>.*<lparen>(.*?)<space>*<alpha>+<rparen>',
                      verbPhrase);
            prep = rexGroup(1)[3];
            break;
        }

        /* 
         *   get the object name - we need to distinguish from everything
         *   else in scope, since we considered everything in scope when
         *   making our pick 
         */
        local nm = obj.getAnnouncementDistinguisher(
            gActor.scopeList()).theName(obj);

        /* build and return the complete phrase */
        return spSuffix(verb) + spSuffix(prep) + nm;
    }

    /* announce all defaulted objects */
    announceAllDefaultObjects(allResolved)
    {
        /* announce a defaulted direct object if appropriate */
        maybeAnnounceDefaultObject(dobjList_, DirectObject, allResolved);

        /* announce a defaulted indirect object if appropriate */
        maybeAnnounceDefaultObject(iobjList_, IndirectObject, allResolved);
    }

    /* show the verb's basic infinitive form for an interrogative */
    getQuestionInf(which)
    {
        local ret;
        local vcomp;
        local dprep;
        local iprep;
        local pro;

        /*
         *   Our verb phrase can one of three formats, depending on which
         *   object role we're asking about (the part in <angle brackets>
         *   is the part we're responsible for generating).  In these
         *   formats, 'verb' is the verb infinitive; 'comp' is the
         *   complementizer, if any (e.g., the 'up' in 'pick up'); 'dprep'
         *   is the direct object preposition (the 'in' in 'dig in x with
         *   y'); and 'iprep' is the indirect object preposition (the
         *   'with' in 'dig in x with y').
         *
         *   asking for dobj: verb vcomp dprep iprep it ('what do you want
         *   to <open with it>?', '<dig in with it>', '<look up in it>').
         *
         *   asking for dobj, but suppressing the iobj part: verb vcomp
         *   dprep ('what do you want to <turn>?', '<look up>?', '<dig
         *   in>')
         *
         *   asking for iobj: verb dprep it vcomp iprep ('what do you want
         *   to <open it with>', '<dig in it with>', '<look it up in>'
         */

        /* parse the verbPhrase into its component parts */
        rexMatch('(.*)/<alphanum|-|squote>+(?:<space>+(<^lparen>*))?'
                 + '<space>+<lparen>(.*?)<space>*<alpha>+<rparen>'
                 + '<space>+<lparen>(.*?)<space>*<alpha>+<rparen>',
                 verbPhrase);

        /* pull out the verb */
        ret = rexGroup(1)[3];

        /* pull out the verb complementizer */
        vcomp = (rexGroup(2) == nil ? '' : rexGroup(2)[3]);

        /* pull out the direct and indirect object prepositions */
        dprep = rexGroup(3)[3];
        iprep = rexGroup(4)[3];

        /* get the pronoun for the other object phrase */
        pro = getOtherMessageObjectPronoun(which);

        /* check what we're asking about */
        if (which == DirectObject)
        {
            /* add the <vcomp dprep> part in all cases */
            ret += spPrefix(vcomp) + spPrefix(dprep);

            /* add the <iprep it> part if we want the indirect object part */
            if (!omitIobjInDobjQuery && pro != nil)
                ret += spPrefix(iprep) + ' ' + pro;
        }
        else
        {
            /* add the <dprep it> part if appropriate */
            if (pro != nil)
                ret += spPrefix(dprep) + ' ' + pro;

            /* add the <vcomp iprep> part */
            ret += spPrefix(vcomp) + spPrefix(iprep);
        }

        /* return the result */
        return ret;
    }

    /*
     *   Get the pronoun for the message object in the given role.
     */
    getOtherMessageObjectPronoun(which)
    {
        local lst;

        /*
         *   Get the resolution list (or tentative resolution list) for the
         *   *other* object, since we want to show a pronoun representing
         *   the other object.  If we don't have a fully-resolved list for
         *   the other object, use the tentative resolution list, which
         *   we're guaranteed to have by the time we start resolving
         *   anything (and thus by the time we need to ask for objects).
         */
        lst = (which == DirectObject ? iobjList_ : dobjList_);
        if (lst == nil || lst == [])
            lst = (which == DirectObject
                   ? tentativeIobj_ : tentativeDobj_);

        /* if we found an object list, use the pronoun for the list */
        if (lst != nil && lst != [])
        {
            /* we got a list - return a suitable pronoun for this list */
            return objListPronoun(lst);
        }
        else
        {
            /* there's no object list, so there's no pronoun */
            return nil;
        }
    }

    /* get the verb phrase in infinitive or participle form */
    getVerbPhrase(inf, ctx)
    {
        local dobj, dobjText, dobjIsPronoun;
        local iobj, iobjText;
        local ret;

        /* use the default context if one wasn't supplied */
        if (ctx == nil)
            ctx = defaultGetVerbPhraseContext;

        /* get the direct object information */
        dobj = getDobj();
        dobjText = ctx.objNameObj(dobj);
        dobjIsPronoun = ctx.isObjPronoun(dobj);

        /* get the indirect object information */
        iobj = getIobj();
        iobjText = (iobj != nil ? ctx.objNameObj(iobj) : nil);

        /* get the phrasing */
        ret = getVerbPhrase2(inf, verbPhrase,
                             dobjText, dobjIsPronoun, iobjText);

        /*
         *   Set the antecedent for the next verb phrase.  Our direct
         *   object is normally the antecedent; however, if the indirect
         *   object matches the current antecedent, keep the current
         *   antecedent, so that 'it' (or whatever) remains the same for
         *   the next verb phrase.
         */
        if (ctx.pronounObj != iobj)
            ctx.setPronounObj(dobj);

        /* return the result */
        return ret;
    }

    /*
     *   Get the verb phrase for a two-object (dobj + iobj) phrasing.  This
     *   is a class method, so that it can be reused by unrelated (i.e.,
     *   non-TIAction) classes that also use two-object syntax but with
     *   other internal structures.  This is the two-object equivalent of
     *   TAction.getVerbPhrase1().
     */
    getVerbPhrase2(inf, vp, dobjText, dobjIsPronoun, iobjText)
    {
        local ret;
        local vcomp;
        local dprep, iprep;

        /* parse the verbPhrase into its component parts */
        rexMatch('(.*)/(<alphanum|-|squote>+)(?:<space>+(<^lparen>*))?'
                 + '<space>+<lparen>(.*?)<space>*<alpha>+<rparen>'
                 + '<space>+<lparen>(.*?)<space>*<alpha>+<rparen>',
                 vp);

        /* start off with the infinitive or participle, as desired */
        if (inf)
            ret = rexGroup(1)[3];
        else
            ret = rexGroup(2)[3];

        /* get the complementizer */
        vcomp = (rexGroup(3) == nil ? '' : rexGroup(3)[3]);

        /* get the direct and indirect object prepositions */
        dprep = rexGroup(4)[3];
        iprep = rexGroup(5)[3];

        /*
         *   add the complementizer BEFORE the direct object, if the
         *   direct object is being shown as a full name ("PICK UP BOX")
         */
        if (!dobjIsPronoun)
            ret += spPrefix(vcomp);

        /*
         *   add the direct object and its preposition, using a pronoun if
         *   applicable
         */
        ret += spPrefix(dprep) + ' ' + dobjText;

        /*
         *   add the complementizer AFTER the direct object, if the direct
         *   object is shown as a pronoun ("PICK IT UP")
         */
        if (dobjIsPronoun)
            ret += spPrefix(vcomp);

        /* if we have an indirect object, add it with its preposition */
        if (iobjText != nil)
            ret += spPrefix(iprep) + ' ' + iobjText;

        /* return the result phrase */
        return ret;
    }
;

/*
 *   English-specific additions for verbs taking a literal phrase as the
 *   sole object.
 */
modify LiteralAction
    /* provide a base verbPhrase, in case an instance leaves it out */
    verbPhrase = 'verb/verbing (what)'

    /* get an interrogative word for an object of the action */
    whatObj(which)
    {
        /* use the same processing as TAction */
        return delegated TAction(which);
    }

    getVerbPhrase(inf, ctx)
    {
        /* handle this as though the literal were a direct object phrase */
        return TAction.getVerbPhrase1(inf, verbPhrase, gLiteral, nil);
    }

    getQuestionInf(which)
    {
        /* use the same handling as for a regular one-object action */
        return delegated TAction(which);
    }

;

/*
 *   English-specific additions for verbs of a direct object and a literal
 *   phrase.
 */
modify LiteralTAction
    announceDefaultObject(obj, whichObj, resolvedAllObjects)
    {
        /*
         *   Use the same handling as for a regular two-object action.  We
         *   can only default the actual object in this kind of verb; the
         *   actual object always fills the DirectObject slot, but in
         *   message generation it might use a different slot, so use the
         *   message generation slot here.
         */
        return delegated TIAction(obj, whichMessageObject,
                                  resolvedAllObjects);
    }

    whatObj(which)
    {
        /* use the same handling we use for a regular two-object action */
        return delegated TIAction(which);
    }

    getQuestionInf(which)
    {
        /*
         *   use the same handling as for a two-object action (but note
         *   that we override getMessageObjectPronoun(), which will affect
         *   the way we present the verb infinitive in some cases)
         */
        return delegated TIAction(which);
    }

    /*
     *   When we want to show a verb infinitive phrase that involves a
     *   pronoun for the literal phrase, refer to the literal as 'that'
     *   rather than 'it' or anything else.
     */
    getOtherMessageObjectPronoun(which)
    {
        /*
         *   If we're asking about the literal phrase, then the other
         *   pronoun is for the resolved object: so, return the pronoun
         *   for the direct object phrase, because we *always* store the
         *   non-literal in the direct object slot, regardless of the
         *   actual phrasing of the action.
         *
         *   If we're asking about the resolved object (i.e., not the
         *   literal phrase), then return 'that' as the pronoun for the
         *   literal phrase.
         */
        if (which == whichMessageLiteral)
        {
            /*
             *   we're asking about the literal, so the other pronoun is
             *   for the resolved object, which is always in the direct
             *   object slot (so the 'other' slot is effectively the
             *   indirect object)
             */
            return delegated TIAction(IndirectObject);
        }
        else
        {
            /*
             *   We're asking about the resolved object, so the other
             *   pronoun is for the literal phrase: always use 'that' to
             *   refer to the literal phrase.
             */
            return 'that';
        }
    }

    getVerbPhrase(inf, ctx)
    {
        local dobj, dobjText, dobjIsPronoun;
        local litText;
        local ret;

        /* use the default context if one wasn't supplied */
        if (ctx == nil)
            ctx = defaultGetVerbPhraseContext;

        /* get the direct object information */
        dobj = getDobj();
        dobjText = ctx.objNameObj(dobj);
        dobjIsPronoun = ctx.isObjPronoun(dobj);

        /* get our literal text */
        litText = gLiteral;

        /*
         *   Use the standard two-object phrasing.  The order of the
         *   phrasing depends on whether our literal phrase is in the
         *   direct or indirect object slot.
         */
        if (whichMessageLiteral == DirectObject)
            ret = TIAction.getVerbPhrase2(inf, verbPhrase,
                                          litText, nil, dobjText);
        else
            ret = TIAction.getVerbPhrase2(inf, verbPhrase,
                                          dobjText, dobjIsPronoun, litText);

        /* use the direct object as the antecedent for the next phrase */
        ctx.setPronounObj(dobj);

        /* return the result */
        return ret;
    }
;

/*
 *   English-specific additions for verbs taking a topic phrase as the sole
 *   object.  
 */
modify TopicAction
    /* get an interrogative word for an object of the action */
    whatObj(which)
    {
        /* use the same processing as TAction */
        return delegated TAction(which);
    }

    getVerbPhrase(inf, ctx)
    {
        /* handle this as though the topic text were a direct object phrase */
        return TAction.getVerbPhrase1(
            inf, verbPhrase, getTopic().getTopicText().toLower(), nil);
    }

    getQuestionInf(which)
    {
        /* use the same handling as for a regular one-object action */
        return delegated TAction(which);
    }

;

/*
 *   English-specific additions for verbs with topic phrases.
 */
modify TopicTAction
    announceDefaultObject(obj, whichObj, resolvedAllObjects)
    {
        /*
         *   Use the same handling as for a regular two-object action.  We
         *   can only default the actual object in this kind of verb; the
         *   actual object always fills the DirectObject slot, but in
         *   message generation it might use a different slot, so use the
         *   message generation slot here.
         */
        return delegated TIAction(obj, whichMessageObject,
                                  resolvedAllObjects);
    }

    whatObj(which)
    {
        /* use the same handling we use for a regular two-object action */
        return delegated TIAction(which);
    }

    getQuestionInf(which)
    {
        /* use the same handling as for a regular two-object action */
        return delegated TIAction(which);
    }

    getOtherMessageObjectPronoun(which)
    {
        /*
         *   If we're asking about the topic, then the other pronoun is
         *   for the resolved object, which is always in the direct object
         *   slot.  If we're asking about the resolved object, then return
         *   a pronoun for the topic.
         */
        if (which == whichMessageTopic)
        {
            /*
             *   we want the pronoun for the resolved object, which is
             *   always in the direct object slot (so the 'other' slot is
             *   effectively the indirect object)
             */
            return delegated TIAction(IndirectObject);
        }
        else
        {
            /* return a generic pronoun for the topic */
            return 'that';
        }
    }

    getVerbPhrase(inf, ctx)
    {
        local dobj, dobjText, dobjIsPronoun;
        local topicText;
        local ret;

        /* use the default context if one wasn't supplied */
        if (ctx == nil)
            ctx = defaultGetVerbPhraseContext;

        /* get the direct object information */
        dobj = getDobj();
        dobjText = ctx.objNameObj(dobj);
        dobjIsPronoun = ctx.isObjPronoun(dobj);

        /* get our topic phrase */
        topicText = getTopic().getTopicText().toLower();

        /*
         *   Use the standard two-object phrasing.  The order of the
         *   phrasing depends on whether our topic phrase is in the direct
         *   or indirect object slot.
         */
        if (whichMessageTopic == DirectObject)
            ret = TIAction.getVerbPhrase2(inf, verbPhrase,
                                          topicText, nil, dobjText);
        else
            ret = TIAction.getVerbPhrase2(inf, verbPhrase,
                                          dobjText, dobjIsPronoun, topicText);

        /* use the direct object as the antecedent for the next phrase */
        ctx.setPronounObj(dobj);

        /* return the result */
        return ret;
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Verbs.
 *
 *   The actual body of each of our verbs is defined in the main
 *   language-independent part of the library.  We only define the
 *   language-specific grammar rules here.
 */

VerbRule(Take)
    ('take' | 'pick' 'up' | 'get') dobjList
    | 'pick' dobjList 'up'
    : TakeAction
    verbPhrase = 'take/taking (what)'
;

VerbRule(TakeFrom)
    ('take' | 'get') dobjList
        ('from' | 'out' 'of' | 'off' | 'off' 'of') singleIobj
    | 'remove' dobjList 'from' singleIobj
    : TakeFromAction
    verbPhrase = 'take/taking (what) (from what)'
;

VerbRule(Remove)
    'remove' dobjList
    : RemoveAction
    verbPhrase = 'remove/removing (what)'
;

VerbRule(Drop)
    ('drop' | 'put' 'down' | 'set' 'down') dobjList
    | ('put' | 'set') dobjList 'down'
    : DropAction
    verbPhrase = 'drop/dropping (what)'
;

VerbRule(Examine)
    ('examine' | 'inspect' | 'x'
     | 'look' 'at' | 'l' 'at' | 'look' | 'l') dobjList
    : ExamineAction
    verbPhrase = 'examine/examining (what)'
;

VerbRule(Read)
    'read' dobjList
    : ReadAction
    verbPhrase = 'read/reading (what)'
;

VerbRule(LookIn)
    ('look' | 'l') ('in' | 'inside') dobjList
    : LookInAction
    verbPhrase = 'look/looking (in what)'
;

VerbRule(Search)
    'search' dobjList
    : SearchAction
    verbPhrase = 'search/searching (what)'
;

VerbRule(LookThrough)
    ('look' | 'l') ('through' | 'thru' | 'out') dobjList
    : LookThroughAction
    verbPhrase = 'look/looking (through what)'
;

VerbRule(LookUnder)
    ('look' | 'l') 'under' dobjList
    : LookUnderAction
    verbPhrase = 'look/looking (under what)'
;

VerbRule(LookBehind)
    ('look' | 'l') 'behind' dobjList
    : LookBehindAction
    verbPhrase = 'look/looking (behind what)'
;

VerbRule(Feel)
    ('feel' | 'touch') dobjList
    : FeelAction
    verbPhrase = 'touch/touching (what)'
;

VerbRule(Taste)
    'taste' dobjList
    : TasteAction
    verbPhrase = 'taste/tasting (what)'
;

VerbRule(Smell)
    ('smell' | 'sniff') dobjList
    : SmellAction
    verbPhrase = 'smell/smelling (what)'

    /*
     *   use the "not aware" version of the no-match message - the object
     *   of SMELL is often intangible, so the default "you can't see that"
     *   message is often incongruous for this verb
     */
    noMatch(msgObj, actor, txt) { msgObj.noMatchNotAware(actor, txt); }
;

VerbRule(SmellImplicit)
    'smell' | 'sniff'
    : SmellImplicitAction
    verbPhrase = 'smell/smelling'
;

VerbRule(ListenTo)
    ('hear' | 'listen' 'to' ) dobjList
    : ListenToAction
    verbPhrase = 'listen/listening (to what)'

    /*
     *   use the "not aware" version of the no-match message - the object
     *   of LISTEN TO is often intangible, so the default "you can't see
     *   that" message is often incongruous for this verb
     */
    noMatch(msgObj, actor, txt) { msgObj.noMatchNotAware(actor, txt); }
;

VerbRule(ListenImplicit)
    'listen' | 'hear'
    : ListenImplicitAction
    verbPhrase = 'listen/listening'
;

VerbRule(PutIn)
    ('put' | 'place' | 'set') dobjList
        ('in' | 'into' | 'in' 'to' | 'inside' | 'inside' 'of') singleIobj
    : PutInAction
    verbPhrase = 'put/putting (what) (in what)'
    askIobjResponseProd = inSingleNoun
;

VerbRule(PutOn)
    ('put' | 'place' | 'drop' | 'set') dobjList
        ('on' | 'onto' | 'on' 'to' | 'upon') singleIobj
    | 'put' dobjList 'down' 'on' singleIobj
    : PutOnAction
    verbPhrase = 'put/putting (what) (on what)'
    askIobjResponseProd = onSingleNoun
;

VerbRule(PutUnder)
    ('put' | 'place' | 'set') dobjList 'under' singleIobj
    : PutUnderAction
    verbPhrase = 'put/putting (what) (under what)'
;

VerbRule(PutBehind)
    ('put' | 'place' | 'set') dobjList 'behind' singleIobj
    : PutBehindAction
    verbPhrase = 'put/putting (what) (behind what)'
;

VerbRule(PutInWhat)
    [badness 500] ('put' | 'place') dobjList
    : PutInAction
    verbPhrase = 'put/putting (what) (in what)'
    construct()
    {
        /* set up the empty indirect object phrase */
        iobjMatch = new EmptyNounPhraseProd();
        iobjMatch.responseProd = inSingleNoun;
    }
;

VerbRule(Wear)
    ('wear' | 'don' | 'put' 'on') dobjList
    | 'put' dobjList 'on'
    : WearAction
    verbPhrase = 'wear/wearing (what)'
;

VerbRule(Doff)
    ('doff' | 'take' 'off') dobjList
    | 'take' dobjList 'off'
    : DoffAction
    verbPhrase = 'take/taking off (what)'
;

VerbRule(Kiss)
    'kiss' singleDobj
    : KissAction
    verbPhrase = 'kiss/kissing (whom)'
;

VerbRule(AskFor)
    ('ask' | 'a') singleDobj 'for' singleTopic
    | ('ask' | 'a') 'for' singleTopic 'from' singleDobj
    : AskForAction
    verbPhrase = 'ask/asking (whom) (for what)'
    omitIobjInDobjQuery = true
    askDobjResponseProd = singleNoun
    askIobjResponseProd = forSingleNoun
;

VerbRule(AskWhomFor)
    ('ask' | 'a') 'for' singleTopic
    : AskForAction
    verbPhrase = 'ask/asking (whom) (for what)'
    omitIobjInDobjQuery = true
    construct()
    {
        /* set up the empty direct object phrase */
        dobjMatch = new EmptyNounPhraseProd();
        dobjMatch.responseProd = singleNoun;
    }
;

VerbRule(AskAbout)
    ('ask' | 'a') singleDobj 'about' singleTopic
    : AskAboutAction
    verbPhrase = 'ask/asking (whom) (about what)'
    omitIobjInDobjQuery = true
    askDobjResponseProd = singleNoun
;

VerbRule(AskAboutImplicit)
    'a' singleTopic
    : AskAboutAction
    verbPhrase = 'ask/asking (whom) (about what)'
    omitIobjInDobjQuery = true
    construct()
    {
        /* set up the empty direct object phrase */
        dobjMatch = new EmptyNounPhraseProd();
        dobjMatch.responseProd = singleNoun;
    }
;

VerbRule(AskAboutWhat)
    [badness 500] 'ask' singleDobj
    : AskAboutAction
    verbPhrase = 'ask/asking (whom) (about what)'
    askDobjResponseProd = singleNoun
    omitIobjInDobjQuery = true
    construct()
    {
        /* set up the empty topic phrase */
        topicMatch = new EmptyNounPhraseProd();
        topicMatch.responseProd = aboutTopicPhrase;
    }
;


VerbRule(TellAbout)
    ('tell' | 't') singleDobj 'about' singleTopic
    : TellAboutAction
    verbPhrase = 'tell/telling (whom) (about what)'
    askDobjResponseProd = singleNoun
    omitIobjInDobjQuery = true
;

VerbRule(TellAboutImplicit)
    't' singleTopic
    : TellAboutAction
    verbPhrase = 'tell/telling (whom) (about what)'
    omitIobjInDobjQuery = true
    construct()
    {
        /* set up the empty direct object phrase */
        dobjMatch = new EmptyNounPhraseProd();
        dobjMatch.responseProd = singleNoun;
    }
;

VerbRule(TellAboutWhat)
    [badness 500] 'tell' singleDobj
    : TellAboutAction
    verbPhrase = 'tell/telling (whom) (about what)'
    askDobjResponseProd = singleNoun
    omitIobjInDobjQuery = true
    construct()
    {
        /* set up the empty topic phrase */
        topicMatch = new EmptyNounPhraseProd();
        topicMatch.responseProd = aboutTopicPhrase;
    }
;

VerbRule(AskVague)
    [badness 500] 'ask' singleDobj singleTopic
    : AskVagueAction
    verbPhrase = 'ask/asking (whom)'
;

VerbRule(TellVague)
    [badness 500] 'tell' singleDobj singleTopic
    : AskVagueAction
    verbPhrase = 'tell/telling (whom)'
;

VerbRule(TalkTo)
    ('greet' | 'say' 'hello' 'to' | 'talk' 'to') singleDobj
    : TalkToAction
    verbPhrase = 'talk/talking (to whom)'
    askDobjResponseProd = singleNoun
;

VerbRule(TalkToWhat)
    [badness 500] 'talk'
    : TalkToAction
    verbPhrase = 'talk/talking (to whom)'
    askDobjResponseProd = singleNoun

    construct()
    {
        /* set up the empty direct object phrase */
        dobjMatch = new EmptyNounPhraseProd();
        dobjMatch.responseProd = onSingleNoun;
    }
;

VerbRule(Topics)
    'topics'
    : TopicsAction
    verbPhrase = 'show/showing topics'
;

VerbRule(Hello)
    ('say' | ) ('hello' | 'hallo' | 'hi')
    : HelloAction
    verbPhrase = 'say/saying hello'
;

VerbRule(Goodbye)
    ('say' | ()) ('goodbye' | 'good-bye' | 'good' 'bye' | 'bye')
    : GoodbyeAction
    verbPhrase = 'say/saying goodbye'
;

VerbRule(Yes)
    'yes' | 'affirmative' | 'say' 'yes'
    : YesAction
    verbPhrase = 'say/saying yes'
;

VerbRule(No)
    'no' | 'negative' | 'say' 'no'
    : NoAction
    verbPhrase = 'say/saying no'
;

VerbRule(Yell)
    'yell' | 'scream' | 'shout' | 'holler'
    : YellAction
    verbPhrase = 'yell/yelling'
;

VerbRule(GiveTo)
    ('give' | 'offer') dobjList 'to' singleIobj
    : GiveToAction
    verbPhrase = 'give/giving (what) (to whom)'
    askIobjResponseProd = toSingleNoun
;

VerbRule(GiveToType2)
    ('give' | 'offer') singleIobj dobjList
    : GiveToAction
    verbPhrase = 'give/giving (what) (to whom)'
    askIobjResponseProd = toSingleNoun

    /* this is a non-prepositional phrasing */
    isPrepositionalPhrasing = nil
;

VerbRule(GiveToWhom)
    ('give' | 'offer') dobjList
    : GiveToAction
    verbPhrase = 'give/giving (what) (to whom)'
    construct()
    {
        /* set up the empty indirect object phrase */
        iobjMatch = new ImpliedActorNounPhraseProd();
        iobjMatch.responseProd = toSingleNoun;
    }
;

VerbRule(ShowTo)
    'show' dobjList 'to' singleIobj
    : ShowToAction
    verbPhrase = 'show/showing (what) (to whom)'
    askIobjResponseProd = toSingleNoun
;

VerbRule(ShowToType2)
    'show' singleIobj dobjList
    : ShowToAction
    verbPhrase = 'show/showing (what) (to whom)'
    askIobjResponseProd = toSingleNoun

    /* this is a non-prepositional phrasing */
    isPrepositionalPhrasing = nil
;

VerbRule(ShowToWhom)
    'show' dobjList
    : ShowToAction
    verbPhrase = 'show/showing (what) (to whom)'
    construct()
    {
        /* set up the empty indirect object phrase */
        iobjMatch = new ImpliedActorNounPhraseProd();
        iobjMatch.responseProd = toSingleNoun;
    }
;

VerbRule(Throw)
    ('throw' | 'toss') dobjList
    : ThrowAction
    verbPhrase = 'throw/throwing (what)'
;

VerbRule(ThrowAt)
    ('throw' | 'toss') dobjList 'at' singleIobj
    : ThrowAtAction
    verbPhrase = 'throw/throwing (what) (at what)'
    askIobjResponseProd = atSingleNoun
;

VerbRule(ThrowTo)
    ('throw' | 'toss') dobjList 'to' singleIobj
    : ThrowToAction
    verbPhrase = 'throw/throwing (what) (to whom)'
    askIobjResponseProd = toSingleNoun
;

VerbRule(ThrowToType2)
    'throw' singleIobj dobjList
    : ThrowToAction
    verbPhrase = 'throw/throwing (what) (to whom)'
    askIobjResponseProd = toSingleNoun

    /* this is a non-prepositional phrasing */
    isPrepositionalPhrasing = nil
;

VerbRule(ThrowDir)
    ('throw' | 'toss') dobjList ('to' ('the' | ) | ) singleDir
    : ThrowDirAction
    verbPhrase = ('throw/throwing (what) ' + dirMatch.dir.name)
;

/* a special rule for THROW DOWN <dobj> */
VerbRule(ThrowDirDown)
    'throw' ('down' | 'd') dobjList
    : ThrowDirAction
    verbPhrase = ('throw/throwing (what) down')

    /* the direction is fixed as 'down' for this phrasing */
    getDirection() { return downDirection; }
;

VerbRule(Follow)
    'follow' singleDobj
    : FollowAction
    verbPhrase = 'follow/following (whom)'
    askDobjResponseProd = singleNoun
;

VerbRule(Attack)
    ('attack' | 'kill' | 'hit' | 'kick' | 'punch') singleDobj
    : AttackAction
    verbPhrase = 'attack/attacking (whom)'
    askDobjResponseProd = singleNoun
;

VerbRule(AttackWith)
    ('attack' | 'kill' | 'hit' | 'kick' | 'punch' | 'strike')
        singleDobj
        'with' singleIobj
    : AttackWithAction
    verbPhrase = 'attack/attacking (whom) (with what)'
    askDobjResponseProd = singleNoun
    askIobjResponseProd = withSingleNoun
;

VerbRule(Inventory)
    'i' | 'inventory' | 'take' 'inventory'
    : InventoryAction
    verbPhrase = 'take/taking inventory'
;

VerbRule(InventoryTall)
    'i' 'tall' | 'inventory' 'tall'
    : InventoryTallAction
    verbPhrase = 'take/taking "tall" inventory'
;

VerbRule(InventoryWide)
    'i' 'wide' | 'inventory' 'wide'
    : InventoryWideAction
    verbPhrase = 'take/taking "wide" inventory'
;

VerbRule(Wait)
    'z' | 'wait'
    : WaitAction
    verbPhrase = 'wait/waiting'
;

VerbRule(Look)
    'look' | 'look' 'around' | 'l' | 'l' 'around'
    : LookAction
    verbPhrase = 'look/looking around'
;

VerbRule(Quit)
    'quit' | 'q'
    : QuitAction
    verbPhrase = 'quit/quitting'
;

VerbRule(Again)
    'again' | 'g'
    : AgainAction
    verbPhrase = 'repeat/repeating the last command'
;

VerbRule(Footnote)
    ('footnote' | 'note') singleNumber
    : FootnoteAction
    verbPhrase = 'show/showing a footnote'
;

VerbRule(FootnotesFull)
    'footnotes' 'full'
    : FootnotesFullAction
    verbPhrase = 'enable/enabling all footnotes'
;

VerbRule(FootnotesMedium)
    'footnotes' 'medium'
    : FootnotesMediumAction
    verbPhrase = 'enable/enabling new footnotes'
;

VerbRule(FootnotesOff)
    'footnotes' 'off'
    : FootnotesOffAction
    verbPhrase = 'hide/hiding footnotes'
;

VerbRule(FootnotesStatus)
    'footnotes'
    : FootnotesStatusAction
    verbPhrase = 'show/showing footnote status'
;

VerbRule(TipsOn)
    ('tips' | 'tip') 'on'
    : TipModeAction

    stat_ = true

    verbPhrase = 'turn/turning tips on'
;

VerbRule(TipsOff)
    ('tips' | 'tip') 'off'
    : TipModeAction

    stat_ = nil

    verbPhrase = 'turn/turning tips off'
;

VerbRule(Verbose)
    'verbose'
    : VerboseAction
    verbPhrase = 'enter/entering VERBOSE mode'
;

VerbRule(Terse)
    'terse' | 'brief'
    : TerseAction
    verbPhrase = 'enter/entering BRIEF mode'
;

VerbRule(Score)
    'score' | 'status'
    : ScoreAction
    verbPhrase = 'show/showing score'
;

VerbRule(FullScore)
    'full' 'score' | 'fullscore' | 'full'
    : FullScoreAction
    verbPhrase = 'show/showing full score'
;

VerbRule(Notify)
    'notify'
    : NotifyAction
    verbPhrase = 'show/showing notification status'
;

VerbRule(NotifyOn)
    'notify' 'on'
    : NotifyOnAction
    verbPhrase = 'turn/turning on score notification'
;

VerbRule(NotifyOff)
    'notify' 'off'
    : NotifyOffAction
    verbPhrase = 'turn/turning off score notification'
;

VerbRule(Save)
    'save'
    : SaveAction
    verbPhrase = 'save/saving'
;

VerbRule(SaveString)
    'save' quotedStringPhrase->fname_
    : SaveStringAction
    verbPhrase = 'save/saving'
;

VerbRule(Restore)
    'restore'
    : RestoreAction
    verbPhrase = 'restore/restoring'
;

VerbRule(RestoreString)
    'restore' quotedStringPhrase->fname_
    : RestoreStringAction
    verbPhrase = 'restore/restoring'
;

VerbRule(SaveDefaults)
    'save' 'defaults'
    : SaveDefaultsAction
    verbPhrase = 'save/saving defaults'
;

VerbRule(RestoreDefaults)
    'restore' 'defaults'
    : RestoreDefaultsAction
    verbPhrase = 'restore/restoring defaults'
;

VerbRule(Restart)
    'restart'
    : RestartAction
    verbPhrase = 'restart/restarting'
;

VerbRule(Pause)
    'pause'
    : PauseAction
    verbPhrase = 'pause/pausing'
;

VerbRule(Undo)
    'undo'
    : UndoAction
    verbPhrase = 'undo/undoing'
;

VerbRule(Version)
    'version'
    : VersionAction
    verbPhrase = 'show/showing version'
;

VerbRule(Credits)
    'credits'
    : CreditsAction
    verbPhrase = 'show/showing credits'
;

VerbRule(About)
    'about'
    : AboutAction
    verbPhrase = 'show/showing story information'
;

VerbRule(Script)
    'script' | 'script' 'on'
    : ScriptAction
    verbPhrase = 'start/starting scripting'
;

VerbRule(ScriptString)
    'script' quotedStringPhrase->fname_
    : ScriptStringAction
    verbPhrase = 'start/starting scripting'
;

VerbRule(ScriptOff)
    'script' 'off' | 'unscript'
    : ScriptOffAction
    verbPhrase = 'end/ending scripting'
;

VerbRule(Record)
    'record' | 'record' 'on'
    : RecordAction
    verbPhrase = 'start/starting command recording'
;

VerbRule(RecordString)
    'record' quotedStringPhrase->fname_
    : RecordStringAction
    verbPhrase = 'start/starting command recording'
;

VerbRule(RecordEvents)
    'record' 'events' | 'record' 'events' 'on'
    : RecordEventsAction
    verbPhrase = 'start/starting event recording'
;

VerbRule(RecordEventsString)
    'record' 'events' quotedStringPhrase->fname_
    : RecordEventsStringAction
    verbPhrase = 'start/starting command recording'
;

VerbRule(RecordOff)
    'record' 'off'
    : RecordOffAction
    verbPhrase = 'end/ending command recording'
;

VerbRule(ReplayString)
    'replay' ('quiet'->quiet_ | 'nonstop'->nonstop_ | )
        (quotedStringPhrase->fname_ | )
    : ReplayStringAction
    verbPhrase = 'replay/replaying command recording'

    /* set the appropriate option flags */
    scriptOptionFlags = ((quiet_ != nil ? ScriptFileQuiet : 0)
                         | (nonstop_ != nil ? ScriptFileNonstop : 0))
;
VerbRule(ReplayQuiet)
    'rq' (quotedStringPhrase->fname_ | )
    : ReplayStringAction

    scriptOptionFlags = ScriptFileQuiet
;

VerbRule(VagueTravel) 'go' | 'walk' : VagueTravelAction
    verbPhrase = 'go/going'
;

VerbRule(Travel)
    'go' singleDir | singleDir
    : TravelAction
    verbPhrase = ('go/going ' + dirMatch.dir.name)
;

/*
 *   Create a TravelVia subclass merely so we can supply a verbPhrase.
 *   (The parser looks for subclasses of each specific Action class to find
 *   its verb phrase, since the language-specific Action definitions are
 *   always in the language module's 'grammar' subclasses.  We don't need
 *   an actual grammar rule, since this isn't an input-able verb, so we
 *   merely need to create a regular subclass in order for the verbPhrase
 *   to get found.)  
 */
class EnTravelVia: TravelViaAction
    verbPhrase = 'use/using (what)'
;

VerbRule(Port)
    'go' 'to' ('port' | 'p')
    : PortAction
    dirMatch: DirectionProd { dir = portDirection }
    verbPhrase = 'go/going to port'
;

VerbRule(Starboard)
    'go' 'to' ('starboard' | 'sb')
    : StarboardAction
    dirMatch: DirectionProd { dir = starboardDirection }
    verbPhrase = 'go/going to starboard'
;

VerbRule(In)
    'enter'
    : InAction
    dirMatch: DirectionProd { dir = inDirection }
    verbPhrase = 'enter/entering'
;

VerbRule(Out)
    'exit' | 'leave'
    : OutAction
    dirMatch: DirectionProd { dir = outDirection }
    verbPhrase = 'exit/exiting'
;

VerbRule(GoThrough)
    ('walk' | 'go' ) ('through' | 'thru')
        singleDobj
    : GoThroughAction
    verbPhrase = 'go/going (through what)'
    askDobjResponseProd = singleNoun
;

VerbRule(Enter)
    ('enter' | 'in' | 'into' | 'in' 'to'
     | ('walk' | 'go') ('to' | 'in' | 'in' 'to' | 'into'))
    singleDobj
    : EnterAction
    verbPhrase = 'enter/entering (what)'
    askDobjResponseProd = singleNoun
;

VerbRule(GoBack)
    'back' | 'go' 'back' | 'return'
    : GoBackAction
    verbPhrase = 'go/going back'
;

VerbRule(Dig)
    ('dig' | 'dig' 'in') singleDobj
    : DigAction
    verbPhrase = 'dig/digging (in what)'
    askDobjResponseProd = inSingleNoun
;

VerbRule(DigWith)
    ('dig' | 'dig' 'in') singleDobj 'with' singleIobj
    : DigWithAction
    verbPhrase = 'dig/digging (in what) (with what)'
    omitIobjInDobjQuery = true
    askDobjResponseProd = inSingleNoun
    askIobjResponseProd = withSingleNoun
;

VerbRule(Jump)
    'jump'
    : JumpAction
    verbPhrase = 'jump/jumping'
;

VerbRule(JumpOffI)
    'jump' 'off'
    : JumpOffIAction
    verbPhrase = 'jump/jumping off'
;

VerbRule(JumpOff)
    'jump' 'off' singleDobj
    : JumpOffAction
    verbPhrase = 'jump/jumping (off what)'
    askDobjResponseProd = singleNoun
;

VerbRule(JumpOver)
    ('jump' | 'jump' 'over') singleDobj
    : JumpOverAction
    verbPhrase = 'jump/jumping (over what)'
    askDobjResponseProd = singleNoun
;

VerbRule(Push)
    ('push' | 'press') dobjList
    : PushAction
    verbPhrase = 'push/pushing (what)'
;

VerbRule(Pull)
    'pull' dobjList
    : PullAction
    verbPhrase = 'pull/pulling (what)'
;

VerbRule(Move)
    'move' dobjList
    : MoveAction
    verbPhrase = 'move/moving (what)'
;

VerbRule(MoveTo)
    ('push' | 'move') dobjList ('to' | 'under') singleIobj
    : MoveToAction
    verbPhrase = 'move/moving (what) (to what)'
    askIobjResponseProd = toSingleNoun
    omitIobjInDobjQuery = true
;

VerbRule(MoveWith)
    'move' singleDobj 'with' singleIobj
    : MoveWithAction
    verbPhrase = 'move/moving (what) (with what)'
    askDobjResponseProd = singleNoun
    askIobjResponseProd = withSingleNoun
    omitIobjInDobjQuery = true
;

VerbRule(Turn)
    ('turn' | 'twist' | 'rotate') dobjList
    : TurnAction
    verbPhrase = 'turn/turning (what)'
;

VerbRule(TurnWith)
    ('turn' | 'twist' | 'rotate') singleDobj 'with' singleIobj
    : TurnWithAction
    verbPhrase = 'turn/turning (what) (with what)'
    askDobjResponseProd = singleNoun
    askIobjResponseProd = withSingleNoun
;

VerbRule(TurnTo)
    ('turn' | 'twist' | 'rotate') singleDobj
        'to' singleLiteral
    : TurnToAction
    verbPhrase = 'turn/turning (what) (to what)'
    askDobjResponseProd = singleNoun
    omitIobjInDobjQuery = true
;

VerbRule(Set)
    'set' dobjList
    : SetAction
    verbPhrase = 'set/setting (what)'
;

VerbRule(SetTo)
    'set' singleDobj 'to' singleLiteral
    : SetToAction
    verbPhrase = 'set/setting (what) (to what)'
    askDobjResponseProd = singleNoun
    omitIobjInDobjQuery = true
;

VerbRule(TypeOn)
    'type' 'on' singleDobj
    : TypeOnAction
    verbPhrase = 'type/typing (on what)'
;

VerbRule(TypeLiteralOn)
    'type' singleLiteral 'on' singleDobj
    : TypeLiteralOnAction
    verbPhrase = 'type/typing (what) (on what)'
    askDobjResponseProd = singleNoun
;

VerbRule(TypeLiteralOnWhat)
    [badness 500] 'type' singleLiteral
    : TypeLiteralOnAction
    verbPhrase = 'type/typing (what) (on what)'
    construct()
    {
        /* set up the empty direct object phrase */
        dobjMatch = new EmptyNounPhraseProd();
        dobjMatch.responseProd = onSingleNoun;
    }
;

VerbRule(EnterOn)
    'enter' singleLiteral
        ('on' | 'in' | 'in' 'to' | 'into' | 'with') singleDobj
    : EnterOnAction
    verbPhrase = 'enter/entering (what) (on what)'
    askDobjResponseProd = singleNoun
;

VerbRule(EnterOnWhat)
    'enter' singleLiteral
    : EnterOnAction
    verbPhrase = 'enter/entering (what) (on what)'
    construct()
    {
        /*
         *   ENTER <text> is a little special, because it could mean ENTER
         *   <text> ON <keypad>, or it could mean GO INTO <object>.  It's
         *   hard to tell which based on the grammar alone, so we have to
         *   do some semantic analysis to make a good decision about it.
         *
         *   We'll start by assuming it's the ENTER <text> ON <iobj> form
         *   of the command, and we'll look for a suitable default object
         *   to serve as the iobj.  If we can't find a suitable default, we
         *   won't prompt for the missing object as we usually would.
         *   Instead, we'll try re-parsing the command as GO INTO.  To do
         *   this, use our custom "asker" - this won't actually prompt for
         *   the missing object, but will instead retry the command as a GO
         *   INTO command.
         */
        dobjMatch = new EmptyNounPhraseProd();
        dobjMatch.setPrompt(onSingleNoun, enterOnWhatAsker);
    }
;

/* our custom "asker" for the missing iobj in an "ENTER <text>" command */
enterOnWhatAsker: ResolveAsker
    askMissingObject(targetActor, action, which)
    {
        /*
         *   This method is called when the resolver has failed to find a
         *   suitable default for the missing indirect object of ENTER
         *   <text> ON <iobj>.
         *
         *   Instead of issuing the prompt that we'd normally issue under
         *   these circumstances, assume that we're totally wrong about the
         *   way we've been interpreting the command: assume that it's not
         *   meant as ENTER <text> ON <iobj> after all, but was actually
         *   meant as GO IN <object>.  So, rephrase the command as such and
         *   start over with the new phrasing.
         */
        throw new ReplacementCommandStringException(
            'get in ' + action.getLiteral(), gIssuingActor, gActor);
    }
;

VerbRule(Consult)
    'consult' singleDobj : ConsultAction
    verbPhrase = 'consult/consulting (what)'
    askDobjResponseProd = singleNoun
;

VerbRule(ConsultAbout)
    'consult' singleDobj ('on' | 'about') singleTopic
    | 'search' singleDobj 'for' singleTopic
    | (('look' | 'l') ('up' | 'for')
       | 'find'
       | 'search' 'for'
       | 'read' 'about')
         singleTopic 'in' singleDobj
    | ('look' | 'l') singleTopic 'up' 'in' singleDobj
    : ConsultAboutAction
    verbPhrase = 'consult/consulting (what) (about what)'
    omitIobjInDobjQuery = true
    askDobjResponseProd = singleNoun
;

VerbRule(ConsultWhatAbout)
    (('look' | 'l') ('up' | 'for')
     | 'find'
     | 'search' 'for'
     | 'read' 'about')
    singleTopic
    | ('look' | 'l') singleTopic 'up'
    : ConsultAboutAction
    verbPhrase = 'look/looking up (what) (in what)'
    whichMessageTopic = DirectObject
    construct()
    {
        /* set up the empty direct object phrase */
        dobjMatch = new EmptyNounPhraseProd();
        dobjMatch.responseProd = inSingleNoun;
    }
;

VerbRule(Switch)
    'switch' dobjList
    : SwitchAction
    verbPhrase = 'switch/switching (what)'
;

VerbRule(Flip)
    'flip' dobjList
    : FlipAction
    verbPhrase = 'flip/flipping (what)'
;

VerbRule(TurnOn)
    ('activate' | ('turn' | 'switch') 'on') dobjList
    | ('turn' | 'switch') dobjList 'on'
    : TurnOnAction
    verbPhrase = 'turn/turning on (what)'
;

VerbRule(TurnOff)
    ('deactivate' | ('turn' | 'switch') 'off') dobjList
    | ('turn' | 'switch') dobjList 'off'
    : TurnOffAction
    verbPhrase = 'turn/turning off (what)'
;

VerbRule(Light)
    'light' dobjList
    : LightAction
    verbPhrase = 'light/lighting (what)'
;

DefineTAction(Strike);
VerbRule(Strike)
    'strike' dobjList
    : StrikeAction
    verbPhrase = 'strike/striking (what)'
;

VerbRule(Burn)
    ('burn' | 'ignite' | 'set' 'fire' 'to') dobjList
    : BurnAction
    verbPhrase = 'light/lighting (what)'
;

VerbRule(BurnWith)
    ('light' | 'burn' | 'ignite' | 'set' 'fire' 'to') singleDobj
        'with' singleIobj
    : BurnWithAction
    verbPhrase = 'light/lighting (what) (with what)'
    omitIobjInDobjQuery = true
    askDobjResponseProd = singleNoun
    askIobjResponseProd = withSingleNoun
;

VerbRule(Extinguish)
    ('extinguish' | 'douse' | 'put' 'out' | 'blow' 'out') dobjList
    | ('blow' | 'put') dobjList 'out'
    : ExtinguishAction
    verbPhrase = 'extinguish/extinguishing (what)'
;

VerbRule(Break)
    ('break' | 'ruin' | 'destroy' | 'wreck') dobjList
    : BreakAction
    verbPhrase = 'break/breaking (what)'
;

VerbRule(CutWithWhat)
    [badness 500] 'cut' singleDobj
    : CutWithAction
    verbPhrase = 'cut/cutting (what) (with what)'
    construct()
    {
        /* set up the empty indirect object phrase */
        iobjMatch = new EmptyNounPhraseProd();
        iobjMatch.responseProd = withSingleNoun;
    }
;

VerbRule(CutWith)
    'cut' singleDobj 'with' singleIobj
    : CutWithAction
    verbPhrase = 'cut/cutting (what) (with what)'
    askDobjResponseProd = singleNoun
    askIobjResponseProd = withSingleNoun
;

VerbRule(Eat)
    ('eat' | 'consume') dobjList
    : EatAction
    verbPhrase = 'eat/eating (what)'
;

VerbRule(Drink)
    ('drink' | 'quaff' | 'imbibe') dobjList
    : DrinkAction
    verbPhrase = 'drink/drinking (what)'
;

VerbRule(Pour)
    'pour' dobjList
    : PourAction
    verbPhrase = 'pour/pouring (what)'
;

VerbRule(PourInto)
    'pour' dobjList ('in' | 'into' | 'in' 'to') singleIobj
    : PourIntoAction
    verbPhrase = 'pour/pouring (what) (into what)'
    askIobjResponseProd = inSingleNoun
;

VerbRule(PourOnto)
    'pour' dobjList ('on' | 'onto' | 'on' 'to') singleIobj
    : PourOntoAction
    verbPhrase = 'pour/pouring (what) (onto what)'
    askIobjResponseProd = onSingleNoun
;

VerbRule(Climb)
    'climb' singleDobj
    : ClimbAction
    verbPhrase = 'climb/climbing (what)'
    askDobjResponseProd = singleNoun
;

VerbRule(ClimbUp)
    ('climb' | 'go' | 'walk') 'up' singleDobj
    : ClimbUpAction
    verbPhrase = 'climb/climbing (up what)'
    askDobjResponseProd = singleNoun
;

VerbRule(ClimbUpWhat)
    [badness 200] ('climb' | 'go' | 'walk') 'up'
    : ClimbUpAction
    verbPhrase = 'climb/climbing (up what)'
    askDobjResponseProd = singleNoun
    construct()
    {
        dobjMatch = new EmptyNounPhraseProd();
        dobjMatch.responseProd = onSingleNoun;
    }
;

VerbRule(ClimbDown)
    ('climb' | 'go' | 'walk') 'down' singleDobj
    : ClimbDownAction
    verbPhrase = 'climb/climbing (down what)'
    askDobjResponseProd = singleNoun
;

VerbRule(ClimbDownWhat)
    [badness 200] ('climb' | 'go' | 'walk') 'down'
    : ClimbDownAction
    verbPhrase = 'climb/climbing (down what)'
    askDobjResponseProd = singleNoun
    construct()
    {
        dobjMatch = new EmptyNounPhraseProd();
        dobjMatch.responseProd = onSingleNoun;
    }
;

VerbRule(Clean)
    'clean' dobjList
    : CleanAction
    verbPhrase = 'clean/cleaning (what)'
;

VerbRule(CleanWith)
    'clean' dobjList 'with' singleIobj
    : CleanWithAction
    verbPhrase = 'clean/cleaning (what) (with what)'
    askIobjResponseProd = withSingleNoun
    omitIobjInDobjQuery = true
;

VerbRule(AttachTo)
    ('attach' | 'connect') dobjList 'to' singleIobj
    : AttachToAction
    askIobjResponseProd = toSingleNoun
    verbPhrase = 'attach/attaching (what) (to what)'
;

VerbRule(AttachToWhat)
    [badness 500] ('attach' | 'connect') dobjList
    : AttachToAction
    verbPhrase = 'attach/attaching (what) (to what)'
    construct()
    {
        /* set up the empty indirect object phrase */
        iobjMatch = new EmptyNounPhraseProd();
        iobjMatch.responseProd = toSingleNoun;
    }
;

VerbRule(DetachFrom)
    ('detach' | 'disconnect') dobjList 'from' singleIobj
    : DetachFromAction
    verbPhrase = 'detach/detaching (what) (from what)'
    askIobjResponseProd = fromSingleNoun
;

VerbRule(Detach)
    ('detach' | 'disconnect') dobjList
    : DetachAction
    verbPhrase = 'detach/detaching (what)'
;

VerbRule(Open)
    'open' dobjList
    : OpenAction
    verbPhrase = 'open/opening (what)'
;

VerbRule(Close)
    ('close' | 'shut') dobjList
    : CloseAction
    verbPhrase = 'close/closing (what)'
;

VerbRule(Lock)
    'lock' dobjList
    : LockAction
    verbPhrase = 'lock/locking (what)'
;

VerbRule(Unlock)
    'unlock' dobjList
    : UnlockAction
    verbPhrase = 'unlock/unlocking (what)'
;

VerbRule(LockWith)
    'lock' singleDobj 'with' singleIobj
    : LockWithAction
    verbPhrase = 'lock/locking (what) (with what)'
    omitIobjInDobjQuery = true
    askDobjResponseProd = singleNoun
    askIobjResponseProd = withSingleNoun
;

VerbRule(UnlockWith)
    'unlock' singleDobj 'with' singleIobj
    : UnlockWithAction
    verbPhrase = 'unlock/unlocking (what) (with what)'
    omitIobjInDobjQuery = true
    askDobjResponseProd = singleNoun
    askIobjResponseProd = withSingleNoun
;

VerbRule(SitOn)
    'sit' ('on' | 'in' | 'down' 'on' | 'down' 'in')
        singleDobj
    : SitOnAction
    verbPhrase = 'sit/sitting (on what)'
    askDobjResponseProd = singleNoun

    /* use the actorInPrep, if there's a direct object available */
    adjustDefaultObjectPrep(prep, obj)
        { return (obj != nil ? obj.actorInPrep + ' ' : prep); }
;

VerbRule(Sit)
    'sit' ( | 'down') : SitAction
    verbPhrase = 'sit/sitting down'
;

VerbRule(LieOn)
    'lie' ('on' | 'in' | 'down' 'on' | 'down' 'in')
        singleDobj
    : LieOnAction
    verbPhrase = 'lie/lying (on what)'
    askDobjResponseProd = singleNoun

    /* use the actorInPrep, if there's a direct object available */
    adjustDefaultObjectPrep(prep, obj)
        { return (obj != nil ? obj.actorInPrep + ' ' : prep); }
;

VerbRule(Lie)
    'lie' ( | 'down') : LieAction
    verbPhrase = 'lie/lying down'
;

VerbRule(StandOn)
    ('stand' ('on' | 'in' | 'onto' | 'on' 'to' | 'into' | 'in' 'to')
     | 'climb' ('on' | 'onto' | 'on' 'to'))
    singleDobj
    : StandOnAction
    verbPhrase = 'stand/standing (on what)'
    askDobjResponseProd = singleNoun

    /* use the actorInPrep, if there's a direct object available */
    adjustDefaultObjectPrep(prep, obj)
        { return (obj != nil ? obj.actorInPrep + ' ' : prep); }
;

VerbRule(Stand)
    'stand' | 'stand' 'up' | 'get' 'up'
    : StandAction
    verbPhrase = 'stand/standing up'
;

VerbRule(GetOutOf)
    ('out' 'of' | 'get' 'out' 'of' | 'climb' 'out' 'of' | 'leave' | 'exit')
    singleDobj
    : GetOutOfAction
    verbPhrase = 'get/getting (out of what)'
    askDobjResponseProd = singleNoun

    /* use the actorOutOfPrep, if there's a direct object available */
    adjustDefaultObjectPrep(prep, obj)
        { return (obj != nil ? obj.actorOutOfPrep + ' ' : prep); }
;

VerbRule(GetOffOf)
    'get' ('off' | 'off' 'of' | 'down' 'from') singleDobj
    : GetOffOfAction
    verbPhrase = 'get/getting (off of what)'
    askDobjResponseProd = singleNoun

    /* use the actorOutOfPrep, if there's a direct object available */
    adjustDefaultObjectPrep(prep, obj)
        { return (obj != nil ? obj.actorOutOfPrep + ' ' : prep); }
;

VerbRule(GetOut)
    'get' 'out'
    | 'get' 'off'
    | 'get' 'down'
    | 'disembark'
    | 'climb' 'out'
    : GetOutAction
    verbPhrase = 'get/getting out'
;

VerbRule(Board)
    ('board'
     | ('get' ('in' | 'into' | 'in' 'to' | 'on' | 'onto' | 'on' 'to'))
     | ('climb' ('in' | 'into' | 'in' 'to')))
    singleDobj
    : BoardAction
    verbPhrase = 'get/getting (in what)'
    askDobjResponseProd = singleNoun
;

VerbRule(Sleep)
    'sleep'
    : SleepAction
    verbPhrase = 'sleep/sleeping'
;

VerbRule(Fasten)
    ('fasten' | 'buckle' | 'buckle' 'up') dobjList
    : FastenAction
    verbPhrase = 'fasten/fastening (what)'
;

VerbRule(FastenTo)
    ('fasten' | 'buckle') dobjList 'to' singleIobj
    : FastenToAction
    verbPhrase = 'fasten/fastening (what) (to what)'
    askIobjResponseProd = toSingleNoun
;

VerbRule(Unfasten)
    ('unfasten' | 'unbuckle') dobjList
    : UnfastenAction
    verbPhrase = 'unfasten/unfastening (what)'
;

VerbRule(UnfastenFrom)
    ('unfasten' | 'unbuckle') dobjList 'from' singleIobj
    : UnfastenFromAction
    verbPhrase = 'unfasten/unfastening (what) (from what)'
    askIobjResponseProd = fromSingleNoun
;

VerbRule(PlugInto)
    'plug' dobjList ('in' | 'into' | 'in' 'to') singleIobj
    : PlugIntoAction
    verbPhrase = 'plug/plugging (what) (into what)'
    askIobjResponseProd = inSingleNoun
;

VerbRule(PlugIntoWhat)
    [badness 500] 'plug' dobjList
    : PlugIntoAction
    verbPhrase = 'plug/plugging (what) (into what)'
    construct()
    {
        /* set up the empty indirect object phrase */
        iobjMatch = new EmptyNounPhraseProd();
        iobjMatch.responseProd = inSingleNoun;
    }
;

VerbRule(PlugIn)
    'plug' dobjList 'in'
    | 'plug' 'in' dobjList
    : PlugInAction
    verbPhrase = 'plug/plugging (what)'
;

VerbRule(UnplugFrom)
    'unplug' dobjList 'from' singleIobj
    : UnplugFromAction
    verbPhrase = 'unplug/unplugging (what) (from what)'
    askIobjResponseProd = fromSingleNoun
;

VerbRule(Unplug)
    'unplug' dobjList
    : UnplugAction
    verbPhrase = 'unplug/unplugging (what)'
;

VerbRule(Screw)
    'screw' dobjList
    : ScrewAction
    verbPhrase = 'screw/screwing (what)'
;

VerbRule(ScrewWith)
    'screw' dobjList 'with' singleIobj
    : ScrewWithAction
    verbPhrase = 'screw/screwing (what) (with what)'
    omitIobjInDobjQuery = true
    askIobjResponseProd = withSingleNoun
;

VerbRule(Unscrew)
    'unscrew' dobjList
    : UnscrewAction
    verbPhrase = 'unscrew/unscrewing (what)'
;

VerbRule(UnscrewWith)
    'unscrew' dobjList 'with' singleIobj
    : UnscrewWithAction
    verbPhrase = 'unscrew/unscrewing (what) (with what)'
    omitIobjInDobjQuery = true
    askIobjResponseProd = withSingleNoun
;

VerbRule(PushTravelDir)
    ('push' | 'pull' | 'drag' | 'move') singleDobj singleDir
    : PushTravelDirAction
    verbPhrase = ('push/pushing (what) ' + dirMatch.dir.name)
;

VerbRule(PushTravelThrough)
    ('push' | 'pull' | 'drag' | 'move') singleDobj
    ('through' | 'thru') singleIobj
    : PushTravelThroughAction
    verbPhrase = 'push/pushing (what) (through what)'
;

VerbRule(PushTravelEnter)
    ('push' | 'pull' | 'drag' | 'move') singleDobj
    ('in' | 'into' | 'in' 'to') singleIobj
    : PushTravelEnterAction
    verbPhrase = 'push/pushing (what) (into what)'
;

VerbRule(PushTravelGetOutOf)
    ('push' | 'pull' | 'drag' | 'move') singleDobj
    'out' ('of' | ) singleIobj
    : PushTravelGetOutOfAction
    verbPhrase = 'push/pushing (what) (out of what)'
;


VerbRule(PushTravelClimbUp)
    ('push' | 'pull' | 'drag' | 'move') singleDobj
    'up' singleIobj
    : PushTravelClimbUpAction
    verbPhrase = 'push/pushing (what) (up what)'
    omitIobjInDobjQuery = true
;

VerbRule(PushTravelClimbDown)
    ('push' | 'pull' | 'drag' | 'move') singleDobj
    'down' singleIobj
    : PushTravelClimbDownAction
    verbPhrase = 'push/pushing (what) (down what)'
;

VerbRule(Exits)
    'exits'
    : ExitsAction
    verbPhrase = 'exits/showing exits'
;

VerbRule(ExitsMode)
    'exits' ('on'->on_ | 'all'->on_
             | 'off'->off_ | 'none'->off_
             | ('status' ('line' | ) | 'statusline') 'look'->on_
             | 'look'->on_ ('status' ('line' | ) | 'statusline')
             | 'status'->stat_ ('line' | ) | 'statusline'->stat_
             | 'look'->look_)
    : ExitsModeAction
    verbPhrase = 'turn/turning off exits display'
;

VerbRule(HintsOff)
    'hints' 'off'
    : HintsOffAction
    verbPhrase = 'disable/disabling hints'
;

VerbRule(Hint)
    'hint' | 'hints'
    : HintAction
    verbPhrase = 'show/showing hints'
;

VerbRule(Oops)
    ('oops' | 'o') singleLiteral
    : OopsAction
    verbPhrase = 'oops/correcting (what)'
;

VerbRule(OopsOnly)
    ('oops' | 'o')
    : OopsIAction
    verbPhrase = 'oops/correcting'
;

/* ------------------------------------------------------------------------ */
/*
 *   "debug" verb - special verb to break into the debugger.  We'll only
 *   compile this into the game if we're compiling a debug version to begin
 *   with, since a non-debug version can't be run under the debugger.  
 */
#ifdef __DEBUG

VerbRule(Debug)
    'debug'
    : DebugAction
    verbPhrase = 'debug/debugging'
;

#endif /* __DEBUG */

