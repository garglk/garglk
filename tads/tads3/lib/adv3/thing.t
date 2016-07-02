#charset "us-ascii"

/* 
 *   Copyright (c) 2000, 2006 by Michael J. Roberts.  All Rights Reserved. 
 *   
 *   TADS 3 Library - Thing
 *   
 *   This module defines Thing, the base class for physical objects in the
 *   simulation.  We also define some utility classes that Thing uses
 *   internally.  
 */

/* include the library header */
#include "adv3.h"


/* ------------------------------------------------------------------------ */
/* 
 *   this property is defined in the 'exits' module, but declare it here
 *   in case we're not including the 'exits' module 
 */
property lookAroundShowExits;


/* ------------------------------------------------------------------------ */
/*
 *   Sense Information entry.  Thing.senseInfoTable() returns a list of
 *   these objects to provide full sensory detail on the objects within
 *   range of a sense.  
 */
class SenseInfo: object
    construct(obj, trans, obstructor, ambient)
    {
        /* set the object being described */
        self.obj = obj;

        /* remember the transparency and obstructor */
        self.trans = trans;
        self.obstructor = obstructor;

        /* 
         *   set the energy level, as seen from the point of view - adjust
         *   the level by the transparency 
         */
        self.ambient = (ambient != nil
                        ? adjustBrightness(ambient, trans)
                        : nil);
    }
    
    /* the object being sensed */
    obj = nil

    /* the transparency from the point of view to this object */
    trans = nil

    /* the obstructor that introduces a non-transparent value of trans */
    obstructor = nil

    /* the ambient sense energy level at this object */
    ambient = nil

    /* 
     *   compare this SenseInfo object's transparency to the other one;
     *   returns a number greater than zero if 'self' is more transparent,
     *   zero if they're equally transparent, or a negative number if
     *   'self' is less transparent 
     */
    compareTransTo(other) { return transparencyCompare(trans, other.trans); }

    /*
     *   Return the more transparent of two SenseInfo objects.  Either
     *   argument can be nil, in which case we'll return the non-nil one;
     *   if both are nil, we'll return nil.  If they're equal, we'll return
     *   the first one.  
     */
    selectMoreTrans(a, b)
    {
        /* if one or the other is nil, return the non-nil one */
        if (a == nil)
            return b;
        else if (b == nil)
            return a;
        else if (a.compareTransTo(b) >= 0)
            return a;
        else
            return b;
    }
;

/*
 *   Can-Touch information.  This object keeps track of whether or not a
 *   given object is able to reach out and touch another object. 
 */
class CanTouchInfo: object
    /* construct, given the touch path */
    construct(path) { touchPath = path; }
    
    /* the full reach-and-touch path from the source to the target */
    touchPath = nil

    /* 
     *   if we have calculated whether or not the source can touch the
     *   target, we'll set the property canTouch to nil or true
     *   accordingly; if this property is left undefined, this information
     *   has never been calculated 
     */
    // canTouch = nil
;

/*
 *   Given a sense information table (a LookupTable returned from
 *   Thing.senseInfoTable()), return a vector of only those objects in the
 *   table that match the given criteria.
 *   
 *   'func' is a function that takes two arguments, func(obj, info), where
 *   'obj' is a simulation object and 'info' is the corresponding
 *   SenseInfo object.  This function is invoked for each object in the
 *   sense info table; if 'func' returns true, then 'obj' is part of the
 *   list that we return.
 *   
 *   The return value is a simple vector of game objects.  (Note that
 *   SenseInfo objects are not returned - just the simulation objects.)  
 */
senseInfoTableSubset(senseTab, func)
{
    local vec;
    
    /* set up a vector for the return list */
    vec = new Vector(32);

    /* scan the table for objects matching criteria given by 'func' */
    senseTab.forEachAssoc(function(obj, info)
    {
        /* if the function accepts this object, include it in the vector */
        if ((func)(obj, info))
            vec.append(obj);
    });

    /* return the result vector */
    return vec;
}

/*
 *   Sense calculation scratch-pad globals.  Many of the sense
 *   calculations involve recursive descents of portions of the
 *   containment tree.  In the course of these calculations, it's
 *   sometimes useful to have information about the entire operation in
 *   one of the recursive calls.  We could pass the information around as
 *   extra parameters, but that adds overhead, and performance is critical
 *   in the sense routines (because they tend to get invoked a *lot*).  To
 *   reduce the overhead, particularly for information that's not needed
 *   very often, we stuff some information into this global object rather
 *   than passing it around through parameters.
 *   
 *   Note that this object is transient because this information is useful
 *   only during the course of a single tree traversal, and so doesn't
 *   need to be saved or undone.  
 */
transient senseTmp: object
    /* 
     *   The point of view of the sense calculation.  This is the starting
     *   point of a sense traversal; it's the object that's viewing the
     *   other objects.  
     */
    pointOfView = nil

    /* post-calculation notification list */
    notifyList = static new Vector(16)
;


/* ------------------------------------------------------------------------ */
/*
 *   Command check status object.  This is an abstract object that we use
 *   in to report results from a check of various kinds.
 *   
 *   The purpose of this object is to consolidate the code for certain
 *   kinds of command checks into a single routine that can be used for
 *   different purposes - verification, selection from multiple
 *   possibilities (such as multiple paths), and command action
 *   processing.  This object encapsulates a status - success or failure -
 *   and, when the status is failure, a message giving the reason for the
 *   failure.
 */
class CheckStatus: object
    /* did the check succeed or fail? */
    isSuccess = nil

    /* 
     *   the message property or string, and parameters, for failure -
     *   this is for use with reportFailure or the like 
     */
    msgProp = nil
    msgParams = []
;

/* 
 *   Success status object.  Note that this is a single object, not a
 *   class - there's no distinct information per success indicator, so we
 *   only need this single success indicator for all uses. 
 */
checkStatusSuccess: CheckStatus
    isSuccess = true
;

/* 
 *   Failure status object.  Unlike the success indicator, this is a
 *   class, because we need to keep track of the separate failure message
 *   for each kind of failure.  
 */
class CheckStatusFailure: CheckStatus
    construct(prop, [params])
    {
        isSuccess = nil;
        msgProp = prop;
        msgParams = params;
    }
;


/* ------------------------------------------------------------------------ */
/*
 *   Equivalent group state information.  This keeps track of a state and
 *   the number of items in that state when we're listing a group of
 *   equivalent items in different states.  
 */
class EquivalentStateInfo: object
    construct(st, obj, nameProp)
    {
        /* remember the state object and the name property to display */
        stateObj = st;
        stateNameProp = nameProp;

        /* this is the first one in this state */
        stateVec = new Vector(8);
        stateVec.append(obj);
    }

    /* add an object to the list of equivalent objects in this state */
    addEquivObj(obj) { stateVec.append(obj); }

    /* get the number of equivalent items in the same state */
    getEquivCount() { return stateVec.length(); }

    /* get the list of equivalent items in the same state */
    getEquivList() { return stateVec; }

    /* get the name to use for listing purposes */
    getName() { return stateObj.(stateNameProp)(stateVec); }

    /* the ThingState object describing the state */
    stateObj = nil

    /* the property to evaluate to get the name for listing purposes */
    stateNameProp = nil

    /* list of items in this same state */
    stateVec = nil
;


/* ------------------------------------------------------------------------ */
/*
 *   Drop Descriptor.  This is passed to the receiveDrop() method of a
 *   "drop destination" when an object is discarded via commands such as
 *   DROP or THROW.  The purpose of the descriptor is to identify the type
 *   of command being performed, so that the receiveDrop() method can
 *   generate an appropriate report message.  
 */
class DropType: object
    /*
     *   Generate the standard report message for the action.  The drop
     *   destination's receiveDrop() method can call this if the standard
     *   message is adequate to describe the result of the action.
     *   
     *   'obj' is the object being dropped, and 'dest' is the drop
     *   destination.  
     */
    // standardReport(obj, dest) { /* subclasses must override */ }

    /*
     *   Get a short report describing the action without saying where the
     *   object ended up.  This is roughly the same as the standard report,
     *   but omits any information on where the object lands, so that the
     *   caller can show a separate message explaining that part.
     *   
     *   The report must be worded such that the object being dropped is
     *   the logical antecedent for any subsequent text.  This means that
     *   callers can use a pronoun to refer back to the object dropped,
     *   allowing for more natural sequences to be constructed.  (It
     *   usually sounds stilted to repeat the full name: "You drop the box.
     *   The box falls into the chasm."  It's better if we can use a
     *   pronoun in the second sentence: "You drop the box. It falls into
     *   the chasm.")
     *   
     *   'obj' is the object being dropped, and 'dest' is the drop
     *   destination.  
     */
    // getReportPrefix(obj, dest) { return ''; }
;

/*
 *   A drop-type descriptor for the DROP command.  Since we have no need to
 *   include any varying parameters in this object, we simply provide this
 *   singleton instance.  
 */
dropTypeDrop: DropType
    standardReport(obj, dest)
    {
        /* show the default "Dropped" response */
        defaultReport(&okayDropMsg);
    }

    getReportPrefix(obj, dest)
    {
        /* return the standard "You drop the <obj>" message */
        return gActor.getActionMessageObj().droppingObjMsg(obj);
    }
;

/*
 *   A drop-type descriptor for the THROW command.  This object keeps track
 *   of the target (the object that was hit by the projectile) and the
 *   projectile's path to the target.  The projectile is simply the direct
 *   object.  
 */
class DropTypeThrow: DropType
    construct(target, path)
    {
        /* remember the target and path */
        target_ = target;
        path_ = path;
    }

    standardReport(obj, dest)
    {
        local nominalDest;
        
        /* get the nominal drop destination */
        nominalDest = dest.getNominalDropDestination();

        /* 
         *   if the actual target and the nominal destination are the same,
         *   just say that it lands on the destination; otherwise, say that
         *   it bounces off the target and falls to the nominal destination
         */
        if (target_ == nominalDest)
            mainReport(&throwFallMsg, obj, target_);
        else
            mainReport(&throwHitFallMsg, obj, target_, nominalDest);
    }

    getReportPrefix(obj, dest)
    {
        /* return the standard "The <projectile> hits the <target>" message */
        return gActor.getActionMessageObj().throwHitMsg(obj, target_);
    }

    /* the object that was hit by the projectile */
    target_ = nil

    /* the path the projectile took to reach the target */
    path_ = nil
;


/* ------------------------------------------------------------------------ */
/*
 *   Bag Affinity Info - this class keeps track of the affinity of a bag
 *   of holding for an object it might contain.  We use this class in
 *   building bag affinity lists.  
 */
class BagAffinityInfo: object
    construct(obj, bulk, aff, bag)
    {
        /* save our parameters */
        obj_ = obj;
        bulk_ = bulk;
        aff_ = aff;
        bag_ = bag;
    }

    /*
     *   Compare this item to another item, for affinity ranking purposes.
     *   Returns positive if I should rank higher than the other item,
     *   zero if we have equal ranking, negative if I rank lower than the
     *   other item.  
     */
    compareAffinityTo(other)
    {
        /* 
         *   if this object is the indirect object of 'take from', treat
         *   it as having the lowest ranking 
         */
        if (gActionIs(TakeFrom) && gIobj == obj_)
            return -1;

        /* if we have different affinities, sort according to affinity */
        if (aff_ != other.aff_)
            return aff_ - other.aff_;

        /* we have the same affinity, so put the higher bulk item first */
        if (bulk_ != other.bulk_)
            return bulk_ - other.bulk_;

        /* 
         *   We have the same affinity and same bulk; rank according to
         *   how recently the items were picked up.  Put away the oldest
         *   items first, so the lower holding (older) index has the
         *   higher ranking.  (Note that because lower holding index is
         *   the higher ranking, we return the negative of the holding
         *   index comparison.)  
         */
        return other.obj_.holdingIndex - obj_.holdingIndex;
    }

    /* 
     *   given a vector of affinities, remove the most recent item (as
     *   indicated by holdingIndex) and return the BagAffinityInfo object 
     */
    removeMostRecent(vec)
    {
        local best = vec[1];
        
        /* find the most recent item */
        foreach (local cur in vec)
        {
            /* if this is better than the best so far, remember it */
            if (cur.obj_.holdingIndex > best.obj_.holdingIndex)
                best = cur;
        }

        /* remove the best item from the vector */
        vec.removeElement(best);

        /* return the best item */
        return best;
    }

    /* the object the bag wants to contain */
    obj_ = nil

    /* the object's bulk */
    bulk_ = nil

    /* the bag that wants to contain the object */
    bag_ = nil

    /* affinity of the bag for the object */
    aff_ = nil
;


/* ------------------------------------------------------------------------ */
/*
 *   "State" of a Thing.  This is an object abstractly describing the
 *   state of an object that can assume different states.
 *   
 *   The 'listName', 'inventoryName', and 'wornName' give the names of
 *   state as displayed in room/contents listings, inventory listings, and
 *   listings of items being worn by an actor.  This state name is
 *   displayed along with the item name (usually parenthetically after the
 *   item name, but the exact nature of the display is controlled by the
 *   language-specific part of the library).
 *   
 *   The 'listingOrder' is an integer giving the listing order of this
 *   state relative to other states of the same kind of object.  When we
 *   show a list of equivalent items in different states, we'll order the
 *   state names in ascending order of listingOrder.  
 */
class ThingState: object
    /* 
     *   The name of the state to use in ordinary room/object contents
     *   listings.  If the name is nil, no extra state information is shown
     *   in a listing for an object in this state.  (It's often desirable
     *   to leave the most ordinary state an object can be in unnamed, to
     *   avoid belaboring the obvious.  For example, a match that isn't
     *   burning would probably not want to mention "(not lit)" every time
     *   it's listed.)
     *   
     *   'lst' is a list of the objects being listed in this state.  If
     *   we're only listing a single object, this will be a list with one
     *   element giving the object being listed.  If we're listing a
     *   counted set of equivalent items all in this same state, this will
     *   be the list of items.  Everything in 'lst' will be equivalent (in
     *   the isEquivalent sense).  
     */
    listName(lst) { return nil; }

    /* 
     *   The state name to use in inventory lists.  By default, we just use
     *   the base name.  'lst' has the same meaning as in listName().  
     */
    inventoryName(lst) { return listName(lst); }

    /* 
     *   The state name to use in listings of items being worn.  By
     *   default, we just use the base name.  'lst' has the same meaning as
     *   in listName().  
     */
    wornName(lst) { return listName(lst); }

    /* the relative listing order */
    listingOrder = 0

    /*
     *   Match the name of an object in this state.  'obj' is the object
     *   to be matched; 'origTokens' and 'adjustedTokens' have the same
     *   meanings they do for Thing.matchName; and 'states' is a list of
     *   all of the possible states the object can assume.
     *   
     *   Implementation of this is always language-specific.  In most
     *   cases, this should do something along the lines of checking for
     *   the presence (in the token list) of words that only apply to
     *   other states, rejecting the match if any such words are found.
     *   For example, the ThingState object representing the unlit state
     *   of a light source might check for the presence of 'lit' as an
     *   adjective, and reject the object if it's found.  
     */
    matchName(obj, origTokens, adjustedTokens, states)
    {
        /* by default, simply match the object */
        return obj;
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Object with vocabulary.  This is the base class for any object that
 *   can define vocabulary words.  
 */
class VocabObject: object
    /*
     *   Match a name as used in a noun phrase in a player's command to
     *   this object.  The parser calls this routine to test this object
     *   for a match to a noun phrase when all of the following conditions
     *   are true:
     *   
     *   - this object is in scope;
     *   
     *   - our vocabulary matches the noun phrase, which means that ALL of
     *   the words in the player's noun phrase are associated with this
     *   object with the corresponding parts of speech.  Note the special
     *   wildcard vocabulary words: '#' as an adjective matches any number
     *   used as an adjective; '*' as a noun matches any word used as any
     *   part of speech.
     *   
     *   'origTokens' is the list of the original input words making up
     *   the noun phrase, in canonical tokenizer format.  Each element of
     *   this list is a sublist representing one token.
     *   
     *   'adjustedTokens' is the "adjusted" token list, which provides
     *   more information on how the parser is analyzing the phrase but
     *   may not contain the exact original tokens of the command.  In the
     *   adjusted list, the tokens are represented by pairs of values in
     *   the list: the first value of each pair is a string giving the
     *   adjusted token text, and the second value of the pair is a
     *   property ID giving the part of speech of the parser's
     *   interpretation of the phrase.  For example, if the noun phrase is
     *   "red book", the list might look like ['red', &adjective, 'book',
     *   &noun].
     *   
     *   The adjusted token list in some cases contains different tokens
     *   than the original input.  For example, when the command contains
     *   a spelled-out number, the parser will translate the spelled-out
     *   number to a numeral format and provide the numeral string in the
     *   adjusted token list: 'a hundred and thirty-four' will become
     *   '134' in the adjusted token list.
     *   
     *   If this object does not match the noun phrase, this routine
     *   returns nil.  If the object is a match, the routine returns
     *   'self'.  The routine can also return a different object, or even
     *   a list of objects - in this case, the parser will consider the
     *   noun phrase to have matched the returned object or objects rather
     *   than this original match.
     *   
     *   Note that it isn't necessary to check that the input tokens match
     *   our defined vocabulary words, because the parser will already
     *   have done that for us.  This routine is only called after the
     *   parser has already determined that all of the noun phrase's words
     *   match ours.  
     *   
     *   By default, we do two things.  First, we check to see if ALL of
     *   our tokens are "weak" tokens, and if they are, we indicate that
     *   we do NOT match the phrase.  Second, if we pass the "weak token"
     *   test, we'll invoke the common handling in matchNameCommon(), and
     *   return the result.
     *   
     *   In most cases, games will want to override matchNameCommon()
     *   instead of this routine.  matchNameCommon() is the common handler
     *   for both normal and disambiguation-reply matching, so overriding
     *   that one routine will take care of both kinds of matching.  Games
     *   will only need to override matchName() separately in cases where
     *   they need to differentiate normal matching and disambiguation
     *   matching.  
     */
    matchName(origTokens, adjustedTokens)
    {
        /* if we have a weak-token list, check the tokens */
    weakTest:
        if (weakTokens != nil)
        {
            local sc = languageGlobals.dictComparator;
                
            /* check to see if all of our tokens are "weak" */
            for (local i = 1, local len = adjustedTokens.length() ;
                 i <= len ; i += 2)
            {
                /* get the current token and its type */
                local tok = adjustedTokens[i];
                local typ = adjustedTokens[i+1];

                /* 
                 *   if this is a miscWord token, skip it - these aren't
                 *   vocabulary matches, so we won't find them in either
                 *   our weak or strong vocabulary lists 
                 */
                if (typ == &miscWord)
                    continue;

                /* if this token isn't in our weak list, it's not weak */
                if (weakTokens.indexWhich({x: sc.matchValues(tok, x) != 0})
                    == nil)
                {
                    /* 
                     *   It's not in the weak list, so this isn't a weak
                     *   token; therefore the phrase isn't weak, so we do
                     *   not wish to veto the match.  There's no need to
                     *   keep looking; simply break out of the weak test
                     *   entirely, since we've now passed. 
                     */
                    break weakTest;
                }
            }

            /* 
             *   If we get here, it means we got through the loop without
             *   finding any non-weak tokens.  This means the entire
             *   phrase is weak, which means that we don't match it.
             *   Return nil to indicate that this is not a match.  
             */
            return nil;
         }

        /* invoke the common handling */
        return matchNameCommon(origTokens, adjustedTokens);
    }

    /*
     *   Match a name in a disambiguation response.  This is similar to
     *   matchName(), but is called for each object in an ambiguous object
     *   list for which a disambiguation response was provided.  As with
     *   matchName(), we only call this routine for objects that match the
     *   dictionary vocabulary.
     *   
     *   This routine is separate from matchName() because a
     *   disambiguation response usually only contains a partial name.
     *   For example, the exchange might go something like this:
     *   
     *   >take box
     *.  Which box do you mean, the cardboard box, or the wood box?
     *   >cardboard
     *   
     *   Note that it is not safe to assume that the disambiguation
     *   response can be prepended to the original noun phrase to make a
     *   complete noun phrase; if this were safe, we'd simply concatenate
     *   the two strings and call matchName().  This would work for the
     *   example above, since we'd get "cardboard box" as the new noun
     *   phrase, but it wouldn't work in general.  Consider these examples:
     *   
     *   >open post office box
     *.  Which post office box do you mean, box 100, box 101, or box 102?
     *   
     *   >take jewel
     *.  Which jewel do you mean, the emerald, or the diamond?
     *   
     *   There's no general way of assembling the disambiguation response
     *   and the original noun phrase together into a new noun phrase, so
     *   rather than trying to use matchName() for both purposes, we
     *   simply use a separate routine to match the disambiguation name.
     *   
     *   Note that, when this routine is called, this object will have
     *   been previously matched with matchName(), so there is no question
     *   that this object matches the original noun phrase.  The only
     *   question is whether or not this object matches the response to
     *   the "which one do you mean" question.
     *   
     *   The return value has the same meaning as for matchName().
     *   
     *   By default, we simply invoke the common handler.  Note that games
     *   will usually want to override matchNameCommon() instead of this
     *   routine, since matchNameCommon() provides common handling for the
     *   main match and disambiguation match cases.  Games should only
     *   override this routine when they need to do something different
     *   for normal vs disambiguation matching.    
     */
    matchNameDisambig(origTokens, adjustedTokens)
    {
        /* by default, use the same common processing */
        return matchNameCommon(origTokens, adjustedTokens);
    }

    /*
     *   Common handling for the main matchName() and the disambiguation
     *   handler matchNameDisambig().  By default, we'll check with our
     *   state object if we have a state object; if not, we'll simply
     *   return 'self' to indicate that we do indeed match the given
     *   tokens.
     *   
     *   In most cases, when a game wishes to customize name matching for
     *   an object, it can simply override this routine.  This routine
     *   provides common handling for matchName() and matchNameDisambig(),
     *   so overriding this routine will take care of both the normal and
     *   disambiguation matching cases.  In cases where a game needs to
     *   customize only normal matching or only disambiguation matching,
     *   it can override one of those other routines instead.  
     */
    matchNameCommon(origTokens, adjustedTokens)
    {
        local st;
        
        /* 
         *   if we have a state, ask our state object to check for words
         *   applying only to other states 
         */
        if ((st = getState()) != nil)
            return st.matchName(self, origTokens, adjustedTokens, allStates);

        /* by default, accept the parser's determination that we match */
        return self;
    }

    /*
     *   Plural resolution order.  When a command contains a plural noun
     *   phrase, we'll sort the items that match the plural phrase in
     *   ascending order of this property value.
     *   
     *   In most cases, the plural resolution order doesn't matter.  Once
     *   in a while, though, a set of objects will be named as "first,"
     *   "second," "third," and so on; in these cases, it's nice to have
     *   the order of resolution match the nominal ordering.
     *   
     *   Note that the sorting order only applies within the matches for a
     *   particular plural phrase, not globally throughout the entire list
     *   of objects in a command.  For example, if the player types TAKE
     *   BOXES AND BOOKS, we'll sort the boxes in one group, and then we'll
     *   sort the books as a separate group - but all of the boxes will
     *   come before any of the books, regardless of the plural orders.
     *   
     *   >take books and boxes
     *.  first book: Taken.
     *.  second book: Taken.
     *.  third book: Taken.
     *.  left box: Taken.
     *.  middle box: Taken.
     *.  right box: Taken.
     */
    pluralOrder = 100

    /*
     *   Disambiguation prompt order.  When we interactively prompt for
     *   help resolving an ambiguous noun phrase, we'll put the list of
     *   ambiguous matches in ascending order of this property value.
     *   
     *   In most cases, the prompt order doesn't matter, so most objects
     *   won't have to override the default setting.  Sometimes, though, a
     *   set of objects will be identified in the game as "first",
     *   "second", "third", etc., and in these cases it's desirable to have
     *   the objects presented in the same order as the names indicate:
     *   
     *   Which door do you mean, the first door, the second door, or the
     *   third door?
     *   
     *   By default, we use the same value as our pluralOrder, since the
     *   plural order has essentially the same purpose.  
     */
    disambigPromptOrder = (pluralOrder)

    /*
     *   Intrinsic parsing likelihood.  During disambiguation, if the
     *   parser finds two objects with equivalent logicalness (as
     *   determined by the 'verify' process for the particular Action being
     *   performed), it will pick the one with the higher intrinsic
     *   likelihood value.  The default value is zero, which makes all
     *   objects equivalent by default.  Set a higher value to make the
     *   parser prefer this object in cases of ambiguity.  
     */
    vocabLikelihood = 0

    /*
     *   Filter an ambiguous noun phrase resolution list.  The parser calls
     *   this method for each object that matches an ambiguous noun phrase
     *   or an ALL phrase, to allow the object to modify the resolution
     *   list.  This method allows the object to act globally on the entire
     *   list, so that the filtering can be sensitive to the presence or
     *   absence in the list of other objects, and can affect the presence
     *   of other objects.
     *   
     *   'lst' is a list of ResolveInfo objects describing the tentative
     *   resolution of the noun phrase.  'action' is the Action object
     *   representing the command.  'whichObj' is the object role
     *   identifier of the object being resolved (DirectObject,
     *   IndirectObject, etc).  'np' is the noun phrase production that
     *   we're resolving; this is usually a subclass of NounPhraseProd.
     *   'requiredNum' is the number of objects required, when an exact
     *   quantity is specified; this is nil in cases where the quantity is
     *   unspecified, as in 'all' or an unquantified plural ("take coins").
     *   
     *   The result is a new list of ResolveInfo objects, which need not
     *   contain any of the objects of the original list, and can add new
     *   objects not in the original list, as desired.
     *   
     *   By default, we simply return the original list.  
     */
    filterResolveList(lst, action, whichObj, np, requiredNum)
    {
        /* return the original list unchanged */
        return lst;
    }

    /*
     *   Expand a pronoun list.  This is essentially complementary to
     *   filterResolveList: the function is to "unfilter" a pronoun binding
     *   that contains this object so that it restores any objects that
     *   would have been filtered out by filterResolveList from the
     *   original noun phrase binding.
     *   
     *   This routine is called whenever the parser is called upon to
     *   resolve a pronoun ("TAKE THEM").  This routine is called for each
     *   object in the "raw" pronoun binding, which is simply the list of
     *   objects that was stored by the previous command as the antecedent
     *   for the pronoun.  After this routine has been called for each
     *   object in the raw pronoun binding, the final list will be passed
     *   through filterResolveList().
     *   
     *   'lst' is the raw pronoun binding so far, which might reflect
     *   changes made by this method called on previous objects in the
     *   list.  'typ' is the pronoun type (PronounIt, PronounThem, etc)
     *   describing the pronoun phrase being resolved.  The return value is
     *   the new pronoun binding list; if this routine doesn't need to make
     *   any changes, it should simply return 'lst'.
     *   
     *   In some cases, filterResolveList chooses which of two or more
     *   possible ways to bind a noun phrase, with the binding dependent
     *   upon other conditions, such as the current action.  In these
     *   cases, it's often desirable for a subsequent pronoun reference to
     *   make the same decision again, choosing from the full set of
     *   possible bindings.  This routine facilitates that by letting the
     *   object put back objects that were filtered out, so that the
     *   filtering can once again run on the full set of possible bindings
     *   for the pronoun reference.
     *   
     *   This base implementation just returns the original list unchanged.
     *   See CollectiveGroup for an override that uses this.  
     */
    expandPronounList(typ, lst) { return lst; }

    /*
     *   Our list of "weak" tokens.  This is a token that is acceptable in
     *   our vocabulary, but which we can only use in combination with one
     *   or more "strong" tokens.  (A token is strong if it's not weak, so
     *   we need only keep track of one or the other kind.  Weak tokens
     *   are much less common than strong tokens, so it takes a lot less
     *   space if we store the weak ones instead of the strong ones.)
     *   
     *   The purpose of weak tokens is to allow players to use more words
     *   to refer to some objects without creating ambiguity.  For
     *   example, if we have a house, and a front door of the house, we
     *   might want to allow the player to call the front door "front door
     *   of house."  If we just defined the door's vocabulary thus,
     *   though, we'd create ambiguity if the player tried to refer to
     *   "house," even though this obviously doesn't create any ambiguity
     *   to a human reader.  Weak tokens fix the problem: we define
     *   "house" as a weak token for the front door, which allows the
     *   player to refer to the front door as "front door of house", but
     *   prevents the front door from matching just "house".
     *   
     *   By default, this is nil to indicate that we don't have any weak
     *   tokens to check.  If the object has weak tokens, this should be
     *   set to a list of strings giving the weak tokens.  
     */
    weakTokens = nil

    /*
     *   By default, every object can be used as the resolution of a
     *   possessive qualifier phrase (e.g., "bob" in "bob's book").  If
     *   this property is set to nil for an object, that object can never
     *   be used as a possessive.  Note that has nothing to do with
     *   establishing ownership relationships between objects; it controls
     *   only the resolution of possessive phrases during parsing to
     *   concrete game objects.  
     */
    canResolvePossessive = true

    /*
     *   Throw an appropriate parser error when this object is used in a
     *   player command as a possessive qualifier (such as when 'self' is
     *   the "bob" in "take bob's key"), and we don't own anything matching
     *   the object name that we qualify.  This is only called when 'self'
     *   is in scope.  By default, we throw the standard parser error ("Bob
     *   doesn't appear to have any such thing").  'txt' is the
     *   lower-cased, HTMLified text that of the qualified object name
     *   ("key" in "bob's key").  
     */
    throwNoMatchForPossessive(txt)
        { throw new ParseFailureException(&noMatchForPossessive, self, txt); }

    /* 
     *   Throw an appropriate parser error when this object is used in a
     *   player command to locationally qualify another object (such as
     *   when we're the box in "examine the key in the box"), and there's
     *   no object among our contents with the given name.  By default, we
     *   throw the standard parser error ("You see no key in the box").  
     */
    throwNoMatchForLocation(txt)
        { throw new ParseFailureException(&noMatchForLocation, self, txt); }

    /*
     *   Throw an appropriate parser error when this object is used in a
     *   player command to locationally qualify "all" (such as when we're
     *   the box in "examine everything in the box"), and we have no
     *   contents.  By default, we throw the standard parser error ("You
     *   see nothing unusual in the box"). 
     */
    throwNothingInLocation()
        { throw new ParseFailureException(&nothingInLocation, self); }

    /*
     *   Get a list of the other "facets" of this object.  A facet is
     *   another program object that to the player looks like the same or
     *   part of the same physical object.  For example, it's often
     *   convenient to represent a door using two game objects - one for
     *   each side - but the two really represent the same door from the
     *   player's perspective.
     *   
     *   The parser uses an object's facets to resolve a pronoun when the
     *   original antecedent goes out of scope.  In our door example, if
     *   we refer to the door, then walk through it to the other side,
     *   then refer to 'it', the parser will realize from the facet
     *   relationship that 'it' now refers to the other side of the door.  
     */
    getFacets() { return []; }

    /*
     *   Ownership: a vocab-object can be marked as owned by a given Thing.
     *   This allows command input to refer to the owned object using
     *   possessive syntax (such as, in English, "x's y").
     *   
     *   This method returns true if 'self' is owned by 'obj'.  The parser
     *   generally tests for ownership in this direction, as opposed to
     *   asking for obj's owner, because a given object might have multiple
     *   owners, and might not be able to enumerate them all (or, at least,
     *   might not be able to enumerate them efficiently).  It's usually
     *   efficient to determine whether a given object qualifies as an
     *   owner, and from the parser's persepctive that's the question
     *   anyway, since it wants to determine if the "x" in "x's y"
     *   qualifies as my owner.
     *   
     *   By default, we simply return true if 'obj' matches our 'owner'
     *   property (and is not nil).  
     */
    isOwnedBy(obj) { return owner != nil && owner == obj; }

    /*
     *   Get our nominal owner.  This is the owner that we report for this
     *   object if we're asked to distinguish this object from another
     *   object in a disambiguation prompt.  The nominal owner isn't
     *   necessarily the only owner.  Note that if getNominalOwner()
     *   returns a non-nil value, isOwnedBy(getNominalOwner()) should
     *   always return true.
     *   
     *   By default, we'll simply return our 'owner' property.  
     */
    getNominalOwner() { return owner; }

    /* our explicit owner, if any */
    owner = nil
;

/* ------------------------------------------------------------------------ */
/*
 *   Find the best facet from the given list of facets, from the
 *   perspective of the given actor.  We'll find the facet that has the
 *   best visibility, or, visibilities being equal, the best touchability. 
 */
findBestFacet(actor, lst)
{
    local infoList;
    local best;

    /* 
     *   if the list has no elements, there's no result; if it has only one
     *   element, it's obviously the best one 
     */
    if (lst.length() == 0)
        return nil;
    if (lst.length() == 1)
        return lst[1];
    
    /* make a list of the visibilities of the given facets */
    infoList = lst.mapAll({x: new SightTouchInfo(actor, x)});

    /* scan the list and find the entry with the best results */
    best = nil;
    foreach (local cur in infoList)
        best = SightTouchInfo.selectBetter(best, cur);

    /* return the best result */
    return best.obj_;
}

/*
 *   A small data structure class recording SenseInfo objects for sight and
 *   touch.  We use this to pick the best facet from a list of facets.  
 */
class SightTouchInfo: object
    construct(actor, obj)
    {
        /* remember our object */
        obj_ = obj;
        
        /* get the visual SenseInfo in the actor's best sight-like sense */
        visInfo = actor.bestVisualInfo(obj);

        /* get the 'touch' SenseInfo for the object */
        touchInfo = actor.senseObj(touch, obj);
    }

    /* the object we're associated with */
    obj_ = nil

    /* our SenseInfo objects for sight and touch */
    visInfo = nil
    touchInfo = nil

    /*
     *   Class method: select the "better" of two SightTouchInfo's.
     *   Returns the one with the more transparent visual status, or,
     *   visual transparencies being equal, the one with the more
     *   transparent touch status. 
     */
    selectBetter(a, b)
    {
        local d;
        
        /* if one or the other is nil, return the non-nil one */
        if (a == nil)
            return b;
        if (b == nil)
            return a;

        /* if the visual transparencies differ, compare visually */
        d = a.visInfo.compareTransTo(b.visInfo);
        if (d > 0)
            return a;
        else if (d < 0)
            return b;
        else
        {
            /* the visual status it the same, so compare by touch */
            d = a.touchInfo.compareTransTo(b.touchInfo);
            if (d >= 0)
                return a;
            else
                return b;
        }
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Thing: the basic class for game objects.  An object of this class
 *   represents a physical object in the simulation.
 */
class Thing: VocabObject
    /* 
     *   on constructing a new Thing, initialize it as we would a
     *   statically instantiated Thing 
     */
    construct()
    {
        /* 
         *   If we haven't already been through here for this object, do
         *   the static initialization.  (Because many library classes and
         *   game objects inherit from multiple Thing subclasses, we'll
         *   sometimes inherit this constructor multiple times in the
         *   course of creating a single new object.  The set-up work we do
         *   isn't meant to be repeated and can create internal
         *   inconsistencies if it is.) 
         */
        if (!isThingConstructed)
        {
            /* inherit base class construction */
            inherited();

            /* initialize the Thing properties */
            initializeThing();

            /* note that we've been through this constructor now */
            isThingConstructed = true;
        }
    }

    /* 
     *   Have we been through Thing.construct() yet for this object?  Note
     *   that this will only be set for dynamically created instances
     *   (i.e., objects created with 'new'). 
     */
    isThingConstructed = nil

    /*
     *   My global parameter name.  This is a name that can be used in
     *   {xxx} parameters in messages to refer directly to this object.
     *   This is nil by default, which means we have no global parameter
     *   name.  Define this to a single-quoted string to set a global
     *   parameter name.
     *   
     *   Global parameter names can be especially useful for objects whose
     *   names change in the course of the game, such as actors who are
     *   known by one name until introduced, after which they're known by
     *   another name.  It's a little nicer to write "{He/the bob}" than
     *   "<<bob.theName>>".  We can do this with a global parameter name,
     *   because it allows us to use {bob} as a message parameter, even
     *   when the actor isn't involved directly in any command.
     *   
     *   Note that these parameter names are global, so no two objects are
     *   allowed to have the same name.  These names are also subordinate
     *   to the parameter names in the current Action, so names that the
     *   actions define, such as 'dobj' and 'actor', should usually be
     *   avoided.  
     */
    globalParamName = nil

    /* 
     *   Set the global parameter name dynamically.  If you need to add a
     *   new global parameter name at run-time, call this rather than
     *   setting the property directly, to ensure that the name is added to
     *   the message builder's name table.  You can also use this to delete
     *   an object's global parameter name, by passing nil for the new
     *   name.
     *   
     *   (You only need to use this method if you want to add or change a
     *   name dynamically at run-time, because the library automatically
     *   initializes the table for objects with globalParamName settings
     *   defined at compile-time.)  
     */
    setGlobalParamName(name)
    {
        /* if we already had a name in the table, remove our old name */
        if (globalParamName != nil
            && langMessageBuilder.nameTable_[globalParamName] == self)
            langMessageBuilder.nameTable_.removeElement(globalParamName);

        /* remember our name locally */
        globalParamName = name;

        /* add the name to the message builder's table */
        if (name != nil)
            langMessageBuilder.nameTable_[name] = self;
    }

    /*
     *   "Special" description.  This is the generic place to put a
     *   description of the object that should appear in the containing
     *   room's full description.  If the object defines a special
     *   description, the object is NOT listed in the basic contents list
     *   of the room, because listing it with the contents would be
     *   redundant with the special description.
     *   
     *   By default, we have no special description.  If a special
     *   description is desired, define this to a double-quoted string
     *   containing the special description, or to a method that displays
     *   the special description.  
     */
    specialDesc = nil

    /*
     *   The special descriptions to use under obscured and distant
     *   viewing conditions.  By default, these simply display the normal
     *   special description, but these can be overridden if desired to
     *   show different messages under these viewing conditions.
     *   
     *   Note that if you override these alternative special descriptions,
     *   you MUST also provide a base specialDesc.  The library figures
     *   out whether or not to show any sort of specialDesc first, based
     *   on the presence of a non-nil specialDesc; only then does the
     *   library figure out which particular variation to use.
     *   
     *   Note that remoteSpecialDesc takes precedence over these methods.
     *   That is, when 'self' is in a separate top-level room from the
     *   point-of-view actor, we'll use remoteSpecialDesc to generate our
     *   description, even if the sense path to 'self' is distant or
     *   obscured.  
     */
    distantSpecialDesc = (specialDesc)
    obscuredSpecialDesc = (specialDesc)

    /*
     *   The "remote" special description.  This is the special
     *   description that will be used when this object is not in the
     *   point-of-view actor's current top-level room, but is visible in a
     *   connected room.  For example, if two top-level rooms are
     *   connected by a window, so that an actor in one room can see the
     *   objects in the other room, this method will be used to generate
     *   the description of the object when the actor is in the other
     *   room, viewing this object through the window.
     *   
     *   By default, we just use the special description.  It's usually
     *   better to customize this to describe the object from the given
     *   point of view.  'actor' is the point-of-view actor.  
     */
    remoteSpecialDesc(actor) { distantSpecialDesc; }

    /*
     *   List order for the special description.  Whenever there's more
     *   than one object showing a specialDesc at the same time (in a
     *   single room description, for example), we'll use this to order the
     *   specialDesc displays.  We'll display in ascending order of this
     *   value.  By default, we use the same value for everything, so
     *   listing order is arbitrary; when one specialDesc should appear
     *   before or after another, this property can be used to control the
     *   relative ordering.  
     */
    specialDescOrder = 100

    /*
     *   Special description phase.  We list special descriptions for a
     *   room's full description in two phases: one phase before we show
     *   the room's portable contents list, and another phase after we show
     *   the contents list.  This property controls the phase in which we
     *   show this item's special description.  This only affects special
     *   descriptions that are shown with room descriptions; in other
     *   cases, such as "examine" descriptions of objects, all of the
     *   special descriptions are usually shown together.
     *   
     *   By default, we show our special description (if any) before the
     *   room's contents listing, because most special descriptions act
     *   like extensions of the room's main description and thus should be
     *   grouped directly with the room's descriptive text.  Objects with
     *   special descriptions that are meant to indicate more ephemeral
     *   properties of the location can override this to be listed after
     *   the room's portable contents.
     *   
     *   One situation where you usually will want to list a special
     *   description after contents is when the special description applies
     *   to an item that's contained in a portable item.  Since the
     *   container will be listed with the room contents, as it's portable,
     *   we'll usually want the special description of this child item to
     *   show up after the contents listing, so that it shows up after its
     *   container is mentioned.
     *   
     *   Note that the specialDescOrder is secondary to this phase
     *   grouping, because we essentially list special items in two
     *   separate groups.  
     */
    specialDescBeforeContents = true

    /*
     *   Show my special description, given a SenseInfo object for the
     *   visual sense path from the point of view of the description.  
     */
    showSpecialDescWithInfo(info, pov)
    {
        local povActor = getPOVActorDefault(gActor);
        
        /* 
         *   Determine what to show, based on the point-of-view location
         *   and the sense path.  If the point of view isn't in the same
         *   top-level location as 'self', OR the actor isn't the same as
         *   the POV, use the remote special description; otherwise, select
         *   the obscured, distant, or basic special description according
         *   to the transparency of the sense path.  
         */
        if (getOutermostRoom() != povActor.getOutermostRoom()
            || pov != povActor)
        {
            /* 
             *   different top-level rooms, or different actor and POV -
             *   use the remote description 
             */
            showRemoteSpecialDesc(povActor);
        }
        else if (info.trans == obscured)
        {
            /* we're obscured, so show our obscured special description */
            showObscuredSpecialDesc();
        }
        else if (info.trans == distant)
        {
            /* we're at a distance, so use our distant special description */
            showDistantSpecialDesc();
        }
        else if (canDetailsBeSensed(sight, info, pov))
        {
            /* 
             *   we're not obscured or distant, and our details can be
             *   sensed, so show our fully-visible special description 
             */
            showSpecialDesc();
        }
    }

    /*
     *   Show the special description, if we have one.  If we are using
     *   our initial description, we'll show that; otherwise, if we have a
     *   specialDesc property, we'll show that.
     *   
     *   Note that the initial description overrides the specialDesc
     *   property whenever useInitSpecialDesc() returns true.  This allows
     *   an object to have both an initial description that is used until
     *   the object is moved, and a separate special description used
     *   thereafter.  
     */
    showSpecialDesc()
    {
        /* 
         *   if we are to use our initial description, show that;
         *   otherwise, show our special description 
         */
        if (useInitSpecialDesc())
            initSpecialDesc;
        else
            specialDesc;
    }

    /* show the special description under obscured viewing conditions */
    showObscuredSpecialDesc()
    {
        if (useInitSpecialDesc())
            obscuredInitSpecialDesc;
        else
            obscuredSpecialDesc;
    }

    /* show the special description under distant viewing conditions */
    showDistantSpecialDesc()
    {
        if (useInitSpecialDesc())
            distantInitSpecialDesc;
        else
            distantSpecialDesc;
    }

    /* show the remote special description */
    showRemoteSpecialDesc(actor)
    {
        if (useInitSpecialDesc())
            remoteInitSpecialDesc(actor);
        else
            remoteSpecialDesc(actor);
    }

    /*
     *   Determine if we should use a special description.  By default, we
     *   have a special description if we have either a non-nil
     *   specialDesc property, or we have an initial description.  
     */
    useSpecialDesc()
    {
        /* 
         *   if we have a non-nil specialDesc, or we are to use an initial
         *   description, we have a special description 
         */
        return propType(&specialDesc) != TypeNil || useInitSpecialDesc();
    }

    /*
     *   Determine if we should use our special description in the given
     *   room's LOOK AROUND description.  By default, this simply returns
     *   useSpecialDesc().  
     */
    useSpecialDescInRoom(room) { return useSpecialDesc(); }

    /*
     *   Determine if we should use our special description in the given
     *   object's contents listings, for the purposes of "examine <cont>"
     *   or "look in <cont>".  By default, we'll use our special
     *   description for a given container if we'd use our special
     *   description in general, AND we're actually inside the container
     *   being examined.  
     */
    useSpecialDescInContents(cont)
    {
        /* 
         *   if we would otherwise use a special description, and we are
         *   contained within the given object, use our special description
         *   in the container 
         */
        return useSpecialDesc() && self.isIn(cont);
    }

    /*
     *   Show our special description as part of a parent's full
     *   description.  
     */
    showSpecialDescInContentsWithInfo(info, pov, cont)
    {
        local povActor = getPOVActorDefault(gActor);
        
        /* determine what to show, based on the location and sense path */
        if (getOutermostRoom() != povActor.getOutermostRoom()
            || pov != povActor)
            showRemoteSpecialDescInContents(povActor, cont);
        else if (info.trans == obscured)
            showObscuredSpecialDescInContents(povActor, cont);
        else if (info.trans == distant)
            showDistantSpecialDescInContents(povActor, cont);
        else if (canDetailsBeSensed(sight, info, pov))
            showSpecialDescInContents(povActor, cont);
    }

    /* 
     *   Show the special description in contents listings under various
     *   sense conditions.  By default, we'll use the corresponding special
     *   descriptions for full room descriptions.  These can be overridden
     *   to show special versions of the description when we're examining
     *   particular containers, if desired.  'actor' is the actor doing the
     *   looking.  
     */
    showSpecialDescInContents(actor, cont)
        { showSpecialDesc(); }
    showObscuredSpecialDescInContents(actor, cont)
        { showObscuredSpecialDesc(); }
    showDistantSpecialDescInContents(actor, cont)
        { showDistantSpecialDesc(); }
    showRemoteSpecialDescInContents(actor, cont)
        { showRemoteSpecialDesc(actor); }

    /*
     *   If we define a non-nil initSpecialDesc, this property will be
     *   called to describe the object in room listings as long as the
     *   object is in its "initial" state (as determined by isInInitState:
     *   this is usually true until the object is first moved to a new
     *   location).  By default, objects don't have initial descriptions.
     *   
     *   If this is non-nil, and the object is portable, this will be used
     *   (as long as the object is in its initial state) instead of
     *   showing the object in an ordinary room-contents listing.  This
     *   can be used to give the object a special mention in its initial
     *   location in the game, rather than letting the ordinary
     *   room-contents lister lump it in with all of the other portable
     *   object lying around.  
     */
    initSpecialDesc = nil

    /*
     *   The initial descriptions to use under obscured and distant
     *   viewing conditions.  By default, these simply show the plain
     *   initSpecialDesc; these can be overridden, if desired, to show
     *   alternative messages when viewing conditions are less than ideal.
     *   
     *   Note that in order for one of these alternative initial
     *   descriptions to be shown, the regular initSpecialDesc MUST be
     *   defined, even if it's never actually used.  We make the decision
     *   to display these other descriptions based on the existence of a
     *   non-nil initSpecialDesc, so always define initSpecialDesc
     *   whenever these are defined.  
     */
    obscuredInitSpecialDesc = (initSpecialDesc)
    distantInitSpecialDesc = (initSpecialDesc)

    /* the initial remote special description */
    remoteInitSpecialDesc(actor) { distantInitSpecialDesc; }

    /*
     *   If we define a non-nil initDesc, and the object is in its initial
     *   description state (as indicated by isInInitState), then we'll use
     *   this property instead of "desc" to describe the object when
     *   examined.  This can be used to customize the description the
     *   player sees in parallel to initSpecialDesc.  
     */
    initDesc = nil

    /*
     *   Am I in my "initial state"?  This is used to determine if we
     *   should show the initial special description (initSpecialDesc) and
     *   initial examine description (initDesc) when describing the
     *   object.  By default, we consider the object to be in its initial
     *   state until the first time it's moved.
     *   
     *   You can override this to achieve other effects.  For example, if
     *   you want to use the initial description only the first time the
     *   object is examined, and then revert to the ordinary description,
     *   you could override this to return (!described).  
     */
    isInInitState = (!moved)

    /*
     *   Determine whether or not I should be mentioned in my containing
     *   room's description (on LOOK AROUND) using my initial special
     *   description (initSpecialDesc).  This returns true if I have an
     *   initial description that isn't nil, and I'm in my initial state.
     *   If this returns nil, the object should be described in room
     *   descriptions using the ordinary generated message (either the
     *   specialDesc, if we have one, or the ordinary mention in the list
     *   of portable room contents).  
     */
    useInitSpecialDesc()
        { return isInInitState && propType(&initSpecialDesc) != TypeNil; }

    /* 
     *   Determine if I should be described on EXAMINE using my initial
     *   examine description (initDesc).  This returns true if I have an
     *   initial examine desription that isn't nil, and I'm in my initial
     *   state.  If this returns nil, we'll show our ordinary description
     *   (given by the 'desc' property).  
     */
    useInitDesc()
        { return isInInitState && propType(&initDesc) != TypeNil; }


    /*
     *   Flag: I've been moved out of my initial location.  Whenever we
     *   move the object to a new location, we'll set this to true.  
     */
    moved = nil

    /*
     *   Flag: I've been seen by the player character.  This is nil by
     *   default; we set this to true whenever we show a room description
     *   from the player character's perspective, and the object is
     *   visible.  The object doesn't actually have to be mentioned in the
     *   room description to be marked as seen - it merely has to be
     *   visible to the player character.
     *   
     *   Note that this is only the DEFAULT 'seen' property, which all
     *   Actor objects use by default.  The ACTUAL property that a given
     *   Actor uses depends on the actor's seenProp, which allows a game to
     *   keep track separately of what each actor has seen by using
     *   different 'seen' properties for different actors.  
     */
    seen = nil

    /*
     *   Flag: suppress the automatic setting of the "seen" status for this
     *   object in room and object descriptions.  Normally, we'll
     *   automatically mark every visible object as seen (by calling
     *   gActor.setHasSeen()) whenever we do a LOOK AROUND.  We'll also
     *   automatically mark as seen every visible object within an object
     *   examined explicitly (such as with EXAMINE, LOOK IN, LOOK ON, or
     *   OPEN).  This property can override this automatic status change:
     *   when this property is true, we will NOT mark this object as seen
     *   in any of these cases.  When this property is true, the game must
     *   explicitly mark the object as seen, if and when desired, by
     *   calling actor.setHasSeen().
     *   
     *   Sometimes, an object is not meant to be immediately obvious.  For
     *   example, a puzzle box might have a hidden button that can't be
     *   seen on casual examination.  In these cases, you can use
     *   suppressAutoSeen to ensure that the object won't be marked as seen
     *   merely by virtue of its being visible at the time of a LOOK AROUND
     *   or EXAMINE command.
     *   
     *   Note that this property does NOT affect the object's actual
     *   visibility or other sensory attributes.  This property merely
     *   controls the automatic setting of the "seen" status of the object.
     */
    suppressAutoSeen = nil

    /*
     *   Flag: I've been desribed.  This is nil by default; we set this to
     *   true whenever the player explicitly examines the object. 
     */
    described = nil

    /*
     *   Mark all visible contents of 'self' as having been seen.
     *   'infoTab' is a LookupTable of sight information, as returned by
     *   visibleInfoTable().  This should be called any time an object is
     *   examined in such a way that its contents should be considered to
     *   have been seen.
     *   
     *   We will NOT mark as seen any objects that have suppressAutoSeen
     *   set to true.  
     */
    setContentsSeenBy(infoTab, actor)
    {
        /* 
         *   run through the table of visible objects, and mark each one
         *   that's contained within 'self' as having been seen 
         */
        infoTab.forEachAssoc(function(obj, info)
        {
            if (obj.isIn(self) && !obj.suppressAutoSeen)
                actor.setHasSeen(obj);
        });
    }

    /*
     *   Mark everything visible as having been seen.  'infoTab' is a
     *   LookupTable of sight information, as returned by
     *   visibleInfoTable().  We'll mark everything visible in the table
     *   as having been seen by the actor, EXCEPT objects that have
     *   suppressAutoSeen set to true.  
     */
    setAllSeenBy(infoTab, actor)
    {
        /* mark everything as seen, except suppressAutoSeen items */
        infoTab.forEachAssoc(function(obj, info)
        {
            if (!obj.suppressAutoSeen)
                actor.setHasSeen(obj);
        });
    }

    /*
     *   Note that I've been seen by the given actor, setting the given
     *   "seen" property.  This routine notifies the object that it's just
     *   been observed by the given actor, allowing it to take any special
     *   action it wants to take in such cases.  
     */
    noteSeenBy(actor, prop)
    {
        /* by default, simply set the given "seen" property to true */
        self.(prop) = true;
    }

    /*
     *   Flag: this object is explicitly "known" to actors in the game,
     *   even if it's never been seen.  This allows the object to be
     *   resolved as a topic in ASK ABOUT commands and the like.
     *   Sometimes, actors know about an object even before it's been seen
     *   - they might simply know about it from background knowledge, or
     *   they might hear about it from another character, for example.
     *   
     *   Like the 'seen' property, this is merely the DEFAULT 'known'
     *   property that we use for actors.  Each actor can individually use
     *   a separate property to track its own knowledge if it prefers; it
     *   can do this simply by overriding its isKnownProp property.  
     */
    isKnown = nil

    /*
     *   Hide from 'all' for a given action.  If this returns true, this
     *   object will never be included in 'all' lists for the given action
     *   class.  This should generally be set only for objects that serve
     *   some sort of internal purpose and don't represent physical
     *   objects in the model world.  By default, objects are not hidden
     *   from 'all' lists.
     *   
     *   Note that returning nil doesn't put the object into an 'all'
     *   list.  Rather, it simply *leaves* it in any 'all' list it should
     *   happen to be in.  Each action controls its own selection criteria
     *   for 'all', and different verbs use different criteria.  No matter
     *   how an action chooses its 'all' list, though, an item will always
     *   be excluded if hideFromAll() returns true for the item.  
     */
    hideFromAll(action) { return nil; }

    /*
     *   Hide from defaulting for a given action.  By default, we're
     *   hidden from being used as a default object for a given action if
     *   we're hidden from the action for 'all'. 
     */
    hideFromDefault(action) { return hideFromAll(action); }
    
    /*
     *   Determine if I'm to be listed at all in my room's description,
     *   and in descriptions of objects containing my container.
     *   
     *   Most objects should be listed normally, but some types of objects
     *   should be suppressed from the normal room listing.  For example,
     *   fixed-in-place scenery objects are generally described in the
     *   custom message for the containing room, so these are normally
     *   omitted from the listing of the room's contents.
     *   
     *   By default, we'll return the same thing as isListedInContents -
     *   that is, if this object is to be listed when its *direct*
     *   container is examined, it'll also be listed by default when any
     *   further enclosing container (including the enclosing room) is
     *   described.  Individual objects can override this to use different
     *   rules.
     *   
     *   Why would we want to be able to list an object when examining in
     *   its direct container, but not when examining an enclosing
     *   container, or the enclosing room?  The most common reason is to
     *   control the level of detail, to avoid overloading the broad
     *   description of the room and the main things in it with every
     *   detail of every deeply-nested container.  
     */
    isListed { return isListedInContents; }

    /*
     *   Determine if I'm listed in explicit "examine" and "look in"
     *   descriptions of my direct container.
     *   
     *   By default, we return true as long as we're not using our special
     *   description in this particular context.  Examining or looking in a
     *   container will normally show special message for any contents of
     *   the container, so we don't want to list the items with special
     *   descriptions in the ordinary list as well.  
     */
    isListedInContents { return !useSpecialDescInContents(location); }

    /*
     *   Determine if I'm listed in inventory listings.  By default, we
     *   include every item in an inventory list.
     */
    isListedInInventory { return true; }

    /* 
     *   by default, regular objects are not listed when they arrive
     *   aboard vehicles (only actors are normally listed in this fashion) 
     */
    isListedAboardVehicle = nil

    /*
     *   Determine if I'm listed as being located in the given room part.
     *   We'll first check to make sure the object is nominally contained
     *   in the room part; if not, it's not listed in the room part.  We'll
     *   then ask the room part itself to make the final determination.  
     */
    isListedInRoomPart(part)
    {
        /* 
         *   list me if I'm nominally contained in the room part AND the
         *   room part wants me listed 
         */
        return (isNominallyInRoomPart(part)
                && part.isObjListedInRoomPart(self));
    }

    /*
     *   Determine if we're nominally in the given room part (floor,
     *   ceiling, wall, etc).  This returns true if we *appear* to be
     *   located directly in/on the given room part object.
     *   
     *   In most cases, a portable object might start out with a special
     *   initial room part location, but once moved and then dropped
     *   somewhere, ends up nominally in the nominal drop destination of
     *   the location where it was dropped.  For example, a poster might
     *   start out being nominally attached to a wall, or a light bulb
     *   might be nominally hanging from the ceiling; if these objects are
     *   taken and then dropped somewhere, they'll simply end up on the
     *   floor.
     *   
     *   Our default behavior models this.  If we've never been moved,
     *   we'll indicate that we're in our initial room part location,
     *   given by initNominalRoomPartLocation.  Once we've been moved (or
     *   if our initNominalRoomPartLocation is nil), we'll figure out what
     *   the nominal drop destination is for our current container, and
     *   then see if we appear to be in that nominal drop destination.  
     */
    isNominallyInRoomPart(part)
    {
        /* 
         *   If we're not directly in the apparent container of the given
         *   room part, then we certainly are not even nominally in the
         *   room part.  We only appear to be in a room part when we're
         *   actually in the room directly containing the room part. 
         */
        if (location == nil
            || location.getRoomPartLocation(part) != location)
            return nil;

        /* if we've never been moved, use our initial room part location */
        if (!moved && initNominalRoomPartLocation != nil)
            return (part == initNominalRoomPartLocation);

        /* if we have an explicit special room part location, use that */
        if (specialNominalRoomPartLocation != nil)
            return (part == specialNominalRoomPartLocation);

        /*
         *   We don't seem to have an initial or special room part
         *   location, but there's still one more possibility: if the room
         *   part is the nominal drop destination, then we are by default
         *   nominally in that room part, because that's where we
         *   otherwise end up in the absence of any special room part
         *   location setting.  
         */
        if (location.getNominalDropDestination() == part)
            return true;

        /* we aren't nominally in the given room part */
        return nil;
    }

    /*
     *   Determine if I should show my special description with the
     *   description of the given room part (floor, ceiling, wall, etc).
     *   
     *   By default, we'll include our special description with a room
     *   part's description if either (1) we are using our initial
     *   description, and our initNominalRoomPartLocation is the given
     *   part; or (2) we are using our special description, and our
     *   specialNominalRoomPartLocation is the given part.
     *   
     *   Note that, by default, we do NOT use our special description for
     *   the "default" room part location - that is, for the nominal drop
     *   destination for our containing room, which is where we end up by
     *   default, in the absence of an initial or special room part
     *   location setting.  We don't use our special description in this
     *   default location because special descriptions are most frequently
     *   used to describe an object that is specially situated, and hence
     *   we don't want to assume a default situation.  
     */
    useSpecialDescInRoomPart(part)
    {
        /* if we're not nominally in the part, rule it out */
        if (!isNominallyInRoomPart(part))
            return nil;

        /* 
         *   if we're using our initial description, and we are explicitly
         *   in the given room part initially, use the special description
         *   for the room part 
         */
        if (useInitSpecialDesc() && initNominalRoomPartLocation == part)
            return true;

        /* 
         *   if we're using our special description, and we are explicitly
         *   in the given room part as our special location, use the
         *   special description 
         */
        if (useSpecialDesc() && specialNominalRoomPartLocation == part)
            return true;

        /* do not use the special description in the room part */
        return nil;
    }

    /* 
     *   Our initial room part location.  By default, we set this to nil,
     *   which means that we'll use the nominal drop destination of our
     *   actual initial location when asked.  If desired, this can be set
     *   to another part; for example, if a poster is initially described
     *   as being "on the north wall," this should set to the default north
     *   wall object.  
     */
    initNominalRoomPartLocation = nil

    /*
     *   Our "special" room part location.  By default, we set this to
     *   nil, which means that we'll use the nominal drop destination of
     *   our actual current location when asked.
     *   
     *   This property has a function similar to
     *   initNominalRoomPartLocation, but is used to describe the nominal
     *   room part container of the object has been moved (and even before
     *   it's been moved, if initNominalRoomPartLocation is nil).
     *   
     *   It's rare for an object to have a special room part location
     *   after it's been moved, because most games simply don't provide
     *   commands for things like re-attaching a poster to a wall or
     *   re-hanging a fan from the ceiling.  When it is possible to move
     *   an object to a new special location, though, this property can be
     *   used to flag its new special location.  
     */
    specialNominalRoomPartLocation = nil

    /*
     *   Determine if my contents are to be listed when I'm shown in a
     *   listing (in a room description, inventory list, or a description
     *   of my container).
     *   
     *   Note that this doesn't affect listing my contents when I'm
     *   *directly* examined - use contentsListedInExamine to control that.
     *   
     *   By default, we'll list our contents in these situations if (1) we
     *   have some kind of presence in the description of whatever it is
     *   the player is looking at, either by being listed in the ordinary
     *   way (that is, isListed is true) or in a custom way (via a
     *   specialDesc), and (2) contentsListedInExamine is true.  The
     *   reasoning behind this default is that if the object itself shows
     *   up in the description, then we usually want its contents to be
     *   mentioned as well, but if the object itself isn't mentioned, then
     *   its contents probably shouldn't be, either.  And in any case, if
     *   the contents aren't listed even when we *directly* examine the
     *   object, then we almost certainly don't want its contents mentioned
     *   when we only indirectly mention the object in the course of
     *   describing something enclosing it.  
     */
    contentsListed = (contentsListedInExamine
                      && (isListed || useSpecialDesc()))

    /*
     *   Determine if my contents are to be listed when I'm directly
     *   examined (with an EXAMINE/LOOK AT command).  By default, we
     *   *always* list our contents when we're directly examined.  
     */
    contentsListedInExamine = true

    /*
     *   Determine if my contents are listed separately from my own list
     *   entry.  If this is true, then my contents will be listed in a
     *   separate sentence from my own listing; for example, if 'self' is a
     *   box, we'll be listed like so:
     *   
     *.    You are carrying a box. The box contains a key and a book.
     *   
     *   By default, this is nil, which means that we list our contents
     *   parenthetically after our name:
     *   
     *.     You are carrying a box (which contains a key and a book).
     *   
     *   Using a separate listing is sometimes desirable for an object that
     *   will routinely contain listable objects that in turn have listable
     *   contents of their own, because it can help break up a long listing
     *   that would otherwise use too many nested parentheticals.
     *   
     *   Note that this only applies to "wide" listings; "tall" listings
     *   will show contents in the indented tree format regardless of this
     *   setting.  
     */
    contentsListedSeparately = nil

    /*
     *   The language-dependent part of the library must provide a couple
     *   of basic descriptor methods that we depend upon.  We provide
     *   several descriptor methods that we base on the basic methods.
     *   The basic methods that must be provided by the language extension
     *   are:
     *   
     *   listName - return a string giving the "list name" of the object;
     *   that is, the name that should appear in inventory and room
     *   contents lists.  This is called with the visual sense info set
     *   according to the point of view.
     *   
     *   countName(count) - return a string giving the "counted name" of
     *   the object; that is, the name as it should appear with the given
     *   quantifier.  In English, this might return something like 'one
     *   box' or 'three books'.  This is called with the visual sense info
     *   set according to the point of view.  
     */

    /*
     *   Show this item as part of a list.  'options' is a combination of
     *   ListXxx flags indicating the type of listing.  'infoTab' is a
     *   lookup table of SenseInfo objects giving the sense information
     *   for the point of view.  
     */
    showListItem(options, pov, infoTab)
    {
        /* show the list, using the 'listName' of the state */
        showListItemGen(options, pov, infoTab, &listName);
    }

    /*
     *   General routine to show the item as part of a list.
     *   'stateNameProp' is the property to use in any listing state
     *   object to obtain the state name.  
     */
    showListItemGen(options, pov, infoTab, stateNameProp)
    {
        local info;
        local st;
        local stName;

        /* get my visual information from the point of view */
        info = infoTab[self];

        /* show the item's list name */
        say(withVisualSenseInfo(pov, info, &listName));

        /* 
         *   If we have a list state with a name, show it.  Note that to
         *   obtain the state name, we have to pass a list of the objects
         *   being listed to the stateNameProp method of the state object;
         *   we're the only object we're showing for this particular
         *   display list element, so we simply pass a single-element list
         *   containing 'self'.  
         */
        if ((st = getStateWithInfo(info, pov)) != nil
            && (stName = st.(stateNameProp)([self])) != nil)
        {
            /* we have a state with a name - show it */
            gLibMessages.showListState(stName);
        }
    }

    /*
     *   Show this item as part of a list, grouped with a count of
     *   list-equivalent items.
     */
    showListItemCounted(lst, options, pov, infoTab)
    {
        /* show the item using the 'listName' property of the state */
        showListItemCountedGen(lst, options, pov, infoTab, &listName);
    }

    /* 
     *   General routine to show this item as part of a list, grouped with
     *   a count of list-equivalent items.  'stateNameProp' is the
     *   property of any list state object that we should use to obtain
     *   the name of the listing state.  
     */
    showListItemCountedGen(lst, options, pov, infoTab, stateNameProp)
    {
        local info;
        local stateList;
        
        /* get the visual information from the point of view */
        info = infoTab[self];
        
        /* show our counted name */
        say(countListName(lst.length(), pov, info));

        /* check for list states */
        stateList = new Vector(10);
        foreach (local cur in lst)
        {
            local st;

            /* get this item's sense information from the list */
            info = infoTab[cur];
            
            /* 
             *   if this item has a list state with a name, include it in
             *   our list of states to show 
             */
            if ((st = cur.getStateWithInfo(info, pov)) != nil
                && st.(stateNameProp)(lst) != nil)
            {
                local stInfo;

                /* 
                 *   if this state is already in our list, simply
                 *   increment the count of items in this state;
                 *   otherwise, add the new state to the list 
                 */
                stInfo = stateList.valWhich({x: x.stateObj == st});
                if (stInfo != nil)
                {
                    /* it's already in the list - count the new item */
                    stInfo.addEquivObj(cur);
                }
                else
                {
                    /* it's not in the list - add it */
                    stateList.append(
                        new EquivalentStateInfo(st, cur, stateNameProp));
                }
            }
        }

        /* if the state list is non-empty, show it */
        if (stateList.length() != 0)
        {
            /* put the state list in its desired order */
            stateList.sort(SortAsc, {a, b: (a.stateObj.listingOrder
                                            - b.stateObj.listingOrder)});
                                           
            /*
             *   If there's only one item in the state list, and all of
             *   the objects are in that state, then show the "all in
             *   state" message.  Otherwise, show the list of states with
             *   counts.
             *   
             *   (Note that it's possible to have just one state in the
             *   list without having all of the objects in this state,
             *   because we could have some of the objects in an unnamed
             *   state, in which case they wouldn't contribute to the list
             *   at all.)  
             */
            if (stateList.length() == 1
                && stateList[1].getEquivCount() == lst.length())
            {
                /* everything is in the same state */
                gLibMessages.allInSameListState(stateList[1].getEquivList(),
                                                stateList[1].getName());
            }
            else
            {
                /* list the state items */
                equivalentStateLister.showListAll(stateList.toList(), 0, 0);
            }
        }
    }

    /*
     *   Single-item counted listing description.  This is used to display
     *   an item with a count of equivalent items ("four gold coins").
     *   'info' is the sense information from the current point of view
     *   for 'self', which we take to be representative of the sense
     *   information for all of the equivalent items.  
     */
    countListName(equivCount, pov, info)
    {
        return withVisualSenseInfo(pov, info, &countName, equivCount);
    }

    /*
     *   Show this item as part of an inventory list.  By default, we'll
     *   show the regular list item name.  
     */
    showInventoryItem(options, pov, infoTab)
    {
        /* show the item, using the inventory state name */
        showListItemGen(options, pov, infoTab, &inventoryName);
    }
    showInventoryItemCounted(lst, options, pov, infoTab)
    {
        /* show the item, using the inventory state name */
        showListItemCountedGen(lst, options, pov, infoTab, &inventoryName);
    }

    /*
     *   Show this item as part of a list of items being worn.
     */
    showWornItem(options, pov, infoTab)
    {
        /* show the item, using the worn-listing state name */
        showListItemGen(options, pov, infoTab, &wornName);
    }
    showWornItemCounted(lst, options, pov, infoTab)
    {
        /* show the item, using the worn-listing state name */
        showListItemCountedGen(lst, options, pov, infoTab, &wornName);
    }

    /*
     *   Get the "listing state" of the object, given the visual sense
     *   information for the object from the point of view for which we're
     *   generating the listing.  This returns a ThingState object
     *   describing the object's state for the purposes of listings.  This
     *   should return nil if the object doesn't have varying states for
     *   listings.
     *   
     *   By default, we return a list state if the visual sense path is
     *   transparent or attenuated, or we have large visual scale.  In
     *   other cases, we assume that the details of the object are not
     *   visible under the current sense conditions; since the list state
     *   is normally a detail of the object, we don't return a list state
     *   when the details of the object are not visible.  
     */
    getStateWithInfo(info, pov)
    {
        /* 
         *   if our details can be sensed, return the list state;
         *   otherwise, return nil, since the list state is a detail 
         */
        if (canDetailsBeSensed(sight, info, pov))
            return getState();
        else
            return nil;
    }

    /* 
     *   Get our state - returns a ThingState object describing the state.
     *   By default, we don't have varying states, so we simply return
     *   nil.  
     */
    getState = nil

    /*
     *   Get a list of all of our possible states.  For an object that can
     *   assume varying states as represented by getState, this should
     *   return the list of all possible states that the object can
     *   assume.  
     */
    allStates = []

    /*
     *   The default long description, which is displayed in response to
     *   an explicit player request to examine the object.  We'll use a
     *   generic library message; most objects should override this to
     *   customize the object's desription.
     *   
     *   Note that we show this as a "default descriptive report," because
     *   this default message indicates merely that there's nothing
     *   special to say about the object.  If we generate any additional
     *   description messages, such as status reports ("it's open" or "it
     *   contains a gold key") or special descriptions for things inside,
     *   we clearly *do* have something special to say about the object,
     *   so we'll want to suppress the nothing-special message.  To
     *   accomplish this suppression, all we have to do is report our
     *   generic description as a default descriptive report, and the
     *   transcript will automatically filter it out if there are any
     *   other reports for this same action.
     *   
     *   Note that any time this is overridden by an object with any sort
     *   of actual description, the override should NOT use
     *   defaultDescReport.  Instead, simply set this to display the
     *   descriptive message directly:
     *   
     *   desc = "It's a big green box. " 
     */
    desc { defaultDescReport(&thingDescMsg, self); }

    /*
     *   Our LOOK IN description.  This is shown when we explicitly LOOK IN
     *   the object.  By default, we just report that there's nothing
     *   unusual inside.  
     */
    lookInDesc { mainReport(&nothingInsideMsg); }

    /*
     *   The default "distant" description.  If this is defined for an
     *   object, then we evaluate it to display the description when an
     *   actor explicitly examines this object from a point of view where
     *   we have a "distant" sight path to the object.
     *   
     *   If this property is left undefined for an object, then we'll
     *   describe this object when it's distant in one of two ways.  If
     *   the object has its 'sightSize' property set to 'large', we'll
     *   display the ordinary 'desc', because its large visual size makes
     *   its details visible at a distance.  If the 'sightSize' is
     *   anything else, we'll instead display the default library message
     *   indicating that the object is too far away to see any details.
     *   
     *   To display a specific description when the object is distant, set
     *   this to a double-quoted string, or to a method that displays a
     *   message.  
     */
    // distantDesc = "the distant description"

    /* the default distant description */
    defaultDistantDesc { gLibMessages.distantThingDesc(self); }

    /*
     *   The "obscured" description.  If this is defined for an object,
     *   then we call it to display the description when an actor
     *   explicitly examines this object from a point of view where we
     *   have an "obscured" sight path to the object.
     *   
     *   If this property is left undefined for an object, then we'll
     *   describe this object when it's obscured in one of two ways.  If
     *   the object has its 'sightSize' property set to 'large', we'll
     *   display the ordinary 'desc', because its large visual size makes
     *   its details visible even when the object is obscured.  If the
     *   'sightSize' is anything else, we'll instead display the default
     *   library message indicating that the object is too obscured to see
     *   any details.
     *   
     *   To display a specific description when the object is visually
     *   obscured, override this to a method that displays your message.
     *   'obs' is the object that's obstructing the view - this will be
     *   something on our sense path, such as a dirty window, that the
     *   actor has to look through to see 'self'.  
     */
    // obscuredDesc(obs) { "the obscured description"; }

    /*
     *   The "remote" description.  If this is defined for an object, then
     *   we call it to display the description when an actor explicitly
     *   examines this object from a separate top-level location.  That
     *   is, when the actor's outermost enclosing room is different from
     *   our own outermost enclosing room, we'll use this description.
     *   
     *   If this property is left undefined, then we'll describe this
     *   object when it's distant as though it were in the same room.  So,
     *   we'll select the obscured, distant, or ordinary description,
     *   according to the sense path.  
     */
    // remoteDesc(pov) { "the remote description" }

    /* the default obscured description */
    defaultObscuredDesc(obs) { gLibMessages.obscuredThingDesc(self, obs); }

    /* 
     *   The "sound description," which is the description displayed when
     *   an actor explicitly listens to the object.  This is used when we
     *   have a transparent sense path and no associated "emanation"
     *   object; when we have an associated emanation object, we use its
     *   description instead of this one.  
     */
    soundDesc { defaultDescReport(&thingSoundDescMsg, self); }

    /* distant sound description */
    distantSoundDesc { gLibMessages.distantThingSoundDesc(self); }

    /* obscured sound description */
    obscuredSoundDesc(obs) { gLibMessages.obscuredThingSoundDesc(self, obs); }

    /*
     *   The "smell description," which is the description displayed when
     *   an actor explicitly smells the object.  This is used when we have
     *   a transparent sense path to the object, and we have no
     *   "emanation" object; when we have an associated emanation object,
     *   we use its description instead of this one.  
     */
    smellDesc { defaultDescReport(&thingSmellDescMsg, self); }

    /* distant smell description */
    distantSmellDesc { gLibMessages.distantThingSmellDesc(self); }

    /* obscured smell description */
    obscuredSmellDesc(obs) { gLibMessages.obscuredThingSmellDesc(self, obs); }

    /*
     *   The "taste description," which is the description displayed when
     *   an actor explicitly tastes the object.  Note that, unlike sound
     *   and smell, we don't distinguish levels of transparency or
     *   distance with taste, because tasting an object requires direct
     *   physical contact with it.  
     */
    tasteDesc { gLibMessages.thingTasteDesc(self); }

    /* 
     *   The "feel description," which is the description displayed when
     *   an actor explicitly feels the object.  As with taste, we don't
     *   distinguish transparency or distance. 
     */
    feelDesc { gLibMessages.thingFeelDesc(self); }

    /*
     *   Show the smell/sound description for the object as part of a room
     *   description.  These are displayed when the object is in the room
     *   and it has a presence in the corresponding sense.  By default,
     *   these show nothing.
     *   
     *   In most cases, regular objects don't override these, because most
     *   regular objects have no direct sensory presence of their own.
     *   Instead, a Noise or Odor is created and added to the object's
     *   direct contents, and the Noise or Odor provides the object's
     *   sense presence.  
     */
    smellHereDesc() { }
    soundHereDesc() { }

    /*
     *   "Equivalence" flag.  If this flag is set, then all objects with
     *   the same immediate superclass will be considered interchangeable;
     *   such objects will be listed collectively in messages (so we would
     *   display "five coins" rather than "a coin, a coin, a coin, a coin,
     *   and a coin"), and will be treated as equivalent in resolving noun
     *   phrases to objects in user input.
     *   
     *   By default, this property is nil, since we want most objects to
     *   be treated as unique.  
     */
    isEquivalent = nil

    /*
     *   My Distinguisher list.  This is a list of Distinguisher objects
     *   that can be used to distinguish this object from other objects.
     *   
     *   Distinguishers are listed in order of priority.  The
     *   disambiguation process looks for distinguishers capable of telling
     *   objects apart, starting with the first in the list.  The
     *   BasicDistinguisher is generally first in every object's list,
     *   because any two objects can be told apart if they come from
     *   different classes.
     *   
     *   By default, each object has the "basic" distinguisher, which tells
     *   objects apart on the basis of the "isEquivalent" property and
     *   their superclasses; the ownership distinguisher, which tells
     *   objects apart based on ownership; and the location distinguisher,
     *   which identifies objects by their immediate containers.  
     */
    distinguishers = [basicDistinguisher,
                      ownershipDistinguisher,
                      locationDistinguisher]

    /*
     *   Get the distinguisher to use for printing this object's name in an
     *   action announcement (such as a multi-object, default object, or
     *   vague-match announcement).  We check the global option setting to
     *   see if we should actually use distinguishers for this; if so, we
     *   call getInScopeDistinguisher() to find the correct distinguisher,
     *   otherwise we use the "null" distinguisher, which simply lists
     *   objects by their base names.
     *   
     *   'lst' is the list of other objects from which we're trying to
     *   differentiate 'self'.  The reason 'lst' is given is that it lets
     *   us choose the simplest name for each object that usefully
     *   distinguishes it; to do this, we need to know exactly what we're
     *   distinguishing it from.  
     */
    getAnnouncementDistinguisher(lst)
    {
        return (gameMain.useDistinguishersInAnnouncements
                ? getBestDistinguisher(lst)
                : nullDistinguisher);
    }

    /*
     *   Get a distinguisher that differentiates me from all of the other
     *   objects in scope, if possible, or at least from some of the other
     *   objects in scope.
     */
    getInScopeDistinguisher()
    {
        /* return the best distinguisher for the objects in scope */
        return getBestDistinguisher(gActor.scopeList());
    }

    /*
     *   Get a distinguisher that differentiates me from all of the other
     *   objects in the given list, if possible, or from as many of the
     *   other objects as possible. 
     */
    getBestDistinguisher(lst)
    {
        local bestDist, bestCnt;

        /* remove 'self' from the list */
        lst -= self;

        /* 
         *   Try the "null" distinguisher, which distinguishes objects
         *   simply by their base names - if that can tell us apart from
         *   the other objects, there's no need for anything more
         *   elaborate.  So, calculate the list of indistinguishable
         *   objects using the null distinguisher, and if it's empty, we
         *   can just use the null distinguisher as our result.  
         */
        if (lst.subset({obj: !nullDistinguisher.canDistinguish(self, obj)})
            .length() == 0)
            return nullDistinguisher;

        /* 
         *   Reduce the list to the set of objects that are basic
         *   equivalents of mine - these are the only ones we care about
         *   telling apart from us, since all of the other objects can be
         *   distinguished by their disambig name.  
         */
        lst = lst.subset(
            {obj: !basicDistinguisher.canDistinguish(self, obj)});

        /* if the basic distinguisher can tell us apart, just use it */
        if (lst.length() == 0)
            return basicDistinguisher;

        /* as a last resort, fall back on the basic distinguisher */
        bestDist = basicDistinguisher;
        bestCnt = lst.countWhich({obj: bestDist.canDistinguish(self, obj)});

        /* 
         *   run through my distinguisher list looking for the
         *   distinguisher that can tell me apart from the largest number
         *   of my vocab equivalents 
         */
        foreach (local dist in distinguishers)
        {
            /* if this is the default, skip it */
            if (dist == bestDist)
                continue;

            /* check to see how many objects 'dist' can distinguish me from */
            local cnt = lst.countWhich({obj: dist.canDistinguish(self, obj)});

            /* 
             *   if it can distinguish me from every other object, use this
             *   one - it uniquely identifies me 
             */
            if (cnt == lst.length())
                return dist;

            /* 
             *   that can't distinguish us from everything else here, but
             *   if it's the best so far, remember it; we'll fall back on
             *   the best that we find if we fail to find a perfect
             *   distinguisher 
             */
            if (cnt > bestCnt)
            {
                bestDist = dist;
                bestCnt = cnt;
            }
        }

        /*
         *   We didn't find any distinguishers that can tell me apart from
         *   every other object, so choose the one that can tell me apart
         *   from the most other objects. 
         */
        return bestDist;
    }

    /*
     *   Determine if I'm equivalent, for the purposes of command
     *   vocabulary, to given object.
     *   
     *   We'll run through our list of distinguishers and check with each
     *   one to see if it can tell us apart from the other object.  If we
     *   can find at least one distinguisher that can tell us apart, we're
     *   not equivalent.  If we have no distinguisher that can tell us
     *   apart from the other object, we're equivalent.  
     */
    isVocabEquivalent(obj)
    {
        /* 
         *   Check each distinguisher - if we can find one that can tell
         *   us apart from the other object, we're not equivalent.  If
         *   there are no distinguishers that can tell us apart, we're
         *   equivalent.  
         */
        return (distinguishers.indexWhich(
            {cur: cur.canDistinguish(self, obj)}) == nil);
    }

    /*
     *   My associated "collective group" objects.  A collective group is
     *   an abstract object, not part of the simulation (i.e, not directly
     *   manipulable by characters as a separate object) that can stand in
     *   for an entire group of related objects in some actions.  For
     *   example, we might have a collective "money" object that stands in
     *   for any group of coins and paper bills for "examine" commands, so
     *   that when the player says something like "look at money" or "count
     *   money," we use the single collective money object to handle the
     *   command rather than running the command iteratively on each of the
     *   individual coins and bills present.
     *   
     *   The value is a list because this object can be associated with
     *   more than one collective group.  For example, a diamond could be
     *   in a "treasure" group as well as a "jewelry" group.  
     *   
     *   The associated collective objects are normally of class
     *   CollectiveGroup.  By default, if our 'collectiveGroup' property is
     *   not nil, our list consists of that one item; otherwise we just use
     *   an empty list.  
     */
    collectiveGroups = (collectiveGroup != nil ? [collectiveGroup] : [])

    /*
     *   Our collective group.  Note - this property is obsolescent; it's
     *   supported only for compatibility with old code.  New games should
     *   use 'collectiveGroups' instead.  
     */
    collectiveGroup = nil

    /*
     *   Are we associated with the given collective group?  We do if it's
     *   in our collectiveGroups list.  
     */
    hasCollectiveGroup(g) { return collectiveGroups.indexOf(g) != nil; }

    /*
     *   "List Group" objects.  This specifies a list of ListGroup objects
     *   that we use to list this object in object listings, such as
     *   inventory lists and room contents lists.
     *   
     *   An object can be grouped in more than one way.  When multiple
     *   groups are specified here, the order is significant:
     *   
     *   - To the extent two groups entirely overlap, which is to say that
     *   one of the pair entirely contains the other (for example, if
     *   every coin is a kind of money, then the "money" listing group
     *   would contain every object in the "coin" group, plus other
     *   objects as well: the coin group is a subset of the money group),
     *   the groups must be listed from most general to most specific (for
     *   our money/coin example, then, money would come before coin in the
     *   group list).
     *   
     *   - When two groups do not overlap, then the earlier one in our
     *   list is given higher priority.
     *   
     *   By default, we return an empty list.
     */
    listWith = []

    /* 
     *   Get the list group for my special description.  This works like
     *   the ordinary listWith, but is used to group me with other objects
     *   showing special descriptions, rather than in ordinary listings.
     */
    specialDescListWith = []

    /*
     *   Our interior room name.  This is the status line name we display
     *   when an actor is within this object and can't see out to the
     *   enclosing room.  Since we can't rely on the enclosing room's
     *   status line name if we can't see the enclosing room, we must
     *   provide one of our own.
     *   
     *   By default, we'll use our regular name.
     */
    roomName = (name)

    /*
     *   Show our interior description.  We use this to generate the long
     *   "look" description for the room when an actor is within this
     *   object and cannot see the enclosing room.
     *   
     *   Note that this is used ONLY when the actor cannot see the
     *   enclosing room - when the enclosing room is visible (because the
     *   nested room is something like a chair that doesn't enclose the
     *   actor, or can enclose the actor but is open or transparent), then
     *   we'll simply use the description of the enclosing room instead,
     *   adding a note to the short name shown at the start of the room
     *   description indicating that the actor is in the nested room.
     *   
     *   By default, we'll show the appropriate "actor here" description
     *   for the posture, so we'll say something like "You are sitting on
     *   the red chair" or "You are in the phone booth."  Instances can
     *   override this to customize the description with something more
     *   detailed, if desired.  
     */
    roomDesc { gActor.listActorPosture(getPOVActorDefault(gActor)); }

    /*
     *   Show our first-time room description.  This is the description we
     *   show when the actor is seeing our interior description for the
     *   first time.  By default, we just show the ordinary room
     *   description, but this can be overridden to provide a special
     *   description the first time the actor sees the room. 
     */
    roomFirstDesc { roomDesc; }

    /*
     *   Show our remote viewing description.  This is used when we show a
     *   description of a room, via lookAroundWithin, and the actor who's
     *   viewing the room is remote.  Note that the library never does this
     *   itself, since there's no command in the basic library that lets a
     *   player view a remote room.  However, a game might want to generate
     *   such a description to handle a command like LOOK THROUGH KEYHOLE,
     *   so we provide this extra description method for the game's use.
     *   
     *   By default, we simply show the normal room description.  You'll
     *   probably want to override this any time you actually use it,
     *   though, to describe what the actor sees from the remote point of
     *   view.  
     */
    roomRemoteDesc(actor) { roomDesc; }

    /* our interior room name when we're in the dark */
    roomDarkName = (gLibMessages.roomDarkName)

    /* show our interior description in the dark */
    roomDarkDesc { gLibMessages.roomDarkDesc; }

    /*
     *   Describe an actor in this location either from the point of view
     *   of a separate top-level room, or at a distance.  This is called
     *   when we're showing a room description (for LOOK AROUND), and the
     *   given actor is in a remote room or at a distance visually from
     *   the actor doing the looking, and the given actor is contained
     *   within this room.
     *   
     *   By default, if we have a location, and the actor doing the
     *   looking can see out into the enclosing room, we'll defer to the
     *   location.  Otherwise, we'll show a default library message ("The
     *   actor is nearby").  
     */
    roomActorThereDesc(actor)
    {
        local pov = getPOV();
        
        /* 
         *   if we have an enclosing location, and the POV actor can see
         *   out to our location, defer to our location; otherwise, show
         *   the default library message 
         */
        if (location != nil && pov != nil && pov.canSee(location))
            location.roomActorThereDesc(actor);
        else
            gLibMessages.actorInRemoteRoom(actor, self, pov);
    }
    
    /*
     *   Look around from the point of view of this object and on behalf of
     *   the given actor.  This can be used to generate a description as
     *   seen from this object by the given actor, and is suitable for
     *   cases where the actor can use this object as a sensing device.  We
     *   simply pass this through to lookAroundPov(), passing 'self' as the
     *   point-of-view object.
     *   
     *   'verbose' is a combination of LookXxx flags (defined in adv3.h)
     *   indicating what style of description we want.  This can also be
     *   'true', in which case we'll show the standard verbose listing
     *   (LookRoomName | LookRoomDesc | LookListSpecials |
     *   LookListPortables); or 'nil', in which case we'll use the standard
     *   terse listing (LookRoomName | LookListSpecials |
     *   LookListPortables).  
     */
    lookAround(actor, verbose)
    {
        /* look around from my point of view */
        lookAroundPov(actor, self, verbose);
    }

    /*
     *   Look around from within this object, looking from the given point
     *   of view and on behalf of the given actor.
     *   
     *   'actor' is the actor doing the looking, and 'pov' is the point of
     *   view of the description.  These are usually the same, but need not
     *   be.  For example, an actor could be looking at a room through a
     *   hole in the wall, in which case the POV would be the object
     *   representing the "side" of the hole in the room being described.
     *   Or, the actor could be observing a remote room via a
     *   closed-circuit television system, in which case the POV would be
     *   the camera in the remote room.  (The POV is the physical object
     *   receiving the photons in the location being described, so it's the
     *   camera, not the TV monitor that the actor's looking at.)
     *   
     *   'verbose' has the same meaning as it does in lookAround().  
     *   
     *   This routine checks to see if 'self' is the "look-around ceiling,"
     *   which is for most purposes the outermost visible container of this
     *   object; see isLookAroundCeiling() for more.  If this object is the
     *   look-around ceiling, then we'll call lookAroundWithin() to
     *   generate the description of the interior of 'self'; otherwise,
     *   we'll recursively defer to our immediate container so that it can
     *   make the same test.  In most cases, the outermost visible
     *   container that actually generates the description will be a Room
     *   or a NestedRoom.  
     */
    lookAroundPov(actor, pov, verbose)
    {
        /*
         *   If we're the "look around ceiling," generate the description
         *   within this room.  Otherwise, we have an enclosing location
         *   that provides the interior description. 
         */
        if (isLookAroundCeiling(actor, pov))
        {
            /* we're the "ceiling," so describe our interior */
            fromPOV(actor, pov, &lookAroundWithin, actor, pov, verbose);
        }
        else
        {
            /* our location provides the interior description */
            location.lookAroundPov(actor, pov, verbose);
        }
    }

    /*
     *   Are we the "look around ceiling"?  If so, it means that a LOOK
     *   AROUND for an actor within this location (or from a point of view
     *   within this location) will use our own interior description, via
     *   lookAroundWithin().  If we're not the ceiling, then we defer to
     *   our location, letting it describe its interior.
     *   
     *   By default, we're the ceiling if we're a top-level room (that is,
     *   we have no enclosing location), OR it's not possible to see out to
     *   our location.  In either of these cases, we can't see anything
     *   outside of this room, so we have to generate our own interior
     *   description.  However, if we do have a location that we can see,
     *   then we'll assume that this object just represents a facet of its
     *   enclosing location, so the enclosing location provides the room
     *   interior description.
     *   
     *   In some cases, a room might want to provide its own LOOK AROUND
     *   interior description directly even if its location is visible.
     *   For example, if the player's inside a wooden booth with a small
     *   window that can see out to the enclosing location, LOOK AROUND
     *   should probably describe the interior of the booth rather than the
     *   enclosing location: even though the exterior is technically
     *   visible, the booth clearly dominates the view, from the player's
     *   perspective.  In this case, we'd want to override this routine to
     *   indicate that we're the LOOK AROUND ceiling, despite our
     *   location's visibility.  
     */
    isLookAroundCeiling(actor, pov)
    {
        /* 
         *   if we don't have a location, or we can't see our location,
         *   then we have to provide our own description, so we're the LOOK
         *   AROUND "ceiling" 
         */
        return (location == nil || !actor.canSee(location));
    }

    /*
     *   Provide a "room description" of the interior of this object.  This
     *   routine is primarily intended as a subroutine of lookAround() and
     *   lookAroundPov() - most game code shouldn't need to call this
     *   routine directly.  Note that if you *do* call this routine
     *   directly, you *must* set the global point-of-view variables, which
     *   you can do most easily by calling this routine indirectly through
     *   the fromPOV() method.
     *   
     *   The library calls this method when an actor performs a "look
     *   around" command, and the actor is within this object, and the
     *   actor can't see anything outside of this object; this can happen
     *   simply because we're a top-level room, but it can also happen when
     *   we're a closed opaque container or there's not enough light to see
     *   the enclosing location.
     *   
     *   The parameters have the same meaning as they do in
     *   lookAroundPov().
     *   
     *   Note that this method must be overridden if a room overrides the
     *   standard mechanism for representing its contents list (i.e., it
     *   doesn't store its complete set of direct contents in its
     *   'contents' list property).  
     *   
     *   In most cases, this routine will only be called in Room and
     *   NestedRoom objects, because actors can normally only enter those
     *   types of objects.  However, it is possible to try to describe the
     *   interior of other types of objects if (1) the game allows actors
     *   to enter other types of objects, or (2) the game provides a
     *   non-actor point-of-view object, such as a video camera, that can
     *   be placed in ordinary containers and which transmit what they see
     *   for remote viewing.  
     */
    lookAroundWithin(actor, pov, verbose)
    {
        local illum;
        local infoTab;
        local info;
        local specialList;
        local specialBefore, specialAfter;

        /* check for the special 'true' and 'nil' settings for 'verbose' */
        if (verbose == true)
        {
            /* true -> show the standard verbose description */
            verbose = (LookRoomName | LookRoomDesc
                       | LookListSpecials | LookListPortables);
        }
        else if (verbose == nil)
        {
            /* nil -> show the standard terse description */
            verbose = (LookRoomName | LookListSpecials | LookListPortables);
        }

        /* 
         *   if we've never seen the room before, include the room
         *   description, even if it wasn't specifically requested 
         */
        if (!actor.hasSeen(self))
            verbose |= LookRoomDesc;

        /* 
         *   get a table of all of the objects that the viewer can sense
         *   in the location using sight-like senses 
         */
        infoTab = actor.visibleInfoTableFromPov(pov);

        /* get the ambient illumination at the point of view */
        info = infoTab[pov];
        if (info != nil)
        {
            /* get the ambient illumination from the info list item */
            illum = info.ambient;
        }
        else
        {
            /* the actor's not in the list, so it must be completely dark */
            illum = 0;
        }

        /* 
         *   adjust the list of described items (usually, this removes the
         *   point-of-view object from the list, to ensure that we don't
         *   list it among the room's contents) 
         */
        adjustLookAroundTable(infoTab, pov, actor);
        
        /* if desired, display the room name */
        if ((verbose & LookRoomName) != 0)
        {
            "<.roomname>";
            lookAroundWithinName(actor, illum);
            "<./roomname>";
        }

        /* if we're in verbose mode, display the full description */
        if ((verbose & LookRoomDesc) != 0)
        {
            /* display the full room description */
            "<.roomdesc>";
            lookAroundWithinDesc(actor, illum);
            "<./roomdesc>";
        }

        /* show the initial special descriptions, if desired */
        if ((verbose & LookListSpecials) != 0)
        {
            local plst;

            /* 
             *   Display any special description messages for visible
             *   objects, other than those carried by the actor.  These
             *   messages are part of the verbose description rather than
             *   the portable item listing, because these messages are
             *   meant to look like part of the room's full description
             *   and thus should not be included in a non-verbose listing.
             *   
             *   Note that we only want to show objects that are currently
             *   using special descriptions AND which aren't contained in
             *   the actor doing the looking, so subset the list
             *   accordingly.  
             */
            specialList = specialDescList(
                infoTab,
                {obj: obj.useSpecialDescInRoom(self) && !obj.isIn(actor)});

            /* 
             *   partition the special list into the parts to be shown
             *   before and after the portable contents list 
             */
            plst = partitionList(specialList,
                                 {obj: obj.specialDescBeforeContents});
            specialBefore = plst[1];
            specialAfter = plst[2];

            /* 
             *   at this point, describe only the items that are to be
             *   listed specially before the room's portable contents list 
             */
            specialContentsLister.showList(pov, nil, specialBefore,
                                           0, 0, infoTab, nil);
        }

        /* show the portable items, if desired */
        if ((verbose & LookListPortables) != 0)
        {
            /* 
             *   Describe each visible object directly contained in the
             *   room that wants to be listed - generally, we list any
             *   mobile objects that don't have special descriptions.  We
             *   list items in all room descriptions, verbose or not,
             *   because the portable item list is more of a status display
             *   than it is a part of the full description.
             *   
             *   Note that the infoTab has sense data that will ensure that
             *   we don't show anything we shouldn't if the room is dark.  
             */
            lookAroundWithinContents(actor, illum, infoTab);
        }

        /*
         *   If we're showing special descriptions, show the special descs
         *   for any items that want to be listed after the room's portable
         *   contents.  
         */
        if ((verbose & LookListSpecials) != 0)
        {
            /* show the subset that's listed after the room contents */
            specialContentsLister.showList(pov, nil, specialAfter,
                                           0, 0, infoTab, nil);
        }
        
        /* show all of the sounds we can hear */
        lookAroundWithinSense(actor, pov, sound, roomListenLister);

        /* show all of the odors we can smell */
        lookAroundWithinSense(actor, pov, smell, roomSmellLister);

        /* show the exits, if desired */
        lookAroundWithinShowExits(actor, illum);

        /* 
         *   If we're not in the dark, note that we've been seen.  Don't
         *   count this as having seen the location if we're in the dark,
         *   since we won't normally describe any detail in the dark.  
         */
        if (illum > 1)
            actor.setHasSeen(self);
    }

    /*
     *   Display the "status line" name of the room.  This is normally a
     *   brief, single-line description.
     *   
     *   By long-standing convention, each location in a game usually has a
     *   distinctive name that's displayed here.  Players usually find
     *   these names helpful in forming a mental map of the game.
     *   
     *   By default, if we have an enclosing location, and the actor can
     *   see the enclosing location, we'll defer to the location.
     *   Otherwise, we'll display our roo interior name.  
     */
    statusName(actor)
    {
        /* 
         *   use the enclosing location's status name if there is an
         *   enclosing location and its visible; otherwise, show our
         *   interior room name 
         */
        if (location != nil && actor.canSee(location))
            location.statusName(actor);
        else
            lookAroundWithinName(actor, actor.getVisualAmbient());
    }

    /*
     *   Adjust the sense table a "look around" command.  This is called
     *   after we calculate the sense info table, but before we start
     *   describing any of the room's contents, to give us a chance to
     *   remove any items we don't want described (or, conceivably, to add
     *   any items we do want described but which won't show up with the
     *   normal sense calculations).
     *   
     *   By default, we simply remove the point-of-view object from the
     *   list, to ensure that it's not included among the objects mentioned
     *   as being in the room.  We don't want to mention the point of view
     *   because the POV object because a POV isn't normally in its own
     *   field of view.  
     */
    adjustLookAroundTable(tab, pov, actor)
    {
        /* remove the POV object from the table */
        tab.removeElement(pov);

        /* give the actor a chance to adjust the table as well */
        if (self not in (actor, pov))
        {
            /* let the POV object adjust the table */
            pov.adjustLookAroundTable(tab, pov, actor);

            /* if the actor differs from the POV, let it adjust the table */
            if (actor != pov)
                actor.adjustLookAroundTable(tab, pov, actor);
        }
    }

    /*
     *   Show my name for a "look around" command.  This is used to display
     *   the location name when we're providing the room description.
     *   'illum' is the ambient visual sense level at the point of view.
     *   
     *   By default, we show our interior room name or interior dark room
     *   name, as appropriate to the ambient light level at the point of
     *   view.  
     */
    lookAroundWithinName(actor, illum)
    {
        /* 
         *   if there's light, show our name; otherwise, show our
         *   in-the-dark name 
         */
        if (illum > 1)
        {
            /* we can see, so show our interior description */
            "\^<<roomName>>";
        }
        else
        {
            /* we're in the dark, so show our default dark name */
            say(roomDarkName);
        }

        /* show the actor's posture as part of the description */
        actor.actorRoomNameStatus(self);
    }

    /*
     *   Show our "look around" long description.  This is used to display
     *   the location's full description when we're providing the room
     *   description - that is, when the actor doing the looking is inside
     *   me.  This displays only the room-specific portion of the room's
     *   description; it does not display the status line or anything
     *   about the room's dynamic contents.  
     */
    lookAroundWithinDesc(actor, illum)
    {
        /* 
         *   check for illumination - we must have at least dim ambient
         *   lighting (level 2) to see the room's long description 
         */
        if (illum > 1)
        {
            local pov = getPOVDefault(actor);
            
            /* 
             *   Display the normal description of the room - use the
             *   roomRemoteDesc if the actor isn't in the same room OR the
             *   point of view isn't the same as the actor; use the
             *   firstDesc if this is the first time in the room; and
             *   otherwise the basic roomDesc.  
             */
            if (!actor.isIn(self) || actor != pov)
            {
                /* we're viewing the room remotely */
                roomRemoteDesc(actor);
            }
            else if (actor.hasSeen(self))
            {
                /* we've seen it already - show the basic room description */
                roomDesc;
            }
            else
            {
                /* 
                 *   we've never seen this location before - show the
                 *   first-time description 
                 */
                roomFirstDesc;
            }
        }
        else
        {
            /* display the in-the-dark description of the room */
            roomDarkDesc;
        }
    }

    /*
     *   Show my room contents for a "look around" description within this
     *   object. 
     */
    lookAroundWithinContents(actor, illum, infoTab)
    {
        local lst;
        local lister;
        local remoteLst;
        local recurse;

        /* get our outermost enclosing room */
        local outer = getOutermostRoom();

        /* mark everything visible from the room as having been seen */
        setAllSeenBy(infoTab, actor);
        
        /* 
         *   if the illumination is less than 'dim' (level 2), display
         *   only self-illuminating items 
         */
        if (illum != nil && illum < 2)
        {
            /* 
             *   We're in the dark - list only those objects that the actor
             *   can sense via the sight-like senses, which will be the
             *   list of self-illuminating objects.  Don't include items
             *   being carried by the actor, though, since those don't
             *   count as part of the actor's surroundings.  (To produce
             *   this list, simply make a list consisting of all of the
             *   objects in the sense info table that aren't contained in
             *   the actor; since the table naturally includes only objects
             *   that are illuminated, the entire contents of the table
             *   will give us the visible objects.)  
             */
            lst = senseInfoTableSubset(
                infoTab, {obj, info: !obj.isIn(actor)});
            
            /* use my dark contents lister */
            lister = darkRoomContentsLister;

            /* there are no remote-room lists to generate */
            remoteLst = nil;

            /* 
             *   Since we can't see what we're looking at, we can't see the
             *   positional relationships among the things we're looking
             *   at, so we can't show the contents tree recursively.  We
             *   just want to show our flat list of what's visible on
             *   account of self-illumination.  
             */
            recurse = nil;
        }
        else
        {
            /* start with my contents list */
            lst = contents;

            /* always remove the actor from the list */
            lst -= actor;

            /* 
             *   Use the normal (lighted) lister.  Note that if the actor
             *   isn't in the same room, or the point of view isn't the
             *   same as the actor, we want to generate remote
             *   descriptions, so use the remote contents lister in these
             *   cases.  
             */
            lister = (actor.isIn(self) && actor == getPOVDefault(actor)
                      ? roomContentsLister
                      : remoteRoomContentsLister(self));

            /*
             *   Generate a list of all of the *other* top-level rooms
             *   with contents visible from here.  To do this, build a
             *   list of all of the unique top-level rooms, other than our
             *   own top-level room, that contain items in the visible
             *   information table.  
             */
            remoteLst = new Vector(5);
            infoTab.forEachAssoc(function(obj, info)
            {
                /* if this object isn't in our top-level room, note it */
                if (obj != outer && !obj.isIn(outer))
                {
                    local objOuter;
                    
                    /* 
                     *   it's not in our top-level room, so find its
                     *   top-level room 
                     */
                    objOuter = obj.getOutermostRoom();

                    /* if this one isn't in our list yet, add it */
                    if (remoteLst.indexOf(objOuter) == nil)
                        remoteLst.append(objOuter);
                }
            });

            /* 
             *   since we can see what we're looking at, show the
             *   relationships among the contents by listing recursively 
             */
            recurse = true;
        }

        /* add a paragraph before the room's contents */
        "<.p>";

        /* show the contents */
        lister.showList(actor, self, lst, (recurse ? ListRecurse : 0),
                        0, infoTab, nil, examinee: self);

        /* 
         *   if we can see anything in remote top-level rooms, show the
         *   contents of each other room with visible contents 
         */
        if (remoteLst != nil)
        {
            /* show each remote room's contents */
            for (local i = 1, local len = remoteLst.length() ; i <= len ; ++i)
            {
                local cur;
                local cont;

                /* get this item */
                cur = remoteLst[i];

                /* start with the direct contents of this remote room */
                cont = cur.contents;

                /* 
                 *   Remove any objects that are also contents of the
                 *   local room or of any other room we've already listed.
                 *   It's possible that we have a MultiLoc that's in one
                 *   or more of the visible top-level locations, so we
                 *   need to make sure we only list each one once. 
                 */
                cont = cont.subset({x: !x.isIn(outer)});
                for (local j = 1 ; j < i ; ++j)
                    cont = cont.subset({x: !x.isIn(remoteLst[j])});
                
                /* 
                 *   list this remote room's contents using our remote
                 *   lister for this remote room 
                 */
                outer.remoteRoomContentsLister(cur).showList(
                    actor, cur, cont, ListRecurse, 0, infoTab, nil,
                    examinee: self);
            }
        }
    }

    /*
     *   Add to the room description a list of things we notice through a
     *   specific sense.  This is used to add things we can hear and smell
     *   to a room description.
     */
    lookAroundWithinSense(actor, pov, sense, lister)
    {
        local infoTab;
        local presenceList;

        /* 
         *   get the information table for the desired sense from the
         *   given point of view 
         */
        infoTab = pov.senseInfoTable(sense);

        /* 
         *   Get the list of everything with a presence in this sense.
         *   Include only items that aren't part of the actor's inventory,
         *   since inventory items aren't part of the room and thus
         *   shouldn't be described as part of the room.  
         */
        presenceList = senseInfoTableSubset(infoTab,
            {obj, info: obj.(sense.presenceProp) && !obj.isIn(actor)});

        /* mark each item in the presence list as known */
        foreach (local cur in presenceList)
            actor.setKnowsAbout(cur);

        /* list the items */
        lister.showList(pov, nil, presenceList, 0, 0, infoTab, nil,
                        examinee: self);
    }

    /*
     *   Show the exits from this room as part of a description of the
     *   room, if applicable.  By default, if we have an exit lister
     *   object, we'll invoke it to show the exits.  
     */
    lookAroundWithinShowExits(actor, illum)
    {
        /* if we have an exit lister, have it show a list of exits */
        if (gExitLister != nil)
            gExitLister.lookAroundShowExits(actor, self, illum);
    }

    /* 
     *   Get my lighted room contents lister - this is the Lister object
     *   that we use to display the room's contents when the room is lit.
     *   We'll return the default library room lister.  
     */
    roomContentsLister { return roomLister; }

    /*
     *   Get my lister for the contents of the given remote room.  A
     *   remote room is a separate top-level room that an actor can see
     *   from its current location; for example, if two top-level rooms
     *   are connected by a window, so that an actor standing in one room
     *   can see into the other room, then the other room is the remote
     *   room, from the actor's perspective.
     *   
     *   This is called on the actor's outermost room to get the lister
     *   for objects visible in the given remote room.  This lets each
     *   room customize the way it describes the objects in adjoining
     *   rooms as seen from its point of view.
     *   
     *   We provide a simple default lister that yields decent results in
     *   most cases.  However, it's often desirable to customize this, to
     *   be more specific about how we see the items: "Through the window,
     *   you see..." or "Further down the hall, you see...".  An easy way
     *   to provide such custom listings is to return a new
     *   CustomRoomLister, supplying the prefix and suffix strings as
     *   parameters:
     *   
     *.    return new CustomRoomLister(
     *.       'Further down the hall, {you/he} see{s}', '.');
     */
    remoteRoomContentsLister(other) { return new RemoteRoomLister(other); }

    /* 
     *   Get my dark room contents lister - this is the Lister object we'll
     *   use to display the room's self-illuminating contents when the room
     *   is dark.  
     */
    darkRoomContentsLister { return darkRoomLister; }

    /*
     *   Get the outermost containing room.  We return our container, if
     *   we have one, or self if not.  
     */
    getOutermostRoom()
    {
        /* return our container's outermost room, if we have one */
        return (location != nil ? location.getOutermostRoom() : self);
    }

    /* get the outermost room that's visible from the given point of view */
    getOutermostVisibleRoom(pov)
    {
        local enc;
        
        /* 
         *   Get our outermost enclosing visible room - this is the
         *   outermost enclosing room that the POV can see.  If we manage
         *   to find an enclosing visible room, return that, since it's
         *   "more outer" than we are.  
         */
        if (location != nil
            && (enc = location.getOutermostVisibleRoom(pov)) != nil)
            return enc;

        /* 
         *   We either don't have any enclosing rooms, or none of them are
         *   visible.  So, our outermost enclosing room is one of two
         *   things: if we're visible, it's us; if we're not even visible
         *   ourselves, there's no visible room at all, so return nil.  
         */
        return (pov.canSee(self) ? self : nil);
    }

    /*
     *   Get the location traveler - this is the object that's actually
     *   going to change location when a traveler within this location
     *   performs a travel command to travel via the given connector.  By
     *   default, we'll indicate what our containing room indicates.  (The
     *   enclosing room might be a vehicle or an ordinary room; in any
     *   case, it'll know what to do, so we merely have to ask it.)
     *   
     *   We defer to our enclosing room by default because this allows for
     *   things like a seat in a car: the actor is sitting in the seat and
     *   starts traveling in the car, so the seat calls the enclosing room,
     *   which is the car, and the car returns itself, since it's the car
     *   that will be traveling.  
     */
    getLocTraveler(trav, conn)
    {
        /* 
         *   if we have a location, defer to it; otherwise, just return the
         *   proposed traveler 
         */
        return (location != nil ? location.getLocTraveler(trav, conn) : trav);
    }

    /*
     *   Get the "location push traveler" - this is the object that's going
     *   to travel for a push-travel action performed by a traveler within
     *   this location.  This is called by a traveler within this location
     *   to find out if the location wants to be involved in the travel, as
     *   a vehicle might be.  By default, we let our location handle it, if
     *   we have one.  
     */
    getLocPushTraveler(trav, obj)
    {
        /* 
         *   defer to our location if we have one, otherwise just use the
         *   proposed traveler 
         */
        return (location != nil
                ? location.getLocPushTraveler(trav, obj)
                : trav);
    }

    /*
     *   Get the travel preconditions for an actor in this location.  For
     *   ordinary objects, we'll just defer to our container, if we have
     *   one.  
     */
    roomTravelPreCond()
    {
        /* if we have a location, defer to it */
        if (location != nil)
            return location.roomTravelPreCond();

        /* 
         *   we have no location, and we impose no conditions of our by
         *   default, so simply return an empty list 
         */
        return [];
    }

    /*
     *   Determine if the current gActor, who is directly in this location,
     *   is "travel ready."  This means that the actor is ready, as far as
     *   this location is concerned, to traverse the given connector.  By
     *   default, we'll defer to our location.
     *   
     *   Note that if you override this to return nil, you should also
     *   provide a custom 'notTravelReadyMsg' property that explains the
     *   objection.  
     */
    isActorTravelReady(conn)
    {
        /* if we have a location, defer to it */
        if (location != nil)
            return location.isActorTravelReady(conn);

        /* 
         *   we have no location, and we impose no conditions of our own by
         *   default, so we have no objection
         */
        return true;
    }

    /* explain the reason isActorTravelReady() returned nil */
    notTravelReadyMsg = (location != nil ? location.notTravelReadyMsg : nil)

    /* show a list of exits as part of failed travel - defer to location */
    cannotGoShowExits(actor)
    {
        if (location != nil)
            location.cannotGoShowExits(actor);
    }

    /* show exits in the statusline - defer to our location */
    showStatuslineExits()
    {
        if (location != nil)
            location.showStatuslineExits();
    }

    /* get the status line exit lister height - defer to our location */
    getStatuslineExitsHeight()
        { return location != nil ? location.getStatuslineExitsHeight() : 0; }

    /*
     *   Get the travel connector from this location in the given
     *   direction.
     *   
     *   Map all of the directional connections to their values in the
     *   enclosing location, except for any explicitly defined in this
     *   object.  (In most cases, ordinary objects won't define any
     *   directional connectors directly, since those connections usually
     *   apply only to top-level rooms.)
     *   
     *   If 'actor' is non-nil, we'll limit our search to enclosing
     *   locations that the actor can currently see.  If 'actor' is nil,
     *   we'll consider any connector in any enclosing location, whether
     *   the actor can see the enclosing location or not.  It's useful to
     *   pass in a nil actor in cases where we're interested in the
     *   structure of the map and not actual travel, such as when
     *   constructing an automatic map or computing possible courses
     *   between locations.  
     */
    getTravelConnector(dir, actor)
    {
        local conn;
        
        /* 
         *   if we explicitly define the link property, and the result is a
         *   non-nil value, use that definition 
         */
        if (propDefined(dir.dirProp) && (conn = self.(dir.dirProp)) != nil)
            return conn;

        /* 
         *   We don't define the direction link explicitly.  If the actor
         *   can see our container, or there's no actor involved, retrieve
         *   the link from our container; otherwise, stop here, and simply
         *   return the default link for the direction.  
         */
        if (location != nil && (actor == nil || actor.canSee(location)))
        {
            /* 
             *   the actor can see out to our location (or there's no actor
             *   involved at all), so get the connector for the location 
             */
            return location.getTravelConnector(dir, actor);
        }
        else
        {
            /* 
             *   the actor can't see our location, so there's no way to
             *   travel this way - return the default connector for the
             *   direction 
             */
            return dir.defaultConnector(self);
        }
    }

    /*
     *   Search our direction list for a connector that will lead the
     *   given actor to the given destination.  
     */
    getConnectorTo(actor, dest)
    {
        local conn;
        
        /* search all of our directions */
        foreach (local dir in Direction.allDirections)
        {
            /* get the connector for this direction */
            conn = getTravelConnector(dir, actor);

            /* ask the connector for a connector to the given destination */
            if (conn != nil)
                conn = conn.connectorGetConnectorTo(self, actor, dest);

            /* if we found a valid connector, return it */
            if (conn != nil)
                return conn;
        }

        /* didn't find a match */
        return nil;
    }

    /*
     *   Find a direction linked to a given connector, for travel by the
     *   given actor.  Returns the first direction (as a Direction object)
     *   we find linked to the connector, or nil if we don't find any
     *   direction linked to it.  
     */
    directionForConnector(conn, actor)
    {
        /* search all known directions */
        foreach (local dir in Direction.allDirections)
        {
            /* if this direction is linked to the connector, return it */
            if (self.getTravelConnector(dir, actor) == conn)
                return dir;
        }

        /* we didn't find any directions linked to the connector */
        return nil;
    }

    /*
     *   Find the "local" direction link from this object to the given
     *   travel connector.  We'll only consider our own local direction
     *   links; we won't consider enclosing objects.  
     */
    localDirectionLinkForConnector(conn)
    {
        /* scan all direction links */
        foreach (local dir in Direction.allDirections)
        {
            /* 
             *   if we define this direction property to point to this
             *   connector, we've found our required location 
             */
            if (self.(dir.dirProp) == conn)
                return dir;
        }

        /* we didn't find a direction linked to the given connector */
        return nil;
    }

    /*
     *   Check that a traveler is directly in this room.  By default, a
     *   Thing is not a room, so a connector within a Thing actually
     *   requires the traveler to be in the Thing's container, not in the
     *   Thing itself.  So, defer to our container.  
     */
    checkTravelerDirectlyInRoom(traveler, allowImplicit)
    {
        /* defer to our location */
        return location.checkTravelerDirectlyInRoom(traveler, allowImplicit);
    }

    /*
     *   Check, using pre-condition rules, that the actor is removed from
     *   this nested location and moved to its exit destination.
     *   
     *   This is called when the actor is attempting to move to an object
     *   outside of this object while the actor is within this object (for
     *   example, if we have a platform containing a box containing a
     *   chair, 'self' is the box, and the actor is in the chair, an
     *   attempt to move to the platform will trigger a call to
     *   box.checkActorOutOfNested).
     *   
     *   By default, we're not a nested location, but we could *contain*
     *   nested locations.  So what we need to do is defer to the child
     *   object that actually contains the actor, to let it figure out what
     *   it means to move the actor out of itself.  
     */
    checkActorOutOfNested(allowImplicit)
    {
        /* find the child containing the actor, and ask it to do the work */
        foreach (local cur in contents)
        {
            if (gActor.isIn(cur))
                return cur.checkActorOutOfNested(allowImplicit);
        }

        /* didn't find a child containing the actor; just fail */
        reportFailure(&cannotDoFromHereMsg);
        exit;
    }

    /*
     *   Get my "room location."  If 'self' is capable of holding actors,
     *   this should return 'self'; otherwise, it should return the
     *   nearest enclosing container that's a room location.  By default,
     *   we simply return our location's room location.  
     */
    roomLocation = (location != nil ? location.roomLocation : nil)

    /*
     *   Get the apparent location of one of our room parts (the floor,
     *   the ceiling, etc).  By default, we'll simply ask our container
     *   about it, since a nested room by default doesn't have any of the
     *   standard room parts.  
     */
    getRoomPartLocation(part)
    {
        if (location != nil)
            return location.getRoomPartLocation(part);
        else
            return nil;
    }

    /*
     *   By default, an object containing the player character will forward
     *   the roomDaemon() call up to the container. 
     */
    roomDaemon()
    {
        if (location != nil)
            location.roomDaemon();
    }

    /*
     *   Our room atmospheric message list.  This can be set to an
     *   EventList that provides messages to be displayed while the player
     *   character is within this object.
     *   
     *   By default, if our container is visible to us, we'll use our
     *   container's atmospheric messages.  This can be overridden to
     *   provide our own atmosphere list when the player character is in
     *   this object.  
     */
    atmosphereList()
    {
        if (location != nil && gPlayerChar.canSee(location))
            return location.atmosphereList;
        else
            return nil;
    }

    /* 
     *   Get the notification list actions performed by actors within this
     *   object.  Ordinary objects don't have room notification lists, but
     *   we might be part of a nested set of objects that includes rooms,
     *   so simply look to our container for the notification list.  
     */
    getRoomNotifyList()
    {
        local lst = [];
        
        /* get the notification lists for our immediate containers */
        forEachContainer(
            {cont: lst = lst.appendUnique(cont.getRoomNotifyList())});

        /* return the result */
        return lst;
    }

    /*
     *   Are we aboard a ship?  By default, we'll return our location's
     *   setting for this property; if we have no location, we'll return
     *   nil.  In general, this should be overridden in shipboard rooms to
     *   return true.
     *   
     *   The purpose of this property is to indicate to the travel system
     *   that shipboard directions (fore, aft, port, starboard) make sense
     *   here.  When this returns true, and an actor attempts travel in a
     *   shipboard direction that doesn't allow travel here, we'll use the
     *   standard noTravel message.  When this returns nil, attempting to
     *   move in a shipboard direction will show a different response that
     *   indicates that such directions are not meaningful when not aboard
     *   a ship of some kind.
     *   
     *   Note that we look to our location unconditionally - we don't
     *   bother to check to see if we're open, transparent, or anything
     *   like that, so we'll check with our location even if the actor
     *   can't see the location.  The idea is that, no matter how nested
     *   within opaque containers we are, we should still know that we're
     *   aboard a ship.  This might not always be appropriate; if the
     *   actor is magically transported into an opaque container that
     *   happens to be aboard a ship somewhere, the actor probably
     *   shouldn't think of the location as shipboard until escaping the
     *   container, unless they'd know because of the rocking of the ship
     *   or some other sensory factor that would come through the opaque
     *   container.  When the shipboard nature of an interior location
     *   should not be known to the actor, this should simply be
     *   overridden to return nil.  
     */
    isShipboard()
    {
        /* if we have a location, use its setting; otherwise return nil */
        return (location != nil ? location.isShipboard() : nil);
    }

    /* 
     *   When an actor takes me, the actor will assign me a holding index
     *   value, which is simply a serial number indicating the order in
     *   which I was picked up.  This lets the actor determine which items
     *   have been held longest: the item with the lowest holding index
     *   has been held the longest.  
     */
    holdingIndex = 0

    /*
     *   An object has "weight" and "bulk" specifying how heavy and how
     *   large the object is.  These are in arbitrary units, and by
     *   default everything has a weight of 1 and a bulk of 1.
     */
    weight = 1
    bulk = 1

    /*
     *   Calculate the bulk contained within this object.  By default,
     *   we'll simply add up the bulks of all of our contents.
     */
    getBulkWithin()
    {
        local total;
        
        /* add up the bulks of our contents */
        total = 0;
        foreach (local cur in contents)
            total += cur.getBulk();
        
        /* return the total */
        return total;
    }

    /* 
     *   get my "destination name," for travel purposes; by default, we
     *   simply defer to our location, if we have one 
     */
    getDestName(actor, origin)
    {
        if (location != nil)
            return location.getDestName(actor, origin);
        else
            return '';
    }
    
    /*
     *   Get the effective location of an actor directly within me, for the
     *   purposes of a "follow" command.  To follow someone, we must have
     *   the same effective follow location that the target had when we
     *   last observed the target leaving.
     *   
     *   For most objects, we simply defer to the location.  
     */
    effectiveFollowLocation = (location != nil
                               ? location.effectiveFollowLocation
                               : self)

    /*
     *   Get the destination for an object dropped within me.  Ordinary
     *   objects can't contain actors, so an actor can't directly drop
     *   something within a regular Thing, but the same effect could occur
     *   if an actor throws a projectile, since the projectile might reach
     *   either the target or an intermediate obstruction, then bounce off
     *   and land in whatever contains the object hit.
     *   
     *   By default, objects dropped within us won't stay within us, but
     *   will land in our container's drop destination.
     *   
     *   'obj' is the object being dropped.  In some cases, the drop
     *   destination might differ according to the object; for example, if
     *   the floor is a metal grating, a large object might land on the
     *   grating when dropped, but a small object such as a coin might drop
     *   through the grating to the room below.  Note that 'obj' can be
     *   nil, if the caller simply wants to determine the generic
     *   destination where a *typical* object would land if dropped - this
     *   routine must therefore not assume that 'obj' is non-nil.
     *   
     *   'path' is the sense path (as returned by selectPathTo and the
     *   like) traversed by the operation that's seeking the drop
     *   destination.  For ordinary single-location objects, the path is
     *   irrelevant, but this information is sometimes useful for
     *   multi-location objects to let them know which "side" we approached
     *   them from.  Note that the path can be nil, so this routine must
     *   not depend upon a path being supplied; if the path is nil, the
     *   routine should assume that the ordinary "touch" sense path from
     *   the current actor to 'self' is to be used, because the actor is
     *   effectively reaching out and placing an object on self.  This
     *   means that when calling this routine, you can supply a nil path
     *   any time the actor is simply dropping an object.  
     */
    getDropDestination(obj, path)
    {
        /* 
         *   by default, the object lands in our container's drop
         *   destination; if we don't have a container, drop it here 
         */
        return location != nil
            ? location.getDropDestination(obj, path)
            : self;
    }

    /*
     *   Adjust a drop destination chosen in a THROW.  This is called on
     *   the object that was chosen as the preliminary landing location for
     *   the thrown object, as calculated by getHitFallDestination(), to
     *   give the preliminary destination a chance to redirect the landing
     *   to another object.  For example, if the preliminary destination
     *   simply isn't a suitable surface or container where a projectile
     *   could land, or it's not big enough to hold the projectile, the
     *   preliminary destination could override this method to redirect the
     *   landing to a more suitable object.
     *   
     *   By default, we don't need to make any adjustment, so we simply
     *   return 'self' to indicate that this can be used as the actual
     *   destination.  
     */
    adjustThrowDestination(thrownObj, path)
    {
        return self;
    }

    /*
     *   Receive a dropped object.  This is called when we are the actual
     *   (not nominal) drop destination for an object being dropped,
     *   thrown, or otherwise discarded.  This routine is responsible for
     *   carrying out the drop operation, and reporting the result of the
     *   command.
     *   
     *   'desc' is a "drop descriptor": an object of class DropType, which
     *   describes how the object is being discarded.  This can be a
     *   DropTypeDrop for a DROP command, a DropTypeThrow for a THROW
     *   command, or custom types defined by the game or by library
     *   extensions.
     *   
     *   In most cases, the actual *result* of dropping the object will
     *   always be the same for a given location, regardless of which
     *   command initiated the drop, so this routine won't usually have to
     *   look at 'desc' at all to figure out what happens.
     *   
     *   However, the *message* we generate does usually vary according to
     *   the drop type, because this is the report for the entire command.
     *   There are three main ways of handling this variation.
     *   
     *   - First, you can check what kind of descriptor it is (using
     *   ofKind, for example), and generate a custom message for each kind
     *   of descriptor.  Be aware that any library extensions you're using
     *   might have added new DropType subclasses.
     *   
     *   - Second, experienced programmers might prefer the arguably
     *   cleaner OO "visitor" pattern: treat 'desc' as a visitor, calling a
     *   single method that you define for your custom message.  You'll
     *   have to define that custom method in each DropType subclass, of
     *   course.
     *   
     *   - Third, you can build a custom message by combining the standard
     *   "message fragment" prefix provided by the drop descriptor with
     *   your own suffix.  The prefix will always be the start of a
     *   sentence describing what the player did: "You drop the box", "The
     *   ball hits the wall and bounces off", and so on.  Since the
     *   fragment always has the form of the start of a sentence, you can
     *   always add your own suffix: ", and falls to the floor", for
     *   example, or ", and falls into the chasm".  Note that if you're
     *   using one of the other approaches above, you'll probably want to
     *   combine that approach with this one to handle cases where you
     *   don't recognize the drop descriptor type.
     *   
     *   By default, we simply move the object into self and display the
     *   standard message using the descriptor (we use the "visitor"
     *   pattern to display that standard message).  This can be overridden
     *   in cases where it's necessary to move the object to a different
     *   location, or to invoke a side effect.  
     */
    receiveDrop(obj, desc)
    {
        /* simply move the object into self */
        obj.moveInto(self);

        /* generate the standard message */
        desc.standardReport(obj, self);
    }

    /*
     *   Get the nominal destination for an object dropped into this
     *   object.  This is used to report where an object ends up when the
     *   object is moved into me by some indirect route, such as throwing
     *   the object.
     *   
     *   By default, most objects simply return themselves, so we'll
     *   report something like "obj lands {in/on} self".  Some objects
     *   might want to report a different object as the destination; for
     *   example, an indoor room might want to report objects as falling
     *   onto the floor.  
     */
    getNominalDropDestination() { return self; }

    /*
     *   Announce myself as a default object for an action.  By default,
     *   we'll show the standard library message announcing a default.  
     */
    announceDefaultObject(whichObj, action, resolvedAllObjects)
    {
        /* use the standard library message for the announcement */
        return gLibMessages.announceDefaultObject(
            self, whichObj, action, resolvedAllObjects);
    }
    
    /*
     *   Calculate my total weight.  Weight is generally inclusive of the
     *   weights of contents or components, because anything inside an
     *   object normally contributes to the object's total weight. 
     */
    getWeight()
    {
        local total;

        /* start with my own intrinsic weight */
        total = weight;
                
        /* 
         *   add the weights of my direct contents, recursively
         *   calculating their total weights 
         */
        foreach (local cur in contents)
            total += cur.getWeight();

        /* return the total */
        return total;
    }

    /*
     *   Calculate my total bulk.  Most objects have a fixed external
     *   shape (and thus bulk) that doesn't vary as contents are added or
     *   removed, so our default implementation simply returns our
     *   intrinsic bulk without considering any contents.
     *   
     *   Some objects do change shape as contents are added.  Such objects
     *   can override this routine to provide the appropriate behavior.
     *   (We don't try to provide a parameterized total bulk calculator
     *   here because there are too many possible ways a container's bulk
     *   could be affected by its contents or by other factors.)
     *   
     *   If an object's bulk depends on its contents, the object should
     *   override notifyInsert() so that it calls checkBulkChange() - this
     *   will ensure that an object won't suddenly become too large for
     *   its container (or holding actor).  
     */
    getBulk()
    {
        /* by default, simply return my intrinsic bulk */
        return bulk;
    }

    /*
     *   Calculate the amount of bulk that this object contributes towards
     *   encumbering an actor directly holding the item.  By default, this
     *   simply returns my total bulk.
     *   
     *   Some types of objects will override this to cause the object to
     *   contribute more or less bulk to an actor holding the object.  For
     *   example, an article of clothing being worn by an actor typically
     *   contributes no bulk at all to the actor's encumbrance, because an
     *   actor wearing an item typically doesn't also have to hold on to
     *   the item.  Or, a small animal might encumber an actor more than
     *   its normal bulk because it's squirming around trying to get free.
     */
    getEncumberingBulk(actor)
    {
        /* by default, return our normal total bulk */
        return getBulk();
    }

    /*
     *   Calculate the amount of weight that this object contributes
     *   towards encumbering an actor directly holding the item.  By
     *   default, this simply returns my total weight.
     *   
     *   Note that we don't recursively sum the encumbering weights of our
     *   contents - we simply sum the actual weights.  We do it this way
     *   because items within a container aren't being directly held by
     *   the actor, so any difference between the encumbering and actual
     *   weights should not apply, even though the actor is indirectly
     *   holding the items.  If a subclass does want the sum of the
     *   encumbering weights, it should override this to make that
     *   calculation.  
     */
    getEncumberingWeight(actor)
    {
        /* by default, return our normal total weight */
        return getWeight();
    }

    /*
     *   "What if" test.  Make the given changes temporarily, bypassing
     *   any side effects that would normally be associated with the
     *   changes; invokes the given callback; then remove the changes.
     *   Returns the result of calling the callback function.
     *   
     *   The changes are expressed as pairs of argument values.  The first
     *   value in a pair is a property, and the second is a new value for
     *   the property.  For each pair, we'll set the given property to the
     *   given value.  The setting is direct - we don't invoke any method,
     *   because we don't want to cause any side effects at this point;
     *   we're interested only in what the world would look like if the
     *   given changes were to go into effect.
     *   
     *   A special property value of 'moveInto' can be used to indicate
     *   that the object should be moved into another object for the test.
     *   In this case, the second element of the pair is not a value to be
     *   stored into the moveInto property, but instead the value is a new
     *   location for the object.  We'll call the baseMoveInto method to
     *   move the object to the given new location.
     *   
     *   In any case, after making the changes, we'll invoke the given
     *   callback function, which we'll call with no arguments.
     *   
     *   Finally, on our way out, we'll restore the properties we changed
     *   to their original values.  We once again do this without any side
     *   effects.  Note that we restore the old values even if we exit
     *   with an exception.  
     */
    whatIf(func, [changes])
    {
        local oldList;
        local cnt;
        local i;
        
        /* 
         *   Allocate a vector with one slot per change, so that we have a
         *   place to store the old values of the properties we're
         *   changing.  Note that the argument list has two entries (prop,
         *   value) for each change. 
         */
        cnt = changes.length();
        oldList = new Vector(cnt / 2);

        /* run through the change list and make each change */
        for (i = 1 ; i <= cnt ; i += 2)
        {
            local curProp;
            local newVal;
            
            /* get the current property and new value */
            curProp = changes[i];
            newVal = changes[i+1];
            
            /* see what we have */
            switch(curProp)
            {
            case &moveInto:
                /*
                 *   It's the special moveInto property.  This indicates
                 *   that we are to move self into the given new 
                 *   location.  First save the current location, then 
                 *   use baseMoveInto to make the change without 
                 *   triggering any side effects.  Note that if this 
                 *   would create circular containment, we won't make 
                 *   the move.
                 */
                oldList.append(saveLocation());
                if (newVal != self && !newVal.isIn(self))
                    baseMoveInto(newVal);
                break;

            default:
                /* 
                 *   Anything else is a property to which we want to
                 *   assign the given new value.  Remember the old value
                 *   in our original values vector, then set the new value
                 *   from the argument.  
                 */
                oldList.append(self.(curProp));
                self.(curProp) = newVal;
                break;
            }
        }

        /* 
         *   the changes are now in effect - invoke the given callback
         *   function to check things out under the new conditions, but
         *   protect the code so that we are sure to restore things on the
         *   way out 
         */
        try
        {
            /* invoke the given callback, returning the result */
            return (func)();
        }
        finally
        {
            /* run through the change list and undo each change */
            for (i = 1, local j = 1 ; i <= cnt ; i += 2, ++j)
            {
                local curProp;
                local oldVal;
            
                /* get the current property and old value */
                curProp = changes[i];
                oldVal = oldList[j];
            
                /* see what we have */
                switch(curProp)
                {
                case &moveInto:
                    /* restore the location that we saved */
                    restoreLocation(oldVal);
                    break;

                default:
                    /* restore the property value */
                    self.(curProp) = oldVal;
                    break;
                }
            }
        }
    }

    /*
     *   Run a what-if test to see what would happen if this object were
     *   being held directly by the given actor.
     */
    whatIfHeldBy(func, actor)
    {
        /* 
         *   by default, simply run the what-if test with this object
         *   moved into the actor's inventory
         */
        return whatIf(func, &moveInto, actor);
    }

    /*
     *   Check a proposed change in my bulk.  When this is called, the new
     *   bulk should already be in effect (the best way to do this when
     *   just making a check is via whatIf).
     *   
     *   This routine can be called during the 'check' or 'action' stages
     *   of processing a command to determine if a change in my bulk would
     *   cause a problem.  If so, we'll add a failure report and exit the
     *   command.
     *   
     *   By default, notify our immediate container of the change to see
     *   if there's any objection.  A change in an object's bulk typically
     *   only aaffects its container or containers.
     *   
     *   The usual way to invoke this routine in a 'check' or 'action'
     *   routine is with something like this:
     *   
     *      whatIf({: checkBulkChange()}, &inflated, true);
     *   
     *   This checks to see if the change in my bulk implied by changing
     *   self.inflated to true would present a problem for my container,
     *   terminating with a reportFailure+exit if so.  
     */
    checkBulkChange()
    {
        /*
         *   Notify each of our containers of a change in our bulk; if
         *   any container has a problem with our new bulk, it can
         *   report a failure and exit.
         */
        forEachContainer(
            {loc: loc.checkBulkChangeWithin(self)});
    }

    /*
     *   Check a bulk change of one of my direct contents, given by obj.
     *   When this is called, 'obj' will be (tentatively) set to reflect
     *   its proposed new bulk; if this routine doesn't like the new bulk,
     *   it should issue a failure report and exit the command, which will
     *   cancel the command that would have caused the change and will
     *   prevent the proposed change from taking effect.
     *   
     *   By default, we'll do nothing; subclasses that are sensitive to
     *   the bulks of their contents should override this.  
     */
    checkBulkChangeWithin(changingObj)
    {
    }

    /*
     *   Get "bag of holding" affinities for the given list of objects.
     *   For each item in the list, we'll get the item's bulk, and get
     *   each bag's affinity for containing the item.  We'll note the bag
     *   with the highest affinity.  Once we have the list, we'll put it
     *   in descending order of affinity, and return the result as a
     *   vector.  
     */
    getBagAffinities(lst)
    {
        local infoVec;
        local bagVec; 
        
        /*
         *   First, build a list of all of the bags of holding within me.
         *   We start with the list of bags because we want to check each
         *   object to see which bag we might want to put it in.  
         */
        bagVec = new Vector(10);
        getBagsOfHolding(bagVec);

        /* if there are no bags of holding, there will be no affinities */
        if (bagVec.length() == 0)
            return [];

        /* create a vector to hold information on each child item */
        infoVec = new Vector(lst.length());

        /* look at each child item */
        foreach (local cur in lst)
        {
            local maxAff;
            local bestBag;

            /* find the bag with the highest affinity for this item */
            maxAff = 0;
            bestBag = nil;
            foreach (local bag in bagVec)
            {
                local aff;

                /* get this bag's affinity for this item */                
                aff = bag.affinityFor(cur);

                /* 
                 *   If this is the best so far, note it.  If this bag has
                 *   an affinity of zero for this item, it means it
                 *   doesn't want it at all, so don't even consider it in
                 *   this case. 
                 */
                if (aff != 0 && (bestBag == nil || aff > maxAff))
                {
                    /* this is the best so far - note it */
                    maxAff = aff;
                    bestBag = bag;
                }
            }

            /* 
             *   if we found a bag that wants this item, add it to our
             *   list of results 
             */
            if (bestBag != nil)
                infoVec.append(new BagAffinityInfo(
                    cur, cur.getEncumberingBulk(self),
                    maxAff, bestBag));
        }
        
        /* sort the list in descending order of affinity */
        infoVec = infoVec.sort(SortDesc, {a, b: a.compareAffinityTo(b)});

        /*
         *   Go through the list and ensure that any item in the list
         *   that's destined to go inside another item that's also in the
         *   list gets put in the other item before the second item gets
         *   put in its bag.  For example, if we have a credit card that
         *   goes in a wallet, and a wallet that goes in a pocket, we want
         *   to ensure that we put the credit card in the wallet before we
         *   put the wallet in the pocket.
         *   
         *   The point of this reordering is that, in some cases, we might
         *   not be able to move the first item once the second item has
         *   been moved.  In our credit-card/wallet/pocket example, the
         *   wallet might have to be closed to go in the pocket, at which
         *   point we'd have to take it out again to put the credit card
         *   into it, defeating the purpose of having put it away in the
         *   first place.  We avoid these kinds of circular traps by
         *   making sure the list is ordered such that we dispose of the
         *   credit card first, then the wallet.
         *   
         *   To avoid looping forever in cases of circular containment,
         *   don't allow any more moves than there are items in the list.  
         */
        for (local i = 1, local len = infoVec.length(), local moves = 0 ;
             i <= len && moves <= len ; ++i)
        {
            /* get this item and its destination */
            local cur = infoVec[i];
            local dest = cur.bag_;
            
            /* 
             *   check to see if the destination itself appears in the
             *   list as an item to be bagged 
             */
            local idx = infoVec.indexWhich({x: x.obj_ == dest});

            /* 
             *   if the destination item appears in the list before the
             *   current item, move the current item before its
             *   destination 
             */
            if (idx != nil && idx < i)
            {
                /* remove the current item from the list */
                infoVec.removeElementAt(i);

                /* re-insert it just before its destination item */
                infoVec.insertAt(idx, cur);

                /* count the move */
                ++moves;

                /* 
                 *   Now, back up and resume the scan from the item just
                 *   after the item that moves our destination bag.  This
                 *   will ensure that any items destined to go in the bag
                 *   we just moved are caught.  (Although note that we
                 *   start just after the destination item, rather than at
                 *   the destination item, because the destination item
                 *   can't be legitimately moved at this point: if it is,
                 *   it's because we have a circular containment plan.  We
                 *   obviously can't resolve circular containment into a
                 *   linear ordering, so there's no point in even looking
                 *   at the destination item again at this point.  If we
                 *   have a circular chain that's several elements long,
                 *   this won't avoid getting stuck; for that, we have to
                 *   count on our 'moves' limit.)  
                 */
                i = idx + 1;
            }
        }

        /* return the result */
        return infoVec;
    }
    
    /*
     *   Find all of the bags of holding contained within this object and
     *   add them to the given vector.  By default, we'll simply recurse
     *   into our children so they can add their bags of holding.  
     */
    getBagsOfHolding(vec)
    {
        /* 
         *   if my contents can't be touched from outside, there's no
         *   point in traversing our children 
         */
        if (transSensingIn(touch) != transparent)
            return;

        /* add each of my children's bags to the list */
        foreach (local cur in contents)
            cur.getBagsOfHolding(vec);
    }

    /*
     *   The strength of the light the object is giving off, if indeed it
     *   is giving off light.  This value should be one of the following:
     *   
     *   0: The object is giving off no light at all.
     *   
     *   1: The object is self-illuminating, but doesn't give off enough
     *   light to illuminate any other objects.  This is suitable for
     *   something like an LED digital clock.
     *   
     *   2: The object gives off dim light.  This level is bright enough to
     *   illuminate nearby objects, but not enough to go through obscuring
     *   media, and not enough for certain activities requiring strong
     *   lighting, such as reading.
     *   
     *   3: The object gives off medium light.  This level is bright enough
     *   to illuminate nearby objects, and is enough for most activities,
     *   including reading and the like.  Traveling through an obscuring
     *   medium reduces this level to dim (2).
     *   
     *   4: The object gives off strong light.  This level is bright enough
     *   to illuminate nearby objects, and travel through an obscuring
     *   medium reduces it to medium light (3).
     *   
     *   Note that the special value -1 is reserved as an invalid level,
     *   used to flag certain events (such as the need to recalculate the
     *   ambient light level from a new point of view).
     *   
     *   Most objects do not give off light at all.  
     */
    brightness = 0
    
    /*
     *   Sense sizes of the object.  Each object has an individual size for
     *   each sense.  By default, objects are medium for all senses; this
     *   allows them to be sensed from a distance or through an obscuring
     *   medium, but doesn't allow their details to be sensed.  
     */
    sightSize = medium
    soundSize = medium
    smellSize = medium
    touchSize = medium

    /*
     *   Determine whether or not the object has a "presence" in each
     *   sense.  An object has a presence in a sense if an actor
     *   immediately adjacent to the object could detect the object by the
     *   sense alone.  For example, an object has a "hearing presence" if
     *   it is making some kind of noise, and does not if it is silent.
     *   
     *   Presence in a given sense is an intrinsic (which does not imply
     *   unchanging) property of the object, in that presence is
     *   independent of the relationship to any given actor.  If an alarm
     *   clock is ringing, it has a hearing presence, unconditionally; it
     *   doesn't matter if the alarm clock is sealed inside a sound-proof
     *   box, because whether or not a given actor has a sense path to the
     *   object is a matter for a different computation.
     *   
     *   Note that presence doesn't control access: an actor might have
     *   access to an object for a sense even if the object has no
     *   presence in the sense.  Presence indicates whether or not the
     *   object is actively emitting sensory data that would make an actor
     *   aware of the object without specifically trying to apply the
     *   sense to the object.
     *   
     *   By default, an object is visible and touchable, but does not emit
     *   any sound or odor.  
     */
    sightPresence = true
    soundPresence = nil
    smellPresence = nil
    touchPresence = true

    /*
     *   My "contents lister."  This is a Lister object that we use to
     *   display the contents of this object for room descriptions,
     *   inventories, and the like.  
     */
    contentsLister = thingContentsLister

    /* 
     *   the Lister to use when showing my contents as part of my own
     *   description (i.e., for Examine commands) 
     */
    descContentsLister = thingDescContentsLister

    /* 
     *   the Lister to use when showing my contents in response to a
     *   LookIn command 
     */
    lookInLister = thingLookInLister

    /* 
     *   my "in-line" contents lister - this is the Lister object that
     *   we'll use to display my contents parenthetically as part of my
     *   list entry in a second-level contents listing
     */
    inlineContentsLister = inlineListingContentsLister

    /*
     *   My "special" contents lister.  This is the Lister to use to
     *   display the special descriptions for objects that have special
     *   descriptions when we're showing a room description for this
     *   object. 
     */
    specialContentsLister = specialDescLister


    /*
     *   Determine if I can be sensed under the given conditions.  Returns
     *   true if the object can be sensed, nil if not.  If this method
     *   returns nil, this object will not be considered in scope for the
     *   current conditions.
     *   
     *   By default, we return nil if the ambient energy level for the
     *   object is zero.  If the ambient level is non-zero, we'll return
     *   true in 'transparent' conditions, nil for 'opaque', and we'll let
     *   the sense decide via its canObjBeSensed() method for any other
     *   transparency conditions.  Note that 'ambient' as given here is the
     *   ambient level *at the object*, not as seen along my sense path -
     *   so this should NOT be given as the ambient value from a SenseInfo,
     *   which has already been adjusted for the sense path.  
     */
    canBeSensed(sense, trans, ambient)
    {
        /* 
         *   adjust the ambient level for the transparency path, if the
         *   sense uses ambience at all 
         */
        if (sense.ambienceProp != nil)
        {
            /* 
             *   adjust the ambient level for the transparency - if that
             *   leaves a level of zero, the object can't be sensed 
             */
            if (adjustBrightness(ambient, trans) == 0)
                return nil;
        }

        /* check the viewing conditions */
        switch(trans)
        {
        case transparent:
        case attenuated:
            /* 
             *   under transparent or attenuated conditions, I appear as
             *   myself 
             */
            return true;

        case obscured:
        case distant:
            /* 
             *   ask the sense to determine if I can be sensed under these
             *   conditions 
             */
            return sense.canObjBeSensed(self, trans, ambient);

        default:
            /* for any other conditions, I can't be sensed at all */
            return nil;
        }
    }

    /*
     *   Determine if I can be sensed IN DETAIL in the given sense, with
     *   the given SenseInfo description.  By default, an object's details
     *   can be sensed if the sense path is 'transparent' or 'attenuated',
     *   OR the object's scale in the sense is 'large'.
     */
    canDetailsBeSensed(sense, info, pov)
    {
        /* if the sense path is opaque, we can be sensed */
        if (info == nil || info.trans == opaque)
            return nil;

        /* 
         *   if we have 'large' scale in the sense, our details can be
         *   sensed under any conditions 
         */
        if (self.(sense.sizeProp) == large)
            return true;

        /* 
         *   we don't have 'large' scale in the sense, so we can be sensed
         *   in detail only if the sense path is 'transparent' or
         *   'attenuated' 
         */
        return (info.trans is in (transparent, attenuated));
    }

    /*
     *   Call a method on this object from the given point of view.  We'll
     *   push the current point of view, call the method, then restore the
     *   enclosing point of view. 
     */
    fromPOV(actor, pov, propToCall, [args])
    {
        /* push the new point of view */
        pushPOV(actor, pov);

        /* make sure we pop the point of view no matter how we leave */
        try
        {
            /* call the method */
            self.(propToCall)(args...);
        }
        finally
        {
            /* restore the enclosing point of view on the way out */
            popPOV();
        }
    }

    /*
     *   Every Thing has a location, which is the Thing that contains this
     *   object.  A Thing's location can only be a simple object
     *   reference, or nil; it cannot be a list, and it cannot be a method.
     *   
     *   If the location is nil, the object does not exist anywhere in the
     *   simulation's physical model.  A nil location can be used to
     *   remove an object from the game world, temporarily or permanently.
     *   
     *   In general, the 'location' property should be declared for each
     *   statically defined object (explicitly or implicitly via the '+'
     *   syntax).  'location' is a private property - it should never be
     *   evaluated or changed by any subclass or by any other object.
     *   Only Thing methods may evaluate or change the 'location'
     *   property.  So, you can declare a 'location' property when
     *   defining an object, but you should essentially never refer to
     *   'location' directly in any other context; instead, use the
     *   location and containment methods (isIn, etc) when you want to
     *   know an object's containment relationship to another object.  
     */
    location = nil

    /*
     *   Get the direct container we have in common with the given object,
     *   if any.  Returns at most one common container.  Returns nil if
     *   there is no common location.  
     */
    getCommonDirectContainer(obj)
    {
        /* we haven't found one yet */
        local found = nil;
        
        /* scan each of our containers for one in common with 'loc' */
        forEachContainer(function(loc) {
            /*
             *   If this location of ours is a direct container of the
             *   other object, it is a common location, so note it.  
             */
            if (obj.isDirectlyIn(loc))
                found = loc;
        });

        /* return what we found */
        return found;
    }

    /*
     *   Get the container (direct or indirect) we have in common with the
     *   object, if any.  
     */
    getCommonContainer(obj)
    {
        /* we haven't found one yet */
        local found = nil;
        
        /* scan each of our containers for one in common with 'loc' */
        forEachContainer(function(loc) {
            /*
             *   If this location of ours is a direct container of the
             *   other object, it is a common location, so note it.  
             */
            if (obj.isIn(loc))
                found = loc;
        });

        /* if we found a common container, return it */
        if (found != nil)
            return found;

        /* 
         *   we didn't find obj's container among our direct containers,
         *   so try our direct container's containers 
         */
        forEachContainer(function(loc) {
            local cur;

            /* try finding for a common container of this container */
            cur = loc.getCommonContainer(obj);

            /* if we found it, note it */
            if (cur != nil)
                found = cur;
        });

        /* return what we found */
        return found;
    }

    /*
     *   Get our "identity" object.  This is the object that we appear to
     *   be an inseparable part of.
     *   
     *   In most cases, an object is its own identity object.  However,
     *   there are times when the object that a player sees isn't the same
     *   as the object that the parser resolves, because we're modeling a
     *   single physical object with several programming objects.  For
     *   example, a complex container has one or more secret internal
     *   program objects representing the different sub-containers; as far
     *   as the player is concerned, all of the sub-containers are just
     *   aspects of the parent complex container, not separate object, so
     *   the sub-containers all have the identity of the parent complex
     *   container.
     *   
     *   By default, this is simply 'self'.  Objects that take their
     *   effective identities from other objects must override this
     *   accordingly.  
     */
    getIdentityObject() { return self; }

    /*
     *   Am I a component of the given object?  The base Thing is not a
     *   component of anything, so we'll simply return nil. 
     */
    isComponentOf(obj) { return nil; }

    /*
     *   General initialization - this will be called during preinit so
     *   that we can set up the initial values of any derived internal
     *   properties.  
     */
    initializeThing()
    {
        /* initialize our location settings */
        initializeLocation();

        /* 
         *   if we're marked 'equivalent', it means we're interchangeable
         *   in parser input with other objects with the same name; set up
         *   our equivalence group listing if so 
         */
        if (isEquivalent)
            initializeEquivalent();

        /* if we have a global parameter name, add it to the global table */
        if (globalParamName != nil)
            langMessageBuilder.nameTable_[globalParamName] = self;
    }

    /*
     *   Initialize my location's contents list - add myself to my
     *   container during initialization
     */
    initializeLocation()
    {
        if (location != nil)
            location.addToContents(self);
    }

    /*
     *   Initialize this class object for listing its instances that are
     *   marked with isEquivalent.  We'll initialize a list group that
     *   lists our equivalent instances as a group.
     *   
     *   Objects are grouped by their equivalence key - each set of objects
     *   with the same key is part of the same group.  The key is
     *   determined by the language module, but is usually just the basic
     *   disambiguation name (the 'disambigName' property in English, for
     *   example).  
     */
    initializeEquivalent()
    {
        /* 
         *   If we're already in an equivalence group (because our key has
         *   changed, for example, or due to inheritance), remove the
         *   existing group from the group list.  If the group isn't
         *   changing after all, we'll just add it back, so removing it
         *   should be harmless.  
         */
        if (equivalentGrouper != nil)
            listWith -= equivalentGrouper;

        /* look up our equivalence key in the global table of groupers */
        equivalentGrouper = equivalentGrouperTable[equivalenceKey];

        /* if there's no grouper for our key, create one */
        if (equivalentGrouper == nil)
        {
            /* create a new grouper */
            equivalentGrouper = equivalentGrouperClass.createInstance();

            /* add it to the table */
            equivalentGrouperTable[equivalenceKey] = equivalentGrouper;
        }

        /* 
         *   Add it to our list, as long as our listWith isn't already
         *   defined as a method.  Note that we intentionally add it as the
         *   last element, because an equivalent group is the most specific
         *   kind of group possible.  
         */
        if (propType(&listWith) != TypeCode)
            listWith += equivalentGrouper;
    }

    /*
     *   A static game-wide table of equivalence groups.  This has a table
     *   of ListGroupEquivalent-derived objects, keyed by equivalence name.
     *   Each group of objects with the same equivalence name is listed in
     *   the same group and so has the same grouper object.  
     */
    equivalentGrouperTable = static (new LookupTable(32, 64))

    /* 
     *   my equivalence grouper class - when we initialize, we'll create a
     *   grouper of this class and store it in equivalentGrouper 
     */
    equivalentGrouperClass = ListGroupEquivalent

    /* 
     *   Our equivalent item grouper.  During initialization, we will
     *   create an equivalent grouper and store it in this property for
     *   each class object that has instances marked with isEquivalent.
     *   Note that this is stored with the class, because we want each of
     *   our equivalent instances to share the same grouper object so that
     *   they are listed together as a group.  
     */
    equivalentGrouper = nil

    /*
     *   My contents.  This is a list of the objects that this object
     *   directly contains.
     */
    contents = []

    /*
     *   Get my associated noise object.  By default, this looks for an
     *   item of class Noise directly within me.
     */
    getNoise()
    {
        /* 
         *   Look for a Noise object among my direct contents.  Only look
         *   for a noise with an active sound presence.  
         */
        return contents.valWhich(
            {obj: obj.ofKind(Noise) && obj.soundPresence});
    }

    /*
     *   Get my associated odor object.  By default, this looks for an
     *   item of class Odor directly within me. 
     */
    getOdor()
    {
        /* 
         *   Look for an Odor object among my direct contents.  Only look
         *   for an odor with an active smell presence. 
         */
        return contents.valWhich(
            {obj: obj.ofKind(Odor) && obj.smellPresence});
    }

    /*
     *   Get a vector of all of my contents, recursively including
     *   contents of contents.  
     */
    allContents()
    {
        local vec;
        
        /* start with an empty vector */
        vec = new Vector(32);

        /* add all of my contents to the vector */
        addAllContents(vec);

        /* return the result */
        return vec;
    }

    /*
     *   Get a list of objects suitable for matching ALL in TAKE ALL FROM
     *   <self>.  By default, this simply returns the list of everything in
     *   scope that's directly inside 'self'.  
     */
    getAllForTakeFrom(scopeList)
    {
        /* 
         *   include only objects contained within 'self' that aren't
         *   components of 'self' 
         */
        return scopeList.subset(
            {x: x != self && x.isDirectlyIn(self) && !x.isComponentOf(self)});
    }

    /* 
     *   add myself and all of my contents, recursively including contents
     *   of contents, to a vector 
     */
    addAllContents(vec)
    {
        /* visit everything in my contents */
        foreach (local cur in contents)
        {
            /* 
             *   if this item is already in the vector, skip it - we've
             *   already visited it and its contents
             */
            if (vec.indexOf(cur) != nil)
                continue;
            
            /* add this item */
            vec.append(cur);

            /* add this item's contents recursively */
            cur.addAllContents(vec);
        }
    }

    /*
     *   Show the contents of this object, as part of a recursive listing
     *   generated as part of the description of our container, our
     *   container's container, or any further enclosing container.
     *   
     *   If the object has any contents, we'll display a listing of the
     *   contents.  This is used to display the object's contents as part
     *   of the description of a room ("look around"), of an object
     *   ("examine box"), or of an object's contents ("look in box").
     *   
     *   'options' is the set of flags that we'll pass to showList(), and
     *   has the same meaning as for that function.
     *   
     *   'infoTab' is a lookup table of SenseInfo objects for the objects
     *   that the actor to whom we're showing the contents listing can see
     *   via the sight-like senses.
     *   
     *   This method should be overridden by any object that doesn't store
     *   its contents using a simple 'contents' list property.  
     */
    showObjectContents(pov, lister, options, indent, infoTab)
    {
        /* get my listable contents */
        local cont = lister.getListedContents(self, infoTab);
        
        /* if the surviving list isn't empty, show it */
        if (cont != [])
            lister.showList(pov, self, cont, options, indent, infoTab, nil);
    }

    /*
     *   Show the contents of this object as part of an inventory listing.
     *   By default, we simply use the same listing we do for the normal
     *   contents listing. 
     */
    showInventoryContents(pov, lister, options, indent, infoTab)
    {
        /* by default, use the normal room/object contents listing */
        showObjectContents(pov, lister, options, indent, infoTab);
    }

    /*
     *   Get my listed contents for recursive object descriptions.  This
     *   returns the list of the direct contents that we'll mention when
     *   we're examining our container's container, a further enclosing
     *   container, or the enclosing room.
     *   
     *   By default, we'll return the same list we'll display on direct
     *   examination of this object.
     */
    getListedContents(lister, infoTab)
    {
        /* 
         *   return the contents we'd list when directly examining self,
         *   limited to the listable contents 
         */
        return getContentsForExamine(lister, infoTab)
            .subset({x: lister.isListed(x)});
    }

    /*
     *   Get the listed contents in a direct examination of this object.
     *   By default, this simply returns a list of all of our direct
     *   contents that are included in the info table.  
     */
    getContentsForExamine(lister, infoTab)
    {
        /*
         *   return only my direct contents that are found in the infoTab,
         *   since these are the objects that can be sensed from the
         *   actor's point of view 
         */
        return contents.subset({x: infoTab[x] != nil});
    }

    /* 
     *   Is this a "top-level" location?  A top-level location is an
     *   object which doesn't have another container, so its 'location'
     *   property is nil, but which is part of the game universe anyway.
     *   In most cases, a top-level location is simply a Room, since the
     *   network of rooms makes up the game's map.
     *   
     *   If an object has no location and is not itself a top-level
     *   location, then the object is not part of the game world.  It's
     *   sometimes useful to remove objects from the game world, such as
     *   when they're destroyed within the context of the game.  
     */
    isTopLevel = nil

    /*
     *   Determine if I'm is inside another Thing.  Returns true if this
     *   object is contained within 'obj', directly or indirectly (that
     *   is, this returns true if my immediate container is 'obj', OR my
     *   immediate container is in 'obj', recursively defined).
     *   
     *   isIn(nil) returns true if this object is "outside" the game
     *   world, which means that the object is not reachable from anywhere
     *   in the game map and is thus not part of the simulation.  This is
     *   the case if our outermost container is NOT a top-level object, as
     *   indicated by its isTopLevel property.  If we're inside an object
     *   marked as a top-level object, or we're inside an object that's
     *   inside a top-level object (and so on), then we're part of the
     *   game world, so isIn(nil) will return nil.  If our outermost
     *   container is has a nil isTopLevel property, isIn(nil) will return
     *   true.
     *   
     *   Note that our notion of "in" is not limited to enclosing
     *   containment, because the same containment hierarchy is used to
     *   represent all types of containment relationships, including
     *   things being "on" other things and part of other things.  
     */
    isIn(obj)
    {
        local loc = location;
        
        /* if we have no location, we're not inside any object */
        if (loc == nil)
        {
            /*
             *   We have no container, so there are two possibilities:
             *   either we're not part of the game world at all, or we're
             *   a top-level object, which is an object that's explicitly
             *   part of the game world and at the top of the containment
             *   tree.  Our 'isTopLevel' property determines which it is.
             *   
             *   If they're asking us if we're inside 'nil', then what
             *   they want to know is if we're outside the game world.  If
             *   we're not a top-level object, we are outside the game
             *   world, so isIn(nil) is true; if we are a top-level
             *   object, then we're explicitly part of the game world, so
             *   isIn(nil) is false.
             *   
             *   If they're asking us if we're inside any non-nil object,
             *   then they simply want to know if we're inside that
             *   object.  So, if 'obj' is not nil, we must return nil:
             *   we're not in any object, so we can't be in 'obj'.  
             */
            if (obj == nil)
            {
                /* 
                 *   they want to know if we're outside the simulation
                 *   entirely: return true if we're NOT a top-level
                 *   object, nil otherwise 
                 */
                return !isTopLevel;
            }
            else
            {
                /*
                 *   they want to know if we're inside some specific
                 *   object; we can't be, because we're not in any object 
                 */
                return nil;
            }
        }

        /* if obj is my immediate container, I'm obviously in it */
        if (loc == obj)
            return true;

        /* I'm in obj if my container is in obj */
        return loc.isIn(obj);
    }

    /*
     *   Determine if I'm directly inside another Thing.  Returns true if
     *   this object is contained directly within obj.  Returns nil if
     *   this object isn't directly within obj, even if it is indirectly
     *   in obj (i.e., its container is directly or indirectly in obj).  
     */
    isDirectlyIn(obj)
    {
        /* I'm directly in obj only if it's my immediate container */
        return location == obj;
    }

    /*
     *   Determine if I'm "nominally" inside the given Thing.  Returns true
     *   if the object is actually within the given object, OR the object
     *   is a room part and I'm nominally in the room part.  
     */
    isNominallyIn(obj)
    {
        /* if I'm actually in the given object, I'm nominally in it as well */
        if (isIn(obj))
            return true;

        /* 
         *   if the object is a room part, and we're nominally in the room
         *   part, then we're nominally in the object 
         */
        if (obj.ofKind(RoomPart) && isNominallyInRoomPart(obj))
            return true;

        /* we're not nominally in the object */
        return nil;
    }

    /*
     *   Determine if this object is contained within an item fixed in
     *   place within the given location.  We'll check our container to
     *   see if its contents are within an object fixed in place in the
     *   given location.
     *   
     *   This is a rather specific check that might seem a bit odd, but
     *   for some purposes it's useful to treat objects within fixed
     *   containers in a location as though they were in the location
     *   itself, because fixtures of a location are to some extent parts
     *   of the location.  
     */
    isInFixedIn(loc)
    {
        /* return true if my location's contents are in fixed things */
        return location != nil && location.contentsInFixedIn(loc);
    }

    /* Am I either inside 'obj', or equal to 'obj'?  */
    isOrIsIn(obj) { return self == obj || isIn(obj); }

    /*
     *   Are my contents within a fixed item that is within the given
     *   location?  By default, we return nil because we are not ourselves
     *   fixed. 
     */
    contentsInFixedIn(loc) { return nil; }

    /*
     *   Determine if I'm "held" by an actor, for the purposes of being
     *   manipulated in an action.  In most cases, an object is considered
     *   held by an actor if it's directly within the actor's inventory,
     *   because the actor's direct inventory represents the contents of
     *   the actors hands (or equivalent).
     *   
     *   Some classes might override this to change the definition of
     *   "held" to include things not directly in the actor's inventory or
     *   exclude things directly in the inventory.  For example, an item
     *   being worn is generally not considered held even though it might
     *   be in the direct inventory, and a key on a keyring is considered
     *   held if the keyring is being held.  
     */
    isHeldBy(actor)
    {
        /* 
         *   by default, an object is held if and only if it's in the
         *   direct inventory of the actor 
         */
        return isDirectlyIn(actor);
    }

    /*
     *   Are we being held by the given actor for the purposes of the
     *   objHeld precondition?  By default, and in almost all cases, this
     *   simply returns isHeldBy().  In some rare cases, it might make
     *   sense to consider an object to meet the objHeld condition even if
     *   isHeldBy returns nil; for example, an actor's hands and other
     *   body parts can't be considered to be held, but they also don't
     *   need to be for any command operating on them.  
     */
    meetsObjHeld(actor) { return isHeldBy(actor); }

    /*
     *   Add to a vector all of my contents that are directly held when
     *   I'm being directly held.  This is used to add the direct contents
     *   of an item to scope when the item itself is being directly held.
     *   
     *   In most cases, we do nothing.  Certain types of objects override
     *   this because they consider their contents to be held if they're
     *   held.  For example, a keyring considers all of its keys to be
     *   held if the keyring itself is held, because the keys are attached
     *   to the keyring rather than contained within it.  
     */
    appendHeldContents(vec)
    {
        /* by default, do nothing */
    }

    /*
     *   Determine if I'm "owned" by another object.  By default, if we
     *   have an explicit owner, then we are owned by 'obj' if and only if
     *   'obj' is our explicit owner; otherwise, if 'obj' is our immediate
     *   location, and our immediate location can own us (as reported by
     *   obj.canOwn(self)), then 'obj' owns us; otherwise, 'obj' owns us
     *   if our immediate location CANNOT own us AND our immediate
     *   location is owned by 'obj'.  This last case is tricky: it means
     *   that if we're inside something other than 'obj' that can own us,
     *   such as another actor, then 'obj' doesn't own us because our
     *   immediate location does; it also means that if we're inside an
     *   object that has an explicit owner rather than an owner based on
     *   location, we have the same explicit owner, so a dollar bill
     *   inside Bob's wallet which is in turn being carried by Charlie is
     *   owned by Bob, not Charlie.
     *   
     *   This is used to determine ownership for the purpose of
     *   possessives in commands.  Ownership is not always exclusive: it
     *   is possible for a given object to have multiple owners in some
     *   cases.  For example, if Bob and Bill are both sitting on a couch,
     *   the couch could be referred to as "bob's couch" or "bill's
     *   couch", so the couch is owned by both Bob and Bill.  It is also
     *   possible for an object to be unowned.
     *   
     *   In most cases, ownership is a function of location (possession is
     *   nine-tenths of the law, as they say), but not always; in some
     *   cases, an object has a particular owner regardless of its
     *   location, such as "bob's wallet".  This default implementation
     *   allows for ownership by location, as well as explicit ownership,
     *   with explicit ownership (as indicated by the self.owner property)
     *   taking precedence.  
     */
    isOwnedBy(obj)
    {
        local cont;
        
        /* 
         *   if I have an explicit owner, then obj is my owner if it
         *   matches my explicit owner 
         */
        if (owner != nil)
            return owner == obj;
        
        /*
         *   Check my immediate container to see if it's the owner in
         *   question.  
         */
        if (isDirectlyIn(obj))
        {
            /* 
             *   My immediate container is the owner of interest.  If this
             *   container can own me, then I'm owned by it because we
             *   didn't find any other owners first.  Otherwise, I'm not
             *   owned by it because it can't own me in the first place. 
             */
            return obj.canOwn(self);
        }

        /* presume we have a single-location container */
        cont = location;

        /* check to see if we have no single location */
        if (cont == nil)
        {
            /* 
             *   I have no location, so either I'm not inside anything at
             *   all, or I have multiple locations.  If we're indirectly
             *   in 'obj', then proceed up the containment tree branch by
             *   which 'obj' contains us.  If we're not even indirectly in
             *   'obj', then there's no containment relationship that
             *   establishes ownership.  
             */
            if (isIn(obj))
            {
                /* 
                 *   we're indirectly in 'obj', so get the containment
                 *   branch on which we're contained by 'obj' 
                 */
                forEachContainer(function(x)
                {
                    if (x == obj || x.isIn(obj))
                        cont = x;
                });
            }
            else
            {
                /* 
                 *   we're not in 'obj', so there's no containment
                 *   relationship that establishes ownership 
                 */
                return nil;
            }
        }

        /*
         *   My immediate container is not the object of interest.  If
         *   this container can own me, then I'm owned by this container
         *   and NOT by obj.  
         */
        if (cont.canOwn(self))
            return nil;

        /*
         *   My container can't own me, so it's not my owner, so our owner
         *   is my container's owner - thus, I'm owned by obj if my
         *   location is owned by obj.  
         */
        return cont.isOwnedBy(obj);
    }

    /*
     *   My explicit owner.  By default, objects do not have explicit
     *   owners, which means that the owner at any given time is
     *   determined by the object's location - my innermost container that
     *   can own me is my owner.
     *   
     *   However, in some cases, an object is inherently owned by a
     *   particular other object (usually an actor), and this is invariant
     *   even when the object moves to a new location not within the
     *   owner.  For such cases, this property can be set to the explicit
     *   owner object, which will cause self.isOwnedBy(obj) to return true
     *   if and only if obj == self.owner.  
     */
    owner = nil

    /*
     *   Get the "nominal owner" of this object.  This is the owner that
     *   we report for the object if asked to distinguish this object from
     *   another via the OwnershipDistinguisher.  Note that the nominal
     *   owner is not necessarily the only owner, because an object can
     *   have multiple owners in some cases; however, the nominal owner
     *   must always be an owner, in that isOwnedBy(getNominalOwner())
     *   should always return true.
     *   
     *   By default, if we have an explicit owner, we'll return that.
     *   Otherwise, if our immediate container can own us, we'll return
     *   our immediate container.  Otherwise, we'll return our immediate
     *   container's nominal owner.  Note that this last case means that a
     *   dollar bill inside Bob's wallet will be Bob's dollar bill, even
     *   if Bob's wallet is currently being carried by another actor.  
     */
    getNominalOwner()
    {
        /* if we have an explicit owner, return that */
        if (owner != nil)
            return owner;

        /* if we have no location, we have no owner */
        if (location == nil)
            return nil;

        /* if our immediate location can own us, return it */
        if (location.canOwn(self))
            return location;

        /* return our immediate location's owner */
        return location.getNominalOwner();
    }

    /*
     *   Can I own the given object?  By default, objects cannot own other
     *   objects.  This can be overridden when ownership is desired.
     *   
     *   This doesn't determine that we *do* own the given object, but
     *   only that we *can* own the given object.
     */
    canOwn(obj) { return nil; }

    /*
     *   Get the carrying actor.  This is the nearest enclosing location
     *   that's an actor. 
     */
    getCarryingActor()
    {
        /* if I don't have a location, there's no carrier */
        if (location == nil)
            return nil;

        /* if my location is an actor, it's the carrying actor */
        if (location.isActor)
            return location;

        /* return my location's carrying actor */
        return location.getCarryingActor();
    }

    /*
     *   Try making the current command's actor hold me.  By default,
     *   we'll simply try a "take" command on the object.  
     */
    tryHolding()
    {
        /*   
         *   Try an implicit 'take' command.  If the actor is carrying the
         *   object indirectly, make the command "take from" instead,
         *   since what we really want to do is take the object out of its
         *   container.  
         */
        if (isIn(gActor))
            return tryImplicitAction(TakeFrom, self, location);
        else
            return tryImplicitAction(Take, self);
    }

    /*
     *   Try moving the given object into this object, with an implied
     *   command.  By default, since an ordinary Thing doesn't have a way
     *   of adding new contents by a player command, this does nothing.
     *   Containers and other objects that can hold new contents can
     *   override this as appropriate.
     */
    tryMovingObjInto(obj) { return nil; }

    /* 
     *   Report a failure of the condition that tryMovingObjInto tries to
     *   bring into effect.  By default, this simply says that the object
     *   must be in 'self'.  Some objects might want to override this when
     *   they describe containment specially; for example, an actor might
     *   want to say that the actor "must be carrying" the object.  
     */
    mustMoveObjInto(obj) { reportFailure(&mustBeInMsg, obj, self); }

    /*
     *   Add an object to my contents.
     *   
     *   Note that this should NOT be overridden to cause side effects -
     *   if side effects are desired when inserting a new object into my
     *   contents, use notifyInsert().  This routine is not allowed to
     *   cause side effects because it is sometimes necessary to bypass
     *   side effects when moving an item.  
     */
    addToContents(obj)
    {
        /* add the object to my contents list */
        if (contents.indexOf(obj) == nil)
            contents += obj;
    }

    /*
     *   Remove an object from my contents.
     *   
     *   Do NOT override this routine to cause side effects.  If side
     *   effects are desired when removing an object, use notifyRemove().  
     */
    removeFromContents(obj)
    {
        /* remove the object from my contents list */
        contents -= obj;
    }

    /*
     *   Save my location for later restoration.  Returns a value suitable
     *   for passing to restoreLocation. 
     */
    saveLocation()
    {
        /* 
         *   I'm an ordinary object with only one location, so simply
         *   return the location 
         */
        return location;
    }

    /*
     *   Restore a previously saved location.  Does not trigger any side
     *   effects. 
     */
    restoreLocation(oldLoc)
    {
        /* move myself without side effects into my old container */
        baseMoveInto(oldLoc);
    }

    /*
     *   Move this object to a new container.  Before the move is actually
     *   performed, we notify the items in the movement path of the
     *   change, then we send notifyRemove and notifyInsert messages to
     *   the old and new containment trees, respectively.
     *   
     *   All notifications are sent before the object is actually moved.
     *   This means that the current game state at the time of the
     *   notifications reflects the state before the move.  
     */
    moveInto(newContainer)
    {
        /* notify the path */
        moveIntoNotifyPath(newContainer);

        /* perform the main moveInto operations */
        mainMoveInto(newContainer);
    }

    /*
     *   Move this object to a new container as part of travel.  This is
     *   almost the same as the regular moveInto(), but does not attempt
     *   to calculate and notify the sense path to the new location.  We
     *   omit the path notification because travel is done via travel
     *   connections, which are not necessarily the same as sense
     *   connections.  
     */
    moveIntoForTravel(newContainer)
    {
        /* 
         *   perform the main moveInto operations, omitting the path
         *   notification 
         */
        mainMoveInto(newContainer);
    }

    /*
     *   Main moveInto - this is the mid-level containment changer; this
     *   routine sends notifications to the old and new container, but
     *   doesn't notify anything along the connecting sense path. 
     */
    mainMoveInto(newContainer)
    {
        /* notify my container that I'm being removed */
        sendNotifyRemove(self, newContainer, &notifyRemove);

        /* notify my new container that I'm about to be added */
        if (newContainer != nil)
            newContainer.sendNotifyInsert(self, newContainer, &notifyInsert);

        /* notify myself that I'm about to be moved */
        notifyMoveInto(newContainer);

        /* perform the basic containment change */
        baseMoveInto(newContainer);

        /* note that I've been moved */
        moved = true;
    }

    /*
     *   Base moveInto - this is the low-level containment changer; this
     *   routine does not send any notifications to any containers, and
     *   does not mark the object as moved.  This form should be used only
     *   for internal library-initiated state changes, since it bypasses
     *   all of the normal side effects of moving an object to a new
     *   container.  
     */
    baseMoveInto(newContainer)
    {
        /* if I have a container, remove myself from its contents list */
        if (location != nil)
            location.removeFromContents(self);

        /* remember my new location */
        location = newContainer;

        /*
         *   if I'm not being moved into nil, add myself to the
         *   container's contents
         */
        if (location != nil)
            location.addToContents(self);
    }

    /*
     *   Notify each element of the move path of a moveInto operation. 
     */
    moveIntoNotifyPath(newContainer)
    {
        local path;
        
        /* calculate the path; if there isn't one, there's nothing to do */
        if ((path = getMovePathTo(newContainer)) == nil)
            return;

        /*
         *   We must fix up the path's final element, depending on whether
         *   we're moving into the new container from the outside or from
         *   the inside.  
         */
        if (isIn(newContainer))
        {
            /*
             *   We're already in the new container, so we're moving into
             *   the new container from the inside.  The final element of
             *   the path is the new container, but since we're stopping
             *   within the new container, we don't need to traverse out
             *   of it.  Simply remove the final two elements of the path,
             *   since we're not going to make this traversal when moving
             *   the object.  
             */
            path = path.sublist(1, path.length() - 2);
        }
        else
        {
            /*   
             *   We're moving into the new container from the outside.
             *   Since we calculated the path to the container, we must
             *   now add a final element to traverse into the container;
             *   the final object in the path doesn't matter, since it's
             *   just a placeholder for the new item inside the container 
             */
            path += [PathIn, nil];
        }

        /* traverse the path, sending a notification to each element */
        traversePath(path, function(target, op) {
            /* notify this path element */
            target.notifyMoveViaPath(self, newContainer, op);

            /* continue the traversal */
            return true;
        });
    }

    /*
     *   Call a function on each container.  If we have no location, we
     *   won't invoke the function at all.  We'll invoke the function as
     *   follows:
     *   
     *   (func)(location, args...)  
     */
    forEachContainer(func, [args])
    {
        /* call the function on our location, if we have one */
        if (location != nil)
            (func)(location, args...);
    }

    /* 
     *   Call a function on each *connected* container.  For most objects,
     *   this is the same as forEachContainer, but objects that don't
     *   connect their containers for sense purposes would do nothing
     *   here. 
     */
    forEachConnectedContainer(func, [args])
    {
        /* by default, use the standard forEachContainer handling */
        forEachContainer(func, args...);
    }

    /* 
     *   Get a list of all of my connected containers.  This simply returns
     *   the list that forEachConnectedContainer() iterates over.  
     */
    getConnectedContainers()
    {
        local loc;

        /* 
         *   if I have a non-nil location, return a list containing it;
         *   otherwise, simply return an empty list 
         */
        return ((loc = location) == nil ? [] : [loc]);
    }

    /*
     *   Clone this object for inclusion in a MultiInstance's contents
     *   tree.  When we clone an instance object for a MultiInstance, we'll
     *   also clone its contents, and their contents, and so on.  This
     *   routine creates a private copy of all of our contents.  
     */
    cloneMultiInstanceContents()
    {
        local origContents;
        
        /* 
         *   Remember my current contents list, then clear it out.  We want
         *   a private copy of everything in our contents list, so we want
         *   to forget our references to the template contents and start
         *   from scratch.  
         */
        origContents = contents;
        contents = [];

        /* clone each entry in our contents list */
        foreach (local cur in origContents)
            cur.cloneForMultiInstanceContents(self);
    }

    /* 
     *   Create a clone for inclusion in MultiInstance contents.  We'll
     *   recursively clone our own contents. 
     */
    cloneForMultiInstanceContents(loc)
    {
        /* create a new instance of myself */
        local cl = createInstance();

        /* move it into the new location */
        cl.baseMoveInto(loc);

        /* clone its contents */
        cl.cloneMultiInstanceContents();
    }

    /*
     *   Determine if I'm a valid staging location for the given nested
     *   room destination 'dest'.  This is called when the actor is
     *   attempting to enter the nested room 'dest', and the travel
     *   handlers find that we're the staging location for the room.  (A
     *   "staging location" is the location the actor is required to
     *   occupy immediately before moving into the destination.)
     *   
     *   If this object is a valid staging location, the routine should
     *   simply do nothing.  If this object isn't valid as a staging
     *   location, this routine should display an appropriate message and
     *   terminate the command with 'exit'.
     *   
     *   An arbitrary object can't be a staging location, simply because
     *   an actor can't enter an arbitrary object.  So, by default, we'll
     *   explain that we can't enter this object.  If the destination is
     *   contained within us, we'll provide a more specific explanation
     *   indicating that the problem is that the destination is within us.
     */
    checkStagingLocation(dest)
    {
        /* 
         *   if the destination is within us, explain specifically that
         *   this is the problem 
         */
        if (dest.isIn(self))
            reportFailure(&invalidStagingContainerMsg, self, dest);
        else
            reportFailure(&invalidStagingLocationMsg, self);

        /* terminate the command */
        exit;
    }

    /*
     *   by default, objects don't accept commands 
     */
    acceptCommand(issuingActor)
    {
        /* report that we don't accept commands */
        gLibMessages.cannotTalkTo(self, issuingActor);

        /* tell the caller we don't accept commands */
        return nil;
    }

    /*
     *   by default, most objects are not logical targets for commands 
     */
    isLikelyCommandTarget = nil

    /*
     *   Get my extra scope items.  This is a list of any items that we
     *   want to add to command scope (i.e., the set of all items to which
     *   an actor is allowed to refer with noun phrases) when we are
     *   ourselves in the command scope.  Returns a list of the items to
     *   add (or just [] if there are no items to add).
     *   
     *   By default, we add nothing.  
     */
    getExtraScopeItems(actor) { return []; }

    /*
     *   Generate a lookup table of all of the objects connected by
     *   containment to this object.  This table includes all containment
     *   connections, even through closed containers and the like.
     *   
     *   The table is keyed by object; the associated values are
     *   meaningless, as all that matters is whether or not an object is
     *   in the table.  
     */
    connectionTable()
    {
        local tab;
        local cache;

        /* if we already have a cached connection list, return it */
        if ((cache = libGlobal.connectionCache) != nil
            && (tab = cache[self]) != nil)
            return tab;

        /* remember the point of view globally, in case anyone needs it */
        senseTmp.pointOfView = self;

        /* create a lookup table for the results */
        tab = new LookupTable(32, 64);

        /* add everything connected to me */
        addDirectConnections(tab);

        /* cache the list, if we caching is active */
        if (cache != nil)
            cache[self] = tab;

        /* return the table */
        return tab;
    }

    /*
     *   Add this item and its direct containment connections to the lookup
     *   table.  This must recursively add all connected objects that
     *   aren't already in the table.  
     */
    addDirectConnections(tab)
    {
        local cur;
        
        /* add myself */
        tab[self] = true;

        /* 
         *   Add my CollectiveGroup objects, if any.  We don't have to
         *   traverse into the CollectiveGroup objects, since they don't
         *   connect us or itself to anything else; our connection to a
         *   collective group is one-way.  
         */
        foreach (cur in collectiveGroups)
            tab[cur] = true;

        /* 
         *   Add my contents to the table.  Loop over our contents list,
         *   and add each item that's not already in the table, then
         *   recursively everything connected to that item.
         *   
         *   Note that we use a C-style 'for' loop to iterate over the list
         *   by index, rather than a more modern tads-style 'foreach' loop,
         *   purely for performance reasons: this code is called an awful
         *   lot, and the iteration by loop index is very slightly faster.
         *   A 'foreach' requires calling a couple of methods of an
         *   iterator object each time through the loop, whereas we can do
         *   the index-based 'for' loop entirely with cached local
         *   variables.  The performance difference is negligible for one
         *   time through any given loop; it only matters to us here
         *   because this routine tends to be invoked *extremely*
         *   frequently (an average of roughly 500 times per turn in the
         *   library sample game, for example).  Game authors are strongly
         *   advised to consider 'foreach' and 'for' equivalent in
         *   performance for most purposes - just choose the clearest
         *   construct for your situation.  
         */
        for (local clst = contents, local i = 1,
             local len = clst.length() ; i <= len ; ++i)
        {
            /* get the current item */
            cur = clst[i];
            
            /* if it's not already in the table, add it (recursively) */
            if (tab[cur] == nil)
                cur.addDirectConnections(tab);
        }

        /* add my container if it's not already in the table */
        if ((cur = location) != nil && tab[cur] == nil)
            cur.addDirectConnections(tab);
    }

    /*
     *   Try an implicit action that would remove this object as an
     *   obstructor to 'obj' from the perspective of the current actor in
     *   the given sense.  This is invoked when this object is acting as
     *   an obstructor between the current actor and 'obj' for the given
     *   sense, and the caller wants to perform a command that requires a
     *   clear sense path to the given object in the given sense.
     *   
     *   If it is possible to perform an implicit command that would clear
     *   the obstruction, try performing the command, and return true.
     *   Otherwise, simply return nil.  The usual implied command rules
     *   should be followed (which can be accomplished simply by using
     *   tryImplictAction() to execute any implied command).
     *   
     *   The particular type of command that would remove this obstructor
     *   can vary by obstructor class.  For a container, for example, an
     *   "open" command is the usual remedy.  
     */
    tryImplicitRemoveObstructor(sense, obj)
    {
        /* by default, we have no way of clearing our obstruction */
        return nil;
    }

    /*
     *   Display a message explaining why we are obstructing a sense path
     *   to the given object.
     */
    cannotReachObject(obj)
    {
        /* 
         *   Default objects have no particular obstructive capabilities,
         *   so we can't do anything but show the default message.  This
         *   is a last resort that should rarely be used; normally, we
         *   will be able to identify a specific obstructor that overrides
         *   this to explain precisely what kind of obstruction is
         *   involved.  
         */
        gLibMessages.cannotReachObject(obj);
    }

    /*
     *   Display a message explaining that the source of a sound cannot be
     *   seen because I am visually obstructing it.  By default, we show
     *   nothing at all; subclasses can override this to provide a better
     *   explanation when possible.  
     */
    cannotSeeSoundSource(obj) { }

    /* explain why we cannot see the source of an odor */
    cannotSeeSmellSource(obj) { }

    /*
     *   Get the path for this object reaching out and touching the given
     *   object.  This can be used to determine whether or not an actor
     *   can touch the given object.  
     */
    getTouchPathTo(obj)
    {
        local path;
        local key = [self, obj];
        local cache;
        local info;

        /* if we have a cache, try finding the data in the cache */
        if ((cache = libGlobal.canTouchCache) != nil
            && (info = cache[key]) != nil)
        {
            /* we have a cache entry - return the path from the cache */
            return info.touchPath;
        }

        /* select the path from here to the target object */
        path = selectPathTo(obj, &canTouchViaPath);

        /* if caching is enabled, add a cache entry for the path */
        if (cache != nil)
            cache[key] = new CanTouchInfo(path);

        /* return the path */
        return path;
    }

    /*
     *   Determine if I can touch the given object.  By default, we can
     *   always touch our immediate container; otherwise, we can touch
     *   anything with a touch path that we can traverse.
     */
    canTouch(obj)
    {
        local path;
        local result;
        local key = [self, obj];
        local cache;
        local info;

        /* if we have a cache, check the cache for the desired data */
        if ((cache = libGlobal.canTouchCache) != nil
            && (info = cache[key]) != nil)
        {
            /* if we cached the canTouch result, return it */
            if (info.propDefined(&canTouch))
                return info.canTouch;

            /* 
             *   we didn't calculate the canTouch result, but we at least
             *   have a cached path that we can avoid recalculating 
             */
            path = info.touchPath;
        }
        else
        {
            /* we don't have a cached path, so calculate it anew */
            path = getTouchPathTo(obj);
        }
        
        /* if there's no 'touch' path, we can't touch the object */
        if (path == nil)
        {
            /* there's no path */
            result = nil;
        }
        else
        {
            /* we have a path - check to see if we can reach along it */
            result = traversePath(path,
                {ele, op: ele.canTouchViaPath(self, obj, op)});
        }

        /* if caching is active, cache our result */
        if (cache != nil)
        {
            /* if we don't already have a cache entry, create one */
            if (info == nil)
                cache[key] = info = new CanTouchInfo(path);

            /* save our canTouch result in the cache entry */
            info.canTouch = result;
        }

        /* return the result */
        return result;
    }

    /*
     *   Find the object that prevents us from touching the given object.
     */
    findTouchObstructor(obj)
    {
        /* cache 'touch' sense path information */
        cacheSenseInfo(connectionTable(), touch);
        
        /* return the opaque obstructor for the sense of touch */
        return findOpaqueObstructor(touch, obj);
    }

    /*
     *   Get the path for moving this object from its present location to
     *   the given new container. 
     */
    getMovePathTo(newLoc)
    {
        /* 
         *   select the path from here to the new location, using the
         *   canMoveViaPath method to discriminate among different path
         *   possibilities. 
         */
        return selectPathTo(newLoc, &canMoveViaPath);
    }

    /*
     *   Get the path for throwing this object from its present location
     *   to the given target object. 
     */
    getThrowPathTo(newLoc)
    {
        /*
         *   select the path from here to the target, using the
         *   canThrowViaPath method to discriminate among different paths 
         */
        return selectPathTo(newLoc, &canThrowViaPath);
    }

    /*
     *   Determine if we can traverse self for moving the given object in
     *   the given manner.  This is used to determine if a containment
     *   connection path can be used to move an object to a new location.
     *   Returns a CheckStatus object indicating success if we can
     *   traverse self, failure if not.
     *   
     *   By default, we'll simply return a success indicator.  Subclasses
     *   might want to override this for particular conditions.  For
     *   example, containers would normally override this to return nil
     *   when attempting to move an object in or out of a closed
     *   container.  Some special containers might also want to override
     *   this to allow moving an object in or out only if the object is
     *   below a certain size threshold, for example.
     *   
     *   'obj' is the object being moved, and 'dest' is the destination of
     *   the move.
     *   
     *   'op' is one of the pathXxx operations - PathIn, PathOut, PathPeer
     *   - specifying what kind of movement is being attempted.  PathIn
     *   indicates that we're moving 'obj' from outside self to inside
     *   self; PathOut indicates the opposite.  PathPeer indicates that
     *   we're moving 'obj' entirely within self - this normally means
     *   that we've moved the object out of one of our contents and will
     *   move it into another of our contents.  
     */
    checkMoveViaPath(obj, dest, op) { return checkStatusSuccess; }

    /*
     *   Determine if we can traverse this object for throwing the given
     *   object in the given manner.  By default, this returns the same
     *   thing as canMoveViaPath, since throwing is in most cases the same
     *   as ordinary movement.  Objects can override this when throwing an
     *   object through this path element should be treated differently
     *   from ordinary movement.
     */
    checkThrowViaPath(obj, dest, op)
        { return checkMoveViaPath(obj, dest, op); }

    /*
     *   Determine if we can traverse self in the given manner for the
     *   purposes of 'obj' touching another object.  'obj' is usually an
     *   actor; this determines if 'obj' is allowed to reach through this
     *   path element on the way to touching another object.
     *   
     *   By default, this returns the same thing as canMoveViaPath, since
     *   touching is in most cases the same as ordinary movement of an
     *   object from one location to another.  Objects can overridet his
     *   when touching an object through this path element should be
     *   treated differently from moving an object.  
     */
    checkTouchViaPath(obj, dest, op)
        { return checkMoveViaPath(obj, dest, op); }

    /*
     *   Determine if we can traverse this object for moving the given
     *   object via a path.  Calls checkMoveViaPath(), and returns true if
     *   checkMoveViaPath() indicates success, nil if it indicates failure.
     *   
     *   Note that this method should generally not be overridden; only
     *   checkMoveViaPath() should usually need to be overridden.  
     */
    canMoveViaPath(obj, dest, op)
        { return checkMoveViaPath(obj, dest, op).isSuccess; }

    /* determine if we can throw an object via this path */
    canThrowViaPath(obj, dest, op)
        { return checkThrowViaPath(obj, dest, op).isSuccess; }

    /* determine if we can reach out and touch an object via this path */
    canTouchViaPath(obj, dest, op)
        { return checkTouchViaPath(obj, dest, op).isSuccess; }

    /*
     *   Check moving an object through this container via a path.  This
     *   method is called during moveInto to notify each element along a
     *   move path that the movement is about to occur.  We call
     *   checkMoveViaPath(); if it indicates failure, we'll report the
     *   failure encoded in the status object and terminate the command
     *   with 'exit'.  
     *   
     *   Note that this method should generally not be overridden; only
     *   checkMoveViaPath() should usually need to be overridden.  
     */
    notifyMoveViaPath(obj, dest, op)
    {
        local stat;

        /* check the move */
        stat = checkMoveViaPath(obj, dest, op);

        /* if it's a failure, report the failure message and terminate */
        if (!stat.isSuccess)
        {
            /* report the failure */
            reportFailure(stat.msgProp, stat.msgParams...);

            /* terminate the command */
            exit;
        }
    }

    /*
     *   Choose a path from this object to a given object.  If no paths
     *   are available, returns nil.  If any paths exist, we'll find the
     *   shortest usable one, calling the given property on each object in
     *   the path to determine if the traversals are allowed.
     *   
     *   If we can find a path, but there are no good paths, we'll return
     *   the shortest unusable path.  This can be useful for explaining
     *   why the traversal is impossible.  
     */
    selectPathTo(obj, traverseProp)
    {
        local allPaths;
        local goodPaths;
        local minPath;
        
        /* get the paths from here to the given object */
        allPaths = getAllPathsTo(obj);

        /* if we found no paths, the answer is obvious */
        if (allPaths.length() == 0)
            return nil;

        /* start off with an empty vector for the good paths */
        goodPaths = new Vector(allPaths.length());

        /* go through the paths and find the good ones */
        for (local i = 1, local len = allPaths.length() ; i <= len ; ++i)
        {
            local path = allPaths[i];
            local ok;
            
            /* 
             *   traverse the path, calling the traversal check property
             *   on each point in the path; if any check property returns
             *   nil, it means that the traversal isn't allowed at that
             *   point, so we can't use the path 
             */
            ok = true;
            traversePath(path, function(target, op) {
                /*
                 *   Invoke the check property on this target.  If it
                 *   doesn't allow the traversal, the path is unusable. 
                 */
                if (target.(traverseProp)(self, obj, op))
                {
                    /* we're still okay - continue the path traversal */
                    return true;
                }
                else
                {
                    /* failed - note that the path is no good */
                    ok = nil;

                    /* there's no need to continue the path traversal */
                    return nil;
                }
            });

            /* 
             *   if we didn't find any objections to the path, add this to
             *   the list of good paths 
             */
            if (ok)
                goodPaths.append(path);
        }

        /* if there are no good paths, take the shortest bad path */
        if (goodPaths.length() == 0)
            goodPaths = allPaths;

        /* find the shortest of the paths we're still considering */
        minPath = nil;
        for (local i = 1, local len = goodPaths.length() ; i <= len ; ++i)
        {
            /* get the current path */
            local path = goodPaths[i];
            
            /* if this is the best so far, note it */
            if (minPath == nil || path.length() < minPath.length())
                minPath = path;
        }

        /* return the shortest good path */
        return minPath;
    }

    /*
     *   Traverse a containment connection path, calling the given
     *   function for each element.  In each call to the callback, 'obj'
     *   is the container object being traversed, and 'op' is the
     *   operation being used to traverse it.
     *   
     *   At each stage, the callback returns true to continue the
     *   traversal, nil if we are to stop the traversal.
     *   
     *   Returns nil if any callback returns nil, true if all callbacks
     *   return true.  
     */
    traversePath(path, func)
    {
        local len;
        local target;

        /* 
         *   if there's no path at all, there's nothing to do - simply
         *   return true in this case, because no traversal callback can
         *   fail when there are no traversal callbacks to begin with 
         */
        if (path == nil || (len = path.length()) == 0)
            return true;
        
        /* traverse from the path's starting point */
        if (path[1] != nil && !(func)(path[1], PathFrom))
            return nil;

        /* run through this path and see if the traversals are allowed */
        for (target = nil, local i = 2 ; i <= len ; i += 2)
        {
            local op;
            
            /* get the next traversal operation */
            op = path[i];
            
            /* check the next traversal to see if it's allowed */
            switch(op)
            {
            case PathIn:
                /* 
                 *   traverse in - notify the previous object, since it's
                 *   the container we're entering 
                 */
                target = path[i-1];
                break;

            case PathOut:
                /*
                 *   traversing out of the current container - tell the
                 *   next object, since it's the object we're leaving 
                 */
                target = path[i+1];
                break;

            case PathPeer:
                /*
                 *   traversing from one object to a containment peer -
                 *   notify the container in common to both peers 
                 */
                target = path[i-1].getCommonDirectContainer(path[i+1]);
                break;

            case PathThrough:
                /* 
                 *   traversing through a multi-location connector (the
                 *   previous and next object will always be the same in
                 *   this case, so it doesn't really matter which we
                 *   choose) 
                 */
                target = path[i-1];
                break;
            }

            /* call the traversal callback */
            if (target != nil && !(func)(target, op))
            {
                /* the callback told us not to continue */
                return nil;
            }
        }

        /* 
         *   Traverse to the path's ending point, if we haven't already.
         *   Some operations do traverse to the right-hand element
         *   (PathOut in particular), in which case we'll already have
         *   reached the target.  Most operations traverse the left-hand
         *   element, though, so in these cases we need to visit the last
         *   element explicitly. 
         */
        if (path[len] != nil
            && path[len] != target
            && !(func)(path[len], PathTo))
            return nil;

        /* all callbacks told us to continue */
        return true;
    }

    /*
     *   Build a vector containing all of the possible paths we can
     *   traverse to get from me to the given object.  The return value is
     *   a vector of paths; each path is a list of containment operations
     *   needed to get from here to there.
     *   
     *   Each path item in the vector is a list arranged like so:
     *   
     *   [obj, op, obj, op, obj]
     *   
     *   Each 'obj' is an object, and each 'op' is an operation enum
     *   (PathIn, PathOut, PathPeer) that specifies how to get from the
     *   preceding object to the next object.  The first object in the
     *   list is always the starting object (i.e., self), and the last is
     *   always the target object ('obj').  
     */
    getAllPathsTo(obj)
    {
        local vec;
        
        /* create an empty vector to hold the return set */
        vec = new Vector(10);

        /* look along each connection from me */
        buildContainmentPaths(vec, [self], obj);

        /* if there's no path, check the object for a special path */
        if (obj != nil && vec.length() == 0)
            obj.specialPathFrom(self, vec);

        /* return the vector */
        return vec;
    }

    /*
     *   Get a "special" path from the given starting object to me.
     *   src.getAllPathsTo(obj) calls this on 'obj' when it can't find any
     *   actual containment path from 'src' to 'obj'.  If desired, this
     *   method should add the path or paths to the vector 'vec'.
     *   
     *   By default, we do nothing at all.  The purpose of this routine is
     *   to allow special objects that exist outside the normal containment
     *   model to insinuate themselves into the sense model under special
     *   conditions of their choosing.  
     */
    specialPathFrom(src, vec) { }

    /*
     *   Service routine for getAllPathsTo: build a vector of the
     *   containment paths starting with this object.  
     */
    buildContainmentPaths(vec, pathHere, obj)
    {
        local i, len;
        local cur;
        
        /* scan each of our contents */
        for (i = 1, len = contents.length() ; i <= len ; ++i)
        {
            /* get the current item */
            cur = contents[i];

            /*
             *   If this item is the target, we've found what we're
             *   looking for - simply add the path to here to the return
             *   vector.
             *   
             *   Otherwise, if this item isn't already in the path,
             *   traverse into it; if it's already in the path, there's no
             *   need to look at it because we've already looked at it to
             *   get here 
             */
            if (cur == obj)
            {
                /* 
                 *   The path to here is the path to the target.  Append
                 *   the target object itself with an 'in' operation.
                 *   Before adding the path to the result set, normalize
                 *   it.  
                 */
                vec.append(normalizePath(pathHere + [PathIn, obj]));
            }
            else if (pathHere.indexOf(cur) == nil)
            {
                /* 
                 *   look at this item, adding it to the path with an
                 *   'enter child' traversal 
                 */
                cur.buildContainmentPaths(vec, pathHere + [PathIn, cur], obj);
            }
        }

        /* scan each of our locations */
        for (local clst = getConnectedContainers, i = 1, len = clst.length() ;
             i <= len ; ++i)
        {
            /* get the current item */
            cur = clst[i];

            /*
             *   If this item is the target, we've found what we're
             *   looking for.  Otherwise, if this item isn't already in
             *   the path, traverse into it 
             */
            if (cur == obj)
            {
                /* 
                 *   We have the path to the target.  Add the traversal to
                 *   the container to the path, normalize the path, and
                 *   add the path to the result set. 
                 */
                vec.append(normalizePath(pathHere + [PathOut, cur]));
            }
            else if (pathHere.indexOf(cur) == nil)
            {
                /* 
                 *   Look at this container, adding it to the path with an
                 *   'exit to container' traversal.
                 */
                cur.buildContainmentPaths(vec,
                                          pathHere + [PathOut, cur], obj);
            }
        }
    }

    /*
     *   "Normalize" a containment path to remove redundant containment
     *   traversals.
     *   
     *   First, we expand any sequence of in+out operations that take us
     *   out of one root-level containment tree and into another to
     *   include a "through" operation for the multi-location object being
     *   traversed.  For example, if 'a' and 'c' do not share a common
     *   container, then we will turn this:
     *   
     *     [a PathIn b PathOut c]
     *   
     *   into this:
     *   
     *     [a PathIn b PathThrough b PathOut c]
     *   
     *   This will ensure that when we traverse the path, we will
     *   explicitly traverse through the connector material of 'b'.
     *   
     *   Second, we replace any sequence of out+in operations through a
     *   common container with "peer" operations across the container's
     *   contents directly.  For example, a path that looks like this
     *   
     *     [a PathOut b PathIn c]
     *   
     *   will be normalized to this:
     *   
     *     [a PathPeer c]
     *   
     *   This means that we go directly from a to c, traversing through
     *   the fill medium of their common container 'b' but not actually
     *   traversing out of 'b' and back into it.  
     */
    normalizePath(path)
    {
        /* 
         *   Traverse the path looking for items to normalize with the
         *   (in-out)->(through) transformation.  Start at the second
         *   element, which is the first path operation code.  
         */
        for (local i = 2 ; i <= path.length() ; i += 2)
        {
            /*
             *   If we're on an 'in' operation, and an 'out' operation
             *   immediately follows, and the object before the 'in' and
             *   the object after the 'out' do not share a common
             *   container, we must add an explicit 'through' step for the
             *   multi-location connector being traversed. 
             */
            if (path[i] == PathIn
                && i + 2 <= path.length()
                && path[i+2] == PathOut
                && path[i-1].getCommonDirectContainer(path[i+3]) == nil)
            {
                /* we need to add a 'through' operation */
                path = path.sublist(1, i + 1)
                       + PathThrough
                       + path.sublist(i + 1);
            }
        }

        /* 
         *   make another pass, this time applying the (out-in)->peer
         *   transformation 
         */
        for (local i = 2 ; i <= path.length() ; i += 2)
        {
            /*
             *   If we're on an 'out' operation, and an 'in' operation
             *   immediately follows, we can collapse the out+in sequence
             *   to a single 'peer' operation.  
             */
            if (path[i] == PathOut
                && i + 2 <= path.length()
                && path[i+2] == PathIn)
            {
                /* 
                 *   this sequence can be collapsed to a single 'peer'
                 *   operation - rewrite the path accordingly 
                 */
                path = path.sublist(1, i - 1)
                       + PathPeer
                       + path.sublist(i + 3);
            }
        }

        /* return the normalized path */
        return path;
    }
    
    /*
     *   Get the visual sense information for this object from the current
     *   global point of view.  If we have explicit sense information set
     *   with setSenseInfo, we'll return that; otherwise, we'll calculate
     *   the current sense information for the given point of view.
     *   Returns a SenseInfo object giving the information.  
     */
    getVisualSenseInfo()
    {
        local infoTab;
        
        /* if we have explicit sense information already set, use it */
        if (explicitVisualSenseInfo != nil)
            return explicitVisualSenseInfo;

        /* calculate the sense information for the point of view */
        infoTab = getPOVDefault(gActor).visibleInfoTable();

        /* return the information on myself from the table */
        return infoTab[self];
    }

    /*
     *   Call a description method with explicit point-of-view and the
     *   related point-of-view sense information.  'pov' is the point of
     *   view object, which is usually an actor; 'senseInfo' is a
     *   SenseInfo object giving the sense information for this object,
     *   which we'll use instead of dynamically calculating the sense
     *   information for the duration of the routine called.  
     */
    withVisualSenseInfo(pov, senseInfo, methodToCall, [args])
    {
        local oldSenseInfo;
        
        /* push the sense information */
        oldSenseInfo = setVisualSenseInfo(senseInfo);

        /* push the point of view */
        pushPOV(pov, pov);

        /* make sure we restore the old value no matter how we leave */
        try
        {
            /* 
             *   call the method with the given arguments, and return the
             *   result 
             */
            return self.(methodToCall)(args...);
        }
        finally
        {
            /* restore the old point of view */
            popPOV();
            
            /* restore the old sense information */
            setVisualSenseInfo(oldSenseInfo);
        }
    }

    /* 
     *   Set the explicit visual sense information; if this is not nil,
     *   getVisualSenseInfo() will return this rather than calculating the
     *   live value.  Returns the old value, which is a SenseInfo or nil.  
     */
    setVisualSenseInfo(info)
    {
        local oldInfo;

        /* remember the old value */
        oldInfo = explicitVisualSenseInfo;

        /* remember the new value */
        explicitVisualSenseInfo = info;

        /* return the original value */
        return oldInfo;
    }

    /* current explicit visual sense information overriding live value */
    explicitVisualSenseInfo = nil

    /*
     *   Determine how accessible my contents are to a sense.  Any items
     *   contained within a Thing are considered external features of the
     *   Thing, hence they are transparently accessible to all senses.
     */
    transSensingIn(sense) { return transparent; }

    /*
     *   Determine how accessible peers of this object are to the contents
     *   of this object, via a given sense.  This has the same meaning as
     *   transSensingIn(), but in the opposite direction: whereas
     *   transSensingIn() determines how accessible my contents are from
     *   the outside, this determines how accessible the outside is from
     *   the contents.
     *
     *   By default, we simply return the same thing as transSensingIn(),
     *   since most containers are symmetrical for sense passing from
     *   inside to outside or outside to inside.  However, we distinguish
     *   this as a separate method so that asymmetrical containers can
     *   have different effects in the different directions; for example,
     *   a box made of one-way mirrors might be transparent when looking
     *   from the inside to the outside, but opaque in the other
     *   direction.
     */
    transSensingOut(sense) { return transSensingIn(sense); }

    /*
     *   Get my "fill medium."  This is an object of class FillMedium that
     *   permeates our interior, such as fog or smoke.  This object
     *   affects the transmission of senses from one of our children to
     *   another, or between our interior and exterior.
     *   
     *   Note that the FillMedium object is usually a regular object in
     *   scope, so that the player can refer to the fill medium.  For
     *   example, if a room is filled with fog, the player might want to
     *   be able to refer to the fog in a command.
     *   
     *   By default, our medium is the same as our parent's medium, on the
     *   assumption that fill media diffuse throughout the location's
     *   interior.  Note, though, that Container overrides this so that a
     *   closed Container is isolated from its parent's fill medium -
     *   think of a closed bottle within a room filled with smoke.
     *   However, a fill medium doesn't expand from a child into its
     *   containers - it only diffuses into nested containers, never out.
     *   
     *   An object at the outermost containment level has no fill medium
     *   by default, so we return nil if our location is nil.
     *   
     *   Note that, unlike the "surface" material, the fill medium is
     *   assumed to be isotropic - that is, it has the same sense-passing
     *   characteristics regardless of the direction in which the energy
     *   is traversing the medium.  Since we don't have any information in
     *   our containment model about the positions of our objects relative
     *   to one another, we have no way to express anisotropy in the fill
     *   medium among our children anyway.
     *   
     *   Note further that energy going in or out of this object must
     *   traverse both the fill medium and the surface of the object
     *   itself.  Since we have no other information on the relative
     *   positions of our contents, we can only assume that they're
     *   uniformly distributed through our interior, so it is necessary to
     *   traverse the same amount of fill material to go from one child to
     *   any other or from a child to our inner surface.
     *   
     *   As a sense is transmitted, several consecutive traversals of a
     *   single fill material (i.e., a single object reference) will be
     *   treated as a single traversal of the material.  Since we don't
     *   have a notion of distance in our containment model, we can't
     *   assume that we cover a certain amount of distance just because we
     *   traverse a certain number of containment levels.  So, if we have
     *   three nested containment levels all inheriting a single fill
     *   material from their outermost parent, traversing from the inner
     *   container to the outer container will count as a single traversal
     *   of the material.  
     */
    fillMedium()
    {
        local loc;
        
        return ((loc = location) != nil ? loc.fillMedium() : nil);
    }

    /*
     *   Can I see/hear/smell the given object?  By default, an object can
     *   "see" (or "hear", etc) another if there's a clear path in the
     *   corresponding basic sense to the other object.  Note that actors
     *   override this, because they have a subjective view of the senses:
     *   an actor might see in a special infrared vision sense rather than
     *   (or in addition to) the normal 'sight' sense, for example.  
     */
    canSee(obj) { return senseObj(sight, obj).trans != opaque; }
    canHear(obj) { return senseObj(sound, obj).trans != opaque; }
    canSmell(obj) { return senseObj(smell, obj).trans != opaque; }

    /* can the given actor see/hear/smell/touch me? */
    canBeSeenBy(actor) { return actor.canSee(self); }
    canBeHeardBy(actor) { return actor.canHear(self); }
    canBeSmelledBy(actor) { return actor.canSmell(self); }
    canBeTouchedBy(actor) { return actor.canTouch(self); }

    /* can the player character see/hear/smell/touch me? */
    canBeSeen = (canBeSeenBy(gPlayerChar))
    canBeHeard = (canBeHeardBy(gPlayerChar))
    canBeSmelled = (canBeSmelledBy(gPlayerChar))
    canBeTouched = (canBeTouchedBy(gPlayerChar))

    /*
     *   Am I occluded by the given Occluder when viewed in the given sense
     *   from the given point of view?  The default Occluder implementation
     *   calls this on each object involved in the sense path to determine
     *   if it should occlude the object.  Returns true if we're occluded
     *   by the given Occluder, nil if not.  By default, we simply return
     *   nil to indicate that we're not occluded.  
     */
    isOccludedBy(occluder, sense, pov) { return nil; }

    /*
     *   Determine how well I can sense the given object.  Returns a
     *   SenseInfo object describing the sense path from my point of view
     *   to the object.
     *   
     *   Note that, because 'distant', 'attenuated', and 'obscured'
     *   transparency levels always compound (with one another and with
     *   themselves) to opaque, there will never be more than a single
     *   obstructor in a path, because any path with two or more
     *   obstructors would be an opaque path, and hence not a path at all.
     */
    senseObj(sense, obj)
    {
        local info;

        /* get the sense information from the table */
        info = senseInfoTable(sense)[obj];

        /* if we couldn't find the object, return an 'opaque' indication */
        if (info == nil)
            info = new SenseInfo(obj, opaque, nil, 0);

        /* return the sense data descriptor */
        return info;
    }

    /*
     *   Find an opaque obstructor.  This can be called immediately after
     *   calling senseObj() when senseObj() indicates that the object is
     *   opaquely obscured.  We will find the nearest (by containment)
     *   object where the sense status is non-opaque, and we'll return
     *   that object.
     *   
     *   senseObj() by itself does not determine the obstructor when the
     *   sense path is opaque, because doing so requires extra work.  The
     *   sense path calculator that senseObj() uses cuts off its search
     *   whenever it reaches an opaque point, because beyond that point
     *   nothing can be sensed.
     *   
     *   This can only be called immediately after calling senseObj()
     *   because we re-use the same cached sense path information that
     *   senseObj() uses.  
     */
    findOpaqueObstructor(sense, obj)
    {
        local path;
        
        /* get all of the paths from here to there */
        path = getAllPathsTo(obj);

        /* 
         *   if there are no paths, we won't be able to find a specific
         *   obstructor - the object simply isn't connected to us at all 
         */
        if (path == nil)
            return nil;

        /* 
         *   Arbitrarily take the first path - there must be an opaque
         *   obstructor on every path or we never would have been called
         *   in the first place.  One opaque obstructor is as good as any
         *   other for our purposes.  
         */
        path = path[1];

        /* 
         *   The last thing in the list that can be sensed is the opaque
         *   obstructor.  Note that the path entries alternate between
         *   objects and traversal operations.  
         */
        for (local i = 3, local len = path.length() ; i <= len ; i += 2)
        {
            local obj;
            local trans;
            local ambient;

            /* get this object */
            obj = path[i];

            /* 
             *   get the appropriate sense direction - if the path takes
             *   us out, look at the interior sense data for the object;
             *   otherwise look at its exterior sense data 
             */
            if (path[i-1] == PathOut)
            {
                /* we're looking outward, so use the interior data */
                trans = obj.tmpTransWithin_;
                ambient = obj.tmpAmbientWithin_;
            }
            else
            {
                /* we're looking inward, so use the exterior data */
                trans = obj.tmpTrans_;
                ambient = obj.tmpAmbient_;
            }

            /* 
             *   if this item cannot be sensed, the previous item is the
             *   opaque obstructor 
             */
            if (!obj.canBeSensed(sense, trans, ambient))
            {
                /* can't sense it - the previous item is the obstructor */
                return path[i-2];
            }
        }

        /* we didn't find any obstructor */
        return nil;
    }
    
    /*
     *   Build a list of full information on all of the objects reachable
     *   from me through the given sense, along with full information for
     *   each object's sense characteristics.
     *   
     *   We return a lookup table of each object that can be sensed (in
     *   the given sense) from the point of view of 'self'.  The key for
     *   each entry in the table is an object, and the corresponding value
     *   is a SenseInfo object describing the sense conditions for the
     *   object.  
     */
    senseInfoTable(sense)
    {
        local objs;
        local tab;
        local siz;
        local cache;
        local key = [self, sense];

        /* if we have cached sense information, simply return it */
        if ((cache = libGlobal.senseCache) != nil
            && (tab = cache[key]) != nil)
            return tab;
        
        /* 
         *   get the list of objects connected to us by containment -
         *   since the only way senses can travel between objects is via
         *   containment relationships, this is the complete set of
         *   objects that could be connected to us by any senses 
         */
        objs = connectionTable();

        /* we're the point of view for this path calculation */
        senseTmp.pointOfView = self;

        /* cache the sensory information for all of these objects */
        cacheSenseInfo(objs, sense);

        /* build a table of all of the objects we can reach */
        siz = objs.getEntryCount();
        tab = new LookupTable(32, siz == 0 ? 32 : siz);
        objs.forEachAssoc(function(cur, val)
        {
            /* add this object's sense information to the table */
            cur.addToSenseInfoTable(sense, tab);
        });

        /* add the information vector to the sense cache */
        if (cache != nil)
            cache[key] = tab;

        /* return the result table */
        return tab;
    }

    /*
     *   Add myself to a sense info table.  This is called by
     *   senseInfoTable() for each object connected by containment to the
     *   source object 'src', after we've fully traversed the containment
     *   tree to initialize our current-sense properties (tmpAmbient_,
     *   tmpTrans_, etc).
     *   
     *   Our job is to figure out if 'src' can sense us, and add a
     *   SenseInfo entry to the LookupTable 'tab' (which is keyed by
     *   object, hence our key is simply 'self') if 'src' can indeed sense
     *   us.
     *   
     *   Note that an object that wants to set up its own special sensory
     *   data can do so by overriding this.  This routine will only be
     *   called on objects connected to 'src' by containment, though, so if
     *   an object overrides this in order to implement a special sensory
     *   system that's outside of the normal containment model, it must
     *   somehow ensure that it gets included in the containment connection
     *   table in the first place.  
     */
    addToSenseInfoTable(sense, tab)
    {
        local trans;
        local ambient;
        local obs;

        /* 
         *   consider the appropriate set of data, depending on whether the
         *   source is looking out from within me, or in from outside of me
         */
        if (tmpPathIsIn_)
        {
            /* we're looking in from without, so use our exterior data */
            trans = tmpTrans_;
            ambient = tmpAmbient_;
            obs = tmpObstructor_;
        }
        else
        {
            /* we're looking out from within, so use our interior data */
            trans = tmpTransWithin_;
            ambient = tmpAmbientWithin_;
            obs = tmpObstructorWithin_;
        }

        /* if the source can sense us, add ourself to the data */
        if (canBeSensed(sense, trans, ambient))
            tab[self] = new SenseInfo(self, trans, obs, ambient);
    }

    /*
     *   Build a list of the objects reachable from me through the given
     *   sense and with a presence in the sense. 
     */
    sensePresenceList(sense)
    {
        local infoTab;
        
        /* get the full sense list */
        infoTab = senseInfoTable(sense);

        /* 
         *   return only the subset of items that have a presence in this
         *   sense, and return only the items themselves, not the
         *   SenseInfo objects 
         */
        return senseInfoTableSubset(infoTab,
            {obj, info: obj.(sense.presenceProp)});
    }

    /*
     *   Determine the highest ambient sense level at this object for any
     *   of the given senses.
     *   
     *   Note that this method changes certain global variables used during
     *   sense and scope calculations.  Because of this, be careful not to
     *   call this method *during* sense or scope calculations.  In
     *   particular, don't call this from an object's canBeSensed() method
     *   or anything it calls.  For example, don't call this from a
     *   Hidden.discovered method.  
     */
    senseAmbientMax(senses)
    {
        local objs;
        local maxSoFar;

        /* we don't have any level so far */
        maxSoFar = 0;
        
        /* get the table of connected objects */
        objs = connectionTable();

        /* go through each sense */
        for (local i = 1, local lst = senses, local len = lst.length() ;
             i <= len ; ++i)
        {
            /* 
             *   cache the ambient level for this sense for everything
             *   connected by containment 
             */
            cacheAmbientInfo(objs, lst[i]);

            /* 
             *   if our cached level for this sense is the highest so far,
             *   remember it 
             */
            if (tmpAmbient_ > maxSoFar)
                maxSoFar = tmpAmbient_;
        }

        /* return the highest level we found */
        return maxSoFar;
    }

    /*
     *   Cache sensory information for all objects in the given list from
     *   the point of view of self.  This caches the ambient energy level
     *   at each object, if the sense uses ambient energy, and the
     *   transparency and obstructor on the best path in the sense to the
     *   object.  'objs' is the connection table, as generated by
     *   connectionTable().  
     */
    cacheSenseInfo(objs, sense)
    {
        /* clear out the end-of-calculation notification list */
        local nlst = senseTmp.notifyList;
        if (nlst.length() != 0)
            nlst.removeRange(1, nlst.length());

        /* first, calculate the ambient energy level at each object */
        cacheAmbientInfo(objs, sense);

        /* next, cache the sense path from here to each object */
        cacheSensePath(sense);

        /* notify each object in the notification list */
        for (local i = 1, local len = nlst.length() ; i <= len ; ++i)
            nlst[i].finishSensePath(objs, sense);
    }

    /*
     *   Cache the ambient energy level at each object in the table.  The
     *   list must include everything connected by containment.  
     */
    cacheAmbientInfo(objs, sense)
    {
        local aprop;
        
        /* 
         *   if this sense has ambience, transmit energy from sources to
         *   all reachable objects; otherwise, just clear out the sense
         *   data 
         */
        if ((aprop = sense.ambienceProp) != nil)
        {
            local sources;

            /* create a vector to hold the set of ambient energy sources */
            sources = new Vector(16);

            /* 
             *   Clear out any cached sensory information from past
             *   calculations, and note objects that have ambient energy to
             *   propagate.  
             */
            objs.forEachAssoc(function(cur, val)
            {
                /* clear old sensory information for this object */
                cur.clearSenseInfo();

                /* if it's an energy source, note it */
                if (cur.(aprop) != 0)
                    sources.append(cur);
            });

            /* 
             *   Calculate the ambient energy level at each object.  To do
             *   this, start at each energy source and transmit its energy
             *   to all objects within reach of the sense. 
             */
            for (local i = 1, local len = sources.length() ; i <= len ; ++i)
            {
                /* get this item */
                local cur = sources[i];
                
                /* if this item transmits energy, process it */
                if (cur.(aprop) != 0)
                    cur.transmitAmbient(sense);
            }
        }
        else
        {
            /* 
             *   this sense doesn't use ambience - all we need to do is
             *   clear out any old sense data for this object 
             */
            objs.forEachAssoc({cur, val: cur.clearSenseInfo()});
        }
    }

    /*
     *   Transmit my radiating energy to everything within reach of the
     *   sense.  
     */
    transmitAmbient(sense)
    {
        local ambient;
        
        /* get the energy level I'm transmitting */
        ambient = self.(sense.ambienceProp);

        /* if this is greater than my ambient level so far, take it */
        if (ambient > tmpAmbient_)
        {
            /* 
             *   remember the new settings: start me with my own ambience,
             *   and with no fill medium in the way (there's nothing
             *   between me and myself, so I shine on myself with full
             *   force and with no intervening fill medium) 
             */
            tmpAmbient_ = ambient;
            tmpAmbientFill_ = nil;

            /* 
             *   if the level is at least 2, transmit to adjacent objects
             *   (level 1 is self-illumination only, so we don't transmit
             *   to anything else) 
             */
            if (ambient >= 2)
            {
                /* transmit to my containers */
                shineOnLoc(sense, ambient, nil);

                /* shine on my contents */
                shineOnContents(sense, ambient, nil);
            }
        }

        /* 
         *   Apply our interior self-illumination if necessary.  If we're
         *   self-illuminating, and our interior surface doesn't already
         *   have higher ambient energy from another source, then the
         *   ambient energy at our inner surface is simply 1, since we
         *   self-illuminate inside as well as outside. 
         */
        if (ambient == 1 && ambient > tmpAmbientWithin_)
            tmpAmbientWithin_ = ambient;
    }

    /*
     *   Transmit ambient energy to my location or locations. 
     */
    shineOnLoc(sense, ambient, fill)
    {
        /* 
         *   shine on my container, if I have one, and its immediate
         *   children 
         */
        if (location != nil)
            location.shineFromWithin(self, sense, ambient, fill);
    }

    /*
     *   Shine ambient energy at my surface onto my contents. 
     */
    shineOnContents(sense, ambient, fill)
    {
        local levelWithin;
        
        /*
         *   Figure the level of energy to transmit to my contents.  To
         *   reach my contents, the ambient energy here must traverse our
         *   surface.  Since we want to know what the ambient light at our
         *   surface looks like when viewed from our interior, we must use
         *   the sensing-out transparency. 
         */
        levelWithin = adjustBrightness(ambient, transSensingOut(sense));

        /* 
         *   Remember our ambient level at our inner surface, if the
         *   adjusted transmission is higher than our tentative ambient
         *   level already set from another source or path.  Note that the
         *   tmpAmbientWithin_ caches the ambient level at our inner
         *   surface, which comes before we adjust for our fill medium
         *   (because our fill medium is enclosed by our inner surface).  
         */
        if (levelWithin >= 2 && levelWithin > tmpAmbientWithin_)
        {
            local fillWithin;

            /* note my new interior ambience */
            tmpAmbientWithin_ = levelWithin;

            /*
             *   If there's a new fill material in my interior that the
             *   ambient energy here hasn't already just traversed, we must
             *   further adjust the ambient level in my interior by the
             *   fill transparency.  
             */
            fillWithin = tmpFillMedium_;
            if (fillWithin != fill && fillWithin != nil)
            {
                /* 
                 *   we're traversing a new fill material - adjust the
                 *   brightness further for the fill material 
                 */
                levelWithin = adjustBrightness(levelWithin,
                                               fillWithin.senseThru(sense));
            }
            
            /* if that leaves any ambience in my interior, transmit it */
            if (levelWithin >= 2)
            {
                /* shine on each object directly within me */
                for (local clst = contents, local i = 1,
                     local len = clst.length() ; i <= len ; ++i)
                {
                    /* transmit the ambient energy to this child item */
                    clst[i].shineFromWithout(self, sense,
                                             levelWithin, fillWithin);
                }
            }
        }
    }

    /*
     *   Transmit ambient energy from an object within me.  This transmits
     *   to my outer surface, and also to my own immediate children - in
     *   other words, to the peers of the child shining on us.  We need to
     *   transmit to the source's peers right now, because it might
     *   degrade the ambient energy to go out through our surface.  
     */
    shineFromWithin(fromChild, sense, ambient, fill)
    {
        local levelWithout;
        local levelWithin;
        local fillWithin;
        
        /*
         *   Calculate the change in energy as the sense makes its way to
         *   our "inner surface," and to peers of the sender - in both
         *   cases, the energy must traverse our fill medium to get to the
         *   next object.
         *   
         *   As always, energy must never traverse a single fill medium
         *   more than once consecutively, so if the last fill material is
         *   the same as the fill material here, no further adjustment is
         *   necessary for another traversal of the same material.  
         */
        levelWithin = ambient;
        fillWithin = tmpFillMedium_;
        if (fillWithin != fill && fillWithin != nil)
        {
            /* adjust the brightness for the fill traversal */
            levelWithin = adjustBrightness(levelWithin,
                                           fillWithin.senseThru(sense));
        }

        /* if there's no energy left to transmit, we're done */
        if (levelWithin < 2)
            return;

        /* 
         *   Since we're transmitting the energy from within us, calculate
         *   any attenuation as the energy goes from our inner surface to
         *   our outer surface - this is the energy that makes it through
         *   to our exterior and thus is the new ambient level at our
         *   surface.  We must calculate the attenuation that a viewer
         *   from outside sees looking at an energy source within us, so
         *   we must use the sensing-in transparency.
         *   
         *   Note that we start here with the level within that we've
         *   already calculated: we assume that the energy from our child
         *   must first traverse our interior medium before reaching our
         *   "inner surface," at which point it must then further traverse
         *   our surface material to reach our "outer surface," at which
         *   point it's the ambient level at our exterior.  
         */
        levelWithout = adjustBrightness(levelWithin, transSensingIn(sense));

        /* 
         *   The level at our outer surface is the new ambient level for
         *   this object.  The last fill material traversed is the fill
         *   material within me.  If it's the best yet, take it.
         */
        if (levelWithout > tmpAmbient_)
        {
            /* it's the best so far - cache it */
            tmpAmbient_ = levelWithout;
            tmpAmbientFill_ = fillWithin;

            /* transmit to our containers */
            shineOnLoc(sense, levelWithout, fillWithin);
        }

        /* transmit the level within to each peer of the sender */
        if (levelWithin > tmpAmbientWithin_)
        {
            /* note our level within */
            tmpAmbientWithin_ = levelWithin;

            /* transmit to each of our children */
            for (local i = 1, local clst = contents,
                 local len = clst.length() ; i <= len ; ++i)
            {
                /* get the current item */
                local cur = clst[i];

                /* if it's not the source, shine on it */
                if (cur != fromChild)
                    cur.shineFromWithout(self, sense,
                                         levelWithin, fillWithin);
            }
        }
    }

    /*
     *   Transmit ambient energy from an object immediately containing me.
     */
    shineFromWithout(fromParent, sense, level, fill)
    {
        /* if this is the best level yet, take it and transmit it */
        if (level > tmpAmbient_)
        {
            /* cache this new best level */
            tmpAmbient_ = level;
            tmpAmbientFill_ = fill;

            /* transmit it down to my children */
            shineOnContents(sense, level, fill);
        }
    }

    /*
     *   Cache the sense path for each object reachable from this point of
     *   view.  Fills in tmpTrans_ and tmpObstructor_ for each object with
     *   the best transparency path from the object to me.  
     */
    cacheSensePath(sense)
    {
        /* the view from me to myself is unobstructed */
        tmpPathIsIn_ = true;
        tmpTrans_ = transparent;
        tmpTransWithin_ = transparent;
        tmpObstructor_ = nil;
        tmpObstructorWithin_ = nil;

        /* build a path to my containers */
        sensePathToLoc(sense, transparent, nil, nil);

        /* build a path to my contents */
        sensePathToContents(sense, transparent, nil, nil);
    }

    /*
     *   Build a path to my location or locations 
     */
    sensePathToLoc(sense, trans, obs, fill)
    {
        /* 
         *   proceed to my container, if I have one, and its immediate
         *   children 
         */
        if (location != nil)
            location.sensePathFromWithin(self, sense, trans, obs, fill);
    }

    /*
     *   Build a sense path to my contents 
     */
    sensePathToContents(sense, trans, obs, fill)
    {
        local transWithin;
        local obsWithin;
        local fillWithin;

        /*
         *   Figure the transparency to my contents.  To reach my
         *   contents, we must look in through our surface.  If we change
         *   the transparency, we're the new obstructor.  
         */
        transWithin = transparencyAdd(trans, transSensingIn(sense));
        obsWithin = (trans == transWithin ? obs : self);

        /*
         *   If there's a new fill material in my interior that we haven't
         *   already just traversed, we must further adjust the
         *   transparency by the fill transparency. 
         */
        fillWithin = tmpFillMedium_;
        if (fillWithin != fill && fillWithin != nil)
        {
            local oldTransWithin = transWithin;
            
            /* we're traversing a new fill material */
            transWithin = transparencyAdd(transWithin,
                                          fillWithin.senseThru(sense));
            if (transWithin != oldTransWithin)
                obsWithin = fill;
        }

        /* if the path isn't opaque, proceed to my contents */
        if (transWithin != opaque)
        {
            /* build a path to each child */
            for (local clst = contents,
                 local i = 1, local len = clst.length() ; i <= len ; ++i)
            {
                /* build a path to this child */
                clst[i].sensePathFromWithout(self, sense, transWithin,
                                             obsWithin, fillWithin);
            }
        }
    }

    /*
     *   Build a path from an object within me. 
     */
    sensePathFromWithin(fromChild, sense, trans, obs, fill)
    {
        local transWithin;
        local fillWithin;
        local transWithout;
        local obsWithout;

        /* 
         *   Calculate the transparency change along the path from the
         *   child to our "inner surface" and to peers of the sender - in
         *   both cases, we must traverse the fill material. 
         *   
         *   As always, energy must never traverse a single fill medium
         *   more than once consecutively, so if the last fill material is
         *   the same as the fill material here, no further adjustment is
         *   necessary for another traversal of the same material.  
         */
        transWithin = trans;
        fillWithin = tmpFillMedium_;
        if (fillWithin != fill && fillWithin != nil)
        {
            /* adjust for traversing a new fill material */
            transWithin = transparencyAdd(transWithin,
                                          fillWithin.senseThru(sense));
            if (transWithin != trans)
                obs = fillWithin;
        }

        /* if we're opaque at this point, we're done */
        if (transWithin == opaque)
            return;

        /*
         *   Calculate the transparency going from our inner surface to
         *   our outer surface - we must traverse our own material to
         *   travel this segment. 
         */
        transWithout = transparencyAdd(transWithin, transSensingOut(sense));
        obsWithout = (transWithout != transWithin ? self : obs);

        /*
         *   We now have the path to our outer surface.  The last fill
         *   material traversed is the fill material within me.  If this
         *   is the best yet, remember it.  
         */
        if (transparencyCompare(transWithout, tmpTrans_) > 0)
        {
            /* it's the best so far - cache it */
            tmpTrans_ = transWithout;
            tmpObstructor_ = obsWithout;

            /* we're coming to this object from within */
            tmpPathIsIn_ = nil;

            /* transmit to our containers */
            sensePathToLoc(sense, transWithout, obsWithout, fillWithin);
        }

        /* 
         *   if this is the best interior transparency yet, build a path
         *   to each peer of the sender 
         */
        if (transparencyCompare(transWithin, tmpTransWithin_) > 0)
        {
            /* it's the best so far - cache it */
            tmpTransWithin_ = transWithin;
            tmpObstructorWithin_ = obs;

            /* we're coming to this object from within */
            tmpPathIsIn_ = nil;
            
            /* build a path to each peer of the sender */
            for (local i = 1, local clst = contents,
                 local len = clst.length() ; i <= len ; ++i)
            {
                /* get this item */
                local cur = clst[i];
                
                /* if it's not the source, build a path to it */
                if (cur != fromChild)
                    cur.sensePathFromWithout(self, sense, transWithin,
                                             obs, fillWithin);
            }
        }
    }

    /*
     *   Build a path from an object immediately containing me. 
     */
    sensePathFromWithout(fromParent, sense, trans, obs, fill)
    {
        /* if this is the best level yet, take it and keep going */
        if (transparencyCompare(trans, tmpTrans_) > 0)
        {
            /* remember this new best level */
            tmpTrans_ = trans;
            tmpObstructor_ = obs;

            /* we're coming to this object from outside */
            tmpPathIsIn_ = true;

            /* build a path down into my children */
            sensePathToContents(sense, trans, obs, fill);
        }
    }
    

    /*
     *   Clear the sensory scratch-pad properties, in preparation for a
     *   sensory calculation pass. 
     */
    clearSenseInfo()
    {
        tmpPathIsIn_ = true;
        tmpAmbient_ = 0;
        tmpAmbientWithin_ = 0;
        tmpAmbientFill_ = nil;
        tmpTrans_ = opaque;
        tmpTransWithin_ = opaque;
        tmpObstructor_ = nil;
        tmpObstructorWithin_ = nil;

        /* pre-calculate my fill medium */
        tmpFillMedium_ = fillMedium();
    }

    /* 
     *   Scratch-pad for calculating ambient energy level - valid only
     *   after calcAmbience and until the game state changes in any way.
     *   This is for internal use within the sense propagation methods
     *   only.  
     */
    tmpAmbient_ = 0

    /*
     *   Last fill material traversed by the ambient sense energy in
     *   tmpAmbient_.  We must keep track of this so that we can treat
     *   consecutive traversals of the same fill material as equivalent to
     *   a single traversal.  
     */
    tmpAmbientFill_ = nil

    /*
     *   Scrach-pad for the best transparency level to this object from
     *   the current point of view.  This is used during cacheSenseInfo to
     *   keep track of the sense path to this object. 
     */
    tmpTrans_ = opaque

    /*
     *   Scratch-pad for the obstructor that contributed to a
     *   non-transparent path to this object in tmpTrans_. 
     */
    tmpObstructor_ = nil

    /*
     *   Scratch-pads for the ambient level, best transparency, and
     *   obstructor to our *interior* surface.  We keep track of these
     *   separately from the exterior data so that we can tell what we
     *   look like from the persepctive of an object within us.  
     */
    tmpAmbientWithin_ = 0
    tmpTransWithin_ = opaque
    tmpObstructorWithin_ = nil

    /*
     *   Scratch-pad for the sense path direction at this object.  If this
     *   is true, the sense path is pointing inward - that is, the path
     *   from the source object to here is entering from outside me.
     *   Otherwise, the sense path is pointing outward.  
     */
    tmpPathIsIn_ = true

    /*
     *   My fill medium.  We cache this during each sense path
     *   calculation, since the fill medium calculation often requires
     *   traversing several containment levels. 
     */
    tmpFillMedium_ = nil

    /*
     *   Merge two senseInfoTable tables.  Merges the second table into
     *   the first.  If an object appears only in the first table, the
     *   entry is left unchanged; if an object appears only in the second
     *   table, the entry is added to the first table.  If an object
     *   appears in both tables, we'll keep the one with better detail or
     *   brightness, adding it to the first table if it's the one in the
     *   second table.  
     */
    mergeSenseInfoTable(a, b)
    {
        /* if either table is nil, return the other table */
        if (a == nil)
            return b;
        else if (b == nil)
            return a;
        
        /* 
         *   iterate over the second table, merging each item from the
         *   second table into the corresponding item in the first table 
         */
        b.forEachAssoc({obj, info: a[obj] = mergeSenseInfo(a[obj], info)});

        /* return the merged first table */
        return a;
    }

    /*
     *   Merge two SenseInfo items.  Chooses the "better" of the two items
     *   and returns it, where "better" is defined as more transparent,
     *   or, transparencies being equal, brighter in ambient energy.  
     */
    mergeSenseInfo(a, b)
    {
        /* if one or the other is nil, return the non-nil one */
        if (a == nil)
            return b;
        if (b == nil)
            return a;

        /*
         *   Both items are non-nil, so keep the better of the two.  If
         *   the transparencies aren't equal, keep the one that's more
         *   transparent.  Otherwise, keep the one with higher ambient
         *   energy. 
         */
        if (a.trans == b.trans)
        {
            /* 
             *   The transparencies are equal, so choose the one with
             *   higher ambient energy.  If those are the same,
             *   arbitrarily keep the first one. 
             */
            if (a.ambient >= b.ambient)
                return a;
            else
                return b;
        }
        else
        {
            /* 
             *   The transparencies are unequal, so pick the one with
             *   better transparency. 
             */
            if (transparencyCompare(a.trans, b.trans) < 0)
                return b;
            else
                return a;
        }
    }

    /*
     *   Receive notification that a command is about to be performed.
     *   This is called on each object connected by containment with the
     *   actor performing the command, and on any objects explicitly
     *   registered with the actor, the actor's location and its locations
     *   up to the outermost container, or the directly involved objects.  
     */
    beforeAction()
    {
        /* by default, do nothing */
    }

    /*
     *   Receive notification that a command has just been performed.
     *   This is called by the same rules as beforeAction(), but under the
     *   conditions prevailing after the command has been completed. 
     */
    afterAction()
    {
        /* by default, do nothing */
    }

    /*
     *   Receive notification that a traveler (an actor or a vehicle, for
     *   example) is about to depart via travelerTravelTo(), OR that an
     *   actor is about to move among nested locations via travelWithin().
     *   This notification is sent to each object connected to the traveler
     *   by containment, just before the traveler departs.
     *   
     *   If the traveler is traveling between top-level locations,
     *   'connector' is the TravelConnector object being traversed.  If an
     *   actor is merely moving between nested locations, 'connector' will
     *   be nil.  
     */
    beforeTravel(traveler, connector) { /* do nothing by default */ }

    /*
     *   Receive notification that a traveler has just arrived via
     *   travelerTravelTo().  This notification is sent to each object
     *   connected to the traveler by containment, in its new location,
     *   just after the travel completes. 
     */
    afterTravel(traveler, connector) { /* do nothing by default */ }

    /*
     *   Get my notification list - this is a list of objects on which we
     *   must call beforeAction and afterAction when this object is
     *   involved in a command as the direct object, indirect object, or
     *   any addition object (other than as the actor performing the
     *   command).
     *   
     *   The general notification mechanism always includes in the
     *   notification list all of the objects connected by containment to
     *   the actor; this method allows for explicit registration of
     *   additional objects that must be notified when commands are
     *   performed on this object even when the other objects are nowhere
     *   nearby.  
     */
    getObjectNotifyList()
    {
        /* return our registration list */
        return objectNotifyList;
    }

    /*
     *   Add an item to our registered notification list for actions
     *   involving this object as the direct object, indirect object, and
     *   so on.
     *   
     *   Items can be added here if they must be notified of actions
     *   involving this object regardless of the physical proximity of
     *   this item and the notification item.  
     */
    addObjectNotifyItem(obj)
    {
        objectNotifyList += obj;
    }

    /* remove an item from the registered notification list */
    removeObjectNotifyItem(obj)
    {
        objectNotifyList -= obj;
    }

    /* our list of registered notification items */
    objectNotifyList = []

    /* -------------------------------------------------------------------- */
    /*
     *   Verify a proposed change of location of this object from its
     *   current container hierarchy to the given new container.  We'll
     *   verify removal from each container up to but not including a
     *   parent that's in common with the new container - we stop upon
     *   reaching the common parent because the object isn't leaving the
     *   common parent, but merely repositioned around within it.  We'll
     *   also verify insertion into each new parent from the first
     *   non-common parent on down to the immediate new container.
     *   
     *   This routine is called any time an actor action would cause this
     *   object to be moved to a new container, so it is the common point
     *   at which to intercept any action that would attempt to move the
     *   object.  
     */
    verifyMoveTo(newLoc)
    {
        /* check removal up to the common parent */
        sendNotifyRemove(self, newLoc, &verifyRemove);

        /* check insertion into parents up to the common parent */
        if (newLoc != nil)
            newLoc.sendNotifyInsert(self, newLoc, &verifyInsert);
    }

    /*
     *   Send the given notification to each direct parent, each of their
     *   direct parents, and so forth, stopping when we reach parents that
     *   we have in common with our new location.  We don't notify parents
     *   in common with new location (or their parents) because we're not
     *   actually removing the object from the common parents.  
     */
    sendNotifyRemove(obj, newLoc, msg)
    {
        /* send notification to each container, as appropriate */
        forEachContainer(function(loc) {
            /*
             *   If this container contains the new location, don't send it
             *   (or its parents) notification, since we're not leaving it.
             *   Otherwise, send the notification and proceed to its
             *   parents.  
             */
            if (newLoc == nil
                || (loc != newLoc && !newLoc.isIn(loc)))
            {
                /* notify this container of the removal */
                loc.(msg)(obj);

                /* recursively notify this container's containers */
                loc.sendNotifyRemove(obj, newLoc, msg);
            }
        });
    }

    /*
     *   Send the given notification to each direct parent, each of their
     *   direct parents, and so forth, stopping when we reach parents that
     *   we have in common with our new location.  We don't notify parents
     *   in common with new location (or their parents).
     *   
     *   This should always be called *before* a change of location is
     *   actually put into effect, so that we will still be in our old
     *   container when this is called.  'obj' is the object being
     *   inserted, and 'newCont' is the new direct container.  
     */
    sendNotifyInsert(obj, newCont, msg)
    {
        /* 
         *   If the object is already in me, there's no need to notify
         *   myself of the insertion; otherwise, send the notification.
         *   
         *   If the object is already inside me indirectly, and we're
         *   moving it directly in me, still notify myself, since we're
         *   still picking up a new direct child.  
         */
        if (!obj.isIn(self))
        {
            /* 
             *   before we notify ourselves, notify my own parents - this
             *   sends the notifications from the outside in 
             */
            forEachContainer({loc: loc.sendNotifyInsert(obj, newCont, msg)});

            /* notify this potential container of the insertion */
            self.(msg)(obj, newCont);
        }
        else if (newCont == self && !obj.isDirectlyIn(self))
        {
            /* notify myself of the new direct insertion */
            self.(msg)(obj, self);
        }
    }

    /*
     *   Verify removal of an object from my contents or a child object's
     *   contents.  By default we allow the removal.  This is to be called
     *   during verification only, so gVerifyResult is valid when this is
     *   called.  
     */
    verifyRemove(obj)
    {
    }

    /*
     *   Verify insertion of an object into my contents.  By default we
     *   allow it, unless I'm already inside the other object.  This is to
     *   be called only during verification.  
     */
    verifyInsert(obj, newCont)
    {
        /* 
         *   If I'm inside the other object, don't allow it, since this
         *   would create circular containment. 
         */
        if (isIn(obj))
            illogicalNow(obj.circularlyInMessage, newCont, obj);
    }

    /* 
     *   my message indicating that another object x cannot be put into me
     *   because I'm already in x 
     */
    circularlyInMessage = &circularlyInMsg

    /*
     *   Receive notification that we are about to remove an object from
     *   this container.  This is normally called during the action()
     *   phase.
     *   
     *   When an object is about to be moved via moveInto(), the library
     *   calls notifyRemove on the old container tree, then notifyInsert on
     *   the new container tree, then notifyMoveInto on the object being
     *   moved.  Any of these routines can cancel the operation by
     *   displaying an explanatory message and calling 'exit'.  
     */
    notifyRemove(obj)
    {
    }

    /*
     *   Receive notification that we are about to insert a new object into
     *   this container.  'obj' is the object being moved, and 'newCont' is
     *   the new direct container (which might be a child of ours).  This
     *   is normally called during the action() phase.
     *   
     *   During moveInto(), this is called on the new container tree after
     *   notifyRemove has been called on the old container tree.  This
     *   routine can cancel the move by displaying an explanatory message
     *   and calling 'exit'.  
     */
    notifyInsert(obj, newCont)
    {
    }

    /*
     *   Receive notification that I'm about to be moved to a new
     *   container.  By default, we do nothing; subclasses can override
     *   this to do any special processing when this object is moved.  This
     *   is normally called during the action() phase.
     *   
     *   During moveInto(), this routine is called after notifyRemove() and
     *   notifyInsert() have been called on the old and new container trees
     *   (respectively).  This routine can cancel the move by displaying an
     *   explanatory message and calling 'exit'.
     *   
     *   This routine is the last in the notification sequence, so if this
     *   routine doesn't cancel the move, then the move will definitely
     *   happen (at least to the extent that we'll definitely call
     *   baseMoveInto() to carry out the move).  
     */
    notifyMoveInto(newCont)
    {
    }

    /* -------------------------------------------------------------------- */
    /*
     *   Determine if one property on this object effectively "hides"
     *   another.  This is a sort of override check for two distinct
     *   properties.
     *   
     *   We look at the object to determine where prop1 and prop2 are
     *   defined in the class hierarchy.  If prop1 isn't defined, it
     *   definitely doesn't hide prop2.  If prop2 isn't defined, prop1
     *   definitely hides it.  If both are defined, then prop1 hides prop2
     *   if and only if it is defined at a point in the class hierarchy
     *   that is "more specialized" than prop2.  That is, for prop1 to
     *   hide prop2, the class that defines prop1 must either be the same
     *   as the class that defines prop2, or the class where prop1 is
     *   defined must inherit from the class that defines prop2, or the
     *   class where prop1 is defined must be earlier in a multiple
     *   inheritance list than the class defining prop2.  
     */
    propHidesProp(prop1, prop2)
    {
        local definer1;
        local definer2;
        
        /* 
         *   get the classes in our hierarchy where the two properties are
         *   defined 
         */
        definer1 = propDefined(prop1, PropDefGetClass);
        definer2 = propDefined(prop2, PropDefGetClass);

        /* if prop1 isn't defined, it definitely doesn't hide prop2 */
        if (definer1 == nil)
            return nil;

        /* if prop1 isn't defined, prop1 definitely hides it */
        if (definer2 == nil)
            return true;

        /* 
         *   They're both defined.  If definer1 inherits from definer2,
         *   then prop1 definitely hides prop2. 
         */
        if (definer1.ofKind(definer2))
            return true;

        /* 
         *   if definer2 inherits from definer1, then prop2 hides prop1,
         *   so prop1 doesn't hide prop2 
         */
        if (definer2.ofKind(definer1))
            return nil;

        /*
         *   The two classes don't have an inheritance relation among
         *   themselves, but they might still be related in our own
         *   hierarchy by multiple inheritance.  In particular, if there
         *   is some point in the hierarchy where we have multiple base
         *   classes, and one class among the multiple bases inherits from
         *   definer1 and the other from definer2, then the one that's
         *   earlier in the multiple inheritance list is the hider. 
         */
        return superHidesSuper(definer1, definer2);
    }

    /*
     *   Determine if a given superclass of ours hides another superclass
     *   of ours, by being inherited (directly or indirectly) in our class
     *   list ahead of the other. 
     */
    superHidesSuper(s1, s2)
    {
        local lst;
        local idx1, idx2;
        
        /* get our superclass list */
        lst = getSuperclassList();

        /* if we have no superclass, there is no hiding */
        if (lst.length() == 0)
            return nil;

        /* 
         *   if we have only one element, there's obviously no hiding
         *   going on at this level, so simply traverse into the
         *   superclass to see if the hiding happens there 
         */
        if (lst.length() == 1)
            return lst[1].superHidesSuper(s1, s2);

        /*
         *   Scan the superclass list to determine the first superclass to
         *   which each of the two superclasses is related.  Stop looking
         *   when we find both superclasses or exhaust our list.  
         */
        for (local i = 1, idx1 = idx2 = nil ;
             i <= lst.length() && (idx1 == nil || idx2 == nil) ; ++i)
        {
            /* 
             *   if we haven't found s1 yet, and this superclass of ours
             *   inherits from s1, this is the earliest at which we've
             *   found s1 
             */
            if (idx1 == nil && lst[i].ofKind(s1))
                idx1 = i;

            /* likewise for the other superclass */
            if (idx2 == nil && lst[i].ofKind(s2))
                idx2 = i;
        }

        /* 
         *   if we found the two superclasses at different points in our
         *   hierarchy, the one that comes earlier hides the other; so, if
         *   idx1 is less than idx2, s1 hides s2, and if idx1 is greater
         *   than idx2, s2 hides s1 (equivalently, s1 doesn't hide s2) 
         */
        if (idx1 != idx2)
            return idx1 < idx2;

        /*
         *   We found both superclasses at the same point in our
         *   hierarchy, so they must be multiply inherited by this base
         *   class or one of its classes.  Keep looking up the hierarchy
         *   for our answer. 
         */
        return lst[idx1].superHidesSuper(s1, s2);
    }

    /* 
     *   Generic "check" failure routine.  This displays the given failure
     *   message, then performs an 'exit' to cancel the current command.
     *   'msg' is the message to display - it can be a single-quoted string
     *   with the message text, or a property pointer.  If 'msg' is a
     *   property pointer, any necessary message parameters can be supplied
     *   as additional 'params' arguments.  
     */
    failCheck(msg, [params])
    {
        reportFailure(msg, params...);
        exit;
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Examine" action 
     */
    dobjFor(Examine)
    {
        preCond = [objVisible]
        verify()
        {
            /* give slight preference to an object being held */
            if (!isIn(gActor))
                logicalRank(80, 'not held');
        }
        action()
        {
            /* 
             *   call our mainExamine method from the current actor's point
             *   of view 
             */
            fromPOV(gActor, gActor, &mainExamine);
        }
    }

    /*
     *   Main examination processing.  This is called with the current
     *   global POV set to the actor performing the 'examine' command. 
     */
    mainExamine()
    {
        /* perform the basic 'examine' action */
        basicExamine();

        /* 
         *   listen to and smell the object, but only show a message if we
         *   have an explicitly associated Noise/Odor object 
         */
        basicExamineListen(nil);
        basicExamineSmell(nil);
        
        /* show special descriptions for any contents */
        examineSpecialContents();
    }

    /*
     *   Perform the basic 'examine' action.  This shows either the normal
     *   or initial long description (the latter only if the object hasn't
     *   been moved yet, and it has a special initial examine
     *   description), and marks the object as having been described at
     *   least once.  
     */
    basicExamine()
    {
        /* check the transparency to viewing this object */
        local info = getVisualSenseInfo();
        local t = info.trans;

        /*
         *   If the viewing conditions are 'obscured' or 'distant', use
         *   the special, custom messages for those conditions.
         *   Otherwise, if the details are visible (which will be the case
         *   even for a distant or obscured object if its sightSize is set
         *   to 'large'), use the ordinary 'desc' description.  Otherwise,
         *   use an appropriate default description indicating that the
         *   object's details aren't visible.  
         */
        if (getOutermostRoom() != getPOVDefault(gActor).getOutermostRoom()
            && propDefined(&remoteDesc))
        {
            /* we're remote, so show our remote description */
            remoteDesc(getPOVDefault(gActor));
        }
        else if (t == obscured && propDefined(&obscuredDesc))
        {
            /* we're obscured, so show our special obscured description */
            obscuredDesc(info.obstructor);
        }
        else if (t == distant && propDefined(&distantDesc))
        {
            /* we're distant, so show our special distant description */
            distantDesc;
        }
        else if (canDetailsBeSensed(sight, info, getPOVDefault(gActor)))
        {
            /* 
             *   Viewing conditions and/or scale are suitable for showing
             *   full details, so show my normal long description.  Use
             *   the initDesc if desired, otherwise show the normal "desc"
             *   description.  
             */
            if (useInitDesc())
                initDesc;
            else
                desc;

            /* note that we've examined it */
            described = true;

            /* show any subclass-specific status */
            examineStatus();
        }
        else if (t == obscured)
        {
            /* 
             *   we're obscured, and we're not large enough to see the
             *   details anyway, and we don't have a special description
             *   for when we're obscured; show the default description of
             *   an obscured object 
             */
            defaultObscuredDesc(info.obstructor);
        }
        else if (t == distant)
        {
            /* 
             *   we're distant, and we're not large enough to see the
             *   details anyway, and we don't have a special distant
             *   description; show the default distant description 
             */
            defaultDistantDesc;
        }
    }

    /*
     *   Show any status associated with the object as part of the full
     *   description.  This is shown after the basicExamine() message, and
     *   is only displayed if we can see full details of the object
     *   according to the viewing conditions.
     *   
     *   By default, we simply show our contents.  Even though this base
     *   class isn't set up as a container from the player's perspective,
     *   we could contain components which are themselves containers.  So,
     *   to ensure that we properly describe any contents of our contents,
     *   we need to list our children.  
     */
    examineStatus()
    {
        /* list my contents */
        examineListContents();
    }

    /* 
     *   List my contents as part of Examine processing.  We'll recursively
     *   list contents of contents; this will ensure that even if we have
     *   no listable contents, we'll list any listable contents of our
     *   contents.  
     */
    examineListContents()
    {
        /* show our contents with our normal contents lister */
        examineListContentsWith(descContentsLister);
    }

    /* list my contents for an "examine", using the given lister */
    examineListContentsWith(lister)
    {
        /* get the actor's visual sense information */
        local tab = gActor.visibleInfoTable();

        /* mark my contents as having been seen */
        setContentsSeenBy(tab, gActor);

        /* if we don't list our contents on Examine, do nothing */
        if (!contentsListedInExamine)
            return;

        /* get my listed contents for 'examine' */
        local lst = getContentsForExamine(lister, tab);
        
        /* show my listable contents, if I have any */
        lister.showList(gActor, self, lst, ListRecurse, 0, tab, nil,
                        examinee: self);
    }

    /*
     *   Basic examination of the object for sound.  If the object has an
     *   associated noise object, we'll describe it.
     *   
     *   If 'explicit' is true, we'll show our soundDesc if we have no
     *   associated Noise object; otherwise, we'll show nothing at all
     *   unless we have a Noise object.  
     */
    basicExamineListen(explicit)
    {
        /* get my associated Noise object, if we have one */
        local obj = getNoise();

        /* 
         *   If this is not an explicit LISTEN command, only add the sound
         *   description if we have a Noise object that is not marked as
         *   "ambient."  
         */
        if (!explicit && (obj == nil || obj.isAmbient))
            return;

        /* get our sensory information from the actor's point of view */
        local info = getPOVDefault(gActor).senseObj(sound, self);
        local t = info.trans;

        /*
         *   If we have a transparent path to the object, or we have
         *   'large' sound scale, show full details.  Otherwise, show the
         *   appropriate non-detailed message for the listening conditions.
         */
        if (canDetailsBeSensed(sound, info, getPOVDefault(gActor)))
        {
            /* 
             *   We can hear full details.  If we have a Noise object, show
             *   its "listen to source" description; otherwise, show our
             *   own default sound description.  
             */
            if (obj != nil)
            {
                /* note the explicit display of the Noise description */
                obj.noteDisplay();
                
                /* show the examine-source description of the Noise */
                obj.sourceDesc;
            }
            else
            {
                /* we have no Noise, so use our own description */
                soundDesc;
            }
        }
        else if (t == obscured)
        {
            /* show our 'obscured' description */
            obscuredSoundDesc(info.obstructor);
        }
        else if (t == distant)
        {
            /* show our 'distant' description */
            distantSoundDesc;
        }

        /* 
         *   If this is an explicit LISTEN TO directed at this object, also
         *   mention any sounds we can hear from objects inside me, other
         *   than the Noise object we've explicitly mentioned, that have a
         *   sound presence.  
         */
        if (explicit)
        {
            local senseTab;
            local presenceList;

            /* get the set of objects we can hear */
            senseTab = getPOVDefault(gActor).senseInfoTable(sound);

            /* get the subset with a sound presence that are inside me */
            presenceList = senseInfoTableSubset(
                senseTab,
                {x, info: x.soundPresence && x.isIn(self) && x != obj});

            /* show the soundHereDesc for each of these */
            foreach (local cur in presenceList)
                cur.soundHereDesc();
        }
    }

    /*
     *   Basic examination of the object for odor.  If the object has an
     *   associated odor object, we'll describe it.  
     */
    basicExamineSmell(explicit)
    {
        local obj;
        local info;
        local t;

        /* get our associated Odor object, if any */
        obj = getOdor();

        /* 
         *   If this is not an explicit SMELL command, only add the odor
         *   description if we have an Odor object that is not marked as
         *   "ambient."  
         */
        if (!explicit && (obj == nil || obj.isAmbient))
            return;
        
        /* get our sensory information from the actor's point of view */
        info = getPOVDefault(gActor).senseObj(smell, self);
        t = info.trans;

        /*
         *   If we have a transparent path to the object, or we have
         *   'large' sound scale, show full details.  Otherwise, show the
         *   appropriate non-detailed message for the listening conditions.
         */
        if (canDetailsBeSensed(smell, info, getPOVDefault(gActor)))
        {
            /* if we have a Noise object, show its "listen to source" */
            if (obj != nil)
            {
                /* note the explicit display of the Odor description */
                obj.noteDisplay();
                
                /* show the examine-source description of the Odor */
                obj.sourceDesc;
            }
            else
            {
                /* we have no associated Odor; show our default description */
                smellDesc;
            }
        }
        else if (t == obscured)
        {
            /* show our 'obscured' description */
            obscuredSmellDesc(info.obstructor);
        }
        else if (t == distant)
        {
            /* show our 'distant' description */
            distantSmellDesc;
        }

        /* 
         *   If this is an explicit SMELL command directed at this object,
         *   also mention any odors we can detect from objects inside me,
         *   other than the Odor object we've explicitly mentioned, that
         *   have an odor presence.  
         */
        if (explicit)
        {
            local senseTab;
            local presenceList;

            /* get the set of objects we can smell */
            senseTab = getPOVDefault(gActor).senseInfoTable(smell);

            /* get the subset with a smell presence that are inside me */
            presenceList = senseInfoTableSubset(
                senseTab,
                {x, info: x.smellPresence && x.isIn(self) && x != obj});

            /* show the smellHereDesc for each of these */
            foreach (local cur in presenceList)
                cur.smellHereDesc();
        }
    }

    /*
     *   Basic examination of an object for taste.  Unlike the
     *   smell/listen examination routines, we don't bother using a
     *   separate sensory emanation object for tasting, as tasting is
     *   always an explicit action, never passive.  Furthermore, since
     *   tasting requires direct physical contact with the object, we
     *   don't differentiate levels of transparency or distance.  
     */
    basicExamineTaste()
    {
        /* simply show our taste description */
        tasteDesc;
    }

    /*
     *   Basic examination of an object for touch.  As with the basic
     *   taste examination, we don't use an emanation object or
     *   distinguish transparency levels, because feeling an object
     *   requires direct physical contact.  
     */
    basicExamineFeel()
    {
        /* simply show our touch description */
        feelDesc;
    }

    /*
     *   Show the special descriptions of any contents.  We'll run through
     *   the visible information list for the location; for any visible
     *   item inside me that is using its special description, we'll
     *   display the special description as a separate paragraph.  
     */
    examineSpecialContents()
    {
        /* get the actor's table of visible items */
        local infoTab = gActor.visibleInfoTable();

        /* 
         *   get the objects using special descriptions and contained
         *   within this object 
         */
        local lst = specialDescList(
            infoTab, {obj: obj.useSpecialDescInContents(self)});

        /* show the special descriptions */
        (new SpecialDescContentsLister(self)).showList(
            gActor, nil, lst, 0, 0, infoTab, nil);
    }

    /*
     *   Given a visible object info table (from Actor.visibleInfoTable()),
     *   get the list of objects, filtered by the given condition and
     *   sorted by specialDescOrder.  
     */
    specialDescList(infoTab, cond)
    {
        local lst;
        
        /* 
         *   get a list of all of the objects in the table - the objects
         *   are the keys, so we just want a list of the keys 
         */
        lst = infoTab.keysToList();

        /* subset the list for the given condition */
        lst = lst.subset(cond);

        /* 
         *   Sort the list in ascending order of specialDescOrder, and
         *   return the result.  Where the specialDescOrder is the same,
         *   and one object is inside another, put the outer object earlier
         *   in the list; this will ensure that we'll generally list
         *   objects before their contents, except when the special desc
         *   list order specifically says otherwise.  
         */
        return lst.sort(SortAsc, function(a, b) {
            /* if the list orders are different, go by list order */
            if (a.specialDescOrder != b.specialDescOrder)
                return a.specialDescOrder - b.specialDescOrder;

            /* 
             *   the list order is the same; if one is inside the other,
             *   list the outer one first 
             */
            if (a.isIn(b))
                return 1;
            else if (b.isIn(a))
                return -1;
            else
                return 0;
        });
    }
    

    /* -------------------------------------------------------------------- */
    /*
     *   "Read" 
     */
    dobjFor(Read)
    {
        preCond = [objVisible]
        verify()
        {
            /* 
             *   reduce the likelihood that they want to read an ordinary
             *   item, but allow it 
             */
            logicalRank(50, 'not readable');

            /* give slight preference to an object being held */
            if (!isIn(gActor))
                logicalRank(80, 'not held');
        }
        action()
        {
            /* simply show the ordinary description */
            actionDobjExamine();
        }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Look in" 
     */
    dobjFor(LookIn)
    {
        preCond = [objVisible]
        verify()
        {
            /* give slight preference to an object being held */
            if (!isIn(gActor))
                logicalRank(80, 'not held');
        }
        action()
        {
            /* show our LOOK IN description */
            lookInDesc;
        }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Search".  By default, we make "search obj" do the same thing as
     *   "look in obj".  
     */
    dobjFor(Search) asDobjFor(LookIn)

    /* -------------------------------------------------------------------- */
    /*
     *   "Look under" 
     */
    dobjFor(LookUnder)
    {
        preCond = [objVisible]
        verify() { }
        action() { mainReport(&nothingUnderMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Look behind" 
     */
    dobjFor(LookBehind)
    {
        preCond = [objVisible]
        verify() { }
        action() { mainReport(&nothingBehindMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Look through" 
     */
    dobjFor(LookThrough)
    {
        verify() { }
        action() { mainReport(&nothingThroughMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Listen to"
     */
    dobjFor(ListenTo)
    {
        preCond = [objAudible]
        verify() { }
        action()
        {
            /* show our "listen" description explicitly */
            fromPOV(gActor, gActor, &basicExamineListen, true);
        }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Smell"
     */
    dobjFor(Smell)
    {
        preCond = [objSmellable]
        verify() { }
        action()
        {
            /* 
             *   show our 'smell' description, explicitly showing our
             *   default description if we don't have an Odor association 
             */
            fromPOV(gActor, gActor, &basicExamineSmell, true);
        }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Taste"
     */
    dobjFor(Taste)
    {
        /* to taste an object, we have to be able to touch it */
        preCond = [touchObj]
        verify()
        {
            /* you *can* taste anything, but for most things it's unlikely */
            logicalRank(50, 'not edible');
        }
        action()
        {
            /* show our "taste" description */
            fromPOV(gActor, gActor, &basicExamineTaste);
        }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Feel" 
     */
    dobjFor(Feel)
    {
        /* to feel an object, we have to be able to touch it */
        preCond = [touchObj]
        verify() { }
        action()
        {
            /* show our "feel" description */
            fromPOV(gActor, gActor, &basicExamineFeel);
        }
    }
    

    /* -------------------------------------------------------------------- */
    /*
     *   "Take" action
     */

    dobjFor(Take)
    {
        preCond = [touchObj, objNotWorn, roomToHoldObj]
        verify()
        {
            /*      
             *   if the object is already being held by the actor, it
             *   makes no sense at the moment to take it 
             */
            if (isDirectlyIn(gActor))
            {
                /* I'm already holding it, so this is not logical */
                illogicalAlready(&alreadyHoldingMsg);
            }
            else
            {
                local carrier;
                
                /*
                 *   If the object isn't being held, it's logical to take
                 *   it.  However, rank objects being carried as less
                 *   likely than objects not being carried, because in an
                 *   ambiguous situation, it's more likely that an actor
                 *   would want to take something not being carried at all
                 *   than something already being carried inside another
                 *   object.  
                 */
                if (isIn(gActor))
                    logicalRank(70, 'already in');

                /*
                 *   If the object is being carried by another actor,
                 *   reduce the likelihood, since taking something from
                 *   another actor is usually less likely than taking
                 *   something out of one's own possessions or from the
                 *   location.  
                 */
                carrier = getCarryingActor();
                if (carrier != nil && carrier != gActor)
                    logicalRank(60, 'other owner');
            }

            /* 
             *   verify transfer from the current container hierarchy to
             *   the new container hierarchy 
             */
            verifyMoveTo(gActor);
        }
    
        action()
        {
            /* move me into the actor's direct contents */
            moveInto(gActor);

            /* issue our default acknowledgment of the command */
            defaultReport(&okayTakeMsg);
        }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Remove" processing.  We'll treat "remove dobj" as meaning "take
     *   dobj from <something>", where <something> is elided and must be
     *   determined.
     *   
     *   This should be overridden in certain cases.  For clothing,
     *   "remove dobj" should be the same as "doff dobj".  For removable
     *   components, this should imply removing the component from its
     *   container.  
     */
    dobjFor(Remove)
    {
        preCond = [touchObj, objNotWorn, roomToHoldObj]
        verify()
        {
            /* if we're already held, there's nothing to remove me from */
            if (isHeldBy(gActor))
                illogicalNow(&cannotRemoveHeldMsg);
        }
        action() { askForIobj(TakeFrom); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Take from" processing 
     */
    dobjFor(TakeFrom)
    {
        preCond = [touchObj, objNotWorn, roomToHoldObj]
        verify()
        {
            /* 
             *   we can only take something from something else if the thing
             *   is inside the other thing 
             */
            if (gIobj != nil && !self.isIn(gIobj))
                illogicalAlready(gIobj.takeFromNotInMessage);

            /* treat this otherwise like a regular "take" */
            verifyDobjTake();
        }
        check()
        {
            /* apply the same checks as for a regular "take" */
            checkDobjTake();
        }
        action()
        {
            /* we've passed our checks, so process it as a regular "take" */
            replaceAction(Take, self);
        }
    }
    iobjFor(TakeFrom)
    {
        verify()
        {
            /* check what we know about the dobj */
            if (gDobj == nil)
            {
                /* 
                 *   We haven't yet resolved the direct object; check the
                 *   tentative direct object list, and count us as
                 *   illogical if none of the possible direct objects are
                 *   in me.  
                 */
                if (gTentativeDobj.indexWhich({x: x.obj_.isIn(self)}) == nil)
                    illogicalAlready(takeFromNotInMessage);
                else if (gTentativeDobj.indexWhich(
                    {x: x.obj_.isDirectlyIn(self)}) != nil)
                    logicalRank(150, 'directly in');
            }
            else if (!gDobj.isIn(self))
            {
                /* 
                 *   the dobj isn't in me, so it's obviously not logical
                 *   to take the dobj out of me 
                 */
                illogicalAlready(takeFromNotInMessage);
            }
            else if (gDobj.isDirectlyIn(self))
            {
                /* 
                 *   it's slightly more likely that they want to remove
                 *   the object from its direct container 
                 */
                logicalRank(150, 'directly in');
            }
        }
    }

    /* general message for "take from" when an object isn't in me */
    takeFromNotInMessage = &takeFromNotInMsg

    /* -------------------------------------------------------------------- */
    /*
     *   "Drop" verb processing 
     */
    dobjFor(Drop)
    {
        preCond = [objHeld]
        verify()
        {
            /* the object must be held by the actor, at least indirectly */
            if (isIn(gActor))
            {
                /* 
                 *   it's being held, so dropping it makes sense; verify
                 *   transfer from the current container hierarchy to the
                 *   new one 
                 */
                verifyMoveTo(gActor.getDropDestination(self, nil));
            }
            else
            {
                /* it's not being held, so this is simply not logical */
                illogicalAlready(&notCarryingMsg);
            }
        }

        action()
        {
            /* send the object to the actor's drop destination */
            gActor.getDropDestination(self, nil)
                .receiveDrop(self, dropTypeDrop);
        }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Put In" verb processing.  Default objects cannot contain other
     *   objects, but they can be put in arbitrary containers.  
     */
    dobjFor(PutIn)
    {
        preCond = [objHeld]
        verify()
        {
            /* 
             *   It makes no sense to put us in a container we're already
             *   directly in.  (It's fine to put it in something it's
             *   indirectly in, though - doing so takes it out of the
             *   intermediate container and moves it directly into the
             *   indirect object.) 
             */
            if (gIobj != nil && isDirectlyIn(gIobj))
                illogicalAlready(&alreadyPutInMsg);

            /* can't put in self, obviously */
            if (gIobj == self)
                illogicalSelf(&cannotPutInSelfMsg);

            /* verify the transfer */
            verifyMoveTo(gIobj);
        }
    }
    iobjFor(PutIn)
    {
        preCond = [touchObj]
        verify()
        {
            /* by default, objects cannot be put in this object */
            illogical(&notAContainerMsg);
        }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Put On" processing.  Default objects cannot have other objects
     *   put on them, but they can be put on surfaces.
     */
    dobjFor(PutOn)
    {
        preCond = [objHeld]
        verify()
        {
            /* it makes no sense to put us on a surface we're already on */
            if (gIobj != nil && isDirectlyIn(gIobj))
                illogicalAlready(&alreadyPutOnMsg);

            /* can't put on self, obviously */
            if (gIobj == self)
                illogicalSelf(&cannotPutOnSelfMsg);

            /* verify the transfer */
            verifyMoveTo(gIobj);
        }
    }

    iobjFor(PutOn)
    {
        preCond = [touchObj]
        verify()
        {
            /* by default, objects cannot be put on this object */
            illogical(&notASurfaceMsg);
        }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "PutUnder" action 
     */
    dobjFor(PutUnder)
    {
        preCond = [objHeld]
        verify() { }
    }

    iobjFor(PutUnder)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotPutUnderMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "PutBehind" action 
     */
    dobjFor(PutBehind)
    {
        preCond = [objHeld]
        verify() { }
    }

    iobjFor(PutBehind)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotPutBehindMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Wear" action 
     */
    dobjFor(Wear)
    {
        verify() { illogical(&notWearableMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Doff" action 
     */
    dobjFor(Doff)
    {
        verify() { illogical(&notDoffableMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Kiss" 
     */
    dobjFor(Kiss)
    {
        preCond = [touchObj]
        verify() { logicalRank(50, 'not kissable'); }
        action() { mainReport(&cannotKissMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Ask for" action 
     */
    dobjFor(AskFor)
    {
        verify() { illogical(&notAddressableMsg, self); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Talk to" 
     */
    dobjFor(TalkTo)
    {
        verify() { illogical(&notAddressableMsg, self); }
    }

    /* -------------------------------------------------------------------- */
    /* 
     *   "Give to" action 
     */
    dobjFor(GiveTo)
    {
        preCond = [objHeld]
        verify()
        {
            /* 
             *   if the intended recipient already has the object, there's
             *   no point in trying this 
             */
            if (gIobj != nil && isHeldBy(gIobj))
                illogicalAlready(&giveAlreadyHasMsg);

            /* inherit any further processing */
            inherited();
        }
    }
    iobjFor(GiveTo)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotGiveToMsg); }
    }

    /* -------------------------------------------------------------------- */
    /* 
     *   "Show to" action 
     */
    dobjFor(ShowTo)
    {
        preCond = [objHeld]
        verify()
        {
            /* it's more likely that we want to show something held */
            if (isHeldBy(gActor))
            {
                /* I'm being held - use the default logical ranking */
            }
            else if (isIn(gActor))
            {
                /* 
                 *   the actor isn't hold me, but I am in the actor's
                 *   inventory, so reduce the likelihood only slightly 
                 */
                logicalRank(80, 'not held');
            }
            else
            {
                /* 
                 *   the actor isn't even carrying me, so reduce our
                 *   likelihood even more 
                 */
                logicalRank(70, 'not carried');
            }
        }
        check()
        {
            /*
             *   The direct object must be visible to the indirect object
             *   in order for the indirect object to be shown the direct
             *   object. 
             */
            if (!gIobj.canSee(self))
            {
                reportFailure(&actorCannotSeeMsg, gIobj, self);
                exit;
            }

            /*
             *   The actor performing the showing must also be visible to
             *   the indirect object, otherwise the actor wouldn't be able
             *   to attract the indirect object's attention to do the
             *   showing.  
             */
            if (!gIobj.canSee(gActor))
            {
                reportFailure(&actorCannotSeeMsg, gIobj, gActor);
                exit;
            }
        }
    }
    iobjFor(ShowTo)
    {
        verify() { illogical(&cannotShowToMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Ask about" action 
     */
    dobjFor(AskAbout)
    {
        verify() { illogical(&notAddressableMsg, self); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Tell about" action 
     */
    dobjFor(TellAbout)
    {
        verify() { illogical(&notAddressableMsg, self); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   Vague "ask" and "tell" - these are for syntactically incorrect ASK
     *   and TELL phrasings, so that we can provide better error feedback.
     */
    dobjFor(AskVague)
    {
        verify() { illogical(&askVagueMsg); }
    }
    dobjFor(TellVague)
    {
        verify() { illogical(&tellVagueMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Follow" action 
     */
    dobjFor(Follow)
    {
        verify()
        {
            /* make sure I'm followable to start with */
            if (!verifyFollowable())
                return;

            /* it makes no sense to follow myself */
            if (gActor == gDobj)
                illogical(&cannotFollowSelfMsg);

            /* ask the actor to verify following the object */
            gActor.actorVerifyFollow(self);
        }
        action()
        {
            /* ask the actor to carry out the follow */
            gActor.actorActionFollow(self);
        }
    }

    /*
     *   Verify that I'm a followable object.  By default, it's not
     *   logical to follow an arbitrary object.  If I'm not followable,
     *   this routine will generate an appropriate illogical() explanation
     *   and return nil.  If I'm followable, we'll return true.  
     */
    verifyFollowable()
    {
        illogical(&notFollowableMsg);
        return nil;
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Attack" action.
     */
    dobjFor(Attack)
    {
        preCond = [touchObj]
        verify() { }
        action() { mainReport(&uselessToAttackMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Attack with" action - attack with (presumably) a weapon.
     */
    dobjFor(AttackWith)
    {
        preCond = [touchObj]

        /* 
         *   it makes as much sense to attack any object as any other, but
         *   by default attacking an object has no effect 
         */
        verify() { }
        action() { mainReport(&uselessToAttackMsg); }
    }
    iobjFor(AttackWith)
    {
        preCond = [objHeld]
        verify() { illogical(&notAWeaponMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Throw" action.  By default, we'll simply re-route this to a
     *   "throw at" action.  Objects that can meaningfully be thrown
     *   without any specific target can override this.
     *   
     *   Note that we don't apply an preconditions or verification, since
     *   we don't really do anything with the action ourselves.  If an
     *   object overrides this, it should add any preconditions and
     *   verifications that are appropriate.  
     */
    dobjFor(Throw)
    {
        verify() { }
        action() { askForIobj(ThrowAt); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Throw <direction>".  By default, we simply reject this and
     *   explain that the command to use is THROW AT.  With one exception:
     *   we treat THROW <down> as equivalent to THROW AT FLOOR, and use the
     *   default library message for that command instead.  
     */
    dobjFor(ThrowDir)
    {
        verify()
        {
            if (gAction.getDirection() == downDirection)
                illogicalAlready(&shouldNotThrowAtFloorMsg);
        }
        action()
        {
            /* 
             *   explain that we should use THROW AT (or DROP, in the case
             *   of THROW DOWN) 
             */
            reportFailure(gAction.getDirection() == downDirection
                          ? &shouldNotThrowAtFloorMsg
                          : &dontThrowDirMsg);
        }
    }


    /* -------------------------------------------------------------------- */
    /*
     *   "Throw at" action 
     */
    dobjFor(ThrowAt)
    {
        preCond = [objHeld]
        verify()
        {
            /* by default, we can throw something if we can drop it */
            verifyMoveTo(gActor.getDropDestination(self, nil));

            /* can't throw something at itself */
            if (gIobj == self)
                illogicalSelf(&cannotThrowAtSelfMsg);

            /* can't throw dobj at iobj if iobj is in dobj */
            if (gIobj != nil && gIobj.isIn(self))
                illogicalNow(&cannotThrowAtContentsMsg);
        }
        action()
        {
            /* 
             *   process a 'throw' operation, finishing with hitting the
             *   target if we get that far 
             */
            processThrow(gIobj, &throwTargetHitWith);
        }
    }
    iobjFor(ThrowAt)
    {
        /* by default, anything can be a target */
        verify() { }
    }

    /*
     *   Process a 'throw' command.  This is common handling that can be
     *   used for any sort of throwing (throw at, throw to, throw in,
     *   etc).  The projectile is self, and 'target' is the thing we're
     *   throwing at or to.  'hitProp' is the property to call on 'target'
     *   if we reach the target.  
     */
    processThrow(target, hitProp)
    {
        local path;
        local stat;
        
        /* get the throw path */
        path = getThrowPathTo(target);
        
        /* traverse the path with throwViaPath */
        stat = traversePath(
            path, {obj, op: throwViaPath(obj, op, target, path)});
        
        /*
         *   If we made it all the way through the path without complaint,
         *   process hitting the target.  If something along the path
         *   finished the traversal (by returning nil), we'll consider the
         *   action complete - the object along that path that canceled
         *   the traversal is responsible for having displayed an
         *   appropriate message.  
         */
        if (stat)
            target.(hitProp)(self, path);
    }

    /*
     *   Carry out a 'throw' operation along a path.  'self' is the
     *   projectile; 'obj' is the path element being traversed, and 'op' is
     *   the operation being used to traverse the element.  'target' is the
     *   object we're throwing 'self' at.  'path' is the projectile's full
     *   path (in getThrowPathTo format).
     *   
     *   By default, we'll use the standard canThrowViaPath handling (which
     *   invokes the even more basic checkThrowViaPath) to determine if we
     *   can make this traversal.  If so, we'll proceed with the throw;
     *   otherwise, we'll stop the throw by calling stopThrowViaPath() and
     *   returning the result.  
     */
    throwViaPath(obj, op, target, path)
    {
        /*
         *   By default, if we can throw the object through self, return
         *   true to allow the caller to proceed; otherwise, describe the
         *   object as hitting this element and falling to the appropriate
         *   point.  
         */
        if (obj.canThrowViaPath(self, target, op))
        {
            /* no objection - allow the traversal to proceed */
            return true;
        }
        else
        {
            /* can't do it - stop the throw and return the result */
            return obj.stopThrowViaPath(self, path);
        }
    }

    /*
     *   Process the effect of throwing the object 'projectile' at the
     *   target 'self'.  By default, we'll move the projectile to the
     *   target's drop location, and display a message saying that there
     *   was no effect other than the projectile dropping to the floor (or
     *   whatever it drops to).  'path' is the path we took to reach the
     *   target, as returned from getThrowPathTo(); this lets us determine
     *   how we approached the target.  
     */
    throwTargetHitWith(projectile, path)
    {
        /* 
         *   figure out where we fall to when we hit this object, then send
         *   the object being thrown to that location 
         */
        getHitFallDestination(projectile, path)
            .receiveDrop(projectile, new DropTypeThrow(self, path));
    }

    /*
     *   Stop a 'throw' operation along a path.  'self' is the object in
     *   the path that is impassable by 'projectile' according to
     *   canThrowViaPath(), and 'path' is the getThrowPathTo-style path of
     *   objects traversed in the projectile's trajectory.
     *   
     *   The return value is taken as a path traversal continuation
     *   indicator: nil means to stop the traversal, which is to say that
     *   the 'throw' command finishes here.  If we don't really want to
     *   stop the traversal, we can return 'true' to let the traversal
     *   continue.
     *   
     *   By default, we'll stop the throw by doing the same thing we would
     *   have done if we had successfully thrown the object at 'self' - the
     *   whole reason we're stopping the throw is that we're in the way, so
     *   the effect is the same as though we were the intended target to
     *   begin with.  This is the normal handling when we can't throw
     *   through 'obj' because 'obj' is a closed container or is otherwise
     *   impassable by self when thrown.  This can be overridden to provide
     *   different handling if needed.  
     */
    stopThrowViaPath(projectile, path)
    {
        /* we've been hit */
        throwTargetHitWith(projectile, path);
        
        /* tell the caller to stop the traversal  */
        return nil;
    }
    
    /*
     *   Get the "hit-and-fall" destination for a thrown object.  This is
     *   called when we interrupt a thrown object's trajectory because
     *   we're in the way of its trajectory.
     *   
     *   For example, if the actor is inside a cage, and tries to throw a
     *   projectile at an object outside the cage, and the cage blocks the
     *   projectile's passage, then this routine is called on the cage to
     *   determine where the projectile ends up.  The projectile's ultimate
     *   destination is the hit-and-fall destination for the cage: it's
     *   where the project ends up when it hits me and then falls to the
     *   ground, its trajectory cut short.
     *   
     *   'thrownObj' is the projectile thrown, 'self' is the target object,
     *   and 'path' is the path the projectile took to reach us.  The path
     *   is of the form returned by getThrowPathTo().  Note that the path
     *   could extend beyond 'self', because the original target might have
     *   been a different object - we could simply have interrupted the
     *   projectile's course.  
     */
    getHitFallDestination(thrownObj, path)
    {
        local prvCont;
        local prvOp;
        local idx;
        local dest;
        local common;

        /* find myself in the path */
        idx = path.indexOf(self);

        /* 
         *   get the container traversed just before us in the path (it's
         *   two positions before us in the path list, because the path
         *   consists of alternating objects and operators) 
         */
        prvCont = path[idx - 2];
        
        /* get the operation that got from the last container to us */
        prvOp = path[idx - 1];

        /* 
         *   If the previous container is within us, we're throwing from
         *   inside to the outside, so the object falls within me;
         *   otherwise, the object bounces off the outside and falls
         *   outside me.
         *   
         *   If the projectile traversed *inward* from the previous item in
         *   the path, then we should simply land in the drop destination
         *   of the previous item itself, since we're coming in through
         *   that item.  If the previous item is a peer of ours, though, we
         *   traversed *across* the item, so land in our common direct
         *   container.
         *   
         *   Note that in certain cases, the previous "container" will be
         *   the projectile itself; this happens when the thrower is
         *   throwing the object at itself (as in "throw it at me").  In
         *   these cases, we'll assume that the the thrower is *holding*
         *   rather than containing the object, in which case the object
         *   isn't actually inside the thrower, in which case we want to
         *   use the location's drop destination.  
         */
        if (prvCont == thrownObj)
        {
            /* throwing object at self - land in my location's drop dest */
            dest = (location != nil
                    ? location.getDropDestination(thrownObj, path)
                    : self);
        }
        else if (prvCont.isIn(self))
        {
            /* throwing from within - land in my own drop destination */
            dest = getDropDestination(thrownObj, path);
        }
        else if (prvOp == PathPeer
                 && (common = getCommonDirectContainer(prvCont)) != nil)
        {
            /* 
             *   We're coming over from a peer, and we found a container in
             *   common with the peer.  Land in the common container's drop
             *   destination.  
             */
            dest = common.getDropDestination(thrownObj, path);
        }
        else
        {
            /*
             *   Either we're coming in from a container, or we weren't
             *   able to find a container in common with the peer.  In
             *   either case, the projectile lands in the 'drop'
             *   destination of the previous object in the path.  
             */
            dest = prvCont.getDropDestination(thrownObj, path);
        }

        /*
         *   Whatever we found, give the destination itself a chance to
         *   make any necessary adjustments.  
         */
        return dest.adjustThrowDestination(thrownObj, path);
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Throw to" action 
     */
    dobjFor(ThrowTo)
    {
        preCond = [objHeld]
        verify()
        {
            /* 
             *   by default, we can throw an object to someone if we can
             *   throw it at them 
             */
            verifyDobjThrowAt();
        }
        action()
        {
            /* 
             *   process a 'throw' operation, finishing with the target
             *   trying to catch the object if we get that far
             */
            processThrow(gIobj, &throwTargetCatch);
        }
    }

    iobjFor(ThrowTo)
    {
        verify()
        {
            /* by default, we don't want to catch anything */
            illogical(&cannotThrowToMsg);
        }
    }

    /*
     *   Process the effect of throwing the object 'obj' to the catcher
     *   'self'.  By default, we'll simply move the projectile into self.
     */
    throwTargetCatch(obj, path)
    {
        /* take the object */
        obj.moveInto(self);

        /* generate the default message if we successfully took the object */
        if (obj.isDirectlyIn(self))
            mainReport(&throwCatchMsg, obj, self);
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Dig" action - by default, simply re-reoute to dig-with, since we
     *   generally need a digging implement to dig in anything.  Some
     *   objects might want to override this to allow digging without any
     *   implement; a sandy beach, for example, might allow digging in the
     *   sand without a shovel.  
     */
    dobjFor(Dig)
    {
        preCond = [touchObj]
        verify() { }
        action() { askForIobj(DigWith); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "DigWith" action 
     */
    dobjFor(DigWith)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotDigMsg); }
    }
    iobjFor(DigWith)
    {
        preCond = [objHeld]
        verify() { illogical(&cannotDigWithMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "jump over" 
     */
    dobjFor(JumpOver)
    {
        verify() { illogical(&cannotJumpOverMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "jump off" 
     */
    dobjFor(JumpOff)
    {
        verify() { illogical(&cannotJumpOffMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Push" action 
     */
    dobjFor(Push)
    {
        preCond = [touchObj]
        verify() { logicalRank(50, 'not pushable'); }
        action() { reportFailure(&pushNoEffectMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Pull" action 
     */
    dobjFor(Pull)
    {
        preCond = [touchObj]
        verify() { logicalRank(50, 'not pullable'); }
        action() { reportFailure(&pullNoEffectMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Move" action 
     */
    dobjFor(Move)
    {
        preCond = [touchObj]
        verify() { logicalRank(50, 'not movable'); }
        action() { reportFailure(&moveNoEffectMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "MoveWith" action 
     */
    dobjFor(MoveWith)
    {
        preCond = [iobjTouchObj]
        verify() { logicalRank(50, 'not movable'); }
        action() { reportFailure(&moveNoEffectMsg); }
    }
    iobjFor(MoveWith)
    {
        preCond = [objHeld]
        verify() { illogical(&cannotMoveWithMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "MoveTo" action 
     */
    dobjFor(MoveTo)
    {
        preCond = [touchObj]
        verify() { logicalRank(50, 'not movable'); }
        action() { reportFailure(&moveToNoEffectMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Turn" action 
     */
    dobjFor(Turn)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotTurnMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Turn to" action 
     */
    dobjFor(TurnTo)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotTurnMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "TurnWith" action 
     */
    dobjFor(TurnWith)
    {
        preCond = [iobjTouchObj]
        verify() { illogical(&cannotTurnMsg); }
    }
    iobjFor(TurnWith)
    {
        preCond = [objHeld]
        verify() { illogical(&cannotTurnWithMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Set" action
     */
    dobjFor(Set)
    {
        verify() { }
        action() { askForIobj(PutOn); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "SetTo" action 
     */
    dobjFor(SetTo)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotSetToMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Consult" action 
     */
    dobjFor(Consult)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotConsultMsg); }
    }

    dobjFor(ConsultAbout)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotConsultMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Type on" action 
     */
    dobjFor(TypeOn)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotTypeOnMsg); }

        /* 
         *   if the verifier is overridden to allow typing on this object,
         *   by default just ask for a missing literal phrase, since we
         *   need something to type on this object 
         */
        action() { askForLiteral(TypeLiteralOn); }
    }

    /*
     *   "Type <literal> on" action 
     */
    dobjFor(TypeLiteralOn)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotTypeOnMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Enter on" action 
     */
    dobjFor(EnterOn)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotEnterOnMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Switch" action 
     */
    dobjFor(Switch)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotSwitchMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Flip" action 
     */
    dobjFor(Flip)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotFlipMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "TurnOn" action 
     */
    dobjFor(TurnOn)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotTurnOnMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "TurnOff" action 
     */
    dobjFor(TurnOff)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotTurnOffMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Light" action.  By default, we treat this as equivalent to
     *   "burn".  
     */
    dobjFor(Light) asDobjFor(Burn)

    /* -------------------------------------------------------------------- */
    /*
     *   "Burn".  By default, we ask for something to use to burn the
     *   object, since most objects are not self-igniting.  
     */
    dobjFor(Burn)
    {
        preCond = [touchObj]
        verify()
        {
            /* 
             *   although we can in principle burn anything, most things
             *   are unlikely choices for burning
             */
            logicalRank(50, 'not flammable');
        }
        action()
        {
            /* rephrase this as a "burn with" command */
            askForIobj(BurnWith);
        }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Burn with" 
     */
    dobjFor(BurnWith)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotBurnMsg); }
    }
    iobjFor(BurnWith)
    {
        preCond = [objHeld]
        verify() { illogical(&cannotBurnWithMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Extinguish" 
     */
    dobjFor(Extinguish)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotExtinguishMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "AttachTo" action 
     */
    dobjFor(AttachTo)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotAttachMsg); }
    }
    iobjFor(AttachTo)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotAttachToMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "DetachFrom" action 
     */
    dobjFor(DetachFrom)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotDetachMsg); }
    }
    iobjFor(DetachFrom)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotDetachFromMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Detach" action 
     */
    dobjFor(Detach)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotDetachMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Break" action 
     */
    dobjFor(Break)
    {
        preCond = [touchObj]
        verify() { illogical(&shouldNotBreakMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Cut with" action 
     */
    dobjFor(CutWith)
    {
        preCond = [touchObj]
        verify() { logicalRank(50, 'not cuttable'); }
        action() { reportFailure(&cutNoEffectMsg); }
    }

    iobjFor(CutWith)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotCutWithMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Climb", "climb up", and "climb down" actions
     */
    dobjFor(Climb)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotClimbMsg); }
    }

    dobjFor(ClimbUp)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotClimbMsg); }
    }

    dobjFor(ClimbDown)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotClimbMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Open" action 
     */
    dobjFor(Open)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotOpenMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Close" action 
     */
    dobjFor(Close)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotCloseMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Lock" action 
     */
    dobjFor(Lock)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotLockMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Unlock" action 
     */
    dobjFor(Unlock)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotUnlockMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "LockWith" action 
     */
    dobjFor(LockWith)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotLockMsg); }
    }
    iobjFor(LockWith)
    {
        preCond = [objHeld]
        verify() { illogical(&cannotLockWithMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "UnlockWith" action 
     */
    dobjFor(UnlockWith)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotUnlockMsg); }
    }
    iobjFor(UnlockWith)
    {
        preCond = [objHeld]
        verify() { illogical(&cannotUnlockWithMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Eat" action 
     */
    dobjFor(Eat)
    {
        /* 
         *   generally, an object must be held to be eaten; this can be
         *   overridden on an object-by-object basis as desired 
         */
        preCond = [objHeld]
        verify() { illogical(&cannotEatMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Drink" action 
     */
    dobjFor(Drink)
    {
        preCond = [objHeld]
        verify() { illogical(&cannotDrinkMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Pour" 
     */
    dobjFor(Pour)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotPourMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Pour into" 
     */
    dobjFor(PourInto)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotPourMsg); }
    }
    iobjFor(PourInto)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotPourIntoMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Pour onto" 
     */
    dobjFor(PourOnto)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotPourMsg); }
    }
    iobjFor(PourOnto)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotPourOntoMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Clean" action 
     */
    dobjFor(Clean)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotCleanMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "CleanWith" action 
     */
    dobjFor(CleanWith)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotCleanMsg); }
    }
    iobjFor(CleanWith)
    {
        preCond = [objHeld]
        verify() { illogical(&cannotCleanWithMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "SitOn" action 
     */
    dobjFor(SitOn)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotSitOnMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "LieOn" action 
     */
    dobjFor(LieOn)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotLieOnMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "StandOn" action 
     */
    dobjFor(StandOn)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotStandOnMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Board" action 
     */
    dobjFor(Board)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotBoardMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Get out of" (unboard) action 
     */
    dobjFor(GetOutOf)
    {
        verify() { illogical(&cannotUnboardMsg); }
    }

    /*
     *   "Get off of" action 
     */
    dobjFor(GetOffOf)
    {
        verify() { illogical(&cannotGetOffOfMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Fasten" action 
     */
    dobjFor(Fasten)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotFastenMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Fasten to" action 
     */
    dobjFor(FastenTo)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotFastenMsg); }
    }
    iobjFor(FastenTo)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotFastenToMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Unfasten" action 
     */
    dobjFor(Unfasten)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotUnfastenMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Unfasten from" action 
     */
    dobjFor(UnfastenFrom)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotUnfastenMsg); }
    }
    iobjFor(UnfastenFrom)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotUnfastenFromMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "PlugIn" action 
     */
    dobjFor(PlugIn)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotPlugInMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "PlugInto" action 
     */
    dobjFor(PlugInto)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotPlugInMsg); }
    }
    iobjFor(PlugInto)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotPlugInToMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Unplug" action 
     */
    dobjFor(Unplug)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotUnplugMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "UnplugFrom" action 
     */
    dobjFor(UnplugFrom)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotUnplugMsg); }
    }
    iobjFor(UnplugFrom)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotUnplugFromMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Screw" action 
     */
    dobjFor(Screw)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotScrewMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "ScrewWith" action 
     */
    dobjFor(ScrewWith)
    {
        preCond = [iobjTouchObj]
        verify() { illogical(&cannotScrewMsg); }
    }
    iobjFor(ScrewWith)
    {
        preCond = [objHeld]
        verify() { illogical(&cannotScrewWithMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Unscrew" action 
     */
    dobjFor(Unscrew)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotUnscrewMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "UnscrewWith" action 
     */
    dobjFor(UnscrewWith)
    {
        preCond = [iobjTouchObj]
        verify() { illogical(&cannotUnscrewMsg); }
    }
    iobjFor(UnscrewWith)
    {
        preCond = [objHeld]
        verify() { illogical(&cannotUnscrewWithMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   "Enter" 
     */
    dobjFor(Enter)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotEnterMsg); }
    }

    /* 
     *   "Go through" 
     */
    dobjFor(GoThrough)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotGoThroughMsg); }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   Push in Direction action - this is for commands like "push
     *   boulder north" or "drag sled into cave". 
     */
    dobjFor(PushTravel)
    {
        preCond = [touchObj]
        verify() { illogical(&cannotPushTravelMsg); }
    }

    /*   
     *   For all of the two-object forms, map these using our general
     *   push-travel mapping.  We do all of this mapping here, rather than
     *   in the action definition, so that individual objects can change
     *   the meanings of these verbs for special cases as appropriate.  
     */
    mapPushTravelHandlers(PushTravelThrough, GoThrough)
    mapPushTravelHandlers(PushTravelEnter, Enter)
    mapPushTravelHandlers(PushTravelGetOutOf, GetOutOf)
    mapPushTravelHandlers(PushTravelClimbUp, ClimbUp)
    mapPushTravelHandlers(PushTravelClimbDown, ClimbDown)
;

