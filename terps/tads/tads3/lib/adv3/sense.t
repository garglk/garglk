#charset "us-ascii"

/* 
 *   Copyright (c) 2000, 2006 Michael J. Roberts.  All Rights Reserved. 
 *   
 *   TADS 3 Library - senses
 *   
 *   This module defines objects and functions related to senses.  This
 *   file is language-independent.  
 */

/* include the library header */
#include "adv3.h"


/* ------------------------------------------------------------------------ */
/*
 *   Material: the base class for library objects that specify the way
 *   senses pass through objects.  
 */
class Material: object
    /*
     *   Determine how a sense passes through the material.  We'll return
     *   a transparency level.  (Individual materials should not need to
     *   override this method, since it simply dispatches to the various
     *   xxxThru methods.)
     */
    senseThru(sense)
    {
        /* dispatch to the xxxThru method for the sense */
        return self.(sense.thruProp);
    }

    /*
     *   For each sense, each material must define an appropriate xxxThru
     *   property that returns the transparency level for that sense
     *   through the material.  Any xxxThru property not defined in an
     *   individual material defaults to opaque.
     */
    seeThru = opaque
    hearThru = opaque
    smellThru = opaque
    touchThru = opaque
;

/*
 *   Adventium is the basic stuff of the game universe.  This is the
 *   default material for any object that doesn't specify a different
 *   material.  This type of material is opaque to all senses.  
 */
adventium: Material
    seeThru = opaque
    hearThru = opaque
    smellThru = opaque
    touchThru = opaque
;

/*
 *   Paper is opaque to sight and touch, but allows sound and smell to
 *   pass.  
 */
paper: Material
    seeThru = opaque
    hearThru = transparent
    smellThru = transparent
    touchThru = opaque
;

/*
 *   Glass is transparent to light, but opaque to touch, sound, and smell.
 */
glass: Material
    seeThru = transparent
    hearThru = opaque
    smellThru = opaque
    touchThru = opaque
;

/*
 *   Fine Mesh is transparent to all senses except touch.  
 */
fineMesh: Material
    seeThru = transparent
    hearThru = transparent
    smellThru = transparent
    touchThru = opaque
;

/*
 *   Coarse Mesh is transparent to all senses, including touch, but
 *   doesn't allow large objects to pass through.  
 */
coarseMesh: Material
    seeThru = transparent
    hearThru = transparent
    smellThru = transparent
    touchThru = transparent
;

/* ------------------------------------------------------------------------ */
/*
 *   Sense: the basic class for senses.  
 */
class Sense: object
    /*
     *   Each sense must define the property thruProp as a property
     *   pointer giving the xxxThru property for the sense.  The xxxThru
     *   property is the property of a material which determines how the
     *   sense passes through that material.  
     */
    thruProp = nil

    /*
     *   Each sense must define the property sizeProp as a property
     *   pointer giving the xxxSize property for the sense.  The xxxSize
     *   property is the property of a Thing which determines how "large"
     *   the object is with respect to the sense.  For example, sightSize
     *   indicates how large the object is visually, while soundSize
     *   indicates how loud the object is.
     *   
     *   The purpose of an object's size in a given sense is to determine
     *   how well the object can be sensed through an obscuring medium or
     *   at a distance.  
     */
    sizeProp = nil

    /*
     *   Each sense must define the property presenceProp as a property
     *   pointer giving the xxxPresence property for the sense.  The
     *   xxxPresence property is the property of a Thing which determines
     *   whether or not the object has a "presence" in this sense, which is
     *   to say whether or not the object is emitting any detectable
     *   sensory data for the sense.  For example, soundPresence indicates
     *   whether or not a Thing is making any noise.
     *   
     *   The sensory presence is used to determine if an object is in
     *   scope.  An object with a detectable sensory presence is normally
     *   in scope.  Note that sounds and smells emitted by a tangible
     *   object are frequently represented as additional intangible
     *   objects, and in these cases the intangible object (the sensory
     *   emanation) is usually the object with a sensory presence, rather
     *   than the tangible object making the noise/odor.  However, it is
     *   sometimes obvious that a particular sound or odor is coming from a
     *   particular kind of object, so the presence of the sound or odor
     *   implies the presence of the source object and thus places the
     *   source object in scope.  In such cases, it is desirable for the
     *   source object to have a sensory presence of its own, in addition
     *   to the sensory presence of the intangible sensory emanation
     *   object.
     *   
     *   Note that the "presence" doesn't have any effect on whether or not
     *   an object can be sensed.  Only the sense path matters for that: an
     *   object without a presence can still be sensed if there's a
     *   non-opaque sense path to the object.  Presence only determines
     *   whether or not an object is *actively* calling attention to
     *   itself.  
     */
    presenceProp = nil

    /*
     *   Each sense can define this property to specify a property pointer
     *   used to define a Thing's "ambient" energy emissions.  Senses
     *   which do not use ambient energy should define this to nil.
     *   
     *   Some senses work only on directly emitted sensory data; human
     *   hearing, for example, has no (at least effectively no) use for
     *   reflected sound, and can sense objects only by the sounds they're
     *   actually emitting.  Sight, on the other hand, can make use not
     *   only of light emitted by an object but of light reflected by the
     *   object.  So, sight defines an ambience property, whereas hearing,
     *   touch, and smell do not.  
     */
    ambienceProp = nil

    /*
     *   Determine if, in general, the given object can be sensed under
     *   the given conditions.  Returns true if so, nil if not.  By
     *   default, if the ambient level is zero, we'll return nil;
     *   otherwise, if the transparency level is 'transparent', we'll
     *   return true; otherwise, we'll consult the object's size:
     *   
     *   - Small objects cannot be sensed under less than transparent
     *   conditions.
     *   
     *   - Medium or large objects can be sensed in any conditions other
     *   than opaque.
     */
    canObjBeSensed(obj, trans, ambient)
    {
        /* 
         *   if we use "reflected" energy, and the ambient energy level is
         *   zero, we can't sense it 
         */
        if (ambienceProp != nil && ambient == 0)
            return nil;

        /* check the transparency level */
        switch(trans)
        {
        case transparent:
        case attenuated:
            /* 
             *   we can always sense under transparent or attenuated
             *   conditions 
             */
            return true;

        case distant:
        case obscured:
            /* 
             *   we can only sense medium and large objects under less
             *   than transparent conditions 
             */
            return obj.(self.sizeProp) != small;

        default:
            /* we can never sense under other conditions */
            return nil;
        }
    }
;

/*
 *   The senses.  We define sight, sound, smell, and touch.  We do not
 *   define a separate sense for taste, since it would add nothing to our
 *   model: you can taste something if and only if you can touch it.
 *   
 *   To add a new sense, you must do the following:
 *   
 *   - Define the sense object itself, in parallel to the senses defined
 *   below.
 *   
 *   - Modify class Material to set the default transparency level for
 *   this sense by defining the property xxxThru - for most senses, the
 *   default transparency level is 'opaque', but you must decide on the
 *   appropriate default for your new sense.
 *   
 *   - Modify class Thing to set the default xxxSize setting, if desired.
 *   
 *   - Modify class Thing to set the default xxxPresence setting, if
 *   desired.
 *   
 *   - Modify each instance of class 'Material' that should have a
 *   non-default transparency for the sense by defining the property
 *   xxxThru for the material.
 *   
 *   - Modify class Actor to add the sense to the default mySenses list;
 *   this is only necessary if the sense is one that all actors should
 *   have by default.  
 */

sight: Sense
    thruProp = &seeThru
    sizeProp = &sightSize
    presenceProp = &sightPresence
    ambienceProp = &brightness
;

sound: Sense
    thruProp = &hearThru
    sizeProp = &soundSize
    presenceProp = &soundPresence
;

smell: Sense
    thruProp = &smellThru
    sizeProp = &smellSize
    presenceProp = &smellPresence
;

touch: Sense
    thruProp = &touchThru
    sizeProp = &touchSize
    presenceProp = &touchPresence

    /*
     *   Override canObjBeSensed for touch.  Unlike other senses, touch
     *   requires physical contact with an object, so it cannot operate at
     *   a distance, regardless of the size of an object.  
     */
    canObjBeSensed(obj, trans, ambient)
    {
        /* if it's distant, we can't sense the object no matter how large */
        if (trans == distant)
            return nil;

        /* for other cases, inherit the default handling */
        return inherited(obj, trans, ambient);
    }
;


/* ------------------------------------------------------------------------ */
/*
 *   "Add" two transparency levels, yielding a new transparency level.
 *   This function can be used to determine the result of passing a sense
 *   through multiple layers of material.  
 */
transparencyAdd(a, b)
{
    /* transparent + x -> x for all x */
    if (a == transparent)
        return b;
    if (b == transparent)
        return a;

    /* opaque + x -> opaque for all x */
    if (a == opaque || b == opaque)
        return opaque;

    /* 
     *   any other combinations yield opaque - we can't have two levels of
     *   attenuation or obscuration without losing all detail and energy
     *   transmission 
     */
    return opaque;
}

/* ------------------------------------------------------------------------ */
/*
 *   Compare two transparency levels to determine which one is more
 *   transparent.  Returns 0 if the two levels are equally transparent, 1
 *   if the first one is more transparent, and -1 if the second one is
 *   more transparent.  The comparison follows this rule:
 *   
 *   transparent > attenuated > distant == obscured > opaque 
 */
transparencyCompare(a, b)
{
    /*
     *   for the purposes of the comparison, consider obscured to be
     *   identical to distant
     */
    if (a == obscured)
        a = distant;
    if (b == obscured)
        b = distant;

    /* if they're the same, return zero to so indicate */
    if (a == b)
        return 0;

    /*
     *   We know they're not equal, so if one is transparent, then the
     *   other one isn't.  Thus, if either one is transparent, it's the
     *   winner.
     */
    if (a == transparent)
        return 1;
    if (b == transparent)
        return -1;

    /*
     *   We know they're not equal and we know neither is transparent, so
     *   if one is attenuated then the other is worse, and the attenuated
     *   one is the winner.  
     */
    if (a == attenuated)
        return 1;
    if (b == attenuated)
        return -1;

    /*
     *   We now know neither one is transparent or attenuated, and we've
     *   already transformed obscured into distant, so the only possible
     *   values remaining are distant and opaque.  We know also they're
     *   not equal, because we would have already returned if that were
     *   the case.  So, we can conclude that one must be distant and the
     *   other must be opaque.  Hence, the one that's opaque is the less
     *   transparent one.  
     */
    if (a == opaque)
        return -1;
    else
        return 1;
}

/* ------------------------------------------------------------------------ */
/*
 *   Given a brightness level and a transparency level, compute the
 *   brightness as modified by the transparency level. 
 */
adjustBrightness(br, trans)
{
    switch(trans)
    {
    case transparent:
    case distant:
        /* 
         *   Transparent medium or distance - this doesn't modify
         *   brightness at all.  (Technically, distance would reduce
         *   brightness somewhat, but the typical scale of an IF setting
         *   isn't usually large enough that brightness should
         *   significantly diminish.)  
         */
        return br;

    case attenuated:
    case obscured:
        /* 
         *   Distant, obscured, or attenuated.  We reduce self-illuminating
         *   light (level 1) and dim light (level 2) to nothing (level 0),
         *   we leave nothing as nothing (obviously), and we reduce all
         *   other levels one step.  So, everything below level 3 goes to
         *   0, and everything at or above level 3 gets decremented by 1.  
         */
        return (br >= 3 ? br - 1 : 0);

    case opaque:
        /* opaque medium - nothing makes it through */
        return 0;

    default:
        /* shouldn't get to other cases */
        return nil;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   SenseConnector: an object that can pass senses across room
 *   boundaries.  This is a mix-in class: add it to the superclass list of
 *   the object before Thing (or a Thing subclass).
 *   
 *   A SenseConnector acts as a sense conduit across all of its locations,
 *   so to establish a connection between locations, simply place a
 *   SenseConnector in each location.  Since a SenseConnector is useful
 *   only when placed placed in multiple locations, SenseConnector is
 *   based on MultiLoc.  
 */
class SenseConnector: MultiLoc
    /*
     *   A SenseConnector's material generally determines how senses pass
     *   through the connection.  
     */
    connectorMaterial = adventium

    /*
     *   Determine how senses pass through this connection.  By default,
     *   we simply use the material's transparency.
     */
    transSensingThru(sense) { return connectorMaterial.senseThru(sense); }

    /*
     *   Add the direct containment connections for this item to a lookup
     *   table. 
     *   
     *   Since we provide a sense connection among all of our containers,
     *   add each of our containers to the list.  
     */
    addDirectConnections(tab)
    {
        /* add myself */
        tab[self] = true;
            
        /* add my CollectiveGroup objects */
        foreach (local cur in collectiveGroups)
            tab[cur] = true;

        /* add my contents */
        foreach (local cur in contents)
        {
            if (tab[cur] == nil)
                cur.addDirectConnections(tab);
        }

        /* add my containers */
        foreach (local cur in locationList)
        {
            if (tab[cur] == nil)
                cur.addDirectConnections(tab);
        }
    }

    /*
     *   Transmit energy from a container onto me.
     */
    shineFromWithout(fromParent, sense, ambient, fill)
    {
        /* if this increases my ambient level, accept the new level */
        if (ambient > tmpAmbient_)
        {
            local levelThru;
            
            /* remember the new level and fill material to this point */
            tmpAmbient_ = ambient;
            tmpAmbientFill_ = fill;

            /* transmit to my contents */
            shineOnContents(sense, ambient, fill);

            /*
             *   We must transmit this energy to each of our other
             *   parents, possibly reduced for traversing our connector.
             *   Calculate the new level after traversing our connector. 
             */
            levelThru = adjustBrightness(ambient, transSensingThru(sense));

            /* 
             *   if there's anything left, transmit it to the other
             *   containers 
             */
            if (levelThru >= 2)
            {
                /* transmit to each container except the source */
                foreach (local cur in locationList)
                {
                    /* if this isn't the sender, transmit to it */
                    if (cur != fromParent)
                        cur.shineFromWithin(self, sense, levelThru, fill);
                }
            }
        }
    }

    /*
     *   Build a sense path from a container to me
     */
    sensePathFromWithout(fromParent, sense, trans, obs, fill)
    {
        /* 
         *   if there's better transparency along this path than along any
         *   previous path we've used to visit this item, take this path 
         */
        if (transparencyCompare(trans, tmpTrans_) > 0)
        {
            local transThru;
            
            /* remember the new path to this point */
            tmpTrans_ = trans;
            tmpObstructor_ = obs;

            /* we're coming to this object from outside */
            tmpPathIsIn_ = true;

            /* transmit to my contents */
            sensePathToContents(sense, trans, obs, fill);

            /*
             *   We must transmit this energy to each of our other
             *   parents, possibly reduced for traversing our connector.
             *   Calculate the new level after traversing our connector. 
             */
            transThru = transparencyAdd(trans, transSensingThru(sense));

            /* if we changed the transparency, we're the obstructor */
            if (transThru != trans)
                obs = self;

            /* 
             *   if there's anything left, transmit it to the other
             *   containers 
             */
            if (transThru != opaque)
            {
                /* transmit to each container except the source */
                foreach (local cur in locationList)
                {
                    /* if this isn't the sender, transmit to it */
                    if (cur != fromParent)
                        cur.sensePathFromWithin(self, sense,
                                                transThru, obs, fill);
                }
            }
        }
    }

    /* 
     *   Call a function on each connected container.  Since we provide a
     *   sense path connection among our containers, we must iterate over
     *   each of our containers.  
     */
    forEachConnectedContainer(func, [args])
    {
        forEachContainer(func, args...);
    }

    /* 
     *   Return a list of my connected containers.  We connect to all of
     *   our containers, so simply return my location list. 
     */
    getConnectedContainers = (locationList)

    /* 
     *   Check moving an object through me.  This is called when we try to
     *   move an object from one of our containers to another of our
     *   containers through me.  By default, we don't allow it.  
     */
    checkMoveThrough(obj, dest)
    {
        /* return an error - cannot move through <self> */
        return new CheckStatusFailure(&cannotMoveThroughMsg, obj, self);
    }

    /*
     *   Check touching an object through me.  This is called when an
     *   actor tries to reach from one of my containers through me into
     *   another of my containers.  By default, we don't allow it. 
     */
    checkTouchThrough(obj, dest)
    {
        /* return an error - cannot reach through <self> */
        return new CheckStatusFailure(&cannotReachThroughMsg, dest, self);
    }

    /*
     *   Check throwing an object through me.  This is called when an actor
     *   tries to throw a projectile 'obj' at 'dest' via a path that
     *   includes 'self'.  By default, we don't allow it.
     */
    checkThrowThrough(obj, dest)
    {
        return new CheckStatusFailure(&cannotThrowThroughMsg, dest, self);
    }

    /* check for moving via a path */
    checkMoveViaPath(obj, dest, op)
    {
        /* if moving through us, run the separate Move check */
        if (op == PathThrough)
            return checkMoveThrough(obj, dest);

        /* if we can inherit, do so */
        if (canInherit())
            return inherited(obj, dest, op);

        /* return success by default */
        return checkStatusSuccess;
    }

    /* check for touching via a path */
    checkTouchViaPath(obj, dest, op)
    {
        /* if reaching through us, run the separate Touch check */
        if (op == PathThrough)
            return checkTouchThrough(obj, dest);

        /* if we can inherit, do so */
        if (canInherit())
            return inherited(obj, dest, op);

        /* return success by default */
        return checkStatusSuccess;
    }

    /* check for throwing via a path */
    checkThrowViaPath(obj, dest, op)
    {
        /* if throwing through us, run the separate Throw check */
        if (op == PathThrough)
            return checkThrowThrough(obj, dest);

        /* if we can inherit, do so */
        if (canInherit())
            return inherited(obj, dest, op);

        /* return success by default */
        return checkStatusSuccess;
    }
;

/*
 *   Occluder: this is a mix-in class that can be used with multiple
 *   inheritance to combine with other classes (such as SenseConnector, or
 *   Thing subclasses), to create an "occluded view."  This lets you
 *   exclude certain objects from view, and you can make the exclusion vary
 *   according to the point of view.
 *   
 *   This class is useful for situations where the view from one location
 *   to another is partially obstructed.  For example, suppose we have two
 *   rooms, connected by a window between them.  The window is the sense
 *   connector that connects the two top-level locations, and it makes
 *   objects in one room visible from the point of view of the other room.
 *   Suppose that one room contains a bookcase, with its back to the
 *   window.  From the point of view of the other room, we can't see
 *   anything inside the bookcase.  This class allows for such special
 *   situations.
 *   
 *   Note that occlusion rules are applied "globally" within a room - that
 *   is, anything that an Occluder occludes will be removed from view, even
 *   if it's visible from another, non-occluding connector.  Hence,
 *   occlusion always takes precedence over "inclusion" - if an object is
 *   occluded just once, then it won't be in view, no matter how many times
 *   it's added back into view by other connectors.  This comes from the
 *   order in which the occlusion rules are considered.  Occlusion rules
 *   are always run last, and they can't distinguish the connector that
 *   added an object to view.  So, we first run around and collect up
 *   everything that can be seen, by considering all of the different paths
 *   to seeing those things.  Then, we go through all of the occlusion
 *   rules that apply to the room, and we remove from view everything that
 *   the occluding connectors want to occlude.  
 */
class Occluder: object
    /*
     *   Do we occlude the given object, in the given sense and from the
     *   given point of view?  This returns true if the object is occluded,
     *   nil if not.  By default, we simply ask the object whether it's
     *   occluded by this occluder from the given POV.  
     */
    occludeObj(obj, sense, pov)
    {
        /* by default, simply ask the object what it thinks */
        return obj.isOccludedBy(self, sense, pov);
    }

    /*
     *   When we initialize for the sense path calculation, register to
     *   receive notification after we've finished building the sense
     *   table.  We'll use the notification to remove any occluded objects
     *   from the sense table.  
     */
    clearSenseInfo()
    {
        /* do the normal work */
        inherited();

        /* register for notification after we've built the table */
        senseTmp.notifyList.append(self);
    }

    /*
     *   Receive notification that the sense path calculation is now
     *   finished.  'objs' is a LookupTable containing all of the objects
     *   involved in the sense path calculation (the objects are the keys
     *   in the table).  Each object in the table now has its tmpXxx_
     *   properties set to the sense path data we've calculated for that
     *   object - tmpTrans_ is the transparency to the object, tmpAmbient_
     *   is the ambient light level at the object, and so on.
     *   
     *   Since our job is to occlude certain objects from view, we'll run
     *   through the table and test each object using our occlusion rule.
     *   If we find that we do occlude an object, we'll set its
     *   transparency to 'opaque' to indicate that it cannot be seen.  
     */
    finishSensePath(objs, sense)
    {
        /* get the point of view of the calculation */
        local pov = senseTmp.pointOfView;
            
        /* run through the table, and apply our rule to each object */
        objs.forEachAssoc(function(key, val)
        {
            /* if this object is occluded, set its path to opaque */
            if (occludeObj(key, sense, pov))
            {
                /* set this object to opaque */
                key.tmpTrans_ = key.tmpTransWithin_ = opaque;

                /* we're the obstructor for the object */
                key.tmpObstructor_ = key.tmpObstructorWithin_ = self;
            }
        });
    }
;

/*
 *   DistanceConnector: a special type of SenseConnector that connects its
 *   locations with distance.  This can be used for things like divided
 *   rooms, where a single physical location is modeled with two or more
 *   Room objects - the north and south end of a large cave, for example.
 *   This is also useful for cases where two rooms are separate but open to
 *   one another, such as a balcony overlooking a courtyard.
 *   
 *   Note that this inherits from both SenseConnector and Intangible.
 *   Intangible is included as a base class because each instance will need
 *   to derive from Thing, so that it fits into the normal sense model, but
 *   will virtually never need any other physical presence in the game
 *   world; Intangible fills both of these needs.  
 */
class DistanceConnector: SenseConnector, Intangible
    /* all senses are connected through us, but at a distance */
    transSensingThru(sense) { return distant; }

    /* 
     *   When checking for reaching through this connector, specialize the
     *   failure message to indicate that distance is the specific problem.
     *   (Without this specialization, we'd get a generic message when
     *   trying to reach through the connector, such as "you can't reach
     *   that through <self>."  
     */
    checkTouchThrough(obj, dest)
    {
        /* we can't touch through this connector due to the distance */
        return new CheckStatusFailure(&tooDistantMsg, dest);
    }

    /*
     *   When checking for throwing through this container, specialize the
     *   failure message to indicate that distance is the specific problem.
     */
    checkThrowThrough(obj, dest)
    {
        return new CheckStatusFailure(&tooDistanceMsg, dest);
    }

    /* 
     *   Do allow moving an object through a distance connector.  This
     *   should generally only be involved at all when we're moving an
     *   object programmatically, in which case we should already have
     *   decided that the movement is allowable.  Any command that tries to
     *   move an object through a distance connector will almost certainly
     *   have a suitable set of preconditions that checks for reachability,
     *   which will in most cases disallow the action anyway before we get
     *   to the point of wanting to move anything.  
     */
    checkMoveThrough(obj, dest) { return checkStatusSuccess; }

    /*
     *   Report the reason that we stopped a thrown projectile from hitting
     *   its intended target.  This is called when we're along the path
     *   between the thrower and the intended target, AND 'self' objects to
     *   the action.
     *   
     *   The default version of this method in Thing reports that the
     *   projectile hits 'self', as though self were a physical obstruction
     *   like a fence or wall.  In the case of a distance connector,
     *   though, the reason isn't usually obstruction, but simply that the
     *   connector imposes such a distance that the actor can't throw the
     *   projectile far enough to reach the intended target.  We therefore
     *   override the Thing version to report that the projectile fell
     *   short of the target.
     *   
     *   Note that if you do want to allow throwing a projectile across the
     *   distance represented by this connector, you can override
     *   checkThrowThrough() to return checkStatusSuccess.  
     */
    throwTargetHitWith(projectile, path)
    {
        /* 
         *   figure out where we fall to when we hit this object, then send
         *   the object being thrown to that location 
         */
        getHitFallDestination(projectile, path)
            .receiveDrop(projectile, new DropTypeShortThrow(self, path));
    }
;

/*
 *   A drop-type descriptor for a "short throw," which occurs when the
 *   target is too far away to reach with our throw (i.e., the thrown
 *   object falls short of the target).  
 */
class DropTypeShortThrow: DropTypeThrow
    construct(target, path)
    {
        /* inherit the default handling */
        inherited(target, path);

        /* we care about the *intended* target, not the distance connector */
        target_ = path[path.length()];
    }

    standardReport(obj, dest)
    {
        /* show the short-throw report */
        mainReport(&throwFallShortMsg, obj, target_,
                   dest.getNominalDropDestination());
    }

    getReportPrefix(obj, dest)
    {
        /* return the short-throw prefix */
        return gActor.getActionMessageObj().throwShortMsg(obj, target_);
    }
;

