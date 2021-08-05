/* Copyright (c) 2006 by Michael J. Roberts.  All Rights Reserved. */
/*
 *   It's often handy for the player to be able to refer to objects by
 *   their current open/closed states ("go through the open door" or "look
 *   in the closed jar").  This is a small extension that adds "open" and
 *   "closed" vocabulary automatically to Openable objects, according to
 *   their current states.  It also lets the disambiguation system
 *   distinguish objects according to their open/closed state, so that the
 *   parser can ask for help accordingly ("Which do you mean, the open jar,
 *   or one of the closed jars?").
 *   
 *   The author hereby grants permission to anyone to use this software for
 *   any purpose and without fee, under the condition that anyone using
 *   this software assumes all liability for his or her use of the
 *   software.  NO WARRANTY: The author specifically disclaims all
 *   warranties with respect to this software, including all implied
 *   warranties of merchantability and fitness.  
 */

#include <adv3.h>
#include <en_us/en_us.h>

/*
 *   State objects for 'open' and 'closed' states 
 */
openState: ThingState
    stateTokens = ['open']
;
closedState: ThingState
    stateTokens = ['closed']
;

/*
 *   A disambiguation distinguisher for open/closed state.  This can tell
 *   apart two objects if they're both openable, and their open/closed
 *   states differ.
 *   
 *   This allows the parser to ask disambiguation questions based on the
 *   open/closed state when it's the only thing that distinguishes the
 *   relevant objects.  "Which do you mean, the open jar, or one of the
 *   closed jars?"
 */
openClosedDistinguisher: Distinguisher
    canDistinguish(a, b)
    {
        return a.ofKind(Openable) && b.ofKind(Openable)
            && a.isOpen != b.isOpen;
    }

    name(obj) { return obj.nameOpenClosed; }
    aName(obj) { return obj.aNameOpenClosed; }
    theName(obj) { return obj.theNameOpenClosed; }
    countName(obj, cnt) { return obj.pluralNameOpenClosed; }
;

/*
 *   In the basic openable class, add handling for state-dependent 'open'
 *   and 'closed' adjectives, and use the current state as a basis for
 *   disambiguation (via a Distinguisher).
 *   
 *   Note that we take advantage of the ThingState mechanism to help with
 *   the name matching, but we don't use the mechanism fully.  In
 *   particular, we never report openState or closedState in getState() or
 *   allStates().  The reason we don't use the full mechanism is that adv3
 *   currently only allows an object to have a single active state, and we
 *   don't want to rule out other states by using up the whole single
 *   active state slot for the open/closed status.  Hopefully this
 *   restriction will be lifted in the future, in which case we'll be able
 *   to simplify this by simply adding openState or closedState to the
 *   current state list, and then fall back on the standard
 *   matchNameCommon() handling.  
 */
modify BasicOpenable
    /* add the adjectives */
    adjective = 'open' 'closed'

    /* ...but only accept the one that applies to our current state */
    matchNameCommon(origTokens, adjustedTokens)
    {
        /* 
         *   check with our appropriate state object - if that fails, the
         *   whole match fails 
         */
        local st = (isOpen ? openState : closedState);
        if (st.matchName(self, origTokens, adjustedTokens,
                         [openState, closedState]) == nil)
            return nil;

        /* inherit the base handling */
        return inherited(origTokens, adjustedTokens);
    }

    /* use the open/closed state as a basis for disambiguation */
    distinguishers = (nilToList(inherited) + openClosedDistinguisher)

    /* open/closed names */
    nameOpenClosed = ((isOpen ? 'open ' : 'closed ') + name)
    theNameOpenClosed = ((isOpen ? 'the open ' : 'the closed ') + name)
    pluralNameOpenClosed = ((isOpen ? 'open ' : 'closed ') + name)
    aNameOpenClosed()
    {
        if (isPlural || isMassNoun)
            return (isOpen ? 'open ' : 'closed ') + name;
        else
            return (isOpen ? 'an open ' : 'a closed ') + name;
    }
;

