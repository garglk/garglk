#charset "us-ascii"

/* 
 *   Copyright (c) 2000, 2006 Michael J. Roberts.  All Rights Reserved. 
 *   
 *   TADS 3 Library: events
 *   
 *   This module defines the event framework.  An event is a programmed
 *   operation that occurs at a particular point in the game; an event can
 *   be turn-based, in which case it occurs after a given number of turns
 *   has elapsed, or it can occur in real time, which means that it occurs
 *   after a particular interval of time has elapsed.  
 */

#include "adv3.h"
#include <dict.h>
#include <gramprod.h>


/* ------------------------------------------------------------------------ */
/*
 *   Run the main scheduling loop.  This continues until we encounter an
 *   end-of-file error reading from the console, or a QuitException is
 *   thrown to terminate the game.  
 */
runScheduler()
{
    /* keep going until we quit the game */
    for (;;)
    {
        /* catch the exceptions that terminate the game */
        try
        {
            /* start with an empty list of schedulable items */
            local vec = new Vector(10);

            /* find the lowest time at which something is ready to run */
            local minTime = nil;
            foreach (local cur in Schedulable.allSchedulables)
            {
                /* get this item's next eligible run time */
                local curTime = cur.getNextRunTime();

                /* 
                 *   if it's not nil, and it's equal to or below the
                 *   lowest we've seen so far, note it 
                 */
                if (curTime != nil && (minTime == nil || curTime <= minTime))
                {
                    /* 
                     *   if this is different from the current minimum
                     *   schedulable time, clear out the list of
                     *   schedulables, because the list keeps track of the
                     *   items at the lowest time only 
                     */
                    if (minTime != nil && curTime < minTime)
                        vec.removeRange(1, vec.length());

                    /* add this item to the list */
                    vec.append(cur);

                    /* note the new lowest schedulable time */
                    minTime = curTime;
                }
            }

            /* 
             *   if nothing's ready to run, the game is over by default,
             *   since we cannot escape this state - we can't ourselves
             *   change anything's run time, so if nothing's ready to run
             *   now, we won't be able to change that, and so nothing will
             *   ever be ready to run 
             */
            if (minTime == nil)
            {
                "\b[Error: nothing is available for scheduling -
                terminating]\b";
                return;
            }

            /* 
             *   Advance the global turn counter by the amount of game
             *   clock time we're consuming now.  
             */
            libGlobal.totalTurns += minTime - Schedulable.gameClockTime;

            /* 
             *   advance the game clock to the minimum run time - nothing
             *   interesting happens in game time until then, so we can
             *   skip straight ahead to this time 
             */
            Schedulable.gameClockTime = minTime;

            /* calculate the schedule order for each item */
            vec.forEach({x: x.calcScheduleOrder()});

            /*
             *   We have a list of everything schedulable at the current
             *   game clock time.  Sort the list in ascending scheduling
             *   order, so that the higher priority items come first in
             *   the list.  
             */
            vec = vec.sort(
                SortAsc, {a, b: a.scheduleOrder - b.scheduleOrder});

            /*
             *   Run through the list and run each item.  Keep running
             *   each item as long as it's ready to run - that is, as long
             *   as its schedulable time equals the game clock time.  
             */
        vecLoop:
            foreach (local cur in vec)
            {
                /* run this item for as long as it's ready to run */
                while (cur.getNextRunTime() == minTime)
                {
                    try
                    {
                        /* 
                         *   execute this item - if it doesn't want to be
                         *   called again without considering other
                         *   objects, stop looping and refigure the
                         *   scheduling order from scratch 
                         */
                        if (!cur.executeTurn())
                            break vecLoop;
                    }
                    catch (Exception exc)
                    {
                        /*
                         *   The scheduled operation threw an exception.
                         *   If the schedulable's next run time didn't get
                         *   updated, then the same schedulable will be
                         *   considered ready to run again immediately on
                         *   the next time through the loop.  It's quite
                         *   possible in this case that we'll simply repeat
                         *   the operation that threw the exception and get
                         *   right back here again.  If this happens, it
                         *   will effectively starve all of the other
                         *   schedulables.  To ensure that other
                         *   schedulables get a chance to run before we try
                         *   this erroneous operation again, advance its
                         *   next run time by one unit if it hasn't already
                         *   been advanced. 
                         */
                        if (cur.getNextRunTime() == minTime)
                            cur.incNextRunTime(1);

                        /* re-throw the exception */
                        throw exc;
                    }
                }
            }
        }
        catch (EndOfFileException eofExc)
        {
            /* end of file reading command input - we're done */
            return;
        }
        catch (QuittingException quitExc)
        {
            /* explicitly quitting - we're done */
            return;
        }
        catch (RestartSignal rsSig)
        {
            /* 
             *   Restarting - re-throw the signal for handling in the
             *   system startup code.  Note that we explicitly catch this
             *   signal, only to rethrow it, because we'd otherwise flag it
             *   as an unhandled error in the catch-all Exception handler.
             */
            throw rsSig;
        }
        catch (RuntimeError rtErr)
        {
            /* if this is a debugger error of some kind, re-throw it */
            if (rtErr.isDebuggerSignal)
                throw rtErr;
            
            /* display the error, but keep going */
            "\b[<<rtErr.displayException()>>]\b";
        }
        catch (TerminateCommandException tce)
        {
            /* 
             *   Aborted command - ignore it.  This is most like to occur
             *   when a fuse, daemon, or the like tries to terminate itself
             *   with this exception, thinking it's operating in a normal
             *   command execution environment.  As a convenience, simply
             *   ignore these exceptions so that any code can use them to
             *   abort everything and return to the main scheduling loop. 
             */
        }
        catch (ExitSignal es)
        {
            /* ignore this, just as we ignore TerminateCommandException */
        }
        catch (ExitActionSignal eas)
        {
            /* ignore this, just as we ignore TerminateCommandException */
        }
        catch (Exception exc)
        {
            /* some other unhandled exception - display it and keep going */
            "\b[Unhandled exception: <<exc.displayException()>>]\b";
        }
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   An item that can be scheduled for time-based notifications.  The main
 *   scheduler loop in runScheduler() operates on objects of this class.
 *   
 *   Note that we build a list of all Schedulable instances during
 *   pre-initialization.  If any Schedulable objects are dynamically
 *   created, they must be added to the list explicitly after creation in
 *   order for the event manager to schedule them for execution.  The
 *   default constructor does this automatically, so subclasses can simply
 *   inherit our constructor to be added to the master list.  
 */
class Schedulable: object
    /* construction - add myself to the Schedulable list */
    construct()
    {
        /* 
         *   Add myself to the master list of Schedulable instances.  Note
         *   that we must update the list in the Schedulable class itself. 
         */
        Schedulable.allSchedulables += self;
    }

    /*
     *   Get the next time (on the game clock) at which I'm eligible for
     *   execution.  We won't receive any scheduling notifications until
     *   this time.  If this object doesn't want any scheduling
     *   notifications, return nil.  
     */
    getNextRunTime() { return nextRunTime; }

    /* advance my next run time by the given number of clock units */
    incNextRunTime(amt)
    {
        if (nextRunTime != nil)
            nextRunTime += amt;
    }

    /*
     *   Notify this object that its scheduled run time has arrived.  This
     *   should perform the scheduled task.  If the scheduled task takes
     *   any game time, the object's internal next run time should be
     *   updated accordingly.
     *   
     *   The scheduler will invoke this method of the same object
     *   repeatedly for as long as its nextRunTime remains unchanged AND
     *   this method returns true.  If the object's scheduling priority
     *   changes relative to other schedulable objects, it should return
     *   nil here to tell the scheduler to recalculate scheduling
     *   priorities.  
     */
    executeTurn() { return true; }

    /*
     *   Scheduling order.  This determines which item goes first when
     *   multiple items are schedulable at the same time (i.e., they all
     *   have the same getNextRunTime() values).  The item with the lowest
     *   number here goes first.
     *   
     *   This should never be evaluated except immediately after a call to
     *   calcScheduleOrder.  
     */
    scheduleOrder = 100

    /*
     *   Calculate the scheduling order, returning the order value and
     *   storing it in our property scheduleOrder.  This is used to
     *   calculate and cache the value prior to sorting a list of
     *   schedulable items.  We use this two-step approach (first
     *   calculate, then sort) so that we avoid repeatedly evaluating a
     *   complex calculation, if indeed there is a complex calculation to
     *   perform.
     *   
     *   By default, we assume that the schedule order is static, so we
     *   simply leave our scheduleOrder property unchanged and return its
     *   present value.  
     */
    calcScheduleOrder() { return scheduleOrder; }

    /* my next running time, in game clock time */
    nextRunTime = nil

    /*
     *   A class variable giving the current game clock time.  This is a
     *   class variable because there's only one global game clock.  The
     *   game clock starts at zero and increments in game time units; a
     *   game time unit is the arbitrary quantum of time for our event
     *   scheduling system.  
     */
    gameClockTime = 0

    /*
     *   A list of all of the Schedulable objects in the game.  We set this
     *   up during pre-initialization; if any Schedulable instances are
     *   created dynamically, they must be explicitly added to this list
     *   after creation.  
     */
    allSchedulables = nil
;

/*
 *   Pre-initializer: build the master list of Schedulable instances 
 */
PreinitObject
    /*
     *   Execute preinitialization.  Build a list of all of the schedulable
     *   objects in the game, so that we can scan this list quickly during
     *   play.  
     */
    execute()
    {
        local vec;
        
        /* set up an empty vector to hold the schedulable objects */
        vec = new Vector(32);

        /* add all of the Schedulable instances to the vector */
        forEachInstance(Schedulable, {s: vec.append(s)});

        /* save the list of Schedulable instances as an ordinary list */
        Schedulable.allSchedulables = vec.toList();
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Basic Event Manager.  This is a common base class for the game-time
 *   and real-time event managers.  This class handles the details of
 *   managing the event queue; the subclasses must define the specifics of
 *   event timing. 
 */
class BasicEventManager: object
    /* add an event */
    addEvent(event)
    {
        /* append the event to our list */
        events_.append(event);
    }

    /* remove an event */
    removeEvent(event)
    {
        /* remove the event from our list */
        events_.removeElement(event);
    }

    /* 
     *   Remove events matching the given object and property combination.
     *   We remove all events that match both the object and property
     *   (events matching only the object or only the property are not
     *   affected).
     *   
     *   This is provided mostly as a convenience for cases where an event
     *   is known to be uniquely identifiable by its object and property
     *   values; this saves the caller the trouble of keeping track of the
     *   Event object created when the event was first registered.
     *   
     *   When a particular object/property combination might be used in
     *   several different events, it's better to keep a reference to the
     *   Event object representing each event, and use removeEvent() to
     *   remove the specific Event object of interest.
     *   
     *   Returns true if we find any matching events, nil if not.  
     */
    removeMatchingEvents(obj, prop)
    {
        local found;
        
        /* 
         *   Scan our list, and remove each event matching the parameters.
         *   Note that it's safe to remove things from a vector that we're
         *   iterating with foreach(), since foreach() makes a safe copy
         *   of the vector for the iteration. 
         */
        found = nil;
        foreach (local cur in events_)
        {
            /* if this one matches, remove it */
            if (cur.eventMatches(obj, prop))
            {
                /* remove the event */
                removeEvent(cur);

                /* note that we found a match */
                found = true;
            }
        }

        /* return our 'found' indication */
        return found;
    }

    /* 
     *   Remove the current event - this is provided for convenience so
     *   that an event can cancel itself in the course of its execution.
     *   
     *   Note that this has no effect on the current event execution -
     *   this simply prevents the event from receiving additional
     *   notifications in the future.  
     */
    removeCurrentEvent()
    {
        /* remove the currently active event from our list */
        removeEvent(curEvent_);
    }

    /* event list - each instance must initialize this to a vector */
    // events_ = nil
;

/*
 *   Event Manager.  This is a schedulable object that keeps track of
 *   fuses and daemons, and schedules their execution.  
 */
eventManager: BasicEventManager, Schedulable
    /*
     *   Use a scheduling order of 1000 to ensure we go after all actors.
     *   By default, actors use scheduling orders in the range 100 to 400,
     *   so our order of 1000 ensures that fuses and daemons run after all
     *   characters on a given turn.  
     */
    scheduleOrder = 1000

    /*
     *   Get the next run time.  We'll find the lowest run time of our
     *   fuses and daemons and return that.  
     */
    getNextRunTime()
    {
        local minTime;
        
        /* 
         *   run through our list of events, and find the event that is
         *   scheduled to run at the lowest game clock time 
         */
        minTime = nil;
        foreach (local cur in events_)
        {
            local curTime;
            
            /* get this item's scheduled run time */
            curTime = cur.getNextRunTime();

            /* if it's not nil and it's the lowest so far, remember it */
            if (curTime != nil && (minTime == nil || curTime < minTime))
                minTime = curTime;
        }

        /* return the minimum time we found */
        return minTime;
    }

    /*
     *   Execute a turn.  We'll execute each fuse and each daemon that is
     *   currently schedulable.  
     */
    executeTurn()
    {
        local lst;
        
        /* 
         *   build a list of all of our events with the current game clock
         *   time - these are the events that are currently schedulable 
         */
        lst = events_.subset({x: x.getNextRunTime()
                                 == Schedulable.gameClockTime});

        /* execute the items in this list */
        executeList(lst);

        /* no change in scheduling priorities */
        return true;
    }

    /*
     *   Execute a command prompt turn.  We'll execute each
     *   per-command-prompt daemon. 
     */
    executePrompt()
    {
        /* execute all of the per-command-prompt daemons */
        executeList(events_.subset({x: x.isPromptDaemon}));
    }

    /*
     *   internal service routine - execute the fuses and daemons in the
     *   given list, in eventOrder priority order 
     */
    executeList(lst)
    {
        /* sort the list in ascending event order */
        lst = lst.toList()
              .sort(SortAsc, {a, b: a.eventOrder - b.eventOrder});

        /* run through the list and execute each item ready to run */
        foreach (local cur in lst)
        {
            /* remember our old active event, then establish the new one */
            local oldEvent = curEvent_;
            curEvent_ = cur;

            /* make sure we restore things on the way out */
            try
            {
                local pc;
                
                /* have the player character note the pre-event conditions */
                pc = gPlayerChar;
                pc.noteConditionsBefore();
                
                /* cancel any sense caching currently in effect */
                libGlobal.disableSenseCache();

                /* execute the event */
                cur.executeEvent();

                /* 
                 *   if the player character is the same as it was, ask
                 *   the player character to note any change in conditions 
                 */
                if (gPlayerChar == pc)
                    pc.noteConditionsAfter();
            }
            catch (Exception exc)
            {
                /* 
                 *   If an event throws an exception out of its handler,
                 *   remove the event from the active list.  If we were to
                 *   leave it active, we'd go back and execute the same
                 *   event again the next time we look for something to
                 *   schedule, and that would in turn probably just
                 *   encounter the same exception - so we'd be stuck in an
                 *   infinite loop executing this erroneous code.  To
                 *   ensure that we don't get stuck, remove the event. 
                 */
                removeCurrentEvent();

                /* re-throw the exception */
                throw exc;
            }
            finally
            {
                /* restore the enclosing current event */
                curEvent_ = oldEvent;
            }
        }
    }

    /* our list of fuses and daemons */
    events_ = static new Vector(20)

    /* the event currently being executed */
    curEvent_ = nil
;

/*
 *   Pseudo-action subclass to represent the action environment while
 *   processing a daemon, fuse, or other event. 
 */
class EventAction: Action
    /* 
     *   event actions are internal system actions; they don't consume
     *   additional turns themselves, since they run between player turns 
     */
    actionTime = 0;
;

/*
 *   A basic event, for game-time and real-time events.
 */
class BasicEvent: object
    /* construction */
    construct(obj, prop)
    {
        /* remember the object and property to call at execution */
        obj_ = obj;
        prop_ = prop;
    }

    /* 
     *   Execute the event.  This must be overridden by the subclass to
     *   perform the appropriate operation when executed.  In particular,
     *   the subclass must reschedule or unschedule the event, as
     *   appropriate. 
     */
    executeEvent() { }

    /* does this event match the given object/property combination? */
    eventMatches(obj, prop) { return obj == obj_ && prop == prop_; }

    /* 
     *   Call our underlying method.  This is an internal routine intended
     *   for use by the executeEvent() implementations.  
     */
    callMethod()
    {
        /* 
         *   invoke the method in our sensory context, and in a simulated
         *   action environment 
         */
        withActionEnv(EventAction, gPlayerChar,
            {: callWithSenseContext(source_, sense_,
                                    {: obj_.(self.prop_)() }) });
    }

    /* the object and property we invoke */
    obj_ = nil
    prop_ = nil

    /* 
     *   The sensory context of the event.  When the event fires, we'll
     *   execute its method in this sensory context, so that any messages
     *   generated will be displayed only if the player character can
     *   sense the source object in the given sense.
     *   
     *   By default, these are nil, which means that the event's messages
     *   will be displayed (or, at least, they won't be suppressed because
     *   of the sensory context).  
     */
    source_ = nil
    sense_ = nil
;

/*
 *   Base class for fuses and daemons 
 */
class Event: BasicEvent
    /* our next run time, in game clock time */
    getNextRunTime() { return nextRunTime; }

    /* delay our scheduled run time by the given number of turns */
    delayEvent(turns) { nextRunTime += turns; }

    /* remove this event from the event manager */
    removeEvent() { eventManager.removeEvent(self); }

    /* 
     *   Event order - this establishes the order we run relative to other
     *   events scheduled to run at the same game clock time.  Lowest
     *   number goes first.  By default, we provide an event order of 100,
     *   which should leave plenty of room for custom events before and
     *   after default events.  
     */
    eventOrder = 100

    /* creation */
    construct(obj, prop)
    {
        /* inherit default handling */
        inherited(obj, prop);

        /* add myself to the event manager's active event list */
        eventManager.addEvent(self);
    }

    /* 
     *   our next execution time, expressed in game clock time; by
     *   default, we'll set this to nil, which means that we are not
     *   scheduled to execute at all 
     */
    nextRunTime = nil

    /* by default, we're not a per-command-prompt daemon */
    isPromptDaemon = nil
;

/*
 *   Fuse.  A fuse is an event that fires once at a given time in the
 *   future.  Once a fuse is executed, it is removed from further
 *   scheduling.  
 */
class Fuse: Event
    /* 
     *   Creation.  'turns' is the number of turns in the future at which
     *   the fuse is executed; if turns is 0, the fuse will be executed on
     *   the current turn.  
     */
    construct(obj, prop, turns)
    {
        /* inherit the base class constructor */
        inherited(obj, prop);

        /* 
         *   set my scheduled time to the current game clock time plus the
         *   number of turns into the future 
         */
        nextRunTime = Schedulable.gameClockTime + turns;
    }

    /* execute the fuse */
    executeEvent()
    {
        /* call my method */
        callMethod();

        /* a fuse fires only once, so remove myself from further scheduling */
        eventManager.removeEvent(self);
    }
;

/*
 *   Sensory-context-sensitive fuse - this is a fuse with an explicit
 *   sensory context.  We'll run the fuse in its sense context, so any
 *   messages generated will be visible only if the given source object is
 *   reachable by the player character in the given sense.
 *   
 *   Conceptually, the source object is considered the source of any
 *   messages that the fuse generates, and the messages pertain to the
 *   given sense; so if the player character cannot sense the source
 *   object in the given sense, the messages should not be displayed.  For
 *   example, if the fuse will describe the noise made by an alarm clock
 *   when the alarm goes off, the source object would be the alarm clock
 *   and the sense would be sound; this way, if the player character isn't
 *   in hearing range of the alarm clock when the alarm goes off, we won't
 *   display messages about the alarm noise.  
 */
class SenseFuse: Fuse
    construct(obj, prop, turns, source, sense)
    {
        /* inherit the base constructor */
        inherited(obj, prop, turns);

        /* remember our sensory context */
        source_ = source;
        sense_ = sense;
    }
;

/*
 *   Daemon.  A daemon is an event that fires repeatedly at given
 *   intervals.  When a daemon is executed, it is scheduled again for
 *   execution after its interval elapses again.  
 */
class Daemon: Event
    /*
     *   Creation.  'interval' is the number of turns between invocations
     *   of the daemon; this should be at least 1, which causes the daemon
     *   to be invoked on each turn.  The first execution will be
     *   (interval-1) turns in the future - so if interval is 1, the
     *   daemon will first be executed on the current turn, and if
     *   interval is 2, the daemon will be executed on the next turn.  
     */
    construct(obj, prop, interval)
    {
        /* inherit the base class constructor */
        inherited(obj, prop);

        /* 
         *   an interval of less than 1 is meaningless, so make sure it's
         *   at least 1 
         */
        if (interval < 1)
            interval = 1;

        /* remember my interval */
        interval_ = interval;

        /* 
         *   set my initial execution time, in game clock time - add one
         *   less than the interval to the current game clock time, so
         *   that we count the current turn as yet to elapse for the
         *   purposes of the interval before the daemon's first execution 
         */
        nextRunTime = Schedulable.gameClockTime + interval - 1;
    }
    
    /* execute the daemon */
    executeEvent()
    {
        /* call my method */
        callMethod();

        /* advance our next run time by our interval */
        nextRunTime += interval_;
    }

    /* our execution interval, in turns */
    interval_ = 1
;

/*
 *   Sensory-context-sensitive daemon - this is a daemon with an explicit
 *   sensory context.  This is the daemon counterpart of SenseFuse.  
 */
class SenseDaemon: Daemon
    construct(obj, prop, interval, source, sense)
    {
        /* inherit the base constructor */
        inherited(obj, prop, interval);

        /* remember our sensory context */
        source_ = source;
        sense_ = sense;
    }
;

/*
 *   Command Prompt Daemon.  This is a special type of daemon that
 *   executes not according to the game clock, but rather once per command
 *   prompt.  The system executes all of these daemons just before each
 *   time it prompts for a command line.  
 */
class PromptDaemon: Event
    /* execute the daemon */
    executeEvent()
    {
        /* 
         *   call my method - there's nothing else to do for this type of
         *   daemon, since our scheduling is not affected by the game
         *   clock 
         */
        callMethod();
    }

    /* flag: we are a special per-command-prompt daemon */
    isPromptDaemon = true
;

/*
 *   A one-time-only prompt daemon is a regular command prompt daemon,
 *   except that it fires only once.  After it fires once, the daemon
 *   automatically deactivates itself, so that it won't fire again.
 *   
 *   Prompt daemons are occasionally useful for non-recurring processing,
 *   when you want to defer some bit of code until a "safe" time between
 *   turns.  In these cases, the regular PromptDaemon is inconvenient to
 *   use because it automatically recurs.  This subclass is handy for these
 *   cases, since it lets you schedule some bit of processing for a single
 *   deferred execution.
 *   
 *   One special situation where one-time prompt daemons can be handy is in
 *   triggering conversational events - such as initiating a conversation -
 *   at the very beginning of the game.  Initiating a conversation can only
 *   be done from within an action context, but no action context is in
 *   effect during the game's initialization.  An easy way to deal with
 *   this is to create a one-time prompt daemon during initialization, and
 *   then trigger the event from the daemon's callback method.  The prompt
 *   daemon will set up a daemon action environment just before the first
 *   command prompt is displayed, at which point the callback will be able
 *   to trigger the event as though it were in ordinary action handler
 *   code.  
 */
class OneTimePromptDaemon: PromptDaemon
    executeEvent()
    {
        /* execute as normal */
        inherited();

        /* remove myself from the event list, so that I don't fire again */
        removeEvent();
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Real-Time Event Manager.  This object manages all of the game's
 *   real-time events, which are events that occur according to elapsed
 *   real-world time. 
 */
realTimeManager: BasicEventManager, InitObject
    /*
     *   Get the elapsed game time at which the next real-time event is
     *   scheduled.  This returns a value which can be compared to that
     *   returned by getElapsedTime(): if this value is less than or equal
     *   to the value from getElapsedTime(), then the next event is reay
     *   for immediate execution; otherwise, the result of subtracting
     *   getElapsedTime() from our return value gives the number of
     *   milliseconds until the next event is schedulable.
     *   
     *   Note that we don't calculate the delta to the next event time,
     *   but instead return the absolute time, because the caller might
     *   need to perform extra processing before using our return value.
     *   If we returned a delta, that extra processing time wouldn't be
     *   figured into the caller's determination of event schedulability.
     *   
     *   If we return nil, it means that there are no scheduled real-time
     *   events.  
     */
    getNextEventTime()
    {
        local tMin;
        
        /* 
         *   run through our event list and find the event with the lowest
         *   scheduled run time 
         */
        tMin = nil;
        foreach (local cur in events_)
        {
            local tCur;

            /* get the current item's time */
            tCur = cur.getEventTime();
            
            /* 
             *   if this one has a valid time, and we don't have a valid
             *   time yet or this one is sooner than the soonest one we've
             *   seen so far, note this one as the soonest so far 
             */
            if (tMin == nil
                || (tCur != nil && tCur < tMin))
            {
                /* this is the soonest so far */
                tMin = tCur;
            }
        }

        /* return the soonest event so far */
        return tMin;
    }

    /*
     *   Run any real-time events that are ready to execute, then return
     *   the next event time.  The return value has the same meaning as
     *   that of getNextEventTime(). 
     */
    executeEvents()
    {
        local tMin;
        
        /* 
         *   Keep checking as long as we find anything to execute.  Each
         *   time we execute an event, we might consume enough time that
         *   an item earlier in our queue that we originally dismissed as
         *   unready has become ready to run.  
         */
        for (;;)
        {
            local foundEvent;

            /* we haven't yet run anything on this pass */
            foundEvent = nil;

            /* we haven't found anything schedulable on this pass yet */
            tMin = nil;
            
            /* run each event whose time is already here */
            foreach (local cur in events_)
            {
                local tCur;

                /* 
                 *   If this event has a non-nil time, and its time is
                 *   less than or equal to the current system clock time,
                 *   run this event.  All event times are in terms of the
                 *   game elapsed time.
                 *   
                 *   If this event isn't schedulable, at least check to
                 *   see if it's the soonest schedulable event so far.  
                 */
                tCur = cur.getEventTime();
                if (tCur != nil && tCur <= getElapsedTime())
                {
                    /* cancel any sense caching currently in effect */
                    libGlobal.disableSenseCache();

                    /* execute this event */
                    cur.executeEvent();

                    /* note that we executed something */
                    foundEvent = true;
                }
                else if (tMin == nil
                         || (tCur != nil && tCur < tMin))
                {
                    /* it's the soonest event so far */
                    tMin = tCur;
                }
            }

            /* if we didn't execute anything on this pass, stop scanning */
            if (!foundEvent)
                break;
        }

        /* return the time of the next event */
        return tMin;
    }

    /*
     *   Get the current game elapsed time.  This is the number of
     *   milliseconds that has elapsed since the game was started,
     *   counting only the continuous execution time.  When the game is
     *   saved, we save the elapsed time at that point; when the game is
     *   later restored, we project that saved time backwards from the
     *   current real-world time at restoration to get the real-world time
     *   where the game would have started if it had actually been played
     *   continuously in one session.  
     */
    getElapsedTime()
    {
        /* 
         *   return the current system real-time counter minus the virtual
         *   starting time 
         */
        return getTime(GetTimeTicks) - startingTime;
    }

    /*
     *   Set the current game elapsed time.  This can be used to freeze
     *   the real-time clock - a caller can note the elapsed game time at
     *   one point by calling getElapsedTime(), and then pass the same
     *   value to this routine to ensure that no real time can effectively
     *   pass between the two calls. 
     */
    setElapsedTime(t)
    {
        /* 
         *   set the virtual starting time to the current system real-time
         *   counter minus the given game elapsed time 
         */
        startingTime = getTime(GetTimeTicks) - t;
    }

    /*
     *   The imaginary real-world time of the starting point of the game,
     *   treating the game as having been played from the start in one
     *   continous session.  Whenever we restore a saved game, we project
     *   backwards from the current real-world time at restoration by the
     *   amount of continuous elapsed time in the saved game to find the
     *   point at which the game would have started if it had been played
     *   continuously in one session up to the restored point.
     *   
     *   We set a static initial value for this, using the interpreter's
     *   real-time clock value at compilation time.  This ensures that
     *   we'll have a meaningful time base if any real-time events are
     *   created during pre-initialization.  This static value will only be
     *   in effect during preinit; we're an InitObject, so our execute()
     *   method will be invoked at run-time start-up, and at that point
     *   we'll reset the zero point to the actual run-time start time.  
     */
    startingTime = static getTime(GetTimeTicks)

    /* 
     *   Initialize at run-time startup.  We want to set the zero point as
     *   the time when the player actually started playing the game (any
     *   time we spent in pre-initialization doesn't count on the real-time
     *   clock, since it's not part of the game per se).  
     */
    execute()
    {
        /* 
         *   note the real-time starting point of the game, so we can
         *   calculate the elapsed game time later 
         */
        startingTime = getTime(GetTimeTicks);
    }

    /* 
     *   save the elapsed time so far - this is called just before we save
     *   a game so that we can pick up where we left off on the elapsed
     *   time clock when we restore the saved game 
     */
    saveElapsedTime()
    {
        /* remember the elapsed time so far */
        elapsedTimeAtSave = getElapsedTime();
    }

    /*
     *   Restore the elapsed time - this is called just after we restore a
     *   game.  We'll project the saved elapsed time backwards to figure
     *   the imaginary starting time the game would have had if it had
     *   been played in one continuous session rather than being saved and
     *   restored. 
     */
    restoreElapsedTime()
    {
        /* 
         *   project backwards from the current time by the saved elapsed
         *   time to get the virtual starting point that will give us the
         *   same current elapsed time on the system real-time clock 
         */
        startingTime = getTime(GetTimeTicks) - elapsedTimeAtSave;
    }

    /* our event list */
    events_ = static new Vector(20)

    /* the event currently being executed */
    curEvent_ = nil

    /* 
     *   saved elapsed time - we use this to figure the virtual starting
     *   time when we restore a saved game 
     */
    elapsedTimeAtSave = 0
;

/*
 *   Real-time manager: pre-save notification receiver.  When we're about
 *   to save the game, we'll note the current elapsed game time, so that
 *   when we later restore the game, we can figure the virtual starting
 *   point that will give us the same effective elapsed time on the system
 *   real-time clock.  
 */
PreSaveObject
    execute()
    {
        /* 
         *   remember the elapsed time at the point we saved the game, so
         *   that we can restore it later 
         */
        realTimeManager.saveElapsedTime();
    }
;

/*
 *   Real-time manager: post-restore notification receiver.  Immediately
 *   after we restore a game, we'll tell the real-time manager to refigure
 *   the virtual starting point of the game based on the saved elapsed
 *   time. 
 */
PostRestoreObject
    execute()
    {
        /* figure the new virtual starting time */
        realTimeManager.restoreElapsedTime();
    }
;

/*
 *   Real-Time Event.  This is an event that occurs according to elapsed
 *   wall-clock time in the real world.  
 */
class RealTimeEvent: BasicEvent
    /*
     *   Get the elapsed real time at which this event is triggered.  This
     *   is a time value in terms of realTimeManager.getElapsedTime().  
     */
    getEventTime()
    {
        /* by default, simply return our eventTime value */
        return eventTime;
    }

    /* construction */
    construct(obj, prop)
    {
        /* inherit default handling */
        inherited(obj, prop);

        /* add myself to the real-time event manager's active list */
        realTimeManager.addEvent(self);
    }

    /* remove this event from the real-time event manager */
    removeEvent() { realTimeManager.removeEvent(self); }

    /* our scheduled event time */
    eventTime = 0
;

/*
 *   Real-time fuse.  This is an event that fires once at a specified
 *   elapsed time into the game. 
 */
class RealTimeFuse: RealTimeEvent
    /*
     *   Creation.  'delta' is the amount of real time (in milliseconds)
     *   that should elapse before the fuse is executed.  If 'delta' is
     *   zero or negative, the fuse will be schedulable immediately.  
     */
    construct(obj, prop, delta)
    {
        /* inherit default handling */
        inherited(obj, prop);

        /* 
         *   set my scheduled time to the current game elapsed time plus
         *   the delta - this will give us the time in terms of elapsed
         *   game time at which we'll be executed 
         */
        eventTime = realTimeManager.getElapsedTime() + delta;
    }

    /* execute the fuse */
    executeEvent()
    {
        /* call my method */
        callMethod();

        /* a fuse fires only once, so remove myself from further scheduling */
        realTimeManager.removeEvent(self);
    }
;

/*
 *   Sensory-context-sensitive real-time fuse.  This is a real-time fuse
 *   with an explicit sensory context. 
 */
class RealTimeSenseFuse: RealTimeFuse
    construct(obj, prop, delta, source, sense)
    {
        /* inherit the base constructor */
        inherited(obj, prop, delta);

        /* remember our sensory context */
        source_ = source;
        sense_ = sense;
    }
;

/*
 *   Real-time daemon.  This is an event that occurs repeatedly at given
 *   real-time intervals.  When a daemon is executed, it is scheduled
 *   again for execution after its real-time interval elapses again.  The
 *   daemon's first execution will occur one interval from the time at
 *   which the daemon is created.
 *   
 *   If a daemon is executed late (because other, more pressing tasks had
 *   to be completed first, or because the user was busy editing a command
 *   line and the local platform doesn't support real-time command
 *   interruptions), the interval is applied to the time the daemon
 *   actually executed, not to the originally scheduled execution time.
 *   For example, if the daemon is scheduled to run once every minute, but
 *   can't run at all for five minutes because of command editing on a
 *   non-interrupting platform, once it actually does run, it won't run
 *   again for (at least) another minute after that.  This means that the
 *   daemon will not run five times all at once when it's finally allowed
 *   to run - there's no making up for lost time.  
 */
class RealTimeDaemon: RealTimeEvent
    /*
     *   Creation.  'interval' is the number of milliseconds between
     *   invocations.  
     */
    construct(obj, prop, interval)
    {
        /* inherit the base constructor */
        inherited(obj, prop);

        /* remember my interval */
        interval_ = interval;

        /* 
         *   figure my initial execution time - wait for one complete
         *   interval from the current time 
         */
        eventTime = realTimeManager.getElapsedTime() + interval;
    }

    /* execute the daemon */
    executeEvent()
    {
        /* call my method */
        callMethod();

        /* 
         *   Reschedule for next time.  To ensure that we keep to our
         *   long-term schedule, reschedule based on our original schedule
         *   time rather than the current clock time; that way, if there
         *   was a delay after our original scheduled time in firing us,
         *   we'll make up for it by shortening the interval until the
         *   next firing.  If that would make us already schedulable, then
         *   our interval must be so short we can't keep up with it; in
         *   that case, add the interval to the current clock time. 
         */
        eventTime += interval_;
        if (realTimeManager.getElapsedTime() < eventTime)
            eventTime = realTimeManager.getElapsedTime() + interval_;
    }

    /* my execution interval, in milliseconds */
    interval_ = 1
;

/*
 *   Sensory-context-sensitive real-time daemon - this is a real-time
 *   daemon with an explicit sensory context.  This is the daemon
 *   counterpart of RealTimeSenseFuse.  
 */
class RealTimeSenseDaemon: RealTimeDaemon
    construct(obj, prop, interval, source, sense)
    {
        /* inherit the base constructor */
        inherited(obj, prop, interval);

        /* remember our sensory context */
        source_ = source;
        sense_ = sense;
    }
;

