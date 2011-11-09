#charset "us-ascii"

/* 
 *   Copyright (c) 2002, 2006 by Michael J. Roberts
 *   
 *   Based on exitslister.t, copyright 2002 by Steve Breslin and
 *   incorporated by permission.  
 *   
 *   TADS 3 Library - Exits Lister
 *   
 *   This module provides an automatic exit lister that shows the apparent
 *   exits from the player character's location.  The automatic exit lister
 *   can optionally provide these main features:
 *   
 *   - An "exits" verb lets the player explicitly show the list of apparent
 *   exits, along with the name of the room to which each exit connects.
 *   
 *   - Exits can be shown automatically as part of the room description.
 *   This extra information can be controlled by the player through the
 *   "exits on" and "exits off" command.
 *   
 *   - Exits can be shown automatically when an actor tries to go in a
 *   direction where no exit exists, as a helpful reminder of which
 *   directions are valid.  
 */

/* include the library header */
#include "adv3.h"


/* ------------------------------------------------------------------------ */
/*
 *   The main exits lister.
 */
exitLister: PreinitObject
    /* preinitialization */
    execute()
    {
        /* install myself as the global exit lister object */
        gExitLister = self;
    }
    
    /*
     *   Flag: use "verbose" listing style for exit lists in room
     *   descriptions.  When this is set to true, we'll show a
     *   sentence-style list of exits ("Obvious exits lead east to the
     *   living room, south, and up.").  When this is set to nil, we'll use
     *   a terse style, enclosing the message in the default system
     *   message's brackets ("[Obvious exits: East, West]").
     *   
     *   Verbose-style room descriptions tend to fit well with a room
     *   description's prose, but at the expense of looking redundant with
     *   the exit list that's usually built into each room's custom
     *   descriptive text to begin with.  Some authors prefer the terse
     *   style precisely because it doesn't look like more prose
     *   description, but looks like a separate bit of information being
     *   offered.
     *   
     *   This is an author-configured setting; the library does not provide
     *   a command to let the player control this setting.  
     */
    roomDescVerbose = nil

    /* 
     *   Flag: show automatic exit listings on attempts to move in
     *   directions that don't allow travel.  Enable this by default,
     *   since most players appreciate having the exit list called out
     *   separately from the room description (where any mention of exits
     *   might be buried in lots of other text) in place of an unspecific
     *   "you can't go that way".  
     *   
     *   This is an author-configured setting; the library does not provide
     *   a command to let the player control this setting.  
     */
    enableReminder = true

    /*
     *   Flag: enable the automatic exit reminder even when the room
     *   description exit listing is enabled.  When this is nil, we will
     *   NOT show a reminder with "can't go that way" messages when the
     *   room description exit list is enabled - this is the default,
     *   because it can be a little much to have the list of exits shown so
     *   frequently.  Some authors might prefer to show the reminder
     *   unconditionally, though, so this option is offered.  
     *   
     *   This is an author-configured setting; the library does not provide
     *   a command to let the player control this setting.  
     */
    enableReminderAlways = nil

    /*
     *   Flag: use hyperlinks in the directions mentioned in room
     *   description exit lists, so that players can click on the direction
     *   name in the listing to enter the direction command. 
     */
    enableHyperlinks = true

    /* flag: we've explained how the exits on/off command works */
    exitsOnOffExplained = nil

    /*
     *   Determine if the "reminder" is enabled.  The reminder is the list
     *   of exits we show along with a "can't go that way" message, to
     *   reminder the player of the valid exits when an invalid one is
     *   attempted.  
     */
    isReminderEnabled()
    {
        /*   
         *   The reminder is enabled if enableReminderAlways is true, OR if
         *   enableReminder is true AND exitsMode.inRoomDesc is nil.  
         */
        return (enableReminderAlways
                || (enableReminder && !exitsMode.inRoomDesc));
    }

    /*
     *   Get the exit lister we use for room descriptions. 
     */
    getRoomDescLister()
    {
        /* use the verbose or terse lister, according to the configuration */
        return roomDescVerbose
            ? lookAroundExitLister
            : lookAroundTerseExitLister;
    }
    
    /* perform the "exits" command to show exits on explicit request */
    showExitsCommand()
    {
        /* show exits for the current actor */
        showExits(gActor);

        /* 
         *   if we haven't explained how to turn exit listing on and off,
         *   do so now 
         */
        if (!exitsOnOffExplained)
        {
            gLibMessages.explainExitsOnOff;
            exitsOnOffExplained = true;
        }
    }

    /* 
     *   Perform an EXITS ON/OFF/STATUS/LOOK command.  'stat' indicates
     *   whether we're turning on (true) or off (nil) the statusline exit
     *   listing; 'look' indicates whether we're turning the room
     *   description listing on or off. 
     */
    exitsOnOffCommand(stat, look)
    {
        /* set the new status */
        exitsMode.inStatusLine = stat;
        exitsMode.inRoomDesc = look;

        /* confirm the new status */
        gLibMessages.exitsOnOffOkay(stat, look);

        /* 
         *   If we haven't already explained how the EXITS ON/OFF command
         *   works, don't bother explaining it now, since they obviously
         *   know how it works if they've actually used it.  
         */
        exitsOnOffExplained = true;
    }
    
    /* show the list of exits from an actor's current location */
    showExits(actor)
    {
        /* show exits from the actor's location */
        showExitsFrom(actor, actor.location);
    }

    /* show an exit list display in the status line, if desired */
    showStatuslineExits()
    {
        /* if statusline exit displays are enabled, show the exit list */
        if (exitsMode.inStatusLine)
            showExitsWithLister(gPlayerChar, gPlayerChar.location,
                                statuslineExitLister,
                                gPlayerChar.location
                                .wouldBeLitFor(gPlayerChar));
    }

    /* 
     *   Calculate the contribution of the exits list to the height of the
     *   status line, in lines of text.  If we're not configured to display
     *   the exits list in the status line, then the contribution is zero;
     *   otherwise, we'll estimate how much space we need to display the
     *   exit list.  
     */
    getStatuslineExitsHeight()
    {
        /* 
         *   if we're enabled, our standard display takes up one line; if
         *   we're disabled, we don't contribute anything to the status
         *   line's vertical extent 
         */
        if (exitsMode.inStatusLine)
            return 1;
        else
            return 0;
    }

    /* show exits as part of a room description */
    lookAroundShowExits(actor, loc, illum)
    {
        /* if room exit displays are enabled, show the exits */
        if (exitsMode.inRoomDesc)
            showExitsWithLister(actor, loc, getRoomDescLister, illum);
    }

    /* show exits as part of a "cannot go that way" error */
    cannotGoShowExits(actor, loc)
    {
        /* if we want to show the reminder, show it */
        if (isReminderEnabled())
            showExitsWithLister(actor, loc, explicitExitLister,
                                loc.wouldBeLitFor(actor));
    }

    /* show the list of exits from a given location for a given actor */
    showExitsFrom(actor, loc)
    {
        /* show exits with our standard lister */
        showExitsWithLister(actor, loc, explicitExitLister,
                            loc.wouldBeLitFor(actor));
    }

    /* 
     *   Show the list of exits using a specific lister.
     *   
     *   'actor' is the actor for whom the display is being generated.
     *   'loc' is the location whose exit list is to be shown; this need
     *   not be the same as the actor's current location.  'lister' is the
     *   Lister object that will show the list of DestInfo objects that we
     *   create to represent the exit list.
     *   
     *   'locIsLit' indicates whether or not the ambient illumination, for
     *   the actor's visual senses, is sufficient that the actor would be
     *   able to see if the actor were in the new location.  We take this
     *   as a parameter so that we don't have to re-compute the
     *   information if the caller has already computed it for other
     *   reasons (such as showing a room description).  If the caller
     *   hasn't otherwise computed the value, it can be easily computed as
     *   loc.wouldBeLitFor(actor).  
     */
    showExitsWithLister(actor, loc, lister, locIsLit)
    {
        local destList;
        local showDest;
        local options;

        /* 
         *   Ask the lister if it shows the destination names.  We need to
         *   know because we want to consolidate exits that go to the same
         *   place if and only if we're going to show the destination in
         *   the listing; if we're not showing the destination, there's no
         *   reason to consolidate. 
         */
        showDest = lister.listerShowsDest;

        /* we have no option flags for the lister yet */
        options = 0;

        /* run through all of the directions used in the game */
        destList = new Vector(Direction.allDirections.length());
        foreach (local dir in Direction.allDirections)
        {
            local conn;
            
            /* 
             *   If the actor's location has a connector in this
             *   direction, and the connector is apparent, add it to the
             *   list.
             *   
             *   If the actor is in the dark, we can only see the
             *   connector if the connector is visible in the dark.  If
             *   the actor isn't in the dark, we can show all of the
             *   connectors.  
             */
            if ((conn = loc.getTravelConnector(dir, actor)) != nil
                && conn.isConnectorApparent(loc, actor)
                && conn.isConnectorListed
                && (locIsLit || conn.isConnectorVisibleInDark(loc, actor)))
            {
                local dest;
                local destName = nil;
                local destIsBack;

                /* 
                 *   We have an apparent connection in this direction, so
                 *   add it to our list.  First, check to see if we know
                 *   the destination. 
                 */
                dest = conn.getApparentDestination(loc, actor);

                /* note if this is the "back to" connector for the actor */
                destIsBack = (conn == actor.lastTravelBack);

                /* 
                 *   If we know the destination, and they want to include
                 *   destination names where possible, get the name.  If
                 *   there's a name to show, include the name.  
                 */
                if (dest != nil
                    && showDest
                    && (destName = dest.getDestName(actor, loc)) != nil)
                {
                    local orig;

                    /* 
                     *   we are going to show a destination name for this
                     *   item, so set the special option flag to let the
                     *   lister know that this is the case 
                     */
                    options |= ExitLister.hasDestNameFlag;

                    /* 
                     *   if this is the back-to connector, note that we
                     *   know the name of the back-to location 
                     */
                    if (destIsBack)
                        options |= ExitLister.hasBackNameFlag;
                    
                    /* 
                     *   If this destination name already appears in the
                     *   list, don't include this one in the list.
                     *   Instead, add this direction to the 'others' list
                     *   for the existing entry, so that we will show each
                     *   known destination only once. 
                     */
                    orig = destList.valWhich({x: x.dest_ == dest});
                    if (orig != nil)
                    {
                        /* 
                         *   this same destination name is already present
                         *   - add this direction to the existing entry's
                         *   list of other directions going to the same
                         *   place 
                         */
                        orig.others_ += dir;

                        /* 
                         *   if this is the back-to connector, note it in
                         *   the original destination item 
                         */
                        if (destIsBack)
                            orig.destIsBack_ = true;

                        /* 
                         *   don't add this direction to the main list,
                         *   since we don't want to list the destination
                         *   redundantly 
                         */
                        continue;
                    }
                }

                /* add it to our list */
                destList.append(new DestInfo(dir, dest, destName,
                                             destIsBack));
            }
        }

        /* show the list */
        lister.showListAll(destList.toList(), options, 0);
    }
;

/*
 *   A destination tracker.  This keeps track of a direction and the
 *   apparent destination in that direction. 
 */
class DestInfo: object
    construct(dir, dest, destName, destIsBack)
    {
        /* remember the direction, destination, and destination name */
        dir_ = dir;
        dest_ = dest;
        destName_ = destName;
        destIsBack_ = destIsBack;
    }

    /* the direction of travel */
    dir_ = nil

    /* the destination room object */
    dest_ = nil

    /* the name of the destination */
    destName_ = nil

    /* flag: this is the "back to" destination */
    destIsBack_ = nil

    /* list of other directions that go to our same destination */
    others_ = []
;

/*
 *   Settings item - show defaults in status line 
 */
exitsMode: SettingsItem
    /* our ID */
    settingID = 'adv3.exits'

    /* show our description */
    settingDesc =
        (gLibMessages.currentExitsSettings(inStatusLine, inRoomDesc))

    /* convert to text */
    settingToText()
    {
        /* just return the two binary variables */
        return (inStatusLine ? 'on' : 'off')
            + ','
            + (inRoomDesc ? 'on' : 'off');
    }

    settingFromText(str)
    {
        /* parse out our format */
        if (rexMatch('<space>*(<alpha>+)<space>*,<space>*(<alpha>+)',
                     str.toLower()) != nil)
        {
            /* pull out the two variables from the regexp groups */
            inStatusLine = (rexGroup(1)[3] == 'on');
            inRoomDesc = (rexGroup(2)[3] == 'on');
        }
    }

    /* 
     *   Our value is in two parts.  inStatusLine controls whether or not
     *   we show the exit list in the status line; inRoomDesc controls the
     *   exit listing in room descriptions.  
     */
    inStatusLine = true
    inRoomDesc = nil
;

