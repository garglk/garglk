#charset "us-ascii"

/* 
 *   Copyright (c) 2000, 2006 Michael J. Roberts.  All Rights Reserved. 
 *   
 *   TADS 3 Library: parser
 *   
 *   This modules defines the language-independent parts of the command
 *   parser.
 *   
 *   Portions based on xiny.t, copyright 2002 by Steve Breslin and
 *   incorporated by permission.  
 */

#include "adv3.h"
#include "tok.h"
#include <dict.h>
#include <gramprod.h>
#include <strcomp.h>


/* ------------------------------------------------------------------------ */
/* 
 *   property we add to ResolveInfo to store the remaining items from an
 *   ambiguous list 
 */
property extraObjects;


/* ------------------------------------------------------------------------ */
/*
 *   ResolveResults - an instance of this class is passed to the
 *   resolveNouns() routine to receive the results of the resolution.
 *   
 *   This class's main purpose is to virtualize the handling of error or
 *   warning conditions during the resolution process.  The ResolveResults
 *   object is created by the initiator of the resolution, so it allows
 *   the initiator to determine how errors are to be handled without
 *   having to pass flags down through the match tree.  
 */
class ResolveResults: object
    /*
     *   Instances must provide the following methods:
     *   
     *   noVocabMatch(action, txt) - there are no objects in scope matching
     *   the given noun phrase vocabulary.  This is "worse" than noMatch(),
     *   in the sense that this indicates that the unqualified noun phrase
     *   simply doesn't refer to any objects in scope, whereas noMatch()
     *   means that some qualification applied to the vocabulary ruled out
     *   any matches.  'txt' is the original text of the noun phrase.
     *   
     *   noMatch(action, txt) - there are no objects in scope matching the
     *   noun phrase.  This is used in cases where we eliminate all
     *   possible matches because of qualifications or constraints, so it's
     *   possible that the noun phrase vocabulary, taken by itself, does
     *   match some object; it's just that when the larger noun phrase
     *   context is considered, there's nothing that matches.
     *   
     *   noMatchPossessive(action, txt) - same as noMatch, but the
     *   unmatched phrase is qualified with a possessive phrase.  For
     *   ranking matches, the possessive version ranks ahead of treating
     *   the possessive words as raw vocabulary when neither matches, since
     *   it's more informative to report that we can't even match the
     *   underlying qualified noun phrase, let alone the whole phrase with
     *   qualification.
     *   
     *   noMatchForAll() - there's nothing matching "all"
     *   
     *   noMatchForAllBut() - there's nothing matching "all except..."
     *   (there might be something matching all, but there's nothing left
     *   when the exception list is applied)
     *   
     *   noMatchForListBut() - there's nothing matching "<list> except..."
     *   
     *   noteEmptyBut() - a "but" (or "except") list matches nothing.  We
     *   don't consider this an error, but we rate an interpretation with a
     *   non-empty "but" list and complete exclusion of the "all" or "any"
     *   phrase whose objects are excluded by the "but" higher than one
     *   with an empty "but" list and a non-empty all/any list - we do this
     *   because it probably means that we came up with an incorrect
     *   interpretation of the "but" phrase in the empty "but" list case
     *   and failed to exclude things we should have excluded.
     *   
     *   noMatchForPronoun(typ, txt) - there's nothing matching a pronoun.
     *   'typ' is one of the PronounXxx constants, and 'txt' is the text of
     *   the word used in the command.
     *   
     *   allNotAllowed() - the command contained the word ALL (or a
     *   synonym), but the verb doesn't allow ALL to be used in its noun
     *   phrases.  
     *   
     *   reflexiveNotAllowed(typ, txt) - the reflexive pronoun isn't
     *   allowed in this context.  This usually means that the pronoun was
     *   used with an intransitive or single-object action.  'typ' is one
     *   of the PronounXxx constants, and 'txt' is the text of the word
     *   used.
     *   
     *   wrongReflexive(typ, txt) - the reflexive pronoun doesn't agree
     *   with its referent in gender, number, or some other way.
     *   
     *   noMatchForPossessive(owner, txt) - there's nothing matching the
     *   phrase 'txt' owned by the resolved possessor object 'owner'.  Note
     *   that 'owner' is a list, since we can have plural possessive
     *   qualifier phrases.
     *   
     *   noMatchForLocation(loc, txt) - there's nothing matching 'txt' in
     *   the location object 'loc'.  This is used when a noun phrase is
     *   explicitly qualified by location ("the book on the table").
     *   
     *   noteBadPrep() - we have a noun phrase or other phrase
     *   incorporating an invalid prepositional phrase structure.  This is
     *   called from "badness" rules that are set up to match phrases with
     *   embedded prepositions, as a last resort when no valid
     *   interpretation can be found.
     *   
     *   nothingInLocation(loc) - there's nothing in the given location
     *   object.  This is used when we try to select the one object or all
     *   of the objects in a given container, but the container doesn't
     *   actually have any contents.
     *   
     *   ambiguousNounPhrase(keeper, asker, txt, matchLst, fullMatchList,
     *   scopeList, requiredNum, resolver) - an ambiguous noun phrase was
     *   entered: the noun phrase matches multiple objects that are all
     *   equally qualified, but we only want the given exact number of
     *   matches.  'asker' is a ResolveAsker object that we'll use to
     *   generate any prompts; if no customization is required, simply pass
     *   the base ResolveAsker.  'txt' is the original text of the noun
     *   list in the command, which the standard prompt messages can use to
     *   generate their questions.  'matchLst' is a list of the qualified
     *   objects matching the phrase, with only one object included for
     *   each set of equivalents in the original full list; 'fullMatchList'
     *   is the full list of matches, including each copy of equivalents;
     *   'scopeList' is the list of everything in scope that matched the
     *   original phrase, including illogical items.  If it's desirable to
     *   interact with the user at this point, prompt the user to resolve
     *   the list, and return a new list with the results.  If no prompting
     *   is desired, the original list can be returned.  If it isn't
     *   possible to determine the final set of objects, and a final set of
     *   objects is required (this is up to the subclass to determine), a
     *   parser exception should be thrown to stop further processing of
     *   the command.  'keeper' is an AmbigResponseKeeper object, which is
     *   usually simply the production object itself; each time we parse an
     *   interactive response (if we are interactive at all), we'll call
     *   addAmbigResponse() on the calling production object to add it to
     *   the saved list of responses, and we'll call getAmbigResponses() to
     *   find previous answers to the same question, in case of
     *   re-resolving the phrase with 'again' or the like.
     *   
     *   unknownNounPhrase(match, resolver) - a noun phrase that doesn't
     *   match any known noun phrase syntax was entered.  'match' is the
     *   production match tree object for the unknown phrase.  Returns a
     *   list of the resolved objects for the noun phrase, if possible.  If
     *   it is not possible to resolve the phrase, and a resolution is
     *   required (this is up to the subclass to determine), a parser
     *   exception should be thrown.
     *   
     *   getImpliedObject(np, resolver) - a noun phrase was left out
     *   entirely.  'np' is the noun phrase production standing in for the
     *   missing noun phrase; this is usually an EmptyNounPhraseProd or a
     *   subclass.  If an object is implicit in the command, or a
     *   reasonable default can be assumed, return the implicit or default
     *   object or objects.  If not, the routine can return nil or can
     *   throw an error.  The result is a ResolveInfo list.
     *   
     *   askMissingObject(asker, resolver, responseProd) - a noun phrase
     *   was left out entirely, and no suitable default can be found
     *   (getImpliedObject has already been called, and that returned nil).
     *   If it is possible to ask the player interactively to fill in the
     *   missing object, ask the player.  If it isn't possible to resolve
     *   an object, an error can be thrown, or an empty list can be
     *   returned.  'asker' is a ResolveAsker object, which can be used to
     *   customize the prompt (if any) that we show; pass the base
     *   ResolveAsker if no customization is needed.  'responseProd' is the
     *   production to use to parse the response.  The return value is the
     *   root match tree object of the player's interactive response, with
     *   its 'resolvedObjects' property set to the ResolveInfo list from
     *   resolving the player's response.  (The routine returns the match
     *   tree for the player's response so that, if we must run resolution
     *   again on another pass, we can re-resolve the same response without
     *   asking the player the same question again.)
     *   
     *   noteLiteral(txt) - note the text of a literal phrase.  When
     *   selecting among alternative interpretations of a phrase, we'll
     *   favor shorter literals, since treating fewer tokens as literals
     *   means that we're actually interpreting more tokens.
     *   
     *   askMissingLiteral(action, which) - a literal phrase was left out
     *   entirely.  If possible, prompt interactively for a player response
     *   and return the result.  If it's not possible to ask for a
     *   response, an error can be thrown, or nil can be returned.  The
     *   return value is simply the text string the player enters.
     *   
     *   emptyNounPhrase(resolver) - a noun phrase involving only
     *   qualifiers was entered ('take the').  In most cases, an exception
     *   should be thrown.  If the empty phrase can be resolved to an
     *   object or list of objects, the resolved list should be returned.
     *   
     *   zeroQuantity(txt) - a noun phrase referred to a zero quantity of
     *   something ("take zero books").
     *   
     *   insufficientQuantity(txt, matchList, requiredNumber) - a noun
     *   phrase is quantified with a number exceeding the number of objects
     *   available: "take five books" when only two books are in scope.
     *   
     *   uniqueObjectRequired(txt, matchList) - a noun phrase yields more
     *   than one object, but only one is allowed.  For example, we'll call
     *   this if the user attempts to use more than one result for a
     *   single-noun phrase (such as by answering a disambiguation question
     *   with 'all').
     *   
     *   singleObjectRequired(txt) - a noun phrase list was used where a
     *   single noun phrase is required.
     *   
     *   noteAdjEnding() - a noun phrase ends in an adjective.  This isn't
     *   normally an error, but is usually less desirable than interpreting
     *   the same noun phrase as ending in a noun (in other words, if a
     *   word can be used as both an adjective and a noun, it is usually
     *   better to interpret the word as a noun rather than as an adjective
     *   when the word appears at the end of a noun phrase, as long as the
     *   noun interpretation matches an object in scope).
     *   
     *   noteIndefinite() - a noun phrase is phrased as an indefinite ("any
     *   book", "one book"), meaning that we can arbitrarily choose any
     *   matching object in case of ambiguity.  Sometimes, an object will
     *   have explicit vocabulary that could be taken to be indefinite: a
     *   button labeled "1" could be a "1 button", for example, or a subway
     *   might have an "A train".  By noting the indefinite interpretation,
     *   we can give priority to the alternative definite interpretation.
     *   
     *   noteMatches(matchList) - notifies the results object that the
     *   given list of objects is being matched.  This allows the results
     *   object to inspect the object list for its strength: for example,
     *   by noting the presence of truncated words.  This should only be
     *   called after the nouns have been resolved to the extent possible,
     *   so any disambiguation or selection that is to be performed should
     *   be performed before this routine is called.
     *   
     *   noteMiscWordList(txt) - a noun phrase is made up of miscellaneous
     *   words.  A miscellaneous word list as a noun phrase has non-zero
     *   badness, so we will never use a misc word list unless we can't
     *   find any more structured interpretation.  In most cases, a
     *   miscellaneous word list indicates an invalid phrasing, but in some
     *   cases objects might use this unstructured type of noun phrase in
     *   conjunction with matchName() to perform dynamic or special-case
     *   parsing.
     *   
     *   notePronoun() - a noun phrase is matching as a pronoun.  In
     *   general, we prefer a match to an object's explicit vocabulary
     *   words to a match to a pronoun phrase: if the game goes to the
     *   trouble of including a word explicitly among an object's
     *   vocabulary, that's a better match than treating the same word as a
     *   generic pronoun.
     *   
     *   notePlural() - a plural phrase is matching.  In some cases we
     *   require a single object match, in which case a plural phrase is
     *   undesirable.  (A plural phrase might still match just one object,
     *   though, so it can't be ruled out on structural grounds alone.)
     *   
     *   beginSingleObjSlot() and endSingleObjSlot() are used to bracket
     *   resolution of a noun phrase that needs to be resolved as a single
     *   object.  We use these to explicitly lower the ranking for plural
     *   structural phrasings within these slots.
     *   
     *   beginTopicSlot() and endTopicSlot() are used to bracket resolution
     *   of a noun phrase that's being used as a conversation topic.  These
     *   are of the *form* of single nouns, but singular and plural words
     *   are equivalent, so when we're in a topic we don't consider a
     *   plural to be a minus the way we would for an ordinary object noun
     *   phrase.
     *   
     *   incCommandCount() - increment the command counter.  This is used
     *   to keep track of how many subcommands are in a command tree.
     *   
     *   noteActorSpecified() - note that the command is explicitly
     *   addressed to an actor.
     *   
     *   noteNounSlots(cnt) - note the number of "noun slots" in the verb
     *   phrase.  This is the number of objects the verb takes: an
     *   intransitive verb has no noun slots; a transitive verb with a
     *   direct object only has one; a verb with a direct and indirect
     *   object has two.  Note this has nothing to do with the number of
     *   objects specified in a noun list - TAKE BOX, BOOK, AND BELL has
     *   only one noun slot (a direct object) even though the slot is
     *   occupied by a list with three objects.
     *   
     *   noteWeakPhrasing(level) - note that the phrasing is "weak," with
     *   the given weakness level - higher is weaker.  The exact meaning of
     *   the weakness levels is up to the language module to define.  The
     *   English module, for example, considers VERB IOBJ DOBJ phrasing
     *   (with no preposition, as in GIVE BOB BOOK) to be weak when the
     *   DOBJ part doesn't have a grammatical marker that clarifies that
     *   it's really a separate noun phrase (an article serves this purpose
     *   in English: GIVE BOB THE BOOK).
     *   
     *   allowActionRemapping - returns true if we can remap the action
     *   during noun phrase resolution.  Remapping is usually allowed only
     *   during the actual execution phase, not during the ranking phase.  
     *   
     *   allowEquivalentFiltering - returns true if we can filter an
     *   ambiguous resolution list by making an arbitrary choice among
     *   equivalent objects.  This is normally allowed only during a final
     *   resolution phase, not during a tentative resolution phase.  
     */
;

/* ------------------------------------------------------------------------ */
/*
 *   Noun phrase resolver "asker."  This type of object can be passed to
 *   certain ResolveResults methods in order to customize the messages
 *   that the parser generates for interactive prompting.  
 */
class ResolveAsker: object
    /*
     *   Ask for help disambiguating a noun phrase.  This asks which of
     *   several possible matching objects was intended.  This method has
     *   the same parameter list as the equivalent message object method.  
     */
    askDisambig(targetActor, promptTxt, curMatchList, fullMatchList,
                requiredNum, askingAgain, dist)
    {
        /* let the target actor's parser message object handle it */
        targetActor.getParserMessageObj().askDisambig(
            targetActor, promptTxt, curMatchList, fullMatchList,
            requiredNum, askingAgain, dist);
    }

    /*
     *   Ask for a missing object.  This prompts for an object that's
     *   structurally required for an action, but which was omitted from
     *   the player's command.  
     */
    askMissingObject(targetActor, action, which)
    {
        /* let the target actor's parser message object handle it */
        targetActor.getParserMessageObj().askMissingObject(
            targetActor, action, which);
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   The resolveNouns() method returns a list of ResolveInfo objects
 *   describing the objects matched to the noun phrase.  
 */
class ResolveInfo: object
    construct(obj, flags, np = nil)
    {
        /* remember the members */
        obj_ = obj;
        flags_ = flags;
        np_ = np;
    }

    /*
     *   Look for a ResolveInfo item in a list of ResolveInfo items that
     *   is equivalent to us.  Returns true if we find such an item, nil
     *   if not.
     *   
     *   Another ResolveInfo is equivalent to us if it refers to the same
     *   underlying game object that we do, or if it refers to a game
     *   object that is indistinguishable from our underlying game object.
     */
    isEquivalentInList(lst)
    {
        /* 
         *   if we can find our exact item in the list, or we can find an
         *   equivalent object, we have an equivalent 
         */
        return (lst.indexWhich({x: x.obj_ == obj_}) != nil
                || lst.indexWhich(
                       {x: x.obj_.isVocabEquivalent(obj_)}) != nil);
    }

    /*
     *   Look for a ResolveInfo item in a list of ResolveInfo items that
     *   is equivalent to us according to a particular Distinguisher. 
     */
    isDistEquivInList(lst, dist)
    {
        /* 
         *   if we can find our exact item in the list, or we can find an
         *   equivalent object, we have an equivalent 
         */
        return (lst.indexWhich({x: x.obj_ == obj_}) != nil
                || lst.indexWhich(
                       {x: !dist.canDistinguish(x.obj_, obj_)}) != nil);
    }
    
    /* the object matched */
    obj_ = nil

    /* flags describing how we matched the object */
    flags_ = 0

    /* 
     *   By default, each ResolveInfo counts as one object, for the
     *   purposes of a quantity specifier (as in "five coins" or "both
     *   hats").  However, in some cases, a single resolved object might
     *   represent a collection of discrete objects and thus count as more
     *   than one for the purposes of the quantifier.  
     */
    quant_ = 1

    /*
     *   The possessive ranking, if applicable.  If this object is
     *   qualified by a possessive phrase ("my books"), we'll set this to
     *   a value indicating how strongly the possession applies to our
     *   object: 2 indicates that the object is explicitly owned by the
     *   object indicated in the possessive phrase, 1 indicates that it's
     *   directly held by the named possessor but not explicitly owned,
     *   and 0 indicates all else.  In cases where there's no posessive
     *   qualifier, this will simply be zero.  
     */
    possRank_ = 0

    /* the pronoun type we matched, if any (as a PronounXxx enum) */
    pronounType_ = nil

    /* the noun phrase we parsed to come up with this object */
    np_ = nil

    /*
     *   The pre-calculated multi-object announcement text for this object.
     *   When we iterate over the object list in a command with multiple
     *   direct or indirect objects (TAKE THE BOOK, BELL, AND CANDLE), we
     *   calculate the little announcement messages ("book:") for the
     *   objects BEFORE we execute the actual commands.  We then use the
     *   pre-calculated announcements during our iteration.  This ensures
     *   consistency in the basis for choosing the names, which is
     *   important in cases where the names include state-dependent
     *   information for the purposes of distinguishing one object from
     *   another.  The relevant state can change over the course of
     *   executing the command on the objects in the iteration, so if we
     *   calculated the names on the fly we could end up with inconsistent
     *   naming.  The user thinks of the objects in terms of their state at
     *   the start of the command, so the pre-calculation approach is not
     *   only more internally consistent, but is also more consistent with
     *   the user's perspective.  
     */
    multiAnnounce = nil
;

/*
 *   Intersect two resolved noun lists, returning a list consisting only
 *   of the unique objects from the two lists.  
 */
intersectNounLists(lst1, lst2)
{
    /* we don't have any results yet */
    local ret = [];
    
    /* 
     *   run through each element of the first list to see if it has a
     *   matching object in the second list 
     */
    foreach(local cur in lst1)
    {
        local other;
        /* 
         *   if this element's object occurs in the second list, we can
         *   include it in the result list 
         */
        if ((other = lst2.valWhich({x: x.obj_ == cur.obj_})) != nil)
        {
            /* if one or the other has a noun phrase, keep it */
            local np = cur.np_ ?? other.np_;
            
            /* 
             *   include this one in the result list, with the combined
             *   flags from the two original entries 
             */
            ret += new ResolveInfo(cur.obj_, cur.flags_ | other.flags_, np);
        }
    }

    /* return the result list */
    return ret;
}

/*
 *   Extract the objects from a list obtained with resolveNouns().
 *   Returns a list composed only of the objects in the resolution
 *   information list.  
 */
getResolvedObjects(lst)
{
    /* 
     *   return a list composed only of the objects from the ResolveInfo
     *   structures 
     */
    return lst.mapAll({x: x.obj_});
}


/* ------------------------------------------------------------------------ */
/*
 *   The basic production node base class.  We'll use this as the base
 *   class for all of our grammar rule match objects.  
 */
class BasicProd: object
    /* get the original text of the command for this match */
    getOrigText()
    {
        /* if we have no token list, return an empty string */
        if (tokenList == nil)
            return '';
        
        /* build the string based on my original token list */
        return cmdTokenizer.buildOrigText(getOrigTokenList());
    }

    /* get my original token list, in canonical tokenizer format */
    getOrigTokenList()
    {
        /* 
         *   return the subset of the full token list from my first token
         *   to my last token
         */
        return nilToList(tokenList).sublist(
            firstTokenIndex, lastTokenIndex - firstTokenIndex + 1);
    }

    /* 
     *   Set my original token list.  This replaces the actual token list
     *   we matched from the parser with a new token list provided by the
     *   caller. 
     */
    setOrigTokenList(toks)
    {
        tokenList = toks;
        firstTokenIndex = 1;
        lastTokenIndex = toks.length();
    }

    /*
     *   Can this object match tree resolve to the given object?  We'll
     *   resolve the phrase as though it were a topic phrase, then look for
     *   the object among the matches.  
     */
    canResolveTo(obj, action, issuingActor, targetActor, which)
    {
        /* set up a topic resolver */
        local resolver = new TopicResolver(
            action, issuingActor, targetActor, self, which,
            action.createTopicQualifierResolver(issuingActor, targetActor));

        /* 
         *   set up a results object - use a tentative results object,
         *   since we're only looking at the potential matches, not doing a
         *   full resolution pass 
         */
        local results = new TentativeResolveResults(issuingActor, targetActor);
        
        /* resolve the phrase as a topic */
        local match = resolveNouns(resolver, results);

        /* 
         *   make sure it's packaged in the canonical topic form, as a
         *   ResolvedTopic object 
         */
        match = resolver.packageTopicList(match, self);

        /* if the topic can match it, it's a possible resolution */
        return (match != nil
                && match.length() > 0
                && match[1].obj_.canMatchObject(obj));
    }

    /*
     *   Is this match a match to the special syntax for a custom missing
     *   object query?  This returns true if the match has a wording that
     *   strongly distinguishes it from an ordinary new command.  In the
     *   English parser, for example, this returns true for the
     *   PrepSingleTopicProd matches (e.g., inSingleNoun) if the phrase
     *   starts with the preposition for the match.
     *   
     *   This property is used when we ask a missing object question:
     *   
     *.    >dig
     *.    What do you want to dig in?
     *.  
     *.    >in the dirt
     *   
     *   In English, the DIG command sets up to receive a response phrased
     *   as "in <noun phrase>" - that's done by setting the response
     *   production to inSingleNoun.  In this case, "in the dirt" would
     *   return true, since that's pretty clearly a match to the expected
     *   inSingleNoun syntax.  In contrast, "the dirt" would return false,
     *   since that's just a noun phrase without the special wording for
     *   this particular verb.
     */
    isSpecialResponseMatch = nil

    /*
     *   Grammar match objects that come from a GrammarProd.parseTokens()
     *   call will always have a set of properties indicating which tokens
     *   from the input matched the grammar rule.  However, we sometimes
     *   synthesize match trees internally rather than getting them from
     *   parser input; for synthesized trees, the parser obviously won't
     *   supply those properties for us, so we need to define suitable
     *   defaults that synthesized match tree nodes can inherit.
     */
    firstTokenIndex = 0
    lastTokenIndex = 0
;


/* ------------------------------------------------------------------------ */
/*
 *   Basic disambiguation production class 
 */
class DisambigProd: BasicProd
    /*
     *   Remove the "ambiguous" flags from a result list.  This can be
     *   used to mark the response to a disambiguation query as no longer
     *   ambiguous.  
     */
    removeAmbigFlags(lst)
    {
        /* remove the "unclear disambig" flag from each result item */
        foreach (local cur in lst)
            cur.flags_ &= ~UnclearDisambig;

        /* return the list */
        return lst;
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Base class for "direction" productions.  Each direction (the compass
 *   directions, the vertical directions, the shipboard directions, and so
 *   on) must have an associated grammar rule, which must produce one of
 *   these.  This should be subclassed with grammar rules like this:
 *   
 *   grammar directionName: 'north' | 'n' : DirectionProd
 *.      dir = northDirection
 *.  ;
 */
class DirectionProd: BasicProd
    /*
     *   Each direction-specific grammar rule subclass must set this
     *   property to the associated direction object (northDirection,
     *   etc). 
     */
    dir = nil
;


/* ------------------------------------------------------------------------ */
/*
 *   The base class for commands.  A command is the root of the grammar
 *   match tree for a single action.  A command line can consist of a
 *   number of commands joined with command separators; in English,
 *   command separators are things like periods, semicolons, commas, and
 *   the words "and" and "then".  
 */
class CommandProd: BasicProd
    hasTargetActor()
    {
        /* 
         *   By default, a command production does not include a
         *   specification of a target actor.  Command phrases that
         *   contain syntax specifically directing the command to a target
         *   actor should return true here. 
         */
        return nil;
    }

    /* 
     *   Get the match tree for the target actor phrase, if any.  By
     *   default, we have no target actor phrase, so just return nil.  
     */
    getActorPhrase = nil

    /* 
     *   "Execute" the actor phrase.  This lets us know that the parser
     *   has decided to use our phrasing to specify the target actor.
     *   We're not required to do anything here; it's just a notification
     *   for subclass use.  Since we don't have a target actor phrase at
     *   all, we obviously don't need to do anything here.  
     */
    execActorPhrase(issuingActor) { }
;

/*
 *   A first-on-line command.  The first command on a command line can
 *   optionally start with an actor specification, to give orders to the
 *   actor.  
 */
class FirstCommandProd: CommandProd
    countCommands(results)
    {
        /* count commands in the underlying command */
        cmd_.countCommands(results);
    }

    getTargetActor()
    {
        /* 
         *   we have no actor specified explicitly, so it's the current
         *   player character 
         */
        return libGlobal.playerChar;
    }

    /* 
     *   The tokens of the entire command except for the target actor
     *   specification.  By default, we take all of the tokens starting
     *   with the first command's first token and running to the end of
     *   the token list.  This assumes that the target actor is specified
     *   at the beginning of the command - languages that use some other
     *   word ordering must override this accordingly.  
     */
    getCommandTokens()
    {
        return tokenList.sublist(cmd_.firstTokenIndex);
    }

    /*
     *   Resolve my first action.  This returns an instance of a subclass
     *   of Action that represents the resolved action.  We'll ask our
     *   first subcommand to resolve its action.  
     */
    resolveFirstAction(issuingActor, targetActor)
    {
        return cmd_.resolveFirstAction(issuingActor, targetActor);
    }

    /* resolve nouns in the command */
    resolveNouns(issuingActor, targetActor, results)
    {
        /* resolve nouns in the underlying command */
        cmd_.resolveNouns(issuingActor, targetActor, results);

        /* count our commands */
        countCommands(results);
    }

    /*
     *   Does this command end a sentence?  The exact meaning of a
     *   sentence may vary by language; in English, a sentence ends with
     *   certain punctuation marks (a period, a semicolon, an exclamation
     *   point).  
     */
    isEndOfSentence()
    {
        /* ask the underlying command phrase */
        return cmd_.isEndOfSentence();
    }

    /*
     *   Get the token index of the first command separator token.  This
     *   is the first token that is not part of the underlying command. 
     */
    getCommandSepIndex()
    {
        /* get the separator index from the underlying command */
        return cmd_.getCommandSepIndex();
    }

    /* 
     *   get the token index of the next command - this is the index of
     *   the next token after our conjunction if we have one, or after our
     *   command if we don't have a conjunction 
     */
    getNextCommandIndex()
    {
        /* get the next command index from the underlying command */
        return cmd_.getNextCommandIndex();
    }
;

/* 
 *   Define the simplest grammar rule for a first-on-line command phrase,
 *   which is just an ordinary command phrase.  The language-specific
 *   grammar must define any other alternatives; specifically, the
 *   language might provide an "actor, command" syntax to direct a command
 *   to a particular actor.  
 */
grammar firstCommandPhrase(commandOnly): commandPhrase->cmd_
    : FirstCommandProd
;

/*
 *   A command with an actor specification.  This should be instantiated
 *   with grammar rules in a language-specific module.
 *   
 *   Instantiating grammar rules should set property actor_ to the actor
 *   match tree, and cmd_ to the underlying 'commandPhrase' production
 *   match tree.  
 */
class CommandProdWithActor: CommandProd
    hasTargetActor()
    {
        /* this command explicitly specifies an actor */
        return true;
    }
    getTargetActor()
    {
        /* return my resolved actor object */
        return resolvedActor_;
    }
    getActorPhrase()
    {
        /* return the actor phrase production */
        return actor_;
    }

    /*
     *   Execute the target actor phrase.  This is a notification, for use
     *   by subclasses; we don't have anything we need to do in this base
     *   class implementation. 
     */
    execActorPhrase(issuingActor) { }

    /*
     *   Resolve nouns.  We'll resolve only the nouns in the target actor
     *   phrase; we do not resolve nouns in the command phrase, because we
     *   must re-parse the command phrase after we've finished resolving
     *   the actor phrase, and thus we can't resolve nouns in the command
     *   phrase until after the re-parse is completed.  
     */
    resolveNouns(issuingActor, targetActor, results)
    {
        local lst;
        
        /* 
         *   Resolve the actor, then we're done.  Note that we do not
         *   attempt to resolve anything in the underlying command yet,
         *   because we can only resolve the command after we've fully
         *   processed the actor clause.  
         */
        lst = actor_.resolveNouns(getResolver(issuingActor), results);

        /* 
         *   there are no noun phrase slots, since we're not looking at the
         *   verb 
         */
        results.noteNounSlots(0);

        /* 
         *   if we have an object in the list, use it - note that our
         *   actor phrase is a single-noun production, and hence should
         *   never yield more than a single entry in its resolution list 
         */
        if (lst.length() > 0)
        {
            /* remember the resolved actor */
            resolvedActor_ = lst[1].obj_;

            /* note in the results that an actor is specified */
            results.noteActorSpecified();
        }

        /* count our commands */
        countCommands(results);
    }

    /* get or create my actor resolver */
    getResolver(issuingActor)
    {
        /* if I don't already have a resolver, create one and cache it */
        if (resolver_ == nil)
            resolver_ = new ActorResolver(issuingActor);

        /* return our cached resolver */
        return resolver_;
    }

    /* my resolved actor object */
    resolvedActor_ = nil

    /* my actor resolver object */
    resolver_ = nil
;

/*
 *   First-on-line command with target actor.  As with
 *   CommandProdWithActor, instantiating grammar rules must set the
 *   property actor_ to the actor match tree, and cmd_ to the underlying
 *   commandPhrase match.  
 */
class FirstCommandProdWithActor: CommandProdWithActor, FirstCommandProd
;


/* ------------------------------------------------------------------------ */
/*
 *   The 'predicate' production is the grammar rule for all individual
 *   command phrases.  We don't define anything about the predicate
 *   grammar here, since it is completely language-dependent, but we do
 *   *use* the predicate production as a sub-production of our
 *   commandPhrase rules.
 *   
 *   The language-dependent implementation of the 'predicate' production
 *   must provide the following methods:
 *   
 *   resolveAction(issuingActor, targetActor): This method returns a
 *   newly-created instance of the Action subclass that the command refers
 *   to.  This method must generally interpret the phrasing of the command
 *   to determine what noun phrases are present and what roles they serve,
 *   and what verb phrase is present, then create an appropriate Action
 *   subclass to represent the verb phrase as applied to the noun phrases
 *   in their determined roles.
 *   
 *   resolveNouns(issuingActor, targetActor, results): This method
 *   resolves all of the noun phrases in the predicate, using the
 *   'results' object (an object of class ResolveResults) to store
 *   information about the status of the resolution.  Generally, this
 *   routine should collect information about the roles of the noun phrase
 *   production matches, plug these into the Action via appropriate
 *   properties (dobjMatch for the direct object of a transitive verb, for
 *   example), resolve the Action, and then call the Action to do the
 *   resolution.  This method has no return value, since the Action is
 *   responsible for keeping track of the results of the resolution.
 *   
 *   For languages like English which encode most information about the
 *   phrase roles in word ordering, a good way to design a predicate
 *   grammar is to specify the syntax of each possible verb phrase as a
 *   'predicate' rule, and base the match object for that rule on the
 *   corresponding Action subclass.  For example, the English grammar has
 *   this rule to define the syntax for the verb 'take':
 *   
 *   VerbRule
 *.     ('take' | 'pick' 'up' | 'get') dobjList
 *.     | 'pick' dobjList 'up'
 *.     : TakeAction
 *.  ;
 *   
 *   Since English encodes everything in this command positionally, it's
 *   straightforward to write grammar rules for the possible syntax
 *   variations of a given action, hence it's easy to associate command
 *   syntax directly with its associated action.  When each 'predicate'
 *   grammar maps to an Action subclass for its match object,
 *   resolveAction() is especially easy to implement: it simply returns
 *   'self', since the grammar match and the Action are the same object.
 *   It's also easy to plug each noun phrase into its appropriate property
 *   slot in the Action subclass: the parser can be told to plug in each
 *   noun phrase directly using the "->" notation in the grammar.
 *   
 *   Many languages encode the roles of noun phrases with case markers or
 *   other syntactic devices, and as a result do not impose such strict
 *   rules as English does on word order.  For such languages, it is
 *   usually better to construct the 'predicate' grammar separately from
 *   any single action, so that the various acceptable phrase orderings
 *   are enumerated, and the verb phrase is just another phrase that plugs
 *   into these top-level orderings.  In such grammars, the predicate must
 *   do some programmatic work in resolveAction(): it must figure out
 *   which Action subclass is involved based on the verb phrase sub-match
 *   object and the noun phrases present, then must create an instance of
 *   that Action subclass.  Furthermore, since we can't plug the noun
 *   phrases into the Action using the "->" notation in the grammar, the
 *   resolveAction() routine must pick out the sub-match objects
 *   representing the noun phrases and plug them into the Action itself.
 *   
 *   Probably the easiest way to accomplish all of this is by designing
 *   each verb phrase match object so that it is associated with one or
 *   more Action subclasses, according to the specific noun phrases
 *   present.  The details of this mapping are obviously specific to each
 *   language, but it should be possible in most cases to build a base
 *   class for all verb phrases that uses parameters to create the Action
 *   and noun phrase associations.  For example, each verb phrase grammar
 *   match object might have a list of possible Action matches, such as
 *   this example from a purely hypothetical English grammar based on this
 *   technique
 *   
 *   grammar verbPhrase: 'take' | 'pick' 'up': VerbProd
 *.      actionMap =
 *.      [
 *.          [TakeAction, BasicProd, &dobjMatch],
 *.          [TakeWithAction, BasicProd, &dobjMatch, WithProd, &iobjMatch]
 *.      ]
 *.  ;
 *   
 *   The idea is that the base VerbProd class looks through the list given
 *   by the actionMap property for a template that matches the number and
 *   type of noun phrases present in the predicate; when it finds a match,
 *   it creates an instance of the Action subclass given in the template,
 *   then plugs the noun phrases into the listed properties of the new
 *   Action instance.
 *   
 *   Note that our verbPhrase example above is not an example of actual
 *   working code, because there is no such thing in the
 *   language-independent library as a VerbProd base class that reads
 *   actionMap properites - this is a purely hypothetical bit of code to
 *   illustrate how such a construction might work in a language that
 *   requires it, and it is up to the language-specific module to define
 *   such a mechanism for its own use.  
 */


/* ------------------------------------------------------------------------ */
/*
 *   Base classes for grammar matches for full commands.
 *   
 *   There are two kinds of command separators: ambiguous and unambiguous.
 *   Unambiguous separators are those that can separate only commands, such
 *   as "then" and periods.  Ambiguous separators are those that can
 *   separate nouns in a noun list as well as commands; for example "and"
 *   and commas.
 *   
 *   First, CommandProdWithDefiniteConj, which is for a single full
 *   command, unambiguously terminated with a separator that can only mean
 *   that a new command follows.  We parse one command at a time when a
 *   command line includes several commands, since we can only resolve
 *   objects - and thus can only choose structural interpretations - when
 *   we reach the game state in which a given command is to be executed.
 *   
 *   Second, CommandProdWithAmbiguousConj, which is for multiple commands
 *   separated by a conjunction that does not end a sentence, but could
 *   just as well separate two noun phrases.  In this case, we parse both
 *   commands, to ensure that we actually have a well-formed command
 *   following the conjunction; this allows us to try interpreting the part
 *   after the conjunction as a command and also as a noun phrase, to see
 *   which interpretation looks better.
 *   
 *   When we find an unambiguous command separator, we can simply use the
 *   '*' grammar match to put off parsing everything after the separator
 *   until later, reducing the complexity of the grammar tree.  When we
 *   find an ambiguous separator, we can't put off parsing the rest with
 *   '*', because we need to know if the part after the separator can
 *   indeed be construed as a new command.  During resolution, we'll take a
 *   noun list interpretation of 'and' over a command list whenever doing
 *   so would give us resolvable noun phrases.  
 */

/*
 *   Match class for a command phrase that is separated from anything that
 *   follows with an unambiguous conjunction. 
 *   
 *   Grammar rules based on this match class must set property cmd_ to the
 *   underlying 'predicate' production.  
 */
class CommandProdWithDefiniteConj: CommandProd
    resolveNouns(issuingActor, targetActor, results)
    {
        /* resolve nouns */
        cmd_.resolveNouns(issuingActor, targetActor, results);

        /* count commands */
        countCommands(results);
    }
    countCommands(results)
    {
        /* we have only one subcommand */
        if (cmdCounted_++ == 0)
            results.incCommandCount();
    }

    /* counter: have we counted our command in the results object yet? */
    cmdCounted_ = 0

    /* resolve my first action */
    resolveFirstAction(issuingActor, targetActor)
    {
        return cmd_.resolveAction(issuingActor, targetActor);
    }

    /* does this command end a sentence */
    isEndOfSentence()
    {
        /* 
         *   it's the end of the sentence if our predicate encompasses all
         *   of the tokens in the command, or the conjunction is a
         *   sentence-ending conjunction 
         */
        return conj_ == nil || conj_.isEndOfSentence();
    }

    /*
     *   Get the token index of the first command separator token.  This
     *   is the first token that is not part of the underlying command. 
     */
    getCommandSepIndex()
    {
        /* 
         *   if we have a conjunction, return the first token index of the
         *   conjunction; otherwise, return the index of the next token
         *   after the command itself 
         */
        if (conj_ != nil)
            return conj_.firstTokenIndex;
        else
            return cmd_.lastTokenIndex + 1;
    }

    /* 
     *   get the token index of the next command - this is the index of
     *   the next token after our conjunction if we have one, or after our
     *   command if we don't have a conjunction 
     */
    getNextCommandIndex()
    {
        return (conj_ == nil ? cmd_ : conj_).lastTokenIndex + 1;
    }
;

/*
 *   Match class for two command phrases separated by an ambiguous
 *   conjunction (i.e., a conjunction that could also separate two noun
 *   phrases).  Grammar rules based on this class must set the properties
 *   'cmd1_' to the underlying 'predicate' production match of the first
 *   command, and 'cmd2_' to the underlying 'commandPhrase' production
 *   match of the second command.  
 */
class CommandProdWithAmbiguousConj: CommandProd
    resolveNouns(issuingActor, targetActor, results)
    {
        /* 
         *   Resolve nouns in the first subcommand only.  Do NOT resolve
         *   nouns in any of the additional subcommands (there might be
         *   more than one, since cmd2_ can be a list of subcommands, not
         *   just a single subcommand), because we cannot assume that the
         *   current scope will continue to be valid after executing the
         *   first subcommand - the first command could take us to a
         *   different location, or change the lighting conditions, or add
         *   or remove objects from the location, or any number of other
         *   things that would invalidate the current scope.
         */
        cmd1_.resolveNouns(issuingActor, targetActor, results);

        /* count our commands */
        countCommands(results);
    }
    countCommands(results)
    {
        /* count our first subcommand (cmd1_) */
        if (cmdCounted_++ == 0)
            results.incCommandCount();

        /* count results in the rest of the list (cmd2_ and its children) */
        cmd2_.countCommands(results);
    }

    /* counter: have we counted our command in the results object yet? */
    cmdCounted_ = 0

    /* resolve my first action */
    resolveFirstAction(issuingActor, targetActor)
    {
        return cmd1_.resolveAction(issuingActor, targetActor);
    }

    /* does this command end a sentence */
    isEndOfSentence()
    {
        /*
         *   it's the end of the sentence if the conjunction is a
         *   sentence-ending conjunction 
         */
        return conj_.isEndOfSentence();
    }

    /*
     *   Get the token index of the first command separator token.  This
     *   is the first token that is not part of the underlying command. 
     */
    getCommandSepIndex()
    {
        /* return the conjunction's first token index */
        return conj_.firstTokenIndex;
    }

    /* 
     *   get the token index of the next command - this is simply the
     *   starting index for our second subcommand tree 
     */
    getNextCommandIndex() { return cmd2_.firstTokenIndex; }
;

/*
 *   The basic grammar rule for an unambiguous end-of-sentence command.
 *   This matches either a predicate with nothing following, or a predicate
 *   with an unambiguous command-only conjunction following.  
 */
grammar commandPhrase(definiteConj):
    predicate->cmd_
    | predicate->cmd_ commandOnlyConjunction->conj_ *
    : CommandProdWithDefiniteConj
;

/*
 *   the basic grammar rule for a pair of commands with an ambiguous
 *   separator (i.e., a separator that could separate commands or
 *   conjunctions) 
 */
grammar commandPhrase(ambiguousConj):
    predicate->cmd1_ commandOrNounConjunction->conj_ commandPhrase->cmd2_
    : CommandProdWithAmbiguousConj
;

/* ------------------------------------------------------------------------ */
/*
 *   We leave almost everything about the grammatical rules of noun
 *   phrases to the language-specific implementation, but we provide a set
 *   of base classes for implementing several conceptual structures that
 *   have equivalents in most languages.  
 */


/*
 *   Basic noun phrase production class. 
 */
class NounPhraseProd: BasicProd
    /*
     *   Determine whether this kind of noun phrase prefers to keep a
     *   collective or the collective's individuals when filtering.  If
     *   this is true, we'll keep a collective and discard its individuals
     *   when filtering a resolution list; otherwise, we'll drop the
     *   collective and keep the individuals.  
     */
    filterForCollectives = nil

    /*
     *   Filter a "verify" result list for the results we'd like to keep
     *   in the final resolution of the noun phrase.  This is called after
     *   we've run through the verification process on the list of
     *   candidate matches, so 'lst' is a list of ResolveInfo objects,
     *   sorted in descending order of logicalness.
     *   
     *   By default, we keep only the items that are equally as logical as
     *   the best item in the results.  Since the items are sorted in
     *   descending order of goodness, the best item is always the first
     *   item.  
     */
    getVerifyKeepers(results)
    {
        local best;
        
        /* 
         *   Reduce the list to the most logical elements - in other
         *   words, keep only the elements that are exactly as logical as
         *   the first element, which we know to have the best logicalness
         *   ranking in the list by virtue of having sorted the list in
         *   descending order of logicalness.  
         */
        best = results[1];
        return results.subset({x: x.compareTo(best) == 0});
    }

    /*
     *   Filter a match list of results for truncated matches.  If we have
     *   a mix of truncated matches and exact matches, we'll keep only the
     *   exact matches.  If we have only truncated matches, though, we'll
     *   return the list unchanged, as we don't have a better offer going.
     */
    filterTruncations(lst, resolver)
    {
        local exactLst;

        /*
         *   If we're in "global scope," it means that we're resolving
         *   something like a topic phrase, and we're not limited to the
         *   current physical scope but can choose objects from the entire
         *   game world.  In this case, don't apply any truncation
         *   filtering.  The reason is that we could have several unrelated
         *   objects in the game with vocabulary that differs only in
         *   truncation, but the user has forgotten about the ones with the
         *   longer names and is thinking of something she's referred to
         *   with truncation in the past, when the longer-named objects
         *   weren't around.  We're aware of all of those longer-named
         *   objects, but it's not helpful to the player, since they might
         *   currently be out of sight.
         */
        if (resolver.isGlobalScope)
            return lst;

        /* get the list of exact (i.e., untruncated) matches */
        exactLst = lst.subset(
            {x: (x.flags_ & (VocabTruncated | PluralTruncated)) == 0});

        /* 
         *   if we have any exact matches, use only the exact matches; if
         *   we don't, use the full list as is 
         */
        return (exactLst.length() != 0 ? exactLst : lst);
    }
;

/*
 *   Basic noun phrase list production class. 
 */
class NounListProd: BasicProd
;

/*
 *   Basic "layered" noun phrase production.  It's often useful to define
 *   a grammar rule that simply defers to an underlying grammar rule; we
 *   make this simpler by defining this class that automatically delegates
 *   resolveNouns to the underlying noun phrase given by the property np_. 
 */
class LayeredNounPhraseProd: NounPhraseProd
    resolveNouns(resolver, results)
    {
        /* let the underlying match tree object do the work */
        return np_.resolveNouns(resolver, results);
    }
;


/* ------------------------------------------------------------------------ */
/*
 *   A single noun is sometimes required where, structurally, a list is
 *   not allowed.  Single nouns should not be used to prohibit lists where
 *   there is no structural reason for the prohibition - these should be
 *   used only where it doesn't make sense to use a list structurally.  
 */
class SingleNounProd: NounPhraseProd
    resolveNouns(resolver, results)
    {
        /* tell the results object we're resolving a single-object slot */
        results.beginSingleObjSlot();

        /* resolve the underlying noun phrase */
        local lst = np_.resolveNouns(resolver, results);

        /* tell the results object we're done with the single-object slot */
        results.endSingleObjSlot();

        /* make sure the list has only one element */
        if (lst.length() > 1)
            results.uniqueObjectRequired(getOrigText(), lst);

        /* return the results */
        return lst;
    }
;

/*
 *   A user could attempt to use a noun list where a single noun is
 *   required.  This is not a grammatical error, so we accept it
 *   grammatically; however, for disambiguation purposes we score it lower
 *   than a singleNoun production with only one noun phrase, and if we try
 *   to resolve it, we'll fail with an error.  
 */
class SingleNounWithListProd: NounPhraseProd
    resolveNouns(resolver, results)
    {
        /* note the problem */
        results.singleObjectRequired(getOrigText());

        /* 
         *   In case we have any unknown words or other detrimental
         *   aspects, resolve the underlying noun list as well, so we can
         *   score even lower.  Note that doing this after noting our
         *   single-object-required problem will avoid any interactive
         *   resolution (such as asking for disambiguation information),
         *   since once we get to the point where we're ready to do
         *   anything interactive, the single-object-required results
         *   notification will fail with an error, so we won't make it to
         *   this point during interactive resolution.  
         */
        np_.resolveNouns(resolver, results);

        /* we have no resolution */
        return [];
    }
;

/*
 *   A topic is a noun phrase used in commands like "ask <person> about
 *   <topic>."  For our purposes, this works as an ordinary single noun
 *   production.  
 */
class TopicProd: SingleNounProd
    /* get the original text and tokens from the underlying phrase */
    getOrigTokenList() { return np_.getOrigTokenList(); }
    getOrigText() { return np_.getOrigText(); }

    resolveNouns(resolver, results)
    {
        /* note that we're entering a topic slot */
        results.beginTopicSlot();

        /* do the inherited work */
        local ret = inherited(resolver, results);

        /* we're leaving the topic slot */
        results.endTopicSlot();

        /* return the resolved list */
        return ret;
    }
;

/*
 *   A literal is a string enclosed in quotes. 
 */
class LiteralProd: BasicProd
;

/*
 *   Basic class for pronoun phrases.  The specific pronouns are
 *   language-dependent; each instance should define its pronounType
 *   property to an appropriate PronounXxx constant.
 */
class PronounProd: NounPhraseProd
    resolveNouns(resolver, results)
    {
        local lst;

        /* 
         *   check for a valid anaphoric binding (i.e., a back-reference to
         *   an object mentioned earlier in the current command: ASK BOB
         *   ABOUT HIMSELF) 
         */
        lst = checkAnaphoricBinding(resolver, results);

        /* 
         *   if we didn't get an anaphoric binding, find an antecedent from
         *   a previous command 
         */
        if (lst == nil)
        {
            /* ask the resolver to get the antecedent */
            lst = resolver.resolvePronounAntecedent(
                pronounType, self, results, isPossessive);

            /* if there are no results, note the error */
            if (lst == [])
                results.noMatchForPronoun(pronounType,
                                          getOrigText().toLower());

            /* remember the pronoun type in each ResolveInfo */
            lst.forEach({x: x.pronounType_ = pronounType});

            /* 
             *   If the pronoun is singular, but we got multiple potential
             *   antecedents, it means that the previous command had
             *   multiple noun slots (as in UNLOCK DOOR WITH KEY) and
             *   didn't want to decide a priori which one is the antecedent
             *   for future pronouns.  So, we have to decide now which of
             *   the potential antecedents to use as the actual antecedent.
             *   Choose the most logical, if there's a clearly more logical
             *   one.  
             */
            if (!isPlural && lst.length() > 1)
            {
                /* filter the phrase using the normal disambiguation */
                lst = resolver.filterAmbiguousNounPhrase(lst, 1, self);
                
                /* 
                 *   if that leaves more than one item, pick the first one
                 *   and mark it as unclearly disambiguated 
                 */
                if (lst.length() > 1)
                {
                    /* arbitrarily keep the first item only */
                    lst = lst.sublist(1, 1);
                    
                    /* mark it as an arbitrary choice */
                    lst[1].flags_ |= UnclearDisambig;
                }
            }
        }

        /* note that we have phrase involving a pronoun */
        results.notePronoun();

        /* return the result list */
        return lst;
    }

    /* 
     *   our pronoun specifier - this must be set in each rule instance to
     *   one of the PronounXxx constants to specify which pronoun to use
     *   when resolving the pronoun phrase 
     */
    pronounType = nil

    /* is this a possessive usage? */
    isPossessive = nil

    /* 
     *   Is this pronoun a singular or a plural?  A pronoun like "it" or
     *   "he" is singular, because it refers to a single antecedent; "them"
     *   is plural.  Language modules that define their own custom pronoun
     *   subclasses should override this as needed.  
     */
    isPlural = nil

    /*
     *   Check for an anaphoric binding.  Returns a list (which is allowed
     *   to be empty) if this can refer back to an earlier noun phrase in
     *   the same command, nil if not.  By default, we consider pronouns
     *   to be non-anaphoric, meaning they refer to something from a
     *   previous sentence, not something in this same sentence.  In most
     *   languages, pronouns don't refer to objects in other noun phrases
     *   within the same predicate unless they're reflexive.  
     */
    checkAnaphoricBinding(resolver, results) { return nil; }
;

/*
 *   For simplicity, define subclasses of PronounProd for the basic set of
 *   pronouns found in most languages.  Language-specific grammar
 *   definitions can choose to use these or not, and can add their own
 *   extra subclasses as needed for types of pronouns we don't define
 *   here.  
 */
class ItProd: PronounProd
    pronounType = PronounIt
;

class ThemProd: PronounProd
    pronounType = PronounThem
    isPlural = true
;

class HimProd: PronounProd
    pronounType = PronounHim
;

class HerProd: PronounProd
    pronounType = PronounHer
;

class MeProd: PronounProd
    pronounType = PronounMe
;

class YouProd: PronounProd
    pronounType = PronounYou
;

/*
 *   Third-person anaphoric reflexive pronouns.  These refer to objects
 *   that appeared earlier in the sentence: "ask bob about himself."  
 */
class ReflexivePronounProd: PronounProd
    resolveNouns(resolver, results)
    {
        /* ask the resolver for the reflexive pronoun binding */
        local lst = resolver.getReflexiveBinding(pronounType);

        /* 
         *   if the result is empty, the verb will provide the binding
         *   later, on a second pass 
         */
        if (lst == [])
            return lst;

        /* 
         *   If the result is nil, the verb is saying that the reflexive
         *   pronoun makes no sense internally within the predicate
         *   structure.  In this case, or if we did get a list that
         *   doesn't agree with the pronoun type (in number or gender, for
         *   example), consider the reflexive to refer back to the actor,
         *   for a construct such as TELL BOB TO HIT HIMSELF.  However,
         *   only do this if the issuer and target actor aren't the same,
         *   since we generally don't refer to the PC in the third person.
         */
        if ((lst == nil || !checkAgreement(lst))
            && resolver.actor_ != resolver.issuer_)
        {
            /* try treating the actor as the reflexive pronoun */
            lst = [new ResolveInfo(resolver.actor_, 0, self)];
        }

        /* if the list is nil, it means reflexives aren't allowed here */
        if (lst == nil)
        {
            results.reflexiveNotAllowed(pronounType, getOrigText.toLower());
            return [];
        }

        /*
         *   Check the list for agreement (in gender, number, and so on).
         *   Don't bother if the list is empty, as this is the action's
         *   way of telling us that it doesn't have a binding for us yet
         *   but will provide one on a subsequent attempt at re-resolving
         *   this phrase.  
         */
        if (!checkAgreement(lst))
            results.wrongReflexive(pronounType, getOrigText().toLower());

        /* return the result list */
        return lst;
    }

    /*
     *   Check that the binding we found for our reflexive pronoun agrees
     *   with the pronoun in gender, number, and anything else that it has
     *   to agree with.  This must be defined by each concrete subclass.
     *   Note that language-specific subclasses can and *should* override
     *   this to test agreement for the local language's rules.
     *   
     *   This should return true if we agree, nil if not.  
     */
    checkAgreement(lst) { return true; }
;

class ItselfProd: ReflexivePronounProd
    pronounType = PronounIt
;

class ThemselvesProd: ReflexivePronounProd
    pronounType = PronounThem
;

class HimselfProd: ReflexivePronounProd
    pronounType = PronounHim
;

class HerselfProd: ReflexivePronounProd
    pronounType = PronounHer
;

/*
 *   The special 'all' constructions are full noun phrases. 
 */
class EverythingProd: BasicProd
    resolveNouns(resolver, results)
    {
        local lst;

        /* check to make sure 'all' is allowed */
        if (!resolver.allowAll())
        {
            /* it's not - flag an error and give up */
            results.allNotAllowed();
            return [];
        }

        /* get the 'all' list */
        lst = resolver.getAll(self);

        /* 
         *   set the "always announce" flag for each item - the player
         *   didn't name these items specifically, so always show what we
         *   chose 
         */
        foreach (local cur in lst)
            cur.flags_ |= AlwaysAnnounce | MatchedAll;

        /* make sure there's something in it */
        if (lst.length() == 0)
            results.noMatchForAll();

        /* return the list */
        return lst;
    }

    /* match Collective objects instead of their individuals */
    filterForCollectives = true
;


/* ------------------------------------------------------------------------ */
/*
 *   Basic exclusion list ("except the silver one") production base class. 
 */
class ExceptListProd: BasicProd
;

/*
 *   Basic "but" rule, which selects a list of plurals minus a list of
 *   specifically excepted objects.  This can be used to construct more
 *   specific production classes for things like "everything but the book"
 *   and "all books except the red ones".
 *   
 *   In each grammar rule based on this class, the 'except_' property must
 *   be set to a suitable noun phrase for the exception list.  We'll
 *   resolve this list and remove the objects in it from our main list.
 */
class ButProd: NounPhraseProd
    resolveNouns(resolver, results)
    {
        local mainList;
        local butList;
        local butRemapList;
        local action;
        local role;
        local remapProp;

        /* get our main list of items to include */
        mainList = getMainList(resolver, results);

        /* filter out truncated matches if we have any exact matches */
        mainList = filterTruncations(mainList, resolver);

        /* 
         *   resolve the 'except' list - use an 'except' resolver based on
         *   our list so that we resolve these objects in the scope of our
         *   main list 
         */
        butList = except_.resolveNouns(
            new ExceptResolver(mainList, getOrigText(), resolver),
            new ExceptResults(results));

        /* if the exception list is empty, tell the results about it */
        if (butList == [])
            results.noteEmptyBut();

        /* 
         *   Get the remapping property for this object role for this
         *   action.  This property applies to each of the objects we're
         *   resolving, and tells us if the resolved object is going to
         *   remap its handling of this action when the object is used in
         *   this role.  For example, the Take action's remapping property
         *   for the direct object would usually be remapDobjTake, so
         *   book.remapDobjTake would tell us if TAKE BOOK were going to
         *   be remapped. 
         */
        action = resolver.getAction();
        role = resolver.whichObject;
        remapProp = action.getRemapPropForRole(role);

        /* get the list of simple synonym remappings for the 'except' list */
        butRemapList = butList.mapAll(
            {x: action.getSimpleSynonymRemap(x.obj_, role, remapProp)});

        /* 
         *   scan the 'all' list, and remove each item that appears in the
         *   'except' list 
         */
        for (local i = 1, local len = mainList.length() ; i <= len ; ++i)
        {
            local curRemap;
            
            /* get the current 'all' list element */
            local cur = mainList[i].obj_;

            /* get the simple synonym remapping for this item, if any */
            curRemap = action.getSimpleSynonymRemap(cur, role, remapProp);
            
            /* 
             *   If this item appears in the 'except' list, remove it.
             *   
             *   Similarly, if this item is remapped to something that
             *   appears in the 'except' list, remove it.
             *   
             *   Similarly, if something in the 'except' list is remapped
             *   to this item, remove this item. 
             */
            if (butList.indexWhich({x: x.obj_ == cur}) != nil
                || butRemapList.indexWhich({x: x == cur}) != nil
                || butList.indexWhich({x: x.obj_ == curRemap}) != nil)
            {
                /* remove it and adjust our loop counters accordingly */
                mainList = mainList.removeElementAt(i);
                --i;
                --len;
            }
        }

        /* if that doesn't leave anything, it's an error */
        if (mainList == [])
            flagAllExcepted(resolver, results);

        /* perform the final filtering on the list for our subclass */
        mainList = filterFinalList(mainList);

        /* add any flags to the result list that our subclass indicates */
        foreach (local cur in mainList)
            cur.flags_ |= addedFlags;

        /* note the matched objects in the results */
        results.noteMatches(mainList);

        /* return whatever we have left after the exclusions */
        return mainList;
    }

    /* get my main list, which is the list of items to include */
    getMainList(resolver, results) { return []; }

    /* flag an error - everything has been excluded by the 'but' list */
    flagAllExcepted(resolver, results) { }

    /* filter the final list - by default we just return the same list */
    filterFinalList(lst) { return lst; }

    /* by default, add no extra flags to our resolved object list */
    addedFlags = 0
;


/* ------------------------------------------------------------------------ */
/*
 *   Base class for "all but" rules, which select everything available
 *   except for objects in a specified list of exceptions; for example, in
 *   English, "take everything but the book".  
 */
class EverythingButProd: ButProd
    /* our main list is given by the "all" list */
    getMainList(resolver, results)
    {
        /* check to make sure 'all' is allowed */
        if (!resolver.allowAll())
        {
            /* it's not - flag an error and give up */
            results.allNotAllowed();
            return [];
        }

        /* return the 'all' list */
        return resolver.getAll(self);
    }

    /* flag an error - our main list has been completely excluded */
    flagAllExcepted(resolver, results)
    {
        results.noMatchForAllBut();
    }

    /*         
     *   set the "always announce" flag for each item - the player didn't
     *   name the selected items specifically, so always show what we
     *   chose 
     */
    addedFlags = AlwaysAnnounce

    /* match Collective objects instead of their individuals */
    filterForCollectives = true
;

/*
 *   Base class for "list but" rules, which select everything in an
 *   explicitly provided list minus a set of exceptions; for example, in
 *   English, "take all of the books except the red ones".
 *   
 *   Subclasses defining grammar rules must set the 'np_' property to the
 *   main noun list; we'll resolve this list to find the objects to be
 *   included before exclusions are applied.  
 */
class ListButProd: ButProd
    /* our main list is given by the 'np_' subproduction */
    getMainList(resolver, results)
    {
        return np_.resolveNouns(resolver, results);
    }

    /* flag an error - everything has been excluded */
    flagAllExcepted(resolver, results)
    {
        results.noMatchForListBut();
    }

    /* 
     *   set the "unclear disambig" flag in our results, so we provide an
     *   indication of which object we chose 
     */
    addedFlags = UnclearDisambig
;
    

/* ------------------------------------------------------------------------ */
/*
 *   Pre-resolved phrase production.  This isn't normally used in any
 *   actual grammar; instead, this is for use when building actions
 *   programmatically.  This allows us to fill in an action tree when we
 *   already know the object we want to resolve.  
 */
class PreResolvedProd: BasicProd
    construct(obj)
    {
        /* if it's not a collection, we need to make it a list */
        if (!obj.ofKind(Collection))
        {
            /* if it's not already a ResolveInfo, wrap it in a ResolveInfo */
            if (!obj.ofKind(ResolveInfo))
                obj = new ResolveInfo(obj, 0, self);

            /* the resolved object list is simply the one ResolveInfo */
            obj = [obj];
        }

        /* store the new ResolveInfo list */
        obj_ = obj;
    }

    /* resolve the nouns: this is easy, since we started out resolved */
    resolveNouns(resolver, results)
    {
        /* return our pre-resolved object */
        return obj_;
    }

    /* 
     *   Our pre-resolved object result.  This is a list containing a
     *   single ResolveInfo representing our resolved object, since this is
     *   the form required by callers of resolveNouns.  
     */
    obj_ = nil
;

/*
 *   A pre-resolved *ambiguous* noun phrase.  This is used when the game
 *   or library wants to suggest a specific set of objects for a new
 *   action, then ask which one to use.  
 */
class PreResolvedAmbigProd: DefiniteNounProd
    construct(objs, asker, phrase)
    {
        /* remember my list of possible objects as a resolved object list */
        objs_ = objs.mapAll({x: new ResolveInfo(x, 0, nil)});

        /* remember the ResolveAsker to use */
        asker_ = asker;

        /* remember the noun phrase to use in disambiguation questions */
        phrase_ = phrase;
    }

    resolveNouns(resolver, results)
    {
        /* resolve our list using definite-phrase rules */
        return resolveDefinite(asker_, phrase_, objs_,
                               self, resolver, results);
    }

    /* my pre-resolved list of ambiguous objects */
    objs_ = nil

    /* the noun phrase to use in disambiguation questions */
    phrase_ = nil

    /* the ResolveAsker to use when prompting for the selection */
    asker_ = nil
;

/*
 *   Pre-resolved literal phrase production
 */
class PreResolvedLiteralProd: BasicProd
    construct(txt)
    {
        /*
         *   If the argument is a ResolveInfo, assume its obj_ property
         *   gives the literal string, and retrieve the string.  
         */
        if (txt.ofKind(ResolveInfo))
            txt = txt.obj_;

        /* save the text */
        text_ = txt;
    }

    /* get the text */
    getLiteralText(results, action, which) { return text_; }
    getTentativeLiteralText() { return text_; }

    /* our underlying text */
    text_ = nil
;

/*
 *   A token list production is an internally generated placeholder when we
 *   synthesize a production rather than matching grammar, and we want to
 *   keep track of the token list that triggered the node.
 */
class TokenListProd: BasicProd
    construct(toks)
    {
        tokenList = toks;
        firstTokenIndex = 1;
        lastTokenIndex = toks.length();
    }

    /* the token list */
    tokenList = nil
;


/* ------------------------------------------------------------------------ */
/*
 *   Mix-in class for noun phrase productions that use
 *   ResolveResults.ambiguousNounPhrase().  This mix-in provides the
 *   methods that ambiguousNounPhrase() uses to keep track of past
 *   responses to the disambiguation question.  
 */
class AmbigResponseKeeper: object
    addAmbigResponse(resp)
    {
        /* add an ambiguous response to our list */
        ambigResponses_ += resp;
    }

    getAmbigResponses()
    {
        /* return our list of past interactive disambiguation responses */
        return ambigResponses_;
    }

    /* our list of saved interactive disambiguation responses */
    ambigResponses_ = []
;


/* ------------------------------------------------------------------------ */
/*
 *   Base class for noun phrase productions with definite articles. 
 */
class DefiniteNounProd: NounPhraseProd, AmbigResponseKeeper
    resolveNouns(resolver, results)
    {
        /* resolve our underlying noun phrase using definite rules */
        return resolveDefinite(ResolveAsker, np_.getOrigText(),
                               np_.resolveNouns(resolver, results),
                               self, resolver, results);
    }

    /*
     *   Resolve an underlying phrase using definite noun phrase rules. 
     */
    resolveDefinite(asker, origText, lst, responseKeeper, resolver, results)
    {
        local scopeList;
        local fullList;

        /* filter the list to remove truncations if we have exact matches */
        lst = filterTruncations(lst, resolver);

        /* 
         *   Remember the current list, before filtering for logical
         *   matches and before filtering out equivalents, as our full
         *   scope list.  If we have to ask for clarification, this is the
         *   scope of the clarification.  
         */
        scopeList = lst;

        /* filter for possessive qualification strength */
        lst = resolver.filterPossRank(lst, 1);

        /* filter out the obvious mismatches */
        lst = resolver.filterAmbiguousNounPhrase(lst, 1, self);

        /* 
         *   remember the current list as the full match list, before
         *   filtering equivalents 
         */
        fullList = lst;

        /* 
         *   reduce the list to include only one of each set of
         *   equivalents; do this only if the results object allows this
         *   kind of filtering 
         */
        if (results.allowEquivalentFiltering)
            lst = resolver.filterAmbiguousEquivalents(lst, self);

        /* do any additional subclass-specific filtering on the list */
        lst = reduceDefinite(lst, resolver, results);

        /* 
         *   This is (explicitly or implicitly) a definite noun phrase, so
         *   we must resolve to exactly one object.  If the list has more
         *   than one object, we must disambiguate it.  
         */
        if (lst.length() == 0)
        {
            /* there are no objects matching the phrase */
            results.noMatch(resolver.getAction(), origText);
        }
        else if (lst.length() > 1)
        {
            /* 
             *   the noun phrase is ambiguous - pass in the full list
             *   (rather than the list with the redundant equivalents
             *   weeded out) so that we can display the appropriate
             *   choices for multiple equivalent items 
             */
            lst = results.ambiguousNounPhrase(
                responseKeeper, asker, origText, lst, fullList,
                scopeList, 1, resolver);
        }

        /* note the matched objects in the results */
        results.noteMatches(lst);

        /* return the results */
        return lst;
    }

    /*
     *   Do any additional subclass-specific filtering to further reduce
     *   the list before we decide whether or not we have sufficient
     *   specificity.  We call this just before deciding whether or not to
     *   prompt for more information ("which book do you mean...?").  By
     *   default, this simply returns the same list unchanged; subclasses
     *   that have some further way of narrowing down the options can use
     *   this opportunity to apply that extra narrowing.  
     */
    reduceDefinite(lst, resolver, results) { return lst; }
;

/*
 *   Base class for a plural production 
 */
class PluralProd: NounPhraseProd
    /*
     *   Basic plural noun resolution.  We'll retrieve the matching objects
     *   and filter them using filterPluralPhrase.  
     */
    basicPluralResolveNouns(resolver, results)
    {
        local lst;
        
        /* resolve the underlying noun phrase */
        lst = np_.resolveNouns(resolver, results);

        /* if there's nothing in it, it's an error */
        if (lst.length() == 0)
            results.noMatch(resolver.getAction(), np_.getOrigText());

        /* filter out truncated matches if we have any exact matches */
        lst = filterTruncations(lst, resolver);

        /* filter the plurals for the logical subset and return the result */
        lst = resolver.filterPluralPhrase(lst, self);

        /* if we have a list, sort it by pluralOrder */
        if (lst != nil)
            lst = lst.sort(SortAsc,
                           {a, b: a.obj_.pluralOrder - b.obj_.pluralOrder});

        /* note the matches */
        results.noteMatches(lst);

        /* note the plural in the results */
        results.notePlural();

        /* return the result */
        return lst;
    }

    /*
     *   Get the verify "keepers" for a plural phrase.
     *   
     *   If the "filter plural matches" configuration flag is set to true,
     *   we'll return the subset of items which are logical for this
     *   command.  If the filter flag is nil, we'll simply return the full
     *   set of vocabulary matches without any filtering.  
     */
    getVerifyKeepers(results)
    {
        /* check the global "filter plural matches" configuration flag */
        if (gameMain.filterPluralMatches)
        {
            local sub;
            
            /* get the subset of items that don't exclude plurals */
            sub = results.subset({x: !x.excludePluralMatches});

            /* 
             *   if that's non-empty, use it as the result; otherwise, just
             *   use the original list 
             */
            return (sub.length() != 0 ? sub : results);
        }
        else
        {
            /* we don't want any filtering */
            return results;
        }
    }
;

/*
 *   A plural phrase that explicitly selects all of matching objects.  For
 *   English, this would be a phrase like "all of the marbles".  This type
 *   of phrase matches all of the objects that match the name in the
 *   plural, except that if we have a collective object and we also have
 *   objects that are part of the collective (such as a bag of marbles and
 *   some individual marbles), we'll omit the collective, and match only
 *   the individual items.
 *   
 *   Grammar rule instantiations in language modules should set property
 *   np_ to the plural phrase match tree.  
 */
class AllPluralProd: PluralProd
    resolveNouns(resolver, results)
    {
        /* return the basic plural resolution */
        return basicPluralResolveNouns(resolver, results);
    }

    /* 
     *   since the player explicitly told us to use ALL of the matching
     *   objects, keep everything in the verify list, logical or not 
     */
    getVerifyKeepers(results) { return results; }

    /* prefer to keep individuals over collectives */
    filterForCollectives = nil
;

/*
 *   A plural phrase qualified by a definite article ("the books").  This
 *   type of phrasing doesn't specify anything about the specific number of
 *   items involved, except that they number more than one.
 *   
 *   In most cases, we take this to imply that all of the matching objects
 *   are intended to be included, with one exception: when we have an
 *   object that can serve as a collective for some of the other objects,
 *   we match only the collective but not the other objects.  For example,
 *   if we type "take marbles," and we have five marbles and a bag of
 *   marbles that serves as a collective object for three of the five
 *   marbles, we'll match the bag and two marbles not in the bag, but NOT
 *   the marbles that are in the bag.  This is usually desirable when
 *   there's a collective object, since it applies the command to the
 *   object standing in for the group rather than applying the command one
 *   by one to each of the individuals in the group.  
 */
class DefinitePluralProd: PluralProd
    resolveNouns(resolver, results)
    {
        /* return the basic plural resolution */
        return basicPluralResolveNouns(resolver, results);
    }

    /* prefer to keep collectives instead of their individuals */
    filterForCollectives = true
;

/*
 *   Quantified plural phrase.  
 */
class QuantifiedPluralProd: PluralProd
    /* 
     *   Resolve the main noun phrase.  By default, we simply resolve np_,
     *   but we make this separately overridable to allow this class to be
     *   subclassed for quantifying other types of plural phrases.
     *   
     *   If this is unable to resolve the list, it can flag an appropriate
     *   error via the results object and return nil.  If this routine
     *   returns nil, our main resolver will simply return an empty list
     *   without further flagging of any errors.  
     */
    resolveMainPhrase(resolver, results)
    {
        /* resolve the main noun phrase */
        return np_.resolveNouns(resolver, results);
    }

    /* 
     *   get the quantity specified - by default, this comes from the
     *   quantifier phrase in "quant_" 
     */
    getQuantity() { return quant_.getval(); }

    /* resolve the noun phrase */
    resolveNouns(resolver, results)
    {
        local lst;
        local num;

        /* resolve the underlying noun phrase */
        if ((lst = resolveMainPhrase(resolver, results)) == nil)
            return [];

        /* filter out truncated matches if we have any exact matches */
        lst = filterTruncations(lst, resolver);

        /* sort the list in ascending order of pluralOrder */
        if (lst != nil)
            lst = lst.sort(SortAsc,
                           {a, b: a.obj_.pluralOrder - b.obj_.pluralOrder});

        /* get the quantity desired */
        num = getQuantity();

        /* 
         *   if we have at least the desired number, arbitrarily choose
         *   the desired number; otherwise, it's an error 
         */
        if (num == 0)
        {
            /* zero objects makes no sense */
            results.zeroQuantity(np_.getOrigText());
        }
        else
        {
            local qsum;
            
            /* if we have too many, disambiguate */
            if (lst.length() >= num)
            {
                local scopeList;

                /* remember the list of everything in scope that matches */
                scopeList = lst;

                /* filter for possessive qualifier strength */
                lst = resolver.filterPossRank(lst, num);

                /*
                 *   Use the normal disambiguation ranking to find the best
                 *   set of possibilities.  
                 */
                lst = resolver.filterAmbiguousNounPhrase(lst, num, self);

                /* 
                 *   If that left us with more than we're looking for, call
                 *   our selection routine to select the subset.  If it
                 *   left us with too few, note it in the results.  
                 */
                if (lst.length() > num)
                {
                    /* select the desired exact count */
                    lst = selectExactCount(lst, num, scopeList,
                                           resolver, results);
                }
            }

            /* 
             *   Check to make sure we have enough items matching.  Go by
             *   the 'quant_' property of the ResolveInfo entries, since we
             *   might have a single ResolveInfo object that represents a
             *   quantity of objects from the player's perspective. 
             */
            qsum = 0;
            lst.forEach({x: qsum += x.quant_});
            if (qsum < num)
            {
                /* note in the results that there aren't enough matches */
                results.insufficientQuantity(np_.getOrigText(), lst, num);
            }
        }

        /* note the matched objects in the results */
        results.noteMatches(lst);

        /* return the results */
        return lst;
    }

    /*
     *   Select the desired number of matches from what the normal
     *   disambiguation filtering leaves us with.
     *   
     *   Note that this will never be called with 'num' larger than the
     *   number in the current list.  This is only called to select a
     *   smaller subset than we currently have.
     *   
     *   By default, we'll simply select an arbitrary subset, since we
     *   simply want any 'num' of the matches.  This can be overridden if
     *   other behaviors are needed.  
     */
    selectExactCount(lst, num, scopeList, resolver, results)
    {
        /* 
         *   If we want less than what we actually got, arbitrarily pick
         *   the first 'num' elements; otherwise, return what we have.  
         */
        if (lst.length() > num)
            return lst.sublist(1, num);
        else
            return lst;
    }

    /* 
     *   Since the player explicitly told us to use a given number of
     *   matching objects, keep the required number, logical or not.  
     */
    getVerifyKeepers(results)
    {
        /* get the quantity desired */
        local num = getQuantity();

        /* 
         *   if we have at least the number required, arbitrarily choose
         *   the initial subset of the desired length; otherwise, use them
         *   all 
         */
        if (results.length() > num)
            return results.sublist(1, num);
        else
            return results;
    }
;

/*
 *   Exact quantified plural phrase.  This is similar to the normal
 *   quantified plural, but has the additional requirement of matching an
 *   unambiguous set of the exact given number ("the five books" means
 *   that we expect to find exactly five books matching the phrase - no
 *   fewer, and no more).  
 */
class ExactQuantifiedPluralProd: QuantifiedPluralProd, AmbigResponseKeeper
    /*
     *   Select the desired number of matches.  Since we want an exact set
     *   of matches, we'll run disambiguation on the set.  
     */
    selectExactCount(lst, num, scopeList, resolver, results)
    {
        local fullList;
        
        /* remember the list before filtering for redundant equivalents */
        fullList = lst;

        /* reduce the list by removing redundant equivalents, if allowed */
        if (results.allowEquivalentFiltering)
            lst = resolver.filterAmbiguousEquivalents(lst, self);

        /* 
         *   if the reduced list has only one element, everything in the
         *   original list must have been equivalent, so arbitrarily pick
         *   the desired number of items from the original list
         */
        if (lst.length() == 1)
            return fullList.sublist(1, num);
        
        /* we still have too many items, so disambiguate the results */
        return results.ambiguousNounPhrase(
            self, ResolveAsker, np_.getOrigText(),
            lst, fullList, scopeList, num, resolver);
    }

    /* get the keepers in the verify stage */
    getVerifyKeepers(results)
    {
        /* 
         *   keep everything: we want an exact quantity, so we want the
         *   keepers to match the required quantity on their own, without
         *   any arbitrary paring down 
         */
        return results;
    }
;

/*
 *   Noun phrase with an indefinite article 
 */
class IndefiniteNounProd: NounPhraseProd
    /* 
     *   resolve the main phrase - this is separately overridable to allow
     *   subclassing 
     */
    resolveMainPhrase(resolver, results)
    {
        /* by default, resolve the main noun phrase */
        return np_.resolveNouns(resolver, results);
    }
    
    resolveNouns(resolver, results)
    {
        local lst;
        local allEquiv = nil;
        
        /* resolve the underlying list */
        if ((lst = resolveMainPhrase(resolver, results)) == nil)
            return [];

        /* filter out truncated matches if we have any exact matches */
        lst = filterTruncations(lst, resolver);

        /* see what we found */
        if (lst.length() == 0)
        {
            /* it turned up nothing - note the problem */
            results.noMatch(resolver.getAction(), np_.getOrigText());
        }
        else if (lst.length() > 1)
        {
            /* 
             *   There are multiple objects, but the phrase is indefinite,
             *   which means that it doesn't refer to a specific matching
             *   object but could refer to any of them.  
             */

            /* start by noting if the choices are all equivalent */
            allEquiv = areAllEquiv(lst);

            /*   
             *   Filter using possessive qualifier strength and then normal
             *   disambiguation ranking to find the best set of
             *   possibilities, then pick which we want.  
             */
            lst = resolver.filterPossRank(lst, 1);
            lst = resolver.filterAmbiguousNounPhrase(lst, 1, self);
            lst = selectFromList(resolver, results, lst);
        }

        /* 
         *   Set the "unclear disambiguation" flag on the item we picked -
         *   our selection was arbitrary, so it's polite to let the player
         *   know which we chose.  However, don't do this if the possible
         *   matches were all equivalent to start with, as the player's
         *   input must already have been as specific as we can be in
         *   reporting the choice.  
         */
        if (lst.length() == 1 && !allEquiv)
            lst[1].flags_ |= UnclearDisambig;

        /* note the matched objects in the results */
        results.noteMatches(lst);

        /* note that this is an indefinite phrasing */
        results.noteIndefinite();

        /* return the results */
        return lst;
    }

    /* are all of the items in the resolve list equivalents? */
    areAllEquiv(lst)
    {
        local first = lst[1].obj_;
        
        /* check each item to see if it's equivalent to the first */
        for (local i = 2, local cnt = lst.length() ; i <= cnt ; ++i)
        {
            /* 
             *   if this one isn't equivalent to the first, then they're
             *   not all equivalent 
             */
            if (!first.isVocabEquivalent(lst[i].obj_))
                return nil;
        }

        /* we didn't find any non-equivalents, so they're all equivalents */
        return true;
    }

    /*
     *   Select an item from the list of potential matches, given the list
     *   sorted from most likely to least likely (according to the
     *   resolver's ambiguous match filter).  We'll ask the resolver to
     *   make the selection, because indefinite noun phrases can mean
     *   different things in different contexts.  
     */
    selectFromList(resolver, results, lst)
    {
        /* ask the resolver to select */
        return resolver.selectIndefinite(results, lst, 1);
    }
    
;

/*
 *   Noun phrase explicitly asking us to choose an object arbitrarily
 *   (with a word like "any").  This is similar to the indefinite noun
 *   phrase, but differs in that this phrase is *explicitly* arbitrary,
 *   rather than merely indefinite.  
 */
class ArbitraryNounProd: IndefiniteNounProd
    /*
     *   Select an object from a list of potential matches.  Since the
     *   choice is explicitly arbitrary, we simply choose the first
     *   (they're in order from most likely to least likely, so this will
     *   choose the most likely).  
     */
    selectFromList(resolver, results, lst)
    {
        /* simply select the first item */
        return lst.sublist(1, 1);
    }
;

/*
 *   Noun phrase with an indefinite article and an exclusion ("any of the
 *   books except the red one") 
 */
class IndefiniteNounButProd: ButProd
    /* resolve our main phrase */
    resolveMainPhrase(resolver, results)
    {
        /* note that this is an indefinite phrasing */
        results.noteIndefinite();

        /* by default, simply resolve the underlying noun phrase */
        return np_.resolveNouns(resolver, results);
    }

    /* get our main list */
    getMainList(resolver, results)
    {
        local lst;
        
        /* resolve the underlying list */
        if ((lst = resolveMainPhrase(resolver, results)) == nil)
            return [];

        /* filter for possessive qualifier strength */
        lst = resolver.filterPossRank(lst, 1);

        /* filter it to pick the most likely objects */
        lst = resolver.filterAmbiguousNounPhrase(lst, 1, self);

        /* return the filtered list */
        return lst;
    }

    /* flag an error - everything has been excluded */
    flagAllExcepted(resolver, results)
    {
        results.noMatchForListBut();
    }

    /* filter the final list */
    filterFinalList(lst)
    {
        /* we want to keep only one item - arbitrarily take the first one */
        return (lst.length() == 0 ? [] : lst.sublist(1, 1));
    }

    /* 
     *   set the "unclear disambig" flag in our results, so we provide an
     *   indication of which object we chose 
     */
    addedFlags = UnclearDisambig
;

/*
 *   A qualified plural phrase explicitly including two objects (such as,
 *   in English, "both books").  
 */
class BothPluralProd: ExactQuantifiedPluralProd
    /* the quantity specified by a "both" phrase is 2 */
    getQuantity() { return 2; }
;

/* ------------------------------------------------------------------------ */
/*
 *   Possessive adjectives
 */

class PossessivePronounAdjProd: PronounProd
    /*
     *   Possessive pronouns can refer to the earlier noun phrases of the
     *   same predicate, which is to say that they're anaphoric.  For
     *   example, in GIVE BOB HIS BOOK, 'his' refers to Bob.  
     */
    checkAnaphoricBinding(resolver, results)
    {
        local lst;

        /* if we simply can't be an anaphor, there's no binding */
        if (!canBeAnaphor)
            return nil;

        /* ask the resolver for the reflexive binding, if any */
        lst = resolver.getReflexiveBinding(pronounType);

        /*
         *   If there's no binding from the verb, or it doesn't match in
         *   number and gender, try an anaphoric binding from the actor. 
         */
        if (lst == nil || (lst != [] && !checkAnaphorAgreement(lst)))
        {
            /* get the actor's anaphoric possessive binding */
            local obj = resolver.actor_.getPossAnaphor(pronounType);

            /* if we got an object or a list, make a resolve list */
            if (obj != nil)
            {
                if (obj.ofKind(Collection))
                    lst = obj.mapAll({x: new ResolveInfo(x, 0, self)});
                else
                    lst = [new ResolveInfo(obj, 0, self)];
            }
        }

        /* 
         *   If we got a binding, make sure we agree in number and gender;
         *   if not, don't use the anaphoric form.  This isn't an error;
         *   it just means we're falling back on the regular antecedent
         *   binding.  If we have an empty list, it means the action isn't
         *   ready to tell us the binding yet, so we can't verify it yet.  
         */
        if (lst != nil && (lst == [] || checkAnaphorAgreement(lst)))
            return lst;

        /* don't use an anaphoric binding */
        return nil;
    }

    /* this is a possessive usage of the pronoun */
    isPossessive = true

    /*
     *   Can we be an anaphor?  By default, we consider third-person
     *   possessive pronouns to be anaphoric, and others to be
     *   non-anaphoric.  For example, in GIVE BOB MY BOOK, MY always refers
     *   to the speaker, so it's clearly not anaphoric within the sentence.
     */
    canBeAnaphor = true

    /* 
     *   Check agreement to a given anaphoric pronoun binding.  The
     *   language module should override this for each pronoun type to
     *   ensure that the actual contents of the list agree in number and
     *   gender with this type of pronoun.  If so, return true; if not,
     *   return nil.  It's not an error or a ranking demerit if we don't
     *   agree; it just means that we'll fall back on the regular pronoun
     *   antecedent rather than trying to use an anaphoric binding.  
     */
    checkAnaphorAgreement(lst) { return true; }

    /* 
     *   By default, the "main text" of a possessive pronoun is the same as
     *   the actual token text.  Languages can override this as needed> 
     */
    getOrigMainText() { return getOrigText(); }
;

class ItsAdjProd: PossessivePronounAdjProd
    pronounType = PronounIt
;

class HisAdjProd: PossessivePronounAdjProd
    pronounType = PronounHim
;

class HerAdjProd: PossessivePronounAdjProd
    pronounType = PronounHer
;

class TheirAdjProd: PossessivePronounAdjProd
    pronounType = PronounThem
;

class YourAdjProd: PossessivePronounAdjProd
    pronounType = PronounYou
    canBeAnaphor = nil
;

class MyAdjProd: PossessivePronounAdjProd
    pronounType = PronounMe
    canBeAnaphor = nil
;

/*
 *   Possessive nouns 
 */
class PossessivePronounNounProd: PronounProd
    /* this is a possessive usage of the pronoun */
    isPossessive = true
;

class ItsNounProd: PossessivePronounNounProd
    pronounType = PronounIt
;

class HisNounProd: PossessivePronounNounProd
    pronounType = PronounHim
;

class HersNounProd: PossessivePronounNounProd
    pronounType = PronounHer
;

class TheirsNounProd: PossessivePronounNounProd
    pronounType = PronounThem
;

class YoursNounProd: PossessivePronounNounProd
    pronounType = PronounYou
;

class MineNounProd: PossessivePronounNounProd
    pronounType = PronounMe
;

/*
 *   Basic possessive phrase.  The grammar rules for these phrases must map
 *   the possessive qualifier phrase to poss_, and the noun phrase being
 *   qualified to np_.  We are based on DefiniteNounProd because we resolve
 *   the possessive qualifier as though it had a definite article.
 *   
 *   The possessive production object poss_ must define the method
 *   getOrigMainText() to return the text of its noun phrase in a format
 *   suitable for disambiguation prompts or error messages.  In English,
 *   for example, this means that the getOrigMainText() must omit the
 *   apostrophe-S suffix if present.  
 */
class BasicPossessiveProd: DefiniteNounProd
    /*
     *   To allow this class to be mixed with other classes that have
     *   mixed-in ambiguous response keepers, create a separate object to
     *   hold our ambiguous response keeper for the possessive phrase.  We
     *   will never use our own ambiguous response keeper properties, so
     *   those are available to any other production class we're mixed
     *   into.  
     */
    construct()
    {
        /* create an AmbigResponseKeeper for the possessive phrase */
        npKeeper = new AmbigResponseKeeper;
    }

    /*
     *   Resolve the possessive, and perform preliminary resolution of the
     *   qualified noun phrase.  We find the owner object and reduce the
     *   resolved objects for the qualified phrase to those owned by the
     *   owner.
     *   
     *   If we fail, we return nil.  Otherwise, we return a list of the
     *   tentatively resolved objects.  The caller can further resolve
     *   this list as needed.  
     */
    resolvePossessive(resolver, results, num)
    {
        /* resolve the underlying noun phrase being qualified */
        local lst = np_.resolveNouns(resolver, results);

        /* if we found no matches for the noun phrase, so note */
        if (lst.length() == 0)
        {
            results.noMatchPossessive(resolver.getAction(), np_.getOrigText());
            return nil;
        }

        /* 
         *   resolve the possessive phrase and reduce the list to select
         *   only the items owned by the possessor 
         */
        lst = selectWithPossessive(resolver, results, lst,
                                   np_.getOrigText(), num);

        /* return the tentative resolution list for the qualified phrase */
        return lst;
    }

    /*
     *   Resolve the possessive, and reduce the given match list by
     *   selecting only those items owned by the resolution of the
     *   possessive phrase.
     *   
     *   'num' is the number of objects we want to select.  If the noun
     *   phrase being qualified is singular, this will be 1; if it's
     *   plural, this will be nil, to indicate that there's no specific
     *   target quantity; if the phrase is something like "bob's five
     *   books," the the number will be the qualifying quantity (5, in this
     *   case).  
     */
    selectWithPossessive(resolver, results, lst, lstOrigText, num)
    {
        /* 
         *   Create the possessive resolver.  Note that we resolve the
         *   possessive phrase in the context of the resolver's indicated
         *   qualifier resolver, which might not be the same as the
         *   resolver for the overall phrase.  
         */
        local possResolver = resolver.getPossessiveResolver();
        
        /* enter a single-object slot for the possessive phrase */
        results.beginSingleObjSlot();
            
        /* resolve the underlying possessive */
        local possLst = poss_.resolveNouns(possResolver, results);

        /* perform the normal resolve list filtering */
        possLst = resolver.getAction().finishResolveList(
            possLst, resolver.whichObject, self, nil);

        /* done with the single-object slot */
        results.endSingleObjSlot();

        /*
         *   If that resolved to an empty list, return now with an empty
         *   list.  The underlying possessive resolver will have noted an
         *   error if this is indeed an error; if it's not an error, it
         *   means that we're pending resolution of the other noun phrase
         *   to resolve an anaphor in the possessive phrase.  
         */
        if (possLst == [])
        {
            /* 
             *   we must have a pending anaphor to resolve - simply return
             *   the current list without any possessive filtering 
             */
            return lst;
        }

        /*
         *   If the possessive phrase itself is singular, treat the
         *   possessive phrase as a definite phrase, requiring an
         *   unambiguous referent.  If it's plural ("the men's books"),
         *   leave it as it is, taking it to mean that we want to select
         *   things that are owned by any/all of the possessors.
         *   
         *   Note that the possessive phrase has no qualifier - any
         *   qualifier applies to the noun phrase our possessive is also
         *   qualifying, not to the possessive phrase itself.  
         */
        local owner;
        if (poss_.isPluralPossessive)
        {
            /* 
             *   The possessive phrase is plural, so don't reduce its match
             *   to a single object; instead, select all of the objects
             *   owned by any of the possessors.  The owner is anyone in
             *   the list.  
             */
            owner = possLst.mapAll({x: x.obj_});
        }
        else
        {
            /* 
             *   the possessive phrase is singular, so resolve the
             *   possessive qualifier as a definite noun 
             */
            possLst = resolveDefinite(
                ResolveAsker, poss_.getOrigMainText(), possLst,
                npKeeper, possResolver, results);

            /* 
             *   if we didn't manage to find a single resolution to the
             *   possessive phrase, we can't resolve the rest of the phrase
             *   yet 
             */
            if (possLst.length() != 1)
            {
                /* if we got more than one object, it's a separate error */
                if (possLst.length() > 1)
                    results.uniqueObjectRequired(poss_.getOrigMainText(),
                        possLst);
                
                /* we can't go on */
                return [];
            }

            /* get the resolved owner object */
            owner = [possLst[1].obj_];
        }

        /* select the objects owned by any of the owners */
        local newLst = lst.subset({x: owner.indexWhich(
            {y: x.obj_.isOwnedBy(y)}) != nil});

        /*
         *   If that didn't leave any results, try one more thing: if the
         *   owner is itself in the list of possessed objects, keep the
         *   owner.  This allows for sequences like this:
         *   
         *   >take book
         *.  Which book?
         *.  >the man's
         *.  Which man, Bob or Dave?
         *.  >bob's
         *   
         *   In this case, the qualified object list will be [bob,dave],
         *   and the owner will be [bob], so we want to keep [bob] as the
         *   result list.
         */
        if (newLst == [])
            newLst = lst.subset({x: owner.indexOf(x.obj_) != nil});

        /* use the new list we found */
        lst = newLst;

        /* 
         *   Give each item an ownership priority ranking, since the items
         *   are being qualified by owner: explicitly owned items have the
         *   highest ranking, items directly held but not explicitly owned
         *   have the second ranking, and items merely held have the
         *   lowest ranking.  If we deem the list to be ambiguous later,
         *   we'll apply the ownership priority ranking first in trying to
         *   disambiguate.  
         */
        foreach (local cur in lst)
        {
            /* 
             *   give the item a ranking: explicitly owned, directly held,
             *   or other 
             */
            if (owner.indexOf(cur.obj_.owner) != nil)
                cur.possRank_ = 2;
            else if (owner.indexWhich({x: cur.obj_.isDirectlyIn(x)}) != nil)
                cur.possRank_ = 1;
        }

        /* if we found nothing, mention it */
        if (lst.length() == 0)
        {
            results.noMatchForPossessive(owner, lstOrigText);
            return [];
        }

        /* return the reduced list */
        return lst;
    }

    /* our ambiguous response keeper */
    npKeeper = nil
;

/*
 *   Possessive phrase + singular noun phrase.  The language grammar rule
 *   must map poss_ to the possessive production and np_ to the noun
 *   phrase being qualified.
 */
class PossessiveNounProd: BasicPossessiveProd
    resolveNouns(resolver, results)
    {
        local lst;

        /* 
         *   perform the initial qualification; if that fails, give up now
         *   and return an empty list 
         */
        if ((lst = resolvePossessive(resolver, results, 1)) == nil)
            return [];

        /* now resolve the underlying list definitely */
        return resolveDefinite(ResolveAsker, np_.getOrigText(), lst, self,
                               resolver, results);
    }

    /* our AmbigResponseKeeper for the qualified noun phrase */
    npKeeper = nil
;

/*
 *   Possessive phrase + plural noun phrase.  The grammar rule must set
 *   poss_ to the possessive and np_ to the plural.  
 */
class PossessivePluralProd: BasicPossessiveProd
    resolveNouns(resolver, results)
    {
        local lst;
        
        /* perform the initial qualifier resolution */
        if ((lst = resolvePossessive(resolver, results, nil)) == nil)
            return [];

        /* filter out truncated matches if we have any exact matches */
        lst = filterTruncations(lst, resolver);

        /* filter the plurals for the logical subset */
        lst = resolver.filterPluralPhrase(lst, self);

        /* if we have a list, sort it by pluralOrder */
        if (lst != nil)
            lst = lst.sort(SortAsc,
                           {a, b: a.obj_.pluralOrder - b.obj_.pluralOrder});

        /* note the matched objects in the results */
        results.noteMatches(lst);

        /* we want everything in the list, so return what we found */
        return lst;
    }
;

/*
 *   Possessive plural with a specific quantity that must be exact
 */
class ExactQuantifiedPossessivePluralProd:
    ExactQuantifiedPluralProd, BasicPossessiveProd

    /* 
     *   resolve the main noun phrase - this is the possessive-qualified
     *   plural phrase 
     */
    resolveMainPhrase(resolver, results)
    {
        return resolvePossessive(resolver, results, getQuantity());
    }
;

/*
 *   Possessive noun used in an exclusion list.  This is for things like
 *   the "mine" in a phrase like "take keys except mine".  
 */
class ButPossessiveProd: BasicPossessiveProd
    resolveNouns(resolver, results)
    {
        /* 
         *   resolve the possessive phrase, and use it to reduce the
         *   resolver's main list (this is the list before the "except"
         *   from which are choosing items to exclude) to those items
         *   owned by the object indicated in the possessive phrase 
         */
        return selectWithPossessive(resolver, results, resolver.mainList,
                                    resolver.mainListText, nil);
    }
;

/*
 *   Possessive phrase production for disambiguation.  This base class can
 *   be used for grammar productions that match possessive phrases in
 *   disambiguation prompt ("which book do you mean...?") responses. 
 */
class DisambigPossessiveProd: BasicPossessiveProd, DisambigProd
    resolveNouns(resolver, results)
    {
        local lst;

        /* 
         *   Remember the original qualified list (this is the list of
         *   objects from which we're trying to choose on the basis of the
         *   possessive phrase we're resolving now).  We can feed the
         *   qualified-object list back into the selection process for the
         *   qualifier itself, because we're looking for a qualifier that
         *   makes sense when combined with one of the qualified objects.  
         */
        qualifiedList_ = resolver.matchList;

        /* select from the match list using the possessive phrase */
        lst = selectWithPossessive(resolver, results,
                                   resolver.matchList, resolver.matchText, 1);

        /* 
         *   if the list has more than one entry, treat the result as still
         *   ambiguous - a simple possessive response to a disambiguation
         *   query is implicitly definite, so we must select a single
         *   object 
         */
        if (lst != nil && lst.length() > 1)
            lst = results.ambiguousNounPhrase(
                self, ResolveAsker, resolver.matchText,
                lst, resolver.fullMatchList, lst, 1, resolver);

        /* 
         *   if we failed to resolve it, return an empty list; otherwise
         *   return the list 
         */
        return (lst == nil ? [] : lst);
    }

    /*
     *   Do extra filter during disambiguation.  Since we have a list of
     *   objects we're trying to qualify, we can look at that list to see
     *   if some of the possible matches for the qualifier phrase are
     *   owners of things in the qualified list.  
     */
    reduceDefinite(lst, resolver, results)
    {
        local newLst;
        
        /* 
         *   try reducing the list to owners of objects that appear in the
         *   qualified object list 
         */
        newLst = lst.subset({x: qualifiedList_.indexWhich(
            {y: y.obj_.isOwnedBy(x.obj_)}) != nil});

        /* if there's anything in that list, keep only the subset */
        if (newLst.length() > 0)
            lst = newLst;

        /* return the result */
        return lst;
    }

    /* 
     *   the list of objects being qualified - this is the list of books,
     *   for example, in "bob's books" 
     */
    qualifiedList_ = []
;

/* ------------------------------------------------------------------------ */
/*
 *   A noun phrase with explicit containment.  Grammar rules based on this
 *   class must set the property np_ to the main noun phrase, and cont_ to
 *   the noun phrase giving the container.
 *   
 *   We're based on the definite noun phrase production, because we need
 *   to resolve the underlying container phrase to a singe, unambiguous
 *   object.  
 */
class ContainerNounPhraseProd: DefiniteNounProd
    resolveNouns(resolver, results)
    {
        local lst;
        local cRes;
        local cLst;
        local cont;
        
        /*
         *   We have two separate noun phrases to resolve: the qualified
         *   noun phrase in np_, and the locational qualifier in cont_.
         *   We then want to filter the object matches for np_ to select
         *   the subset that is contained in cont_.
         *   
         *   We must resolve cont_ to a single, unambiguous object.
         *   However, we want to be smart about it by limiting the range
         *   of choices to objects that actually contain something that
         *   could match the possible resolutions of np_.  So, tentatively
         *   resolve np_ first, to get the range of possible matches.
         */
        lst = np_.resolveNouns(resolver, results);

        /* 
         *   We have the tentative resolution of the main noun phrase, so
         *   we can now resolve the locational qualifier phrase.  Use our
         *   special container resolver for this step, since this will try
         *   to pick objects that contain something in the tentative
         *   results for the main list.  Resolve the container as a
         *   definite noun phrase, since we want a single, unambiguous
         *   match.
         *   
         *   Note that we must base our special container resolver on the
         *   qualifier resolver, not on the main resolver.  The location is
         *   a qualifying phrase, so it's resolved in the scope of any
         *   other qualifying phrase.
         *   
         *   The container phrase has to be a single object, so note in the
         *   results that we're working on a single-object slot.  
         */
        results.beginSingleObjSlot();
        
        cRes = new ContainerResolver(lst, np_.getOrigText(),
                                     resolver.getQualifierResolver());
        cLst = resolveDefinite(ResolveAsker, cont_.getOrigText(),
                               cont_.resolveNouns(cRes, results),
                               self, cRes, results);
        
        results.endSingleObjSlot();

        /* 
         *   We need a single object in the container list.  If we have
         *   no objects, or more than one object, it's an error. 
         */
        if (cLst.length() != 1)
        {
            /* it's a separate error if we got more than one object */
            if (cLst.length() > 1)
                results.uniqueObjectRequired(cont_.getOrigText(), cLst);

            /* we can't go on */
            return [];
        }

        /* we have a unique item, so it's the container */
        cont = cLst[1].obj_;

        /* reduce the list to those objects inside the container */
        lst = lst.subset({x: x.obj_.isNominallyIn(cont)});

        /*
         *   If we have some objects directly in the container, and other
         *   objects indirectly in the container, filter the list to
         *   include only the directly contained items. 
         */
        if (lst.indexWhich({x: x.obj_.isDirectlyIn(cont)}) != nil)
            lst = lst.subset({x: x.obj_.isDirectlyIn(cont)});

        /* if that leaves nothing, mention it */
        if (lst.length() == 0)
        {
            results.noMatchForLocation(cont, np_.getOrigText());
            return [];
        }

        /* return the list */
        return lst;
    }
;

/*
 *   Basic container resolver.  This is a common subclass for the standard
 *   container resolver and the "vague" container resolver. 
 */
class BasicContainerResolver: ProxyResolver
    /* we're a sub-phrase resolver */
    isSubResolver = true

    /* resolve any qualifiers in the main scope */
    getQualifierResolver() { return origResolver; }

    /* filter an ambiguous noun phrase */
    filterAmbiguousNounPhrase(lst, requiredNum, np)
    {
        local outer;
        local lcl;
        
        /* do the normal filtering first */
        lst = inherited(lst, requiredNum, np);

        /* 
         *   get the subset that includes only local objects - that is,
         *   objects within the same outermost room as the target actor 
         */
        outer = actor_.getOutermostRoom();
        lcl = lst.subset({x: x.obj_.isIn(outer)});

        /* if there's a local subset, take the subset */
        if (lcl.length() != 0)
            lst = lcl;

        /* return the result */
        return lst;
    }
;    

/*
 *   Container Resolver.  This is a proxy for the main qualifier resolver
 *   that prefers to match objects that are plausible in the sense that
 *   they contain something in the tentative resolution of the main list.  
 */
class ContainerResolver: BasicContainerResolver
    construct(mainList, mainText, origResolver)
    {
        /* inherit base handling */
        inherited(origResolver);

        /* remember my tentative main match list */
        self.mainList = mainList;
        self.mainListText = mainText;
    }

    /* filter ambiguous equivalents */
    filterAmbiguousEquivalents(lst, np)
    {
        local vec;
        
        /*
         *   Check to see if any of the objects in the list are plausible
         *   containers for objects in our main list.  If we can find any
         *   plausible entries, keep only the plausible ones. 
         */
        vec = new Vector(lst.length());
        foreach (local cur in lst)
        {
            /* if this item is plausible, add it to our result vector */
            if (mainList.indexWhich(
                {x: x.obj_.isNominallyIn(cur.obj_)}) != nil)
                vec.append(cur);
        }

        /* 
         *   if we found anything plausible, return only the plausible
         *   subset; otherwise, return the full original list, since
         *   they're all equally implausible 
         */
        if (vec.length() != 0)
            return vec.toList();
        else
            return lst;
    }

    /* the tentative match list for the main phrase we're qualifying */
    mainList = nil

    /* the text of the main phrase we're qualifying */
    mainListText = nil
;

/* ------------------------------------------------------------------------ */
/*
 *   A "vague" container noun phrase.  This is a phrase that specifies a
 *   container but nothing else: "the one in the box", "the ones in the
 *   box", "everything in the box".  
 */
class VagueContainerNounPhraseProd: DefiniteNounProd
    resolveNouns(resolver, results)
    {
        local cRes;
        local cLst;
        local lst;
        local cont;
        
        /* resolve the container as a single-object slot */
        results.beginSingleObjSlot();

        /*
         *   Resolve the container.  Use a special resolver that prefers
         *   objects that have any contents.  
         */
        cRes = new VagueContainerResolver(resolver.getQualifierResolver());
        cLst = resolveDefinite(ResolveAsker, cont_.getOrigText(),
                               cont_.resolveNouns(cRes, results),
                               self, cRes, results);

        /* done with the single-object slot */
        results.endSingleObjSlot();

        /* make sure we resolved to a unique container */
        if (cLst.length() != 1)
        {
            /* it's a separate error if we got more than one object */
            if (cLst.length() > 1)
                results.uniqueObjectRequired(cont_.getOrigText(), cLst);

            /* we can't go on */
            return [];
        }

        /* we have a unique item, so it's the container */
        cont = cLst[1].obj_;

        /* 
         *   If the container is the nominal drop destination for the
         *   actor's location, then use the actor's location as the actual
         *   container.  This way, if we try to get "all on floor" or the
         *   like, we'll correctly get objects that are directly in the
         *   room. 
         */
        if (cont == resolver.actor_.location.getNominalDropDestination())
            cont = resolver.actor_.location;

        /* get the contents */
        lst = cont.getAllForTakeFrom(resolver.getScopeList());

        /* keep only visible objects */
        lst = lst.subset({x: resolver.actor_.canSee(x)});

        /* map the contents to a resolved object list */
        lst = lst.mapAll({x: new ResolveInfo(x, 0, self)});

        /* make sure the list isn't empty */
        if (lst.length() == 0)
        {
            /* there's nothing in this container */
            results.nothingInLocation(cont);
            return [];
        }

        /* make other subclass-specific checks on the list */
        lst = checkContentsList(resolver, results, lst, cont);

        /* note the matches */
        results.noteMatches(lst);

        /* return the list */
        return lst;
    }

    /* check a contents list */
    checkContentsList(resolver, results, lst, cont)
    {
        /* by default, just return the list */
        return lst;
    }
;

/*
 *   "All in container" 
 */
class AllInContainerNounPhraseProd: VagueContainerNounPhraseProd
    /* check a contents list */
    checkContentsList(resolver, results, lst, cont)
    {
        /* keep only items that aren't hidden from "all" */
        lst = lst.subset({x: !x.obj_.hideFromAll(resolver.getAction())});

        /* set the "matched all" and "always announce as multi" flags */
        foreach (local cur in lst)
            cur.flags_ |= AlwaysAnnounce | MatchedAll;

        /* if that emptied the list, so note */
        if (lst.length() == 0)
            results.nothingInLocation(cont);

        /* return the result list */
        return lst;
    }
;

/*
 *   A definite vague container phrase.  This selects a single object in a
 *   given container ("the one in the box").  If more than one object is
 *   present, we'll try to disambiguate it.
 *   
 *   Grammar rules instantiating this class must set the property
 *   'mainPhraseText' to the text to display for a disambiguation prompt
 *   involving the main phrase.  
 */
class VagueContainerDefiniteNounPhraseProd: VagueContainerNounPhraseProd
    construct()
    {
        /* create a disambiguator for the main phrase */
        npKeeper = new AmbigResponseKeeper();
    }

    /* check a contents list */
    checkContentsList(resolver, results, lst, cont)
    {
        /* 
         *   This production type requires a single object in the
         *   container (since it's for phrases like "the one in the box").
         *   If we have more than one object, try disambiguating.
         */
        if (lst.length() > 1)
        {
            local scopeList;
            local fullList;
            
            /* 
             *   There's more than one object in this container.  First,
             *   try filtering it by possessive qualifier strength and
             *   then the normal disambiguation ranking.  
             */
            scopeList = lst;
            lst = resolver.filterPossRank(lst, 1);
            lst = resolver.filterAmbiguousNounPhrase(lst, 1, self);

            /* try removing redundant equivalents */
            fullList = lst;
            if (results.allowEquivalentFiltering)
                lst = resolver.filterAmbiguousEquivalents(lst, self);

            /* if we still have too many objects, it's ambiguous */
            if (lst.length() > 1)
            {
                /* ask the results object to handle the ambiguous phrase */
                lst = results.ambiguousNounPhrase(
                    npKeeper, ResolveAsker, mainPhraseText,
                    lst, fullList, scopeList, 1, resolver);
            }
        }
        
        /* return the contents of the container */
        return lst;
    }

    /* our disambiguation result keeper */
    npKeeper = nil
;

/*
 *   An indefinite vague container phrase.  This selects a single object,
 *   choosing arbitrarily if multiple objects are in the container. 
 */
class VagueContainerIndefiniteNounPhraseProd: VagueContainerNounPhraseProd
    /* check a contents list */
    checkContentsList(resolver, results, lst, cont)
    {
        /* choose one object arbitrarily */
        if (lst.length() > 1)
            lst = lst.sublist(1, 1);

        /* return the (possibly trimmed) list */
        return lst;
    }
;


/*
 *   Container Resolver for vaguely-specified containment phrases.  We'll
 *   select for objects that have contents, but that's about as much as we
 *   can do, since the main phrase is bounded only by the container in
 *   vague containment phrases (and thus provides no information that
 *   would help us narrow down the container itself).  
 */
class VagueContainerResolver: BasicContainerResolver
    /* filter ambiguous equivalents */
    filterAmbiguousEquivalents(lst, np)
    {
        local vec;
        
        /* prefer objects with contents */
        vec = new Vector(lst.length());
        foreach (local cur in lst)
        {
            /* if this item has any contents, add it to the new list */
            if (cur.obj_.contents.length() > 0)
                vec.append(cur);
        }

        /* 
         *   if we found anything plausible, return only the plausible
         *   subset; otherwise, return the full original list, since
         *   they're all equally implausible 
         */
        if (vec.length() != 0)
            return vec.toList();
        else
            return lst;
    }
;


/* ------------------------------------------------------------------------ */
/*
 *   Noun phrase with vocabulary resolution.  This is a base class for the
 *   various noun phrases that match adjective, noun, and plural tokens.
 *   This class provides dictionary resolution of a vocabulary word into a
 *   list of objects.
 *   
 *   In non-declined languages such as English, the parts of speech of our
 *   words are usually simply 'adjective' and 'noun'.  A language
 *   "declines" its noun phrases if the words in a noun phrase have
 *   different forms that depend on the function of the noun phrase in a
 *   sentence; for example, in German, adjectives take suffixes that
 *   depend upon the gender of the noun being modified and the function of
 *   the noun phrase in the sentence (subject, direct object, etc).  In a
 *   declined language, it might be desirable to use separate parts of
 *   speech for separate declined adjective and noun forms.  
 */
class NounPhraseWithVocab: NounPhraseProd
    /*
     *   Get a list of the matches in the main dictionary for the given
     *   token as the given part of speech (&noun, &adjective, &plural, or
     *   others as appropriate for the local language) that are in scope
     *   according to the resolver.  
     */
    getWordMatches(tok, partOfSpeech, resolver, flags, truncFlags)
    {
        local lst;

        /* start with the dictionary matches */
        lst = cmdDict.findWord(tok, partOfSpeech);

        /* filter it to eliminate redundant matches */
        lst = filterDictMatches(lst);

        /* add the objects that match the full dictionary word */
        lst = inScopeMatches(lst, flags, flags | truncFlags, resolver);

        /* return the combined results */
        return lst;
    }

    /*
     *   Combine two word match lists.  This simply adds each entry from
     *   the second list that doesn't already have a corresponding entry
     *   in the first list, returning the combined list.  
     */
    combineWordMatches(aLst, bLst)
    {
        /* create a vector copy of the original 'a' list */
        aLst = new Vector(aLst.length(), aLst);
        
        /* 
         *   add each entry from the second list whose object isn't
         *   already represented in the first list 
         */
        foreach (local b in bLst)
        {
            local ia;

            /* look for an existing entry for this object in the 'a' list */
            ia = aLst.indexWhich({aCur: aCur.obj_ == b.obj_});

            /* 
             *   If this 'b' entry isn't already in the 'a' list, simply
             *   add the 'b' entry.  If both lists have this entry, combine
             *   the flags into the existing entry.  
             */
            if (ia == nil)
            {
                /* it's not already in the 'a' list, so simply add it */
                aLst += b;
            }
            else
            {
                /* 
                 *   Both lists have the entry, so keep the existing 'a'
                 *   entry, but merge the flags.  Note that the 
                 *   original 'a' might still be interesting to the 
                 *   caller separately, so create a copy of it before 
                 *   modifying it.
                 */
                local a = aLst[ia];
                aLst[ia] = a = a.createClone();
                combineWordMatchItems(a, b);
            }
        }

        /* return the combined list */
        return aLst;
    }

    /*
     *   Combine the given word match entries.  We'll merge the flags of
     *   the two entries to produce a single merged entry in 'a'.  
     */
    combineWordMatchItems(a, b)
    {
        /*
         *   If one item was matched with truncation and the other wasn't,
         *   remove the truncation flag entirely.  This will make the
         *   combined entry reflect the fact that we were able to match the
         *   word without any truncation.  The fact that we also matched it
         *   with truncation isn't relevant, since matching without
         *   truncation is the stronger condition.  
         */
        if ((b.flags_ & VocabTruncated) == 0)
            a.flags_ &= ~VocabTruncated;

        /* likewise for plural truncation */
        if ((b.flags_ & PluralTruncated) == 0)
            a.flags_ &= ~PluralTruncated;

        /*
         *   Other flags generally are set for the entire list of matching
         *   objects for a production, rather than at the level of
         *   individual matching objects, so we don't have to worry about
         *   combining them - they'll naturally be the same at this point. 
         */
    }

    /*
     *   Filter a dictionary match list.  This is called to clean up the
     *   raw match list returned from looking up a vocabulary word in the
     *   dictionary.
     *   
     *   The main purpose of this routine is to eliminate unwanted
     *   redundancy from the dictionary matches; in particular, the
     *   dictionary might have multiple matches for a given word at a given
     *   object, due to truncation, upper/lower folding, accent removal,
     *   and so on.  In general, we want to keep only the single strongest
     *   match from the dictionary for a given word matching a given
     *   object.
     *   
     *   The meaning of "stronger" and "exact" matches is
     *   language-dependent, so we abstract these with the separate methods
     *   dictMatchIsExact() and dictMatchIsStronger().
     *   
     *   Keep in mind that the raw dictionary match list has alternating
     *   entries: object, comparator flags, object, comparator flags, etc.
     *   The return list should be in the same format.  
     */
    filterDictMatches(lst)
    {
        local ret;

        /* set up a vector to hold the result */
        ret = new Vector(lst.length());
        
        /* 
         *   check each inexact element of the list for another entry with
         *   a stronger match; keep only the ones without a stronger match 
         */
    truncScan:
        /* scan for a stronger match */
        for (local i = 1, local len = lst.length() ; i < len ; i += 2)
        {
            /* get the current object and its flags */
            local curObj = lst[i];
            local curFlags = lst[i+1];
            
            /* if it's not an exact match, check for a stronger match */
            if (!dictMatchIsExact(curFlags))
            {
                /* scan for an equal or stronger match later in the list */
                for (local j = i + 2 ; j < len ; j += 2)
                {
                    /* 
                     *   if entry j is stronger than or identical to the
                     *   current entry, omit the current entry in favor of
                     *   entry j
                     */
                    local jObj = lst[j], jFlags = lst[j+1];
                    if (jObj == curObj
                        && (jFlags == curFlags
                            || dictMatchIsStronger(jFlags, curFlags)))
                    {
                        /* there's a better entry; omit the current one */
                        continue truncScan;
                    }
                }
            }
            
            /* keep this entry - add it to the result vector */
            ret.append(curObj);
            ret.append(curFlags);
        }

        /* return the result vector */
        return ret;
    }

    /*
     *   Check a dictionary match's string comparator flags to see if the
     *   match is "exact."  The exact meaning of "exact" is dependent on
     *   the language's lexical rules; this generic base version considers
     *   a match to be exact if it doesn't have any string comparator flags
     *   other than the base "matched" flag and the case-fold flag.  This
     *   should be suitable for most languages, as (1) case folding usually
     *   doesn't improve match strength, and (2) any additional comparator
     *   flags usually indicate some kind of inexact match status.
     *   
     *   A language that depends on upper/lower case as a marker of match
     *   strength will need to override this to consider the case-fold flag
     *   as significant in determining match exactness.  In addition, a
     *   language that uses additional string comparator flags to indicate
     *   better (rather than worse) matches will have to override this to
     *   require the presence of those flags.  
     */
    dictMatchIsExact(flags)
    {
        /* 
         *   the match is exact if it doesn't have any qualifier flags
         *   other than the basic "yes I matched" flag and the case-fold
         *   flag 
         */
        return ((flags & ~(StrCompMatch | StrCompCaseFold)) == 0);
    }

    /*
     *   Compare two dictionary matches for the same object and determine
     *   if the first one is stronger than the second.  Both are for the
     *   same object; the only difference is the string comparator flags.
     *   
     *   Language modules might need to override this to supplement the
     *   filtering with their own rules.  This generic base version
     *   considers truncation: an untruncated match is stronger than a
     *   truncated match.  Non-English languages might want to consider
     *   other lexical factors in the match strength, such as whether we
     *   matched the exact accented characters or approximated with
     *   unaccented equivalents - this information will, of course, need to
     *   be coordinated with the dictionary's string comparator, and
     *   reflected in the comparator match flags.  It's the comparator
     *   match flags that we're looking at here.  
     */
    dictMatchIsStronger(flags1, flags2)
    {
        /* 
         *   if the first match (flags1) indicates no truncation, and the
         *   second (flags2) was truncated, then the first match is
         *   stronger; otherwise, there's no distinction as far as we're
         *   concerned 
         */
        return ((flags1 & StrCompTrunc) == 0
                && (flags2 & StrCompTrunc) != 0);
    }

    /*
     *   Get a list of the matches in the main dictionary for the given
     *   token, intersecting the resulting list with the given list. 
     */
    intersectWordMatches(tok, partOfSpeech, resolver, flags, truncFlags, lst)
    {
        local newList;
        
        /* get the matches to the given word */
        newList = getWordMatches(tok, partOfSpeech, resolver,
                                 flags, truncFlags);

        /* intersect the result with the other list */
        newList = intersectNounLists(newList, lst);

        /* return the combined results */
        return newList;
    }

    /*
     *   Given a list of dictionary matches to a given word, construct a
     *   list of ResolveInfo objects for the matches that are in scope.
     *   For regular resolution, "in scope" means the resolver thinks the
     *   object is in scope.  
     */
    inScopeMatches(dictionaryMatches, flags, truncFlags, resolver)
    {
        local ret;

        /* set up a vector to hold the results */
        ret = new Vector(dictionaryMatches.length());

        /* 
         *   Run through the list and include only the words that are in
         *   scope.  Note that the list of dictionary matches has
         *   alternating objects and match flags, so we must scan it two
         *   elements at a time.  
         */
        for (local i = 1, local cnt = dictionaryMatches.length() ;
             i <= cnt ; i += 2)
        {
            /* get this object */
            local cur = dictionaryMatches[i];

            /* if it's in scope, add a ResolveInfo for this object */
            if (resolver.objInScope(cur))
            {
                local curResults;
                local curFlags;

                /* get the comparator match results for this object */
                curResults = dictionaryMatches[i+1];
                
                /* 
                 *   If the results indication truncation, use the the
                 *   truncated flags; otherwise use the ordinary flags.
                 */
                if (dataType(curResults) == TypeInt
                    && (curResults & StrCompTrunc) != 0)
                    curFlags = truncFlags;
                else
                    curFlags = flags;

                /* add the item to the results */
                ret.append(new ResolveInfo(cur, curFlags, self));
            }
        }

        /* return the results as a list */
        return ret.toList();
    }

    /*
     *   Resolve the objects.
     */
    resolveNouns(resolver, results)
    {
        local matchList;
        local starList;
        
        /* 
         *   get the preliminary match list - this is simply the set of
         *   objects that match all of the words in the noun phrase 
         */
        matchList = getVocabMatchList(resolver, results, 0);

        /* 
         *   get a list of any in-scope objects that match '*' for nouns -
         *   these are objects that want to do all of their name parsing
         *   themselves 
         */
        starList = inScopeMatches(cmdDict.findWord('*', &noun),
                                  0, 0, resolver);

        /* combine the lists */
        matchList = combineWordMatches(matchList, starList);

        /* run the results through matchName */
        return resolveNounsMatchName(results, resolver, matchList);
    }

    /*
     *   Run a set of resolved objects through matchName() or a similar
     *   routine.  Returns the filtered results.  
     */
    resolveNounsMatchName(results, resolver, matchList)
    {
        local origTokens;
        local adjustedTokens;
        local objVec;
        local ret;

        /* get the original token list for the command */
        origTokens = getOrigTokenList();

        /* get the adjusted token list for the command */
        adjustedTokens = getAdjustedTokens();

        /* set up to receive about the same number of results as inputs */
        objVec = new Vector(matchList.length());

        /* consider each preliminary match */
        foreach (local cur in matchList)
        {
            /* ask this object if it wants to be included */
            local newObj = resolver.matchName(
                cur.obj_, origTokens, adjustedTokens);

            /* check the result */
            if (newObj == nil)
            {
                /* 
                 *   it's nil - this means it's not a match for the name
                 *   after all, so leave it out of the results 
                 */
            }
            else if (newObj.ofKind(Collection))
            {
                /* 
                 *   it's a collection of some kind - add each element to
                 *   the result list, using the same flags as the original 
                 */
                foreach (local curObj in newObj)
                    objVec.append(new ResolveInfo(curObj, cur.flags_, self));
            }
            else
            {
                /* 
                 *   it's a single object - add it ito the result list,
                 *   using the same flags as the original 
                 */
                objVec.append(new ResolveInfo(newObj, cur.flags_, self));
            }
        }

        /* convert the result vector to a list */
        ret = objVec.toList();

        /* if our list is empty, note it in the results */
        if (ret.length() == 0)
        {
            /* 
             *   If the adjusted token list contains any tokens of type
             *   "miscWord", send the phrase to the results object for
             *   further consideration.  
             */
            if (adjustedTokens.indexOf(&miscWord) != nil)
            {
                /* 
                 *   we have miscWord tokens, so this is a miscWordList
                 *   match - let the results object process it specially.  
                 */
                ret = results.unknownNounPhrase(self, resolver);
            }

            /* 
             *   if the list is empty, note that we have a noun phrase
             *   whose vocabulary words don't match anything in the game 
             */
            if (ret.length() == 0)
                results.noVocabMatch(resolver.getAction(), getOrigText());
        }

        /* return the result list */
        return ret;
    }

    /*
     *   Each subclass must override getAdjustedTokens() to provide the
     *   appropriate set of tokens used to match the object.  This is
     *   usually simply the original set of tokens, but in some cases it
     *   may differ; for example, spelled-out numbers normally adjust to
     *   the numeral form of the number.
     *   
     *   For each adjusted token, the list must have two entries: the
     *   first is a string giving the token text, and the second is the
     *   property giving the part of speech for the token.  
     */
    getAdjustedTokens() { return nil; }

    /*
     *   Get the vocabulary match list.  This is simply the set of objects
     *   that match all of the words in the noun phrase.  Each rule
     *   subclass must override this to return an appropriate list.  Note
     *   that subclasses should use getWordMatches() and
     *   intersectWordMatches() to build the list.  
     */
    getVocabMatchList(resolver, results, extraFlags) { return nil; }
;


/* ------------------------------------------------------------------------ */
/*
 *   An empty noun phrase production is one that matches, typically with
 *   non-zero badness value, as a placeholder when a command is missing a
 *   noun phrase where one is required.
 *   
 *   Each grammar rule instance of this rule class must define the
 *   property 'responseProd' to be the production that should be used to
 *   parse any response to an interactive prompt for the missing object.  
 */
class EmptyNounPhraseProd: NounPhraseProd
    /* customize the way we generate the prompt and parse the response */
    setPrompt(response, asker)
    {
        /* remember the new response production and ResolveAsker */
        responseProd = response;
        asker_ = asker;
    }
    
    /* resolve the empty phrase */
    resolveNouns(resolver, results)
    {
        /* 
         *   if we've filled in our missing phrase already, return the
         *   resolution of that list 
         */
        if (newMatch != nil)
            return newMatch.resolveNouns(resolver, results);
        
        /* 
         *   The noun phrase was left out entirely, so try to get an
         *   implied object.
         */
        local match = getImpliedObject(resolver, results);

        /* if that succeeded, return the result */
        if (match != nil)
            return match;

        /* 
         *   Parse the next input with our own (subclassed) responseProd if
         *   we have one.  If we don't (i.e., it's nil), get one from the
         *   action. 
         */
        local rp = responseProd;
        if (rp == nil && resolver.getAction() != nil)
            rp = resolver.getAction().getObjResponseProd(resolver);

        /* as a last resort, use the fallback response */
        if (rp == nil)
            rp = fallbackResponseProd;

        /* 
         *   There is no implied object, so try to get a result
         *   interactively.  Use the production that our rule instance
         *   specifies via the responseProd property to parse the
         *   interactive response.  
         */
        match = results.askMissingObject(asker_, resolver, rp);
        
        /* if we didn't get a match, we have nothing to return */
        if (match == nil)
            return [];
        
        /* we got a match - remember it as a proxy for our noun phrase */
        newMatch = match;
        
        /* return the resolved noun phrase from the proxy match */
        return newMatch.resolvedObjects;
    }

    /*
     *   Get an implied object to automatically fill in for the missing
     *   noun phrase.  By default, we simply ask the 'results' object for
     *   the missing object.  
     */
    getImpliedObject(resolver, results)
    {
        /* ask the 'results' object for the information */
        return results.getImpliedObject(self, resolver);
    }

    /*
     *   Get my tokens.  If I have a new match tree, return the tokens
     *   from the new match tree.  Otherwise, we don't have any tokens,
     *   since we're empty.  
     */
    getOrigTokenList()
    {
        return (newMatch != nil ? newMatch.getOrigTokenList() : []);
    }

    /* 
     *   Get my original text.  If I have a new match tree, return the
     *   text from the new match tree.  Otherwise, we have no original
     *   text, since we're an empty phrase.  
     */
    getOrigText()
    {
        return (newMatch != nil ? newMatch.getOrigText() : '');
    }

    /*
     *   I'm an empty noun phrase, unless I already have a new match
     *   object. 
     */
    isEmptyPhrase { return newMatch == nil; }

    /*
     *   the new match, when we get an interactive response to a query for
     *   the missing object 
     */
    newMatch = nil

    /* 
     *   Our "response" production - this is the production we use to parse
     *   the player's input in response to our disambiguation prompt.  A
     *   subclass can leave this as nil, in which case we'll attempt to get
     *   the appropriate response production from the action.
     */
    responseProd = nil

    /* 
     *   Our fallback response production - if responseProd is nil, this
     *   must be supplied for cases where we can't get the production from
     *   the action.  This is ignored if responseProd is non-nil.
     */
    fallbackResponseProd = nil

    /* 
     *   The ResolveAsker we use to generate our prompt.  Use the base
     *   ResolveAsker by default; this can be overridden when the prompt
     *   is to be customized. 
     */
    asker_ = ResolveAsker
;

/*
 *   An empty noun phrase production for verb phrasings that imply an
 *   actor, but don't actually include one by name.
 *   
 *   This is similar to EmptyNounPhraseProd, but has an important
 *   difference: if the actor carrying out the command has a current or
 *   implied conversation partner, then we choose the conversation partner
 *   as the implied object.  This is important in that we don't count the
 *   noun phrase as technically missing in this case, for the purposes of
 *   command ranking.  This is useful for phrasings that inherently imply
 *   an actor strongly enough that there should be no match-strength
 *   penalty for leaving it out.  
 */
class ImpliedActorNounPhraseProd: EmptyNounPhraseProd
    /* get my implied object */
    getImpliedObject(resolver, results)
    {
        /* 
         *   If the actor has a default interlocutor, use that, bypassing
         *   the normal implied object search. 
         */
        local actor = resolver.actor_.getDefaultInterlocutor();
        if (actor != nil)
            return [new ResolveInfo(actor, DefaultObject, nil)];

        /* ask the 'results' object for the information */
        return results.getImpliedObject(self, resolver);
    }
;

/*
 *   Empty literal phrase - this serves a purpose similar to that of
 *   EmptyNounPhraseProd, but can be used where literal phrases are
 *   required. 
 */
class EmptyLiteralPhraseProd: LiteralProd
    getLiteralText(results, action, which)
    {
        local toks;
        local prods;
        
        /* if we already have an interactive response, return it */
        if (newText != nil)
            return newText;

        /* 
         *   ask for the missing phrase and remember it for the next time
         *   we're asked for our text 
         */
        newText = results.askMissingLiteral(action, which);

        /*
         *   Parse the text (if any) as a literal phrase.  In most cases,
         *   anything can be parsed as a literal phrase, so this might
         *   seem kind of pointless; however, this is useful when the
         *   language-specific library defines rules for things like
         *   removing quotes from a quoted string.  
         */
        if (newText != nil)
        {
            try
            {
                /* tokenize the input */
                toks = cmdTokenizer.tokenize(newText);
            }
            catch (TokErrorNoMatch exc)
            {
                /* note the token error */
                gLibMessages.invalidCommandToken(exc.curChar_.htmlify());

                /* treat the command as empty */
                throw new ReplacementCommandStringException(nil, nil, nil);
            }

            /* parse the input as a literal phrase */
            prods = literalPhrase.parseTokens(toks, cmdDict);

            /* if we got a match, use it */
            if (prods.length() > 0)
            {
                /* 
                 *   we got a match - this will be a LiteralProd, so ask
                 *   the matching LiteralProd for its literal text, and
                 *   use that as our new literal text 
                 */
                newText = prods[1].getLiteralText(results, action, which);
            }
        }

        /* return the text */
        return newText;
    }

    /* 
     *   Tentatively get my literal text.  This is used for pre-resolution
     *   when we have another phrase we want to resolve first, but we want
     *   to provide a tentative form of the text in the meantime.  We won't
     *   attempt to ask for more information interactively, but we'll
     *   return any information we do have.  
     */
    getTentativeLiteralText()
    {
        /* 
         *   if we have a result from a previous interaactive request,
         *   return it; otherwise we have no tentative text 
         */
        return newText;
    }

    /* I'm an empty phrase, unless I already have a text response */
    isEmptyPhrase { return newText == nil; }

    /* the response to a previous interactive query */
    newText = nil
;

/*
 *   Empty topic phrase production.  This is the topic equivalent of
 *   EmptyNounPhraseProd. 
 */
class EmptyTopicPhraseProd: TopicProd
    resolveNouns(resolver, results)
    {
        local match;

        /* 
         *   if we've filled in our missing phrase already, return the
         *   resolution of that list 
         */
        if (newMatch != nil)
            return newMatch.resolveNouns(resolver, results);
        
        /* ask for a topic interactively, using our responseProd */
        match = results.askMissingObject(asker_, resolver, responseProd);
        
        /* if we didn't get a match, we have nothing to return */
        if (match == nil)
            return [];
        
        /* we got a match - remember it as a proxy for our noun phrase */
        newMatch = match;
        
        /* return the resolved noun phrase from the proxy match */
        return newMatch.resolvedObjects;
    }

    /* we're an empty phrase if we don't have a new topic yet */
    isEmptyPhrase { return newMatch = nil; }

    /* get my tokens - use the underlying new match tree if we have one */
    getOrigTokenList()
    {
        return (newMatch != nil ? newMatch.getOrigTokenList() : inherited());
    }

    /* get my original text - use the new match tree if we have one */
    getOrigText()
    {
        return (newMatch != nil ? newMatch.getOrigText() : inherited());
    }

    /* our new underlying topic phrase */
    newMatch = nil

    /* 
     *   by default, parse our interactive response as an ordinary topic
     *   phrase 
     */
    responseProd = topicPhrase

    /* our ResolveAsker object - this is for customizing the prompt */
    asker_ = ResolveAsker
;


/* ------------------------------------------------------------------------ */
/*
 *   Look for an undefined word in a list of tokens, and give the player a
 *   chance to correct a typo with "OOPS" if appropriate.
 *   
 *   If we find an unknown word and we can prompt for interactive
 *   resolution, we'll do so, and we'll throw an appropriate exception to
 *   handle the response.  If we can't resolve the missing word
 *   interactively, we'll throw a parse failure exception.
 *   
 *   If there are no undefined words in the command, we'll simply return.
 *   
 *   tokList is the list of tokens under examination; this is a subset of
 *   the full command token list.  cmdTokenList is the full command token
 *   list, in the usual tokenizer format.  firstTokenIndex is the index of
 *   the first token in tokList within cmdTokenList.
 *   
 *   cmdType is an rmcXxx code giving the type of input we're reading.  
 */
tryOops(tokList, issuingActor, targetActor,
        firstTokenIndex, cmdTokenList, cmdType)
{
    /* run the main "oops" processor in the player character sense context */
    callWithSenseContext(nil, sight,
                         {: tryOopsMain(tokList, issuingActor, targetActor,
                                        firstTokenIndex, cmdTokenList,
                                        cmdType) });
}

/* main "oops" processor */
tryOopsMain(tokList, issuingActor, targetActor,
            firstTokenIndex, cmdTokenList, cmdType)
{
    local str;
    local unknownIdx;
    local oopsMatch;
    local toks;
    local w;

    /*
     *   Look for a word not in the dictionary. 
     */
    for (unknownIdx = nil, local i = 1, local len = tokList.length() ;
         i <= len ; ++i)
    {
        local cur;
        local typ;
        
        /* get the token value for this word */
        cur = getTokVal(tokList[i]);
        typ = getTokType(tokList[i]);
        
        /* check to see if this word is defined in the dictionary */
        if (typ == tokWord && !cmdDict.isWordDefined(cur))
        {
            /* note that we found an unknown word */
            unknownIdx = i;
            
            /* no need to look any further */
            break;
        }
    }

    /* 
     *   if we didn't find an unknown word, there's no need to offer the
     *   user a chance to correct a typo - simply return without any
     *   further processing 
     */
    if (unknownIdx == nil)
        return;

    /*
     *   We do have an unknown word, but check one more thing: if we were
     *   asking some kind of follow-up question, such as a missing-object
     *   or disambiguation query, check to see if the new entry would parse
     *   successfully as a new command.  It's possible that the new entry
     *   is a brand new command rather than a response to our question, and
     *   that the unknown word is valid in the context of a new command -
     *   it could be part of a literal-phrase, for example.  
     */
    if (cmdType != rmcCommand)
    {
        /* parse the command */
        local lst = firstCommandPhrase.parseTokens(cmdTokenList, cmdDict);

        /* resolve actions */
        lst = lst.subset(
            {x: x.resolveFirstAction(issuingActor, targetActor) != nil});

        /* if we managed to match something, treat it as a new command */
        if (lst.length() != 0)
            throw new ReplacementCommandStringException(
                cmdTokenizer.buildOrigText(cmdTokenList), nil, nil);
    }
            

    /* get the unknown word, in presentable form */
    w = getTokOrig(tokList[unknownIdx]).htmlify();

    /* 
     *   Give them a chance to correct a typo via OOPS if the player
     *   issued the command.  If the command came from an actor other than
     *   the player character, simply fail the command.  
     */
    if (!issuingActor.isPlayerChar())
    {
        /* we can't do this interactively - treat it as a failure */
        throw new ParseFailureException(&wordIsUnknown, w);
    }
    
    /* 
     *   tell the player about the unknown word, implicitly asking for
     *   an OOPS to fix it 
     */
    targetActor.getParserMessageObj().askUnknownWord(targetActor, w);
    
    /* 
     *   Prompt for a new command.  We'll use the main command prompt,
     *   because we want to pretend that we're asking for a brand new
     *   command, which we'll accept.  However, if the player enters
     *   an OOPS command, we'll process it specially. 
     */
getResponse:
    str = readMainCommandTokens(rmcOops);

    /* re-enable the transcript, if we have one */
    if (gTranscript)
        gTranscript.activate();

    /* 
     *   if the command reader fully processed the input via preparsing,
     *   we have nothing more to do here - simply throw a replace-command
     *   exception with the nil string 
     */
    if (str == nil)
        throw new ReplacementCommandStringException(nil, nil, nil);
    
    /* extract the tokens and string from the result */
    toks = str[2];
    str = str[1];
    
    /* try parsing it as an "oops" command */
    oopsMatch = oopsCommand.parseTokens(toks, cmdDict);

    /* 
     *   if we found a match, process the OOPS command; otherwise,
     *   treat it as a brand new command 
     */
    if (oopsMatch.length() != 0)
    {
        local badIdx;

        /* 
         *   if they typed in just OOPS without any tokens, show an error
         *   and ask again 
         */
        if (oopsMatch[1].getNewTokens() == nil)
        {
            /* tell them they need to supply the missing word */
            gLibMessages.oopsMissingWord;

            /* go back and try reading another response */
            goto getResponse;
        }
            
        /* 
         *   Build a new token list by removing the errant token, and
         *   splicing the new tokens into the original token list to
         *   replace the errant one.
         *   
         *   Note that we'll arbitrarily take the first "oops" match,
         *   even if there are several.  There should be no ambiguity
         *   in the "oops" grammar rule, but even if there is, it
         *   doesn't matter, since ultimately all we care about is the
         *   list of tokens after the "oops".  
         */
        badIdx = firstTokenIndex + unknownIdx - 1;
        cmdTokenList = spliceList(cmdTokenList, badIdx,
                                  oopsMatch[1].getNewTokens());

        /*
         *   Turn the token list back into a string.  Since we've edited
         *   the original text, we want to start over with the new input,
         *   including running pre-parsing on the text.  
         */
        str = cmdTokenizer.buildOrigText(cmdTokenList);

        /* 
         *   run it through pre-parsing as the same kind of input the
         *   caller was reading 
         */
        str = StringPreParser.runAll(str, cmdType);

        /* 
         *   if it came back nil, it means the preparser fully handled it;
         *   in this case, simply throw a nil replacement command string 
         */
        if (str == nil)
            throw new ReplacementCommandStringException(nil, nil, nil);

        /* re-tokenize the result of pre-parsing */
        cmdTokenList = cmdTokenizer.tokenize(str);

        /* retry parsing the edited token list */
        throw new RetryCommandTokensException(cmdTokenList);
    }
    else
    {
        /* 
         *   they didn't enter something that looks like an "OOPS", so
         *   it must be a brand new command - parse it as a new
         *   command by throwing a replacement command exception with
         *   the new string 
         */
        throw new ReplacementCommandStringException(str, nil, nil);
    }
}

/* 
 *   splice a new sublist into a list, replacing the item at 'idx' 
 */
spliceList(lst, idx, newItems)
{
    return (lst.sublist(1, idx - 1)
            + newItems
            + lst.sublist(idx + 1));
}


/* ------------------------------------------------------------------------ */
/*
 *   Try reading a response to a missing object question.  If we
 *   successfully read a noun phrase that matches the given production
 *   rule, we'll resolve it, stash the resolved list in the
 *   resolvedObjects_ property of the match tree, and return the match
 *   tree.  If they enter something that doesn't look like a response to
 *   the question at all, we'll throw a new-command exception to process
 *   it.  
 */
tryAskingForObject(issuingActor, targetActor,
                   resolver, results, responseProd)
{
    /* 
     *   Prompt for a new command.  We'll use the main command prompt,
     *   because we want to pretend that we're asking for a brand new
     *   command, which we'll accept.  However, if the player enters
     *   something that looks like a response to the missing-object query,
     *   we'll handle it as an answer rather than as a new command.  
     */
    local str = readMainCommandTokens(rmcAskObject);

    /* re-enable the transcript, if we have one */
    if (gTranscript)
        gTranscript.activate();

    /* 
     *   if it came back nil, it means that the preparser fully processed
     *   the input, which means we have nothing more to do here - simply
     *   treat this is a nil replacement command 
     */
    if (str == nil)
        throw new ReplacementCommandStringException(nil, nil, nil);
    
    /* extract the input line and tokens */
    local toks = str[2];
    str = str[1];

    /* keep going as long as we get replacement token lists */
    for (;;)
    {    
        /* try parsing it as an object list */
        local matchList = responseProd.parseTokens(toks, cmdDict);
        
        /* 
         *   if we didn't find any match at all, it's probably a brand new
         *   command - go process it as a replacement for the current
         *   command 
         */
        if (matchList == [])
        {
            /* 
             *   they didn't enter something that looks like a valid
             *   response, so assume it's a brand new command - parse it
             *   as a new command by throwing a replacement command
             *   exception with the new string 
             */
            throw new ReplacementCommandStringException(str, nil, nil);
        }

        /* if we're in debug mode, show the interpretations */
        dbgShowGrammarList(matchList);
        
        /* create an interactive sub-resolver for resolving the response */
        local ires = new InteractiveResolver(resolver);

        /* 
         *   rank them using our response ranker - use the original
         *   resolver to resolve the object list 
         */
        local rankings = MissingObjectRanking.sortByRanking(matchList, ires);

        /*
         *   If the best item has unknown words, try letting the user
         *   correct typos with OOPS.  
         */
        if (rankings[1].nonMatchCount != 0
            && rankings[1].unknownWordCount != 0)
        {
            try
            {
                /* 
                 *   complain about the unknown word and look for an OOPS
                 *   reply 
                 */
                tryOops(toks, issuingActor, targetActor,
                        1, toks, rmcAskObject);
            }
            catch (RetryCommandTokensException exc)
            {
                /* get the new token list */
                toks = exc.newTokens_;

                /* replace the string as well */
                str = cmdTokenizer.buildOrigText(toks);

                /* go back for another try at parsing the response */
                continue;
            }
        }

        /*
         *   If the best item we could find has no matches, check to see
         *   if it has miscellaneous noun phrases - if so, it's probably
         *   just a new command, since it doesn't have anything we
         *   recognize as a noun phrase.  
         */
        if (rankings[1].nonMatchCount != 0
            && rankings[1].miscWordListCount != 0)
        {
            /* 
             *   it's probably not an answer at all - treat it as a new
             *   command 
             */
            throw new ReplacementCommandStringException(str, nil, nil);
        }

        /* the highest ranked object is the winner */
        local match = rankings[1].match;

        /* 
         *   Check to see if this looks like an ordinary new command as
         *   well as a noun phrase.  For example, "e" looks like the noun
         *   phrase "east wall," in that "e" is a synonym for the adjective
         *   "east".  But "e" also looks like an ordinary new command.
         *   
         *   We'd have to be able to read the user's mind to know which
         *   they mean in such cases, so we have to make some assumptions
         *   to deal with the ambiguity.  In particular, if the phrasing
         *   has special syntax that makes it look like a particularly
         *   close match to the query phrase, assume it's a query response;
         *   otherwise, assume it's a new command.  For example:
         *   
         *.    >dig
         *.    What do you want to dig in?  [sets up for "in <noun>" reply]
         *   
         *   If the user answers "IN THE DIRT", we have a match to the
         *   special syntax of the reply (the "in <noun>" phrasing), so we
         *   will assume this is a reply to the query, even though it also
         *   matches a valid new command phrasing for ENTER DIRT.  If the
         *   user answers "E" or "EAST", we *don't* have a match to the
         *   special syntax, but merely an ordinary noun phrase match, so
         *   we'll assume this is an ordinary GO EAST command.
         */
        local cmdMatchList = firstCommandPhrase.parseTokens(toks, cmdDict);
        if (cmdMatchList != [])
        {
            /* 
             *   The phrasing looks like it's a valid new command as well
             *   as a noun phrase reply.  Check the query reply match for
             *   special syntax that would distinguish it from a new
             *   command; if it doesn't match any special syntax, assume
             *   that it is indeed a new command instead of a query reply.
             */
            if (!match.isSpecialResponseMatch)
                throw new ReplacementCommandStringException(str, nil, nil);
        }

        /* show our winning interpretation */
        dbgShowGrammarWithCaption('Missing Object Winner', match);

        /* 
         *   actually resolve the response to objects, using the original
         *   results and resolver objects 
         */
        local objList = match.resolveNouns(ires, results);

        /* stash the resolved object list in a property of the match tree */
        match.resolvedObjects = objList;

        /* return the match tree */
        return match;
    }
}

/* 
 *   a property we use to hold the resolved objects of a match tree in
 *   tryAskingForObject 
 */
property resolvedObjects;

/*
 *   Missing-object response ranker. 
 */
class MissingObjectRanking: CommandRanking
    /* 
     *   missing-object responses have no verb, so they won't count any
     *   noun slots; we just need to give these an arbitrary value so that
     *   we can compare them (and find them equal) 
     */
    nounSlotCount = 0
;

/* ------------------------------------------------------------------------ */
/*
 *   An implementation of ResolveResults for normal noun resolution.
 */
class BasicResolveResults: ResolveResults
    /*
     *   The target and issuing actors for the command being resolved. 
     */
    targetActor_ = nil
    issuingActor_ = nil

    /* set up the actor parameters */
    setActors(target, issuer)
    {
        targetActor_ = target;
        issuingActor_ = issuer;
    }

    /*
     *   Results gatherer methods
     */

    noVocabMatch(action, txt)
    {
        /* indicate that we couldn't match the phrase */
        throw new ParseFailureException(&noMatch,
                                        action, txt.toLower().htmlify());
    }

    noMatch(action, txt)
    {
        /* show an error */
        throw new ParseFailureException(&noMatch,
                                        action, txt.toLower().htmlify());
    }

    noMatchPossessive(action, txt)
    {
        /* use the basic noMatch handling */
        noMatch(action, txt);
    }

    allNotAllowed()
    {
        /* show an error */
        throw new ParseFailureException(&allNotAllowed);
    }

    noMatchForAll()
    {
        /* show an error */
        throw new ParseFailureException(&noMatchForAll);
    }

    noteEmptyBut()
    {
        /* this isn't an error - ignore it */
    }

    noMatchForAllBut()
    {
        /* show an error */
        throw new ParseFailureException(&noMatchForAllBut);
    }

    noMatchForListBut()
    {
        /* show an error */
        throw new ParseFailureException(&noMatchForListBut);
    }

    noMatchForPronoun(typ, txt)
    {
        /* show an error */
        throw new ParseFailureException(&noMatchForPronoun,
                                        typ, txt.toLower().htmlify());
    }

    reflexiveNotAllowed(typ, txt)
    {
        /* show an error */
        throw new ParseFailureException(&reflexiveNotAllowed,
                                        typ, txt.toLower().htmlify());
    }

    wrongReflexive(typ, txt)
    {
        /* show an error */
        throw new ParseFailureException(&wrongReflexive,
                                        typ, txt.toLower().htmlify());
    }

    noMatchForPossessive(owner, txt)
    {
        /* if the owner is a list, it's a plural owner */
        if (owner.length() > 1)
        {
            /* it's a plural owner */
            throw new ParseFailureException(&noMatchForPluralPossessive, txt);
        }
        else
        {
            /* let the (singular) owner object generate the error */
            owner[1].throwNoMatchForPossessive(txt.toLower().htmlify());
        }
    }

    noMatchForLocation(loc, txt)
    {
        /* let the location object generate the error */
        loc.throwNoMatchForLocation(txt.toLower().htmlify());
    }

    nothingInLocation(loc)
    {
        /* let the location object generate the error */
        loc.throwNothingInLocation();
    }

    noteBadPrep()
    {
        /* 
         *   bad prepositional phrase structure - assume that the
         *   preposition was intended as part of the verb phrase, so
         *   indicate that the entire command is not understood 
         */
        throw new ParseFailureException(&commandNotUnderstood);
    }

    /*
     *   Service routine - determine if we can interactively resolve a
     *   need for more information.  If the issuer is the player, and the
     *   target actor can talk to the player, then we can resolve a
     *   question interactively; otherwise, we cannot.
     *   
     *   We can't interactively resolve a question if the issuer isn't the
     *   player, because there's no interactive user to prompt for more
     *   information.
     *   
     *   We can't interactively resolve a question if the target actor
     *   can't talk to the player, because the question to the player
     *   would be coming out of thin air.
     */
    canResolveInteractively(action)
    {
        /* 
         *   If this is an implied action, and the target actor is in
         *   "NPC" mode, we can't resolve interactively.  Note that if
         *   there's no action, it can't be implicit (we won't have an
         *   action if we're resolving the actor to whom the action is
         *   targeted).  
         */
        if (action != nil
            && action.isImplicit
            && targetActor_.impliedCommandMode() == ModeNPC)
            return nil;

        /* 
         *   if the issuer is the player, and either the target is the
         *   player or the target can talk to the player, we can resolve
         *   interactively 
         */
        return (issuingActor_.isPlayerChar()
                && (targetActor_ == issuingActor_
                    || targetActor_.canTalkTo(issuingActor_)));
    }

    /*
     *   Handle an ambiguous noun phrase.  We'll first check to see if we
     *   can find a Distinguisher that can tell at least some of the
     *   matches apart; if we fail to do that, we'll just pick the required
     *   number of objects arbitrarily, since we have no way to distinguish
     *   any of them.  Once we've chosen a Distinguisher, we'll ask the
     *   user for help interactively if possible.  
     */
    ambiguousNounPhrase(keeper, asker, txt,
                        matchList, fullMatchList, scopeList,
                        requiredNum, resolver)
    {
        /* put the match list in disambigPromptOrder order */
        matchList = matchList.sort(
            SortAsc, {a, b: (a.obj_.disambigPromptOrder
                             - b.obj_.disambigPromptOrder) });

        /* 
         *   Get the token list for the original phrase.  Since the whole
         *   point is to disambiguate a phrase that matched multiple
         *   objects, all of the matched objects came from the same phrase,
         *   so we can just grab the tokens from the first item. 
         */
        local np = matchList[1].np_;
        local npToks = np ? np.getOrigTokenList() : [];

        /* for the prompt, use the lower-cased version of the input text */
        local promptTxt = txt.toLower().htmlify();

        /* ask the response keeper for its list of past responses, if any */
        local pastResponses = keeper.getAmbigResponses();

        /* 
         *   set up a results object - use the special disambiguation
         *   results object instead of the basic resolver type 
         */
        local disambigResults = new DisambigResults(self);

        /* 
         *   start out with an empty still-to-resolve list - we have only
         *   the one list to resolve so far 
         */
        local stillToResolve = [];

        /* we have nothing in the result list yet */
        local resultList = [];

        /* determine if we can ask for clarification interactively */
        if (!canResolveInteractively(resolver.getAction()))
        {
            /* 
             *   don't attempt to resolve this interactively - just treat
             *   it as a resolution failure 
             */
            throw new ParseFailureException(
                &ambiguousNounPhrase, txt.htmlify(),
                matchList, fullMatchList);
        }

        /* we're asking for the first time */
        local everAsked = nil;
        local askingAgain = nil;

        /* 
         *   Keep going until we run out of things left to resolve.  Each
         *   time an answer looks valid but doesn't completely
         *   disambiguate the results, we'll queue it for another question
         *   iteration, so we must continue until we run out of queued
         *   questions.  
         */
    queryLoop:
        for (local pastIdx = 1 ;; )
        {
            local str;
            local toks;
            local dist;
            local curMatchList = [];

            /*
             *   Find the first distinguisher that can tell one or more of
             *   these objects apart.  Work through the distinguishers in
             *   priority order, which is the order they appear in an
             *   object's 'distinguishers' property.
             *   
             *   If these objects aren't all equivalents, then we'll
             *   immediately find that the basic distinguisher can tell
             *   them all apart.  Every object's first distinguisher is
             *   the basic distinguisher.  If these objects are all
             *   equivalents, then they'll all have the same set of
             *   distinguishers.  In either case, we don't have to worry
             *   about what set of objects we have, because we're
             *   guaranteed to have a suitable set of distinguishers in
             *   common - so just arbitrarily pick an object and work
             *   through its distinguishers.  
             */
            foreach (dist in matchList[1].obj_.distinguishers)
            {
                /* filter the match list using this distinguisher only */
                curMatchList = filterWithDistinguisher(matchList, dist);

                /* 
                 *   if this list has more than the required number of
                 *   results, then this distinguisher is capable of
                 *   telling apart enough of these objects, so use this
                 *   distinguisher for now 
                 */
                if (curMatchList.length() > requiredNum)
                    break;
            }

            /*
             *   If we didn't find any distinguishers that could tell
             *   apart enough of the objects, then we've exhausted our
             *   ability to choose among these objects, so return an
             *   arbitrary set of the desired size.
             */
            if (curMatchList.length() <= requiredNum)
                return fullMatchList.sublist(1, requiredNum);
            
            /*
             *   If we have past responses, try reusing the past responses
             *   before asking for new responses. 
             */
            if (pastIdx <= pastResponses.length())
            {
                /* we have another past response - use the next one */
                str = pastResponses[pastIdx++];

                /* tokenize the new command */
                toks = cmdTokenizer.tokenize(str);
            }
            else
            {
                /* 
                 *   Try filtering the options with the basic
                 *   distinguisher, to see if all of the remaining options
                 *   are basic equivalents - if they are, then we can
                 *   refer to them by the object name, since every one has
                 *   the same object name.  
                 */
                local basicDistList = filterWithDistinguisher(
                    matchList, basicDistinguisher);

                /* 
                 *   if we filtered to one object, everything remaining is
                 *   a basic equivalent of that one object, so they all
                 *   have the same name, so we can use that name 
                 */
                if (basicDistList.length() == 1)
                    promptTxt = basicDistList[1].obj_.disambigEquivName;
                
                /* 
                 *   let the distinguisher know we're prompting for more
                 *   information based on this distinguisher 
                 */
                dist.notePrompt(curMatchList);

                /*
                 *   We don't have any special insight into which object
                 *   the user might have intended, and we have no past
                 *   responses to re-use, so simply ask for clarification.
                 *   Ask using the full match list, so that we correctly
                 *   indicate where there are multiple equivalents.  
                 */
                asker.askDisambig(targetActor_, promptTxt,
                                  curMatchList, fullMatchList, requiredNum,
                                  everAsked && askingAgain, dist);

                /* ask for a new command */
                str = readMainCommandTokens(rmcDisambig);

                /* re-enable the transcript, if we have one */
                if (gTranscript)
                    gTranscript.activate();

                /* note that we've asked for input interactively */
                everAsked = true;

                /* 
                 *   if it came back nil, the input was fully processed by
                 *   the preparser, so throw a nil replacement string
                 *   exception 
                 */
                if (str == nil)
                    throw new ReplacementCommandStringException(
                        nil, nil, nil);

                /* extract the tokens and the string result */
                toks = str[2];
                str = str[1];
            }

            /* presume we won't have to ask again */
            askingAgain = nil;

            /* 
             *   Add the new tokens to the noun phrase token list, at the
             *   end, enclosed in parentheses.  The effect is that each
             *   time we answer a disambiguation question, the answer
             *   appears as a parenthetical at the end of the phrase: "take
             *   the box (cardboard) (the big one)".
             */
            npToks = npToks.append(['(', tokPunct, '(']);
            npToks += toks;
            npToks = npToks.append([')', tokPunct, ')']);

        retryParse:
            /*
             *   Check for narrowing syntax.  If the command doesn't
             *   appear to match a narrowing syntax, then treat it as an
             *   entirely new command.  
             */
            local prodList = mainDisambigPhrase.parseTokens(toks, cmdDict);

            /* 
             *   if we didn't get any structural matches for a
             *   disambiguation response, it must be an entirely new
             *   command 
             */
            if (prodList == [])
                throw new ReplacementCommandStringException(str, nil, nil);

            /* if we're in debug mode, show the interpretations */
            dbgShowGrammarList(prodList);

            /* create a disambiguation response resolver */
            local disResolver = new DisambigResolver(
                txt, matchList, fullMatchList, fullMatchList, resolver, dist);

            /* 
             *   create a disambiguation resolver that uses the full scope
             *   list - we'll use this as a fallback if we can't match any
             *   objects with the narrowed possibility list 
             */
            local scopeDisResolver = new DisambigResolver(
                txt, matchList, fullMatchList, scopeList, resolver, dist);

            /*
             *   Run the alternatives through the disambiguation response
             *   ranking process.  
             */
            local rankings =
                DisambigRanking.sortByRanking(prodList, disResolver);

            /*
             *   If the best item has unknown words, try letting the user
             *   correct typos with OOPS.  
             */
            if (rankings[1].nonMatchCount != 0
                && rankings[1].unknownWordCount != 0)
            {
                try
                {
                    /* 
                     *   complain about the unknown word and look for an
                     *   OOPS reply 
                     */
                    tryOops(toks, issuingActor_, targetActor_,
                            1, toks, rmcDisambig);
                }
                catch (RetryCommandTokensException exc)
                {
                    /* get the new token list */
                    toks = exc.newTokens_;
                    
                    /* replace the string as well */
                    str = cmdTokenizer.buildOrigText(toks);

                    /* go back for another try at parsing the response */
                    goto retryParse;
                }
            }

            /*
             *   If the best item we could find has no matches, check to
             *   see if it has miscellaneous noun phrases - if so, it's
             *   probably just a new command, since it doesn't have
             *   anything we recognize as a noun phrase. 
             */
            if (rankings[1].nonMatchCount != 0
                && rankings[1].miscWordListCount != 0)
            {
                /* 
                 *   it's probably not an answer to our disambiguation
                 *   question, so treat it as a whole new command -
                 *   abandon the current command and start over with the
                 *   new string 
                 */
                throw new ReplacementCommandStringException(str, nil, nil);
            }

            /* if we're in debug mode, show the winning intepretation */
            dbgShowGrammarWithCaption('Disambig Winner', rankings[1].match);

            /* get the response list */
            local respList = rankings[1].match.getResponseList();

            /* 
             *   Select the objects for each response in our winning list.
             *   The user can select more than one of the objects we
             *   offered, so simply take each one they specify here
             *   separately.  
             */
            foreach (local resp in respList)
            {
                try
                {
                    try
                    {
                        /* 
                         *   select the objects for this match, and add
                         *   them into the matches so far 
                         */
                        local newObjs = resp.resolveNouns(
                            disResolver, disambigResults);

                        /* set the new token list in the new matches */
                        for (local n in newObjs)
                        {
                            if (n.np_ != nil)
                                n.np_.setOrigTokenList(npToks);
                        }

                        /* add them to the matches so far */
                        resultList += newObjs;
                    }
                    catch (UnmatchedDisambigException udExc)
                    {
                        /*
                         *   The response didn't match anything in the
                         *   narrowed list.  Try again with the full scope
                         *   list, in case they actually wanted to apply
                         *   the command to something that's in scope but
                         *   which didn't make the cut for the narrowed
                         *   list we originally offered. 
                         */
                        resultList +=
                            resp.resolveNouns(scopeDisResolver,
                                              disambigResults);
                    }
                }
                catch (StillAmbiguousException saExc)
                {
                    /* 
                     *   Get the new "reduced" list from the exception.
                     *   Use our original full list, and reduce it to
                     *   include only the elements in the reduced list -
                     *   this will ensure that the items in the new list
                     *   are in the same order as they were in the
                     *   original list, which keeps the next iteration's
                     *   question in the same order.  
                     */
                    local newList = new Vector(saExc.matchList_.length());
                    foreach (local cur in fullMatchList)
                    {
                        /* 
                         *   If this item from the original list has an
                         *   equivalent in the reduced list, include it in
                         *   the new match list.  
                         */
                        if (cur.isDistEquivInList(saExc.matchList_, dist))
                            newList.append(cur);
                    }

                    /* convert it to a list */
                    newList = newList.toList();

                    /*
                     *   If that left us with nothing, just keep the
                     *   original list unchanged.  This can occasionally
                     *   happen, such as when a sub-phrase (a locational
                     *   qualifier, for example) is itself ambiguous. 
                     */
                    if (newList == [])
                        newList = matchList;

                    /*
                     *   Generate the new "full" list.  This is a list of
                     *   all of the items from our current "full" list
                     *   that either are directly in the new match list,
                     *   or are indistinguishable from items in the new
                     *   reduced list.
                     *   
                     *   The exception thrower is not capable of providing
                     *   us with a new full list, because it only knows
                     *   about the reduced list, as the reduced list is
                     *   its scope for narrowing the results.  
                     */
                    local newFullList = new Vector(fullMatchList.length());
                    foreach (local cur in fullMatchList)
                    {
                        /* 
                         *   if this item is in the new reduced list, or
                         *   has an equivalent in the new match list,
                         *   include it in the new full list 
                         */
                        if (cur.isDistEquivInList(newList, dist))
                        {
                            /* 
                             *   we have this item or something
                             *   indistinguishable - include it in the
                             *   revised full match list 
                             */
                            newFullList.append(cur);
                        }
                    }

                    /* convert it to a list */
                    newFullList = newFullList.toList();
                    
                    /* 
                     *   They answered, but with insufficient specificity.
                     *   Add the given list to the still-to-resolve list,
                     *   so that we try again with this new list.  
                     */
                    stillToResolve +=
                        new StillToResolveItem(newList, newFullList,
                                               saExc.origText_);
                }
                catch (DisambigOrdinalOutOfRangeException oorExc)
                {
                    /* 
                     *   Explain the problem (note that if we didn't want
                     *   to offer another chance here, we could simply
                     *   throw this message as a ParseFailureException
                     *   instead of continuing with the query loop).
                     *   
                     *   If we've never asked interactively for input
                     *   (because we've obtained all of our input from
                     *   past responses to a command we're repeating),
                     *   don't show this message, because it makes no
                     *   sense when they haven't answered any questions in
                     *   the first place.  
                     */
                    if (everAsked)
                        targetActor_.getParserMessageObj().
                            disambigOrdinalOutOfRange(
                                targetActor_, oorExc.ord_, txt.htmlify());

                    /* go back to the outer loop to ask for a new response */
                    askingAgain = true;
                    continue queryLoop;
                }
                catch (UnmatchedDisambigException udExc)
                {
                    local newList;
                    
                    /*
                     *   They entered something that looked like a
                     *   disambiguation response, but didn't refer to any
                     *   of our objects.  Try parsing the input as though
                     *   it were a new command, and if it matches any
                     *   command syntax, treat it as a new command.  
                     */
                    newList = firstCommandPhrase.parseTokens(toks, cmdDict);
                    if (newList.length() != 0)
                    {
                        /* 
                         *   it appears syntactically to be a new command
                         *   - treat it as such by throwing a command
                         *   replacement exception 
                         */
                        throw new ReplacementCommandStringException(
                            str, nil, nil);
                    }

                    /* 
                     *   Explain the problem (note that if we didn't want
                     *   to continue with the query loop, we could simply
                     *   throw this message as a ParseFailureException).
                     *   
                     *   Don't show any error if we've never asked
                     *   interactively on this turn (which can only be
                     *   because we're using interactive responses from a
                     *   previous turn that we're repeating), because it
                     *   makes no sense when we haven't apparently asked
                     *   for anything yet.  
                     */
                    if (everAsked)
                        targetActor_.getParserMessageObj().
                            noMatchDisambig(targetActor_, txt.htmlify(),
                                            udExc.resp_);

                    /* go back to the outer loop to ask for a new response */
                    askingAgain = true;
                    continue queryLoop;
                }
            }

            /*
             *   If we got this far, the last input we parsed was actually
             *   a response to our question, as opposed to a new command
             *   or something unintelligible.  
             *   
             *   If this was an interactive response, add the response to
             *   the response keeper's list of past responses.  This will
             *   allow the production to re-use the same list if it has to
             *   re-resolve the phrase in the future, without asking the
             *   user to answer the same questions again 
             */
            if (everAsked)
                keeper.addAmbigResponse(str);

            /* 
             *   if there's nothing left in the still-to-resolve list, we
             *   have nothing left to do, so we can stop working 
             */
            if (stillToResolve.length() == 0)
                break;

            /* 
             *   set up for the next iteration with the first item in the
             *   still-to-resolve list 
             */
            matchList = stillToResolve[1].matchList;
            fullMatchList = stillToResolve[1].fullMatchList;
            txt = stillToResolve[1].origText;

            /* remove the first item, since we're going to process it now */
            stillToResolve = stillToResolve.sublist(2);
        }

        /* success - return the final match list */
        return resultList;
    }

    /*
     *   filter a match list with a specific Distinguisher
     */
    filterWithDistinguisher(lst, dist)
    {
        local result;

        /* create a vector for the result list */
        result = new Vector(lst.length());

        /* scan the list */
        foreach (local cur in lst)
        {
            /*  
             *   if we have no equivalent of the current item (for the
             *   purposes of our Distinguisher) in the result list, add
             *   the current item to the result list 
             */
            if (!cur.isDistEquivInList(result, dist))
                result.append(cur);
        }

        /* return the result list */
        return result.toList();
    }

    /* 
     *   handle a noun phrase that doesn't match any legal grammar rules
     *   for noun phrases 
     */
    unknownNounPhrase(match, resolver)
    {
        local wordList;
        local ret;
        
        /* 
         *   ask the resolver to handle it - if it gives us a resolved
         *   object list, simply return it 
         */
        wordList = match.getOrigTokenList();
        if ((ret = resolver.resolveUnknownNounPhrase(wordList)) != nil)
            return ret;

        /*
         *   The resolver doesn't handle unknown words, so we can't
         *   resolve the phrase.  Look for an undefined word and give the
         *   player a chance to correct typos with OOPS.  
         */
        tryOops(wordList, issuingActor_, targetActor_,
                match.firstTokenIndex, match.tokenList, rmcCommand);

        /*
         *   If we didn't find any unknown words, it means that they used
         *   a word that's in the dictionary in a way that makes no sense
         *   to us.  Simply return an empty list and let the resolver
         *   proceed with its normal handling for unmatched noun phrases.  
         */
        return [];
    }

    getImpliedObject(np, resolver)
    {
        /* ask the resolver to supply an implied default object */
        return resolver.getDefaultObject(np);
    }

    askMissingObject(asker, resolver, responseProd)
    {
        /* if we can't resolve this interactively, fail with an error */
        if (!canResolveInteractively(resolver.getAction()))
        {
            /* interactive resolution isn't allowed - fail */
            throw new ParseFailureException(&missingObject,
                                            resolver.getAction(),
                                            resolver.whichMessageObject);
        }

        /* have the action show objects already defaulted */
        resolver.getAction().announceAllDefaultObjects(nil);

        /* ask for a default object */
        asker.askMissingObject(targetActor_, resolver.getAction(),
                               resolver.whichMessageObject);

        /* try reading an object response */
        return tryAskingForObject(issuingActor_, targetActor_,
                                  resolver, self, responseProd);
    }

    noteLiteral(txt)
    {
        /* 
         *   there's nothing to do with a literal at this point, since
         *   we're not ranking anything 
         */
    }

    askMissingLiteral(action, which)
    {
        local ret;
        
        /* if we can't resolve this interactively, fail with an error */
        if (!canResolveInteractively(action))
        {
            /* interactive resolution isn't allowed - fail */
            throw new ParseFailureException(&missingLiteral, action, which);
        }

        /* ask for the missing literal */
        targetActor_.getParserMessageObj().
            askMissingLiteral(targetActor_, action, which);

        /* read the response */
        ret = readMainCommand(rmcAskLiteral);

        /* re-enable the transcript, if we have one */
        if (gTranscript)
            gTranscript.activate();

        /* return the response */
        return ret;
    }

    emptyNounPhrase(resolver)
    {
        /* abort with an error */
        throw new ParseFailureException(&emptyNounPhrase);
    }

    zeroQuantity(txt)
    {
        /* abort with an error */
        throw new ParseFailureException(&zeroQuantity,
                                        txt.toLower().htmlify());
    }

    insufficientQuantity(txt, matchList, requiredNum)
    {
        /* abort with an error */
        throw new ParseFailureException(
            &insufficientQuantity, txt.toLower().htmlify(),
            matchList, requiredNum);
    }

    uniqueObjectRequired(txt, matchList)
    {
        /* abort with an error */
        throw new ParseFailureException(
            &uniqueObjectRequired, txt.toLower().htmlify(), matchList);
    }

    singleObjectRequired(txt)
    {
        /* abort with an error */
        throw new ParseFailureException(
            &singleObjectRequired, txt.toLower().htmlify());
    }

    noteAdjEnding()
    {
        /* we don't care about adjective-ending noun phrases at this point */
    }

    noteIndefinite()
    {
        /* we don't care about indefinites at this point */
    }

    noteMiscWordList(txt)
    {
        /* we don't care about unstructured noun phrases at this point */
    }

    notePronoun()
    {
        /* we don't care about pronouns right now */
    }

    noteMatches(matchList)
    {
        /* we don't care about the matches just now */
    }

    notePlural()
    {
        /* we don't care about these right now */
    }

    beginSingleObjSlot() { }
    endSingleObjSlot() { }

    beginTopicSlot() { }
    endTopicSlot() { }

    incCommandCount()
    {
        /* we don't care about how many subcommands there are */
    }

    noteActorSpecified()
    {
        /* 
         *   we don't care about this during execution - it only matters
         *   for determining the strength of the command during the
         *   ranking process 
         */
    }

    noteNounSlots(cnt)
    {
        /* 
         *   we don't care about this during execution; it only matters
         *   for the ranking process 
         */
    }

    noteWeakPhrasing(level)
    {
        /* ignore this during execution; it only matters during ranking */
    }

    /* allow remapping the action */
    allowActionRemapping = true

    /* allow making an arbitrary choice among equivalents */
    allowEquivalentFiltering = true
;


/*
 *   List entry for the still-to-resolve list 
 */
class StillToResolveItem: object
    construct(lst, fullList, txt)
    {
        /* remember the equivalent-reduced and full match lists */
        matchList = lst;
        fullMatchList = fullList;

        /* note the text */
        origText = txt;
    }

    /* the reduced (equivalent-eliminated) match list */
    matchList = []

    /* full (equivalent-inclusive) match list */
    fullMatchList = []

    /* the original command text being disambiguated */
    origText = ''
;

/* ------------------------------------------------------------------------ */
/*
 *   Specialized noun-phrase resolution results gatherer for resolving a
 *   command actor (i.e., the target actor of a command).
 */
class ActorResolveResults: BasicResolveResults
    construct()
    {
        /* do the inherited work */
        inherited();

        /* 
         *   set the initial actor context to the PC - this type of
         *   resolver is set up to determine the actor context, so we don't
         *   usually know the actual actor context yet when setting up this
         *   resolver 
         */
        targetActor_ = issuingActor_ = gPlayerChar;
    }
    
    getImpliedObject(np, resolver)
    {
        /* 
         *   there's no default for the actor - it's usually simply a
         *   syntax error when the actor is omitted 
         */
        throw new ParseFailureException(&missingActor);
    }

    uniqueObjectRequired(txt, matchList)
    {
        /* an actor phrase must address a single actor */
        throw new ParseFailureException(&singleActorRequired);
    }

    singleObjectRequired(txt)
    {
        /* an actor phrase must address a single actor */
        throw new ParseFailureException(&singleActorRequired);
    }

    /* don't allow action remapping while resolving the actor */
    allowActionRemapping = nil
;

/*
 *   A results object for resolving an actor in a command with an unknown
 *   word or invalid phrasing in the predicate.  For this type of
 *   resolution, we're trying to interpret the actor portion of the command
 *   as a noun phrase referring to an actor, but it could also just be
 *   another command.  E.g., we could have "bob, asdf" or "east, asdf".
 *   Since we're only tentatively interpreting the phrase as a noun phrase,
 *   to see if that interpretation goes anywhere, we don't want to throw
 *   any errors on failures; instead we simply allow empty match lists.
 */
class TryAsActorResolveResults: ResolveResults
    noVocabMatch(action, txt) { }
    noMatch(action, txt) { }
    noMatchPossessive(action, txt) { }
    noMatchForAll() { }
    noMatchForAllBut() { }
    noMatchForListBut() { }
    noteEmptyBut() { }
    noMatchForPronoun() { }
    allNotAllowed() { }
    reflexiveNotAllowed() { }
    wrongReflexive(typ, txt) { }
    noMatchForPossessive(owner, txt) { }
    noMatchForLocation(loc, txt) { }
    noteBadPrep() { }
    nothingInLocation(loc) { }
    ambiguousNounPhrase(keeper, askwer, txt, matchLst, fullMatchLst,
                        scopeList, requiredNum, resolver) { }
    unknownNounPhrase(match, resolver) { }
    getImpliedObject(np, resolver) { return []; }
    askMissingObject(asker, resolver, responseProd) { return []; }
    noteLiteral(txt) { }
    askMissingLiteral(action, which) { return nil; }
    emptyNounPhrase(resolver) { }
    zeroQuantity(txt) { }
    insufficientQuantity(txt, matchList, requiredNum) { }
    uniqueObjectRequired(txt, matchList) { }
    singleObjectRequired(txt) { }
    noteAdjEnding() { }
    noteIndefinite() { }
    noteMatches(matchList) { }
    noteMiscWord(txt) { }
    notePronoun() { }
    notePlural() { }
    beginSingleObjSlot() { }
    endSingleObjSlot() { }
    beginTopicSlot() { }
    endTopicSlot() { }
    incCommandCount() { }
    noteActorSpecified() { }
    noteNounSlots(cnt) { }
    noteWeakPhrasing() { }
    allowActionRemapping = true
    allowEquivalentFiltering = true
;


/* ------------------------------------------------------------------------ */
/*
 *   Command ranking criterion.  This is used by the CommandRanking class
 *   to represent one criterion for comparing two parse trees.
 *   
 *   Rankings are performed in two passes.  The first pass is the rough,
 *   qualitative pass, meant to determine if one parse tree has big,
 *   obvious differences from another.  In most cases, this means that one
 *   tree has a particular type of problem or special advantage that the
 *   other doesn't have at all.
 *   
 *   The second pass is the fine-grained pass.  We only reach the second
 *   pass if we can't find any coarse differences on the first rough pass.
 *   In most cases, the second pass compares the magnitude of problems or
 *   advantages to determine if one tree is slightly better than the other.
 */
class CommandRankingCriterion: object
    /*
     *   Compare two CommandRanking objects on the basis of this criterion,
     *   for the first, coarse-grained pass.  Returns a positive number if
     *   a is better than b, 0 if they're indistinguishable, or -1 if a is
     *   worse than b.  
     */
    comparePass1(a, b) { return 0; }

    /* compare two rankings for the second, fine-grained pass */
    comparePass2(a, b) { return 0; }
;

/* 
 *   A command ranking criterion that measures a "problem" by a count of
 *   occurrences stored in a property of the CommandRanking object.  For
 *   example, we could count the number of noun phrases that don't resolve
 *   to any objects.
 *   
 *   On the first, coarse-grained pass, we measure only the presence or
 *   absence of our problem.  That is, if one parse tree has zero
 *   occurrences of the problem and the other has a non-zero number of
 *   occurrences of the problem (as measured by our counting property),
 *   then we'll prefer the one with zero occurrences.  If both have no
 *   occurrences, or both have a non-zero number of occurrences, we'll
 *   consider the two equivalent for the first pass, since we only care
 *   about the presence or absence of the problem.
 *   
 *   On the second, fine-grained pass, we measure the actual number of
 *   occurrences of the problem, and choose the parse tree with the lower
 *   number.  
 */
class CommandRankingByProblem: CommandRankingCriterion
    /* 
     *   our ranking property - this is a property of the CommandRanking
     *   object that gives us a count of the number of times our "problem"
     *   has occurred in the ranking object's parse tree 
     */
    prop_ = nil

    /* first pass - compare by presence or absence of the problem */
    comparePass1(a, b)
    {
        local acnt = a.(self.prop_);
        local bcnt = b.(self.prop_);

        /* if b has the problem but a doesn't, a is better */
        if (acnt == 0 && bcnt != 0)
            return 1;

        /* if a has the problem but b doesn't, b is better */
        if (acnt != 0 && bcnt == 0)
            return -1;

        /* we can't tell the difference at this stage */
        return 0;
    }

    /* second pass - compare by number of occurrences of the problem */
    comparePass2(a, b)
    {
        /* 
         *   Return the difference in the problem counts.  We want to
         *   return >0 if a has fewer problems, <0 if b has fewer problems:
         *   so compute (a-b) and negate it, which is the same as computing
         *   (a-b). 
         */
        return b.(self.prop_) - a.(self.prop_);
    }
;

/*
 *   A "weakness" criterion.  This is similar to the rank-by-problem
 *   criterion, but rather than ranking on an actual structural problem, it
 *   ranks on a structural weakness.  This is suitable for things like
 *   adjective endings and truncations, where the weakness isn't on the
 *   same order as a "problem" but where we'd still rather avoid the
 *   weakness if we can.
 *   
 *   The point of the separate "weakness" criterion is that we only allow
 *   weaknesses to come into play on pass 2, after we've already
 *   discriminated based on problems.  If we can discriminate based on
 *   problems, we'll do so in pass 1 and won't even get to pass 2; we'll
 *   only discriminate based on weakness if we can't tell the difference
 *   based on real problems.  
 */
class CommandRankingByWeakness: CommandRankingCriterion
    /* on pass 1, ignore weaknesses */
    comparePass1(a, b) { return 0; }

    /* on pass 2, compare based on weaknesses */
    comparePass2(a, b) { return b.(self.prop_) - a.(self.prop_); }

    /* our command-ranking property */
    prop_ = nil
;

/* 
 *   command-ranking-by-problem and by-weakness objects for the pre-defined
 *   ranking criteria 
 */
rankByVocabNonMatch: CommandRankingByProblem prop_ = &vocabNonMatchCount;
rankByNonMatch: CommandRankingByProblem prop_ = &nonMatchCount;
rankByInsufficient: CommandRankingByProblem prop_ = &insufficientCount;
rankByListForSingle: CommandRankingByProblem prop_ = &listForSingle;
rankByEmptyBut: CommandRankingByProblem prop_ = &emptyButCount;
rankByAllExcluded: CommandRankingByProblem prop_ = &allExcludedCount;
rankByActorSpecified: CommandRankingByProblem prop_ = &actorSpecifiedCount;
rankByMiscWordList: CommandRankingByProblem prop_ = &miscWordListCount;
rankByPluralTrunc: CommandRankingByWeakness prop_ = &pluralTruncCount;
rankByEndAdj: CommandRankingByWeakness prop_ = &endAdjCount;
rankByIndefinite: CommandRankingByProblem prop_ = &indefiniteCount;
rankByTrunc: CommandRankingByWeakness prop_ = &truncCount;
rankByMissing: CommandRankingByProblem prop_ = &missingCount;
rankByPronoun: CommandRankingByWeakness prop_ = &pronounCount;
rankByWeakness: CommandRankingByWeakness prop_ = &weaknessLevel;
rankByUnwantedPlural: CommandRankingByProblem prop_ = &unwantedPluralCount;

/*
 *   Rank by unmatched possessive-qualified phrases.  If we have two
 *   unknown phrases, one with a possessive qualifier and one without, and
 *   other things being equal, prefer the one with the possessive
 *   qualifier.
 *   
 *   We prefer the qualified version because it lets us report a smaller
 *   phrase that we can't match.  For example, in X BOB'S WALLET, if we
 *   can't match WALLET all by itself, it's more useful to report that "you
 *   see no wallet" than to report that you see no "bob's wallet", because
 *   the latter incorrectly implies that there might still be a wallet in
 *   scope as long as it's not Bob's we're looking for.  
 */
rankByNonMatchPoss: CommandRankingCriterion
    /* 
     *   ignore on pass 1 - this only counts if other factors are equal, so
     *   we want to consider all of the other factors on pass 1 before
     *   taking this criterion into account 
     */
    comparePass1(a, b) { return 0; }

    /* pass 2 - more possessives are better */
    comparePass2(a, b)
    {
        /* 
         *   if we don't have the same underlying non-match count,
         *   possessive qualification is irrelevant 
         */
        if (a.nonMatchCount != b.nonMatchCount)
            return 0;

        /* more possessives are better */
        return a.nonMatchPossCount - b.nonMatchPossCount;
    }
;        

/*
 *   Command ranking by literal phrase length.  We prefer interpretations
 *   that treat less text as uninterpreted literal text.  By "less text,"
 *   we simply mean that one has a shorter string treated as literal text
 *   than the other.  (We prefer shorter literals because when the parser
 *   matches a string of literal text, it's essentially throwing up its
 *   hands and admitting it can't parse the text; so the less text is
 *   contained in literals, the more text the parser is actually parsing,
 *   and more parsed is better.)
 */
rankByLiteralLength: CommandRankingCriterion
    /* first pass */
    comparePass1(a, b)
    {
        /* 
         *   Compare our lengths.  We want to return >0 if a is shorter and
         *   <0 if a is longer (and, of course, 0 if they're the same
         *   length).  So, we can just compute (a-b) and negate the result,
         *   which is the same as computing (b-a).
         *   
         *   The CommandRanking objects keep track of the length of text in
         *   literals in their literalLength properties.  
         */
        return b.literalLength - a.literalLength;
    }

    /* 
     *   Second pass - we use our full powers of discrimination on the
     *   first pass, so if we make it to the second pass, we couldn't tell
     *   a difference on the first pass and thus can't tell a difference
     *   now.  So, just inherit the default implementation, which simply
     *   returns 0 to indicate that there's no difference.  
     */
;

/*
 *   Command ranking by subcommand count: we prefer the match with fewer
 *   subcommands.  If one has fewer subcommands than the other, it means
 *   that we were able to interpret ambiguous conjunctions (such as "and")
 *   as noun phrase conjunctions rather than as command conjunctions; other
 *   things being equal, we'd rather take the interpretation that gives us
 *   noun phrases than the one that involves more separate commands.  
 */
rankBySubcommands: CommandRankingCriterion
    /* first pass - compare subcommand counts */
    comparePass1(a, b)
    {
        /* 
         *   if a has fewer subcommands, return <0, and if b has fewer
         *   subcommands, return >0: so we can just return the negative of
         *   (a-b), or (b-a) 
         */
        return b.commandCount - a.commandCount;
    }

    /* second pass - do nothing, as we do all of our work on the first pass */
;

/*
 *   Rank by token count.  Other things being equal, we'd rather pick a
 *   longer match.  If one match is shorter than the other in terms of the
 *   number of tokens it encompasses, then it means that the shorter match
 *   left more tokens at the end of the command to be interpreted as
 *   separate commands.  If we have an interpretation that can take more of
 *   those tokens and parse them as part of the current command, that
 *   interpretation is probably better. 
 */
rankByTokenCount: CommandRankingCriterion
    /* first pass - compare token counts */
    comparePass1(a, b)
    {
        /* choose the one that matched more tokens */
        return a.tokCount - b.tokCount;
    }

    /* first pass - we do all our work on the first pass */
;

/*
 *   Rank by "verb structure."  This gives more weight to an
 *   interpretation that has more structural noun phrases in the verb.
 *   For example, "DETACH dobj FROM iobj" is given more weight than
 *   "DETACH dobj", because the former has two structural noun phrases
 *   whereas the latter has only one.  This will make us prefer to treat
 *   DETACH WIRE FROM BOX as a two-object action, for example, even if we
 *   could treat WIRE FROM BOX as a single "locational" noun phrase.  
 */
rankByVerbStructure: CommandRankingCriterion
    comparePass2(a, b)
    {
        /* take the one with more structural noun slots in the verb phrase */
        return a.nounSlotCount - b.nounSlotCount;
    }
;

/* 
 *   Rank by ambiguous noun phrases.  We apply this criterion on the second
 *   pass only, because it's a weak test: we might end up narrowing things
 *   down through automatic "logicalness" tests during the noun resolution
 *   process, so ambiguity at this stage in the parsing process doesn't
 *   necessarily indicate that there's real ambiguity in the command.
 *   However, if we can already tell that one interpretation is unambiguous
 *   and another is ambiguous, and the two interpretations are otherwise
 *   equally good, pick the one that's already unambiguous: the ambiguous
 *   interpretation might or might not stay ambiguous, but the unambiguous
 *   interpretation will definitely stay unambiguous.  
 */
rankByAmbiguity: CommandRankingCriterion
    /* 
     *   Do nothing on the first pass, because we want any first-pass
     *   criterion to prevail over our weak test.  Instead, check for a
     *   difference in ambiguity only on the second pass. 
     */
    comparePass2(a, b)
    {
        /* the one with lower ambiguity is better */
        return b.ambigCount - a.ambigCount;
    }
;


/*
 *   Production match ranking object.  We create one of these objects for
 *   each match tree that we wish to rank.
 *   
 *   This class is generally not instantiated by client code - instead,
 *   clients use the sortByRanking() class method to rank a list of
 *   production matches.  
 */
class CommandRanking: ResolveResults
    /*
     *   Sort a list of productions, as returned from
     *   GrammarProd.parseTokens(), in descending order of command
     *   strength.  We return a list of CommandRanking objects whose first
     *   element is the best command interpretation.
     *   
     *   Note that this can be used as a class-level method.  
     */
    sortByRanking(lst, [resolveArguments])
    {
        /* 
         *   create a vector to hold the ranking information - we
         *   need one ranking item per match 
         */
        local rankings = new Vector(lst.length());
        
        /* get the ranking information for each command */
        foreach(local cur in lst)
        {
            local curRank;
            
            /* create a ranking item for the entry */
            curRank = self.createInstance(cur);
            
            /* rank this entry */
            curRank.calcRanking(resolveArguments);
            
            /* add this to our ranking list */
            rankings.append(curRank);
        }
        
        /* sort the entries by descending ranking, and return the results */
        return rankings.sort(SortDesc, {x, y: x.compareRanking(y)});
    }

    /* create a new entry */
    construct(match)
    {
        /* remember the match object */
        self.match = match;

        /* remember the number of tokens in the match */
        tokCount = match.lastTokenIndex - match.firstTokenIndex + 1;
    }

    /* calculate my ranking */
    calcRanking(resolveArguments)
    {
        /* 
         *   Ask the match tree to resolve nouns, using this ranking
         *   object as the resolution results receiver - when an error or
         *   warning occurs during resolution, we'll merely note the
         *   condition rather than say anything about it.
         *   
         *   Note that 'self' is the results object, because the point of
         *   this resolution pass is to gather statistics into this
         *   results object.  
         */
        match.resolveNouns(resolveArguments..., self);
    }

    /*
     *   Compare two production list entries for ranking purposes.  Returns
     *   a negative number if this one ranks worse than the other, 0 if
     *   they have the same ranking, or a positive number if this one ranks
     *   better than the other one.
     *   
     *   This routine is designed to run entirely off of our
     *   rankingCriteria property.  In most cases, subclasses should be
     *   able to customize the ranking system simply by overriding the
     *   rankingCriteria property to provide a customized list of criteria
     *   objects.  
     */
    compareRanking(other)
    {
        local ret;

        /* 
         *   Run through our ranking criteria and apply the first pass to
         *   each one.  Return the indication of the first criterion that
         *   can tell a difference.  
         */
        foreach (local cur in rankingCriteria)
        {
            /* if the rankings differ in this criterion, return the result */
            if ((ret = cur.comparePass1(self, other)) != 0)
                return ret;
        }

        /*
         *   We couldn't tell any difference on the first pass, so try
         *   again with the finer-grained second pass. 
         */
        foreach (local cur in rankingCriteria)
        {
            /* run the second pass */
            if ((ret = cur.comparePass2(self, other)) != 0)
                return ret;
        }

        /* we couldn't tell any difference between the two */
        return 0;
    }

    /*
     *   Our list of ranking criteria.  This is a list of
     *   CommandRankingCriterion objects.  The list is given in order of
     *   importance: the first criterion is the most important, so if it
     *   can discriminate the two match trees, we use its result; if the
     *   first criterion can't tell any difference, then we move on to the
     *   second criterion; and so on through the list.
     *   
     *   The most important thing is whether or not we have irresolvable
     *   noun phrases (vocabNonMatchCount).  If one of us has a noun phrase
     *   that refers to nothing anywhere in the game, it's not as good as a
     *   phrase that at least matches something somewhere.  
     *   
     *   Next, if one of us has noun phrases that cannot be resolved to
     *   something in scope (nonMatchCount), and the other can successfully
     *   resolve its noun phrases, the one that can resolve the phrases is
     *   preferred.
     *   
     *   Next, check for insufficient numbers of matches to counted phrases
     *   (insufficientCount).
     *   
     *   Next, check for noun lists in single-noun-only slots
     *   (listForSingle).  
     *   
     *   Next, if we have an empty "but" list in one but not the other,
     *   take the one with the non-empty "but" list (emptyButCount).  We
     *   prefer a non-empty "but" list with an empty "all" even to a
     *   non-empty "all" list with an empty "but", because in the latter
     *   case we probably failed to exclude anything because we
     *   misinterpreted the noun phrase to be excluded.  
     *   
     *   Next, if we have an empty "all" or "any" phrase due to "but"
     *   exclusion, take the one that's not empty (allExcludedCount).
     *   
     *   Next, prefer a command that addresses an actor
     *   (actorSpecifiedCount) - if the actor name looks like a command (we
     *   have someone named "Open Bob," maybe?), we'd prefer to interpret
     *   the name appearing as a command prefix as an actor name.
     *   
     *   Next, prefer no unstructured word lists as noun phrases
     *   (miscWordList phrases) (miscWordListCount).  
     *   
     *   Next, prefer interpretations that treat less text as uninterpreted
     *   literal text.  By "less text," we simply mean that one has a
     *   shorter string treated as a literal than the other.
     *   
     *   Prefer no indefinite noun phrases (indefiniteCount).
     *   
     *   Prefer no truncated plurals (pluralTruncCount).
     *   
     *   Prefer no noun phrases ending in adjectives (endAdjCount).
     *   
     *   Prefer no truncated words of any kind (truncCount).
     *   
     *   Prefer fewer pronouns.  If we have an interpretation that matches
     *   a word to explicit vocabulary, take it over matching a word as a
     *   pronoun: if a word is given explicitly as vocabulary for an
     *   object, use it if possible.
     *   
     *   Prefer no missing phrases (missingCount).
     *   
     *   Prefer the one with fewer subcommands - if one has fewer
     *   subcommands than the other, it means that we were able to
     *   interpret ambiguous conjunctions (such as "and") as noun phrase
     *   conjunctions rather than as command conjunctions; since we know by
     *   now that we both either have or don't have unresolved noun
     *   phrases, we'd rather take the interpretation that gives us noun
     *   phrases than the one that involves more separate commands.
     *   
     *   Prefer the tree that matches more tokens.
     *   
     *   Prefer the one with more structural noun phrases in the verb.  For
     *   example, if we have one interpretation that's DETACH (X FROM Y)
     *   (where X FROM Y is a 'locational' phrase that we treat as the
     *   direct object), and one that's DETACH X FROM Y (where X is the
     *   direct object and Y is in the indirect object), prefer the latter,
     *   because it has both direct and indirect object phrases, whereas
     *   the former has only a direct object phrase.  English speakers
     *   almost always try to put prepositions into a structural role in
     *   the verb phrase like this when they could be either in the verb
     *   phrase or part of a noun phrase.
     *   
     *   If all else fails, prefer the one that is initially less
     *   ambiguous.  Ambiguity is a weak test at this point, since we might
     *   end up narrowing things down through automatic "logicalness" tests
     *   later, but it's slightly better to have the match be less
     *   ambiguous now, all other things being equal.  
     */
    rankingCriteria = [rankByVocabNonMatch,
                       rankByNonMatch,
                       rankByNonMatchPoss,
                       rankByInsufficient,
                       rankByListForSingle,
                       rankByEmptyBut,
                       rankByAllExcluded,
                       rankByActorSpecified,
                       rankByUnwantedPlural,
                       rankByMiscWordList,
                       rankByWeakness,
                       rankByLiteralLength,
                       rankByIndefinite,
                       rankByPluralTrunc,
                       rankByEndAdj,
                       rankByTrunc,
                       rankByPronoun,
                       rankByMissing,
                       rankBySubcommands,
                       rankByTokenCount,
                       rankByVerbStructure,
                       rankByAmbiguity]

    /* the match tree I'm ranking */
    match = nil

    /* the number of tokens my match tree consumes */
    tokCount = 0

    /*
     *   Ranking information.  calcRanking() fills in these members, and
     *   compareRanking() uses these to calculate the relative ranking.  
     */

    /* 
     *   The number of structural "noun phrase slots" in the verb.  An
     *   intransitive verb has no noun phrase slots; a transitive verb
     *   with a direct object has one; a verb with a direct and indirect
     *   object has two slots. 
     */
    nounSlotCount = nil

    /* number of noun phrases matching nothing anywhere in the game */
    vocabNonMatchCount = 0

    /* number of noun phrases matching nothing in scope */
    nonMatchCount = 0

    /* 
     *   Number of possessive-qualified noun phrases matching nothing in
     *   scope.  For example, "bob's desk" when there's no desk in scope
     *   (Bob's or otherwise).
     */
    nonMatchPossCount = 0

    /* number of phrases requiring quantity higher than can be fulfilled */
    insufficientCount = 0

    /* number of noun lists in single-noun slots */
    listForSingle = 0

    /* number of empty "but" lists */
    emptyButCount = 0

    /* number of "all" or "any" lists totally excluded by "but" */
    allExcludedCount = 0

    /* missing phrases (structurally omitted, as in "put book") */
    missingCount = 0

    /* number of truncated plurals */
    pluralTruncCount = 0

    /* number of phrases ending in adjectives */
    endAdjCount = 0

    /* number of phrases with indefinite noun phrase structure */
    indefiniteCount = 0

    /* number of miscellaneous word lists as noun phrases */
    miscWordListCount = 0

    /* number of truncated words overall */
    truncCount = 0

    /* number of ambiguous noun phrases */
    ambigCount = 0

    /* number of subcommands in the command */
    commandCount = 0

    /* an actor is specified */
    actorSpecifiedCount = 0

    /* unknown words */
    unknownWordCount = 0

    /* total character length of literal text phrases */
    literalLength = 0

    /* number of pronoun phrases */
    pronounCount = 0

    /* weakness level (for noteWeakPhrasing) */
    weaknessLevel = 0

    /* number of plural phrases encountered in single-object slots */
    unwantedPluralCount = 0

    /* -------------------------------------------------------------------- */
    /*
     *   ResolveResults implementation.  We use this results receiver when
     *   we're comparing the semantic strengths of multiple structural
     *   matches, so we merely note each error condition without showing
     *   any message to the user or asking the user for any input.  Once
     *   we've ranked all of the matches, we'll choose the one with the
     *   best attributes and then resolve it for real, at which point if
     *   we chose one with any errors, we'll finally get around to showing
     *   the errors to the user.  
     */

    noVocabMatch(action, txt)
    {
        /* note the unknown phrase */
        ++vocabNonMatchCount;
    }

    noMatch(action, txt)
    {
        /* note that we have a noun phrase that matches nothing */
        ++nonMatchCount;
    }

    noMatchPossessive(action, txt)
    {
        /* note that we have an unmatched possessive-qualified noun phrase */
        ++nonMatchCount;
        ++nonMatchPossCount;
    }

    allNotAllowed()
    {
        /* treat this as a non-matching noun phrase */
        ++nonMatchCount;
    }

    noMatchForAll()
    {
        /* treat this as any other noun phrase that matches nothing */
        ++nonMatchCount;
    }

    noteEmptyBut()
    {
        /* note it */
        ++emptyButCount;
    }

    noMatchForAllBut()
    {
        /* count the total exclusion */
        ++allExcludedCount;
    }

    noMatchForListBut()
    {
        /* treat this as any other noun phrase that matches nothing */
        ++allExcludedCount;
    }

    noMatchForPronoun(typ, txt)
    {
        /* treat this as any other noun phrase that matches nothing */
        ++nonMatchCount;
    }

    reflexiveNotAllowed(typ, txt)
    {
        /* treat this as any other noun phrase that matches nothing */
        ++nonMatchCount;
    }

    wrongReflexive(typ, txt)
    {
        /* treat this as any other noun phrase that matches nothing */
        ++nonMatchCount;
    }

    noMatchForPossessive(owner, txt)
    {
        /* treat this as any other noun phrase that matches nothing */
        ++nonMatchCount;
    }

    noMatchForLocation(loc, txt)
    {
        /* treat this as any other noun phrase that matches nothing */
        ++nonMatchCount;
    }

    noteBadPrep()
    {
        /* don't do anything at this point */
    }

    nothingInLocation(txt)
    {
        /* treat this as any other noun phrase that matches nothing */
        ++nonMatchCount;
    }

    ambiguousNounPhrase(keeper, asker, txt,
                        matchList, fullMatchList, scopeList,
                        requiredNum, resolver)
    {
        local lst;
        
        /* note the ambiguity */
        ++ambigCount;

        /* 
         *   There's no need to disambiguate the list at this stage, since
         *   we're only testing the strength of the structure.
         *   
         *   As a tentative approximation of the results, return a list
         *   consisting of the required number only, but stash away the
         *   remainder of the full list as a property of the first element
         *   of the return list so we can find the full list again later.  
         */
        lst = matchList.sublist(1, requiredNum);
        if (matchList.length() > requiredNum && lst.length() >= 1)
            lst[1].extraObjects = matchList.sublist(requiredNum + 1);

        /* return the abbreviated list */
        return lst;
    }

    unknownNounPhrase(match, resolver)
    {
        local wordList;
        local ret;
        
        /* 
         *   if the resolver can handle this set of unknown words, treat
         *   it as a good noun phrase; otherwise, treat it as an unmatched
         *   noun phrase 
         */
        wordList = match.getOrigTokenList();
        if ((ret = resolver.resolveUnknownNounPhrase(wordList)) == nil)
        {
            /* count the unmatchable phrase */
            ++nonMatchCount;

            /* count the unknown word */
            ++unknownWordCount;

            /* 
             *   since this is only a ranking pass, resolve to an empty
             *   list for now 
             */
            ret = [];
        }

        /* return the results */
        return ret;
    }

    getImpliedObject(np, resolver)
    {
        /* count the missing object phrase */
        ++missingCount;
        return nil;
    }

    askMissingObject(asker, resolver, responseProd)
    {
        /* 
         *   no need to do anything here - we'll count the missing object
         *   in getImpliedObject, and we don't want to ask for anything
         *   interactively at this point 
         */
        return nil;
    }

    noteLiteral(txt)
    {
        /* add the length of this literal to the total literal length */
        literalLength += txt.length();
    }

    emptyNounPhrase(resolver)
    {
        /* treat this as a non-matching noun phrase */
        ++nonMatchCount;
        return [];
    }

    zeroQuantity(txt)
    {
        /* treat this as a non-matching noun phrase */
        ++nonMatchCount;
    }

    insufficientQuantity(txt, matchList, requiredNum)
    {
        /* treat this as a non-matching noun phrase */
        ++insufficientCount;
    }

    singleObjectRequired(txt)
    {
        /* treat this as a non-matching noun phrase */
        ++listForSingle;
    }

    uniqueObjectRequired(txt, matchList)
    {
        /* 
         *   ignore this for now - we might get a unique object via
         *   disambiguation during the execution phase 
         */
    }

    noteAdjEnding()
    {
        /* count it */
        ++endAdjCount;
    }

    noteIndefinite()
    {
        /* count it */
        ++indefiniteCount;
    }

    noteMiscWordList(txt)
    {
        /* note the presence of an unstructured noun phrase */
        ++miscWordListCount;

        /* count this as a literal as well */
        noteLiteral(txt);
    }

    notePronoun()
    {
        /* note the presence of a pronoun */
        ++pronounCount;
    }

    noteMatches(matchList)
    {
        /* 
         *   Run through the match list and note each weak flag.  Note
         *   that each element of the match list is a ResolveInfo
         *   instance. 
         */
        foreach (local cur in matchList)
        {
            /* if this object was matched with a truncated word, note it */
            if ((cur.flags_ & VocabTruncated) != 0)
                ++truncCount;

            /* if this object was matched with a truncated plural, note it */
            if ((cur.flags_ & PluralTruncated) != 0)
                ++pluralTruncCount;
        }
    }

    beginSingleObjSlot() { ++inSingleObjSlot; }
    endSingleObjSlot() { --inSingleObjSlot; }
    inSingleObjSlot = 0

    beginTopicSlot() { ++inTopicSlot; }
    endTopicSlot() { --inTopicSlot; }
    inTopicSlot = 0

    notePlural()
    {
        /* 
         *   If we're resolving a single-object slot, we want to avoid
         *   plurals, since they could resolve to multiple objects as
         *   though we'd typed a list of objects here.  This isn't a
         *   problem for topics, though, since a topic slot isn't iterated
         *   for execution.  
         */
        if (inSingleObjSlot && !inTopicSlot)
            ++unwantedPluralCount;
    }

    incCommandCount()
    {
        /* increase our subcommand counter */
        ++commandCount;
    }

    noteActorSpecified()
    {
        /* note it */
        ++actorSpecifiedCount;
    }

    noteNounSlots(cnt)
    {
        /* 
         *   If this is the first noun slot count we've received, remember
         *   it.  If we already have a count, ignore the new one - we only
         *   want to consider the first verb phrase if there are multiple
         *   verb phrases, since we'll reconsider the next verb phrase when
         *   we're ready to execute it. 
         */
        if (nounSlotCount == nil)
            nounSlotCount = cnt;
    }

    noteWeakPhrasing(level)
    {
        /* note the weak phrasing level */
        weaknessLevel = level;
    }

    /* don't allow action remapping while ranking */
    allowActionRemapping = nil
;

/*
 *   Another preliminary results gatherer that does everything the way the
 *   CommandRanking results object does, except that we perform
 *   interactive resolution of unknown words via OOPS.  
 */
class OopsResults: CommandRanking
    construct(issuingActor, targetActor)
    {
        /* remember the actors */
        issuingActor_ = issuingActor;
        targetActor_ = targetActor;
    }

    /*
     *   handle a phrase with unknown words 
     */
    unknownNounPhrase(match, resolver)
    {
        
        /* 
         *   if the resolver can handle this set of unknown words, treat
         *   it as a good noun phrase
         */
        local wordList = match.getOrigTokenList();
        local ret = resolver.resolveUnknownNounPhrase(wordList);
        if (ret != nil)
            return ret;
        
        /* 
         *   we still can't resolve it; try prompting for a correction of
         *   any misspelled words in the phrase 
         */
        tryOops(wordList, issuingActor_, targetActor_,
                match.firstTokenIndex, match.tokenList, rmcCommand);

        /* 
         *   if we got this far, we still haven't resolved it; resolve to
         *   an empty phrase 
         */
        return [];
    }

    /* the command's issuing actor */
    issuingActor_ = nil

    /* the command's target actor */
    targetActor_ = nil
;

/* ------------------------------------------------------------------------ */
/*
 *   Exception list resolver.  We use this type of resolution for noun
 *   phrases in the "but" list of an "all but" construct.
 *   
 *   We scope the "all but" list to the objects in the "all" list, since
 *   there's no point in excluding objects that aren't in the "all" list.
 *   In addition, if a phrase in the exclusion list matches more than one
 *   object in the "all" list, we consider it a match to all of those
 *   objects, even if it's a definite phrase - this means that items in
 *   the "but" list are never ambiguous.  
 */
class ExceptResolver: ProxyResolver
    construct(mainList, mainListText, resolver)
    {
        /* invoke the base class constructor */
        inherited(resolver);
        
        /* remember the main list, from which we're excluding items */
        self.mainList = mainList;
        self.mainListText = mainListText;
    }

    /* we're a sub-phrase resolver */
    isSubResolver = true

    /* 
     *   match an object's name - we'll use the disambiguation name
     *   resolver, so that they can give us partial names just like in
     *   answer to a disambiguation question
     */
    matchName(obj, origTokens, adjustedTokens)
    {
        return obj.matchNameDisambig(origTokens, adjustedTokens);
    }

    /*
     *   Resolve qualifiers in the enclosing main scope, since qualifier
     *   phrases are not part of the narrowed list - qualifiers apply to
     *   the main phrase from which we're excluding, not to the exclusion
     *   list itself.  
     */
    getQualifierResolver() { return origResolver; }

    /* 
     *   determine if an object is in scope - it's in scope if it's in the
     *   original main list 
     */
    objInScope(obj)
    {
        return mainList.indexWhich({x: x.obj_ == obj}) != nil;
    }

    /* for 'all', simply return the whole original list */
    getAll(np)
    {
        return mainList;
    }

    /* filter ambiguous equivalents */
    filterAmbiguousEquivalents(lst, np)
    {
        /* 
         *   keep all of the equivalent items in an exception list,
         *   because we want to exclude all of the equivalent items from
         *   the main list 
         */
        return lst;
    }

    /* filter an ambiguous noun list */
    filterAmbiguousNounPhrase(lst, requiredNum, np)
    {
        /*
         *   noun phrases in an exception list are never ambiguous,
         *   because they implicitly refer to everything they match -
         *   simply return the full matching list 
         */
        return lst;
    }

    /* filter a plural noun list */
    filterPluralPhrase(lst, np)
    {
        /* return all of the original plural matches */
        return lst;
    }

    /* the main list from which we're excluding things */
    mainList = nil

    /* the original text for the main list */
    mainListText = ''

    /* the original underlying resolver */
    origResolver = nil
;

/*
 *   Except list results object 
 */
class ExceptResults: object
    construct(results)
    {
        /* remember the original results object */
        origResults = results;
    }

    /* 
     *   ignore failed matches in the exception list - if they try to
     *   exclude something that's not in the original list, the object is
     *   excluded to begin with 
     */
    noMatch(action, txt) { }
    noMatchPoss(action, txt) { }
    noVocabMatch(action, txt) { }

    /* ignore failed matches for possessives in the exception list */
    noMatchForPossessive(owner, txt) { }

    /* ignore failed matches for location in the exception list */
    noMatchForLocation(loc, txt) { }

    /* ignore failed matches for location in the exception list */
    nothingInLocation(loc) { }

    /* 
     *   in case of ambiguity, simply keep everything and treat it as
     *   unambiguous - if they say "take coin except copper", we simply
     *   want to treat "copper" as unambiguously excluding every copper
     *   coin in the original list 
     */
    ambiguousNounPhrase(keeper, asker, txt,
                        matchList, fullMatchList, scopeList,
                        requiredNum, resolver)
    {
        /* return the full match list - exclude everything that matches */
        return fullMatchList;
    }

    /* proxy anything we don't override to the underlying results object */
    propNotDefined(prop, [args])
    {
        return origResults.(prop)(args...);
    }

    /* my original underlying results object */
    origResults = nil
;


/* ------------------------------------------------------------------------ */
/*
 *   Base class for parser exceptions
 */
class ParserException: Exception
;

/*
 *   Terminate Command exception - when the parser encounters an error
 *   that makes it impossible to go any further processing a command, we
 *   throw this error to abandon the current command and proceed to the
 *   next.  This indicates a syntax error or semantic resolution error
 *   that renders the command meaningless or makes it impossible to
 *   proceed.
 *   
 *   When this exception is thrown, all processing of the current command
 *   termintes immediately.  No further action processing is performed; we
 *   don't continue iterating the command on any additional objects for a
 *   multi-object command; and we discard any remaining commands on the
 *   same command line.  
 */
class TerminateCommandException: ParserException
;

/*
 *   Cancel Command Line exception.  This is used to cancel any *remaining*
 *   commands on a command line after finishing execution of one command on
 *   the line.  For example, if the player types "TAKE BOX AND GO NORTH",
 *   the handler for TAKE BOX can throw this exception to cancel everything
 *   later on the command line (in this case, the GO NORTH part).
 *   
 *   This is handled almost identically to TerminateCommandException.  The
 *   only difference is that some games might want to alert the player with
 *   an explanation that extra commands are being ignored.
 */
class CancelCommandLineException: TerminateCommandException
;
    

/*
 *   Parsing failure exception.  This exception is parameterized with
 *   message information describing the failure, and can be used to route
 *   the failure notification to the issuing actor.  
 */
class ParseFailureException: ParserException
    construct(messageProp, [args])
    {
        /* remember the message property and the parameters */
        message_ = messageProp;
        args_ = args;
    }

    /* notify the issuing actor of the problem */
    notifyActor(targetActor, issuingActor)
    {
        /* 
         *   Tell the target actor to notify the issuing actor.  We route
         *   the notification from the target to the issuer in keeping
         *   with conversation we're modelling: the issuer asked the
         *   target to do something, so the target is now replying with
         *   information explaining why the target can't do as asked. 
         */
        targetActor.notifyParseFailure(issuingActor, message_, args_);
    }

    displayException() { "Parse failure exception"; }

    /* the message property ID */
    message_ = nil

    /* the (varargs) parameters to the message */
    args_ = nil
;

/*
 *   Exception: Retry a command with new tokens.  In some cases, the
 *   parser processes a command by replacing the command with a new one
 *   and processing the new one instead of the original.  When this
 *   happens, the parser will throw this exception, filling in newTokens_
 *   with the replacement token list.
 *   
 *   Note that this is meant to replace the current command only - this
 *   exception effectively *edits* the current command.  Any pending
 *   tokens after the current command should be retained when this
 *   exception is thrown.  
 */
class RetryCommandTokensException: ParserException
    construct(lst)
    {
        /* remember the new token list */
        newTokens_ = lst;
    }

    /* 
     *   The replacement token list.  Note that this is in the same format
     *   as the token list returned from the tokenizer, so this is a list
     *   consisting of two sublists - one for the token strings, the other
     *   for the corresponding token types.  
     */
    newTokens_ = []
;

/*
 *   Replacement command string exception.  Abort any current command
 *   line, and start over with a brand new input string.  Note that any
 *   pending, unparsed tokens on the previous command line should be
 *   discarded.  
 */
class ReplacementCommandStringException: ParserException
    construct(str, issuer, target)
    {
        /* remember the new command string */
        newCommand_ = str;

        /* 
         *   note the issuing actor; if the caller specified this as nil,
         *   use the current player character as the default 
         */
        issuingActor_ = (issuer == nil ? libGlobal.playerChar : issuer);

        /* note the default target actor, defaulting to the player */
        targetActor_ = (target == nil ? libGlobal.playerChar : target);
    }
    
    /* the new command string */
    newCommand_ = ''

    /* the actor issuing the command */
    issuingActor_ = nil

    /* the default target actor of the command */
    targetActor_ = nil
;

/* ------------------------------------------------------------------------ */
/*
 *   Parser debugging helpers 
 */

#ifdef PARSER_DEBUG
/*
 *   Show a list of match trees 
 */
showGrammarList(matchList)
{
    /* show the list only if we're in debug mode */
    if (libGlobal.parserDebugMode)
    {
        /* show each match tree in the list */
        foreach (local cur in matchList)
        {
            "\n----------\n";
            showGrammar(cur, 0);
        }
    }
}

/*
 *   Show a winning match tree 
 */
showGrammarWithCaption(headline, match)
{
    /* show the list only if we're in debug mode */
    if (libGlobal.parserDebugMode)
    {
        "\n----- <<headline>> -----\n";
        showGrammar(match, 0);
    }
}

/*
 *   Show a grammar tree 
 */
showGrammar(prod, indent)
{
    local info;

    /* if we're not in parser debug mode, do nothing */
    if (!libGlobal.parserDebugMode)
        return;
    
    /* indent to the requested level */
    for (local i = 0 ; i < indent ; ++i)
        "\ \ ";

    /* check for non-production objects */
    if (prod == nil)
    {
        /* this tree element isn't used - skip it */
        return;
    }
    else if (dataType(prod) == TypeSString)
    {
        /* show the item literally, and we're done */
        "'<<prod>>'\n";
        return;
    }

    /* get the information for this item */
    info = prod.grammarInfo();

    /* if it's nil, there's nothing more to do */
    if (info == nil)
    {
        "<no information>\n";
        return;
    }

    /* show the name */
    "<<info[1]>> [<<prod.getOrigText()>>]\n";

    /* show the subproductions */
    for (local i = 2 ; i <= info.length ; ++i)
        showGrammar(info[i], indent + 1);
}
#endif /* PARSER_DEBUG */

