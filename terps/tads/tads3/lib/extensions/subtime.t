/*
 *   Copyright 2003, 2006 Michael J. Roberts
 *   
 *   "Subjective Time" module.  This implements a form of in-game
 *   time-keeping that attempts to mimic the player's subjective
 *   experience of time passing in the scenario while still allowing for
 *   occasional, reasonably precise time readings, such as from a
 *   wristwatch in the game world.
 *   
 *   It's always been difficult to handle the passage of time in
 *   interactive fiction.  It's such a vexing problem that most IF avoids
 *   the problem entirely by setting the game action in a world that
 *   forgot time, where there are no working clocks and where night never
 *   falls.  Like so many of the hard parts of interactive fiction, the
 *   difficulty here comes from the uneasy union of simulation and
 *   narrative that's at the heart of all IF.
 *   
 *   In ordinary static narratives, the passage of time can swing between
 *   extremes: a scene that only takes moments of real-time within the
 *   story, such as an intense action scene or an extended inner
 *   monologue, can go on for many pages; and elsewhere in the same story,
 *   days and years can pass in the space of a sentence or two.  Narrative
 *   time has no relationship to the length of a passage of text;
 *   narrative time is related only to the density of story-significant
 *   action.
 *   
 *   It might seem at first glance that this break between in-story time
 *   and chapter length is a special privilege of the staticness of a
 *   book: the text just sits there on a page, unchanging in time, so of
 *   course there's no real-time anchor for the story's internal clock.
 *   But that can't be it, because we see exactly the same narrative
 *   manipulation of time in film, which is a fundamentally time-based
 *   medium.  Films don't present their action in real-time any more than
 *   books do.  What's more, film follows the same rules as text-based
 *   fiction in compressing and expanding story-time: many minutes of film
 *   can spool by as the last seven seconds tick off a bomb's count-down
 *   timer, while the seasons can change in the course of a five-second
 *   dissolve.  Even Fox's "24," a television series whose entire conceit
 *   is that the action is presented in real-time, can be seen on closer
 *   inspection to hew to its supposed real-time anchors only every ten
 *   minutes or so, where the ads are inserted; in between the ads, the
 *   normal bag of tricks is liberally employed to compress and expand
 *   time.
 *   
 *   In simulations, we have a different situation, because the pacing of
 *   the story is under the indirect control of the player.  (I call the
 *   control indirect because it's not presented to the player as a simple
 *   control knob with settings for "slower" and "faster."  The player can
 *   directly control how quickly she enters commands, but this has only a
 *   very minor effect on the pace of the story; the major component of
 *   the story's pacing is the player's rate of absorbing information from
 *   the story and synthesizing solutions to its obstacles.  In most
 *   cases, this is the gating factor: the player is motivated, by a
 *   desire to win the game, to go as fast as possible, but is unable to
 *   go any faster because of the need to peform time-consuming mental
 *   computation.  Thus the player's control is indirect; there's the
 *   illusion of a control knob for "faster" and "slower," but in practice
 *   the knob's setting is forced to a fixed value, determined by the
 *   player's ability to solve puzzles: no player ever wants to choose a
 *   "slower" setting than they have to, and the "faster" settings are
 *   unavailable because of the gating mental computation factor.)
 *   
 *   But even so, the player in a simulation does have complete control
 *   over the specific actions that the player character performs.  The
 *   player makes the character walk around, examine things, try a few
 *   random verbs on an object, walk around some more, and so on.  All of
 *   these things clearly ought to take time in the simulation.  The
 *   question is: how much time?
 *   
 *   The most obvious approach is to tie the passage of game-time to the
 *   actions the player character performs, by making each turn take some
 *   amount of time.  At the simplest, each turn could take a fixed amount
 *   of time, say 1 minute.  A more sophisticated approach might be for
 *   some kinds of actions to take more time than others; walking to a new
 *   room might take 2 minutes, say, while examining a wall might take 30
 *   seconds.  Some early IF games took this approach.  Infocom's
 *   Planetfall, for example, has a cycle of day and night that's tied to
 *   the number of turns taken by the player.
 *   
 *   Unfortunately, such a direct mapping of turns to game-time hasn't
 *   proved to be very satisfactory in practice.  This approach doesn't
 *   seem to make the treatment of time more realistic -- in fact, it's
 *   just the opposite.  For example, in Planetfall, as objective
 *   game-time passes, it's necessary to eat and sleep; the game has a
 *   limited supply of food, so if you spend too much time exploring, you
 *   can lock yourself out of victory simply by exhausting all available
 *   food and starving to death.  The workaround, of course, is to restart
 *   the game after you've explored enough to know your way around, at
 *   which point you can cruise through the parts you've been through
 *   before.  The end result is that we get two opposite but equally
 *   unrealistic treatments of time: on the first pass, far too much
 *   objective time passes as we wander around exploring, given how little
 *   we accomplish; and on the second pass, far too little objective time
 *   elapses, given how rapidly we progress through the story.
 *   
 *   One problem would seem to be that verbs are far too coarse a means to
 *   assign times to actions: picking up five small items off a table
 *   might take almost no time at all, while picking up the table itself
 *   could require a minute or two; walking to the next room might take
 *   ten seconds, while walking across an airport terminal takes ten
 *   minutes.  An author could in principle assign timings to actions at
 *   the level of individual verb-object combinations, but this would
 *   obviously be unmanageable in anything but the tiniest game.  
 *   
 *   A second problem is that it might be better to see the player's
 *   actual sequence of command input as somewhat distinct from the player
 *   character's subjective storyline.  In the turns-equals-time approach,
 *   we're essentially asserting a literal equivalence between the
 *   player's commands and the player character's story.  In typical IF
 *   game play, the player spends quite a lot of time exploring,
 *   examining, and experimenting.  If we were to imagine ourselves as an
 *   observer inside the game world, what we'd see would be highly
 *   unnatural and unrealistic: no one actually behaves like an IF player
 *   character.  If we wanted to achieve more realism, we might consider
 *   that the medium imposes a certain amount of this exploration activity
 *   artificially, as a means for the player to gather information that
 *   would, in reality, be immediately and passively accessible to the
 *   player character - just by being there, the player character would
 *   instantly absorb a lot of information that the player can only learn
 *   by examination and experimentation.  Therefore, we could say that,
 *   realistically, a lot of the exploration activity that a player
 *   performs isn't really part of the story, but is just an artifact of
 *   the medium, and thus shouldn't count against the story world's
 *   internal clock.
 *   
 *   One ray of hope in all of this is that the human sense of time is
 *   neither precise nor objective.  People do have an innate sense of the
 *   passage of time, but it doesn't work anything like the
 *   turns-equals-time approach would have it.  People don't internally
 *   track time by counting up the actions they perform.  In fact, people
 *   generally don't even have a very precise intuitive sense for how long
 *   a particular action takes; in most cases, people are actually pretty
 *   bad at estimating the time it will take to perform ordinary tasks.
 *   (Unless you're asking about areas where they have a lot of hands-on
 *   experience actually timing things on a clock, such as you might find
 *   in certain professions.  And a lot of people aren't even good at that
 *   - just try getting a decent schedule estimate out of a programmer,
 *   for example.)
 *   
 *   The human sense of time is not, then, an objective accounting of
 *   recent activity.  Rather, it's a highly subjective sense, influenced
 *   by things like emotion and focus of attention.  A couple of familiar
 *   examples illustrate how a person's subjective experience of time can
 *   wildly depart from time's objective passage.  First, think about how
 *   time seems to stretch out when you have nothing to do but wait for
 *   something, like when you're stuck in an airport waiting for a late
 *   flight.  Second, think about how time flies by when you're doing
 *   something that intensely focuses your attention, like when you're
 *   playing a really good computer game or you're deeply engaged in a
 *   favorite hobby: hours can seem to disappear into a black hole.  Being
 *   bored tends to expand time, while being intensely occupied tends to
 *   compress it.  There are other factors that affect the subjective time
 *   sense, but occupation of attention seems to be one of the most
 *   important.
 *   
 *   So where does this lead us?  If it's not clear which "turns" are part
 *   of the story and which are merely artifacts of the medium, the turn
 *   counter can't be a reliable source of game-time.  But if the human
 *   sense of time is inherently subjective, then maybe we're better off
 *   tying the passage of time to something other than the raw turn
 *   counter anyway.
 *   
 *   The key to our solution is to treat the turn counter as correlated to
 *   the player's subjective time, rather than the story's objective time.
 *   In other words, rather than saying that a LOOK command actually takes
 *   30 seconds in the game world, we say that a LOOK command might *feel*
 *   like it takes some amount of time to the player, but that the actual
 *   time it takes depends on what it accomplishes in the story.  How much
 *   time really passes for a single LOOK command?  It all depends on how
 *   many LOOK commands it takes to accomplish something.
 *   
 *   So, if the player spends long periods of time exploring and frobbing
 *   around with objects, but not making any progress, we've had a lot of
 *   subjective time pass (many turns), but very little story time (few
 *   story-significant events).  This is exactly what people are used to
 *   in real life: when you're bored and not accomplishing anything, time
 *   seems to drag.  If the player starts knocking down puzzles left and
 *   right, making great progress through the story, we suddenly have a
 *   feeling of little subjective time passing (few turns) while story
 *   time is flying past (many story-significant events).  Again, just
 *   what we're used to in real life when deeply engaged in some activity.
 *   
 *   This basic approach isn't new.  Gareth Rees's Christminster includes
 *   plot elements that occur at particular clock-times within the story,
 *   and a working clock-tower to let the player character tell the time.
 *   Time advances in the game strictly according to the player's progress
 *   through the plot (i.e., solving puzzles).  At key plot events, the
 *   clock advances; at all other times, it just stays fixed in place.
 *   The game works around the "precision problem" (which we'll come to
 *   shortly) with the somewhat obvious contrivance of omitting the
 *   minute-hand from the clock, so the time can only be read to the
 *   nearest hour.  Michael Gentry's Anchorhead also ties events to the
 *   story's clock-time, and provides a story clock of sorts that advances
 *   by the player's progress in solving puzzles.  Anchorhead handles the
 *   precision problem by using an even coarser clock than Christminster,
 *   specifically the position of the sun in the sky, which I think can
 *   only be read to a precision of "day" or "night."  The
 *   precision-avoiding contrivance here is much less obvious than in
 *   Christminster, and it especially helps that the setting and
 *   atmosphere of the game practically demand a continuous dark overcast.
 *   
 *   But what if we wanted a normal clock in the game - one with a minute
 *   hand?  This is where the precision problem comes in.  The problem
 *   with subjective time is that, in the real world, a tool-using human
 *   in possession of a wristwatch can observe the objective time whenever
 *   she wants.  In games like Anchorhead and Christminster, the clock
 *   only advances at key plot points, so if we permitted the player
 *   character a timepiece with a minute hand, we'd get unrealistic
 *   exchanges like this:
 *   
 *   >x watch
 *.  It's 3:05pm.
 *   
 *   >east. get bucket. west. fill bucket with water. drink water. east.
 *.  ... done ...
 *   
 *   >etc, etc, etc...
 *.  ... done ...
 *   
 *   >x watch
 *.  It's 3:05pm.
 *   
 *   That's why Christminster doesn't have a minute hand, and why
 *   Anchorhead doesn't have clocks at all.  When the time is only
 *   reported vaguely, the player won't notice that the clock is frozen
 *   between plot events: it's not a problem when we're told that it's
 *   still "some time after three" for dozens of turns in a row.
 *   
 *   This is where this implementation tries to add something new.  We try
 *   to achieve a compromise between an exclusively narrative advancement
 *   of time, and an exclusively turn-based clock.  The goal is to have
 *   the story drive the actual advancement of the clock, while still
 *   giving plausible time readings whenever the player wants them.
 *   
 *   The compromise hinges on the way people use clocks in the real world:
 *   a person will typically glance at the clock occasionally, but not
 *   watch it continuously.  Our approach also depends upon the
 *   observation that people are generally bad at estimating how long a
 *   particular activity takes.  If people are bad at judging the time for
 *   one activity, then they're even worse at judging it for a series of
 *   activities.  This is an advantage for us, because it means that the
 *   "error bars" around a player's estimate of how long something ought
 *   to be taking are inherently quite large, which in turn means that we
 *   have a large range of plausible results - as long as we don't say
 *   it's 3:05pm every time we're asked.
 *   
 *   Here's the scheme.  We start by having the game assign the times of
 *   important events.  The game doesn't have to tell us in advance about
 *   everything; it only has to tell us the starting time, and the game
 *   clock time of the next important event in the plot.  For example, the
 *   game could tell us that it's now noon, and the next important plot
 *   point will be when the player character manages to get into the
 *   castle, which happens at 6pm, just as the sun is starting to set.
 *   Note that the story asserts that this event happens at 6pm; it
 *   doesn't matter how many turns it takes the player to solve the
 *   puzzles and get into the castle, since *in the story*, we always get
 *   in at 6pm.  
 *   
 *   If the player looks at a clock at the start of the game, they'll see
 *   that it's noon.  The clock module then starts keeping track of the
 *   number of turns the player takes.  The next time the player looks at
 *   a clock, we'll check to see how many turns it's been since the last
 *   look at the clock.  (We'll also consider plot points where the story
 *   actually sets the clock to be the same as the player looking at the
 *   clock, since these have the same effect of making us commit to a
 *   particular time.)  We'll scale this linearly to a number of minutes
 *   passing.  Then, we'll crank down this linear scaling factor according
 *   to how close we're getting to the next event time - this ensures that
 *   we'll leave ourselves room to keep the clock advancing without
 *   bumping up against the next event time.  So, we have a sort of Zeno's
 *   paradox factor: the closer we get to the next event time, the slower
 *   we'll approach it.
 *   
 *   The point is to create a plausible illusion of precision.  A player
 *   who checks infrequently, as should be typical, should see a plausible
 *   series of intermediate times between major plot points.
 *   
 *   One final note: if you want to display a clock in the status line, or
 *   show the current time with every prompt, this is the wrong module to
 *   use.  A key assumption of the scheme implemented here is that the
 *   time will be checked only occasionally, when it occurs to the player
 *   to look.  If the game is constantly checking the time
 *   programmatically, it'll defeat the whole purpose, since we won't be
 *   able to exploit the player's presumed uncertainty about exactly how
 *   much time should have elapsed between checks.  
 */

#include <tads.h>


/* ------------------------------------------------------------------------ */
/*
 *   The Clock Manager is the object that keeps track of the game-world
 *   wall-clock time.  Timepieces in the game can consult this for the
 *   current official time.
 *   
 *   To use the clock manager's services, you should decide on a set of
 *   plot events in your game that occur at particular times.  This
 *   requires you to "linearize" your game's plot to the extent that these
 *   events must occur in a particular order and at particular times within
 *   the game world.  This might sound at odds with the idea that the
 *   player is in control, but most IF stories are actually structured this
 *   way anyway, with at least a few plot points that always happen in a
 *   particular order, no matter what the player does.  Puzzles tend to
 *   introduce a locally linear structure by their nature, in that a player
 *   can't reach a plot point that depends on the solution to a puzzle
 *   until solving that puzzle.
 *   
 *   Once you've decided on your important plot points, create a ClockEvent
 *   object to represent each one.  Each ClockEvent object specifies the
 *   game-clock time at which the event occurs.  Once you've defined these
 *   objects, add code to your game to call the eventReached() method of
 *   the appropriate ClockEvent object when each plot event occurs.  This
 *   anchors the game-world clock to the player's progress through the
 *   special set of plot points.
 *   
 *   Note that you must always create a ClockEvent to represent the very
 *   beginning of the game - this establishes the time at the moment the
 *   game opens.  At start-up, we'll initialize the game clock to the time
 *   of the earliest event we find, so it's never necessary to call
 *   eventReached() on this special first event - its function is to tell
 *   the clock manager the game's initial wall-clock time.
 *   
 *   We keep track of the "day" in game time, but we don't have a calendar
 *   feature.  We simply keep track of the day relative to the moment the
 *   game begins; it's up to the game to make this into a calendar date, if
 *   desired.  For example, if the game is designed to begin at noon on
 *   March 21, 1882, the game could figure out the current calendar day by
 *   adding the current day to March 20 (since the first day is day 1).
 *   The game would have to figure out when month and year boundaries are
 *   crossed, of course, to show the resulting calendar date.  If you don't
 *   care about tying your story to a particular calendar date, but you do
 *   want to nail it down to particular days of the week, this is a lot
 *   easier, since you can use "mod-7" arithmetic to compute the weekday -
 *   just use "(day % 7) + 1" as an index into a seven-element list of
 *   weekday names.
 *   
 *   A recommendation on usage: if you're using the clock for scheduling
 *   appointments that the player character is responsible for keeping,
 *   it's a good idea to structure the game so that there's a plot event a
 *   little before an appointment.  For example, if you've told the player
 *   that they have a noon lunch planned with a friend, you should set up
 *   the plot sequence so that there's some important event that happens at
 *   11:45am, or thereabouts.  The event doesn't need to be related to the
 *   lunch in any visible way, as far as the player is concerned - the
 *   point of the event is to gate the lunch appointment.  The event should
 *   usually be something of the nature of solving a puzzle.  This way, the
 *   player works on the puzzle, since it's not yet time for the lunch
 *   appointment; suddenly, when they solve the puzzle, the game can let
 *   them know that it's almost lunch time, and that they should go meet
 *   their friend.  This plays well into the whole subjective-time design
 *   of the clock manager, because the player will presumably have her
 *   attention occupied solving the puzzle, and so it will seem perfectly
 *   natural for the game clock to have changed substantially when she
 *   comes up for air after having been working on the puzzle.  You can
 *   then set up another plot event to represent the player character's
 *   arrival at the restaurant, which might occur in the story at 11:55am
 *   (or maybe 12:20pm, if the PC is someone who tends to annoy his friends
 *   by always running late).
 *   
 *   When the player character isn't supposed to be waiting for a
 *   particular plot event, there's no need to gate it like this.  If you
 *   have a plot event that coincides with night falling, 
 *   
 *   Note that you can use the clock manager even if your game has no
 *   pre-defined plot points.  Just create one event to represent the start
 *   of the game, to give the clock its initial setting.  The clock manager
 *   will use the same subjective time scheme it would if there were more
 *   plot points, but with no ending boundary to worry about.  The clock
 *   manager probably isn't as interesting in a game without significant
 *   time-anchored plot events, since the passage of time doesn't have any
 *   real relation to the plot in such a game;s but it might be desirable
 *   to have a working clock anyway, if only for the added sense of detail.
 */
clockManager: PreinitObject
    /*
     *   Get the current game-clock time.  This returns a list in the same
     *   format as ClockEvent.eventTime: [day,hour,minute].
     *   
     *   Remember that our time-keeping scheme is a sort of "Schrodinger's
     *   clock" [see footnote 1].  Between time checks, the game time
     *   clock is in a vague, fuzzy state, drifting along at an
     *   indeterminate pace from the most recent check.  When this method
     *   is called, though, the clock manager is forced to commit to a
     *   particular time, because we have to give a specific answer to the
     *   question we're being asked ("what time is it?").  As in quantum
     *   mechanics, then, the act of observation affects the quantity
     *   being observed.  Therefore, you should avoid calling this routine
     *   unnecessarily; call it only when you actually have to tell the
     *   player what time it is - and don't tell the player what time it
     *   is unless they ask, or there's some other good reason.
     *   
     *   If you want a string-formatted version of the time (as in
     *   '9:05pm'), you can call checkTimeFmt().  
     */
    checkTime()
    {
        local turns;
        local mm;
        
        /* 
         *   Determine how many turns it's been since we last committed to
         *   a specific wall-clock time.  This will give us the
         *   psychological "scale" of the amount of elapsed wall-clock the
         *   user might expect.  
         */
        turns = Schedulable.gameClockTime - turnLastCommitted;

        /* 
         *   start with the base scaling factor - this is the number of
         *   minutes of game time we impute to a hundred turns, in the
         *   absence of the constraint of running up against the next event
         */
        mm = (turns * baseScaleFactor) / 100;

        /*
         *   If the base scaled time would take us within two hours of the
         *   next event time, slow the clock down from our base scaling
         *   factor so that we always leave ourselves room to advance the
         *   clock further on the next check.  Reduce the passage of time
         *   in proportion to our reduced window - so if we have only 60
         *   minutes left, advance time at half the normal pace.  
         */
        if (nextTime != nil)
        {
            /* get the minutes between now and the next scheduled event */
            local delta = diffMinutes(nextTime, curTime);

            /* check to see if the raw increment would leave under 2 hours */
            if (delta - mm < 120)
            {
                /*
                 *   The raw time increment would leave us under two hours
                 *   away.  If we have under two hours to go before the
                 *   next event, scale down the rate of time in proportion
                 *   to our share under two hours.  (Note that we might
                 *   have more than two hours to go and still be here,
                 *   because the raw adjusted time leaves under two
                 *   hours.)  
                 */
                if (delta < 120)
                    mm = (mm * delta) / 120;

                /* 
                 *   In any case, cap it at half the remaining time, to
                 *   ensure that we won't ever make it to the next event
                 *   time until the next event occurs.
                 */
                if (mm > delta / 2)
                    mm = delta / 2;
            }
        }

        /* 
         *   If our calculation has left us with no passage of time, simply
         *   return the current time unchanged, and do not treat this as a
         *   commit point.  We don't consider this a commit point because
         *   we treat it as not even checking again - it's effectively just
         *   a repeat of the last check, since it's still the same time.
         *   This ensures that we won't freeze the clock for good due to
         *   rounding - enough additional turns will eventually accumulate
         *   to nudge the clock forward.  
         */
        if (mm == 0)
            return curTime;

        /* add the minutes to the current time */
        curTime = addMinutes(curTime, mm);

        /* the current turn is now the last commit point */
        turnLastCommitted = Schedulable.gameClockTime;

        /* return the new time */
        return curTime;
    }

    /*
     *   The base scaling factor: this is the number of minutes per hundred
     *   turns when we have unlimited time until the next event.  This
     *   number is pretty arbitrary, since we're depending so much on the
     *   player's uncertainty about just how long things take, and also
     *   because we'll adjust it anyway when we're running out of time
     *   before the next event.  Even so, you might want to adjust this
     *   value up or down according to your sense of the pacing of your
     *   game.  
     */
    baseScaleFactor = 60

    /*
     *   Get the current game-clock time, formatted into a string with the
     *   given format mask - see formatTime() for details on how to write a
     *   mask string.
     *   
     *   Note that the same cautions for checkTime() apply here - calling
     *   this routine commits us to a particular time, so you should call
     *   this routine only when you're actually ready to display a time to
     *   the player.  
     */
    checkTimeFmt(fmt) { return formatTime(checkTime(), fmt); }
    
    /* 
     *   Get a formatted version of the given wall-clock time.  The time is
     *   expressed as a list, in the same format as ClockEvent.eventTime:
     *   [day,hour,minute], where 'day' is 1 for the first day of the game,
     *   2 for the second, and so on.
     *   
     *   The format string consists of one or more prefixes, followed by a
     *   format mask.  The prefixes are flags that control the formatting,
     *   but don't directly insert any text into the result string:
     *   
     *   24 -> use 24-hour time; if this isn't specified, a 12-hour clock
     *   is used instead.  On the 24-hour clock, midnight is hour zero, so
     *   12:10 AM is represented as 00:10.
     *   
     *   [am][pm] -> use 'am' as the AM string, and 'pm' as the PM string,
     *   for the 'a' format mask character.  This lets you specify an
     *   arbitrary formatting for the am/pm marker, overriding the default
     *   of 'am' or 'pm'.  For example, if you want to use 'A.M.' and
     *   'P.M.'  as the markers, you'd write a prefix of [A.M.][P.M.].  If
     *   you want to use ']' within the marker string itself, quote it with
     *   a '%': '[[AM%]][PM%]]' indicates markers of '[AM]' and '[PM]'.
     *   
     *   Following the prefix flags, you specify the format mask.  This is
     *   a set of special characters that specify parts of the time to
     *   insert.  Each special character is replaced with the corresponding
     *   formatted time information in the result string.  Any character
     *   that isn't special is just copied to the result string as is.  The
     *   special character are:
     *   
     *   h -> hour, no leading zero for single digits (hence 9:05, for
     *   example)
     *   
     *   hh -> hour, leading zero (09:05)
     *   
     *   m -> minutes, no leading zero (9:5)
     *   
     *   mm -> minutes with a leading zero (9:05)
     *   
     *   a -> AM/PM marker.  If an [am][pm] prefix was specified, the 'am'
     *   or 'pm' string from the prefix is used.  Otherwise, 'am' or 'pm'
     *   is literally inserted.
     *   
     *   % -> quote next character (so %% -> a single %)
     *   
     *   other -> literal
     *   
     *   Examples:
     *   
     *   'hh:mma' produces '09:05am'
     *.  '[A.M][P.M]h:mma' produces '9:05 P.M.'
     *.  '24hhmm' produces '2105'.  
     */
    formatTime(t, fmt)
    {
        local hh = t[2];
        local mm = t[3];
        local pm = (hh >= 12);
        local use24 = nil;
        local amStr = nil;
        local pmStr = nil;
        local ret;
        local match;
            
        /* check flags */
        for (;;)
        {
            local fl;
            
            /* check for a flag string */
            match = rexMatch(
                '24|<lsquare>(<^rsquare>|%%<rsquare>)+<rsquare>', fmt, 1);

            /* if we didn't find another flag, we're done */
            if (match == nil)
                break;

            /* pull out the flag text */
            fl = fmt.substr(1, match);
            fmt = fmt.substr(match + 1);

            /* check the match */
            if (fl == '24')
            {
                /* note 24-hour time */
                use24 = true;
            }
            else
            {
                /* it's an am/pm marker - strip the brackets */
                fl = fl.substr(2, fl.length() - 2);

                /* change any '%]' sequences into just ']' */
                fl = fl.findReplace('%]', ']', ReplaceAll, 1);

                /* set AM if we haven't set it already, else set PM */
                if (amStr == nil)
                    amStr = fl;
                else
                    pmStr = fl;
            }
        }

        /* if we didn't select an AM/PM, use the default */
        amStr = (amStr == nil ? 'am' : amStr);
        pmStr = (pmStr == nil ? 'pm' : pmStr);

        /* adjust for a 12-hour clock if we're using one */
        if (!use24)
        {
            /* subtract 12 from PM times */
            if (pm)
                hh -= 12;

            /* hour 0 on a 12-hour clock is written as 12 */
            if (hh == 0)
                hh = 12;
        }

        /* run through the format and build the result string */
        for (ret = '', local i = 1, local len = fmt.length() ; i <= len ; ++i)
        {
            /* check what we have */
            match = rexMatch(
                '<case>h|hh|m|mm|a|A|am|AM|a%.m%.|A.%M%.|24|%%', fmt, i);
            if (match == nil)
            {
                /* no match - copy this character literally */
                ret += fmt.substr(i, 1);
            }
            else
            {
                /* we have a match - check what we have */
                switch (fmt.substr(i, match))
                {
                case 'h':
                    /* add the hour, with no leading zero */
                    ret += toString(hh);
                    break;

                case 'hh':
                    /* add the hour, with a leading zero if needed */
                    if (hh < 10)
                        ret += '0';
                    ret += toString(hh);
                    break;

                case 'm':
                    /* add the minute, with no leading zero */
                    ret += toString(mm);
                    break;
                    
                case 'mm':
                    /* add the minute, with a leading zero if needed */
                    if (mm < 10)
                        ret += '0';
                    ret += toString(mm);
                    break;
                    
                case 'a':
                    /* add the am/pm indicator */
                    ret += (pm ? pmStr : amStr);
                    break;
                    
                case '%':
                    /* add the next character literally */
                    ++i;
                    ret += fmt.substr(i, 1);
                    break;
                }

                /* skip any extra characters in the field */
                i += match - 1;
            }
        }

        /* return the result string */
        return ret;
    }

    /* pre-initialize */
    execute()
    {
        local vec;
        
        /* build a list of all of the ClockEvent objects in the game */
        vec = new Vector(10);
        forEachInstance(ClockEvent, {x: vec.append(x)});

        /* sort the list by time */
        vec.sort(SortAsc, {a, b: a.compareTime(b)});

        /* store it */
        eventList = vec.toList();

        /* 
         *   The earliest event is always the marker for the beginning of
         *   the game.  Since it's now the start of the game, mark the
         *   first event in our list as reached.  (The first event is
         *   always the earliest we find, by virtue of the sort we just
         *   did.)  
         */
        vec[1].eventReached();
    }

    /* 
     *   Receive notification from a clock event that an event has just
     *   occurred.  (This isn't normally called directly from game code;
     *   instead, game code should usually call the ClockEvent object's
     *   eventReached() method.)  
     */
    eventReached(evt)
    {
        local idx;
        
        /* find the event in our list */
        idx = eventList.indexOf(evt);

        /* 
         *   Never go backwards - if events fire out of order, keep only
         *   the later event.  (Games should generally be constructed in
         *   such a way that events can only fire in order to start with,
         *   but in case a weird case slips through, we make this extra
         *   test to ensure that the player doesn't see any strange
         *   retrograde motion on the clock.) 
         */
        if (lastEvent != nil && lastEvent.compareTime(evt) > 0)
            return;
        
        /* note the current time */
        curTime = evt.eventTime;

        /* if there's another event following, note the next time */
        if (idx < eventList.length())
            nextTime = eventList[idx + 1].eventTime;
        else
            nextTime = nil;

        /* 
         *   we're committing to an exact wall-clock time, so remember the
         *   current turn counter as the last commit point 
         */
        turnLastCommitted = Schedulable.gameClockTime;
    }

    /* add minutes to a [dd,hh,mm] value, returning a new [dd,hh,mm] value */
    addMinutes(t, mm)
    {
        /* add the minutes; if that takes us over 60, carry to hours */
        if ((t[3] += mm) >= 60)
        {
            local hh;
            
            /* we've passed 60 minutes - figure how many hours that is */
            hh = t[3] / 60;

            /* keep only the excess-60 minutes in the minutes slot */
            t[3] %= 60;

            /* add the hours; if that takes us over 24, carry to days */
            if ((t[2] += hh) >= 24)
            {
                local dd;
                
                /* we've passed 24 hours - figure how many days that is */
                dd = t[2] / 24;

                /* keep only the excess-24 hours in the hours slot */
                t[2] %= 24;

                /* add the days */
                t[1] += dd;
            }
        }

        /* return the adjusted time */
        return t;
    }

    /* get the difference in minutes between two [dd,hh,mm] values */
    diffMinutes(t1, t2)
    {
        local mm;
        local hh;
        local dd;
        local bhh = 0;
        local bdd = 0;
        
        /* get the difference in minutes; if negative, note the borrow */
        mm = t1[3] - t2[3];
        if (mm < 0)
        {
            mm += 60;
            bhh = 1;
        }

        /* get the difference in hours; if negative, note the borrow */
        hh = t1[2] - t2[2] - bhh;
        if (hh < 0)
        {
            hh += 24;
            bdd = 1;
        }

        /* get the difference in days */
        dd = t1[1] - t2[1] - bdd;

        /* add them all together to get the total minutes */
        return mm + 60*hh + 60*24*dd;
    }

    /* 
     *   our list of clock events (we build this automatically during
     *   pre-initialization) 
     */
    eventList = nil

    /* the current game clock time */
    curTime = nil

    /* the most recent event that we reached */
    lastEvent = nil

    /* the next event's game clock time */
    nextTime = nil

    /* 
     *   The turn counter (Schedulable.gameClockTime) on the last turn
     *   where committed to a specific time.  Each time we check the time,
     *   we look here to see how many turns have elapsed since the last
     *   time check, and we use this to choose a plausible scale for the
     *   wall-clock time change.  
     */
    turnLastCommitted = 0
;

/*
 *   Clock-setting plot event.  This object represents a plot point that
 *   occurs at a particular time in the story world.  Create one of these
 *   for each of your plot events.  The Clock Manager automatically builds
 *   a list of all of these objects during pre-initialization, so you don't
 *   have to explicitly tell the clock manager about these.
 *   
 *   Whenever the story reaches one of these events, you should call the
 *   eventReached() method of the event object.  This will set the clock
 *   time to the event's current time, and take note of how long we have
 *   until the next plot event.  
 */
class ClockEvent: object
    /*
     *   The time at which this event occurs.  This is expressed as a list
     *   with three elements: the day number, the hour (on a 24-hour
     *   clock), and the minute.  The day number is relative to the start
     *   of the game - day 1 is the first day of the game.  So, for
     *   example, to express 2:40pm on the second day of the game, you'd
     *   write [2,14,40].  Note that 12 AM is written as 0 (zero) on a
     *   24-hour clock, so 12:05am on day 1 would be [1,0,5].  
     */
    eventTime = [1,0,0]

    /* get a formatted version of the event time */
    formatTime(fmt) { return clockManager.formatTime(eventTime, fmt); }

    /* 
     *   Compare our event time to another event's time.  Returns -1 if our
     *   time is earlier than other's, 0 if we're equal, and 1 if we're
     *   after other. 
     */
    compareTime(other)
    {
        local a = eventTime;
        local b = other.eventTime;
        
        /* compare based on the most significant element that differs */
        if (a[1] != b[1])
            return a[1] - b[1];
        else if (a[2] != b[2])
            return a[2] - b[2];
        else
            return a[3] - b[3];
    }

    /*
     *   Notify the clock manager that this event has just occurred.  This
     *   sets the game clock to the event's time.  The game code must call
     *   this method when our point in the plot is reached.  
     */
    eventReached()
    {
        /* notify the clock manager */
        clockManager.eventReached(self);
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   [Footnote 1]
 *   
 *   "Schrodinger's cat" is a famous thought experiment in quantum
 *   physics, concerning how a quantum mechanical system exists in
 *   multiple, mutually exclusive quantum states simultaneously until an
 *   observer forces the system to assume only one of the states by the
 *   act of observation.  The thought experiment has been popularized as
 *   an illustration of how weird and wacky QM is, but it's interesting to
 *   note that Schrodinger actually devised it to expose what he saw as an
 *   unacceptable paradox in quantum theory.
 *   
 *   The thought experiment goes like this: a cat is locked inside a
 *   special box that's impervious to light, X-rays, etc., so that no one
 *   on the outside can see what's going on inside.  The box contains,
 *   apart from the cat, a little radiation source and a radiation
 *   counter.  When the counter detects a certain radioactive emission, it
 *   releases some poison gas, killing the cat.  The radioactive emission
 *   is an inherently quantum mechanical, unpredictable process, and as
 *   such can (and must) be in a superposition of "emitted" and "not
 *   emitted" states until observed.  Because the whole system is
 *   unobservable from the outside, the supposition is that everything
 *   inside is "entangled" with the quantum state of the radioactive
 *   emission, hence the cat is simultaneously living and dead until
 *   someone opens the box and checks.  It's not just that no one knows;
 *   rather, the cat is actually and literally alive and dead at the same
 *   time.
 *   
 *   Schrodinger's point was that this superposition of the cat's states
 *   is a necessary consequence of the way QM was interpreted at the time
 *   he devised the experiment, but that it's manifestly untrue, since we
 *   know that cats are macroscopic objects that behave according to
 *   classical, deterministic physics.  Hence a paradox, hence the
 *   interpretation of the theory must be wrong.  The predominant
 *   interpretation of QM has since shifted a bit so that the cat would
 *   now count as an observer - not because it's alive or conscious or
 *   anything metaphysical, but simply because it's macroscopic - so the
 *   cat's fate is never actually entangled with the radioactive source's
 *   quantum state.  Popular science writers have continued to talk about
 *   Schrodinger's cat as though it's for real, maybe to make QM seem more
 *   exotic to laypersons, but most physicists today wouldn't consider the
 *   experiment to be possible as literally stated.  Physicists today
 *   might think of it as a valid metaphor to decribe systems where all of
 *   the components are on an atomic or subatomic scale, but no one today
 *   seriously thinks you can create an actual cat that's simultaneously
 *   alive and dead.  
 */

