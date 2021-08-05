#charset "us-ascii"

/* 
 *   Copyright (c) 2000, 2006 Michael J. Roberts.  All Rights Reserved. 
 *.  Portions based on work by Kevin Forchione, used by permission.  
 *   
 *   TADS 3 Library - objects
 *   
 *   This module defines the basic physical simulation objects (apart from
 *   Thing, the base class for most game objects, which is so large that
 *   it's defined in its own separate module for convenience).  We define
 *   such basic classes as containers, surfaces, fixed-in-place objects,
 *   openables, and lockables.  
 */

/* include the library header */
#include "adv3.h"


/* ------------------------------------------------------------------------ */
/*
 *   LocateInParent - this is a mix-in superclass that defines the location
 *   of the object as the object's lexical parent.  This is useful for
 *   nested object definitions where the next object should be located
 *   within the enclosing object.
 *   
 *   When this class is mixed with Thing or its subclasses, LocateInParent
 *   should go first, so that the location we define here takes precedence.
 */
class LocateInParent: object
    location = (lexicalParent)
;

/* ------------------------------------------------------------------------ */
/*
 *   Intangible - this is an object that represents something that can be
 *   sensed but which has no tangible existence, such as a ray of light, a
 *   sound, or an odor. 
 */
class Intangible: Thing
    /*
     *   The base intangible object has no presence in any sense,
     *   including sight.  Subclasses should override these as appropriate
     *   for the senses in which the object can be sensed.  
     */
    sightPresence = nil
    soundPresence = nil
    smellPresence = nil
    touchPresence = nil

    /* intangibles aren't included in regular room/inventory/contents lists */
    isListed = nil
    isListedInInventory = nil
    isListedInContents = nil

    /* hide intangibles from 'all' for all actions by default */
    hideFromAll(action) { return true; }

    /* don't hide from defaults, though */
    hideFromDefault(action) { return nil; }

    /*
     *   Essentially all verbs are meaningless on intangibles.  Each
     *   subclass should re-enable verbs that are meaningful for that
     *   specific type of intangible; to re-enable an action, just define
     *   a verify() handler for the action.
     *   
     *   Note that the verbs we handle via the Default handlers have no
     *   preconditions; since these verbs don't do anything anyway,
     *   there's no need to apply any preconditions to them.  
     */
    dobjFor(Default)
    {
        preCond = []
        verify() { illogical(&notWithIntangibleMsg, self); }
    }
    iobjFor(Default)
    {
        preCond = []
        verify() { illogical(&notWithIntangibleMsg, self); }
    }
;

/*
 *   A "vaporous" object is a visible but intangible object: something
 *   visible, and possibly with an odor and a sound, but not something that
 *   can be touched or otherwise physically manipulated.  Fire, smoke, and
 *   fog are examples of this kind of object.  
 */
class Vaporous: Intangible
    /* we have a sight presence */
    sightPresence = true

    /* 
     *   EXAMINE ALL, LISTEN TO ALL, and SMELL ALL apply to us, but hide
     *   from ALL for other actions, as not much else makes sense on us 
     */
    hideFromAll(action)
    {
        return !(action.ofKind(ExamineAction)
                 || action.ofKind(ListenToAction)
                 || action.ofKind(SmellAction));
    }

    /* 
     *   We can examine, smell, and listen to these objects, as normal for
     *   any Thing.  To make these verbs work as normal for Thing, we need
     *   to explicitly override the corresponding verifiers, so that we
     *   bypass the dobjFor(Default) verifier in Intangible.  We don't need
     *   to do anything special in the overrides, so just inherit the
     *   default handling; what's important is that we do override the
     *   methods at all. 
     */
    dobjFor(Examine) { verify() { inherited(); } }
    dobjFor(Smell) { verify() { inherited(); } }
    dobjFor(ListenTo) { verify() { inherited(); } }

    /* 
     *   look in, look through, look behind, look under, search: since
     *   vaporous objects are usually essentially transparent, these
     *   commands reveal nothing interesting 
     */
    lookInDesc { mainReport(&lookInVaporousMsg, self); }

    /* 
     *   downgrade the likelihood of these slightly, and map everything to
     *   LOOK IN 
     */
    dobjFor(LookIn) { verify() { logicalRank(70, 'look in vaporous'); } }
    dobjFor(LookThrough) asDobjFor(LookIn)
    dobjFor(LookBehind) asDobjFor(LookIn)
    dobjFor(LookUnder) asDobjFor(LookIn)
    dobjFor(Search) asDobjFor(LookIn)

    /* the message we display for commands we disallow */
    notWithIntangibleMsg = &notWithVaporousMsg
;


/*
 *   A sensory emanation.  This is an intangible object that represents a
 *   sound, odor, or the like. 
 */
class SensoryEmanation: Intangible
    /*
     *   Are we currently emanating our sensory information?  This can be
     *   used as an on/off switch to control when we're active.  
     */
    isEmanating = true
    
    /* 
     *   The description shown when the *source* is examined (with "listen
     *   to", "smell", or whatever verb is appropriate to the type of sense
     *   the subclass involves).  This will also be appended to the regular
     *   "examine" description, if we're not marked as ambient.  
     */
    sourceDesc = ""

    /* our description, with and without being able to see the source */
    descWithSource = ""
    descWithoutSource = ""

    /* 
     *   Our "I am here" message, with and without being able to see the
     *   source.  These are displayed in room descriptions, inventory
     *   descriptions, and by the daemon that schedules background messages
     *   for sensory emanations.
     *   
     *   If different messages are desired as the emanation is mentioned
     *   repeatedly while the emanation remains continuously within sense
     *   range of the player character ("A phone is ringing", "The phone is
     *   still ringing", etc), you can do one of two things.  The easier
     *   way is to use a Script object; each time we need to show a
     *   message, we'll invoke the script.  The other way, which is more
     *   manual but gives you greater control, is to write a method that
     *   checks the displayCount property of self to determine which
     *   iteration of the message is being shown.  displayCount is set to 1
     *   the first time a message is displayed for the object when the
     *   object can first be sensed, and is incremented each we invoke one
     *   of these display routines.  Note that displayCount resets to nil
     *   when the object leaves sense scope, so the sequence of messages
     *   will automatically start over each time the object comes back into
     *   scope.
     *   
     *   The manual way (writing a method that checks the displayCount)
     *   might be desirable if you want the emanation to fade into the
     *   background gradually as the player character stays in the same
     *   location repeatedly.  This mimics human perception: we notice a
     *   noise or odor most when we first hear it, but if it continues for
     *   an extended period without changing, we'll eventually stop
     *   noticing it.  
     */
    hereWithSource = ""
    hereWithoutSource = ""

    /* 
     *   A message to display when the emanation ceases to be within sense
     *   range.  In most cases, this displays nothing at all, but some
     *   emanations might want to note explicitly when the noise/etc
     *   stops.
     */
    noLongerHere = ""

    /*
     *   Flag: I'm an "ambient" emanation.  This means we essentially are
     *   part of the background, and are not worth mentioning in our own
     *   right.  If this is set to true, then we won't mention this
     *   emanation at all when it first becomes reachable in its sense.
     *   This should be used for background noises and the like: we won't
     *   ever make an unsolicited mention of them, but they'll still show
     *   up in explicit 'listen' commands and so on.  
     */
    isAmbient = nil

    /*
     *   The schedule for displaying messages about the emanation.  This
     *   is a list of intervals between messages, in game clock times.
     *   When the player character can repeatedly sense this emanation for
     *   multiple consecutive turns, we'll use this schedule to display
     *   messages periodically about the noise/odor/etc.
     *   
     *   Human sensory perception tends to be "edge-sensitive," which
     *   means that we tend to perceive sensory input most acutely when
     *   something changes.  When a sound or odor is continually present
     *   without variation for an extended period, it tends to fade into
     *   the background of our awareness, so that even though it remains
     *   audible, we gradually stop noticing it.  This message display
     *   schedule mechanism is meant to approximate this perceptual model
     *   by allowing the sensory emanation to specify how noticeable the
     *   emanation remains during continuous exposure.  Typically, a
     *   continuous emanation would have relatively frequent messages
     *   (every two turns, say) for a couple of iterations, then would
     *   switch to infrequent messages.  Emanations that are analogous to
     *   white noise would probably not be mentioned at all after the
     *   first couple of messages, because the human senses are especially
     *   given to treating such input as background.
     *   
     *   We use this list by applying each interval in the list once and
     *   then moving to the next entry in the list.  The first entry in
     *   the list is the interval between first sensing the emanation and
     *   displaying the first "still here" message.  When we reach the end
     *   of the list, we simply repeat the last interval in the list
     *   indefinitely.  If the last entry in the list is nil, though, we
     *   simply never produce another message.  
     */
    displaySchedule = [nil]

    /*
     *   Show our "I am here" description.  This is the description shown
     *   as part of our room's description.  We show our hereWithSource or
     *   hereWithoutSource message, according to whether or not we can see
     *   the source object.  
     */
    emanationHereDesc()
    {
        local actor;
        local prop;

        /* if we're not currently emanating, there's nothing to do */
        if (!isEmanating)
            return;
        
        /* note that we're mentioning the emanation */
        noteDisplay();

        /* 
         *   get the actor driving the description - if there's a command
         *   active, use the command's actor; otherwise use the player
         *   character
         */
        if ((actor = gActor) == nil)
            actor = gPlayerChar;

        /* our display varies according to our source's visibility */
        prop = (canSeeSource(actor) ? &hereWithSource : &hereWithoutSource);

        /* 
         *   if it's a Script object, invoke the script; otherwise, just
         *   invoke the property 
         */
        if (propType(prop) == TypeObject && self.(prop).ofKind(Script))
            self.(prop).doScript();
        else
            self.(prop);
    }

    /*
     *   Show a message describing that we cannot see the source of this
     *   emanation because the given obstructor is in the way.  This
     *   should be overridden for each subclass. 
     */
    cannotSeeSource(obs) { }

    /* 
     *   Get the source of the noise/odor/whatever, as perceived by the
     *   current actor.  This is the object we appear to be coming from.
     *   By default, an emanation is generated by its direct container,
     *   and by default this is apparent to actors, so we'll simply return
     *   our direct container.
     *   
     *   If the source is not apparent, this should simply return nil.  
     */
    getSource() { return location; }

    /* determine if our source is apparent and visible */
    canSeeSource(actor)
    {
        local src;
        
        /* get our source */
        src = getSource();

        /* 
         *   return true if we have an apparent source, and the apparent
         *   source is visible to the current actor 
         */
        return src != nil && actor.canSee(src);
    }

    /*
     *   Note that we're displaying a message about the emanation.  This
     *   method should be called any time a message about the emanation is
     *   displayed, either by an explicit action or by our background
     *   daemon.
     *   
     *   We'll adjust our next display time so that we wait the full
     *   interval at the current point in the display schedule before we
     *   show any background message about this object.  Note we do not
     *   advance through the schedule list; instead, we merely delay any
     *   further message by the interval at the current point in the
     *   schedule list.  
     */
    noteDisplay()
    {
        /* calculate our next display time */
        calcNextDisplayTime();

        /* count the display */
        if (displayCount == nil)
            displayCount = 1;
        else
            ++displayCount;
    }

    /*
     *   Note an indirect message about the emanation.  This can be used
     *   when we don't actually display a message ourselves, but another
     *   object (usually our source object) describes the emanation; for
     *   example, if our source object mentions the noise it's making when
     *   it is examined, it should call this method to let us know we have
     *   been described indirectly.  This method advances our next display
     *   time, just as noteDisplay() does, but this method doesn't count
     *   the display as a direct display. 
     */
    noteIndirectDisplay()
    {
        /* calculate our next display time */
        calcNextDisplayTime();
    }
    
    /*
     *   Begin the emanation.  This is called from the sense change daemon
     *   when the item first becomes noticeable to the player character -
     *   for example, when the player character first enters the room
     *   containing the emanation, or when the emanation is first
     *   activated.  
     */
    startEmanation()
    {
        /* if we're an ambient emanation only, don't mention it */
        if (isAmbient)
            return;

        /* 
         *   if we've already initialized our scheduling, we must have
         *   been explicitly mentioned, such as by a room description - in
         *   this case, act as though we're continuing our emanation 
         */
        if (scheduleIndex != nil)
        {
            continueEmanation();
            return;
        }

        /* show our message */
        emanationHereDesc;
    }

    /*
     *   Continue the emanation.  This is called on each turn in which the
     *   emanation remains continuously within sense range of the player
     *   character.  
     */
    continueEmanation()
    {
        /* 
         *   if we are not to run again, our next display time will be set
         *   to zero - do nothing in this case 
         */
        if (nextDisplayTime == 0 || nextDisplayTime == nil)
            return;

        /* if we haven't yet reached our next display time, do nothing */
        if (Schedulable.gameClockTime < nextDisplayTime)
            return;

        /* 
         *   Advance to the next schedule interval, if we have one.  If
         *   we're already on the last schedule entry, simply repeat it
         *   forever. 
         */
        if (scheduleIndex < displaySchedule.length())
            ++scheduleIndex;

        /* show our description */
        emanationHereDesc;
    }

    /*
     *   End the emanation.  This is called when the player character can
     *   no longer sense the emanation. 
     */
    endEmanation()
    {
        /* show our "no longer here" message */
        noLongerHere;

        /* uninitialize the display scheduling */
        scheduleIndex = nil;
        nextDisplayTime = nil;

        /* reset the display count */
        displayCount = nil;
    }

    /*
     *   Calculate our next display time.  The caller must set our
     *   scheduleIndex to the correct index prior to calling this.  
     */
    calcNextDisplayTime()
    {
        local delta;

        /* if our scheduling isn't initialized, set it up now */
        if (scheduleIndex == nil)
        {
            /* start at the first display schedule interval */
            scheduleIndex = 1;
        }
        
        /* get the next display interval from the schedule list */
        delta = displaySchedule[scheduleIndex];

        /* 
         *   if the current display interval is nil, it means that we're
         *   never to display another message 
         */
        if (delta == nil)
        {
            /* 
             *   we're not to display again - simply set the next display
             *   time to zero and return 
             */
            nextDisplayTime = 0;
            return;
        }

        /* 
         *   our next display time is the current game clock time plus the
         *   interval 
         */
        nextDisplayTime = Schedulable.gameClockTime + delta;
    }

    /*
     *   Internal counters that keep track of our display scheduling.
     *   scheduleIndex is the index in the displaySchedule list of the
     *   interval we're waiting to expire; nextDisplayTime is the game
     *   clock time of our next display.  noiseList and odorList are lists
     *   of senseInfo entries for the sound and smell senses,
     *   respectively, indicating which objects were within sense range on
     *   the last turn.  displayCount is the number of times in a row
     *   we've displayed a message already.  
     */
    scheduleIndex = nil
    nextDisplayTime = nil
    noiseList = nil
    odorList = nil
    displayCount = nil

    /*
     *   Class method implementing the sensory change daemon.  This runs
     *   on each turn to check for changes in the set of objects the
     *   player can hear and smell, and to generate "still here" messages
     *   for objects continuously within sense range for multiple turns.  
     */
    noteSenseChanges()
    {
        /* emanations don't change anything, so turn on caching */
        libGlobal.enableSenseCache();

        /* note sound changes */
        noteSenseChangesFor(sound, &noiseList, Noise);

        /* note odor changes */
        noteSenseChangesFor(smell, &odorList, Odor);

        /* done with sense caching */
        libGlobal.disableSenseCache();
    }

    /*
     *   Note sense changes for a particular sense.  'listProp' is the
     *   property of SensoryEmanation giving the list of SenseInfo entries
     *   for the sense on the previous turn.  'sub' is a subclass of ours
     *   (such as Noise) giving the type of sensory emanation used for
     *   this sense. 
     */
    noteSenseChangesFor(sense, listProp, sub)
    {
        local newInfo;
        local oldInfo;

        /* get the old table of SenseInfo entries for the sense */
        oldInfo = self.(listProp);

        /* 
         *   Get the new table of items we can reach in the given sense,
         *   and reduce it to include only emanations of the subclass of
         *   interest.  
         */
        newInfo = gPlayerChar.senseInfoTable(sense);
        newInfo.forEachAssoc(function(obj, info)
        {
            /* 
             *   remove this item if it's not of the subclass of interest,
             *   or if it's not currently emanating 
             */
            if (!obj.ofKind(sub) || !obj.isEmanating)
                newInfo.removeElement(obj);
        });

        /* run through the new list and note each change */
        newInfo.forEachAssoc(function(obj, info)
        {
            /* treat this as a new command visually */
            "<.commandsep>";
        
            /* 
             *   Check to see whether the item is starting anew or was
             *   already here on the last turn.  If the item was in our
             *   list from the previous turn, it was already here.  
             */
            if (oldInfo == nil || oldInfo[obj] == nil)
            {
                /* 
                 *   the item wasn't in sense range on the last turn, so
                 *   it is becoming newly noticeable 
                 */
                obj.startEmanation();
            }
            else
            {
                /* the item was already here - continue its emanation */
                obj.continueEmanation();
            }
        });

        /* run through the old list and note each item no longer sensed */
        if (oldInfo != nil)
        {
            oldInfo.forEachAssoc(function(obj, info)
            {
                /* if this item isn't in the new list, note its departure */
                if (newInfo[obj] == nil)
                {
                    /* treat this as a new command visually */
                    "<.commandsep>";
                    
                    /* note the departure */
                    obj.endEmanation();
                }
            });
        }

        /* store the current list for comparison the next time we run */
        self.(listProp) = newInfo;
    }

    /* 
     *   Examine the sensory emanation.  We'll show our descWithSource or
     *   descWithoutSource, according to whether or not we can see the
     *   source object. 
     */
    dobjFor(Examine)
    {
        verify() { inherited(); }
        action()
        {
            /* note that we're displaying a message about us */
            noteDisplay();
            
            /* display our sound description */
            if (canSeeSource(gActor))
            {
                /* we can see the source */
                descWithSource;
            }            
            else
            {
                local src;
            
                /* show the unseen-source version of the description */
                descWithoutSource;

                /* 
                 *   If we have a source, find out what's keeping us from
                 *   seeing the source; in other words, find the opaque
                 *   visual obstructor on the sense path to the source.  
                 */
                if ((src = getSource()) != nil)
                {
                    local obs;
                    
                    /* get the visual obstructor */
                    obs = gActor.findVisualObstructor(src);
                    
                    /* 
                     *   If we found an obstructor, and we can see it, add
                     *   a message describing the obstruction.  If we
                     *   can't see the obstructor, we can't localize the
                     *   sensory emanation at all.  
                     */
                    if (obs != nil && gActor.canSee(obs))
                        cannotSeeSource(obs);
                }
            }
        }
    }
;

/*
 *   Noise - this is an intangible object representing a sound.
 *   
 *   A Noise object is generally placed directly within the object that is
 *   generating the noise.  This will ensure that the noise is
 *   automatically in scope whenever the object is in scope (or, more
 *   precisely, whenever the object's contents are in scope) and with the
 *   same sense attributes.
 *   
 *   By default, when a noise is specifically examined via "listen to",
 *   and the container is visible, we'll mention that the noise is coming
 *   from the container.
 */
class Noise: SensoryEmanation
    /* 
     *   by default, we have a definite presence in the sound sense if
     *   we're emanating our noise
     */
    soundPresence = (isEmanating)

    /* 
     *   By default, a noise is listed in a room description (i.e., on LOOK
     *   or entry to a room) unless it's an ambient background noise..  Set
     *   this to nil to omit the noise from the room description, while
     *   still allowing it to be heard in an explicit LISTEN command.  
     */
    isSoundListedInRoom = (!isAmbient && isEmanating)

    /* show our description as part of a room description */
    soundHereDesc() { emanationHereDesc(); }

    /* explain that we can't see the source because of the obstructor */
    cannotSeeSource(obs) { obs.cannotSeeSoundSource(self); }

    /* treat "listen to" the same as "examine" */
    dobjFor(ListenTo) asDobjFor(Examine)

    /* "examine" requires that the object is audible */
    dobjFor(Examine)
    {
        preCond = [objAudible]
    }
;

/*
 *   Odor - this is an intangible object representing an odor. 
 */
class Odor: SensoryEmanation
    /* 
     *   by default, we have a definite presence in the smell sense if
     *   we're currently emanating our odor 
     */
    smellPresence = (isEmanating)

    /* 
     *   By default, an odor is listed in a room description (i.e., on LOOK
     *   or entry to a room) unless it's an ambient background odor.  Set
     *   this to nil to omit the odor from the room description, while
     *   still allowing it to be listed in an explicit SMELL command.  
     */
    isSmellListedInRoom = (!isAmbient && isEmanating)

    /* mention the odor as part of a room description */
    smellHereDesc() { emanationHereDesc(); }

    /* explain that we can't see the source because of the obstructor */
    cannotSeeSource(obs) { obs.cannotSeeSmellSource(self); }

    /* handle "smell" using our "examine" handler */
    dobjFor(Smell) asDobjFor(Examine)

    /* "examine" requires that the object is smellable */
    dobjFor(Examine)
    {
        preCond = [objSmellable]
    }
;

/*
 *   SimpleNoise is for cases where a noise is an ongoing part of a
 *   location, so (1) it's not necessary to distinguish source and
 *   sourceless versions of the description, and (2) there are no
 *   scheduled reports for the noise.  For these cases, all of the
 *   messages default to the basic 'desc' property.  Note that we make
 *   this type of noise "ambient" by default, which means that we won't
 *   automatically include it in room descriptions.  
 */
class SimpleNoise: Noise
    isAmbient = true
    sourceDesc { desc; }
    descWithSource { desc; }
    descWithoutSource { desc; }
    hereWithSource { desc; }
    hereWithoutSource { desc; }
;

/* SimpleOdor is the olfactory equivalent of SimpleNoise */
class SimpleOdor: Odor
    isAmbient = true
    sourceDesc { desc; }
    descWithSource { desc; }
    descWithoutSource { desc; }
    hereWithSource { desc; }
    hereWithoutSource { desc; }
;

/* ------------------------------------------------------------------------ */
/*
 *   Sensory Event.  This is an object representing a transient event,
 *   such as a sound, visual display, or odor, to which some objects
 *   observing the event might react.
 *   
 *   A sensory event differs from a sensory emanation in that an emanation
 *   is ongoing and passive, while an event is isolated in time and
 *   actively notifies observers.  
 */
class SensoryEvent: object
    /* 
     *   Trigger the event.  This routine must be called at the time when
     *   the event is to occur.  We'll notify every interested observer
     *   capable of sensing the event that the event is occurring, so
     *   observers can take appropriate action in response to the event.
     *   
     *   'source' is the source object - this is the physical object in
     *   the simulation that is causing the event.  For example, if the
     *   event is the sound of a phone ringing, the phone would probably
     *   be the source object.  The source is used to determine which
     *   observers are capable of detecting the event: an observer must be
     *   able to sense the source object in the appropriate sense to be
     *   notified of the event.  
     */
    triggerEvent(source)
    {
        /* 
         *   Run through all objects connected to the source object by
         *   containment, and notify any that are interested and can
         *   detect the event.  Containment is the only way sense
         *   information can propagate, so we can limit our search
         *   accordingly.
         *   
         *   Connection by containment is no guarantee of a sense
         *   connection: it's a necessary, but not sufficient, condition.
         *   Because it's a necessary condition, though, we can use it to
         *   limit the number of objects we have to test with a more
         *   expensive sense path calculation.  
         */
        source.connectionTable().forEachAssoc(function(cur, val)
        {
            /* 
             *   If this object defines the observer notification method,
             *   then it might be interested in the event.  If the object
             *   doesn't define this method, then there's no way it could
             *   be interested.  (We make this test before checking the
             *   sense path because checking to see if an object defines a
             *   property is fast and simple, while the sense path
             *   calculation could be expensive.) 
             */
            if (cur.propDefined(notifyProp, PropDefAny))
            {
                local info;
                
                /* 
                 *   This object might be interested in the event, so
                 *   check to see if the object can sense the event.  If
                 *   this object can sense the source object at all (i.e.,
                 *   the sense path isn't 'opaque'), then notify the
                 *   object of the event.  
                 */
                info = cur.senseObj(sense, source);
                if (info.trans != opaque)
                {
                    /* 
                     *   this observer object can sense the source of the
                     *   event, so notify it of the event 
                     */
                    cur.(notifyProp)(self, source, info);
                }
            }
        });
    }

    /* the sense in which the event is observable */
    sense = nil

    /* 
     *   the notification property - this is the property we'll invoke on
     *   each observer to notify it of the event 
     */
    notifyProp = nil
;

/*
 *   Visual event 
 */
class SightEvent: SensoryEvent
    sense = sight
    notifyProp = &notifySightEvent
;

/* 
 *   Visual event observer.  This is a mix-in that can be added to any
 *   other classes.  
 */
class SightObserver: object
    /*
     *   Receive notification of a sight event.  This routine is called
     *   whenever a SightEvent occurs within view of this object.
     *   
     *   'event' is the SightEvent object; 'source' is the physical
     *   simulation object that is making the visual display; and 'info'
     *   is a SenseInfo object describing the viewing conditions from this
     *   object to the source object.  
     */
    notifySightEvent(event, source, info) { }
;

/*
 *   Sound event 
 */
class SoundEvent: SensoryEvent
    sense = sound
    notifyProp = &notifySoundEvent
;

/* 
 *   Sound event observer.  This is a mix-in that can be added to any
 *   other classes.  
 */
class SoundObserver: object
    /*
     *   Receive notification of a sound event.  This routine is called
     *   whenever a SoundEvent occurs within hearing range of this object.
     */
    notifySoundEvent(event, source, info) { }
;

/*
 *   Smell event 
 */
class SmellEvent: SensoryEvent
    sense = smell
    notifyProp = &notifySmellEvent
;

/* 
 *   Smell event observer.  This is a mix-in that can be added to any
 *   other classes.  
 */
class SmellObserver: object
    /*
     *   Receive notification of a smell event.  This routine is called
     *   whenever a SmellEvent occurs within smelling range of this
     *   object.  
     */
    notifySmellEvent(event, source, info) { }
;


/* ------------------------------------------------------------------------ */
/*
 *   Hidden - this is an object that's present but not visible to any
 *   actors.  The object will simply not be visible in the 'sight' sense
 *   until discovered. 
 */
class Hidden: Thing
    /* we can't be seen until discovered */
    canBeSensed(sense, trans, ambient)
    {
        /* 
         *   If the sense is sight, and we haven't been discovered yet, we
         *   cannot be sensed.  Otherwise, inherit the normal handling. 
         */
        if (sense == sight && !discovered)
            return nil;
        else
            return inherited(sense, trans, ambient);
    }

    /* 
     *   Have we been discovered yet?
     *   
     *   Note that this should be a simple property value, not a method.
     *   It's risky to make this a method because it's evaluated from
     *   within some of the low-level scope/sense calculations, and those
     *   calculations depend upon certain global variables.  If you make
     *   this property into a method, you could indirectly call another
     *   method that changes some of the same globals, which could disrupt
     *   the main scope/sense calculations and cause other, seemingly
     *   unrelated objects to mysteriously appear or disappear at the wrong
     *   times.  If you need to calculate this value dynamically, you could
     *   explicitly assign the property a new value in something like a
     *   daemon or an afterAction() method.
     *   
     *   (The warning above is a bit more conservative than is strictly
     *   necessary.  It actually is safe to make 'discovered' a method,
     *   *provided* that the method doesn't ever call anything that's
     *   involved in the scope/sense calculations.  For example, never call
     *   methods like senseObj(), senseAmbientMax(), or
     *   sensePresenceList(), or anything that calls those.  In most cases,
     *   it's safe to call non-sense-related methods, like isOpen() or
     *   isIn().)  
     */
    discovered = nil

    /* mark the object as discovered */
    discover()
    {
        local pc;
        
        /* note that we've been discovered */
        discovered = true;

        /* mark me and my contents as having been seen */
        if ((pc = gPlayerChar).canSee(self))
        {
            /* mark me as seen */
            pc.setHasSeen(self);

            /* mark my visible contents as see */
            setContentsSeenBy(pc.visibleInfoTable(), pc);
        }
    }
;
 

/* ------------------------------------------------------------------------ */
/*
 *   Collective - this is an object that can be used to refer to a group of
 *   other (usually equivalent) objects collectively.  In most cases, this
 *   object will be a separate game object that contains or can contain the
 *   individuals: a bag of marbles can be a collective for the marbles, or
 *   a book of matches can be a collective for the matchsticks.
 *   
 *   A collective object is usually given the same plural vocabulary as its
 *   individuals.  When we use that plural vocabulary, we will filter for
 *   or against the collective, as determined by the noun phrase
 *   production, when the player uses the collective term.
 *   
 *   This is a mix-in class, intended to be used along with other (usually
 *   Thing-derived) superclasses.  
 */
class Collective: object
    filterResolveList(lst, action, whichObj, np, requiredNum)
    {
        /* scan for my matching individuals */
        foreach (local cur in lst)
        {
            /* if this one's a matching individual, decide what to do */
            if (isCollectiveFor(cur.obj_))
            {
                /* 
                 *   We're a collective for this object.  If the noun
                 *   phrase production wants us to filter for collectives,
                 *   remove the individual and keep me (the collective);
                 *   otherwise, keep the individual and remove me. 
                 */
                if (np.filterForCollectives)
                {
                    /* 
                     *   we want to keep the collective, so remove this
                     *   individual item 
                     */
                    lst -= cur;
                }
                else
                {
                    /* 
                     *   we want to keep individuals, so remove the
                     *   collective (i.e., myself) 
                     */
                    lst -= lst.valWhich({x: x.obj_ == self});

                    /* 
                     *   we can only be in the list once, so there's no
                     *   need to keep looking - if we found another item
                     *   for which we're a collective, all we'd do is try
                     *   to remove myself again, which would be pointless
                     *   since I'm already gone 
                     */
                    break;
                }
            }
        }

        /* return the result */
        return lst;
    }

    /*
     *   Determine if I'm a collective object for the given object.
     *   
     *   In order to be a collective for some objects, an object must have
     *   vocubulary for the plural name, and must return true from this
     *   method for the collected objects.  
     */
    isCollectiveFor(obj) { return nil; }
;

/*
 *   A "collective group" object.  This is an abstract object: the player
 *   doesn't think of this as a physically separate object, but rather as a
 *   collection of a bunch of individual objects.  For example, if you had
 *   a group of floor-number buttons in an elevator, you might create a
 *   CollectiveGroup to represent the buttons as a collection - from the
 *   player's perspective, there's not a separate physical object called
 *   "the buttons," but it might nonetheless be handy to refer to "the
 *   buttons" collectively as a single entity in commands.  CollectiveGroup
 *   is designed for such situations.
 *   
 *   There are two ways to use CollectiveGroup: as a non-physical,
 *   non-simulation object whose only purpose is to field a few specific
 *   commands; or as a physical simulation object that shows up separately
 *   as an object in its own right.
 *   
 *   First: you can use a CollectiveGroup as a non-physical object, which
 *   essentially means it has a nil 'location'.  The group object doesn't
 *   actually appear in any location.  Instead, it'll be brought into the
 *   sensory system automatically by its individuals, and it'll have the
 *   same effective sensory status as the most visible/audible/etc of its
 *   individuals.  This choice is appropriate when the individuals are
 *   mobile, so they might be scattered around the game map, hence the
 *   group object might need to be invoked anywhere.  With this option, you
 *   normally won't want to make the CollectiveGroup handle very many
 *   commands, because you'll have to completely customize each command you
 *   want it to handle, in order to properly account for the possible
 *   scattering of the individuals.  For example, if you want the group
 *   object to handle the TAKE command, you'll have to figure out which
 *   individuals are in reach, and specially program the procedure for
 *   taking each of the available individuals.
 *   
 *   Second: you can use CollectiveGroup as a simulation object, and
 *   actually set its 'location' to the location of its individuals.  The
 *   group object in this case shows up in the simulation alongside its
 *   individuals.  This is a good choice if the individuals are fixed in
 *   place, all in one place, because you can simply put the group object
 *   in the same location as the individuals without worrying that the
 *   individuals will move around the game later on.  This is much easier
 *   to handle than the first case above, mostly because commands that
 *   physically manipulate the individuals (such as TAKE) aren't a factor.
 *   In this set-up, you can easily let the group object handle many
 *   actions, since it won't have to do much apart from showing the default
 *   failure messages that a Fixed would generate in any other situation.
 *   Note that if you use this approach, the CollectiveGroup should *also*
 *   inherit from Fixture or the like, so that the group object is fixed in
 *   place just like its corresponding individuals.
 *   
 *   The parser will substitute a CollectiveGroup object for its
 *   individuals when (1) any of the individuals are in scope, (2) the
 *   CollectiveGroup has vocabulary that matches a noun phrase in the
 *   player's input, and (3) the conditions for substitution, defined by
 *   isCollectiveQuant and isCollectiveAction, are met.
 *   
 *   (The substitution itself is handled in two steps.  First, an
 *   individual will add the group object to the sense connection list
 *   whenever the individual is in the connection list, which will bring
 *   the object into scope, so the parser will be able to match the
 *   vocabulary from the group object any time an individual is in scope.
 *   Once the group object is matched, its filterResolveList method will
 *   throw out either the group object or all of the individuals, depending
 *   on whether or not the isCollectiveQuant and isCollectiveAction tests
 *   are met.)
 *   
 *   For example, we might have a bunch of coins and paper bills in a game,
 *   and give them all a plural word 'money'.  We then also create a
 *   collective group object with plural word 'money'.  We set the
 *   collectiveGroup property of each coin and bill object to refer to the
 *   collective group object.  Whenever the player uses 'money' in a
 *   command, the individual coins and bills will initially match, and the
 *   group object will also match.  The group object will then either throw
 *   itself out, keeping only the individuals, or will throw out the
 *   individuals.  If the group object decides to field the command, it
 *   will be the only matching object, so a command like "examine money"
 *   will be directed to the single collective group object, rather than
 *   directed to the matching individuals one at a time.  This allows the
 *   game to present simpler, more elegant responses to commands on the
 *   individuals as a group.
 *   
 *   By default, the only action we handle is Examine.  Each instance must
 *   provide a suitable description so that when the collective is
 *   examined, we describe the group of individuals appropriately.  
 */
class CollectiveGroup: Thing
    /* collective group objects are usually named in plural terms */
    isPlural = true

    /*
     *   Filter a noun phrase resolution list.
     *   
     *   If there are any objects in the resolution list for which we're a
     *   collective, we'll check to see whether we want to the collective
     *   or keep the individuals.  We want to keep the collective if the
     *   action is one we can handle collectively; otherwise, we want to
     *   drop the collective and let the individuals handle the action
     *   instead.
     *   
     *   Note that, when any of our individuals are in scope, we're in
     *   scope.  This means that the collective is always in the
     *   resolution list, along with the individuals, if (1) any
     *   individuals are in scope, and (2) the vocabulary used in the noun
     *   phrase matches the collective object.  If the vocabulary doesn't
     *   match the collective, the parser simply won't include the
     *   collective in the resolution list by virtue of the normal
     *   vocabulary selection mechanism, so we'll never reach this point.
     *   
     *   By default, the collective object will be ignored if a specific
     *   number of objects is required.  When the player explicitly
     *   specifies a quantity (by a phrase like "the five coins" or "both
     *   coins"), we'll assume they want to iterate over individuals
     *   rather than operate on the collection.    
     */
    filterResolveList(lst, action, whichObj, np, requiredNum)
    {
        /*
         *   If we want to use the collective for the current action and
         *   the required quantity, keep the collective; otherwise, if
         *   there are any individuals, keep the individuals and filter
         *   out the collective group.  If there are no matching
         *   individuals, keep the collective group object, since there's
         *   nothing to replace it.  
         */
        if (isCollectiveQuant(np, requiredNum)
            && isCollectiveAction(action, whichObj))
        {
            /* 
             *   We can handle the action collectively, so keep myself, and
             *   get rid of the individuals.  We want to discard the
             *   individuals because we want the entire action to be
             *   handled by the collective object, rather than iterating
             *   over the individuals.  So, discard each object that has
             *   'self' as a collectiveGroup (which is to say, keep each
             *   object that *doesn't* have collectiveGroup 'self').  
             */
            lst = lst.subset({x: !x.obj_.hasCollectiveGroup(self)});
        }
        else if (lst.indexWhich({x: x.obj_.hasCollectiveGroup(self)}) != nil)
        {
            /* 
             *   We can't handle the action collectively, and the list
             *   includes at least one of our individuals, so let the
             *   individuals handle it.  Simply remove myself from the
             *   list.  
             */
            lst = lst.removeElementAt(lst.indexWhich({x: x.obj_ == self}));
        }

        /* return the updated list */
        return lst;
    }

    /*
     *   "Unfilter" a pronoun antecedent list.  We'll restore the
     *   individuals to the list so that we can choose anew, for the new
     *   command, whether to select the group object or the individuals.
     *   
     *   For example, suppose there's a CollectiveGroup for a set of
     *   elevator buttons that handles the Examine command, but no other
     *   commands.  Now suppose the player types in these commands:
     *   
     *.  >examine buttons
     *.  >push them
     *   
     *   On the first command, the CollectiveGroup object will filter out
     *   the individual buttons in filterResolveList, because the group
     *   object handles the Examine command on behalf of the individuals.
     *   This will set the pronoun antecedent for IT and THEM to the group
     *   object, because that's the program object that handled the
     *   action.  On the second command, if the player had typed simply
     *   PUSH BUTTONS, the collective group object would have filtered
     *   *itself* out, keeping the individuals.  However, the raw pronoun
     *   binding for THEM is the group object; if we did nothing to change
     *   this, we'd get a different response for PUSH THEM than we'd get
     *   for PUSH BUTTONS.  That's where this routine comes in: by
     *   restoring the individuals, we let filterResolveList() make the
     *   decision about what to keep anew for the pronoun.  
     */
    expandPronounList(typ, lst)
    {
        /* restore our individuals to the list */
        forEachInstance(Thing, function(obj) {
            if (obj.hasCollectiveGroup(self))
                lst += obj;
        });

        /* return the list */
        return lst;
    }
    
    /*
     *   Check the action to determine if it's one that we want to handle
     *   collectively.  If so, return true; if not, return nil. 
     */
    isCollectiveAction(action, whichObj)
    {
        /* we handle 'Examine' */
        if (action.ofKind(ExamineAction))
            return true;

        /* it's not one of ours */
        return nil;
    }

    /*
     *   Check to see if we're a collective for the given quantity.  By
     *   default, we return true only when no quantity is specified.  
     */
    isCollectiveQuant(np, requiredNum)
    {
        /* if no quantity was specified, use the collective */
        return (requiredNum == nil);
    }

    /*
     *   Get a list of the individuals that can be sensed, given the
     *   information table for the desired sense (for visible items, this
     *   can be obtained by calling gActor.visibleInfoTable()).  This is a
     *   service routine that can be useful for purposes such as writing a
     *   description routine for the collective.  For example, a "money"
     *   collective object might want to count up the sum of money visible
     *   and show that.
     *   
     *   Note that it's possible for this to return an empty list.  The
     *   caller can deal with this in a description, for example, by
     *   indicating that the collection cannot be seen.  
     */
    getVisibleIndividuals(tab)
    {
        /* keep only those items that are individuals of this collective */
        tab.forEachAssoc(function(key, val)
        {
            /* remove this item if it's not an individual of mine */
            if (!key.hasCollectiveGroup(self))
                tab.removeElement(key);
        });

        /* return a list of the objects (i.e., the table's keys) */
        return tab.keysToList();
    }

    /*
     *   When we have no location, we're an abstract object without any
     *   physical presence in the game world.  However, we still want to
     *   show up in the senses to the same extent our individuals do.  To
     *   do this, we override this method so that we use the same sense
     *   data as the most visible (or whatever) of our individuals.  
     */
    addToSenseInfoTable(sense, tab)
    {
        /* if we have no location, mimic our best individual */
        if (location == nil && !ofKind(BaseMultiLoc))
        {
            /* check everything in the connection table */
            tab.forEachAssoc(function(cur, val) {
                /* if this is one of our individuals, check it */
                if (cur.hasCollectiveGroup(self))
                {
                    local t;
                    
                    /* 
                     *   If it's the best or only one so far, adopt its
                     *   sense status.  Consider it the best if it has a
                     *   more transparent transparency than the best so
                     *   far, or its transparency is the same and it has a
                     *   high ambient level.  
                     */
                    t = transparencyCompare(cur.tmpTrans_, tmpTrans_);
                    if (t > 0 || (t == 0 && cur.tmpAmbient_ > tmpAmbient_))
                    {
                        /* it's better than our settings; mimic it */
                        tmpTrans_ = cur.tmpTrans_;
                        tmpAmbient_ = cur.tmpAmbient_;
                        tmpObstructor_ = cur.tmpObstructor_;
                    }
                }
            });
        }

        /* inherit the standard handling */
        inherited(sense, tab);
    }

    /*
     *   When we have no location, we want to create our own special
     *   containment path, just as we create our own special SenseInfo.  
     */
    specialPathFrom(src, vec)
    {
        /* if we have a location, use the normal handling */
        if (location != nil || ofKind(BaseMultiLoc))
            inherited(src, vec);

        /* look for an individual among the source object's connections */
        src.connectionTable().forEachAssoc(function(cur, val) {
            /* if this is one of our individuals, check it */
            if (cur.hasCollectiveGroup(self))
            {
                /* add this individual's paths to the vector */
                vec.appendAll(src.getAllPathsTo(cur));
            }
        });
    }

    /*
     *   CollectiveGroup objects are not normally listable in any
     *   situations.  Since a collective group is merely a parser stand-in
     *   for its individuals, we don't want it to appear as a separate
     *   object in the game. 
     */
    isListedInContents = nil
    isListedInInventory = nil
;

/*
 *   An "itemizing" collective group is like a regular collective group,
 *   but the Examine action itemizes the individual visible items making up
 *   the group.  We itemize the individuals instead of showing the 'desc'
 *   for the overall group object, as the basic collective group class
 *   does.  
 */
class ItemizingCollectiveGroup: CollectiveGroup
    /*
     *   Override the main Examine handling.  By default, we'll list the
     *   individuals that are visible, and separately list those that are
     *   being carried by the actor.  If none of our individuals are
     *   visible, simply say so.
     */
    mainExamine()
    {
        local info;
        local vis;
        local carried, here;

        /* get the visible info table */
        info = gActor.visibleInfoTable();
        
        /* get the list of visible individuals */
        vis = getVisibleIndividuals(info);

        /* if any individuals are visible, list them */
        if (vis.length() != 0)
        {
            /* separate out the individuals being carried */
            carried = vis.subset({x: x.isIn(gActor)});
            here = vis - carried;

            /* show the items that are here but not being carried, if any */
            if (here.length() != 0)
            {
                /* get the room contents lister */
                local lister = gActor.location.roomContentsLister;
                
                /* get the subset that the room contents lister won't list */
                local xlist = here.subset({x: !lister.isListed(x)});

                /* show the list through the room contents lister */
                lister.showList(gActor, nil, here, 0, 0, info, nil);

                /* Examine any objects not part of the room description */
                foreach (local x in xlist)
                    examineUnlisted(x);

                /* 
                 *   if that showed anything, add a paragraph break before
                 *   the carried list 
                 */
                if (xlist.length() != 0 && carried.length() != 0)
                    "<.p>";
            }

            /* separately, show the items being carried, if any */
            if (carried.length() != 0)
                gActor.inventoryLister.showList(
                    gActor, gActor, carried, 0, 0, info, nil);
        }
        else
        {
            /* 
             *   None are visible.  If it's dark in the location, simply
             *   say so; otherwise, say that we can't see any of me. 
             */
            if (!gActor.isLocationLit())
                reportFailure(&tooDarkMsg);
            else
                reportFailure(&mustBeVisibleMsg, self);
        }
    }

    /*
     *   Examine an unlisted individual object.  This will be called for
     *   each object in the room that's not listable via the room contents
     *   lister. 
     */
    examineUnlisted(x)
    {
        "<.p>";
        nestedAction(Examine, x);
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   A readable object.  Any ordinary object will show its normal full
 *   description when read, but an object that is explicitly readable will
 *   have elevated logicalness for the "read" action, and can optionally
 *   show a separate description when read. 
 */
class Readable: Thing
    /* 
     *   Show my special reading desription.  By default, we set this to
     *   nil to indicate that we should use our default "examine"
     *   description; objects can override this to show a special message
     *   for reading the object as desired.  
     */
    readDesc = nil

    /* our reading description when obscured */
    obscuredReadDesc() { gLibMessages.obscuredReadDesc(self); }

    /* our reading description in dim light */
    dimReadDesc() { gLibMessages.dimReadDesc(self); }

    /* "Read" action */
    dobjFor(Read)
    {
        verify()
        {
            /* give slight preference to an object being held */
            if (!isIn(gActor))
                logicalRank(80, 'not held');
        }
        action()
        {
            /* 
             *   if we have a special reading description defined, show
             *   it; otherwise, use the same handling as "examine"
             */
            if (propType(&readDesc) != TypeNil)
            {
                local info;
                
                /* 
                 *   Reading requires a transparent sight path and plenty
                 *   of light; in the absence of either of these, we can't
                 *   make out the details. 
                 */
                info = gActor.bestVisualInfo(self);
                if (info.trans != transparent)
                    obscuredReadDesc;
                else if (info.ambient < 3)
                    dimReadDesc;
                else
                    readDesc;
            }
            else
            {
                /* 
                 *   we have no special reading description, so use the
                 *   default "examine" handling 
                 */
                actionDobjExamine();
            }
        }
    }
;


/* ------------------------------------------------------------------------ */
/*
 *   A "consultable" object.  This is an inanimate object that can be
 *   consulted about various topics, almost the way an actor can be asked
 *   about topics.  Examples include individual objects that contain
 *   voluminous information, such as books, phone directories, and maps, as
 *   well as collections of individual information-carrying objects, such
 *   as file cabinets or bookcases.
 *   
 *   A consultable keeps a database of TopicEntry objects; this works in
 *   much the same way as the topic database system that actors use.
 *   Create one or more ConsultTopic objects and place them inside the
 *   Consultable (using the 'location' property, or using the '+' syntax).
 *   When an actor consults the object about a topic, we'll search our
 *   database for a ConsultTopic object that matches the topic and is
 *   currently active, and show the response for the best one we can find.
 *   
 *   From an IF design perspective, consultables have two nice properties.
 *   
 *   First, they hide the boundaries of implementation, by letting the game
 *   *suggest* that there's an untold wealth of information in a particular
 *   book (or whatever) without the need to actually implement all of it.
 *   We only have to show the entries the player specifically asks for, so
 *   the game never has to admit when it's run out of things to show, and
 *   the player can never know for sure that there's not more to find.  Be
 *   careful, though, because this is a double-edge sword, design-wise;
 *   it's easy to abuse this property to hide information gratuitously from
 *   the player.
 *   
 *   Second, consultables help "match impedances" between the narrative
 *   level of detail and the underlying world model.  At the narrative
 *   level, we paint in fairly broad strokes: when we visit a new location,
 *   we describe the *important* features of the setting, not every last
 *   detail.  If the player wants to examine something in closer detail, we
 *   zoom in on that detail, assuming we've implemented it, but it's up to
 *   the player to determine where the attention is focused.  Consultable
 *   objects give us the same capability for books and the like.  With a
 *   consultable, we can describe the way a book looks without immediately
 *   dumping the literal contents of the book onto the screen; but when the
 *   player chooses some aspect of the book to read in detail, we can zoom
 *   in on that page or chapter and show that literal content, if we
 *   choose.
 *   
 *   Also, note that we assume that consultables convey their information
 *   through visual information, such as printed text or a display screen.
 *   Because of this, we by default require that the object be visible to
 *   be consulted.  This might not be appropriate in some cases, such as
 *   Braille books or talking PDA's; to remove the visual condition,
 *   override the pre-condition for the Consult action.  
 */
class Consultable: Thing, TopicDatabase
    /* 
     *   If they consult us without a topic, just ask for a topic.  Treat
     *   it as logical, but rank it as improbable, in case there's
     *   anything else around that can be consulted without any topic
     *   specified.  
     */
    dobjFor(Consult)
    {
        preCond = [touchObj, objVisible]
        verify() { logicalRank(50, 'need a topic'); }
        action() { askForTopic(ConsultAbout); }
    }

    /* consult about a topic */
    dobjFor(ConsultAbout)
    {
        verify() { }
        action()
        {
            /* remember that we're the last object the actor consulted */
            gActor.noteConsultation(self);

            /* try handling the topic through our topic database */
            if (!handleTopic(gActor, gTopic, consultConvType, nil))
                topicNotFound();
        }
    }

    /* show the default response for a topic we couldn't find */
    topicNotFound()
    {
        /* 
         *   Report the absence of the topic.  Note that we use an
         *   ordinary, successful report, not a failure report, because
         *   the consultation really did succeed in the sense of the
         *   physical action of consulting: we successfully flipped
         *   through the book, scanned the file cabinet, or whatever.  We
         *   didn't find what we were looking for, but in terms of the
         *   physical action undertaken, we successfully did exactly what
         *   we were asked to do.  
         */
        mainReport(&cannotFindTopicMsg);
    }

    /*
     *   Resolve the topic phrase for a CONSULT ABOUT command.  The CONSULT
     *   ABOUT action refers this to the direct object of the action, so
     *   that the direct object can filter the topic match according to
     *   what makes sense for the consultable.
     *   
     *   By default, we resolve the topic phrase a little differently than
     *   we would for conversational commands, such as ASK ABOUT.  By
     *   default, we don't differentiate objects at all based on physical
     *   scope or actor knowledge when deciding on a match for a topic
     *   phrase.  For example, if you create a Consultable representing a
     *   phone book, and the player enters a command like FIND BOB IN PHONE
     *   BOOK, the topic BOB will be found even if the 'bob' object isn't
     *   known to the player character.  The reason for this difference
     *   from ASK ABOUT et al is that consultables are generally the kinds
     *   of objects where, in real life, a person could browse through the
     *   object and come across entries whether or not the person knew
     *   enough to look for them.  For example, you could go through a
     *   phone book and find an entry for "Bob" even if you didn't know
     *   anyone named Bob.
     *   
     *   'lst' is the list of ResolveInfo objects giving the full set of
     *   matches for the vocabulary words; 'np' is the grammar production
     *   object for the topic phrase; and 'resolver' is the TopicResolver
     *   that's resolving the topic phrase.  Note that 'lst' contains
     *   ResolveInfo objects, so to get the game-world object for a given
     *   list entry, use lst[i].obj_.
     *   
     *   We return a ResolvedTopic object that encapsulates the matching
     *   objects.  
     *   
     *   Note that the resolver object can be used to get certain useful
     *   information.  The resolver's getAction() method returns the action
     *   (which you should use instead of gAction, since this routine is
     *   called during the resolution process, not during command
     *   execution); its getTargetActor() method returns the actor
     *   performing the action; and its objInPhysicalScope(obj) method lets
     *   you determine if an object is in physical scope for the actor.  
     */
    resolveConsultTopic(lst, np, resolver)
    {
        /* 
         *   by default, simply return an undifferentiated list with
         *   everything given equal weight, whether known or not, and
         *   whether in scope or not 
         */
        return new ResolvedTopic(lst, [], [], np);
    }

    /* 
     *   Our topic entry database for consultatation topics.  This will be
     *   automatically built during initialization from the set of
     *   ConsultTopic objects located within me, so there's usually no
     *   need to initialize this manually.  
     */
    consultTopics = nil
;

/*
 *   A consultation topic.  You can place one or more of these inside a
 *   Consultable object (using the 'location' property, or the '+'
 *   notation), to create a database of topics that can be looked up in
 *   the consultable.  
 */
class ConsultTopic: TopicMatchTopic
    /* include in the consultation list */
    includeInList = [&consultTopics]

    /* 
     *   don't set any pronouns for the topic - the consultable itself
     *   should be the pronoun antecedent 
     */
    setTopicPronouns(fromActor, obj) { }
;

/* 
 *   A default topic entry for a consultable.  You can include one (or
 *   more) of these in a consultable's database to provide a topic of last
 *   resort that answers to any topics that aren't in the database
 *   themselves.  
 */
class DefaultConsultTopic: DefaultTopic
    includeInList = [&consultTopics]
    setTopicPronouns(fromActor, obj) { }
;


/* ------------------------------------------------------------------------ */
/*
 *   A common, abstract base class for things that cannot be moved.  You
 *   shouldn't use this class to create game objects directly; you should
 *   always use one of the concrete subclasses, such as Fixture or
 *   Immovable.  This base class doesn't provide the full behavior
 *   necessary to make an object immovable; it's just here as a
 *   programming abstraction for the common elements of all immovable
 *   objects.
 *   
 *   This class has two purposes.  First, it defines some behavior common
 *   to all non-portable objects.  Second, you can test an object to see
 *   if it's based on this class to determine whether it's a portable or
 *   unportable type of Thing.  
 */
class NonPortable: Thing
    /* 
     *   An immovable objects is not listed in room or container contents
     *   listings.  Since the object is immovable, it's in effect a
     *   permanent feature of its location, so it should be described as
     *   such: either directly as part of its location's description text,
     *   or via its own specialDesc.  
     */
    isListed = nil
    isListedInContents = nil
    isListedInInventory = nil

    /* 
     *   By default, if the object's contents would be listed in a direct
     *   examination, then also list them when showing an inventory list,
     *   or describing the enclosing room or an enclosing object.  
     */
    contentsListed = (contentsListedInExamine)

    /*
     *   Are my contents within a fixed item that is within the given
     *   location?  Since we're fixed in place, our contents are certainly
     *   within a fixed item, so we merely need to check if we're fixed in
     *   place within the given location.  We are if we're in the given
     *   location or we ourselves are fixed in place in the given location.
     */
    contentsInFixedIn(loc)
    {
        return isDirectlyIn(loc) || isInFixedIn(loc);
    }

    /*
     *   Since non-portables aren't carried, their weight and bulk are
     *   largely irrelevant.  Even so, when a non-portable is a component
     *   of another object, or otherwise contained in another object, its
     *   weight and/or bulk can affect the behavior of the parent object.
     *   So, it's simplest to use a default of zero for these so that there
     *   are no surprises about the parent's behavior.  
     */
    weight = 0
    bulk = 0

    /*
     *   Non-portable objects can't be held, since they can't be carried.
     *   However, in some cases, it's useful to include non-portable
     *   objects within an actor, such as when creating component parts of
     *   an actor (hands, say).  In these cases, the non-portables aren't
     *   held, but rather are components or similar.  
     */
    isHeldBy(actor) { return nil; }

    /*
     *   We're not being held, but if our location is an actor, then we're
     *   as good as held because we're effectively part of the actor.
     */
    meetsObjHeld(actor) { return actor == location; }

    /* 
     *   showing an immovable to someone simply requires that it be in
     *   sight: we're not holding it up to show it, we're simply pointing
     *   it out 
     */
    dobjFor(ShowTo) { preCond = [objVisible] }

    /*
     *   Thing decreases the likelihood that we want to examine an object
     *   when the object isn't being held.  That's fine for portable
     *   objects, but nonportables can never be held, so we don't want that
     *   decrease in logicalness. 
     */
    dobjFor(Examine)
    {
        /* override Thing's likelihood downgrade for un-held items */
        verify() { }
    }
;


/* ------------------------------------------------------------------------ */
/*
 *   A "fixture," which is something that's obviously a part of the room.
 *   These objects cannot be removed from their containers.  This class is
 *   meant for permanent features of rooms that obviously cannot be moved
 *   to a new container, such as walls, floors, doors, built-in bookcases,
 *   light switches, buildings, and the like.
 *   
 *   The important feature of a Fixture is that it's *obvious* that it's
 *   part of its container, so it should be safe to assume that a character
 *   normally wouldn't even try to take it or move it.  For objects that
 *   might appear portable but turn out to be immovable, other classes are
 *   more appropriate: use Heavy for objects that are immovable simply
 *   because they're very heavy, for example, or Immovable for objects that
 *   are immovable for some non-obvious reason.  
 */
class Fixture: NonPortable
    /* 
     *   Hide fixtures from "all" for certain commands.  Fixtures are
     *   obviously part of the location, so a reaonable person wouldn't
     *   even consider trying to do things like take them or move them.  
     */
    hideFromAll(action)
    {
        return (action.ofKind(TakeAction)
                || action.ofKind(DropAction)
                || action.ofKind(PutInAction)
                || action.ofKind(PutOnAction));
    }

    /* don't hide from defaults, though */
    hideFromDefault(action) { return nil; }

    /* a fixed item can't be moved by an actor action */
    verifyMoveTo(newLoc)
    {
        /* it's never possible to do this */
        illogical(cannotMoveMsg);
    }

    /* 
     *   a fixed item can't be taken - this would be caught by
     *   verifyMoveTo anyway, but provide a more explicit message when a
     *   fixed item is explicitly taken 
     */
    dobjFor(Take) { verify() { illogical(cannotTakeMsg); }}
    dobjFor(TakeFrom) { verify() { illogical(cannotTakeMsg); }}

    /* fixed objects can't be put anywhere */
    dobjFor(PutIn) { verify() { illogical(cannotPutMsg); }}
    dobjFor(PutOn) { verify() { illogical(cannotPutMsg); }}
    dobjFor(PutUnder) { verify() { illogical(cannotPutMsg); }}
    dobjFor(PutBehind) { verify() { illogical(cannotPutMsg); }}

    /* fixed objects can't be pushed, pulled, or moved */
    dobjFor(Push) { verify() { illogical(cannotMoveMsg); }}
    dobjFor(Pull) { verify() { illogical(cannotMoveMsg); }}
    dobjFor(Move) { verify() { illogical(cannotMoveMsg); }}
    dobjFor(MoveWith) { verify() { illogical(cannotMoveMsg); }}
    dobjFor(MoveTo) { verify() { illogical(cannotMoveMsg); }}
    dobjFor(PushTravel) { verify() { illogical(cannotMoveMsg); }}
    dobjFor(ThrowAt) { verify() { illogical(cannotMoveMsg); }}
    dobjFor(ThrowDir) { verify() { illogical(cannotMoveMsg); }}

    /*
     *   The messages to use for illogical messages.  These can be
     *   overridden with new properties (of playerActionMessages and the
     *   like), or simply with single-quoted strings to display.  
     */
    cannotTakeMsg = &cannotTakeFixtureMsg
    cannotMoveMsg = &cannotMoveFixtureMsg
    cannotPutMsg = &cannotPutFixtureMsg

    /*
     *   A component can be said to be owned by its location's owner or by
     *   its location. 
     */
    isOwnedBy(obj)
    {
        /* 
         *   if I'm owned by the object under the normal rules, then we
         *   won't say otherwise 
         */
        if (inherited(obj))
            return true;

        /* 
         *   we can be said to be owned by our location, since we're a
         *   direct and permanent part of the location 
         */
        if (obj == location)
            return true;

        /* 
         *   if my location is owned by the given object, consider
         *   ourselves owned by it as well, as we're an extension of our
         *   location 
         */
        if (location != nil && location.isOwnedBy(obj))
            return true;

        /* we didn't find anything that establishes ownership */
        return nil;
    }
;

/*
 *   A component object.  These objects cannot be removed from their
 *   containers because they are permanent features of other objects, which
 *   may themselves be portable: the hands of a watch, a tuning dial on a
 *   radio.  This class behaves essentially the same way as Fixture, but
 *   its messages are more suitable for objects that are component parts of
 *   other objects rather than fixed features of rooms.  
 */
class Component: Fixture
    /* a component cannot be removed from its container by an actor action */
    verifyMoveTo(newLoc)
    {
        /* it's never possible to do this */
        illogical(&cannotMoveComponentMsg, location);
    }

    /*
     *   Hide components from EXAMINE ALL, as well as any commands hidden
     *   from ALL for ordinary fixtures.  Components are small parts of
     *   larger objects, so when we EXAMINE ALL, it's enough to examine the
     *   larger objects of which we're a part; we don't want components to
     *   show up separately in these cases.  
     */
    hideFromAll(action)
    {
        /* hide from EXAMINE ALL, plus anything the base class hides */
        return (action.ofKind(ExamineAction)
                || inherited(action));
    }

    /* 
     *   We are a component of our direct cotnainer, and we're indirectly a
     *   component of anything that it's a component of.  
     */
    isComponentOf(obj)
    {
        return (obj == location
                || (location != nil && location.isComponentOf(obj)));
    }

    /*
     *   Consider ourself to be held by the given actor if we're a
     *   component of the actor.  
     */
    meetsObjHeld(actor) { return isComponentOf(actor); }

    /* a component cannot be taken separately */
    dobjFor(Take)
        { verify() { illogical(&cannotTakeComponentMsg, location); }}
    dobjFor(TakeFrom)
        { verify() { illogical(&cannotTakeComponentMsg, location); }}

    /* a component cannot be separately put somewhere */
    dobjFor(PutIn)
        { verify() { illogical(&cannotPutComponentMsg, location); }}
    dobjFor(PutOn)
        { verify() { illogical(&cannotPutComponentMsg, location); }}
    dobjFor(PutUnder)
        { verify() { illogical(&cannotPutComponentMsg, location); }}
    dobjFor(PutBehind)
        { verify() { illogical(&cannotPutComponentMsg, location); }}
;

/*
 *   A "secret fixture" is a kind of fixture that we use for internal
 *   implementation purposes, and which we don't intend to be visible to
 *   the player.  Objects of this type usually have no vocabulary, since we
 *   don't want the player to be able to refer to them.  
 */
class SecretFixture: Fixture
    /* 
     *   this kind of object is internal to the game's implementation, so
     *   we don't want it to show up in "all" lists 
     */
    hideFromAll(action) { return true; }
;

/*
 *   A fixture that uses the same custom message for taking, moving, and
 *   putting.  In many cases, it's useful to customize the message for a
 *   fixture, using the same custom message for all sorts of moving.  Just
 *   override cannotTakeMsg, and the other messages will copy it.  
 */
class CustomFixture: Fixture
    cannotMoveMsg = (cannotTakeMsg)
    cannotPutMsg = (cannotTakeMsg)
;

/* ------------------------------------------------------------------------ */
/*
 *   An Immovable is an object that can't be moved, but not because it's
 *   obviously a fixture or component of another object.  This class is
 *   suitable for things like furniture, which are in principle portable
 *   but which actors aren't actually allowed to pick up or move around.
 *   
 *   Note that Immovable is a lot like Fixture.  The difference is that
 *   Fixture is for objects that are *obviously* fixed in place by their
 *   very nature, whereas Immovable is for objects that common sense would
 *   tell us are portable, but which the game doesn't in fact allow the
 *   player to move.
 *   
 *   The practical difference between Immovable and Fixture is that Fixture
 *   considers taking or moving to be illogical actions, whereas Immovable
 *   considers these actions logical but simply doesn't allow them.  To be
 *   more specific, Fixture disallows taking and moving in the verify()
 *   methods for those actions, while Immovable disallows the actions in
 *   the check() methods.  This means, for example, that Fixture objects
 *   will be removed from consideration during the noun resolution phase
 *   when there are more logical choices.
 */
class Immovable: NonPortable
    /* an Immovable can't be taken */
    dobjFor(Take) { check() { failCheck(cannotTakeMsg); }}

    /* Immovables can't be put anywhere */
    dobjFor(PutIn) { check() { failCheck(cannotPutMsg); }}
    dobjFor(PutOn) { check() { failCheck(cannotPutMsg); }}
    dobjFor(PutUnder) { check() { failCheck(cannotPutMsg); }}
    dobjFor(PutBehind) { check() { failCheck(cannotPutMsg); }}

    /* Immovables can't be pushed, pulled, or otherwise moved */
    dobjFor(Drop) { action() { reportFailure(cannotMoveMsg); }}
    dobjFor(Push) { action() { reportFailure(cannotMoveMsg); }}
    dobjFor(Pull) { action() { reportFailure(cannotMoveMsg); }}
    dobjFor(Move) { action() { reportFailure(cannotMoveMsg); }}
    dobjFor(MoveWith) { check() { failCheck(cannotMoveMsg); }}
    dobjFor(MoveTo) { check() { failCheck(cannotMoveMsg); }}
    dobjFor(PushTravel) { action() { reportFailure(cannotMoveMsg); }}
    dobjFor(ThrowAt) { verify() { illogical(cannotMoveMsg); }}
    dobjFor(ThrowDir) { verify() { illogical(cannotMoveMsg); }}
    dobjFor(Turn)
    {
        verify() { logicalRank(50, 'turn heavy'); }
        action() { reportFailure(cannotMoveMsg); }
    }

    /*
     *   The messages to use for the failure messages.  These can be
     *   overridden with new properties (of playerActionMessages and the
     *   like), or simply with single-quoted strings to display.  
     */
    cannotTakeMsg = &cannotTakeImmovableMsg
    cannotMoveMsg = &cannotMoveImmovableMsg
    cannotPutMsg = &cannotPutImmovableMsg
;

/*
 *   An immovable that uses the same custom message for taking, moving, and
 *   putting.  In many cases, it's useful to customize the message for an
 *   immovable, using the same custom message for all sorts of moving.
 *   Just override cannotTakeMsg, and the other messages will copy it. 
 */
class CustomImmovable: Immovable
    cannotMoveMsg = (cannotTakeMsg)
    cannotPutMsg = (cannotTakeMsg)
;

/*
 *   Heavy: an object that's immovable because it's very heavy.  This is
 *   suitable for things like large boulders, heavy furniture, or the like:
 *   things that aren't nailed down, but nonetheless are too heavy to be
 *   carried or otherwise move.
 *   
 *   This is a simple specialization of Immovable; the only thing we change
 *   is the messages we use to describe why the object can't be moved.  
 */
class Heavy: Immovable
    cannotTakeMsg = &cannotTakeHeavyMsg
    cannotMoveMsg = &cannotMoveHeavyMsg
    cannotPutMsg = &cannotPutHeavyMsg
;


/* ------------------------------------------------------------------------ */
/*
 *   Decoration.  This is an object that is included for scenery value but
 *   which has no other purpose, and which the author wants to make clear
 *   is not important.  We use the catch-all action routine to respond to
 *   any command on this object with a flat "that's not important"
 *   message, so that the player can plainly see that there's no point
 *   wasting any time trying to manipulate this object.
 *   
 *   We use the "default" catch-all verb verify handling to report our
 *   "that's not important" message, so a decoration can be made
 *   responsive to specific verbs simply by defining an action handler for
 *   those verbs.  
 */
class Decoration: Fixture
    /* don't include decorations in 'all' */
    hideFromAll(action) { return true; }

    /* don't hide from defaults */
    hideFromDefault(action) { return nil; }

    /* 
     *   use the default response "this object isn't important" when we're
     *   used as either a direct or indirect object 
     */
    dobjFor(Default)
    {
        verify() { illogical(notImportantMsg, self); }
    }
    iobjFor(Default)
    {
        verify() { illogical(notImportantMsg, self); }
    }

    /* use the standard not-important message for decorations */
    notImportantMsg = &decorationNotImportantMsg

    /*
     *   The catch-all Default verifier makes all actions illogical, but we
     *   can override this to allow specific actions by explicitly defining
     *   them here so that they hide the Default verify handlers.  In
     *   addition, give decorations a reduced logical rank, so that any
     *   in-scope non-decoration object with similar vocabulary will be
     *   matched for an Examine command ahead of a decoration.  
     */
    dobjFor(Examine)
        { verify() { inherited(); logicalRank(70, 'decoration'); } }

    /* 
     *   likewise for LISTEN TO and SMELL, which are the auditory and
     *   olfactory equivalents of EXAMINE 
     */
    dobjFor(ListenTo)
        { verify() { inherited(); logicalRank(70, 'decoration'); } }
    dobjFor(Smell)
        { verify() { inherited(); logicalRank(70, 'decoration'); } }

    /* likewise for READ */
    dobjFor(Read)
        { verify() { inherited(); logicalRank(70, 'decoration'); } }

    /* likewise for LOOK IN and SEARCH */
    dobjFor(LookIn)
        { verify() { inherited(); logicalRank(70, 'decoration'); } }
    dobjFor(Search)
        { verify() { inherited(); logicalRank(70, 'decoration'); } }

    /* the default LOOK IN response is our standard "that's not important" */
    lookInDesc { mainReport(&notImportantMsg, self); }
;

/* ------------------------------------------------------------------------ */
/*
 *   An "unthing" is an object that represents the *absence* of an object.
 *   It's occasionally useful to respond specially when the player mentions
 *   an object that isn't present, especially when the player is likely to
 *   assume that something is present.
 *   
 *   An unthing is essentially a decoration, but we use a customized
 *   message that says "that isn't here" rather than "that isn't
 *   important".  
 */
class Unthing: Decoration
    /* 
     *   The message to display when the player refers to this object.
     *   This can be a library message property, or a single-quoted string.
     *   This message will probably always be overridden in practice, since
     *   the point of this class is to provide a more specific explanation
     *   of why the object isn't here.  
     */
    notHereMsg = &unthingNotHereMsg

    /* an Unthing shouldn't be picked as a default */
    hideFromDefault(action) { return true; }

    /* 
     *   by default, use our 'not here' message for our descriptions (in
     *   all of the standard senses) 
     */
    basicExamine() { mainReport(notHereMsg, self); }
    basicExamineListen(explicit)
    {
        if (explicit)
            mainReport(notHereMsg, self);
    }
    basicExamineSmell(explicit)
    {
        if (explicit)
            mainReport(notHereMsg, self);
    }

    /* use our custom message for the inherited Decoration responses */
    notImportantMsg = (notHereMsg)

    /*
     *   Because we're not actually here, use custom error messages when
     *   we're used as a possessive or locational qualifier.  The standard
     *   messages say things like "Bob doesn't appear to have that" or "You
     *   don't see that in the box," but these don't make sense for an
     *   Unthing - we're not actually here, so we can't "appear" or "seem"
     *   to own or contain anything.  Instead, we need to indicate that the
     *   qualifying object itself (i.e., 'self') isn't here at all.  
     */
    throwNoMatchForPossessive(txt) { throwUnthingAsQualifier(); }
    throwNoMatchForLocation(txt) { throwUnthingAsQualifier(); }
    throwNothingInLocation() { throwUnthingAsQualifier(); }

    /* 
     *   throw a generic message when we're used as a qualifier - we'll
     *   simply get our "not here" message and display that 
     */
    throwUnthingAsQualifier()
    {
        local msg;
        
        /* 
         *   resolve our "not here" message to a string - we need to do
         *   this here, since we're too early in the parsing sequence for
         *   the normal "mainResult" type of processing 
         */
        msg = MessageResult.resolveMessageText([self], &notHereMsg, [self]);

        /* throw a parser exception that will display this literal text */
        throw new ParseFailureException(&parserErrorString, msg);
    }

    /* 
     *   if there's anything at all in a resolve list other than me, always
     *   remove me 
     */
    filterResolveList(lst, action, whichObj, np, requiredNum)
    {
        /* if the list has anything else in it, remove myself */
        if (lst.length() != 1)
            lst = lst.removeElementAt(lst.indexWhich({x: x.obj_ == self}));

        /* return the list */
        return lst;
    }

    /* 
     *   trying to given an order to an Unthing acts the same way as any
     *   other kind of interaction 
     */
    acceptCommand(issuingActor) { mainReport(notHereMsg, self); }
;
    

/* ------------------------------------------------------------------------ */
/*
 *   Distant item.  This is an object that's too far away to manipulate,
 *   but can be seen.  This is useful for scenery objects that are at a
 *   great distance within a large location.
 *   
 *   A Distant item is essentially just like a decoration, but the default
 *   message is different.  Note that this class is based on Fixture, which
 *   means that it should be *obvious* that the object is too far away to
 *   take or move.  
 */
class Distant: Fixture
    /* don't include in 'all' */
    hideFromAll(action) { return true; }

    dobjFor(Default)
    {
        verify() { illogical(&tooDistantMsg, self); }
    }
    iobjFor(Default)
    {
        verify() { illogical(&tooDistantMsg, self); }
    }

    /* 
     *   Explicitly allow examining and listening to a Distant item.  To
     *   do this, override the 'verify' methods explicitly; we only need
     *   to inherit the base class handling, but we need to explicitly do
     *   so to 'override' the catch-all default handlers. 
     */
    dobjFor(Examine) { verify { inherited() ; } }
    dobjFor(ListenTo) { verify() { inherited(); } }

    /* similarly, allow showing a distant item */
    dobjFor(ShowTo) { verify() { inherited(); } }
;

/*
 *   Out Of Reach - this is a special mix-in that can be used to create an
 *   object that places its *contents* out of reach under customizable
 *   conditions, and can optionally place itself out of reach as well.
 */
class OutOfReach: object
    checkTouchViaPath(obj, dest, op)
    {
        /* check how we're traversing the object */
        if (op == PathTo)
        {
            /* 
             *   we're reaching from outside for this object itself -
             *   check to see if the source can reach me
             */
            if (!canObjReachSelf(obj))
                return new CheckStatusFailure(
                    cannotReachFromOutsideMsg(dest), dest);
        }
        else if (op == PathIn)
        {
            /* 
             *   we're reaching in to touch one of my contents - check to
             *   see if the source object is within reach of my contents 
             */
            if (!canObjReachContents(obj))
                return new CheckStatusFailure(
                    cannotReachFromOutsideMsg(dest), dest);
        }
        else if (op == PathOut)
        {
            local ok;
            
            /*
             *   We're reaching out.  If we're reaching for the object
             *   itself, check to see if we're reachable from within;
             *   otherwise, check to see if we can reach objects outside
             *   us from within.  
             */
            if (dest == self)
                ok = canReachSelfFromInside(obj);
            else
                ok = canReachFromInside(obj, dest);

            /* if we can't reach the object, say so */
            if (!ok)
                return new CheckStatusFailure(
                    cannotReachFromInsideMsg(dest), dest);
        }
        
        /* if we didn't find a problem, allow the operation */
        return checkStatusSuccess;
    }

    /* 
     *   The message to use to indicate that we can't reach an object,
     *   because the actor is outside me and the target is inside, or vice
     *   versa.  Each of these can return a property ID giving an actor
     *   action message property, or can simply return a string with the
     *   message text.  
     */
    cannotReachFromOutsideMsg(dest) { return &tooDistantMsg; }
    cannotReachFromInsideMsg(dest) { return  &tooDistantMsg; }

    /*
     *   Determine if the given object can reach my contents.  'obj' is
     *   the object (usually an actor) attempting to reach my contents
     *   from outside of me.
     *   
     *   By default, we'll return nil, so that nothing within me can be
     *   reached from anyone outside.  This can be overridden to allow my
     *   contents to become reachable from some external locations but not
     *   others; for example, a high shelf could allow an actor standing
     *   on a chair to reach my contents. 
     */
    canObjReachContents(obj) { return nil; }

    /*
     *   Determine if the given object can reach me.  'obj' is the object
     *   (usually an actor) attempting to reach this object.
     *   
     *   By default, make this object subject to the same rules as its
     *   contents.  
     */
    canObjReachSelf(obj) { return canObjReachContents(obj); }

    /*
     *   Determine if the given object outside of me is reachable from
     *   within me.  'obj' (usually an actor) is attempting to reach
     *   'dest'.
     *   
     *   By default, we return nil, so nothing outside of me is reachable
     *   from within me.  This can be overridden as needed.  This should
     *   usually behave symmetrically with canObjReachContents().  
     */
    canReachFromInside(obj, dest) { return nil; }

    /* 
     *   Determine if we can reach this object itself from within.  This
     *   is used when 'obj' tries to touch this object when 'obj' is
     *   located within this object.
     *   
     *   By default, we we use the same rules as we use to reach an
     *   external object from within.  
     */
    canReachSelfFromInside(obj) { return canReachFromInside(obj, self); }

    /*
     *   We cannot implicitly remove this obstruction, so simply return
     *   nil when asked.  
     */
    tryImplicitRemoveObstructor(sense, obj) { return nil; }
;

/* ------------------------------------------------------------------------ */
/*
 *   A Fill Medium - this is the class of object returned from
 *   Thing.fillMedium().  
 */
class FillMedium: Thing
    /*
     *   Get the transparency sensing through this medium. 
     */
    senseThru(sense)
    {
        /* 
         *   if I have a meterial, use its transparency; otherwise, we're
         *   transparent 
         */
        return (material != nil ? material.senseThru(sense) : transparent);
    }

    /* my material */
    material = nil
;

/* ------------------------------------------------------------------------ */
/*
 *   Base multi-location item with automatic initialization.  This is the
 *   base class for various multi-located object classes.
 *   
 *   We provide four ways of initializing a multi-located object's set of
 *   locations.
 *   
 *   First, the object can simply enumerate the locations explicitly, by
 *   setting the 'locationList' property to the list of locations.
 *   
 *   Second, the object can indicate that it's located in every object of a
 *   given class, by setting the 'initialLocationClass' property to the
 *   desired class.
 *   
 *   Third, the object can define a rule that specifies which objects are
 *   its initial locations, by defining the 'isInitiallyIn(obj)' method to
 *   return true if 'obj' is an initial location, nil if not.  This can be
 *   combined with the 'initialLocationClass' mechanism: if
 *   'initialLocationClass' is non-nil, then only objects of the given
 *   class will be tested with 'isInitiallyIn()'; if 'initialLocationClass'
 *   is nil, then every object in the entire game will be tested.
 *   
 *   Fourth, you can override the method buildLocationList() to build an
 *   return the initial list of locations.  You can use this approach if
 *   you have a complex set of rules for determining the initial location
 *   list, and none of the above approaches are flexible enough to
 *   implement it.  If you override buildLocationList(), simply compute and
 *   return the list of initial locations; the library will automatically
 *   call the method during pre-initialization.
 *   
 *   If you don't define any of these, then the object simply has no
 *   initial locations by default.  
 */
class BaseMultiLoc: object
    /* 
     *   The location list.  Instances can override this to manually
     *   enumerate our initial locations.  By default, we'll call
     *   buildLocationList() the first time this is invoked, and store the
     *   result. 
     */
    locationList = perInstance(buildLocationList())

    /*
     *   The class of our initial locations.  If this is nil, then our
     *   default buildLocationList() method will test every object in the
     *   entire game with our isInitiallyIn() method; otherwise, we'll test
     *   only objects of the given class.  
     */
    initialLocationClass = nil

    /*
     *   Test an object for inclusion in our initial location list.  By
     *   default, we'll simply return true to include every object.  We
     *   return true by default so that an instance can merely specify a
     *   value for initialLocationClass in order to place this object in
     *   every instance of the given class.
     */
    isInitiallyIn(obj) { return true; }

    /*
     *   Build my list of locations, and return the list.  This default
     *   implementation looks for an 'initialLocationClass' property value,
     *   and if one is found, looks at every object of that class;
     *   otherwise, it looks at every object in the entire game.  In either
     *   case, each object is then passed to our isInitiallyIn() method,
     *   and is included in our result list if isInitiallyIn() returns
     *   true.  
     */
    buildLocationList()
    {
        /*
         *   If the object doesn't define any of the standard rules, which
         *   it would do by overriding initialLocationClass and/or
         *   isInitiallyIn(), then simply return an empty list.  We take
         *   the absence of overrides for any of the rules to mean that the
         *   object simply has no initial locations.  
         */
        if (initialLocationClass == nil
            && !overrides(self, BaseMultiLoc, &isInitiallyIn))
            return [];

        /* start with an empty list */
        local lst = new Vector(16);

        /*
         *   if initialLocationClass is defined, loop over all objects of
         *   that class; otherwise, loop over all objects
         */
        if (initialLocationClass != nil)
        {
            /* loop over all instances of the given class */
            for (local obj = firstObj(initialLocationClass) ; obj != nil ;
                 obj = nextObj(obj, initialLocationClass))
            {
                /* if the object passes the test, include it */
                if (isInitiallyIn(obj))
                    lst.append(obj);
            }
        }
        else
        {
            /* loop over all objects */
            for (local obj = firstObj() ; obj != nil ; obj = nextObj(obj))
            {
                /* if the object passes the test, include it */
                if (isInitiallyIn(obj))
                    lst.append(obj);
            }
        }

        /* return the list of locations */
        return lst.toList();
    }

    /* determine if I'm in a given object, directly or indirectly */
    isIn(obj)
    {
        /* first, check to see if I'm directly in the given object */
        if (isDirectlyIn(obj))
            return true;

        /*
         *   Look at each object in my location list.  For each location
         *   object, if the location is within the object, I'm within the
         *   object.
         */
        return locationList.indexWhich({loc: loc.isIn(obj)}) != nil;
    }

    /* determine if I'm directly in the given object */
    isDirectlyIn(obj)
    {
        /*
         *   we're directly in the given object only if the object is in
         *   my list of immediate locations
         */
        return (locationList.indexOf(obj) != nil);
    }

    /* 
     *   Determine if I'm to be listed within my immediate container.  As a
     *   multi-location object, we have multiple immediate containers, so
     *   we need to know which direct container we're talking about.
     *   Thing.examineListContents() passes this down via "cont:", a named
     *   parameter.  Other callers might not always provide this argument,
     *   though, so if it's not present simply base this on whether we have
     *   a special description in any context.
     */
    isListedInContents(examinee:?)
    {
        return (examinee != nil
                ? !useSpecialDescInContents(examinee)
                : !useSpecialDesc());
    }

    /* Am I either inside 'obj', or equal to 'obj'?  */
    isOrIsIn(obj) { return self == obj || isIn(obj); }
;

/* ------------------------------------------------------------------------ */
/*
 *   MultiLoc: this class can be multiply inherited by any object that
 *   must exist in more than one place at a time.  To use this class, put
 *   it BEFORE Thing (or any subclass of Thing) in the object's superclass
 *   list, to ensure that we override the default containment
 *   implementation for the object.
 *   
 *   Note that a MultiLoc object appears *in its entirety* in each of its
 *   locations.  This means that MultiLoc is most suitable for a couple of
 *   specific situations:
 *   
 *   - several locations overlap slightly so that they include a common
 *   object: a large statue at the center of a public square, for example;
 *   
 *   - an object forms a sense connection among its location: a window;
 *   
 *   - a distant object that is seen in its entirety from several
 *   locations: the moon, say, or a mountain range.
 *   
 *   Note that MultiLoc is NOT suitable for cases where an object spans
 *   several locations but isn't contained entirely in any one of them:
 *   it's not good for something like a rope or a river, for example.
 *   MultiLoc also isn't good for cases where you simply want to avoid
 *   creating a bunch of repeated decorations in different locations.
 *   MultiLoc isn't good for these cases because a MultiLoc is treated as
 *   though it exists ENTIRELY and SIMULTANEOUSLY in all of its locations,
 *   which means that all of its sense information and internal state is
 *   shared among all of its locations.
 *   
 *   MultiInstance is better than MultiLoc for cases where you want to
 *   share a decoration object across several locations.  MultiInstance is
 *   better because it creates individual copies of the object in the
 *   different locations, so each copy has its own separate sense
 *   information and its own separate identity.
 *   
 *   MultiFaceted is better for objects that span several locations, such
 *   as a river or a long rope.  Like MultiInstance, MultiFaceted creates
 *   a separate copy in each location; in addition, MultiFaceted relates
 *   the copies together as "facets" of the same object, so that the
 *   parser knows they're all actually parts of one larger object.  
 */
class MultiLoc: BaseMultiLoc
    /*
     *   Initialize my location's contents list - add myself to my
     *   container during initialization
     */
    initializeLocation()
    {
        /* add myself to each of my container's contents lists */
        locationList.forEach({loc: loc.addToContents(self)});
    }

    /*
     *   Re-initialize the location list.  This calls buildLocationList()
     *   to re-evaluate the location rules, then updates the locationList
     *   to match the new results.  We'll remove the MultiLoc from any old
     *   locations that are no longer part of the location list, and we'll
     *   add it to any new locations that weren't previously in the
     *   location list.  You can call this at any time to update the
     *   MutliLoc's presence to reflect applying our location rules to the
     *   current game state.
     *   
     *   Note that this doesn't trigger any moveInto notifications.  This
     *   routine is a re-initialization rather than an in-game action, so
     *   it's not meant to behave as though an actor in the game were
     *   walking around moving the MultiLoc around; thus no notifications
     *   are sent.  Note also that we attempt to minimize our work by
     *   computing the "delta" from the old state - hence we only move the
     *   MultiLoc into containers it wasn't in previously, and we only
     *   remove it from existing containers that it's no longer in.  
     */
    reInitializeLocation()
    {
        local newList;

        /* build the new location list */
        newList = buildLocationList();

        /*
         *   Update any containers that are not in the intersection of the
         *   two lists.  Note that we don't simply move ourselves out of
         *   the old list and into the new list, because the two lists
         *   could have common members; to avoid unnecessary work that
         *   might result from removing ourselves from a container and
         *   then adding ourselves right back in to the same container, we
         *   only notify containers when we're actually moving out or
         *   moving in. 
         */

        /* 
         *   For each item in the old list, if it's not in the new list,
         *   notify the old container that we're being removed.
         */
        foreach (local loc in locationList)
        {
            /* if it's not in the new list, remove me from the container */
            if (newList.indexOf(loc) == nil)
                loc.removeFromContents(self);
        }

        /* 
         *   for each item in the new list, if we weren't already in this
         *   location, add ourselves to the location 
         */
        foreach (local loc in newList)
        {
            /* if it's not in the old list, add me to the new container */
            if (!isDirectlyIn(loc) == nil)
                loc.addToContents(self);
        }
        
        /* make the new location list current */
        locationList = newList;
    }

    /*
     *   Note that we don't need to override any of the contents
     *   management methods, since we provide special handling for our
     *   location relationships, not for our contents relationships.
     */

    /* save my location for later restoration */
    saveLocation()
    {
        /* return my list of locations */
        return locationList;
    }

    /* restore a previously saved location */
    restoreLocation(oldLoc)
    {
        /* remove myself from each current location not in the saved list */
        foreach (local cur in locationList)
        {
            /* 
             *   if this present location isn't in the saved list, remove
             *   myself from the location 
             */
            if (oldLoc.indexOf(cur) == nil)
                cur.removeFromContents(self);
        }

        /* add myself to each saved location not in the current list */
        foreach (local cur in oldLoc)
        {
            /* if I'm not already in this location, add me to it */
            if (locationList.indexOf(cur) == nil)
                cur.addToContents(self);
        }

        /* set my own list to the original list */
        locationList = oldLoc;
    }

    /*
     *   Basic routine to move this object into a given single container.
     *   Removes the object from all of its other containers.  Performs no
     *   notifications.  
     */
    baseMoveInto(newContainer)
    {
        /* remove myself from all of my current contents */
        locationList.forEach({loc: loc.removeFromContents(self)});

        /* set my location list to include only the new location */
        if (newContainer != nil)
        {
            /* set my new location */
            locationList = [newContainer];

            /* add myself to my new container's contents */
            newContainer.addToContents(self);
        }
        else
        {
            /* we have no new locations */
            locationList = [];
        }
    }

    /*
     *   Add this object to a new location - base version that performs no
     *   notifications.  
     */
    baseMoveIntoAdd(newContainer)
    {
        /* add the new container to my list of locations */
        locationList += newContainer;

        /* add myself to my new container's contents */
        newContainer.addToContents(self);
    }

    /*
     *   Add this object to a new location.
     */
    moveIntoAdd(newContainer)
    {
        /* notify my new container that I'm about to be added */
        if (newContainer != nil)
            newContainer.sendNotifyInsert(self, newContainer, &notifyInsert);

        /* perform base move-into-add operation */
        baseMoveIntoAdd(newContainer);
        
        /* note that I've been moved */
        moved = true;
    }

    /*
     *   Base routine to move myself out of a given container.  Performs
     *   no notifications. 
     */
    baseMoveOutOf(cont)
    {
        /* remove myself from this container's contents list */
        cont.removeFromContents(self);

        /* remove this container from my location list */
        locationList -= cont;
    }

    /*
     *   Remove myself from a given container, leaving myself in any other
     *   containers.
     */
    moveOutOf(cont)
    {
        /* if I'm not actually directly in this container, do nothing */
        if (!isDirectlyIn(cont))
            return;

        /* 
         *   notify this container (and only this container) that we're
         *   being removed from it 
         */
        cont.sendNotifyRemove(obj, nil, &notifyRemove);

        /* perform base operation */
        baseMoveOutOf(cont);

        /* note that I've been moved */
        moved = true;
    }

    /*
     *   Call a function on each container.  We'll invoke the function as
     *   follows for each container 'cont':
     *   
     *   (func)(cont, args...)  
     */
    forEachContainer(func, [args])
    {
        /* call the function for each location in our list */
        foreach(local cur in locationList)
            (func)(cur, args...);
    }

    /* 
     *   Call a function on each connected container.  By default, we
     *   don't connect our containers for sense purposes, so we do nothing
     *   here. 
     */
    forEachConnectedContainer(func, ...) { }

    /* 
     *   get a list of my connected containers; by default, we don't
     *   connect our containers, so this is an empty list 
     */
    getConnectedContainers = []

    /* 
     *   Clone this object's contents for inclusion in a MultiInstance's
     *   contents tree.  A MultiLoc is capable of being in multiple places
     *   at once, so we can just use our original contents tree as is.
     */
    cloneMultiInstanceContents(loc) { }

    /*
     *   Create a clone of this object for inclusion in a MultiInstance's
     *   contents tree.  We don't actually need to make a copy of the
     *   object, because a MultiLoc can be in several locations
     *   simultaneously; all we need to do is add ourselves to the new
     *   location. 
     */
    cloneForMultiInstanceContents(loc)
    {
        /* add myself into the new container */
        baseMoveIntoAdd(loc);
    }

    /*
     *   Add the direct containment connections for this item to a lookup
     *   table. 
     *   
     *   A MultiLoc does not, by default, connect its multiple locations
     *   together.  This means that if we're traversing in from a point of
     *   view outside the MultiLoc object, we don't add any of our other
     *   containers to the connection table.  However, the MultiLoc
     *   itself, and its contents, *can* see out to all of its locations;
     *   so if we're traversing from a point of view inside self, we will
     *   add all of our containers to the connection list. 
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
        
        /* 
         *   If we're traversing from the outside in, don't connect any of
         *   our other containers.  However, if we're traversing from our
         *   own point of view, or from a point of view inside us, we do
         *   get to see out to all of our containers.  
         */
        if (senseTmp.pointOfView == self || senseTmp.pointOfView.isIn(self))
        {
            /* add my locations */
            foreach (local cur in locationList)
            {
                if (tab[cur] == nil)
                    cur.addDirectConnections(tab);
            }
        }
    }

    /*
     *   Transmit ambient energy to my location or locations.  Note that
     *   even though we don't by default shine light from one of our
     *   containers to another, we still shine light from within me to
     *   each of our containers.  
     */
    shineOnLoc(sense, ambient, fill)
    {
        /* shine on each of my containers and their immediate children */
        foreach (local cur in locationList)
            cur.shineFromWithin(self, sense, ambient, fill);
    }


    /*
     *   Build a sense path to my location or locations.  Note that even
     *   though we don't by default connect our different containers
     *   together, we still build a sense path from within to outside,
     *   because we can see from within out to all of our containers.  
     */
    sensePathToLoc(sense, trans, obs, fill)
    {
        /* build a path to each of my containers and their children */
        foreach (local cur in locationList)
            cur.sensePathFromWithin(self, sense, trans, obs, fill);
    }


    /*
     *   Get the drop destination.  The default implementation in Thing
     *   won't work for us, because it delegates to its location to find
     *   the drop destination; we can't do that because we could have
     *   several locations.  To figure out which of our multiple locations
     *   to delegate to, we'll look for 'self' in the supplied sense path;
     *   if we can find it, and the previous path element is a container or
     *   peer of ours, then we'll delegate to that container, because it's
     *   the "side" we approached from.  If there's no path, or if we're
     *   not preceded in the path by a container of ours, we'll arbitrarily
     *   delegate to our first container.
     *   
     *   Note that when we don't have a path, or there's no container of
     *   ours preceding us in the path, the object being dropped must be
     *   starting inside us.  It would be highly unusual for this to happen
     *   with a multi-location object, because MutliLoc isn't designed for
     *   use as a "nested room" or the like.  However, it's not an
     *   impossible situation; if the game does want to create such a
     *   scenario, then the game simply needs to override this routine so
     *   that it does whatever makes sense in the game scenario.  There's
     *   no general way to handle such situations, but it should be
     *   possible to determine the correct handling for specific scenarios.
     */
    getDropDestination(obj, path)
    {
        local idx;

        /* 
         *   if there's no path, get the ordinary "touch" path from the
         *   current actor to us, since this is how the actor would reach
         *   out and touch this object 
         */
        if (path == nil)
            path = gActor.getTouchPathTo(self);
        
        /* 
         *   if there's a path, check to see if we're in it; if so, and
         *   we're not the first element, and the preceding element is a
         *   container or peer of ours, delegate to the preceding element 
         */
        if (path != nil
            && (idx = path.indexOf(self)) != nil
            && idx >= 3
            && path[idx - 1] is in (PathIn, PathPeer))
        {
            /* 
             *   we're preceded in the path by a container or peer of ours,
             *   so we know that we're approaching from that "side" -
             *   delegate to that container, since we're coming from that
             *   direction 
             */
            return path[idx - 2].getDropDestination(obj, path);
        }

        /* 
         *   We either don't have a path, or we're not preceded in the path
         *   by one of our containers or peers, so we don't have any idea
         *   which "side" we're approaching from.  This means we have no
         *   good basis for deciding where the object being dropped will
         *   fall.  Arbitrarily delegate to our first container, if we have
         *   one.  
         */
        return locationList.length() > 0
            ? locationList[1].getDropDestination(obj, path)
            : nil;
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   A "multi-instance" object is a simple way of creating copies of an
 *   object in several places.  This is often useful for decorations and
 *   other features that recur in a whole group of rooms. 
 *   
 *   You define a multi-instance object in two parts.
 *   
 *   First, you define a MultiInstance object, which is just a hollow
 *   shell of an object that sets up the location relationships.  This
 *   shell object doesn't have any presence in the game world; it's just a
 *   programming abstraction.
 *   
 *   Second, as part of the shell object, you define an example of the
 *   object that will actually show up in the game in each of the multiple
 *   locations.  You do this by defining a nested object under the
 *   'instanceObject' property of the shell object.  This is otherwise a
 *   perfectly ordinary object.  In most cases, you'll want to make this a
 *   Decoration, Fixture, or some other non-portable object class, since
 *   the "cloned" nature of these objects means that you usually won't
 *   want them moving around (if they did, you might run into situations
 *   where you had several of them in the same place, leading to
 *   disambiguation headaches for the player).
 *   
 *   Here's an example of how you set up a multi-instance object:
 *   
 *   trees: MultiInstance
 *.    locationList = [forest1, forest2, forest3]
 *.    instanceObject: Fixture { 'tree/trees' 'trees'
 *.      "Many tall, old trees grow here. "
 *.      isPlural = true
 *.    }
 *.  ;
 *   
 *   Note that the instanceObject itself has no location, because it
 *   doesn't appear in the game-world model itself - it's just a template
 *   for the real objects. 
 *   
 *   During initialization, the library will automatically create several
 *   instances (i.e., subclasses) of the example object - one instance per
 *   location, to be exact.  These instances are the real objects that
 *   show up in the game world.
 *   
 *   MultiInstance has one more helpful feature: it lets you dynamically
 *   change the set of locations where the instances appear.  You do this
 *   using the same interface that you use to move around MultiLoc objects
 *   - moveInto(), moveIntoAdd(), moveOutOf().  When you call these
 *   routines on the MultiInstance shell object, it will add and remove
 *   object instances as needed to keep everything consistent.  Thanks to
 *   a little manipulation we do on the instance objects when we set them up,
 *   you can also move the instance objects around directly using
 *   moveInto(), and they'll update the MultiInstance parent to keep its
 *   location list consistent.  
 */
class MultiInstance: BaseMultiLoc
    /* the template object */
    instanceObject = nil

    /* initialize my locations */
    initializeLocation()
    {
        /* create a copy of our template object for each of our locations */
        locationList.forEach({loc: addInstance(loc)});
    }

    /* 
     *   Move the MultiInstance into the given location.  This removes us
     *   from any other existing locations and adds us (if we're not
     *   already there) to the given location.  
     */
    moveInto(loc)
    {
        /* remove all instances that aren't in the new location */
        foreach (local cur in instanceList)
        {
            /* if this instance isn't directly in 'loc', remove it */
            if (!cur.isDirectlyIn(loc))
                cur.moveInto(nil);
        }

        /* 
         *   If I don't have an instance object in the new location, add
         *   one.  Since I've dropped every other instance already, we
         *   either have exactly one location now, which is in the new
         *   location, or we have no locations at all; so we need only
         *   check to see if we have any instances and add one in the new
         *   location if not.  
         */
        if (loc != nil && locationList.length() == 0)
            addInstance(loc);
    }

    /* 
     *   Add the new location to our set of locations.  Any existing
     *   locations are unaffected. 
     */
    moveIntoAdd(loc)
    {
        /* if I'm not already in the location, add an instance there */
        if (locationList.indexOf(loc) == nil)
            addInstance(loc);
    }

    /* 
     *   Remove me from the given location.  Other locations are
     *   unaffected.  
     */
    moveOutOf(loc)
    {
        local inst;
        
        /* find our instance that's in the given location */
        inst = getInstanceIn(loc);

        /* if we found it, remove this instance from its location */
        if (inst != nil)
            inst.moveInto(nil);
    }

    /* get our instance object (if any) that's in the given location */
    getInstanceIn(loc)
        { return instanceList.valWhich({x: x.isDirectlyIn(loc)}); }

    /* internal service routine - add an instance for a given location */
    addInstance(loc)
    {
        local inst;

        /* 
         *   Create an instance of the template object, mixing in our
         *   special instance superclass using multiple inheritance.  The
         *   MultiInstanceInstance superclass overrides the location
         *   manipulation methods so that we keep the MultiInstance parent
         *   (i.e., us) synchronized if we move around the instance object
         *   directly (by calling its moveInto() method directly, for
         *   example).  
         */
        inst = TadsObject.createInstanceOf(
            [instanceMixIn, self], [instanceObject]);

        /* add it to our list of active instances */
        instanceList.append(inst);

        /* move the instance into its new location */
        inst.moveInto(loc);
    }

    /* 
     *   If any contents are added to the MultiInstance object, they must
     *   be contents of the template object, so add them to the template
     *   object instead of the MultiInstance parent. 
     */
    addToContents(obj) { instanceObject.addToContents(obj); }

    /* 
     *   remove an object from our contents - we'll delegate this to our
     *   template object just like we delegate addToContents 
     */
    removeFromContents(obj) { instanceObject.removeFromContents(obj); }

    /* the mix-in superclass for our instance objects */
    instanceMixIn = MultiInstanceInstance

    /* our vector of active instance objects */
    instanceList = perInstance(new Vector(5))
;

/*
 *   An instance of a MultiInstance object.  This is a mix-in class that
 *   we add (using mutiple inheritance) to each instance.  This overrides
 *   the location manipulation methods, to ensure that we keep the
 *   MultiInstance parent object in sync with any changes made directly to
 *   the instance objects.
 *   
 *   IMPORTANT - the library adds this class to each instance object
 *   *automatically*.  Game code shouldn't ever have to use this class
 *   directly.  
 */
class MultiInstanceInstance: object
    construct(parent)
    {
        /* remember our MultiInstance parent object */
        miParent = parent;

        /* 
         *   clone my contents tree for the new instance, so that we have a
         *   private copy of any components within the instance 
         */
        cloneMultiInstanceContents();
    }

    /* move to a new location */
    baseMoveInto(newCont)
    {
        /* 
         *   if we currently have a location, take the location out of our
         *   MultiInstance parent's location list 
         */
        if (location != nil)
            miParent.locationList -= location;

        /* inherit the standard behavior */
        inherited(newCont);

        /* 
         *   if we have a new location, add the new location to our
         *   MultiInstance parent's location list; otherwise, drop out of
         *   the parent's instance list 
         */
        if (newCont != nil)
        {
            /* 
             *   add the new location to the parent's location list, if
             *   we're not already there 
             */
            if (miParent.locationList.indexOf(newCont) == nil)
                miParent.locationList += newCont;
        }
        else
        {
            /* 
             *   we're being removed from the game world, so remove this
             *   instance from the parent's instance list 
             */
            miParent.instanceList.removeElement(self);
        }
    }

    /* 
     *   All instances of a given MultiInstance are equivalent to one
     *   another, for parsing purposes.  
     */
    isEquivalent = true

    /* our MultiInstance parent */
    miParent = nil
;


/* ------------------------------------------------------------------------ */
/*
 *   A "multi-faceted" object is similar to a MultiInstance object, with
 *   the addition that the instance objects are "facets" of one another.
 *   This means that they have the same identity, from the perspective of
 *   a character in the scenario: all of the instance objects are part of
 *   the same conceptual object, not separate objects.
 *   
 *   This is especially useful for large objects that span multiple
 *   locations, such as a river or a long rope.
 *   
 *   You define a multi-faceted object the same way you set up a
 *   MultiInstance: definfe a MultiFaceted shell object, and as part of
 *   the shell, define the facet object using the instanceObject property.
 *   Here's an example:
 *   
 *   river: MultiFaceted
 *.    locationList = [riverBank, meadow, canyon]
 *.    instanceObject: Fixture { 'river' 'river'
 *.      "The river meanders by. "
 *.    }
 *.  ;
 *   
 *   The main difference between MultiInstance and MultiFaceted is that
 *   the "facet" objects of a MultiFaceted are related as facets of a
 *   common object from the parser's perspective.  For example, if a
 *   player refers to one facet, then travels to another location that
 *   contains a different facet, then refers to "it", the parser will
 *   realize that the pronoun refers to the new facet in the new location.
 */
class MultiFaceted: MultiInstance
    /* our instance objects represent our facets for parsing purposes */
    getFacets() { return instanceList; }

    /* the mix-in superclass for our instance objects */
    instanceMixIn = MultiFacetedFacet
;

/* 
 *   The mix-in superclass for MultiFaceted facet instances.
 *   
 *   IMPORTANT - the library adds this class to each instance object
 *   *automatically*.  Game code shouldn't ever have to use this class
 *   directly.  
 */
class MultiFacetedFacet: MultiInstanceInstance
    /* 
     *   Get our other facets for parsing purposes - our parent maintains
     *   the list of all of its facets, so simply return that list.  (Note
     *   that we'll be in the list as well, but that's harmless, so don't
     *   bother removing us.) 
     */
    getFacets() { return miParent.getFacets(); }
;

/* ------------------------------------------------------------------------ */
/*
 *   A "linkable" object is one that can participate in a master/slave
 *   relationship.  This kind of relationship means that the state of both
 *   objects in the pair is controlled by one of the objects, called the
 *   master; the other object defers to the other to get and set all of
 *   its linkable state.
 *   
 *   Note that this base class doesn't provide for the management of any
 *   of the actual linked state.  Subclasses are responsible for doing
 *   this.  The general pattern is to create a getter/setter method pair
 *   for each bit of linked state, and in these methods refer to
 *   masterObject.xxx rather than just self.xxx.
 *   
 *   This is useful for objects such as doors that have two separate
 *   objects representing the two sides of the door.  The two sides are
 *   always linked for things like open/closed and locked/unlocked state;
 *   this can be handled by linking the two sides, and managing all state
 *   of both sides in one side designated as the master.  
 */
class Linkable: object
    /*
     *   Get the master object, which holds our state.  By default, this
     *   is simply 'self', but some objects might want to override this.
     *   For example, doors are usually implemented with two separate
     *   objects, representing the two sides of the door, which share
     *   common state; in such cases, one of the pair can be designated as
     *   the master, which holds the common state of the door, and this
     *   method can be overridden so that all state operations on the lock
     *   are performed on the master side of the door.
     *   
     *   We return self by default so that a linkable object can stand
     *   alone if desired.  That is, a linkable object doesn't have to be
     *   part of a pair; it can just as well be a single object.  
     */
    masterObject()
    {
        /* 
         *   inherit from the next superclass, if possible; otherwise, use
         *   'self' as the default master object 
         */
        if (canInherit())
            return inherited();
        else
            return self;
    }

    /*
     *   We're normally mixed into a Thing; do some extra work in
     *   initialization. 
     */
    initializeThing()
    {
        /* inherit the default handling */
        inherited();

        /* 
         *   If we're tied to a separate master object, check the master
         *   object to see if it's tied back to us as its master object.
         *   Only one can be the master; if each says the other is the
         *   master, we'll get stuck in infinite loops as each tries to
         *   defer to the other.  To avoid this, break the loop by
         *   arbitrarily choosing one or the other as the master.  Note
         *   that we don't have to worry about the other object making a
         *   different decision and breaking the relationship, because if
         *   we detect the loop, it means we're going first - if the other
         *   object had gone first then it would have detected and broken
         *   the loop itself, and we wouldn't be finding the loop now.
         */
        if (masterObject != self && masterObject.masterObject == self)
        {
            /* 
             *   We're tied together in a loop - break the loop by
             *   arbitrarily electing myself as the master object.
             *   Because these relationships are symmetric, it shouldn't
             *   matter which we choose.  
             */
            masterObject = self;
        }
    }
;


/* ------------------------------------------------------------------------ */
/*
 *   A "basic openable" is an object that keeps open/closed status, and
 *   which can be linked to another object to maintain that status.  This
 *   basic class doesn't handle any special commands; it's purely for
 *   keeping track of internal open/closed state.  
 */
class BasicOpenable: Linkable
    /* 
     *   Initial open/closed setting.  Set this to true to make the object
     *   open initially.  If this object is linked to another object (as
     *   in the two sides of a door), you only need to set this property
     *   in the *master* object - the other side will automatically link
     *   up to the master object during initialization.  
     */
    initiallyOpen = nil

    /*
     *   Flag: door is open.  Travel is only possible when the door is
     *   open.  Return the master's status.
     */
    isOpen()
    {
        /* 
         *   If we're the master, simply use our isOpen_ property;
         *   otherwise, call our master's isOpen method.  This way, if the
         *   master has a different way of calculating isOpen, we'll defer
         *   to its different handling. 
         */
        if (masterObject == self)
            return isOpen_;
        else
            return masterObject.isOpen();
    }

    /*
     *   Make the object open or closed.  By default, we'll simply set the
     *   isOpen flag to the new status.  Objects can override this to
     *   apply side effects of opening or closing the object.  
     */
    makeOpen(stat)
    {
        /* 
         *   if we're the master, simply set our isOpen_ property;
         *   otherwise, defer to the master 
         */
        if (masterObject == self)
            isOpen_ = stat;
        else
            masterObject.makeOpen(stat);

        /* inherit the next superclass's handling */
        inherited(stat);
    }

    /*
     *   Open status name.  This is an adjective describing whether the
     *   object is opened or closed.  In English, this will return "open"
     *   or "closed."  
     */
    openDesc = (isOpen ? gLibMessages.openMsg(self)
                       : gLibMessages.closedMsg(self))

    /* initialization */
    initializeThing()
    {
        /* inherit the default handling */
        inherited();

        /* if we're the master, set our initial open/closed state */
        if (masterObject == self)
            isOpen_ = initiallyOpen;
    }

    /*
     *   If we're obstructing a sense path, it must be because we're
     *   closed.  Try implicitly opening.  
     */
    tryImplicitRemoveObstructor(sense, obj)
    {
        /* 
         *   If I'm not already open, try opening me.  As usual for 'try'
         *   routines, we return true if we attempt a command, nil if not.
         *   
         *   Note that we might be creating an obstruction despite already
         *   being open; in this case, we don't want to do anything, since
         *   an implied 'open' won't help when we're already open.  
         */
        return isOpen ? nil : tryImplicitAction(Open, self);
    }

    /* 
     *   if we can't reach or move something through the container, it
     *   must be because we're closed 
     */
    cannotTouchThroughMsg = &cannotTouchThroughClosedMsg
    cannotMoveThroughMsg = &cannotMoveThroughClosedMsg

    /* 
     *   Internal open/closed status.  Do not use this for initialization
     *   - set initiallyOpen in the master object instead. 
     */
    isOpen_ = nil
;

/* ------------------------------------------------------------------------ */
/*
 *   Openable: a mix-in class that can be combined with an object's other
 *   superclasses to make the object respond to the verbs "open" and
 *   "close."  We also add some extra features for other related verbs,
 *   such as a must-be-open precondition "look in" and "board".  
 */
class Openable: BasicOpenable
    /*
     *   Describe our contents using a special version of the contents
     *   lister, so that we add our open/closed status to the listing.  The
     *   message we add is given by our openStatus method, so if all you
     *   want to change is the "it's open" status message, you can just
     *   override openStatus rather than providing a whole new lister.  
     */
    descContentsLister = openableDescContentsLister

    /*
     *   Contents lister to use when we're opening the object.  This
     *   lister shows the items that are newly revealed when the object is
     *   opened. 
     */
    openingLister = openableOpeningLister

    /* 
     *   Get our "open status" message - this is a complete sentence saying
     *   that we're open or closed.  By default, in English, we just say
     *   "it's open" (adjusted for number and gender, of course).
     *   
     *   Note that this message has to be a stand-alone independent clause.
     *   In particular note that we don't put any spacing after it, since
     *   we need to be able to add sentence-ending or clause-ending
     *   punctuation immediately after it.  
     */
    openStatus() { return gLibMessages.openStatusMsg(self); }

    /*
     *   By default, an Openable that's also a Lockable must be closed to
     *   be locked.  This means that when it's open, the object is
     *   implicitly unlocked, in which case "It's unlocked" isn't worth
     *   mentioning when the description says "It's open."
     */
    lockStatusReportable = (!isOpen)

    /*
     *   Action handlers 
     */
    dobjFor(Open)
    {
        verify()
        {
            /* it makes no sense to open something that's already open */
            if (isOpen)
                illogicalAlready(&alreadyOpenMsg);
        }
        action()
        {
            local trans;
            
            /* 
             *   note the effect we have currently, while still closed, on
             *   sensing from outside into our contents 
             */
            trans = transSensingIn(sight);
            
            /* make it open */
            makeOpen(true);

            /* 
             *   make the default report - if we make a non-default
             *   report, the default will be ignored, so we don't need to
             *   worry about whether or not we'll make a non-default
             *   report now 
             */
            defaultReport(&okayOpenMsg);

            /*
             *   If the actor is outside me, and we have any listable
             *   contents, and our sight transparency is now better than it
             *   was before we were open, reveal the new contents.
             *   Otherwise, just show our default 'opened' message.
             *   
             *   As a special case, if we're running as an implied command
             *   within a LookIn or Search action on this same object,
             *   don't bother showing this result.  Doing so would be
             *   redundant with the explicit examination of the contents
             *   that we'll be doing anyway with the main action.  
             */
            if (!gActor.isIn(self)
                && transparencyCompare(transSensingIn(sight), trans) > 0
                && !(gAction.isImplicit
                     && (gAction.parentAction.ofKind(LookInAction)
                         || gAction.parentAction.ofKind(SearchAction))
                     && gAction.parentAction.getDobj() == self))
            {
                local tab;

                /* get the table of visible objects */
                tab = gActor.visibleInfoTable();
                
                /* show my contents list, if I have any */
                openingLister.showList(gActor, self, contents, ListRecurse,
                                       0, tab, nil);

                /* mark my contents as having been seen */
                setContentsSeenBy(tab, gActor);

                /* show any special contents as well */
                examineSpecialContents();
            }
        }
    }

    dobjFor(Close)
    {
        verify()
        {
            /* it makes no sense to close something that's already closed */
            if (!isOpen)
                illogicalAlready(&alreadyClosedMsg);
        }
        action()
        {
            /* make it closed */
            makeOpen(nil);

            /* show the default report */
            defaultReport(&okayCloseMsg);
        }
    }

    dobjFor(LookIn)
    {
        /* 
         *   to look in an openable object, we must be open, unless the
         *   object is transparent or the actor is inside us 
         */
        preCond
        {
            local lst;

            /* get the inherited preconditions */
            lst = nilToList(inherited());

            /* 
             *   if I'm not transparent looking in, and the actor isn't
             *   already inside me, try opening me 
             */
            if (transSensingIn(sight) != transparent && !gActor.isIn(self))
                lst += objOpen;

            /* return the result */
            return lst;
        }
    }

    dobjFor(Search)
    {
        /* 
         *   To search an openable object, we must be open - unlike LOOK
         *   IN, this applies even if the object is transparent, since
         *   SEARCH is inherently more aggressive than LOOK IN, and implies
         *   physically picking through the contents.  This doesn't apply
         *   if the actor is already inside me.  
         */
        preCond
        {
            /* get the inherited preconditions */
            local lst = nilToList(inherited());

            /* if the actor isn't in me, make sure I'm open */
            if (!gActor.isIn(self))
                lst += objOpen;

            /* 
             *   searching implies physically sifting through the contents,
             *   so we need to be able to touch the object 
             */
            lst += touchObj;

            /* return the updated list */
            return lst;
        }
    }

    /*
     *   Generate a precondition to make sure gActor can reach the interior
     *   of the container.  We consider the inside reachable if either the
     *   actor is located inside the container, or the actor is outside and
     *   the container is open.  
     */
    addInteriorReachableCond(lst)
    {
        /* 
         *   If the actor's inside us, they can reach our interior whether
         *   we're open or not, so there's no need for any additional
         *   condition.  If not, we need to be open for the actor to be
         *   able to reach our interior.  
         */
        if (!gActor.isIn(self))
            lst = nilToList(lst) + objOpen;

        /* return the result */
        return lst;
    }

    iobjFor(PutIn)
    {
        /* make sure that our interior is reachable */
        preCond { return addInteriorReachableCond(inherited()); }
    }

    iobjFor(PourInto)
    {
        /* make sure that our interior is reachable */
        preCond { return addInteriorReachableCond(inherited()); }
    }

    /* can't lock an openable that isn't closed */
    dobjFor(Lock)
    {
        preCond { return nilToList(inherited()) + objClosed; }
    }
    dobjFor(LockWith)
    {
        preCond { return nilToList(inherited()) + objClosed; }
    }

    /* must be open to get out of a nested room */
    dobjFor(GetOutOf)
    {
        preCond()
        {
            return nilToList(inherited())
                + new ObjectPreCondition(self, objOpen);
        }
    }

    /* must be open to get into a nested room */
    dobjFor(Board)
    {
        preCond()
        {
            return nilToList(inherited())
                + new ObjectPreCondition(self, objOpen);
        }
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Lockable: a mix-in class that can be combined with an object's other
 *   superclasses to make the object respond to the verbs "lock" and
 *   "unlock."  A Lockable requires no key.
 *   
 *   Note that Lockable should usually go BEFORE a Thing-derived class in
 *   the superclass list.  
 */
class Lockable: Linkable
    /* 
     *   Our initial locked state (i.e., at the start of the game).  By
     *   default, we start out locked. 
     */
    initiallyLocked = true

    /*
     *   Current locked state.  Use our isLocked_ status if we're the
     *   master, otherwise defer to the master.  
     */
    isLocked()
    {
        if (masterObject == self)
            return isLocked_;
        else
            return masterObject.isLocked();
    }

    /*
     *   Make the object locked or unlocked.  Objects can override this to
     *   apply side effects of locking or unlocking.  By default, if we're
     *   the master, we'll simply set our isLocked_ property to the new
     *   status, and otherwise defer to the master object.  
     */
    makeLocked(stat)
    {
        /* apply to self or the master object, as appropriate */
        if (masterObject == self)
            isLocked_ = stat;
        else
            masterObject.makeLocked(stat);

        /* inherit the next superclass's handling */
        inherited(stat);
    }

    /* show our status */
    examineStatus()
    {
        /* inherit the default handling */
        inherited();

        /* 
         *   if our lock status is visually apparent, and we want to
         *   mention the lock status in our current state, show the lock
         *   status 
         */
        if (lockStatusObvious && lockStatusReportable)
            say(isLocked ? gLibMessages.currentlyLocked
                         : gLibMessages.currentlyUnlocked);
    }

    /* 
     *   Description of the object's current locked state.  In English,
     *   this simply returns one of 'locked' or 'unlocked'.  (Note that
     *   this is provided as a convenience to games, for generating
     *   messages about the object that include its state.  The library
     *   doesn't use this message itself, so overriding this won't change
     *   any library messages - in particular, it won't change the
     *   examineStatus message.)  
     */
    lockedDesc = (isLocked() ? gLibMessages.lockedMsg(self)
                             : gLibMessages.unlockedMsg(self))

    /*
     *   Is our 'locked' status obvious?  This should be set to true for an
     *   object whose locked/unlocked status can be visually observed, nil
     *   for an object whose status is not visuall apparent.  For example,
     *   you can usually tell from the inside that a door is locked by
     *   looking at the position of the lock's paddle, but on the outside
     *   of a door there's usually no way to see the status.
     *   
     *   By default, since we can be locked and unlocked with simple LOCK
     *   and UNLOCK commands, we assume the status is as obvious as the
     *   mechanism must be to allow such simple commands.  
     */
    lockStatusObvious = true

    /*
     *   Is our 'locked' status reportable in our current state?  This is
     *   similar to lockStatusObvious, but serves a separate purpose: this
     *   tells us if we wish to report the lock status for aesthetic
     *   reasons.
     *   
     *   This property is primarily of interest to mix-ins.  To allow
     *   mix-ins to get a say, regardless of the order of superclasses,
     *   we'll by default defer to any inherited value if there is in fact
     *   an inherited value.  If there's no inherited value, we'll simply
     *   return true.
     *   
     *   We use this in the library for one case in particular: when we're
     *   mixed with Openable, we don't want to report the lock status for
     *   an open object because an Openable must by default be closed to be
     *   locked.  That is, when an Openable is open, it's always unlocked,
     *   so reporting that it's unlocked is essentially redundant
     *   information.  
     */
    lockStatusReportable = (canInherit() ? inherited() : true)

    /* 
     *   Internal locked state.  Do not use this to set the initial state
     *   - set initiallyLocked in the master object instead. 
     */
    isLocked_ = nil
    
    /* initialization */
    initializeThing()
    {
        /* inherit the default handling */
        inherited();
        
        /* if we're the master, set our initial state */
        if (masterObject == self)
            isLocked_ = initiallyLocked;
    }

    /*
     *   Action handling 
     */

    /* "lock" */
    dobjFor(Lock)
    {
        preCond = (nilToList(inherited()) + [touchObj])
        verify()
        {
            /* if we're already locked, there's no point in locking us */
            if (isLocked)
                illogicalAlready(&alreadyLockedMsg);
        }
        action()
        {
            /* make it locked */
            makeLocked(true);

            /* make the default report */
            defaultReport(&okayLockMsg);
        }
    }

    /* "unlock" */
    dobjFor(Unlock)
    {
        preCond = (nilToList(inherited()) + [touchObj])
        verify()
        {
            /* if we're already unlocked, there's no point in doing this */
            if (!isLocked)
                illogicalAlready(&alreadyUnlockedMsg);
        }
        action()
        {
            /* make it unlocked */
            makeLocked(nil);

            /* make the default report */
            defaultReport(&okayUnlockMsg);
        }
    }

    /* "lock with" */
    dobjFor(LockWith)
    {
        preCond = (nilToList(inherited()) + [touchObj])
        verify() { illogical(&noKeyNeededMsg); }
    }

    /* "unlock with" */
    dobjFor(UnlockWith)
    {
        preCond = (nilToList(inherited()) + [touchObj])
        verify() { illogical(&noKeyNeededMsg); }
    }

    /*
     *   Should we automatically unlock this door on OPEN?  By default, we
     *   do this only if the lock status is obvious.  
     */
    autoUnlockOnOpen = (lockStatusObvious)
    
    /* 
     *   A locked object can't be opened - apply a precondition and a check
     *   for "open" that ensures that we unlock this object before we can
     *   open it.
     *   
     *   If the lock status isn't obvious, don't try to unlock the object
     *   as a precondition.  Instead, test to make sure it's unlocked in
     *   the 'check' routine, and fail.  
     */
    dobjFor(Open)
    {
        preCond()
        {
            /* start with the inherited preconditions */
            local ret = nilToList(inherited());

            /* automatically unlock on open, if appropriate */
            if (autoUnlockOnOpen)
                ret += objUnlocked;

            /* return the result */
            return ret;
        }

        check()
        {
            /* make sure we're unlocked */
            if (isLocked)
            {
                /* let them know we're locked */
                reportFailure(&cannotOpenLockedMsg);

                /* set 'it' to me, so UNLOCK IT works */
                gActor.setPronounObj(self);

                /* we cannot proceed */
                exit;
            }

            /* inherit the default handling */
            inherited();
        }
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   A lockable that can't be locked and unlocked by direct action.  The
 *   LOCK and UNLOCK commands cannot be used with this kind of lockable.
 *   
 *   This is useful for a couple of situations.  First, it's useful when we
 *   want to create a locked object that simply can't be unlocked, such as
 *   a locked door that forms a permanent boundary of the map.  Second,
 *   it's useful for locked objects that must be unlocked by some other
 *   means, such as manipulating an external mechanism (pulling a lever,
 *   say).  In these cases, the trick is to figure out the separate means
 *   of unlocking the door, so we don't want the LOCK and UNLOCK commands
 *   to work directly.  
 */
class IndirectLockable: Lockable
    dobjFor(Lock)
    {
        check()
        {
            reportFailure(cannotLockMsg);
            exit;
        }
    }
    dobjFor(LockWith) asDobjFor(Lock)
    dobjFor(Unlock)
    {
        check()
        {
            reportFailure(cannotUnlockMsg);
            exit;
        }
    }
    dobjFor(UnlockWith) asDobjFor(Unlock)

    /*
     *   Since we can't be locked and unlocked with simple LOCK and UNLOCK
     *   commands, presume that the lock status isn't obvious.  If the
     *   alternative mechanism that locks and unlocks the object makes the
     *   current status readily apparent, this should be overridden and set
     *   to true.  
     */
    lockStatusObvious = nil

    /* the message we display in response to LOCK/UNLOCK */
    cannotLockMsg = &unknownHowToLockMsg
    cannotUnlockMsg = &unknownHowToUnlockMsg
;


/* ------------------------------------------------------------------------ */
/*
 *   LockableWithKey: a mix-in class that can be combined with an object's
 *   other superclasses to make the object respond to the verbs "lock" and
 *   "unlock," with a key as an indirect object.  A LockableWithKey cannot
 *   be locked or unlocked except with the keys listed in the keyList
 *   property.
 *   
 *   Note that LockableWithKey should usually go BEFORE a Thing-derived
 *   class in the superclass list.  
 */
class LockableWithKey: Lockable
    /*
     *   Determine if the key fits this lock.  Returns true if so, nil if
     *   not.  By default, we'll return true if the key is in my keyList.
     *   This can be overridden to use other key selection criteria.  
     */
    keyFitsLock(key) { return keyList.indexOf(key) != nil; }

    /*
     *   Determine if the key is plausibly of the right type for this
     *   lock.  This doesn't check to see if the key actually fits the
     *   lock - rather, this checks to see if the key is generally the
     *   kind of object that might plausibly be used with this lock.
     *   
     *   The point of this routine is to make this class concerned only
     *   with the abstract notion of objects that serve to lock and unlock
     *   other objects, without requiring that the key objects resemble
     *   little notched metal sticks or that the lock objects resemble
     *   cylinders with pins - or, more specifically, without requiring
     *   that all of the kinds of keys in a game remotely resemble one
     *   another.
     *   
     *   For example, one kind of "key" in a game might be a plastic card
     *   with a magnetic stripe, and the corresponding lock would be a
     *   card slot; another kind of key might the traditional notched
     *   metal stick.  Clearly, no one would ever think to use a plastic
     *   card with a conventional door lock, nor would one try to put a
     *   house key into a card slot (not with the expectation that it
     *   would actually work, anyway).  This routine is meant to
     *   facilitate this kind of distinction: the card slot can use this
     *   routine to indicate that only plastic card objects are plausible
     *   as keys, and door locks can indicate that only metal keys are
     *   plausible.
     *   
     *   This routine can be used for disambiguation and other purposes
     *   when we must programmatically select a key that is not specified
     *   or is only vaguely specified.  For example, the keyring searcher
     *   uses it so that, when we're searching for a key on a keyring to
     *   open this lock, we implicitly try only the kinds of keys that
     *   would be plausibly useful for this kind of lock.
     *   
     *   By default, we'll simply return true.  Subclasses specific to a
     *   game (such as the "card reader" base class or the "door lock"
     *   base class) can override this to discriminate among the
     *   game-specific key classes.  
     */
    keyIsPlausible(key) { return true; }

    /* the list of objects that can serve as keys for this object */
    keyList = []

    /* 
     *   The list of keys which the player knows will fit this lock.  This
     *   is used to make key disambiguation automatic once the player
     *   knows the correct key for a lock.  
     */
    knownKeyList = []

    /* 
     *   Get my known key list.  This simply returns the known key list
     *   from the known key owner. 
     */
    getKnownKeyList() { return getKnownKeyOwner().knownKeyList; }

    /* 
     *   Get the object that own our known key list.  If we explicitly have
     *   our own non-empty known key list, we own the key list; otherwise,
     *   our master object owns the list, as long as it has a non-nil key
     *   list at all.  
     */
    getKnownKeyOwner()
    {
        /* 
         *   if we have a non-empty key list, or our master object doesn't
         *   have a key list at all, use our list; otherwise, use our
         *   master object's list so use our list 
         */
        if (knownKeyList.length() != 0 || masterObject.knownKeyList == nil)
            return self;
        else
            return masterObject;
    }

    /*
     *   Flag: remember my keys after they're successfully used.  If this
     *   is true, whenever a key is successfully used to lock or unlock
     *   this object, we'll add the key to our known key list;
     *   subsequently, whenever we try to use a key in this lock, we will
     *   automatically disambiguate the key based on the keys known to
     *   work previously.
     *   
     *   Some authors might prefer not to assume that the player should
     *   remember which keys operate which locks, so this property can be
     *   changed to nil to eliminate this memory feature.  By default we
     *   set this to true, since it shouldn't generally give away any
     *   secrets or puzzles for the game to assume that a key that was
     *   used successfully once with a given lock is the one to be used
     *   subsequently with the same lock.  
     */
    rememberKnownKeys = true

    /*
     *   Determine if the player knows that the given key operates this
     *   lock.  Returns true if the key is in our known key list, nil if
     *   not.  
     */
    isKeyKnown(key) { return getKnownKeyList().indexOf(key) != nil; }

    /* 
     *   By default, the locked/unlocked status of a keyed lockable is nil.
     *   In most cases, an object that's locked and unlocked using a key
     *   doesn't have a visible indication of the status; for example, you
     *   usually can't tell just by looking at it from the outside whether
     *   or not an exterior door to a building is locked.  Usually, the
     *   only way to tell from the outside that an exterior door is locked
     *   is to try opening it and see if it opens.  
     */
    lockStatusObvious = nil

    /*
     *   Should we automatically unlock on OPEN?  We will if our inherited
     *   handling says so, OR if the current actor is carrying a key
     *   that's known to work with this object.  We automatically unlock
     *   when a known key is present as a convenience: if we have a known
     *   key, then there's no mystery in unlocking this object, and thus
     *   for playability we want to make its operation fully automatic.  
     */
    autoUnlockOnOpen()
    {
        return (inherited()
                || getKnownKeyList.indexWhich({x: x.isIn(gActor)}) != nil);
    }

    /*
     *   Action handling 
     */

    dobjFor(Lock)
    {
        preCond
        {
            /* 
             *   remove any objClosed from our precondition - since we
             *   won't actually do any locking but will instead merely ask
             *   for an indirect object, we don't want to apply the normal
             *   closed precondition here 
             */
            return inherited() - objClosed;
        }
        verify()
        {
            /* if we're already locked, there's no point in locking us */
            if (isLocked)
                illogicalAlready(&alreadyLockedMsg);
        }
        action()
        {
            /* ask for an indirect object to use as the key */
            askForIobj(LockWith);
        }
    }

    /* "unlock" */
    dobjFor(Unlock)
    {
        verify()
        {
            /* if we're not locked, there's no point in unlocking us */
            if (!isLocked)
                illogicalAlready(&alreadyUnlockedMsg);
        }
        action()
        {
            /*
             *   We need a key.  If we're running as an implied action, the
             *   player hasn't specifically proposed unlocking the object,
             *   so it's a little weird to ask a follow-up question about
             *   what key to use.  So, if the action is implicit and
             *   there's no default key, don't proceed; simply fail with an
             *   explanation.  
             */
            if (gAction.isImplicit
                && !UnlockWithAction.testRetryDefaultIobj(gAction))
            {
                /* explain that we need a key, and we're done */
                reportFailure(&unlockRequiresKeyMsg);
                return;
            }
            
            /* ask for a key */
            askForIobj(UnlockWith);
        }
    }

    /* 
     *   perform the action processing for LockWith or UnlockWith - these
     *   are highly symmetrical, in that the only thing that varies is the
     *   new lock state we establish 
     */
    lockOrUnlockAction(lock)
    {
        /* 
         *   If it's a keyring, let the keyring's action handler do the
         *   work.  Otherwise, if it's my key, lock/unlock; it's not a
         *   key, fail.  
         */
        if (gIobj.ofKind(Keyring))
        {
            /* 
             *   do nothing - let the indirect object action handler do
             *   the work 
             */
        }
        else if (keyFitsLock(gIobj))
        {
            local ko;

            /* 
             *   get the object (us or our master object) that owns the
             *   known key list 
             */
            ko = getKnownKeyOwner();
            
            /* 
             *   if the key owner remembers known keys, and it doesn't know
             *   about this working key yet, remember this in the list of
             *   known keys 
             */
            if (ko.rememberKnownKeys
                && ko.knownKeyList.indexOf(gIobj) == nil)
                ko.knownKeyList += gIobj;
            
            /* set my new state and issue a default report */
            makeLocked(lock);
            defaultReport(lock ? &okayLockMsg : &okayUnlockMsg);
        }
        else
        {
            /* the key doesn't work in this lock */
            reportFailure(&keyDoesNotFitLockMsg);
        }
    }

    /* "lock with" */
    dobjFor(LockWith)
    {
        verify()
        {
            /* if we're already locked, there's no point in locking us */
            if (isLocked)
                illogicalAlready(&alreadyLockedMsg);
        }
        action()
        {
            /* perform the generic lock/unlock action processing */
            lockOrUnlockAction(true);
        }
    }

    /* "unlock with" */
    dobjFor(UnlockWith)
    {
        verify()
        {
            /* if we're not locked, there's no point in unlocking us */
            if (!isLocked)
                illogicalAlready(&alreadyUnlockedMsg);
        }
        action()
        {
            /* perform the generic lock/unlock action processing */
            lockOrUnlockAction(nil);
        }
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   The common base class for containers and surfaces: things that have
 *   limited bulk capacities.  This class isn't usually used directly;
 *   subclasses such as Surface and Container are usually used instead.  
 */
class BulkLimiter: Thing
    /*
     *   A container can limit the cumulative amount of bulk of its
     *   contents, and the maximum bulk of any one object, using
     *   bulkCapacity and maxSingleBulk.  We count the cumulative and
     *   single-item limits separately, since we want to allow modelling
     *   some objects as so large that they won't fit in this container at
     *   all, even if the container is carrying nothing else, without
     *   limiting the number of small items we can carry.
     *   
     *   By default, we set bulkCapacity to a very large number, making
     *   the total capacity of the object essentially unlimited.  However,
     *   we set maxSingleBulk to a relatively low number - this way, if an
     *   author wants to designate certain objects as especially large and
     *   thus unable to fit in ordinary containers, the author merely
     *   needs to set the bulk of those large items to something greater
     *   than 10.  On the other hand, if an author doesn't want to worry
     *   about bulk and limited carrying capacities and simply uses
     *   library defaults for everything, we will be able to contain
     *   anything and everything.
     *   
     *   In a game that models bulk realistically, a container's bulk
     *   should generally be equal to or slightly greater than its
     *   bulkCapacity, because a container shouldn't be smaller on the
     *   outside than on the inside.  If bulkCapacity exceeds bulk, the
     *   player can work around a holding bulk limit by piling objects
     *   into the container, thus "hiding" the bulks of the contents
     *   behind the smaller bulk of the container.  
     */
    bulkCapacity = 10000
    maxSingleBulk = 10

    /* 
     *   receive notification that we're about to insert an object into
     *   this container 
     */
    notifyInsert(obj, newCont)
    {
        /* if I'm the new direct container, check our bulk limit */
        if (newCont == self)
        {
            /* 
             *   do a 'what if' test to see what would happen to our
             *   contained bulk if we moved this item into me 
             */
            obj.whatIf({: checkBulkInserted(obj)}, &moveInto, self);
        }

        /* inherit base class handling */
        inherited(obj, newCont);
    }

    /*
     *   Check to see if a proposed insertion - already tentatively in
     *   effect when this routine is called - would overflow our bulk
     *   limits.  Reports failure and exits if the inserted object would
     *   exceed our capacity. 
     */
    checkBulkInserted(insertedObj)
    {
        local objBulk;

        /* get the bulk of the inserted object itself */
        objBulk = insertedObj.getBulk();

        /*
         *   Check the object itself to see if it fits by itself.  If it
         *   doesn't, we can report the simple fact that the object is too
         *   big for the container.  
         */
        if (objBulk > maxSingleBulk || objBulk > bulkCapacity)
        {
            reportFailure(&tooLargeForContainerMsg, insertedObj, self);
            exit;
        }
            
        /* 
         *   If our contained bulk is over our maximum, don't allow it.
         *   Note that we merely need to check our current bulk within,
         *   since this routine is called with the insertion already
         *   tentatively in effect. 
         */
        if (getBulkWithin() > bulkCapacity)
        {
            reportFailure(tooFullMsg, insertedObj, self);
            exit;
        }
    }

    /* 
     *   the message property to use when we're too full to hold a new
     *   object (i.e., the object's bulk would push us over our bulk
     *   capacity limit) 
     */
    tooFullMsg = &containerTooFullMsg

    /* 
     *   the message property to use when doing something to one of our
     *   contents would make it too large to fit all by itself into this
     *   container (that is, it would cause that object's bulk to exceed
     *   our maxSingleBulk) 
     */
    becomingTooLargeMsg = &becomingTooLargeForContainerMsg

    /* 
     *   the message property to use when doing something to one of our
     *   contents would cause our overall contents to exceed our capacity 
     */
    becomingTooFullMsg = &containerBecomingTooFullMsg

    /*
     *   Check a bulk change of one of my direct contents. 
     */
    checkBulkChangeWithin(obj)
    {
        local objBulk;
        
        /* get the object's new bulk */
        objBulk = obj.getBulk();
        
        /* 
         *   if this change would cause the object to exceed our
         *   single-item bulk limit, don't allow it 
         */
        if (objBulk > maxSingleBulk || objBulk > bulkCapacity)
        {
            reportFailure(becomingTooLargeMsg, obj, self);
            exit;
        }

        /* 
         *   If our total carrying capacity is exceeded with this change,
         *   don't allow it.  Note that 'obj' is already among our
         *   contents when this routine is called, so we can simply check
         *   our current total bulk within.  
         */
        if (getBulkWithin() > bulkCapacity)
        {
            reportFailure(becomingTooFullMsg, obj, self);
            exit;
        }
    }

    /*
     *   Adjust a THROW destination.  Since we only allow a limited amount
     *   of bulk within our contents, we need to make sure the thrown
     *   object would fit if it landed here.  If it doesn't, we'll redirect
     *   the landing site to our container.  
     */
    adjustThrowDestination(thrownObj, path)
    {
        local thrownBulk = thrownObj.getBulk();
        local newBulk;
        local dest;

        /* 
         *   do a 'what if' test to test our total bulk with the projectile
         *   added to my contents 
         */
        newBulk = thrownObj.whatIf({: getBulkWithin()}, &moveInto, self);

        /* 
         *   If that exceeds our maximum bulk, or the object's bulk
         *   individually is over our limit, we can't be the landing site.
         *   In this case, defer to our location's drop destination, if it
         *   has one.  
         */
        if ((newBulk > bulkCapacity
            || thrownBulk > bulkCapacity
            || thrownBulk > maxSingleBulk)
            && location != nil
            && (dest = location.getDropDestination(thrownObj, path)) != nil)
        {
            /* 
             *   It won't fit, so defer to our container's drop
             *   destination.  Give the new destination a chance to further
             *   adjust the destination.  
             */
            return dest.adjustThrowDestination(thrownObj, path);
        }

        /* 
         *   the projectile fits, or we just can't find a container to
         *   defer to; use the original destination, i.e., self 
         */
        return self;
    }

    /*
     *   Examine my interior.  This can be used to handle the action() for
     *   LOOK IN, or for other commands appropriate to the subclass.  
     */
    examineInterior()
    {
        /* examine the interior with our normal look-in lister */
        examineInteriorWithLister(lookInLister);

        /* 
         *   Anything that the an overriding caller (a routine that called
         *   us with 'inherited') wants to add is an addendum to our
         *   description, so add a transcript marker to indicate that the
         *   main description is now finished.
         *   
         *   The important thing about this is that any message that an
         *   overriding caller wants to add is not considered part of the
         *   description, in the sense that we don't want it to suppress
         *   any default description we've already generated.  One of the
         *   transformations we apply to the transcript is to suppress any
         *   default descriptive text if there's any more specific
         *   descriptive text following (for example, we suppress "It's an
         *   ordinary <thing>" if we also are going to say "it's open" or
         *   "it contains three coins").  If we have an overriding caller
         *   who's going to add anything, then we must assume that what the
         *   caller's adding is something about the act of examining the
         *   object, rather than a description of the object, so we don't
         *   want it to suppress a default description.  
         */
        gTranscript.endDescription();
    }

    /* examine my interior, listing the contents with the given lister */
    examineInteriorWithLister(lister)
    {
        local tab;

        /* if desired, reveal any "Hidden" items concealed within */
        if (revealHiddenItems)
        {
            /* scan our contents and reveal each Hidden item */
            foreach (local cur in contents)
            {
                /* if it's a Hidden item, reveal it */
                if (cur.ofKind(Hidden))
                    cur.discover();
            }
        }

        /* get my visible sense info */
        tab = gActor.visibleInfoTable();
            
        /* show my contents, if I have any */
        lister.showList(gActor, self, contents, ListRecurse,  0, tab, nil);

        /* mark my contents as having been seen */
        setContentsSeenBy(tab, gActor);

        /* examine my special contents */
        examineSpecialContents();
    }

    /*
     *   Verify putting something new in my interior.  This is suitable
     *   for use as a verify() method for a command like PutIn or PutOn.
     *   Note that this routine assumes and requires that gDobj be the
     *   object to be added, and gIobj be self.  
     */
    verifyPutInInterior()
    {
        /* 
         *   if we haven't resolved the direct object yet, we can at least
         *   check to see if all of the potential direct objects are
         *   already in me, and rule out this indirect object as illogical
         *   if so 
         */
        if (gDobj == nil)
        {
            /* 
             *   check the tentative direct objects to see if (1) all of
             *   them are directly inside me already, or (2) all of them
             *   are at least indirectly inside me already 
             */
            if (gTentativeDobj.indexWhich(
                {x: !x.obj_.isDirectlyIn(self)}) == nil)
            {
                /*
                 *   All of the potential direct objects are already
                 *   directly inside me.  This makes this object
                 *   illogical, since there's no need to move any of these
                 *   objects into me.  
                 */
                illogicalAlready(&alreadyPutInMsg);
            }
            else if (gTentativeDobj.indexWhich(
                {x: !x.obj_.isIn(self)}) == nil)
            {
                /* 
                 *   All of the potential direct objects are already in
                 *   me, at least indirectly.  This makes this object
                 *   somewhat less likely, since we're more likely to want
                 *   to put something in here that wasn't already within.
                 *   Note that this isn't actually illogical, though,
                 *   since we could be moving something from deeper inside
                 *   me to directly inside me.  
                 */
                logicalRank(50, 'dobjs already inside');
            }
        }
        else
        {
            /* 
             *   We can't put myself in myself, obviously.  We also can't
             *   put something into any component of itself, so the command
             *   is illogical if we're a component of the direct object. 
             */
            if (gDobj == self || isComponentOf(gDobj))
                illogicalSelf(&cannotPutInSelfMsg);

            /* if it's already directly inside me, this is illogical */
            if (gDobj.isDirectlyIn(self))
                illogicalAlready(&alreadyPutInMsg);
        }

        /* 
         *   if I'm not held by the actor, give myself a slightly lower
         *   ranking than fully logical, so that objects being held are
         *   preferred 
         */
        if (!isIn(gActor))
            logicalRank(60, 'not indirectly held');
        else if (!isHeldBy(gActor))
            logicalRank(70, 'not held');
    }

    /*
     *   Flag: reveal any hidden items contained directly within me when
     *   my interior is explicitly examined, via a command such as LOOK IN
     *   <self>.  By default, we reveal our hidden contents on
     *   examination; hidden objects are in most cases meant to be more
     *   inconspicuous than actually camouflaged, so a careful, explicit
     *   examination would normally reveal them.  If our hidden objects
     *   are so concealed that even explicit examination of our interior
     *   wouldn't reveal them, set this to nil.  
     */
    revealHiddenItems = true
;


/* ------------------------------------------------------------------------ */
/*
 *   A basic container is an object that can enclose its contents.  This is
 *   the core of the Container type, but this class only has the bare-bones
 *   sense-related enclosing features, without any action implementation.
 *   This can be used for cases where an object isn't meant to have its
 *   contents be manipulable by the player (so we don't want to allow "put
 *   in" and so on), but where we do want the ability to conceal our
 *   contents when we're closed.  
 */
class BasicContainer: BulkLimiter
    /* 
     *   My current open/closed state.  By default, this state never
     *   changes, but is fixed in the object's definition; for example, a
     *   box without a lid would always be open, while a hollow glass cube
     *   would always be closed.  Our default state is open. 
     */
    isOpen = true

    /* the material that we're made of */
    material = adventium

    /* prepositional phrase for objects being put into me */
    putDestMessage = &putDestContainer

    /*
     *   Determine if I can move an object via a path through this
     *   container. 
     */
    checkMoveViaPath(obj, dest, op)
    {
        /* 
         *   if we're moving the object in or out of me, we must consider
         *   our openness and whether or not the object fits through our
         *   opening 
         */
        if (op is in (PathIn, PathOut))
        {
            /* if we're closed, we can't move anything in or out */
            if (!isOpen)
                return new CheckStatusFailure(cannotMoveThroughMsg,
                                              obj, self);

            /* if it doesn't fit through our opening, don't allow it */
            if (!canFitObjThruOpening(obj))
                return new CheckStatusFailure(op == PathIn
                                              ? &cannotFitIntoOpeningMsg
                                              : &cannotFitOutOfOpeningMsg,
                                              obj, self);
        }
        
        /* in any other cases, allow the operation */
        return checkStatusSuccess;
    }

    /* 
     *   The message property we use when we can't move an object through
     *   the containment boundary.  This is a playerActionMessages
     *   property.  
     */
    cannotMoveThroughMsg = &cannotMoveThroughContainerMsg

    /*
     *   Determine if an actor can touch an object via a path through this
     *   container.  
     */
    checkTouchViaPath(obj, dest, op)
    {
        /* 
         *   if we're reaching from inside directly to me, allow it -
         *   treat this as touching our interior, which we allow from
         *   within regardless of our open/closed status 
         */
        if (op == PathOut && dest == self)
            return checkStatusSuccess;

        /* 
         *   if we're reaching in or out of me, consider our openness and
         *   whether or not the actor's hand fits through our opening 
         */
        if (op is in (PathIn, PathOut))
        {
            /* if we're closed, we can't reach into/out of the container */
            if (!isOpen)
                return new CheckStatusFailure(cannotTouchThroughMsg,
                                              obj, self);

            /* 
             *   if the object's "hand" doesn't fit through our opening,
             *   don't allow it 
             */
            if (!canObjReachThruOpening(obj))
                return new CheckStatusFailure(op == PathIn
                                              ? &cannotReachIntoOpeningMsg
                                              : &cannotReachOutOfOpeningMsg,
                                              obj, self);
        }
        
        /* in any other cases, allow the operation */
        return checkStatusSuccess;
    }

    /* 
     *   Library message (in playerActionMessages) explaining why we can't
     *   touch an object through this container.  This is used when an
     *   actor on the outside tries to reach something on the inside, or
     *   vice versa.  
     */
    cannotTouchThroughMsg = &cannotTouchThroughContainerMsg

    /*
     *   Determine if the given object fits through our opening.  This is
     *   only called when we're open; this determines if the object can be
     *   moved in or out of this container.  By default, we'll return
     *   true; some objects might want to override this to disallow
     *   objects over a certain size from being moved in or out of this
     *   container.
     *   
     *   Note that this method doesn't care whether or not the object can
     *   actually fit inside the container once through the opening; we
     *   only care about whether or not the object can fit through the
     *   opening itself.  This allows for things like narrow-mouthed
     *   bottles which have greater capacity within than in their
     *   openings.  
     */
    canFitObjThruOpening(obj) { return true; }

    /*
     *   Determine if the given object can "reach" through our opening,
     *   for the purposes of touching an object on the other side of the
     *   opening.  This is used to determine if the object, which is
     *   usually an actor, can its "hand" (or whatever appendange 'obj'
     *   uses to reach things) through our opening.  This is only called
     *   when we're open.  By default, we'll simply return true.
     *   
     *   This differs from canFitObjThruOpening() in that we don't care if
     *   all of 'obj' is able to fit through the opening; we only care
     *   whether obj's hand (or whatever it uses for reaching) can fit.  
     */
    canObjReachThruOpening(obj) { return true; }

    /*
     *   Determine how a sense passes to my contents.  If I'm open, the
     *   sense passes through directly, since there's nothing in the way.
     *   If I'm closed, the sense must pass through my material.  
     */
    transSensingIn(sense)
    {
        if (isOpen)
        {
            /* I'm open, so the sense passes through without interference */
            return transparent;
        }
        else
        {
            /* I'm closed, so the sense must pass through my material */
            return material.senseThru(sense);
        }
    }

    /*
     *   Get my fill medium.  If I'm open, inherit my parent's medium,
     *   assuming that the medium behaves like fog or smoke and naturally
     *   disperses to fill any nested open containers.  If I'm closed, I
     *   am by default filled with no medium.  
     */
    fillMedium()
    {
        if (isOpen && location != nil)
        {
            /* I'm open, so return my location's medium */
            return location.fillMedium();
        }
        else
        {
            /* 
             *   I'm closed, so we're cut off from the parent - assume
             *   we're filled with nothing 
             */
            return nil;
        }
    }

    /*
     *   Display a message explaining why we are obstructing a sense path
     *   to the given object.
     */
    cannotReachObject(obj)
    {
        /* 
         *   We must be obstructing by containment.  Show an appropriate
         *   message depending on whether the object is inside me or not -
         *   if not, then the actor trying to reach the object must be
         *   inside me. 
         */
        if (obj.isIn(self))
            gLibMessages.cannotReachContents(obj, self);
        else
            gLibMessages.cannotReachOutside(obj, self);
    }

    /* explain why we can't see the source of a sound */
    cannotSeeSoundSource(obj)
    {
        /* we must be obstructing by containment */
        if (obj.isIn(self))
            gLibMessages.soundIsFromWithin(obj, self);
        else
            gLibMessages.soundIsFromWithout(obj, self);
    }

    /* explain why we can't see the source of an odor */
    cannotSeeSmellSource(obj)
    {
        /* we must be obstructing by containment */
        if (obj.isIn(self))
            gLibMessages.smellIsFromWithin(obj, self);
        else
            gLibMessages.smellIsFromWithout(obj, self);
    }

    /* message when an object is too large (all by itself) to fit in me */
    tooLargeForContainerMsg = &tooLargeForContainerMsg
;

/* ------------------------------------------------------------------------ */
/*
 *   Container: an object that can have other objects placed within it.  
 */
class Container: BasicContainer
    /*
     *   Our fixed "look in" description, if any.  This is shown on LOOK
     *   IN before our normal listing of our portable contents; it can be
     *   used to describe generally what the interior looks like, for
     *   example.  By default, we show nothing here.  
     */
    lookInDesc = nil

    /* 
     *   Show our status for "examine".  This shows our open/closed status,
     *   and lists our contents. 
     */
    examineStatus()
    {
        /* show any special container-specific status */
        examineContainerStatus();

        /* inherit the default handling to show my contents */
        inherited();
    }

    /*
     *   mention my open/closed status for Examine processing 
     */
    examineContainerStatus()
    {
        /*
         *   By default, show nothing extra.  This can be overridden by
         *   subclasses as needed to show any extra status before our
         *   contents list.  
         */
    }

    /*
     *   Try putting an object into me when I'm serving as a bag of
     *   holding.  For a container, this simply does a "put obj in bag".  
     */
    tryPuttingObjInBag(target)
    {
        /* if the object won't fit all by itself, don't even try */
        if (target.getBulk() > maxSingleBulk)
            return nil;

        /* if we can't fit the object with other contents, don't try */
        if (target.whatIf({: getBulkWithin() > bulkCapacity},
                          &moveInto, self))
            return nil;

        /* we're a container, so use "put in" to get the object */
        return tryImplicitActionMsg(&announceMoveToBag, PutIn, target, self);
    }

    /* 
     *   Try moving an object into this container.  For a container, this
     *   performs a PUT IN command to move the object into self.  
     */
    tryMovingObjInto(obj) { return tryImplicitAction(PutIn, obj, self); }

    /* -------------------------------------------------------------------- */
    /*
     *   "Look in" 
     */
    dobjFor(LookIn)
    {
        verify() { }
        check()
        {
            /* 
             *   If I'm closed, and I can't see my contents when closed, we
             *   can't go on.  Unless, of course, the actor is inside us,
             *   in which case our external boundary isn't relevant.  
             */
            if (!isOpen
                && transSensingIn(sight) == opaque
                && !gActor.isIn(self))
            {
                /* we can't see anything because we're closed */
                reportFailure(&cannotLookInClosedMsg);
                exit;
            }
        }
        action()
        {
            /* show our fixed "look in" description, if any */
            lookInDesc;

            /* examine my interior */
            examineInterior();
        }
    }

    /*
     *   "Search".  This is mostly like Open, except that the actor has to
     *   be able to reach into the object, not just see into it - searching
     *   implies a more thorough sort of examination, usually including
     *   physically poking through the object's contents.  
     */
    dobjFor(Search)
    {
        preCond = (nilToList(inherited()) + [touchObj])
        check()
        {
            /* 
             *   if I'm closed, and the actor isn't inside me, make sure my
             *   contents are reachable from the outside 
             */
            if (!isOpen
                && transSensingIn(touch) != transparent
                && !gActor.isIn(self))
            {
                /* we can't search an object that we can't reach into */
                reportFailure(&cannotTouchThroughMsg, gActor, self);
                exit;
            }
        }
    }
    

    /* -------------------------------------------------------------------- */
    /*
     *   Put In processing.  A container can accept new contents. 
     */

    iobjFor(PutIn)
    {
        verify()
        {
            /* use the standard verification for adding new contents */
            verifyPutInInterior();
        }

        action()
        {
            /* move the direct object into me */
            gDobj.moveInto(self);
            
            /* issue our default acknowledgment of the command */
            defaultReport(&okayPutInMsg);
        }
    }
;


/*
 *   A "restricted holder" is a generic mix-in class for various container
 *   types (Containers, Surfaces, Undersides, RearContainers, RearSurfaces)
 *   that adds a restriction to what can be contained.  
 */
class RestrictedHolder: object
    /* 
     *   A list of acceptable items for the container.  This list can be
     *   used to identify the objects that can be put in the container (or
     *   on the surface, under the underside, or behind the rear container
     *   or surface).  
     */
    validContents = []

    /*
     *   Is the given object allowed to go in this container (or
     *   on/under/behind it, as appropriate for the type)?  Returns true if
     *   so, nil if not.  By default, we'll return true if the object is
     *   found in our validContents list, nil if not.  This can be
     *   overridden if a subclass wants to determine which objects are
     *   acceptable with some other kind of per-object test; for example, a
     *   subclass might accept only objects of a given class as contents,
     *   or might accept only contents with some particular attribute.  
     */
    canPutIn(obj) { return validContents.indexOf(obj) != nil; }

    /*
     *   Check a PUT IN/ON/UNDER/BEHIND action to ensure that the direct
     *   object is in our approved-contents list.  
     */
    checkPutDobj(msgProp)
    {
        /* validate the direct object */
        if (!canPutIn(gDobj))
        {
            /* explain the problem */
            reportFailure(self.(msgProp)(gDobj));

            /* terminate the command */
            exit;
        }
    }
;


/*
 *   A special kind of container that only accepts specific contents.  The
 *   acceptable contents can be specified by a list of enumerated items,
 *   or by a method that indicates whether or not an item is allowed.  
 */
class RestrictedContainer: RestrictedHolder, Container
    /* 
     *   A message that explains why the direct object can't be put in this
     *   container.  In most cases, the rather generic default message
     *   should be overridden to provide a specific reason that the dobj
     *   can't be put in this object.  The rejected object is provided as a
     *   parameter in case the message needs to vary by object, but we
     *   ignore this and just use a single blanket failure message by
     *   default.  
     */
    cannotPutInMsg(obj) { return &cannotPutInRestrictedMsg; }

    /* override PutIn to enforce our contents restriction */
    iobjFor(PutIn) { check() { checkPutDobj(&cannotPutInMsg); } }
;

/*
 *   A single container is a special kind of container that can only
 *   contain a single item.  If another object is put into this container,
 *   we'll remove any current contents.  
 */
class SingleContainer: Container
    /* override PutIn to enforce our single-contents rule */
    iobjFor(PutIn)
    {
        preCond { return inherited() + objEmpty; }
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   OpenableContainer: an object that can contain things, and which can
 *   be opened and closed.  
 */
class OpenableContainer: Openable, Container
;

/* ------------------------------------------------------------------------ */
/*
 *   LockableContainer: an object that can contain things, and that can be
 *   opened and closed as well as locked and unlocked.  
 */
class LockableContainer: Lockable, OpenableContainer
;

/* ------------------------------------------------------------------------ */
/*
 *   KeyedContainer: an openable container that can be locked and
 *   unlocked, but only with a specified key.  
 */
class KeyedContainer: LockableWithKey, OpenableContainer
;


/* ------------------------------------------------------------------------ */
/*
 *   Surface: an object that can have other objects placed on top of it.
 *   A surface is essentially the same as a regular container, but the
 *   contents of a surface behave as though they are on the surface's top
 *   rather than contained within the object.  
 */
class Surface: BulkLimiter
    /* 
     *   Our fixed LOOK IN description.  This is shown in response to LOOK
     *   IN before we list our portable contents; it can be used to show
     *   generally what the surface looks like.  By default, we say
     *   nothing here.  
     */
    lookInDesc = nil

    /* my contents lister */
    contentsLister = surfaceContentsLister
    descContentsLister = surfaceDescContentsLister
    lookInLister = surfaceLookInLister
    inlineContentsLister = surfaceInlineContentsLister

    /* 
     *   we're a surface, so taking something from me that's not among my
     *   contents shows the message as "that's not on the iobj" 
     */
    takeFromNotInMessage = &takeFromNotOnMsg

    /* 
     *   my message indicating that another object x cannot be put into me
     *   because I'm already in x 
     */
    circularlyInMessage = &circularlyOnMsg

    /* message phrase for objects put into me */
    putDestMessage = &putDestSurface

    /* message when we're too full for another object */
    tooFullMsg = &surfaceTooFullMsg

    /* 
     *   Try moving an object into this container.  For a surface, this
     *   performs a PUT ON command to move the object onto self.  
     */
    tryMovingObjInto(obj) { return tryImplicitAction(PutOn, obj, self); }

    /* -------------------------------------------------------------------- */
    /*
     *   Put On processing 
     */
    iobjFor(PutOn)
    {
        verify()
        {
            /* use the standard put-in verification */
            verifyPutInInterior();
        }

        action()
        {
            /* move the direct object onto me */
            gDobj.moveInto(self);
            
            /* issue our default acknowledgment */
            defaultReport(&okayPutOnMsg);
        }
    }

    /*
     *   Looking "in" a surface simply shows the surface's contents. 
     */
    dobjFor(LookIn)
    {
        verify() { }
        action()
        {
            /* show our fixed lookInDesc */
            lookInDesc;

            /* show our contents */
            examineInterior();
        }
    }

    /* use the PUT ON forms of the verifier messages */
    cannotPutInSelfMsg = &cannotPutOnSelfMsg
    alreadyPutInMsg = &alreadyPutOnMsg
;

/*
 *   A special kind of surface that only accepts specific contents.
 */
class RestrictedSurface: RestrictedHolder, Surface
    /* 
     *   A message that explains why the direct object can't be put on this
     *   surface.  In most cases, the rather generic default message should
     *   be overridden to provide a specific reason that the dobj can't be
     *   put on this surface.  The rejected object is provided as a
     *   parameter in case the message needs to vary by object, but we
     *   ignore this and just use a single blanket failure message by
     *   default.  
     */
    cannotPutOnMsg(obj) { return &cannotPutOnRestrictedMsg; }

    /* override PutOn to enforce our contents restriction */
    iobjFor(PutOn) { check() { checkPutDobj(&cannotPutOnMsg); } }
;

/* ------------------------------------------------------------------------ */
/*
 *   Food - something you can eat.  By default, when an actor eats a food
 *   item, the item disappears.  
 */
class Food: Thing
    dobjFor(Taste)
    {
        /* tasting food is perfectly logical */
        verify() { }
    }

    dobjFor(Eat)
    {
        verify() { }
        action()
        {
            /* describe the consumption */
            defaultReport(&okayEatMsg);

            /* the object disappears */
            moveInto(nil);
        }
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   OnOffControl - a generic control that can be turned on and off.  We
 *   keep track of an internal on/off state, and recognize the commands
 *   "turn on" and "turn off".  
 */
class OnOffControl: Thing
    /*
     *   The current on/off setting.  We'll start in the 'off' position by
     *   default.  
     */
    isOn = nil

    /*
     *   On/off status name.  This returns the appropriate name ('on' or
     *   'off' in English) for our current status. 
     */
    onDesc = (isOn ? gLibMessages.onMsg(self) : gLibMessages.offMsg(self))

    /*
     *   Change our on/off setting.  Subclasses can override this to apply
     *   any side effects of changing the value. 
     */
    makeOn(val)
    {
        /* remember the new value */
        isOn = val;
    }

    dobjFor(TurnOn)
    {
        verify()
        {
            /* if it's already on, complain */
            if (isOn)
                illogicalAlready(&alreadySwitchedOnMsg);
        }
        action()
        {
            /* set to 'on' and generate a default report */
            makeOn(true);
            defaultReport(&okayTurnOnMsg);
        }
    }

    dobjFor(TurnOff)
    {
        verify()
        {
            /* if it's already off, complain */
            if (!isOn)
                illogicalAlready(&alreadySwitchedOffMsg);
        }
        action()
        {
            /* set to 'off' and generate a default report */
            makeOn(nil);
            defaultReport(&okayTurnOffMsg);
        }
    }
;

/*
 *   Switch - a simple extension of the generic on/off control that can be
 *   used with a "switch" command without specifying "on" or "off", and
 *   treats "flip" synonymously.  
 */
class Switch: OnOffControl
    /* "switch" with no specific new setting - reverse our setting */
    dobjFor(Switch)
    {
        verify() { }
        action()
        {
            /* reverse our setting and generate a report */
            makeOn(!isOn);
            defaultReport(isOn ? &okayTurnOnMsg : &okayTurnOffMsg);
        }
    }

    /* "flip" is the same as "switch" for our purposes */
    dobjFor(Flip) asDobjFor(Switch)
;

/* ------------------------------------------------------------------------ */
/*
 *   Settable - an abstract class for things you can set to different
 *   settings; the settings can be essentially anything, such as numbers
 *   (or other markers) on a dial, or stops on a sliding switch. 
 */
class Settable: Thing
    /*
     *   Our current setting.  This is an arbitrary string value.  The
     *   value initially assigned here is our initial setting; we'll
     *   update this whenever we're set to another setting.  
     */
    curSetting = '1'

    /*
     *   Canonicalize a proposed setting.  This ensures that the setting is
     *   in a specific primary format when there are superficially
     *   different ways of expressing the same value.  For example, if the
     *   setting is numeric, this could do things like trim off leading
     *   zeros; for a text value, it could ensure the value is in the
     *   proper case.  
     */
    canonicalizeSetting(val)
    {
        /* 
         *   by default, we don't have any special canonical format, so
         *   just return the value as it is 
         */
        return val;
    }

    /*
     *   Change our setting.  This is always called with the canonical
     *   version of the new setting, as returned by canonicalizeSetting().
     *   Subclasses can override this routine to apply any side effects of
     *   changing the value.  
     */
    makeSetting(val)
    {
        /* remember the new value */
        curSetting = val;
    }

    /*
     *   Is the given text a valid setting?  Returns true if so, nil if
     *   not.  This should not display any messages; simply indicate
     *   whether or not the setting is valid.
     *   
     *   This is always called with the *canonical* value of the proposed
     *   new setting, as returned by canonicalizeSetting().  
     */
    isValidSetting(val)
    {
        /* 
         *   By default, allow anything; subclasses should override to
         *   enforce our valid set of values.  
         */
        return true;
    }

    /*
     *   "set <self>" action 
     */
    dobjFor(Set)
    {
        verify() { logicalRank(150, 'settable'); }
        action() { askForLiteral(SetTo); }
    }

    /*
     *   "set <self> to <literal>" action
     */
    dobjFor(SetTo)
    {
        preCond = [touchObj]
        verify()
        {
            local txt;
            
            /* 
             *   If we already know our literal text, and it's not valid,
             *   reduce the logicalness.  Don't actually make it
             *   illogical, as it's probably still more logical to set a
             *   settable to an invalid setting than to set something that
             *   isn't settable at all.  
             */
            if ((txt = gAction.getLiteral()) != nil
                && !isValidSetting(canonicalizeSetting(txt)))
                logicalRank(50, 'invalid setting');
        }
        check()
        {
            /* if the setting is not valid, don't allow it */
            if (!isValidSetting(canonicalizeSetting(gAction.getLiteral())))
            {
                /* there is no such setting */
                reportFailure(setToInvalidMsgProp);
                exit;
            }
        }
        action()
        {
            /* set the new value */
            makeSetting(canonicalizeSetting(gAction.getLiteral()));

            /* remark on the change */
            defaultReport(okaySetToMsgProp, curSetting);
        }
    }

    /* our message property for an invalid setting */
    setToInvalidMsgProp = &setToInvalidMsg

    /* our message property for acknowledging a new setting */
    okaySetToMsgProp = &okaySetToMsg
;

/*
 *   Dial - something you can turn to different settings.  Note that dials
 *   are usually used as components of larger objects; since our base
 *   class is the basic Settable, component dials should be created to
 *   inherit multiply from Dial and Component, in that order.
 *   
 *   This is almost hte same as a regular Settable; the only thing we add
 *   is that we make "turn <self> to <literal>" equivalent to "set <self>
 *   to <literal>", as this is the verb most people would use to set a
 *   dial.  
 */
class Dial: Settable
    /* "turn" with no destination - indicate that we need a setting */
    dobjFor(Turn)
    {
        verify() { illogical(&mustSpecifyTurnToMsg); }
    }

    /* treat "turn <self> to <literal>" the same as "set to" */
    dobjFor(TurnTo) asDobjFor(SetTo)

    /* refer to setting the dial as turning it in our messages */
    setToInvalidMsgProp = &turnToInvalidMsg
    okaySetToMsgProp = &okayTurnToMsg
;

/*
 *   Numbered Dial - something you can turn to a range of numeric values. 
 */
class NumberedDial: Dial
    /*
     *   The range of settings - the dial can be set to values from the
     *   minimum to the maximum, inclusive. 
     */
    minSetting = 1
    maxSetting = 10

    /*
     *   Canonicalize a proposed setting value.  For numbers, strip off any
     *   leading zeros, since these don't change the meaning of the value.
     */
    canonicalizeSetting(val)
    {
        local num;
        
        /* try parsing it as a digit string or a spelled-out number */
        if ((num = parseInt(val)) != nil)
        {
            /* 
             *   we parsed it successfully - return the string
             *   representation of the numeric value 
             */
            return toString(num);
        }

        /* it didn't parse as a number, so just return it as-is */
        return val;
    }

    /* 
     *   Check a setting for validity.  A setting is valid only if it's a
     *   number within the allowed range for the dial. 
     */
    isValidSetting(val)
    {
        local num;
        
        /* if it doesn't look like a number, it's not valid */
        if (rexMatch('<digit>+', val) != val.length())
            return nil;

        /* get the numeric value */
        num = toInteger(val);

        /* it's valid if it's within range */
        return num >= minSetting && num <= maxSetting;
    }
;

/*
 *   Labeled Dial - something you can turn to a set of arbitrary text
 *   labels. 
 */
class LabeledDial: Dial
    /*
     *   The list of valid settings.  Each entry in this list should be a
     *   string value.  We ignore the case of these labels (we convert
     *   everything to upper-case when comparing labels).  
     */
    validSettings = []

    /*
     *   Canonicalize the setting.  We consider case insignificant in
     *   matching our labels, but the canonical version of a setting is the
     *   one that appears in the validSettings list - so if the player
     *   types in SET DIAL TO EXTRA LOUD, and the validSettings list
     *   contains 'Extra Loud', we'll want to convert the 'EXTRA LOUD' to
     *   the capitalization of the validSettings entry.  
     */
    canonicalizeSetting(val)
    {
        local txt;
        
        /* 
         *   convert it to upper-case, so that we can compare it to our
         *   valid labels without regard to case
         */
        txt = val.toUpper();

        /* 
         *   if we find a match in the validSettings list, return the match
         *   from the list, since that's the canonical format 
         */
        if ((txt = validSettings.valWhich({x: x.toUpper() == txt})) != nil)
            return txt;

        /* we didn't find a match, so leave the original value unchanged */
        return val;
    }

    /* 
     *   Check a setting for validity.  A setting is valid only if it
     *   appears in the validSettings list for this dial.
     */
    isValidSetting(val)
    {
        /* 
         *   If the given value appears in our validSettings list, it's a
         *   valid setting; otherwise, it's not valid.  Ignore case when
         *   comparing values by converting the valid labels to upper case;
         *   we've already converted the value we're testing to upper case,
         *   so the case mix won't matter in our comparison.
         *   
         *   Note that we're handed a canonical setting value, so we don't
         *   have to worry about case differences.  
         */
        return validSettings.indexOf(val) != nil;
    }
;


/* ------------------------------------------------------------------------ */
/*
 *   Button - something you can push to activate, as a control for a
 *   mechanical device.  
 */
class Button: Thing
    dobjFor(Push)
    {
        verify() { }
        action()
        {
            /* 
             *   individual buttons should override this to carry out any
             *   special action for the button; by default, we'll just
             *   show a simple acknowledgment  
             */
            defaultReport(&okayPushButtonMsg);
        }
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Lever - something you can push, pull, or move, generally as a control
 *   for a mechanical device.  Our basic lever has two states, "pushed"
 *   and "pulled".  
 */
class Lever: Thing
    /*
     *   The current state.  We have two states: "pushed" and "pulled".
     *   We start in the pushed state, so the lever can initially be
     *   pulled, since "pull" is the verb most people would first think to
     *   apply to a lever.  
     */
    isPulled = nil

    /* 
     *   Set the state.  This can be overridden to apply side effects as
     *   needed. 
     */
    makePulled(pulled)
    {
        /* note the new state */
        isPulled = pulled;
    }

    /*
     *   Action handlers.  We handle push and pull, and we treat "move" as
     *   equivalent to whichever of push or pull is appropriate to reverse
     *   the current state.  
     */
    dobjFor(Push)
    {
        verify()
        {
            /* if it's already pushed, pushing it again makes no sense */
            if (!isPulled)
                illogicalAlready(&alreadyPushedMsg);
        }
        action()
        {
            /* set the new state to pushed (i.e., not pulled) */
            makePulled(nil);

            /* make the default report */
            defaultReport(&okayPushLeverMsg);
        }
    }
    dobjFor(Pull)
    {
        verify()
        {
            /* if it's already pulled, pulling it again makes no sense */
            if (isPulled)
                illogicalAlready(&alreadyPulledMsg);
        }
        action()
        {
            /* set the new state to pulled */
            makePulled(true);

            /* make the default report */
            defaultReport(&okayPullLeverMsg);
        }
    }
    dobjFor(Move)
    {
        verify() { }
        check()
        {
            /* run the check for pushing or pulling, as appropriate */
            if (isPulled)
                checkDobjPush();
            else
                checkDobjPull();
        }
        action()
        {
            /* if we're pulled, push the lever; otherwise pull it */
            if (isPulled)
                actionDobjPush();
            else
                actionDobjPull();
        }
    }
;

/*
 *   A spring-loaded lever is a lever that bounces back to its starting
 *   position after being pulled.  This is essentially equivalent in terms
 *   of functionality to a button, but can at least provide superficial
 *   variety.  
 */
class SpringLever: Lever
    dobjFor(Pull)
    {
        action()
        {
            /*
             *   Individual objects should override this to perform the
             *   appropriate action when the lever is pulled.  By default,
             *   we'll do nothing except show a default report. 
             */
            defaultReport(&okayPullSpringLeverMsg);
        }
    }
;
    

/* ------------------------------------------------------------------------ */
/*
 *   An item that can be worn
 */
class Wearable: Thing
    /* is the item currently being worn? */
    isWorn()
    {
        /* it's being worn if the wearer is non-nil */
        return wornBy != nil;
    }

    /* 
     *   make the item worn by the given actor; if actor is nil, the item
     *   isn't being worn by anyone 
     */
    makeWornBy(actor)
    {
        /* remember who's wearing the item */
        wornBy = actor;
    }

    /*
     *   An item being worn is not considered to be held in the wearer's
     *   hands. 
     */
    isHeldBy(actor)
    {
        if (isWornBy(actor))
        {
            /* it's being worn by the actor, so it's not also being held */
            return nil;
        }
        else
        {
            /* 
             *   it's not being worn by this actor, so use the default
             *   interpretation of being held 
             */
            return inherited(actor);
        }
    }

    /*
     *   A wearable is not considered held by an actor when it is being
     *   worn, so we must do a what-if test for removing the item if the
     *   actor is currently wearing the item.  If the actor isn't wearing
     *   the item, we can use the default test of moving the item into the
     *   actor's inventory.  
     */
    whatIfHeldBy(func, newLoc)
    {
        /*
         *   If the article is being worn, and it's already in the same
         *   location we're moving it to, simply test with the article no
         *   longer being worn.  Otherwise, inherit the default handling.  
         */
        if (location == newLoc && wornBy != nil)
            return whatIf(func, &wornBy, nil);
        else
            return inherited(func, newLoc);
    }

    /*
     *   Try making the current command's actor hold me.  If I'm already
     *   directly in the actor's inventory and I'm being worn, we'll try a
     *   'doff' command; otherwise, we'll use the default handling.  
     */
    tryHolding()
    {
        /*   
         *   Try an implicit 'take' command.  If the actor is carrying the
         *   object indirectly, make the command "take from" instead,
         *   since what we really want to do is take the object out of its
         *   container.  
         */
        if (location == gActor && isWornBy(gActor))
            return tryImplicitAction(Doff, self);
        else
            return inherited();
    }

    /* 
     *   The object wearing this object, if any; if I'm not being worn,
     *   this is nil.  The wearer should always be a container (direct or
     *   indirect) of this object - in order to wear something, you must
     *   be carrying it.  In most cases, the wearer should be the direct
     *   container of the object.
     *   
     *   The reason we keep track of who's wearing the object (rather than
     *   simply keeping track of whether it's being worn) is to allow for
     *   cases where an actor is carrying another actor.  Since this
     *   object will be (indirectly) inside both actors in such cases, we
     *   would have to inspect intermediate containers to determine
     *   whether or not the outer actor was wearing the object if we
     *   didn't keep track of the wearer directly.  
     */
    wornBy = nil

    /* am I worn by the given object? */
    isWornBy(actor)
    {
        return wornBy == actor;
    }

    /*
     *   An article of clothing that is being worn by an actor does not
     *   typically encumber the actor at all, so by default we'll return
     *   zero if we're being worn by the actor, and our normal bulk
     *   otherwise.  
     */
    getEncumberingBulk(actor)
    {
        /* 
         *   if we're being worn by the actor, we create no encumbrance at
         *   all; otherwise, return our normal bulk 
         */
        return isWornBy(actor) ? 0 : getBulk();
    }

    /*
     *   An article of clothing typically encumbers an actor with the same
     *   weight whether or not the actor is wearing the item.  However,
     *   this might not apply to all objects; a suit of armor, for
     *   example, might be slightly less encumbering in terms of weight
     *   when worn than it is when held because the distribution of weight
     *   is more manageable when worn.  By default, we simply return our
     *   normal weight, whether worn or not; subclasses can override as
     *   needed to differentiate.  
     */
    getEncumberingWeight(actor)
    {
        return getWeight();
    }

    /* get my state */
    getState = (isWorn() ? wornState : unwornState)

    /* my list of possible states */
    allStates = [wornState, unwornState]


    /* -------------------------------------------------------------------- */
    /*
     *   Action processing 
     */

    dobjFor(Wear)
    {
        preCond = [objHeld]
        verify()
        {
            /* make sure the actor isn't already wearing the item */
            if (isWornBy(gActor))
                illogicalAlready(&alreadyWearingMsg);
        }
        action()
        {
            /* make the item worn and describe what happened */
            makeWornBy(gActor);
            defaultReport(&okayWearMsg);
        }
    }

    dobjFor(Doff)
    {
        preCond = [roomToHoldObj]
        verify()
        {
            /* 
             *   Make sure the actor is actually wearing the item.  If
             *   they're not, it's illogical, but if they are, it's an
             *   especially likely thing to remove. 
             */
            if (!isWornBy(gActor))
                illogicalAlready(&notWearingMsg);
            else
                logicalRank(150, 'worn');
        }
        action()
        {
            /* un-wear the item and describe what happened */
            makeWornBy(nil);
            defaultReport(&okayDoffMsg);
        }
    }

    /* "remove <wearable>" is the same as "doff <wearable>" */
    dobjFor(Remove) asDobjFor(Doff)

    /* 
     *   if a wearable is being worn, showing it off to someone doesn't
     *   require holding it 
     */
    dobjFor(ShowTo)
    {
        preCond()
        {
            /* get the standard handling */
            local lst = inherited();

            /* if we're being worn, don't require us to be held */
            if (isWornBy(gActor))
                lst -= objHeld;

            /* return the result */
            return lst;
        }
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   An item that can provide light.
 *   
 *   Any Thing can provide light, but this class should be used for
 *   objects that explicitly serve as light sources from the player's
 *   perspective.  Objects of this class display a "providing light"
 *   status message in inventory listings, and can be turned on and off
 *   via the isLit property.  
 */
class LightSource: Thing
    /* is the light source currently turned on? */
    isLit = true

    /*
     *   Turn the light source on or off.  Note that we don't have to make
     *   any special check for a change to the light level, because the
     *   main action handler always checks for a change in light/dark
     *   status over the course of the turn.  
     */
    makeLit(lit)
    {
        /* change the status */
        isLit = lit;
    }

    /* 
     *   We can distinguish light sources according to their isLit status.
     *   Give the lit/unlit distinction higher priority than the normal
     *   ownership/containment distinction. 
     */
    distinguishers = [basicDistinguisher, litUnlitDistinguisher,
                      ownershipDistinguisher, locationDistinguisher]

    /* the brightness that the object has when it is on and off */
    brightnessOn = 3
    brightnessOff = 0

    /* 
     *   return the appropriate on/off brightness, depending on whether or
     *   not we're currently lit 
     */
    brightness { return isLit ? brightnessOn : brightnessOff; }

    /* get our current state: lit or unlit */
    getState = (brightness > 1 ? lightSourceStateOn : lightSourceStateOff)

    /* get our set of possible states */
    allStates = [lightSourceStateOn, lightSourceStateOff]
;

/*
 *   A Flashlight is a special kind of light source that can be switched
 *   on and off.
 *   
 *   To create a limited-use flashlight (with a limited battery life, for
 *   example), you can combine this class with FueledLightSource.  The
 *   flashlight's on/off switch status is a separate property from its
 *   lit/unlit light-source status, so combining Flashlight with
 *   FueledLightSource will actually allow the two to become decoupled: a
 *   flashlight can be on without providing light, when the battery is
 *   dead.  For this reason, you might want to override the decription,
 *   and possibly the TurnOn action() handler, to customize the messages
 *   for the case when the flashlight is switched on but out of power.  
 */
class Flashlight: LightSource, Switch
    /* our switch status - start in the 'off' position */
    isOn = nil

    /* 
     *   Change the on/off status.  Note that switching the flashlight on
     *   or off should always be done via makeOn - the makeLit inherited
     *   from the LightSource should never be called directly on a
     *   Flashlight object, because it doesn't keep the switch on/off and
     *   flashlight lit/unlit status in sync.  This routine is the one to
     *   call because it keeps everything properly synchronized.  
     */
    makeOn(stat)
    {
        /* inherit the default handling */
        inherited(stat);

        /* 
         *   Set the 'lit' status to track the on/off status.  Note that
         *   we don't simply do this by deriving isLit from isOn because
         *   we want to invoke the side effects of changing the status by
         *   calling makeLit explicitly.  We also want to allow the two to
         *   be decoupled when necessary, such as might happen when the
         *   flashlight's bulb is burned out, or its battery has run down.
         */
        makeLit(stat);
    }

    /* initialize */
    initializeThing()
    {
        /* inherit default handling */
        inherited();

        /* 
         *   Make sure our initial isLit setting (for the LightSource)
         *   matches our initial isOn steting (for the Switch).  The
         *   switch status drives the light source status, so initialize
         *   the latter from the former.  
         */
        isLit = isOn;
    }

    /* treat 'light' and 'extinguish' as 'turn on' and 'turn off' */
    dobjFor(Light) asDobjFor(TurnOn)
    dobjFor(Extinguish) asDobjFor(TurnOff)

    /* if we turn on the flashlight, but it doesn't light, mention this */
    dobjFor(TurnOn)
    {
        action()
        {
            /* do the normal work */
            inherited();

            /* 
             *   If we're now on but not lit, mention this.  This can
             *   happen when we run out of power in the battery, or our
             *   bulb is missing or burned out, or we're simply broken. 
             */
            if (isOn && !isLit)
                mainReport(&flashlightOnButDarkMsg);
        }
    }
;

