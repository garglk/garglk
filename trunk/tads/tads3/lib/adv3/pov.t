#charset "us-ascii"

/* 
 *   Copyright (c) 2000, 2006 Michael J. Roberts.  All Rights Reserved. 
 *   
 *   TADS 3 Library - point of view
 *   
 *   This module provides definitions related to point of view and sensory
 *   context.  When we generate output, we do so with respect to a
 *   particular point of view; different points of view can result in
 *   different output, because of the viewer's distance from an object, for
 *   example, or because of the presence of obscuring materials between the
 *   viewer and the viewed object.  We also generate output in a particular
 *   sensory context, which controls whether or not a message that
 *   describes an object with respect to a particular sense should be
 *   generated at all; for example, if the viewer can't see an object
 *   because of darkness or an obscuring layer of material, messages about
 *   the object's visual appearance should not be generated.  
 */

/* include the library header */
#include "adv3.h"


/* ------------------------------------------------------------------------ */
/*
 *   Call a function with a given sensory context.
 *   
 *   The sensory context specifies the source of any messages generated in
 *   the course of the routine we invoke and the sense which those
 *   messages use to convey information.  If the player character cannot
 *   sense the source object in the given sense, then we block all
 *   messages generated while calling this function.
 *   
 *   If the source object is nil, this establishes a neutral sense context
 *   in which all messages are visible.
 *   
 *   This can be used for processing events that are not directly
 *   initiated by the player character, such as non-player character
 *   activities or scheduled events (fuses and daemons).  The idea is that
 *   anything described in the course of calling our routine is physically
 *   associated with the source object and relates to the given sense, so
 *   if the player character cannot sense the source object, then the
 *   player should not be aware of these happenings and thus should not
 *   see the messages.
 *   
 *   Sense contexts are not nested in their effects - we will show or hide
 *   the messages that our callback routine generates regardless of
 *   whether or not messages are hidden by an enclosing sensory context.
 *   So, this routine effectively switches to the new sense context for
 *   the duration of the callback, eliminating the effect of any enclosing
 *   context.  However, we do restore the enclosing sense context before
 *   returning, so there is no lasting net effect on the global sense
 *   context.  
 */
callWithSenseContext(source, sense, func)
{
    return senseContext.withSenseContext(source, sense, func);
}

/*
 *   Sense context output filter.  When the sense context doesn't allow
 *   the player character to sense whatever's going on, we'll block all
 *   output; otherwise, we'll pass output through unchanged.  
 */
senseContext: SwitchableCaptureFilter
    /*
     *   Recalculate the current sense context.  We will check to see if
     *   the player character can sense the current sense context's source
     *   object in the current sense context's sense, and show or hide
     *   output from this point forward accordingly.  This can be called
     *   any time conditions change in such a way that the sense context
     *   should be refigured.  
     */
    recalcSenseContext()
    {
        /* 
         *   simply invalidate the cached status; this will ensure that we
         *   recalculate the status the next time we're called upon to
         *   determine whether or not we need to block output 
         */
        cached_ = nil;
    }

    /* 
     *   Get our current blocking status.  If we've already cached the
     *   status, we'll return the cached status; otherwise, we'll compute
     *   and cache the new blocking status, based on the current sensory
     *   environment. 
     */
    isBlocking()
    {
        /* if we haven't cached the status, compute the new status */
        if (!cached_)
        {
            /* calculate the new status based on the current environment */
            isBlocking_ = shouldBlock();

            /* we now have a valid cached status */
            cached_ = true;
        }

        /* return the cached status */
        return isBlocking_;
    }

    /* our current cached blocking status, and its validity */
    isBlocking_ = nil
    cached_ = true

    /*
     *   Calculate whether or not I should be blocking output according to
     *   the current game state.  Returns true if so, nil if not.  
     */
    shouldBlock()
    {
        /*
         *   Determine if the new sense context allows messages to be
         *   displayed.  If there is no source object, we allow
         *   everything; otherwise, we only allow messages if the player
         *   character can sense the source object in the given sense.  
         */
        if (source_ == nil)
        {
            /* neutral sense context - allow messages */
            return nil;
        }
        else
        {
            /* 
             *   Determine if the player character can sense the given
             *   object.  If the source can be sensed with any degree of
             *   transparency other than 'opaque', allow the messages.  
             */
            return (libGlobal.playerChar.senseObj(sense_, source_)
                    .trans == opaque);
        }
    }

    /* invoke a callback with a given sense context */
    withSenseContext(source, sense, func)
    {
        local oldSource, oldSense;

        /* remember the old sense and source values */
        oldSource = source_;
        oldSense = sense_;

        /* set up the new sense context */
        setSenseContext(source, sense);

        /* make sure we restore the old status on the way out */
        try
        {
            /* invoke the callback */
            return (func)();
        }
        finally
        {
            /* restore the old sense context */
            setSenseContext(oldSource, oldSense);
        }
    }

    /*
     *   Set a sense context. 
     */
    setSenseContext(source, sense)
    {
        /* remember the new setings */
        source_ = source;
        sense_ = sense;

        /* calculate the new sensory status */
        recalcSenseContext();
    }

    /* the source object and sense of the sensory context */
    sense_ = nil
    source_ = nil
;

/* ------------------------------------------------------------------------ */
/*
 *   Get the current point-of-view actor - this is the actor who's
 *   performing the action (LOOK AROUND, EXAMINE, SMELL, etc) that's
 *   generating the current description.  
 */
getPOVActor()
{
    return libGlobal.pointOfViewActor;
}

/*
 *   Get the current point of view.  In *most* cases, this is the same as
 *   the point-of-view actor: the actor is looking around with its own
 *   eyes, so it's the point of view.  However, this can differ from the
 *   actor when the actor is viewing the location being described through
 *   an intermediary of some kind.  For example, if an actor is observing a
 *   remote room through a closed-circuit TV system, the point of view
 *   would be the camera in the remote room (not the TV - the point of view
 *   is intended to be the object that's physically absorbing the light
 *   rays or other sensory equivalents).  
 */
getPOV()
{
    return libGlobal.pointOfView;
}

/* get the POV actor, returning the given default if there isn't one set */
getPOVActorDefault(dflt)
{
    /* start with the global setting */
    local val = libGlobal.pointOfViewActor;

    /* if that's not nil, return it; otherwise, return the default */
    return (val != nil ? val : dflt);
}

/* get the POV, returning the given default if there isn't one set */
getPOVDefault(dflt)
{
    /* start with the global setting */
    local val = libGlobal.pointOfView;

    /* if that's not nil, return it; otherwise, return the default */
    return (val != nil ? val : dflt);
}

/*
 *   Change the point of view without altering the point-of-view stack 
 */
setPOV(actor, pov)
{
    /* set the new point of view */
    libGlobal.pointOfViewActor = actor;
    libGlobal.pointOfView = pov;
}

/*
 *   Set the root point of view.  This doesn't affect the current point of
 *   view unless there is no current point of view; this merely sets the
 *   outermost default point of view.  
 */
setRootPOV(actor, pov)
{
    local stk = libGlobal.povStack;
    
    /* 
     *   if there's nothing in the stacked list, set the current point of
     *   view; otherwise, just set the innermost stacked element 
     */
    if (stk.length() == 0)
    {
        /* there is no point of view, so set the current point of view */
        libGlobal.pointOfViewActor = actor;
        libGlobal.pointOfView = pov;
    }
    else
    {
        /* set the innermost stacked point of view */
        stk[1] = pov;
        stk[2] = actor;
    }
}

/*
 *   Push the current point of view
 */
pushPOV(actor, pov)
{
    /* stack the current one */
    libGlobal.povStack.append(libGlobal.pointOfView);
    libGlobal.povStack.append(libGlobal.pointOfViewActor);

    /* set the new point of view */
    setPOV(actor, pov);
}

/*
 *   Pop the most recent point of view pushed 
 */
popPOV()
{
    local stk = libGlobal.povStack;
    local len;
    
    /* check if there's anything left on the stack */
    len = stk.length();
    if (len != 0)
    {
        /* take the most recent element off the stack */
        libGlobal.pointOfViewActor = stk[len];
        libGlobal.pointOfView = stk[len - 1];

        /* take the actor and POV objects off the stack */
        stk.removeRange(len - 1, len);
    }
    else
    {
        /* nothing on the stack - clear the point of view */
        libGlobal.pointOfViewActor = nil;
        libGlobal.pointOfView = nil;
    }
}

/*
 *   Clear the point of view and all stacked elements
 */
clearPOV()
{
    local len;
    local stk = libGlobal.povStack;
    
    /* forget the current point of view */
    setPOV(nil, nil);

    /* drop everything on the stack */
    len = stk.length();
    stk.removeRange(1, len);
}

/*
 *   Call a function from a point of view.  We'll set the new point of
 *   view, call the function with the given arguments, then restore the
 *   original point of view. 
 */
callFromPOV(actor, pov, funcToCall, [args])
{
    /* push the new point of view */
    pushPOV(actor, pov);

    /* make sure we pop the point of view no matter how we leave */
    try
    {
        /* call the function */
        (funcToCall)(args...);
    }
    finally
    {
        /* restore the enclosing point of view on the way out */
        popPOV();
    }
}


