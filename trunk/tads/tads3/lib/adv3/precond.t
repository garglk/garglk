#charset "us-ascii"

/* 
 *   Copyright (c) 2000, 2006 Michael J. Roberts.  All Rights Reserved. 
 *   
 *   TADS 3 Library: Pre-Conditions.
 *   
 *   This module defines the library pre-conditions.  A pre-condition is an
 *   abstract object that encapsulates a condition that is required to
 *   apply before a command can be executed, and optionally an implied
 *   command that can bring the condition into effect.  Pre-conditions can
 *   be associated with actions or with the objects of an action.  
 */

#include "adv3.h"


/* ------------------------------------------------------------------------ */
/*
 *   An action pre-condition object.  Each condition of an action is
 *   represented by a subclass of this class.  
 */
class PreCondition: object
    /*
     *   Check the condition on the given object (which may be nil, if
     *   this condition doesn't apply specifically to one of the objects
     *   in the command).  If it is possible to meet the condition with an
     *   implicit command, and allowImplicit is true, try to execute the
     *   command.  If the condition cannot be met, report a failure and
     *   use 'exit' to terminate the command.
     *   
     *   If allowImplicit is nil, an implicit command may not be
     *   attempted.  In this case, if the condition is not met, we must
     *   simply report a failure and use 'exit' to terminate the command.
     */
    checkPreCondition(obj, allowImplicit) { }

    /*
     *   Verify the condition.  This is called during the object
     *   verification step so that the pre-condition can add verifications
     *   of its own.  This can be used, for example, to add likelihood to
     *   objects that already meet the condition.  Note that it is
     *   generally not desirable to report illogical for conditions that
     *   checkPreCondition() enforces, because doing so will prevent
     *   checkPreCondition() from ever being reached and thus will prevent
     *   checkPreCondition() from attempting to carry out implicit actions
     *   to meet the condition.
     *   
     *   'obj' is the object being checked.  Note that because this is
     *   called during verification, the explicitly passed-in object must
     *   be used in the check rather than the current object in the global
     *   current action.  
     */
    verifyPreCondition(obj) { }

    /*
     *   Precondition execution order.  When we execute preconditions for a
     *   given action, we'll sort the list of all applicable preconditions
     *   in ascending execution order.
     *   
     *   For the most part, the relative order of two preconditions is
     *   arbitrary.  In some unusual cases, though, the order is important,
     *   such as when applying one precondition can destroy the conditions
     *   that the other would try to create but not vice versa.  When the
     *   order doesn't matter, this can be left at the default setting.  
     */
    preCondOrder = 100
;

/* ------------------------------------------------------------------------ */
/*
 *   A pre-condition that applies to a specific, pre-determined object,
 *   rather than the direct/indirect object of the command.
 */
class ObjectPreCondition: PreCondition
    construct(obj, cond)
    {
        /* 
         *   remember the specific object I act upon, and the underlying
         *   precondition to apply to that object 
         */
        obj_ = obj;
        cond_ = cond;
    }

    /* route our check to the pre-condition using our specific object */
    checkPreCondition(obj, allowImplicit)
    {
        /* check the precondition */
        return cond_.checkPreCondition(obj_, allowImplicit);
    }

    /* route our verification check to the pre-condition */
    verifyPreCondition(obj)
    {
        cond_.verifyPreCondition(obj_);
    }

    /* use the same order as our underlying condition */
    preCondOrder = (cond_.preCondOrder)

    /* the object we check with the condition */
    obj_ = nil

    /* the pre-condition we check */
    cond_ = nil
;


/* ------------------------------------------------------------------------ */
/*
 *   Pre-condition: object must be visible.  This condition doesn't
 *   attempt any implied command to make the object visible, but merely
 *   enforces visibility before allowing the command.
 *   
 *   This condition is useful for commands that rely on visibly inspecting
 *   the object, such as "examine" or "look in".  It is possible for an
 *   object to be in scope without being visible, since an object can be
 *   in scope by way of a non-visual sense.
 *   
 *   We enforce visibility with a verification test, not a precondition
 *   check.  
 */
objVisible: PreCondition
    verifyPreCondition(obj)
    {
        /* if the object isn't visible, disallow the command */
        if (obj != nil && !gActor.canSee(obj))
        {
            /*
             *   If the actor is in the dark, that must be the problem.
             *   Otherwise, if the object can be heard or smelled but not
             *   seen, say so.  In any other case, issue a generic message
             *   that we can't see the object.  
             */
            if (!gActor.isLocationLit())
                inaccessible(&tooDarkMsg);
            else if (obj.soundPresence && gActor.canHear(obj))
                inaccessible(&heardButNotSeenMsg, obj);
            else if (obj.smellPresence && gActor.canSmell(obj))
                inaccessible(&smelledButNotSeenMsg, obj);
            else
                inaccessible(&mustBeVisibleMsg, obj);
        }
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Pre-condition: object must be audible; that is, it must be within
 *   hearing range of the actor.  This condition doesn't attempt any
 *   implied command to make the object audible, but merely enforces
 *   audibility before allowing the command.
 *   
 *   It is possible for an object to be in scope without being audible,
 *   since an object can be inside a container that is transparent to
 *   light but blocks all sound.
 *   
 *   We enforce this condition with a verification test.  
 */
objAudible: PreCondition
    verifyPreCondition(obj)
    {
        /* if the object isn't audible, disallow the command */
        if (obj != nil && !gActor.canHear(obj))
            inaccessible(&cannotHearMsg, obj);
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Pre-condition: object must be within smelling range of the actor.
 *   This condition doesn't attempt any implied command to make the object
 *   smellable, but merely enforces the condition before allowing the
 *   command.
 *   
 *   It is possible for an object to be in scope without being smellable,
 *   since an object can be inside a container that is transparent to
 *   light but blocks all odors.
 *   
 *   We enforce this condition with a verification test.  
 */
objSmellable: PreCondition
    verifyPreCondition(obj)
    {
        /* if the object isn't within sense range, disallow the command */
        if (obj != nil && !gActor.canSmell(obj))
            inaccessible(&cannotSmellMsg, obj);
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Pre-condition: actor must be standing.  This is useful for travel
 *   commands to ensure that the actor is free of any entanglements from
 *   nested rooms prior to travel.  
 */
actorStanding: PreCondition
    checkPreCondition(obj, allowImplicit)
    {
        /* check to see if the actor is standing - if so, we're done */
        if (gActor.posture == standing)
            return nil;

        /* the actor isn't standing - try a "stand up" command */
        if (allowImplicit && tryImplicitAction(Stand))
        {
            /* 
             *   make sure that leaves the actor standing - if not,
             *   exit silently, since the reason for failure will have
             *   been reported by the "stand up" action 
             */
            if (gActor.posture != standing)
                exit;
            
            /* indicate that we executed an implicit command */
            return true;
        }
        
        /* we can't stand up implicitly - report the problem and exit */
        reportFailure(&mustBeStandingMsg);
        exit;
    }
;

/*
 *   Pre-condition: actor must be "travel ready."  The exact meaning of
 *   "travel ready" is provided by the actor's immediately container.  The
 *   'obj' argument is always the travel connector to be traversed.  
 */
actorTravelReady: PreCondition
    checkPreCondition(obj, allowImplicit)
    {
        local loc = gActor.location;
        
        /* check to see if the actor is standing - if so, we're done */
        if (loc.isActorTravelReady(obj))
            return nil;
        
        /* the actor isn't standing - try a "stand up" command */
        if (allowImplicit && gActor.location.tryMakingTravelReady(obj))
        {
            /* 
             *   make sure that the actor really is travel-ready now - if
             *   not, exit silently, since the reason for failure will have
             *   been reported by the implicit action 
             */
            if (!loc.isActorTravelReady(obj))
                exit;
            
            /* indicate that we executed an implicit command */
            return true;
        }
        
        /* we can't make the actor travel-ready - report failure and exit */
        reportFailure(loc.notTravelReadyMsg);
        exit;
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Pre-condition: the traveler is directly in the given room.  This will
 *   attempt to remove the traveler from any nested rooms within the given
 *   room, but cannot perform travel between rooms not related by
 *   containment.
 *   
 *   Note that the traveler is not necessarily the actor, because the actor
 *   could be in a vehicle.
 *   
 *   This is a class, because it has to be instantiated with more
 *   parameters than just a single 'obj' passed by default when evaluating
 *   preconditions.  In particular, we need to know the actor performing
 *   the travel, the connector being traversed, and the room we need to be
 *   directly in.  
 */
class TravelerDirectlyInRoom: PreCondition
    construct(actor, conn, loc)
    {
        /* remember the actor, connector, and room */
        actor_ = actor;
        conn_ = conn;
        loc_ = loc;
    }
    
    checkPreCondition(obj, allowImplicit)
    {
        /* ask the traveler to do the work */
        return actor_.getTraveler(conn_)
            .checkDirectlyInRoom(loc_, allowImplicit);
    }

    /* the actor doing the travel */
    actor_ = nil

    /* the connector being traversed */
    conn_ = nil

    /* the room we need to be directly in  */
    loc_ = nil
;

/*
 *   Pre-condition: the actor is directly in the given room.  This differs
 *   from TravelerDirectlyInRoom in that this operates directly on the
 *   actor, regardless of whether the actor is in a vehicle.  
 */
actorDirectlyInRoom: PreCondition
    checkPreCondition(obj, allowImplicit)
    {
        /* ask the actor to do the work */
        return gActor.checkDirectlyInRoom(obj, allowImplicit);
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Pre-condition: actor is ready to enter a nested location.  This is
 *   useful for commands that cause travel within a location, such as "sit
 *   on chair": this ensures that the actor is either already in the given
 *   nested location, or is in the main location; and that the actor is
 *   standing.  We simply call the actor to do the work.  
 */
actorReadyToEnterNestedRoom: PreCondition
    checkPreCondition(obj, allowImplicit)
    {
        /* ask the actor to make the determination */
        return gActor.checkReadyToEnterNestedRoom(obj, allowImplicit);
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Pre-condition: the target actor must be able to talk to the object.
 *   This is useful for actions that require communications, such as ASK
 *   ABOUT, TELL ABOUT, and TALK TO.  
 */
canTalkToObj: PreCondition
    checkPreCondition(obj, allowImplicit)
    {
        /* 
         *   if the current actor can't talk to the given object, disallow
         *   the command 
         */
        if (obj != nil && !gActor.canTalkTo(obj))
        {
            reportFailure(&objCannotHearActorMsg, obj);
            exit;
        }

        /* we don't perform any implicit commands */
        return nil;
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Pre-condition: object must be held.  This condition requires that an
 *   object of a command must be held by the actor.  If it is not, we will
 *   attempt a recursive "take" command on the object.
 *   
 *   This condition is useful for commands where the object is to be
 *   manipulated in some way, or used to manipulate some other object.
 *   For example, the key in "unlock door with key" would normally have to
 *   be held.  
 */
objHeld: PreCondition
    checkPreCondition(obj, allowImplicit)
    {
        /* if the object is already held, there's nothing we need to do */
        if (obj == nil || obj.meetsObjHeld(gActor))
            return nil;
        
        /* the object isn't being held - try an implicit 'take' command */
        if (allowImplicit && obj.tryHolding())
        {
            /* 
             *   we successfully executed the command; check to make sure
             *   it worked, and if not, abort the command without further
             *   comment (if the command failed, presumably the command
             *   showed an explanation as to why) 
             */
            if (!obj.meetsObjHeld(gActor))
                exit;

            /* tell the caller we executed an implicit command */
            return true;
        }

        /* it's not held and we can't take it - fail */
        reportFailure(&mustBeHoldingMsg, obj);

        /* make it the pronoun */
        gActor.setPronounObj(obj);

        /* abort the command */
        exit;
    }

    /* lower the likelihood rating for anything not being held */
    verifyPreCondition(obj)
    {
        /* if the object isn't being held, reduce its likelihood rating */
        if (obj != nil && !obj.meetsObjHeld(gActor))
            logicalRankOrd(80, 'implied take', 150);
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Pre-condition: a given source object must be able to touch the
 *   object.  This requires that the source object (given by our property
 *   'sourceObj') has a clear 'touch' path to the target object.
 *   
 *   This is a base class for arbitrary object-to-object touch conditions.
 *   In most cases, you'll want to use the more specific touchObj, which
 *   tests that the current actor can touch the current object.  
 */
class TouchObjCondition: PreCondition
    /* construct with a given source object */
    construct(src) { sourceObj = src; }

    /* 
     *   the source object - this is the object that is attempting to
     *   touch the target object 
     */
    sourceObj = nil

    /* check the condition */
    checkPreCondition(obj, allowImplicit)
    {
        local pastObs;
        
        /* 
         *   If we can touch the object, we can proceed with no implicit
         *   actions.
         */
        if (sourceObj.canTouch(obj))
            return nil;
        
        /* we haven't tried removing any obstructors yet */
        pastObs = new Vector(8);
        
        /*
         *   Repeatedly look for and attempt to remove obstructions.
         *   There could be multiple things in the way, so try to remove
         *   each one we find until either we fail to remove an
         *   obstruction or we run out of obstructions.  
         */
        for (;;)
        {
            local stat;
            local path;
            local result;
            local obs;

            /* get the path for reaching out and touching the object */
            path = sourceObj.getTouchPathTo(obj);

            /* if we have a path, look for an obstructor */
            if (path != nil)
            {
                /* traverse the path to find what blocks our touch */
                stat = sourceObj.traversePath(path, function(ele, op)
                {
                    /*
                     *   If we can continue the reach via this path element,
                     *   simply keep going.  Otherwise, stop the reach here. 
                     */
                    result = ele.checkTouchViaPath(sourceObj, obj, op);
                    if (result.isSuccess)
                    {
                        /* no objection here - keep going */
                        return true;
                    }
                    else
                    {
                        /* stop here, noting the obstruction */
                        obs = ele;
                        return nil;
                    }
                });

                /* 
                 *   if we now have a clear path, we're done - simply return
                 *   true to indicate that we ran one or more implicit
                 *   commands 
                 */
                if (stat)
                    return true;
            }
            else
            {
                /* 
                 *   we have no path, so the object must be in an
                 *   unconnected location; we don't know the obstructor in
                 *   this case 
                 */
                obs = nil;
            }

            /*
             *   'result' is a CheckStatus object explaining why we can't
             *   reach past 'obs', which is the first object that
             *   obstructs our reach.
             *   
             *   If the obstructor is not visible or we couldn't find one,
             *   we can't do anything to try to remove it; simply report
             *   that we can't reach the target object and give up.  
             */
            if (obs == nil || !gActor.canSee(obs))
            {
                reportFailure(&cannotReachObjectMsg, obj);
                exit;
            }

            /* 
             *   Ask the obstructor to get out of the way if possible.
             *   
             *   If we've already tried to remove this same obstructor on
             *   a past iteration, don't try again, as there's no reason
             *   to think an implicit command will work any better this
             *   time.
             */
            if (pastObs.indexOf(obs) != nil
                || !allowImplicit
                || !obs.tryImplicitRemoveObstructor(touch, obj))
            {
                /* 
                 *   We can't remove the obstruction - either we've tried
                 *   an implicit command on this same obstructor and
                 *   failed, or we can't try an implicit command at all.
                 *   In any case, use the explanation of the problem from
                 *   the CheckStatus result object.  
                 */
                reportFailure(result.msgProp, result.msgParams...);
                exit;
            }

            /* 
             *   if the implied command failed, simply give up now -
             *   there's no need to go on, since the implied command will
             *   have already explained why it failed 
             */
            if (gTranscript.currentActionHasReport({x: x.isFailure}))
                exit;

            /* 
             *   We've tried an implied command to remove this obstructor,
             *   but that isn't guaranteed to make the target touchable,
             *   as there could be further obstrutions, or the implied
             *   command could have failed to actually remove the
             *   obstruction.  Keep iterating.  To avoid looping forever
             *   in the event the implicit command we just tried isn't
             *   good enough to remove this obstruction, make a note of
             *   the obstruction we just tried to remove; if we find it
             *   again on a subsequent iteration, we'll know that we've
             *   tried before to remove it and failed, and thus we'll know
             *   to give up without making the same doomed attempt again. 
             */
            pastObs.append(obs);
        }
    }

    verifyPreCondition(obj)
    {
        /*
         *   If there's no source object, do nothing at this point.  We can
         *   have a nil source object when we're resolving nouns for a
         *   two-object action, and we have a cross-object condition (for
         *   example, we require that the indirect object can touch the
         *   direct object).  In these cases, when we're resolving the
         *   first-resolved noun phrase, the second-resolved noun phrase
         *   won't be known yet.  The only purpose of verification at times
         *   like these is to improve our guess about an ambiguous match,
         *   but we have nothing to add at such times, so we can simply
         *   return without doing anything.  
         */
        if (sourceObj == nil)
            return;

        /* if we can't touch the object, make it less likely */
        if (!sourceObj.canTouch(obj))
        {
            /* 
             *   If we can't see the object, we must be able to sense it
             *   by some means other than sight, so it must have a
             *   sufficiently distinctive sound or odor to put it in
             *   scope.  Explain this: "you can hear it but you can't see
             *   it", or the like.  
             */
            if (gActor.canSee(obj))
            {
                local info;
                
                /* 
                 *   It's visible but cannot be reached from here, so it
                 *   must be too far away, inside a closed but transparent
                 *   container, or something like that.
                 *   
                 *   If it's at a distance, rule it illogical, since
                 *   there's not usually anything automatic we can do to
                 *   remove the distance obstruction.
                 *   
                 *   If it's not distant, don't rule it illogical, but do
                 *   reduce the likelihood ranking, so that we'll prefer a
                 *   different object that can readily be touched.  Since
                 *   we can see where the object is, we might know how to
                 *   remove the obstruction to reachability.  
                 */
                info = gActor.bestVisualInfo(obj);
                if (info != nil && info.trans == distant)
                {
                    /* it's distant - assume we can't fix this */
                    inaccessible(&tooDistantMsg, obj);
                }
                else
                {
                    /* 
                     *   it's not distant; rank it logical (since we might
                     *   be able to clear the obstruction with an implied
                     *   action), but at reduced likelihood (in case
                     *   there's something that doesn't need any prior
                     *   implied action to reach) 
                     */
                    logicalRankOrd(80, 'unreachable but visible', 150);
                }
            }
            else
            {
                /* 
                 *   if it has a sound presence, then "you can hear it but
                 *   you can't see it"; if it has a smell presence, then
                 *   "you can smell it but you can't see it"; otherwise,
                 *   you simply can't see it 
                 */
                if (obj.soundPresence && gActor.canHear(obj))
                {
                    /* it can be heard but not seen */
                    inaccessible(&heardButNotSeenMsg, obj);
                }
                else if (obj.smellPresence && gActor.canSmell(obj))
                {
                    /* it can be smelled but not seen */
                    inaccessible(&smelledButNotSeenMsg, obj);
                }
                else if (!gActor.isLocationLit())
                {
                    /* it's too dark to see the object */
                    inaccessible(&tooDarkMsg);
                }
                else
                {
                    /* it simply cannot be seen */
                    inaccessible(&mustBeVisibleMsg, obj);
                }
            }
        }
    }

    /*
     *   This condition tends to be fragile, in the sense that other
     *   preconditions for the same action have the potential to undo any
     *   implicit action that we perform to make an object touchable.  This
     *   is most likely to happen when we implicitly move the actor (moving
     *   in or out of a nested room, for example) to put the actor within
     *   reach of the target object.  To reduce the likelihood that this
     *   fragility will be visible to a player, try to execute this
     *   condition after other conditions.  Most other preconditions tend
     *   to be "stickier" - less likely to be undone by subsequent
     *   preconditions.  
     */
    preCondOrder = 200
;

/* ------------------------------------------------------------------------ */
/*
 *   Pre-condition: actor must be able to touch the object.  This doesn't
 *   require that the actor is actually holding the object, but the actor
 *   must be able to physically touch the object.  This ensures that the
 *   actor and object are not, for example, separated by a transparent
 *   barrier.
 *   
 *   If there is a transparent barrier, we will attempt to remove the
 *   barrier by calling the barrier object's tryImplicitRemoveObstructor
 *   method.  Objects that can be opened in an obvious fashion will
 *   perform an implicit recursive "open" command, and other types of
 *   objects can provide customized behavior as appropriate.  
 */
touchObj: TouchObjCondition
    /* we want to test reaching from the current actor to the target object */
    sourceObj = (gActor)
;

/*
 *   Pre-condition: the indirect object must be able to touch the target
 *   object.  This can be used for actions where the direct object is going
 *   to be manipulated by an "agent" of the action (i.e., the indirect
 *   object), rather than directly by the actor: MOVE X WITH Y, for
 *   example.
 *   
 *   Note that the target object of this condition should be the direct
 *   object in most cases, so this condition should usually be used like
 *   this:
 *   
 *   dobjFor(MoveWith) { preCond = [iobjTouchObj] }
 *   
 *   In other words, this is a precondition that we apply in most cases to
 *   the *direct* object.  
 */
iobjTouchObj: TouchObjCondition
    /* the indirect object has to be able to touch the target object */
    sourceObj = (gIobj)
;

/*
 *   Pre-condition: the direct object can touch the target object.  This
 *   is useful for situations where the direct object is being manipulated
 *   directly and the indirect object is more of a passive participant in
 *   the action, such as PLUG CORD INTO OUTLET.
 */
dobjTouchObj: TouchObjCondition
    /* the direct object has to be able to touch the target object */
    sourceObj = (gDobj)
;

/* ------------------------------------------------------------------------ */
/*
 *   A precondition ensuring that the target object is in the same
 *   immediate location as a given object.
 */
class SameLocationCondition: PreCondition
    /* 
     *   construct dynamically, setting the other object whose location we
     *   must match 
     */
    construct(obj) { sourceObj = obj; }

    /* the object whose location we must match */
    sourceObj = nil

    /* check the condition */
    checkPreCondition(obj, allowImplicit)
    {
        local moveObj;
        local targetLoc;
        
        /* if we're in the same container, we're fine */
        if (obj.location == sourceObj.location)
            return nil;

        /* 
         *   Pick an object to move.  By default, pick the target object;
         *   but if the target object is non-portable, then trying to move
         *   it will fail, so pick the source object.  
         */
        if (obj.ofKind(NonPortable))
        {
            /* 'obj' is unportable, so try moving the source object */
            moveObj = sourceObj;
            targetLoc = obj.location;
        }
        else
        {
            /* 'obj' is portable, so try moving it by default */
            moveObj = obj;
            targetLoc = sourceObj.location;
        }

        /* try moving the object, and return the result */
        if (allowImplicit && targetLoc.tryMovingObjInto(moveObj))
        {
            /* if it didn't work, abort the action */
            if (obj.location != sourceObj.location)
                exit;

            /* tell the caller we executed an implied action */
            return true;
        }

        /* we can't move it - report the failure and abort the action */
        targetLoc.mustMoveObjInto(moveObj);
        exit;
    }
;

/* 
 *   require that the target object be in the same immediate location as
 *   the direct object 
 */
sameLocationAsDobj: SameLocationCondition
    sourceObj = (gDobj)
;

/* 
 *   require that the target object be in the same immediate location as
 *   the indirect object 
 */
sameLocationAsIobj: SameLocationCondition
    sourceObj = (gIobj)
;

/* ------------------------------------------------------------------------ */
/*
 *   Pre-condition: actor must have room to hold the object directly (such
 *   as in the actor's hands).  We'll let the actor do the work.
 */
roomToHoldObj: PreCondition
    checkPreCondition(obj, allowImplicit)
    {
        /* let the actor check the precondition */
        return gActor.tryMakingRoomToHold(obj, allowImplicit);
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Pre-condition: the actor must not be wearing the object.  If the
 *   actor is currently wearing the object, we'll try asking the actor to
 *   doff the object.
 *   
 *   Note that this pre-condition never needs to be combined with objHeld,
 *   because an object being worn is not considered to be held, and
 *   Wearable implicitly doffs an article when it must be held.  
 */
objNotWorn: PreCondition
    checkPreCondition(obj, allowImplicit)
    {
        /* if the object isn't being worn, we have nothing to do */
        if (obj == nil || !obj.isWornBy(gActor))
            return nil;
        
        /* try an implicit 'doff' command */
        if (allowImplicit && tryImplicitAction(Doff, obj))
        {
            /* 
             *   we executed the command - make sure it worked, and abort
             *   if it didn't 
             */
            if (obj.isWornBy(gActor))
                exit;

            /* tell the caller we executed an implicit command */
            return true;
        }

        /* report the problem and terminate the command */
        reportFailure(&cannotBeWearingMsg, obj);

        /* make it the pronoun */
        gActor.setPronounObj(obj);

        /* abort the command */
        exit;
    }

    /* lower the likelihood rating for anything being worn */
    verifyPreCondition(obj)
    {
        /* if the object is being worn, reduce its likelihood rating */
        if (obj != nil && obj.isWornBy(gActor))
            logicalRankOrd(80, 'implied doff', 150);
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Pre-condition: the object is open.
 */
class ObjOpenCondition: PreCondition
    checkPreCondition(obj, allowImplicit)
    {
        /* if the object is already open, we're already done */
        if (obj == nil || obj.isOpen)
            return nil;
        
        /* try an implicit 'open' command on the object */
        if (allowImplicit && tryImplicitAction(Open, obj))
        {
            /* 
             *   we executed the command - make sure it worked, and abort
             *   if it didn't 
             */
            if (!obj.isOpen)
                exit;

            /* tell the caller we executed an implied command */
            return true;
        }

        /* can't open it implicitly - report the failure */
        conditionFailed(obj);
        exit;
    }

    /*
     *   The condition failed - report the failure and give up.  We
     *   separate this to allow subclasses to report failure differently
     *   for specialized types of opening.  
     */
    conditionFailed(obj)
    {
        /* can't open it implicitly - report failure and give up */
        reportFailure(&mustBeOpenMsg, obj);

        /* make it the pronoun */
        gActor.setPronounObj(obj);
    }

    /* reduce the likelihood rating for anything that isn't already open */
    verifyPreCondition(obj)
    {
        /* if the object is closed, reduce its likelihood rating */
        if (obj != nil && !obj.isOpen)
            logicalRankOrd(80, 'implied open', 150);
    }
;

/*
 *   The basic object-open condition 
 */
objOpen: ObjOpenCondition;

/*
 *   Pre-condition: a door must be open.  This differs from the regular
 *   objOpen condition only in that we use a customized version of the
 *   failure report. 
 */
doorOpen: ObjOpenCondition
    conditionFailed(obj)
    {
        /* 
         *   We can generate implicit open-door commands as a result of
         *   travel, which means that the actor issuing the command might
         *   never have explicitly referred to the door.  (This is not the
         *   case for most preconditions, which refer to objects directly
         *   used in the command and thus within the actor's awareness, at
         *   least initially.)  So, if the door isn't visible to the
         *   actor, don't tell the actor they have to open the door;
         *   instead, just show the standard no-travel message for the
         *   door.  
         */
        if (gActor.canSee(obj))
        {
            /* they can see the door, so tell them they need to open it */
            reportFailure(&mustOpenDoorMsg, obj);

            /* set this as the pronoun antecedent */
            gActor.setPronounObj(obj);
        }
        else
        {
            /* 
             *   they can't see the door - call the door's routine to
             *   indicate that travel is not possible 
             */
            obj.cannotTravel();
        }
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Pre-condition: the object is closed.
 */
objClosed: PreCondition
    checkPreCondition(obj, allowImplicit)
    {
        /* if the object is already closed, we're already done */
        if (obj == nil || !obj.isOpen)
            return nil;
        
        /* try an implicit 'close' command on the object */
        if (allowImplicit && tryImplicitAction(Close, obj))
        {
            /* 
             *   we executed the command - make sure it worked, and abort
             *   if it didn't 
             */
            if (obj.isOpen)
                exit;

            /* tell the caller we executed an implied command */
            return true;
        }

        /* can't close it implicitly - report failure and give up */
        reportFailure(&mustBeClosedMsg, obj);

        /* make it the pronoun */
        gActor.setPronounObj(obj);

        /* abort the command */
        exit;
    }

    /* reduce the likelihood rating for anything that isn't already closed */
    verifyPreCondition(obj)
    {
        /* if the object is closed, reduce its likelihood rating */
        if (obj != nil && obj.isOpen)
            logicalRankOrd(80, 'implied close', 150);
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Pre-condition: the object is unlocked.
 */
objUnlocked: PreCondition
    checkPreCondition(obj, allowImplicit)
    {
        /* if the object is already unlocked, we're already done */
        if (obj == nil || !obj.isLocked)
            return nil;
        
        /* try an implicit 'unlock' command on the object */
        if (allowImplicit && tryImplicitAction(Unlock, obj))
        {
            /* 
             *   we executed the command - make sure it worked, and abort
             *   if it didn't 
             */
            if (obj.isLocked)
                exit;

            /* tell the caller we executed an implied command */
            return true;
        }

        /* can't unlock it implicitly - report failure and give up */
        reportFailure(&mustBeUnlockedMsg, obj);

        /* make it the pronoun */
        gActor.setPronounObj(obj);

        /* abort the command */
        exit;
    }

    /* reduce the likelihood rating for anything that's locked */
    verifyPreCondition(obj)
    {
        /* if the object is locked, reduce its likelihood rating */
        if (obj != nil && obj.isLocked)
            logicalRankOrd(80, 'implied unlock', 150);
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Pre-condition: destination for "drop" is an outermost room.  If the
 *   drop destination is a nested room, we'll try returning the actor to
 *   the outermost room via an implicit command.  
 */
dropDestinationIsOuterRoom: PreCondition
    checkPreCondition(obj, allowImplicit)
    {
        local dest;
        
        /* 
         *   if the actor's location's drop location is the outermost
         *   room, we don't need to do anything special 
         */
        dest = gActor.getDropDestination(obj, nil);
        if (dest.getOutermostRoom() == dest)
            return nil;

        /* 
         *   the default drop destination is not an outermost room; try an
         *   implicit command to return the actor to an outermost room 
         */
        return actorDirectlyInRoom.checkPreCondition(
            dest.getOutermostRoom(), allowImplicit);
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Pre-condition: object is burning.  This can be used for matches,
 *   candles, and the like.  If the object's isLit is nil, we'll attempt a
 *   "burn" command on the object.  
 */
objBurning: PreCondition
    checkPreCondition(obj, allowImplicit)
    {
        /* if it's already burning, there's nothing to do */
        if (obj == nil || obj.isLit)
            return nil;

        /* try an implicit 'burn' command */
        if (allowImplicit && tryImplicitAction(Burn, obj))
        {
            /* we executed a 'burn' - give up if it didn't work */
            if (!obj.isLit)
                exit;

            /* tell the caller we executed an implied command */
            return true;
        }

        /* we can't burn it implicitly - report failure and give up */
        reportFailure(&mustBeBurningMsg, obj);

        /* make it the pronoun */
        gActor.setPronounObj(obj);

        /* abort the command */
        exit;
    }

    verifyPreCondition(obj)
    {
        /* if the object is not already burning, reduce its likelihood */
        if (obj != nil && !obj.isLit)
            logicalRankOrd(80, 'implied burn', 150);
    }
;


/* ------------------------------------------------------------------------ */
/*
 *   Pre-condition: the object is empty.  This ensures that the object
 *   does not contain any other objects.
 *   
 *   Note that we unconditionally try to remove all objects.  If a
 *   container needs to have some objects that can be removed and others
 *   that can't (such as components within the container), then the
 *   container will have to be implemented as a ComplexContainer - the
 *   non-removable components should be made contents of the enclosing
 *   ComplexContainer, and the secret inner container should be the one
 *   subject to this precondition.  
 */
objEmpty: PreCondition
    checkPreCondition(obj, allowImplicit)
    {
        local chi;

        /* 
         *   if there's no object, or the object already has no contents,
         *   there's nothing to do 
         */
        if (obj == nil || obj.contents.length() == 0)
            return nil;

        /* 
         *   Try an implicit 'take x' on the object's first child.
         *   
         *   Note that we only try this on the first object, because the
         *   precondition mechanism automatically re-applies all
         *   preconditions after any one of them performs an implied
         *   command.  If we have multiple objects that must be removed,
         *   that basic loop will ensure that we'll come back here as many
         *   times as necessary.  
         */
        chi = obj.contents[1];
        if (allowImplicit && tryImplicitAction(TakeFrom, chi, obj))
        {
            /* make sure it worked */
            if (chi.isIn(obj))
                exit;
            
            /* tell the caller we tried an implied command */
            return true;
        }
            
        /* we can't remove the objects implicitly, so give up */
        reportFailure(&mustBeEmptyMsg, obj);

        /* make it the pronoun */
        gActor.setPronounObj(obj);

        /* abort the command */
        exit;
    }
;
