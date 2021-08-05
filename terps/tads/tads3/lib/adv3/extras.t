#charset "us-ascii"

/* 
 *   Copyright (c) 2000, 2006 Michael J. Roberts.  All Rights Reserved. 
 *   
 *   TADS 3 Library - extras: special-purpose object classes
 *   
 *   This module defines classes for specialized simulation objects.
 *   
 *   Portions are based on original work by Eric Eve, incorporated by
 *   permission.  
 */

/* include the library header */
#include "adv3.h"


/* ------------------------------------------------------------------------ */
/*
 *   A "complex" container is an object that can have multiple kinds of
 *   contents simultaneously.  For example, a complex container could act
 *   as both a surface, so that some objects are sitting on top of it, and
 *   simultaneously as a container, with objects inside.
 *   
 *   The standard containment model only allows one kind of containment per
 *   container, because the nature of the containment is a feature of the
 *   container itself.  The complex container handles multiple simultaneous
 *   containment types by using one or more sub-containers: for example, if
 *   we want to be able to act as both a surface and a regular container,
 *   we use two sub-containers, one of class Surface and one of class
 *   Container, to hold the different types of contents.  When we need to
 *   perform an operation specific to a certain containment type, we
 *   delegate the operation to the sub-container of the appropriate type.
 *   
 *   Note that the complex container itself treats its direct contents as
 *   components, so any component parts can be made direct contents of the
 *   complex container object.
 *   
 *   If you want to include objects in your source code that are initially
 *   located within the component sub-containers, define them as directly
 *   within the ComplexContainer object, but give each one a 'subLocation'
 *   property set to the property of the component sub-container that will
 *   initially contain it.  For example, here's how you'd place a blanket
 *   inside a washing machine, and a laundry basket on top of it:
 *   
 *.  + washingMachine: ComplexContainer 'washing machine' 'washing machine'
 *.    subContainer: ComplexComponent, Container { etc }
 *.    subSurface: ComplexComponent, Surface { etc }
 *.  ;
 *.  
 *.  ++ Thing 'big cotton blanket' 'blanket'
 *.    subLocation = &subContainer
 *.  ;
 *.  
 *.  ++ Container 'laundry basket' 'laundry basket'
 *.    subLocation = &subSurface
 *.  ;
 *   
 *   The subLocation setting is only used for initialization, and we
 *   automatically set it to nil right after we use it to set up the
 *   initial location.  If you want to move something into one of the
 *   sub-containers on the fly, simply refer to the desired component
 *   directly:
 *   
 *   pants.moveInto(washingMachine.subContainer); 
 */
class ComplexContainer: Thing
    /*
     *   Our inner container, if any.  This is a "secret" object (in other
     *   words, it doesn't appear to players as a separate named object)
     *   that we use to store the contents that are meant to be within the
     *   complex container.  If this is to be used, it should be set to a
     *   Container object - the most convenient way to do this is by using
     *   the nested object syntax to define a ComplexComponent Container
     *   instance, like so:
     *   
     *   washingMachine: ComplexContainer
     *.    subContainer: ComplexComponent, Container { etc }
     *.  ;
     *   
     *   Note that we use the ComplexComponent class (as well as
     *   Container) for the sub-container object.  This makes the
     *   sub-container automatically use the name of its enclosing object
     *   in messages (in this case, the sub-container will use the same
     *   name as the washing machine).
     *   
     *   Note that the sub-containers don't have to be of class
     *   ComplexComponent, but using that class makes your job a little
     *   easier because the class sets the location and naming
     *   automatically.  If you prefer to define your sub-containers as
     *   separate objects, not nested in the ComplexContainer's
     *   definition, there's no need to make them ComplexComponents; just
     *   make them ordinary Component objects.
     *   
     *   If this property is left as nil, then we don't have an inner
     *   container.  
     */
    subContainer = nil

    /*
     *   Our inner surface, if any.  This is a secret object like the
     *   inner container; this object acts as our surface. 
     */
    subSurface = nil

    /*
     *   Our underside, if any.  This is a secret object like the inner
     *   container; this object can act as the space underneath us, or as
     *   our bottom surface. 
     */
    subUnderside = nil

    /*
     *   Our rear surface or container, if any.  This is a secret internal
     *   object like the inner container; this object can act as our back
     *   surface, or as the space just behind us.  
     */
    subRear = nil

    /* a list of all of our component objects */
    allSubLocations = [subContainer, subSurface, subUnderside, subRear]

    /* 
     *   Show our status.  We'll show the status for each of our
     *   sub-objects, so that we list any contents of our sub-container or
     *   sub-surface along with our description. 
     */
    examineStatus()
    {
        /* if we have a sub-container, show its status */
        if (subContainer != nil)
            subContainer.examineStatus();

        /* if we have a sub-surface, show its status */
        if (subSurface != nil)
            subSurface.examineStatus();

        /* if we have a sub-rear, show its status */
        if (subRear != nil)
            subRear.examineStatus();

        /* if we have a sub-underside, show its status */
        if (subUnderside != nil)
            subUnderside.examineStatus();
    }

    /* 
     *   In most cases, the open/closed and locked/unlocked status of a
     *   complex container refer to the status of the sub-container.  
     */
    isOpen = (subContainer != nil ? subContainer.isOpen : inherited)
    isLocked = (subContainer != nil ? subContainer.isLocked : inherited)

    makeOpen(stat)
    {
        if (subContainer != nil)
            subContainer.makeOpen(stat);
        else
            inherited(stat);
    }

    makeLocked(stat)
    {
        if (subContainer != nil)
            subContainer.makeLocked(stat);
        else
            inherited(stat);
    }
        

    /* 
     *   route all commands that treat us as a container to our
     *   sub-container object 
     */
    dobjFor(Open) maybeRemapTo(subContainer != nil, Open, subContainer)
    dobjFor(Close) maybeRemapTo(subContainer != nil, Close, subContainer)
    dobjFor(LookIn) maybeRemapTo(subContainer != nil, LookIn, subContainer)
    iobjFor(PutIn) maybeRemapTo(subContainer != nil,
                                PutIn, DirectObject, subContainer)
    dobjFor(Lock) maybeRemapTo(subContainer != nil, Lock, subContainer)
    dobjFor(LockWith) maybeRemapTo(subContainer != nil,
                                   LockWith, subContainer, IndirectObject)
    dobjFor(Unlock) maybeRemapTo(subContainer != nil, Unlock, subContainer)
    dobjFor(UnlockWith) maybeRemapTo(subContainer != nil,
                                     UnlockWith, subContainer, IndirectObject)

    /* route commands that treat us as a surface to our sub-surface */
    iobjFor(PutOn) maybeRemapTo(subSurface != nil,
                                PutOn, DirectObject, subSurface)

    /* route commands that affect our underside to our sub-underside */
    iobjFor(PutUnder) maybeRemapTo(subUnderside != nil,
                                   PutUnder, DirectObject, subUnderside)
    dobjFor(LookUnder) maybeRemapTo(subUnderside != nil,
                                    LookUnder, subUnderside)

    /* route commands that affect our rear to our sub-rear-side */
    iobjFor(PutBehind) maybeRemapTo(subRear != nil,
                                    PutBehind, DirectObject, subRear)
    dobjFor(LookBehind) maybeRemapTo(subRear != nil, LookBehind, subRear)

    /* route commands relevant to nested rooms to our components */
    dobjFor(StandOn) maybeRemapTo(getNestedRoomDest(StandOnAction) != nil,
                                  StandOn, getNestedRoomDest(StandOnAction))
    dobjFor(SitOn) maybeRemapTo(getNestedRoomDest(SitOnAction) != nil,
                                SitOn, getNestedRoomDest(SitOnAction))
    dobjFor(LieOn) maybeRemapTo(getNestedRoomDest(LieOnAction) != nil,
                                LieOn, getNestedRoomDest(LieOnAction))
    dobjFor(Board) maybeRemapTo(getNestedRoomDest(BoardAction) != nil,
                                Board, getNestedRoomDest(BoardAction))
    dobjFor(Enter) maybeRemapTo(getNestedRoomDest(EnterAction) != nil,
                                Enter, getNestedRoomDest(EnterAction))

    /* map GET OUT/OFF to whichever complex component we're currently in */
    dobjFor(GetOutOf) maybeRemapTo(getNestedRoomSource(gActor) != nil,
                                   GetOutOf, getNestedRoomSource(gActor))
    dobjFor(GetOffOf) maybeRemapTo(getNestedRoomSource(gActor) != nil,
                                   GetOffOf, getNestedRoomSource(gActor))

    /*
     *   Get the destination for nested travel into this object.  By
     *   default, we'll look at the sub-container and sub-surface
     *   components to see if either is a nested room, and if so, we'll
     *   return that.  The surface takes priority if both are nested rooms.
     *   
     *   You can override this to differentiate by verb, if desired; for
     *   example, you could have SIT ON and LIE ON refer to the sub-surface
     *   component, while ENTER and BOARD refer to the sub-container
     *   component.
     *   
     *   Note that if you do need to override this method to distinguish
     *   between a sub-container ("IN") and a sub-surface ("ON") for nested
     *   room purposes, there's a subtlety to watch out for.  The English
     *   library maps "sit on" and "sit in" to the single Action SitOn;
     *   likewise with "lie in/on" for LieOn and "stand in/on" for StandOn.
     *   If you're distinguishing the sub-container from the sub-surface,
     *   you'll probably want to distinguish SIT IN from SIT ON (and
     *   likewise for LIE and STAND).  Fortunately, even though the action
     *   class is the same for both phrasings, you can still find out
     *   exactly which preposition the player typed using
     *   action.getEnteredVerbPhrase().  
     */
    getNestedRoomDest(action)
    {
        /* 
         *   check the sub-surface first to see if it's a nested room;
         *   failing that, check the sub-container; failing that, we don't
         *   have a suitable component destination 
         */
        if (subSurface != nil && subSurface.ofKind(NestedRoom))
            return subSurface;
        else if (subContainer != nil && subContainer.ofKind(NestedRoom))
            return subContainer;
        else
            return nil;
    }

    /*
     *   Get the source for nested travel out of this object.  This is used
     *   for GET OUT OF <self> - we figure out which nested room component
     *   the actor is in, so that we can remap the command to GET OUT OF
     *   <that component>. 
     */
    getNestedRoomSource(actor)
    {
        /* figure out which child the actor is in */
        foreach (local chi in allSubLocations)
        {
            if (chi != nil && actor.isIn(chi))
                return chi;
        }

        /* the actor doesn't appear to be in one of our component locations */
        return nil;
    }

    /*
     *   Get a list of objects suitable for matching ALL in TAKE ALL FROM
     *   <self>.  By default, if we have a sub-surface and/or
     *   sub-container, we return everything in scope that's inside either
     *   one of those.  Otherwise, if we have a sub-rear-surface and/or an
     *   underside, we'll return everything from those.  
     */
    getAllForTakeFrom(scopeList)
    {
        local containers;

        /* 
         *   Make a list of the containers in which we're going to look.
         *   If we have a sub-container or sub-surface, look only in those.
         *   Otherwise, if we have a rear surface or underside, look in
         *   those. 
         */
        containers = [];
        if (subContainer != nil)
            containers += subContainer;
        if (subSurface != nil)
            containers += subSurface;
        if (containers == [])
        {
            if (subRear != nil)
                containers += subRear;
            if (subUnderside != nil)
                containers += subUnderside;
        }

        /* 
         *   return the list of everything in scope that's directly in one
         *   of the selected containers, but isn't a component of its
         *   direct container 
         */
        return scopeList.subset(
            {x: (x != self
                 && containers.indexOf(x) == nil
                 && containers.indexWhich(
                     {c: x.isDirectlyIn(c) && !x.isComponentOf(c)}) != nil)});
    }

    /*
     *   Add an object to my contents.  If the object has a subLocation
     *   setting, take it as indicating which of my subcontainers is to
     *   contain the object.  
     */
    addToContents(obj)
    {
        local sub;
        
        /* 
         *   if the object has a subLocation, add it to my appropriate
         *   component object; if not, add to my own contents as usual 
         */
        if ((sub = obj.subLocation) != nil)
        {
            /* 
             *   It specifies a subLocation - add it to the corresponding
             *   component's contents.  Note that subLocation is a property
             *   pointer - &subContainer for my container component,
             *   &subSurface for my surface component, etc.  
             */
            self.(sub).addToContents(obj);

            /* 
             *   The object's present location is merely for set-up
             *   purposes, so that the '+' object definition notation can
             *   be used to give the object its initial location.  The
             *   object really wants to be in the sub-container, to whose
             *   contents list we've just added it.  Set its location to
             *   the sub-container.  
             */
            obj.location = self.(sub);

            /*
             *   Now that we've moved the object into its sub-location,
             *   forget the subLocation setting, since this property is
             *   only for initialization. 
             */
            obj.subLocation = nil;
        }
        else
        {
            /* there's no subLocation, so use the default handling */
            inherited(obj);
        }
    }

    /*
     *   If we have any SpaceOverlay children, abandon the contents of the
     *   overlaid spaces as needed. 
     */
    mainMoveInto(newCont)
    {
        /* 
         *   If any of our components are SpaceOverlays, notify them.  We
         *   only worry about the rear and underside components, since it's
         *   never appropriate for our container and surface components to
         *   act as space overlays. 
         */
        notifyComponentOfMove(subRear);
        notifyComponentOfMove(subUnderside);

        /* do the normal work */
        inherited(newCont);
    }

    /* 
     *   if we're being pushed into a new location (as a PushTraveler),
     *   abandon the contents of any SpaceOverlay components 
     */
    beforeMovePushable(traveler, connector, dest)
    {
        /* 
         *   notify our SpaceOverlay components that we're being moved, if
         *   we're going to end up in a new location 
         */
        if (dest != location)
        {
            /* notify our rear and underside components of the move */
            notifyComponentOfMove(subRear);
            notifyComponentOfMove(subUnderside);
        }
        
        /* do the normal work */
        inherited(traveler, connector, dest);
    }

    /* 
     *   if the given component is a SpaceOverlay, notify it that we're
     *   moving, so that it can abandon its contents as needed 
     */
    notifyComponentOfMove(sub)
    {
        /* if it's a SpaceOverlay, abandon its contents if necessary */
        if (sub != nil && sub.ofKind(SpaceOverlay))
            sub.abandonContents();
    }

    /* pass bag-of-holding operations to our sub-container */
    tryPuttingObjInBag(target)
    {
        /* if we have a subcontainer, let it handle the operation */
        return (subContainer != nil
                ? subContainer.tryPuttingObjInBag(target)
                : nil);
    }

    /* pass implicit PUT x IN self operations to our subcontainer */
    tryMovingObjInto(obj)
    {
        /* if we have a subcontainer, let it handle the operation */
        return (subContainer != nil
                ? subContainer.tryMovingObjInto(obj)
                : nil);
    }
;

/* 
 *   we don't actually define any subLocation property values anywhere, so
 *   declare it to make sure the compiler knows it's a property name 
 */
property subLocation;

/*
 *   A component object of a complex container.  This class can be used as
 *   a mix-in for sub-objects of a complex container (the subContainer or
 *   subSurface) defined as nested objects.
 *   
 *   This class is based on Component, which is suitable for complex
 *   container sub-objects because it makes them inseparable from the
 *   complex container.  It's also based on NameAsParent, which makes the
 *   object automatically use the same name (in messages) as the lexical
 *   parent object.  This is usually what one wants for a sub-object of a
 *   complex container, because it makes the sub-object essentially
 *   invisible to the user by referring to the sub-object in messages as
 *   though it were the complex container itself: "The washing machine
 *   contains...".
 *   
 *   This class also automatically initializes our location to our lexical
 *   parent, during the pre-initialization process.  Any of these that are
 *   dynamically created at run-time (using 'new') must have their
 *   locations set manually, because initializeLocation() won't be called
 *   automatically in those cases.  
 */
class ComplexComponent: Component, NameAsParent
    initializeLocation()
    {
        /* set our location to our lexical parent */
        location = lexicalParent;

        /* inherit default so we initialize our container's 'contents' list */
        inherited();
    }

    /* 
     *   Get our "identity" object.  We take our identity from our parent
     *   object, if we have one.  Note that our identity isn't simply our
     *   parent, but rather is our parent's identity, recursively defined.
     */
    getIdentityObject()
    {
        return (location != nil ? location.getIdentityObject() : self);
    }

    /* don't participate in 'all', since we're a secret internal object */
    hideFromAll(action) { return true; }

    /* 
     *   In case this component is being used to implement a nested room of
     *   some kind (a platform, booth, etc), use the complex container's
     *   location as the staging location.  Normally our staging location
     *   would be our direct container, but as with other aspects of
     *   complex containers, the container/component are meant to act as a
     *   single combined object, so we'd want to bypass the complex
     *   container and move directly between the enclosing location and
     *   'self'.  
     */
    stagingLocations = [lexicalParent.location]
;

/*
 *   A container door.  This is useful for cases where you want to create
 *   the door to a container as a separate object in its own right.  
 */
class ContainerDoor: Component
    /*
     *   In most cases, you should create a ContainerDoor as a component of
     *   a ComplexContainer.  It's usually necessary to use a
     *   ComplexContainer in order to use a door, since the door has to go
     *   somewhere, and it can't go inside the container it controls
     *   (because if it were inside, it wouldn't be accessible when the
     *   container is closed).
     *   
     *   By default, we assume that our immediate location is a complex
     *   container, and its subContainer is the actual container for which
     *   we're the door.  You can override this property to create a
     *   different relationship if necessary.  
     */
    subContainer = (location.subContainer)

    /* we're open if our associated sub-container is open */
    isOpen = (subContainer.isOpen)

    /* our status description mentions our open status */
    examineStatus()
    {
        /* add our open status */
        say(isOpen
            ? gLibMessages.currentlyOpen : gLibMessages.currentlyClosed);

        /* add the base class behavior */
        inherited();
    }

    /* looking in or behind a door is like looking inside the container */
    dobjFor(LookIn) remapTo(LookIn, subContainer)
    dobjFor(LookBehind) remapTo(LookIn, subContainer)

    /* door-like operations on the door map to the container */
    dobjFor(Open) remapTo(Open, subContainer)
    dobjFor(Close) remapTo(Close, subContainer)
    dobjFor(Lock) remapTo(Lock, subContainer)
    dobjFor(LockWith) remapTo(LockWith, subContainer, IndirectObject)
    dobjFor(Unlock) remapTo(Unlock, subContainer)
    dobjFor(UnlockWith) remapTo(UnlockWith, subContainer, IndirectObject)
;


/* ------------------------------------------------------------------------ */
/*
 *   A "space overlay" is a special type of container whose contents are
 *   supposed to be adjacent to the container object (i.e., self), but are
 *   not truly contained in the usual sense.  This is used to model spatial
 *   relationships such as UNDER and BEHIND, which aren't directly
 *   supported in the normal containment model.
 *   
 *   The special feature of a space overlay is that the contents aren't
 *   truly attached to the container object, so they don't move with it the
 *   way that the contents of an ordinary container do.  For example,
 *   suppose we have a space overlay representing a bookcase and the space
 *   behind it, so that we can hide a painting behind the bookcase: in this
 *   case, moving the bookcase should leave the painting where it was,
 *   because it was just sitting there in that space.  In the real world,
 *   of course, the painting was sitting on the floor all along, so moving
 *   the bookcase would have no effect on it; but our spatial relationship
 *   model isn't quite as good as reality's, so we have to resort to an
 *   extra fix-up step.  Specifically, when we move a space overlay, we
 *   always check to see if its contents need to be relocated to the place
 *   where they were really supposed to be all along.  
 */
class SpaceOverlay: BulkLimiter
    /* 
     *   If we move this object, the objects we contain might stay put
     *   rather than moving along with the container.  For example, if we
     *   represent the space behind a bookcase, moving the bookcase would
     *   leave objects that were formerly behind the bookcase just sitting
     *   on the floor (or attached to the wall, or whatever).  
     */
    mainMoveInto(newContainer)
    {
        /* check to see if our objects need to be left behind */
        abandonContents();

        /* now do the normal work */
        inherited(newContainer);
    }

    /* 
     *   when we're being pushed to a new location via push-travel, abandon
     *   our contents before we're moved 
     */
    beforeMovePushable(traveler, connector, dest)
    {
        /* check to see if our objects need to be left behind */
        if (dest != getIdentityObject().location)
            abandonContents();

        /* do the normal work */
        inherited(traveler, connector, dest);
    }

    /* 
     *   abandonLocation is where the things under me end up when I'm
     *   moved.
     *   
     *   An Underside or RearContainer represents an object that has a
     *   space underneath or behind it, respectively, but the space itself
     *   isn't truly part of the container object (i.e., self).  This
     *   means that when the container moves, the objects under/behind it
     *   shouldn't move.  For example, if there's a box under a bed,
     *   moving the bed out of the room should leave the box sitting on
     *   the floor where the bed used to be.
     *   
     *   By default, our abandonLocation is simply the location of our
     *   "identity object" - that is, the location of our nearest
     *   enclosing object that isn't a component.
     *   
     *   This can be overridden if the actual abandonment location should
     *   be somewhere other than our assembly location.  In addition, you
     *   can set this to nil to indicate that objects under/behind me will
     *   NOT be abandoned when I move; instead, they'll simply stay with
     *   me, as though they're attached to my underside/back surface.  
     */
    abandonLocation = (getIdentityObject().location)
    
    /* 
     *   By default we list our direct contents the first time we're
     *   moved, and ONLY the first time.  If alwaysListOnMove is
     *   overridden to true, then we'll list our contents EVERY time we're
     *   moved.  If neverListOnMove is set to true, then we'll NEVER list
     *   our contents automatically when moved; this can be used in cases
     *   where the game wants to produce its own listing explicitly,
     *   rather than using the default listing we generate.  (Obviously,
     *   setting both 'always' and 'never' is meaningless, but in case
     *   you're wondering, 'never' overrides 'always' in this case.)
     *   
     *   Setting abandonLocation to nil overrules alwaysListOnMove: if
     *   there's no abandonment, then we consider nothing to be revealed
     *   when we're moved, since my contents move along with me.  
     */
    alwaysListOnMove = nil
    neverListOnMove = nil

    /*
     *   The lister we use to describe the objects being revealed when we
     *   move the SpaceOverlay object and abandon the contents.  Each
     *   concrete kind of SpaceOverlay must provide a lister that uses
     *   appropriate language; the list should be roughly of the form
     *   "Moving the armoire reveals a rusty can underneath."  Individual
     *   objects can override this to customize the message further.  
     */
    abandonContentsLister = nil

    /*
     *   Abandon my contents when I'm moved.  This is called whenever we're
     *   moved to a new location, to take care of leaving behind the
     *   objects that were formerly under me.
     *   
     *   We'll move my direct contents into abandonLocation, unless that's
     *   set to nil.  We don't move any Component objects within me, since
     *   we assume those to be attached.  
     */
    abandonContents()
    {
        local dest;
        
        /* 
         *   if there's no abandonment location, our contents move with us,
         *   so there's nothing to do 
         */
        if ((dest = abandonLocation) == nil)
            return;

        /* 
         *   If we've never been moved before, or we always reveal my
         *   contents when moved, list our contents now.  In any case, if
         *   we *never* list on move, don't generate the listing.  
         */
        if ((alwaysListOnMove || !getIdentityObject().moved)
            && !neverListOnMove)
        {
            local marker1, marker2;
            
            /*
             *   We want to generate a listing of what is revealed by
             *   moving the object, which we can do by generating a
             *   listing of what we would normally see by looking in the
             *   overlay interior.  We want the listing as it stands now,
             *   but in most cases, we don't actually want to generate the
             *   list quite yet, because we want the action that's moving
             *   the object to complete and show all of its messages first.
             *   
             *   However, if the action leaves the actor in a new
             *   location, do generate the listing before the rest of the
             *   action output, since the listing won't make any sense
             *   after we've moved to (and displayed the description of)
             *   the new location.
             *   
             *   To accomplish all of this, generate the listing now,
             *   before the rest of the action output, but insert special
             *   report markers into the transcript before and after the
             *   listing.  Then, register to receive after-action
             *   notification; at the end of the action, we'll go back and
             *   move the range of transcript output between the markers
             *   to the end of the command's transcript entries, if we
             *   haven't moved to a new room.
             *   
             *   One final complication: we don't want our listing here to
             *   hide any default report from the main command, so run it
             *   as a sub-action.  A sub-action doesn't override the
             *   visibility of its parent's default report.  
             */

            /* first, add a marker to the transcript before the listing */
            marker1 = gTranscript.addMarker();

            /* generate the listing, using a generic sub-action context */
            withActionEnv(Action, gActor, {: listContentsForMove() });

            /* add another transcript marker after the listing */
            marker2 = gTranscript.addMarker();

            /* 
             *   create our special handler object to receive notification
             *   at the end of the command - it'll move the reports to the
             *   end of the command output if need be 
             */
            new SpaceOverlayAbandonFinisher(marker1, marker2);
        }

        /* now move my non-Component contents to the abandonment location */
        foreach(obj in contents)
        {
            /* if it's not a component, move it */
            if(!obj.ofKind(Component))
                obj.moveInto(dest);
        }
    }

    /*
     *   My weight does NOT include my "contents" if we abandon our
     *   contents on being moved.  Our contents are not attached to us as
     *   they are in a normal sort of container; instead, they're merely
     *   colocated, so when we're moved, that colocation relationship ends.
     */
    getWeight()
    {
        /* 
         *   if we abandon our contents on being moved, our weight doesn't
         *   include them, because they're not attached; otherwise, they do
         *   act like they're attached, and hence must be included in our
         *   weight as for any ordinary container 
         */
        if (abandonLocation != nil)
        {
            /* 
             *   our contents are not included in our weight, so our total
             *   weight is simply our own intrinsic weight 
             */
            return weight;
        }
        else
        {
            /* our contents are attached, so include their weight as normal */
            return inherited();
        }
    }

    /* 
     *   List our contents for moving the object.  By default, we examine
     *   our interior using our abandonContentsLister.  
     */
    listContentsForMove()
    {
        /* examine our contents with the abandonContentsLister */
        examineInteriorWithLister(abandonContentsLister);
    }
;

/* 
 *   Space Overlay Abandon Finisher - this is an internal object that we
 *   create in SpaceOverlay.abandonContents().  Its purpose is to receive
 *   an afterAction notification and clean up the report order if
 *   necessary. 
 */
class SpaceOverlayAbandonFinisher: object
    construct(m1, m2)
    {
        /* remember the markers */
        marker1 = m1;
        marker2 = m2;

        /* remember the actor's starting location */
        origLocation = gActor.location;

        /* register for afterAction notification */
        gAction.addBeforeAfterObj(self);
    }

    /* the transcript markers identifying the listing reports */
    marker1 = nil
    marker2 = nil

    /* the actor's location at the time we generated the listing */
    origLocation = nil

    /* receive our after-action notification */
    afterAction()
    {
        /* 
         *   If the actor hasn't changed locations, move the reports we
         *   generated for the listing to the end of the transcript. 
         */
        if (gActor.location == origLocation)
            gTranscript.moveRangeAppend(marker1, marker2);
    }
;

/*
 *   An "underside" is a special type of container that describes its
 *   contents as being under the object.  This is appropriate for objects
 *   that have a space underneath, such as a bed or a table.  
 */
class Underside: SpaceOverlay
    /*
     *   Can actors put new objects under self, using the PUT UNDER
     *   command?  By default, we allow it.  Override this property to nil
     *   if new objects cannot be added by player commands.  
     */
    allowPutUnder = true

    /* we need to LOOK UNDER this object to see its contents */
    nestedLookIn() { nestedAction(LookUnder, self); }

    /* use custom contents listers, for our special "under" wording */
    contentsLister = undersideContentsLister
    descContentsLister = undersideDescContentsLister
    lookInLister = undersideLookUnderLister
    inlineContentsLister = undersideInlineContentsLister
    abandonContentsLister = undersideAbandonContentsLister
    
    /* customize the message for taking something from that's not under me */
    takeFromNotInMessage = &takeFromNotUnderMsg
 
    /* customize the message indicating another object is already in me */
    circularlyInMessage = &circularlyUnderMsg
 
    /* message phrase for objects put under me */
    putDestMessage = &putDestUnder
 
    /* message when we don't have room to put another object under me */
    tooFullMsg = &undersideTooFull

    /* message when an object is too large (all by itself) to fit under me */
    tooLargeForContainerMsg = &tooLargeForUndersideMsg

    /* can't put self under self */
    cannotPutInSelfMsg = &cannotPutUnderSelfMsg

    /* can't put something under me when it's already under me */
    alreadyPutInMsg = &alreadyPutUnderMsg

    /* our implied containment verb is PUT UNDER */
    tryMovingObjInto(obj) { return tryImplicitAction(PutUnder, obj, self); }

    /* -------------------------------------------------------------------- */
    /*
     *   Handle putting things under me 
     */
    iobjFor(PutUnder)
    {
        verify()
        {
            /* use the standard put-in-interior verification */
            verifyPutInInterior();
        }
        check()
        {
            /* only allow it if PUT UNDER commands are allowed */
            if (!allowPutUnder)
            {
                reportFailure(&cannotPutUnderMsg);
                exit;
            }
        }
        action()
        {
            /* move the direct object onto me */
            gDobj.moveInto(self);
             
            /* issue our default acknowledgment */
            defaultReport(&okayPutUnderMsg);
        }
    }
    
    /*
     *   Looking "under" a surface simply shows the surface's contents. 
     */
    dobjFor(LookUnder)
    {
        verify() { }
        action() { examineInterior(); }
    }
;

/*
 *   A special kind of Underside that only accepts specific contents.
 */
class RestrictedUnderside: RestrictedHolder, Underside
    /* 
     *   A message that explains why the direct object can't be put under
     *   this object.  In most cases, the rather generic default message
     *   should be overridden to provide a specific reason that the dobj
     *   can't be put under me.  The rejected object is provided as a
     *   parameter in case the message needs to vary by object, but we
     *   ignore this and just use a single blanket failure message by
     *   default.  
     */
    cannotPutUnderMsg(obj) { return &cannotPutUnderRestrictedMsg; }

    /* override PutUnder to enforce our contents restriction */
    iobjFor(PutUnder) { check() { checkPutDobj(&cannotPutUnderMsg); } }
;

/*
 *   A "rear container" is similar to an underside: it models the space
 *   behind an object.  
 */
class RearContainer: SpaceOverlay
    /*
     *   Can actors put new objects behind self, using the PUT BEHIND
     *   command?  By default, we allow it.  Override this property to nil
     *   if new objects cannot be added by player commands.  
     */
    allowPutBehind = true
     
    /* we need to LOOK BEHIND this object to see its contents */
    nestedLookIn() { nestedAction(LookBehind, self); }

    /* use custom contents listers */
    contentsLister = rearContentsLister
    descContentsLister = rearDescContentsLister
    lookInLister = rearLookBehindLister
    inlineContentsLister = rearInlineContentsLister 
    abandonContentsLister = rearAbandonContentsLister

    /* the message for taking things from me that aren't behind me */
    takeFromNotInMessage = &takeFromNotBehindMsg

    /* 
     *   my message indicating that another object x cannot be put into me
     *   because I'm already in x 
     */
    circularlyInMessage = &circularlyBehindMsg
 
    /* message phrase for objects put under me */
    putDestMessage = &putDestBehind
 
    /* message when we're too full for another object */
    tooFullMsg = &rearTooFullMsg

    /* message when object is too large to fit behind me */
    tooLargeForContainerMsg = &tooLargeForRearMsg

    /* customize the verification messages */
    cannotPutInSelfMsg = &cannotPutBehindSelfMsg
    alreadyPutInMsg = &alreadyPutBehindMsg

    /* our implied containment verb is PUT BEHIND */
    tryMovingObjInto(obj) { return tryImplicitAction(PutBehind, obj, self); }

    /* -------------------------------------------------------------------- */
    /*
     *   Handle the PUT UNDER command
     */
    iobjFor(PutBehind)
    {
        verify()  { verifyPutInInterior(); }
        check()
        {
            /* only allow it if PUT BEHIND commands are allowed */
            if (!allowPutBehind)
            {
                reportFailure(&cannotPutBehindMsg);
                exit;
            }
        }
        action()
        {
            /* move the direct object behind me */
            gDobj.moveInto(self);
            
            /* issue our default acknowledgment */
            defaultReport(&okayPutBehindMsg);
        }
    }
    
    /*
     *   Looking "behind" a surface simply shows the surface's contents. 
     */
    dobjFor(LookBehind)
    {
        verify() { }
        action() { examineInterior(); }
    }
;

/*
 *   A special kind of RearContainer that only accepts specific contents.
 */
class RestrictedRearContainer: RestrictedHolder, RearContainer
    /* 
     *   A message that explains why the direct object can't be put behind
     *   this object.  In most cases, the rather generic default message
     *   should be overridden to provide a specific reason that the dobj
     *   can't be put behind me.  The rejected object is provided as a
     *   parameter in case the message needs to vary by object, but we
     *   ignore this and just use a single blanket failure message by
     *   default.  
     */
    cannotPutBehindMsg(obj) { return &cannotPutBehindRestrictedMsg; }

    /* override PutBehind to enforce our contents restriction */
    iobjFor(PutBehind) { check() { checkPutDobj(&cannotPutBehindMsg); } }
;

/*
 *   A "rear surface" is essentially the same as a "rear container," but
 *   models the contents as being attached to the back of the object rather
 *   than merely sitting behind it.
 *   
 *   The only practical difference between the "container" and the
 *   "surface" is that moving a surface moves its contents along with it,
 *   whereas moving a container abandons the contents, leaving them behind
 *   where the container used to be.  
 */
class RearSurface: RearContainer
    /*
     *   We're a surface, not a space, so our contents stay attached when
     *   we move.  
     */
    abandonLocation = nil
;

/*
 *   A restricted-contents RearSurface
 */
class RestrictedRearSurface: RestrictedHolder, RearSurface
    /* explain the problem */
    cannotPutBehindMsg(obj) { return &cannotPutBehindRestrictedMsg; }

    /* override PutBehind to enforce our contents restriction */
    iobjFor(PutBehind) { check() { checkPutDobj(&cannotPutBehindMsg); } }
;

/* ------------------------------------------------------------------------ */
/*
 *   A "stretchy container."  This is a simple container subclass whose
 *   external bulk changes according to the bulks of the contents.  
 */
class StretchyContainer: Container
    /* 
     *   Our minimum bulk.  This is the minimum bulk we'll report, even
     *   when the aggregate bulks of our contents are below this limit. 
     */
    minBulk = 0

    /* get my total external bulk */
    getBulk()
    {
        local tot;

        /* start with my own intrinsic bulk */
        tot = bulk;

        /* add the bulk contribution from my contents */
        tot += getBulkForContents();

        /* return the total, but never less than the minimum */
        return tot >= minBulk ? tot : minBulk;
    }

    /*
     *   Calculate the contribution to my external bulk of my contents.
     *   The default for a stretchy container is to conform exactly to the
     *   contents, as though the container weren't present at all, hence
     *   we simply sum the bulks of our contents.  Subclasses can override
     *   this to define other aggregate bulk effects as needed.  
     */
    getBulkForContents()
    {
        local tot;
        
        /* sum the bulks of the items in our contents */
        tot = 0;
        foreach (local cur in contents)
            tot += cur.getBulk();

        /* return the total */
        return tot;
    }

    /*
     *   Check what happens when a new object is inserted into my
     *   contents.  This is called with the new object already tentatively
     *   added to my contents, so we can examine our current status to see
     *   if everything works.
     *   
     *   Since we can change our own size when a new item is added to our
     *   contents, we'll trigger a full bulk change check. 
     */
    checkBulkInserted(insertedObj)
    {
        /* 
         *   inherit the normal handling to ensure that the new object
         *   fits within this container 
         */
        inherited(insertedObj);

        /* 
         *   since we can change our own shape when items are added to our
         *   contents, trigger a full bulk check on myself 
         */
        checkBulkChange();
    }

    /* 
     *   Check a bulk change of one of my direct contents.  Since my own
     *   bulk changes whenever the bulk of one of my contents changes, we
     *   must propagate the bulk change of our contents as a change in our
     *   own bulk. 
     */
    checkBulkChangeWithin(changingObj)
    {
        /*
         *   Do any inherited work, in case we have a limit on our own
         *   internal bulk. 
         */
        inherited(changingObj);

        /* 
         *   This might cause a change in my own bulk, since my bulk
         *   depends on the bulks of my contents.  When this is called,
         *   obj is already set to indicate its new bulk; since we
         *   calculate our own bulk by looking at our contents' bulks,
         *   this means that our own getBulk will now report the latest
         *   value including obj's new bulk. 
         */
        checkBulkChange();
    }
;


/* ------------------------------------------------------------------------ */
/*
 *   "Bag of Holding."  This is a mix-in that actively moves items from the
 *   holding actor's direct inventory into itself when the actor's hands
 *   are too full.
 *   
 *   The bag of holding offers a solution to the conflict between "realism"
 *   and playability.  On the one hand, in real life, you can only hold so
 *   many items at once, so at first glance it seems a simulation ought to
 *   have such a limit in order to be more realistic.  On the other hand,
 *   most players justifiably hate having to deal with a carrying limit,
 *   because it forces the player to spend a lot of time doing tedious
 *   inventory management.
 *   
 *   The Bag of Holding is a compromise solution.  The concept is borrowed
 *   from live role-playing games, where it's usually a magical item that
 *   can hold objects of unlimited size and weight, thereby allowing
 *   characters to transport impossibly large objects.  In text IF, a bag
 *   of holding isn't usually magical - it's usually just something like a
 *   large backpack, or a trenchcoat with lots of pockets.  And it usually
 *   isn't meant as a solution to an obvious puzzle; rather, it's meant to
 *   invisibly prevent inventory management from becoming a puzzle in the
 *   first place, by shuffling objects out of the PC's hands automatically
 *   to free up space as needed.
 *   
 *   This Bag of Holding implementation works by automatically moving
 *   objects from an actor's hands into the bag object, whenever the actor
 *   needs space to pick up a new item.  Whenever an action has a
 *   "roomToHoldObj" precondition, the precondition will automatically look
 *   for a BagOfHolding object within the actor's inventory, and then move
 *   as many items as necessary from the actor's hands to the bag.  
 */
class BagOfHolding: object
    /*
     *   Get my bags of holding.  Since we are a bag of holding, we'll add
     *   ourselves to the vector, then we'll inherit the normal handling
     *   to pick up our contents. 
     */
    getBagsOfHolding(vec)
    {
        /* we're a bag of holding */
        vec.append(self);

        /* inherit the normal handling */
        inherited(vec);
    }

    /*
     *   Get my "affinity" for the given object.  This is an indication of
     *   how strongly this bag wants to contain the object.  The affinity
     *   is a number in arbitrary units; higher numbers indicate stronger
     *   affinities.  An affinity of zero means that the bag does not want
     *   to contain the object at all.
     *   
     *   The purpose of the affinity is to support specialized holders
     *   that are designed to hold only specific types of objects, and
     *   allow these specialized holders to implicitly gather their
     *   specific objects.  For example, a key ring might only hold keys,
     *   so it would have a high affinity for keys and a zero affinity for
     *   everything else.  A lunchbox might have a higher affinity for
     *   things like sandwiches than for anything else, but might be
     *   willing to serve as a general container for other small items as
     *   well.
     *   
     *   The units of affinity are arbitrary, but the library uses the
     *   following values for its own classes:
     *   
     *   0 - no affinity at all; the bag cannot hold the object
     *   
     *   50 - willing to hold the object, but not of the preferred type
     *   
     *   100 - default affinity; willing and able to hold the object, but
     *   just as willing to hold most other things
     *   
     *   200 - special affinity; this object is of a type that we
     *   especially want to hold
     *   
     *   We intentionally space these loosely so that games can use
     *   intermediate levels if desired.
     *   
     *   When we are looking for bags of holding to consolidate an actor's
     *   directly-held inventory, note that we always move the object with
     *   the highest bag-to-object affinity out of all of the objects
     *   under consideration.  So, if you want to give a particular kind
     *   of bag priority so that the library uses that bag before any
     *   other bag, make this routine return a higher affinity for the
     *   bag's objects than any other bags do.
     *   
     *   By default, we'll return the default affinity of 100.
     *   Specialized bags that don't hold all types of objects must
     *   override this to return zero for objects they can't hold.  
     */
    affinityFor(obj)
    {
        /* 
         *   my affinity for myself is zero, for obvious reasons; for
         *   everything else, use the default affinity 
         */
        return (obj == self ? 0 : 100);
    }
;


/* ------------------------------------------------------------------------ */
/*
 *   Keyring - a place to stash keys
 *   
 *   Keyrings have some special properties:
 *   
 *   - A keyring is a bag of holding with special affinity for keys.
 *   
 *   - A keyring can only contain keys.
 *   
 *   - Keys are considered to be on the outside of the ring, so a key can
 *   be used even if attached to the keyring (in other words, if the ring
 *   itself is held, a key attached to the ring is also considered held).
 *   
 *   - If an actor in possession of a keyring executes an "unlock" command
 *   without specifying what key to use, we will automatically test each
 *   key on the ring to find the one that works.
 *   
 *   - When an actor takes one of our keys, and the actor is in possession
 *   of this keyring, we'll automatically attach the key to the keyring
 *   immediately.  
 */
class Keyring: BagOfHolding, Thing
    /* lister for showing our contents in-line as part of a list entry */
    inlineContentsLister = keyringInlineContentsLister

    /* lister for showing our contents as part of "examine" */
    descContentsLister = keyringExamineContentsLister

    /* 
     *   Determine if a key fits our keyring.  By default, we will accept
     *   any object of class Key.  However, subclasses might want to
     *   override this to associate particular keys with particular
     *   keyrings rather than having a single generic keyring.  To allow
     *   only particular keys onto this keyring, override this routine to
     *   return true only for the desired keys.  
     */
    isMyKey(key)
    {
        /* accept any object of class Key */
        return key.ofKind(Key);
    }

    /* we have high affinity for our keys */
    affinityFor(obj)
    {
        /* 
         *   if the object is one of my keys, we have high affinity;
         *   otherwise we don't accept it at all 
         */
        if (isMyKey(obj))
            return 200;
        else
            return 0;
    }

    /* implicitly put a key on the keyring */
    tryPuttingObjInBag(target)
    {
        /* we're a container, so use "put in" to get the object */
        return tryImplicitActionMsg(&announceMoveToBag, PutOn, target, self);
    }

    /* on taking the keyring, attach any loose keys */
    dobjFor(Take)
    {
        action()
        {
            /* do the normal work */
            inherited();
            
            /* get the list of loose keys */
            local lst = getLooseKeys(gActor);

            /* consider only the subset that are my valid keys */
            lst = lst.subset({x: isMyKey(x)});

            /* if there are any, move them onto the keyring */
            if (lst.length() != 0)
            {
                /* put each loose key on the keyring */
                foreach (local cur in lst)
                    cur.moveInto(self);

                /* announce what happened */
                extraReport(&movedKeysToKeyringMsg, self, lst);
            }
        }
    }

    /* 
     *   Get the loose keys in the given actor's possession.  On taking the
     *   keyring, we'll attach these loose keys to the keyring
     *   automatically.  By default, we return any keys the actor is
     *   directly holding. 
     */
    getLooseKeys(actor) { return actor.contents; }

    /* allow putting a key on the keyring */
    iobjFor(PutOn)
    {
        /* we can only put keys on keyrings */
        verify()
        {
            /* we'll only allow our own keys to be attached */
            if (gDobj == nil)
            {
                /* 
                 *   we don't know the actual direct object yet, but we
                 *   can at least check to see if any of the possible
                 *   dobj's is my kind of key 
                 */
                if (gTentativeDobj.indexWhich({x: isMyKey(x.obj_)}) == nil)
                    illogical(&objNotForKeyringMsg);
            }
            else if (!isMyKey(gDobj))
            {
                /* the dobj isn't a valid key for this keyring */
                illogical(&objNotForKeyringMsg);
            }
        }
        
        /* put a key on me */
        action()
        {
            /* move the key into me */
            gDobj.moveInto(self);
            
            /* show the default "put on" response */
            defaultReport(&okayPutOnMsg);
        }
    }

    /* treat "attach x to keyring" as "put x on keyring" */
    iobjFor(AttachTo) remapTo(PutOn, DirectObject, self)

    /* treat "detach x from keyring" as "take x from keyring" */
    iobjFor(DetachFrom) remapTo(TakeFrom, DirectObject, self)

    /* receive notification before an action */
    beforeAction()
    {
        /*
         *   Note whether or not we want to consider moving the direct
         *   object to the keyring after a "take" command.  We will
         *   consider doing so only if the direct object isn't already on
         *   the keyring - if it is, we don't want to move it back right
         *   after removing it, obviously.
         *   
         *   Skip the implicit keyring attachment if the current command
         *   is implicit, because they must be doing something that
         *   requires holding the object, in which case taking it is
         *   incidental.  It could be actively annoying to attach the
         *   object to the keyring in such cases - for example, if the
         *   command is "put key on keyring," attaching it as part of the
         *   implicit action would render the explicit command redundant
         *   and cause it to fail.  
         */
        moveAfterTake = (!gAction.isImplicit
                         && gDobj != nil
                         && !gDobj.isDirectlyIn(self));
    }

    /* flag: consider moving to keyring after this "take" action */
    moveAfterTake = nil

    /* receive notification after an action */
    afterAction()
    {
        /*
         *   If the command was "take", and the direct object was a key,
         *   and the actor involved is holding the keyring and can touch
         *   it, and the command succeeded in moving the key to the
         *   actor's direct inventory, then move the key onto the keyring.
         *   Only consider this if we decided to during the "before"
         *   notification.  
         */
        if (moveAfterTake
            && gActionIs(Take)
            && isMyKey(gDobj)
            && isIn(gActor)
            && gActor.canTouch(self)
            && gDobj.isDirectlyIn(gActor))
        {
            /* move the key to me */
            gDobj.moveInto(self);

            /* 
             *   Mention what we did.  If the only report for this action
             *   so far is the default 'take' response, then use the
             *   combined taken-and-attached message.  Otherwise, append
             *   our 'attached' message, which is suitable to use after
             *   other messages. 
             */
            if (gTranscript.currentActionHasReport(
                {x: (x.ofKind(CommandReportMessage)
                     && x.messageProp_ != &okayTakeMsg)}))
            {
                /* 
                 *   we have a non-default message already, so add our
                 *   message indicating that we added the key to the
                 *   keyring 
                 */
                reportAfter(&movedKeyToKeyringMsg, self);
            }
            else
            {
                /* use the combination taken-and-attached message */
                mainReport(&takenAndMovedToKeyringMsg, self);
            }
        }
    }

    /* find among our keys a key that works the direct object */
    findWorkingKey(lock)
    {
        /* try each key on the keyring */
        foreach (local key in contents)
        {
            /* 
             *   if this is the key that unlocks the lock, replace the
             *   command with 'unlock lock with key' 
             */
            if (lock.keyFitsLock(key))
            {
                /* note that we tried keys and found the right one */
                extraReport(&foundKeyOnKeyringMsg, self, key);
                
                /* return the key */
                return key;
            }
        }

        /* we didn't find the right key - indicate failure */
        reportFailure(&foundNoKeyOnKeyringMsg, self);
        return nil;
    }

    /*
     *   Append my directly-held contents to a vector when I'm directly
     *   held.  We consider all of the keys on the keyring to be
     *   effectively at the same containment level as the keyring, so if
     *   the keyring is held, so are its attached keys.  
     */
    appendHeldContents(vec)
    {
        /* append all of our contents, since they're held when we are */
        vec.appendUnique(contents);
    }

    /*
     *   Announce myself as a default object for an action.
     *   
     *   Do not announce a keyring as a default for "lock with" or "unlock
     *   with".  Although we can use a keyring as the indirect object of a
     *   lock/unlock command, we don't actually do the unlocking with the
     *   keyring; so, when we're chosen as the default, suppress the
     *   announcement, since it would imply that we're being used to lock
     *   or unlock something.  
     */
    announceDefaultObject(whichObj, action, resolvedAllObjects)
    {
        /* if it's not a lock-with or unlock-with, use the default message */
        if (!action.ofKind(LockWithAction)
            && !action.ofKind(UnlockWithAction))
        {
            /* for anything but our special cases, use the default handling */
            return inherited(whichObj, action, resolvedAllObjects);
        }

        /* use no announcement */
        return '';
    }
    
    /* 
     *   Allow locking or unlocking an object with a keyring.  This will
     *   automatically try each key on the keyring to see if it fits the
     *   lock. 
     */
    iobjFor(LockWith)
    {
        verify()
        {
            /* if we don't have any keys, we're not locking anything */
            if (contents.length() == 0)
                illogical(&cannotLockWithMsg);

            /* 
             *   if we know the direct object, and we don't have any keys
             *   that are plausible for the direct object, we're an
             *   unlikely match 
             */
            if (gDobj != nil)
            {
                local foundPlausibleKey;
                
                /* 
                 *   try each of my keys to see if it's plausible for the
                 *   direct object 
                 */
                foundPlausibleKey = nil;
                foreach (local cur in contents)
                {
                    /* 
                     *   if this is a plausible key, note that we have at
                     *   least one plausible key 
                     */
                    if (gDobj.keyIsPlausible(cur))
                    {
                        /* note that we found a plausible key */
                        foundPlausibleKey = true;

                        /* no need to look any further - one is good enough */
                        break;
                    }
                }

                /* 
                 *   If we didn't find a plausible key, we're an unlikely
                 *   match.
                 *   
                 *   If we did find a plausible key, increase the
                 *   likelihood that this is the indirect object so that
                 *   it's greater than the likelihood for any random key
                 *   that's plausible for the lock (which has the default
                 *   likelihood of 100), but less than the likelihood of
                 *   the known good key (which is 150).  This will cause a
                 *   keyring to be taken as a default over any ordinary
                 *   key, but will cause the correct key to override the
                 *   keyring as the default if the correct key is known to
                 *   the player already.  
                 */
                if (foundPlausibleKey)
                    logicalRank(140, 'keyring with plausible key');
                else
                    logicalRank(50, 'no plausible key');
            }
        }

        action()
        {
            local key;

            /* 
             *   Try finding a working key.  If we find one, replace the
             *   command with 'lock <lock> with <key>, so that we have the
             *   full effect of the 'lock with' command using the key
             *   itself. 
             */
            if ((key = findWorkingKey(gDobj)) != nil)
                replaceAction(LockWith, gDobj, key);
        }
    }

    iobjFor(UnlockWith)
    {
        /* verify the same as for LockWith */
        verify()
        {
            /* if we don't have any keys, we're not unlocking anything */
            if (contents.length() == 0)
                illogical(&cannotUnlockWithMsg);
            else
                verifyIobjLockWith();
        }

        action()
        {
            local key;

            /* 
             *   if we can find a working key, run an 'unlock with' action
             *   using the key 
             */
            if ((key = findWorkingKey(gDobj)) != nil)
                replaceAction(UnlockWith, gDobj, key);
        }
    }
;

/*
 *   Key - this is an object that can be used to unlock things, and which
 *   can be stored on a keyring.  The key that unlocks a lock is
 *   identified with a property on the lock, not on the key.
 */
class Key: Thing
    /*
     *   A key on a keyring that is being held by an actor is considered
     *   to be held by the actor, since the key does not have to be
     *   removed from the keyring in order to be manipulated as though it
     *   were directly held.  
     */
    isHeldBy(actor)
    {
        /* 
         *   if I'm on a keyring, I'm being held if the keyring is being
         *   held; otherwise, use the default definition 
         */
        if (location != nil && location.ofKind(Keyring))
            return location.isHeldBy(actor);
        else
            return inherited(actor);
    }

    /*
     *   Try making the current command's actor hold me.  If we're on a
     *   keyring, we'll simply try to make the keyring itself held, rather
     *   than taking the key off the keyring; otherwise, we'll inherit the
     *   default behavior to make ourselves held.  
     */
    tryHolding()
    {
        if (location != nil && location.ofKind(Keyring))
            return location.tryHolding();
        else
            return inherited();
    }

    /* -------------------------------------------------------------------- */
    /*
     *   Action processing 
     */

    /* treat "detach key" as "take key" if it's on a keyring */
    dobjFor(Detach)
    {
        verify()
        {
            /* if I'm not on a keyring, there's nothing to detach from */
            if (location == nil || !location.ofKind(Keyring))
                illogical(&keyNotDetachableMsg);
        }
        remap()
        {
            /* if I'm on a keyring, remap to "take self" */
            if (location != nil && location.ofKind(Keyring))
                return [TakeAction, self];
            else
                return inherited();
        }
    }

    /* "lock with" */
    iobjFor(LockWith)
    {
        verify()
        {
            /* 
             *   if we know the direct object is a LockableWithKey, we can
             *   perform some additional checks on the likelihood of this
             *   key being the intended key for the lock 
             */
            if (gDobj != nil
                && gDobj.ofKind(LockableWithKey))
            {
                /*
                 *   If the player should know that we're the key for the
                 *   lock, boost our likelihood so that we'll be picked
                 *   out automatically from an ambiguous set of keys.  
                 */
                if (gDobj.isKeyKnown(self))
                    logicalRank(150, 'known key');

                /* 
                 *   if this isn't a plausible key for the lockable, it's
                 *   unlikely that this is a match 
                 */
                if (!gDobj.keyIsPlausible(self))
                    illogical(keyNotPlausibleMsg);
            }
        }
    }

    /* 
     *   the message to use when the key is obviously not plausible for a
     *   given lock 
     */
    keyNotPlausibleMsg = &keyDoesNotFitLockMsg

    /* "unlock with" */
    iobjFor(UnlockWith)
    {
        verify()
        {
            /* use the same key selection we use for "lock with" */
            verifyIobjLockWith();
        }
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   A Dispenser is a container for a special type of item, such as a book
 *   of matches or a box of candy.
 */
class Dispenser: Container
    /* 
     *   Can we return one of our items to the dispenser once the item is
     *   dispensed?  Books of matches wouldn't generally allow this, since
     *   a match must be torn out to be removed, but simple box dispensers
     *   probably would.  By default, we won't allow returning an item
     *   once dispensed.  
     */
    canReturnItem = nil

    /* 
     *   Is the item one of the types of items we dispense?  Normally, we
     *   dispense identical items, so our default implementation simply
     *   determines if the item is an instance of our dispensable class.
     *   If the dispenser can hand out items of multiple, unrelated
     *   classes, this can be overridden to use a different means of
     *   identifying the dispensed items.  
     */
    isMyItem(obj) { return obj.ofKind(myItemClass); }

    /*
     *   The class of items we dispense.  This is used by the default
     *   implementation of isMyItem(), so subclasses that inherit that
     *   implementation should provide the appropriate base class here.  
     */
    myItemClass = Dispensable

    /* "put in" indirect object handler */
    iobjFor(PutIn)
    {
        verify()
        {
            /* if we know the direct object, consider it further */
            if (gDobj != nil)
            {
                /* if we don't allow returning our items, don't allow it */
                if (!canReturnItem && isMyItem(gDobj))
                    illogical(&cannotReturnToDispenserMsg);

                /* if it's not my dispensed item, it can't go in here */
                if (!isMyItem(gDobj))
                    illogical(&cannotPutInDispenserMsg);
            }

            /* inherit default handling */
            inherited();
        }
    }
;

/*
 *   A Dispensable is an item that comes from a Dispenser.  This is in
 *   most respects an ordinary item; the only special thing about it is
 *   that if we're still in our dispenser, we're an unlikely match for any
 *   command except "take" and the like.  
 */
class Dispensable: Thing
    /* 
     *   My dispenser.  This is usually my initial location, so by default
     *   we'll pre-initialize this to our location. 
     */
    myDispenser = nil

    /* pre-initialization */
    initializeThing()
    {
        /* inherit the default initialization */
        inherited();

        /* 
         *   We're usually in our dispenser initially, so assume that our
         *   dispenser is simply our initial location.  If myDispenser is
         *   overridden in a subclass, don't overwrite the inherited
         *   value.  
         */
        if (propType(&myDispenser) == TypeNil)
            myDispenser = location;
    }

    dobjFor(All)
    {
        verify()
        {
            /* 
             *   If we're in our dispenser, and the command isn't "take"
             *   or "take from", reduce our disambiguation likelihood -
             *   it's more likely that the actor is referring to another
             *   equivalent item that they've already removed from the
             *   dispenser.  
             */
            if (isIn(myDispenser)
                && !gActionIs(Take) && !gActionIs(TakeFrom))
            {
                /* we're in our dispenser - reduce the likelihood */
                logicalRank(60, 'in dispenser');
            }
        }
    }
;


/* ------------------------------------------------------------------------ */
/*
 *   A Matchbook is a special dispenser for matches. 
 */
class Matchbook: Collective, Openable, Dispenser
    /* we cannot return a match to a matchbook */
    canReturnItem = nil

    /* 
     *   we dispense matches (subclasses can override this if they want to
     *   dispense a specialized match subclass) 
     */
    myItemClass = Matchstick

    /*
     *   Act as a collective for any items within me.  This will have no
     *   effect unless we also have a plural name that matches that of the
     *   contained items.
     *   
     *   It is usually desirable for a matchbook to act as a collective
     *   for the contained items, so that a command like "take matches"
     *   will be taken to apply to the matchbook rather than the
     *   individual matches.  
     */
    isCollectiveFor(obj) { return obj.isIn(self); }

    /*
     *   Append my directly-held contents to a vector when I'm directly
     *   held.  When the matchbook is open, append our matches, because we
     *   consider the matches to be effectively attached to the matchbook
     *   (rather than contained within it).  
     */
    appendHeldContents(vec)
    {
        /* if we're open, append our contents */
        if (isOpen)
            vec.appendUnique(contents);
    }
;

/*
 *   A FireSource is an object that can set another object on fire.  This
 *   is a mix-in class that can be used with other classes.  
 */
class FireSource: object
    /* 
     *   We can use a fire source to light another object, provided the
     *   fire source is itself burning.  We don't provide any action
     *   handling - we leave that to the direct object.  
     */
    iobjFor(BurnWith)
    {
        preCond = [objHeld, objBurning]
        verify()
        {
            /* don't allow using me to light myself */
            if (gDobj == self)
                illogicalNow(&cannotBurnDobjWithMsg);

            /* 
             *   If we're already lit, make this an especially good choice
             *   for lighting other objects - this will ensure that we
             *   choose this over a match that isn't already lit, which is
             *   what you'd normally want to do to avoid wasting a match.
             *   
             *   Note that our ranking is specifically coordinated with
             *   that used by Matchstick.  We'll use a lit match over any
             *   normal FireSource (rank 160); we'll use a lit FireSource
             *   (rank 150) over an unlit match (rank 140).
             *   
             *   If we're not lit, make the action non-obvious so that
             *   we're not taken as a default to light another object on
             *   fire.  We *could* light something once we're lit, but that
             *   presumes there's a way to light me in the first place,
             *   which might require yet another object (a match, for
             *   example) - so ignore me as a default if we're not already
             *   lit, and go directly to some other object.  This should be
             *   overridden for self-lighting objects such as matches.  
             */
            if (isLit)
                logicalRank(150, 'fire source');
            else
                nonObvious;
        }
    }
;

/*
 *   A Matchstick is a self-igniting match from a matchbook.  (We use this
 *   lengthy name rather than simply "Match" because the latter is too
 *   generic, and could be taken by a casual reader for an object
 *   representing a successful search result or the like.)  
 */
class Matchstick: FireSource, LightSource
    /* matches have fairly feeble light */
    brightnessOn = 2

    /* not lit initially */
    isLit = nil

    /* amount of time we burn, in turns */
    burnLength = 2

    /* default long description describes burning status */
    desc()
    {
        if (isLit)
            gLibMessages.litMatchDesc(self);
        else
            gLibMessages.unlitMatchDesc(self);
    }

    /* get our state */
    getState = (isLit ? matchStateLit : matchStateUnlit)

    /* get a list of all states */
    allStates = [matchStateLit, matchStateUnlit]
    
    /* "burn" action */
    dobjFor(Burn)
    {
        preCond = [objHeld]
        verify()
        {
            /* can't light a match that's already burning */
            if (isLit)
                illogicalAlready(&alreadyBurningMsg);
        }
        action()
        {
            local t;
            
            /* describe it */
            defaultReport(&okayBurnMatchMsg);

            /* make myself lit */
            makeLit(true);

            /* get our default burn length */
            t = burnLength;

            /* 
             *   if this is an implicit command, reduce the burn length by
             *   one turn - this ensures that the player can't
             *   artificially extend the match's useful life by doing
             *   something that implicitly lights the match 
             */
            if (gAction.isImplicit)
                --t;

            /* start our burn-out timer going */
            new SenseFuse(self, &matchBurnedOut, t, self, sight);
        }
    }

    iobjFor(BurnWith)
    {
        verify()
        {
            /*
             *   Whether or not a match is burning, it's an especially
             *   good choice to light something else on fire.  Make it
             *   even more likely when it's burning already.
             *   
             *   Note that this is specifically coordinated with the base
             *   FireSource ranking.  We'll pick a lit match (160) over an
             *   ordinary lit FireSource (150), but we'll pick a lit
             *   FireSource (150) over an unlit match (140).  This will
             *   avoid consuming a match that's not already lit when
             *   another fire source is already available.  
             */
            logicalRank(isLit ? 160 : 140, 'fire source');
        }
    }

    /* "extinguish" */
    dobjFor(Extinguish)
    {
        verify()
        {
            /* can't extinguish a match that isn't burning */
            if (!isLit)
                illogicalAlready(&matchNotLitMsg);
        }
        action()
        {
            /* describe the match going out */
            defaultReport(&okayExtinguishMatchMsg);

            /* no longer lit */
            makeLit(nil);

            /* remove the match from the game */
            moveInto(nil);
        }
    }

    /* fuse handler for burning out */
    matchBurnedOut()
    {
        /* 
         *   if I'm not still burning, I must have been extinguished
         *   explicitly already, so there's nothing to do 
         */
        if (!isLit)
            return;
        
        /* make sure we separate any output from other commands */
        "<.p>";

        /* report that we're done burning */
        gLibMessages.matchBurnedOut(self);

        /* 
         *   remove myself from the game (for simplicity, a match simply
         *   disappears when it's done burning) 
         */
        moveInto(nil);
    }

    /* matches usually come in bunches of equivalents */
    isEquivalent = true
;

/*
 *   A light source that produces light using a fuel supply.  This kind of
 *   light source uses a daemon to consume fuel whenever it's lit.
 */
class FueledLightSource: LightSource
    /* provide a bright light by default */
    brightnessOn = 3

    /* not lit initially */
    isLit = nil

    /*
     *   Our fuel source object.  If desired, this can be set to a
     *   separate object to model the fuel supply separately from the
     *   light source itself; for example, you could set this to point to
     *   a battery, or to a vial of oil.  By default, for simplicity, the
     *   fuel supply and light source are the same object.
     *   
     *   The fuel supply object must expose two methods: getFuelLevel()
     *   and consumeFuel().  
     */
    fuelSource = (self)

    /* 
     *   Get my fuel level, and consume fuel.  We use these methods only
     *   when we're our own fuelSource (which we are by default).  When
     *   we're not our own fuel source, the fuel source object must
     *   provide these methods instead of us.
     *   
     *   Our fuel level is the number of turns that we can continue to
     *   burn.  Each turn we're lit, we'll reduce the fuel level by one.
     *   We'll automatically extinguish ourself when the fuel level
     *   reaches zero.
     *   
     *   If the light source can burn forever, simply return nil as the
     *   fuel level.  
     */
    getFuelLevel() { return fuelLevel; }
    consumeFuel(amount) { fuelLevel -= amount; }

    /* our fuel level - we use this when we're our own fuel source */
    fuelLevel = 20

    /* light or extinguish */
    makeLit(lit)
    {
        /* if the current fuel level is zero, we can't be lit */
        if (lit && fuelSource.getFuelLevel() == 0)
            return;

        /* inherit the default handling */
        inherited(lit);

        /* if we're lit, activate our daemon; otherwise, stop our daemon */
        if (isLit)
        {
            /* start our burn daemon going */
            burnDaemonObj =
                new SenseDaemon(self, &burnDaemon, 1, self, sight);
        }
        else
        {
            /* stop our daemon */
            eventManager.removeEvent(burnDaemonObj);

            /* forget out daemon */
            burnDaemonObj = nil;
        }
    }

    /* burn daemon - this is called on each turn while we're burning */
    burnDaemon()
    {
        local level = fuelSource.getFuelLevel();
        
        /* if we use fuel, consume one increment of fuel for this turn */
        if (level != nil)
        {
            /* 
             *   If our fuel level has reached zero, stop burning.  Note
             *   that the daemon is called on the first turn after we
             *   start burning, so we must go through a turn with the fuel
             *   level at zero before we stop burning.  
             */
            if (level == 0)
            {
                /* make sure we separate any output from other commands */
                "<.p>";

                /* mention that the candle goes out */
                sayBurnedOut();

                /* 
                 *   Extinguish the candle.  Note that we do this *after*
                 *   we've already displayed the message about the candle
                 *   burning out, because that message is displayed in our
                 *   own sight context.  If we're the only light source
                 *   present, then we're invisible once we're not providing
                 *   light, so our message about burning out would be
                 *   suppressed if we displayed it after cutting off our
                 *   own light.  To make sure we can see the message, wait
                 *   until after the message to cut off our light.  
                 */
                makeLit(nil);
            }
            else
            {
                /* reduce our fuel level by one */
                fuelSource.consumeFuel(1);
            }
        }
    }

    /* mention that we've just burned out */
    sayBurnedOut() { gLibMessages.objBurnedOut(self); }

    /* our daemon object, valid while we're burning */
    burnDaemonObj = nil
;

/*
 *   A candle is an item that can be set on fire for a controlled burn.
 *   Although we call this a candle, this class can be used for other types
 *   of fuel burners, such as torches and oil lanterns.
 *   
 *   Ordinary candles are usually fire sources as well, in that you can
 *   light one candle with another once the first one is lit.  To get this
 *   effect, mix FireSource into the superclass list (but put it before
 *   Candle, since FireSource is specifically designed as a mix-in class).
 */
class Candle: FueledLightSource
    /* 
     *   The message we display when we try to light the candle and we're
     *   out of fuel.  This message can be overridden by subclasses that
     *   don't fit the default message.  
     */
    outOfFuelMsg = &candleOutOfFuelMsg

    /* the message we display when we successfully light the candle */
    okayBurnMsg = &okayBurnCandleMsg

    /* show a message when the candle runs out fuel while burning */
    sayBurnedOut()
    {
        /* by default, show our standard library message */
        gLibMessages.candleBurnedOut(self);
    }

    /* 
     *   Determine if I can be lit with the specific indirect object.  By
     *   default, we'll allow any object to light us if the object passes
     *   the normal checks applied by its own iobjFor(BurnWith) handlers.
     *   This can be overridden if we can only be lit with specific
     *   sources of fire; for example, a furnace with a deeply-recessed
     *   burner could refuse to be lit by anything but particular long
     *   matches, or a particular type of fuel could refuse to be lit
     *   except by certain especially hot flames.  
     */
    canLightWith(obj) { return true; }

    /* 
     *   Default long description describes burning status.  In most
     *   cases, this should be overridden to provide more details, such as
     *   information on our fuel level. 
     */
    desc()
    {
        if (isLit)
            gLibMessages.litCandleDesc(self);
        else
            inherited();
    }

    /* "burn with" action */
    dobjFor(BurnWith)
    {
        preCond = [touchObj]
        verify()
        {
            /* can't light it if it's already lit */
            if (isLit)
                illogicalAlready(&alreadyBurningMsg);
        }
        check()
        {
            /* 
             *   make sure the object being used to light us is a valid
             *   source of fire for us 
             */
            if (!canLightWith(obj))
            {
                reportFailure(&cannotBurnDobjWithMsg);
                exit;
            }

            /* if the fuel level is zero, we can't be lit */
            if (fuelSource.getFuelLevel() == 0)
            {
                reportFailure(outOfFuelMsg);
                exit;
            }
        }
        action()
        {
            /* make myself lit */
            makeLit(true);

            /* describe it */
            defaultReport(okayBurnMsg);
        }
    }

    /* "extinguish" */
    dobjFor(Extinguish)
    {
        verify()
        {
            /* can't extinguish a match that isn't burning */
            if (!isLit)
                illogicalAlready(&candleNotLitMsg);
        }
        action()
        {
            /* describe the match going out */
            defaultReport(&okayExtinguishCandleMsg);

            /* no longer lit */
            makeLit(nil);
        }
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   "Tour Guide" is a mix-in class for Actors.  This class can be
 *   multiply inherited by objects along with Actor or a subclass of
 *   Actor.  This mix-in makes the Follow action, when applied to the tour
 *   guide, initiate travel according to where the tour guide wants to go
 *   next.  So, if the tour guide is here and is waving us through the
 *   door, FOLLOW GUIDE will initiate travel through the door.
 *   
 *   This class should appear in the superclass list ahead of Actor or the
 *   Actor subclass.  
 */
class TourGuide: object
    dobjFor(Follow)
    {
        verify()
        {
            /*
             *   If the actor can see us, and we're in a "guided tour"
             *   state, we can definitely perform the travel.  Otherwise,
             *   use the standard "follow" behavior. 
             */
            if (gActor.canSee(self) && getTourDest() != nil)
            {
                /* 
                 *   we're waiting to show the actor to the next stop on
                 *   the tour, so we can definitely proceed with this
                 *   action 
                 */
            }
            else
            {
                /* we're not in a tour state, so use the standard handling */
                inherited();
            }
        }

        action()
        {
            local dest;
            
            /* 
             *   if we're in a guided tour state, initiate travel to our
             *   escort destination; otherwise, use the standard handling 
             */
            if (gActor.canSee(self) && (dest = getTourDest()) != nil)
            {
                /* initiate travel to our destination */
                replaceAction(TravelVia, dest);
                return;
            }
            else
            {
                /* no tour state; use the standard handling */
                inherited();
            }
        }
    }

    /*
     *   Get the travel connector that takes us to our next guided tour
     *   destination.  By default, this returns the escortDest from our
     *   current actor state if our state is a guided tour state, or nil
     *   if our state is any other kind of state.  Subclasses must
     *   override this if they use other kinds of states to represent
     *   guided tours, since we'll only detect that we're in a guided tour
     *   state if our current actor state object is of class
     *   GuidedTourState (or any subclass).  
     */
    getTourDest()
    {
        return (curState.ofKind(GuidedTourState)
                ? curState.escortDest
                : nil);
    }
;

/*
 *   Guided Tour state.  This provides a simple way of defining a "guided
 *   tour," which is a series of locations to which we try to guide the
 *   player character.  We don't force the player character to travel as
 *   specified; we merely try to lead the player.  The actual travel is up
 *   to the player.
 *   
 *   Here's how this works.  For each location on the guided tour, create
 *   one of these state objects.  Set escortDest to the travel connector
 *   to which we're attempting to guide the player character from the
 *   current location.  Set stateAfterEscort to the state object for the
 *   next location on the tour.  Set stateDesc to something indicating
 *   that we're trying to show the player to the next stop - something
 *   along the lines of "Bob waits for you by the door."  Set
 *   arrivingWithDesc to a message indicating that we just showed up in
 *   the current location and are ready to show the player to the next -
 *   "Bob goes to the door and waits for you to follow him."  
 */
class GuidedTourState: AccompanyingState
    /* the travel connector we're trying to show the player into */
    escortDest = nil

    /* 
     *   The next state for our actor to assume after the travel.  This
     *   should be overridden and set to the state object for the next
     *   stop on the tour. 
     */
    stateAfterEscort = nil

    /* the actor we're escorting - this is usually the player character */
    escortActor = (gPlayerChar)

    /* 
     *   The class we use for our actor state during the escort travel.
     *   By default, we use the basic guided-tour accompanying travel
     *   state class, but games will probably want to use a customized
     *   subclass of this basic class in most cases.  The main reason to
     *   use a custom subclass is to provide customized messages to
     *   describe the departure of the escorting actor.  
     */
    escortStateClass = GuidedInTravelState
    
    /* 
     *   we should accompany the travel if the actor we're guiding will be
     *   traveling, and they're traveling to the next stop on our tour 
     */
    accompanyTravel(traveler, conn)
    {
        return (traveler.isActorTraveling(escortActor) && conn == escortDest);
    }

    /* 
     *   get our accompanying state object - we'll create an instance of
     *   the class specified in our escortStateClass property 
     */
    getAccompanyingTravelState(traveler, conn)
    {
        return escortStateClass.createInstance(
            location, gActor, stateAfterEscort);
    }
;

/*
 *   A subclass of the basic accompanying travel state specifically
 *   designed for guided tours.  This is almost the same as the basic
 *   accompanying travel state, but provides customized messages to
 *   describe the departure of our associated actor, which is the actor
 *   serving as the tour guide.  
 */
class GuidedInTravelState: AccompanyingInTravelState
    sayDeparting(conn)
        { gLibMessages.sayDepartingWithGuide(location, leadActor); }
;

/* ------------------------------------------------------------------------ */
/*
 *   An Attachable is an object that can be attached to another, using an
 *   ATTACH X TO Y command.  This is a mix-in class that is meant to be
 *   combined with a Thing-derived class to create an attachable object.
 *   
 *   Attachment is symmetrical: we can only attach to other Attachable
 *   objects.  As a result, the verb handling for ATTACH can be performed
 *   symmetrically - ATTACH X TO Y is handled the same way as ATTACH Y TO
 *   X.  Sometimes reversing the roles makes the command nonsensical, but
 *   when the reversal makes sense, it seems unlikely that it'll ever
 *   change the meaning of the command.  This makes it program the verb
 *   handling, because it means that we can designate one of X or Y as the
 *   handler for the verb, and just write the code once there.  Refer to
 *   the handleAttach() method to see how this works.
 *   
 *   There's an important detail that we leave to instances, because
 *   there's no good general rule we can implement.  Specifically, there's
 *   the matter of imposing appropriate constraints on the relative
 *   locations of objects once they're attached to one another.  There are
 *   numerous anomalies that become possible once two objects are attached.
 *   Consider the example of a battery connected to a jumper cable that's
 *   in turn connected to a lamp:
 *   
 *   - if we put the battery in a box but leave the lamp outside the box,
 *   we shouldn't be able to close the lid of the box all the way without
 *   breaking the cables
 *   
 *   - if we're carrying the battery but not the lamp, traveling to a new
 *   room should drag the lamp along
 *   
 *   - if we drop the battery down a well, the lamp should be dragged down
 *   with it
 *   
 *   Our world model isn't sophisticated enough to properly model an
 *   attachment relationship, so it can't deal with these contingencies by
 *   proper physical simulation.  Which is why we have to leave these for
 *   the game to handle.
 *   
 *   There are two main strategies you can apply to handle these problems.
 *   
 *   First, you can impose limits that prevent these sorts of situations
 *   from coming up in the first place, either by carefully designing the
 *   scenario so they simply don't come up, or by imposing more or less
 *   artificial constraints.  For example, you could solve all of the
 *   problems above by eliminating the jumper cable and attaching the lamp
 *   directly to the battery, or by making the jumper cable very short.
 *   Anything attached to the battery would effectively become located "in"
 *   the battery, so it would move everywhere along with the battery
 *   automatically.  Detaching the lamp would move the lamp back outside
 *   the battery, and conversely, moving the lamp out of the battery would
 *   detach the objects.
 *   
 *   Second, you can detect the anomalous cases and handle them explicitly
 *   with special-purpose code.  You could use beforeAction and afterAction
 *   methods on one of the attached objects, for example, to detect the
 *   various problematic actions, either blocking them or implementing
 *   appropriate consequences.
 *   
 *   Given the number of difficult anomalies possible with rope-like
 *   objects, the second approach is challenging on its own.  However, it
 *   often helps to combine it with the first approach, limiting the
 *   scenario.  In other words, you'd limit the scenario to some extent,
 *   but not totally: rather than completely excising the difficult
 *   behavior, you'd narrow it down to a manageable subset of the full
 *   range of real-world possibilities; then, you'd deal with the remaining
 *   anomalies on a case-by-case basis.  For example, you could make the
 *   battery too heavy to carry, which would guarantee that it would never
 *   be put in a box, thrown down a well, or carried out of the room.  That
 *   would only leave a few issues: walking away while carrying the plugged
 *   in lamp, which could be handled with an afterAction that severs the
 *   attachment; putting the lamp in a box and closing the box, which could
 *   be handled with a beforeAction by blocking Close actions whenever the
 *   lamp is inside the object being closed.  
 */
class Attachable: object
    /* 
     *   The list of objects I'm currently attached to.  Note that each of
     *   the objects in this list must usually be an Attachable, and we
     *   must be included in the attachedObjects list in each of these
     *   objects.  
     */
    attachedObjects = []

    /*
     *   Perform programmatic attachment, without any notifications.  This
     *   simply updates my attachedObjects list and the other object's list
     *   to indicate that we're attached to the other object (and vice
     *   versa). 
     */
    attachTo(obj)
    {
        attachedObjects += obj;
        obj.attachedObjects += self;
    }

    /* perform programmatic detachment, without any notifications */
    detachFrom(obj)
    {
        attachedObjects -= obj;
        obj.attachedObjects -= self;
    }

    /* get the subset of my attachments that are non-permanent */
    getNonPermanentAttachments()
    {
        /* return the subset of objects not permanently attached */
        return attachedObjects.subset({x: !isPermanentlyAttachedTo(x)});
    }

    /* am I attached to the given object? */
    isAttachedTo(obj)
    {
        /* we are attached to the other object if it's in our list */
        return (attachedObjects.indexOf(obj) != nil);
    }

    /*
     *   Am I the "major" item in my attachment relationship to the given
     *   object?  This affects how our relationship is described in our
     *   status message: in an asymmetrical relationship, where one object
     *   is the "major" item, we will always describe the minor item as
     *   being attached to the major item rather than vice versa.  This
     *   allows you to ensure that the message is always "the sign is
     *   attached to the wall", and never "the wall is attached to the
     *   sign": the wall is the major item in this relationship, so it's
     *   always the sign that's attached to it.
     *   
     *   By default, we always return nil here, which means that
     *   attachment relationships are symmetrical by default.  In a
     *   symmetrical relationship, we'll describe the other things as
     *   attached to 'self' when describing self.  
     */
    isMajorItemFor(obj) { return nil; }

    /*
     *   Am I *listed* as attached to the given object?  If this is true,
     *   then our examineStatus() will list 'obj' among the things I'm
     *   attached to: "Self is attached to obj."  If this is nil, I'm not
     *   listed as attached.
     *   
     *   By default, we're listed if (1) we're not permanently attached to
     *   'obj', AND (2) we're not the "major" item in the attachment
     *   relationship.  The reason we're not listed if we're permanently
     *   attached is that the attachment information is presumably better
     *   handled via the fixed description of the object rather than in
     *   the extra status message; this is analogous to the way immovable
     *   items (such as Fixtures) aren't normally listed in the
     *   description of a room.  The reason we're not listed if we're the
     *   "major" item in the relationship is that the "major" status
     *   reverses the relationship: when we're the major item, the other
     *   item is described as attached to *us*, rather than vice versa.  
     */
    isListedAsAttachedTo(obj)
    {
        /* 
         *   only list the item if it's not permanently attached, and
         *   we're not the "major" item for the object 
         */
        return (!isPermanentlyAttachedTo(obj) && !isMajorItemFor(obj));
    }

    /*
     *   Is 'obj' listed as attached to me when I'm described?  If this is
     *   true, then our examineStatus() will list 'obj' among the things
     *   attached to me: "Attached to self is obj."  If this is nil, then
     *   'obj' is not listed among the things attached to me when I'm
     *   described.
     *   
     *   This routine is simply the "major" list counterpart of
     *   isListedAsAttachedTo().
     *   
     *   By default, we list 'obj' among my attachments if (1) I'm the
     *   "major" item for 'obj', AND (2) 'obj' is listed as attached to
     *   me, as indicated by obj.isListedAsAttachedTo(self).  We only list
     *   our minor attachments here, because we list all of our other
     *   listable attachments separately, as the things I'm attached to.
     *   We also only list items that are themselves listable as
     *   attachments, for obvious reasons.  
     */
    isListedAsMajorFor(obj)
    {
        /* 
         *   only list the item if we're the "major" item for the object,
         *   and the object is itself listable as an attachment 
         */
        return (isMajorItemFor(obj) && obj.isListedAsAttachedTo(self));
    }
    
    /*
     *   Can I attach to the given object?  This returns true if the other
     *   object is allowable as an attachment, nil if not.
     *   
     *   By default, we look to see if the other side is an Attachable, and
     *   if so, if it overrides canAttachTo(); if so, we'll call its
     *   canAttachTo to ask whether it thinks it can attach to us.  If the
     *   other side doesn't override this, we'll simply return nil.  This
     *   arrangement is convenient because it means that only one side of
     *   an attachable pair needs to implement this; the other side will
     *   automatically figure it out by calling the first side and relying
     *   on the symmetry of the relationship.  
     */
    canAttachTo(obj)
    {
        /* 
         *   if the other side's an Attachable, and it overrides this
         *   method, call the override; if not, it's by default not one of
         *   our valid attachments 
         */
        if (overrides(obj, Attachable, &canAttachTo))
        {
            /* 
             *   the other side is an Attachable that defines a specific
             *   attachment rule, so ask the other side if it thinks we're
             *   one of its attachments; by the symmetry of the
             *   relationship, if we're one of its attachments, then it's
             *   one of ours 
             */
            return obj.canAttachTo(self);
        }
        else
        {
            /* 
             *   the other side doesn't want to tell us, so we're on our
             *   own; we don't recognize any attachments on our own, so
             *   it's not a valid attachment 
             */
            return nil;
        }
    }

    /*
     *   Explain why we can't attach to the given object.  This should
     *   simply display an appropriate mesage.  We use reportFailure to
     *   flag it as a failure report, but that's not actually required,
     *   since we call this from our 'check' routine, which will mark the
     *   action as having failed even if we don't here.  
     */
    explainCannotAttachTo(obj) { reportFailure(&wrongAttachmentMsg); }

    /*
     *   Is it possible for me to detach from the given object?  This asks
     *   whether a given attachment relationship can be dissolved with
     *   DETACH FROM. 
     *   
     *   By default, we'll use similar logic to canAttachTo: if the other
     *   object overrides canDetachFrom(), we'll let it make the
     *   determination.  Otherwise, we'll return nil if one or the other
     *   side is a PermanentAttachment, true if not.  This lets you prevent
     *   detachment by overriding canDetachFrom() on just one side of the
     *   relationship.  
     */
    canDetachFrom(obj)
    {
        /* if the other object overrides canDetachFrom, defer to it */
        if (overrides(obj, Attachable, &canDetachFrom))
        {
            /* let the other side make the judgment */
            return obj.canDetachFrom(self);
        }
        else
        {
            /* 
             *   the other side doesn't override it, so assume we can
             *   detach unless one or the other side is a
             *   PermanentAttachment 
             */
            return !isPermanentlyAttachedTo(obj);
        }
    }

    /*
     *   Am I permanently attached to the other object?  This returns true
     *   if I'm a PermanentAttachment or the other object is.  
     */
    isPermanentlyAttachedTo(obj)
    {
        /* 
         *   if either one of us is a PermanentAttachment, we're
         *   permanently attached to each other 
         */
        return ofKind(PermanentAttachment) || obj.ofKind(PermanentAttachment);
    }
    

    /* 
     *   A message explaining why we can't detach from the given object.
     *   Note that 'obj' can be nil, because we could be attempting a
     *   DETACH command with no indirect object.  
     */
    cannotDetachMsgFor(obj)
    {
        /* 
         *   if we have an object, it must be the wrong one; otherwise, we
         *   simply can't detach generically, since the object to detach
         *   from wasn't specified, and there's nothing obvious we can
         *   detach from 
         */
        return obj != nil ? &wrongDetachmentMsg : &cannotDetachMsg;
    }

    /*
     *   Process attachment to a new object.  This routine is called on
     *   BOTH the direct and indirect object during the attachment process
     *   - that is, it's called on the direct object with the indirect
     *   object as the argument, and then it's called on the indirect
     *   object with the direct object as the argument.
     *   
     *   This symmetrical handling makes it easy to handle the frequent
     *   cases where the player might say ATTACH X TO Y or ATTACH Y TO X
     *   and mean the same thing either way.  Because this method is called
     *   for both X and Y in either phrasing, you can simply choose to
     *   write the handler code in either X or Y - you only have to write
     *   it once, because the handler will be called on each of the
     *   objects, regardless of the phrasing.  So, if you choose to
     *   designate X as the official ATTACH handler, write a handleAttach()
     *   method on X, and leave the one on Y doing nothing: during
     *   execution, the X method will do its work, and the Y method will do
     *   nothing, so regardless of phrasing order, the net result will be
     *   the same.
     *   
     *   By default we do nothing.  Each instance should override this to
     *   display any extra message and take any extra action needed to
     *   process the attachment status change.  Note that the override
     *   doesn't need to worry about managing the attachedObjects list, as
     *   the main action handler does that automatically.
     *   
     *   Note that handleAttach() is always called after both objects have
     *   updated their attachedObjects lists.  This means that you can turn
     *   right around and detach the objects here, if you don't want to
     *   leave them attached.  
     */
    handleAttach(other)
    {
        /* do nothing by default */
    }

    /*
     *   Receive notification that this object or one of its attachments
     *   is being moved to a new container.  When an attached object is
     *   moved, we'll call this on the object being moved AND on every
     *   object attached to it.  'movedObj' is the object being moved, and
     *   'newCont' is the new container it's being moved into.
     *   
     *   By default we do nothing.  Instances can override this as needed.
     *   For example, if you wish to enforce a rule that this object and
     *   all of its attached objects share a common direct container, you
     *   could either block the move (by displaying an error and using
     *   'exit') or run a nested DetachFrom action to sever the attachment
     *   with the object being moved.  
     */
    moveWhileAttached(movedObj, newCont)
    {
        /* do nothing by default */
    }

    /*
     *   Receive notification that this object or one of its attachments is
     *   being moved in the course of an actor traveling to a new location.
     *   Whenever anyone travels while carrying an attachable object
     *   (directly or indirectly), we'll call this on the object being
     *   moved AND on every object attached to it.  'movedObj' is the
     *   object being carried by the traveling actor, 'traveler' is the
     *   Traveler performing the travel, and 'connector' is the
     *   TravelConnector that the traveler is traversing.
     *   
     *   By default, we do nothing.  Instances can override this as needed.
     */
    travelWhileAttached(movedObj, traveler, connector)
    {
        /* do nothing by default */
    }

    /*
     *   Handle detachment.  This works like handleAttach(), in that this
     *   routine is invoked symmetrically for both sides of a DETACH X FROM
     *   Y commands.
     *   
     *   As with handleAttach(), we do nothing by default, so instances
     *   should override as needed.  Note that the override doesn't need to
     *   worry about managing the attachedObjects list, as the main action
     *   handler does that automatically.  As with handleAttach(), this is
     *   called after the attachedObjects lists for both objects are
     *   updated.  
     */
    handleDetach(other)
    {
        /* do nothing by default */
    }

    /* the Lister we use to show our list of attached objects */
    attachmentLister = perInstance(new SimpleAttachmentLister(self))

    /* 
     *   the Lister we use to list the items attached to us (i.e., the
     *   items for which we're the "major" item in the attachment
     *   relationship) 
     */
    majorAttachmentLister = perInstance(new MajorAttachmentLister(self))

    /* add a list of our attachments to the desription */
    examineStatus()
    {
        local tab;
        
        /* inherit the normal status description */
        inherited();

        /* get the actor's visual sense table */
        tab = gActor.visibleInfoTable();

        /* add our list of attachments */
        attachmentLister.showList(gActor, self, attachedObjects,
                                  0, 0, tab, nil);

        /* add our list of major attachments */
        majorAttachmentLister.showList(gActor, self, attachedObjects,
                                       0, 0, tab, nil);
    }

    /* 
     *   Move into a new container.  If I'm attached to anything, we'll
     *   notify ourself and our attachments. 
     */
    mainMoveInto(newCont)
    {
        /* if I'm attached to anything, notify everyone */
        if (attachedObjects.length() != 0)
        {
            /* notify myself */
            moveWhileAttached(self, newCont);

            /* notify my attachments */
            attachedObjects.forEach({x: x.moveWhileAttached(self, newCont)});
        }

        /* inherit the base handling */
        inherited(newCont);
    }

    /*
     *   Receive notification of travel.  If I'm involved in the travel,
     *   and I'm attached to anything, we'll notify ourself and our
     *   attachments. 
     */
    beforeTravel(traveler, connector)
    {
        /* 
         *   If we're traveling with the traveler, and we're attached to
         *   anything, notify everything that's attached.
         */
        if (attachedObjects.length() != 0
            && traveler.isTravelerCarrying(self))
        {
            /* notify myself */
            travelWhileAttached(self, traveler, connector);

            /* notify each of my attachments */
            attachedObjects.forEach(
                {x: x.travelWhileAttached(self, traveler, connector)});
        }
    }

    /* 
     *   during initialization, make sure the attachedObjects list is
     *   symmetrical for both sides of the attachment relationship 
     */
    initializeThing()
    {
        /* do the normal work */
        inherited();

        /* 
         *   check to make sure that each of our attached objects points
         *   back at us 
         */
        foreach (local cur in attachedObjects)
        {
            /* 
             *   if we're not in this one's attachedObjects list, add
             *   ourselves to the list, so that everyone's consistent
             */
            if (cur.attachedObjects.indexOf(self) == nil)
                cur.attachedObjects += self;
        }
    }

    /* handle attachment on the direct object side */
    dobjFor(AttachTo)
    {
        /* require that the actor can touch the direct object */
        preCond = [touchObj]
        
        verify()
        {
            /* 
             *   it makes sense to attach to anything but myself, or things
             *   we're already attached to 
             */
            if (gIobj != nil)
            {
                if (isAttachedTo(gIobj))
                    illogicalAlready(&alreadyAttachedMsg);
                else if (gIobj == self)
                    illogicalSelf(&cannotAttachToSelfMsg);
            }
        }
        
        check()
        {
            /* only allow it if we can attach to the other object */
            if (!canAttachTo(gIobj))
            {
                explainCannotAttachTo(gIobj);
                exit;
            }
        }
        
        action()
        {
            /* add the other object to our list of attached objects */
            attachedObjects += gIobj;

            /* add our default acknowledgment */
            defaultReport(&okayAttachToMsg);

            /* fire the handleAttach event if we're ready */
            maybeHandleAttach(gIobj);
        }
    }

    /* handle attachment on the indirect object side */
    iobjFor(AttachTo)
    {
        /*
         *   Require that the direct object can touch the indirect object.
         *   This ensures that the two objects to be attached can touch
         *   one another.  Note that we don't also require that the actor
         *   be able to touch the indirect object directly, since it's
         *   good enough that (1) the actor can touch the direct object
         *   (which we enforce with the dobj precondition), and (2) the
         *   direct object can touch the indirect object.  This allows for
         *   odd things like plugging something into a recessed outlet,
         *   where the recessed bit can't be reached directly but can be
         *   reached using the plug.  
         */
        preCond = [dobjTouchObj]
        
        verify()
        {
            /* 
             *   it makes sense to attach to anything but myself, or things
             *   we're already attached to 
             */
            if (gDobj != nil)
            {
                if (isAttachedTo(gDobj))
                    illogicalAlready(&alreadyAttachedMsg);
                else if (gDobj == self)
                    illogicalSelf(&cannotAttachToSelfMsg);
            }
        }
        
        check()
        {
            /* only allow it if we can attach to the other object */
            if (!canAttachTo(gDobj))
            {
                explainCannotAttachTo(gDobj);
                exit;
            }
        }
        
        action()
        {
            /* add the other object to our list of attached objects */
            attachedObjects += gDobj;

            /* fire the handleAttach event if we're ready */
            maybeHandleAttach(gIobj);
        }
    }

    /* 
     *   Fire the handleAttach event - we'll notify both sides as soon as
     *   both sides are hooked up with each other.  This ensures that both
     *   lists are updated before we notify either side, so the ordering
     *   doesn't depend on whether we handle the dobj or iobj first. 
     */
    maybeHandleAttach(other)
    {
        /* if both lists are hooked up, send the notifications */
        if (attachedObjects.indexOf(other) != nil
            && other.attachedObjects.indexOf(self) != nil)
        {
            /* notify our side */
            handleAttach(other);

            /* notify the other side */
            other.handleAttach(self);
        }
    }

    /* handle simple, unspecified detachment (DETACH OBJECT) */
    dobjFor(Detach)
    {
        verify()
        {
            /* if I'm not attached to anything, this is illogical */
            if (attachedObjects.length() == 0)
                illogicalAlready(cannotDetachMsgFor(nil));
        }
        action()
        {
            local lst;

            /* get the non-permanent attachment subset */
            lst = getNonPermanentAttachments();

            /* check what that leaves us */
            if (lst.length() == 0)
            {
                /* 
                 *   we're not attached to anything that we can detach
                 *   from, so simply report that we can't detach
                 *   generically 
                 */
                reportFailure(cannotDetachMsgFor(nil));
            }
            else if (lst.length() == 1)
            {
                /* 
                 *   we have exactly one attached object from which we can
                 *   detach, so they must want to detach from that -
                 *   process this as DETACH FROM my one attached object 
                 */
                replaceAction(DetachFrom, self, lst[1]);
            }
            else
            {
                /* 
                 *   we have more than one detachable attachment, so ask
                 *   which one they mean 
                 */
                askForIobj(DetachFrom);
            }
        }
    }

    /* handle detaching me from a specific other object */
    dobjFor(DetachFrom)
    {
        verify()
        {
            /* it only makes sense to try detaching us from our attachments */
            if (gIobj != nil && !isAttachedTo(gIobj))
                illogicalAlready(&notAttachedToMsg);
        }
        check()
        {
            /* make sure I'm allowed to detach from the given object */
            if (!canDetachFrom(gIobj))
            {
                reportFailure(cannotDetachMsgFor(gIobj));
                exit;
            }
        }
        action()
        {
            /* remove the other object from our list of attached objects */
            attachedObjects -= gIobj;

            /* add our default acknowledgment */
            defaultReport(&okayDetachFromMsg);

            /* fire the handleDetach event if appropriate */
            maybeHandleDetach(gIobj);
        }
    }

    /* handle detachment on the indirect object side */
    iobjFor(DetachFrom)
    {
        verify()
        {
            /* it only makes sense to try detaching my attachments */
            if (gDobj == nil)
            {
                /* 
                 *   we don't know the dobj yet, but we can check the
                 *   tentative list for the possible set 
                 */
                if (gTentativeDobj
                    .indexWhich({x: isAttachedTo(x.obj_)}) == nil)
                    illogicalAlready(&notAttachedToMsg);
            }
            else if (gDobj != nil && !isAttachedTo(gDobj))
                illogicalAlready(&notAttachedToMsg);
        }
        check()
        {
            /* make sure I'm allowed to detach from the given object */
            if (!canDetachFrom(gDobj))
            {
                reportFailure(cannotDetachMsgFor(gDobj));
                exit;
            }
        }
        action()
        {
            /* remove the other object from our list of attached objects */
            attachedObjects -= gDobj;

            /* fire the handleDetach event if appropriate */
            maybeHandleDetach(gDobj);
        }
    }

    /* 
     *   Fire the handleDetach event - we'll notify both sides as soon as
     *   both sides are un-hooked up.  This ensures that both lists are
     *   updated before we notify either side, so the ordering doesn't
     *   depend on whether we handle the dobj or iobj first.  
     */
    maybeHandleDetach(other)
    {
        /* if both lists are un-hooked up, send the notifications */
        if (attachedObjects.indexOf(other) == nil
            && other.attachedObjects.indexOf(self) == nil)
        {
            /* notify our side */
            handleDetach(other);

            /* notify the other side */
            other.handleDetach(self);
        }
    }

    /* 
     *   TAKE X FROM Y is the same as DETACH X FROM Y for things we're
     *   attached to, but use the inherited handling otherwise 
     */
    dobjFor(TakeFrom)
    {
        verify()
        {
            /* 
             *   use the inherited handling only if we're not attached -
             *   if we're attached, consider it logical, overriding any
             *   containment relationship check we might otherwise make 
             */
            if (gIobj == nil || !isAttachedTo(gIobj))
                inherited();
        }
        check()
        {
            /* inherit the default check only if we're not attached */
            if (!isAttachedTo(gIobj))
                inherited();
        }
        action()
        {
            /* 
             *   if we're attached, change this into a DETACH FROM action;
             *   otherwise, use the inherited TAKE FROM handling 
             */
            if (isAttachedTo(gIobj))
                replaceAction(DetachFrom, self, gIobj);
            else
                inherited();
        }
    }
    iobjFor(TakeFrom)
    {
        verify()
        {
            /* use the inherited handling only if we're not attached */
            if (gDobj == nil || !isAttachedTo(gDobj))
                inherited();
        }
        check()
        {
            /* inherit the default check only if we're not attached */
            if (!isAttachedTo(gDobj))
                inherited();
        }
        action()
        {
            /* inherit the default action only if we're not attached */
            if (!isAttachedTo(gDobj))
                inherited();
        }
    }
;

/*
 *   An Attachable-specific precondition: the Attachable isn't already
 *   attached to something else.  This can be added to the preCond list for
 *   an Attachable (for iobjFor(AttachTo) and dobjFor(AttachTo)) to ensure
 *   that any existing attachment is removed before a new attachment is
 *   formed.  This is useful when the Attachable can connect to only one
 *   thing at a time.  
 */
objNotAttached: PreCondition
    checkPreCondition(obj, allowImplicit)
    {
        /* 
         *   if we don't already have any non-permanent attachments, we're
         *   fine (as we don't require removing permanent attachments) 
         */
        if (obj.attachedObjects.indexWhich(
            {x: !obj.isPermanentlyAttachedTo(x)}) == nil)
            return nil;

        /* 
         *   Try an implicit Detach command.  It should be safe to use the
         *   form that doesn't specify what we're detaching from, since the
         *   whole point of this condition is that the object can have only
         *   one non-permanent attachment, hence the vague Detach handler
         *   should be able to figure out what we mean.  
         */
        if (allowImplicit && tryImplicitAction(Detach, obj))
        {
            /* if we're still attached to anything, we failed, so abort */
            if (obj.attachedObjects.indexWhich(
                {x: !obj.isPermanentlyAttachedTo(x)}) != nil)
                exit;

            /* tell the caller we executed an implied action */
            return true;
        }

        /* we must detach first */
        reportFailure(&mustDetachMsg, obj);
        exit;
    }
;

/*
 *   A "nearby" attachable is a subclass of Attachable that adds a
 *   requirement that the attached objects be in a given location.  By
 *   default, we simply require that they have a common immediate
 *   container, but this can be overridden so that each object's location
 *   is negotiated separately.  This is a simple and effective pattern that
 *   avoids many of the potential anomalies with attachment (see the
 *   Attachable comments for examples).
 *   
 *   In AttachTo actions, we enforce the nearby requirement with a
 *   precondition requiring the direct object to be in the same immediate
 *   container as the indirect object, and vice versa.  In
 *   moveWhileAttached(), we enforce the rule by detaching the objects if
 *   one is being moved away from the other's immediate container.  
 */
class NearbyAttachable: Attachable
    dobjFor(AttachTo)
    {
        /* require that the objects be in the negotiated locations */
        preCond = (inherited() + nearbyAttachableCond)
    }
    iobjFor(AttachTo)
    {
        /* require that the objects be in the negotiated locations */
        preCond = (inherited() + nearbyAttachableCond)
    }

    /*
     *   Get the target locations for attaching to the given other object.
     *   The "target locations" are the locations where the objects are
     *   required to be in order to carry out the ATTACH command to attach
     *   this object to the other object (or vice versa). 
     *   
     *   This method returns a list with three elements.  The first
     *   element is the target location for 'self', and the second is the
     *   target location for 'other', the object we're attaching to.  The
     *   third element is an integer giving the priority; a higher number
     *   means higher priority.
     *   
     *   The priority is an arbitrary value that we use to determine which
     *   of the two objects involved in the attach gets to decide on the
     *   target locations.  We call this method on both of the two objects
     *   being attached to one another, then we use the target locations
     *   returned by the object that claims the higher priority.  If the
     *   two priorities are equal, we pick one arbitrarily.
     *   
     *   The default implementation chooses my own immediate container as
     *   the target location for both objects.  However, if the other
     *   object is non-portable, we'll choose its immediate location
     *   instead, since we obviously can't move it to our container.  
     */
    getNearbyAttachmentLocs(other)
    {
        /* 
         *   If the other object is portable, use our immediate container
         *   as the proposed location for both objects; otherwise, use the
         *   other object's immediate container.  In any case, use a low
         *   priority, since we're just the default base class
         *   implementation; any override will generally have higher
         *   priority. 
         */
        if (other.ofKind(NonPortable))
        {
            /* the other can't be moved, so use its location */
            return [other.location, other.location, 0];
        }
        else
        {
            /* 
             *   the other can be moved, so use our own location, in a
             *   paraphrase of the realty agent's favorite mantra 
             */
            return [location, location, 0];
        }
    }

    /* when an attached object is being moved, detach the objects */
    moveWhileAttached(movedObj, newCont)
    {
        /* 
         *   If I'm the one being moved, detach me from all of my
         *   non-permanent attachments; otherwise, just detach me from the
         *   other object, since it's the only one of my attachments being
         *   moved.  
         */
        if (movedObj == self)
        {
            /* I'm being moved - detach from everything */
            foreach (local cur in attachedObjects)
            {
                /* 
                 *   If we're not permanently attached to this one, and
                 *   it's not inside me, detach from it.  We don't need to
                 *   detach from objects inside this one, because they'll
                 *   be moved along with us automatically.  
                 */
                if (!cur.isIn(self) && !isPermanentlyAttachedTo(cur))
                    nestedDetachFrom(cur);
            }
        }
        else
        {
            /* just detach from the one object */
            nestedDetachFrom(movedObj);
        }
    }

    /* perform a nested DetachFrom action on the given object */
    nestedDetachFrom(obj)
    {
        /* run the nested DetachFrom as an implied action */
        tryImplicitAction(DetachFrom, self, obj);

        /* 
         *   if we're still attached to this object, the implied command
         *   must have failed, so abort the entire action 
         */
        if (attachedObjects.indexOf(obj) != nil)
            exit;
    }
;

/*
 *   Precondition for nearby-attachables.  This ensures that the two
 *   objects being attached are in their negotiated locations.  
 */
nearbyAttachableCond: PreCondition
    /* carry out the precondition */
    checkPreCondition(obj, allowImplicit)
    {
        local dobjProposal, iobjProposal;
        local dobjTargetLoc, iobjTargetLoc;
        local iobjRet, dobjRet;

        /*
         *   Ask each of the NearbyAttachable objects (the direct and
         *   indirect objects) what it thinks.  If an object isn't a
         *   NearbyAttachable, it won't have any opinion, so use a
         *   placeholder result with an extremely negative priority
         *   (ensuring that it won't be chosen).  In order for this
         *   precondition to have been triggered, one or the other of the
         *   objects must have been a nearby-attachable.  
         */
        dobjProposal = (gDobj.ofKind(NearbyAttachable)
                        ? gDobj.getNearbyAttachmentLocs(gIobj)
                        : [nil, nil, -2147483648]);
        iobjProposal = (gIobj.ofKind(NearbyAttachable)
                        ? gIobj.getNearbyAttachmentLocs(gDobj)
                        : [nil, nil, -2147483648]);

        /* 
         *   If the direct object claims higher priority, use its
         *   attachment locations; otherwise, use the direct object's
         *   locations.  (This means that we take the indirect object's
         *   proposed locations if the priorites are equal.)  
         */
        if (dobjProposal[3] > iobjProposal[3])
        {
            /* the direct object claims higher priority, so use its results */
            dobjTargetLoc = dobjProposal[1];
            iobjTargetLoc = dobjProposal[2];
        }
        else
        {
            /* 
             *   The direct object doesn't have a higher priority, so use
             *   the indirect object's results.  Note that the iobj
             *   results list has the iobj in the first position, since it
             *   was the 'self' when we asked it for its proposal.  
             */
            dobjTargetLoc = iobjProposal[2];
            iobjTargetLoc = iobjProposal[1];
        }
        
        /* carry out the pair of moves as needed */
        dobjRet = moveObject(gDobj, dobjTargetLoc, allowImplicit);
        iobjRet = moveObject(gIobj, iobjTargetLoc, allowImplicit);

        /* 
         *   Return the indication of whether or not we carried out an
         *   implied command.  (Note that we can't call moveObject in this
         *   'return' expression directly, because of the short-circuit
         *   behavior of the '||' operator.  We must call both, even if
         *   both carry out an action.)  
         */
        return (dobjRet || iobjRet);
    }

    /* carry out an implied action to move an object to a location */
    moveObject(obj, loc, allowImplicit)
    {
        /* if the object is already there, we have nothing to do */
        if (obj.location == loc)
            return nil;

        /* try the implied move */
        if (allowImplicit && loc.tryMovingObjInto(obj))
        {
            /* make sure it worked */
            if (obj.location != loc)
                exit;

            /* we performed an implied action */
            return true;
        }

        /* we can't move it - report failure and abort */
        loc.mustMoveObjInto(obj);
        exit;
    }
;

/*
 *   A PlugAttachable is a mix-in class that turns PLUG INTO into ATTACH TO
 *   and UNPLUG FROM into DETACH FROM.  This can be combined with
 *   Attachable or an Attachable subclass for objects that can be attached
 *   with PLUG INTO commands.  
 */
class PlugAttachable: object
    /* PLUG IN - to what? */
    dobjFor(PlugIn)
    {
        verify() { }
        action() { askForIobj(PlugInto); }
    }

    /* PLUG INTO is the same as ATTACH TO for us */
    dobjFor(PlugInto) remapTo(AttachTo, self, IndirectObject)
    iobjFor(PlugInto) remapTo(AttachTo, DirectObject, self)

    /* UNPLUG FROM is the same as DETACH FROM */
    dobjFor(Unplug) remapTo(Detach, self)
    dobjFor(UnplugFrom) remapTo(DetachFrom, self, IndirectObject)
    iobjFor(UnplugFrom) remapTo(DetachFrom, DirectObject, self)
;

/* ------------------------------------------------------------------------ */
/*
 *   Permanent attachments.  This class is for things that are described
 *   in the story text as attached to one another, but which can never be
 *   separated.  This is a mix-in class that can be combined with a Thing
 *   subclass.
 *   
 *   Descriptions of attachment tend to invite the player to try detaching
 *   the parts; the purpose of this class is to provide responses that are
 *   better than the defaults.  A good custom message for this class
 *   should usually acknowledge the attachment relationship, and explain
 *   why the parts can't be separated.
 *   
 *   There are two ways to express the attachment relationship.
 *   
 *   First, the more flexible way: in each PermanentAttachment object,
 *   define the 'attachedObjects' property to contain a list of the
 *   attached objects.  All of those other attached objects should usually
 *   be PermanentAttachment objects themselves, because the real-world
 *   relationship we're modeling is obviously symmetrical.  Because of the
 *   symmetrical relationship, it's only necessary to include the list
 *   entry on one side of a pair of attached objects - each side will
 *   automatically link itself to the other at start-up if it appears in
 *   the other's attachedObjects list.
 *   
 *   Second, the really easy way: if one of the attached objects is
 *   directly inside the other (which often happens for permanent
 *   attachments, because one is a component of the other), make the
 *   parent a PermanentAttachment, make the inner one a
 *   PermanentAttachmentChild, and you're done.  The two will
 *   automatically link up their attachment lists at start-up.
 *   
 *   Note that this is a subclass of Attachable.  Note also that a
 *   PermanentAttachment can be freely combined with a regular Attachable;
 *   for example, you could create a rope with a hook permanently
 *   attached, but stil allow the rope to be attached to other things as
 *   well: you'd make the rope a regular Attachable, and make the hook a
 *   PermanentAttachment.  The hook would be unremovable because of its
 *   permanent status, and this would symmetrical prevent the rope from
 *   being removed from the hook.  But the rope could still be attached to
 *   and detached from other objects.  
 */
class PermanentAttachment: Attachable
    /*
     *   Get the message explaining why we can't detach from 'obj'.
     *   
     *   By default, if our container is also a PermanentAttachment, and
     *   we're attached to it, we'll simply return its message.  This
     *   makes it really easy to define symmetrical permanent attachment
     *   relationships using containment, since all you have to do is make
     *   the container and the child both be PermanentAttachments, and
     *   then just define the cannot-detach message in the container.  If
     *   the container isn't a PermanentAttachment, or we're not attached
     *   to it, we'll return our default library message.  
     */
    cannotDetachMsgFor(obj)
    {
        if (location != nil
            && location.ofKind(PermanentAttachment)
            && isAttachedTo(location))
            return location.cannotDetachMsgFor(obj);
        else
            return baseCannotDetachMsg;
    }

    /* basic message to use when we try to detach something from self */
    baseCannotDetachMsg = &cannotDetachPermanentMsg
;

/*
 *   A permanent attachment "child" - this is an attachment that's
 *   explicitly attached to its container object.  This is a convenient
 *   way of setting up an attachment relationship between container and
 *   contents when the contents object isn't a Component.  
 */
class PermanentAttachmentChild: PermanentAttachment
    /* we're attached directly to our container */
    attachedObjects = perInstance([location])
;

/* ------------------------------------------------------------------------ */
/*
 *   A mix-in class for objects that don't come into play until some
 *   future event.  This class lets us initialize these objects with their
 *   *eventual* location, using the standard '+' syntax, but they won't
 *   actually appear in the given location until later in the game.
 *   During pre-initialization, we'll remember the starting location, then
 *   set the actual location to nil; later, the object can be easily moved
 *   to its eventual location by calling makePresent().  
 */
class PresentLater: object
    /*
     *   My "key" - this is an optional property you can add to a
     *   PresentLater object to associate it with a group of objects.  You
     *   can then use makePresentByKey() to move every object with a given
     *   key into the game world at once.  This is useful when an event
     *   triggers a whole set of objects to come into the game world:
     *   rather than having to write a method that calls makePresent() on
     *   each of the related objects individually, you can simply give each
     *   related object the same key value, then call makePresentByKey() on
     *   that key.
     *   
     *   You don't need to define this for an object unless you want to use
     *   makePresentByKey() with the object.  
     */
    plKey = nil

    /*
     *   Flag: are we present initially?  By default, we're only present
     *   later, as that's the whole point.  In some cases, though, we have
     *   objects that come and go, but start out present.  Setting this
     *   property to true makes the object present initially, but still
     *   allows it to come and go using the standard PresentLater
     *   mechanisms.  
     */
    initiallyPresent = nil

    initializeLocation()
    {
        /* 
         *   Save the initial location for later, and then clear out the
         *   current location.  We want to start out being out of the game,
         *   but remember where we'll appear when called upon.  To
         *   accommodate MultiLoc objects, check locationList first.  
         */
        if (locationList != nil)
        {
            /* save the location list */
            eventualLocation = locationList;

            /* 
             *   clear my location list if I'm not initially present; if I
             *   am initially present, inherit the normal initialization 
             */
            if (!initiallyPresent)
                locationList = [];
            else
                inherited();
        }
        else
        {
            /* save my eventual location */
            eventualLocation = location;

            /* 
             *   clear my location if I'm not initially present; if I am
             *   present initially, inherit the normal set-up 
             */
            if (!initiallyPresent)
                location = nil;
            else
                inherited();
        }
    }

    /* bring the object into the game world in its eventual location(s) */
    makePresent()
    {
        local pc;
        
        /* 
         *   If we have a list, add ourself to each location in the list;
         *   otherwise, simply move ourself to the single location.  
         */
        if (eventualLocation != nil && eventualLocation.ofKind(Collection))
            eventualLocation.forEach({loc: moveIntoAdd(loc)});
        else
            moveInto(eventualLocation);

        /* if the player character can now see me, mark me as seen */
        pc = gPlayerChar;
        if (pc.canSee(self))
        {
            /* mark me as seen */
            pc.setHasSeen(self);

            /* mark my visible contents as seen */
            setContentsSeenBy(pc.visibleInfoTable(), pc);
        }
    }

    /* 
     *   make myself present if the given condition is true; otherwise,
     *   remove me from the game world (i.e. move me into nil)
     */
    makePresentIf(cond)
    {
        if (cond)
            makePresent();
        else
            moveInto(nil);
    }

    /* 
     *   Bring every PresentLater object with the given key into the game.
     *   Note that this is a "class" method that you call on PresentLater
     *   itself:
     *   
     *   PresentLater.makePresentByKey('foo'); 
     */
    makePresentByKey(key)
    {
        /* 
         *   scan every PresentLater object, and move each one with the
         *   given key into the game 
         */
        forEachInstance(PresentLater, function(obj) {
            if (obj.plKey == key)
                obj.makePresent();
        });
    }

    /* 
     *   Bring every PresentLater object with the given key into the game,
     *   or move every one out of the game, according to the condition
     *   'cond'.
     *   
     *   If 'cond' is a function pointer, we'll invoke it once per object
     *   with the given key, passing the object as the parameter, and use
     *   the return value as the in game/out of game setting.  For example,
     *   if you wanted to show every object with key 'foo' AND with the
     *   property 'showObj' set to true, you could write this:
     *   
     *   PresentLater.makePresentByKeyIf('foo', {x: x.showObj});
     *   
     *   Note that this is a "class" method that you call on PresentLater
     *   itself.  
     */
    makePresentByKeyIf(key, cond)
    {
        /* 
         *   scan every PresentLater object, check each one's key, and make
         *   each one with the given key present 
         */
        forEachInstance(PresentLater, function(obj) {
            /* consider this object if its key matches */
            if (obj.plKey == key)
            {
                local flag = cond;
                
                /* 
                 *   evaluate the condition - if it's a function pointer,
                 *   invoke it on the current object, otherwise just take
                 *   it as a pre-evaluated condition value 
                 */
                if (dataTypeXlat(cond) == TypeFuncPtr)
                    flag = (cond)(obj);

                /* show or hide the object according to the condition */
                obj.makePresentIf(flag);
            }
        });
    }

    /* our eventual location */
    eventualLocation = nil
;

