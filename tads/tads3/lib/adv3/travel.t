#charset "us-ascii"

/* 
 *   Copyright (c) 2000, 2006 Michael J. Roberts.  All Rights Reserved. 
 *   
 *   TADS 3 Library - travel
 *   
 *   This module defines the parts of the simulation model related to
 *   travel: rooms and other locations, directions, passages.  
 */

/* include the library header */
#include "adv3.h"

infiniteLoop()
{
    for (local i = 1 ; i < 100 ; ) ;
}

/* ------------------------------------------------------------------------ */
/*
 *   Directions.  Each direction is represented by an instance of
 *   Direction.
 *   
 *   A Direction object encapsulates information about a travel direction.
 *   By using an object to represent each possible travel direction, we
 *   make it possible for a game or library extension to add new custom
 *   directions without having to change the basic library.  
 */
class Direction: object
    /*
     *   The link property for the direction.  This is a property pointer
     *   that we use to ask an actor's location for a TravelConnector when
     *   the actor attempts to travel in this direction.  
     */
    dirProp = nil

    /* 
     *   The default TravelConnector when an actor attempts travel in this
     *   direction, but the actor's location does not define the dirProp
     *   property for this direction.  This is almost always a connector
     *   that goes nowhere, such as noTravel, since this is used only when
     *   a location doesn't have a link for travel in this direction.  
     */
    defaultConnector(loc) { return noTravel; }

    /*
     *   Initialize.  We'll use this routine to add each Direction
     *   instance to the master direction list (Direction.allDirections)
     *   during pre-initialization.  
     */
    initializeDirection()
    {
        /* add myself to the master direction list */
        Direction.allDirections.append(self);
    }

    /*
     *   Class initialization - this is called once on the class object.
     *   We'll build our master list of all of the Direction objects in
     *   the game, and then sort the list using the sorting order.  
     */
    initializeDirectionClass()
    {
        /* initialize each individual Direction object */
        forEachInstance(Direction, { dir: dir.initializeDirection() });

        /* 
         *   sort the direction list according to the individual Directin
         *   objects' defined sorting orders 
         */
        allDirections.sort(SortAsc, {a, b: a.sortingOrder - b.sortingOrder});
    }

    /* 
     *   Our sorting order in the master list.  We use this to present
     *   directions in a consistent, aesthetically pleasing order in
     *   listings involving multiple directions.  The sorting order is
     *   simply an integer that gives the relative position in the list;
     *   the list of directions is sorted from lowest sorting order to
     *   highest.  Sorting order numbers don't have to be contiguous,
     *   since we simply put the directions in an order that makes the
     *   sortingOrder values ascend through the list.  
     */
    sortingOrder = 1

    /* 
     *   A master list of the defined directions.  We build this
     *   automatically during initialization to include each Direction
     *   instance.  Any operation that wants to iterate over all possible
     *   directions can run through this list.
     *   
     *   By using this master list to enumerate all possible directions
     *   rather than a hard-coded set of pre-defined directions, we can
     *   ensure that any new directions that a game or library extension
     *   adds will be incorporated automatically into the library simply
     *   by virtue of the existence Direction instances to represent the
     *   new directions.  
     */
    allDirections = static new Vector(10)
;

/*
 *   The base class for compass directions (north, south, etc).  We don't
 *   add anything to the basic Direction class, but we use a separate
 *   class for compass directions to allow language-specific
 *   customizations for all of the directions and to allow travel commands
 *   to treat these specially when needed.  
 */
class CompassDirection: Direction
;

/*
 *   The base class for shipboard directions (port, aft, etc).
 */
class ShipboardDirection: Direction
    defaultConnector(loc)
    {
        /* 
         *   If 'loc' is aboard a ship, use noTravel as the default
         *   connector, since we simply can't go this direction.  If we're
         *   not aboard ship, use the special connector noShipTravel,
         *   which is meant to convey that shipboard directions make no
         *   sense when not aboard a ship.  
         */
        return (loc.isShipboard ? noTravel : noShipTravel);
    }
;

/*
 *   The base class for vertical directions (up, down) 
 */
class VerticalDirection: Direction
;

/*
 *   The base class for "relative" directions (in, out) 
 */
class RelativeDirection: Direction
;

/*
 *   Individual compass directions.  
 *   
 *   Our macro defines a direction object with a name based on the
 *   direction's room travel link property and the base class.  So,
 *   DefineDirection(north, Compass) defines a direction object called
 *   'northDirection' based on the CompassDirection class, with the link
 *   property 'north' and default travel connector 'noTravel'.
 *   
 *   Note that we define a sorting order among the default directions as
 *   follows.  First, we define several groups of related directions,
 *   which we put in a relative order: the compass directions, then the
 *   vertical directions, then the "relative" (in/out) directions, and
 *   finally the shipboard directions.  Then, we order the directions
 *   within these groups.  For the sortingOrder values, we use arbitrary
 *   integers with fairly wide separations, to leave plenty of room for
 *   custom game-specific directions to be added before, between, after,
 *   or within these pre-defined groups.  
 */
#define DefineDirection(prop, base, order) \
    prop##Direction: base##Direction \
    dirProp = &prop \
    sortingOrder = order

DefineDirection(north, Compass, 1000);
DefineDirection(south, Compass, 1100);
DefineDirection(east, Compass,  1200);
DefineDirection(west, Compass,  1300);
DefineDirection(northeast, Compass, 1400);
DefineDirection(northwest, Compass, 1500);
DefineDirection(southeast, Compass, 1600);
DefineDirection(southwest, Compass, 1700);

DefineDirection(up, Vertical, 3000);
DefineDirection(down, Vertical, 3100);

DefineDirection(in, Relative, 5000)
    defaultConnector(loc) { return noTravelIn; }
;

DefineDirection(out, Relative, 5100)
    defaultConnector(loc) { return noTravelOut; }
;

DefineDirection(fore, Shipboard, 7000);
DefineDirection(aft, Shipboard, 7100);
DefineDirection(port, Shipboard, 7200);
DefineDirection(starboard, Shipboard, 7300);


/* ------------------------------------------------------------------------ */
/*
 *   Travel Message Handler.  This contains a set of messages that are
 *   specific to different types of TravelConnector objects, to describe
 *   NPC arrivals and departures via these connectors when the NPC's are in
 *   view of the player character.
 *   
 *   This base class implements the methods simply by calling the
 *   corresponding gLibMessages message methods.
 *   
 *   The purpose of providing this variety of connector-specific handlers
 *   is to make it easy for individual travelers to customize the
 *   arrival/departure messages for specific connector subclasses.  These
 *   messages sometimes benefit from customization for specific
 *   traveler/connector combinations; the easiest way to enable such
 *   granular customization is to make it possible to override a message
 *   per connector type on the traveler object.  
 */
class TravelMessageHandler: object
    /* 
     *   Get the traveler for the purposes of arrival/departure messages.
     *   Implementations that aren't themselves the travelers should
     *   override this to supply the correct nominal traveler. 
     */
    getNominalTraveler() { return self; }

    /* generic arrival/departure - for the base TravelConnector class */
    sayArriving(conn) { gLibMessages.sayArriving(getNominalTraveler()); }
    sayDeparting(conn) { gLibMessages.sayDeparting(getNominalTraveler()); }

    /* generic local arrival and departure messages */
    sayArrivingLocally(dest, conn)
        { gLibMessages.sayArrivingLocally(getNominalTraveler(), dest); }
    sayDepartingLocally(dest, conn)
        { gLibMessages.sayDepartingLocally(getNominalTraveler(), dest); }

    /* generic remote travel message */
    sayTravelingRemotely(dest, conn)
        { gLibMessages.sayTravelingRemotely(getNominalTraveler(), dest); }

    /* directional arrival/departure - for RoomConnector */
    sayArrivingDir(dir, conn) { dir.sayArriving(getNominalTraveler()); }
    sayDepartingDir(dir, conn) { dir.sayDeparting(getNominalTraveler()); }

    /* arrival/departure via a ThroughPassage */
    sayArrivingThroughPassage(conn)
    {
        gLibMessages.sayArrivingThroughPassage(getNominalTraveler(), conn);
    }
    sayDepartingThroughPassage(conn)
    {
        gLibMessages.sayDepartingThroughPassage(getNominalTraveler(), conn);
    }

    /* arrival/departure via a PathPassage */
    sayArrivingViaPath(conn)
        { gLibMessages.sayArrivingViaPath(getNominalTraveler(), conn); }
    sayDepartingViaPath(conn)
        { gLibMessages.sayDepartingViaPath(getNominalTraveler(), conn); }

    /* arrival/departure up/down stairs */
    sayArrivingUpStairs(conn)
        { gLibMessages.sayArrivingUpStairs(getNominalTraveler(), conn); }
    sayArrivingDownStairs(conn)
        { gLibMessages.sayArrivingDownStairs(getNominalTraveler(), conn); }
    sayDepartingUpStairs(conn)
        { gLibMessages.sayDepartingUpStairs(getNominalTraveler(), conn); }
    sayDepartingDownStairs(conn)
        { gLibMessages.sayDepartingDownStairs(getNominalTraveler(), conn); }
;

/* ------------------------------------------------------------------------ */
/*
 *   A Traveler is an object that can travel.  The two main kinds of
 *   travelers are Actors and Vehicles.  A vehicle can contain multiple
 *   actors, so a single vehicle travel operation could move several
 *   actors.
 *   
 *   This class is intended to be multiply inherited, since it is not based
 *   on any simulation object class.  
 */
class Traveler: TravelMessageHandler
    /*
     *   Check, using pre-condition rules, that the traveler is directly
     *   in the given room.  We'll attempt to implicitly remove the actor
     *   from any nested rooms within the given room.  
     */
    checkDirectlyInRoom(dest, allowImplicit)
    {
        /* ask the destination to ensure the traveler is in it directly */
        return dest.checkTravelerDirectlyInRoom(self, allowImplicit);
    }

    /*
     *   Check, using pre-condition rules, that the traveler is in the
     *   given room, moving the traveler to the room if possible. 
     */
    checkMovingTravelerInto(room, allowImplicit)
    {
        /* subclasses must override */
    }

    /*
     *   Get the travel preconditions specific to the traveler, for the
     *   given connector.  By default, we return no extra conditions.  
     */
    travelerPreCond(conn) { return []; }

    /*
     *   Can the traveler travel via the given connector to the given
     *   destination?  Returns true if the travel is permitted, nil if not.
     *   
     *   By default, this simply returns true to indicate that the travel
     *   is allowed.  Individual instances can override this to enforce
     *   limitations on what kind of travel the traveler can perform.  
     */
    canTravelVia(connector, dest) { return true; }

    /* 
     *   Explain why the given travel is not possible.  This is called when
     *   canTravelVia() returns nil, to display a report to the player
     *   explaining why the travel was disallowed.
     *   
     *   By default, we do nothing, since our default canTravelVia() never
     *   disallows any travel.  If canTravelVia() is overridden to disallow
     *   travel under some conditions, this must be overridden to generate
     *   an appropriate explanatory report.  
     */
    explainNoTravelVia(connector, dest) { }

    /*
     *   Show my departure message - this is shown when I'm leaving a room
     *   where the player character can see me for another room where the
     *   player character cannot see me.  
     */
    describeDeparture(dest, connector)
    {
        /* 
         *   If we're visible to the player character, describe the
         *   departure.  Ask the connector to describe the travel, since
         *   it knows the direction of travel and can describe special
         *   things like climbing stairs.
         *   
         *   Don't bother to describe the departure if the traveler is the
         *   player character, or the player character is inside the
         *   traveler.  The PC obviously can't leave the presence of
         *   itself.
         */
        if (!isActorTraveling(gPlayerChar))
        {
            /* 
             *   invoke the departure with a visual sense context for the
             *   traveler 
             */
            callWithSenseContext(self, sight,
                                 {: describeNpcDeparture(dest, connector)});
        }
    }

    /*
     *   Describe the departure of a non-player character, or any traveler
     *   not involving the player character. 
     */
    describeNpcDeparture(dest, connector)
    {
        /*
         *   If the PC can see our destination, we're doing "local travel"
         *   - we're traveling from one top-level location to another, but
         *   entirely in view of the PC.  In this case, the PC was already
         *   aware of our presence, so we don't want to describe the
         *   departure as though we're actually leaving; we're just moving
         *   around.  If the destination isn't in sight, then we're truly
         *   departing. 
         */
        if (gPlayerChar.canSee(dest))
        {
            /* 
             *   We're moving from somewhere within the PC's sight to
             *   another location within the PC's sight, so we're not
             *   actually departing, from the PC's point of view: we're
             *   just moving around a bit.
             *   
             *   - If we're moving further away from the PC - that is, if
             *   we're moving out of the PC's own top-level location and
             *   into a different top-level location, describe this as a
             *   "local departure."
             *   
             *   - If we're moving closer to the PC - moving from a top
             *   level location that doesn't contain the PC to the PC's
             *   current top-level location - say nothing.  We'll leave it
             *   to the "local arrival" message to mention it, because
             *   that's the more relevant aspect of the travel from the
             *   PC's POV: we're approaching the PC.
             *   
             *   - If we're moving from one top-level location to another,
             *   say nothing.  We'll leave this to arrival time as well.
             *   Whether we describe remote-to-remote travel as arrival or
             *   departure is just arbitrary; we only want to generate one
             *   message, but there's no general reason to prefer
             *   generating the message at departure vs arrival.  
             */
            if (gPlayerChar.isIn(getOutermostRoom()))
            {
                /* 
                 *   we're moving further away from the player - describe
                 *   this as a local departure 
                 */
                connector.describeLocalDeparture(self, location, dest);
            }
        }
        else
        {
            /* we're really departing - let the connector describe it */
            connector.describeDeparture(self, location, dest);
        }
    }

    /*
     *   Show my arrival message - this is shown when I'm entering a room
     *   in view of the player character from a location where the player
     *   character could not see me.  'backConnector' is the connector in
     *   the destination location from which we appear to be emerging.  
     */
    describeArrival(origin, backConnector)
    {
        /*
         *   If the player character is participating in the travel,
         *   describe the arrival from the point of view of the player
         *   character by showing the "look around" description of the new
         *   location.
         *   
         *   Otherwise, describe the NPC traveler's arrival.  Only
         *   describe the arrival if the PC can see the traveler, for
         *   obvious reasons.  
         */
        if (isActorTraveling(gPlayerChar))
        {
            /*
             *   Add an intra-report separator, to visually separate the
             *   description of the arrival from any result text that came
             *   before.  (For example, if we performed an implied command
             *   before we could begin travel, this will visually separate
             *   the results of the implied command from the arrival
             *   message.)  
             */
            say(gLibMessages.intraCommandSeparator);

            /* 
             *   The player character is traveling - show the travel from
             *   the PC's perspective.  
             */
            gPlayerChar.lookAround(gameMain.verboseMode.isOn);
        }
        else if (travelerSeenBy(gPlayerChar))
        {
            /* 
             *   The player character isn't traveling, but the PC can see
             *   me now that I'm in my new location, so describe the
             *   arrival of the traveler.  Do this within a visual sense
             *   context for the traveler.  
             */
            callWithSenseContext(
                self, sight, {: describeNpcArrival(origin, backConnector)});
        }
    }

    /*
     *   Describe the arrival of a non-player character or any other
     *   traveler that doesn't involve the player character. 
     */
    describeNpcArrival(origin, backConnector)
    {
        /*  
         *   If we know the connector back, use its arrival message;
         *   otherwise, use a generic arrival message.  
         */
        if (backConnector != nil)
        {
            /*
             *   If the PC can see the origin, we're not actually arriving
             *   anew; instead, we're simply moving around within the PC's
             *   field of view.  In these cases, we don't want to use the
             *   normal arrival message, because we're not truly arriving;
             *   we could be moving closer to the player, further away from
             *   the player, or moving laterally from one remote location
             *   to another.  If the PC can't see the origin, we're truly
             *   arriving.  
             */
            if (gPlayerChar.canSee(origin))
            {
                /*
                 *   We're not arriving anew, but just moving around within
                 *   the PC's field of view.
                 *   
                 *   - If we're moving closer to the PC - moving from a
                 *   top-level location that doesn't contain the PC to one
                 *   that does - describe this as a "local arrival."
                 *   
                 *   - If we're moving further away from the PC - from
                 *   within the PC's top-level location to a different
                 *   top-level location - don't say anything.  We will have
                 *   already described this as a "local departure" during
                 *   the departure message phase, and from the PC's point
                 *   of view, that fully covers it.
                 *   
                 *   - If we're moving laterally - from one remote
                 *   top-level location to another ("remote" means "does
                 *   not contain the PC") - it's arbitrary whether this
                 *   should be described at arrival or departure time.  We
                 *   arbitrarily pick arrival, so describe the lateral
                 *   local travel now.  
                 */
                if (gPlayerChar.isIn(getOutermostRoom()))
                {
                    /* 
                     *   We're now in the same top-level location as the
                     *   PC, so we've moved closer to the PC, so describe
                     *   this as a "local arrival."  
                     */
                    backConnector.describeLocalArrival(
                        self, origin, location);
                }
                else if (!gPlayerChar.isIn(origin))
                {
                    /* 
                     *   We're *not* in the same top-level location as the
                     *   PC, and we weren't on departure either, so this is
                     *   a lateral move - a move from one remote location
                     *   to another.  Describe it as such now.  
                     */
                    backConnector.describeRemoteTravel(
                        self, origin, location);
                }
            }
            else
            {
                /* we're arriving anew - let the connector describe it */
                backConnector.describeArrival(self, origin, location);
            }
        }
        else
        {
            /* there's no back-connector, so use a generic arrival message */
            gLibMessages.sayArriving(self);
        }
    }

    /*
     *   Travel to a new location.  Moves the traveler to a new location
     *   via the given connector, triggering any side effects of the
     *   travel.
     *   
     *   Note that this routine is not normally called directly; in most
     *   cases, the actor's travelTo is called, and it in turn invokes
     *   this method in the appropriate traveler.
     *   
     *   'dest' is the new location to which we're traveling.  'connector'
     *   is the TravelConnector we're traversing from the source location
     *   to reach the new destination; the connector is normally the
     *   invoker of this routine.  'backConnector' is the connector in the
     *   destination from which the actor will appear to emerge on the
     *   other end of the travel.  
     */
    travelerTravelTo(dest, connector, backConnector)
    {
        local origin;
        local isDescribed;
        local actors;

        /* 
         *   Remember my departure original location - this is the
         *   location where the traveler was before we moved. 
         */
        origin = location;

        /*
         *   Determine if we're describing the travel.  We'll describe it
         *   if we're actually changing locations, or if the connector is
         *   explicitly a "circular" passage, which means that it connects
         *   back to the same location but involves a non-trivial passage,
         *   as sometimes happens in settings like caves.  
         */
        isDescribed = (dest != origin || connector.isCircularPassage);

        /* 
         *   Send a before-departure notification to everyone connected to
         *   by containment to the traveler. 
         */
        getNotifyTable().forEachAssoc(
            {obj, val: obj.beforeTravel(self, connector)});

        /* notify the actor initiating the travel */
        if (gActor != nil)
            gActor.actorTravel(self, connector);
        
        /* tell the old room we're leaving, if we changed locations */
        if (origin != nil && isDescribed)
            origin.travelerLeaving(self, dest, connector);

        /* notify the connector that we're traversing it */
        connector.noteTraversal(self);

        /* get the list of traveling actors */        
        actors = getTravelerActors();

        /* move to the destination */
        moveIntoForTravel(dest);

        /* 
         *   We've successfully completed the travel, so remember it for
         *   each actor involved in the travel.  Do this only if we
         *   actually went somewhere, or if the connector is circular - if
         *   we didn't end up going anywhere, and the connector isn't
         *   circular, we must have turned back halfway without completing
         *   the travel. 
         */
        if (dest != origin || connector.rememberCircularPassage)
        {
            foreach (local cur in actors)
            {
                /* ask the actor to remember the travel */
                cur.rememberTravel(origin, dest, backConnector);
                
                /* remember the destination of the connector for this actor */
                connector.rememberTravel(origin, cur, dest);
                
                /* 
                 *   If there's a back-connector, also remember travel in
                 *   the other direction.  The actor can reasonably be
                 *   expected to assume that the connector through which
                 *   it's arriving connects back to the source location, so
                 *   remember the association.  
                 */
                if (backConnector != nil)
                    backConnector.rememberTravel(dest, cur, origin);
            }
        }

        /*
         *   Recalculate the global sense context for message generation
         *   purposes, since we've moved to a new location.  It's possible
         *   that we've arrived at the player character's location, in
         *   which case we might have been suppressing messages (due to
         *   our being in a different room) but should now show messages
         *   (because we're now near the player character).  
         */
        if (gAction != nil)
            gAction.recalcSenseContext();

        /* tell the new room we're arriving, if we changed locations */
        if (location != nil && isDescribed)
            location.travelerArriving(self, origin, connector, backConnector);

        /* notify objects now connected by containment of the arrival */
        getNotifyTable().forEachAssoc(
            {obj, val: obj.afterTravel(self, connector)});
    }

    /*
     *   Perform "local" travel - that is, travel between nested rooms
     *   within a single top-level location.  By default, we simply defer
     *   to the actor to let it perform the local travel.  
     */
    travelerTravelWithin(actor, dest) 
    {
        actor.travelerTravelWithin(actor, dest);
    }

    /*
     *   Get a lookup table giving the set of objects to be notified of a
     *   beforeTravel/afterTravel event.  By default, we return a table
     *   including every object connected to the traveler by containment.  
     */
    getNotifyTable()
    {
        /* return the table of objects connected by containment */
        return connectionTable();
    }

    /*
     *   Determine if the given actor can see this traveler.  By default,
     *   we'll simply check to see if the actor can see 'self'.  
     */
    travelerSeenBy(actor) { return actor.canSee(self); }

    /*
     *   Is the given actor traveling with this traveler?  Returns true if
     *   the actor is in my getTravelerActors list. 
     */
    isActorTraveling(actor)
    {
        /* check to see if the given actor is in my actor list */
        return getTravelerActors.indexOf(actor) != nil;
    }

    /*
     *   Is the given object being carried by the traveler?  Returns true
     *   if the object is inside the traveler itself, or is inside any of
     *   the actors traveling. 
     */
    isTravelerCarrying(obj)
    {
        /* if the object is inside the traveler, it's being carried */
        if (obj.isIn(self))
            return true;

        /* if the object is inside any traveling actor, it's being carried */
        foreach (local cur in getTravelerActors)
        {
            if (obj.isIn(cur))
                return true;
        }

        /* the object isn't being carried */
        return nil;
    }

    /* invoke a callback function for each traveling actor */
    forEachTravelingActor(func)
    {
        /* by default, get the list, and invoke the callback per item */
        getTravelerActors.forEach(func);
    }

    /*
     *   Get the list of actors taking part in the travel.  When an actor
     *   is the traveler, this list simply contains the actor itself; for
     *   a vehicle or other composite traveler that moves more than one
     *   actor at a time, this should return the list of all of the actors
     *   involved in the travel.  
     */
    getTravelerActors = []

    /*
     *   Get the list of actors traveling undo their own power.  In the
     *   case of an actor traveling directly, this is just the actor; in
     *   the case of an actor pushing something, this is likewise the
     *   actor; in the case of a group of actors traveling together, this
     *   is the list of traveling actors; in the case of a vehicle, this
     *   is an empty list, since anyone traveling with the vehicle is
     *   traveling under the vehicle's power.  
     */
    getTravelerMotiveActors = []
;

/* ------------------------------------------------------------------------ */
/*
 *   A Travel Connector is a special connection interface that allows for
 *   travel from one location to another.  Most actor movement, except for
 *   movement between locations related by containment (such as from a room
 *   to sitting in a chair within the room) are handled through travel
 *   connector objects.
 *   
 *   Travel connectors are used in the directional link properties in rooms
 *   - north, south, east, west, in, out, etc.  A room direction link
 *   property is always set to a travel connector - but note that a room is
 *   itself a travel connector, so a travel link in one room can simply be
 *   set to point directly to another room.  In many cases, rooms
 *   themselves serve as travel connectors, so that one room can point a
 *   direction link property directly to another room.
 *   
 *   Some travel connectors are physical objects in the simulation, such as
 *   doors or stairways; other connectors are just abstract objects that
 *   represent connections, but don't appear as manipulable objects in the
 *   game.
 *   
 *   A travel connector provides several types of information about travel
 *   through its connection:
 *   
 *   - For actors actually traveling, the connector provides a method that
 *   moves an actor through the connector.  This method can trigger any
 *   side effects of the travel.  
 *   
 *   - For automatic map builders, actor scripts, and other callers who
 *   want to learn what can be known about the link without actually
 *   traversing it, the connector provides an "apparent destination"
 *   method.  This method returns the destination of travel through the
 *   connector that a given actor would expect just by looking at the
 *   connector.  The important thing about this routine is that it doesn't
 *   trigger any side effects, but simply indicates whether travel is
 *   apparently possible, and if so what the destination of the travel
 *   would be.  
 */
class TravelConnector: Thing
    /*
     *   Get any connector-specific pre-conditions for travel via this
     *   connector.
     */
    connectorTravelPreCond()
    {
        local lst;

        /* start with no conditions */
        lst = [];
        
        /* if we have a staging location, require that we're in it */
        if (connectorStagingLocation != nil)
            lst = [new TravelerDirectlyInRoom(gActor, self,
                                              connectorStagingLocation)];

        /* 
         *   If we're a physical Thing with a non-nil location, require
         *   that we be touchable.  Only physical objects need to be
         *   touchable; connectors are sometimes abstract objects, which
         *   obviously can't be touched.  
         */
        if (ofKind(Thing) && location != nil)
        {
            /* require that the traveler can touch the connector */
            local cond = new TouchObjCondition(gActor.getTraveler(self));
            lst += new ObjectPreCondition(self, cond);
        }

        /* return the result */
        return lst;
    }

    /* 
     *   The "staging location" for travel through this connector.  By
     *   default, if we have a location, that's our staging location; if
     *   we don't have a location (in which case we probably an outermost
     *   room), we don't have a staging location.  
     */
    connectorStagingLocation = (location)

    /*
     *   Get the travel preconditions that this connector requires for
     *   travel by the given actor.  In most cases, this won't depend on
     *   the actor, but it's provided as a parameter anyway; in most cases,
     *   this will just apply the conditions that are relevant to actors as
     *   travelers.
     *   
     *   By default, we require actors to be "travel ready" before
     *   traversing a connector.  The exact meaning of "travel ready" is
     *   provided by the actor's immediate location, but it usually simply
     *   means that the actor is standing.  This ensures that the actor
     *   isn't sitting in a chair or lying down or something like that.
     *   Some connectors might not require this, so this routine can be
     *   overridden per connector.
     *   
     *   Note that this will only be called when an actor is the traveler.
     *   When a vehicle or other kind of traveler is doing the travel, this
     *   will not be invoked.  
     */
    actorTravelPreCond(actor)
    {
        /* 
         *   create an object precondition ensuring that the actor is
         *   "travel ready"; the object of this precondition is the
         *   connector itself 
         */
        return [new ObjectPreCondition(self, actorTravelReady)];
    }

    /*
     *   Barrier or barriers to travel.  This property can be set to a
     *   single TravelBarrier object or to a list of TravelBarrier
     *   objects.  checkTravelBarriers() checks each barrier specified
     *   here.  
     */
    travelBarrier = []

    /*
     *   Check barriers.  The TravelVia check() routine must call this to
     *   enforce barriers.  
     */
    checkTravelBarriers(dest)
    {
        local traveler;
        local lst;

        /* get the traveler */
        traveler = gActor.getTraveler(self);

        /* ask the traveler what it thinks of travel through this connector */
        if (!traveler.canTravelVia(self, dest))
        {
            /* explain why the traveler can't pass */
            traveler.explainNoTravelVia(self, dest);

            /* terminate the command */
            exit;
        }

        /* check any travel conditions we apply directly */
        if (!canTravelerPass(traveler))
        {
            /* explain why the traveler can't pass */
            explainTravelBarrier(traveler);

            /* terminate the command */
            exit;
        }

        /* get the barrier list */
        lst = travelBarrier;

        /* if it's just a single object, make it a list of one element */
        if (!lst.ofKind(Collection))
            lst = [lst];
        
        /* check each item in our barrier list */
        foreach (local cur in lst)
        {
            /* if this barrier doesn't allow travel, we cannot travel */
            if (!cur.canTravelerPass(traveler))
            {
                /* ask the barrier to explain why travel isn't possible */
                cur.explainTravelBarrier(traveler);

                /* terminate the command */
                exit;
            }
        }
    }

    /*
     *   Check to see if the Traveler object is allowed to travel through
     *   this connector.  Returns true if travel is allowed, nil if not.
     *   
     *   This is called from checkTravelBarriers() to check any conditions
     *   coded directly into the TravelConnector.  By default, we simply
     *   return true; subclasses can override this to apply special
     *   conditions.
     *   
     *   If an override wants to disallow travel, it should return nil
     *   here, and then provide an override for explainTravelBarrier() to
     *   provide a descriptive message explaining why the travel isn't
     *   allowed.
     *   
     *   Conditions here serve essentially the same purpose as barrier
     *   conditions.  The purpose of providing this additional place for
     *   the same type of conditions is simply to improve the convenience
     *   of defining travel conditions for cases where barriers are
     *   unnecessary.  The main benefit of using a barrier is that the same
     *   barrier object can be re-used with multiple connectors, so if the
     *   same set of travel conditions apply to several different
     *   connectors, barriers allow the logic to be defined once in a
     *   single barrier object and then re-used easily in each place it's
     *   needed.  However, when a particular condition is needed in only
     *   one place, creating a barrier to represent the condition is a bit
     *   verbose; in such cases, the condition can be placed in this method
     *   more conveniently.  
     */
    canTravelerPass(traveler) { return true; }

    /*
     *   Explain why canTravelerPass() returned nil.  This is called to
     *   display an explanation of why travel is not allowed by
     *   self.canTravelerPass().
     *   
     *   Since the default canTravelerPass() always allows travel, the
     *   default implementation of this method does nothing.  Whenever
     *   canTravelerPass() is overridden to return nil, this should also be
     *   overridden to provide an appropriate explanation.  
     */
    explainTravelBarrier(traveler) { }

    /*
     *   Is this connector listed?  This indicates whether or not the exit
     *   is allowed to be displayed in lists of exits, such as in the
     *   status line or in "you can't go that way" messages.  By default,
     *   all exits are allowed to appear in listings.
     *   
     *   Note that this indicates if listing is ALLOWED - it doesn't
     *   guarantee that listing actually occurs.  A connector can be
     *   listed only if this is true, AND the point-of-view actor for the
     *   listing can perceive the exit (which means that
     *   isConnectorApparent must return true, and there must be
     *   sufficient light to see the exit).  
     */
    isConnectorListed = true

    /*
     *   Get an unlisted proxy for this connector.  This is normally
     *   called from the asExit() macro to set up one room exit direction
     *   as an unlisted synonym for another.  
     */
    createUnlistedProxy() { return new UnlistedProxyConnector(self); }

    /*
     *   Determine if the travel connection is apparent - as a travel
     *   connector - to the actor in the given origin location.  This
     *   doesn't indicate whether or not travel is possible, or where
     *   travel goes, or that the actor can tell where the passage goes;
     *   this merely indicates whether or not the actor should realize
     *   that the passage exists at all.
     *   
     *   A closed door, for example, would return true, because even a
     *   closed door makes it clear that travel is possible in the
     *   direction, even if it's not possible currently.  A secret door,
     *   on the other hand, would return nil while closed, because it
     *   would not be apparent to the actor that the object is a door at
     *   all.  
     */
    isConnectorApparent(origin, actor)
    {
        /* by default, passages are apparent */
        return true;
    }

    /*
     *   Determine if the travel connection is passable by the given
     *   traveler in the current state.  For example, a door would return
     *   true when open, nil when closed.
     *   
     *   This information is intended to help game code probing the
     *   structure of the map.  This information is NOT used in actor
     *   travel; for actor travel, we rely on custom checks in the
     *   connector's TravelVia handler to enforce the conditions of travel.
     *   Actor travel uses TravelVia customizations rather than this method
     *   because that allows better specificity in reporting failures.
     *   This method lets game code get at the same information, but in a
     *   more coarse-grained fashion.  
     */
    isConnectorPassable(origin, traveler)
    {
        /* by default, we're passable */
        return true;
    }

    /*
     *   Get the apparent destination of travel by the actor to the given
     *   origin.  This returns the location to which the connector
     *   travels, AS FAR AS THE ACTOR KNOWS.  If the actor does not know
     *   and cannot tell where the connector leads, this should return nil.
     *   
     *   Note that this method does NOT necessarily return the actual
     *   destination, because we obviously can't know the destination for
     *   certain until we traverse the connection.  Rather, the point of
     *   this routine is to return as much information as the actor is
     *   supposed to have.  This can be used for purposes like
     *   auto-mapping, where we'd want to show what the player character
     *   knows of the map, and NPC goal-seeking, where an NPC tries to
     *   figure out how to get from one point to another based on the
     *   NPC's knowledge of the map.  In these sorts of applications, it's
     *   important to use only knowledge that the actor is supposed to
     *   have within the parameters of the simulation.
     *   
     *   Callers should always test isConnectorApparent() before calling
     *   this routine.  This routine does not check to ensure that the
     *   connector is apparent, so it could return misleading information
     *   if used independently of isConnectorApparent(); for example, if
     *   the connector *formerly* worked but has now disappeared, and the
     *   actor has a memory of the former destination, we'll return the
     *   remembered destination.
     *   
     *   The actor can know the destination by a number of means:
     *   
     *   1.  The location is familiar to the character.  For example, if
     *   the setting is the character's own house, the character would
     *   obviously know the house well, so would know where you'd end up
     *   going east from the living room or south from the kitchen.  We
     *   use the origin method actorKnowsDestination() to determine this.
     *   
     *   2.  The destination is readily visible from the origin location,
     *   or is clearly marked.  For example, in an outdoor setting, it
     *   might be clear that going east from the field takes you to the
     *   hilltop.  In an indoor setting, an open passage might make it
     *   clear that going east from the living room takes you to the
     *   dining room.  We use the origin method actorKnowsDestination() to
     *   determine this.
     *   
     *   3. The actor has been through the connector already in the course
     *   of the game, and so remembers the connection by virtue of recent
     *   experience.  If our travelMemory class property is set to a
     *   non-nil lookup table object, then we'll automatically use the
     *   lookup table to remember the destination each time an actor
     *   travels via a connector, and use this information by default to
     *   provide apparent destination information.  
     */
    getApparentDestination(origin, actor)
    {
        local dest;
        
        /* 
         *   Ask the origin if the actor knows the destination for the
         *   given connector.  If so, and we can determine our
         *   destination, then return the destination.  
         */
        if (origin.actorKnowsDestination(actor, self)
            && (dest = getDestination(origin,
                                      actor.getTraveler(self))) != nil)
            return dest;

        /*
         *   If we have a travelMemory table, look to see if the traversal
         *   of this actor via this connector from this origin is recorded
         *   in the table, and if so, assume that the destination is the
         *   same as it was last time.
         *   
         *   Note that we ignore our memory of travel if we never saw the
         *   destination of the travel (which would be the case if the
         *   destination was dark every time we've been there, so we've
         *   never seen any details about the location).  
         */
        if (travelMemory != nil
            && (dest = travelMemory[[actor, origin, self]]) != nil
            && actor.hasSeen(dest))
        {
            /* we know the destination from past experience */
            return dest;
        }

        /* we don't know the destination */
        return nil;
    }

    /*
     *   Get our destination, given the traveler and the origin location.
     *   
     *   This method is required to return the current destination for the
     *   travel.  If the connector doesn't go anywhere, this should return
     *   nil.  The results of this method must be stable for the extent of
     *   a turn, up until the time travel actually occurs; in other words,
     *   it must be possible to call this routine simply for information
     *   purposes, to determine where the travel will end up.
     *   
     *   This method should not trigger any side effects, since it's
     *   necessary to be able to call this method more than once in the
     *   course of a given travel command.  If it's necessary to trigger
     *   side effects when the connector is actually traversed, apply the
     *   side effects in noteTraversal().
     *   
     *   For auto-mapping and the like, note that getApparentDestination()
     *   is a better choice, since this method has internal information
     *   that might not be apparent to the characters in the game and thus
     *   shouldn't be revealed through something like an auto-map.  This
     *   method is intended for internal use in the course of processing a
     *   travel action, since it knows the true destination of the travel.
     */
    getDestination(origin, traveler) { return nil; }

    /*
     *   Get the travel connector leading to the given destination from the
     *   given origin and for the given travel.  Return nil if we don't
     *   know a connector leading there.
     *   
     *   By default, we simply return 'self' if our destination is the
     *   given destination, or nil if not.
     *   
     *   Some subclasses might encapsulate one or more "secondary"
     *   connectors - that is, the main connector might choose among
     *   multiple other connectors.  In these cases, the secondary
     *   connectors typically won't be linked to directions on their own,
     *   so the room can't see them directly - it can only find them
     *   through us, since we're effectively a wrapper for the secondary
     *   connectors.  In these cases, we won't have any single destination
     *   ourself, so getDestination() will have to return nil.  But we
     *   *can* work backwards: given a destination, we can find the
     *   secondary connector that points to that destination.  That's what
     *   this routine is for.  
     */
    connectorGetConnectorTo(origin, traveler, dest)
    {
        /* if we go there, return 'self', else return nil */
        return (getDestination(origin, traveler) == dest ? self : nil);
    }

    /* 
     *   Note that the connector is being traversed.  This is invoked just
     *   before the traveler is moved; this notification is fired after the
     *   other travel-related notifications (beforeTravel, actorTravel,
     *   travelerLeaving).  This is a good place to display any special
     *   messages describing what happens during the travel, because any
     *   messages displayed here will come after any messages related to
     *   reactions from other objects.  
     */
    noteTraversal(traveler)
    {
        /* do nothing by default */
    }

    /*
     *   Service routine: add a memory of a successful traversal of a
     *   travel connector.  If we have a travel memory table, we'll add
     *   the traversal to the table, so that we can find it later.
     *   
     *   This is called from Traveler.travelerTravelTo() on successful
     *   travel.  We're called for each actor participating in the travel.
     */
    rememberTravel(origin, actor, dest)
    {
        /* 
         *   If we have a travelMemory table, add this traversal.  Store
         *   the destination, keyed by the combination of the actor,
         *   origin, and connector object (i.e., self) - this will allow
         *   us to remember the destination we reached last time if we
         *   want to know where the same route goes in the future.  
         */
        if (TravelConnector.travelMemory != nil)
            TravelConnector.travelMemory[[actor, origin, self]] = dest;
    }

    /*
     *   Our "travel memory" table.  If this contains a non-nil lookup
     *   table object, we'll store a record of each successful traversal
     *   of a travel connector here - we'll record the destination keyed
     *   by the combination of actor, origin, and connector, so that we
     *   can later check to see if the actor has any memory of where a
     *   given connector goes from a given origin.
     *   
     *   We keep this information by default, which is why we statically
     *   create the table here.  Keeping this information does involve
     *   some overhead, so some authors might want to get rid of this
     *   table (by setting the property to nil) if the game doesn't make
     *   any use of the information.  Note that this table is stored just
     *   once, in the TravelConnector class itself - there's not a
     *   separate table per connector.  
     */
    travelMemory = static new LookupTable(256, 512)

    /*
     *   Is this a "circular" passage?  A circular passage is one that
     *   explicitly connects back to its origin, so that traveling through
     *   the connector leaves us where we started.  When a passage is
     *   marked as circular, we'll describe travel through the passage
     *   exactly as though we had actually gone somewhere.  By default, if
     *   traveling through a passage leaves us where we started, we assume
     *   that nothing happened, so we don't describe any travel.
     *   
     *   Circular passages don't often occur in ordinary settings; these
     *   are mostly useful in disorienting environments, such as twisty
     *   cave networks, where a passage between locations can change
     *   direction and even loop back on itself.  
     */
    isCircularPassage = nil

    /*
     *   Should we remember a circular trip through this passage?  By
     *   default, we remember the destination of a passage that takes us
     *   back to our origin only if we're explicitly marked as a circular
     *   passage; in other cases, we assume that the travel was blocked
     *   somehow instead.  
     */
    rememberCircularPassage = (isCircularPassage)

    /*
     *   Describe an actor's departure through the connector from the
     *   given origin to the given destination.  This description is from
     *   the point of view of another actor in the origin location.  
     */
    describeDeparture(traveler, origin, dest)
    {
        local dir;
        
        /* 
         *   See if we can find a direction linked to this connector from
         *   the origin location.  If so, describe the departure using the
         *   direction; otherwise, describe it using the generic departure
         *   message.
         *   
         *   Find the connector from the player character's perspective,
         *   because the description we're generating is for the player's
         *   benefit and thus should be from the PC's perspective.  
         */
        if ((dir = origin.directionForConnector(self, gPlayerChar)) != nil)
        {
            /*
             *   We found a direction linked to the connector, so this
             *   must have been the way they traveled.  Describe the
             *   departure as being in that direction.
             *   
             *   Note that it's possible that more than one direction is
             *   linked to the same connector.  In such cases, we'll just
             *   take the first link we find, because it's equally
             *   accurate to say that the actor went in any of the
             *   directions linked to the same connector.  
             */
            traveler.sayDepartingDir(dir, self);
        }
        else
        {
            /*
             *   We didn't find any direction out of the origin linking to
             *   this connector, so we don't know how what direction they
             *   went.  Show the generic departure message.  
             */
            traveler.sayDeparting(self);
        }
    }

    /*
     *   Describe an actor's arrival through the connector from the given
     *   origin into the given destination.  This description is from the
     *   point of view of another actor in the destination.
     *   
     *   Note that this is called on the connector that reverses the
     *   travel, NOT on the connector the actor is actually traversing -
     *   that is, 'self' is the backwards connector, leading from the
     *   destination back to the origin location.  So, if we have two
     *   sides to a door, and the actor traverses the first side, this
     *   will be called on the second side - the one that links the
     *   destination back to the origin.  
     */
    describeArrival(traveler, origin, dest)
    {
        local dir;
        
        /*
         *   See if we can find a direction linked to this connector in
         *   the destination location.  If so, describe the arrival using
         *   the direction; otherwise, describe it using a generic arrival
         *   message.  
         *   
         *   Find the connector from the player character's perspective,
         *   because the description we're generating is for the player's
         *   benefit and thus should be from the PC's perspective.  
         */
        if ((dir = dest.directionForConnector(self, gPlayerChar)) != nil)
        {
            /* 
             *   we found a direction linked to this back connector, so
             *   describe the arrival as coming from the direction we
             *   found 
             */
            traveler.sayArrivingDir(dir, self);
        }
        else
        {
            /* 
             *   we didn't find any direction links, so use a generic
             *   arrival message 
             */
            traveler.sayArriving(self);
        }
    }

    /*
     *   Describe a "local departure" via this connector.  This is called
     *   when a traveler moves around entirely within the field of view of
     *   the player character, and move *further away* from the PC - that
     *   is, the traveler's destination is visible to the PC when we're
     *   leaving our origin, AND the origin's top-level location contains
     *   the PC.  We'll describe the travel not in terms of truly
     *   departing, but simply in terms of moving away. 
     */
    describeLocalDeparture(traveler, origin, dest)
    {
        /* say that we're departing locally */
        traveler.sayDepartingLocally(dest, self);
    }

    /*
     *   Describe a "local arrival" via this connector.  This is called
     *   when the traveler moves around entirely within the field of view
     *   of the player character, and comes *closer* to the PC - that is,
     *   the traveler's origin is visible to the player character when we
     *   arrive in our destination, AND the destination's top-level
     *   location contains the PC.  We'll describe the travel not in terms
     *   of truly arriving, since the traveler was already here to start
     *   with, but rather as entering the destination, but just in terms of
     *   moving closer.  
     */
    describeLocalArrival(traveler, origin, dest)
    {
        /* say that we're arriving locally */
        traveler.sayArrivingLocally(dest, self);
    }

    /*
     *   Describe "remote travel" via this connector.  This is called when
     *   the traveler moves around entirely within the field of view of the
     *   PC, but between two "remote" top-level locations - "remote" means
     *   "does not contain the PC."  In this case, the traveler isn't
     *   arriving or departing, exactly; it's just moving laterally from
     *   one top-level location to another.  
     */
    describeRemoteTravel(traveler, origin, dest)
    {
        /* say that we're traveling laterally */
        traveler.sayTravelingRemotely(dest, self);
    }

    /*
     *   Find a connector in the destination location that connects back as
     *   the source of travel from the given connector when traversed from
     *   the source location.  Returns nil if there is no such connector.
     *   This must be called while the traveler is still in the source
     *   location; we'll attempt to find the connector back to the
     *   traveler's current location.
     *   
     *   The purpose of this routine is to identify the connector by which
     *   the traveler arrives in the new location.  This can be used, for
     *   example, to generate a connector-specific message describing the
     *   traveler's emergence from the connector (so we can say one thing
     *   if the traveler arrives via a door, and another if the traveler
     *   arrives by climing up a ladder).
     *   
     *   By default, we'll try to find a travel link in the destination
     *   that links us back to this same connector, in which case we'll
     *   return 'self' as the connector from which the traveler emerges in
     *   the new location.  Failing that, we'll look for a travel link
     *   whose apparent source is the origin location.
     *   
     *   This should be overridden for any connector with an explicit
     *   complementary connector.  For example, it is common to implement a
     *   door using a pair of objects, one representing each side of the
     *   door; in such cases, each door object would simply return its twin
     *   here.  Note that a complementary connector doesn't actually have
     *   to go anywhere, since it's still useful to have a connector back
     *   simply for describing travelers arriving on the connector.
     *   
     *   This *must* be overridden when the destination location doesn't
     *   have a simple connector whose apparent source is this connector,
     *   because in such cases we won't be able to find the reverse
     *   connector with our direction search.  
     */
    connectorBack(traveler, dest)
    {
        local origin;

        /* if there's no destination, there's obviously no connector */
        if (dest == nil)
            return nil;

        /* 
         *   get the origin location - this the traveler's current
         *   immediate container 
         */
        origin = traveler.location;
        
        /* 
         *   First, try to find a link back to this same connector - this
         *   will handle simple symmetrical links with the same connector
         *   object shared between two rooms.
         *   
         *   We try to find the actual connector before looking for a
         *   connector back to the origin - it's possible that there are
         *   several ways back to the starting point, and we want to make
         *   sure we pick the one that was actually traversed if possible.
         */
        foreach (local dir in Direction.allDirections)
        {
            /* 
             *   If this direction link from the destination is linked back
             *   to this same connector, we have a symmetrical connection,
             *   so we're the connector back.  Note that we're interested
             *   only in map structure here, so we don't pass an actor; the
             *   actor isn't actually in the new location, so what the
             *   actor can see is irrelevant to us here.  
             */
            if (dest.getTravelConnector(dir, nil) == self)
            {
                /* the same connector goes in both directions */
                return self;
            }
        }

        /*
         *   we didn't find a link back to the same connector, so try to
         *   find a link from the destination whose apparent source is the
         *   origin 
         */
        foreach (local dir in Direction.allDirections)
        {
            local conn;
            
            /* 
             *   if this link from the destination has an apparent source
             *   of our origin, the traveler appears to be arriving from
             *   this link 
             */
            if ((conn = dest.getTravelConnector(dir, nil)) != nil
                && conn.fixedSource(dest, traveler) == origin)
            {
                /* 
                 *   this direction has an apparent source of the origin
                 *   room - it's not necessarily the same link they
                 *   traversed, but at least it appears to come from the
                 *   same place they came from, so it'll have to do 
                 */
                return conn;
            }
        }

        /* we couldn't find any link back to the origin */
        return nil;
    }

    /*
     *   Get the "fixed" source for travelers emerging from this connector,
     *   if possible.  This can return nil if the connector does not have a
     *   fixed relationship with another connector.
     *   
     *   The purpose of this routine is to find complementary connectors
     *   for simple static map connections.  This is especially useful for
     *   direct room-to-room connections.
     *   
     *   When a connector relationship other than a simple static mapping
     *   exists, the connectors must generally override connectorBack(), in
     *   which case this routine will not be needed (at least, this routine
     *   won't be needed as long as the overridden connectorBack() doesn't
     *   call it).  Whenever it is not clear how to implement this routine,
     *   don't - implement connectorBack() instead.  
     */
    fixedSource(dest, traveler)
    {
        /* by default, return nothing */
        return nil;
    }

    /*
     *   Can the given actor see this connector in the dark, looking from
     *   the given origin?  Returns true if so, nil if not.
     *   
     *   This is used to determine if the actor can travel from the given
     *   origin via this connector when the actor (in the origin location)
     *   is in darkness.
     *   
     *   By default, we implement the usual convention, which is that
     *   travel from a dark room is possible only when the destination is
     *   lit.  If we can't determine our destination, we will assume that
     *   the connector is not visible.
     */
    isConnectorVisibleInDark(origin, actor)
    {
        local dest;
        
        /* 
         *   Get my destination - if we can't determine our destination,
         *   then assume we're not visible. 
         */
        if ((dest = getDestination(origin, actor.getTraveler(self))) == nil)
            return nil;

        /*
         *   Check the ambient illumination level in the destination.  If
         *   it's 2 or higher, then it's lit; otherwise, it's dark.  If
         *   the destination is lit, consider the connector to be visible,
         *   on the theory that the connector lets a little bit of the
         *   light from the destination leak into the origin room - just
         *   enough to make the connection itself visible without making
         *   anything else in the origin room visible.  
         */
        return (dest.wouldBeLitFor(actor));
    }

    /*
     *   Handle travel in the dark.  Specifically, this is called when an
     *   actor attempts travel from one dark location to another dark
     *   location.  (We don't invoke this in any other case:
     *   light-to-light, light-to-dark, and dark-to-light travel are all
     *   allowed without any special checks.)
     *   
     *   By default, we will prohibit dark-to-dark travel by calling the
     *   location's darkTravel handler.  Individual connectors can
     *   override this to allow such travel or apply different handling.  
     */
    darkTravel(actor, dest)
    {
        /* 
         *   by default, simply call the actor's location's darkTravel
         *   handler 
         */
        actor.location.roomDarkTravel(actor);
    }

    /*
     *   Action handler for the internal "TravelVia" action.  This is not a
     *   real action, but is instead a pseudo-action that we implement
     *   generically for travel via the connector.  Subclasses that want to
     *   handle real actions by traveling via the connector can use
     *   remapTo(TravelVia) to implement the real action handlers.  Note
     *   that remapTo should be used (rather than, say, asDobjFor), since
     *   this will ensure that every type of travel through the connector
     *   actually looks like a TravelVia action, which is useful for
     *   intercepting travel actions generically in other code.  
     */
    dobjFor(TravelVia)
    {
        preCond()
        {
            /* 
             *   For our preconditions, use the traveler's preconditions,
             *   plus the location's preconditions, plus any special
             *   connector-specific preconditions we supply. 
             */
            return gActor.getTraveler(self).travelerPreCond(self)
                + gActor.location.roomTravelPreCond()
                + connectorTravelPreCond();
        }
        verify()
        {
            /*
             *   Verify travel for the current command's actor through this
             *   connector.  This performs normal action verify processing.
             *   
             *   The main purpose of this routine is to allow the connector
             *   to flag obviously dangerous travel to allow a caller to
             *   avoid such travel as an implicit or scripted action.  In
             *   most cases, there's no need to make travel illogical
             *   because there's generally no potential ambiguity involved
             *   in analyzing a travel verb.
             *   
             *   Note that this routine must check with the actor to
             *   determine if the actor or a vehicle will actually be
             *   performing the travel, by calling gActor.getTraveler(), if
             *   the routine cares about the difference.  
             */
        }
        check()
        {
            local t = gActor.getTraveler(self);
            local dest;

            /*
             *   Check the travel. 
             *   
             *   This routine should take into account the light levels at
             *   the source and destination locations, if travel between
             *   dark rooms is to be disallowed.  
             */
            
            /* get my destination */
            dest = getDestination(t.location, t);
            
            /* check dark-to-dark travel */
            gActor.checkDarkTravel(dest, self);

            /* enforce barriers */
            checkTravelBarriers(dest);
        }
        
        action()
        {
            local t = gActor.getTraveler(self);
            local dest;

            /*
             *   Execute the travel, moving the command's actor through the
             *   travel connection: we carry out any side effects of the
             *   travel and deliver the actor (if appropriate) to the
             *   destination of the connector.
             *   
             *   Note that this routine must check with the actor to
             *   determine if the actor or a vehicle will actually be
             *   performing the travel, by calling gActor.getTraveler(), if
             *   the routine cares about the difference.  In most cases,
             *   the routine won't care: most implementations of this
             *   routine will (if they effect any travel at all) eventually
             *   call gActor.travelTo() to carry out the travel, and that
             *   routine will always route the actual movement to the
             *   vehicle if necessary.  
             */
            
            /* get my destination */
            dest = getDestination(t.location, t);

            /* travel to my destination */
            gActor.travelTo(dest, self, connectorBack(t, dest));
        }
    }
;

/*
 *   A "probe" object, for testing light levels in rooms.  This is a dummy
 *   object that we use for what-if testing - it's not actually part of
 *   the simulation.  
 */
lightProbe: Thing;

/*
 *   A TravelBarrier can be attached to a TravelConnector, via the
 *   travelBarrier property, to form a conditional barrier to travel.
 */
class TravelBarrier: object
    /*
     *   Determine if this barrier blocks the given traveler.  By default,
     *   we don't block anyone.  This doesn't make us much of a barrier, so
     *   subclasses should override this with a more specific condition.  
     */
    canTravelerPass(traveler) { return true; }

    /*
     *   Explain why travel isn't allowed.  This should generate an
     *   appropriate failure report explaining the problem.  This is
     *   invoked when travel is attempted and canTravelerPass returns nil.
     *   Subclasses must override this.  
     */
    explainTravelBarrier(traveler) { }
;

/*
 *   An "unlisted proxy" connector acts as a proxy for another connector.
 *   We act exactly like the underlying connector, except that we suppress
 *   the connector from automatic exit lists.  This can be used for cases
 *   where an otherwise normal connector is needed but the connector is
 *   not to appear in automatic exit lists (such as the status line).
 *   
 *   The most common situation where this kind of connector is useful is
 *   where multiple directions in a given room all go to the same
 *   destination.  In these cases, it's often desirable for some of the
 *   directions to be unlisted alternatives.  The asExit() macro can be
 *   used for convenience to set up these direction synonyms.  
 */
class UnlistedProxyConnector: object
    construct(pri)
    {
        /* remember my underlying primary connector */
        primaryConn = pri;
    }

    /* 
     *   Our underlying connector.  Start out with a default TadsObject
     *   rather than nil in case anyone wants to call a property or test
     *   inheritance before we're finished with our constructor - this will
     *   produce reasonable default behavior without having to test for nil
     *   everywhere.  
     */
    primaryConn = TadsObject

    /* we're not listed */
    isConnectorListed = nil

    /* map any TravelVia action to our underlying connector */
    dobjFor(TravelVia) remapTo(TravelVia, primaryConn)

    /* redirect everything we don't handle to the underlying connector */
    propNotDefined(prop, [args]) { return primaryConn.(prop)(args...); }

    /* 
     *   As a proxy, we don't want to disguise the fact that we're a proxy,
     *   if someone specifically asks, so admist to being of our own true
     *   kind; but we also act mostly like our underlying connector, so if
     *   someone wants to know if we're one of those, say yes to that as
     *   well.  So, return true if the inherited version returns true, and
     *   also return true if our primary connector would return true.  
     */
    ofKind(cls) { return inherited(cls) || primaryConn.ofKind(cls); }
;


/*
 *   A travel connector that doesn't allow any travel - if travel is
 *   attempted, we simply use the origin's cannotTravel method to display
 *   an appropriate message.  
 */
noTravel: TravelConnector
    /* it is obvious that is no passage this way */
    isConnectorApparent(origin, actor) { return nil; }

    /* this is not a passable connector */
    isConnectorPassable(origin, traveler) { return nil; }

    dobjFor(TravelVia)
    {
        /* 
         *   we know that no travel will occur, so we don't need to satisfy
         *   any preconditions 
         */
        preCond = []

        action()
        {
            /* we can't go this way - use the origin's default message */
            gActor.location.cannotTravel();
        }
    }
;

/*
 *   An "ask which" travel connector.  Rather than just traversing a
 *   connector, we ask for a direct object for a specified travel verb; if
 *   the player supplies the missing indirect object (or if the parser can
 *   automatically choose a default), we'll perform the travel verb using
 *   that direct object.
 *   
 *   This type of connector has two uses.
 *   
 *   First, the library various instances, with appropriate specified
 *   travel verbs, as the default connector for certain directions that
 *   frequently end up mapping to in-scenario objects.  Specifically,
 *   noTravelIn, noTravelDown, and noTravelOut are used as the default in,
 *   down, and out connectors.  If the player types DOWN, for example, and
 *   there's no override for 'down' in a given room, then we'll invoke
 *   noTravelDown; this will in turn ask for a missing direct object for
 *   the GetOffOf action, since DOWN can mean getting off of a platform or
 *   other nested room when on such a thing.  When there's an obvious
 *   thing to get down from, the parser will provide the default
 *   automatically, which will make DOWN into a simple synonym for GET OFF
 *   OF <whatever>.
 *   
 *   Second, games can use this kind of connector for a given direction
 *   when the direction is ambiguous.  For example, you can use this as
 *   the 'north' connector when there are two doors leading north from the
 *   location.  When the player types NORTH, the parser will ask which
 *   door the player wants to go through.  
 */
class AskConnector: TravelConnector, ResolveAsker
    /* 
     *   The specific travel action to attempt.  This must be a TAction -
     *   an action that takes a direct object (and only a direct object).
     *   The default is TravelVia, but this should usually be customized
     *   in each instance to the type of travel appropriate for the
     *   possible connectors.  
     */
    travelAction = TravelViaAction

    /*
     *   The list of possible direct objects for the travel action.  If
     *   this is nil, we'll simply treat the direct object of the
     *   travelAction as completely missing, forcing the parser to either
     *   find a default or ask the player for the missing object.  If the
     *   travel is limited to a specific set of objects (for example, if
     *   there are two doors leading north, and we want to ask which one
     *   to use), this should be set to the list of possible objects; the
     *   parser will then use the ambiguous noun phrase rules instead of
     *   the missing noun phrase rules to ask the player for more
     *   information. 
     */
    travelObjs = nil

    /*
     *   The phrase to use in the disambiguation question to ask which of
     *   the travelObjs entries is to be used.  The language-specific
     *   module provides a suitable default, but this should usually be
     *   overridden if travelObjs is overridden.  
     */
    travelObjsPhrase = nil

    /*
     *   An extra prompt message to show before the normal parser prompt
     *   for a missing or ambiguous object.  We'll show this just before
     *   the normal parser message, if it's specified.
     *   
     *   If you want to customize the messages more completely, you can
     *   override askDisambig() or askMissingObject().  The parser will
     *   invoke these to generate the prompt, so you can customize the
     *   entire messages by overriding these.  
     */
    promptMessage = nil

    /*
     *   For each of the ResolveAsker methods that might be invoked, add
     *   the promptMessage text before the normal parser question. 
     */
    askDisambig(targetActor, promptTxt, curMatchList, fullMatchList,
                requiredNum, askingAgain, dist)
    {
        promptMessage;
        inherited(targetActor, promptTxt, curMatchList, fullMatchList,
                  requiredNum, askingAgain, dist);
    }

    askMissingObject(targetActor, action, which)
    {
        promptMessage;
        inherited(targetActor, action, which);
    }
    
    /* handle travel via this connector */
    dobjFor(TravelVia)
    {
        /* 
         *   No preconditions or checks are necessary, since we don't
         *   actually perform any travel on our own; we simply recast the
         *   command as a new action, hence we want to delegate the
         *   preconditions and check() handling to the replacement action.
         *   Note that this means that you can't put a travel barrier
         *   directly on an AskConnector - you have to put any barriers on
         *   the underlying real connectors instead.  
         */
        preCond = []
        check() { }

        /* 
         *   Recast the travel into our specified action, asking for the
         *   direct object we need for that action.  
         */
        action()
        {
            /* 
             *   if we have a set of possible direct objects, retry this
             *   with the ambiguous object set; otherwise, retry with a
             *   completely missing direct object 
             */
            if (travelObjs != nil)
                travelAction.retryWithAmbiguousDobj(
                    gAction, travelObjs, self, travelObjsPhrase);
            else
                travelAction.retryWithMissingDobj(gAction, self);
        }
    }

    /*
     *   Get a connector leading to the given destination.  We'll scan our
     *   travel objects; for each one that's a TravelConnector, we'll ask
     *   it to find the connector, and return the result if we get one.  
     */
    connectorGetConnectorTo(origin, traveler, dest)
    {
        /* if we have no travel objects, there's nothing to check */
        if (travelObjs == nil)
            return nil;
        
        /* scan our secondary connectors */
        foreach (local cur in travelObjs)
        {
            /* if this is a travel connector, ask it what it thinks */
            if (cur.ofKind(TravelConnector))
            {
                local conn;
                
                /* 
                 *   if this secondary connector can give us a connector to
                 *   the destination, use that connector 
                 */
                conn = cur.connectorGetConnectorTo(origin, traveler, dest);
                if (conn != nil)
                    return conn;
            }
        }

        /* didn't find a match */
        return nil;
    }
;

/*
 *   A "default ask connector" is an AskConnector that we use for certain
 *   directions (down, in, out) as the library default connector for the
 *   directions.
 */
class DefaultAskConnector: AskConnector
    /* 
     *   since this is a default connector for all locations, indicate that
     *   no travel is apparently possible in this direction 
     */
    isConnectorApparent(origin, actor) { return nil; }

    /* this is not a passable connector */
    isConnectorPassable(origin, traveler) { return nil; }
;

/*
 *   A default travel connector for going in.  When travel in the relative
 *   direction "in" isn't allowed, we'll try recasting the command as an
 *   "enter" command with an unknown direct object. 
 */
noTravelIn: DefaultAskConnector
    /* when we go 'in', we'll try to ENTER something */
    travelAction = EnterAction
;

/*
 *   A default travel connector for going out.  When travel in the
 *   relative direction "out" isn't allowed, we'll try recasting the
 *   command as an "get out of" command with an unknown direct object. 
 */
noTravelOut: DefaultAskConnector
    /* when we go 'out', we'll try to GET OUT OF something */
    travelAction = GetOutOfAction
;

/*
 *   A default travel connector for going out from a nested room.  This
 *   works the same way as noTravelOut, except that we'll show OUT as a
 *   listed exit.  
 */
nestedRoomOut: noTravelOut
    isConnectorApparent(origin, actor) { return true; }
;

/*
 *   A special travel connector for 'down' that recasts the command as a
 *   "get off of" command.  This can be used for platforms and the like,
 *   where a 'down' command should usually be taken to mean "get off
 *   platform" rather than "down from enclosing room". 
 */
noTravelDown: DefaultAskConnector
    /* when we go 'down', we'll try to GET OFF OF something */
    travelAction = GetOffOfAction
;

/*
 *   A travel connector for going in that explicitly redirects the command
 *   to "enter" and asks for the missing direct object.  This behaves the
 *   same way as noTravelIn, but explicitly makes the inward travel
 *   apparent; this can be used to override the noTravelIn default for
 *   locations where travel in is explicitly allowed.  
 */
askTravelIn: AskConnector
    travelAction = EnterAction
;

/* explicitly redirect travel out to "get out of" */
askTravelOut: AskConnector
    travelAction = GetOutOfAction
;

/* explicitly redirect travel down to "get off of" */
askTravelDown: AskConnector
    travelAction = GetOffOfAction
;

/*
 *   "No Shipboard Directions" travel connector.  This is used as the
 *   default connector for the shipboard directions for the base Room
 *   class.  This connector displays a special message indicating that the
 *   room is not a boat hence the shipboard directions don't work here.  
 */
noShipTravel: noTravel
    dobjFor(TravelVia)
    {
        action()
        {
            /* simply indicate that this direction isn't applicable here */
            gLibMessages.notOnboardShip();
        }
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   A mix-in class that can be added to objects that also inherit from
 *   TravelConnector to add a message as the connector is traversed.
 *   
 *   Note that this isn't itself a travel connector; it's just a class
 *   that should be combined with TravelConnector or one of its
 *   subclasses.  This class should be in the superclass list before the
 *   TravelConnector-derived superclass.  
 */
class TravelWithMessage: object
    /*
     *   My message to display when the player character traverses the
     *   connector.  This should be overridden with the custom message for
     *   the connector.  By default, if we're a Script, we'll invoke the
     *   script to show the next message.  
     */
    travelDesc()
    {
        if (ofKind(Script))
            doScript();
    }

    /*
     *   My message to display when any non-player character traverses the
     *   connector.  If this is not overridden, no message will be
     *   displayed when an NPC travels through the connector.  
     */
    npcTravelDesc = ""

    /*
     *   Display my message.  By default, we show one message for the
     *   player character and another message for NPC's.  
     */
    showTravelDesc()
    {
        if (gActor.isPlayerChar())
            travelDesc;
        else
            npcTravelDesc;
    }

    /* on traversing the connector, show our message */
    noteTraversal(traveler)
    {
        /* display my message */
        showTravelDesc();

        /* inherit any other superclass handling as well */
        inherited(traveler);
    }
;

/*
 *   A simple connector that displays a message when the connector is
 *   traversed.
 */
class TravelMessage: TravelWithMessage, TravelConnector
    /* my destination location */
    destination = nil

    /* get my destination */
    getDestination(origin, traveler) { return destination; }

    /* our source is the same as our destination */
    fixedSource(dest, traveler) { return destination; }
;

/*
 *   A travel connector that can't be traversed, and which shows a custom
 *   failure message when traversal is attempted.  Instances should define
 *   travelDesc to the message to display when travel is attempted.
 *   
 *   Travel is not apparently possible in this direction, so this type of
 *   connector will not show up in automatic exit lists or maps.  This
 *   class is designed for connections that are essentially the same as no
 *   connection at all, but where it's desirable to use a special message
 *   to describe why travel can't be accomplished.  
 */
class NoTravelMessage: TravelMessage
    dobjFor(TravelVia)
    {
        /* as in noTravel, we need no preconditions or checks */
        preCond = []
        action()
        {
            /* simply show my message - that's all we do */
            showTravelDesc();
        }
    }

    /* 
     *   Because no travel is possible, we want a non-empty message for
     *   NPC's as well as for the PC; by default, use the same message for
     *   all actors by using travelDesc for NPC's.  
     */
    npcTravelDesc = (travelDesc)

    /* travel is not apparently possible in this direction */
    isConnectorApparent(origin, actor) { return nil; }
    isConnectorPassable(origin, traveler) { return nil; }
;

/*
 *   A "fake" connector.  This is a connector that doesn't actually allow
 *   travel, but acts like it *could*.  We simply show a special message
 *   when travel is attempted, but we don't move the actor.
 *   
 *   This is a subclass of NoTravelMessage, so instances should customize
 *   travelDes with the special failure message when travel is attempted.
 *   
 *   Note that this type of connector is by default *apparently* an exit,
 *   even though it doesn't actually go anywhere.  This is useful for "soft
 *   boundaries," where the game's map is meant to appear to the player to
 *   continue but beyond which no more locations actually exist in the map.
 *   This is an oft-used device meant to create an illusion that the game's
 *   map exists in a larger world even though the larger world is not
 *   modeled.  The message we display should in such cases attribute the
 *   actor's inability to traverse the connector to a suitable constraint
 *   within the context of the game world; for example, the actor could be
 *   said to be unwilling to go beyond this point because the actor knows
 *   or suspects there's nothing for a long way in this direction, or
 *   because the actor's goals require staying within the modeled map, or
 *   because the actor is afraid of what lies beyond.
 *   
 *   Note that FakeConnector should only be used when the reason for
 *   blocking the travel is apparent before we even try travel.  In
 *   particular, FakeConnector is good for motivational reasons not to
 *   travel, where the actor decides for reasons of its own not to even
 *   attempt the travel.  It's especially important not to use
 *   FakeConnector in cases where the travel is described as attempted but
 *   aborted halfway in - things like encountering a blocked tunnel.  This
 *   is important because FakeConnector aborts the travel immediately,
 *   before sending out any of the notifications that would accompany
 *   ordinary travel, which means that physical barriers (trolls blocking
 *   the way, being tied to a chair) that would otherwise block ordinary
 *   travel will be bypassed.  For cases where travel is attempted, but
 *   something turns you back halfway in, use DeadEndConnector.  
 */
class FakeConnector: NoTravelMessage
    /* 
     *   travel is *apparently* possible in this direction (even though
     *   it's not *actually* possible) 
     */
    isConnectorApparent(origin, actor) { return true; }
    isConnectorPassable(origin, traveler) { return nil; }
;

/* ------------------------------------------------------------------------ */
/*
 *   A Dead End Connector is a connector that appears to lead somewhere,
 *   but which turns out to be impassable for reasons that aren't apparent
 *   until we get some distance into the passage.
 *   
 *   The Dead End Connector might look a lot like the Fake Connector, but
 *   there's an important difference.  A Fake Connector is a connector that
 *   can't even be physically attempted: the reason not to take a fake
 *   connector is something that shows up before we even start moving,
 *   usually a motivational reason ("You really can't leave town until you
 *   find your missing brother").  A Dead End Connector, on the other hand,
 *   is meant to model a physical attempt to travel that's blocked by some
 *   problem halfway along, such as travel down a tunnel that turns out to
 *   have caved in.  
 */
class DeadEndConnector: TravelMessage
    /* 
     *   The apparent destination name.  If the actor is meant to know the
     *   apparent destination from the outset, or if traversing the
     *   connector gives the actor an idea of where the connector
     *   supposedly goes, this can be used to give the name of that
     *   destination.  This name will show up in exit listings, for
     *   example, once the PC knows where the connector supposedly goes.
     *   
     *   Note that this isn't the actual destination of the connector,
     *   since the actual destination is simply back to the origin (that's
     *   the whole point of the dead end, after all).  This is simply where
     *   the connector *appears* to go.  If an attempted traversal doesn't
     *   even reveal that much, then you should just leave this nil, since
     *   the destination will never become apparent to the PC.  
     */
    apparentDestName = nil

    /*
     *   Our apparent destination.  By default, we create a FakeDestination
     *   object to represent our apparent destination if we have a non-nil
     *   name for the apparent destination.
     *   
     *   If the supposed-but-unreachable destination of the connector is in
     *   fact a real location in the game, you can override this to point
     *   directly to that actual location.  This default is for the typical
     *   case where the supposed destination doesn't actually exist on the
     *   game map as a real room.  
     */
    apparentDest()
    {
        /* 
         *   if we have an apparent destination name, create a
         *   FakeDestination to represent the apparent destination, and
         *   plug that in as our apparent destination for future reference 
         */
        if (apparentDestName != nil)
        {
            local fake;
            
            /* create a fake destination */
            fake = new FakeDestination(self);

            /* plug it in as our new apparentDest for future calls */
            apparentDest = fake;

            /* return the new object as the result */
            return fake;
        }

        /* our apparent destination is unknown */
        return nil;
    }

    /* get our apparent destination */
    getApparentDestination(origin, actor)
    {
        /* 
         *   If the actor knows the destination for the given connector (as
         *   determined by the origin room), or we have a memory of this
         *   traversal, return our fake destination object to represent the
         *   destination.  We can only return our fake destination if we
         *   have an explicit apparent destination, since it's only in this
         *   case that our apparent destination ever actually becomes
         *   known, even after an attempted traversal.
         *   
         *   Our actual destination is always just the origin, but we
         *   *appear* to have some other destination.  Even though we can
         *   never actually reach that other, apparent destination, we at
         *   least want to give the appearance of going there, which we
         *   provide through our fake destination object.  
         */
        if (apparentDest != nil
            && (origin.actorKnowsDestination(actor, self)
                || travelMemory[[actor, origin, self]] != nil))
            return apparentDest;

        /* our unreachable destination is not apparent */
        return nil;
    }

    /* there's no corresponding connector back for a dead end */
    connectorBack(traveler, dest) { return nil; }

    /* our actual destination is always our origin */
    getDestination(origin, traveler) { return origin; }

    /* do remember circular trips, since that's the only kind we make */
    rememberCircularPassage = true
;

/*
 *   A fake apparent destination, for dead-end connectors.  The dead-end
 *   connector will create an object of this class to represent its
 *   apparent but actually unreachable destination, if it has an apparent
 *   destination name. 
 */
class FakeDestination: object
    /* construct - remember our associated connector */
    construct(conn) { connector = conn; }
    
    /* get our destination name - this is the name from our connector */
    getDestName(actor, origin) { return connector.apparentDestName; }

    /* our underlying connector (usually a DeadEndConnector) */
    connector = nil
;

/* ------------------------------------------------------------------------ */
/*
 *   A direct room-to-room connector.  In most cases, it is not necessary
 *   to create one of these objects, because rooms can serve as their own
 *   connectors.  These objects are needed in certain cases, though, such
 *   as when a room-to-room connection requires a travel barrier.  
 */
class RoomConnector: TravelConnector
    /* the two rooms we link */
    room1 = nil
    room2 = nil

    /* 
     *   get the destination, given the origin: this is the one of the two
     *   rooms we link that we're not in now 
     */
    getDestination(origin, traveler)
    {
        if (origin == room1)
            return room2;
        else if (origin == room2)
            return room1;
        else
            return nil;
    }

    fixedSource(origin, traveler)
    {
        /* 
         *   we're a symmetrical two-way connector, so our source from the
         *   perspective of one room is the same as the destination from
         *   that same perspective 
         */
        return getDestination(origin, traveler);
    }

    /*
     *   Get the precondition for this connector.  The normal
     *   TravelConnector rule that the traveler must be in the outbound
     *   connector's location is not meaningful for an abstract room
     *   connector, because this type of connection itself isn't
     *   represented as a physical game object with a location; it's just
     *   an abstract data structure.  This means that we must have a
     *   directional property in the *source* location that points directly
     *   to the destination (i.e., self).
     *   
     *   So, the appropriate starting point for room connectors is the
     *   object that contains the connection.  In other words, we must
     *   search for the nearest object enclosing the *traveler* that has a
     *   direction property directly linked to 'self'; that enclosing
     *   container is the required starting location for the travel.  
     */
    connectorTravelPreCond()
    {
        /* 
         *   Scan upwards from the traveler's location, looking for an
         *   object that has a directional property linked to self.  Only
         *   scan into locations that the actor can see.  
         */
        for (local loc = gActor.getTraveler(self).location ;
             loc != nil && gActor.canSee(loc) ; loc = loc.location)
        {
            /* look for a directional connector directly from 'loc' to us */
            if (loc.localDirectionLinkForConnector(self) != nil)
            {
                /* 
                 *   we're linked from this enclosing location, so this is
                 *   where we have to be before travel 
                 */
                return [new TravelerDirectlyInRoom(gActor, self, loc)];
            }
        }

        /* we couldn't find a link, so apply no condition */
        return [];
    }
;

/*
 *   A one-way room connector.  This works like an ordinary room
 *   connector, but connects only in one direction.  To use this class,
 *   simply define the 'destination' property to point to the room we
 *   connect to.
 */
class OneWayRoomConnector: RoomConnector
    /* my destination - instances must define this */
    destination = nil

    /* we always have a fixed destination */
    getDestination(origin, traveler) { return destination; }
;

/*
 *   A room "auto-connector".  This is a special subclass of RoomConnector
 *   that can be mixed in to any BasicLocation subclass to make the room
 *   usable as the direct target of a directional property in another room
 *   - so you could say "east = myRoom", for example, without creating any
 *   intermediate connector. 
 */
class RoomAutoConnector: RoomConnector
    /*   
     *   Suppose that roomA.north = roomB.  This means that if an actor is
     *   in roomA, and executes a "north" command, we'll execute a
     *   TravelVia action on room B, because the "connector" will be
     *   roomB.  In these cases, the destination of the travel and the
     *   travel connector are one and the same.  So, when the connector is
     *   roomB, the destination of travel is also simply roomB.  
     */
    getDestination(origin, traveler)
    {
        /* we are our own destination */
        return self;
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Base class for passages between rooms.  This can be used for a
 *   passage that not only connects the rooms but also exists as an object
 *   in its own right within the rooms it connects.
 *   
 *   In most cases, two passage objects will exist - one on each side of
 *   the passage, so one object in each room connected.  One of the
 *   objects should be designated as the "master"; the other is the
 *   "slave."  The master object is the one which should implement all
 *   special behavior involving state changes (such as opening or closing
 *   a door).
 *   
 *   This basic passage is not designed to be opened and closed; use Door
 *   for a passage that can be opened and closed.  
 */
class Passage: Linkable, Fixture, TravelConnector
    /*
     *   Our destination - this is where the actor ends up when traveling
     *   through the passage (assuming the passage is open).  By default,
     *   we return the "room location" of our other side's container; in
     *   cases where our other side is not directly in our destination
     *   room (for example, the other side is part of some larger object
     *   structure), this property should be overridden to specify the
     *   actual destination.
     *   
     *   If our otherSide is nil, the passage doesn't go anywhere.  This
     *   can be useful to create things that look and act like passages,
     *   but which don't actually go anywhere - in other words,
     *   passage-like decorations.  
     */
    destination = (otherSide != nil ? otherSide.location.roomLocation : nil)

    /* get my destination - just return my 'destination' property */
    getDestination(origin, traveler) { return destination; }

    /* get our open/closed status */
    isOpen()
    {
        /* 
         *   if we have a separate master object, defer to it; otherwise,
         *   use our own status 
         */
        return (masterObject == self ? isOpen_ : masterObject.isOpen);
    }

    /* internal open/closed status - open by default */
    isOpen_ = true

    /* 
     *   We're not visible in the dark if we're closed.  If we're open,
     *   the normal rules apply.
     *   
     *   Normally, a passage is visible in the dark if there's light in
     *   the adjoining location: our model is that enough light is leaking
     *   in through the passage to make the passage itself visible, but
     *   not enough to light anything else in the current room.  In the
     *   case of a closed passage, though, we assume that it completely
     *   blocks any light from the other room, eliminating any indication
     *   of a passage.
     *   
     *   If you do want an openable passage to be visible in the dark even
     *   when it's closed, it's probably better to make the passage
     *   self-illuminating (i.e., with brightness 1), because this will
     *   put the passage in scope and thus allow it to be manipulated.  
     */
    isConnectorVisibleInDark(origin, actor)
        { return isOpen() && inherited(origin, actor); }

    /* a passage is passable when it's open */
    isConnectorPassable(origin, traveler) { return isOpen(); }

    /*
     *   Initialize.  If we're a slave, we'll set up the otherSide
     *   relationship between this passage and our master passage.  
     */
    initializeThing()
    {
        /* inherit default handling */
        inherited();

        /* 
         *   if we have a master side, initialize our relationship with
         *   the master side
         */
        if (masterObject != self)
        {
            /* set our otherSide to point to the master */
            otherSide = masterObject;

            /* set the master's otherSide to point to us */
            masterObject.initMasterObject(self);
        }
    }

    /* 
     *   Initialize the master object.  The other side of a two-sided door
     *   will call this on the master object to let the master object know
     *   about the other side.  'other' is the other-side object.  By
     *   default, we'll simply remember the other object in our own
     *   'otherSide' property.  
     */
    initMasterObject(other) { otherSide = other; }
    
    /*
     *   Our corresponding passage object on the other side of the
     *   passage.  This will be set automatically during initialization
     *   based on the masterObject property of the slave - it is not
     *   generally necessary to set this manually.  
     */
    otherSide = nil

    /* our other side is the other facet of the passage */
    getFacets() { return otherSide != nil ? [otherSide] : inherited(); }

    /* 
     *   our source is always our destination, since we have a one-to-one
     *   relationship with our comlementary passage in the destination
     *   location (we point to it, it points to us) 
     */
    fixedSource(origin, traveler) { return destination; }

    /* the connector back is our complementary side, if we have one */
    connectorBack(traveler, dest)
    {
        /* if we have a complementary side, it's the connector back */
        if (otherSide != nil)
            return otherSide;

        /* we don't have a complementary side, so use the default handling */
        return inherited(traveler, dest);
    }

    /* can the given actor travel through this passage? */
    canActorTravel(actor)
    {
        /* 
         *   by default, the actor can travel through the passage if and
         *   only if the passage is open 
         */
        return isOpen;
    }

    /*
     *   Display our message when we don't allow the actor to travel
     *   through the passage because the passage is closed.  By default,
     *   we'll simply display the default cannotTravel message for the
     *   actor's location, but this can be overridden to provide a more
     *   specific report of the problem.  
     */
    cannotTravel()
    {
        /* use the actor's location's cannotTravel handling */
        gActor.location.cannotTravel();
    }

    /* carry out travel via this connector */
    dobjFor(TravelVia)
    {
        /* check travel */
        check()
        {
            /* 
             *   Move the actor only if we're open; if we're not, use the
             *   standard no-travel handling for the actor's location.
             *   Note that we don't try to implicitly open the passage,
             *   because the basic passage is not openable; if we're
             *   closed, it means we're impassable for some reason that
             *   presumably cannot be remedied by a simple "open" command.
             */
            if (!canActorTravel(gActor))
            {
                /* we're closed, so use our no-travel handling */
                cannotTravel();
                exit;
            }
            else
            {
                /* inherit the default checks */
                inherited();
            }
        }
    }

    dobjFor(LookThrough)
    {
        action()
        {
            /* 
             *   if we're open, we simply "can't see much from here";
             *   otherwise, we can't see through it at all 
             */
            if (isOpen)
                mainReport(&nothingThroughPassageMsg);
            else
                inherited();
        }
    }
;

/*
 *   A passage that an actor can travel through (with a "go through" or
 *   "enter" command).  A "go through" command applied to the passage
 *   simply makes the actor travel through the passage as though using the
 *   appropriate directional command.
 *   
 *   We describe actors arriving or departing via the passage using
 *   "through" descriptions ("Bob arrives through the door," etc).  
 */
class ThroughPassage: Passage
    describeDeparture(traveler, origin, dest)
    {
        /* describe the traveler departing through this passage */
        traveler.sayDepartingThroughPassage(self);
    }

    describeArrival(traveler, origin, dest)
    {
        /* describe the traveler arriving through this passage */
        traveler.sayArrivingThroughPassage(self);
    }

    /* treat "go through self" as travel through the passage */
    dobjFor(GoThrough) remapTo(TravelVia, self)

    /* "enter" is the same as "go through" for this type of passage */
    dobjFor(Enter) asDobjFor(GoThrough)

    /* 
     *   Explicitly map the indirect object 'verify' handlers for the
     *   push-travel commands corresponding to the regular travel commands
     *   we provide.  This isn't strictly necessary except when we're mixed
     *   in to something like a Decoration, in which case explicitly
     *   defining the mapping here is important because it will give the
     *   mapping higher precedence than a catch-all handlers overriding the
     *   same mapping we inherit from Thing.  We use the same mapping that
     *   Thing provides by default.  
     */
    mapPushTravelIobj(PushTravelThrough, TravelVia)
    mapPushTravelIobj(PushTravelEnter, TravelVia)
;

/*
 *   A Path Passage is a specialization of through passage that's more
 *   suitable for outdoor locations.  This type of passage is good for
 *   things like outdoor walkways, paths, and streets, where travelers walk
 *   along the connector but aren't enclosed by it.  The main practical
 *   difference is how we describe departures and arrivals; in English, we
 *   describe these as being "via" the path rather than "through" the path,
 *   since there's no enclosure involved in these connections.  
 */
class PathPassage: ThroughPassage
    describeDeparture(traveler, origin, dest)
    {
        traveler.sayDepartingViaPath(self);
    }

    describeArrival(traveler, origin, dest)
    {
        /* describe the traveler arriving through this passage */
        traveler.sayArrivingViaPath(self);
    }

    /* putting something on a path is the same as dropping it */
    iobjFor(PutOn) remapTo(Drop, DirectObject)

    /* use a special message for standing on a path */
    cannotStandOnMsg = &cannotStandOnPathMsg

    /* FOLLOW PATH -> travel via the path */
    dobjFor(Follow) remapTo(TravelVia, DirectObject)
;


/* ------------------------------------------------------------------------ */
/*
 *   The exit portal of a one-way passage.  This isn't a fully functional
 *   passage, but rather an object that acts as the receiving end of a
 *   passage that can only be traversed in one direction.
 *   
 *   This can be used for various purposes: the underside of a trap door,
 *   the bottom of a chute, the exit of a wormhole.  
 */
class ExitOnlyPassage: ThroughPassage
    dobjFor(TravelVia)
    {
        action()
        {
            /* 
             *   Show our default failure message.  This can be overridden
             *   to provide a more customized description of why the
             *   passage cannot be entered from this side.  
             */
            reportFailure(&cannotEnterExitOnlyMsg, self);
        }
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Stairway - this is a special kind of passage that is used for
 *   vertical connections, such as stairways and ladders.  This type of
 *   passage doesn't allow "enter" or "go through," but does allow "climb".
 *   
 *   The basic Stairway should generally not be used, as it doesn't know
 *   where it is relative to connected stairs.  Instead, the more specific
 *   StairwayUp and StairwayDown should be used in most cases.
 *   
 *   Note that at midpoints along long stairways (landings, for example),
 *   separate stairway objects should normally be used: one going up and
 *   one going down.  
 */
class Stairway: Passage
    /*
     *   Treat "climb self" as travel through the passage 
     */
    dobjFor(Climb) remapTo(TravelVia, self)
;

/*
 *   A stairway going up from here. 
 */
class StairwayUp: Stairway
    describeArrival(traveler, origin, dest)
    {
        /* 
         *   describe the actor arriving by coming down these stairs (they
         *   leave up, so they arrive down) 
         */
        traveler.sayArrivingDownStairs(self);
    }

    describeDeparture(traveler, origin, dest)
    {
        /* describe the actor leaving up these stairs */
        traveler.sayDepartingUpStairs(self);
    }

    /* "climb up" is the specific direction for "climb" here */
    dobjFor(ClimbUp) asDobjFor(Climb)

    /* cannot climb down from here */
    dobjFor(ClimbDown)
    {
        verify() { illogical(&stairwayNotDownMsg); }
    }
;

/*
 *   A stairway going down from here. 
 */
class StairwayDown: Stairway
    /* "climb down" is the specific direction for "climb" here */
    dobjFor(ClimbDown) asDobjFor(Climb)

    describeArrival(traveler, origin, dest)
    {
        /* 
         *   describe the actor arriving by coming up these stairs (they
         *   leave down, so they arrive up) 
         */
        traveler.sayArrivingUpStairs(self);
    }

    describeDeparture(traveler, origin, dest)
    {
        /* describe the actor traveling down these stairs */
        traveler.sayDepartingDownStairs(self);
    }

    /* cannot climb up from here */
    dobjFor(ClimbUp)
    {
        verify() { illogical(&stairwayNotUpMsg); }
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Basic Door class.  This is the base class for door-like objects: a
 *   travel connector with two sides, and which can be opened and closed.
 *   Each side of the door is linked to the other, so that opening or
 *   closing one side makes the same change to the other side.  
 *   
 *   A basic door has the internal functionality of a door, but doesn't
 *   provide handling for player commands that manipulate the open/closed
 *   status.  
 */
class BasicDoor: BasicOpenable, ThroughPassage
    /* 
     *   Open/close the door.  If we have a complementary door object
     *   representing the other side, we'll remark in the sensory context
     *   of its location that it's also opening or closing. 
     */
    makeOpen(stat)
    {
        /* inherit the default behavior */
        inherited(stat);

        /* 
         *   if our new status is in effect, notify the other side so that
         *   it can generate a message in its location 
         */
        if (isOpen == stat && otherSide != nil)
            otherSide.noteRemoteOpen(stat);
    }

    /*
     *   Note a "remote" change to our open/close status.  This is an
     *   open/close operation performed on our complementary object
     *   representing the other side of the door.  We'll remark on the
     *   change in the sensory context of this side, but only if we're
     *   suppressing output in the current context - if we're not, then
     *   the player will see the message generated by the side that we
     *   directly acted upon, so we don't need a separate report for the
     *   other side.  
     */
    noteRemoteOpen(stat)
    {
        /* 
         *   If I'm not visible to the player character in the current
         *   sense context, where the action is actually taking place,
         *   switch to my own sensory context and display a report.  This
         *   way, if the player can see this door but not the other side,
         *   and the action is taking place on the other side, we'll still
         *   see a note about the change.  We only need to do this if
         *   we're not already visible to the player, because if we are,
         *   we'll generate an ordinary report of the door opening in the
         *   action's context.  
         */
        if (senseContext.isBlocking)
        {
            /* show a message in my own sensory context */
            callWithSenseContext(self, sight, {: describeRemoteOpen(stat) });
        }
    }

    /* 
     *   Describe the door being opened remotely (that is, by someone on
     *   the other side).  This is called from noteRemoteOpen to actually
     *   display the message.  
     */
    describeRemoteOpen(stat)
    {
        /* show the default library message for opening the door remotely */
        gLibMessages.sayOpenDoorRemotely(self, stat);
    }

    /* carry out travel through the door */
    dobjFor(TravelVia)
    {
        action()
        {
            /* move the actor to our destination */
            inherited();

            /* remember that this is the last door the actor traversed */
            gActor.rememberLastDoor(self);
        }
    }

    /*
     *   Boost the likelihood that a command is referring to us if we just
     *   traveled through the door; this is meant to be called from
     *   verify() routines.  For a few commands (close, lock), the most
     *   likely door being referred to is the door just traversed.  
     */
    boostLikelihoodOnTravel()
    {
        /* 
         *   if this (or our other side) is the last door the actor
         *   traversed, boost the likelihood that they're referring to us 
         */
        if (gActor.lastDoorTraversed == self
            || (otherSide != nil && gActor.lastDoorTraversed == otherSide))
            logicalRank(120, 'last door traversed');
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Door.  This is a travel connector that can be opened and closed with
 *   player commands.  This is a simple subclass of BasicDoor that adds
 *   support for player commands to manipulate the door.  
 */
class Door: Openable, BasicDoor
    /* make us initially closed */
    initiallyOpen = nil

    /* if we can't travel because the door is closed, say so */
    cannotTravel()
    {
        if (gActor.canSee(self) && !isOpen)
            reportFailure(&cannotGoThroughClosedDoorMsg, self);
        else
            inherited();
    }

    /* 
     *   get the 'door open' precondition - by default, we create a
     *   standard doorOpen precondition for this object, but this can be
     *   overridden if desired to create custom doorOpen variations 
     */
    getDoorOpenPreCond() { return new ObjectPreCondition(self, doorOpen); }

    /* the door must be open before we can travel this way */
    connectorTravelPreCond()
    {
        local ret;
        local doorCond;

        /* start with the inherited conditions */
        ret = inherited();

        /* if there's a door-open condition, add it as well */
        if ((doorCond = getDoorOpenPreCond()) != nil)
            ret += doorCond;

        /* return the result */
        return ret;
    }

    dobjFor(Close)
    {
        verify()
        {
            /* inherit default handling */
            inherited();

            /* boost the likelihood if they just traveled through us */
            boostLikelihoodOnTravel();
        }
    }

    dobjFor(Lock)
    {
        verify()
        {
            /* inherit default handling */
            inherited();

            /* boost the likelihood if they just traveled through us */
            boostLikelihoodOnTravel();
        }
    }

    /* 
     *   looking behind a door implies opening it to see what's on the
     *   other side 
     */
    dobjFor(LookBehind)
    {
        preCond = (inherited + objOpen)
        action()
        {
            /* 
             *   If the door is open, AND we implicitly opened the door to
             *   carry out this action, use a special report that takes
             *   into account that we were specifically looking to see what
             *   was on the other side of the door; otherwise, use the
             *   default behavior 
             */
            if (isOpen
                && gTranscript.currentActionHasReport(
                    {x: (x.action_.ofKind(OpenAction)
                         && x.isActionImplicit())}))
                mainReport(&nothingBeyondDoorMsg);
            else
                inherited();
        }
    }

    /* looking through a door requires it to be open */
    dobjFor(LookThrough)
    {
        preCond = (nilToList(inherited) + objOpen)
    }

;

/* ------------------------------------------------------------------------ */
/*
 *   Secret Door.  This is a special kind of door that gives no hint of
 *   its being a door when it's closed.  This can be used for objects that
 *   serve as a secret passage but are otherwise normal objects, such as a
 *   bookcase that conceals a passage.  
 */
class SecretDoor: BasicDoor
    /* a secret passage usually starts off secret (i.e., closed) */
    initiallyOpen = nil

    isConnectorApparent(origin, actor)
    {
        /* 
         *   A secret passage is not apparent as a pasage unless it's
         *   open. 
         */
        if (isOpen)
        {
            /* the passage is open - use the inherited handling */
            return inherited(origin, actor);
        }
        else
        {
            /* 
             *   the passage is closed, so it's not apparently a passage
             *   at all 
             */
            return nil;
        }
    }
;

/*
 *   Hidden Door.  This is a secret door that is invisible when closed.
 *   This can be used when the passage isn't even visible when it's
 *   closed, such as a door that seamlessly melds into the wall. 
 */
class HiddenDoor: SecretDoor
    /*
     *   When we're closed, we're completely invisible, so we have no
     *   sight presence.  When we're open, we have our normal visual
     *   presence.  
     */
    sightPresence = (isOpen)
;

/* ------------------------------------------------------------------------ */
/*
 *   Automatically closing door.  This type of door closes behind an actor
 *   as the actor traverses the connector. 
 */
class AutoClosingDoor: Door
    dobjFor(TravelVia)
    {
        action()
        {
            /* inherit the default handling */
            inherited();

            /* 
             *   Only close the door if the actor performing the travel
             *   isn't accompanying another actor.  If the actor is
             *   accompanying someone, we essentially want them to hold the
             *   door for the other actor - at the very least, we don't
             *   want to slam it in the other actor's face! 
             */
            if (!gActor.curState.ofKind(AccompanyingInTravelState))
            {
                /* close the door */
                makeOpen(nil);

                /* mention that the automatic closing */
                reportAutoClose();
            }
        }
    }

    /* 
     *   Report the automatic closure.  The TravelVia action() calls this
     *   after closing the door to generate a message mentioning that the
     *   door was closed.  By default, we just show the standard
     *   doorClosesBehindMsg library message.  
     */
    reportAutoClose() { mainReport(&doorClosesBehindMsg, self); }
;

/* ------------------------------------------------------------------------ */
/*
 *   The base class for Enterables and Exitables.  These are physical
 *   objects associated with travel connectors.  For example, the object
 *   representing the exterior of a building in the location containing
 *   the building could be an Enterable, so that typing ENTER BUILDING
 *   takes us into the building via the travel connector that leads inside.
 *   
 *   Enterables and Exitables are physical covers for travel connectors.
 *   These objects aren't travel connectors themselves, and they don't
 *   specify the destination; instead, these just point to travel
 *   connectors.  
 */
class TravelConnectorLink: object
    /* the underlying travel connector */
    connector = nil

    /* 
     *   The internal "TravelVia" action just maps to travel via the
     *   underlying connector.  However, we want to apply our own
     *   preconditions, so we don't directly remap to the underlying
     *   connector.  Instead, we provide our own full TravelVia
     *   implementation, and then we perform the travel on the underlying
     *   connector via a replacement action in our own action() handler.  
     */
    dobjFor(TravelVia)
    {
        /* the physical link object has to be touchable */
        preCond = [touchObj]
        
        verify() { }
        action()
        {
            /* carry out the action by traveling via our connector */
            replaceAction(TravelVia, connector);
        }
    }

    /*
     *   These objects are generally things like buildings (exterior or
     *   interior), which tend to be large enough that their details can be
     *   seen at a distance.  
     */
    sightSize = large
;

/*
 *   An Enterable is an object that exists in one location, and which can
 *   be entered to take an actor to another location.  Enterables are used
 *   for things such as the outsides of buildings, so that the building
 *   can have a presence on its outside and can be entered via a command
 *   like "go into building".
 *   
 *   An Enterable isn't a connector, but points to a connector.  This type
 *   of object is most useful when there's already a connector that exists
 *   as a separate object, such as the door to a house: the house object
 *   can be made an Enterable that simply points to the door object.  
 */
class Enterable: TravelConnectorLink, Fixture
    /*
     *   "Enter" action this simply causes the actor to travel via the
     *   connector.  
     */
    dobjFor(Enter) remapTo(TravelVia, self)

    /* explicitly define the push-travel indirect object mapping */
    mapPushTravelIobj(PushTravelEnter, TravelVia)
;

/*
 *   An Exitable is like an Enterable, except that you exit it rather than
 *   enter it.  This can be used for objects representing the current
 *   location as an enclosure (a jail cell), or an exit door. 
 */
class Exitable: TravelConnectorLink, Fixture
    /* Get Out Of/Exit action - this simply maps to travel via the connector */
    dobjFor(GetOutOf) remapTo(TravelVia, self)

    /* explicitly define the push-travel indirect object mapping */
    mapPushTravelIobj(PushTravelGetOutOf, TravelVia)
;

/*
 *   An EntryPortal is just like an Enterable, except that "go through"
 *   also works on it.  Likewise, an ExitPortal is just like an Exitable
 *   but accepts "go through" as well.  
 */
class EntryPortal: Enterable
    dobjFor(GoThrough) remapTo(TravelVia, self)
;
class ExitPortal: Exitable
    dobjFor(GoThrough) remapTo(TravelVia, self)
;

/* ------------------------------------------------------------------------ */
/*
 *   A "TravelPushable" is an object that can't be taken, but can be moved
 *   from one location to another via commands of the form "push obj dir,"
 *   "push obj through passage," and the like.
 *   
 *   TravelPushables tend to be rather rare, and we expect that instances
 *   will almost always be special cases that require additional
 *   specialized code.  This is therefore only a general framework for
 *   pushability.  
 */
class TravelPushable: Immovable
    cannotTakeMsg = &cannotTakePushableMsg
    cannotMoveMsg = &cannotMovePushableMsg
    cannotPutMsg = &cannotPutPushableMsg

    /* can we be pushed via the given travel connector? */
    canPushTravelVia(connector, dest) { return true; }

    /* explain why canPushTravelVia said we can't be pushed this way */
    explainNoPushTravelVia(connector, dest) { }

    /*
     *   Receive notification that we're about to be pushed somewhere.
     *   This is called just before the underlying traveler performs the
     *   actual travel.  (By default, we don't even define this method, to
     *   ensure that if we're combined with another base class that
     *   overrides the method, the overrider will be called.)  
     */
    // beforeMovePushable(traveler, connector, dest) { }

    /*
     *   Move the object to a new location as part of a push-travel
     *   operation.  By default, this simply uses moveInto to move the
     *   object, but subclasses can override this to apply conditions to
     *   the pushing, put the object somewhere else in some cases, display
     *   extra messages, or do anything else that's necessary.
     *   
     *   Our special PushTraveler calls this routine after the underlying
     *   real traveler has finished its travel to the new location, so the
     *   traveler's location will indicate the destination of the travel.
     *   Note that this routine is never called if the traveler ends up in
     *   its original location after the travel, so this routine isn't
     *   called when travel isn't allowed for the underlying traveler.  
     */
    movePushable(traveler, connector)
    {
        /* move me to the traveler's new location */
        moveIntoForTravel(traveler.location);

        /* describe what we're doing */
        describeMovePushable(traveler, connector);
    }

    /*
     *   Describe the actor pushing the object into the new location.
     *   This is called from movePushable; we pull this out as a separate
     *   routine so that the description of the pushing can be overridden
     *   without having to override all of movePushable.  
     */
    describeMovePushable(traveler, connector)
    {
        /* 
         *   If the actor is the player character, mention that we're
         *   pushing the object with us.  For an NPC, show nothing,
         *   because the normal travel message will mention that the NPC
         *   is pushing the object as part of the normal travel report
         *   (thanks to PushTraveler's name override). 
         */
        if (gActor.isPlayerChar)
            mainReport(&okayPushTravelMsg, self);
    }

    dobjFor(PushTravel)
    {
        verify() { }
        action()
        {
            local newTrav;
            local oldTrav;
            local wasExcluded;
            
            /*
             *   Create a push traveler to coordinate the travel.  The
             *   push traveler performs the normal travel, and also moves
             *   the object being pushed.  
             */
            newTrav = pushTravelerClass.createInstance(
                self, gActor.getPushTraveler(self));
            
            /* set the actor's special traveler to our push traveler */
            oldTrav = gActor.setSpecialTraveler(newTrav);

            /* 
             *   Add myself to the actor's look-around exclusion list -
             *   this ensures that we won't be listed among the objects in
             *   the new location when we show the description on arrival.
             *   This is desirable because of how we describe the travel:
             *   we first want to show the new location's description, then
             *   describe pushing this object into the new location.  But
             *   we actually have to move the object first, to make sure
             *   any side effects of its presence in the new location are
             *   taken into account when we describe the location - because
             *   by the time we're in the room enough to have a look
             *   around, the pushable will be in the room with us.  Even
             *   so, we don't actually want to describe the pushable as in
             *   the room at this stage, because from the player's
             *   perspective we're really still in the process of pushing
             *   it; describing it as already settled into the new location
             *   sounds weird because it makes it seem like the object was
             *   already in place when we arrived.  To avoid this
             *   weirdness, suppress it from the new location's initial
             *   description on arrival.  
             */
            wasExcluded = gActor.excludeFromLookAround(self);

            /* make sure we undo our global changes when we're done */
            try
            {
                /* 
                 *   Now that we've activated our special push traveler
                 *   for the actor, simply perform the ordinary travel
                 *   command.  The travel command we perform depends on
                 *   the kind of push-travel command we attempted, so
                 *   simply let the current action carry out the action.
                 */
                gAction.performTravel();
            }
            finally
            {
                /* restore the actor's old special traveler on the way out */
                gActor.setSpecialTraveler(oldTrav);

                /* 
                 *   if we weren't in the actor's 'look around' exclusion
                 *   list before, remove us from the list
                 */
                if (!wasExcluded)
                    gActor.unexcludeFromLookAround(self);
            }
        }
    }

    /* 
     *   The class we create for our special push traveler - by default,
     *   this is PushTraveler, but we parameterize this via this property
     *   to allow special PushTraveler subclasses to be created; this
     *   could be useful, for example, to customize the traveler name
     *   messages. 
     */
    pushTravelerClass = PushTraveler
;

/*
 *   A special Traveler class for travel involving pushing an object from
 *   one room to another.  This class encapsulates the object being pushed
 *   and the actual Traveler performing the travel.
 *   
 *   For the most part, we refer Traveler methods to the underlying
 *   Traveler.  We override a few methods to provide special handling.  
 */
class PushTraveler: object
    construct(obj, traveler)
    {
        /* remember the object being pushed and the real traveler */
        obj_ = obj;
        traveler_ = traveler;
    }

    /* the object being pushed */
    obj_ = nil

    /* 
     *   the underlying Traveler - this is the real Traveler that will
     *   move to a new location 
     */
    traveler_ = nil

    /*
     *   Travel to a new location.  We'll run the normal travel routine
     *   for the underlying real traveler; then, if we ended up in a new
     *   location, we'll move the object being pushed to the traveler's
     *   new location.  
     */
    travelerTravelTo(dest, connector, backConnector)
    {
        local oldLoc;
        local origin;
        
        /* remember the traveler's origin, so we can tell if we moved */
        origin = traveler_.location;

        /* let the object being pushed describe the departure */
        obj_.beforeMovePushable(traveler_, connector, dest);

        /* 
         *   *Tentatively* move the pushable to its new location.  Just do
         *   the basic move-into for this, since it's only tentative - this
         *   is just so that we get any side effects of its presence in the
         *   new location for the purposes of describing the new location.
         */
        oldLoc = location;
        obj_.baseMoveInto(dest);

        /* 
         *   Call the traveler's normal travel routine, to perform the
         *   actual movement of the underlying traveler.  
         */
        traveler_.travelerTravelTo(dest, connector, backConnector);

        /* undo the tentative move of the pushable */
        obj_.baseMoveInto(oldLoc);

        /* 
         *   If we moved to a new location, we can now actually move the
         *   pushable object to the new location.  
         */
        if (traveler_.location != origin)
            obj_.movePushable(traveler_, connector);
    }

    /*
     *   Perform local travel, between nested rooms within a top-level
     *   location.  By default, we simply don't allow pushing objects
     *   between nested rooms.
     *   
     *   To allow pushing an object between nested rooms, override this in
     *   parallel with travelerTravelTo().  Note that you'll have to call
     *   travelerTravelWithin() on the underlying traveler (which will
     *   generally be the actor), and you'll probably want to set up a new
     *   set of notifiers parallel to beforeMovePushable() and
     *   movePushable().  You'll probably particularly need to customize
     *   the report in your parallel for movePushable() - the default ("you
     *   push x into the area") isn't very good when nested rooms are
     *   involved, and you'll probably want something more specific.  
     */
    travelerTravelWithin(actor, dest)
    {
        reportFailure(&cannotPushObjectNestedMsg, obj_);
        exit;
    }

    /*
     *   Can we travel via the given connector?  We'll ask our underlying
     *   traveler first, and if that succeeds, we'll ask the object we're
     *   pushing. 
     */
    canTravelVia(connector, dest)
    {
        /* ask the underlying traveler first, then our pushed object */
        return (traveler_.canTravelVia(connector, dest)
                && obj_.canPushTravelVia(connector, dest));
    }

    /*
     *   Explain why the given travel is not possible.  If our underlying
     *   traveler raised the objection, let it explain; otherwise, let our
     *   pushed object explain. 
     */
    explainNoTravelVia(connector, dest)
    {
        if (!traveler_.canTravelVia(connector, dest))
            traveler_.explainNoTravelVia(connector, dest);
        else
            obj_.explainNoPushTravelVia(connector, dest);
    }

    /* by default, send everything to the underlying Traveler */
    propNotDefined(prop, [args]) { return traveler_.(prop)(args...); }
;

/*
 *   A PushTravelBarrier is a TravelConnector that allows regular travel,
 *   but not travel that involves pushing something.  By default, we block
 *   all push travel, but subclasses can customize this so that we block
 *   only specific objects.  
 */
class PushTravelBarrier: TravelBarrier
    /*
     *   Determine if the given pushed object is allowed to pass.  Returns
     *   true if so, nil if not.  By default, we'll return nil for every
     *   object; subclasses can override this to allow some objects to be
     *   pushed through the barrier but not others. 
     */
    canPushedObjectPass(obj) { return nil; }

    /* explain why an object can't pass */
    explainTravelBarrier(traveler)
    {
        reportFailure(&cannotPushObjectThatWayMsg, traveler.obj_);
    }

    /*
     *   Determine if the given traveler can pass through this connector.
     *   If the traveler isn't a push traveler, we'll allow the travel;
     *   otherwise, we'll block the travel if our canPushedObjectPass
     *   routine says the object being pushed can pass. 
     */
    canTravelerPass(traveler)
    {
        /* if it's not a push traveler, it can pass */
        if (!traveler.ofKind(PushTraveler))
            return true;

        /* it can pass if we can pass the object being pushed */
        return canPushedObjectPass(traveler.obj_);
    }
;


/* ------------------------------------------------------------------------ */
/*
 *   A basic location - this is the base class for locations that can
 *   contain actors.
 */
class BasicLocation: Thing
    /*
     *   Get the nested room list grouper for an actor in the given
     *   posture directly in this room.  This is used when we're listing
     *   the actors within the nested room as 
     *   
     *   By default, we maintain a lookup table, and store one nested
     *   actor grouper object for each posture.  This makes it so that we
     *   show one group per posture in this room; for example, if we
     *   contain two sitting actors and three standing actors, we'll say
     *   something like "bill and bob are sitting on the stage, and jill,
     *   jane, and jack are standing on the stage."  This can be
     *   overridden if a different arrangement of groups is desired; for
     *   example, an override could simply return a single grouper to list
     *   everyone in the room together, regardless of posture.  
     */
    listWithActorIn(posture)
    {
        /* if we don't have a lookup table for this yet, create one */
        if (listWithActorInTable == nil)
            listWithActorInTable = new LookupTable(5, 5);

        /* if this posture isn't in the table yet, create a grouper for it */
        if (listWithActorInTable[posture] == nil)
            listWithActorInTable[posture] =
                new RoomActorGrouper(self, posture);

        /* return the grouper for this posture */
        return listWithActorInTable[posture];
    }

    /* 
     *   our listWithActorIn table - this gets initialized to a
     *   LookupTable as soon as we need one (in listWithActorIn) 
     */
    listWithActorInTable = nil

    /*
     *   Check the ambient illumination level in the room for the given
     *   actor's senses to determine if the actor would be able to see if
     *   the actor were in this room without any additional light sources.
     *   Returns true if the room is lit for the purposes of the actor's
     *   visual senses, nil if not.
     *   
     *   Note that if the actor is already in this location, there's no
     *   need to run this test, since we could get the information from
     *   the actor directly.  The point of this test is to determine the
     *   light level in this location without the actor having to be
     *   present.  
     */
    wouldBeLitFor(actor)
    {
        /*
         *   Check for a simple, common case before running the more
         *   expensive full what-if test.  Most rooms provide their own
         *   illumination in the actor's 'sight' sense; if this room does
         *   provide its own interior 'brightness' of at least 2, and the
         *   actor has 'sight' among its visual senses, then the room is
         *   lit.  Note that we must use transSensingOut() to determine
         *   how we transmit our own brightness to our interior, since
         *   that gives the transparency for looking from within this room
         *   at objects outside the room; our own intrinsic brightness is
         *   defined as the brightness of our exterior surface.  
         */
        if (actor.sightlikeSenses.indexOf(sight) != nil
            && brightness > 1
            && transSensingOut(sight) == transparent)
            return true;

        /*
         *   We can't determine for sure that the location is lit with a
         *   simple test, so run a full what-if test, checking what
         *   ambient light level our "probe" object would see if it were
         *   moved to this location.  Return true if the ambient light
         *   level is higher than the "self-lit" value of 1, nil if not.  
         */
        return (lightProbe.whatIf(
            {: senseAmbientMax(actor.sightlikeSenses) },
            &moveInto, self) > 1);
    }

    /*
     *   Show our room description: this is the interior description of
     *   the room, for use when the room is viewed by an actor within the
     *   room.  By default, we show our ordinary 'desc'.  
     */
    roomDesc { desc; }

    /* as part of a room description, mention an actor in this room */
    roomActorHereDesc(actor) { gLibMessages.actorInRoom(actor, self); }

    /*
     *   Provide a default description for an actor in this location, as
     *   seen from a remote location (i.e., from a separate top-level room
     *   that's linked to our top-level room by a sense connector of some
     *   kind).  By default, we'll describe the actor as being in this
     *   nested room.  
     */
    roomActorThereDesc(actor)
    {
        local pov = getPOV();
        local outer;
        
        /* get the outermost visible enclosing room */
        outer = getOutermostVisibleRoom(pov);

        /* 
         *   If we found a room, and it's not us (i.e., we found something
         *   outside this room that can be seen from the current point of
         *   view), use the three-part description: actor in nested room
         *   (self) in outer room.  If we didn't find a room, or it's the
         *   same as us, all we can say is that the actor is in the nested
         *   room. 
         */
        if (outer not in (nil, self))
        {
            /* use the three-part description: actor in self in outer */
            gLibMessages.actorInRemoteNestedRoom(actor, self, outer, pov);
        }
        else
        {
            /* we're as far out as we can see: just say actor is in self */
            gLibMessages.actorInRemoteRoom(actor, self, pov);
        }
    }

    /* show the status addendum for an actor in this location */
    roomActorStatus(actor) { gLibMessages.actorInRoomStatus(actor, self); }

    /* describe the actor's posture while in this location */
    roomActorPostureDesc(actor)
        { gLibMessages.actorInRoomPosture(actor, self); }

    /* acknowledge a posture change while in this location */
    roomOkayPostureChange(actor)
        { defaultReport(&roomOkayPostureChangeMsg, actor.posture, self); }

    /* 
     *   describe the actor's posture as part of the EXAMINE description of
     *   the nested room 
     */
    roomListActorPosture(actor)
        { gLibMessages.actorInRoom(actor, self); }
    
    /*
     *   Prefix and suffix messages for listing a group of actors
     *   nominally in this location.  'posture' is the posture of the
     *   actors.  'remote' is the outermost visible room containing the
     *   actors, but only if that room is remote from the point-of-view
     *   actor; if everything is local, this will be nil.  'lst' is the
     *   list of actors being listed.  By default, we'll just show the
     *   standard library messages.  
     */
    actorInGroupPrefix(pov, posture, remote, lst)
    {
        if (remote == nil)
            gLibMessages.actorInGroupPrefix(posture, self, lst);
        else
            gLibMessages.actorInRemoteGroupPrefix(
                pov, posture, self, remote, lst);
    }
    actorInGroupSuffix(pov, posture, remote, lst)
    {
        if (remote == nil)
            gLibMessages.actorInGroupSuffix(posture, self, lst);
        else
            gLibMessages.actorInRemoteGroupSuffix(
                pov, posture, self, remote, lst);
    }

    /*
     *   Show a list of exits from this room as part of failed travel
     *   ("you can't go that way"). 
     */
    cannotGoShowExits(actor)
    {
        /* if we have an exit lister, ask it to show exits */
        if (gExitLister != nil)
            gExitLister.cannotGoShowExits(actor, self);
    }

    /* show the exit list in the status line */
    showStatuslineExits()
    {
        /* if we have a global exit lister, ask it to show the exits */
        if (gExitLister != nil)
            gExitLister.showStatuslineExits();
    }

    /* 
     *   Get the estimated height, in lines of text, of the exits display's
     *   contribution to the status line.  This is used to calculate the
     *   extra height we need in the status line, if any, to display the
     *   exit list.  If we're not configured to display exits in the status
     *   line, this should return zero. 
     */
    getStatuslineExitsHeight()
    {
        if (gExitLister != nil)
            return gExitLister.getStatuslineExitsHeight();
        else
            return 0;
    }

    /*
     *   Make the actor stand up from this location.  By default, we'll
     *   simply change the actor's posture to "standing," and show a
     *   default success report.
     *   
     *   Subclasses might need to override this.  For example, a chair
     *   will set the actor's location to the room containing the chair
     *   when the actor stands up from the chair.  
     */
    makeStandingUp()
    {
        /* simply set the actor's new posture */
        gActor.makePosture(standing);

        /* issue a default report of the change */
        defaultReport(&okayPostureChangeMsg, standing);
    }

    /* 
     *   Default posture for an actor in the location.  This is the
     *   posture assumed by an actor when moving out of a nested room
     *   within this location. 
     */
    defaultPosture = standing

    /* failure report we issue when we can't return to default posture */
    mustDefaultPostureProp = &mustBeStandingMsg

    /* run the appropriate implied command to achieve our default posture */
    tryMakingDefaultPosture()
    {
        return defaultPosture.tryMakingPosture(self);
    }

    /*
     *   Check this object as a staging location.  We're a valid location,
     *   so we allow this. 
     */
    checkStagingLocation(dest)
    {
        /* we've valid, so we don't need to do anything */
    }

    /*
     *   Try moving the actor into this location.
     */
    checkMovingActorInto(allowImplicit)
    {
        /* 
         *   If the actor isn't somewhere within us, we can't move the
         *   actor here implicitly - we have no generic way of causing
         *   implicit travel between top-level locations.  Note that some
         *   rooms might want to override this when travel between
         *   adjacent locations makes sense as an implicit action; we
         *   expect that such cases will be rare, so we don't attempt to
         *   generalize this possibility.  
         */
        if (!gActor.isIn(self))
        {
            reportFailure(&cannotDoFromHereMsg);
            exit;
        }

        /*
         *   if the actor is already directly in me, simply check to make
         *   sure the actor is in the default posture for the room - if
         *   not, try running an appropriate implied command to change the
         *   posture 
         */
        if (gActor.isDirectlyIn(self))
        {
            /* if the actor's already in the default posture, we're okay */
            if (gActor.posture == defaultPosture)
                return nil;
            
            /* run the implied command to stand up (or whatever) */
            if (allowImplicit && tryMakingDefaultPosture())
            {
                /* make sure we're in the proper posture now */
                if (gActor.posture != defaultPosture)
                    exit;
                
                /* note that we ran an implied command */
                return true;
            }
            
            /* we couldn't get into the default posture - give up */
            reportFailure(mustDefaultPostureProp);
            exit;
        }

        /*
         *   The actor is within a nested room within me.  Find our
         *   immediate child containing the actor, and remove the actor
         *   from the child. 
         */
        foreach (local cur in contents)
        {
            /* if this is the one containing the actor, remove the actor */
            if (gActor.isIn(cur))
                return cur.checkActorOutOfNested(allowImplicit);
        }

        /* we didn't find the nested room with the actor, so give up */
        reportFailure(&cannotDoFromHereMsg);
        exit;
    }

    /*
     *   Check, using pre-condition rules, that the actor is ready to
     *   enter this room as a nested location.  By default, we do nothing,
     *   since we're not designed as a nested location.  
     */
    checkActorReadyToEnterNestedRoom(allowImplicit)
    {
        return nil;
    }

    /*
     *   Check that the traveler is directly in the given room, using
     *   pre-condition rules.  'nested' is the nested location immediately
     *   within this room that contains the actor (directly or
     *   indirectly).  
     */
    checkTravelerDirectlyInRoom(traveler, allowImplicit)
    {
        /* if the actor is already directly in this room, we're done */
        if (traveler.isDirectlyIn(self))
            return nil;

        /* try moving the actor here */
        return traveler.checkMovingTravelerInto(self, allowImplicit);
    }

    /*
     *   Check, using pre-condition rules, that the actor is removed from
     *   this nested location and moved to its exit destination.  By
     *   default, we're not a nested location, so there's nothing for us
     *   to do.  
     */
    checkActorOutOfNested(allowImplicit)
    {
        /* we're not a nested location, so there's nothing for us to do */
        return nil;
    }

    /*
     *   Determine if the current gActor, who is directly in this location,
     *   is "travel ready."  This means that the actor is ready, as far as
     *   this location is concerned, to traverse the given connector.  By
     *   default, we consider an actor to be travel-ready if the actor is
     *   standing; this takes care of most nested room situations, such as
     *   chairs and beds, automatically.  
     */
    isActorTravelReady(conn) { return gActor.posture == standing; }

    /*
     *   Run an implicit action, if possible, to make the current actor
     *   "travel ready."  This will be called if the actor is directly in
     *   this location and isActorTravelReady() returns nil.  By default,
     *   we try to make the actor stand up.  This should always be paired
     *   with isActorTravelReady - the condition that routine tests should
     *   be the condition this routine tries to bring into effect.  If no
     *   implicit action is possible, simply return nil.  
     */
    tryMakingTravelReady(conn) { return tryImplicitAction(Stand); }

    /* the message explaining what we must do to be travel-ready */
    notTravelReadyMsg = &mustBeStandingMsg

    /*
     *   An actor is attempting to disembark this location.  By default,
     *   we'll simply turn this into an "exit" command.  
     */
    disembarkRoom()
    {
        /* treat this as an 'exit' command */
        replaceAction(Out);
    }

    /*
     *   The destination for objects explicitly dropped by an actor within
     *   this room.  By default, we'll return self, because items dropped
     *   should simply go in the room itself.  Some types of rooms will
     *   want to override this; for example, a room that represents the
     *   middle of a tightrope would probably want to set the drop
     *   destination to the location below the tightrope.  Likewise,
     *   objects like chairs will usually prefer to have dropped items go
     *   into the enclosing room.  
     */
    getDropDestination(objToDrop, path)
    {
        /* by default, objects dropped in this room end up in this room */
        return self;
    }

    /* 
     *   The nominal drop destination - this is the location where objects
     *   are *reported* to go when dropped by an actor in this location.
     *   By default, we simply return 'self'.
     *   
     *   The difference between the actual drop location and the nominal
     *   drop location is that the nominal drop location is used only for
     *   reporting messages, while the actual drop location is the
     *   location where objects are moved on 'drop' or equivalent actions.
     *   Rooms, for example, want to report that a dropped object lands on
     *   the floor (or the ground, or whatever), even though the room
     *   itself is the location where the object actually ends up.  We
     *   distinguish between the nominal and actual drop location to allow
     *   these distinctions in reported messages.  
     */
    getNominalDropDestination() { return self; }

    /*
     *   The "nominal actor container" - this is the container which we'll
     *   say actors are in when we describe actors who are actually in
     *   this location.  By default, this simply returns self, but it's
     *   sometimes useful to describe actors as being in some object other
     *   than self.  The most common case is that normal top-level rooms
     *   usually want to describe actors as being "on the floor" or
     *   similar.  
     */
    getNominalActorContainer(posture) { return self; }

    /*
     *   Get any extra items in scope for an actor in this location.
     *   These are items that are to be in scope even if they're not
     *   reachable through any of the normal sense paths (so they'll be in
     *   scope even in the dark, for example).
     *   
     *   By default, this returns nothing.  Subclasses can override as
     *   necessary to include additional items in scope.  For example, a
     *   chair would probably want to include itself in scope, since the
     *   actor presumably knows he or she is sitting in a chair even if
     *   it's too dark to see the chair.  
     */
    getExtraScopeItems(actor) { return []; }
    
    /*
     *   Receive notification that we're about to perform a command within
     *   this location.  This is called on the outermost room first, then
     *   on the nested rooms, from the outside in, until reaching the room
     *   directly containing the actor performing the command.  
     */
    roomBeforeAction()
    {
    }

    /*
     *   Receive notification that we've just finished a command within
     *   this location.  This is called on the room immediately containing
     *   the actor performing the command, then on the room containing
     *   that room, and so on to the outermost room. 
     */
    roomAfterAction()
    {
    }

    /*
     *   Get my notification list - this is a list of objects on which we
     *   must call beforeAction and afterAction when an action is
     *   performed within this room.
     *   
     *   We'll also include any registered notification items for all of
     *   our containing rooms up to the outermost container.
     *   
     *   The general notification mechanism always includes in the
     *   notification list all of the objects connected by containment to
     *   the actor, so objects that are in this room need not register for
     *   explicit notification.  
     */
    getRoomNotifyList()
    {
        local lst;
        
        /* start with our explicitly registered list */
        lst = roomNotifyList;

        /* add notification items for our immediate locations  */
        forEachContainer(
            {cont: lst = lst.appendUnique(cont.getRoomNotifyList())});

        /* return the result */
        return lst;
    }

    /*
     *   Add an item to our registered notification list for actions in
     *   the room.
     *   
     *   Items can be added here if they must be notified of actions
     *   performed by within the room even when the items aren't in the
     *   room at the time of the action.  All items connected by
     *   containment with the actor performing an action are automatically
     *   notified of the action; only items that must receive notification
     *   even when not connected by containment need to be registered
     *   here.  
     */
    addRoomNotifyItem(obj)
    {
        roomNotifyList += obj;
    }

    /* remove an item from the registered notification list */
    removeRoomNotifyItem(obj)
    {
        roomNotifyList -= obj;
    }

    /* our list of registered notification items */
    roomNotifyList = []

    /*
     *   Get the room location.  Since we're capable of holding actors, we
     *   are our own room location. 
     */
    roomLocation = (self)

    /*
     *   Receive notification that a traveler is arriving.  This is a
     *   convenience method that rooms can override to carry out side
     *   effects of arrival.  This is called just before the room's
     *   arrival message (usually the location description) is displayed,
     *   so the method can make any adjustments to the room's status or
     *   contents needed for the arrival.  By default, we do nothing.
     */
    enteringRoom(traveler) { }

    /*
     *   Receive notification that a traveler is leaving.  This is a
     *   convenience method that rooms can override to carry out side
     *   effects of departure.  This is called just after any departure
     *   message is displayed.  By default, we do nothing.  
     */
    leavingRoom(traveler) { }

    /*
     *   Receive notification that a traveler is about to leave the room.
     *   'traveler' is the object actually traveling.  In most cases this
     *   is simply the actor; but when the actor is in a vehicle, this is
     *   the vehicle instead.  
     *   
     *   By default, we describe the traveler's departure if the traveler's
     *   destination is different from its present location.  
     */
    travelerLeaving(traveler, dest, connector)
    {
        /* describe the departure */
        if (dest != traveler.location)
            traveler.describeDeparture(dest, connector);

        /* run the departure notification */
        leavingRoom(traveler);
    }

    /*
     *   Receive notification that a traveler is arriving in the room.
     *   'traveler' is the object actually traveling.  In most cases this
     *   is simply the actor; but when the actor is in a vehicle, this is
     *   the vehicle instead.  
     *   
     *   By default, we set each of the "motive" actors to its default
     *   posture, then describe the arrival.  
     */
    travelerArriving(traveler, origin, connector, backConnector)
    {
        /* 
         *   Set the self-motive actors into the proper default posture
         *   for the location.  We only do this for actors moving under
         *   their own power, since actors in vehicles will presumably
         *   just stay in the posture that's appropriate for the vehicle. 
         */
        foreach (local actor in traveler.getTravelerMotiveActors)
        {
            /*
             *   If the actor isn't in this posture already, set the actor
             *   to the new posture.  Note that we set the new posture
             *   directly, rather than via a nested command; we don't want
             *   the travel to consist of a NORTH plus a SIT, but simply of
             *   a NORTH.  Note that this could bypass side effects
             *   normally associated with the SIT (or whatever), but we
             *   assume that when a room with a specific posture is linked
             *   directly from a separate location, the travel connector
             *   linking up to the new room will take care of the necessary
             *   side effects.  
             */
            if (actor.posture != defaultPosture)
                actor.makePosture(defaultPosture);
        }
        
        /* run the arrival notification */
        enteringRoom(traveler);
        
        /* describe the arrival */
        traveler.describeArrival(origin, backConnector);
    }

    /*
     *   Receive notification of travel among nested rooms.  When an actor
     *   moves between two locations related directly by containing (such
     *   as from a chair to the room containing the chair, or vice versa),
     *   we first call this routine on the origin of the travel, then we
     *   move the actor, then we call this same routine on the destination
     *   of the travel.
     *   
     *   This routine is used any time an actor is moved with
     *   travelWithin().  This is not used when an actor travels between
     *   locations related by a TravelConnector object rather than by
     *   direct containment.
     *   
     *   We do nothing by default.  Locations can override this if they
     *   wish to perform any special handling during this type of travel.  
     */
    actorTravelingWithin(origin, dest)
    {
    }

    /*
     *   Determine if the given actor has "intrinsic" knowledge of the
     *   destination of the given travel connector leading away from this
     *   location.  This knowledge is independent of any memory the actor
     *   has of actual travel through the connector in the course of the
     *   game, which we track separately via the TravelConnector's travel
     *   memory mechanism.
     *   
     *   There are two main reasons an actor would have intrinsic
     *   knowledge of a connector's destination:
     *   
     *   1. The actor is supposed to be familiar with the location and its
     *   surroundings, within the context of the game.  For example, if
     *   part of the game is the player character's own house, the PC
     *   would probably know where all of the connections among rooms go.
     *   
     *   2. The destination location is plainly visible from this location
     *   or is clearly marked (such as with a sign).  For example, if the
     *   current location is an open field, a nearby hilltop to the east
     *   might be visible from here, so we could see from here where we'll
     *   end up by going east.  Alternatively, if we're in a lobby, and
     *   the passage to the west is marked with a sign reading "electrical
     *   room," an actor would have good reason to think an electrical
     *   room lies to the west.
     *   
     *   We handle case (1) automatically through our actorIsFamiliar()
     *   method: if the actor is familiar with the location, we assume by
     *   default that the actor knows where all of the connectors from
     *   here go.  We don't have any default handling for case (2), so
     *   individual rooms (or subclasses) must override this method if
     *   they want to specify intrinsic knowledge for any of their
     *   outgoing connectors.  
     */
    actorKnowsDestination(actor, conn)
    {
        /* 
         *   if the actor is familiar with this location, then the actor
         *   by default knows where all of the outgoing connections go 
         */
        if (actorIsFamiliar(actor))
            return true;

        /* there's no other way the actor would know the destination */
        return nil;
    }

    /*
     *   Is the actor familiar with this location?  In other words, is the
     *   actor supposed to know the location well at the start of the game?
     *   
     *   This should return true if the actor is familiar with this
     *   location, nil if not.  By default, we return nil, since actors
     *   are not by default familiar with any locations.
     *   
     *   The purpose of this routine is to determine if the actor is meant
     *   to know the location well, within the context of the game, even
     *   before the game starts.  For example, if an area in the game is
     *   an actor's own house, the actor would naturally be familiar,
     *   within the context of the game, with the locations making up the
     *   house.
     *   
     *   Note that this routine doesn't need to "learn" based on the
     *   events of the game.  The familiarity here is meant only to model
     *   the actor's knowledge as of the start of the game.  
     */
    actorIsFamiliar(actor) { return nil; }

    /* 
     *   The default "you can't go that way" message for travel within this
     *   location in directions that don't allow travel.  This is shown
     *   whenever an actor tries to travel in one of the directions we have
     *   set to point to noTravel.  A room can override this to produce a
     *   different, customized message for unset travel directions - this
     *   is an easy way to change the cannot-travel message for several
     *   directions at once.
     *   
     *   The handling depends on whether or not it's dark.  If it's dark,
     *   we don't want to reveal whether or not it's actually possible to
     *   perform the travel, since there's no light to see where the exits
     *   are.  
     */
    cannotTravel()
    {
        /* check for darkness */
        if (!gActor.isLocationLit())
        {
            /* the actor is in the dark - use our dark travel message */
            cannotGoThatWayInDark();
        }
        else
        {
            /* use the standard "can't go that way" routine */
            cannotGoThatWay();
        }
    }

    /*
     *   Receive notification of travel from one dark location to another.
     *   This is called before the actor is moved from the source
     *   location, and can cancel the travel if desired by using 'exit' to
     *   terminate the command.
     *   
     *   By default, we'll simply display the same handler we do when the
     *   player attempts travel in a direction with no travel possible in
     *   the dark (cannotGoThatWayInDark), and then use 'exit' to cancel
     *   the command.  This default behavior provides the player with no
     *   mapping information in the dark, since the same message is
     *   generated whether or not travel would be possible in a given
     *   direction were light present.  
     */
    roomDarkTravel(actor)
    {
        /* 
         *   show the same message we would show if we attempted travel in
         *   the dark in a direction with no exit 
         */
        cannotGoThatWayInDark();

        /* terminate the command */
        exit;
    }

    /* 
     *   Show the default "you can't go that way" message for this
     *   location.  By default, we show a generic message, but individual
     *   rooms might want to override this to provide a more specific
     *   description of why travel isn't allowed.  
     */
    cannotGoThatWay()
    {
        /* "you can't go that way" */
        reportFailure(cannotGoThatWayMsg);

        /* show a list of exits, if appropriate */
        cannotGoShowExits(gActor);
    }

    /* 
     *   The message to display when it's not possible to travel in a given
     *   direction from this room; this is either a single-quoted string or
     *   an actor action messages property (by default, it's the latter,
     *   giving a default library message).  
     */
    cannotGoThatWayMsg = &cannotGoThatWayMsg

    /*
     *   Show a version of the "you can't go that way" message for travel
     *   while in the dark.  This is called when the actor is in the dark
     *   (i.e., there's no ambient light at the actor) and attempts to
     *   travel in a direction that doesn't allow travel.  By default, we
     *   show a generic "you can't see where you're going in the dark"
     *   message.
     *   
     *   This routine is essentially a replacement for the
     *   cannotGoThatWay() routine that we use when the actor is in the
     *   dark.  
     */
    cannotGoThatWayInDark()
    {
        /* "it's too dark; you can't see where you're going */
        reportFailure(&cannotGoThatWayInDarkMsg);
    }

    /*
     *   Get preconditions for travel for an actor in this location.  These
     *   preconditions should be applied by any command that will involve
     *   travel from this location.  By default, we impose no additional
     *   requirements.
     */
    roomTravelPreCond = []

    /*
     *   Get the effective location of an actor directly within me, for
     *   the purposes of a "follow" command.  To follow someone, we must
     *   have the same effective follow location that the target had when
     *   we last observed the target leaving.
     *   
     *   For most rooms, this is simply the room itself.  
     */
    effectiveFollowLocation = (self)

    /*
     *   Dispatch the room daemon.  This is a daemon routine invoked once
     *   per turn; we in turn invoke roomDaemon on the current player
     *   character's current location.  
     */
    dispatchRoomDaemon()
    {
        /* call roomDaemon on the player character's location */
        if (gPlayerChar.location != nil)
            gPlayerChar.location.roomDaemon();
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Room: the basic class for top-level game locations (that is, game
 *   locations that aren't inside any other simulation objects, but are at
 *   the top level of the containment hierarchy).  This is the smallest
 *   unit of movement; we do not distinguish among locations within a
 *   room, even if a Room represents a physically large location.  If it
 *   is necessary to distinguish among different locations in a large
 *   physical room, simply divide the physical room into sections and
 *   represent each section with a separate Room object.
 *   
 *   A Room is not necessarily indoors; it is simply a location where an
 *   actor can be located.  This peculiar usage of "room" to denote any
 *   atomic location, even outdoors, was adopted by the authors of the
 *   earliest adventure games, and has been the convention ever since.
 *   
 *   A room's contents are the objects contained directly within the room.
 *   These include fixed features of the room as well as loose items in
 *   the room, which are effectively "on the floor" in the room.
 *   
 *   The Room class implements the Travel Connector interface in such a
 *   way that travel from one room to another can be established simply by
 *   setting a direction property (north, south, etc) in the origin room
 *   to point to the destination room.  This type of travel link has no
 *   side effects and is unconditional.
 *   
 *   A room is by default an indoor location; this means that it contains
 *   walls, floor, and ceiling.  An outdoor location should be based on
 *   OutdoorRoom rather than Room.  
 */
class Room: Fixture, BasicLocation, RoomAutoConnector
    /* 
     *   Initialize 
     */
    initializeThing()
    {
        /* inherit default handling */
        inherited();

        /* 
         *   Add my room parts to my contents list.  Only include room
         *   parts that don't have explicit locations. 
         */
        contents += roomParts.subset({x: x.location == nil});
    }

    /* 
     *   we're a "top-level" location: we don't have any other object
     *   containing us, but we're nonetheless part of the game world, so
     *   we're at the top level of the containment tree 
     */
    isTopLevel = true

    /*
     *   we generally do not want rooms to be included when a command
     *   refers to 'all' 
     */
    hideFromAll(action) { return true; }

    /* don't consider myself a default for STAND ON, SIT ON, or LIE ON */
    hideFromDefault(action)
    {
        /* don't hide from STAND ON, SIT ON, LIE ON */
        if (action.ofKind(StandOnAction)
            || action.ofKind(SitOnAction)
            || action.ofKind(LieOnAction))
            return nil;

        /* don't hide from defaults for other actions, though */
        return inherited(action);
    }


    /*
     *   Most rooms provide their own implicit lighting.  We'll use
     *   'medium' lighting (level 3) by default, which provides enough
     *   light for all activities, but is reduced to dim light (level 2)
     *   when it goes through obscuring media or over distance.  
     */
    brightness = 3

    /*
     *   Get my "destination name," as seen by the given actor from the
     *   given origin location.  This gives the name we can use to
     *   describe this location from the perspective of an actor in an
     *   adjoining location looking at a travel connector from that
     *   location to here.
     *   
     *   By default, we simply return our destName property.  This default
     *   behavior can be overridden if it's necessary for a location to
     *   have different destination names in different adjoining
     *   locations, or when seen by different actors.
     *   
     *   If this location's name cannot or should not be described from an
     *   adjoining location, this should simply return nil.  
     */
    getDestName(actor, origin) { return destName; }

    /* 
     *   Our destination name, if we have one.  By default, we make this
     *   nil, which means the room cannot be described as a destination of
     *   connectors from adjoining locations. 
     */
    destName = nil

    /*
     *   My "atmosphere" list.  This can be set to an EventList object to
     *   provide atmosphere messages while the player character is within
     *   this room.  The default roomDaemon will show one message from this
     *   EventList (by calling the EventList's doScript() method) on each
     *   turn the player character is in this location.  
     */
    atmosphereList = nil

    /*
     *   Room daemon - this is invoked on the player character's immediate
     *   location once per turn in a daemon.
     */
    roomDaemon()
    {
        /* 
         *   if we have an atmosphere message list, display the next
         *   message 
         */
        if (atmosphereList != nil)
        {
            /* show visual separation, then the current atmosphere message */
            "<.commandsep>";
            atmosphereList.doScript();
        }
    }

    /* 
     *   The nominal drop destination - this is the location where we
     *   describe objects as being when they're actually directly within
     *   the room.
     *   
     *   By default, we return the object representing the room's floor.
     *   If there's no floor, we simply return 'self'.  
     */
    getNominalDropDestination()
    {
        local floor;

        /* 
         *   if there's a floor, it's the nominal drop destination;
         *   otherwise, just indicate that our contents are in 'self' 
         */
        return ((floor = roomFloor) != nil ? floor : self);
    }

    /*
     *   Since we could be our own nominal drop destination, we need a
     *   message to describe things being put here.
     */
    putDestMessage = &putDestRoom

    /*
     *   The nominal actor container.  By default, this is the room's
     *   nominal drop destination, which is usually the floor or
     *   equivalent.  
     */
    getNominalActorContainer(posture) { return getNominalDropDestination(); }

    /* 
     *   move something into a room is accomplished by putting the object
     *   on the floor 
     */
    tryMovingObjInto(obj)
    {
        local floor;

        /* if we have a floor, put the object there */
        if ((floor = roomFloor) != nil)
            return tryImplicitAction(PutOn, obj, floor);
        else
            return nil;
    }

    /* explain that something must be in the room first */
    mustMoveObjInto(obj)
    {
        local floor;

        /* if we have a floor, say that the object has to go there */
        if ((floor = roomFloor) != nil)
            reportFailure(&mustBeInMsg, obj, floor);
        else
            inherited(obj);
    }

    /*
     *   Get the apparent location of one of our room parts.
     *   
     *   In most cases, we use generic objects (defaultFloor,
     *   defaultNorthWall, etc.) for the room parts.  There's only one
     *   instance of each of these generic objects in the whole game -
     *   there's only one floor, one north wall, and so on - so these
     *   instances can't have specific locations the way normal objects do.
     *   Thus the need for this method: this tells us the *apparent*
     *   location of one of the room part objects as perceived from this
     *   room.
     *   
     *   If the part isn't in this location, we'll return nil.  
     */
    getRoomPartLocation(part)
    {
        local loc;
        
        /* 
         *   if this part is in our part list, then its apparent location
         *   is 'self' 
         */
        if (roomParts.indexOf(part) != nil)
            return self;

        /* 
         *   if the room part has an explicit location itself, and that
         *   location is either 'self' or is in 'self', return the
         *   location 
         */
        if (part != nil
            && (loc = part.location) != nil
            && (loc == self || loc.isIn(self)))
            return loc;

        /* we don't have the part */
        return nil;
    }

    /*
     *   Get the list of extra parts for the room.  An indoor location has
     *   walls, floor, and ceiling; these are all generic objects that are
     *   included for completeness only.
     *   
     *   A room with special walls, floor, or ceiling should override this
     *   to eliminate the default objects from appearing in the room.
     *   Note that if the room has a floor, it should always be
     *   represented by an object of class Floor, and should always be
     *   part of this list.  
     */
    roomParts = [defaultFloor, defaultCeiling,
                 defaultNorthWall, defaultSouthWall,
                 defaultEastWall, defaultWestWall]

    /*
     *   Get the room's floor.  This looks for an object of class Floor in
     *   the roomParts list; if there is no such object, we'll return nil,
     *   indicating that the room has no floor at all.  
     */
    roomFloor = (roomParts.valWhich({x: x.ofKind(Floor)}))

    /*
     *   Get any extra items in scope in this location.  These are items
     *   that are to be in scope even if they're not reachable through any
     *   of the normal sense paths (so they'll be in scope even in the
     *   dark, for example).
     *   
     *   By default, if we have a floor, and the actor is directly in this
     *   room, we return our floor: this is because an actor is presumed to
     *   be in physical contact with the floor whenever directly in the
     *   room, and thus the actor would be aware that there's a floor
     *   there, even if the actor can't see the floor.  In some rooms, it's
     *   desirable to have certain objects in scope because they're
     *   essential features of the room; for example, a location that is
     *   part of a stairway would probably have the stairs in scope by
     *   virtue of the actor standing on them.  
     */
    getExtraScopeItems(actor)
    {
        local floor;
        
        /* 
         *   if we have a floor, and the actor is in this room, explicitly
         *   make the floor in scope 
         */
        if ((floor = roomFloor) != nil && actor.isDirectlyIn(self))
            return [floor];
        else
            return [];
    }

    /*
     *   When we're in the room, treat EXAMINE <ROOM> the same as LOOK
     *   AROUND.  (This will only work if the room is given vocabulary
     *   like a normal object.)  
     */
    dobjFor(Examine)
    {
        verify()
        {
            /* 
             *   When we're in the room, downgrade the likelihood a bit, as
             *   we'd rather inspect an object within the room if there's
             *   something with the same name.  When we're *not* in the
             *   room - meaning this is a remote room that's visible from
             *   the player's location - downgrade the likelihood even
             *   more, since we're more likely to want to examine the room
             *   we're actually in than a remote room with the same name.  
             */
            if (gActor.isIn(self))
                logicalRank(80, 'x room');
            else
                logicalRank(70, 'x room');
        }

        action()
        {
            /* 
             *   if the looker is within the room, replace EXAMINE <self>
             *   with LOOK AROUND; otherwise, use the normal description 
             */
            if (gActor.isIn(self) && gActor.canSee(self))
                gActor.lookAround(LookRoomDesc | LookListSpecials
                                  | LookListPortables);
            else
                inherited();
        }
    }

    /* treat LOOK IN <room> as EXAMINE <room> */
    dobjFor(LookIn) remapTo(Examine, self)

    /* LOOK UNDER and BEHIND are illogical */
    dobjFor(LookUnder) { verify { illogical(&cannotLookUnderMsg); } }
    dobjFor(LookBehind) { verify { illogical(&cannotLookBehindMsg); } }

    /* treat SMELL/LISTEN TO <room> as just SMELL/LISTEN */
    dobjFor(Smell) remapTo(SmellImplicit)
    dobjFor(ListenTo) remapTo(ListenImplicit)

    /* map STAND/SIT/LIE ON <room> to my default floor */
    dobjFor(StandOn) maybeRemapTo(roomFloor != nil, StandOn, roomFloor)
    dobjFor(SitOn) maybeRemapTo(roomFloor != nil, SitOn, roomFloor)
    dobjFor(LieOn) maybeRemapTo(roomFloor != nil, LieOn, roomFloor)

    /* 
     *   treat an explicit GET OUT OF <ROOM> as OUT if there's an apparent
     *   destination for OUT; otherwise treat it as "vague travel," which
     *   simply tells the player that they need to specify a direction 
     */
    dobjFor(GetOutOf)
    {
        remap()
        {
            /* remap only if this isn't an implied action */
            if (gAction.parentAction == nil)
            {
                /* 
                 *   if we have an apparent Out connection, go there;
                 *   otherwise it's not obvious where we're meant to go 
                 */
                if (out != nil && out.isConnectorApparent(self, gActor))
                    return [OutAction];
                else
                    return [VagueTravelAction];
            }

            /* don't remap here - use the standard handling */
            return inherited();
        }
    }

    /* 
     *   for BOARD and ENTER, there are three possibilities:
     *   
     *   - we're already directly in this room, in which case it's
     *   illogical to travel here again
     *   
     *   - we're in a nested room within this room, in which case ENTER
     *   <self> is the same as GET OUT OF <outermost nested room within
     *   self>
     *   
     *   - we're in a separate top-level room that's connected by a sense
     *   connector, in which case ENTER <self> should be handled as TRAVEL
     *   VIA <connector from actor's current location to self> 
     */
    dobjFor(Board) asDobjFor(Enter)
    dobjFor(Enter)
    {
        verify()
        {
            /* 
             *   if we're already here, entering the same location is
             *   redundant; if we're not in a nested room, we need a travel
             *   connector to get there from here 
             */
            if (gActor.isDirectlyIn(self))
                illogicalAlready(&alreadyInLocMsg);
            else if (!gActor.isIn(self)
                     && gActor.location.getConnectorTo(gActor, self) == nil)
                illogicalNow(&whereToGoMsg);
        }
        preCond()
        {
            /* 
             *   if we're in a different top-level room, and there's a
             *   travel connector, we'll simply travel via the connector,
             *   so we don't need to impose any pre-conditions of our own 
             */
            if (!gActor.isIn(self)
                && gActor.location.getConnectorTo(gActor, self) != nil)
                return [];

            /* 
             *   if we're in a nested room within this object, we'll
             *   replace the action with "get out of <outermost nested
             *   room>", so there's no need for any extra preconditions
             *   here 
             */
            if (gActor.isIn(self) && !gActor.isDirectlyIn(self))
                return [];

            /* otherwise, use the default conditions */
            return inherited();
        }
        action()
        {
            /* 
             *   if we're in a nested room, get out; otherwise travel via a
             *   suitable travel connector 
             */
            if (gActor.isIn(self) && !gActor.isDirectlyIn(self))
            {
                /* 
                 *   get out of the *outermost* nested room - that is, our
                 *   direct child that contains the actor 
                 */
                local chi = contents.valWhich({x: gActor.isIn(x)});

                /* if we found it, get out of it */
                if (chi != nil)
                    replaceAction(GetOutOf, chi);
            }
            else
            {
                /* get the connector from here to there */
                local conn = gActor.location.getConnectorTo(gActor, self);

                /* if we found it, go that way */
                if (conn != nil)
                    replaceAction(TravelVia, conn);
            }

            /* 
             *   if we didn't replace the action yet, we can't figure out
             *   how to get here from the actor's current location 
             */
            reportFailure(&whereToGoMsg);
        }
    }
;

/*
 *   A dark room, which provides no light of its own 
 */
class DarkRoom: Room
    /* 
     *   turn off the lights 
     */
    brightness = 0
;

/*
 *   An outdoor location.  This differs from an indoor location in that it
 *   has ground and sky rather than floor and ceiling, and has no walls.  
 */
class OutdoorRoom: Room
    /* an outdoor room has ground and sky, but no walls */
    roomParts = [defaultGround, defaultSky]
;

/*
 *   A shipboard room.  This is a simple mix-in class: it can be used
 *   along with any type of Room to indicate that this room is aboard a
 *   ship.  When a room is aboard a ship, the shipboard travel directions
 *   (port, starboard, fore, aft) are allowed; these directions normally
 *   make no sense.
 *   
 *   This is a mix-in class rather than a Room subclass to allow it to be
 *   used in conjunction with any other Room subclass.  To make a room
 *   shipboard, simply declare your room like this:
 *   
 *   mainDeck: Shipboard, Room // etc 
 */
class Shipboard: object
    /* mark the location as being aboard ship */
    isShipboard = true
;

/* 
 *   For convenience, we define ShipboardRoom as a shipboard version of the
 *   basic Room type. 
 */
class ShipboardRoom: Shipboard, Room
;

/* ------------------------------------------------------------------------ */
/*
 *   Make a room "floorless."  This is a mix-in class that you can include
 *   in a superclass list ahead of Room or any of its subclasses to create
 *   a room where support is provided by some means other than standing on
 *   a surface, or where there's simply no support.  Examples: hanging on a
 *   rope over a chasm; climbing a ladder; in free-fall after jumping out
 *   of a plane; levitating in mid-air.
 *   
 *   There are two main special features of a floorless room.  First, and
 *   most obviously, there's no "floor" or "ground" object among the room
 *   parts.  We accomplish this by simply subtracting out any object of
 *   class Floor from the room parts list inherited from the combined base
 *   room class.
 *   
 *   Second, there's no place to put anything down, so objects dropped here
 *   either disappear from the game or are transported to another location
 *   (the room at the bottom of the chasm, for example).  
 */
class Floorless: object
    /* 
     *   Omit the default floor/ground objects from the room parts list.
     *   Room classes generally have static room parts lists, so calculate
     *   this once per instance and store the results.
     *   
     *   NOTE - if you combine Floorless with a base Room class that has a
     *   dynamic room parts list, you'll need to override this to calculate
     *   the subset dynamically on each invocation.  
     */
    roomParts = perInstance(inherited().subset({x: !x.ofKind(Floor)}))

    /* 
     *   The room below, if any - this is where objects dropped here will
     *   actually end up.  By default, this is nil, which means that
     *   objects dropped here simply disappear from the game.  If there's a
     *   "bottom of chasm" location where dropped objects should land,
     *   provide it here.  
     */
    bottomRoom = nil

    /* receive a dropped object */
    receiveDrop(obj, desc)
    {
        /* 
         *   move the dropped object to the room at the bottom of whatever
         *   it is we're suspended over; if there is no bottom room, we'll
         *   simply remove the dropped object from the game 
         */
        obj.moveInto(bottomRoom);

        /* 
         *   Say that the object drops out of sight below.  Build this by
         *   combining the generic report prefix from the drop descriptor
         *   with our generic suffix, which describes the object as
         *   vanishing below. 
         */
        mainReport(desc.getReportPrefix(obj, self)
                   + gActor.getActionMessageObj().floorlessDropMsg(obj));
    }
;

/* 
 *   For convenience, provide a combination of Floorless with the ordinary
 *   Room. 
 */
class FloorlessRoom: Floorless, Room;

/* ------------------------------------------------------------------------ */
/*
 *   Room Part - base class for "parts" of rooms, such as floors and walls.
 *   Room parts are unusual in a couple of ways.
 *   
 *   First, room parts are frequently re-used widely throughout a game.  We
 *   define a single instance of each of several parts that are found in
 *   typical rooms, and then re-use those instances in all rooms with those
 *   parts.  For example, we define one "default floor" object, and then
 *   use that object in most or all rooms that have floors.  We do this for
 *   efficiency, to avoid creating hundreds of essentially identical copies
 *   of the common parts.
 *   
 *   Second, because room parts are designed to be re-used, things that are
 *   in or on the parts are actually represented in the containment model
 *   as being directly in their containing rooms.  For example, an object
 *   that is said to be "on the floor" actually has its 'location' property
 *   set to its immediate room container, and the 'contents' list that
 *   contains the object is that of the room, not of the floor object.  We
 *   must therefore override some of the normal handling for object
 *   locations within room parts, in order to make it appear (for the
 *   purposes of command input and descriptive messages) that the things
 *   are in/on their room parts, even though they're not really represented
 *   that way in the containment model.  
 */
class RoomPart: Fixture
    /*
     *   When we explicitly examine a RoomPart, list any object that's
     *   nominally contained in the room part, as long it doesn't have a
     *   special description for the purposes of the room part.  (If it
     *   does have a special description, then examining the room part will
     *   automatically display that special desc, so we don't want to
     *   include the object in a separate list of miscellaneous contents of
     *   the room part.)  
     */
    isObjListedInRoomPart(obj)
    {
         /* 
          *   list the object *unless* it has a special description for the
          *   purposes of examining this room part 
          */
        return !obj.useSpecialDescInRoomPart(self);
    }

    /*
     *   Add this room part to the given room.
     *   
     *   Room parts don't have normal "location" properties.  Instead, a
     *   room part explicitly appears in the "roomParts" list of each room
     *   that contains it.  For the most part, room parts are static -
     *   they're initialized in the room definitions and never changed.
     *   However, if you need to dynamically add a room part to a room
     *   during the game, you can do so using this method.  
     */
    moveIntoAdd(room)
    {
        /* add me to the room's 'roomParts' and 'contents' lists */
        room.roomParts += self;
        room.contents += self;
    }

    /*
     *   Remove this room part from the given room.  This can be used if
     *   it's necessary to remove the room part dynamically from a room.  
     */
    moveOutOf(room)
    {
        /* remove me from the room's 'roomParts' and 'contents' lists */
        room.roomParts -= self;
        room.contents -= self;
    }

    /* 
     *   Don't include room parts in 'all'.  Room parts are so ubiquitous
     *   that we never want to assume that they're involved in a command
     *   except when it is specifically so stated.  
     */
    hideFromAll(action) { return true; }

    /* do allow use as a default, though */
    hideFromDefault(action) { return nil; }

    /*
     *   When multiple room parts show up in a resolve list, and some of
     *   the parts are local to the actor's immediate location and others
     *   aren't, keep only the local ones.  This helps avoid pointless
     *   ambiguity in cases where two (or more) top-level locations are
     *   linked with a sense connector, and one or the other location has
     *   custom room part objects. 
     */
    filterResolveList(lst, action, whichObj, np, requiredNum)
    {
        /* if a definite number of objects is required, check ambiguity */
        if (requiredNum != nil)
        {
            /* get the subset that's just RoomParts */
            local partLst = lst.subset({x: x.obj_.ofKind(RoomPart)});

            /* 
             *   get the *remote* subset - this is the subset that's not in
             *   the outermost room of the target actor 
             */
            local outer = action.actor_.getOutermostRoom();
            local remoteLst = partLst.subset({x: !x.obj_.isIn(outer)});

            /* 
             *   If all of the objects are remote, or all of them are
             *   local, we can't narrow things down on this basis; but if
             *   we found some remote and some local, eliminate the remote
             *   items, since we want to favor the local ones 
             */
            if (remoteLst.length() not in (0, partLst.length()))
                lst -= remoteLst;
        }

        /* now do any inherited work, and return the result */
        return inherited(lst, action, whichObj, np, requiredNum);
    }

    /*
     *   Since room parts are generally things like walls and floors that
     *   enclose the entire room, they're typically visually large, and
     *   tend to have fairly large-scale details (such as doors and
     *   windows).  So, by default we set the sightSize to 'large' so that
     *   the details are visible at a distance.  
     */
    sightSize = large

    /* 
     *   as with decorations, downgrade the likelihood for Examine, as the
     *   standard walls, floors, etc. are pretty much background noise
     *   that are just here in case someone wants to refer to them
     *   explicitly 
     */
    dobjFor(Examine)
    {
        verify()
        {
            inherited();
            logicalRank(70, 'x decoration');
        }
    }

    /* describe the status - shows the things that are in/on the part */
    examineStatus()
    {
        /* show the contents of the room part */
        examinePartContents(&descContentsLister);
    }

    /* show our contents */
    examinePartContents(listerProp)
    {
        /* 
         *   Get my location, as perceived by the actor - this is the room
         *   that contains this part.  If I don't have a location as
         *   perceived by the actor, then we can't show any contents.  
         */
        local loc = gActor.location.getRoomPartLocation(self);
        if (loc == nil)
            return;

        /* 
         *   create a copy of the lister customized for this part, if we
         *   haven't already done so 
         */
        if (self.(listerProp).part_ == nil)
        {
            self.(listerProp) = self.(listerProp).createClone();
            self.(listerProp).part_ = self;
        }

        /* show the contents of the containing location */
        self.(listerProp).showList(gActor, self, loc.contents, 0, 0,
                                   gActor.visibleInfoTable(), nil);
    }

    /* 
     *   show our special contents - this shows objects with special
     *   descriptions that are specifically in this room part 
     */
    examineSpecialContents()
    {
        local infoTab;
        local lst;
        
        /* get the actor's list of visible items */
        infoTab = gActor.visibleInfoTable();

        /* 
         *   get the list of special description items, using only the
         *   subset that uses special descriptions in this room part 
         */
        lst = specialDescList(infoTab,
                              {obj: obj.useSpecialDescInRoomPart(self)});

        /* show the list */
        specialContentsLister.showList(gActor, nil, lst, 0, 0, infoTab, nil);
    }

    /* 
     *   Get the destination for a thrown object that hits me.  Since we
     *   don't have a real location, we must ask the actor for our room
     *   part location, and then use its hit-and-fall destination.  
     */
    getHitFallDestination(thrownObj, path)
    {
        local loc;
        local dest;
        
        /* 
         *   if we have an explicit location, start with it; otherwise, ask
         *   the actor's location to find us; if we can't even find
         *   ourselves there, just use the actor's current location 
         */
        if ((loc = location) == nil
            && (loc = gActor.location.getRoomPartLocation(self)) == nil)
            loc = gActor.location;

        /* use the location's drop destination for thrown objects */
        dest = loc.getDropDestination(thrownObj, path);

        /* give the destination a chance to make adjustments */
        return dest.adjustThrowDestination(thrownObj, path);
    }

    /* consider me to be in any room of which I'm a part */
    isIn(loc)
    {
        local rpl;

        /* 
         *   get the room-part location of this room part, from the
         *   perspective of the prospective location we're asking about 
         */
        if (loc != nil && (rpl = loc.getRoomPartLocation(self)) != nil)
        {
            /* 
             *   We indeed have a room part location in the given
             *   location, so we're a part of the overall location.  We
             *   might be directly in the location (i.e., 'rpl' could
             *   equal 'loc'), or we might be somewhere relative to 'loc'
             *   (for example, we could be within a nested room within
             *   'loc', in which case 'rpl' is a nested room unequal to
             *   'loc' but is within 'loc').  So, if 'rpl' equals 'loc' or
             *   is contained in 'loc', I'm within 'loc', because I'm
             *   within 'rpl'.  
             */
            if (rpl == loc || rpl.isIn(loc))
                return true;
        }

        /* 
         *   we don't appear to be in 'loc' on the basis of our special
         *   room-part location; fall back on the inherited handling 
         */
        return inherited(loc);
    }

    /* our contents listers */
    contentsLister = roomPartContentsLister
    descContentsLister = roomPartDescContentsLister
    lookInLister = roomPartLookInLister
    specialContentsLister = specialDescLister

    /* look in/on: show our contents */
    dobjFor(LookIn)
    {
        verify() { }
        action()
        {
            /* show my contents */
            examinePartContents(&lookInLister);

            /* show my special contents */
            examineSpecialContents();
        }
    }

    /* we can't look behind/through/under a room part by default */
    nothingUnderMsg = &cannotLookUnderMsg
    nothingBehindMsg = &cannotLookBehindMsg
    nothingThroughMsg = &cannotLookThroughMsg

    /* 
     *   initialization - add myself to my location's roomPart list if I
     *   have an explicit location 
     */
    initializeThing()
    {
        /* do the normal work first */
        inherited();

        /* 
         *   if I have an explicit location, and I'm not in my location's
         *   roomPart list, add myself to the list 
         */
        if (location != nil && location.roomParts.indexOf(self) == nil)
            location.roomParts += self;
    }
;


/*
 *   A floor for a nested room.  This should be placed directly within a
 *   nested room object if the nested room is to be described as having a
 *   floor separate from the nested room itself.  We simply remap any
 *   commands relating to using the floor as a surface (Put On, Throw At,
 *   Sit On, Lie On, Stand On) to the enclosing nested room.  
 */
class NestedRoomFloor: Fixture
    iobjFor(PutOn) remapTo(PutOn, DirectObject, location)
    iobjFor(ThrowAt) remapTo(ThrowAt, DirectObject, location)
    dobjFor(SitOn) remapTo(SitOn, location)
    dobjFor(LieOn) remapTo(LieOn, location)
    dobjFor(StandOn) remapTo(StandOn, location)
;

/*
 *   Base class for the default floor and the default ground of a top-level
 *   room.  The floor and ground are where things usually go when dropped,
 *   and they're the locations where actors within a room are normally
 *   standing.  
 */
class Floor: RoomPart
    /* specifically allow me as a default for STAND ON, SIT ON, and LIE ON */
    hideFromDefault(action)
    {
        /* don't hide from STAND ON, SIT ON, LIE ON */
        if (action.ofKind(StandOnAction)
            || action.ofKind(SitOnAction)
            || action.ofKind(LieOnAction))
            return nil;

        /* for other actions, use the standard handling */
        return inherited(action);
    }

    /* 
     *   When explicitly examining a Floor object, list any objects that
     *   are listed in the normal room description (as in LOOK AROUND).  By
     *   default, the floor is the nominal container for anything directly
     *   in the room, so we'll normally want LOOK AROUND and LOOK AT FLOOR
     *   to produce the same list of objects.  
     */
    isObjListedInRoomPart(obj)
    {
         /* list the object if it's listed in a normal LOOK AROUND */
        return obj.isListed;
    }

    /* 
     *   'put x on floor' equals 'drop x'.  Add a precondition that the
     *   drop destination is the main room, since otherwise we could have
     *   strange results if we dropped something inside a nested room.  
     */
    iobjFor(PutOn)
    {
        preCond()
        {
            /* 
             *   require that this floor object itself is reachable, and
             *   that the drop destination for the direct object is an
             *   outermost room 
             */
            return [touchObj, new ObjectPreCondition(
                gDobj, dropDestinationIsOuterRoom)];
        }
        verify() { }
        action() { replaceAction(Drop, gDobj); }
    }

    /* 
     *   The message we use to describe this object prepositionally, as the
     *   destination of a throw or drop.  This should be a gLibMessages
     *   property with the appropriate prepositional phrase.  We use a
     *   custom message specific to floor-like objects.  
     */
    putDestMessage = &putDestFloor

    /* 'throw x at floor' */
    iobjFor(ThrowAt)
    {
        check()
        {
            /* 
             *   If I'm reachable, suggest just putting it down instead.
             *   We only make the suggestion, rather than automatically
             *   treating the command as DROP, because a player explicitly
             *   typing THROW <obj> AT FLOOR is probably attempting to
             *   express something more violent than merely putting the
             *   object down; the player probably is thinking in terms of
             *   breaking the object (or the floor).  
             */
            if (canBeTouchedBy(gActor))
            {
                mainReport(&shouldNotThrowAtFloorMsg);
                exit;
            }
        }
    }

    /* is the given actor already on the floor? */
    isActorOnFloor(actor)
    {
        /* 
         *   the actor is on the floor if the actor is directly in the
         *   floor's room-part-location for the actor's location 
         */
        return actor.isDirectlyIn(actor.location.getRoomPartLocation(self));
    }

    /* verify sitting/standing/lying on the floor */
    verifyEntry(newPosture, alreadyMsg)
    {
        /* 
         *   If we're already in my location, and we're in the desired
         *   posture, this command is illogical because we're already
         *   where they want us to end up.  Otherwise, it's logical to
         *   stand/sit/lie on the floor, but rank it low since we don't
         *   want the floor to interfere with selecting a default if
         *   there's anything around that's actually like a chair.
         *   
         *   If it's logical, note that we've verified okay for the
         *   action.  On a future pass, we might have enforced a
         *   precondition that moved us here, at which point our work will
         *   be done - but we don't want to complain in that case, since
         *   it was logical from the player's perspective to carry out the
         *   command even though we have nothing left to do.  
         */
        if (gActor.posture == newPosture
            && isActorOnFloor(gActor)
            && gAction.verifiedOkay.indexOf(self) == nil)
        {
            /* we're already on the floor in the desired posture */
            illogicalNow(alreadyMsg);
        }
        else
        {
            /* 
             *   it's logical, but rank it low in case there's something
             *   more special we can stand/sit/lie on 
             */
            logicalRank(50, 'on floor');

            /* 
             *   note that we've verified okay, so we don't complain on a
             *   future pass if we discover that a precondition has
             *   brought us into compliance with the request prematurely 
             */
            gAction.verifiedOkay += self;
        }
    }

    /* perform sitting/standing/lying on the floor */
    performEntry(newPosture)
    {
        /* 
         *   bring the new posture into effect; there's no need for
         *   actually moving the actor, since the preconditions will have
         *   moved us to our main enclosing room already 
         */
        gActor.makePosture(newPosture);

        /* report success */
        defaultReport(&roomOkayPostureChangeMsg, newPosture, self);
    }

    /* 'stand on floor' causes actor to stand in the containing room */
    dobjFor(StandOn)
    {
        preCond = [touchObj,
                   new ObjectPreCondition(gActor.location.getOutermostRoom(),
                                          actorDirectlyInRoom)]
        verify() { verifyEntry(standing, &alreadyStandingOnMsg); }
        action() { performEntry(standing); }
    }

    /* 'sit on floor' causes the actor to sit in the containing room */
    dobjFor(SitOn)
    {
        preCond = [touchObj,
                   new ObjectPreCondition(gActor.location.getOutermostRoom(),
                                          actorDirectlyInRoom)]
        verify() { verifyEntry(sitting, &alreadySittingOnMsg); }
        action() { performEntry(sitting); }
    }

    /* 'lie on floor' causes the actor to lie down in the room */
    dobjFor(LieOn)
    {
        preCond = [touchObj,
                   new ObjectPreCondition(gActor.location.getOutermostRoom(),
                                          actorDirectlyInRoom)]
        verify() { verifyEntry(lying, &alreadyLyingOnMsg); }
        action() { performEntry(lying); }
    }

    /* 
     *   Mention that an actor is here, as part of a room description.
     *   When the actor is standing, just say that the actor is here, since
     *   it's overstating the obvious to say that the actor is standing on
     *   the floor.  For other postures, do mention the floor.  
     */
    roomActorHereDesc(actor)
    {
        /* 
         *   if we're standing, just say that the actor is "here";
         *   otherwise, say that the actor is sitting/lying/etc on self 
         */
        if (actor.posture == standing)
            gLibMessages.roomActorHereDesc(actor);
        else
            gLibMessages.actorInRoom(actor, self);
    }

    /* 
     *   Mention that an actor is here, as part of a room description.
     *   Since a floor is a trivial part of its enclosing room, there's no
     *   point in mentioning that we're on the floor, as that's stating the
     *   obvious; instead, simply describe the actor as being in the
     *   actor's actual enclosing room.  
     */
    roomActorThereDesc(actor) { actor.location.roomActorThereDesc(actor); }

    /*
     *   Show our room name status for an actor on the floor.  Since
     *   standing on the floor is the trivial default for any room, we
     *   won't bother mentioning it.  Other postures we'll mention the same
     *   way we would for any nested room.  
     */
    roomActorStatus(actor)
    {
        if (actor.posture != standing)
            gLibMessages.actorInRoomStatus(actor, self);
    }

    /* 
     *   Show the actor's posture here.  When we're standing on the floor,
     *   don't mention the posture, as this is too trivial a condition to
     *   state.  Otherwise, mention it as normal for a nested room.  
     */
    roomActorPostureDesc(actor)
    {
        if (actor.posture != standing)
            gLibMessages.actorInRoomPosture(actor, self);
    }

    /* 
     *   Generate an acknowledgment for a posture change here.  If the
     *   actor is standing, just say "okay, you're now standing" without
     *   mentioning the floor, since standing on the floor is the trivial
     *   default.  For other postures, say that we're sitting/lying/etc on
     *   the floor.  
     */
    roomOkayPostureChange(actor)
    {
        if (actor.posture == standing)
            defaultReport(&okayPostureChangeMsg, standing);
        else
            defaultReport(&roomOkayPostureChangeMsg, actor.posture, self);
    }

    /* 
     *   mention the actor as part of the EXAMINE description of a nested
     *   room containing the actor 
     */
    roomListActorPosture(actor)
    {
        /*
         *   Since standing is the default posture for an actor, and since
         *   the floor (or equivalent) is the default place to be standing,
         *   don't bother mentioning actors standing on a floor.
         *   Otherwise, mention that the actor is sitting/lying/etc here.  
         */
        if (actor.posture != standing)
            gLibMessages.actorInRoom(actor, self);
    }
    
    /*
     *   Prefix and suffix messages for listing a group of actors
     *   nominally on the this floor.  Actors are said to be on the floor
     *   when they're really in the location containing the floor.
     *   
     *   If we're talking about a remote location, simply describe it as
     *   the location rather than mentioning the floor, since the floor is
     *   a trivial part of the remote location not worth mentioning.
     *   
     *   If we're local, and we're standing, we'll simply say that we're
     *   "standing here"; again, saying that we're standing on the floor
     *   is stating the obvious.  If we're not standing, we will mention
     *   that we're on the floor.  
     */
    actorInGroupPrefix(pov, posture, remote, lst)
    {
        if (remote != nil)
            gLibMessages.actorThereGroupPrefix(pov, posture, remote, lst);
        else if (posture == standing)
            gLibMessages.actorHereGroupPrefix(posture, lst);
        else
            gLibMessages.actorInGroupPrefix(posture, self, lst);
    }
    actorInGroupSuffix(pov, posture, remote, lst)
    {
        if (remote != nil)
            gLibMessages.actorThereGroupSuffix(pov, posture, remote, lst);
        else if (posture == standing)
            gLibMessages.actorHereGroupSuffix(posture, lst);
        else
            gLibMessages.actorInGroupSuffix(posture, self, lst);
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Define the default room parts. 
 */

/*
 *   the default floor, for indoor locations 
 */
defaultFloor: Floor
;

/*
 *   the default ceiling, for indoor locations 
 */
defaultCeiling: RoomPart
;

/*
 *   The default walls, for indoor locations.  We provide a north, south,
 *   east, and west wall in each indoor location by default. 
 */
class DefaultWall: RoomPart
;

defaultNorthWall: DefaultWall
;

defaultSouthWall: DefaultWall
;

defaultEastWall: DefaultWall
;

defaultWestWall: DefaultWall
;

/*
 *   the default sky, for outdoor locations 
 */
defaultSky: Distant, RoomPart
;

/*
 *   The default ground, for outdoor locations.
 */
defaultGround: Floor
;

/* ------------------------------------------------------------------------ */
/*
 *   A "room part item" is an object that's specially described as being
 *   part of, or attached to, a RoomPart (a wall, ceiling, floor, or the
 *   like).  This is a mix-in class that can be combined with any ordinary
 *   object class (but usually with something non-portable, such as a
 *   Fixture or Immovable).  The effect of adding RoomPartItem to an
 *   object's superclasses is that a command like EXAMINE EAST WALL (or
 *   whichever room part the object is associated with) will display the
 *   object's specialDesc, but a simple LOOK will not.  This class is
 *   sometimes useful for things like doors, windows, ceiling fans, and
 *   other things attached to the room.
 *   
 *   Note that this is a mix-in class, so you should always combine it with
 *   a regular Thing-based class.
 *   
 *   When using this class, you should define two properties in the object:
 *   specialNominalRoomPartLocation, which you should set to the RoomPart
 *   (such as a wall) where the object should be described; and
 *   specialDesc, which is the description to show when the room part is
 *   examined.  Alternatively (or in addition), you can define
 *   initNominalRoomPartLocation and initSpecialDesc - these work the same
 *   way, but will only be in effect until the object is moved.  
 */
class RoomPartItem: object
    /* 
     *   show our special description when examining our associated room
     *   part, as long as we actually define a special description 
     */
    useSpecialDescInRoomPart(part)
    {
        /* only show the special description in our associated room part */
        if (!isNominallyInRoomPart(part))
            return nil;

        /* 
         *   if we define an initial special description, and this is our
         *   nominal room part for that description, use it
         */
        if (isInInitState
            && propType(&initSpecialDesc) != TypeNil
            && initNominalRoomPartLocation == part)
            return true;

        /* likewise for our specialDesc */
        if (propType(&specialDesc) != TypeNil
            && specialNominalRoomPartLocation == part)
            return true;

        /* otherwise, don't use the special description */
        return nil;
    }

    /* 
     *   don't use the special description in room descriptions, or in
     *   examining any other container 
     */
    useSpecialDescInRoom(room) { return nil; }
    useSpecialDescInContents(cont) { return nil; }
;

/* ------------------------------------------------------------------------ */
/*
 *   A Nested Room is any object that isn't a room but which can contain
 *   an actor: chairs, beds, platforms, vehicles, and the like.
 *   
 *   An important property of nested rooms is that they're not
 *   full-fledged rooms for the purposes of actor arrivals and departures.
 *   Specifically, an actor moving from a room to a nested room within the
 *   room does not trigger an actor.travelTo invocation, but simply moves
 *   the actor from the containing room to the nested room.  Moving from
 *   the nested room to the containing room likewise triggers no
 *   actor.travelTo invocation.  The travelTo method is not applicable for
 *   intra-room travel because no TravelConnector objects are traversed in
 *   such travel; we simply move in and out of contained objects.  To
 *   mitigate this loss of notification, we instead call
 *   actor.travelWithin() when moving among nested locations.
 *   
 *   By default, an actor attempting to travel from a nested location via
 *   a directional command will simply attempt the travel as though the
 *   actor were in the enclosing location.  
 */
class NestedRoom: BasicLocation
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
     *   Show our interior room description.  We use this to generate the
     *   long "look" description for the room when an actor is within the
     *   room and cannot see the enclosing room.
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
    roomDesc
    {
        local pov = getPOVActorDefault(gActor);
        pov.listActorPosture(pov);
    }

    /*
     *   The maximum bulk the room can hold.  We'll define this to a large
     *   number by default so that bulk isn't a concern.
     *   
     *   Lower numbers here can be used, for example, to limit the seating
     *   capacity of a chair.  
     */
    bulkCapacity = 10000

    /*
     *   Check for ownership.  For a nested room, an actor can be taken to
     *   own the nested room by virtue of being inside the room - the
     *   chair Bob is sitting in can be called "bob's chair".
     *   
     *   If we don't have an explicit owner, and the potential owner 'obj'
     *   is in me and can own me, we'll report that 'obj' does in fact own
     *   me.  Otherwise, we'll defer to the inherited implementation.  
     */
    isOwnedBy(obj)
    {
        /* 
         *   if we're not explicitly owned, and 'obj' can own me, and
         *   'obj' is inside me, consider us owned by 'obj' 
         */
        if (owner == nil && obj.isIn(self) && obj.canOwn(self))
            return true;

        /* defer to the inherited definition of ownership */
        return inherited(obj);
    }

    /*
     *   Get the extra scope items within this room.  Normally, the
     *   immediately enclosing nested room is in scope for an actor in the
     *   room.  So, if the actor is directly in 'self', return 'self'.  
     */
    getExtraScopeItems(actor)
    {
        /* if the actor is directly in the nested room, return it */
        if (actor.isDirectlyIn(self))
            return [self];
        else
            return [];
    }

    /* 
     *   By default, 'out' within a nested room should take us out of the
     *   nested room itself.  The easy way to accomplish this is to set up
     *   a 'nestedRoomOut' connector for the nested room, which will
     *   automatically try a GET OUT command.  If we didn't do this, we'd
     *   *usually* pick up the noTravelOut from our enclosing room, but
     *   only when the enclosing room didn't override 'out' to point
     *   somewhere else.  Explicitly setting up a 'noTravelOut' here
     *   ensures that we'll consistently GET OUT of the nested room even if
     *   the enclosing room has its own 'out' destination.
     *   
     *   Note that nestedRoomOut shows as a listed exit in exit listings
     *   (for the EXITS command and in the status line).  If you don't want
     *   OUT to be listed as an available exit for the nested room, you
     *   should override this to use noTravelOut instead.  
     */
    out = nestedRoomOut

    /*
     *   An actor is attempting to "get out" while in this location.  By
     *   default, we'll treat this as getting out of this object.  This
     *   can be overridden if "get out" should do something different.  
     */
    disembarkRoom()
    {
        /* run the appropriate command to get out of this nested room */
        removeFromNested();
    }

    /*
     *   Make the actor stand up from this location.  By default, we'll
     *   cause the actor to travel (using travelWithin) to our container,
     *   and assume the appropriate posture for the container.  
     */
    makeStandingUp()
    {
        /* remember the old posture, in case the travel fails */
        local oldPosture = gActor.posture;

        /* get the exit destination; if there isn't one, we can't proceed */
        local dest = exitDestination;
        if (dest == nil)
        {
            reportFailure(&cannotDoFromHereMsg);
            exit;
        }
        
        /* 
         *   Set the actor's posture to the default for the destination.
         *   Do this before effecting the actual travel, so that the
         *   destination can change this default if it wants.  
         */
        gActor.makePosture(dest.defaultPosture);

        /* protect against 'exit' and the like during the travel attempt */
        try
        {
            /* 
             *   move the actor to our exit destination, traveling entirely
             *   within nested locations 
             */
            gActor.travelWithin(dest);
        }
        finally
        {
            /* if we didn't end up traveling, restore the old posture */
            if (gActor.isIn(self))
                gActor.makePosture(oldPosture);
        }

        /* generate the appropriate default for the new location */
        gActor.okayPostureChange();
    }

    /*
     *   Try an implied command to move the actor from outside of this
     *   nested room into this nested room.  This must be overridden in
     *   subclasses to carry out the appropriate implied command.  Returns
     *   the result of tryImplicitAction().
     *   
     *   This is called when we need to move an actor into this location
     *   as part of an implied command.  We use an overridable method
     *   because different kinds of nested rooms have different commands
     *   for entering: SIT ON CHAIR, LIE ON BED, GET IN CAR, RIDE BIKE,
     *   and so on.  This should be normally be overridden imply by
     *   calling tryImplicitAction() with the appropriate command for the
     *   specific type of nested room, and returning the result.  
     */
    tryMovingIntoNested()
    {
        /* do nothing by default - subclasses must override */
        return nil;
    }

    /* 
     *   message property to use for reportFailure when
     *   tryMovingIntoNested fails 
     */
    mustMoveIntoProp = nil

    /*
     *   Try an implied command to remove an actor from this location and
     *   place the actor in my immediate containing location.  This must
     *   be overridden in subclasses to carry out the appropriate implied
     *   command.  Returns the result of tryImplicitAction().
     *   
     *   This is essentially the reverse of tryMovingIntoNested(), and
     *   should in most cases be implemented by calling
     *   tryImplicitAction() with the appropriate command to get out of
     *   the room, and returning the result.  
     */
    tryRemovingFromNested()
    {
        /* do nothing by default - subclasses must override */
        return nil;
    }

    /*
     *   Replace the current action with one that removes the actor from
     *   this nested room.  This is used to implement the GET OUT command
     *   when the actor is directly in this nested room.  In most cases,
     *   this should simply be implemented with a call to replaceAction()
     *   with the appropriate command.  
     */
    removeFromNested()
    {
        /* subclasses must override */
    }

    /*
     *   Try moving the actor into this location.  This is used to move
     *   the actor into this location as part of meeting preconditions,
     *   and we use the normal precondition check protocol: we return nil
     *   if the condition (actor is in this room) is already met; we
     *   return true if we successfully execute an implied command to meet
     *   the condition; and we report a failure message and terminate the
     *   command with 'exit' if we don't know how to meet the condition or
     *   the implied command we try to execute fails or fails to satisfy
     *   the condition.
     *   
     *   This does not normally need to be overridden in subclasses.  
     */
    checkMovingActorInto(allowImplicit)
    {
        /* if the actor is within me, use default handling */
        if (gActor.isIn(self))
            return inherited(allowImplicit);

        /* try an implied command to move the actor into this nested room */
        if (allowImplicit && tryMovingIntoNested())
        {
            /* if we didn't succeed, terminate the command */
            if (!gActor.isDirectlyIn(self))
                exit;

            /* tell the caller we executed an implied command */
            return true;
        }

        /* 
         *   if we can be seen, report that the actor must travel here
         *   first; if we can't be seen, simply say that this can't be done
         *   from here 
         */
        if (gActor.canSee(self))
        {
            /* report that we have to move into 'self' first */
            reportFailure(mustMoveIntoProp, self);
        }
        else
        {
            /* 
             *   we can't be seen; simply say this we can't do this command
             *   from the current location 
             */
            reportFailure(&cannotDoFromHereMsg);
        }

        /* terminate the action */
        exit;
    }

    /*
     *   Check, using pre-condition rules, that the actor is removed from
     *   this nested location and moved to my immediate location.  This is
     *   used to enforce a precondition that the actor is in the enclosing
     *   location.
     *   
     *   This isn't normally overridden in subclasses.  
     */
    checkActorOutOfNested(allowImplicit)
    {
        /* try removing the actor from this nested location */
        if (allowImplicit && tryRemovingFromNested())
        {
            /* 
             *   make sure we managed to move the actor to our exit
             *   destination 
             */
            if (!gActor.isDirectlyIn(exitDestination))
                exit;

            /* indicate that we carried out an implied command */
            return true;
        }

        /* we can't carry out our implied departure plan - fail */
        reportFailure(&cannotDoFromMsg, self);
        exit;
    }

    /*
     *   Check, using pre-condition rules, that the actor is ready to
     *   enter this room as a nested location.
     *   
     *   This isn't normally overridden in subclasses.  
     */
    checkActorReadyToEnterNestedRoom(allowImplicit)
    {
        /* 
         *   If the actor is directly in this room, we obviously need do
         *   nothing, as the actor is already in this nested room.  
         */
        if (gActor.isDirectlyIn(self))
            return nil;

        /*
         *   If the actor isn't within us (directly or indirectly), we
         *   must move the actor to a valid "staging location," so that
         *   the actor can move from the staging location into us.  (A
         *   staging location is simply any location from which we can
         *   move directly into this nested room without any intervening
         *   travel in or out of other nested rooms.)  
         */
        if (!gActor.isIn(self))
            return checkActorInStagingLocation(allowImplicit);

        /*
         *   The actor is within us, but isn't directly within us, so
         *   handle this with the normal routine to move the actor into
         *   this room. 
         */
        return checkMovingActorInto(allowImplicit);
    }

    /*
     *   Check, using precondition rules, that the actor is in a valid
     *   "staging location" for entering this nested room.  We'll ensure
     *   that the actor is directly in one of the locations in our
     *   stagingLocations list, running an appropriate implicit command to
     *   move the actor to the first item in that list if the actor isn't
     *   in any of them.
     *   
     *   This isn't normally overridden in subclasses.
     */
    checkActorInStagingLocation(allowImplicit)
    {
        local lst;
        local target;

        /* get the list of staging locations */
        lst = stagingLocations;

        /* if there are no valid staging locations, we can't move here */
        if (lst.length() == 0)
        {
            cannotMoveActorToStagingLocation();
            exit;
        }
        
        /*
         *   Try each of the locations in our staging list, to see if the
         *   actor is directly in any of them. 
         */
        foreach (local cur in lst)
        {
            /* 
             *   if the actor is directly in this staging location, then
             *   the actor can reach the destination with no additional
             *   intervening travel - simply return nil in this case to
             *   indicate that no implicit commands are needed before the
             *   proposed nested room entry 
             */
            if (gActor.isDirectlyIn(cur))
                return nil;
        }

        /*
         *   The actor isn't directly in any staging location, so we must
         *   move the actor to an appropriate staging location before we
         *   can proceed.  Choose a staging location based on the actor's
         *   current location.  
         */
        if ((target = chooseStagingLocation()) != nil)
        {
            /* 
             *   We've chosen a target staging location.  First, check to
             *   make sure the location we've chosen is valid - we might
             *   have chosen a default (such as the nested room's
             *   immediate container) that isn't usable as a staging
             *   location, so we need to check with it first to make sure
             *   it's willing to allow this.  
             */
            target.checkStagingLocation(self);

            /* 
             *   The check routine didn't abort the command, so try an
             *   appropriate implicit command to move the actor into the
             *   chosen staging location. 
             */
            return target.checkMovingActorInto(allowImplicit);
        }

        /*
         *   There's no apparent intermediate staging location given the
         *   actor's current location.  We thus cannot proceed with the
         *   command; simply report that we can't get there from here.  
         */
        cannotMoveActorToStagingLocation();
        exit;
    }

    /*
     *   Choose an intermediate staging location, given the actor's
     *   current location.  This routine is called when the actor is
     *   attempting to move into 'self', but isn't in any of the allowed
     *   staging locations for 'self'; this routine's purpose is to choose
     *   the staging location that the actor should implicitly try to
     *   reach on the way to 'self'.
     *   
     *   By default, we'll attempt to find the first of our staging
     *   locations that indirectly contains the actor.  (We know none of
     *   the staging locations directly contains the actor, because if one
     *   did, we wouldn't be called in the first place - we're only called
     *   when the actor isn't already directly in one of our staging
     *   locations.)  This approach is appropriate when nested rooms are
     *   related purely by containment: if an actor is in a nested room
     *   within one of our staging locations, we can reach that staging
     *   location by having the actor get out of the more deeply nested
     *   room.
     *   
     *   However, this default approach is not appropriate when nested
     *   rooms are related in some way other than simple containment.  We
     *   don't have any general framework for other types of nested room
     *   relationships, so this routine must be overridden in such a case
     *   with special-purpose code defining the special relationship.
     *   
     *   If we fail to find any staging location indirectly containing the
     *   actor, we'll return the result of defaultStagingLocation().  
     */
    chooseStagingLocation()
    {
        /* look for a staging location indirectly containing the actor */
        foreach (local cur in stagingLocations)
        {
            /* 
             *   if the actor is indirectly in this staging location,
             *   choose it as the target intermediate staging location
             */
            if (gActor.isIn(cur))
                return cur;
        }
            
        /*
         *   We didn't find any locations in the staging list that
         *   indirectly contain the actor, so use the default staging
         *   location.  
         */
        return defaultStagingLocation();
    }

    /*
     *   The default staging location for this nested room.  This is the
     *   staging location we'll attempt to reach implicitly if the actor
     *   isn't in any of the rooms in the stagingLocations list already.
     *   We'll return the first element of our stagingLocations list for
     *   which isStagingLocationKnown returns true.  
     */
    defaultStagingLocation()
    {
        local lst;
        
        /* get the list of valid staging locations */
        lst = stagingLocations;

        /* find the first element which is known to the actor */
        foreach (local cur in lst)
        {
            /* if this staging location is known, take it as the default */
            if (isStagingLocationKnown(cur))
                return cur;
        }

        /* we didn't find any known staging locations - there's no default */
        return nil;
    }

    /*
     *   Report that we are unable to move an actor to any staging
     *   location for this nested room.  By default, we'll generate the
     *   message "you can't do that from here," but this can overridden to
     *   provide a more specific if desired.  
     */
    cannotMoveActorToStagingLocation()
    {
        /* report the standard "you can't do that from here" message */
        reportFailure(&cannotDoFromHereMsg);
    }

    /*
     *   Report that we are unable to move an actor out of this nested
     *   room, because there's no valid 'exit destination'.  This is
     *   called when we attempt to GET OUT OF the nested room, and the
     *   'exitDestination' property is nil.  
     */
    cannotMoveActorOutOf()
    {
        /* report the standard "you can't do that from here" message */
        reportFailure(&cannotDoFromHereMsg);
    }
    

    /*
     *   The valid "staging locations" for this nested room.  This is a
     *   list of the rooms from which an actor can DIRECTLY reach this
     *   nested room; in other words, the actor will be allowed to enter
     *   'self', with no intervening travel, if the actor is directly in
     *   any of these locations.
     *   
     *   If the list is empty, there are no valid staging locations.
     *   
     *   The point of listing staging locations is to make certain that
     *   the actor has to go through one of these locations in order to
     *   get into this nested room.  This ensures that we enforce any
     *   conditions or trigger any side effects of moving through the
     *   staging locations, so that a player can't bypass a puzzle by
     *   trying to move directly from one location to another without
     *   going through the required intermediate steps.  Since we always
     *   require that an actor go through one of our staging locations in
     *   order to enter this nested room, and since we carry out the
     *   travel to the staging location using implied commands (which are
     *   just ordinary commands, entered and executed automatically by the
     *   parser), we can avoid having to code any checks redudantly in
     *   both the staging locations and any other nearby locations.
     *   
     *   By default, an actor can only enter a nested room from the room's
     *   direct container.  For example, if a chair is on a stage, an
     *   actor must be standing on the stage before the actor can sit on
     *   the chair.  
     */
    stagingLocations = [location]

    /*
     *   Our exit destination.  This is where an actor ends up when the
     *   actor is immediately inside this nested room and uses a "get out
     *   of" or equivalent command to exit the nested room.
     *   
     *   By default, we'll use the default staging location as the exit
     *   destination.  
     */
    exitDestination = (defaultStagingLocation())

    /*
     *   Is the given staging location "known"?  This returns true if the
     *   staging location is usable as a default, nil if not.  If this
     *   returns true, then the location can be used in an implied command
     *   to move the actor to the staging location in order to move the
     *   actor into self.
     *   
     *   If this returns nil, no implied command will be attempted for
     *   this possible staging location.  This doesn't mean that an actor
     *   gets a free pass through the staging location; on the contrary,
     *   it simply means that we won't try any automatic command to move
     *   an actor to the staging location, hence travel from a non-staging
     *   location to this nested room will simply fail.  This can be used
     *   when part of the puzzle is to figure out that moving to the
     *   staging location is required in the first place: if we allowed an
     *   implied command in such cases, we'd give away the puzzle by
     *   solving it automatically.
     *   
     *   By default, we'll treat all of our staging locations as known.  
     */
    isStagingLocationKnown(loc) { return true; }

    /*
     *   Get the travel preconditions for an actor in this location.  By
     *   default, if we have a container, and the actor can see the
     *   container, we'll return its travel preconditions; otherwise, we'll
     *   use our inherited preconditions.  
     */
    roomTravelPreCond()
    {
        local ret;

        /* 
         *   If we can see out to our location, use the location's
         *   conditions, since by default we'll try traveling from the
         *   location; if we can't see out to our location, we won't be
         *   attempting travel through our location's connectors, so use
         *   our own preconditions instead. 
         */
        if (location != nil && gActor.canSee(location))
            ret = location.roomTravelPreCond();
        else
            ret = inherited();

        /* return the results */
        return ret;
    }

    /*
     *   We cannot take a nested room that the actor is occupying 
     */
    dobjFor(Take)
    {
        verify()
        {
            /* it's illogical to take something that contains the actor */
            if (gActor.isIn(self))
                illogicalNow(&cannotTakeLocationMsg);

            /* inherit the default handling */
            inherited();
        }
    }

    /*
     *   "get out of" action - exit the nested room
     */
    dobjFor(GetOutOf)
    {
        preCond()
        {
            return [new ObjectPreCondition(self, actorDirectlyInRoom)];
        }
        verify()
        {
            /* 
             *   the actor must be located on the platform; but allow the
             *   actor to be indirectly on the platform, since we'll use a
             *   precondition to move the actor out of any more nested
             *   rooms within us 
             */
            if (!gActor.isIn(self))
                illogicalNow(&notOnPlatformMsg);
        }
        check()
        {
            /* 
             *   If we have no 'exit destination' - that is, we have
             *   nowhere to go when we GET OUT OF the nested room - then
             *   prohibit the operation. 
             */
            if (exitDestination == nil)
            {
                /* explain the problem and terminate the command */
                cannotMoveActorOutOf();
                exit;
            }
        }
        action()
        {
            /* travel to our get-out-of destination */
            gActor.travelWithin(exitDestination);

            /* 
             *   set the actor's posture to the default posture for the
             *   new location 
             */
            gActor.makePosture(gActor.location.defaultPosture);

            /* issue a default report of the change */
            defaultReport(&okayNotStandingOnMsg);
        }
    }

    /* explicitly define the push-travel indirect object mappings */
    mapPushTravelIobj(PushTravelGetOutOf, TravelVia)
;


/* ------------------------------------------------------------------------ */
/*
 *   A "high nested room" is a nested room that is elevated above the rest
 *   of the room.  This specializes the staging location handling so that
 *   it generates more specific messages.  
 */
class HighNestedRoom: NestedRoom
    /* report that we're unable to move to a staging location */
    cannotMoveActorToStagingLocation()
    {
        reportFailure(&nestedRoomTooHighMsg, self);
    }

    /* if we can't get out, report that it's because we're too high up */
    cannotMoveActorOutOf()
    {
        reportFailure(&nestedRoomTooHighToExitMsg, self);
    }

    /*
     *   Staging locations.  By default, we'll return an empty list,
     *   because a high location is not usually reachable directly from its
     *   containing location.
     *   
     *   Note that puzzles involving moving platforms will have to manage
     *   this list dynamically, which could be done either by writing a
     *   method here that returns a list of currently valid staging
     *   locations, or by adding objects to this list as they become valid
     *   staging locations and removing them when they cease to be.  For
     *   example, if we have an air vent in the ceiling that we can only
     *   reach when a chair is placed under the vent, this property could
     *   be implemented as a method that returns a list containing the
     *   chair only when the chair is in the under-the-vent state.
     *   
     *   Note that this empty default setting will also give us no exit
     *   destination, since the default exit location is the default
     *   staging location.  
     */
    stagingLocations = []
;


/* ------------------------------------------------------------------------ */
/*
 *   A chair is an item that an actor can sit on.  When an actor is sitting
 *   on a chair, the chair contains the actor.  In addition to sitting,
 *   chairs can optionally allow standing as well.
 *   
 *   We define the "BasicChair" as something that an actor can sit on, and
 *   then subclass this with the standard "Chair", which adds surface
 *   capabilities.  
 */
class BasicChair: NestedRoom
    /*
     *   A list of the allowed postures for this object.  By default, we
     *   can sit and stand on a chair, since most ordinary chairs are
     *   suitable for both.  
     */
    allowedPostures = [sitting, standing]

    /*
     *   A list of the obvious postures for this object.  The only obvious,
     *   default thing you do with most ordinary chairs is sit on them,
     *   even they allow other postures.  Something like a large sofa might
     *   want to allow both sitting and lying.
     *   
     *   This list differs from the allowed postures list because some
     *   postures might be possible but not probable.  For most ordinary
     *   chairs, standing is possible, but it's not the first thing you'd
     *   think of doing with the chair.  
     */
    obviousPostures = [sitting]

    /*
     *   A chair's effective follow location is usually its location's
     *   effective follow location, because we don't usually want to treat
     *   a chair as a separate location for the purposes of "follow."
     *   That is, if A and B are in the same room, and A sits down on a
     *   chair in the room, we don't want to count this as a move that B
     *   could follow.  
     */
    effectiveFollowLocation = (location.effectiveFollowLocation)

    /*
     *   Try an implied command to move the actor from outside of this
     *   nested room into this nested room.  By default, we'll call upon
     *   our default posture object to activate its command to move the
     *   actor into this object in the default posture.  For a chair, the
     *   default posture is typically sitting, so the 'sitting' posture
     *   will perform a SIT ON <self> command.  
     */
    tryMovingIntoNested()
    {
        /* 
         *   ask our default posture object to carry out the appropriate
         *   command to move the actor into 'self' in that posture 
         */
        return defaultPosture.tryMakingPosture(self);
    }

    /* tryMovingIntoNested failure message is "must sit on chair" */
    mustMoveIntoProp = &mustSitOnMsg

    /* default posture in this nested room is sitting */
    defaultPosture = sitting

    /* 
     *   by default, objects dropped while sitting in a chair go into the
     *   enclosing location's drop destination 
     */
    getDropDestination(obj, path)
    {
        return location != nil
            ? location.getDropDestination(obj, path)
            : self;
    }

    /*
     *   Remove an actor from the chair.  By default, we'll simply stand
     *   up, since this is the normal way out of a chair.  
     */
    tryRemovingFromNested()
    {
        /* try standing up */
        if (gActor.posture == sitting)
            return tryImplicitAction(Stand);
        else
            return tryImplicitAction(GetOffOf, self);
    }

    /*
     *   Run the appropriate command to remove us from this nested
     *   container, as a replacement command. 
     */
    removeFromNested()
    {
        /* to get out of a chair, we simply stand up */
        if (gActor.posture == sitting)
            replaceAction(Stand);
        else
            replaceAction(GetOutOf, self);
    }

    /*
     *   "sit on" action 
     */
    dobjFor(SitOn)
    {
        preCond = (preCondForEntry(sitting))
        verify()
        {
            /* verify entering the chair in a 'sitting' posture */
            if (verifyEntry(sitting, &alreadySittingOnMsg, &noRoomToSitMsg))
                inherited();
        }
        action()
        {
            /* enter the chair in the 'sitting' posture */
            performEntry(sitting);
        }
    }

    /*
     *   "stand on" action 
     */
    dobjFor(StandOn)
    {
        /* if it's allowed, use the same preconditions as 'sit on' */
        preCond = (preCondForEntry(standing))

        verify()
        {
            /* verify entering in a 'standing' posture */
            if (verifyEntry(standing, &alreadyStandingOnMsg,
                            &noRoomToStandMsg))
                inherited();
        }
        action()
        {
            /* enter the chair in the 'standing' posture */
            performEntry(standing);
        }
    }

    /*
     *   "lie on" action 
     */
    dobjFor(LieOn)
    {
        preCond = (preCondForEntry(lying))
        verify()
        {
            /* verify entering in a 'lying' posture */
            if (verifyEntry(lying, &alreadyLyingOnMsg, &noRoomToLieMsg))
                inherited();
        }
        action()
        {
            /* enter in the 'lying' posture */
            performEntry(lying);
        }
    }

    /* 
     *   For "get on/in" / "board", let our default posture object handle
     *   it, by running the appropriate nested action that moves the actor
     *   into self in the default posture. 
     */
    dobjFor(Board)
    {
        verify() { }
        action() { defaultPosture.setActorToPosture(gActor, self); }
    }

    /* "get off of" is the same as "get out of" */
    dobjFor(GetOffOf) asDobjFor(GetOutOf)

    /* standard preconditions for sitting/lying/standing on the chair */
    preCondForEntry(posture)
    {
        /* 
         *   if this is not among my allowed postures, we don't need any
         *   special preconditions, since we'll fail in the verify 
         */
        if (allowedPostures.indexOf(posture) == nil)
            return inherited();

        /* 
         *   in order to enter the chair, we have to be able to touch it
         *   and we have to be able to enter it as a nested room 
         */
        return [touchObj,
                new ObjectPreCondition(self, actorReadyToEnterNestedRoom)];
    }

    /*
     *   Verify that we can enter the chair in the given posture.  This
     *   performs verification work common to SIT ON, LIE ON, and STAND ON.
     *   If this returns true, the caller should inherit the base class
     *   default handling, otherwise it shouldn't.  
     */
    verifyEntry(posture, alreadyMsg, noRoomMsg)
    {
        /* 
         *   if the given posture isn't allowed, tell the caller to use the
         *   inherited default handling 
         */
        if (allowedPostures.indexOf(posture) == nil)
            return true;

        /* this posture is allowed, but it might not be obvious */
        if (obviousPostures.indexOf(posture) == nil)
            nonObvious;

        /* 
         *   If the actor is already on this chair in the given posture,
         *   this action is redundant.  If we already verified okay on this
         *   point for this same action, ignore the repeated command - it
         *   must mean that we applied a precondition that did all of our
         *   work for us (such as moving us out of a nested room
         *   immediately within us).  
         */
        if (gActor.posture == posture && gActor.isDirectlyIn(self)
            && gAction.verifiedOkay.indexOf(self) == nil)
            illogicalNow(alreadyMsg);
        else
            gAction.verifiedOkay += self;

        /*
         *   If there's not room for the actor's added bulk, don't allow
         *   the actor to sit/lie/stand on the chair.  If the actor is
         *   already within the chair, there's no need to add the actor's
         *   bulk for this change, since it's already counted as being
         *   within us.  
         */
        if (!gActor.isIn(self)
            && getBulkWithin() + gActor.getBulk() > bulkCapacity)
            illogicalNow(noRoomMsg);

        /* we can't sit/stand/lie on something the actor is holding */
        if (isIn(gActor))
            illogicalNow(&cannotEnterHeldMsg);

        /* 
         *   if the actor is already in me, but in a different posture,
         *   boost the likelihood slightly 
         */
        if (gActor.isDirectlyIn(self) && gActor.posture != posture)
            logicalRank(120, 'already in');

        /* tell the caller we don't want to inherit the base class handling */
        return nil;
    }

    /*
     *   Perform entry in the given posture.  This carries out the common
     *   actions for SIT ON, LIE ON, and STAND ON. 
     */
    performEntry(posture)
    {
        /* 
         *   Move the actor into me - this counts as interior travel within
         *   the enclosing room.  Note that we move the actor before
         *   changing the actor's posture in case the travel fails.  
         */
        gActor.travelWithin(self);
            
        /* set the actor to the desired posture */
        gActor.makePosture(posture);

        /* report success */
        defaultReport(&roomOkayPostureChangeMsg, posture, self);
    }
;

/*
 *   A Chair is a basic chair with the addition of being a Surface.
 */
class Chair: BasicChair, Surface
    /*
     *   By default, a chair has a seating capacity of one person, so use
     *   a maximum bulk that only allows one actor to occupy the chair at
     *   a time.  
     */
    bulkCapacity = 10
;

/*
 *   Bed.  This is an extension of Chair that allows actors to lie on it
 *   as well as sit on it.  As with chairs, we have a basic bed, plus a
 *   regular bed that serves as a surface as well.  
 */
class BasicBed: BasicChair
    /* 
     *   we can sit, lie, and stand on a typical bed, but only sitting and
     *   lying are obvious default actions 
     */
    allowedPostures = [sitting, lying, standing]
    obviousPostures = [sitting, lying]

    /* tryMovingIntoNested failure message is "must sit on chair" */
    mustMoveIntoProp = &mustLieOnMsg

    /* default posture in this nested room is sitting */
    defaultPosture = lying
;

/*
 *   A Bed is a basic bed with the addition of Surface capabilities. 
 */
class Bed: BasicBed, Surface
;

/*
 *   A Platform is a nested room upon which an actor can stand.  In
 *   general, when you can stand on something, you can also sit and lie on
 *   it as well (it might not be comfortable, but it is usually at least
 *   possible), so we make this a subclass of Bed.
 *   
 *   The main difference between a platform and a chair that allows
 *   standing is that a platform is more of a mini-room.  In particular,
 *   items an actor drops while standing on a platform land on the platform
 *   itself, whereas items dropped while sitting (or standing) on a chair
 *   land in the enclosing room.  In addition, the obvious default action
 *   for a chair is to sit on it, while the obvious default action for a
 *   platform is to stand on it.  
 */
class BasicPlatform: BasicBed
    /* 
     *   we can sit, lie, and stand on a typical platform, and all of
     *   these could be reasonably expected to be done
     */
    allowedPostures = [sitting, lying, standing]
    obviousPostures = [sitting, lying, standing]

    /* an actor can follow another actor onto or off of a platform */
    effectiveFollowLocation = (self)

    /* tryMovingIntoNested failure message is "must get on platform" */
    mustMoveIntoProp = &mustGetOnMsg

    /* default posture in this nested room is sitting */
    defaultPosture = standing

    /* by default, objects dropped on a platform go onto the platform */
    getDropDestination(obj, path)
    {
        return self;
    }

    /*
     *   Remove an actor from the platform.  "Get off" is the normal
     *   command to leave a platform.  
     */
    tryRemovingFromNested()
    {
        /* try getting off of the platform */
        return tryImplicitAction(GetOffOf, self);
    }

    /*
     *   Replace the current action with one that removes the actor from
     *   this nested room.
     */
    removeFromNested()
    {
        /* get off of the platform */
        replaceAction(GetOffOf, self);
    }

    /*
     *   Make the actor stand up.  On a platform, standing is normally
     *   allowed, so STAND doesn't usually imply "get off platform" as it
     *   does in the base class.  
     */
    makeStandingUp()
    {
        /* 
         *   If standing isn't among my allowed postures, inherit the
         *   default behavior, which is to get out of the nested room. 
         */
        if (allowedPostures.indexOf(standing) == nil)
        {
            /* we can't stand on the platform, so use the default handling */
            inherited();
        }
        else
        {
            /* we can stand on the platform, so make the actor stand */
            gActor.makePosture(standing);
            
            /* issue a default report of the change */
            defaultReport(&roomOkayPostureChangeMsg, standing, self);
        }
    }

    /*
     *   Traveling 'down' from a platform should generally be taken to
     *   mean 'get off platform'. 
     */
    down = noTravelDown
;

/*
 *   A Platform is a basic platform with the addition of Surface behavior. 
 */
class Platform: BasicPlatform, Surface
;

/*
 *   A "nominal platform" is a named place where NPC's can stand.  This
 *   class makes it easy to arrange for an NPC to be described as standing
 *   in a particular location in the room: for example, we could have an
 *   actor "standing in a doorway", or "leaning against the streetlamp."
 *   
 *   In most cases, a nominal platform is a "secret" object, in that it
 *   won't be listed in a room's contents and it won't have any vocabulary
 *   words.  So, the player will never be able to refer to the object in a
 *   command.  
 *   
 *   To use this class, instantiate it with an object located in the room
 *   containing the pseudo-platform.  Don't give the object any vocabulary
 *   words.  Locate actors within the pseudo-platform to give them the
 *   special description.
 *   
 *   For simple platform-like "standing on" descriptions, just define the
 *   name.  For descriptions like "standing in", "standing under", or
 *   "standing near", where only the preposition needs to be customized,
 *   define the name and define actorInPrep.  For more elaborate
 *   customizations, such as "leaning against the streetlamp", you'll need
 *   to override roomActorHereDesc, roomActorStatus, roomActorPostureDesc,
 *   roomListActorPosture, and actorInGroupPrefix/Suffix, 
 */
class NominalPlatform: Fixture, Platform
    /* don't let anyone stand/sit/lie here via a command */
    dobjFor(StandOn) { verify() { illogical(&cannotStandOnMsg); } }
    dobjFor(SitOn) { verify() { illogical(&cannotSitOnMsg); } }
    dobjFor(LieOn) { verify() { illogical(&cannotLieOnMsg); } }

    /* ignore me for 'all' and object defaulting */
    hideFromAll(action) { return true; }
    hideFromDefault(action) { return true; }

    /* 
     *   nominal platforms are internal objects only, not part of the
     *   visible game world structure, so treat them as equivalent to their
     *   location for FOLLOW purposes 
     */
    effectiveFollowLocation = (location.effectiveFollowLocation)
;

/*
 *   A booth is a nested room that serves as a small enclosure within a
 *   larger room.  Booths can serve as regular containers as well as
 *   nested rooms, and can be made openable by addition of the Openable
 *   mix-in class.  Note that booths don't have to be fully enclosed, nor
 *   do they actually have to be closable.
 *   
 *   Examples of booths: a cardboard box large enough for an actor can
 *   stand in; a closet; a shallow pit.  
 */
class Booth: BasicPlatform, Container
    /*
     *   Try an implied command to move the actor from outside of this
     *   nested room into this nested room.
     */
    tryMovingIntoNested()
    {
        /* try getting in me */
        return tryImplicitAction(Board, self);
    }

    /*
     *   Remove an actor from the booth.  "Get out" is the normal command
     *   to leave this type of room.  
     */
    tryRemovingFromNested()
    {
        /* try getting out of the object */
        return tryImplicitAction(GetOutOf, self);
    }

    /*
     *   Replace the current action with one that removes the actor from
     *   this nested room.
     */
    removeFromNested()
    {
        /* get out of the object */
        replaceAction(GetOutOf, self);
    }

    /*
     *   "Enter" is equivalent to "get in" (or "board") for a booth 
     */
    dobjFor(Enter) asDobjFor(Board)

    /* explicitly define the push-travel indirect object mapping */
    mapPushTravelIobj(PushTravelEnter, Board)
;

/* ------------------------------------------------------------------------ */
/*
 *   A Vehicle is a special type of nested room that moves instead of the
 *   actor in response to travel commands.  When an actor in a vehicle
 *   types, for example, "go north," the vehicle moves north, not the
 *   actor.
 *   
 *   In most cases, a Vehicle should multiply inherit from one of the
 *   other nested room subclasses to make it more specialized.  For
 *   example, a bicycle might inherit from Chair, so that actors can sit
 *   on the bike.
 *   
 *   Note that because Vehicle inherits from NestedRoom, the OUT direction
 *   in the vehicle by default means what it does in NestedRoom -
 *   specifically, getting out of the vehicle.  This is appropriate for
 *   vehicles where we'd describe passengers as being inside the vehicle,
 *   such as a car or a boat.  However, if the vehicle is something you
 *   ride on, like a horse or a bike, it's probably more appropriate for
 *   OUT to mean "ride the vehicle out of the enclosing room."  To get
 *   this effect, simply override the "out" property and set it to nil;
 *   this will prevent the NestedRoom definition from being inherited,
 *   which will make us look for the OUT location of the enclosing room as
 *   the travel destination.  
 */
class Vehicle: NestedRoom, Traveler
    /*
     *   When a traveler is in a vehicle, and the traveler performs a
     *   travel command, the vehicle is what changes location; the
     *   contained traveler simply stays put while the vehicle moves.  
     */
    getLocTraveler(trav, conn)
    {
        local stage;
        
        /*
         *   If the connector is contained within the vehicle, take it as
         *   leading out of the vehicle - this means we want to move
         *   traveler within the vehicle rather than the vehicle.
         *   Consider the connector be inside the vehicle if it's a
         *   physical object (a Thing) that's inside the vehicle, or one
         *   of the vehicle's own directional links points directly to the
         *   connector.
         *   
         *   Likewise, if the connector is marked with the vehicle or any
         *   object inside the vehicle as its staging location, the
         *   vehicle obviously isn't involved in the travel.  
         */
        if ((conn != nil && conn.ofKind(Thing) && conn.isIn(self))
            || Direction.allDirections.indexWhich(
                {dir: self.(dir.dirProp) == conn}) != nil
            || ((stage = conn.connectorStagingLocation) != nil
                && (stage == self || stage.isIn(self))))
        {
            /* 
             *   this connector leads out from within the vehicle - move
             *   the inner traveler rather than the vehicle itself 
             */
            return trav;
        }

        /* 
         *   If we have a location, ask it who travels when the VEHICLE is
         *   the traveler within it; otherwise, the vehicle is the
         *   traveler.
         *   
         *   We ask the location, because the location might itself be a
         *   vehicle, in which case it might want us to be driving around
         *   the enclosing vehicle.  However, we pass ourselves (i.e.,
         *   this vehicle) as the inner traveler, rather than the traveler
         *   we were passed, because a traveler within a vehicle moves the
         *   vehicle when traveling.  
         */
        return (location != nil ? location.getLocTraveler(self, conn) : self);
    }

    /*
     *   An OUT command while within a vehicle could mean one of two
     *   things: either to GET OUT of the vehicle, or to ride/drive the
     *   vehicle out of its enclosing location.
     *   
     *   There's no good way of guessing which meaning the player intends,
     *   so we have to choose one or the other.  We choose the ride/drive
     *   interpretation as the default, for two reasons.  First, it seems
     *   to be what most players expect.  Second, the other interpretation
     *   leaves no standard way of expressing the ride/drive meaning.  We
     *   return nil here to indicate to the OUT action that we want the
     *   enclosing location's 'out' connector to be used while an actor is
     *   in the vehicle.
     *   
     *   For some vehicles, it might be more appropriate for OUT to mean
     *   GET OUT.  In these cases, simply override this so that it returns
     *   nestedRoomOut.  
     */
    out = nil

    /*
     *   Get the "location push traveler" - this is the traveler when a
     *   push-travel command is performed by a traveler within this
     *   location.  If the object we're trying to push is within me, use
     *   the contained traveler, since the contained traveler must be
     *   trying to push the object around directly.  If the object isn't
     *   inside me, then we're presumably trying to use the vehicle to push
     *   around the object, so the traveler is the vehicle or something
     *   containing the vehicle.  
     */
    getLocPushTraveler(trav, obj)
    {
        /* 
         *   If the object is inside me, use the nested traveler;
         *   otherwise, we're presumably trying to use the vehicle to move
         *   the object. 
         */
        if (obj.isIn(self))
        {
            /* 
             *   we're moving something around inside me; use the
             *   contained traveler 
             */
            return trav;
        }
        else if (location != nil)
        {
            /* 
             *   we're pushing something around outside me, so we're
             *   probably trying to use the vehicle to do so; we have a
             *   location, so ask it what it thinks, passing myself as the
             *   new suggested traveler 
             */
            return location.getLocPushTraveler(self, obj);
        }
        else
        {
            /* 
             *   we're pushing something around outside me, and I have no
             *   location, so I must be the traveler 
             */
            return self;
        }
    }

    /* 
     *   Determine if an actor is traveling with me.  The normal base
     *   class implementation works, but it's more efficient just to check
     *   to see if the actor is inside this object than to construct the
     *   entire nested contents list just to check to see if the actor's
     *   in that list. 
     */
    isActorTraveling(actor) { return actor.isIn(self); }

    /* invoke a callback for each actor traveling with us */
    forEachTravelingActor(func)
    {
        /* invoke the callback on each actor in our contents */
        allContents().forEach(function(obj) {
            if (obj.isActor)
                (func)(obj);
        });
    }

    /* 
     *   Get the actors involved in the travel.  This is a list consisting
     *   of all of the actors contained within the vehicle. 
     */
    getTravelerActors = (allContents().subset({x: x.isActor}))

    /* 
     *   there are no self-motive actors in a vehicle - the vehicle is
     *   doing the travel, and the actors within are just moving along
     *   with it as cargo 
     */
    getTravelerMotiveActors = []

    /*
     *   Traveler preconditions for the vehicle.  By default, we add no
     *   preconditions of our own, but specific vehicles might want to
     *   override this.  For example, a car might want to require that the
     *   doors are closed, the engine is running, and the seatbelts are
     *   fastened before it can travel.  
     */
    travelerPreCond(conn) { return []; }

    /*
     *   Check, using pre-condition rules, that the traveler is in the
     *   given room, moving the traveler to the room if possible. 
     */
    checkMovingTravelerInto(room, allowImplicit)
    {
        /* if we're in the desired location, we're set */
        if (isDirectlyIn(room))
            return nil;

        /* 
         *   By default, we can't move a vehicle into a room implicitly.
         *   Individual vehicles can override this when there's an obvious
         *   way of moving the vehicle in and out of nested rooms.  
         */
        reportFailure(&vehicleCannotDoFromMsg, self);
        exit;
    }

    /* 
     *   the lister object we use to display the list of actors aboard, in
     *   arrival and departure messages for the vehicle 
     */
    aboardVehicleListerObj = aboardVehicleLister
;

/*
 *   A VehicleBarrier is a TravelConnector that allows actors to travel,
 *   but blocks vehicles.  By default, we block all vehicles, but
 *   subclasses can customize this so that we block only specific
 *   vehicles. 
 */
class VehicleBarrier: TravelBarrier
    /*
     *   Determine if the given traveler can pass through this connector.
     *   By default, we'll return nil for a Vehicle, true for anything
     *   else.  This can be overridden to allow specific vehicles to pass,
     *   or to filter on any other criteria.  
     */
    canTravelerPass(traveler) { return !traveler.ofKind(Vehicle); }

    /* explain why we can't pass */
    explainTravelBarrier(traveler)
    {
        reportFailure(&cannotGoThatWayInVehicleMsg, traveler);
    }
;

