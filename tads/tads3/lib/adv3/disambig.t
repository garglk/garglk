#charset "us-ascii"

/* 
 *   Copyright (c) 2000, 2006 Michael J. Roberts.  All Rights Reserved. 
 *   
 *   TADS 3 Library: disambiguation
 *   
 *   This module defines classes related to resolving ambiguity in noun
 *   phrases in command input.  
 */

#include "adv3.h"


/* ------------------------------------------------------------------------ */
/*
 *   Distinguisher.  This object encapsulates logic that determines
 *   whether or not we can tell two objects apart.
 *   
 *   Each game object has a list of distinguishers.  For most objects, the
 *   distinguisher list contains only BasicDistinguisher, since most game
 *   objects are unique and thus are inherently distinguishable from all
 *   other objects.  
 */
class Distinguisher: object
    /* can we distinguish the given two objects? */
    canDistinguish(a, b) { return true; }

    /* 
     *   Note that we're showing a prompt to the player asking for help in
     *   narrowing the object list, based on this distinguisher.  'lst' is
     *   the list of ResolveInfo objects which we're mentioning in the
     *   prompt.
     *   
     *   By default, we do nothing.  Some types of distinguishers might
     *   want to do something special here.  For example, an ownership
     *   distinguisher might want to set pronoun antecedents based on the
     *   owners mentioned in the disambiguation prompt, so that the
     *   player's response can refer anaphorically to the nouns in the
     *   prompt.  
     */
    notePrompt(lst) { }
;

/*
 *   A "null" distinguisher.  This can tell two objects apart if they have
 *   different names (so it's inherently language-specific).  
 */
nullDistinguisher: Distinguisher
;

/*
 *   "Basic" Distinguisher.  This distinguisher can tell two objects apart
 *   if one or the other object is not marked as isEquivalent, OR if the
 *   two objects don't have an identical superclass list.  This
 *   distinguisher thus can tell apart objects unless they're "basic
 *   equivalents," marked with isEquivalent and having the same equivalence
 *   keys.  
 */
basicDistinguisher: Distinguisher
    canDistinguish(a, b)
    {
        /*
         *   If the two objects are both marked isEquivalent, and they have
         *   the same equivalence key, they are basic equivalents, so we
         *   cannot distinguish them.  Otherwise, we consider them
         *   distinguishable.  
         */
        return !(a.isEquivalent
                 && b.isEquivalent
                 && a.equivalenceKey == b.equivalenceKey);
    }
;

/*
 *   Ownership Distinguisher.  This distinguisher can tell two objects
 *   apart if they have different owners.  "Unowned" objects are
 *   identified by their immediate containers instead of their owners.
 *   
 *   Note that while location *can* distinguish items with this
 *   distinguisher, ownership takes priority: if an object has an owner,
 *   the owner is the distinguishing feature.  The reason location is a
 *   factor at all is that we need something parallel to ownership for the
 *   purposes of phrasing distinguishing descriptions of unowned objects.
 *   The best-sounding phrasing, at least in English, is to refer to the
 *   unowned objects by location.  
 */
ownershipDistinguisher: Distinguisher
    canDistinguish(a, b)
    {
        local aOwner;
        local bOwner;

        /* get the nominal owner of each object */
        aOwner = a.getNominalOwner();
        bOwner = b.getNominalOwner();

        /* 
         *   If neither object is owned, we can't tell them apart on the
         *   basis of ownership, so check to see if we can tell them apart
         *   on the basis of their immediate locations.  
         */
        if (aOwner == nil && bOwner == nil)
        {
            /* 
             *   neither is owned - we can tell them apart only if they
             *   have different immediate containers 
             */
            return a.location != b.location;
        }

        /*
         *   One or both objects are owned, so we can tell them apart if
         *   and only if they have different owners.  
         */
        return aOwner != bOwner;
    }
;

/*
 *   Location Distinguisher.  This distinguisher identifies objects purely
 *   by their immediate locations.  
 */
locationDistinguisher: Distinguisher
    canDistinguish(a, b)
    {
        /* we tell the objects apart by their immediate locations */
        return a.location != b.location;
    }
;

/*
 *   Lit/unlit Distinguisher.  This distinguisher can tell two objects
 *   apart if one is lit (i.e., its isLit property is true) and the other
 *   isn't. 
 */
litUnlitDistinguisher: Distinguisher
    canDistinguish(a, b)
    {
        /* we can tell them apart if one is lit and the other isn't */
        return a.isLit != b.isLit;
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   A command ranking criterion for comparing by the number of ordinal
 *   phrases ("first", "the second one") we find in a result. 
 */
rankByDisambigOrdinals: CommandRankingByProblem
    prop_ = &disambigOrdinalCount
;

/*
 *   Disambiguation Ranking.  This is a special version of the command
 *   ranker that we use to rank the intepretations of a disambiguation
 *   response.  
 */
class DisambigRanking: CommandRanking
    /*
     *   Add the ordinal count ranking criterion at the end of the
     *   inherited list of ranking criteria.  If we can't find any
     *   differences on the basis of the other criteria, choose the
     *   interpretation that uses fewer ordinal phrases.  (We prefer an
     *   non-ordinal interpretation, because this will prefer matches to
     *   explicit vocabulary for objects over matches for generic
     *   ordinals.)
     *   
     *   Insert the 'ordinal' rule just before the 'indefinite' rule -
     *   avoiding an ordinal match is more important.  
     */
    rankingCriteria = static (inherited().insertAt(
        inherited().indexOf(rankByIndefinite), rankByDisambigOrdinals))
    
    /*
     *   note the an ordinal response is out of range 
     */
    noteOrdinalOutOfRange(ord)
    {
        /* count it as a non-matching entry */
        ++nonMatchCount;
    }

    /* 
     *   note a list ordinal (i.e., "the first one" to refer to the first
     *   item in the ambiguous list) - we take list ordinals as less
     *   desirable than treating ordinal words as adjectives or nouns
     */
    noteDisambigOrdinal()
    {
        /* count it as an ordinal entry */
        ++disambigOrdinalCount;
    }

    /* number of list ordinals in the match */
    disambigOrdinalCount = 0

    /* 
     *   disambiguation commands have no verbs, so there's no verb
     *   structure to rank; so just use an arbitrary noun slot count
     */
    nounSlotCount = 0
;

/* ------------------------------------------------------------------------ */
/*
 *   Base class for resolvers used when answering interactive questions.
 *   This class doesn't do anything in the library directly, but it
 *   provides a structured point for language extensions to hook in as
 *   needed with 'modify'.  
 */
class InteractiveResolver: ProxyResolver
;

/* ------------------------------------------------------------------------ */
/*
 *   Disambiguation Resolver.  This is a special resolver that we use for
 *   resolving disambiguation responses.  
 */
class DisambigResolver: InteractiveResolver
    construct(matchText, ordinalMatchList, matchList, fullMatchList, resolver)
    {
        /* inherit the base class constructor */
        inherited(resolver);
        
        /* remember the original match text and lists */
        self.matchText = matchText;
        self.ordinalMatchList = ordinalMatchList;
        self.matchList = matchList;
        self.fullMatchList = fullMatchList;
    }

    /*
     *   Match an object's name - we'll use the disambiguation name
     *   resolver.  
     */
    matchName(obj, origTokens, adjustedTokens)
    {
        return obj.matchNameDisambig(origTokens, adjustedTokens);
    }

    /*
     *   Resolve qualifiers in the enclosing main scope, since qualifier
     *   phrases in responses are not part of the narrowed list being
     *   disambiguated.  
     */
    getQualifierResolver() { return origResolver; }

    /* 
     *   determine if an object is in scope - it's in scope if it's in the
     *   original full match list 
     */
    objInScope(obj)
    {
        return fullMatchList.indexWhich({x: x.obj_ == obj}) != nil;
    }

    /* 
     *   we allow ALL in interactive disambiguation responses, regardless
     *   of the verb, because we're just selecting from the list presented
     *   in the prompt in these cases 
     */
    allowAll = true

    /* for 'all', use the full current full match list */
    getAll(np) { return fullMatchList; }

    /* filter an ambiguous noun list */
    filterAmbiguousNounPhrase(lst, requiredNum, np)
    {
        /* 
         *   we're doing disambiguation, so we're only narrowing the
         *   original match list, which we've already filtered as well as
         *   we can - just return the list unchanged 
         */
        return lst;
    }

    /* filter a plural noun list */
    filterPluralPhrase(lst, np)
    {
        /* 
         *   we're doing disambiguation, so we're only narrowing the
         *   original match list, which we've already filtered as well as
         *   we can - just return the list unchanged 
         */
        return lst;
    }

    /*
     *   Select the match for an indefinite noun phrase.  In interactive
     *   disambiguation, an indefinite noun phrase simply narrows the
     *   list, rather than selecting any match, so treat this as still
     *   ambiguous.  
     */
    selectIndefinite(results, lst, requiredNumber)
    {
        /* note the ambiguous list in the results */
        return results.ambiguousNounPhrase(nil, ResolveAsker, '',
                                           lst, lst, lst,
                                           requiredNumber, self);
    }

    /* the text of the phrase we're disambiguating */
    matchText = ''

    /* 
     *   The "ordinal" match list: this includes the exact list offered as
     *   interactive choices in the same order as they were shown in the
     *   prompt.  This list can be used to correlate ordinal responses to
     *   the prompt list, since it contains exactly the items listed in
     *   the prompt.  Note that this list will only contain one of each
     *   indistinguishable object.  
     */
    ordinalMatchList = []

    /* 
     *   the original match list we are disambiguating, which includes all
     *   of the objects offered as interactive choices, and might include
     *   indistinguishable equivalents of offered items 
     */
    matchList = []

    /* 
     *   the full original match list, which might include items in scope
     *   beyond those offered as interactive choices 
     */
    fullMatchList = []
;

/* ------------------------------------------------------------------------ */
/*
 *   General class for disambiguation exceptions 
 */
class DisambigException: Exception
;

/*
 *   Still Ambiguous Exception - this is thrown when the user answers a
 *   disambiguation question with insufficient specificity, so that we
 *   still have an ambiguous list. 
 */
class StillAmbiguousException: DisambigException
    construct(matchList, origText)
    {
        /* remember the new match list and text */
        matchList_ = matchList;
        origText_ = origText;
    }

    /* the narrowed, but still ambiguous, match list */
    matchList_ = []

    /* the text of the new phrasing */
    origText_ = ''
;

/*
 *   Unmatched disambiguation - we throw this when the user answers a
 *   disambiguation question with a syntactically valid response that
 *   doesn't refer to any of the objects in the list of choices offered. 
 */
class UnmatchedDisambigException: DisambigException
    construct(resp)
    {
        /* remember the response text */
        resp_ = resp;
    }

    /* the response text */
    resp_ = nil
;


/*
 *   Disambiguation Ordinal Out Of Range - this is thrown when the user
 *   answers a disambiguation question with an ordinal, but the ordinal is
 *   outside the bounds of the offered list (for example, we ask "which
 *   book do you mean, the red book, or the blue book?", and the user
 *   answers "the fourth one"). 
 */
class DisambigOrdinalOutOfRangeException: DisambigException
    construct(ord)
    {
        /* remember the ordinal word */
        ord_ = ord;
    }

    /* a string giving the ordinal word entered by the user */
    ord_ = ''
;

/* ------------------------------------------------------------------------ */
/*
 *   A disambiguation results gatherer object.  We use this to manage the
 *   results of resolution of a disambiguation response.  
 */
class DisambigResults: BasicResolveResults
    construct(parent)
    {
        /* copy the actor information from the parent resolver */
        setActors(parent.targetActor_, parent.issuingActor_);
    }

    ambiguousNounPhrase(keeper, asker, txt,
                        matchList, fullMatchList, scopeList,
                        requiredNum, resolver)
    {
        /* if we're resolving a sub-phrase, inherit the standard handling */
        if (resolver.isSubResolver)
            return inherited(keeper, asker, txt,
                             matchList, fullMatchList, scopeList,
                             requiredNum, resolver);

        /*
         *   Our disambiguation response itself requires further
         *   disambiguation.  Do not handle it recursively, since doing so
         *   could allow the user to blow the stack simply by answering
         *   with the same response over and over.  Instead, throw a
         *   "still ambiguous" exception - the original disambiguation
         *   loop will note the situation and iterate on the resolution
         *   list, ensuring that we can run forever without blowing the
         *   stack, if that's the game the user wants to play.  
         */
        throw new StillAmbiguousException(matchList, txt.toLower().htmlify());
    }

    /*
     *   note the an ordinal response is out of range 
     */
    noteOrdinalOutOfRange(ord)
    {
        /* this is an error */
        throw new DisambigOrdinalOutOfRangeException(ord);
    }

    /*
     *   show a message on not matching an object - for a disambiguation
     *   response, failing to match means that the combination of the
     *   disambiguation response plus the original text doesn't name any
     *   objects, not that the object in the response itself isn't present 
     */
    noMatch(action, txt)
    {
        /* throw an error indicating the problem */
        throw new UnmatchedDisambigException(txt.toLower().htmlify());
    }

    noVocabMatch(action, txt)
    {
        /* throw an error indicating the problem */
        throw new UnmatchedDisambigException(txt.toLower().htmlify());
    }

    noMatchForPossessive(owner, txt)
    {
        /* throw an error indicating the problem */
        throw new UnmatchedDisambigException(txt.toLower().htmlify());
    }
;


