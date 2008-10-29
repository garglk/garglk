#charset "us-ascii"

/* 
 *   Copyright (c) 2000, 2006 Michael J. Roberts.  All Rights Reserved. 
 *   
 *   TADS 3 Library: verification
 *   
 *   This module defines classes related to "verification," which is the
 *   phase of command execution where the parser attempts to determine how
 *   logical a command is.  
 */

#include "adv3.h"


/* ------------------------------------------------------------------------ */
/*
 *   Verification result class.  Verification routines return a
 *   verification result describing whether or not an action is allowed,
 *   and how much sense the command seems to make.  When a verification
 *   fails, it must include a message describing why the command isn't
 *   allowed.
 *   
 *   It is important to understand that the purpose of verification
 *   results is to guess what's in the player's mind, not to reflect the
 *   full internal state of the game.  We use verification results to
 *   figure out what a player means with a command, so if we were to rely
 *   on information the player doesn't have, we would not correctly guess
 *   the player's intentions.  So, in choosing a verification result, only
 *   information that ought to be obvious to the player should be
 *   consdidered.
 *   
 *   For example, suppose we have a closed door; suppose further that the
 *   door happens to be locked, but that there's no way for the player to
 *   see that just by looking at the door.  Now, if the player types
 *   "close door," we should return "currently illogical" - common sense
 *   tells the player that the door is something that can be opened and
 *   closed, so we wouldn't return "always illogical," but the player can
 *   plainly see that the door is already closed and thus would know that
 *   it makes no sense to close it again.  In other words, the player
 *   would conclude looking at the door that closing it is currently
 *   illogical, so that's the result we should generate.
 *   
 *   What if the player types "open door," though?  In this case, should
 *   we return "currently illogical" as well, because the door is locked?
 *   The answer is no.  We know that the command won't succeed because we
 *   know from looking at the internal game state that the door is locked,
 *   but that doesn't matter - it's what the *player* knows that's
 *   important, not what the internal game state tells us.  So, what
 *   should we return here?  It might seem strange, but the correct result
 *   is "logical" - as far as the player is concerned, the door is
 *   something that can be opened and closed, and it is currently closed,
 *   so it makes perfect sense to open it.  
 */
class VerifyResult: MessageResult
    /* 
     *   Is the action allowed?  This returns true if the command can be
     *   allowed to proceed on the basis of the verification, nil if not.  
     */
    allowAction = true

    /*
     *   Is the action allowed as an implicit action?  This returns true
     *   if the command can be allowed to proceed AND the command can be
     *   undertaken simply because it's implied by another command, even
     *   though the player never explicitly entered the command.  We
     *   distinguish this from allowAction so that we can prevent certain
     *   actions from being undertaken implicitly; we might want to
     *   disallow an implicit action when our best guess is that a player
     *   should know better than to perform an action because it's
     *   obviously dangerous.  
     */
    allowImplicit
    {
        /* 
         *   by default, any allowable action is also allowed as an
         *   implicit action 
         */
        return allowAction;
    }

    /*
     *   Am I worse than another result?  Returns true if this result is
     *   more disapproving than the other. 
     */
    isWorseThan(other)
    {
        /* I'm worse if my result ranking is lower */
        return (resultRank < other.resultRank);
    }

    /* 
     *   compare to another: negative if I'm worse than the other, zero if
     *   we're the same, positive if I'm better 
     */
    compareTo(other)
    {
        /* compare based on result rankings */
        return resultRank - other.resultRank;
    }

    /* 
     *   Determine if I should appear in a result list before the given
     *   result object.  By default, this is true if I'm worse than the
     *   given result, but some types of results use special sorting
     *   orders.  
     */
    shouldInsertBefore(other)
    {
        /* 
         *   by default, I come before the other in a result list if I'm
         *   worse than the other, because we keep result lists in order
         *   from worst to best 
         */
        return compareTo(other) < 0;
    }

    /* 
     *   Determine if I'm identical to another result.  Note that it's
     *   possible for two items to compare the same but not be identical -
     *   compareTo() is concerned only with logicalness ranking, but
     *   identicalTo() determines if the two items are exactly the same.
     *   Some subclasses (such as LogicalVerifyResult) distinguish among
     *   items that compare the same but have different reasons for their
     *   rankings.  
     */
    identicalTo(other)
    {
        /* by default, I'm identical if my comparison shows I rank the same */
        return compareTo(other) == 0;
    }
    
    /*
     *   Our result ranking relative to other results.  Each result class
     *   defines a ranking level so that we can determine whether one
     *   result is better (more approving) or worse (more disapproving)
     *   than another.
     *   
     *   To allow easy insertion of new library extension result types or
     *   game-specific result types, we assign widely spaced rankings to
     *   the pre-defined results.  This is arbitrary; the only thing that
     *   matters in comparing two results is the order of the rank values.
     */
    resultRank = nil

    /* 
     *   Should we exclude plurals from being matched, when this type of
     *   result is present?  By default, we don't; some illogical types
     *   might want to exclude plurals because the result types indicate
     *   such obvious illogicalities.  
     */
    excludePluralMatches = nil
;

/*
 *   Verification result - command is logical and allowed.
 *   
 *   This can provide additional information ranking the likelihood of the
 *   command intepretation, which can be useful to distinguish among
 *   logical but not equally likely possibilities.  For example, if the
 *   command is "take book," and the actor has a book inside his or her
 *   backpack, and there is also a book on a table in the actor's
 *   location, it would make sense to take either book, but the game might
 *   prefer to take the book on the table because it's not already being
 *   carried.  The likelihood level can be used to rank these
 *   alternatives: if the object is being carried indirectly, a lower
 *   likelihood ranking would be returned than if the object were not
 *   already somewhere in the actor's inventory.  
 */
class LogicalVerifyResult: VerifyResult
    construct(likelihoodRank, key, ord)
    {
        /* remember my likelihood ranking */
        likelihood = likelihoodRank;

        /* remember my key value */
        keyVal = key;

        /* remember my list order */
        listOrder = ord;
    }

    /* am I worse than the other result? */
    isWorseThan(other)
    {
        /* 
         *   I'm worse if my result ranking is lower; or, if we are both
         *   LogicalVerifyResult objects, I'm worse if my likelihood is
         *   lower. 
         */
        if (resultRank == other.resultRank)
            return likelihood < other.likelihood;
        else
            return inherited(other);
    }

    /* compare to another result */
    compareTo(other)
    {
        /* 
         *   if we're not both of the same rank (i.e., 'logical'), inherit
         *   the default comparison 
         */
        if (resultRank != other.resultRank)
            return inherited(other);

        /* 
         *   we're both 'logical' results, so compare based on our
         *   respective likelihoods 
         */
        return likelihood - other.likelihood;
    }

    /* determine if I go in a result list before the given result */
    shouldInsertBefore(other)
    {
        /* if we're not both of the same rank, use the default handling */
        if (resultRank != other.resultRank)
            return inherited(other);

        /* 
         *   we're both 'logical' results, so order in the list based on
         *   our priority ordering; if we're of the same priority, use the
         *   default ordering 
         */
        if (listOrder != other.listOrder)
            return listOrder < other.listOrder;
        else
            return inherited(other);
    }

    /* determine if I'm identical to another result */
    identicalTo(other)
    {
        /*
         *   I'm identical if I compare the same and my key value is the
         *   same.
         */
        return compareTo(other) == 0 && keyVal == other.keyVal;
    }

    /*
     *   The likelihood of the command - the higher the number, the more
     *   likely.  We use 100 as the default, so that there's plenty of
     *   room for specific rankings above or below the default. Particular
     *   actions might want to rank likelihoods based on action-specific
     *   factors.  
     */
    likelihood = 100

    /*
     *   Our list ordering.  This establishes how we are entered into the
     *   master results list relative to other 'logical' results.  Results
     *   are entered into the master list in ascending list order, so a
     *   lower order number means an earlier place in the list.
     *   
     *   The list ordering is more important than the likelihood ranking.
     *   Suppose we have two items: one is at list order 10 and has
     *   likelihood 100, and the other is at list order 20 and has
     *   likelihood 50.  The order of the likelihoods stored in the list
     *   will be (100, 50).  This is inverted from the normal ordering,
     *   which would put the worst item first.
     *   
     *   The point of this ordering is to allow for logical results with
     *   higher or lower importances in establishing the likelihood.  The
     *   library uses the following list order values:
     *   
     *   100 - the default ranking.  This is used in most cases.
     *   
     *   150 - secondary ranking.  This is used for rankings that aren't
     *   of great importance but which can be useful to distinguish
     *   objects in cases where no more important rankings are present.
     *   The library uses this for precondition verification rankings.  
     */
    listOrder = 100

    /* 
     *   my key value, to distinguish among different results with the
     *   same likelihood ranking 
     */
    keyVal = ''

    /* result rank - we're the most approving kind of result */
    resultRank = 100
;

/*
 *   Verification result - command is logical and allowed, but is
 *   dangerous.  As with all verify results, this should reflect our best
 *   guess as to the player's intentions, so this should only be used when
 *   it is meant to be obvious to the player that the action is dangerous.
 */
class DangerousVerifyResult: VerifyResult
    /*
     *   don't allow dangerous actions to be undertaken implicitly - we do
     *   allow these actions, but only when explicitly requested 
     */
    allowImplicit = nil

    /* result rank - we're only slightly less approving than 'logical' */
    resultRank = 90

    /* this result indicates danger */
    isDangerous = true
;

/*
 *   Verification result - command is currently illogical due to the state
 *   of the object, but might be logically applied to the object at other
 *   times.  For example, "open door" on a door that's already open is
 *   illogical at the moment, but makes more sense than opening something
 *   that has no evident way to be opened or closed to begin with.
 */
class IllogicalNowVerifyResult: VerifyResult
    /* the command isn't allowed */
    allowAction = nil

    /* result rank */
    resultRank = 40
;

/*
 *   Verification result - command is currently illogical, because the
 *   state that the command seeks to impose already obtains.  For example,
 *   we're trying to open a door that's already open, or drop an object
 *   that we're not carrying.
 *   
 *   This is almost exactly the same as an "illogical now" result, so this
 *   is a simple subclass of that result type.  We act almost the same as
 *   an "illogical now" result; the only reason to distinguish this type is
 *   that it's an especially obvious kind of condition, so we might want to
 *   use it to exclude some vocabulary matches that we wouldn't normally
 *   exclude for the more general "illogical now" result type.  
 */
class IllogicalAlreadyVerifyResult: IllogicalNowVerifyResult
    /* exclude plural matches when this result type is present */
    excludePluralMatches = true
;

/*
 *   Verification result - command is always illogical, regardless of the
 *   state of the object.  "Close fish" might fall into this category.  
 */
class IllogicalVerifyResult: VerifyResult
    /* the command isn't allowed */
    allowAction = nil

    /* result rank - this is the most disapproving of the disapprovals */
    resultRank = 30
;

/*
 *   Verification result - command is always illogical, because it's trying
 *   to use an object on itself in some invalid way, as in PUT BOX IN BOX.
 *   
 *   This is almost identical to a regular always-illogical result, so
 *   we're a simple subclass of that result type.  We distinguish these
 *   from the basic always-illogical type because it's especially obvious
 *   that the "self" kind is illogical, so we might in some cases want to
 *   exclude a vocabulary match for the "self" kind that we wouldn't
 *   exclude for the basic kind.  
 */
class IllogicalSelfVerifyResult: IllogicalVerifyResult
    /* exclude plural matches when this result type is present */
    excludePluralMatches = true
;

/*
 *   Verification result - command is logical and allowed, but is
 *   non-obvious on this object.  This should be used when the command is
 *   logical, but should not be obvious to the player.  When this
 *   verification result is present, the command is allowed when performed
 *   explicitly but will never be taken as a default.
 *   
 *   In cases of ambiguity, a non-obvious object is equivalent to an
 *   always-illogical object.  A non-obvious object *appears* to be
 *   illogical at first glance, so we want to treat it the same as an
 *   ordinarily illogical object if we're trying to choose among ambiguous
 *   objects.  
 */
class NonObviousVerifyResult: VerifyResult
    /* 
     *   don't allow non-obvious actions to be undertaken implicitly - we
     *   allow these actions, but only when explicitly requested 
     */
    allowImplicit = nil

    /* 
     *   non-obvious objects are illogical at first glance, so rank them
     *   the same as objects that are actually illogical 
     */
    resultRank = (IllogicalVerifyResult.resultRank)
;

/*
 *   Verification result - object is inaccessible.  This should be used
 *   when a command is applied to an object that is not accessibile in a
 *   sense required for the command; for example, "look at" requires that
 *   its target object be visible, so a "look at" command in the dark
 *   would fail with this type of result. 
 */
class InaccessibleVerifyResult: VerifyResult
    /* the command isn't allowed */
    allowAction = nil

    /*
     *   This ranks below any illogical result - inaccessibility is a
     *   stronger disapproval than mere illogicality.  
     */
    resultRank = 10
;

/*
 *   Default 'logical' verify result.  If a verification result list
 *   doesn't have an explicitly set result, this is the default value. 
 */
defaultLogicalVerifyResult: LogicalVerifyResult
    showMessage()
    {
        /* the default logical result has no message */
    }
    keyVal = 'default'
;

/*
 *   Verification result list.  
 */
class VerifyResultList: object
    construct()
    {
        /* initialize the results vector */
        results_ = new Vector(5);
    }

    /*
     *   Add a result to our result list.  
     */
    addResult(result)
    {
        local i;
        
        /*
         *   Find the insertion point.  We want to keep the results sorted
         *   in order from worst to best, so insert this result before the
         *   first item in our list that's better than this item. 
         */
        for (i = 1 ; i <= results_.length() ; ++i)
        {
            /* 
             *   if it's exactly the same as this item, don't add it -
             *   keep only one of each unique result 
             */
            if (result.identicalTo(results_[i]))
                return;
            
            /* 
             *   If the new result is to be inserted before the result at
             *   the current index, insert the new result at the current
             *   index.  
             */
            if (result.shouldInsertBefore(results_[i]))
                break;
        }
        
        /* add the result to our list at the index we found */
        results_.insertAt(i, result);
    }

    /* 
     *   Is the action allowed?  We return true if we have no results;
     *   otherwise, we allow the action if *all* of our results allow it,
     *   nil if even one disapproves.  
     */
    allowAction()
    {
        /* approve if the effective result approves */
        return results_.indexWhich({x: !x.allowAction}) == nil;
    }

    /*
     *   Do we exclude plural matches?  We do if we have at least one
     *   result that excludes plural matches. 
     */
    excludePluralMatches()
    {
        /* exclude plural matches if we have any result that says to */
        return results_.indexWhich({x: x.excludePluralMatches}) != nil;
    }

    /*
     *   Is the action allowed as an implicit action?  Returns true if we
     *   have no results; otherwise, returns true if *all* of our results
     *   allow the implicit action, nil if even one disapproves.  
     */
    allowImplicit()
    {
        /* search for disapprovals; if we find none, allow it */
        return results_.indexWhich({x: !x.allowImplicit}) == nil;
    }

    /*
     *   Show the message.  If I have any results, we'll show the message
     *   for the effective (i.e., most disapproving) result; otherwise we
     *   show nothing.  
     */
    showMessage()
    {
        local res;
        
        /* 
         *   Find the first result that disapproves.  Only disapprovers
         *   will have messages, so we need to find a disapprover.
         *   Entries are in ascending order of approval, and we want the
         *   most disapproving disapprover, so take the first one we find.
         */
        if ((res = results_.valWhich({x: !x.allowAction})) != nil)
            res.showMessage();
    }

    /*
     *   Get my effective result object.  If I have no explicitly-set
     *   result object, my effective result is the defaut logical result.
     *   Otherwise, we return the most disapproving result in our list.  
     */
    getEffectiveResult()
    {
        /* if our list is empty, return the default logical result */
        if (results_.length() == 0)
            return defaultLogicalVerifyResult;

        /* 
         *   return the first item in the list - we keep the list sorted
         *   from worst to best, so the first item is the most
         *   disapproving result we have 
         */
        return results_[1];
    }

    /*
     *   Compare my cumulative result (i.e., my most disapproving result)
     *   to that of another result list's cumulative result.  Returns a
     *   value suitable for sorting: -1 if I'm worse than the other one, 0
     *   if we're the same, and 1 if I'm better than the other one.  This
     *   can be used to compare the cumulative verification results for
     *   two objects to determine which object is more logical.  
     */
    compareTo(other)
    {
        local lst1;
        local lst2;
        local idx;

        /* get private copies of the two lists */
        lst1 = results_.toList();
        lst2 = other.results_.toList();

        /* keep going until we find differing items or run out of items */
        for (idx = 1 ; idx <= lst1.length() || idx <= lst2.length(); ++idx)
        {
            local a, b;
            local diff;

            /*
             *   Get the current item from each list.  If we're past the
             *   end of one or the other list, use the default logical
             *   result as the current item from that list.  
             */
            a = idx <= lst1.length() ? lst1[idx] : defaultLogicalVerifyResult;
            b = idx <= lst2.length() ? lst2[idx] : defaultLogicalVerifyResult;

            /*
             *   If the two items have distinct rankings, simply return
             *   the sense of the ranking. 
             */
            if ((diff = a.compareTo(b)) != 0)
                return diff;

            /*
             *   The two items at the current position have equivalent
             *   rankings, so ignore them for the purposes of comparing
             *   these two items.  Simply proceed to the next item in each
             *   list.  Before we do, though, check to see if we can
             *   eliminate that current item from our own list - if we
             *   have an identical item (not just ranked the same, but
             *   actually identical) in the other list, throw the item out
             *   of both lists.  
             */
            for (local j = 1 ; j < lst2.length() ; ++j)
            {
                /* 
                 *   if this item in the other list is identical to the
                 *   current item from our list, throw out both items 
                 */
                if (lst2[j].identicalTo(a))
                {
                    /* remove the items from both lists */
                    lst1 -= a;
                    lst2 -= lst2[j];

                    /* consider the new current item at this position */
                    --idx;

                    /* no need to scan any further */
                    break;
                }
            }
        }

        /* 
         *   We've run out of items in both lists, so everything must have
         *   been identical in both lists.  Since we have no 'verify' basis
         *   for preferring one object over the other, fall back on our
         *   intrinsic vocabLikelihood values as a last resort.
         */
        return obj_.obj_.vocabLikelihood - other.obj_.obj_.vocabLikelihood;
    }

    /*
     *   Determine if we match another verify result list after remapping.
     *   This determines if the other verify result is equivalent to us
     *   after considering the effects of remapping.  We'll return true if
     *   all of the following are true:
     *   
     *   - compareTo returns zero, indicating that we have the same
     *   weighting in the verification results
     *   
     *   - we refer to the same object after remapping; the effective
     *   object after remapping is our original resolved object, if we're
     *   not remapped, or our remap target if we are
     *   
     *   - we use the object for the same action and in the same role
     *   
     *   Note: this can only be called on remapped results.  Results can
     *   only be combined in the first place when remapped, so there's no
     *   need to ever call this on an unremapped result.  
     */
    matchForCombineRemapped(other, action, role)
    {
        /* if our verification values aren't identical, we can't combine */
        if (compareTo(other) != 0)
            return nil;

        /* 
         *   check the other - how we check depends on whether it's been
         *   remapped or not 
         */
        if (other.remapTarget_ == nil)
        {
            /* 
             *   The other one hasn't been remapped, so our remapped
             *   object, action, and role must match the other's original
             *   object, action, and role.  Note that to compare the two
             *   actions, we compare their baseActionClass properties,
             *   since this property gives us the identifying canonical
             *   base class for the actions.  
             */
            return (remapTarget_ == other.obj_.obj_
                    && remapAction_.baseActionClass == action.baseActionClass
                    && remapRole_ == role);
        }
        else
        {
            /*
             *   The other one has been remapped as well, so our remapped
             *   object, action, and role must match the other's remapped
             *   object, action, and role. 
             */
            return (remapTarget_ == other.remapTarget_
                    && remapAction_.baseActionClass
                       == other.remapAction_.baseActionClass
                    && remapRole_ == other.remapRole_);
        }
    }

    /*
     *   The remapped target object.  This will filled in during
     *   verification if we decide that we want to remap the nominal
     *   object of the command to a different object.  This should be set
     *   to the ultimate target object after all remappings.  
     */
    remapTarget_ = nil

    /* the action and role of the remapped action */
    remapAction_ = nil
    remapRole_ = nil

    /* our list of results */
    results_ = []

    /* 
     *   The ResolveInfo for the object being verified.  Note that this
     *   isn't saved until AFTER the verification is completed.  
     */
    obj_ = nil

    /* 
     *   The original list index for this result.  We use this when sorting
     *   a list of results to preserve the original ordering of otherwise
     *   equivalent items.  
     */
    origOrder = 0
;

